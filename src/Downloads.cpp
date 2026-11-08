#include "Downloads.h"

#include <cstdlib>

#ifdef _WIN32
#include <stdlib.h>
#define DW_TEXT(text) L##text
#else
#define DW_TEXT(text) text
#endif

std::filesystem::path downloadsFolder() {
#ifdef _WIN32
    const wchar_t* home = _wgetenv(L"USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    if (!home || !*home) {
        return std::filesystem::path("Downloads");
    }
    return std::filesystem::path(home) / "Downloads";
}

std::filesystem::path uniqueDownloadPath(const std::filesystem::path& folder, const std::string& suggestedName) {
    std::filesystem::path name = std::filesystem::path(suggestedName).filename();
    if (name.empty() || name == "." || name == "..") {
        name = "download";
    }
    std::filesystem::path candidate = folder / name;
    // The numbered names keep the original's stem and extension: "archive.tar" becomes "archive.tar (1).gz" for "archive.tar.gz".
    const std::filesystem::path::string_type stem = name.stem().native();
    const std::filesystem::path::string_type extension = name.extension().native();
    for (int number = 1; std::filesystem::exists(candidate); ++number) {
        std::filesystem::path::string_type numbered = stem;
        numbered += DW_TEXT(" (");
        const std::string digits = std::to_string(number);
        numbered += std::filesystem::path::string_type(digits.begin(), digits.end());
        numbered += DW_TEXT(")");
        numbered += extension;
        candidate = folder / numbered;
    }
    return candidate;
}
