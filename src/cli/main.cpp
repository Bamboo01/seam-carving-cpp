/**
 * @file src/cli/main.cpp
 * @brief Command-line front end: argument parsing and batch runs.
 *
 * Seam Carving in C++ - content-aware image resizing.
 * Ported from https://github.com/vivianhylee/seam-carving
 *
 * @author Teo Pek Lin
 */

#include "seam/carver.hpp"
#include "seam/energy.hpp"
#include "seam/image.hpp"
#include "seam/seam_search.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace
{
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
            "      --method NAME   seam finder: dp (default), dp-backward, greedy, graphcut\n"
            "      --seams FILE    also write the source with every carved seam in red\n"
            "      --compare DIR   resize with every method, write results, seams and\n"
            "                      DIR/summary.csv, and print a timing table\n"
            "  -h, --help          this message\n"
            "\n"
            "examples:\n"
            "  %s assets/sample.jpg -w 400 --height 300 -o out/smaller.png\n"
            "  %s assets/sample.jpg -w 640 --height 480 -o out/wider.png\n"
            "  %s assets/sample.jpg --remove out/mask.png -o out/clean.png\n"
            "  %s assets/sample.jpg -w 400 --method graphcut --seams out/seams.png\n"
            "  %s assets/sample.jpg -w 400 --compare out/compare\n",
            argv0, argv0, argv0, argv0, argv0, argv0);
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

    void save_float(const seam::ImageF& image, const std::string& path)
    {
        ensure_parent_directory(path);
        seam::save_image(seam::to_bytes(image), path);
    }

    // Runs one resize per method on the same input and reports time, the energy
    // of the first seam each method picks on the untouched image, and the total
    // energy of everything it carved.
    int run_compare(const seam::ImageF& source, const seam::CostOptions& options, int width,
                    int height, const std::string& directory)
    {
        const seam::SeamMethod methods[] = {
            seam::SeamMethod::DynamicProgramming,
            seam::SeamMethod::DpBackward,
            seam::SeamMethod::Greedy,
            seam::SeamMethod::GraphCut,
        };

        const seam::ImageF energy = seam::energy_map(source);
        std::filesystem::create_directories(directory);

        const std::string csv_path = directory + "/summary.csv";
        std::FILE* csv = std::fopen(csv_path.c_str(), "w");
        if (csv == nullptr)
        {
            std::fprintf(stderr, "error: cannot write %s\n", csv_path.c_str());
            return 1;
        }
        std::fprintf(csv, "method,width,height,seams,time_ms,ms_per_seam,first_seam_energy,"
                          "total_seam_energy\n");

        std::printf("\n%-12s %8s %10s %10s %14s %16s\n", "method", "seams", "time ms",
                    "ms/seam", "first seam E", "total seam E");
        for (const seam::SeamMethod method : methods)
        {
            seam::Carver carver(source, options);
            carver.set_method(method);

            const double first = seam::seam_energy(energy, carver.next_seam());
            carver.resize_to(width, height);

            const char* name = seam::method_name(method);
            save_float(carver.image(), directory + "/" + name + ".png");
            save_float(carver.seam_overlay(), directory + "/" + name + "_seams.png");

            const int seams = carver.seams_carved();
            const double per_seam = seams > 0 ? carver.last_elapsed_ms() / seams : 0.0;
            std::printf("%-12s %8d %10.1f %10.3f %14.1f %16.1f\n", name, seams,
                        carver.last_elapsed_ms(), per_seam, first, carver.total_seam_energy());
            std::fprintf(csv, "%s,%d,%d,%d,%.3f,%.4f,%.3f,%.3f\n", name, carver.image().width,
                         carver.image().height, seams, carver.last_elapsed_ms(), per_seam, first,
                         carver.total_seam_energy());
        }
        std::fclose(csv);
        std::printf("\nwrote %s/{method}.png, {method}_seams.png and summary.csv\n",
                    directory.c_str());
        return 0;
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

    const std::string input = argv[1];
    std::string output = "out/result.png";
    std::string protect_mask;
    std::string object_mask;
    std::string energy_path;
    int target_width = 0;
    int target_height = 0;
    bool fix_edges = false;
    seam::SeamMethod method = seam::SeamMethod::DynamicProgramming;
    std::string seams_path;
    std::string compare_dir;

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
        else if (argument == "--method")
        {
            if (!has_value || !seam::parse_method(argv[i + 1], method))
            {
                std::fprintf(stderr,
                             "error: --method expects dp, dp-backward, greedy or graphcut\n");
                return 2;
            }
            ++i;
        }
        else if (argument == "--seams")
        {
            if (!has_value)
            {
                std::fprintf(stderr, "error: --seams needs a path\n");
                return 2;
            }
            seams_path = argv[++i];
        }
        else if (argument == "--compare")
        {
            if (!has_value)
            {
                std::fprintf(stderr, "error: --compare needs a directory\n");
                return 2;
            }
            compare_dir = argv[++i];
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
        const seam::Image loaded = seam::load_image(input, 3);
        std::printf("input   %s  (%dx%d)\n", input.c_str(), loaded.width, loaded.height);

        seam::CostOptions options;
        options.fix_right_edge = fix_edges;

        if (!compare_dir.empty())
        {
            if (!protect_mask.empty() || !object_mask.empty())
            {
                std::fprintf(stderr, "error: --compare runs plain resizes; drop the masks\n");
                return 2;
            }
            const int width = target_width > 0 ? target_width : loaded.width;
            const int height = target_height > 0 ? target_height : loaded.height;
            return run_compare(seam::to_float(loaded), options, width, height, compare_dir);
        }

        seam::Carver carver(seam::to_float(loaded), options);
        carver.set_method(method);
        std::printf("method  %s\n", seam::method_name(method));

        if (!object_mask.empty())
        {
            carver.set_object_mask(seam::to_float_gray(seam::load_image(object_mask, 1)));
            carver.remove_object();
            std::printf("removed object using %s\n", object_mask.c_str());
        }
        else
        {
            if (!protect_mask.empty())
            {
                carver.set_protect_mask(
                    seam::to_float_gray(seam::load_image(protect_mask, 1)));
            }

            const int width = target_width > 0 ? target_width : loaded.width;
            const int height = target_height > 0 ? target_height : loaded.height;
            carver.resize_to(width, height);
        }

        const seam::ImageF& result = carver.image();
        std::printf("output  %s  (%dx%d)  %.1f ms\n", output.c_str(), result.width,
                    result.height, carver.last_elapsed_ms());

        ensure_parent_directory(output);
        seam::save_image(seam::to_bytes(result), output);

        if (!seams_path.empty())
        {
            save_float(carver.seam_overlay(), seams_path);
            std::printf("seams   %s  (%d carved)\n", seams_path.c_str(), carver.seams_carved());
        }

        if (!energy_path.empty())
        {
            ensure_parent_directory(energy_path);
            save_energy_preview(carver.energy(), energy_path);
            std::printf("energy  %s\n", energy_path.c_str());
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "error: %s\n", error.what());
        return 1;
    }
}
