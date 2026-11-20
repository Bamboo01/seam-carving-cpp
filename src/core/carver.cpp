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
#include <cmath>
#include <stdexcept>
#include <utility>

namespace seam
{
namespace
{

// +1000 to shield a protected region and -1000 to force a seam
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
    start_run();
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
    start_run();
}

void Carver::start_run()
{
    positions_.allocate(source_.width, source_.height, 2);
    for (int y = 0; y < source_.height; ++y)
    {
        for (int x = 0; x < source_.width; ++x)
        {
            positions_.at(x, y, 0) = x;
            positions_.at(x, y, 1) = y;
        }
    }
    carved_.clear();
    seams_carved_ = 0;
    total_seam_energy_ = 0.0;
}

void Carver::record_seam(const ImageF& positions, const Seam& seam)
{
    for (int y = 0; y < positions.height; ++y)
    {
        const int x = seam[static_cast<std::size_t>(y)];
        const int sx = std::clamp(static_cast<int>(std::lround(positions.at(x, y, 0))), 0,
                                  source_.width - 1);
        const int sy = std::clamp(static_cast<int>(std::lround(positions.at(x, y, 1))), 0,
                                  source_.height - 1);
        carved_.push_back(static_cast<std::size_t>(sy) * static_cast<std::size_t>(source_.width) +
                          static_cast<std::size_t>(sx));
    }
    ++seams_carved_;
}

ImageF Carver::seam_overlay() const
{
    ImageF out = source_;
    for (std::size_t index : carved_)
    {
        const int x = static_cast<int>(index % static_cast<std::size_t>(source_.width));
        const int y = static_cast<int>(index / static_cast<std::size_t>(source_.width));
        if (out.channels >= 3)
        {
            out.at(x, y, 0) = 255.0;
            out.at(x, y, 1) = 40.0;
            out.at(x, y, 2) = 40.0;
        }
        else
        {
            out.at(x, y, 0) = 255.0;
        }
    }
    return out;
}

Seam Carver::choose_seam(const ImageF& image, const ImageF& energy, bool insertion) const
{
    switch (method_)
    {
    case SeamMethod::Greedy:
        return find_seam_greedy(energy);
    case SeamMethod::GraphCut:
        return find_seam_graph_cut(energy);
    case SeamMethod::DpBackward:
        return find_seam(cumulative_cost_backward(energy, options_), options_.fix_right_edge);
    case SeamMethod::DynamicProgramming:
        break;
    }

    // The reference removes with forward energy and inserts with backward.
    const ImageF cost = insertion ? cumulative_cost_backward(energy, options_)
                                  : cumulative_cost_forward(image, energy, options_);
    return find_seam(cost, options_.fix_right_edge);
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
    return choose_seam(image_, energy, false);
}

void Carver::resize_to(int width, int height)
{
    const int target_width = std::max(width, 1);
    const int target_height = std::max(height, 1);

    const auto started = std::chrono::steady_clock::now();

    image_ = source_;
    mask_ = mask_original_;
    start_run();

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
        const Seam seam = choose_seam(image_, energy, false);
        total_seam_energy_ += seam_energy(energy, seam);
        record_seam(positions_, seam);

        delete_seam(image_, seam);
        delete_seam(positions_, seam);
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
        positions_ = rotate_ccw(positions_);

        if (mask_kind_ == MaskKind::Protect)
            mask_ = rotate_ccw(mask_);

        seams_removal(-delta_rows);
        image_ = rotate_cw(image_);
        positions_ = rotate_cw(positions_);
    }

    else if (delta_rows > 0)
    {
        image_ = rotate_ccw(image_);
        positions_ = rotate_ccw(positions_);

        if (mask_kind_ == MaskKind::Protect)
            mask_ = rotate_ccw(mask_);

        seams_insertion(delta_rows);
        image_ = rotate_cw(image_);
        positions_ = rotate_cw(positions_);
    }
}

void Carver::seams_insertion(int count)
{
    if (count <= 0)
    {
        return;
    }

    // record the seams on a throwaway copy (they are found on a shrinking image),
    // then replay them onto the original, shifting each recorded seam by the two
    // columns every earlier insertion added.
    ImageF scratch_image = image_;
    ImageF scratch_mask = mask_;
    ImageF scratch_positions = positions_;
    const bool protect = mask_kind_ == MaskKind::Protect;

    std::vector<Seam> recorded;
    for (int i = 0; i < count && scratch_image.width > 1; ++i)
    {
        const ImageF energy = [&]
        {
            ImageF map = energy_map(scratch_image);
            if (protect)
            {
                scale_where_masked(map, scratch_mask, kMaskWeight);
            }
            return map;
        }();

        const Seam seam = choose_seam(scratch_image, energy, true);
        total_seam_energy_ += seam_energy(energy, seam);
        record_seam(scratch_positions, seam);

        recorded.push_back(seam);
        delete_seam(scratch_image, seam);
        delete_seam(scratch_positions, seam);
        if (protect)
        {
            delete_seam_on_mask(scratch_mask, seam);
        }
    }

    for (std::size_t i = 0; i < recorded.size(); ++i)
    {
        add_seam(image_, recorded[i]);
        add_seam(positions_, recorded[i]);
        if (protect)
        {
            add_seam_on_mask(mask_, recorded[i]);
        }
        for (std::size_t j = i + 1; j < recorded.size(); ++j)
        {
            shift_seam(recorded[j], recorded[i]);
        }
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
    start_run();

    const bool rotated = [this]
    {
        const auto [object_height, object_width] = object_bounds();
        return object_height < object_width;
    }();

    if (rotated)
    {
        image_ = rotate_ccw(image_);
        mask_ = rotate_ccw(mask_);
        positions_ = rotate_ccw(positions_);
    }

    // Every pass deletes one seam and carves that same seam out of the mask, so
    // the loop ends once the mask is empty
    int guard = image_.width + image_.height + 8;
    while (mask_has_pixels() && image_.width > 1 && guard-- > 0)
    {
        const ImageF energy = weighted_energy();
        const Seam seam = choose_seam(image_, energy, false);
        total_seam_energy_ += seam_energy(energy, seam);
        record_seam(positions_, seam);

        delete_seam(image_, seam);
        delete_seam(positions_, seam);
        delete_seam_on_mask(mask_, seam);
    }

    const int removed =
        rotated ? (source_.height - image_.width) : (source_.width - image_.width);
    seams_insertion(std::max(removed, 0));

    if (rotated)
    {
        image_ = rotate_cw(image_);
        positions_ = rotate_cw(positions_);
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
