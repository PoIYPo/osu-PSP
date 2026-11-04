// CLI that loads .osu beatmaps, compiles the gameplay data and prints a summary.
// Used to sanity-check parsing/geometry against real maps without the PSP.
//
//   osu_dump <file.osu> [--objects]   full dump of one map
//   osu_dump --brief <file.osu>...    one line per map, non-zero exit on failure
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/standard.h"

using namespace osu;

namespace {

void print_full(const PlayableBeatmap& play, bool print_objects) {
    const Beatmap& bm = play.map;
    const StandardDifficulty& diff = play.difficulty;

    std::printf("%s - %s [%s]\n", bm.artist.c_str(), bm.title.c_str(), bm.version.c_str());
    std::printf("  mapper      : %s\n", bm.creator.c_str());
    std::printf("  mode        : %s\n", game_mode_name(bm.mode));
    std::printf("  format      : v%d\n", bm.format_version);
    std::printf("  audio       : %s\n", bm.audio_filename.c_str());
    std::printf("  background  : %s\n",
                bm.background_file.empty() ? "(none)" : bm.background_file.c_str());
    std::printf("  CS %.2f  AR %.2f  OD %.2f  HP %.2f  SM %.2f  tickRate %.2f\n",
                diff.circle_size, diff.approach_rate, diff.overall_difficulty, diff.hp_drain,
                diff.slider_multiplier, diff.slider_tick_rate);
    std::printf("  radius      : %.2f px   preempt %.0f ms\n", diff.circle_radius(),
                diff.preempt_ms());
    std::printf("  windows     : 300 +-%.1f  100 +-%.1f  50 +-%.1f ms\n", diff.hit_window_300(),
                diff.hit_window_100(), diff.hit_window_50());

    std::printf("  timing      : %zu points\n", bm.timing_points.size());
    std::printf("  objects     : %zu (first %.0f ms, last end %.0f ms)\n", bm.objects.size(),
                bm.first_object_time(), bm.last_object_time());

    size_t circles = 0, sliders = 0, spinners = 0, holds = 0, path_points = 0;
    for (size_t i = 0; i < bm.objects.size(); ++i) {
        switch (bm.objects[i].kind) {
            case ObjectKind::Circle: ++circles; break;
            case ObjectKind::Slider:
                ++sliders;
                path_points += play.sliders[i].path.point_count();
                break;
            case ObjectKind::Spinner: ++spinners; break;
            case ObjectKind::HoldNote: ++holds; break;
        }
    }
    std::printf("  breakdown   : %zu circles, %zu sliders, %zu spinners, %zu holds\n", circles,
                sliders, spinners, holds);
    if (sliders > 0) {
        std::printf("  slider poly : %zu points total (%.1f per slider)\n", path_points,
                    static_cast<double>(path_points) / static_cast<double>(sliders));
    }

    if (!print_objects) return;
    std::printf("\n  %-7s %8s %8s %8s %8s  %s\n", "kind", "time", "end", "x", "y", "extra");
    const size_t limit = bm.objects.size() < 40 ? bm.objects.size() : 40;
    for (size_t i = 0; i < limit; ++i) {
        const HitObject& o = bm.objects[i];
        std::printf("  %-7s %8.0f %8.0f %8.1f %8.1f", object_kind_name(o.kind), o.time, o.end_time,
                    o.pos.x, o.pos.y);
        if (o.is_slider()) {
            std::printf("  len %.1f x%d, %zu events", o.slider.pixel_length, o.slider.repeats,
                        play.sliders[i].events.size());
        }
        std::printf("\n");
    }
    if (bm.objects.size() > limit) std::printf("  ... %zu more\n", bm.objects.size() - limit);
}

void print_brief(const char* path, bool ok, const PlayableBeatmap& play, const std::string& error) {
    if (!ok) {
        std::printf("FAIL  %-40s %s\n", path, error.c_str());
        return;
    }
    const Beatmap& bm = play.map;
    size_t sliders = 0;
    for (const HitObject& o : bm.objects) {
        if (o.is_slider()) ++sliders;
    }
    const double duration = bm.last_object_time() - bm.first_object_time();
    std::printf("ok    %-40s v%-2d %-14s CS%.1f AR%.1f OD%.1f  %5zu obj %4zu sl  %6.1fs\n", path,
                bm.format_version, game_mode_name(bm.mode), play.difficulty.circle_size,
                play.difficulty.approach_rate, play.difficulty.overall_difficulty,
                bm.objects.size(), sliders, duration / 1000.0);
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<const char*> paths;
    bool brief = false;
    bool objects = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--brief") == 0) {
            brief = true;
        } else if (std::strcmp(argv[i], "--objects") == 0) {
            objects = true;
        } else {
            paths.push_back(argv[i]);
        }
    }
    if (paths.empty()) {
        std::printf("usage: osu_dump [--brief] [--objects] <file.osu>...\n");
        return 2;
    }

    int failures = 0;
    int total = 0;
    for (const char* path : paths) {
        ++total;
        Beatmap map;
        std::string error;
        if (!load_osu_file(path, map, &error)) {
            ++failures;
            if (brief) print_brief(path, false, PlayableBeatmap{}, error);
            else std::printf("failed: %s\n", error.c_str());
            continue;
        }
        PlayableBeatmap play = compile_beatmap(std::move(map));
        if (brief) print_brief(path, true, play, error);
        else print_full(play, objects);
    }

    if (brief) std::printf("\n%d/%d maps loaded\n", total - failures, total);
    return failures == 0 ? 0 : 1;
}
