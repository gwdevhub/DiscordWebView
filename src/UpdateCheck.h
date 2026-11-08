// Checks GitHub for a newer release of the program, and installs it. The build number is the commit count, which each release's tag
// carries as r<count>, so a release is newer when its count is higher than this build's.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#ifndef DW_BUILD_NUMBER
#define DW_BUILD_NUMBER 0
#endif

inline constexpr std::uint32_t kBuildNumber = DW_BUILD_NUMBER;

struct UpdateInfo {
    std::uint32_t build = 0;
    // The release's page, which the user opens when there's no downloadable file for this platform.
    std::string url;
    // The release's file for this platform, and the SHA-256 GitHub publishes for it. Empty when the release has no such file.
    std::string download;
    std::string sha256;
};

// The latest release, when it's newer than this build. Missing when it isn't newer. Throws std::runtime_error with the reason when
// GitHub can't be read, or the release doesn't say what it is.
std::optional<UpdateInfo> fetchUpdate();

// The startup check: the latest release when it's newer, and missing otherwise, including when GitHub can't be read.
std::optional<UpdateInfo> latestUpdate();

// Fetches the body at a URL, for GitHub's API. Each platform's host file defines it. Missing when the request fails.
std::optional<std::string> httpGet(const std::string& url);

// The release file for this platform, by its name in the release. Each platform's host file defines it.
const char* releaseAssetName();
// The running program, which an update replaces. Each platform's host file defines it.
std::filesystem::path currentExecutable();
// Downloads a URL to dest. Each platform's host file defines it. False when the download fails, and dest is then removed.
bool downloadTo(const std::string& url, const std::filesystem::path& dest);
// Puts the staged file in place of the running program. The old program is kept beside it until the swap has worked. Throws
// std::runtime_error with the reason when it fails. Each platform's host file defines it.
void replaceExecutable(const std::filesystem::path& current, const std::filesystem::path& staged);
// Starts a new copy of the program, detached from this one. Each platform's host file defines it.
void restartProgram(const std::filesystem::path& program);
// Deletes what a previous update left beside the running program. Each platform's host file defines it.
void removeUpdateLeftovers(const std::filesystem::path& current);
