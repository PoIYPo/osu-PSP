#include "timing.h"

#include <algorithm>

namespace osu {

namespace {
constexpr double kDefaultBeatLength = 500.0;  // 120 BPM, osu!'s implicit default
}

void TimingInfo::build(std::vector<TimingPoint> points) {
    points_ = std::move(points);
    std::stable_sort(points_.begin(), points_.end(),
                     [](const TimingPoint& a, const TimingPoint& b) {
                         if (a.time != b.time) return a.time < b.time;
                         return a.uninherited && !b.uninherited;
                     });

    uninherited_.clear();
    inherited_.clear();
    for (size_t i = 0; i < points_.size(); ++i) {
        (points_[i].uninherited ? uninherited_ : inherited_).push_back(i);
    }
}

size_t TimingInfo::last_index_at(const std::vector<size_t>& indices, double time_ms) const {
    // First element whose time is strictly greater than `time_ms`, minus one.
    size_t lo = 0, hi = indices.size();
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (points_[indices[mid]].time <= time_ms) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;  // number of points at or before `time_ms`
}

double TimingInfo::uninherited_beat_length(double time_ms) const {
    if (uninherited_.empty()) return kDefaultBeatLength;
    const size_t count = last_index_at(uninherited_, time_ms);
    // Before the first uninherited point osu! uses the first one, not the default.
    const size_t idx = count == 0 ? uninherited_.front() : uninherited_[count - 1];
    const double beat = points_[idx].beat_length;
    return beat > 0.0 ? beat : kDefaultBeatLength;
}

double TimingInfo::slider_velocity(double time_ms) const {
    if (inherited_.empty()) return 1.0;
    const size_t count = last_index_at(inherited_, time_ms);
    // An inherited point that precedes every uninherited one has no effect yet,
    // which matches how osu! applies velocity from the active timing section.
    if (count == 0) return 1.0;
    const TimingPoint& tp = points_[inherited_[count - 1]];
    const double sv = tp.slider_velocity_multiplier();
    return sv > 0.0 ? sv : 1.0;
}

double TimingInfo::bpm(double time_ms) const {
    const double beat = uninherited_beat_length(time_ms);
    return beat > 0.0 ? 60000.0 / beat : 0.0;
}

int TimingInfo::meter(double time_ms) const {
    if (uninherited_.empty()) return 4;
    const size_t count = last_index_at(uninherited_, time_ms);
    const size_t idx = count == 0 ? uninherited_.front() : uninherited_[count - 1];
    return points_[idx].meter > 0 ? points_[idx].meter : 4;
}

}  // namespace osu
