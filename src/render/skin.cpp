#include "render/skin.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace osu {

const char* const kFontGlyphs = "0123456789x%.-";

namespace {

// 3x5 glyph rows, bits 2..0 = left..right (same shapes as the software renderer).
constexpr uint8_t kGlyphRows[][5] = {
    {7, 5, 5, 5, 7},  // 0
    {2, 6, 2, 2, 7},  // 1
    {7, 1, 7, 4, 7},  // 2
    {7, 1, 7, 1, 7},  // 3
    {5, 5, 7, 1, 1},  // 4
    {7, 4, 7, 1, 7},  // 5
    {7, 4, 7, 5, 7},  // 6
    {7, 1, 1, 1, 1},  // 7
    {7, 5, 7, 5, 7},  // 8
    {7, 5, 7, 1, 7},  // 9
    {5, 5, 2, 5, 5},  // x
    {5, 1, 2, 4, 5},  // %
    {0, 0, 0, 0, 2},  // .
    {0, 0, 7, 0, 0},  // -
};

struct Rgba {
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
};

void blend(Image& img, int x, int y, Rgba c) {
    if (x < 0 || y < 0 || x >= img.width || y >= img.height) return;
    if (c.a <= 0.0f) return;
    const float a = std::min(1.0f, c.a);
    uint32_t& dst = img.pixels[static_cast<size_t>(y) * static_cast<size_t>(img.width) + static_cast<size_t>(x)];
    const float dr = static_cast<float>(dst & 0xFFu);
    const float dg = static_cast<float>((dst >> 8) & 0xFFu);
    const float db = static_cast<float>((dst >> 16) & 0xFFu);
    const float da = static_cast<float>((dst >> 24) & 0xFFu) / 255.0f;

    const float out_a = a + da * (1.0f - a);
    const float r = (c.r * a + dr * da * (1.0f - a)) / std::max(1e-6f, out_a);
    const float g = (c.g * a + dg * da * (1.0f - a)) / std::max(1e-6f, out_a);
    const float b = (c.b * a + db * da * (1.0f - a)) / std::max(1e-6f, out_a);

    dst = (static_cast<uint32_t>(std::lround(std::min(1.0f, out_a) * 255.0f)) << 24) |
          (static_cast<uint32_t>(std::lround(std::min(1.0f, b) * 255.0f)) << 16) |
          (static_cast<uint32_t>(std::lround(std::min(1.0f, g) * 255.0f)) << 8) |
          static_cast<uint32_t>(std::lround(std::min(1.0f, r) * 255.0f));
}

// Coverage of a disc at distance d from the centre, antialiased over ~1 px.
float disc_coverage(float d, float radius) {
    return std::clamp(radius - d + 0.5f, 0.0f, 1.0f);
}

void draw_disc(Image& img, float cx, float cy, float radius, Rgba color, float inner_scale = 0.0f,
               Rgba inner_color = {}) {
    const int x0 = std::max(0, static_cast<int>(std::floor(cx - radius - 1.0f)));
    const int x1 = std::min(img.width - 1, static_cast<int>(std::ceil(cx + radius + 1.0f)));
    const int y0 = std::max(0, static_cast<int>(std::floor(cy - radius - 1.0f)));
    const int y1 = std::min(img.height - 1, static_cast<int>(std::ceil(cy + radius + 1.0f)));

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float dx = static_cast<float>(x) + 0.5f - cx;
            const float dy = static_cast<float>(y) + 0.5f - cy;
            const float d = std::sqrt(dx * dx + dy * dy);
            float cov = disc_coverage(d, radius);
            if (cov <= 0.0f) continue;

            Rgba c = color;
            if (inner_scale > 0.0f) {
                const float t = std::clamp(d / std::max(1e-3f, radius * inner_scale), 0.0f, 1.0f);
                c.r = inner_color.r + (color.r - inner_color.r) * t;
                c.g = inner_color.g + (color.g - inner_color.g) * t;
                c.b = inner_color.b + (color.b - inner_color.b) * t;
            }
            c.a *= cov;
            blend(img, x, y, c);
        }
    }
}

void draw_ring(Image& img, float cx, float cy, float radius, float thickness, Rgba color) {
    const float half = thickness * 0.5f;
    const float outer = radius + half + 1.0f;
    const int x0 = std::max(0, static_cast<int>(std::floor(cx - outer)));
    const int x1 = std::min(img.width - 1, static_cast<int>(std::ceil(cx + outer)));
    const int y0 = std::max(0, static_cast<int>(std::floor(cy - outer)));
    const int y1 = std::min(img.height - 1, static_cast<int>(std::ceil(cy + outer)));

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float dx = static_cast<float>(x) + 0.5f - cx;
            const float dy = static_cast<float>(y) + 0.5f - cy;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float cov = std::clamp(half - std::fabs(d - radius) + 0.5f, 0.0f, 1.0f);
            if (cov <= 0.0f) continue;
            Rgba c = color;
            c.a *= cov;
            blend(img, x, y, c);
        }
    }
}

void draw_rect(Image& img, int x0, int y0, int w, int h, Rgba color) {
    for (int y = y0; y < y0 + h; ++y) {
        for (int x = x0; x < x0 + w; ++x) blend(img, x, y, color);
    }
}

}  // namespace

int font_glyph_index(char c) {
    const char* p = kFontGlyphs;
    for (int i = 0; *p; ++p, ++i) {
        if (*p == c) return i;
    }
    return -1;
}

Skin make_default_skin(int size) {
    Skin skin;
    skin.size = size;
    const float half = static_cast<float>(size) * 0.5f;
    const float radius = half - 2.0f;

    // Hit circle: soft radial gradient plus a brighter rim, white for tinting.
    skin.hit_circle.resize(size, size);
    draw_disc(skin.hit_circle, half, half, radius, {1.0f, 1.0f, 1.0f, 0.92f}, 0.62f,
              {0.72f, 0.72f, 0.78f, 0.92f});
    draw_ring(skin.hit_circle, half, half, radius - 1.0f, 3.0f, {1.0f, 1.0f, 1.0f, 0.95f});

    skin.hit_circle_overlay.resize(size, size);
    draw_ring(skin.hit_circle_overlay, half, half, radius * 0.78f, 2.0f, {1.0f, 1.0f, 1.0f, 0.55f});

    // Approach circle: a ring that is scaled down by the renderer.
    skin.approach_circle.resize(size, size);
    draw_ring(skin.approach_circle, half, half, radius, std::max(2.0f, size * 0.03f),
              {1.0f, 1.0f, 1.0f, 0.85f});

    skin.slider_ball.resize(size, size);
    draw_disc(skin.slider_ball, half, half, radius * 0.55f, {1.0f, 0.9f, 0.5f, 1.0f}, 0.5f,
              {1.0f, 1.0f, 0.85f, 1.0f});
    draw_ring(skin.slider_ball, half, half, radius * 0.55f, 2.0f, {1.0f, 1.0f, 1.0f, 0.9f});

    skin.follow_circle.resize(size, size);
    draw_ring(skin.follow_circle, half, half, radius * 0.8f, 2.0f, {1.0f, 1.0f, 1.0f, 0.35f});

    skin.cursor.resize(size, size);
    draw_ring(skin.cursor, half, half, radius * 0.42f, 3.0f, {1.0f, 1.0f, 1.0f, 0.95f});
    draw_rect(skin.cursor, static_cast<int>(half - radius * 0.42f) - 2, static_cast<int>(half) - 1,
              6, 2, {1.0f, 1.0f, 1.0f, 0.9f});
    draw_rect(skin.cursor, static_cast<int>(half) - 1, static_cast<int>(half - radius * 0.42f) - 2,
              2, 6, {1.0f, 1.0f, 1.0f, 0.9f});

    // Font atlas: 3x5 glyphs scaled 2x inside 8x12 cells.
    const int cells = static_cast<int>(std::strlen(kFontGlyphs));
    skin.font_cell_w = 8;
    skin.font_cell_h = 12;
    // 128x16: both dimensions must be powers of two for the PSP's GE.
    skin.font.resize(128, 16);
    for (int i = 0; i < cells; ++i) {
        const uint8_t* rows = kGlyphRows[i];
        const int origin_x = i * skin.font_cell_w + 1;
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 3; ++col) {
                if (rows[row] & (1u << (2 - col))) {
                    draw_rect(skin.font, origin_x + col * 2, 1 + row * 2, 2, 2,
                              {1.0f, 1.0f, 1.0f, 1.0f});
                }
            }
        }
    }

    return skin;
}

}  // namespace osu
