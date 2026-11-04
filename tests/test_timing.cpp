#include "core/timing.h"
#include "test_framework.h"

using namespace osu;

namespace {

std::vector<TimingPoint> make_points() {
    std::vector<TimingPoint> pts;

    TimingPoint a;
    a.time = 0.0;
    a.beat_length = 500.0;  // 120 BPM
    a.uninherited = true;
    pts.push_back(a);

    TimingPoint sv;
    sv.time = 1000.0;
    sv.beat_length = -50.0;  // 2x slider velocity
    sv.uninherited = false;
    pts.push_back(sv);

    TimingPoint b;
    b.time = 2000.0;
    b.beat_length = 400.0;  // 150 BPM
    b.uninherited = true;
    pts.push_back(b);

    TimingPoint sv2;
    sv2.time = 3000.0;
    sv2.beat_length = -200.0;  // 0.5x slider velocity
    sv2.uninherited = false;
    pts.push_back(sv2);

    return pts;
}

}  // namespace

TEST_CASE(bpm_follows_uninherited_points) {
    TimingInfo t;
    t.build(make_points());
    CHECK_NEAR(t.bpm(0.0), 120.0, 1e-9);
    CHECK_NEAR(t.bpm(1500.0), 120.0, 1e-9);
    CHECK_NEAR(t.bpm(2000.0), 150.0, 1e-9);
    CHECK_NEAR(t.bpm(9999.0), 150.0, 1e-9);
}

TEST_CASE(beat_length_before_first_point_uses_first_point) {
    TimingInfo t;
    t.build(make_points());
    CHECK_NEAR(t.uninherited_beat_length(-5000.0), 500.0, 1e-9);
}

TEST_CASE(slider_velocity_follows_inherited_points) {
    TimingInfo t;
    t.build(make_points());
    CHECK_NEAR(t.slider_velocity(0.0), 1.0, 1e-9);
    CHECK_NEAR(t.slider_velocity(999.0), 1.0, 1e-9);
    CHECK_NEAR(t.slider_velocity(1000.0), 2.0, 1e-9);
    CHECK_NEAR(t.slider_velocity(2999.0), 2.0, 1e-9);
    CHECK_NEAR(t.slider_velocity(3000.0), 0.5, 1e-9);
}

TEST_CASE(empty_timing_uses_defaults) {
    TimingInfo t;
    t.build({});
    CHECK(t.empty());
    CHECK_NEAR(t.uninherited_beat_length(0.0), 500.0, 1e-9);
    CHECK_NEAR(t.slider_velocity(0.0), 1.0, 1e-9);
}

TEST_CASE(meter_is_taken_from_uninherited_point) {
    std::vector<TimingPoint> pts = make_points();
    pts[2].meter = 3;  // the 150 BPM section is in 3/4
    TimingInfo t;
    t.build(pts);
    CHECK_EQ(t.meter(0.0), 4);
    CHECK_EQ(t.meter(2500.0), 3);
}
