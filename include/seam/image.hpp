#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace seam
{

// Plain interleaved 8-bit image. This is the unit that crosses the
// file-format boundary; the carving core works on float64 buffers instead.
struct Image
{
    int width = 0;
    int height = 0;
    int channels = 0;                 // 1 = gray, 3 = RGB, 4 = RGBA
    std::vector<std::uint8_t> pixels; // row-major, interleaved

    bool empty() const
    {
        return pixels.empty();
    }

    std::size_t index(int x, int y, int c = 0) const
    {
        return (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                static_cast<std::size_t>(x)) *
                   static_cast<std::size_t>(channels) +
               static_cast<std::size_t>(c);
    }
};

// Loads an image, converted to `desired_channels` components (stb resamples
// gray <-> RGB as needed). Throws std::runtime_error on failure.
Image load_image(const std::string& path, int desired_channels = 3);

// Writers. Format is chosen by extension for save_image(); the explicit
// helpers are for when the extension lies.
void save_png(const Image& image, const std::string& path);
void save_jpg(const Image& image, const std::string& path, int quality = 90);
void save_image(const Image& image, const std::string& path);

} // namespace seam
