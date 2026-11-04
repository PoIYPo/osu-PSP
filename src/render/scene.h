// Software renderer for one gameplay frame at the PSP's 480x272. It is the
// reference implementation of the layout that the GU renderer must reproduce.
#pragma once

#include "game/gameplay.h"
#include "render/framebuffer.h"

namespace osu {

constexpr int kScreenWidth = 480;
constexpr int kScreenHeight = 272;

// The 4:3 playfield occupies 320x240 in the middle of the 16:9 screen; the two
// 80 px side strips hold the HUD, exactly like osu! on a widescreen monitor.
constexpr float kPlayfieldScreenHeight = 240.0f;
constexpr float kPlayfieldScreenWidth = kPlayfieldScreenHeight * (kPlayfieldWidth / kPlayfieldHeight);
constexpr float kPlayfieldScreenX = (kScreenWidth - kPlayfieldScreenWidth) * 0.5f;
constexpr float kPlayfieldScreenY = (kScreenHeight - kPlayfieldScreenHeight) * 0.5f;
constexpr float kPlayfieldScale = kPlayfieldScreenHeight / kPlayfieldHeight;

inline Vec2 playfield_to_screen(Vec2 p) {
    return {kPlayfieldScreenX + p.x * kPlayfieldScale, kPlayfieldScreenY + p.y * kPlayfieldScale};
}

// Digits, 'x', '%', '.', '-' and space. Enough for combo/accuracy HUD text
// without dragging in a font library (and with no bundled font assets).
void draw_text(Framebuffer& fb, float x, float y, float pixel_size, const char* text, Color color);

// Renders the current Gameplay state.
void render_scene(Framebuffer& fb, const Gameplay& game);

}  // namespace osu
