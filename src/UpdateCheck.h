// Checks GitHub for a newer release of the program. The build number is the commit count, which each release's tag carries as
// r<count>, so a release is newer when its count is higher than this build's.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#ifndef DW_BUILD_NUMBER
#define DW_BUILD_NUMBER 0
#endif

inline constexpr std::uint32_t kBuildNumber = DW_BUILD_NUMBER;

struct UpdateInfo {
    std::uint32_t build = 0;
    // The release's page, which the user opens to download it.
    std::string url;
};

// The latest release, when it's newer than this build. Missing when it isn't newer, or when GitHub can't be read.
std::optional<UpdateInfo> latestUpdate();

// Fetches the body at a URL, for GitHub's API. Each platform's host file defines it. Missing when the request fails.
std::optional<std::string> httpGet(const std::string& url);
