// The desktop functions the host defines for the library on Linux. The pointer and the monitors' work areas come from the library
// itself, which already loads GTK.

#include <spawn.h>
#include <sys/wait.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

#include "WebViewWindow.h"

extern char** environ;

void showError(const std::string& message) {
    std::fprintf(stderr, "%s\n", message.c_str());
}

std::filesystem::path settingsFolder() {
    // The XDG config folder, as the desktop spec defines it.
    if (const char* config = std::getenv("XDG_CONFIG_HOME"); config && *config) {
        return std::filesystem::path(config) / "DiscordWebView";
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path(home) / ".config" / "DiscordWebView";
    }
    return std::filesystem::path(".") / "DiscordWebView";
}

void launchUrl(const std::string& url) {
    char* argv[] = {const_cast<char*>("xdg-open"), const_cast<char*>(url.c_str()), nullptr};
    pid_t pid = 0;
    if (posix_spawnp(&pid, "xdg-open", nullptr, nullptr, argv, environ) == 0) {
        // Reaped off the UI thread, since xdg-open may take a while to return.
        std::thread([pid] {
            int status = 0;
            waitpid(pid, &status, 0);
        }).detach();
    }
}
