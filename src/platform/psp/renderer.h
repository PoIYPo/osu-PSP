// GU renderer: textured quads, tinting, solid rects and bitmap text.
//
// Textures live in main RAM (the PSP-1000 only has 2 MB of VRAM and the two
// 8888 framebuffers already eat 1.1 MB), so they are uploaded once with
// 16-byte aligned copies and sampled by the GE directly.
#pragma once

#include <cstdint>

#include "render/skin.h"

namespace osu {

struct GuTexture {
    void* data = nullptr;     // aligned 16, ABGR8888, size = width*height*4
    int width = 0;
    int height = 0;

    bool valid() const { return data != nullptr && width > 0 && height > 0; }
};

class Renderer {
public:
    void init();
    void shutdown();

    void begin_frame(unsigned int clear_abgr);
    void end_frame();

    // Uploads a procedural image into a GE-readable buffer. Must be called after
    // init() and the result must outlive the frame it is drawn in.
    GuTexture upload(const Image& image);

    void draw_sprite(const GuTexture& texture, float x, float y, float w, float h,
                     unsigned int tint_abgr);
    // Nearest-filtered variant: used for slider stamps, where linear filtering
    // makes every overlapping edge show up as a ring.
    void draw_sprite_crisp(const GuTexture& texture, float x, float y, float w, float h,
                           unsigned int tint_abgr);
    void draw_rect(float x, float y, float w, float h, unsigned int abgr);
    // Solid 3x5 bitmap text batched into a single triangle list. Textured glyph
    // atlases did not survive the GU texture-cache switching on hardware, and
    // rectangles are both cheaper and crisper at this size.
    void draw_text(float x, float y, float scale, const char* text, unsigned int abgr_color);
    // Draws the text so that it ends at `right_x`.
    void draw_text_right(float right_x, float y, float scale, const char* text,
                         unsigned int abgr_color);

private:
    void bind_texture(const GuTexture& texture, bool linear_filter);

    GuTexture current_;
};

// ABGR helpers (GU colour order is 0xAABBGGRR).
constexpr unsigned int abgr(int r, int g, int b, int a = 0xFF) {
    return (static_cast<unsigned int>(a) << 24) | (static_cast<unsigned int>(b) << 16) |
           (static_cast<unsigned int>(g) << 8) | static_cast<unsigned int>(r);
}

}  // namespace osu
