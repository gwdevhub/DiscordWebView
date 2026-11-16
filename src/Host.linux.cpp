// The desktop functions the host defines for the library on Linux. The pointer and the monitors' work areas come from the library
// itself, which already loads GTK.

#include <spawn.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "Host.h"
#include "UpdateCheck.h"
#include "WebViewWindow.h"

extern char** environ;

namespace {

// The socket a running copy listens on, in the settings folder.
std::filesystem::path showSocketPath() {
    return settingsFolder() / "discord-webview.sock";
}

// The listening socket, so stopListeningForShowRequests can end its accept. -1 when this copy isn't listening.
std::atomic<int> listeningSocket{-1};

// The address for the socket path, or false when the path is too long for one.
bool showSocketAddress(const std::string& path, sockaddr_un& address) {
    if (path.size() >= sizeof(address.sun_path)) {
        return false;
    }
    address = sockaddr_un{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
    return true;
}

}  // namespace

bool forwardShowRequest() {
    sockaddr_un address{};
    if (!showSocketAddress(showSocketPath().string(), address)) {
        return false;
    }
    const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return false;
    }
    bool sent = connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
    if (sent) {
        static const char request[] = "show\n";
        sent = send(fd, request, sizeof(request) - 1, MSG_NOSIGNAL) == static_cast<ssize_t>(sizeof(request) - 1);
    }
    if (sent) {
        // The running copy answers "ok" once it has taken the request. Without that, nothing is listening, and this copy carries on.
        timeval wait{2, 0};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &wait, sizeof(wait));
        char reply[8] = {};
        const ssize_t received = recv(fd, reply, sizeof(reply) - 1, 0);
        sent = received >= 2 && std::string(reply, static_cast<std::size_t>(received)).rfind("ok", 0) == 0;
    }
    close(fd);
    return sent;
}

void listenForShowRequests(std::function<void()> handle) {
    const std::filesystem::path path = showSocketPath();
    sockaddr_un address{};
    if (!showSocketAddress(path.string(), address)) {
        std::fprintf(stderr, "could not listen for other starts: the path is too long\n");
        return;
    }
    std::error_code ignored;
    std::filesystem::create_directories(path.parent_path(), ignored);
    const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return;
    }
    // Nothing answered forwardShowRequest, so a socket left by a copy that exited is replaced.
    unlink(path.c_str());
    if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(fd, 8) != 0) {
        std::fprintf(stderr, "could not listen for other starts\n");
        close(fd);
        return;
    }
    listeningSocket = fd;
    std::thread([fd, handle = std::move(handle)] {
        for (;;) {
            const int client = accept4(fd, nullptr, nullptr, SOCK_CLOEXEC);
            if (client < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
            // A client that connects and says nothing gives up after two seconds, so it can't hold up the next one.
            timeval wait{2, 0};
            setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &wait, sizeof(wait));
            std::string line;
            char buffer[64];
            ssize_t received = 0;
            while (line.find('\n') == std::string::npos && line.size() < sizeof(buffer) &&
                   (received = recv(client, buffer, sizeof(buffer), 0)) > 0) {
                line.append(buffer, static_cast<std::size_t>(received));
            }
            const bool show = line == "show\n";
            if (show) {
                send(client, "ok\n", 3, MSG_NOSIGNAL);
            }
            close(client);
            if (show) {
                handle();
            }
        }
        close(fd);
    }).detach();
}

void stopListeningForShowRequests() {
    const int fd = listeningSocket.exchange(-1);
    if (fd >= 0) {
        // Ends the accept in the listener, which then closes the socket.
        shutdown(fd, SHUT_RDWR);
    }
    unlink(showSocketPath().c_str());
}

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
