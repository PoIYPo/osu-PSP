#include "render/framebuffer.h"

#include <algorithm>
#include <cmath>

namespace osu {

Framebuffer::Framebuffer(int width, int height) : width_(width), height_(height) {
    data_.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u, 0);
}

void Framebuffer::clear(Color color) {
    for (size_t i = 0; i < data_.size(); i += 4) {
        data_[i + 0] = color.r;
        data_[i + 1] = color.g;
        data_[i + 2] = color.b;
        data_[i + 3] = color.a;
    }
}

void Framebuffer::blend(int x, int y, Color color) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
    uint8_t* p = &data_[(static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)) * 4u];
    if (color.a == 255) {
        p[0] = color.r;
        p[1] = color.g;
        p[2] = color.b;
        p[3] = 255;
        return;
    }
    if (color.a == 0) return;
    const int a = color.a;
    p[0] = static_cast<uint8_t>((color.r * a + p[0] * (255 - a)) / 255);
    p[1] = static_cast<uint8_t>((color.g * a + p[1] * (255 - a)) / 255);
    p[2] = static_cast<uint8_t>((color.b * a + p[2] * (255 - a)) / 255);
    p[3] = 255;
}

void Framebuffer::fill_rect(float x, float y, float w, float h, Color color) {
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = static_cast<int>(std::ceil(x + w));
    const int y1 = static_cast<int>(std::ceil(y + h));
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) blend(px, py, color);
    }
}

void Framebuffer::fill_disc(float cx, float cy, float radius, Color color) {
    if (radius <= 0.0f) return;
    const int y0 = static_cast<int>(std::floor(cy - radius));
    const int y1 = static_cast<int>(std::ceil(cy + radius));
    const float r2 = radius * radius;
    for (int py = y0; py <= y1; ++py) {
        const float dy = static_cast<float>(py) + 0.5f - cy;
        const float remain = r2 - dy * dy;
        if (remain < 0.0f) continue;
        const float dx = std::sqrt(remain);
        const int x_start = static_cast<int>(std::ceil(cx - dx));
        const int x_end = static_cast<int>(std::floor(cx + dx));
        for (int px = x_start; px <= x_end; ++px) blend(px, py, color);
    }
}

void Framebuffer::stroke_circle(float cx, float cy, float radius, float thickness, Color color) {
    if (radius <= 0.0f) return;
    const float half = thickness * 0.5f;
    const int y0 = static_cast<int>(std::floor(cy - radius - half));
    const int y1 = static_cast<int>(std::ceil(cy + radius + half));
    for (int py = y0; py <= y1; ++py) {
        const float dy = static_cast<float>(py) + 0.5f - cy;
        const float dy2 = dy * dy;
        if (dy2 > (radius + half) * (radius + half)) continue;
        const int x0 = static_cast<int>(std::floor(cx - radius - half));
        const int x1 = static_cast<int>(std::ceil(cx + radius + half));
        for (int px = x0; px <= x1; ++px) {
            const float dx = static_cast<float>(px) + 0.5f - cx;
            const float d = std::sqrt(dx * dx + dy2);
            if (d >= radius - half && d <= radius + half) blend(px, py, color);
        }
    }
}

void Framebuffer::thick_line(Vec2 a, Vec2 b, float thickness, Color color) {
    const float len = distance(a, b);
    if (len < 0.01f) {
        fill_disc(a.x, a.y, thickness * 0.5f, color);
        return;
    }
    const int steps = std::max(1, static_cast<int>(len / std::max(0.5f, thickness * 0.25f)));
    for (int i = 0; i <= steps; ++i) {
        const Vec2 p = lerp(a, b, static_cast<float>(i) / static_cast<float>(steps));
        fill_disc(p.x, p.y, thickness * 0.5f, color);
    }
}

}  // namespace osu
