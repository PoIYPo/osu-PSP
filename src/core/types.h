// Basic portable geometry/math types shared by every subsystem.
// Deliberately dependency-free: the same headers compile for Windows/x64 (tests)
// and for the PSP's Allegrex (MIPS32r2, no FPU -- float only, avoid double in hot paths).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace osu {

constexpr float kPi = 3.14159265358979323846f;

// The osu! playfield is a fixed 512x384 coordinate space; every .osu file and
// every difficulty setting (circle size, etc.) is expressed in those units.
constexpr float kPlayfieldWidth = 512.0f;
constexpr float kPlayfieldHeight = 384.0f;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

    constexpr Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vec2 operator/(float s) const { return {x / s, y / s}; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
};

inline constexpr float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline constexpr float cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
inline float length(Vec2 v) { return std::sqrt(dot(v, v)); }
inline float distance(Vec2 a, Vec2 b) { return length(b - a); }
inline Vec2 normalize(Vec2 v) {
    const float len = length(v);
    return len > 1e-6f ? v / len : Vec2{};
}
inline Vec2 lerp(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }

// Normalises an angle into [0, 2*pi).
inline float normalize_angle(float a) {
    const float two_pi = 2.0f * kPi;
    a = std::fmod(a, two_pi);
    if (a < 0.0f) a += two_pi;
    return a;
}

struct Rect {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;

    constexpr bool contains(Vec2 p) const {
        return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;
    }
};

}  // namespace osu
