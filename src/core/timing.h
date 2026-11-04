// Resolves timing (BPM + slider velocity) at an arbitrary point in the song.
#pragma once

#include <cstddef>
#include <vector>

#include "beatmap.h"

namespace osu {

class TimingInfo {
public:
    // Takes ownership of a copy of the beatmap's timing points and precomputes the
    // lookup tables. Points do not have to be sorted.
    void build(std::vector<TimingPoint> points);

    bool empty() const { return points_.empty(); }
    size_t size() const { return points_.size(); }

    // Milliseconds per beat from the last uninherited point at or before `time_ms`.
    // Before the first point the first point's value is used, as in osu!.
    // Falls back to 500 ms (120 BPM) if the map has no uninherited point at all.
    double uninherited_beat_length(double time_ms) const;

    // Slider velocity multiplier from the last inherited point (1.0 if none).
    double slider_velocity(double time_ms) const;

    double bpm(double time_ms) const;
    int meter(double time_ms) const;

private:
    size_t last_index_at(const std::vector<size_t>& indices, double time_ms) const;

    std::vector<TimingPoint> points_;
    std::vector<size_t> uninherited_;
    std::vector<size_t> inherited_;
};

}  // namespace osu
