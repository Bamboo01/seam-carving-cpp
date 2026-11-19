/**
 * @file tests/test_cost.cpp
 * @brief Hand-computed 3x3 unit tests for cumulative cost maps.
 *
 * @author Tea Zhi Xun
 */
#include "seam/cost.hpp"
#include "seam/image.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{

seam::ImageF make_energy_3x3(const std::vector<double>& values)
{
    seam::ImageF energy;
    energy.allocate(3, 3, 1);
    energy.data = values;
    return energy;
}

seam::ImageF make_flat_rgb_3x3(double value)
{
    seam::ImageF image;
    image.allocate(3, 3, 3);
    for (double& sample : image.data)
    {
        sample = value;
    }
    return image;
}

bool check_grid(const char* name, const seam::ImageF& actual,
                const std::vector<double>& expected)
{
    if (actual.width != 3 || actual.height != 3 || actual.channels != 1 ||
        actual.data.size() != expected.size())
    {
        std::fprintf(stderr, "FAIL %s: unexpected dimensions (%dx%dx%d)\n", name,
                     actual.width, actual.height, actual.channels);
        return false;
    }

    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            const std::size_t idx = static_cast<std::size_t>(y * 3 + x);
            const double got = actual.at(x, y);
            const double want = expected[idx];
            if (std::fabs(got - want) > 1e-9)
            {
                std::fprintf(stderr, "FAIL %s at (%d, %d): got %.6f, expected %.6f\n",
                             name, x, y, got, want);
                return false;
            }
        }
    }
    return true;
}

} // namespace

int main()
{
    // Hand-computed 3x3 energy grid:
    //   [ 10,  5,  1 ]
    //   [  2,  8,  3 ]
    //   [  4,  1,  9 ]
    const seam::ImageF energy = make_energy_3x3({
        10.0, 5.0, 1.0,
        2.0, 8.0, 3.0,
        4.0, 1.0, 9.0,
    });

    // Default matches Python reference's exclusive right-edge slice
    // min(x + 2, width - 1): column 2 only looks at column 1 above it.
    //   row 0: [ 10,  5,  1 ]
    //   row 1: [  7, 13,  8 ]
    //   row 2: [ 11,  8, 22 ]
    const seam::ImageF backward_ref = seam::cumulative_cost_backward(energy);
    if (!check_grid("backward_reference_window", backward_ref,
                    {10.0, 5.0, 1.0, 7.0, 13.0, 8.0, 11.0, 8.0, 22.0}))
    {
        return 1;
    }

    // With fix_right_edge = true, the window is symmetric [max(x-1,0), min(x+2,width)):
    //   row 0: [ 10,  5,  1 ]
    //   row 1: [  7,  9,  4 ]
    //   row 2: [ 11,  5, 13 ]
    seam::CostOptions fixed_opts;
    fixed_opts.fix_right_edge = true;
    const seam::ImageF backward_fixed =
        seam::cumulative_cost_backward(energy, fixed_opts);
    if (!check_grid("backward_fixed_right_edge", backward_fixed,
                    {10.0, 5.0, 1.0, 7.0, 9.0, 4.0, 11.0, 5.0, 13.0}))
    {
        return 1;
    }

    // On a flat image all neighbour difference terms are 0, so forward DP matches
    // the symmetric 3-neighbour accumulation regardless of energy linkage.
    // TODO: Add a non-flat forward-energy 3x3 test once energy.cpp is merged.
    const seam::ImageF flat = make_flat_rgb_3x3(128.0);
    const seam::ImageF forward_flat = seam::cumulative_cost_forward(flat, energy);
    if (!check_grid("forward_flat_image", forward_flat,
                    {10.0, 5.0, 1.0, 7.0, 9.0, 4.0, 11.0, 5.0, 13.0}))
    {
        return 1;
    }

    // Empty input should return an empty map without crashing.
    const seam::ImageF empty;
    if (!seam::cumulative_cost_backward(empty).empty() ||
        !seam::cumulative_cost_forward(empty, empty).empty())
    {
        std::fprintf(stderr, "FAIL empty_input: expected empty output\n");
        return 1;
    }

    std::printf("all cost tests passed\n");
    return 0;
}
