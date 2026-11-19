/**
 * @file include/seam/cost.hpp
 * @brief Cumulative cost maps: forward and backward dynamic programming.
 *
 * Seam Carving in C++ - content-aware image resizing.
 * Ported from https://github.com/vivianhylee/seam-carving
 *
 * @author Tea Zhi Xun
 */
#pragma once

#include "seam/energy.hpp"
#include "seam/image.hpp"

namespace seam
{

// Upstream keeps an off-by-one in the right-edge window of both dynamic
// programming passes: `cumulative[row, prv-1 : min(prv+2, n-1)]` is end-exclusive,
// so the rightmost column never considers the pixel directly above it. The
// default reproduces that exactly; flip this on to get the symmetric window.
struct CostOptions
{
    bool fix_right_edge = false;
};

// Accumulated cost, top to bottom, using the forward-energy terms. `image` is
// needed because the neighbour matrices come from the image, not the energy.
ImageF cumulative_cost_forward(const ImageF& image, const ImageF& energy,
                               const CostOptions& options = {});

// Accumulated cost, top to bottom, using plain energy only. Used by seam
// insertion and by the GUI's "next seam" preview.
ImageF cumulative_cost_backward(const ImageF& energy, const CostOptions& options = {});

} // namespace seam
