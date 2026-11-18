#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace seam
{

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

        // Converts 2D indices to 1D
        std::size_t index(int x, int y, int c = 0) const
        {
            // Shut the fucking compiler up
            return (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(x)) *
                       static_cast<std::size_t>(channels) +
                   static_cast<std::size_t>(c);
        }
    };

    // Loads in 3 channels by default
    Image load_image(const std::string& path, int desired_channels = 3);

    // Self-explanatory
    void save_png(const Image& image, const std::string& path);
    void save_jpg(const Image& image, const std::string& path, int quality = 90);
    void save_image(const Image& image, const std::string& path);

    // The original python ref keeps everything in float64
    struct ImageF
    {
        int width = 0;
        int height = 0;
        int channels = 3;
        std::vector<double> data;
    };

}
