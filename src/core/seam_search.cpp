/**
 * @file src/core/seam_search.cpp
 * @brief Greedy and graph-cut seam search.
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
#include <stdexcept>
#include <vector>

namespace seam
{

const char* method_name(SeamMethod method)
{
    switch (method)
    {
    case SeamMethod::DynamicProgramming:
        return "dp";
    case SeamMethod::DpBackward:
        return "dp-backward";
    case SeamMethod::Greedy:
        return "greedy";
    case SeamMethod::GraphCut:
        return "graphcut";
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
    double total = 0.0;
    for (int y = 0; y < energy.height && y < static_cast<int>(seam.size()); ++y)
    {
        total += energy.at(seam[static_cast<std::size_t>(y)], y);
    }
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

    Seam seam(static_cast<std::size_t>(height), 0);

    int best = 0;
    for (int x = 1; x < width; ++x)
    {
        if (energy.at(x, 0) < energy.at(best, 0))
            best = x;
    }
    seam[0] = best;

    for (int y = 1; y < height; ++y)
    {
        const int previous = seam[static_cast<std::size_t>(y - 1)];
        const int start = std::max(previous - 1, 0);
        const int end = std::min(previous + 1, width - 1);

        int pick = start;
        for (int x = start + 1; x <= end; ++x)
        {
            if (energy.at(x, y) < energy.at(pick, y))
                pick = x;
        }
        seam[static_cast<std::size_t>(y)] = pick;
    }
    return seam;
}

// ---------------------------------------------------------------------------
// Graph cut
// ---------------------------------------------------------------------------

namespace
{

constexpr double kInfinity = std::numeric_limits<double>::infinity();

// Boykov-Kolmogorov max-flow (2004), the augmenting-path method built for
// image grids. Two search trees grow from the source and the sink; where they
// touch is an augmenting path. After each augmentation the trees are repaired
// instead of rebuilt, which is what makes it much faster than Edmonds-Karp or
// Dinic here: those redo a full search for every path.
//
// Edges are stored in pairs, so the reverse of edge e is e ^ 1, and both
// directions carry their own residual capacity.
class FlowGraph
{
public:
    explicit FlowGraph(int nodes) : head_(static_cast<std::size_t>(nodes), -1)
    {
    }

    void reserve(std::size_t edge_pairs)
    {
        to_.reserve(edge_pairs * 2);
        next_.reserve(edge_pairs * 2);
        cap_.reserve(edge_pairs * 2);
    }

    void add_edge(int from, int to, double forward, double backward)
    {
        push(from, to, forward);
        push(to, from, backward);
    }

    double max_flow(int source, int sink, double epsilon)
    {
        epsilon_ = epsilon;
        const std::size_t nodes = head_.size();
        tree_.assign(nodes, kFree);
        parent_.assign(nodes, kNone);
        active_.assign(nodes, 0);
        stamp_.assign(nodes, 0);
        clock_ = 0;
        queue_.clear();
        queue_read_ = 0;
        orphans_.clear();

        tree_[at(source)] = kSourceTree;
        tree_[at(sink)] = kSinkTree;
        parent_[at(source)] = kTerminal;
        parent_[at(sink)] = kTerminal;
        activate(source);
        activate(sink);

        double total = 0.0;
        for (;;)
        {
            const int middle = grow();
            if (middle < 0)
                break;
            ++clock_;
            total += augment(middle);
            adopt();
        }
        return total;
    }

    // Nodes reachable from source through edges with spare capacity.
    std::vector<std::uint8_t> reachable(int source) const
    {
        std::vector<std::uint8_t> seen(head_.size(), 0);
        std::vector<int> stack{source};
        seen[at(source)] = 1;
        while (!stack.empty())
        {
            const int node = stack.back();
            stack.pop_back();
            for (int e = head_[at(node)]; e != -1; e = next_[at(e)])
            {
                const int other = to_[at(e)];
                if (cap_[at(e)] > epsilon_ && !seen[at(other)])
                {
                    seen[at(other)] = 1;
                    stack.push_back(other);
                }
            }
        }
        return seen;
    }

private:
    static constexpr std::uint8_t kFree = 0;
    static constexpr std::uint8_t kSourceTree = 1;
    static constexpr std::uint8_t kSinkTree = 2;
    static constexpr int kNone = -1;
    static constexpr int kTerminal = -2;
    static constexpr int kOrphan = -3;

    std::vector<int> head_;
    std::vector<int> to_;
    std::vector<int> next_;
    std::vector<double> cap_;

    // parent_[v] is the edge from v to its parent in its tree.
    std::vector<std::uint8_t> tree_;
    std::vector<int> parent_;
    std::vector<std::uint8_t> active_;
    std::vector<int> stamp_;
    std::vector<int> queue_;
    std::size_t queue_read_ = 0;
    std::vector<int> orphans_;
    int clock_ = 0;
    double epsilon_ = 0.0;

    static std::size_t at(int index)
    {
        return static_cast<std::size_t>(index);
    }

    void push(int from, int to, double capacity)
    {
        to_.push_back(to);
        cap_.push_back(capacity);
        next_.push_back(head_[at(from)]);
        head_[at(from)] = static_cast<int>(to_.size()) - 1;
    }

    void activate(int node)
    {
        if (!active_[at(node)])
        {
            active_[at(node)] = 1;
            queue_.push_back(node);
        }
    }

    // Capacity available for flow to move *down* the tree across edge e,
    // where e leaves node `from`: source-tree flow runs from -> to,
    // sink-tree flow runs to -> from.
    double tree_capacity(std::uint8_t tree, int e) const
    {
        return tree == kSourceTree ? cap_[at(e)] : cap_[at(e ^ 1)];
    }

    // Grows both trees until they touch. Returns the edge that joins them,
    // oriented from the source tree to the sink tree, or -1 when no path is left.
    int grow()
    {
        while (queue_read_ < queue_.size())
        {
            const int node = queue_[queue_read_];
            const std::uint8_t tree = tree_[at(node)];
            if (!active_[at(node)] || tree == kFree)
            {
                active_[at(node)] = 0;
                ++queue_read_;
                continue;
            }

            for (int e = head_[at(node)]; e != -1; e = next_[at(e)])
            {
                if (tree_capacity(tree, e) <= epsilon_)
                    continue;
                const int other = to_[at(e)];
                const std::uint8_t other_tree = tree_[at(other)];
                if (other_tree == kFree)
                {
                    tree_[at(other)] = tree;
                    parent_[at(other)] = e ^ 1;
                    stamp_[at(other)] = stamp_[at(node)];
                    activate(other);
                }
                else if (other_tree != tree)
                {
                    // Leave node active: it may have more paths to offer.
                    return tree == kSourceTree ? e : (e ^ 1);
                }
            }

            active_[at(node)] = 0;
            ++queue_read_;
        }

        // Compact the queue so it does not grow without bound.
        queue_.clear();
        queue_read_ = 0;
        return -1;
    }

    double augment(int middle)
    {
        double bottleneck = cap_[at(middle)];
        for (int node = to_[at(middle ^ 1)]; parent_[at(node)] != kTerminal;)
        {
            const int e = parent_[at(node)];
            bottleneck = std::min(bottleneck, cap_[at(e ^ 1)]);
            node = to_[at(e)];
        }
        for (int node = to_[at(middle)]; parent_[at(node)] != kTerminal;)
        {
            const int e = parent_[at(node)];
            bottleneck = std::min(bottleneck, cap_[at(e)]);
            node = to_[at(e)];
        }

        cap_[at(middle)] -= bottleneck;
        cap_[at(middle ^ 1)] += bottleneck;

        // Source side: flow runs parent -> node across e ^ 1.
        for (int node = to_[at(middle ^ 1)]; parent_[at(node)] != kTerminal;)
        {
            const int e = parent_[at(node)];
            const int up = to_[at(e)];
            cap_[at(e ^ 1)] -= bottleneck;
            cap_[at(e)] += bottleneck;
            if (cap_[at(e ^ 1)] <= epsilon_)
            {
                parent_[at(node)] = kOrphan;
                orphans_.push_back(node);
            }
            node = up;
        }
        // Sink side: flow runs node -> parent across e.
        for (int node = to_[at(middle)]; parent_[at(node)] != kTerminal;)
        {
            const int e = parent_[at(node)];
            const int up = to_[at(e)];
            cap_[at(e)] -= bottleneck;
            cap_[at(e ^ 1)] += bottleneck;
            if (cap_[at(e)] <= epsilon_)
            {
                parent_[at(node)] = kOrphan;
                orphans_.push_back(node);
            }
            node = up;
        }
        return bottleneck;
    }

    // True when node still hangs from its terminal. Nodes proven this round
    // are stamped with the clock so later checks stop there.
    bool rooted(int node)
    {
        int walk = node;
        for (;;)
        {
            if (stamp_[at(walk)] == clock_)
                break;
            const int e = parent_[at(walk)];
            if (e == kTerminal)
                break;
            if (e == kOrphan || e == kNone)
                return false;
            walk = to_[at(e)];
        }
        for (walk = node; stamp_[at(walk)] != clock_;)
        {
            stamp_[at(walk)] = clock_;
            const int e = parent_[at(walk)];
            if (e == kTerminal)
                break;
            walk = to_[at(e)];
        }
        return true;
    }

    void adopt()
    {
        while (!orphans_.empty())
        {
            const int node = orphans_.back();
            orphans_.pop_back();
            const std::uint8_t tree = tree_[at(node)];

            // A new parent q must be in the same tree and able to pass flow
            // down to node: q -> node for the source tree, node -> q for the sink.
            int found = kNone;
            for (int e = head_[at(node)]; e != -1; e = next_[at(e)])
            {
                const int other = to_[at(e)];
                if (tree_[at(other)] != tree)
                    continue;
                const double spare = tree == kSourceTree ? cap_[at(e ^ 1)] : cap_[at(e)];
                if (spare > epsilon_ && rooted(other))
                {
                    found = e;
                    break;
                }
            }

            if (found != kNone)
            {
                parent_[at(node)] = found;
                stamp_[at(node)] = clock_;
                continue;
            }

            // No parent: free the node, orphan its children, and wake up
            // neighbours that could grow back into it.
            for (int e = head_[at(node)]; e != -1; e = next_[at(e)])
            {
                const int other = to_[at(e)];
                if (tree_[at(other)] != tree)
                    continue;
                const double spare = tree == kSourceTree ? cap_[at(e ^ 1)] : cap_[at(e)];
                if (spare > epsilon_)
                    activate(other);
                const int other_parent = parent_[at(other)];
                if (other_parent >= 0 && to_[at(other_parent)] == node)
                {
                    parent_[at(other)] = kOrphan;
                    orphans_.push_back(other);
                }
            }
            tree_[at(node)] = kFree;
            parent_[at(node)] = kNone;
            active_[at(node)] = 0;
        }
    }
};

} // namespace

Seam find_seam_graph_cut(const ImageF& energy)
{
    const int width = energy.width;
    const int height = energy.height;
    if (width <= 0 || height <= 0)
        return {};
    if (width == 1)
        return Seam(static_cast<std::size_t>(height), 0);

    // Shift so every capacity is non-negative. Each seam has exactly one pixel
    // per row, so this adds height * shift to all of them alike.
    double lowest = 0.0;
    for (double value : energy.data)
        lowest = std::min(lowest, value);
    const double shift = -lowest;

    double largest = 0.0;
    for (double value : energy.data)
        largest = std::max(largest, value + shift);
    const double epsilon = std::max(largest, 1.0) * 1e-12;

    const int pixels = width * height;
    const int source = pixels;
    const int sink = pixels + 1;
    auto id = [width](int x, int y) { return y * width + x; };

    FlowGraph graph(pixels + 2);
    graph.reserve(static_cast<std::size_t>(pixels) * 3 + static_cast<std::size_t>(height) * 2);

    for (int y = 0; y < height; ++y)
    {
        graph.add_edge(source, id(0, y), kInfinity, 0.0);
        graph.add_edge(id(width - 1, y), sink, kInfinity, 0.0);

        for (int x = 0; x + 1 < width; ++x)
        {
            // Cutting this edge removes pixel (x, y); the infinite way back
            // stops the cut from crossing a row twice.
            graph.add_edge(id(x, y), id(x + 1, y), energy.at(x, y) + shift, kInfinity);
        }

        for (int x = 1; x < width; ++x)
        {
            // Keeps neighbouring rows within one column of each other.
            if (y > 0)
                graph.add_edge(id(x, y), id(x - 1, y - 1), kInfinity, 0.0);
            if (y + 1 < height)
                graph.add_edge(id(x, y), id(x - 1, y + 1), kInfinity, 0.0);
        }
    }

    graph.max_flow(source, sink, epsilon);
    const std::vector<std::uint8_t> side = graph.reachable(source);

    Seam seam(static_cast<std::size_t>(height), 0);
    for (int y = 0; y < height; ++y)
    {
        int rightmost = 0;
        for (int x = 0; x < width; ++x)
        {
            if (side[static_cast<std::size_t>(id(x, y))])
                rightmost = x;
        }
        seam[static_cast<std::size_t>(y)] = std::min(rightmost, width - 2);
    }
    return seam;
}

} // namespace seam
