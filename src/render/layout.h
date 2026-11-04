// Screen layout shared by the software renderer and the PSP renderer.
#pragma once

#include "core/types.h"

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

}  // namespace osu
