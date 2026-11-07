# Builds the program for each platform from the sources copied in, then exports bin/ to the host.
# The builder image comes from Dockerfile.toolchain, and WebViewWindow comes in as the "webview" build context; build.sh does both.
ARG BUILDER_IMAGE=discordwebview-builder:latest
FROM ${BUILDER_IMAGE} AS build

ARG BUILDER_IMAGE
ARG PLATFORMS="linux windows"
# The commit count, which the release tags and the update check use. The container has no .git to count from.
ARG BUILD_NUMBER=0

WORKDIR /src
COPY CMakeLists.txt ./
COPY src src
COPY --from=webview CMakeLists.txt WebViewWindow/
COPY --from=webview include WebViewWindow/include
COPY --from=webview src WebViewWindow/src
COPY --from=webview platform WebViewWindow/platform
COPY --from=webview third_party WebViewWindow/third_party
COPY --from=webview cmake WebViewWindow/cmake

# The build folders persist between builds, so only the changed files compile. An interrupted compile leaves an empty object
# that looks up to date, so empty objects are removed first to make them compile again.
RUN --mount=type=cache,id=discordwebview-${BUILDER_IMAGE},target=/src/build,sharing=locked \
    find build \( -name '*.o' -o -name '*.obj' \) -size 0 -delete 2>/dev/null; \
    for platform in $PLATFORMS; do \
      toolchain=""; \
      if [ "$platform" = windows ]; then toolchain="-DCMAKE_TOOLCHAIN_FILE=/src/WebViewWindow/cmake/toolchains/mingw-w64.cmake"; fi; \
      cmake -S . -B "build/$platform" -G Ninja -DCMAKE_BUILD_TYPE=Release -DWEBVIEW_WINDOW_DIR=/src/WebViewWindow -DDW_BUILD_NUMBER="$BUILD_NUMBER" $toolchain \
      && cmake --build "build/$platform" || exit 1; \
    done

FROM scratch AS export
COPY --from=build /src/bin/ /
