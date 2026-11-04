// Procedural skin: the default look of the game, generated at startup instead of
// shipped as image files.
//
// Why procedural: osu!'s own skin assets are not redistributable, and a PNG
// decoder on the PSP would cost RAM and CPU we do not have. Round elements are
// drawn with antialiased coverage maths into ABGR8888 buffers that both the
// software renderer and the GU renderer can consume directly.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace osu {

// 0xAABBGGRR -- the byte order the PSP's GU expects for 8888 textures.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> pixels;

    void resize(int w, int h) {
        width = w;
        height = h;
        pixels.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0u);
    }
    bool empty() const { return pixels.empty(); }
};

struct Skin {
    int size = 128;  // texture size for round elements (96..128 is the PSP budget)

    Image hit_circle;          // white disc, tinted with the combo colour at draw time
    Image hit_circle_overlay;  // thin rim drawn on top
    Image approach_circle;     // ring that shrinks towards the hit time
    Image slider_ball;
    Image follow_circle;       // soft ring around the ball
    Image cursor;
    Image font;                // glyph atlas, see kFontGlyphs

    int font_cell_w = 8;
    int font_cell_h = 12;
};

// "0123456789x%.-" in that order, one glyph per cell of font_cell_w x font_cell_h.
extern const char* const kFontGlyphs;

// `size` is the edge of the round element textures (128 by default; 96 fits an
// even tighter VRAM budget).
Skin make_default_skin(int size = 128);

// Index of a character in the font atlas, or -1.
int font_glyph_index(char c);

// 5 rows of a 3x5 glyph (bits 2..0 = left..right), or nullptr if unsupported.
const uint8_t* glyph_rows(char c);

// Horizontal advance of one glyph in glyph pixels (3 wide + 1 gap).
constexpr float kGlyphAdvance = 4.0f;

}  // namespace osu
