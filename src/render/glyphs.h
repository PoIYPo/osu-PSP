// 3x5 bitmap glyphs shared by the software renderer, the GU renderer and the
// skin atlas generator. Latin uppercase is included for HUD hints and menus;
// lowercase letters fall back to their uppercase shape where it exists.
#pragma once

#include <cstdint>

namespace osu {

struct GlyphDef {
    char c;
    uint8_t rows[5];  // bits 2..0 = left..right
};

extern const GlyphDef kGlyphs[];
extern const int kGlyphCount;

// Horizontal advance of one glyph in glyph pixels (3 wide + 1 gap).
constexpr float kGlyphAdvance = 4.0f;

// 5 rows of the glyph, or nullptr if the character is not in the table.
const uint8_t* glyph_rows(char c);

// Index of the character in the table, or -1.
int glyph_index(char c);

}  // namespace osu
