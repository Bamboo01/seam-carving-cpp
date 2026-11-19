// Tests for the seam module: seam search, delete/insert, masks, rotation.
// Cost maps are built by hand, so this needs nothing from energy/cost.
// Run: ctest --test-dir build -R seam_ops --output-on-failure

#include "seam/image.hpp"
#include "seam/seam_ops.hpp"

#include <cstdio>
#include <cstdlib>
#include <random>
#include <stdexcept>
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

// Each sample encodes where it came from: 1000*y + 10*x + c.
seam::ImageF coded(int w, int h, int ch)
{
    seam::ImageF img;
    img.allocate(w, h, ch);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (int c = 0; c < ch; ++c)
                img.at(x, y, c) = 1000.0 * y + 10.0 * x + c;
    return img;
}

seam::ImageF random_image(std::mt19937& rng, int w, int h, int ch)
{
    std::uniform_real_distribution<double> value(0.0, 255.0);
    seam::ImageF img;
    img.allocate(w, h, ch);
    for (double& v : img.data)
        v = value(rng);
    return img;
}

bool same(const seam::ImageF& a, const seam::ImageF& b)
{
    return a.width == b.width && a.height == b.height && a.channels == b.channels &&
           a.data == b.data;
}

bool valid_seam(const seam::Seam& s, int w, int h)
{
    if (static_cast<int>(s.size()) != h)
        return false;
    for (int y = 0; y < h; ++y)
    {
        if (s[y] < 0 || s[y] >= w)
            return false;
        if (y > 0 && std::abs(s[y] - s[y - 1]) > 1)
            return false;
    }
    return true;
}

// Literal ports of the Python reference (vivianhylee/seam-carving), written
// the slow, obvious way so the fast in-place versions can be checked.
seam::ImageF reference_delete(const seam::ImageF& in, const seam::Seam& s)
{
    seam::ImageF out;
    out.allocate(in.width - 1, in.height, in.channels);
    for (int y = 0; y < in.height; ++y)
    {
        int ox = 0;
        for (int x = 0; x < in.width; ++x)
        {
            if (x == s[y])
                continue;
            for (int c = 0; c < in.channels; ++c)
                out.at(ox, y, c) = in.at(x, y, c);
            ++ox;
        }
    }
    return out;
}

// Python add_seam, statement for statement. Note the col == 0 branch: the
// averaged pixel written to index 1 is overwritten by the slice on the next line.
seam::ImageF reference_add(const seam::ImageF& in, const seam::Seam& s)
{
    seam::ImageF out;
    out.allocate(in.width + 1, in.height, in.channels);
    for (int y = 0; y < in.height; ++y)
    {
        const int col = s[y];
        for (int c = 0; c < in.channels; ++c)
        {
            if (col == 0)
            {
                const double p = in.width > 1 ? 0.5 * (in.at(0, y, c) + in.at(1, y, c))
                                              : in.at(0, y, c);
                out.at(0, y, c) = in.at(0, y, c);
                out.at(1, y, c) = p;
                for (int x = 0; x < in.width; ++x)
                    out.at(x + 1, y, c) = in.at(x, y, c);
            }
            else
            {
                const double p = 0.5 * (in.at(col - 1, y, c) + in.at(col, y, c));
                for (int x = 0; x < col; ++x)
                    out.at(x, y, c) = in.at(x, y, c);
                out.at(col, y, c) = p;
                for (int x = col; x < in.width; ++x)
                    out.at(x + 1, y, c) = in.at(x, y, c);
            }
        }
    }
    return out;
}

seam::Seam random_seam(std::mt19937& rng, int w, int h)
{
    std::uniform_int_distribution<int> start(0, w - 1);
    std::uniform_int_distribution<int> step(-1, 1);
    seam::Seam s(static_cast<std::size_t>(h));
    s[0] = start(rng);
    for (int y = 1; y < h; ++y)
    {
        const int x = s[y - 1] + step(rng);
        s[y] = x < 0 ? 0 : (x >= w ? w - 1 : x);
    }
    return s;
}

// ------------------------------------------------------------------ find_seam

void test_find_seam_hand_maps()
{
    std::printf("find_seam: hand-computed maps\n");

    // Diagonal valley.
    CHECK((seam::find_seam(grid(4, 3,
                                {1, 5, 5, 5, //
                                 5, 1, 5, 5, //
                                 5, 5, 1, 5})) == seam::Seam{0, 1, 2}));

    // Ties resolve to the lowest index, like np.argmin.
    CHECK((seam::find_seam(grid(4, 2,
                                {0, 0, 0, 0, //
                                 3, 1, 1, 3})) == seam::Seam{0, 1}));

    // Left edge: previous column 0 looks at columns 0 and 1 only.
    CHECK((seam::find_seam(grid(3, 2,
                                {9, 0, 0, //
                                 0, 9, 9})) == seam::Seam{1, 0}));

    // Right-edge quirk: the reference window stops at n - 1 (exclusive), so
    // the cheap last column is never reachable from above.
    const seam::ImageF edge = grid(4, 2,
                                   {9, 9, 9, 0, //
                                    9, 9, 9, 0});
    CHECK((seam::find_seam(edge) == seam::Seam{2, 3}));
    CHECK((seam::find_seam(edge, true) == seam::Seam{3, 3}));

    const seam::ImageF edge2 = grid(4, 2,
                                    {9, 9, 9, 0, //
                                     9, 9, 0, 9});
    CHECK((seam::find_seam(edge2) == seam::Seam{1, 2}));
    CHECK((seam::find_seam(edge2, true) == seam::Seam{3, 2}));

    // Degenerate sizes.
    CHECK((seam::find_seam(grid(1, 3, {4, 5, 6})) == seam::Seam{0, 0, 0}));
    CHECK(seam::find_seam(seam::ImageF{}).empty());
}

void test_find_seam_bounds(std::mt19937& rng)
{
    std::printf("find_seam: bounds and connectivity on random maps\n");
    std::uniform_int_distribution<int> size(1, 16);
    bool all_valid = true;
    for (int i = 0; i < 500; ++i)
    {
        const int w = size(rng);
        const int h = size(rng);
        const seam::ImageF map = random_image(rng, w, h, 1);
        all_valid = all_valid && valid_seam(seam::find_seam(map), w, h) &&
                    valid_seam(seam::find_seam(map, true), w, h);
    }
    CHECK(all_valid);
}

// --------------------------------------------------------- delete / add_seam

void test_delete_seam(std::mt19937& rng)
{
    std::printf("delete_seam: explicit case, random cases, 1-wide guard\n");

    seam::ImageF img = coded(4, 3, 3);
    seam::delete_seam(img, {0, 2, 3});
    CHECK(img.width == 3 && img.height == 3);
    CHECK(img.data.size() == 3u * 3u * 3u);
    CHECK(img.at(0, 0, 0) == 10 && img.at(1, 0, 0) == 20 && img.at(2, 0, 0) == 30);
    CHECK(img.at(0, 1, 2) == 1002 && img.at(1, 1, 2) == 1012 && img.at(2, 1, 2) == 1032);
    CHECK(img.at(0, 2, 1) == 2001 && img.at(1, 2, 1) == 2011 && img.at(2, 2, 1) == 2021);

    std::uniform_int_distribution<int> size(2, 20);
    bool all_match = true;
    for (int i = 0; i < 300; ++i)
    {
        const int w = size(rng);
        const int h = size(rng);
        const int ch = (i % 2) ? 3 : 1;
        seam::ImageF fast = random_image(rng, w, h, ch);
        const seam::Seam s = random_seam(rng, w, h);
        const seam::ImageF slow = reference_delete(fast, s);
        seam::delete_seam(fast, s);
        all_match = all_match && same(fast, slow);
    }
    CHECK(all_match);

    seam::ImageF thin = coded(1, 2, 3);
    bool threw = false;
    try
    {
        seam::delete_seam(thin, {0, 0});
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }
    CHECK(threw);
}

void test_add_seam_interior(std::mt19937& rng)
{
    std::printf("add_seam: interior columns match the reference\n");

    seam::ImageF img = coded(4, 2, 3);
    seam::add_seam(img, {2, 1});
    CHECK(img.width == 5 && img.height == 2);
    CHECK(img.data.size() == 5u * 2u * 3u);
    // Row 0, seam at 2: [x0, x1, avg(x1,x2), x2, x3]
    CHECK(img.at(0, 0, 0) == 0 && img.at(1, 0, 0) == 10 && img.at(2, 0, 0) == 15 &&
          img.at(3, 0, 0) == 20 && img.at(4, 0, 0) == 30);
    // Row 1, seam at 1: [x0, avg(x0,x1), x1, x2, x3]
    CHECK(img.at(0, 1, 1) == 1001 && img.at(1, 1, 1) == 1006 && img.at(2, 1, 1) == 1011 &&
          img.at(3, 1, 1) == 1021 && img.at(4, 1, 1) == 1031);

    std::uniform_int_distribution<int> size(2, 20);
    bool all_match = true;
    for (int i = 0; i < 300; ++i)
    {
        const int w = size(rng);
        const int h = size(rng);
        const int ch = (i % 2) ? 3 : 1;
        seam::ImageF fast = random_image(rng, w, h, ch);
        seam::Seam s = random_seam(rng, w, h);
        for (int& x : s)
            x = x == 0 ? 1 : x; // interior only
        const seam::ImageF slow = reference_add(fast, s);
        seam::add_seam(fast, s);
        all_match = all_match && same(fast, slow);
    }
    CHECK(all_match);
}

void test_add_seam_column_zero(std::mt19937& rng)
{
    std::printf("add_seam: column 0 duplicates pixel 0, like the reference\n");

    seam::ImageF row = coded(4, 1, 1); // [0, 10, 20, 30]
    seam::add_seam(row, {0});
    CHECK((row.data == std::vector<double>{0, 0, 10, 20, 30}));

    seam::ImageF mask;
    mask.allocate(3, 1, 1);
    mask.at(1, 0) = 1.0; // [0, 1, 0]
    seam::add_seam_on_mask(mask, {0});
    CHECK((mask.data == std::vector<double>{0, 0, 1, 0}));

    // Seams that hit column 0 on every other row, against the literal port.
    std::uniform_int_distribution<int> size(1, 20);
    bool all_match = true;
    for (int i = 0; i < 300; ++i)
    {
        const int w = size(rng);
        const int h = size(rng);
        seam::ImageF fast = random_image(rng, w, h, (i % 2) ? 3 : 1);
        seam::Seam s = random_seam(rng, w, h);
        for (std::size_t y = 0; y < s.size(); y += 2)
            s[y] = 0;
        const seam::ImageF slow = reference_add(fast, s);
        seam::add_seam(fast, s);
        all_match = all_match && same(fast, slow);
    }
    CHECK(all_match);
}

void test_masks()
{
    std::printf("mask delete/insert keep a 1-channel mask aligned\n");

    seam::ImageF mask;
    mask.allocate(5, 2, 1);
    mask.at(2, 0) = 1.0;
    mask.at(3, 1) = 1.0;

    seam::delete_seam_on_mask(mask, {1, 1});
    CHECK(mask.width == 4 && mask.channels == 1);
    CHECK(mask.at(1, 0) == 1.0 && mask.at(2, 1) == 1.0);

    seam::add_seam_on_mask(mask, {1, 1});
    CHECK(mask.width == 5 && mask.channels == 1);
    CHECK(mask.at(2, 0) == 1.0 && mask.at(1, 0) == 0.5);
    CHECK(mask.at(3, 1) == 1.0 && mask.at(1, 1) == 0.0);
}

void test_shift_seams()
{
    std::printf("shift_seam: indices at or right of the insert move by 2\n");

    seam::Seam s = {1, 3, 0};
    seam::shift_seam(s, {1, 2, 1});
    CHECK((s == seam::Seam{3, 5, 0}));

    std::vector<seam::Seam> many = {{0, 0}, {2, 2}, {5, 1}};
    seam::shift_seams(many, {2, 1});
    CHECK((many[0] == seam::Seam{0, 0}));
    CHECK((many[1] == seam::Seam{4, 4}));
    CHECK((many[2] == seam::Seam{7, 3}));
}

// ------------------------------------------------------------------- rotation

void test_rotation(std::mt19937& rng)
{
    std::printf("rotation: numpy orientation and round trips\n");

    // [[a b c], [d e f]] -> ccw [[c f], [b e], [a d]] ; cw [[d a], [e b], [f c]]
    const seam::ImageF img = coded(3, 2, 1);
    const double a = 0, b = 10, c = 20, d = 1000, e = 1010, f = 1020;

    const seam::ImageF ccw = seam::rotate_ccw(img);
    CHECK(ccw.width == 2 && ccw.height == 3);
    CHECK((ccw.data == std::vector<double>{c, f, b, e, a, d}));

    const seam::ImageF cw = seam::rotate_cw(img);
    CHECK(cw.width == 2 && cw.height == 3);
    CHECK((cw.data == std::vector<double>{d, a, e, b, f, c}));

    std::uniform_int_distribution<int> size(1, 24);
    bool round_trips = true;
    for (int i = 0; i < 200; ++i)
    {
        const seam::ImageF src = random_image(rng, size(rng), size(rng), (i % 2) ? 3 : 1);
        const seam::ImageF quarter = seam::rotate_ccw(src);
        const seam::ImageF full =
            seam::rotate_ccw(seam::rotate_ccw(seam::rotate_ccw(quarter)));
        round_trips = round_trips && same(seam::rotate_cw(quarter), src) &&
                      same(seam::rotate_ccw(seam::rotate_cw(src)), src) &&
                      same(full, src);
    }
    CHECK(round_trips);

    // Carving a row: rotate, drop a vertical seam, rotate back.
    seam::ImageF tall = coded(6, 5, 3);
    seam::ImageF turned = seam::rotate_ccw(tall);
    seam::delete_seam(turned, seam::Seam(static_cast<std::size_t>(turned.height), 0));
    tall = seam::rotate_cw(turned);
    CHECK(tall.width == 6 && tall.height == 4);
}

} // namespace

int main()
{
    std::mt19937 rng(1234);

    test_find_seam_hand_maps();
    test_find_seam_bounds(rng);
    test_delete_seam(rng);
    test_add_seam_interior(rng);
    test_add_seam_column_zero(rng);
    test_masks();
    test_shift_seams();
    test_rotation(rng);

    std::printf("\n%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
