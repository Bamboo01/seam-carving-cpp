/**
 * @file src/viewer/main.cpp
 * @brief raylib entry point: window, input, drag and drop.
 *
 * Seam Carving in C++ - content-aware image resizing.
 * Ported from https://github.com/vivianhylee/seam-carving
 *
 * @author Loh Jun Wei
 */
#include "app.hpp"
#include "ui.hpp"

#include "raylib.h"
#include "rlgl.h"

#include <algorithm>
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

bool has_flag(int argc, char** argv, const char* flag)
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], flag) == 0)
        {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char** argv)
{
    const std::string image = argument_value(argc, argv, "--image");
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

    viewer::App app;
    app.load(image.empty() ? std::string("assets/sample.jpg") : image);

    // Test hooks: paint a patch or open the credits without a mouse, so the
    // interface can be screenshotted from a script.
    if (has_flag(argc, argv, "--paint") && app.ready())
    {
        const int x0 = app.source.width * 72 / 100;
        const int y0 = app.source.height * 58 / 100;
        const int w = std::max(8, app.source.width * 10 / 100);
        const int h = std::max(8, app.source.height * 16 / 100);

        for (int y = y0; y < y0 + h && y < app.mask.height; ++y)
        {
            for (int x = x0; x < x0 + w && x < app.mask.width; ++x)
            {
                app.mask.at(x, y) = 255.0;
            }
        }
        app.mask_dirty = true;
        app.show_seam = true;
        app.rebuild_seam();
    }
    if (has_flag(argc, argv, "--credits"))
    {
        app.credits_open = true;
    }

    if (demo_width > 0)
    {
        app.target_width = demo_width;
        app.target_height = demo_height;
        app.resize_to_target();
    }

    int frames = 0;
    bool shot_taken = false;

    while (!WindowShouldClose())
    {
        if (IsFileDropped())
        {
            FilePathList dropped = LoadDroppedFiles();
            if (dropped.count > 0)
            {
                app.load(dropped.paths[0]);
            }
            UnloadDroppedFiles(dropped);
        }

        app.update();
        ++frames;

        BeginDrawing();
        ClearBackground(Color{14, 15, 18, 255});
        app.draw();
        viewer::draw_panel(app);
        if (app.credits_open)
        {
            viewer::draw_credits(app);
        }

        const bool ready_to_capture =
            demo_width > 0 ? (app.view == viewer::View::Result && frames > 3) : frames == 4;

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

    if (app.textures_valid)
    {
        UnloadTexture(app.image_texture);
    }
    if (app.mask_texture_valid)
    {
        UnloadTexture(app.mask_texture);
    }

    CloseWindow();
    return 0;
}
