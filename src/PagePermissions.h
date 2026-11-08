// The page permissions the host gives Discord's web client. Only Discord's own address is answered: the microphone, camera and
// notifications are allowed there, and every other request keeps the platform's default.
#pragma once

#include <optional>
#include <string>

#include "WebViewWindow.h"

// The answer for a permission request from a page at pageUrl: true to allow, or nothing to leave the platform's default in place.
std::optional<bool> discordPermission(PagePermission permission, const std::string& pageUrl);

// Whether the address is an https address on discord.com or one of its subdomains.
bool isDiscordAddress(const std::string& url);
