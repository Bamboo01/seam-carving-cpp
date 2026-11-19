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

#include <algorithm>
#include <cmath>

namespace viewer
{
namespace
{

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

void App::reset_mask()
{
    std::fill(mask.data.begin(), mask.data.end(), 0.0);
    mask_dirty = true;
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
