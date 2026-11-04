#include <cmath>

#include "core/standard.h"
#include "game/gameplay.h"
#include "test_framework.h"

using namespace osu;

namespace {

Beatmap make_base() {
    Beatmap bm;
    bm.circle_size = 5.0f;
    bm.overall_difficulty = 8.0f;  // windows: 32 / 76 / 120
    bm.approach_rate = 9.0f;
    bm.slider_multiplier = 1.4f;
    bm.slider_tick_rate = 1.0f;
    bm.hp_drain = 5.0f;

    TimingPoint tp;
    tp.time = 0.0;
    tp.beat_length = 500.0;
    tp.uninherited = true;
    bm.timing_points.push_back(tp);
    return bm;
}

HitObject make_circle(Vec2 pos, double time) {
    HitObject o;
    o.kind = ObjectKind::Circle;
    o.pos = pos;
    o.time = time;
    o.end_time = time;
    return o;
}

HitObject make_slider(Vec2 pos, double time, float length, int repeats = 1) {
    HitObject o;
    o.kind = ObjectKind::Slider;
    o.pos = pos;
    o.time = time;
    o.slider.curve = CurveType::Linear;
    o.slider.control_points = {pos, {pos.x + length, pos.y}};
    o.slider.pixel_length = length;
    o.slider.repeats = repeats;
    return o;
}

HitObject make_spinner(Vec2 pos, double time, double end_time) {
    HitObject o;
    o.kind = ObjectKind::Spinner;
    o.pos = pos;
    o.time = time;
    o.end_time = end_time;
    return o;
}

// Steps the simulation with the given cursor/button for [from, to].
void run(Gameplay& game, double from, double to, double step, Vec2 cursor, bool pressed) {
    for (double t = from; t <= to + 1e-6; t += step) {
        InputState in;
        in.cursor = cursor;
        in.pressed = pressed;
        game.update(t, in);
    }
}

}  // namespace

TEST_CASE(circle_hit_on_time_is_300) {
    Beatmap bm = make_base();
    bm.objects.push_back(make_circle({100.0f, 100.0f}, 1000.0));
    PlayableBeatmap play = compile_beatmap(bm);
    Gameplay game(play);

    run(game, 900.0, 900.0, 10.0, {100.0f, 100.0f}, false);
    game.update(1000.0, {{100.0f, 100.0f}, true});

    CHECK(game.state(0) == ObjectState::Hit);
    CHECK_EQ(game.stats().count_300, 1);
    CHECK_EQ(game.stats().combo, 1);
    CHECK_NEAR(game.accuracy(), 1.0, 1e-9);
    CHECK(game.stats().score > 0);
}

TEST_CASE(late_hit_within_window_is_100) {
    Beatmap bm = make_base();
    bm.objects.push_back(make_circle({100.0f, 100.0f}, 1000.0));
    PlayableBeatmap play = compile_beatmap(bm);
    Gameplay game(play);

    game.update(900.0, {{100.0f, 100.0f}, false});
    game.update(1060.0, {{100.0f, 100.0f}, true});  // 60 ms late, window100 = 76

    CHECK(game.state(0) == ObjectState::Hit);
    CHECK_EQ(game.stats().count_100, 1);
    CHECK_EQ(game.stats().count_300, 0);
}

TEST_CASE(circle_is_missed_after_window) {
    Beatmap bm = make_base();
    bm.objects.push_back(make_circle({100.0f, 100.0f}, 1000.0));
    PlayableBeatmap play = compile_beatmap(bm);
    Gameplay game(play);

    run(game, 900.0, 1130.0, 10.0, {300.0f, 300.0f}, false);

    CHECK(game.state(0) == ObjectState::Missed);
    CHECK_EQ(game.stats().count_miss, 1);
    CHECK_EQ(game.stats().combo, 0);
    CHECK_NEAR(game.accuracy(), 0.0, 1e-9);
}

TEST_CASE(press_outside_the_circle_does_not_hit) {
    Beatmap bm = make_base();
    bm.objects.push_back(make_circle({100.0f, 100.0f}, 1000.0));
    PlayableBeatmap play = compile_beatmap(bm);
    Gameplay game(play);

    game.update(900.0, {{300.0f, 300.0f}, false});
    game.update(1000.0, {{300.0f, 300.0f}, true});

    CHECK(game.state(0) == ObjectState::Pending);
    CHECK_EQ(game.stats().count_300, 0);
}

TEST_CASE(slider_ticks_while_holding) {
    Beatmap bm = make_base();
    bm.objects.push_back(make_slider({100.0f, 100.0f}, 1000.0, 400.0f));
    PlayableBeatmap play = compile_beatmap(bm);
    Gameplay game(play);

    // Head hit at 1000, then hold until the slider ends at 2428.6.
    for (double t = 950.0; t <= 2600.0; t += 10.0) {
        InputState in;
        in.cursor = {100.0f, 100.0f};
        in.pressed = t >= 998.0;
        game.update(t, in);
    }

    CHECK(game.state(0) == ObjectState::Hit);
    CHECK_EQ(game.stats().count_300, 1);
    CHECK_EQ(game.stats().ticks, 4);  // 3 ticks + tail
    CHECK_EQ(game.stats().combo, 5);  // head + 4 events
}

TEST_CASE(slider_breaks_when_button_released) {
    Beatmap bm = make_base();
    bm.objects.push_back(make_slider({100.0f, 100.0f}, 1000.0, 400.0f));
    PlayableBeatmap play = compile_beatmap(bm);
    Gameplay game(play);

    for (double t = 950.0; t <= 1500.0; t += 10.0) {
        InputState in;
        in.cursor = {100.0f, 100.0f};
        in.pressed = t >= 998.0 && t <= 1100.0;
        game.update(t, in);
    }

    CHECK(game.slider(0).broken);
    CHECK_EQ(game.stats().ticks, 0);
    CHECK_EQ(game.stats().combo, 0);
}

TEST_CASE(spinner_fills_with_enough_rotation) {
    Beatmap bm = make_base();
    bm.objects.push_back(make_spinner({256.0f, 192.0f}, 1000.0, 3000.0));
    PlayableBeatmap play = compile_beatmap(bm);
    Gameplay game(play);

    // 3 turns per second over a 2 s spinner; 2 turns/s are required.
    for (double t = 900.0; t <= 3100.0; t += 16.0) {
        InputState in;
        const double progress = std::clamp((t - 1000.0) / 2000.0, 0.0, 1.0);
        const float angle = static_cast<float>(progress * 2.0 * kPi * 6.0);
        in.cursor = {256.0f + std::cos(angle) * 90.0f, 192.0f + std::sin(angle) * 90.0f};
        in.pressed = false;
        game.update(t, in);
    }

    CHECK(game.state(0) == ObjectState::Hit);
    CHECK_EQ(game.stats().count_300, 1);
}

TEST_CASE(accuracy_mixes_hits_and_misses) {
    Beatmap bm = make_base();
    bm.objects.push_back(make_circle({100.0f, 100.0f}, 1000.0));
    bm.objects.push_back(make_circle({200.0f, 100.0f}, 2000.0));
    PlayableBeatmap play = compile_beatmap(bm);
    Gameplay game(play);

    game.update(900.0, {{100.0f, 100.0f}, false});
    game.update(1000.0, {{100.0f, 100.0f}, true});
    game.update(1020.0, {{100.0f, 100.0f}, false});
    run(game, 1100.0, 2200.0, 10.0, {400.0f, 300.0f}, false);

    CHECK_EQ(game.stats().count_300, 1);
    CHECK_EQ(game.stats().count_miss, 1);
    CHECK_NEAR(game.accuracy(), 0.5, 1e-9);
    CHECK_EQ(game.stats().max_combo, 1);
}

TEST_CASE(health_drains_over_time) {
    Beatmap bm = make_base();
    bm.objects.push_back(make_circle({100.0f, 100.0f}, 100000.0));  // keeps the map open
    PlayableBeatmap play = compile_beatmap(bm);
    Gameplay game(play);

    run(game, 0.0, 8000.0, 100.0, {256.0f, 192.0f}, false);

    CHECK(game.stats().hp < 100.0);
    CHECK(!game.stats().failed);
}
