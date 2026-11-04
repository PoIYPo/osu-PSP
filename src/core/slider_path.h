// Turns a slider's control points into a sampled polyline that the renderer and
// the gameplay code can walk by distance.
//
// The maths mirrors osu!lazer's SliderPath (MIT licensed) so that the geometry
// matches what the editor authored: bezier curves are flattened adaptively,
// perfect-circle arcs are exact circles through three points, catmull curves use
// the legacy 50-samples-per-segment approximation, and the declared length from
// the file extends or truncates the result.
#pragma once

#include <vector>

#include "beatmap.h"
#include "types.h"

namespace osu {

class SliderPath {
public:
    // `control_points` must contain the head first; `expected_length` is the
    // pixel length of one span as stored in the .osu file (<= 0 means "unknown").
    void build(const std::vector<Vec2>& control_points, CurveType curve, float expected_length);

    void clear();

    bool empty() const { return points_.empty(); }
    size_t point_count() const { return points_.size(); }
    const std::vector<Vec2>& points() const { return points_; }

    // Total polyline length in osu! pixels. Named total_length() rather than
    // distance() so that the free distance(a, b) helper stays visible in here.
    float total_length() const { return distance_; }

    // `progress` is clamped to [0,1] and scaled by distance().
    Vec2 position_at(float progress) const;
    Vec2 position_at_distance(float d) const;

private:
    void build_linear(const std::vector<Vec2>& control);
    void build_bezier(const std::vector<Vec2>& control);
    void build_catmull(const std::vector<Vec2>& control);
    void build_circle(const std::vector<Vec2>& control);

    void dedupe();
    void rebuild_cumulative();
    void adjust_to(float expected_length);

    std::vector<Vec2> points_;
    std::vector<float> cumulative_;  // cumulative_[i] = length from start to points_[i]
    float distance_ = 0.0f;
};

}  // namespace osu
