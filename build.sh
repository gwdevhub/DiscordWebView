#!/usr/bin/env bash
# Builds the program in Docker into bin/: build.sh [linux|windows|all]
# WebViewWindow is read from WEBVIEW_WINDOW_DIR, which defaults to a checkout beside this one.
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
if [ ! -f "$webview_window_dir/CMakeLists.txt" ]; then
    echo "WebViewWindow isn't at $webview_window_dir. Check it out there, or set WEBVIEW_WINDOW_DIR to its folder." >&2
    exit 1
fi

if ! docker info >/dev/null 2>&1; then
    echo "Docker isn't running: start the Docker daemon and run this again." >&2
    exit 1
fi

# The builder image is tagged by its Dockerfile.toolchain hash, so the compilers are only installed again when that file changes.
builder_hash="$(sha256sum "$root/Dockerfile.toolchain" | cut -c1-12)"
builder_image="discordwebview-builder:$builder_hash"
if ! docker image inspect "$builder_image" >/dev/null 2>&1; then
    docker build -t "$builder_image" -f "$root/Dockerfile.toolchain" "$root"
    # Older builder images are dropped so each change doesn't leave another one behind.
    docker image ls discordwebview-builder --format '{{.Repository}}:{{.Tag}}' | { grep -v ":$builder_hash\$" || true; } | xargs -r docker rmi >/dev/null
fi

docker build --target export --output "type=local,dest=$root/bin" \
    --build-arg BUILDER_IMAGE="$builder_image" --build-arg PLATFORMS="$platforms" \
    --build-context webview="$webview_window_dir" \
    -f "$root/Dockerfile" "$root"

echo "Built $platforms into $root/bin"
