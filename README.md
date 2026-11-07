# DiscordWebView

Runs Discord's web client (`https://discord.com/app`) in a WebViewWindow window, instead of the Electron app. The repo
contains none of Discord's code: the page is loaded from discord.com at run time.

## Build (Linux)

Needs GTK 3, WebKitGTK 4.1, CMake 3.20+, Ninja and a C++20 compiler. The WebViewWindow checkout is expected next to this
one (`../WebViewWindow`); point `WEBVIEW_WINDOW_DIR` at another checkout if it lives elsewhere.

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ./build/discord_webview

## Known limitations

- The window is undecorated (WebViewWindow always creates frameless windows on Linux). Move it with Alt+drag and close it with
  Alt+F4.
- Links that open a new window or tab do nothing: WebViewWindow has no new-window handling on Linux yet.
- Voice and video calls, notifications, tray icon and the Discord desktop-only features are not available. WebKitGTK's WebRTC
  support is limited.
- Closing the window quits the program.
