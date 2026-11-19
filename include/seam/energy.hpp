/**
 * @file include/seam/energy.hpp
 * @brief Energy map and the forward-energy neighbour terms.
 *
 * Seam Carving in C++ - content-aware image resizing.
 * Ported from https://github.com/vivianhylee/seam-carving
 *
 * @author Lim Rui Heng
 */
#pragma once

#include "seam/image.hpp"

namespace seam
{

// Per-pixel energy: |Scharr_x| + |Scharr_y| summed over the colour channels,
// with cv2's BORDER_REFLECT_101 edge handling.
//
// The Python calls cv2.Scharr on each BGR plane, takes the absolute value and
// adds the two directions, then adds the three channels together. Getting the
// border mode wrong shifts every edge pixel and quietly reroutes seams, so
// this is where fidelity lives.
ImageF energy_map(const ImageF& image);

// Three "new neighbour" cost matrices used by the forward-energy pass. They are
// convolutions of the *image* (not the energy), each reduced to a single plane
// by summing the absolute value over the channels:
//
//   neighbour_x       = |I(x+1, y) - I(x-1, y)|          (horizontal step)
//   neighbour_y_left  = |I(x+1, y) - I(x, y+1)|          (step from lower right)
//   neighbour_y_right = |I(x-1, y) - I(x, y+1)|          (step from lower left)
struct ForwardNeighbours
{
    ImageF x;
    ImageF y_left;
    ImageF y_right;
};

ForwardNeighbours forward_energy_neighbours(const ImageF& image);

} // namespace seam
