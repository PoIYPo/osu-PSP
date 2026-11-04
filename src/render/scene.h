// Software renderer for one gameplay frame at the PSP's 480x272. It is the
// reference implementation of the layout that the GU renderer must reproduce.
#pragma once

#include "game/gameplay.h"
#include "render/framebuffer.h"
#include "render/layout.h"

namespace osu {

// Digits, 'x', '%', '.', '-' and space. Enough for combo/accuracy HUD text
// without dragging in a font library (and with no bundled font assets).
void draw_text(Framebuffer& fb, float x, float y, float pixel_size, const char* text, Color color);

// Renders the current Gameplay state.
void render_scene(Framebuffer& fb, const Gameplay& game);

}  // namespace osu
