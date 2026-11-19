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

    void ensure_parent_directory(const std::string& path)
    {
        const auto parent = std::filesystem::path(path).parent_path();
        if (!parent.empty())
        {
            std::filesystem::create_directories(parent);
        }
    }

    void print_usage(const char* argv0)
    {
        std::printf(
            "seamcarve - content-aware image resizing (seam carving)\n"
            "\n"
            "usage: %s <input> [options]\n"
            "\n"
            "  -o, --out FILE      output image (default: out/result.png)\n"
            "  -w, --width N       target width  (default: unchanged)\n"
            "      --height N      target height (default: unchanged)\n"
            "      --protect FILE  mask of pixels to keep intact while resizing\n"
            "      --remove FILE   mask of an object to delete, then restore size\n"
            "      --energy FILE   also write the energy map as a PNG\n"
            "      --fix-edges     repair the upstream right-edge window bug\n"
            "  -h, --help          this message\n"
            "\n"
            "examples:\n"
            "  %s assets/sample.jpg -w 400 --height 300 -o out/smaller.png\n"
            "  %s assets/sample.jpg -w 700 -o out/wider.png\n"
            "  %s assets/sample.jpg --remove out/mask.png -o out/clean.png\n",
            argv0, argv0, argv0, argv0);
    }

    int parse_int(const char* text, const char* what)
    {
        char* end = nullptr;
        const long value = std::strtol(text, &end, 10);
        if (end == text || *end != '\0' || value <= 0)
        {
            std::fprintf(stderr, "error: %s expects a positive integer, got '%s'\n", what,
                         text);
            std::exit(2);
        }
        return static_cast<int>(value);
    }

    // Energy is unbounded; stretch it into 0..255 so it is visible as an image.
    void save_energy_preview(const seam::ImageF& energy, const std::string& path)
    {
        double maximum = 0.0;
        for (double value : energy.data)
        {
            maximum = std::max(maximum, value);
        }
        if (maximum <= 0.0)
        {
            maximum = 1.0;
        }

        seam::Image preview;
        preview.width = energy.width;
        preview.height = energy.height;
        preview.channels = 1;
        preview.pixels.resize(energy.data.size());

        for (std::size_t i = 0; i < energy.data.size(); ++i)
        {
            const double scaled = 255.0 * energy.data[i] / maximum;
            preview.pixels[i] = static_cast<std::uint8_t>(std::clamp(scaled, 0.0, 255.0));
        }
        seam::save_png(preview, path);
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc > 1 && (std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0))
    {
        print_usage(argv[0]);
        return 0;
    }
    if (argc < 2)
    {
        print_usage(argv[0]);
        return 2;
    }

    const std::string input = argc > 1 ? argv[1] : "assets/sample.jpg";
    std::string output = "out/result.png";
    std::string protect_mask;
    std::string object_mask;
    std::string energy_path;
    int target_width = 0;
    int target_height = 0;
    bool fix_edges = false;

    for (int i = 2; i < argc; ++i)
    {
        const std::string argument = argv[i];
        const bool has_value = (i + 1 < argc);

        if (argument == "-o" || argument == "--out")
        {
            if (!has_value)
            {
                std::fprintf(stderr, "error: %s needs a path\n", argument.c_str());
                return 2;
            }
            output = argv[++i];
        }
        else if (argument == "-w" || argument == "--width")
        {
            if (!has_value)
            {
                std::fprintf(stderr, "error: %s needs a number\n", argument.c_str());
                return 2;
            }
            target_width = parse_int(argv[++i], "--width");
        }
        else if (argument == "--height")
        {
            if (!has_value)
            {
                std::fprintf(stderr, "error: --height needs a number\n");
                return 2;
            }
            target_height = parse_int(argv[++i], "--height");
        }
        else if (argument == "--protect")
        {
            if (!has_value)
            {
                std::fprintf(stderr, "error: --protect needs a path\n");
                return 2;
            }
            protect_mask = argv[++i];
        }
        else if (argument == "--remove")
        {
            if (!has_value)
            {
                std::fprintf(stderr, "error: --remove needs a path\n");
                return 2;
            }
            object_mask = argv[++i];
        }
        else if (argument == "--energy")
        {
            if (!has_value)
            {
                std::fprintf(stderr, "error: --energy needs a path\n");
                return 2;
            }
            energy_path = argv[++i];
        }
        else if (argument == "--fix-edges")
        {
            fix_edges = true;
        }
        else
        {
            std::fprintf(stderr, "error: unknown option '%s'\n", argument.c_str());
            return 2;
        }
    }

    if (!protect_mask.empty() && !object_mask.empty())
    {
        std::fprintf(stderr, "error: --protect and --remove are mutually exclusive\n");
        return 2;
    }

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
