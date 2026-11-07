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

#include "PageScript.h"
#include "UpdateCheck.h"
#include "WebViewWindow.h"

// Opens a web address in the default browser. Each platform's host file defines it. The address is never run through a shell.
void launchUrl(const std::string& url);

// The folder the program keeps its settings in. Each platform's host file defines it.
std::filesystem::path settingsFolder();

// Graphics setup before the window opens, and the web process's memory-pressure thresholds once it has. Each host file defines them.
void configureGraphics();
void configureMemoryPressure();

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

// Opens a web address in the default browser, when it's an http or https address.
void openInBrowser(const std::string& url) {
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
        return;
    }
    launchUrl(url);
}

class DiscordWindow : public WebViewWindow {
public:
    DiscordWindow() : WebViewWindow(options()) {
        auto& methods = handler();
        methods.on("moveDrag", [](WebViewWindow& window, const Json&, const Reply& reply) {
            window.beginMove();
            reply.result(nullptr);
        });
        methods.on("resizeDrag", [](WebViewWindow& window, const Json& args, const Reply& reply) {
            const int edge = args.at(0).get<int>();
            if (edge >= 0 && edge <= static_cast<int>(ResizeEdge::SouthEast)) {
                window.beginResize(static_cast<ResizeEdge>(edge));
            }
            reply.result(nullptr);
        });
        methods.on("minimize", [](WebViewWindow& window, const Json&, const Reply& reply) {
            window.minimize();
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

    std::string devUrl() const override { return kDiscordUrl; }
    std::string title() const override { return "Discord"; }

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
        // Discord's page is opaque, so there is nothing to see through.
        options.transparent = false;
        options.hardwareAcceleration = HardwareAcceleration::Always;
        return options;
    }

    // The script that adds the update button to the bar, for the address of the newer release.
    std::string updateScript() const {
        return "window.gwShowUpdate && window.gwShowUpdate(" + Json(updateUrl_).dump() + ");";
    }

    bool frameRestored_ = false;
    std::optional<SavedFrame> lastSaved_;
    std::string updateUrl_;
};

}  // namespace

int main() {
    try {
        setApplicationName("Discord");
        configureGraphics();
        initializeWindowing();
        DiscordWindow window;
        window.open();
        configureMemoryPressure();
        window.restoreFrame();
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
