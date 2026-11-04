#include "game/gameplay.h"

#include <algorithm>
#include <cmath>

namespace osu {

namespace {

constexpr size_t kNoIndex = static_cast<size_t>(-1);

}  // namespace

Gameplay::Gameplay(const PlayableBeatmap& map, GameplayConfig config)
    : map_(&map), config_(config) {
    difficulty_ = map.difficulty;
    reset();
}

void Gameplay::reset() {
    const size_t n = map_->map.objects.size();
    states_.assign(n, ObjectState::Pending);
    sliders_.assign(n, SliderRuntime{});
    spinner_fill_.assign(n, 0.0f);

    stats_ = GameplayStats{};
    time_ = 0.0;
    last_health_time_ = map_->map.first_object_time();
    next_index_ = 0;
    active_spinner_ = kNoIndex;
    spinner_rotations_ = 0.0f;
    spinner_last_angle_ = 0.0f;
    spinner_has_angle_ = false;
    cursor_ = {kPlayfieldWidth * 0.5f, kPlayfieldHeight * 0.5f};
    prev_cursor_ = cursor_;
    prev_pressed_ = false;
    finished_ = false;
}

double Gameplay::accuracy() const {
    const int judged = stats_.count_300 + stats_.count_100 + stats_.count_50 + stats_.count_miss;
    if (judged == 0) return 1.0;
    const double earned = 300.0 * stats_.count_300 + 100.0 * stats_.count_100 + 50.0 * stats_.count_50;
    return earned / (300.0 * judged);
}

float Gameplay::spinner_progress(size_t index) const {
    return index < spinner_fill_.size() ? spinner_fill_[index] : 0.0f;
}

bool Gameplay::in_range_of(size_t index, Vec2 cursor) const {
    const HitObject& obj = map_->map.objects[index];
    const float radius = difficulty_.circle_radius() * 1.02f;
    return distance(cursor, obj.pos) <= radius;
}

void Gameplay::apply_magnet(const InputState& input) {
    cursor_ = input.cursor;
    if (config_.magnet_scale <= 0.0f) return;

    const float radius = difficulty_.circle_radius() * config_.magnet_scale;
    float best = radius;
    Vec2 snapped = cursor_;
    const auto& objects = map_->map.objects;
    for (size_t i = next_index_; i < objects.size(); ++i) {
        if (objects[i].time > time_ + difficulty_.hit_window_50() * config_.window_scale) break;
        if (states_[i] != ObjectState::Pending) continue;
        const float d = distance(cursor_, objects[i].pos);
        if (d < best) {
            best = d;
            snapped = objects[i].pos;
        }
    }
    cursor_ = snapped;
}

void Gameplay::update(double song_time_ms, const InputState& input) {
    if (finished_) return;
    if (song_time_ms < time_) return;
    time_ = song_time_ms;

    apply_magnet(input);

    const bool pressed_edge = input.pressed && !prev_pressed_;
    if (pressed_edge) try_hit(song_time_ms);

    update_sliders(song_time_ms, input);
    update_spinner(song_time_ms, input);
    update_misses(song_time_ms);
    update_health(song_time_ms);

    prev_cursor_ = input.cursor;
    prev_pressed_ = input.pressed;

    while (next_index_ < states_.size() && states_[next_index_] != ObjectState::Pending) {
        ++next_index_;
    }

    if (!finished_ && song_time_ms > map_->map.last_object_time() + 1500.0) {
        finished_ = true;
        stats_.completed = !stats_.failed;
    }
}

void Gameplay::try_hit(double song_time) {
    const double w300 = difficulty_.hit_window_300() * config_.window_scale;
    const double w100 = difficulty_.hit_window_100() * config_.window_scale;
    const double w50 = difficulty_.hit_window_50() * config_.window_scale;

    size_t best = kNoIndex;
    double best_delta = 1e9;

    const auto& objects = map_->map.objects;
    for (size_t i = next_index_; i < objects.size(); ++i) {
        const HitObject& obj = objects[i];
        if (obj.time > song_time + w50) break;
        if (states_[i] != ObjectState::Pending) continue;
        if (obj.kind == ObjectKind::Spinner) continue;

        const double delta = song_time - obj.time;
        if (std::fabs(delta) > w50) continue;
        if (!in_range_of(i, cursor_)) continue;
        if (std::fabs(delta) < best_delta) {
            best_delta = std::fabs(delta);
            best = i;
        }
    }
    if (best == kNoIndex) return;

    Judgement judgement = Judgement::Hit50;
    if (best_delta <= w300) {
        judgement = Judgement::Hit300;
    } else if (best_delta <= w100) {
        judgement = Judgement::Hit100;
    }

    judge_object(best, judgement);

    if (objects[best].kind == ObjectKind::Slider && judgement != Judgement::Miss) {
        sliders_[best].following = true;
        sliders_[best].next_event = 0;
    }
}

void Gameplay::judge_object(size_t index, Judgement judgement) {
    if (index >= states_.size() || states_[index] != ObjectState::Pending) return;

    switch (judgement) {
        case Judgement::Hit300:
            states_[index] = ObjectState::Hit;
            ++stats_.count_300;
            add_combo_hit(300);
            stats_.hp = std::min(100.0, stats_.hp + 1.0);
            break;
        case Judgement::Hit100:
            states_[index] = ObjectState::Hit;
            ++stats_.count_100;
            add_combo_hit(100);
            stats_.hp = std::min(100.0, stats_.hp + 0.6);
            break;
        case Judgement::Hit50:
            states_[index] = ObjectState::Hit;
            ++stats_.count_50;
            add_combo_hit(50);
            stats_.hp = std::min(100.0, stats_.hp + 0.3);
            break;
        case Judgement::Miss:
        default:
            states_[index] = ObjectState::Missed;
            ++stats_.count_miss;
            break_combo();
            stats_.hp = std::max(0.0, stats_.hp - 4.0);
            break;
    }
}

void Gameplay::add_combo_hit(int base_value) {
    ++stats_.combo;
    stats_.max_combo = std::max(stats_.max_combo, stats_.combo);
    // Provisional ScoreV1-ish multiplier; stable's exact formula is still TODO.
    const double multiplier = 1.0 + std::min(stats_.combo, 100) * 0.01;
    stats_.score += static_cast<int>(std::lround(base_value * multiplier));
}

void Gameplay::break_combo() {
    stats_.combo = 0;
}

void Gameplay::update_sliders(double song_time, const InputState& input) {
    const auto& objects = map_->map.objects;
    for (size_t i = 0; i < objects.size(); ++i) {
        const HitObject& obj = objects[i];
        if (obj.kind != ObjectKind::Slider) continue;
        if (states_[i] != ObjectState::Hit) continue;

        SliderRuntime& rt = sliders_[i];
        if (!rt.following) continue;

        const SliderGeometry& geo = map_->sliders[i];

        if (!input.pressed) {
            rt.following = false;
            rt.broken = true;
            rt.next_event = static_cast<int>(geo.events.size());
            break_combo();
            continue;
        }

        const double span = geo.span_duration;
        const int repeats = std::max(1, obj.slider.repeats);
        const double elapsed = song_time - obj.time;

        int span_index = 0;
        double in_span = 0.0;
        float frac = 0.0f;
        if (span > 0.0) {
            span_index = std::clamp(static_cast<int>(elapsed / span), 0, repeats - 1);
            in_span = elapsed - span_index * span;
            frac = static_cast<float>(std::clamp(in_span / span, 0.0, 1.0));
        }
        // Distance along the path: even spans run forward, odd spans run back.
        const float along = (span_index % 2 == 0) ? frac : 1.0f - frac;
        rt.progress = along;

        if (config_.auto_follow_sliders) {
            cursor_ = geo.path.position_at(along);
        }

        while (rt.next_event < static_cast<int>(geo.events.size()) &&
               geo.events[rt.next_event].time <= song_time) {
            ++stats_.ticks;
            add_combo_hit(10);
            stats_.hp = std::min(100.0, stats_.hp + 0.05);
            ++rt.next_event;
        }

        if (song_time >= obj.end_time) {
            rt.following = false;
        }
    }
}

void Gameplay::update_spinner(double song_time, const InputState& input) {
    (void)input;
    const auto& objects = map_->map.objects;

    for (size_t i = next_index_; i < objects.size(); ++i) {
        const HitObject& obj = objects[i];
        if (obj.kind != ObjectKind::Spinner) continue;
        if (states_[i] != ObjectState::Pending) continue;
        if (song_time < obj.time) break;

        if (active_spinner_ != i) {
            active_spinner_ = i;
            spinner_rotations_ = 0.0f;
            spinner_has_angle_ = false;
        }

        const Vec2 delta = cursor_ - obj.pos;
        const float angle = std::atan2(delta.y, delta.x);
        if (spinner_has_angle_ && length(delta) > 8.0f) {
            float step = angle - spinner_last_angle_;
            while (step > kPi) step -= 2.0f * kPi;
            while (step < -kPi) step += 2.0f * kPi;
            spinner_rotations_ += std::fabs(step) / (2.0f * kPi);
        }
        spinner_last_angle_ = angle;
        spinner_has_angle_ = length(delta) > 8.0f;

        const double duration = std::max(1.0, obj.end_time - obj.time);
        const float required =
            std::max(0.25f, static_cast<float>(duration / 1000.0 * config_.spinner_rotations_per_second));
        const float ratio = spinner_rotations_ / required;
        spinner_fill_[i] = std::min(1.0f, ratio);

        if (song_time >= obj.end_time) {
            Judgement judgement = Judgement::Miss;
            if (ratio >= 1.0f) {
                judgement = Judgement::Hit300;
            } else if (ratio >= 0.75f) {
                judgement = Judgement::Hit100;
            } else if (ratio >= 0.5f) {
                judgement = Judgement::Hit50;
            }
            judge_object(i, judgement);
            spinner_fill_[i] = std::min(1.0f, ratio);
            active_spinner_ = kNoIndex;
            spinner_has_angle_ = false;
        }
        break;
    }
}

void Gameplay::update_misses(double song_time) {
    const auto& objects = map_->map.objects;
    const double w50 = difficulty_.hit_window_50() * config_.window_scale;

    for (size_t i = next_index_; i < objects.size(); ++i) {
        if (states_[i] != ObjectState::Pending) continue;
        const HitObject& obj = objects[i];
        if (obj.kind == ObjectKind::Spinner) continue;  // judged at its end
        if (song_time > obj.time + w50) judge_object(i, Judgement::Miss);
    }
}

void Gameplay::update_health(double song_time) {
    const double dt = song_time - last_health_time_;
    if (dt > 0.0) {
        const double drain_per_second = 0.2 + difficulty_.hp_drain * 0.04;
        stats_.hp -= dt / 1000.0 * drain_per_second;
    }
    last_health_time_ = song_time;

    stats_.hp = std::clamp(stats_.hp, 0.0, 100.0);
    if (stats_.hp <= 0.0 && !stats_.failed) {
        stats_.failed = true;
        finished_ = true;
    }
}

}  // namespace osu
