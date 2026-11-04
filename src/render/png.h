// Minimal PNG writer (RGBA8, no compression beyond stored deflate blocks).
// Dependency-free on purpose: the host tool must build anywhere, and the file
// size does not matter for debug frames.
#pragma once

#include <cstdint>

namespace osu {

bool write_png_rgba(const char* path, const uint8_t* rgba, int width, int height);

}  // namespace osu
