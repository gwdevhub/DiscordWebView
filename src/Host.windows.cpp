// The desktop functions the host defines for the library on Windows. src/Host.linux.cpp has the Linux version.

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

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

// Windows has no EGL driver choice to make, and WebKit's memory-pressure thresholds are only tuned on Linux for now.
void configureGraphics() {}
void configureMemoryPressure() {}
