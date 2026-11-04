# CMake toolchain for the PSP (Allegrex MIPS32r2) using pspdev.
#
# Usage:
#   cmake -B build-psp -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-psp.cmake
#   cmake --build build-psp
#
# Expects pspdev to be installed (the official pspdev docker image sets $PSPDEV).
# psp-g++ in that image does not search the pspsdk include directory on its own,
# so the paths are injected here rather than repeated in every target.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR mips)

set(OSU_TARGET_PSP ON CACHE INTERNAL "Cross compiling for the PSP")

if(DEFINED ENV{PSPDEV})
    set(_psp_bin "$ENV{PSPDEV}/bin/psp-")
    set(_psp_root "$ENV{PSPDEV}/psp")
else()
    set(_psp_bin "psp-")
    set(_psp_root "/usr/local/pspdev/psp")
endif()

message(STATUS "pspdev root: ${_psp_root}")

set(CMAKE_C_COMPILER "${_psp_bin}gcc")
set(CMAKE_CXX_COMPILER "${_psp_bin}g++")
set(CMAKE_AR "${_psp_bin}ar")
set(CMAKE_RANLIB "${_psp_bin}ranlib")
set(CMAKE_OBJCOPY "${_psp_bin}objcopy")
set(CMAKE_SIZE "${_psp_bin}size")

set(CMAKE_FIND_ROOT_PATH "${_psp_root}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# pspsdk headers and libraries are not in the compiler's default search path.
set(CMAKE_C_FLAGS_INIT "-I${_psp_root}/include")
set(CMAKE_CXX_FLAGS_INIT "-I${_psp_root}/include")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-L${_psp_root}/lib")

# We only need to know that the compiler can emit code, not that it can link a
# host executable (there is no OS to link against).
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
