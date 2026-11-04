#include "slider_path.h"

#include <algorithm>
#include <cmath>

namespace osu {

namespace {

// Flattening tolerance in osu! pixels. 0.2 px is invisible on a 480x272 screen
// once the playfield is scaled down, and keeps the polyline small (~1 point per
// 8-10 px of curve), which matters for RAM on a 32 MiB PSP.
constexpr float kFlatness = 0.2f;
constexpr int kMaxBezierDepth = 20;
constexpr int kCatmullSamplesPerSegment = 50;
// Differences below this are treated as rounding noise from the editor.
constexpr float kLengthTolerance = 0.5f;

float point_to_segment_distance(Vec2 p, Vec2 a, Vec2 b) {
    const Vec2 ab = b - a;
    const float len2 = dot(ab, ab);
    if (len2 < 1e-8f) return distance(p, a);
    const float t = std::clamp(dot(p - a, ab) / len2, 0.0f, 1.0f);
    return distance(p, a + ab * t);
}

// de Casteljau split at t; `left` and `right` share the split point.
void split_bezier(const std::vector<Vec2>& pts, float t,
                  std::vector<Vec2>& left, std::vector<Vec2>& right) {
    const size_t n = pts.size() - 1;
    std::vector<Vec2> row = pts;
    left.assign(1, row.front());
    right.assign(1, row.back());
    for (size_t level = 0; level < n; ++level) {
        for (size_t i = 0; i + 1 < row.size(); ++i) row[i] = lerp(row[i], row[i + 1], t);
        row.pop_back();
        left.push_back(row.front());
        right.push_back(row.back());
    }
    std::reverse(right.begin(), right.end());
}

void append_bezier(const std::vector<Vec2>& control, std::vector<Vec2>& out, int depth) {
    if (control.size() < 2) return;
    if (control.size() == 2) {
        out.push_back(control[1]);
        return;
    }

    float flatness = 0.0f;
    for (size_t i = 1; i + 1 < control.size(); ++i) {
        flatness = std::max(flatness,
                            point_to_segment_distance(control[i], control.front(), control.back()));
    }
    if (flatness <= kFlatness || depth >= kMaxBezierDepth) {
        out.push_back(control.back());
        return;
    }

    std::vector<Vec2> left, right;
    split_bezier(control, 0.5f, left, right);
    append_bezier(left, out, depth + 1);
    append_bezier(right, out, depth + 1);
}

Vec2 catmull_point(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float t) {
    const float t2 = t * t;
    const float t3 = t2 * t;
    return {
        0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t +
                (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),
        0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t +
                (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3),
    };
}

}  // namespace

void SliderPath::clear() {
    points_.clear();
    cumulative_.clear();
    distance_ = 0.0f;
}

void SliderPath::build(const std::vector<Vec2>& control, CurveType curve, float expected_length) {
    clear();
    if (control.empty()) return;

    switch (curve) {
        case CurveType::Linear: build_linear(control); break;
        case CurveType::Bezier: build_bezier(control); break;
        case CurveType::Catmull: build_catmull(control); break;
        case CurveType::PerfectCircle: build_circle(control); break;
    }

    dedupe();
    rebuild_cumulative();
    if (expected_length > 0.0f) adjust_to(expected_length);
}

void SliderPath::build_linear(const std::vector<Vec2>& control) {
    points_.reserve(control.size());
    for (const Vec2& p : control) points_.push_back(p);
}

void SliderPath::build_bezier(const std::vector<Vec2>& control) {
    points_.push_back(control.front());

    // Consecutive duplicate points act as anchors that split the curve into
    // independent bezier pieces (a red anchor node in the editor).
    std::vector<Vec2> segment;
    segment.reserve(control.size());
    segment.push_back(control.front());

    for (size_t i = 1; i < control.size(); ++i) {
        const Vec2& p = control[i];
        if (segment.size() >= 2 && distance(segment.back(), p) < 0.01f) {
            append_bezier(segment, points_, 0);
            segment.clear();
            segment.push_back(p);
        } else {
            segment.push_back(p);
        }
    }
    append_bezier(segment, points_, 0);
}

void SliderPath::build_catmull(const std::vector<Vec2>& control) {
    points_.push_back(control.front());
    if (control.size() == 2) {
        points_.push_back(control[1]);
        return;
    }

    for (size_t i = 0; i + 1 < control.size(); ++i) {
        const Vec2 p1 = control[i];
        const Vec2 p2 = control[i + 1];
        const Vec2 p0 = (i == 0) ? p1 + (p1 - p2) : control[i - 1];
        const Vec2 p3 = (i + 2 < control.size()) ? control[i + 2] : p2 + (p2 - p1);
        for (int s = 1; s <= kCatmullSamplesPerSegment; ++s) {
            const float t = static_cast<float>(s) / kCatmullSamplesPerSegment;
            points_.push_back(catmull_point(p0, p1, p2, p3, t));
        }
    }
}

void SliderPath::build_circle(const std::vector<Vec2>& control) {
    points_.push_back(control.front());
    if (control.size() < 3) {
        for (size_t i = 1; i < control.size(); ++i) points_.push_back(control[i]);
        return;
    }

    const Vec2 p0 = control[0], p1 = control[1], p2 = control[2];
    const double ax = p0.x, ay = p0.y, bx = p1.x, by = p1.y, cx = p2.x, cy = p2.y;
    const double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));

    if (std::fabs(d) > 1e-6) {
        const double a2 = ax * ax + ay * ay;
        const double b2 = bx * bx + by * by;
        const double c2 = cx * cx + cy * cy;
        const double ux = (a2 * (by - cy) + b2 * (cy - ay) + c2 * (ay - by)) / d;
        const double uy = (a2 * (cx - bx) + b2 * (ax - cx) + c2 * (bx - ax)) / d;
        const double radius = std::hypot(ax - ux, ay - uy);

        if (radius > 0.5) {
            const double a0 = std::atan2(ay - uy, ax - ux);
            const double a1 = std::atan2(by - uy, bx - ux);
            const double a2a = std::atan2(cy - uy, cx - ux);
            const double two_pi = 2.0 * static_cast<double>(kPi);

            auto delta = [two_pi](double from, double to) {
                double v = to - from;
                while (v < 0.0) v += two_pi;
                while (v >= two_pi) v -= two_pi;
                return v;
            };

            const double to_mid = delta(a0, a1);
            const double to_end = delta(a0, a2a);
            const bool ccw = to_mid <= to_end;
            const double sweep = ccw ? to_end : two_pi - to_end;

            if (sweep > 1e-4) {
                const int steps = std::max(8, static_cast<int>(std::ceil(sweep / two_pi * 180.0)));
                points_.reserve(points_.size() + static_cast<size_t>(steps));
                for (int i = 1; i <= steps; ++i) {
                    const double angle = a0 + (ccw ? 1.0 : -1.0) * sweep * i / steps;
                    points_.push_back({static_cast<float>(ux + radius * std::cos(angle)),
                                       static_cast<float>(uy + radius * std::sin(angle))});
                }
                return;
            }
        }
    }

    // Collinear or degenerate: fall back to the control polygon.
    points_.push_back(p1);
    points_.push_back(p2);
}

void SliderPath::dedupe() {
    if (points_.size() < 2) return;
    size_t write = 1;
    for (size_t i = 1; i < points_.size(); ++i) {
        if (distance(points_[write - 1], points_[i]) > 1e-4f) {
            points_[write++] = points_[i];
        }
    }
    points_.resize(write);
}

void SliderPath::rebuild_cumulative() {
    cumulative_.assign(points_.size(), 0.0f);
    float acc = 0.0f;
    for (size_t i = 1; i < points_.size(); ++i) {
        acc += distance(points_[i - 1], points_[i]);
        cumulative_[i] = acc;
    }
    distance_ = acc;
}

void SliderPath::adjust_to(float expected_length) {
    if (std::fabs(expected_length - distance_) <= kLengthTolerance) {
        // Keep the authored geometry; only the nominal length differs by rounding.
        return;
    }

    if (expected_length > distance_) {
        if (points_.size() < 2 || distance_ < 1e-4f) return;  // cannot extrapolate a point
        const Vec2 dir = normalize(points_.back() - points_[points_.size() - 2]);
        if (length(dir) < 0.5f) return;
        points_.push_back(points_.back() + dir * (expected_length - distance_));
        rebuild_cumulative();
        return;
    }

    // Truncate: walk the polyline until the requested length is reached.
    float acc = 0.0f;
    for (size_t i = 1; i < points_.size(); ++i) {
        const float seg = distance(points_[i - 1], points_[i]);
        if (acc + seg >= expected_length && seg > 1e-6f) {
            const float t = (expected_length - acc) / seg;
            points_[i] = lerp(points_[i - 1], points_[i], t);
            points_.resize(i + 1);
            rebuild_cumulative();
            return;
        }
        acc += seg;
    }
    rebuild_cumulative();
}

Vec2 SliderPath::position_at_distance(float d) const {
    if (points_.empty()) return {};
    if (points_.size() == 1 || distance_ <= 0.0f) return points_.front();
    d = std::clamp(d, 0.0f, distance_);

    // First point whose cumulative length exceeds d.
    const auto it = std::lower_bound(cumulative_.begin(), cumulative_.end(), d);
    if (it == cumulative_.begin()) return points_.front();
    if (it == cumulative_.end()) return points_.back();

    const size_t i = static_cast<size_t>(it - cumulative_.begin());
    const float seg_start = cumulative_[i - 1];
    const float seg_len = cumulative_[i] - seg_start;
    if (seg_len <= 1e-6f) return points_[i];
    return lerp(points_[i - 1], points_[i], (d - seg_start) / seg_len);
}

Vec2 SliderPath::position_at(float progress) const {
    return position_at_distance(std::clamp(progress, 0.0f, 1.0f) * distance_);
}

}  // namespace osu
