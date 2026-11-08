#include "SettingsWindow.h"

#include "Application.h"
#include "Pages.h"

SettingsWindow::SettingsWindow(Application& app)
    : WebViewWindow(WindowOptions{
          .title = "Settings",
          .width = 440,
          .height = 440,
          .resizable = false,
      }),
      app_(app) {
    auto& methods = handler();
    methods.on("moveDrag", [](WebViewWindow& window, const Json&, const Reply& reply) {
        window.beginMove();
        reply.result(nullptr);
    });
    methods.on("close", [](WebViewWindow& window, const Json&, const Reply& reply) {
        window.close();
        reply.result(nullptr);
    });
    methods.on("setTaskbarWhenMinimised", [](WebViewWindow& window, const Json& args, const Reply& reply) {
        static_cast<SettingsWindow&>(window).app_.setKeepInTaskbar(args.at(0).get<bool>());
        reply.result(nullptr);
    });
    methods.on("checkForUpdate", [](WebViewWindow& window, const Json&, const Reply& reply) {
        static_cast<SettingsWindow&>(window).app_.checkForUpdate();
        reply.result(nullptr);
    });
    methods.on("updateNow", [](WebViewWindow& window, const Json&, const Reply& reply) {
        static_cast<SettingsWindow&>(window).app_.updateNow();
        reply.result(nullptr);
    });
}

std::string SettingsWindow::html() const {
    return pages::settings();
}

std::string SettingsWindow::title() const {
    return "Settings";
}

void SettingsWindow::onPageReady() {
    emit("state", app_.settingsState());
}

bool SettingsWindow::allowClose() {
    hide();
    return false;
}
