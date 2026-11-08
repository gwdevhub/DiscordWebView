#include "DiscordWindow.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>

#include "Application.h"
#include "Host.h"
#include "PageScript.h"

namespace {

constexpr const char* kDiscordUrl = "https://discord.com/app";
constexpr int kMinWidth = 480;
constexpr int kMinHeight = 360;

// Opens a web address in the default browser, when it's an http or https address.
void openInBrowser(const std::string& url) {
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
        return;
    }
    launchUrl(url);
}

}  // namespace

namespace {

std::filesystem::path frameFile() {
    return settingsFolder() / "window.json";
}

}  // namespace

DiscordWindow::DiscordWindow(Application& app) : WebViewWindow(WindowOptions{
                                                     .title = "Discord",
                                                     .width = 1280,
                                                     .height = 800,
                                                     .minWidth = kMinWidth,
                                                     .minHeight = kMinHeight,
                                                     // The library draws the resize strips along the edges, so the page doesn't.
                                                     .resizeEdges = true,
                                                     // Discord's page is opaque, so there is nothing to see through.
                                                     .transparent = false,
                                                     .hardwareAcceleration = HardwareAcceleration::Always,
                                                 }),
                                                 app_(app) {
    auto& methods = handler();
    methods.on("moveDrag", [](WebViewWindow& window, const Json&, const Reply& reply) {
        window.beginMove();
        reply.result(nullptr);
    });
    methods.on("minimize", [](WebViewWindow& window, const Json&, const Reply& reply) {
        // With the tray icon shown and the taskbar not wanted, minimising hides the window to the tray instead. Its web view is kept,
        // so it comes back without loading Discord again.
        auto& self = static_cast<DiscordWindow&>(window);
        if (!self.app_.keepInTaskbar() && self.app_.trayShown()) {
            window.hide();
            window.keepWebView();
        } else {
            window.minimize();
        }
        reply.result(nullptr);
    });
    methods.on("openSettings", [](WebViewWindow& window, const Json&, const Reply& reply) {
        static_cast<DiscordWindow&>(window).app_.openSettings();
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

std::string DiscordWindow::devUrl() const {
    return kDiscordUrl;
}

std::string DiscordWindow::title() const {
    return "Discord";
}

void DiscordWindow::restoreFrame() {
    try {
        std::ifstream in(frameFile());
        if (in) {
            const Json saved = Json::parse(in);
            const SavedFrame frame{saved.at("x").get<int>(), saved.at("y").get<int>(), saved.at("width").get<int>(),
                                   saved.at("height").get<int>()};
            // The library keeps the window inside its monitor's work area, so a saved frame that's now off the screen is brought back in.
            resize(std::max(frame.width, kMinWidth), std::max(frame.height, kMinHeight));
            move(frame.x, frame.y);
            lastSaved_ = frame;
        }
    } catch (const std::exception&) {
        // A missing or unreadable file means the window opens where it would by default.
    }
    frameRestored_ = true;
}

void DiscordWindow::onPageLoaded() {
    runScript(kPageScript);
    runScript(app_.discordNoticeScript());
}

void DiscordWindow::onPageReady() {
    app_.discordReady();
}

void DiscordWindow::onClosed() {
    app_.discordClosed();
}

// Saves the frame after the user moves or resizes the window. Changes before restoreFrame are the window's own starting frame.
void DiscordWindow::onFrameChanged() {
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
        try {
            std::filesystem::create_directories(frameFile().parent_path());
            std::ofstream out(frameFile(), std::ios::trunc);
            out << Json{{"x", current.x}, {"y", current.y}, {"width", current.width}, {"height", current.height}}.dump(2) << '\n';
        } catch (const std::exception&) {
            // Failing to save the frame isn't worth stopping the program for.
        }
        lastSaved_ = current;
    }
}
