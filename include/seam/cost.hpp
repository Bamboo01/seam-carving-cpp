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

// TODO: #include "seam/energy.hpp" once the energy module is merged.
#include "seam/image.hpp"

namespace seam
{

// Accumulated cost, top to bottom, using the forward-energy terms. `image` is
// needed because the neighbour matrices come from the image, not the energy.
ImageF cumulative_cost_forward(const ImageF& image, const ImageF& energy);

// Accumulated cost, top to bottom, using plain energy only. Used by seam
// insertion and by the GUI's "next seam" preview.
ImageF cumulative_cost_backward(const ImageF& energy);

} // namespace seam
