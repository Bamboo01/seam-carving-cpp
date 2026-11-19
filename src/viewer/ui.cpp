/**
 * @file src/viewer/ui.cpp
 * @brief Widgets, control panel layout, and the credits screen.
 *
 * Seam Carving in C++ - content-aware image resizing.
 * Ported from https://github.com/vivianhylee/seam-carving
 *
 * @author Loh Jun Wei
 */
#include "ui.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace viewer
{
namespace
{

constexpr Color kPanel{20, 22, 26, 255};
constexpr Color kText{228, 230, 235, 255};
constexpr Color kDim{130, 136, 146, 255};
constexpr Color kAccent{255, 196, 66, 255};
constexpr Color kButton{44, 48, 56, 255};
constexpr Color kButtonHot{62, 68, 78, 255};
constexpr Color kButtonDown{88, 96, 108, 255};

void* g_active_slider = nullptr;

bool button(Rectangle bounds, const char* label, bool enabled)
{
    const Vector2 mouse = GetMousePosition();
    const bool hot = enabled && CheckCollisionPointRec(mouse, bounds);
    const bool held = hot && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    Color fill = kButton;
    if (!enabled)
    {
        fill = Color{34, 36, 42, 255};
    }
    else if (held)
    {
        fill = kButtonDown;
    }
    else if (hot)
    {
        fill = kButtonHot;
    }

    DrawRectangleRounded(bounds, 0.25f, 6, fill);
    if (hot && enabled)
    {
        DrawRectangleRoundedLines(bounds, 0.25f, 6, kAccent);
    }

    const int size = 18;
    const int text_width = MeasureText(label, size);
    DrawText(label, static_cast<int>(bounds.x + (bounds.width - text_width) / 2),
             static_cast<int>(bounds.y + (bounds.height - size) / 2), size,
             enabled ? kText : kDim);

    return enabled && hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

bool slider(Rectangle bounds, int& value, int minimum, int maximum, const char* label)
{
    const Vector2 mouse = GetMousePosition();
    const bool hot = CheckCollisionPointRec(mouse, bounds);

    if (hot && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        g_active_slider = &value;
    }
    const bool active = (g_active_slider == &value);
    if (active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
    {
        g_active_slider = nullptr;
    }

    if (active)
    {
        const float t = std::clamp((mouse.x - bounds.x) / bounds.width, 0.0f, 1.0f);
        value = minimum + static_cast<int>(std::lround(t * (maximum - minimum)));
    }

    const float normalized =
        maximum > minimum ? static_cast<float>(value - minimum) / (maximum - minimum)
                          : 0.0f;

    DrawText(TextFormat("%s  %d", label, value), static_cast<int>(bounds.x),
             static_cast<int>(bounds.y) - 22, 17, kDim);

    const Rectangle track{bounds.x, bounds.y + bounds.height * 0.5f - 2.0f, bounds.width,
                          4.0f};
    DrawRectangleRounded(track, 1.0f, 4, Color{50, 54, 62, 255});
    DrawRectangleRounded({track.x, track.y, track.width * normalized, track.height}, 1.0f,
                         4, kAccent);
    DrawCircle(static_cast<int>(track.x + track.width * normalized),
               static_cast<int>(track.y + 2.0f), 7.0f, kAccent);

    return active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

bool toggle(Rectangle bounds, bool& value, const char* label)
{
    const bool clicked = button(bounds, label, true);
    if (clicked)
    {
        value = !value;
    }

    const Rectangle box{bounds.x + bounds.width - 30.0f, bounds.y + 10.0f, 18.0f, 18.0f};
    DrawRectangleRounded(box, 0.3f, 4, value ? kAccent : Color{50, 54, 62, 255});
    if (value)
    {
        DrawCircle(static_cast<int>(box.x + box.width * 0.5f),
                   static_cast<int>(box.y + box.height * 0.5f), 4.0f,
                   Color{20, 22, 26, 255});
    }
    return clicked;
}

void section(const char* title, float y)
{
    DrawText(title, 24, static_cast<int>(y), 14, kDim);
    DrawLine(24, static_cast<int>(y) + 20, 296, static_cast<int>(y) + 20,
             Color{44, 48, 56, 255});
}

} // namespace

void draw_panel(App& app)
{
    DrawRectangle(0, 0, 320, GetScreenHeight(), kPanel);
    DrawLine(320, 0, 320, GetScreenHeight(), Color{44, 48, 56, 255});

    DrawText("SEAM CARVING", 24, 24, 26, kText);
    DrawText("content-aware resize", 24, 56, 15, kDim);

    section("SOURCE", 96);
    if (app.ready())
    {
        DrawText(TextFormat("%d x %d", app.source.width, app.source.height), 24, 120, 16,
                 kText);
        const bool small = app.source.width < 1024 && app.source.height < 1024;
        DrawText(small ? "preview: full detail" : "preview: downscaled", 24, 140, 14,
                 kDim);
    }
    else
    {
        DrawText("no image", 24, 120, 16, kDim);
    }

    section("TARGET SIZE", 172);
    const int max_width = app.ready() ? static_cast<int>(app.source.width * 1.5f) : 100;
    const int max_height = app.ready() ? static_cast<int>(app.source.height * 1.5f) : 100;

    float unused = 0.0f;
    app.target_width = std::clamp(app.target_width, 32, std::max(32, max_width));
    app.target_height = std::clamp(app.target_height, 32, std::max(32, max_height));

    slider({24.0f, 216.0f, 272.0f, 16.0f}, app.target_width, 32, std::max(32, max_width),
           "width");
    slider({24.0f, 266.0f, 272.0f, 16.0f}, app.target_height, 32,
           std::max(32, max_height), "height");
    (void)unused;

    if (button({24.0f, 306.0f, 272.0f, 40.0f}, "Carve", app.ready()))
    {
        app.resize_to_target();
    }

    section("MASK", 372);
    DrawText("paint with left drag, erase with right", 24, 396, 14, kDim);

    slider({24.0f, 440.0f, 272.0f, 16.0f}, app.brush_size, 4, 60, "brush radius");

    if (button({24.0f, 464.0f, 130.0f, 36.0f}, "Remove object", app.ready()))
    {
        app.remove_object();
    }
    if (button({166.0f, 464.0f, 130.0f, 36.0f}, "Clear mask", app.ready()))
    {
        app.reset_mask();
    }

    if (toggle({24.0f, 512.0f, 272.0f, 36.0f}, app.show_seam, "Show next seam") &&
        app.show_seam && app.seam.empty())
    {
        app.rebuild_seam();
    }

    section("RESULT", 570);
    if (app.view == View::Result)
    {
        DrawText(TextFormat("%d x %d", app.result.width, app.result.height), 24, 594, 16,
                 kText);
        if (button({24.0f, 620.0f, 130.0f, 36.0f}, "Save PNG", true))
        {
            app.status = app.save_png("out/viewer_result.png") ? "saved out/viewer_result.png"
                                                               : "could not write output";
        }
        if (button({166.0f, 620.0f, 130.0f, 36.0f}, "Back to mask", true))
        {
            app.view = View::Edit;
            app.dirty = true;
        }
    }
    else
    {
        DrawText("nothing carved yet", 24, 594, 15, kDim);
    }

    if (button({24.0f, 690.0f, 130.0f, 36.0f}, "Credits (F1)", true))
    {
        app.credits_open = true;
    }

    DrawText(app.status.c_str(), 24, 744, 15, kAccent);
    DrawText("L load sample   R clear mask   F1 credits", 24, 768, 13, kDim);
}

void draw_credits(const App& app)
{
    (void)app;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 170});

    const Rectangle panel{240.0f, 120.0f, 800.0f, 540.0f};
    DrawRectangleRounded(panel, 0.04f, 8, Color{26, 28, 34, 255});
    DrawRectangleRoundedLines(panel, 0.04f, 8, kAccent);

    DrawText("SEAM CARVING", 280, 156, 34, kText);
    DrawText("content-aware image resizing in C++", 280, 198, 17, kDim);

    struct Entry
    {
        const char* name;
        const char* role;
    };
    const Entry entries[] = {
        {"Sherwyn Low", "image data, loading and float64 conversion"},
        {"Lim Rui Heng", "energy map: Scharr gradients, reflect-101"},
        {"Tea Zhi Xun", "cumulative cost, dynamic programming"},
        {"Sim Wei Siang", "seam search, removal, insertion, rotation"},
        {"Teo Pek Lin", "carver pipeline, masks, object removal, CLI"},
        {"Loh Jun Wei", "raylib viewer: canvas, painting, controls"},
    };

    float y = 244.0f;
    for (const Entry& entry : entries)
    {
        DrawText(entry.name, 280, static_cast<int>(y), 21, kAccent);
        DrawText(entry.role, 520, static_cast<int>(y) + 3, 16, kText);
        y += 54.0f;
    }

    DrawText("built with raylib 5.5 and stb_image", 280, 592, 14, kDim);
    DrawText("press F1 or Esc to close", 280, 616, 14, kDim);
}

} // namespace viewer
