// The desktop functions each platform's host file defines, for the program's own use.

#pragma once

#include <filesystem>
#include <string>

// Opens a web address in the default browser. The address is never run through a shell.
void launchUrl(const std::string& url);

// The folder the program keeps its settings in.
std::filesystem::path settingsFolder();
