// TEMPORARY diagnostic build: minimal GU usage (clear + swap only).
// Used to bisect "the pipeline is broken" vs "my drawing code is broken".
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>

PSP_MODULE_INFO("osu_psp", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

namespace {
constexpr int kScreenWidth = 480;
constexpr int kScreenHeight = 272;
constexpr int kBufferWidth = 512;

unsigned int __attribute__((aligned(16))) gu_list[16384];

// VRAM offsets, exactly like the pspsdk samples: draw at 0, display right after
// one 8888 buffer (512*272*4 = 0x88000), depth after the second one.
constexpr unsigned int kDrawBuffer = 0x000000;
constexpr unsigned int kDispBuffer = 0x088000;
constexpr unsigned int kDepthBuffer = 0x110000;
}  // namespace

int main() {
    sceGuInit();
    sceGuStart(GU_DIRECT, gu_list);
    sceGuDrawBuffer(GU_PSM_8888, reinterpret_cast<void*>(kDrawBuffer), kBufferWidth);
    sceGuDispBuffer(kScreenWidth, kScreenHeight, reinterpret_cast<void*>(kDispBuffer), kBufferWidth);
    sceGuDepthBuffer(reinterpret_cast<void*>(kDepthBuffer), kBufferWidth);
    sceGuOffset(2048 - (kScreenWidth / 2), 2048 - (kScreenHeight / 2));
    sceGuViewport(2048, 2048, kScreenWidth, kScreenHeight);
    sceGuScissor(0, 0, kScreenWidth, kScreenHeight);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    for (int i = 0; i < 100000; ++i) {
        sceGuStart(GU_DIRECT, gu_list);
        sceGuClearColor(0xFF0000FF);  // ABGR: opaque red
        sceGuClear(GU_COLOR_BUFFER_BIT);
        sceGuFinish();
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
