/**
 * @file include/seam/cost.hpp
 * @brief Cumulative cost maps: backward dynamic programming.
 *
 * Seam Carving in C++ - content-aware image resizing.
 * Ported from https://github.com/vivianhylee/seam-carving
 *
 * @author Tea Zhi Xun
 */
#pragma once

#include "seam/image.hpp"

namespace seam
{

// Accumulated cost, top to bottom, using plain energy only. Used by seam
// insertion and by the GUI's "next seam" preview.
ImageF cumulative_cost_backward(const ImageF& energy);

} // namespace seam
