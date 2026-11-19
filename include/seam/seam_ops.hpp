/**
 * @file include/seam/seam_ops.hpp
 * @brief Seam search, deletion, insertion, and image rotation.
 *
 * Seam Carving in C++ - content-aware image resizing.
 * Ported from https://github.com/vivianhylee/seam-carving
 *
 * @author Sim Wei Siang
 */
#pragma once

#include "seam/image.hpp"

#include <vector>

namespace seam
{

// One column index per row. Always index 0 <= seam[y] < image.width.
using Seam = std::vector<int>;

// Backtracks the minimum-cost path through an accumulated cost map. Ties go to
// the lowest index, matching np.argmin.
Seam find_seam(const ImageF& cumulative, bool fix_right_edge = false);

// In-place edits. Width shrinks or grows by one; height is untouched.
void delete_seam(ImageF& image, const Seam& seam);
void add_seam(ImageF& image, const Seam& seam);

// The same two operations for a single-channel mask.
void delete_seam_on_mask(ImageF& mask, const Seam& seam);
void add_seam_on_mask(ImageF& mask, const Seam& seam);

// After inserting `inserted`, recorded seams that sit at or to the right of it
// must move two columns over (one for the insert, one for the paired pixel).
void shift_seam(Seam& seam, const Seam& inserted);
void shift_seams(std::vector<Seam>& seams, const Seam& inserted);

// Geometry. Row-wise scanning happens in the rotated frame, exactly as the
// Python rotates the image to reuse one code path for width and height.
ImageF rotate_ccw(const ImageF& image);
ImageF rotate_cw(const ImageF& image);

} // namespace seam
