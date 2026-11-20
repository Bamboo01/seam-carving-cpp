/**
 * @file include/seam/seam_search.hpp
 * @brief Alternative seam finders: greedy and graph cut (min-cut / max-flow).
 *
 * Seam Carving in C++ - content-aware image resizing.
 * Ported from https://github.com/vivianhylee/seam-carving
 *
 * @author Sherwyn Low
 */
#pragma once

#include "seam/image.hpp"
#include "seam/seam_ops.hpp"

#include <string>

namespace seam
{

// How the Carver picks each seam.
//
//   DynamicProgramming  forward-energy DP, exactly as the Python reference
//   DpBackward          plain-energy DP; the optimum the other two are judged by
//   Greedy              top-row minimum, then the cheapest of the three below
//   GraphCut            minimum s-t cut on a pixel graph, solved with max-flow
enum class SeamMethod
{
    DynamicProgramming,
    DpBackward,
    Greedy,
    GraphCut
};

const char* method_name(SeamMethod method);

// Accepts "dp", "dp-backward", "greedy", "graphcut". Returns false otherwise.
bool parse_method(const std::string& text, SeamMethod& method);

// Greedy: start at the lowest-energy pixel of the top row, then in each row
// below take the lowest-energy pixel among the three touching the previous
// one. Ties go to the lowest index. O(height) per seam after the energy map,
// but blind to anything it has not reached yet.
Seam find_seam_greedy(const ImageF& energy);

// Graph cut: one node per pixel; source -> first column and last column ->
// sink with infinite capacity; (y, x) -> (y, x + 1) costs energy(y, x) with an
// infinite edge back; infinite diagonals (y, x) -> (y +- 1, x - 1). Every
// finite cut crosses each row once and forms a connected seam, so the minimum
// cut is the cheapest seam. The seam pixel in each row is the rightmost pixel
// still reachable from the source in the residual graph.
//
// Max-flow uses Edmonds-Karp. Negative energies (object masks) are
// shifted up by one constant first, which adds the same amount to every seam
// and leaves the minimum where it was. The last column can never be cut, so
// the seam always lies in columns 0 .. width - 2.
Seam find_seam_graph_cut(const ImageF& energy);

// Sum of energy along a seam: the quantity all three methods try to minimise.
double seam_energy(const ImageF& energy, const Seam& seam);

} // namespace seam
