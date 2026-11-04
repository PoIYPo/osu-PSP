#include "standard.h"

#include <algorithm>
#include <cmath>

namespace osu {

namespace {

float clamp_difficulty(float v) { return std::clamp(v, 0.0f, 10.0f); }

// Slider ticks are laid out at a fixed distance along the path, so their time
// spacing scales with slider velocity. This is why slow sliders end up with fewer
// ticks and fast ones with more -- exactly as in osu!.
constexpr double kTickDistanceBase = 100.0;
constexpr double kEpsilon = 0.01;

void build_events(SliderGeometry& geoms, const HitObject& obj, double velocity_px_per_ms,
                  float length, double tick_rate) {
    const int repeats = std::max(1, obj.slider.repeats);
    const double span = geoms.span_duration;

    double tick_distance = 0.0;
    if (tick_rate > 0.0) tick_distance = kTickDistanceBase / tick_rate;

    geoms.events.clear();
    if (velocity_px_per_ms <= 0.0 || length <= 0.0f) {
        // Degenerate slider: only the tail exists.
        if (repeats > 0) {
            geoms.events.push_back({obj.time + span * repeats, length, obj.pos,
                                    SliderEventKind::Tail});
        }
        return;
    }

    for (int span_index = 0; span_index < repeats; ++span_index) {
        const double span_start = obj.time + span * span_index;
        const bool reversed = (span_index % 2) == 1;

        if (tick_distance > 0.0) {
            for (double d = tick_distance; d < static_cast<double>(length) - kEpsilon;
                 d += tick_distance) {
                const double t = span_start + d / velocity_px_per_ms;
                const float path_distance = reversed ? length - static_cast<float>(d)
                                                     : static_cast<float>(d);
                geoms.events.push_back({t, path_distance, geoms.path.position_at_distance(path_distance),
                                        SliderEventKind::Tick});
            }
        }

        const bool is_last_span = span_index + 1 == repeats;
        const float end_distance = reversed ? 0.0f : length;
        geoms.events.push_back({span_start + span, end_distance,
                                geoms.path.position_at_distance(end_distance),
                                is_last_span ? SliderEventKind::Tail : SliderEventKind::Repeat});
    }
}

}  // namespace

StandardDifficulty StandardDifficulty::from(const Beatmap& map) {
    StandardDifficulty diff;
    diff.circle_size = clamp_difficulty(map.circle_size);
    diff.overall_difficulty = clamp_difficulty(map.overall_difficulty);
    // ApproachRate falls back to OverallDifficulty when the file does not set it.
    diff.approach_rate = clamp_difficulty(map.effective_approach_rate());
    diff.hp_drain = clamp_difficulty(map.hp_drain);
    diff.slider_multiplier = map.slider_multiplier > 0.0f ? map.slider_multiplier : 1.4f;
    diff.slider_tick_rate = map.slider_tick_rate > 0.0f ? map.slider_tick_rate : 1.0f;
    return diff;
}

float StandardDifficulty::circle_radius() const {
    // osu! standard: diameter shrinks by 4.48 px per point of Circle Size.
    const float radius = 54.4f - 4.48f * circle_size;
    return std::max(radius, 1.0f);
}

double StandardDifficulty::preempt_ms() const {
    const double ar = approach_rate;
    return ar < 5.0 ? 1800.0 - 120.0 * ar : 1200.0 - 150.0 * (ar - 5.0);
}

double StandardDifficulty::hit_window_300() const {
    return std::max(80.0 - 6.0 * overall_difficulty, 1.0);
}

double StandardDifficulty::hit_window_100() const {
    return std::max(140.0 - 8.0 * overall_difficulty, hit_window_300());
}

double StandardDifficulty::hit_window_50() const {
    return std::max(200.0 - 10.0 * overall_difficulty, hit_window_100());
}

Judgement judge_hit(const StandardDifficulty& diff, double delta_ms) {
    const double d = std::fabs(delta_ms);
    if (d <= diff.hit_window_300()) return Judgement::Hit300;
    if (d <= diff.hit_window_100()) return Judgement::Hit100;
    if (d <= diff.hit_window_50()) return Judgement::Hit50;
    return Judgement::Miss;
}

namespace {

// osu!'s stacking: notes that land on almost the same spot within the stack
// window are drawn as a diagonal cascade instead of an unreadable pile.
void apply_stacking(Beatmap& map, const StandardDifficulty& diff) {
    const double window = diff.preempt_ms() * map.stack_leniency;
    constexpr float kSameSpot = 3.0f;   // osu pixels
    // 9 osu pixels is a bit more than desktop osu! uses: the PSP screen is only
    // 480x272 and the default offset was still unreadable on a 40 px circle.
    constexpr float kOffset = 20.0f;    // osu pixels per stack level

    const size_t count = map.objects.size();
    for (size_t i = 0; i < count; ++i) {
        HitObject& base = map.objects[i];
        if (base.kind == ObjectKind::Spinner || base.stack_height != 0) continue;

        int height = 0;
        for (size_t j = i + 1; j < count; ++j) {
            HitObject& other = map.objects[j];
            if (other.time - base.time > window) break;
            if (other.kind == ObjectKind::Spinner) continue;
            if (distance(base.pos, other.pos) > kSameSpot) continue;
            other.stack_height = ++height;
        }
    }

    for (HitObject& obj : map.objects) {
        if (obj.stack_height == 0) continue;
        const float shift = kOffset * static_cast<float>(obj.stack_height);
        obj.pos.x += shift;
        obj.pos.y += shift;
        for (Vec2& point : obj.slider.control_points) {
            point.x += shift;
            point.y += shift;
        }
    }
}

}  // namespace

PlayableBeatmap compile_beatmap(Beatmap map) {
    PlayableBeatmap out;
    out.difficulty = StandardDifficulty::from(map);
    out.timing.build(map.timing_points);
    out.map = std::move(map);
    apply_stacking(out.map, out.difficulty);

    const size_t count = out.map.objects.size();
    out.sliders.resize(count);
    out.object_end_times.resize(count, 0.0);

    int combo_index = 0;
    for (size_t i = 0; i < count; ++i) {
        HitObject& obj = out.map.objects[i];

        if (obj.new_combo || obj.kind == ObjectKind::Spinner) combo_index = 0;
        obj.number = ++combo_index;

        switch (obj.kind) {
            case ObjectKind::Circle:
                obj.end_time = obj.time;
                break;

            case ObjectKind::Spinner:
            case ObjectKind::HoldNote:
                // end_time comes straight from the file
                break;

            case ObjectKind::Slider: {
                SliderGeometry& geo = out.sliders[i];
                geo.path.build(obj.slider.control_points, obj.slider.curve, obj.slider.pixel_length);

                const double beat = out.timing.uninherited_beat_length(obj.time);
                const double sv = out.timing.slider_velocity(obj.time);
                // 100 px per beat at slider multiplier 1, then scaled by SV.
                const double velocity = 100.0 * out.difficulty.slider_multiplier * sv / beat;

                double length = obj.slider.pixel_length;
                if (length <= 0.0) length = geo.path.total_length();

                geo.span_duration = velocity > 0.0 ? length / velocity : 0.0;
                if (!std::isfinite(geo.span_duration) || geo.span_duration < 0.0) geo.span_duration = 0.0;
                geo.total_duration = geo.span_duration * std::max(1, obj.slider.repeats);

                obj.end_time = obj.time + geo.total_duration;
                build_events(geo, obj, velocity, static_cast<float>(length),
                             out.difficulty.slider_tick_rate);
                break;
            }
        }

        out.object_end_times[i] = obj.end_time;
    }

    return out;
}

}  // namespace osu
