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
        // A copy that's already running is asked to show its window, and this start ends.
        if (forwardShowRequest()) {
            return 0;
        }
        setApplicationName("Discord");
        setApplicationIcon(kAppIconPng);
        initializeWindowing();
        // An update leaves its old program beside the new one, and the new one removes it on its first start.
        if (const auto current = currentExecutable(); !current.empty()) {
            removeUpdateLeftovers(current);
        }
        Application app;
        // Listening starts before the window opens, so a start made while it's opening is handed over too. Each request is passed
        // to the window's thread.
        listenForShowRequests([&app] { runOnUiThread([&app] { app.showDiscord(); }); });
        app.run();
        stopListeningForShowRequests();
    } catch (const RuntimeMissingError&) {
        // The dialog has already said what's missing.
        return 1;
    } catch (const std::exception& error) {
        showError(error.what());
        return 1;
    }
    return 0;
}
