// Unit tests for the permission answers Discord's page is given. Exits with 1 when a check fails, and prints each failure.
#include <cstdio>
#include <optional>
#include <string>

#include "PagePermissions.h"

namespace {

int failures = 0;

void expectAnswer(PagePermission permission, const std::string& url, std::optional<bool> want) {
    const std::optional<bool> got = discordPermission(permission, url);
    if (got != want) {
        std::printf("FAIL: permission %d from \"%s\" gave %s, want %s\n", static_cast<int>(permission), url.c_str(),
                    got ? (*got ? "true" : "false") : "nothing", want ? (*want ? "true" : "false") : "nothing");
        ++failures;
    }
}

void expectDiscord(const std::string& url, bool want) {
    if (isDiscordAddress(url) != want) {
        std::printf("FAIL: isDiscordAddress(\"%s\") is not %s\n", url.c_str(), want ? "true" : "false");
        ++failures;
    }
}

}  // namespace

int main() {
    // The three permissions are allowed on Discord's own page.
    expectAnswer(PagePermission::Microphone, "https://discord.com/app", true);
    expectAnswer(PagePermission::Camera, "https://discord.com/app", true);
    expectAnswer(PagePermission::Notifications, "https://discord.com/app", true);

    // Subdomains, the bare address, upper case and an explicit port are Discord's too.
    expectAnswer(PagePermission::Microphone, "https://canary.discord.com/app", true);
    expectAnswer(PagePermission::Microphone, "https://discord.com", true);
    expectAnswer(PagePermission::Notifications, "https://DISCORD.com/app", true);
    expectAnswer(PagePermission::Camera, "https://discord.com:443/app", true);

    // Other permission kinds are left to the platform, even on Discord.
    expectAnswer(PagePermission::Other, "https://discord.com/app", std::nullopt);

    // Anything else gets no answer: other sites, look-alike hosts, plain http, and malformed addresses.
    expectAnswer(PagePermission::Microphone, "https://evil.com/", std::nullopt);
    expectAnswer(PagePermission::Microphone, "https://notdiscord.com/", std::nullopt);
    expectAnswer(PagePermission::Microphone, "https://xdiscord.com/", std::nullopt);
    expectAnswer(PagePermission::Microphone, "https://discord.com.evil.net/", std::nullopt);
    expectAnswer(PagePermission::Microphone, "https://evil.com/discord.com", std::nullopt);
    expectAnswer(PagePermission::Microphone, "https://evil.com/?discord.com", std::nullopt);
    expectAnswer(PagePermission::Microphone, "http://discord.com/app", std::nullopt);
    expectAnswer(PagePermission::Microphone, "", std::nullopt);
    expectAnswer(PagePermission::Microphone, "https://", std::nullopt);

    expectDiscord("https://discord.com/app", true);
    expectDiscord("https://ptb.discord.com/app", true);
    expectDiscord("http://discord.com/app", false);
    expectDiscord("https://discord.com.evil.net/", false);

    if (failures == 0) {
        std::printf("All permission checks passed.\n");
        return 0;
    }
    std::printf("%d permission check(s) failed.\n", failures);
    return 1;
}
