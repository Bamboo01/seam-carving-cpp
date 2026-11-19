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
#include <utility>

namespace seam
{
namespace
{

// Python: output[row - 1, max(col - 1, 0) : min(col + 2, n - 1)]
// The slice end is exclusive, so the rightmost column loses its right-hand
// candidate. `fix_right_edge` restores the symmetric window.
std::pair<int, int> backward_window(int x, int width, bool fix_right_edge)
{
    const int start = std::max(x - 1, 0);
    const int end = fix_right_edge ? std::min(x + 2, width) : std::min(x + 2, width - 1);
    return {start, std::max(end, start + 1)};
}

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

ImageF cumulative_cost_forward(const ImageF& image, const ImageF& energy,
                               const CostOptions& options)
{
    // The forward pass always looks at the same three neighbours, so the
    // right-edge option does not apply here (the backward pass and seam search
    // are where upstream's window is off by one).
    (void)options;

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

ImageF cumulative_cost_backward(const ImageF& energy, const CostOptions& options)
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
            const auto [start, end] = backward_window(x, width, options.fix_right_edge);

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
