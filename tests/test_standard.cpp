#include <string>

#include "core/standard.h"
#include "test_framework.h"

using namespace osu;

namespace {

Beatmap base_map() {
    Beatmap bm;
    bm.circle_size = 5.0f;
    bm.overall_difficulty = 8.0f;
    bm.approach_rate = 9.0f;
    bm.slider_multiplier = 1.4f;
    bm.slider_tick_rate = 1.0f;

    TimingPoint tp;
    tp.time = 0.0;
    tp.beat_length = 500.0;  // 120 BPM
    tp.uninherited = true;
    bm.timing_points.push_back(tp);
    return bm;
}

}  // namespace

TEST_CASE(difficulty_scaling) {
    StandardDifficulty diff = StandardDifficulty::from(base_map());
    CHECK_NEAR(diff.circle_radius(), 32.0, 1e-3);   // CS5
    CHECK_NEAR(diff.preempt_ms(), 600.0, 1e-6);     // AR9
    CHECK_NEAR(diff.hit_window_300(), 32.0, 1e-6);  // OD8: 80 - 48
    CHECK_NEAR(diff.hit_window_100(), 76.0, 1e-6);  // OD8: 140 - 64
    CHECK_NEAR(diff.hit_window_50(), 120.0, 1e-6);  // OD8: 200 - 80

    Beatmap hard = base_map();
    hard.circle_size = 0.0f;
    hard.approach_rate = 0.0f;
    hard.overall_difficulty = 10.0f;
    StandardDifficulty d2 = StandardDifficulty::from(hard);
    CHECK_NEAR(d2.circle_radius(), 54.4, 1e-3);
    CHECK_NEAR(d2.preempt_ms(), 1800.0, 1e-6);  // AR0
    CHECK_NEAR(d2.hit_window_300(), 20.0, 1e-6);
}

TEST_CASE(approach_rate_falls_back_to_od) {
    Beatmap bm = base_map();
    bm.approach_rate = -1.0f;
    bm.overall_difficulty = 8.0f;
    StandardDifficulty diff = StandardDifficulty::from(bm);
    CHECK_NEAR(diff.approach_rate, 8.0, 1e-6);
    CHECK_NEAR(diff.preempt_ms(), 750.0, 1e-6);  // 1200 - 150 * 3
}

TEST_CASE(judgement_boundaries) {
    StandardDifficulty diff = StandardDifficulty::from(base_map());
    CHECK(judge_hit(diff, 0.0) == Judgement::Hit300);
    CHECK(judge_hit(diff, 32.0) == Judgement::Hit300);
    CHECK(judge_hit(diff, 32.1) == Judgement::Hit100);
    CHECK(judge_hit(diff, -76.0) == Judgement::Hit100);
    CHECK(judge_hit(diff, 76.1) == Judgement::Hit50);
    CHECK(judge_hit(diff, 120.0) == Judgement::Hit50);
    CHECK(judge_hit(diff, 120.1) == Judgement::Miss);
}

TEST_CASE(slider_timing_and_ticks) {
    Beatmap bm = base_map();
    HitObject slider;
    slider.kind = ObjectKind::Slider;
    slider.pos = {100.0f, 100.0f};
    slider.time = 1000.0;
    slider.slider.curve = CurveType::Linear;
    slider.slider.control_points = {{100.0f, 100.0f}, {500.0f, 100.0f}};
    slider.slider.pixel_length = 400.0f;
    slider.slider.repeats = 2;
    bm.objects.push_back(slider);

    PlayableBeatmap play = compile_beatmap(bm);
    CHECK_EQ(play.sliders.size(), 1u);
    if (play.sliders.empty()) return;

    const SliderGeometry& geo = play.sliders[0];
    // velocity = 100 * 1.4 / 500 = 0.28 px/ms -> span = 400 / 0.28
    CHECK_NEAR(geo.span_duration, 1428.5714, 1e-2);
    CHECK_NEAR(geo.total_duration, 2857.1429, 1e-2);
    CHECK_NEAR(play.map.objects[0].end_time, 3857.1429, 1e-2);
    CHECK_NEAR(play.object_end_times[0], 3857.1429, 1e-2);

    // 3 ticks per span (every 100 px along a 400 px path) + repeat + tail.
    CHECK_EQ(geo.events.size(), 8u);
    if (geo.events.size() >= 8) {
        CHECK(geo.events[0].kind == SliderEventKind::Tick);
        CHECK_NEAR(geo.events[0].time, 1000.0 + 100.0 / 0.28, 1e-2);
        CHECK_NEAR(geo.events[0].distance, 100.0, 1e-3);

        // Events are emitted per span: ticks first, then that span's end marker.
        CHECK(geo.events[3].kind == SliderEventKind::Repeat);
        CHECK_NEAR(geo.events[3].time, 1000.0 + 1428.5714, 1e-2);
        CHECK_NEAR(geo.events[3].distance, 400.0, 1e-3);

        // Second span runs backwards, so its first tick is 300 px along the path.
        CHECK_NEAR(geo.events[4].time, 1000.0 + 1428.5714 + 100.0 / 0.28, 1e-2);
        CHECK_NEAR(geo.events[4].distance, 300.0, 1e-3);

        CHECK(geo.events[7].kind == SliderEventKind::Tail);
        CHECK_NEAR(geo.events[7].time, 3857.1429, 1e-2);
    }
}

TEST_CASE(slider_velocity_multiplier_changes_duration) {
    Beatmap bm = base_map();
    TimingPoint sv;
    sv.time = 0.0;
    sv.beat_length = -50.0;  // 2x
    sv.uninherited = false;
    bm.timing_points.push_back(sv);

    HitObject slider;
    slider.kind = ObjectKind::Slider;
    slider.pos = {0.0f, 0.0f};
    slider.time = 0.0;
    slider.slider.control_points = {{0.0f, 0.0f}, {400.0f, 0.0f}};
    slider.slider.pixel_length = 400.0f;
    bm.objects.push_back(slider);

    PlayableBeatmap play = compile_beatmap(bm);
    CHECK_EQ(play.sliders.size(), 1u);
    if (play.sliders.empty()) return;
    // Twice the velocity -> half the duration.
    CHECK_NEAR(play.sliders[0].span_duration, 714.2857, 1e-1);
}

TEST_CASE(spinner_keeps_authored_end_time) {
    Beatmap bm = base_map();
    HitObject spinner;
    spinner.kind = ObjectKind::Spinner;
    spinner.time = 1000.0;
    spinner.end_time = 4000.0;
    bm.objects.push_back(spinner);

    PlayableBeatmap play = compile_beatmap(bm);
    CHECK_NEAR(play.object_end_times[0], 4000.0, 1e-9);
    CHECK(play.sliders[0].path.empty());
}

TEST_CASE(stacked_objects_get_diagonal_offsets) {
    Beatmap bm = base_map();
    for (int i = 0; i < 3; ++i) {
        HitObject circle;
        circle.kind = ObjectKind::Circle;
        circle.pos = {200.0f, 150.0f};
        circle.time = 1000.0 + i * 100.0;  // inside the 420 ms stack window at AR9
        bm.objects.push_back(circle);
    }

    PlayableBeatmap play = compile_beatmap(bm);
    CHECK_EQ(play.map.objects.size(), 3u);
    if (play.map.objects.size() != 3) return;

    CHECK_EQ(play.map.objects[0].stack_height, 0);
    CHECK_EQ(play.map.objects[1].stack_height, 1);
    CHECK_EQ(play.map.objects[2].stack_height, 2);

    // Each level shifts the note diagonally so the pile becomes a small cascade.
    CHECK_NEAR(play.map.objects[1].pos.x, 220.0, 1e-3);
    CHECK_NEAR(play.map.objects[1].pos.y, 170.0, 1e-3);
    CHECK_NEAR(play.map.objects[2].pos.x, 240.0, 1e-3);
    CHECK_NEAR(play.map.objects[2].pos.y, 190.0, 1e-3);
}

TEST_CASE(distant_objects_are_not_stacked) {
    Beatmap bm = base_map();
    HitObject a;
    a.kind = ObjectKind::Circle;
    a.pos = {100.0f, 100.0f};
    a.time = 1000.0;
    HitObject b = a;
    b.pos = {300.0f, 200.0f};
    b.time = 1050.0;
    bm.objects.push_back(a);
    bm.objects.push_back(b);

    PlayableBeatmap play = compile_beatmap(bm);
    CHECK_EQ(play.map.objects[0].stack_height, 0);
    CHECK_EQ(play.map.objects[1].stack_height, 0);
    CHECK_NEAR(play.map.objects[1].pos.x, 300.0, 1e-3);
}

TEST_CASE(stacking_also_shifts_slider_control_points) {
    Beatmap bm = base_map();
    HitObject base;
    base.kind = ObjectKind::Circle;
    base.pos = {200.0f, 150.0f};
    base.time = 1000.0;
    bm.objects.push_back(base);

    HitObject slider;
    slider.kind = ObjectKind::Slider;
    slider.pos = {200.0f, 150.0f};
    slider.time = 1100.0;
    slider.slider.control_points = {{200.0f, 150.0f}, {300.0f, 150.0f}};
    slider.slider.pixel_length = 100.0f;
    bm.objects.push_back(slider);

    PlayableBeatmap play = compile_beatmap(bm);
    if (play.map.objects.size() < 2) return;
    CHECK_EQ(play.map.objects[1].stack_height, 1);
    const Vec2& head = play.map.objects[1].slider.control_points[0];
    CHECK_NEAR(head.x, 220.0, 1e-3);
    CHECK_NEAR(head.y, 170.0, 1e-3);
}

TEST_CASE(circle_end_time_equals_start) {
    Beatmap bm = base_map();
    HitObject circle;
    circle.kind = ObjectKind::Circle;
    circle.time = 1234.0;
    bm.objects.push_back(circle);

    PlayableBeatmap play = compile_beatmap(bm);
    CHECK_NEAR(play.object_end_times[0], 1234.0, 1e-9);
}
