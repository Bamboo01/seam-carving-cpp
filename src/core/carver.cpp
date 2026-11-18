/**
 * @file src/core/carver.cpp
 * @brief Orchestration of resize, expand, protect, and object removal.
 *
 * @author Teo Pek Lin
 */
#include "seam/carver.hpp"

#include "seam/energy.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace seam
{
namespace
{

// +1000 to shield a protected region and -100 to force a seam
// thorugh an object
constexpr double kMaskWeight = 1000.0;

void scale_where_masked(ImageF& energy, const ImageF& mask, double factor)
{
    for (int y = 0; y < energy.height; ++y)
    {
        for (int x = 0; x < energy.width; ++x)
        {
            if (mask.at(x, y) > 0.0)
            {
                energy.at(x, y) *= factor;
            }
        }
    }
}

void require_same_size(const ImageF& mask, const ImageF& image, const char* what)
{
    if (mask.width != image.width || mask.height != image.height)
    {
        throw std::invalid_argument(
            std::string(what) + ": mask is " + std::to_string(mask.width) + "x" +
            std::to_string(mask.height) + ", image is " + std::to_string(image.width) +
            "x" + std::to_string(image.height));
    }
}

} // namespace

Carver::Carver(ImageF source, CostOptions options)
    : source_(std::move(source)), image_(source_), options_(options)
{
    if (source_.width <= 0 || source_.height <= 0 || source_.channels <= 0)
    {
        throw std::invalid_argument("Carver: source image is empty");
    }
}

void Carver::set_protect_mask(ImageF mask)
{
    require_same_size(mask, source_, "set_protect_mask");
    mask_ = std::move(mask);
    mask_original_ = mask_;
    mask_kind_ = MaskKind::Protect;
}

void Carver::set_object_mask(ImageF mask)
{
    require_same_size(mask, source_, "set_object_mask");
    mask_ = std::move(mask);
    mask_original_ = mask_;
    mask_kind_ = MaskKind::Object;
}

void Carver::reset()
{
    image_ = source_;
    mask_ = mask_original_;
    last_elapsed_ms_ = 0.0;
}

ImageF Carver::weighted_energy() const
{
    ImageF energy = energy_map(image_);

    if (mask_kind_ == MaskKind::Protect)
    {
        scale_where_masked(energy, mask_, kMaskWeight);
    }
    else if (mask_kind_ == MaskKind::Object)
    {
        scale_where_masked(energy, mask_, -kMaskWeight);
    }
    return energy;
}

ImageF Carver::energy() const
{
    return weighted_energy();
}

Seam Carver::next_seam() const
{
    const ImageF energy = weighted_energy();
    const ImageF cost = cumulative_cost_forward(image_, energy, options_);
    return find_seam(cost, options_.fix_right_edge);
}

void Carver::resize_to(int width, int height)
{
    const int target_width = std::max(width, 1);
    const int target_height = std::max(height, 1);

    const auto started = std::chrono::steady_clock::now();

    image_ = source_;
    mask_ = mask_original_;

    seams_carving(target_width, target_height);

    const auto finished = std::chrono::steady_clock::now();
    last_elapsed_ms_ = std::chrono::duration<double, std::milli>(finished - started).count();
}

void Carver::seams_removal(int count)
{
    const bool protect = mask_kind_ == MaskKind::Protect;

    for (int i = 0; i < count && image_.width > 1; ++i)
    {
        const ImageF energy = weighted_energy();
        const ImageF cost = cumulative_cost_forward(image_, energy, options_);
        const Seam seam = find_seam(cost, options_.fix_right_edge);

        delete_seam(image_, seam);
        if (protect)
        {
            delete_seam_on_mask(mask_, seam);
        }
    }
}

void Carver::seams_carving(int target_width, int target_height)
{
    const int delta_rows = target_height - source_.height;
    const int delta_cols = target_width - source_.width;

    if (delta_cols < 0)
    {
        seams_removal(-delta_cols);
    }
    else if (delta_cols > 0)
    {
        seams_insertion(delta_cols);
    }

    // rows are carved by rotating the problem onto its side and reusing the same column machinery.
    if (delta_rows < 0)
    {
        image_ = rotate_ccw(image_);

        if (mask_kind_ == MaskKind::Protect)
            mask_ = rotate_ccw(mask_);

        seams_removal(-delta_rows);
        image_ = rotate_cw(image_);
    }

    else if (delta_rows > 0)
    {
        image_ = rotate_ccw(image_);

        if (mask_kind_ == MaskKind::Protect)
            mask_ = rotate_ccw(mask_);

        seams_insertion(delta_rows);
        image_ = rotate_cw(image_);
    }
}

void Carver::remove_object()
{
    if (mask_kind_ != MaskKind::Object)
    {
        throw std::runtime_error("remove_object: no object mask was set");
    }

    const auto started = std::chrono::steady_clock::now();

    image_ = source_;
    mask_ = mask_original_;

    const bool rotated = [this]
    {
        const auto [object_height, object_width] = object_bounds();
        return object_height < object_width;
    }();

    if (rotated)
    {
        image_ = rotate_ccw(image_);
        mask_ = rotate_ccw(mask_);
    }

    // Every pass deletes one seam and carves that same seam out of the mask, so
    // the loop ends once the mask is empty
    int guard = image_.width + image_.height + 8;
    while (mask_has_pixels() && image_.width > 1 && guard-- > 0)
    {
        const ImageF energy = weighted_energy();
        const ImageF cost = cumulative_cost_forward(image_, energy, options_);
        const Seam seam = find_seam(cost, options_.fix_right_edge);

        delete_seam(image_, seam);
        delete_seam_on_mask(mask_, seam);
    }

    const int removed =
        rotated ? (source_.height - image_.width) : (source_.width - image_.width);
    seams_insertion(std::max(removed, 0));

    if (rotated)
    {
        image_ = rotate_cw(image_);
    }

    const auto finished = std::chrono::steady_clock::now();
    last_elapsed_ms_ =
        std::chrono::duration<double, std::milli>(finished - started).count();
}

std::pair<int, int> Carver::object_bounds() const
{
    int min_x = mask_.width;
    int min_y = mask_.height;
    int max_x = -1;
    int max_y = -1;

    for (int y = 0; y < mask_.height; ++y)
    {
        for (int x = 0; x < mask_.width; ++x)
        {
            if (mask_.at(x, y) > 0.0)
            {
                min_x = std::min(min_x, x);
                min_y = std::min(min_y, y);
                max_x = std::max(max_x, x);
                max_y = std::max(max_y, y);
            }
        }
    }

    if (max_x < 0)
    {
        throw std::runtime_error("remove_object: the object mask is empty");
    }
    return {max_y - min_y + 1, max_x - min_x + 1};
}

bool Carver::mask_has_pixels() const
{
    for (double value : mask_.data)
    {
        if (value > 0.0)
        {
            return true;
        }
    }
    return false;
}

} // namespace seam
