/**
 * @file src/viewer/app.cpp
 * @brief Viewer state: loading, painting, carving, and
 * canvas drawing.
 *
 * Seam Carving in C++ - content-aware image resizing.
 * Ported
 * from https://github.com/vivianhylee/seam-carving
 *
 * @author Loh Jun Wei
 */
#include "app.hpp"

#include "seam/carver.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>

namespace viewer
{
namespace
{

// Carving grows with pixels; keep the preview interactive on large photos. The
// CLI always runs at full resolution.
constexpr int kPreviewLimit = 800;

bool point_in(Rectangle rectangle, Vector2 point)
{
    return CheckCollisionPointRec(point, rectangle);
}

void upload(Texture2D& texture, bool& valid, const seam::Image& bytes, int format)
{
    if (bytes.width <= 0 || bytes.height <= 0)
    {
        return;
    }

    if (valid && (texture.width != bytes.width || texture.height != bytes.height))
    {
        UnloadTexture(texture);
        valid = false;
    }

    Image raw{};
    raw.data = const_cast<std::uint8_t*>(bytes.pixels.data());
    raw.width = bytes.width;
    raw.height = bytes.height;
    raw.mipmaps = 1;
    raw.format = format;

    if (!valid)
    {
        texture = LoadTextureFromImage(raw);
        valid = true;
    }
    else
    {
        UpdateTexture(texture, raw.data);
    }
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
}

} // namespace

bool App::load(const std::string& path)
{
    try
    {
        const seam::Image loaded = seam::load_image(path, 3);
        seam::ImageF pixels = seam::to_float(loaded);
        pixels = seam::downscale_to_fit(pixels, kPreviewLimit);

        source = std::move(pixels);
        image_path = path;

        target_width = source.width;
        target_height = source.height;

        mask.allocate(source.width, source.height, 1);
        result = seam::ImageF{};
        seam.clear();

        view = View::Edit;
        dirty = true;
        mask_dirty = true;
        canvas_valid = false;

        status = "loaded " + std::to_string(source.width) + "x" +
                 std::to_string(source.height);
        return true;
    }
    catch (const std::exception& error)
    {
        status = std::string("load failed: ") + error.what();
        return false;
    }
}

void App::reset_mask()
{
    std::fill(mask.data.begin(), mask.data.end(), 0.0);
    mask_dirty = true;
    seam.clear();
}

bool App::has_mask() const
{
    return !mask.empty();
}

bool App::mask_empty() const
{
    return std::all_of(mask.data.begin(), mask.data.end(),
                       [](double value)
                       {
                           return value <= 0.0;
                       });
}

void App::stroke(Vector2 mouse, bool erase)
{
    if (!canvas_valid || !point_in(canvas_destination, mouse) || mask.empty())
    {
        return;
    }

    const float scale = canvas_scale > 0.0f ? canvas_scale : 1.0f;
    const float image_x = (mouse.x - canvas_destination.x) / scale;
    const float image_y = (mouse.y - canvas_destination.y) / scale;

    const int radius = std::max(1, static_cast<int>(std::lround(brush_size / scale)));
    const int cx = static_cast<int>(std::lround(image_x));
    const int cy = static_cast<int>(std::lround(image_y));

    const int x0 = std::max(0, cx - radius);
    const int x1 = std::min(mask.width - 1, cx + radius);
    const int y0 = std::max(0, cy - radius);
    const int y1 = std::min(mask.height - 1, cy + radius);

    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
        {
            const int dx = x - cx;
            const int dy = y - cy;
            if (dx * dx + dy * dy <= radius * radius)
            {
                mask.at(x, y) = erase ? 0.0 : 255.0;
            }
        }
    }

    mask_dirty = true;
    seam.clear();
}

void App::resize_to_target()
{
    if (source.empty())
    {
        return;
    }

    seam::Carver carver(source);
    if (!mask_empty())
    {
        carver.set_protect_mask(mask);
    }

    carver.resize_to(target_width, target_height);

    result = carver.image();
    last_milliseconds = carver.last_elapsed_ms();
    view = View::Result;
    dirty = true;
    status = "resized in " + std::to_string(static_cast<int>(last_milliseconds)) + " ms";
}

void App::remove_object()
{
    if (source.empty() || mask_empty())
    {
        status = "paint on the object first";
        return;
    }

    seam::Carver carver(source);
    carver.set_object_mask(mask);
    carver.remove_object();

    result = carver.image();
    last_milliseconds = carver.last_elapsed_ms();
    view = View::Result;
    dirty = true;
    status = "object removed in " + std::to_string(static_cast<int>(last_milliseconds)) +
             " ms";
}

void App::rebuild_seam()
{
    if (source.empty())
    {
        return;
    }

    seam::Carver carver(source);
    if (!mask_empty())
    {
        carver.set_protect_mask(mask);
    }
    seam = carver.next_seam();
}

bool App::save_png(const std::string& path) const
{
    try
    {
        const auto parent = std::filesystem::path(path).parent_path();
        if (!parent.empty())
        {
            std::filesystem::create_directories(parent);
        }
        seam::save_png(seam::to_bytes(displayed()), path);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

void App::update()
{
    if (view == View::Edit && canvas_valid)
    {
        const Vector2 mouse = GetMousePosition();
        const bool erasing =
            IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsKeyDown(KEY_LEFT_SHIFT);

        if ((IsMouseButtonDown(MOUSE_BUTTON_LEFT) ||
             IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) &&
            point_in(canvas_destination, mouse))
        {
            stroke(mouse, erasing);
        }
    }

    if (IsKeyPressed(KEY_F1))
    {
        credits_open = !credits_open;
    }
    if (IsKeyPressed(KEY_L) && !credits_open)
    {
        load("assets/sample.jpg");
    }
    if (IsKeyPressed(KEY_R) && view == View::Edit)
    {
        reset_mask();
    }
}

void App::draw()
{
    const Rectangle canvas{340.0f, 40.0f, 900.0f, 720.0f};
    DrawRectangleRec(canvas, Color{24, 26, 30, 255});

    if (source.empty())
    {
        DrawText("Drop an image here, or press L", static_cast<int>(canvas.x) + 40,
                 static_cast<int>(canvas.y) + 40, 24, LIGHTGRAY);
        canvas_valid = false;
        return;
    }

    const seam::ImageF& picture = displayed();
    if (picture.empty())
    {
        canvas_valid = false;
        return;
    }

    if (dirty)
    {
        upload(image_texture, textures_valid, seam::to_bytes(picture),
               PIXELFORMAT_UNCOMPRESSED_R8G8B8);
        dirty = false;
    }

    if (mask_dirty)
    {
        seam::Image overlay;
        overlay.width = mask.width;
        overlay.height = mask.height;
        overlay.channels = 4;
        overlay.pixels.resize(static_cast<std::size_t>(mask.width) * mask.height * 4);

        for (std::size_t i = 0; i < mask.data.size(); ++i)
        {
            const bool painted = mask.data[i] > 0.0;
            overlay.pixels[i * 4 + 0] = 255;
            overlay.pixels[i * 4 + 1] = 60;
            overlay.pixels[i * 4 + 2] = 60;
            overlay.pixels[i * 4 + 3] = painted ? 130 : 0;
        }
        upload(mask_texture, mask_texture_valid, overlay,
               PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        mask_dirty = false;
    }

    const float scale = std::min(canvas.width / static_cast<float>(picture.width),
                                 canvas.height / static_cast<float>(picture.height));
    const float width = picture.width * scale;
    const float height = picture.height * scale;

    canvas_destination = {canvas.x + (canvas.width - width) * 0.5f,
                          canvas.y + (canvas.height - height) * 0.5f, width, height};
    canvas_scale = scale;
    canvas_valid = true;

    const Rectangle source_rect{0.0f, 0.0f, static_cast<float>(picture.width),
                                static_cast<float>(picture.height)};

    if (textures_valid)
    {
        DrawTexturePro(image_texture, source_rect, canvas_destination, Vector2{0.0f, 0.0f},
                       0.0f, WHITE);
    }

    if (view == View::Edit)
    {
        DrawTexturePro(mask_texture, source_rect, canvas_destination, Vector2{0.0f, 0.0f},
                       0.0f, WHITE);

        if (show_seam && !seam.empty())
        {
            Vector2 previous{};
            for (std::size_t y = 0; y < seam.size(); ++y)
            {
                const float px = canvas_destination.x + (seam[y] + 0.5f) * scale;
                const float py =
                    canvas_destination.y + (static_cast<float>(y) + 0.5f) * scale;
                if (y > 0)
                {
                    DrawLineEx(previous, Vector2{px, py}, 1.5f, Color{255, 220, 80, 255});
                }
                previous = {px, py};
            }
        }

        const Vector2 mouse = GetMousePosition();
        if (canvas_valid && point_in(canvas_destination, mouse))
        {
            DrawCircleLines(static_cast<int>(mouse.x), static_cast<int>(mouse.y),
                            static_cast<float>(brush_size), Color{255, 220, 80, 255});
        }
    }

    DrawRectangleLinesEx(canvas, 1.0f, Color{70, 74, 82, 255});
}

} // namespace viewer
