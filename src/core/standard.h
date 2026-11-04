// osu!standard rules: difficulty scaling, hit windows, slider timing and ticks.
#pragma once

#include <vector>

#include "beatmap.h"
#include "slider_path.h"
#include "timing.h"

namespace osu {

// Difficulty settings after defaults resolution (AR falls back to OD).
struct StandardDifficulty {
    float circle_size = 5.0f;
    float approach_rate = 5.0f;
    float overall_difficulty = 5.0f;
    float hp_drain = 5.0f;
    float slider_multiplier = 1.4f;
    float slider_tick_rate = 1.0f;

    static StandardDifficulty from(const Beatmap& map);

    // Radius of a hit circle in osu! pixels (playfield is 512x384).
    float circle_radius() const;

    // How long a hit object is visible before its hit time, in ms.
    double preempt_ms() const;

    // Half-widths of the hit windows in ms. A hit at |delta| <= window earns at
    // least that judgement.
    double hit_window_300() const;
    double hit_window_100() const;
    double hit_window_50() const;
};

enum class Judgement {
    None,
    Hit300,
    Hit100,
    Hit50,
    Miss,
};

// Maps a signed timing error (hit_time - object_time) to a judgement.
Judgement judge_hit(const StandardDifficulty& diff, double delta_ms);

enum class SliderEventKind : uint8_t {
    Tick,    // slider tick: worth 10 points, keeps combo alive
    Repeat,  // repeat arrow at the end of a span
    Tail,    // final slider end
};

struct SliderEvent {
    double time = 0.0;
    float distance = 0.0f;  // along the path, from the slider head
    Vec2 pos;
    SliderEventKind kind = SliderEventKind::Tick;
};

struct SliderGeometry {
    SliderPath path;
    double span_duration = 0.0;   // ms for a single span
    double total_duration = 0.0;  // span_duration * repeats
    std::vector<SliderEvent> events;
};

struct PlayableBeatmap {
    Beatmap map;
    StandardDifficulty difficulty;
    TimingInfo timing;
    std::vector<SliderGeometry> sliders;   // parallel to map.objects (empty for non-sliders)
    std::vector<double> object_end_times;  // parallel to map.objects
};

// Computes slider geometry, tick/end times and object end times. Takes the
// beatmap by value: the result owns everything it needs.
PlayableBeatmap compile_beatmap(Beatmap map);

}  // namespace osu
