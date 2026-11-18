// Sandbox for the image layer: load, convert, write something, look at it.
// Run from the project root so "assets/sample.jpg" and "out/" resolve.

#include "seam/image.hpp"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>

namespace
{
    // Dump any working image to disk so it can be eyeballed. One line per stage.
    void write_png(const seam::ImageF& image, const std::string& path)
    {
        seam::save_png(seam::to_bytes(image), path);
        std::printf("  wrote %-28s %d x %d\n", path.c_str(), image.width, image.height);
    }

    void write_png(const seam::Image& image, const std::string& path)
    {
        seam::save_png(image, path);
        std::printf("  wrote %-28s %d x %d\n", path.c_str(), image.width, image.height);
    }
} // namespace

int main(int argc, char** argv)
{
    const std::string input = argc > 1 ? argv[1] : "assets/sample.jpg";

    try
    {
        std::filesystem::create_directories("out");

        // load sample image
        const seam::Image bytes = seam::load_image(input, 3);
        std::printf("loaded %s : %d x %d, %d channels\n", input.c_str(), bytes.width,
                    bytes.height, bytes.channels);

        // Submit to double
        const seam::ImageF pixels = seam::to_float(bytes);
        std::printf("  pixel(0,0) = %.1f %.1f %.1f\n", pixels.at(0, 0, 0), pixels.at(0, 0, 1),
                    pixels.at(0, 0, 2));

        // Testo Pesto
        write_png(pixels, "out/1_float.png");
        write_png(bytes, "out/2_roundtrip.png");

        // Sample image but GRAY WOW
        const seam::ImageF gray = seam::to_float_gray(seam::load_image(input, 1));
        write_png(gray, "out/3_gray.png");

        // Funny
        seam::ImageF scratch = pixels;
        for (double& value : scratch.data)
        {
            value = (double)((int)(value / 85.0)) * 85.0;
        }
        write_png(scratch, "out/4_scratch.png");

        std::printf("\nopen the PNGs in out/ to check them\n");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "error: %s\n", error.what());
        return 1;
    }
}
