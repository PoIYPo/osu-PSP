// Headless gameplay driver: plays a beatmap with a scripted "perfect" player,
// prints the result and dumps reference frames as PNG.
//
//   osu_play <map.osu> [--frames N] [--out DIR] [--step MS]
//
// This is how gameplay is verified without the PSP: the numbers prove the
// judging, the PNGs prove the rendering.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "game/gameplay.h"
#include "render/png.h"
#include "render/scene.h"

using namespace osu;

namespace {

double arg_double(int argc, char** argv, const char* name, double fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0) return std::atof(argv[i + 1]);
    }
    return fallback;
}

std::string arg_string(int argc, char** argv, const char* name, const char* fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0) return argv[i + 1];
    }
    return fallback;
}

InputState autoplay_input(const PlayableBeatmap& play, const Gameplay& game, size_t index, double t,
                          bool holding_slider) {
    InputState in;
    const auto& objects = play.map.objects;

    // While a slider is being followed the button stays down and the cursor is
    // whatever the auto-follow logic produced -- otherwise the drive would drop
    // the slider the moment the head is registered as hit.
    if (holding_slider) {
        in.cursor = game.cursor();
        in.pressed = true;
        return in;
    }

    if (index >= objects.size()) {
        in.cursor = {kPlayfieldWidth * 0.5f, kPlayfieldHeight * 0.5f};
        return in;
    }

    const HitObject& obj = objects[index];
    switch (obj.kind) {
        case ObjectKind::Spinner: {
            const double duration = std::max(1.0, obj.end_time - obj.time);
            const double progress = std::clamp((t - obj.time) / duration, 0.0, 1.0);
            const float angle = static_cast<float>(progress * 2.0 * kPi * 3.0);  // 3 turns/second
            in.cursor = {obj.pos.x + std::cos(angle) * 90.0f,
                         obj.pos.y + std::sin(angle) * 90.0f};
            in.pressed = false;
            break;
        }
        case ObjectKind::Slider:
            in.cursor = obj.pos;
            in.pressed = t >= obj.time - 2.0;
            break;
        case ObjectKind::Circle:
        default:
            in.cursor = obj.pos;
            in.pressed = t >= obj.time - 2.0 && t <= obj.time + 40.0;
            break;
    }
    return in;
}

}  // namespace

int main(int argc, char** argv) {
    const char* path = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '-') {
            path = argv[i];
            break;
        }
    }
    if (!path) {
        std::printf("usage: osu_play <map.osu> [--frames N] [--out DIR] [--step MS]\n");
        return 2;
    }

    const int frame_count = static_cast<int>(arg_double(argc, argv, "--frames", 8.0));
    const std::string out_dir = arg_string(argc, argv, "--out", "frames");
    const double step = arg_double(argc, argv, "--step", 2.0);

    Beatmap bm;
    std::string error;
    if (!load_osu_file(path, bm, &error)) {
        std::printf("failed: %s\n", error.c_str());
        return 1;
    }
    PlayableBeatmap play = compile_beatmap(std::move(bm));

    GameplayConfig cfg;
    cfg.auto_follow_sliders = true;
    Gameplay game(play, cfg);

    const double start = std::max(0.0, play.map.first_object_time() - 1200.0);
    const double end = play.map.last_object_time() + 800.0;

    std::vector<double> shot_times;
    const double at_time = arg_double(argc, argv, "--at", -1.0);
    if (at_time >= 0.0) {
        shot_times.push_back(at_time);
    } else {
        for (int i = 0; i < frame_count; ++i) {
            const double frac = (frame_count == 1) ? 0.0 : static_cast<double>(i) / (frame_count - 1);
            shot_times.push_back(start + (end - start) * frac);
        }
    }

    if (!out_dir.empty()) std::filesystem::create_directories(out_dir);

    Framebuffer fb(kScreenWidth, kScreenHeight);
    int frame_index = 0;
    size_t cursor_object = 0;

    for (double t = start; t <= end; t += step) {
        if (at_time >= 0.0 && t < 0.0) continue;
        while (cursor_object < play.map.objects.size() &&
               game.state(cursor_object) != ObjectState::Pending) {
            ++cursor_object;
        }
        bool holding = false;
        for (size_t i = 0; i < play.map.objects.size(); ++i) {
            if (game.slider(i).following) {
                holding = true;
                break;
            }
        }
        game.update(t, autoplay_input(play, game, cursor_object, t, holding));

        if (frame_index < frame_count && t >= shot_times[frame_index]) {
            render_scene(fb, game);
            char name[512];
            std::snprintf(name, sizeof(name), "%s/frame_%02d.png", out_dir.c_str(), frame_index);
            if (!write_png_rgba(name, fb.pixels(), fb.width(), fb.height())) {
                std::printf("could not write %s\n", name);
            }
            ++frame_index;
        }
        if (game.finished()) break;
    }

    // Floating point stepping can stop just short of the last shot time; render
    // the remaining frames so the output always has the requested count.
    while (frame_index < frame_count) {
        render_scene(fb, game);
        char name[512];
        std::snprintf(name, sizeof(name), "%s/frame_%02d.png", out_dir.c_str(), frame_index);
        write_png_rgba(name, fb.pixels(), fb.width(), fb.height());
        ++frame_index;
    }

    const GameplayStats& s = game.stats();
    std::printf("%s - %s [%s]\n", play.map.artist.c_str(), play.map.title.c_str(),
                play.map.version.c_str());
    std::printf("  objects     : %zu\n", play.map.objects.size());
    std::printf("  result      : 300 x%d, 100 x%d, 50 x%d, miss x%d, ticks %d\n", s.count_300,
                s.count_100, s.count_50, s.count_miss, s.ticks);
    std::printf("  accuracy    : %.2f%%\n", game.accuracy() * 100.0);
    std::printf("  max combo   : %d\n", s.max_combo);
    std::printf("  score       : %d\n", s.score);
    std::printf("  health      : %.1f%s\n", s.hp, s.failed ? " (failed)" : "");
    std::printf("  frames      : %d -> %s/\n", frame_index, out_dir.c_str());
    return 0;
}
