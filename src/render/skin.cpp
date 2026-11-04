#include "render/skin.h"

#include <algorithm>
#include <cmath>

namespace osu {

namespace {

struct Rgba {
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
};

void blend(Image& img, int x, int y, Rgba c) {
    if (x < 0 || y < 0 || x >= img.width || y >= img.height) return;
    if (c.a <= 0.0f) return;
    const float a = std::min(1.0f, c.a);
    uint32_t& dst =
        img.pixels[static_cast<size_t>(y) * static_cast<size_t>(img.width) + static_cast<size_t>(x)];
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

float disc_coverage(float d, float radius) { return std::clamp(radius - d + 0.5f, 0.0f, 1.0f); }

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

// Glowing ring: a bright rim with a soft halo outside and a translucent interior,
// so a hit circle does not hide the notes stacked behind it.
void draw_glow_ring(Image& img, float cx, float cy, float radius, float thickness, float glow,
                    Rgba rim, Rgba fill) {
    const float half = thickness * 0.5f;
    const float outer = radius + half + glow + 1.0f;
    const int x0 = std::max(0, static_cast<int>(std::floor(cx - outer)));
    const int x1 = std::min(img.width - 1, static_cast<int>(std::ceil(cx + outer)));
    const int y0 = std::max(0, static_cast<int>(std::floor(cy - outer)));
    const int y1 = std::min(img.height - 1, static_cast<int>(std::ceil(cy + outer)));

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float dx = static_cast<float>(x) + 0.5f - cx;
            const float dy = static_cast<float>(y) + 0.5f - cy;
            const float d = std::sqrt(dx * dx + dy * dy);

            const float rim_cov = std::clamp(half - std::fabs(d - radius) + 0.5f, 0.0f, 1.0f);
            float glow_cov = 0.0f;
            if (d > radius - half && glow > 0.0f) {
                const float t = std::clamp((d - (radius - half)) / glow, 0.0f, 1.0f);
                glow_cov = (1.0f - t) * (1.0f - t) * 0.55f;
            }
            const float fill_cov =
                d < radius - half ? std::clamp(radius - half - d + 0.5f, 0.0f, 1.0f) : 0.0f;

            // Rim wins over glow, fill stays translucent.
            if (rim_cov > 0.0f) {
                Rgba c = rim;
                c.a *= rim_cov;
                blend(img, x, y, c);
            }
            if (glow_cov > 0.001f) {
                Rgba c = rim;
                c.a = glow_cov;
                blend(img, x, y, c);
            }
            if (fill_cov > 0.0f && fill.a > 0.0f) {
                Rgba c = fill;
                c.a *= fill_cov;
                blend(img, x, y, c);
            }
        }
    }
}
}  // namespace

Skin make_default_skin(int size) {
    Skin skin;
    skin.size = size;
    const float half = static_cast<float>(size) * 0.5f;
    const float radius = half - 2.0f;

    // Hit circle: a glowing rim with a translucent middle. The see-through centre
    // is what makes a stack of notes readable -- each one shows the next.
    const float ring_thickness = std::max(6.0f, size * 0.11f);
    const float glow_width = size * 0.16f;
    skin.hit_circle.resize(size, size);
    // Monochrome skin: grey translucent body, white rim and glow.
    draw_glow_ring(skin.hit_circle, half, half, radius - ring_thickness * 0.5f, ring_thickness,
                   glow_width, {1.0f, 1.0f, 1.0f, 0.95f}, {0.62f, 0.62f, 0.66f, 0.32f});

    skin.hit_circle_overlay.resize(size, size);
    draw_ring(skin.hit_circle_overlay, half, half, radius * 0.78f, 2.0f, {1.0f, 1.0f, 1.0f, 0.55f});

    // Approach circle: a ring that is scaled down by the renderer.
    skin.approach_circle.resize(size, size);
    draw_ring(skin.approach_circle, half, half, radius, std::max(2.0f, size * 0.03f),
              {1.0f, 1.0f, 1.0f, 0.85f});

    skin.slider_ball.resize(size, size);
    draw_disc(skin.slider_ball, half, half, radius * 0.55f, {0.55f, 0.9f, 1.0f, 1.0f}, 0.5f,
              {0.85f, 0.98f, 1.0f, 1.0f});
    draw_ring(skin.slider_ball, half, half, radius * 0.78f, 3.0f, {0.55f, 0.9f, 1.0f, 0.55f});
    draw_ring(skin.slider_ball, half, half, radius * 0.55f, 2.0f, {1.0f, 1.0f, 1.0f, 0.9f});

    skin.follow_circle.resize(size, size);
    draw_ring(skin.follow_circle, half, half, radius * 0.8f, 2.0f, {1.0f, 1.0f, 1.0f, 0.35f});

    // Cursor: the same glowing ring look as the skin in the reference screenshot,
    // with a small solid centre so the exact aim point stays readable.
    skin.cursor.resize(size, size);
    draw_glow_ring(skin.cursor, half, half, radius * 0.62f, std::max(5.0f, size * 0.08f),
                   size * 0.14f, {1.0f, 1.0f, 1.0f, 0.95f}, {1.0f, 1.0f, 1.0f, 0.10f});
    draw_disc(skin.cursor, half, half, radius * 0.10f, {1.0f, 1.0f, 1.0f, 0.95f});

    // Slider body stamp: a plain disc stamped densely along the path. It uses a
    // hard edge on purpose: the antialiased rims of overlapping stamps would show
    // up as seams inside the track.
    skin.slider_dot.resize(size, size);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = static_cast<float>(x) + 0.5f - half;
            const float dy = static_cast<float>(y) + 0.5f - half;
            if (dx * dx + dy * dy <= radius * radius) {
                skin.slider_dot.pixels[static_cast<size_t>(y) * size + static_cast<size_t>(x)] =
                    0xFFFFFFFFu;
            }
        }
    }

    return skin;
}

}  // namespace osu