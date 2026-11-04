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

# pspsdk headers live in <pspdev>/psp/sdk/include and libraries in
# <pspdev>/psp/sdk/lib; psp-g++ does not look there on its own. The plain
# psp/include and psp/lib paths are kept for toolchain layouts that use them.
set(_psp_include "${_psp_root}/sdk/include")
set(_psp_lib "${_psp_root}/sdk/lib")
foreach(dir "${_psp_root}/include" "${_psp_root}/lib")
    if(EXISTS "${dir}")
        list(APPEND _psp_include "${dir}")
        list(APPEND _psp_lib "${dir}")
    endif()
endforeach()

set(_psp_include_flags "")
foreach(dir IN LISTS _psp_include)
    string(APPEND _psp_include_flags " -isystem ${dir}")
endforeach()
set(_psp_lib_flags "")
foreach(dir IN LISTS _psp_lib)
    string(APPEND _psp_lib_flags " -L${dir}")
endforeach()

set(CMAKE_C_FLAGS_INIT "${_psp_include_flags}")
set(CMAKE_CXX_FLAGS_INIT "${_psp_include_flags}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_psp_lib_flags}")

# We only need to know that the compiler can emit code, not that it can link a
# host executable (there is no OS to link against).
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
