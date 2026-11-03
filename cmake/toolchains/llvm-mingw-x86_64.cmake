# Cross-compiles the Windows x64 VST3 on Linux/macOS with llvm-mingw (https://github.com/mstorsjo/llvm-mingw).
#   cmake -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/llvm-mingw-x86_64.cmake -DLLVM_MINGW_ROOT=/path/to/llvm-mingw
# On Windows itself use Visual Studio 2022 instead (see README); this file is for building without a Windows PC.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

if(NOT LLVM_MINGW_ROOT)
    set(LLVM_MINGW_ROOT "$ENV{LLVM_MINGW_ROOT}")
endif()
set(LLVM_MINGW_ROOT "${LLVM_MINGW_ROOT}" CACHE PATH "llvm-mingw installation")

set(_triple x86_64-w64-mingw32)
set(CMAKE_C_COMPILER   "${LLVM_MINGW_ROOT}/bin/${_triple}-clang")
set(CMAKE_CXX_COMPILER "${LLVM_MINGW_ROOT}/bin/${_triple}-clang++")
set(CMAKE_RC_COMPILER  "${LLVM_MINGW_ROOT}/bin/${_triple}-windres")
set(CMAKE_AR           "${LLVM_MINGW_ROOT}/bin/llvm-ar")
set(CMAKE_RANLIB       "${LLVM_MINGW_ROOT}/bin/llvm-ranlib")

set(CMAKE_FIND_ROOT_PATH "${LLVM_MINGW_ROOT}/${_triple}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Self-contained binary: no libc++ / libunwind / winpthread DLLs next to the plugin.
set(CMAKE_EXE_LINKER_FLAGS_INIT    "-static")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-static")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-static")

# JUCE 8 refuses MinGW and expects a few Windows SDK names that mingw-w64 lacks; see cmake/mingw-compat/.
# The build also needs a JUCE checkout with the two-line patch in cmake/patches/juce-llvm-mingw.patch.
get_filename_component(_compat "${CMAKE_CURRENT_LIST_DIR}/../mingw-compat" ABSOLUTE)
set(CMAKE_CXX_FLAGS_INIT "-isystem \"${_compat}\" -include \"${_compat}/clipline_mingw_compat.h\" -Wno-unknown-pragmas")   # JUCE uses MSVC-only pragmas
set(CMAKE_C_FLAGS_INIT   "-isystem \"${_compat}\"")

# JUCE requests its system libraries with #pragma comment(lib, ...), which the MinGW linker ignores.
set(CMAKE_CXX_STANDARD_LIBRARIES
    "-lws2_32 -lwinmm -lshlwapi -lversion -lwininet -ldbghelp -limm32 -lcomctl32 -lcomdlg32 -ld2d1 -ld3d11 -ldxgi -ldxguid -ldcomp -ldwrite -ldwmapi -lopengl32 -lole32 -loleaut32 -luuid -lshell32 -luser32 -lgdi32 -ladvapi32 -lrpcrt4 -lsetupapi -lsapi -lkernel32")
