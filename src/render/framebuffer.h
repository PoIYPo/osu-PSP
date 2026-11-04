// Tiny RGBA software canvas. Used for headless frame dumps (so gameplay can be
// verified without a PSP) and as the reference for the GU renderer's geometry.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/types.h"

namespace osu {

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;

    static constexpr Color rgb(int r_, int g_, int b_) {
        return {static_cast<uint8_t>(r_), static_cast<uint8_t>(g_), static_cast<uint8_t>(b_), 255};
    }
    static constexpr Color rgba(int r_, int g_, int b_, int a_) {
        return {static_cast<uint8_t>(r_), static_cast<uint8_t>(g_), static_cast<uint8_t>(b_),
                static_cast<uint8_t>(a_)};
    }
};

class Framebuffer {
public:
    Framebuffer(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }
    const uint8_t* pixels() const { return data_.data(); }

    void clear(Color color);
    void blend(int x, int y, Color color);

    void fill_rect(float x, float y, float w, float h, Color color);
    void fill_disc(float cx, float cy, float radius, Color color);
    void stroke_circle(float cx, float cy, float radius, float thickness, Color color);
    void thick_line(Vec2 a, Vec2 b, float thickness, Color color);

private:
    int width_;
    int height_;
    std::vector<uint8_t> data_;
};

}  // namespace osu
