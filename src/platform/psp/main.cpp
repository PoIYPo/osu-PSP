// PSP entry point: loads a beatmap from the Memory Stick, runs the gameplay
// simulation with the analog stick and renders it with the GU renderer.
//
// Target hardware: PSP-1000 (32 MiB RAM, 2 MiB VRAM, ARK CFW, 443 MHz). The two
// 8888 framebuffers take 1.1 MiB of VRAM, so textures live in main RAM and the
// skin is procedural (see skin.cpp) at 128 px per element.
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <psprtc.h>
#include <pspuser.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>

#include "core/standard.h"
#include "game/gameplay.h"
#include "platform/psp/audio.h"
#include "platform/psp/renderer.h"
#include "render/glyphs.h"
#include "render/layout.h"
#include "render/skin.h"

using namespace osu;

PSP_MODULE_INFO("osu_psp", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

namespace {

constexpr const char* kAppDir = "ms0:/PSP/GAME/OSUPSP/";

bool read_file(const std::string& path, std::string& out) {
    out.clear();
    const SceUID fd = sceIoOpen(path.c_str(), PSP_O_RDONLY, 0);
    if (fd < 0) return false;

    char buffer[4096];
    for (;;) {
        const int read = sceIoRead(fd, buffer, sizeof(buffer));
        if (read <= 0) break;
        out.append(buffer, static_cast<size_t>(read));
        if (out.size() > 8u * 1024u * 1024u) break;
    }
    sceIoClose(fd);
    return !out.empty();
}

// The RTC ticks at 1 MHz and works in user mode; sceKernelGetSystemTimeWide does
// not resolve from this kind of module.
double now_ms() {
    u64 tick = 0;
    sceRtcGetCurrentTick(&tick);
    return static_cast<double>(tick) / 1000.0;
}

struct Textures {
    GuTexture hit_circle;
    GuTexture overlay;
    GuTexture approach;
    GuTexture ball;
    GuTexture follow;
    GuTexture cursor;
    GuTexture dot;
    float round_radius = 62.0f;  // radius of the disc inside the texture
    float skin_size = 128.0f;
};

Textures upload_skin(Renderer& renderer, const Skin& skin) {
    Textures textures;
    textures.hit_circle = renderer.upload(skin.hit_circle);
    textures.overlay = renderer.upload(skin.hit_circle_overlay);
    textures.approach = renderer.upload(skin.approach_circle);
    textures.ball = renderer.upload(skin.slider_ball);
    textures.follow = renderer.upload(skin.follow_circle);
    textures.cursor = renderer.upload(skin.cursor);
    textures.dot = renderer.upload(skin.slider_dot);
    textures.skin_size = static_cast<float>(skin.size);
    textures.round_radius = static_cast<float>(skin.size) * 0.5f - 2.0f;
    return textures;
}

void draw_round(Renderer& renderer, const GuTexture& texture, const Textures& t, float cx, float cy,
                float screen_radius, unsigned int tint) {
    if (!texture.valid() || screen_radius <= 0.0f) return;
    const float scale = screen_radius / t.round_radius;
    const float size = t.skin_size * scale;
    renderer.draw_sprite(texture, cx - size * 0.5f, cy - size * 0.5f, size, size, tint);
}

unsigned int combo_tint(size_t index) {
    static const unsigned int palette[] = {
        abgr(255, 120, 160), abgr(120, 200, 255), abgr(255, 200, 110), abgr(160, 230, 150),
        abgr(200, 160, 255),
    };
    return palette[(index / 8) % 5];
}

InputState read_input(const SceCtrlData& pad) {
    InputState input;
    float nx = static_cast<float>(pad.Lx) - 128.0f;
    float ny = static_cast<float>(pad.Ly) - 128.0f;

    constexpr float kDeadzone = 26.0f;
    if (std::fabs(nx) < kDeadzone) nx = 0.0f;
    if (std::fabs(ny) < kDeadzone) ny = 0.0f;

    nx = std::clamp(nx / 110.0f, -1.0f, 1.0f);
    ny = std::clamp(ny / 110.0f, -1.0f, 1.0f);

    input.cursor = {kPlayfieldWidth * 0.5f + nx * 250.0f, kPlayfieldHeight * 0.5f + ny * 185.0f};
    input.pressed = (pad.Buttons & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)) != 0;
    return input;
}

// Stamps a disc along a slider polyline: the same trick as the original slider
// body (a thick, round-capped track), just built from sprites instead of a mesh.
void stamp_path(Renderer& r, const GuTexture& texture, const Textures& t,
                const std::vector<Vec2>& points, float screen_radius, unsigned int tint,
                float step) {
    if (points.size() < 2) return;

    Vec2 prev = playfield_to_screen(points[0]);
    draw_round(r, texture, t, prev.x, prev.y, screen_radius, tint);

    float accum = 0.0f;
    for (size_t k = 1; k < points.size(); ++k) {
        const Vec2 cur = playfield_to_screen(points[k]);
        const float seg = distance(prev, cur);
        if (seg <= 1e-4f) continue;

        float pos = 0.0f;
        while (accum + (seg - pos) >= step) {
            pos += step - accum;
            accum = 0.0f;
            const Vec2 p = lerp(prev, cur, pos / seg);
            draw_round(r, texture, t, p.x, p.y, screen_radius, tint);
        }
        accum += seg - pos;
        prev = cur;
    }
}

// Autoplay used for demos and for verifying rendering: it plays the map perfectly,
// which is also the clearest way to see the auto-follow slider behaviour.
InputState autoplay_input(const Gameplay& game, size_t& index, double t) {
    const auto& objects = game.map().map.objects;

    for (size_t i = 0; i < objects.size(); ++i) {
        if (game.slider(i).following) {
            InputState in;
            in.cursor = game.cursor();
            in.pressed = true;
            return in;
        }
    }

    while (index < objects.size() && game.state(index) != ObjectState::Pending) ++index;

    InputState in;
    if (index >= objects.size()) {
        in.cursor = {kPlayfieldWidth * 0.5f, kPlayfieldHeight * 0.5f};
        return in;
    }

    const HitObject& obj = objects[index];
    switch (obj.kind) {
        case ObjectKind::Spinner: {
            const double duration = std::max(1.0, obj.end_time - obj.time);
            const double progress = std::clamp((t - obj.time) / duration, 0.0, 1.0);
            const float angle = static_cast<float>(progress * 2.0 * kPi * 3.0);
            in.cursor = {obj.pos.x + std::cos(angle) * 90.0f, obj.pos.y + std::sin(angle) * 90.0f};
            in.pressed = false;
            break;
        }
        case ObjectKind::Slider:
            in.cursor = obj.pos;
            in.pressed = t >= obj.time - 2.0;
            break;
        default:
            in.cursor = obj.pos;
            in.pressed = t >= obj.time - 2.0 && t <= obj.time + 40.0;
            break;
    }
    return in;
}
void render_gameplay(Renderer& r, const Textures& t, const Gameplay& game) {
    const Beatmap& bm = game.map().map;
    const StandardDifficulty& diff = game.map().difficulty;
    const double now = game.time();
    const double preempt = diff.preempt_ms();
    const float radius = diff.circle_radius() * kPlayfieldScale;

    // Side panels and the playfield.
    r.draw_rect(0.0f, kPlayfieldScreenY, kPlayfieldScreenX, kPlayfieldScreenHeight, abgr(22, 22, 28));
    r.draw_rect(kPlayfieldScreenX + kPlayfieldScreenWidth, kPlayfieldScreenY,
                kScreenWidth - (kPlayfieldScreenX + kPlayfieldScreenWidth), kPlayfieldScreenHeight,
                abgr(22, 22, 28));
    r.draw_rect(kPlayfieldScreenX, kPlayfieldScreenY, kPlayfieldScreenWidth, kPlayfieldScreenHeight,
                abgr(28, 28, 36));
    r.draw_rect(kPlayfieldScreenX, kPlayfieldScreenY, kPlayfieldScreenWidth, 1.0f, abgr(70, 70, 96));

    // First seconds of a map: explain the one rule that differs from the original.
    if (now < bm.first_object_time() + 6000.0) {
        const char* hint = "SLIDERS AUTO-FOLLOW";
        const float scale = 1.1f;
        const float text_w = static_cast<float>(std::strlen(hint)) * kGlyphAdvance * scale;
        const float bx = kPlayfieldScreenX + (kPlayfieldScreenWidth - text_w) * 0.5f;
        const float by = kPlayfieldScreenY + 12.0f;
        r.draw_rect(bx - 6.0f, by - 4.0f, text_w + 12.0f, 5.0f * scale + 8.0f, abgr(0, 0, 0, 150));
        r.draw_text(bx, by, scale, hint, abgr(235, 240, 255));
    }

    // Slider bodies, drawn like the original: a round-capped track with a light
    // border. Two passes so every border sits below every body.
    std::vector<size_t> visible_sliders;
    for (size_t i = 0; i < bm.objects.size(); ++i) {
        const HitObject& obj = bm.objects[i];
        if (obj.kind != ObjectKind::Slider) continue;
        if (game.state(i) == ObjectState::Missed) continue;
        if (now < obj.time - preempt || now > obj.end_time) continue;
        visible_sliders.push_back(i);
    }

    const float stamp_step = std::max(2.0f, radius * 0.22f);
    for (size_t index : visible_sliders) {
        stamp_path(r, t.dot, t, game.map().sliders[index].path.points(), radius + 2.0f,
                   abgr(245, 245, 250, 240), stamp_step);
    }
    for (size_t index : visible_sliders) {
        stamp_path(r, t.dot, t, game.map().sliders[index].path.points(), radius,
                   combo_tint(index), stamp_step);
    }

    for (size_t i = 0; i < bm.objects.size(); ++i) {
        const HitObject& obj = bm.objects[i];
        const ObjectState state = game.state(i);
        if (state == ObjectState::Missed) continue;

        if (obj.kind == ObjectKind::Spinner) {
            if (now < obj.time - preempt || now > obj.end_time) continue;
            const Vec2 c = playfield_to_screen(obj.pos);
            const float base = 70.0f * kPlayfieldScale * 1.6f;
            draw_round(r, t.approach, t, c.x, c.y, base, abgr(120, 200, 255, 120));
            const float fill = game.spinner_progress(i);
            draw_round(r, t.approach, t, c.x, c.y, base * (1.0f - 0.4f * fill),
                       abgr(255, 255, 255, static_cast<int>(80 + 150 * fill)));
            continue;
        }

        const bool is_slider = obj.kind == ObjectKind::Slider;
        const double disappear = is_slider ? obj.end_time : (obj.time + 180.0);
        if (now < obj.time - preempt || now > disappear) continue;
        if (state == ObjectState::Hit && !is_slider) continue;

        const Vec2 screen = playfield_to_screen(obj.pos);
        const unsigned int tint = combo_tint(i);

        // Approach ring shrinks from 3.2x to 1x of the circle radius.
        const double appear = obj.time - preempt;
        const double span = std::max(1.0, obj.time - appear);
        const float progress = static_cast<float>(std::clamp((now - appear) / span, 0.0, 1.0));
        const float approach_radius = radius * (1.0f + 2.2f * (1.0f - progress));
        draw_round(r, t.approach, t, screen.x, screen.y, approach_radius,
                   abgr(255, 255, 255, static_cast<int>(70 + 120 * progress)));

        draw_round(r, t.hit_circle, t, screen.x, screen.y, radius, tint);
        draw_round(r, t.overlay, t, screen.x, screen.y, radius * 0.78f, abgr(255, 255, 255, 90));

        if (is_slider && game.slider(i).following) {
            const Vec2 ball = playfield_to_screen(
                game.map().sliders[i].path.position_at(game.slider(i).progress));
            draw_round(r, t.follow, t, ball.x, ball.y, radius * 0.9f, abgr(255, 255, 255, 80));
            draw_round(r, t.ball, t, ball.x, ball.y, radius * 0.5f, abgr(255, 255, 255, 255));
        }
    }

    const Vec2 cursor = playfield_to_screen(game.cursor());
    draw_round(r, t.cursor, t, cursor.x, cursor.y, 48.0f * kPlayfieldScale, abgr(255, 255, 255, 255));

    // HUD.
    const GameplayStats& stats = game.stats();
    char text[32];
    std::snprintf(text, sizeof(text), "%d.%d%%", static_cast<int>(game.accuracy() * 100.0),
                  static_cast<int>(game.accuracy() * 1000.0) % 10);
    r.draw_text(6.0f, 26.0f, 0.9f, text, abgr(200, 210, 230));

    std::snprintf(text, sizeof(text), "x%d", stats.combo);
    r.draw_text_right(kScreenWidth - 6.0f, 22.0f, 1.4f, text, abgr(255, 255, 255));

    std::snprintf(text, sizeof(text), "%d", stats.score);
    r.draw_text(6.0f, kScreenHeight - 32.0f, 0.9f, text, abgr(160, 170, 190));

    const float bar_x = kScreenWidth - 24.0f;
    const float bar_h = 180.0f;
    const float bar_y = 52.0f;
    r.draw_rect(bar_x, bar_y, 10.0f, bar_h, abgr(48, 48, 60));
    const float filled = bar_h * static_cast<float>(std::clamp(stats.hp, 0.0, 100.0) / 100.0);
    r.draw_rect(bar_x, bar_y + bar_h - filled, 10.0f, filled, abgr(120, 220, 140));
}

void render_demo(Renderer& r, const Textures& t, double time_ms, Vec2 cursor) {
    r.draw_rect(kPlayfieldScreenX, kPlayfieldScreenY, kPlayfieldScreenWidth, kPlayfieldScreenHeight,
                abgr(28, 28, 36));
    r.draw_rect(kPlayfieldScreenX, kPlayfieldScreenY, kPlayfieldScreenWidth, 1.0f, abgr(70, 70, 96));

    // Shown when no map was found: proves textures, tinting and stick input work.
    const float cx = kPlayfieldScreenX + kPlayfieldScreenWidth * 0.5f;
    const float cy = kPlayfieldScreenY + kPlayfieldScreenHeight * 0.5f;
    const float pulse = 40.0f + 8.0f * static_cast<float>(std::sin(time_ms * 0.004));
    draw_round(r, t.hit_circle, t, cx, cy, pulse, abgr(120, 200, 255));
    draw_round(r, t.overlay, t, cx, cy, pulse * 0.78f, abgr(255, 255, 255, 90));
    draw_round(r, t.approach, t, cx, cy, pulse * 2.0f, abgr(255, 255, 255, 120));

    const Vec2 c = playfield_to_screen(cursor);
    draw_round(r, t.cursor, t, c.x, c.y, 48.0f * kPlayfieldScale, abgr(255, 255, 255, 255));
}

bool load_beatmap(const std::string& path, PlayableBeatmap& out, std::string& error) {
    std::string text;
    if (!read_file(path, text)) {
        error = "cannot read " + path;
        return false;
    }
    Beatmap map;
    if (!parse_osu(text, map, &error)) return false;
    out = compile_beatmap(std::move(map));
    return true;
}

}  // namespace

int main() {
    Renderer renderer;
    renderer.init();

    const Skin skin = make_default_skin(128);
    const Textures textures = upload_skin(renderer, skin);

    // Map discovery: an explicit path in map.txt wins, then the conventional
    // Songs/beatmap.osu next to the EBOOT.
    std::optional<PlayableBeatmap> play;
    std::string error;
    std::string map_path;
    {
        std::string configured;
        if (read_file(std::string(kAppDir) + "map.txt", configured)) {
            const size_t start = configured.find_first_not_of(" \t\r\n");
            const size_t end = configured.find_last_not_of(" \t\r\n");
            if (start != std::string::npos) {
                std::string path = configured.substr(start, end - start + 1);
                if (path.rfind("ms0:", 0) != 0) path = std::string(kAppDir) + path;
                PlayableBeatmap loaded;
                if (load_beatmap(path, loaded, error)) {
                    play = std::move(loaded);
                    map_path = path;
                }
            }
        }
        if (!play) {
            const char* candidates[] = {
                "ms0:/PSP/GAME/OSUPSP/Songs/beatmap.osu",
                "beatmap.osu",
            };
            for (const char* candidate : candidates) {
                PlayableBeatmap loaded;
                if (load_beatmap(candidate, loaded, error)) {
                    play = std::move(loaded);
                    map_path = candidate;
                    break;
                }
            }
        }
    }

    std::optional<Gameplay> game;
    if (play) {
        GameplayConfig config;
        config.auto_follow_sliders = true;
        game.emplace(*play, config);
    }

    // Audio: when it starts, the song becomes the clock. Otherwise the wall clock
    // (RTC) keeps the game running, just without sound.
    AudioPlayer audio;
    audio.init();
    std::string audio_path;
    if (play && !play->map.audio_filename.empty() && !map_path.empty()) {
        const size_t slash = map_path.find_last_of('/');
        const std::string dir = slash == std::string::npos ? std::string() : map_path.substr(0, slash + 1);
        audio_path = dir + play->map.audio_filename;
    }
    bool using_audio = !audio_path.empty() && audio.play(audio_path);

    // Drop autoplay.txt next to the EBOOT to watch the map play itself.
    std::string autoplay_flag;
    const bool autoplay = read_file(std::string(kAppDir) + "autoplay.txt", autoplay_flag);
    size_t autoplay_index = 0;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    SceCtrlData pad{};
    // Start the timeline so the first object reaches the playfield after 1.5 s.
    const double lead_in = 1500.0;
    const double first_object = play ? play->map.first_object_time() : 0.0;
    double start_time = now_ms() + lead_in - first_object;
    double last_song_time = first_object - lead_in;
    double rtc_anchor = now_ms();
    constexpr double kAudioOffsetMs = 0.0;  // calibration, adjustable later

    for (;;) {
        sceCtrlReadBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_HOME) break;

        const InputState input = (autoplay && game)
                                     ? autoplay_input(*game, autoplay_index, now_ms() - start_time)
                                     : read_input(pad);

        double song_time = 0.0;
        if (using_audio && audio.active()) {
            song_time = audio.position_ms() + kAudioOffsetMs;
            last_song_time = song_time;
            rtc_anchor = now_ms();
        } else if (using_audio) {
            // Song finished (or the stream died): continue on the wall clock.
            song_time = last_song_time + (now_ms() - rtc_anchor);
        } else {
            song_time = now_ms() - start_time;
        }

        if (game) {
            if (pad.Buttons & PSP_CTRL_SELECT) {
                game->reset();
                autoplay_index = 0;
                start_time = now_ms() + lead_in - first_object;
                last_song_time = first_object - lead_in;
                rtc_anchor = now_ms();
                if (using_audio) {
                    audio.stop();
                    audio.play(audio_path);
                }
            }
            game->update(song_time, input);
        }

        renderer.begin_frame(abgr(14, 14, 18));
        if (game) {
            render_gameplay(renderer, textures, *game);
        } else {
            render_demo(renderer, textures, now_ms(), input.cursor);
        }
        renderer.end_frame();
    }

    audio.shutdown();
    renderer.shutdown();
    sceKernelExitGame();
    return 0;
}