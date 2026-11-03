#!/usr/bin/env bash
# Cross-compiles the Windows x64 plugin, tests and installer on Linux with llvm-mingw, and runs the tests under
# Wine when it is available. This is how the published installer was built.
#
#   ./scripts/build-windows-cross.sh                    (downloads llvm-mingw into ./.toolchains if needed)
#   LLVM_MINGW_ROOT=/opt/llvm-mingw ./scripts/build-windows-cross.sh
#   JUCE_DIR=/path/to/JUCE ./scripts/build-windows-cross.sh   (a JUCE 8.0.8 checkout; it is patched in a copy)
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT=$PWD
BUILD=${BUILD:-$ROOT/build-win}
LLVM_MINGW_VERSION=${LLVM_MINGW_VERSION:-20250613}

if [ -z "${LLVM_MINGW_ROOT:-}" ]; then
    LLVM_MINGW_ROOT=$ROOT/.toolchains/llvm-mingw-$LLVM_MINGW_VERSION
    if [ ! -x "$LLVM_MINGW_ROOT/bin/x86_64-w64-mingw32-clang++" ]; then
        echo "Downloading llvm-mingw $LLVM_MINGW_VERSION ..."
        mkdir -p "$LLVM_MINGW_ROOT"
        curl -fsSL "https://github.com/mstorsjo/llvm-mingw/releases/download/$LLVM_MINGW_VERSION/llvm-mingw-$LLVM_MINGW_VERSION-ucrt-ubuntu-22.04-x86_64.tar.xz" \
            | tar -xJ -C "$LLVM_MINGW_ROOT" --strip-components=1
    fi
fi

EXTRA=()
if [ -n "${JUCE_DIR:-}" ]; then
    # Patch a private copy so the caller's checkout stays untouched.
    rm -rf "$BUILD/juce-patched" && mkdir -p "$BUILD" && cp -r "$JUCE_DIR" "$BUILD/juce-patched"
    (cd "$BUILD/juce-patched" && cmake -DGIT=git -DPATCH_FILE="$ROOT/cmake/patches/juce-8.0.8-clipline.patch" -P "$ROOT/cmake/ApplyPatch.cmake")
    EXTRA+=(-DCLIPLINE_JUCE_DIR="$BUILD/juce-patched")
fi

# juceaide (a JUCE build tool) is compiled for the host; it does not need the X11 extensions.
export CXXFLAGS="${CXXFLAGS:-} -DJUCE_USE_XRANDR=0 -DJUCE_USE_XINERAMA=0 -DJUCE_USE_XCURSOR=0"

cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchains/llvm-mingw-x86_64.cmake" \
      -DLLVM_MINGW_ROOT="$LLVM_MINGW_ROOT" -DCLIPLINE_ENABLE_LTO=OFF "${EXTRA[@]}"
cmake --build "$BUILD" --parallel

INSTALLER=$(ls "$BUILD"/Installer/Windows/*-Setup.exe)
PLUGIN=$BUILD/Clipline_artefacts/Release/VST3/Clipline.vst3

if command -v wine >/dev/null 2>&1; then
    RUN=(wine); command -v xvfb-run >/dev/null 2>&1 && RUN=(xvfb-run -a wine)
    WINEDEBUG=-all "${RUN[@]}" "$BUILD/Tests/ClipperTests_artefacts/Release/ClipperTests.exe"
    WINEDEBUG=-all "${RUN[@]}" "$BUILD/Tests/ClipperHostCheck_artefacts/Release/ClipperHostCheck.exe" "$(winepath -w "$PLUGIN")"
else
    echo "wine not found: the Windows test executables were built but not run."
fi

echo
echo "Plugin:    $PLUGIN"
echo "Installer: $INSTALLER"
