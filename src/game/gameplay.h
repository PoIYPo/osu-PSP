// osu!standard gameplay simulation: judging, combo, accuracy, health, sliders
// (with the PSP-specific auto-follow) and spinners.
//
// The class is pure logic: it takes the song time and an InputState and mutates
// its state. No rendering, no audio, no OS calls -- which is what makes it
// testable on the host and reusable by both the software renderer and the PSP.
#pragma once

#include <cstdint>
#include <vector>

#include "core/standard.h"
#include "game/input.h"

namespace osu {

enum class ObjectState : uint8_t {
    Pending,
    Hit,
    Missed,
};

struct SliderRuntime {
    bool following = false;
    bool broken = false;      // button released before the slider ended
    int next_event = 0;       // index of the next unprocessed tick/repeat/tail
    float progress = 0.0f;    // 0..1 along the whole slider (for rendering)
};

struct GameplayConfig {
    // PSP adaptation: once the head is hit, the cursor rides the path by itself.
    bool auto_follow_sliders = true;
    // Assist: snap radius as a fraction of the circle radius (0 = disabled).
    float magnet_scale = 0.0f;
    // Multiplier for the hit windows (1.0 = vanilla osu!).
    double window_scale = 1.0;
    // Spinner speed needed to fill it; osu!'s exact rule is still to be matched.
    double spinner_rotations_per_second = 2.0;
};

struct GameplayStats {
    int combo = 0;
    int max_combo = 0;
    int count_300 = 0;
    int count_100 = 0;
    int count_50 = 0;
    int count_miss = 0;
    int ticks = 0;        // slider ticks / repeats / tails collected
    int score = 0;
    double hp = 100.0;    // 0..100
    bool failed = false;
    bool completed = false;
};

class Gameplay {
public:
    Gameplay(const PlayableBeatmap& map, GameplayConfig config = {});

    void reset();

    // Advances the simulation to an absolute song time. Times must be
    // monotonically non-decreasing; earlier values are ignored.
    void update(double song_time_ms, const InputState& input);

    const PlayableBeatmap& map() const { return *map_; }
    const GameplayConfig& config() const { return config_; }
    const GameplayStats& stats() const { return stats_; }

    double time() const { return time_; }
    Vec2 cursor() const { return cursor_; }
    bool finished() const { return finished_; }

    size_t object_count() const { return states_.size(); }
    ObjectState state(size_t index) const { return states_[index]; }
    const SliderRuntime& slider(size_t index) const { return sliders_[index]; }

    // 0..1 accuracy in the osu! sense (300/100/50, ticks excluded).
    double accuracy() const;

    // How far a spinner has been filled (0..1), for rendering.
    float spinner_progress(size_t index) const;

private:
    void apply_magnet(const InputState& input);
    void try_hit(double song_time);
    void update_sliders(double song_time, const InputState& input);
    void update_spinner(double song_time, const InputState& input);
    void update_misses(double song_time);
    void update_health(double song_time);

    void judge_object(size_t index, Judgement judgement);
    void add_combo_hit(int base_value);
    void break_combo();

    [[nodiscard]] bool in_range_of(size_t index, Vec2 cursor) const;

    const PlayableBeatmap* map_ = nullptr;
    GameplayConfig config_;
    StandardDifficulty difficulty_;

    std::vector<ObjectState> states_;
    std::vector<SliderRuntime> sliders_;
    std::vector<float> spinner_fill_;  // 0..1 per object, only for spinners

    GameplayStats stats_;
    double time_ = 0.0;
    double last_health_time_ = 0.0;
    size_t next_index_ = 0;  // first object that is neither hit nor missed
    size_t active_spinner_ = static_cast<size_t>(-1);
    float spinner_rotations_ = 0.0f;
    float spinner_last_angle_ = 0.0f;
    bool spinner_has_angle_ = false;

    Vec2 cursor_;
    Vec2 prev_cursor_;
    bool prev_pressed_ = false;
    bool finished_ = false;
};

}  // namespace osu
