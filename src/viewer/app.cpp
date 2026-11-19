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
 * Lim Rui Heng
 * @author Loh Jun Wei
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

} // namespace viewer
