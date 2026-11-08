# Desktop features the Discord app has that DiscordWebView lacks

The Discord desktop app is an Electron shell around the same web client this program loads. The shell gives the web client a
bridge, `window.DiscordNative`, which the page uses for audio, video, screen sharing, notifications, badges, file dialogs and
more. This program loads `https://discord.com/app` into a WebView and does not provide that bridge, so the web client runs
as it would in a plain browser.

The inventory below comes from the desktop build 1.0.161 (Linux, Flatpak): `resources/app.asar` and
`modules/discord_desktop_core/core.asar`, in particular `mainScreenPreload.js`, where `DiscordNative` is defined. Module names
are the keys of that object. The main-process side of the channels (`bundle.js`) was not read.

## Status key

- **Missing**: not started.
- **Partial**: some of the behaviour is in place. The entry says what.
- **Done**: implemented and covered by a test. The test is named in the entry.

## Audio and video

| Feature | Discord bridge | Status |
| --- | --- | --- |
| Voice and video engine | `nativeModules.requireModule('discord_voice')` (`discord_voice.node`, a Node addon) | Missing. Needs a Node-addon host, so it can't be dropped in. |
| Noise suppression | `nativeModules` → `discord_krisp` | Missing (depends on the voice engine). |
| Microphone and camera permission | WebView permission requests | Done: allowed for discord.com (`PagePermissions`). Not yet checked with a live call. |
| Screen share sources | `desktopCapture.getDesktopCaptureSources`, `window.getMediaSourceId` | Missing. |
| Keep the display awake during calls | `powerSaveBlocker.blockDisplaySleep` / `unblockDisplaySleep` | Missing. |
| Network time for voice | `ntpClock` | Missing. |

## Permissions

| Feature | Discord bridge | Status |
| --- | --- | --- |
| Microphone, camera and notifications allowed for discord.com | (WebView) | Done: `src/PagePermissions.cpp`, `tests/PagePermissions_test.cpp` (unit), and `WebViewWindow/tests/linux_permission_test.sh` (Linux, live). |
| Other permission kinds | (WebView) | Left to the platform default. |

## Notifications and badges

| Feature | Discord bridge | Status |
| --- | --- | --- |
| Desktop notifications | `ipc`: `NOTIFICATION_SHOW`, `NOTIFICATION_CLOSE`, `NOTIFICATIONS_*` | Done on Linux: the page's Notification API is allowed for discord.com, and WebKitGTK presents the notification to the desktop (`WebViewWindow/tests/linux_notification_test.sh` watches the session bus for it). Windows: not checked. The `ipc` channels aren't used; WebKit and WebView2 present the notifications themselves. |
| Unread badge on the taskbar or dock | `ipc`: `APP_SET_BADGE_COUNT`, `APP_DOCK_SET_BADGE`, `APP_DOCK_BOUNCE` | Partial: Discord's title carries the count, `(3) Discord`, and the custom title bar shows it. No taskbar or dock badge. |
| Tray icon state (unread, mute) | `ipc`: `SYSTEM_TRAY_SET_STATES`, `SYSTEM_TRAY_SET_ICON`, `SYSTEM_TRAY_SET_STRINGS` | Missing. The tray icon exists, but Discord doesn't update it. |
| Taskbar progress and flash | `window.setProgressBar`, `window.flashFrame` | Missing. |
| Windows taskbar thumbnail buttons | `thumbar.setThumbarButtons` | Missing (Windows only). |

## Window control

| Feature | Discord bridge | Status |
| --- | --- | --- |
| Always on top | `window.setAlwaysOnTop`, `isAlwaysOnTop` | Missing. |
| Background throttling and frame rate | `window.setBackgroundThrottling`, `setFrameRate` | Missing. |
| Minimum size | `window.setMinimumSize` | Missing. |
| Zoom | `window.setZoomFactor` | Missing. |
| Picture-in-picture window | `window.openDocumentPip` | Missing. |
| Content protection (hide from screen capture) | `window.setContentProtection` | Missing. |

## Files, clipboard and downloads

| Feature | Discord bridge | Status |
| --- | --- | --- |
| Downloads saved to disk | (WebView download events) | Done: saved to the Downloads folder under the suggested name, never over an existing file (`tests/Downloads_test.cpp`; `WebViewWindow/tests/linux_download_test.sh` and `windows_download_test.ps1`). |
| Open and save dialogs | `fileManager.showOpenDialog`, `saveWithDialog`, `showItemInFolder` | Missing. The page can't reach a host dialog yet: see the bridge plan below. |
| Native clipboard (images, files, cut) | `clipboard.copyImage`, `copyFile`, `cut`, `paste`, `read` | Missing. The web clipboard API is untested. |

## Settings, storage and system

| Feature | Discord bridge | Status |
| --- | --- | --- |
| Persistent settings | `settings.get`, `set` | Missing. The page keeps its own state in the WebView's storage. |
| Encrypted storage for tokens | `safeStorage.encryptString`, `decryptString` | Missing. |
| Launch at login, start minimised, minimise to tray | `app`: `TOGGLE_OPEN_ON_STARTUP`, `TOGGLE_START_MINIMIZED`, `TOGGLE_MINIMIZE_TO_TRAY` | Missing. A tray menu entry exists, but the page's toggles don't reach it. |
| Relaunch | `app.relaunch` | Missing. |
| Power events (suspend, resume, lock) and idle time | `powerMonitor` | Missing. |
| Spell check | `spellCheck` | Missing. |
| Hardware acceleration switch | `gpuSettings` | Missing. |
| Display count | `hardware.getDisplayCount` | Missing. |
| Feature negotiation | `features.supports`, `declareSupported` | Missing. The web client asks which desktop features exist. |
| Crash reporting | `crashReporter`, `processUtils.setCrashInformation` | Missing. |
| Image conversion (HEIC, JXR) | `sysimg` | Missing. |
| Accessibility state | `accessibility.getEnabled` | Missing. |
| Game integrations | `riotGames`, `cs2Gsi`, `dotaGsi` | Missing. Out of scope for this program. |

## Verification

- `WebViewWindow/tests/linux_permission_test.sh`: a local page asks for the microphone and notifications. Without a host answer
  the microphone is refused and notifications are at the default. With `--allow-permissions` both are granted. Passes on Linux.
- `WebViewWindow/tests/linux_example_test.sh`: the existing window checks still pass after the change.
- `tests/PagePermissions_test.cpp` (`cmake -DDW_BUILD_TESTS=ON`, then `ctest`): which addresses count as Discord's. Passes.
- Not checked: the Windows build of the permission hook (it compiles with MinGW, but hasn't run), a live Discord login, and any
  voice or video call.

## Plan

1. Permissions for microphone, camera and notifications, by a hook in WebViewWindow. Done.
2. Downloads. Done. A save dialog would need a page-to-host call, which is item 6.
3. Settings and `safeStorage` through the host.
4. Badges and tray state from the page, through a bridge the page can call.
5. Voice and video. Needs a decision: the engine is a Node addon, so this either waits for a different approach or stays
   out of scope.
6. A bridge the page can call. Most of the features above need the web client to call `DiscordNative`, and it only does so when
   the object exists. Providing it is a design decision: it would switch the web client into desktop mode, and each call it makes
   would then need a real answer. This needs a decision before any more of the bridge is built.
