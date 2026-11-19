/**
 * @file src/core/cost.cpp
 * @brief Dynamic programming cost accumulation, both directions.
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
namespace
{

// TODO: Remove this temporary placeholder and use ForwardNeighbours from
// "seam/energy.hpp" once the energy module is merged.
struct TempForwardNeighbours
{
    ImageF x;
    ImageF y_left;
    ImageF y_right;
};

TempForwardNeighbours placeholder_forward_neighbours(const ImageF& energy)
{
    TempForwardNeighbours out;
    if (energy.width > 0 && energy.height > 0)
    {
        out.x.allocate(energy.width, energy.height, 1);
        out.y_left.allocate(energy.width, energy.height, 1);
        out.y_right.allocate(energy.width, energy.height, 1);
    }
    return out;
}

} // namespace

ImageF cumulative_cost_forward(const ImageF& image, const ImageF& energy)
{
    // TODO: Replace placeholder with:
    //   const ForwardNeighbours neighbours = forward_energy_neighbours(image);
    (void)image;
    const TempForwardNeighbours neighbours = placeholder_forward_neighbours(energy);
    ImageF out = energy;

    const int width = energy.width;
    const int height = energy.height;
    if (width <= 0 || height <= 0)
        return out;

    for (int y = 1; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const double pixel_energy = energy.at(x, y);

            if (width == 1)
            {
                out.at(x, y) =
                    pixel_energy + out.at(x, y - 1) + neighbours.x.at(x, y - 1);
                continue;
            }

            const double up = out.at(x, y - 1) + neighbours.x.at(x, y - 1);

            if (x == 0)
            {
                const double from_right = out.at(x + 1, y - 1) +
                                          neighbours.x.at(x + 1, y - 1) +
                                          neighbours.y_right.at(x + 1, y - 1);
                out.at(x, y) = pixel_energy + std::min(from_right, up);
            }
            else if (x == width - 1)
            {
                const double from_left = out.at(x - 1, y - 1) +
                                         neighbours.x.at(x - 1, y - 1) +
                                         neighbours.y_left.at(x - 1, y - 1);
                out.at(x, y) = pixel_energy + std::min(from_left, up);
            }
            else
            {
                const double from_left = out.at(x - 1, y - 1) +
                                         neighbours.x.at(x - 1, y - 1) +
                                         neighbours.y_left.at(x - 1, y - 1);
                const double from_right = out.at(x + 1, y - 1) +
                                          neighbours.x.at(x + 1, y - 1) +
                                          neighbours.y_right.at(x + 1, y - 1);
                out.at(x, y) = pixel_energy + std::min({from_left, from_right, up});
            }
        }
    }
    return out;
}

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
