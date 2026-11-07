#!/usr/bin/env bash
# Builds the program into bin/: build.sh [linux|windows|all]
# Linux builds natively. Windows is cross-compiled with MinGW-w64 from the WebViewWindow checkout's toolchain file.
set -euo pipefail

target="${1:-all}"
case "$target" in
    linux) platforms="linux" ;;
    windows) platforms="windows" ;;
    all) platforms="linux windows" ;;
    *) echo "usage: build.sh [linux|windows|all]" >&2; exit 2 ;;
esac

root="$(cd "$(dirname "$0")" && pwd)"
webview_window_dir="${WEBVIEW_WINDOW_DIR:-$root/../WebViewWindow}"

for platform in $platforms; do
    build="$root/build/$platform"
    if [ "$platform" = windows ]; then
        cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
            -DWEBVIEW_WINDOW_DIR="$webview_window_dir" \
            -DCMAKE_TOOLCHAIN_FILE="$webview_window_dir/cmake/toolchains/mingw-w64.cmake"
    else
        cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DWEBVIEW_WINDOW_DIR="$webview_window_dir"
    fi
    cmake --build "$build"
done

echo "Built $platforms into $root/bin"
