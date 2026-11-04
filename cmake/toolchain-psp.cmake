# CMake toolchain for the PSP (Allegrex MIPS32r2) using pspdev.
#
# Usage:
#   cmake -B build-psp -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-psp.cmake
#   cmake --build build-psp
#
# Expects psp-g++ / psp-ar / psp-ranlib on PATH (the official pspdev docker image
# exports them), or $PSPDEV/bin in PATH.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR mips)

set(OSU_TARGET_PSP ON CACHE INTERNAL "Cross compiling for the PSP")

# pspdev installs prefixed wrappers; honour $PSPDEV when it is set.
if(DEFINED ENV{PSPDEV})
    set(_psp_prefix "$ENV{PSPDEV}/bin/psp-")
else()
    set(_psp_prefix "psp-")
endif()

set(CMAKE_C_COMPILER "${_psp_prefix}gcc")
set(CMAKE_CXX_COMPILER "${_psp_prefix}g++")
set(CMAKE_AR "${_psp_prefix}ar")
set(CMAKE_RANLIB "${_psp_prefix}ranlib")
set(CMAKE_OBJCOPY "${_psp_prefix}objcopy")
set(CMAKE_SIZE "${_psp_prefix}size")

set(CMAKE_FIND_ROOT_PATH "$ENV{PSPDEV}/psp")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# We only need to know that the compiler can emit code, not that it can link a
# host executable (there is no OS to link against).
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
