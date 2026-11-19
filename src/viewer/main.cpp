/**
 * @file src/viewer/main.cpp
 * @brief raylib entry point: window, input, drag and drop.
 *
 * Seam Carving in C++ - content-aware image resizing.
 * Ported from https://github.com/vivianhylee/seam-carving
 *
 * @author Loh Jun Wei
 */
#include "raylib.h"
#include "rlgl.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{

std::string argument_value(int argc, char** argv, const char* flag)
{
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (std::strcmp(argv[i], flag) == 0)
        {
            return argv[i + 1];
        }
    }
    return {};
}

// "--carve 400x300" drives one resize without a mouse, so the viewer can be
// exercised (and screenshotted) in a script.
bool parse_size(const std::string& text, int& width, int& height)
{
    const auto x = text.find('x');
    if (x == std::string::npos)
    {
        return false;
    }
    width = std::atoi(text.substr(0, x).c_str());
    height = std::atoi(text.substr(x + 1).c_str());
    return width > 0 && height > 0;
}

} // namespace

int main(int argc, char** argv)
{
    const std::string capture = argument_value(argc, argv, "--capture");
    const std::string carve = argument_value(argc, argv, "--carve");

    int demo_width = 0;
    int demo_height = 0;
    if (!carve.empty() && !parse_size(carve, demo_width, demo_height))
    {
        std::fprintf(stderr, "error: --carve expects WIDTHxHEIGHT, got '%s'\n",
                     carve.c_str());
        return 2;
    }

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1280, 800, "Seam Carving");

    if (!IsWindowReady())
    {
        std::fprintf(stderr,
                     "error: could not create a window. If this is a headless or remote "
                     "session without OpenGL, run it from a normal desktop session.\n");
        return 1;
    }
    SetTargetFPS(60);

    // The default font is a 10px bitmap atlas; point sampling keeps it crisp
    // when scaled up to 16-26px.
    SetTextureFilter(GetFontDefault().texture, TEXTURE_FILTER_POINT);

    // TODO: Create the app, load the sample or --image, and apply the demo flags.
    int frames = 0;
    bool shot_taken = false;

    while (!WindowShouldClose())
    {
        // TODO: Handle dropped files and call App::update().
        ++frames;

        BeginDrawing();
        ClearBackground(Color{14, 15, 18, 255});
        // TODO: Draw the app, control panel, and credits.
        // TODO: Wait for the result view when capturing a --carve run.
        const bool ready_to_capture = frames == 4;
        if (!capture.empty() && ready_to_capture && !shot_taken)
        {
            // raylib batches geometry until EndDrawing(); without this flush the
            // readback would only see the clear colour.
            rlDrawRenderBatchActive();
            ::Image shot = LoadImageFromScreen();
            ExportImage(shot, capture.c_str());
            UnloadImage(shot);
            shot_taken = true;
            std::fprintf(stderr, "viewer: captured %s\n", capture.c_str());
        }
        EndDrawing();

        if (shot_taken)
        {
            break;
        }
    }

    // TODO: Unload the app textures before closing the window.
    CloseWindow();
    return 0;
}
