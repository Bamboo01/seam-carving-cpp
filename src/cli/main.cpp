// Stub entry point: proves the image I/O path works end to end.
// The carving pipeline lands in src/core/ next.

#include "seam/image.hpp"

#include <cstdio>
#include <exception>
#include <string>

namespace
{
    void print_usage(const char* argv0)
    {
        std::printf("usage: %s [input-image] [output-image]\n"
                    "\n"
                    "  input-image    defaults to assets/sample.jpg\n"
                    "  output-image   optional; writes a round-tripped copy\n"
                    "\n"
                    "Stub build: loads the image and reports it. No carving yet.\n",
                    argv0);
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help"))
    {
        print_usage(argv[0]);
        return 0;
    }

    const std::string input = argc > 1 ? argv[1] : "assets/sample.jpg";

    try
    {
        const seam::Image image = seam::load_image(input, 3);
        std::printf("loaded  %s\n", input.c_str());
        std::printf("        %d x %d, %d channels, %zu bytes\n", image.width,
                    image.height, image.channels, image.pixels.size());

        if (argc > 2)
        {
            const std::string output = argv[2];
            seam::save_image(image, output);
            std::printf("wrote   %s\n", output.c_str());
        }

        // TODO: energy map (Scharr, reflect-101 borders)
        // TODO: cumulative cost (forward + backward)
        // TODO: seam search, removal, insertion
        // TODO: mask-aware protect / object removal
        std::printf("\ncarving core not implemented yet\n");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "error: %s\n", error.what());
        return 1;
    }
}
