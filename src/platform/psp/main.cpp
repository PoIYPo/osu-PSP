// PSP entry point. Draws the 4:3 playfield layout the game will use, plus a
// couple of primitives, to prove the GU pipeline, the pad and the frame loop on
// both hardware and PPSSPP.
//
// Colour note: GU colours (clear colour and GU_COLOR_8888 vertex colours) are
// ABGR, not RGBA -- 0xAABBGGRR.
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspkernel.h>

#include <cmath>

PSP_MODULE_INFO("osu_psp", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

namespace {

constexpr int kScreenWidth = 480;
constexpr int kScreenHeight = 272;
constexpr int kBufferWidth = 512;

// VRAM offsets, exactly like the pspsdk samples: one 8888 buffer is
// 512 * 272 * 4 = 0x88000 bytes, so draw at 0, display at 0x88000 and depth
// right after the second buffer at 0x110000. Overlapping these is what produced
// a garbled image in the first build.
constexpr unsigned int kDrawBuffer = 0x000000;
constexpr unsigned int kDispBuffer = 0x088000;
constexpr unsigned int kDepthBuffer = 0x110000;

unsigned int __attribute__((aligned(16))) gu_list[16384];

struct Vertex {
    float u, v;
    unsigned int color;
    float x, y, z;
};

constexpr unsigned int abgr(int r, int g, int b, int a = 0xFF) {
    return (static_cast<unsigned int>(a) << 24) | (static_cast<unsigned int>(b) << 16) |
           (static_cast<unsigned int>(g) << 8) | static_cast<unsigned int>(r);
}

void init_gu() {
    sceGuInit();
    sceGuStart(GU_DIRECT, gu_list);

    sceGuDrawBuffer(GU_PSM_8888, reinterpret_cast<void*>(kDrawBuffer), kBufferWidth);
    sceGuDispBuffer(kScreenWidth, kScreenHeight, reinterpret_cast<void*>(kDispBuffer), kBufferWidth);
    sceGuDepthBuffer(reinterpret_cast<void*>(kDepthBuffer), kBufferWidth);

    sceGuOffset(2048 - (kScreenWidth / 2), 2048 - (kScreenHeight / 2));
    sceGuViewport(2048, 2048, kScreenWidth, kScreenHeight);
    sceGuScissor(0, 0, kScreenWidth, kScreenHeight);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuFrontFace(GU_CW);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    sceGuClearDepth(0);
    sceGuClearColor(abgr(16, 16, 20));
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
}

void end_frame() {
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

// A circle as a triangle fan; a stand-in until the skin texture atlas exists.
void draw_circle(float cx, float cy, float radius, unsigned int color, int segments = 40) {
    Vertex* v = static_cast<Vertex*>(sceGuGetMemory(sizeof(Vertex) * (segments + 2)));
    v[0] = {0.0f, 0.0f, color, cx, cy, 0.0f};
    for (int i = 0; i <= segments; ++i) {
        const float a = static_cast<float>(i) / static_cast<float>(segments) * 6.2831853f;
        v[i + 1] = {0.0f, 0.0f, color, cx + std::cos(a) * radius, cy + std::sin(a) * radius, 0.0f};
    }
    sceGuDrawArray(GU_TRIANGLE_FAN,
                   GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
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
    init_gu();

    SceCtrlData pad{};
    float pulse = 0.0f;
    bool running = true;

    while (running) {
        sceCtrlReadBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_HOME) running = false;

        begin_frame(abgr(16, 16, 20));

        // The 4:3 osu! playfield centred on the 480x272 screen: exactly the layout
        // the game will use (HUD lives in the left/right panels).
        const float playfield_height = 240.0f;
        const float playfield_width = playfield_height * (512.0f / 384.0f);
        const float px = (kScreenWidth - playfield_width) * 0.5f;
        const float py = (kScreenHeight - playfield_height) * 0.5f;

        draw_rect(px, py, playfield_width, playfield_height, abgr(26, 26, 34));
        draw_rect(px, py, playfield_width, 1.0f, abgr(80, 80, 110));  // top border

        const float cx = px + playfield_width * 0.5f;
        const float cy = py + playfield_height * 0.5f;
        const float r = 40.0f + 6.0f * std::sin(pulse);
        draw_circle(cx, cy, r, abgr(102, 204, 255));                        // approach ring stand-in
        draw_circle(cx, cy, r * 0.35f, abgr(255, 255, 255, 200));           // hit circle stand-in
        draw_circle(px + playfield_width * 0.25f, py + playfield_height * 0.3f, 18.0f,
                    abgr(255, 102, 170));                                   // another note
        draw_circle(px + playfield_width * 0.72f, py + playfield_height * 0.68f, 18.0f,
                    abgr(255, 102, 170));

        // Side panels make it obvious the HUD area is reserved and empty.
        draw_rect(4, py, 68, playfield_height, abgr(20, 20, 26));
        draw_rect(kScreenWidth - 72, py, 68, playfield_height, abgr(20, 20, 26));

        end_frame();
        pulse += 0.06f;
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
