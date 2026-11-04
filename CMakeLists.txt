cmake_minimum_required(VERSION 3.20)
project(osu_psp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Portable core: beatmap parsing, timing, slider geometry, standard rules.
# No OS APIs, only std containers, safe to compile for MIPS.
# ---------------------------------------------------------------------------
add_library(osu_core STATIC
    src/core/beatmap.cpp
    src/core/timing.cpp
    src/core/slider_path.cpp
    src/core/standard.cpp
    src/game/gameplay.cpp
    src/render/framebuffer.cpp
    src/render/skin.cpp
    src/render/png.cpp
    src/render/scene.cpp
)
target_include_directories(osu_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
    target_compile_options(osu_core PRIVATE /W4)
else()
    target_compile_options(osu_core PRIVATE -Wall -Wextra -Wshadow)
endif()

if(OSU_TARGET_PSP)
    # -----------------------------------------------------------------------
    # PSP build (cross compiled with pspdev, see cmake/toolchain-psp.cmake)
    # -----------------------------------------------------------------------
    message(STATUS "Building for PSP with ${CMAKE_CXX_COMPILER}")

    add_executable(osu_psp src/platform/psp/main.cpp src/platform/psp/renderer.cpp)
    target_link_libraries(osu_psp PRIVATE osu_core)
    target_compile_options(osu_psp PRIVATE -G0 -O2 -Wall -Wextra -D_PSP_FW_VERSION=661)
    # Order matters: pspsdk libraries are static archives.
    target_link_libraries(osu_psp PRIVATE
        pspgu pspgum pspctrl pspdisplay pspge pspaudio pspmp3 psprtc pspuser pspkernel pspdebug
    )
    # libpspkernel's SysclibForKernel stub also defines strtol/strtod, which
    # collides with newlib's libc. Every pspsdk project hits this.
    target_link_options(osu_psp PRIVATE -Wl,--allow-multiple-definition)
    set_target_properties(osu_psp PROPERTIES SUFFIX ".elf")

    # Mandatory for every pspsdk ELF: rewrites the import table so the loader
    # resolves sce* calls. Without it the app jumps to a garbage address right
    # after boot ("Bad Execution Address" in PPSSPP).
    add_custom_command(TARGET osu_psp POST_BUILD
        COMMAND "${PSP_BIN_DIR}/psp-fixup-imports" $<TARGET_FILE:osu_psp>
        COMMENT "psp-fixup-imports"
    )
else()
    # -----------------------------------------------------------------------
    # Host build: unit tests + a small CLI that dumps parsed beatmaps.
    # -----------------------------------------------------------------------
    enable_testing()

    add_executable(osu_tests
        tests/test_main.cpp
        tests/test_parser.cpp
        tests/test_timing.cpp
        tests/test_slider.cpp
        tests/test_standard.cpp
        tests/test_gameplay.cpp
        tests/test_skin.cpp
    )
    target_link_libraries(osu_tests PRIVATE osu_core)
    target_include_directories(osu_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_compile_options(osu_tests PRIVATE -Wall -Wextra)

    add_test(NAME core COMMAND osu_tests)
    set_tests_properties(core PROPERTIES LABELS "unit")

    add_executable(osu_dump tools/osu_dump.cpp)
    target_link_libraries(osu_dump PRIVATE osu_core)

    add_executable(osu_play tools/osu_play.cpp)
    target_link_libraries(osu_play PRIVATE osu_core)
endif()
