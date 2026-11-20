/**
 * @file src/core/seam_search.cpp
 * @brief Greedy and graph-cut seam search.
 *
 * I don't know what the point of this assignment is other than
 * to add more pointless work to my already crappy GAM300 ridden
 * year 3 life in SIT DigiPen LOL
 *
 * Seam Carving in C++ - content-aware image resizing.
 * Ported from https://github.com/vivianhylee/seam-carving
 *
 * @author Sherwyn Low
 */
#include "seam/seam_search.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace seam
{
    const char* method_name(SeamMethod method)
    {
        switch (method)
        {
        case SeamMethod::DynamicProgramming: return "dp";
        case SeamMethod::DpBackward:         return "dp-backward";
        case SeamMethod::Greedy:             return "greedy";
        case SeamMethod::GraphCut:           return "graphcut";
        }
        return "unknown";
    }

    bool parse_method(const std::string& text, SeamMethod& method)
    {
        if (text == "dp")
            method = SeamMethod::DynamicProgramming;
        else if (text == "dp-backward")
            method = SeamMethod::DpBackward;
        else if (text == "greedy")
            method = SeamMethod::Greedy;
        else if (text == "graphcut" || text == "graph-cut")
            method = SeamMethod::GraphCut;
        else
            return false;
        return true;
    }

    double seam_energy(const ImageF& energy, const Seam& seam)
    {
        const int rows = std::min(energy.height, static_cast<int>(seam.size()));
        double total = 0.0;
        for (int y = 0; y < rows; ++y)
            total += energy.at(seam[y], y);
        return total;
    }

    // ---------------------------------------------------------------------------
    // Greedy
    // ---------------------------------------------------------------------------

    Seam find_seam_greedy(const ImageF& energy)
    {
        const int width = energy.width;
        const int height = energy.height;
        if (width <= 0 || height <= 0)
            return {};

        Seam seam(height, 0);

        // Cheapest pixel in the top row.
        for (int x = 1; x < width; ++x)
        {
            if (energy.at(x, 0) < energy.at(seam[0], 0))
                seam[0] = x;
        }

        //  cheapest of the three pixels below
        for (int y = 1; y < height; ++y)
        {
            const int start = std::max(seam[y - 1] - 1, 0);
            const int end = std::min(seam[y - 1] + 1, width - 1);

            seam[y] = start;
            for (int x = start + 1; x <= end; ++x)
                if (energy.at(x, y) < energy.at(seam[y], y))
                    seam[y] = x;
        }

        return seam;
    }

    // ---------------------------------------------------------------------------
    // Graph cut
    // ---------------------------------------------------------------------------

    namespace
    {

        constexpr double kInfinity = std::numeric_limits<double>::infinity();

        // Max-flow by Edmonds-Karp is a breadth-first search for a path from source to sink
        // We basically push as much flow as its narrowest edge allows, and repeat until no such path is left.
        //
        // The graph is an adjacency list kept in flat arrays:
        //   head_[v]  first edge leaving node v (-1 if none)
        //   next_[e]  next edge leaving the same node as e
        //   to_[e]    node that edge e points at
        //   cap_[e]   capacity edge e has left
        //
        // Edges are added in pairs (forward, then reverse), so the reverse of edge e is e ^ 1.

        class FlowGraph
        {
        public:
            explicit FlowGraph(int nodes) : head_(nodes, -1) {}

            void add_edge(int from, int to, double forward, double backward)
            {
                push(from, to, forward);
                push(to, from, backward);
            }

            double max_flow(int source, int sink, double epsilon)
            {
                double total = 0.0;
                for (;;)
                {
                    // parent[v] is the edge the search used to reach v
                    const std::vector<int> parent = search(source, sink, epsilon);
                    if (parent[sink] == -1)
                        return total; // no path with room left: the flow is maximal

                    // Walk back from the sink to find the narrowest edge on the path...
                    double bottleneck = kInfinity;
                    for (int node = sink; node != source; node = to_[parent[node] ^ 1])
                        bottleneck = std::min(bottleneck, cap_[parent[node]]);

                    // ...then push that much flow along every edge of the path.
                    for (int node = sink; node != source; node = to_[parent[node] ^ 1])
                    {
                        cap_[parent[node]] -= bottleneck;
                        cap_[parent[node] ^ 1] += bottleneck;
                    }
                    total += bottleneck;
                }
            }

            // After max_flow the nodes the source can still reach through edges with spare capacity
            std::vector<std::uint8_t> reachable(int source, double epsilon) const
            {
                const std::vector<int> parent = search(source, -1, epsilon);
                std::vector<std::uint8_t> side(parent.size(), 0);
                for (std::size_t v = 0; v < parent.size(); ++v)
                    side[v] = parent[v] != -1;
                return side;
            }

        private:
            std::vector<int> head_;
            std::vector<int> next_;
            std::vector<int> to_;
            std::vector<double> cap_;

            void push(int from, int to, double capacity)
            {
                // index of the new edge
                const int e = static_cast<int>(to_.size());
                to_.push_back(to);
                cap_.push_back(capacity);
                next_.push_back(head_[from]);
                head_[from] = e;
            }

            // Plain BFS from source over edges with capacity left.
            // We stops early once `sink` is reached and pass -1 to explore everything reachable.
            std::vector<int> search(int source, int sink, double epsilon) const
            {
                constexpr int kStart = -2; // marks the source as reached
                std::vector<int> parent(head_.size(), -1);
                std::vector<int> queue{source};
                parent[source] = kStart;

                for (std::size_t read = 0; read < queue.size(); ++read)
                {
                    for (int e = head_[queue[read]]; e != -1; e = next_[e])
                    {
                        const int other = to_[e];
                        if (cap_[e] <= epsilon || parent[other] != -1)
                            continue; // full, or already visited

                        parent[other] = e;
                        if (other == sink)
                            return parent;
                        queue.push_back(other);
                    }
                }
                return parent;
            }
        };

        inline int Index2DTo1D(int x, int y, int width)
        {
            return y * width + x;
        }

    }

    Seam find_seam_graph_cut(const ImageF& energy)
    {
        const int width = energy.width;
        const int height = energy.height;
        if (width <= 0 || height <= 0)
            return {};
        if (width == 1)
            return Seam(height, 0);

        // Shift so every capacity is non-negative. Each seam has exactly one pixel
        // per row, so this adds height * shift to all of them alike.
        double lowest = 0.0;
        double largest = 0.0;
        for (double value : energy.data)
        {
            lowest = std::min(lowest, value);
        }

        const double shift = -lowest;

        for (double value : energy.data)
        {
            largest = std::max(largest, value + shift);
        }

        const double epsilon = std::max(largest, 1.0) * 1e-12;

        const int pixels = width * height;
        const int source = pixels;
        const int sink = pixels + 1;

        FlowGraph graph(pixels + 2);
        for (int y = 0; y < height; ++y)
        {
            graph.add_edge(source, Index2DTo1D(0, y, width), kInfinity, 0.0);
            graph.add_edge(Index2DTo1D(width - 1, y, width), sink, kInfinity, 0.0);

            // Cutting this edge removes pixel (x, y); the infinite way back
            // stops the cut from crossing a row twice.
            for (int x = 0; x + 1 < width; ++x)
                graph.add_edge(Index2DTo1D(x, y, width), Index2DTo1D(x + 1, y, width), energy.at(x, y) + shift, kInfinity);

            // Keeps neighbouring rows within one column of each other.
            for (int x = 1; x < width; ++x)
            {
                if (y > 0)
                    graph.add_edge(Index2DTo1D(x, y, width), Index2DTo1D(x - 1, y - 1, width), kInfinity, 0.0);
                if (y + 1 < height)
                    graph.add_edge(Index2DTo1D(x, y, width), Index2DTo1D(x - 1, y + 1, width), kInfinity, 0.0);
            }
        }

        graph.max_flow(source, sink, epsilon);
        const std::vector<std::uint8_t> side = graph.reachable(source, epsilon);

        // In each row, the seam pixel is the rightmost pixel still on the source side.
        Seam seam(height, 0);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
                if (side[Index2DTo1D(x, y, width)])
                    seam[y] = x;
            seam[y] = std::min(seam[y], width - 2);
        }
        return seam;
    }

} // namespace seam
