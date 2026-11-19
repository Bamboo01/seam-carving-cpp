/**
 * @file src/core/cost.cpp
 * @brief Dynamic programming cost accumulation.
 *
 * Seam Carving in C++ - content-aware image resizing.
 * Ported from https://github.com/vivianhylee/seam-carving
 *
 * @author Tea Zhi Xun
 */
#include "seam/cost.hpp"

#include <algorithm>

namespace seam
{

ImageF cumulative_cost_backward(const ImageF& energy)
{
    ImageF out = energy;

    const int width = energy.width;
    const int height = energy.height;
    if (width <= 0 || height <= 0)
        return out;

    for (int y = 1; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const int start = std::max(x - 1, 0);
            const int end = std::min(x + 2, width);

            double best = out.at(start, y - 1);
            for (int i = start + 1; i < end; ++i)
            {
                best = std::min(best, out.at(i, y - 1));
            }
            out.at(x, y) = energy.at(x, y) + best;
        }
    }
    return out;
}

} // namespace seam
