// The desktop functions the host defines for the library on Linux. The pointer and the monitors' work areas come from the library
// itself, which already loads GTK.

#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <csignal>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

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

const char* releaseAssetName() {
    return "discord-webview-linux";
}

std::filesystem::path currentExecutable() {
    std::error_code error;
    const auto self = std::filesystem::read_symlink("/proc/self/exe", error);
    return error ? std::filesystem::path() : self;
}

bool downloadTo(const std::string& url, const std::filesystem::path& dest, const std::atomic<bool>& cancel) {
    const std::string target = dest.string();
    std::vector<std::string> arguments{"curl", "-sfL", "--max-time", "600", "-o", target, url};
    std::vector<char*> argv;
    for (const std::string& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);
    pid_t pid = 0;
    bool ok = false;
    if (posix_spawnp(&pid, argv[0], nullptr, nullptr, argv.data(), environ) == 0) {
        // The child is polled, so a cancel stops the download part way through rather than when it finishes.
        for (;;) {
            int status = 0;
            const pid_t finished = waitpid(pid, &status, WNOHANG);
            if (finished == pid) {
                ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
                break;
            }
            if (finished < 0) {
                break;
            }
            if (cancel.load()) {
                kill(pid, SIGTERM);
                waitpid(pid, &status, 0);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    if (!ok) {
        std::error_code error;
        std::filesystem::remove(dest, error);
    }
    return ok;
}

void replaceExecutable(const std::filesystem::path& current, const std::filesystem::path& staged) {
    namespace fs = std::filesystem;
    const fs::path old = current.parent_path() / (".old-" + current.filename().string());
    std::error_code error;
    fs::permissions(staged,
                    fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec | fs::perms::others_read | fs::perms::others_exec,
                    fs::perm_options::replace, error);
    if (error) {
        fs::remove(staged, error);
        throw std::runtime_error("The update couldn't be made runnable: " + error.message());
    }
    // The running program is moved aside rather than overwritten, so a failed swap can be undone.
    fs::remove(old, error);
    fs::rename(current, old, error);
    if (error) {
        fs::remove(staged, error);
        throw std::runtime_error("The program couldn't be moved aside for the update: " + error.message());
    }
    fs::rename(staged, current, error);
    if (error) {
        std::error_code restore;
        fs::rename(old, current, restore);
        fs::remove(staged, restore);
        throw std::runtime_error("The update couldn't be put in place: " + error.message());
    }
}

void restartProgram(const std::filesystem::path& program) {
    const std::string path = program.string();
    char* argv[] = {const_cast<char*>(path.c_str()), nullptr};
    pid_t pid = 0;
    if (posix_spawn(&pid, path.c_str(), nullptr, nullptr, argv, environ) == 0) {
        // Reaped off the UI thread, so the new copy isn't left a zombie if this one keeps running for a moment.
        std::thread([pid] {
            int status = 0;
            waitpid(pid, &status, 0);
        }).detach();
    }
}

void removeUpdateLeftovers(const std::filesystem::path& current) {
    std::error_code error;
    std::filesystem::remove(current.parent_path() / (".old-" + current.filename().string()), error);
}
