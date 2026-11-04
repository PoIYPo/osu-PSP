#include "render/glyphs.h"

#include <cctype>

namespace osu {

const GlyphDef kGlyphs[] = {
    {'0', {7, 5, 5, 5, 7}}, {'1', {2, 6, 2, 2, 7}}, {'2', {7, 1, 7, 4, 7}},
    {'3', {7, 1, 7, 1, 7}}, {'4', {5, 5, 7, 1, 1}}, {'5', {7, 4, 7, 1, 7}},
    {'6', {7, 4, 7, 5, 7}}, {'7', {7, 1, 1, 1, 1}}, {'8', {7, 5, 7, 5, 7}},
    {'9', {7, 5, 7, 1, 7}},
    {'A', {2, 5, 7, 5, 5}}, {'B', {6, 5, 6, 5, 6}}, {'C', {3, 4, 4, 4, 3}},
    {'D', {6, 5, 5, 5, 6}}, {'E', {7, 4, 6, 4, 7}}, {'F', {7, 4, 6, 4, 4}},
    {'G', {3, 4, 5, 5, 3}}, {'H', {5, 5, 7, 5, 5}}, {'I', {7, 2, 2, 2, 7}},
    {'J', {1, 1, 1, 5, 2}}, {'K', {5, 5, 6, 5, 5}}, {'L', {4, 4, 4, 4, 7}},
    {'M', {5, 7, 7, 5, 5}}, {'N', {5, 7, 7, 7, 5}}, {'O', {2, 5, 5, 5, 2}},
    {'P', {6, 5, 6, 4, 4}}, {'Q', {2, 5, 5, 6, 3}}, {'R', {6, 5, 6, 5, 5}},
    {'S', {3, 4, 2, 1, 6}}, {'T', {7, 2, 2, 2, 2}}, {'U', {5, 5, 5, 5, 7}},
    {'V', {5, 5, 5, 5, 2}}, {'W', {5, 5, 7, 7, 5}}, {'X', {5, 5, 2, 5, 5}},
    {'Y', {5, 5, 2, 2, 2}}, {'Z', {7, 1, 2, 4, 7}},
    {'%', {5, 1, 2, 4, 5}}, {'.', {0, 0, 0, 0, 2}}, {'-', {0, 0, 7, 0, 0}},
    {':', {0, 2, 0, 2, 0}}, {' ', {0, 0, 0, 0, 0}},
};

const int kGlyphCount = static_cast<int>(sizeof(kGlyphs) / sizeof(kGlyphs[0]));

int glyph_index(char c) {
    for (int i = 0; i < kGlyphCount; ++i) {
        if (kGlyphs[i].c == c) return i;
    }
    // Lowercase uses the uppercase shape when one exists.
    const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (upper != c) {
        for (int i = 0; i < kGlyphCount; ++i) {
            if (kGlyphs[i].c == upper) return i;
        }
    }
    return -1;
}

const uint8_t* glyph_rows(char c) {
    const int index = glyph_index(c);
    if (index < 0) return nullptr;
    return kGlyphs[index].rows;
}

}  // namespace osu
