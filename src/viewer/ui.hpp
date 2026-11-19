/**
 * @file src/viewer/ui.hpp
 * @brief Control panel and credits screen entry points.
 *
 * Seam Carving in C++ - content-aware image resizing.
 * Ported from https://github.com/vivianhylee/seam-carving
 *
 * @author Loh Jun Wei
 */
#pragma once

#include "app.hpp"

namespace viewer
{

void draw_panel(App& app);
void draw_credits(const App& app);

} // namespace viewer
