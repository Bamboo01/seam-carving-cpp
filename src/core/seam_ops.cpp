/**
 * @file src/core/seam_ops.cpp
 * @brief Seam search, mutation, and 90-degree rotation.
 *
 * Seam Carving in C++ - content-aware image resizing.
 * Ported from https://github.com/vivianhylee/seam-carving
 *
 * @author Sim Wei Siang
 */
#include "seam/seam_ops.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace seam
{
namespace
{

// np.argmin returns the first minimum, so use a strict comparison.
int argmin(const ImageF& map, int y, int start, int end)
{
    int best = start;
    double best_value = map.at(start, y);
    for (int x = start + 1; x < end; ++x)
    {
        if (map.at(x, y) < best_value)
        {
            best_value = map.at(x, y);
            best = x;
        }
    }
    return best;
}

} // namespace

Seam find_seam(const ImageF& cumulative, bool fix_right_edge)
{
    const int width = cumulative.width;
    const int height = cumulative.height;
    if (width <= 0 || height <= 0)
        return {};

    Seam seam(static_cast<std::size_t>(height), 0);
    seam[height - 1] = argmin(cumulative, height - 1, 0, width);

    for (int y = height - 2; y >= 0; --y)
    {
        const int previous = seam[y + 1];

        if (previous == 0)
        {
            seam[y] = argmin(cumulative, y, 0, std::min(2, width));
            continue;
        }

        const int start = previous - 1;
        const int end = fix_right_edge ? std::min(previous + 2, width)
                                       : std::min(previous + 2, width - 1);
        seam[y] = argmin(cumulative, y, start, std::max(end, start + 1));
    }
    return seam;
}

void delete_seam(ImageF& image, const Seam& seam)
{
    const int width = image.width;
    const int height = image.height;
    const int channels = image.channels;
    if (width <= 1)
        throw std::runtime_error("delete_seam: image is already one pixel wide");

    const auto row_in = static_cast<std::size_t>(width) * channels;
    const auto row_out = static_cast<std::size_t>(width - 1) * channels;
    double* data = image.data.data();

    // Compact rows forward: for y > 0 the destination sits behind the source,
    // and each row's destination never reaches the next row's source, so a
    // single left-to-right pass is safe without a scratch buffer.
    for (int y = 0; y < height; ++y)
    {
        const int col = seam[static_cast<std::size_t>(y)];
        const double* src = data + static_cast<std::size_t>(y) * row_in;
        double* dst = data + static_cast<std::size_t>(y) * row_out;

        if (col > 0)
        {
            std::memmove(dst, src,
                         static_cast<std::size_t>(col) * channels * sizeof(double));
        }
        if (col + 1 < width)
        {
            std::memmove(dst + static_cast<std::size_t>(col) * channels,
                         src + static_cast<std::size_t>(col + 1) * channels,
                         static_cast<std::size_t>(width - col - 1) * channels *
                             sizeof(double));
        }
    }

    image.width = width - 1;
    image.data.resize(static_cast<std::size_t>(height) * row_out);
}

void add_seam(ImageF& image, const Seam& seam)
{
    const int width = image.width;
    const int height = image.height;
    const int channels = image.channels;

    const auto row_in = static_cast<std::size_t>(width) * channels;
    const auto row_out = static_cast<std::size_t>(width + 1) * channels;

    std::vector<double> scratch(row_out);
    image.data.resize(static_cast<std::size_t>(height) * row_out);
    double* data = image.data.data();

    // Rows grow by one pixel, so each destination sits ahead of its source:
    // walk backwards or row y would overwrite the not-yet-read row y + 1.
    for (int y = height - 1; y >= 0; --y)
    {
        const int col = seam[static_cast<std::size_t>(y)];
        const double* src = data + static_cast<std::size_t>(y) * row_in;

        // Upstream averages the two pixels the seam sits between. Column 0 has no
        // left neighbour: the reference writes the average of pixels 0 and 1 into
        // index 1, then its next line (output[row, col + 1:] = image[row, col:])
        // overwrites it, so what it really does is duplicate pixel 0. Match that:
        // keep pixel 0 and insert a copy of it at index 1. Every other column
        // inserts before the seam pixel.
        const int insert_index = (col == 0) ? 1 : col;
        const int head_count = (col == 0) ? 1 : col;

        const int left = (col == 0) ? 0 : col - 1;
        const int right = (col == 0) ? 0 : col;

        for (int c = 0; c < channels; ++c)
        {
            const double average =
                0.5 * (src[static_cast<std::size_t>(left) * channels + c] +
                       src[static_cast<std::size_t>(right) * channels + c]);
            scratch[static_cast<std::size_t>(insert_index) * channels + c] = average;
        }

        if (head_count > 0)
        {
            std::memcpy(scratch.data(), src,
                        static_cast<std::size_t>(head_count) * channels * sizeof(double));
        }
        if (head_count < width)
        {
            std::memcpy(
                scratch.data() + static_cast<std::size_t>(insert_index + 1) * channels,
                src + static_cast<std::size_t>(head_count) * channels,
                static_cast<std::size_t>(width - head_count) * channels * sizeof(double));
        }
        std::memcpy(data + static_cast<std::size_t>(y) * row_out, scratch.data(),
                    row_out * sizeof(double));
    }

    image.width = width + 1;
}

void delete_seam_on_mask(ImageF& mask, const Seam& seam)
{
    delete_seam(mask, seam);
}

void add_seam_on_mask(ImageF& mask, const Seam& seam)
{
    add_seam(mask, seam);
}

void shift_seam(Seam& seam, const Seam& inserted)
{
    for (std::size_t y = 0; y < seam.size(); ++y)
    {
        if (seam[y] >= inserted[y])
        {
            seam[y] += 2;
        }
    }
}

void shift_seams(std::vector<Seam>& seams, const Seam& inserted)
{
    for (Seam& seam : seams)
    {
        shift_seam(seam, inserted);
    }
}

ImageF rotate_ccw(const ImageF& image)
{
    ImageF out;
    out.allocate(image.height, image.width, image.channels);

    for (int y = 0; y < out.height; ++y)
    {
        for (int x = 0; x < out.width; ++x)
        {
            for (int c = 0; c < image.channels; ++c)
            {
                // out(x, y) = image(width - 1 - y, x)
                out.at(x, y, c) = image.at(image.width - 1 - y, x, c);
            }
        }
    }
    return out;
}

ImageF rotate_cw(const ImageF& image)
{
    ImageF out;
    out.allocate(image.height, image.width, image.channels);

    for (int y = 0; y < out.height; ++y)
    {
        for (int x = 0; x < out.width; ++x)
        {
            for (int c = 0; c < image.channels; ++c)
            {
                // out(x, y) = image(y, height - 1 - x)
                out.at(x, y, c) = image.at(y, image.height - 1 - x, c);
            }
        }
    }
    return out;
}

} // namespace seam
