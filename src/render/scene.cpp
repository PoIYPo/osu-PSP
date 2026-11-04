#include "render/scene.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace osu {

namespace {

// 3x5 glyphs, one byte per row, bits 2..0 = left..right.
struct Glyph {
    char c;
    uint8_t rows[5];
};

constexpr Glyph kGlyphs[] = {
    {'0', {7, 5, 5, 5, 7}}, {'1', {2, 6, 2, 2, 7}}, {'2', {7, 1, 7, 4, 7}},
    {'3', {7, 1, 7, 1, 7}}, {'4', {5, 5, 7, 1, 1}}, {'5', {7, 4, 7, 1, 7}},
    {'6', {7, 4, 7, 5, 7}}, {'7', {7, 1, 1, 1, 1}}, {'8', {7, 5, 7, 5, 7}},
    {'9', {7, 5, 7, 1, 7}}, {'x', {5, 5, 2, 5, 5}}, {'%', {5, 1, 2, 4, 5}},
    {'.', {0, 0, 0, 0, 2}}, {'-', {0, 0, 7, 0, 0}}, {' ', {0, 0, 0, 0, 0}},
};

const uint8_t* find_glyph(char c) {
    for (const Glyph& g : kGlyphs) {
        if (g.c == c) return g.rows;
    }
    return nullptr;
}

Color combo_color(int combo) {
    static const Color palette[] = {
        Color::rgb(255, 120, 160), Color::rgb(120, 200, 255), Color::rgb(255, 200, 110),
        Color::rgb(160, 230, 150), Color::rgb(200, 160, 255),
    };
    const int index = (combo / 8) % 5;
    return palette[index];
}

}  // namespace

void draw_text(Framebuffer& fb, float x, float y, float pixel_size, const char* text, Color color) {
    const float glyph_w = 3.0f * pixel_size + pixel_size;
    float cx = x;
    for (const char* p = text; *p; ++p) {
        const uint8_t* rows = find_glyph(*p);
        if (rows) {
            for (int row = 0; row < 5; ++row) {
                for (int col = 0; col < 3; ++col) {
                    if (rows[row] & (1u << (2 - col))) {
                        fb.fill_rect(cx + static_cast<float>(col) * pixel_size,
                                     y + static_cast<float>(row) * pixel_size, pixel_size,
                                     pixel_size, color);
                    }
                }
            }
        }
        cx += glyph_w;
    }
}

void render_scene(Framebuffer& fb, const Gameplay& game) {
    const Beatmap& bm = game.map().map;
    const StandardDifficulty& diff = game.map().difficulty;
    const double now = game.time();
    const double preempt = diff.preempt_ms();
    const float radius = diff.circle_radius() * kPlayfieldScale;

    fb.clear(Color::rgb(14, 14, 18));

    // HUD strips.
    fb.fill_rect(0.0f, kPlayfieldScreenY, kPlayfieldScreenX, kPlayfieldScreenHeight,
                 Color::rgb(22, 22, 28));
    fb.fill_rect(kPlayfieldScreenX + kPlayfieldScreenWidth, kPlayfieldScreenY,
                 kScreenWidth - (kPlayfieldScreenX + kPlayfieldScreenWidth), kPlayfieldScreenHeight,
                 Color::rgb(22, 22, 28));

    // Playfield.
    fb.fill_rect(kPlayfieldScreenX, kPlayfieldScreenY, kPlayfieldScreenWidth,
                 kPlayfieldScreenHeight, Color::rgb(28, 28, 36));
    fb.fill_rect(kPlayfieldScreenX, kPlayfieldScreenY, kPlayfieldScreenWidth, 1.0f,
                 Color::rgb(70, 70, 96));

    const auto& objects = bm.objects;

    // Slider paths first so the heads and the ball sit on top of them.
    for (size_t i = 0; i < objects.size(); ++i) {
        const HitObject& obj = objects[i];
        if (obj.kind != ObjectKind::Slider) continue;
        if (game.state(i) == ObjectState::Missed) continue;
        if (now < obj.time - preempt || now > obj.end_time) continue;

        const SliderGeometry& geo = game.map().sliders[i];
        const std::vector<Vec2>& pts = geo.path.points();
        const Color track = Color::rgba(90, 120, 160, 90);
        for (size_t k = 1; k < pts.size(); ++k) {
            fb.thick_line(playfield_to_screen(pts[k - 1]), playfield_to_screen(pts[k]),
                          3.0f, track);
        }
    }

    for (size_t i = 0; i < objects.size(); ++i) {
        const HitObject& obj = objects[i];
        const ObjectState state = game.state(i);
        if (state == ObjectState::Missed) continue;

        if (obj.kind == ObjectKind::Spinner) {
            if (now < obj.time - preempt || now > obj.end_time) continue;
            const Vec2 c = playfield_to_screen(obj.pos);
            const float r = 70.0f * kPlayfieldScale * 1.6f;
            fb.stroke_circle(c.x, c.y, r, 4.0f, Color::rgba(120, 200, 255, 120));
            const float fill = game.spinner_progress(i);
            fb.stroke_circle(c.x, c.y, r * (1.0f - 0.4f * fill), 2.0f,
                             Color::rgba(255, 255, 255, static_cast<int>(80 + 150 * fill)));
            continue;
        }

        const SliderRuntime& rt = game.slider(i);
        const bool is_slider = obj.kind == ObjectKind::Slider;
        const double disappear = is_slider ? obj.end_time : (obj.time + 180.0);
        if (now < obj.time - preempt || now > disappear) continue;
        if (state == ObjectState::Hit && !is_slider) continue;

        const Color color = combo_color(static_cast<int>(i));
        const Vec2 screen = playfield_to_screen(obj.pos);

        // Approach ring shrinks as the hit time approaches.
        const double appear = obj.time - preempt;
        const double span = std::max(1.0, obj.time - appear);
        const float progress = static_cast<float>(std::clamp((now - appear) / span, 0.0, 1.0));
        const float approach_radius = radius * (1.0f + 2.2f * (1.0f - progress));
        fb.stroke_circle(screen.x, screen.y, approach_radius, 2.0f,
                         Color::rgba(255, 255, 255, static_cast<int>(70 + 120 * progress)));

        // Hit circle.
        fb.fill_disc(screen.x, screen.y, radius, Color::rgba(color.r, color.g, color.b, 210));
        fb.stroke_circle(screen.x, screen.y, radius, 2.0f, Color::rgb(250, 250, 255));

        // Slider ball rides the path; the player only holds the button (PSP rule).
        if (is_slider && rt.following) {
            const SliderGeometry& geo = game.map().sliders[i];
            const Vec2 ball = playfield_to_screen(geo.path.position_at(rt.progress));
            fb.fill_disc(ball.x, ball.y, radius * 0.55f, Color::rgb(255, 230, 120));
            fb.stroke_circle(ball.x, ball.y, radius * 0.75f, 1.5f, Color::rgba(255, 255, 255, 140));
        }
    }

    // Cursor: a ring plus a crosshair, readable at 320x240.
    const Vec2 cur = playfield_to_screen(game.cursor());
    fb.stroke_circle(cur.x, cur.y, 6.0f, 2.0f, Color::rgb(255, 255, 255));
    fb.fill_rect(cur.x - 9.0f, cur.y - 1.0f, 18.0f, 2.0f, Color::rgba(255, 255, 255, 170));
    fb.fill_rect(cur.x - 1.0f, cur.y - 9.0f, 2.0f, 18.0f, Color::rgba(255, 255, 255, 170));

    // HUD: accuracy on the left, combo on the right.
    const GameplayStats& stats = game.stats();
    char acc[16];
    std::snprintf(acc, sizeof(acc), "%d.%d%%", static_cast<int>(game.accuracy() * 100.0),
                  static_cast<int>(game.accuracy() * 1000.0) % 10);
    draw_text(fb, 8.0f, 30.0f, 2.0f, acc, Color::rgb(200, 210, 230));

    char combo[24];
    std::snprintf(combo, sizeof(combo), "x%d", stats.combo);
    const float combo_size = 3.0f;
    const float combo_width = static_cast<float>(std::strlen(combo)) * (3.0f * combo_size + combo_size);
    draw_text(fb, kScreenWidth - 8.0f - combo_width, 26.0f, combo_size, combo,
              Color::rgb(255, 255, 255));

    char score[24];
    std::snprintf(score, sizeof(score), "%d", stats.score);
    draw_text(fb, 8.0f, kScreenHeight - 30.0f, 2.0f, score, Color::rgb(160, 170, 190));

    // Health bar along the bottom of the right strip.
    const float bar_x = kScreenWidth - 24.0f;
    const float bar_h = 180.0f;
    const float bar_y = 46.0f;
    fb.fill_rect(bar_x, bar_y, 10.0f, bar_h, Color::rgb(48, 48, 60));
    const float filled = bar_h * static_cast<float>(std::clamp(stats.hp, 0.0, 100.0) / 100.0);
    fb.fill_rect(bar_x, bar_y + bar_h - filled, 10.0f, filled, Color::rgb(120, 220, 140));
}

}  // namespace osu
