// PSP entry point. Right now this is a rendering smoke test: it proves that the
// GU pipeline, sprite-free geometry drawing and the pad all work on hardware /
// PPSSPP. Gameplay replaces the scene drawing once the renderer lands.
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspkernel.h>

#include <cmath>

#include "core/standard.h"
#include "core/types.h"

PSP_MODULE_INFO("osu_psp", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

namespace {

constexpr int kScreenWidth = 480;
constexpr int kScreenHeight = 272;
constexpr int kBufferWidth = 512;

unsigned int __attribute__((aligned(16))) gu_list[262144];

struct Vertex {
    float u, v;
    unsigned int color;
    float x, y, z;
};

int exit_callback(int, int, void*) {
    sceKernelExitGame();
    return 0;
}

int callback_thread(SceSize, void*) {
    const int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, nullptr);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int setup_callbacks() {
    const int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, nullptr);
    if (thid >= 0) sceKernelStartThread(thid, 0, nullptr);
    return thid;
}

void init_gu() {
    sceGuInit();
    sceGuStart(GU_DIRECT, gu_list);

    void* frame_buffer = reinterpret_cast<void*>(0x04000000);
    void* depth_buffer = reinterpret_cast<void*>(0x04000000 + kBufferWidth * kScreenHeight * 4);
    sceGuDrawBuffer(GU_PSM_8888, frame_buffer, kBufferWidth);
    sceGuDispBuffer(kScreenWidth, kScreenHeight, reinterpret_cast<void*>(0x04088000), kBufferWidth);
    sceGuDepthBuffer(depth_buffer, kBufferWidth);

    sceGuOffset(2048 - (kScreenWidth / 2), 2048 - (kScreenHeight / 2));
    sceGuViewport(2048, 2048, kScreenWidth, kScreenHeight);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, kScreenWidth, kScreenHeight);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuFrontFace(GU_CW);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuClearColor(0xFF101014);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void begin_frame(unsigned int color) {
    sceGuStart(GU_DIRECT, gu_list);
    sceGuClearColor(color);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuDisable(GU_TEXTURE_2D);
}

void end_frame() {
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

// A circle as a triangle fan. Used until the skin/texture pipeline exists.
void draw_circle(float cx, float cy, float radius, unsigned int color, int segments = 32) {
    Vertex* v = static_cast<Vertex*>(sceGuGetMemory(sizeof(Vertex) * (segments + 2)));
    v[0] = {0.0f, 0.0f, color, cx, cy, 0.0f};
    for (int i = 0; i <= segments; ++i) {
        const float a = static_cast<float>(i) / segments * 6.2831853f;
        v[i + 1] = {0.0f, 0.0f, color, cx + std::cos(a) * radius, cy + std::sin(a) * radius, 0.0f};
    }
    sceGuDrawArray(GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   segments + 2, nullptr, v);
}

void draw_rect(float x, float y, float w, float h, unsigned int color) {
    Vertex* v = static_cast<Vertex*>(sceGuGetMemory(sizeof(Vertex) * 2));
    v[0] = {0.0f, 0.0f, color, x, y, 0.0f};
    v[1] = {0.0f, 0.0f, color, x + w, y + h, 0.0f};
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   2, nullptr, v);
}

}  // namespace

int main() {
    setup_callbacks();
    init_gu();

    SceCtrlData pad{};
    float pulse = 0.0f;

    while (true) {
        sceCtrlReadBufferPositive(&pad, 1);

        begin_frame(0xFF101014);

        // The 4:3 osu! playfield centred on the 480x272 screen, exactly the
        // layout the real game will use (HUD goes into the side panels).
        const float playfield_height = 240.0f;
        const float playfield_width = playfield_height * (512.0f / 384.0f);
        const float px = (kScreenWidth - playfield_width) * 0.5f;
        const float py = (kScreenHeight - playfield_height) * 0.5f;

        draw_rect(px, py, playfield_width, playfield_height, 0xFF181820);
        draw_circle(px + playfield_width * 0.5f, py + playfield_height * 0.5f,
                    40.0f + 10.0f * std::sin(pulse), 0xFF66CCFF);
        draw_circle(px + playfield_width * 0.25f, py + playfield_height * 0.3f, 18.0f, 0xFFFF66AA);

        end_frame();

        pulse += 0.05f;
        if (pad.Buttons & PSP_CTRL_HOME) break;
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
