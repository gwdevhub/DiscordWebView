#include "PagePermissions.h"

#include <algorithm>
#include <cctype>

bool isDiscordAddress(const std::string& url) {
    const std::string scheme = "https://";
    if (url.size() <= scheme.size()) {
        return false;
    }
    std::string lower = url;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower.rfind(scheme, 0) != 0) {
        return false;
    }
    // The host runs to the first '/', '?' or '#', and drops any port.
    std::string host = lower.substr(scheme.size(), lower.find_first_of("/?#", scheme.size()) - scheme.size());
    if (const auto colon = host.find(':'); colon != std::string::npos) {
        host.erase(colon);
    }
    constexpr const char* kDomain = "discord.com";
    if (host == kDomain) {
        return true;
    }
    const std::string suffix = std::string(".") + kDomain;
    return host.size() > suffix.size() && host.compare(host.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::optional<bool> discordPermission(PagePermission permission, const std::string& pageUrl) {
    if (!isDiscordAddress(pageUrl)) {
        return std::nullopt;
    }
    switch (permission) {
        case PagePermission::Microphone:
        case PagePermission::Camera:
        case PagePermission::Notifications:
            return true;
        case PagePermission::Other:
            return std::nullopt;
    }
    return std::nullopt;
}
