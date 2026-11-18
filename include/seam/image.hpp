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

        bool empty() const
        {
            return data.empty();
        }

        std::size_t index(int x, int y, int c = 0) const
        {
            return (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(x)) *
                       static_cast<std::size_t>(channels) +
                   static_cast<std::size_t>(c);
        }

        double& at(int x, int y, int c = 0)
        {
            return data[index(x, y, c)];
        }

        const double& at(int x, int y, int c = 0) const
        {
            return data[index(x, y, c)];
        }

        void allocate(int w, int h, int ch)
        {
            width = w;
            height = h;
            channels = ch;
            data.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) *
                            static_cast<std::size_t>(ch),
                        0.0);
        }
    };

    // convert images from stbi to float64 with the same channel order
    ImageF to_float(const Image& image);

    // convert back to images with rgba ints
    Image to_bytes(const ImageF& image);

    // to bytes but single channel
    ImageF to_float_gray(const Image& image);


}
