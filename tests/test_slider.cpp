#include <cmath>
#include <vector>

#include "core/slider_path.h"
#include "test_framework.h"

using namespace osu;

namespace {

Vec2 quad_bezier(Vec2 a, Vec2 b, Vec2 c, float t) {
    const float u = 1.0f - t;
    return a * (u * u) + b * (2.0f * u * t) + c * (t * t);
}

float distance_to_quad_bezier(Vec2 p, Vec2 a, Vec2 b, Vec2 c) {
    float best = 1e9f;
    for (int i = 0; i <= 4000; ++i) {
        best = std::min(best, distance(p, quad_bezier(a, b, c, static_cast<float>(i) / 4000.0f)));
    }
    return best;
}

}  // namespace

TEST_CASE(linear_path_is_straight) {
    SliderPath path;
    path.build({{0.0f, 0.0f}, {100.0f, 0.0f}}, CurveType::Linear, 0.0f);
    CHECK_NEAR(path.total_length(), 100.0, 1e-4);
    CHECK_NEAR(path.position_at(0.5f).x, 50.0, 1e-4);
    CHECK_NEAR(path.position_at(0.5f).y, 0.0, 1e-4);
    CHECK_NEAR(path.position_at(1.0f).x, 100.0, 1e-4);
}

TEST_CASE(bezier_matches_reference_curve) {
    // Quadratic bezier (0,0) -> (0,100) -> (100,100); analytic length 162.323...
    SliderPath path;
    path.build({{0.0f, 0.0f}, {0.0f, 100.0f}, {100.0f, 100.0f}}, CurveType::Bezier, 0.0f);

    CHECK_NEAR(path.total_length(), 162.323, 1.5);

    // Every sampled point of the flattened polyline must lie on the real curve.
    float worst = 0.0f;
    for (const Vec2& p : path.points()) {
        worst = std::max(worst, distance_to_quad_bezier(p, {0.0f, 0.0f}, {0.0f, 100.0f}, {100.0f, 100.0f}));
    }
    CHECK_MSG(worst < 0.3f, "max deviation " + test::num(worst));

    CHECK_NEAR(path.position_at(0.0f).x, 0.0, 1e-4);
    CHECK_NEAR(path.position_at(1.0f).x, 100.0, 1e-3);
    CHECK_NEAR(path.position_at(1.0f).y, 100.0, 1e-3);
}

TEST_CASE(bezier_splits_on_repeated_anchor) {
    // Two straight beziers joined at (100,0), written as "B|100:0|100:0|100:100".
    SliderPath path;
    path.build({{0.0f, 0.0f}, {100.0f, 0.0f}, {100.0f, 0.0f}, {100.0f, 100.0f}},
               CurveType::Bezier, 0.0f);
    CHECK_NEAR(path.total_length(), 200.0, 0.5);
    CHECK_NEAR(path.position_at(0.5f).x, 100.0, 0.5);
    CHECK_NEAR(path.position_at(0.5f).y, 0.0, 0.5);
    CHECK_NEAR(path.position_at(1.0f).y, 100.0, 0.5);
}

TEST_CASE(perfect_circle_arc_length) {
    // Quarter circle of radius 100 centred on the origin.
    const float r = 70.710678f;
    SliderPath path;
    path.build({{100.0f, 0.0f}, {r, r}, {0.0f, 100.0f}}, CurveType::PerfectCircle, 0.0f);

    CHECK_NEAR(path.total_length(), 157.0796, 1.0);
    const Vec2 mid = path.position_at(0.5f);
    CHECK_NEAR(mid.x, r, 1.0);
    CHECK_NEAR(mid.y, r, 1.0);
}

TEST_CASE(perfect_circle_with_collinear_points_falls_back_to_line) {
    SliderPath path;
    path.build({{0.0f, 0.0f}, {50.0f, 0.0f}, {100.0f, 0.0f}}, CurveType::PerfectCircle, 0.0f);
    CHECK_NEAR(path.total_length(), 100.0, 0.5);
}

TEST_CASE(declared_length_extends_path) {
    SliderPath path;
    path.build({{0.0f, 0.0f}, {100.0f, 0.0f}}, CurveType::Linear, 150.0f);
    CHECK_NEAR(path.total_length(), 150.0, 1e-3);
    CHECK_NEAR(path.position_at(1.0f).x, 150.0, 1e-3);
}

TEST_CASE(declared_length_truncates_path) {
    SliderPath path;
    path.build({{0.0f, 0.0f}, {100.0f, 0.0f}}, CurveType::Linear, 50.0f);
    CHECK_NEAR(path.total_length(), 50.0, 1e-3);
    CHECK_NEAR(path.position_at(1.0f).x, 50.0, 1e-3);
}

TEST_CASE(catmull_rom_passes_through_anchors) {
    const std::vector<Vec2> control = {{0.0f, 0.0f}, {100.0f, 0.0f}, {100.0f, 100.0f}, {0.0f, 100.0f}};
    SliderPath path;
    path.build(control, CurveType::Catmull, 0.0f);
    CHECK(path.total_length() > 250.0);
    const std::vector<Vec2>& pts = path.points();
    // 50 samples per segment: the end of each segment is an anchor.
    CHECK(pts.size() >= 151);
    if (pts.size() >= 151) {
        CHECK_NEAR(distance(pts[50], control[1]), 0.0, 0.05);
        CHECK_NEAR(distance(pts[100], control[2]), 0.0, 0.05);
        CHECK_NEAR(distance(pts[150], control[3]), 0.0, 0.05);
    }
}

TEST_CASE(degenerate_paths_are_safe) {
    SliderPath single;
    single.build({{10.0f, 20.0f}}, CurveType::Linear, 0.0f);
    CHECK_NEAR(single.total_length(), 0.0, 1e-6);
    CHECK_NEAR(single.position_at(0.5f).x, 10.0, 1e-6);

    SliderPath none;
    none.build({}, CurveType::Linear, 100.0f);
    CHECK(none.empty());

    SliderPath zero_len;
    zero_len.build({{0.0f, 0.0f}, {0.0f, 0.0f}}, CurveType::Linear, 80.0f);
    // Cannot extrapolate a direction out of a zero-length path.
    CHECK_NEAR(zero_len.total_length(), 0.0, 1e-6);
}

TEST_CASE(progress_is_clamped) {
    SliderPath path;
    path.build({{0.0f, 0.0f}, {100.0f, 0.0f}}, CurveType::Linear, 0.0f);
    CHECK_NEAR(path.position_at(-1.0f).x, 0.0, 1e-6);
    CHECK_NEAR(path.position_at(2.0f).x, 100.0, 1e-6);
    CHECK_NEAR(path.position_at_distance(1e6f).x, 100.0, 1e-6);
}
