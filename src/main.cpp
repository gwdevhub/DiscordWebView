// Shows Discord's web client in a WebViewWindow window. Nothing from Discord is bundled here: the page is loaded from
// discord.com at run time. The windows and what they share are in Application.h.

#include <exception>

#include "AppIcon.h"
#include "Application.h"
#include "Host.h"
#include "UpdateCheck.h"
#include "WebViewWindow.h"

int main() {
    try {
        setApplicationName("Discord");
        setApplicationIcon(kAppIconPng);
        initializeWindowing();
        // An update leaves its old program beside the new one, and the new one removes it on its first start.
        if (const auto current = currentExecutable(); !current.empty()) {
            removeUpdateLeftovers(current);
        }
        Application app;
        app.run();
    } catch (const RuntimeMissingError&) {
        // The dialog has already said what's missing.
        return 1;
    } catch (const std::exception& error) {
        showError(error.what());
        return 1;
    }
    return 0;
}
