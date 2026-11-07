// The desktop functions the host defines for the library on Linux. The pointer and the monitors' work areas come from the library
// itself, which already loads GTK.

#include <dlfcn.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>

#include "UpdateCheck.h"
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

std::optional<std::string> httpGet(const std::string& url) {
    int fds[2];
    if (pipe(fds) != 0) {
        return std::nullopt;
    }
    // curl writes the body to the pipe, and fails on an HTTP error, so only a successful body comes back.
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, fds[0]);
    posix_spawn_file_actions_addclose(&actions, fds[1]);
    char* argv[] = {const_cast<char*>("curl"),
                    const_cast<char*>("-sfL"),
                    const_cast<char*>("--max-time"),
                    const_cast<char*>("20"),
                    const_cast<char*>("-H"),
                    const_cast<char*>("Accept: application/vnd.github+json"),
                    const_cast<char*>("-H"),
                    const_cast<char*>("User-Agent: DiscordWebView"),
                    const_cast<char*>(url.c_str()),
                    nullptr};
    pid_t pid = 0;
    const int spawned = posix_spawnp(&pid, "curl", &actions, nullptr, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(fds[1]);
    if (spawned != 0) {
        close(fds[0]);
        return std::nullopt;
    }
    std::string body;
    char buffer[4096];
    ssize_t count = 0;
    while ((count = read(fds[0], buffer, sizeof buffer)) > 0) {
        body.append(buffer, static_cast<std::size_t>(count));
    }
    close(fds[0]);
    int status = 0;
    if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return std::nullopt;
    }
    return body;
}

// The graphics setup before the window opens. With the NVIDIA driver installed, EGL uses only that driver: loading Mesa's EGL as
// well pulls Mesa's gallium and LLVM into the web process, about 50 MB of resident memory with no benefit on this machine.
void configureGraphics() {
    if (std::getenv("__EGL_VENDOR_LIBRARY_FILENAMES")) {
        return;
    }
    const char* nvidiaVendor = "/usr/share/glvnd/egl_vendor.d/10_nvidia.json";
    if (!std::filesystem::exists(nvidiaVendor)) {
        return;
    }
    // Only when the driver's library loads: otherwise EGL would have no driver at all.
    if (void* driver = dlopen("libEGL_nvidia.so.0", RTLD_LAZY)) {
        dlclose(driver);
        setenv("__EGL_VENDOR_LIBRARY_FILENAMES", nvidiaVendor, 1);
    }
}

namespace {

// The WebKit functions the memory-pressure settings use. The library is already loaded by the time they're called.
struct MemoryPressureApi {
    void* (*web_context_get_default)() = nullptr;
    void* (*web_context_get_website_data_manager)(void*) = nullptr;
    void* (*settings_new)() = nullptr;
    void (*settings_set_memory_limit)(void*, unsigned int) = nullptr;
    void (*settings_set_conservative_threshold)(void*, double) = nullptr;
    void (*settings_set_strict_threshold)(void*, double) = nullptr;
    void (*settings_set_kill_threshold)(void*, double) = nullptr;
    void (*settings_set_poll_interval)(void*, double) = nullptr;
    void (*data_manager_set_settings)(void*, void*) = nullptr;
    void (*settings_free)(void*) = nullptr;
};

template <typename Fn>
bool bindSymbol(void* library, Fn& target, const char* name) {
    target = reinterpret_cast<Fn>(dlsym(library, name));
    return target != nullptr;
}

}  // namespace

// Sets WebKit's memory-pressure thresholds for the web process, when DW_MEMORY_LIMIT_MB is set. Off by default; set for the test.
void configureMemoryPressure() {
    const char* limitText = std::getenv("DW_MEMORY_LIMIT_MB");
    if (!limitText || !*limitText) {
        return;
    }
    void* library = dlopen("libwebkit2gtk-4.1.so.0", RTLD_LAZY | RTLD_NOLOAD);
    if (!library) {
        return;
    }
    MemoryPressureApi api;
    const bool bound = bindSymbol(library, api.web_context_get_default, "webkit_web_context_get_default") &&
                       bindSymbol(library, api.web_context_get_website_data_manager, "webkit_web_context_get_website_data_manager") &&
                       bindSymbol(library, api.settings_new, "webkit_memory_pressure_settings_new") &&
                       bindSymbol(library, api.settings_set_memory_limit, "webkit_memory_pressure_settings_set_memory_limit") &&
                       bindSymbol(library, api.settings_set_conservative_threshold, "webkit_memory_pressure_settings_set_conservative_threshold") &&
                       bindSymbol(library, api.settings_set_strict_threshold, "webkit_memory_pressure_settings_set_strict_threshold") &&
                       bindSymbol(library, api.settings_set_kill_threshold, "webkit_memory_pressure_settings_set_kill_threshold") &&
                       bindSymbol(library, api.settings_set_poll_interval, "webkit_memory_pressure_settings_set_poll_interval") &&
                       bindSymbol(library, api.data_manager_set_settings, "webkit_website_data_manager_set_memory_pressure_settings") &&
                       bindSymbol(library, api.settings_free, "webkit_memory_pressure_settings_free");
    if (bound) {
        void* manager = api.web_context_get_website_data_manager(api.web_context_get_default());
        void* settings = api.settings_new();
        api.settings_set_memory_limit(settings, static_cast<unsigned int>(std::strtoul(limitText, nullptr, 10)));
        api.settings_set_conservative_threshold(settings, 0.3);
        api.settings_set_strict_threshold(settings, 0.5);
        api.settings_set_kill_threshold(settings, 0.95);
        api.settings_set_poll_interval(settings, 2.0);
        api.data_manager_set_settings(manager, settings);
        api.settings_free(settings);
    }
    dlclose(library);
}
