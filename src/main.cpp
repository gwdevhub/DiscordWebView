// Shows Discord's web client in a WebViewWindow window. Nothing from Discord is bundled here: the page is loaded from
// discord.com at run time, and PageScript.h adds this program's title bar and link handling to it.

#include <algorithm>
#include <atomic>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include "AppIcon.h"
#include "LoadingPage.h"
#include "PageScript.h"
#include "UpdateCheck.h"
#include "WebViewWindow.h"

// Opens a web address in the default browser. Each platform's host file defines it. The address is never run through a shell.
void launchUrl(const std::string& url);

// The folder the program keeps its settings in. Each platform's host file defines it.
std::filesystem::path settingsFolder();

namespace {

constexpr const char* kDiscordUrl = "https://discord.com/app";

// The window's position on the screen and its size without the UI scale, as the window saved them.
struct SavedFrame {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool operator==(const SavedFrame& other) const {
        return x == other.x && y == other.y && width == other.width && height == other.height;
    }
};

std::filesystem::path frameFile() {
    return settingsFolder() / "window.json";
}

std::optional<SavedFrame> loadFrame() {
    try {
        std::ifstream in(frameFile());
        if (!in) {
            return std::nullopt;
        }
        const Json saved = Json::parse(in);
        return SavedFrame{saved.at("x").get<int>(), saved.at("y").get<int>(), saved.at("width").get<int>(),
                          saved.at("height").get<int>()};
    } catch (const std::exception&) {
        // A missing or unreadable file means the window opens where it would by default.
        return std::nullopt;
    }
}

void saveFrame(const SavedFrame& frame) {
    try {
        std::filesystem::create_directories(frameFile().parent_path());
        std::ofstream out(frameFile(), std::ios::trunc);
        out << Json{{"x", frame.x}, {"y", frame.y}, {"width", frame.width}, {"height", frame.height}}.dump(2) << '\n';
    } catch (const std::exception&) {
        // Failing to save the frame isn't worth stopping the program for.
    }
}

std::filesystem::path behaviourFile() {
    return settingsFolder() / "behaviour.json";
}

// Whether minimising keeps the window in the taskbar. The default is to keep it there.
bool loadKeepInTaskbar() {
    try {
        std::ifstream in(behaviourFile());
        if (!in) {
            return true;
        }
        return Json::parse(in).at("keepInTaskbar").get<bool>();
    } catch (const std::exception&) {
        return true;
    }
}

void saveKeepInTaskbar(bool keepInTaskbar) {
    try {
        std::filesystem::create_directories(behaviourFile().parent_path());
        std::ofstream out(behaviourFile(), std::ios::trunc);
        out << Json{{"keepInTaskbar", keepInTaskbar}}.dump(2) << '\n';
    } catch (const std::exception&) {
        // The choice still applies until the program closes, so a failed save isn't worth stopping for.
    }
}

// Opens a web address in the default browser, when it's an http or https address.
void openInBrowser(const std::string& url) {
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
        return;
    }
    launchUrl(url);
}

class DiscordWindow : public WebViewWindow {
public:
    DiscordWindow() : WebViewWindow(options()), keepInTaskbar_(loadKeepInTaskbar()) {
        auto& methods = handler();
        methods.on("moveDrag", [](WebViewWindow& window, const Json&, const Reply& reply) {
            window.beginMove();
            reply.result(nullptr);
        });
        methods.on("minimize", [](WebViewWindow& window, const Json&, const Reply& reply) {
            static_cast<DiscordWindow&>(window).minimiseToTaskbarOrTray();
            reply.result(nullptr);
        });
        methods.on("setTaskbarWhenMinimised", [](WebViewWindow& window, const Json& args, const Reply& reply) {
            static_cast<DiscordWindow&>(window).setKeepInTaskbar(args.at(0).get<bool>());
            reply.result(nullptr);
        });
        methods.on("toggleMaximize", [](WebViewWindow& window, const Json&, const Reply& reply) {
            window.toggleMaximize();
            reply.result(nullptr);
        });
        methods.on("close", [](WebViewWindow& window, const Json&, const Reply& reply) {
            window.close();
            reply.result(nullptr);
        });
        methods.on("openDevTools", [](WebViewWindow& window, const Json&, const Reply& reply) {
            window.openDevTools();
            reply.result(nullptr);
        });
        methods.on("openExternal", [](WebViewWindow&, const Json& args, const Reply& reply) {
            openInBrowser(args.at(0).get<std::string>());
            reply.result(nullptr);
        });
    }

    // The window opens on the loading page, which moves on to Discord. A reload after the page's process died starts over the same way.
    std::string html() const override { return loadingPage(Json(kDiscordUrl).dump()); }
    std::string title() const override { return "Discord"; }

    // Puts the window's icon in the system tray, with a menu to show it or quit. Where the platform has no tray icon yet, minimising
    // always uses the taskbar.
    void showTray() {
        trayShown_ = showTrayIcon(
            "Discord", [this] { present(); },
            {{"Show Discord", [this] { present(); }}, {"Quit", [this] { close(); }}});
    }

    // Minimise, as the bar's button asks for it. When minimising should go to the tray, the window is hidden instead, and its web
    // view is kept so it comes back without loading Discord again.
    void minimiseToTaskbarOrTray() {
        if (!keepInTaskbar_ && trayShown_) {
            hide();
            keepWebView();
        } else {
            minimize();
        }
    }

    // Saves whether minimising keeps the window in the taskbar. The page's choice takes effect at once.
    void setKeepInTaskbar(bool keepInTaskbar) {
        keepInTaskbar_ = keepInTaskbar;
        saveKeepInTaskbar(keepInTaskbar);
    }

    // Shows the update button on the bar, now and after each page load. Called on the window's thread.
    void showUpdate(const UpdateInfo& update) {
        updateUrl_ = update.url;
        runScript(updateScript());
    }

    // Puts the window where it was last saved, at the size it had. Called once, after open. The library keeps the window inside
    // its monitor's work area, so a saved frame that's now off the screen is brought back in.
    void restoreFrame() {
        if (const auto saved = loadFrame()) {
            resize(std::max(saved->width, options().minWidth), std::max(saved->height, options().minHeight));
            move(saved->x, saved->y);
            lastSaved_ = SavedFrame{saved->x, saved->y, saved->width, saved->height};
        }
        frameRestored_ = true;
    }

protected:
    // Puts the title bar back on each page the window loads, including after navigations, and the update button with it.
    void onPageLoaded() override {
        runScript(kPageScript);
        runScript(behaviourScript());
        if (!updateUrl_.empty()) {
            runScript(updateScript());
        }
    }
    // Closing the only window ends the program.
    void onClosed() override { stopEventLoop(); }
    // Saves the frame after the user moves or resizes the window. Changes before restoreFrame are the window's own starting frame.
    void onFrameChanged() override {
        if (!frameRestored_) {
            return;
        }
        const auto bounds = frame();
        const auto size = frameUnscaled();
        if (!bounds || !size) {
            return;
        }
        const SavedFrame current{bounds->x, bounds->y, size->first, size->second};
        if (!lastSaved_ || !(*lastSaved_ == current)) {
            saveFrame(current);
            lastSaved_ = current;
        }
    }

private:
    static WindowOptions options() {
        WindowOptions options;
        options.title = "Discord";
        options.width = 1280;
        options.height = 800;
        options.minWidth = 480;
        options.minHeight = 360;
        // The library draws the resize strips along the edges, so the page doesn't.
        options.resizeEdges = true;
        // Discord's page is opaque, so there is nothing to see through.
        options.transparent = false;
        options.hardwareAcceleration = HardwareAcceleration::Always;
        return options;
    }

    // Tells the page the saved choice, and whether the tray icon is shown on this platform.
    std::string behaviourScript() const {
        return std::string("window.gwSetBehaviour && window.gwSetBehaviour(") + (keepInTaskbar_ ? "true" : "false") + ", " +
               (trayShown_ ? "true" : "false") + ");";
    }

    // The script that adds the update button to the bar, for the address of the newer release.
    std::string updateScript() const {
        return "window.gwShowUpdate && window.gwShowUpdate(" + Json(updateUrl_).dump() + ");";
    }

    bool frameRestored_ = false;
    std::optional<SavedFrame> lastSaved_;
    std::string updateUrl_;
    bool keepInTaskbar_ = true;
    bool trayShown_ = false;
};

}  // namespace

int main() {
    try {
        setApplicationName("Discord");
        setApplicationIcon(kAppIconPng);
        initializeWindowing();
        DiscordWindow window;
        window.open();
        window.restoreFrame();
        window.showTray();
        // Checks for a newer release in the background. The result comes back on the window's thread, and only while the window is
        // still running: the flag is cleared before the window goes.
        auto running = std::make_shared<std::atomic<bool>>(true);
        std::thread([&window, running] {
            const auto update = latestUpdate();
            if (update) {
                runOnUiThread([&window, running, update = *update] {
                    if (*running) window.showUpdate(update);
                });
            }
        }).detach();
        runEventLoop();
        *running = false;
    } catch (const RuntimeMissingError&) {
        // The dialog has already said what's missing.
        return 1;
    } catch (const std::exception& error) {
        showError(error.what());
        return 1;
    }
    return 0;
}
