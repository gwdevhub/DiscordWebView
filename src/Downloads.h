// Where Discord's downloads are saved: the user's Downloads folder, under the name the page suggests, without replacing a file that's
// already there.
#pragma once

#include <filesystem>
#include <string>

// The user's Downloads folder.
std::filesystem::path downloadsFolder();

// A path in folder for a file called suggestedName that isn't already there: the name itself, or "name (1)", "name (2)" and so on
// when it is. The name is reduced to its file name, so a page can't choose a folder, and an empty name is saved as "download".
// Nothing is created.
std::filesystem::path uniqueDownloadPath(const std::filesystem::path& folder, const std::string& suggestedName);
