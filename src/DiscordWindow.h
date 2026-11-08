// The window that shows Discord's web client. Discord loads in it directly, hidden until its page reports ready, so the loading window
// covers the wait without a second load.
#pragma once

#include <optional>
#include <string>

#include "WebViewWindow.h"

class Application;

class DiscordWindow : public WebViewWindow {
public:
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

    explicit DiscordWindow(Application& app);

    // Puts the window where it was last saved, at the size it had. Called once, after open.
    void restoreFrame();

    std::string devUrl() const override;
    std::string title() const override;

protected:
    void onPageLoaded() override;
    void onPageReady() override;
    void onClosed() override;
    void onFrameChanged() override;
    std::optional<bool> onPermissionRequest(PagePermission permission, const std::string& pageUrl) override;

private:
    Application& app_;
    bool frameRestored_ = false;
    std::optional<SavedFrame> lastSaved_;
};
