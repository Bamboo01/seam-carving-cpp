/**
 * @file src/core/energy.cpp
 * @brief Scharr energy map and forward-energy neighbour matrices.
 *
 * Seam Carving in C++ - content-aware image resizing.
 * Ported from https://github.com/vivianhylee/seam-carving
 *
 * @author Lim Rui Heng
 */
#include "seam/energy.hpp"

#include <cmath>

namespace seam
{
namespace
{

// cv2's BORDER_REFLECT_101 mirrors without repeating the edge sample:
//   ... c b | a b c d | c b ...
// so -1 maps to 1 and size maps to size - 2. Only a one-pixel halo is ever
// needed here, and images thinner than two pixels fall back to the edge.
int reflect101(int index, int size)
{
    if (size <= 1)
        return 0;
    if (index < 0)
        return -index;
    if (index >= size)
        return 2 * size - index - 2;
    return index;
}

} // namespace

ImageF energy_map(const ImageF& image)
{
    ImageF out;
    out.allocate(image.width, image.height, 1);

    const int width = image.width;
    const int height = image.height;

    for (int y = 0; y < height; ++y)
    {
        const int ym1 = reflect101(y - 1, height);
        const int yp1 = reflect101(y + 1, height);

        for (int x = 0; x < width; ++x)
        {
            const int xm1 = reflect101(x - 1, width);
            const int xp1 = reflect101(x + 1, width);

            double energy = 0.0;
            for (int c = 0; c < image.channels; ++c)
            {
                // Scharr, dx = 1:  [-3 0 3; -10 0 10; -3 0 3]
                const double gx =
                    -3.0 * image.at(xm1, ym1, c) - 10.0 * image.at(xm1, y, c) -
                    3.0 * image.at(xm1, yp1, c) + 3.0 * image.at(xp1, ym1, c) +
                    10.0 * image.at(xp1, y, c) + 3.0 * image.at(xp1, yp1, c);

                // Scharr, dy = 1: [-3 -10 -3; 0 0 0; 3 10 3]
                const double gy =
                    -3.0 * image.at(xm1, ym1, c) - 10.0 * image.at(x, ym1, c) -
                    3.0 * image.at(xp1, ym1, c) + 3.0 * image.at(xm1, yp1, c) +
                    10.0 * image.at(x, yp1, c) + 3.0 * image.at(xp1, yp1, c);

                energy += std::fabs(gx) + std::fabs(gy);
            }
            out.at(x, y) = energy;
        }
    }
    return out;
}

ForwardNeighbours forward_energy_neighbours(const ImageF& image)
{
    const int width = image.width;
    const int height = image.height;

    ForwardNeighbours out;
    out.x.allocate(width, height, 1);
    out.y_left.allocate(width, height, 1);
    out.y_right.allocate(width, height, 1);

    for (int y = 0; y < height; ++y)
    {
        const int yp1 = reflect101(y + 1, height);

        for (int x = 0; x < width; ++x)
        {
            const int xm1 = reflect101(x - 1, width);
            const int xp1 = reflect101(x + 1, width);

            double across = 0.0;
            double from_right = 0.0;
            double from_left = 0.0;

            for (int c = 0; c < image.channels; ++c)
            {
                const double below = image.at(x, yp1, c);
                across += std::fabs(image.at(xp1, y, c) - image.at(xm1, y, c));
                from_right += std::fabs(image.at(xp1, y, c) - below);
                from_left += std::fabs(image.at(xm1, y, c) - below);
            }

            out.x.at(x, y) = across;
            out.y_right.at(x, y) = from_right;
            out.y_left.at(x, y) = from_left;
        }
    }
    return out;
}

} // namespace seam
