#include "UpdateCheck.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <stdexcept>

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

std::optional<UpdateInfo> fetchUpdate() {
    // DISCORD_WEBVIEW_RELEASE_URL points the check at another release feed, for testing.
    const char* override = std::getenv("DISCORD_WEBVIEW_RELEASE_URL");
    const std::string url = override && *override ? override : kLatestReleaseUrl;
    const auto body = httpGet(url);
    if (!body) {
        throw std::runtime_error("GitHub couldn't be reached.");
    }
    const Json release = Json::parse(*body, nullptr, false);
    if (release.is_discarded() || !release.is_object()) {
        throw std::runtime_error("GitHub returned no release.");
    }
    const auto build = buildOfTag(release.value("tag_name", std::string()));
    if (!build) {
        throw std::runtime_error("The latest release has no build number.");
    }
    if (*build <= kBuildNumber) {
        return std::nullopt;
    }
    UpdateInfo info;
    info.build = *build;
    info.url = release.value("html_url", std::string());
    if (info.url.rfind("https://", 0) != 0) {
        throw std::runtime_error("The latest release has no page.");
    }
    if (release.contains("assets") && release["assets"].is_array()) {
        for (const Json& asset : release["assets"]) {
            if (asset.value("name", std::string()) != releaseAssetName()) {
                continue;
            }
            const std::string digest = asset.value("digest", std::string());
            const std::string prefix = "sha256:";
            if (digest.compare(0, prefix.size(), prefix) != 0) {
                throw std::runtime_error("GitHub gave no checksum for the update.");
            }
            info.download = asset.value("browser_download_url", std::string());
            info.sha256 = digest.substr(prefix.size());
            if (info.download.empty()) {
                throw std::runtime_error("GitHub gave no download for the update.");
            }
            break;
        }
    }
    return info;
}

std::optional<UpdateInfo> latestUpdate() {
    try {
        return fetchUpdate();
    } catch (const std::exception&) {
        // The startup check is quiet: the settings dialog reports a failure when it's asked for.
        return std::nullopt;
    }
}
