// The settings window: the taskbar choice for minimising, and the update check and install. It hides when closed, so it opens again
// straight away.
#pragma once

#include <string>

#include "WebViewWindow.h"

class Application;

class SettingsWindow : public WebViewWindow {
public:
    explicit SettingsWindow(Application& app);

    std::string html() const override;
    std::string title() const override;

protected:
    void onPageReady() override;
    // Closing hides the window, which the settings window keeps for its next opening.
    bool allowClose() override;

private:
    Application& app_;
};
