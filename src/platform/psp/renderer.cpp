#include "platform/psp/renderer.h"

#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>

#include <cstdlib>
#include <cstring>

namespace osu {

namespace {

constexpr int kScreenWidth = 480;
constexpr int kScreenHeight = 272;
constexpr int kBufferWidth = 512;

// VRAM layout for a 2D game: two 8888 framebuffers, no depth buffer (depth
// testing is pointless here and 557 KB of VRAM is worth more as texture space).
constexpr unsigned int kDrawBuffer = 0x000000;
constexpr unsigned int kDispBuffer = 0x088000;

unsigned int __attribute__((aligned(16))) gu_list[16384];

struct Vertex {
    float u, v;
    unsigned int color;
    float x, y, z;
};

}  // namespace

void Renderer::init() {
    sceGuInit();
    sceGuStart(GU_DIRECT, gu_list);

    sceGuDrawBuffer(GU_PSM_8888, reinterpret_cast<void*>(kDrawBuffer), kBufferWidth);
    sceGuDispBuffer(kScreenWidth, kScreenHeight, reinterpret_cast<void*>(kDispBuffer), kBufferWidth);

    sceGuOffset(2048 - (kScreenWidth / 2), 2048 - (kScreenHeight / 2));
    sceGuViewport(2048, 2048, kScreenWidth, kScreenHeight);
    sceGuScissor(0, 0, kScreenWidth, kScreenHeight);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuFrontFace(GU_CW);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexEnvColor(0xFFFFFFFF);

    sceGuClearColor(0xFF000000);
    sceGuClear(GU_COLOR_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void Renderer::shutdown() {
    sceGuTerm();
}

GuTexture Renderer::upload(const Image& image) {
    GuTexture texture;
    if (image.empty()) return texture;

    const size_t bytes = static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * 4u;
    void* data = std::malloc(bytes + 64u);
    if (!data) return texture;
    const uintptr_t raw = reinterpret_cast<uintptr_t>(data);
    const uintptr_t aligned = (raw + 63u) & ~static_cast<uintptr_t>(63u);
    std::memcpy(reinterpret_cast<void*>(aligned), image.pixels.data(), bytes);

    texture.data = reinterpret_cast<void*>(aligned);
    texture.width = image.width;
    texture.height = image.height;
    return texture;
}

void Renderer::bind_texture(const GuTexture& texture, bool linear_filter) {
    if (!texture.valid()) return;
    if (current_.data == texture.data && current_.width == texture.width &&
        current_.height == texture.height) {
        return;
    }
    current_ = texture;

    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, texture.width, texture.height, texture.width, texture.data);
    if (linear_filter) {
        sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    } else {
        sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    }
}

void Renderer::begin_frame(unsigned int clear_abgr) {
    sceGuStart(GU_DIRECT, gu_list);
    sceGuClearColor(clear_abgr);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    current_ = GuTexture{};
    sceGuDisable(GU_TEXTURE_2D);
}

void Renderer::end_frame() {
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

void Renderer::draw_rect(float x, float y, float w, float h, unsigned int abgr_color) {
    sceGuDisable(GU_TEXTURE_2D);
    current_ = GuTexture{};
    Vertex* v = static_cast<Vertex*>(sceGuGetMemory(sizeof(Vertex) * 2));
    v[0] = {0.0f, 0.0f, abgr_color, x, y, 0.0f};
    v[1] = {0.0f, 0.0f, abgr_color, x + w, y + h, 0.0f};
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   2, nullptr, v);
}

void Renderer::draw_sprite(const GuTexture& texture, float x, float y, float w, float h,
                           unsigned int tint_abgr) {
    if (!texture.valid()) return;
    bind_texture(texture, true);

    Vertex* v = static_cast<Vertex*>(sceGuGetMemory(sizeof(Vertex) * 2));
    v[0] = {0.0f, 0.0f, tint_abgr, x, y, 0.0f};
    v[1] = {static_cast<float>(texture.width), static_cast<float>(texture.height), tint_abgr,
            x + w, y + h, 0.0f};
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   2, nullptr, v);
}

void Renderer::draw_text(const GuTexture& font, float x, float y, float scale, const char* text,
                         unsigned int tint_abgr) {
    if (!font.valid()) return;
    bind_texture(font, false);

    const float cell_w = 8.0f;
    const float cell_h = 12.0f;
    float cursor_x = x;

    for (const char* p = text; *p; ++p) {
        const int index = font_glyph_index(*p);
        if (index < 0) {
            cursor_x += cell_w * scale;
            continue;
        }
        const float u0 = static_cast<float>(index) * cell_w;
        const float u1 = u0 + cell_w;

        Vertex* v = static_cast<Vertex*>(sceGuGetMemory(sizeof(Vertex) * 2));
        v[0] = {u0, 0.0f, tint_abgr, cursor_x, y, 0.0f};
        v[1] = {u1, cell_h, tint_abgr, cursor_x + cell_w * scale, y + cell_h * scale, 0.0f};
        sceGuDrawArray(GU_SPRITES,
                       GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2,
                       nullptr, v);

        cursor_x += cell_w * scale;
    }
}

void Renderer::draw_text_right(const GuTexture& font, float right_x, float y, float scale,
                               const char* text, unsigned int tint_abgr) {
    const float width = static_cast<float>(std::strlen(text)) * 8.0f * scale;
    draw_text(font, right_x - width, y, scale, text, tint_abgr);
}

}  // namespace osu
