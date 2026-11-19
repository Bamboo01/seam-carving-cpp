/**
 * @file src/viewer/app.hpp
 * @brief Viewer application state and commands.
 *
 * Seam Carving in C++ - content-aware image resizing.
 * Ported from https://github.com/vivianhylee/seam-carving
 *
 * @author Loh Jun Wei
 */
#pragma once

#include "raylib.h"
#include "seam/image.hpp"

#include <string>

namespace viewer
{

enum class View
{
    Edit,  // source plus mask, painting enabled
    Result // carved output
};

struct App
{
    seam::ImageF source;
    seam::ImageF mask; // single channel, 0 or 255
    seam::ImageF result;

    Texture2D image_texture{};
    Texture2D mask_texture{};
    bool textures_valid = false;
    bool mask_texture_valid = false;

    std::string image_path;
    std::string status = "drop an image, or press L for the sample";

    int target_width = 0;
    int target_height = 0;
    int brush_size = 18;

    View view = View::Edit;
    bool show_seam = false;
    bool credits_open = false;
    bool dirty = true;
    bool mask_dirty = true;
    double last_milliseconds = 0.0;

    // Last drawn placement, so mouse coordinates map back to pixels.
    Rectangle canvas_destination{};
    float canvas_scale = 1.0f;
    bool canvas_valid = false;

    bool load(const std::string& path);
    void reset_mask();
    void stroke(Vector2 mouse, bool erase);
    void update();
    void draw();

    // Both run on the caller's thread and return when the result is ready.
    void resize_to_target();
    void remove_object();

    void rebuild_seam();
    bool save_png(const std::string& path) const;

    bool has_mask() const;
    bool mask_empty() const;
    bool ready() const
    {
        return !source.empty();
    }
    const seam::ImageF& displayed() const
    {
        return view == View::Edit ? source : result;
    }
};

} // namespace viewer
