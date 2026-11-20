// Tests for the alternative seam finders: greedy and graph cut.
// The optimum is computed here with a plain symmetric DP, so these tests do not
// depend on cost.cpp's reference-window quirk.
// Run: ctest --test-dir build -R seam_search --output-on-failure

#include "seam/image.hpp"
#include "seam/seam_ops.hpp"
#include "seam/seam_search.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
#include <vector>

namespace
{

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                                      \
    do                                                                                   \
    {                                                                                    \
        ++g_checks;                                                                      \
        if (!(cond))                                                                     \
        {                                                                                \
            ++g_failures;                                                                \
            std::printf("  FAIL line %d: %s\n", __LINE__, #cond);                        \
        }                                                                                \
    } while (0)

seam::ImageF grid(int w, int h, const std::vector<double>& rows)
{
    seam::ImageF map;
    map.allocate(w, h, 1);
    map.data = rows;
    return map;
}

seam::ImageF random_energy(std::mt19937& rng, int w, int h, double low, double high)
{
    std::uniform_real_distribution<double> value(low, high);
    seam::ImageF map;
    map.allocate(w, h, 1);
    for (double& v : map.data)
        v = value(rng);
    return map;
}

bool is_connected_seam(const seam::Seam& seam, int width, int height)
{
    if (static_cast<int>(seam.size()) != height)
        return false;
    for (int y = 0; y < height; ++y)
    {
        if (seam[y] < 0 || seam[y] >= width)
            return false;
        if (y > 0 && std::abs(seam[y] - seam[y - 1]) > 1)
            return false;
    }
    return true;
}

// Cheapest connected seam over columns [0, columns), by plain DP.
double optimum(const seam::ImageF& energy, int columns)
{
    std::vector<double> row(static_cast<std::size_t>(columns));
    for (int x = 0; x < columns; ++x)
        row[x] = energy.at(x, 0);
    for (int y = 1; y < energy.height; ++y)
    {
        std::vector<double> next(static_cast<std::size_t>(columns));
        for (int x = 0; x < columns; ++x)
        {
            double best = row[x];
            if (x > 0)
                best = std::min(best, row[x - 1]);
            if (x + 1 < columns)
                best = std::min(best, row[x + 1]);
            next[x] = energy.at(x, y) + best;
        }
        row.swap(next);
    }
    return *std::min_element(row.begin(), row.end());
}

void test_greedy_hand_case()
{
    std::printf("greedy: follows the cheap start into an expensive column\n");
    // Top-left is cheapest, but the only cheap path runs down column 2.
    const seam::ImageF energy = grid(4, 3, {
                                               0, 2, 2, 2, //
                                               9, 9, 0, 0, //
                                               9, 9, 0, 0, //
                                           });
    const seam::Seam greedy = seam::find_seam_greedy(energy);
    CHECK(greedy == (seam::Seam{0, 0, 0}));
    CHECK(seam::seam_energy(energy, greedy) == 18.0);
    CHECK(optimum(energy, 4) == 2.0);
}

void test_greedy_random()
{
    std::printf("greedy: connected, in bounds, never cheaper than the optimum\n");
    std::mt19937 rng(7);
    std::uniform_int_distribution<int> size(1, 14);
    bool all_ok = true;
    for (int trial = 0; trial < 300; ++trial)
    {
        const int w = size(rng);
        const int h = size(rng);
        const seam::ImageF energy = random_energy(rng, w, h, 0.0, 100.0);
        const seam::Seam seam = seam::find_seam_greedy(energy);
        if (!is_connected_seam(seam, w, h) ||
            seam::seam_energy(energy, seam) < optimum(energy, w) - 1e-9)
            all_ok = false;
    }
    CHECK(all_ok);
}

void test_graph_cut_hand_case()
{
    std::printf("graph cut: finds the cheap column the greedy seam misses\n");
    const seam::ImageF energy = grid(4, 3, {
                                               0, 2, 2, 2, //
                                               9, 9, 0, 0, //
                                               9, 9, 0, 0, //
                                           });
    const seam::Seam cut = seam::find_seam_graph_cut(energy);
    CHECK(is_connected_seam(cut, 4, 3));
    CHECK(seam::seam_energy(energy, cut) == 2.0);
}

void test_graph_cut_matches_dp()
{
    std::printf("graph cut: cost equals the DP optimum on 300 random maps\n");
    std::mt19937 rng(11);
    std::uniform_int_distribution<int> size(2, 14);
    bool all_valid = true;
    bool all_optimal = true;
    for (int trial = 0; trial < 300; ++trial)
    {
        const int w = size(rng);
        const int h = size(rng);
        const seam::ImageF energy = random_energy(rng, w, h, 0.0, 100.0);
        const seam::Seam cut = seam::find_seam_graph_cut(energy);
        if (!is_connected_seam(cut, w, h) ||
            *std::max_element(cut.begin(), cut.end()) > w - 2)
            all_valid = false;
        // The sink owns the last column, so the cut ranges over 0 .. w - 2.
        if (std::fabs(seam::seam_energy(energy, cut) - optimum(energy, w - 1)) > 1e-6)
            all_optimal = false;
    }
    CHECK(all_valid);
    CHECK(all_optimal);
}

void test_graph_cut_negative_energy()
{
    std::printf("graph cut: negative energy (object masks) still gives the optimum\n");
    std::mt19937 rng(23);
    bool all_ok = true;
    for (int trial = 0; trial < 100; ++trial)
    {
        const seam::ImageF energy = random_energy(rng, 9, 7, -500.0, 100.0);
        const seam::Seam cut = seam::find_seam_graph_cut(energy);
        if (!is_connected_seam(cut, 9, 7) ||
            std::fabs(seam::seam_energy(energy, cut) - optimum(energy, 8)) > 1e-6)
            all_ok = false;
    }
    CHECK(all_ok);
}

void test_edges()
{
    std::printf("edges: one-pixel-wide images and method names\n");
    const seam::ImageF tall = grid(1, 4, {3, 1, 4, 1});
    CHECK(seam::find_seam_greedy(tall) == (seam::Seam{0, 0, 0, 0}));
    CHECK(seam::find_seam_graph_cut(tall) == (seam::Seam{0, 0, 0, 0}));

    seam::SeamMethod method = seam::SeamMethod::DynamicProgramming;
    CHECK(seam::parse_method("graphcut", method) && method == seam::SeamMethod::GraphCut);
    CHECK(seam::parse_method("greedy", method) && method == seam::SeamMethod::Greedy);
    CHECK(seam::parse_method("dp-backward", method) &&
          method == seam::SeamMethod::DpBackward);
    CHECK(!seam::parse_method("astar", method));
}

} // namespace

int main()
{
    test_greedy_hand_case();
    test_greedy_random();
    test_graph_cut_hand_case();
    test_graph_cut_matches_dp();
    test_graph_cut_negative_energy();
    test_edges();

    std::printf("\n%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
