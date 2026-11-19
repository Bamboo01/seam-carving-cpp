/**
 * @file include/seam/carver.hpp
 * @brief Carver pipeline: resize, expand, protect, and object removal.
 *
 * Seam Carving in C++ - content-aware image resizing.
 * Ported from https://github.com/vivianhylee/seam-carving
 *
 * @author Teo Pek Lin
 */
#pragma once

#include "seam/cost.hpp"
#include "seam/image.hpp"
#include "seam/seam_ops.hpp"

#include <utility>

namespace seam
{

class Carver
{
public:
    explicit Carver(ImageF source, CostOptions options = {});

    // mask must match the source dimensions.
    // large +ve value so seams route around it
    // large -ve value so seams route straight through it
    void set_protect_mask(ImageF mask);
    void set_object_mask(ImageF mask);

    bool has_protect_mask() const
    {
        return mask_kind_ == MaskKind::Protect;
    }

    bool has_object_mask() const
    {
        return mask_kind_ == MaskKind::Object;
    }

    void resize_to(int width, int height);
    void remove_object();

    const ImageF& image() const
    {
        return image_;
    }

    const ImageF& source() const
    {
        return source_;
    }

    // Energy of the current image, with mask weighting applied when relevant.
    ImageF energy() const;

    // The seam that the next removal would take, for previewing.
    Seam next_seam() const;

    void reset();

    double last_elapsed_ms() const
    {
        return last_elapsed_ms_;
    }

private:
    enum class MaskKind
    {
        None,
        Protect,
        Object
    };

    ImageF source_;
    ImageF image_;
    ImageF mask_;
    ImageF mask_original_;
    MaskKind mask_kind_ = MaskKind::None;
    CostOptions options_;
    double last_elapsed_ms_ = 0.0;

    void seams_removal(int count);
    void seams_carving(int target_width, int target_height);
    void seams_insertion(int count);
    void carve_object();
    ImageF weighted_energy() const;
    bool mask_has_pixels() const;
    std::pair<int, int> object_bounds() const;
};

} // namespace seam
