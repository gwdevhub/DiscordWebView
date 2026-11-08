# DiscordWebView

Runs Discord's web client (`https://discord.com/app`) in a WebViewWindow window, instead of the Electron app. The repo
contains none of Discord's code: the page is loaded from discord.com at run time.

## Build

Needs Docker. The WebViewWindow checkout is expected next to this one (`../WebViewWindow`); point `WEBVIEW_WINDOW_DIR` at another
checkout if it lives elsewhere.

    ./build.sh [linux|windows|all]

The builds run in Docker: `Dockerfile.toolchain` holds the compilers (GCC for Linux, MinGW-w64 for Windows), and `Dockerfile` builds
the program. The results go to `bin/linux/release/` and `bin/windows/release/`. The Windows build needs `WebView2Loader.dll` beside
the `.exe`, which the build copies there.

CI (`.github/workflows/build.yml`) runs `build.sh all` on every push to `main` and on pull requests, and uploads `bin/` as an artifact.

## Known limitations

- The window is undecorated (WebViewWindow always creates frameless windows on Linux). The page draws its own title bar: move the
  window with its bar, and close it with its close button or Alt+F4.
- Links to other sites open in the default browser. Links to discord.com load in the window.
- Voice and video calls don't work yet. Discord's desktop voice engine isn't available here, and the page gets the microphone,
  camera and notifications only as a browser would. `docs/MISSING_FEATURES.md` lists what the desktop app has and this one lacks.
- Closing the window quits the program.
