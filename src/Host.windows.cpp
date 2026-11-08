// The desktop functions the host defines for the library on Windows. src/Host.linux.cpp has the Linux version.

#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>

#include "UpdateCheck.h"
#include "WebViewWindow.h"

namespace {

// The UTF-8 text as a wide string, for the Windows calls that take one.
std::wstring toWide(const std::string& text) {
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), length);
    wide.resize(static_cast<size_t>(length) - 1);
    return wide;
}

}  // namespace

void showError(const std::string& message) {
    std::fprintf(stderr, "%s\n", message.c_str());
    MessageBoxA(nullptr, message.c_str(), applicationName().c_str(), MB_OK | MB_ICONERROR);
}

std::filesystem::path settingsFolder() {
    // The roaming application data folder, which is where Windows programs keep their settings.
    if (const char* appData = std::getenv("APPDATA"); appData && *appData) {
        return std::filesystem::path(appData) / "DiscordWebView";
    }
    return std::filesystem::path(".") / "DiscordWebView";
}

void launchUrl(const std::string& url) {
    // ShellExecute takes the address as a wide string, never through a command line.
    ShellExecuteW(nullptr, L"open", toWide(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

std::optional<std::string> httpGet(const std::string& url) {
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE readEnd = nullptr;
    HANDLE writeEnd = nullptr;
    if (!CreatePipe(&readEnd, &writeEnd, &security, 0)) {
        return std::nullopt;
    }
    // Only the child gets the write end, so the read ends when the child exits.
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writeEnd;
    // curl.exe ships with Windows 10 1803 and later. It's a console program, so it's started without a console.
    std::wstring command = L"curl.exe -sfL --max-time 20 -H \"Accept: application/vnd.github+json\" -H \"User-Agent: DiscordWebView\" \"" +
                           toWide(url) + L"\"";
    PROCESS_INFORMATION process{};
    const BOOL started = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup,
                                        &process);
    CloseHandle(writeEnd);
    if (!started) {
        CloseHandle(readEnd);
        return std::nullopt;
    }
    std::string body;
    char buffer[4096];
    DWORD count = 0;
    while (ReadFile(readEnd, buffer, sizeof buffer, &count, nullptr) && count > 0) {
        body.append(buffer, count);
    }
    CloseHandle(readEnd);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (exitCode != 0) {
        return std::nullopt;
    }
    return body;
}

const char* releaseAssetName() {
    return "discord-webview-windows.exe";
}

std::filesystem::path currentExecutable() {
    wchar_t path[MAX_PATH * 4];
    const DWORD length = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    if (length == 0 || length >= std::size(path)) {
        return {};
    }
    return std::filesystem::path(std::wstring(path, length));
}

bool downloadTo(const std::string& url, const std::filesystem::path& dest, const std::atomic<bool>& cancel) {
    // curl.exe ships with Windows 10 1803 and later. It's started without a console, as httpGet does.
    std::wstring command = L"curl.exe -sfL --max-time 600 -o \"" + dest.wstring() + L"\" \"" + toWide(url) + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const BOOL started = CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                                        &startup, &process);
    bool ok = false;
    if (started) {
        // The child is polled, so a cancel stops the download part way through rather than when it finishes.
        for (;;) {
            const DWORD wait = WaitForSingleObject(process.hProcess, 100);
            if (wait == WAIT_OBJECT_0) {
                DWORD exitCode = 1;
                GetExitCodeProcess(process.hProcess, &exitCode);
                ok = exitCode == 0;
                break;
            }
            if (wait != WAIT_TIMEOUT) {
                break;
            }
            if (cancel.load()) {
                TerminateProcess(process.hProcess, 1);
                WaitForSingleObject(process.hProcess, INFINITE);
                break;
            }
        }
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    if (!ok) {
        std::error_code error;
        std::filesystem::remove(dest, error);
    }
    return ok;
}

void replaceExecutable(const std::filesystem::path& current, const std::filesystem::path& staged) {
    namespace fs = std::filesystem;
    // Windows locks a running exe against deleting, but not against renaming, so the running one is renamed aside.
    const fs::path old = current.parent_path() / ".old.exe";
    std::error_code error;
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
    std::wstring command = L"\"" + program.wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
}

void removeUpdateLeftovers(const std::filesystem::path& current) {
    std::error_code error;
    std::filesystem::remove(current.parent_path() / ".old.exe", error);
}
