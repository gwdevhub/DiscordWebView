#include "UpdateCheck.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <exception>

#include "WebViewWindow.h"

namespace {

constexpr const char* kLatestReleaseUrl = "https://api.github.com/repos/gwdevhub/DiscordWebView/releases/latest";

// The build number a release tag carries, as in r<count>. Missing for any other tag.
std::optional<std::uint32_t> buildOfTag(const std::string& tag) {
    if (tag.size() < 2 || tag[0] != 'r') {
        return std::nullopt;
    }
    const std::string number = tag.substr(1);
    const bool digits = std::all_of(number.begin(), number.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
    if (number.size() > 9 || !digits) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(std::stoul(number));
}

}  // namespace

std::optional<UpdateInfo> latestUpdate() {
    // DISCORD_WEBVIEW_RELEASE_URL points the check at another release feed, for testing.
    const char* override = std::getenv("DISCORD_WEBVIEW_RELEASE_URL");
    const std::string url = override && *override ? override : kLatestReleaseUrl;
    const auto body = httpGet(url);
    if (!body) {
        return std::nullopt;
    }
    try {
        const Json release = Json::parse(*body, nullptr, false);
        if (release.is_discarded() || !release.is_object()) {
            return std::nullopt;
        }
        const auto build = buildOfTag(release.value("tag_name", std::string()));
        const std::string page = release.value("html_url", std::string());
        if (!build || *build <= kBuildNumber || page.rfind("https://", 0) != 0) {
            return std::nullopt;
        }
        return UpdateInfo{*build, page};
    } catch (const std::exception&) {
        return std::nullopt;
    }
}
