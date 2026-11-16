// Unit tests for where downloads are saved. Exits with 1 when a check fails, and prints each failure.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "Downloads.h"

namespace fs = std::filesystem;

namespace {

int failures = 0;

void expectPath(const fs::path& got, const fs::path& want, const std::string& label) {
    if (got != want) {
        std::printf("FAIL: %s gave %s, want %s\n", label.c_str(), got.string().c_str(), want.string().c_str());
        ++failures;
    }
}

void touch(const fs::path& path) {
    std::ofstream(path) << "existing";
}

}  // namespace

int main() {
    const fs::path folder = fs::temp_directory_path() / "discordwebview-downloads-test";
    fs::remove_all(folder);
    fs::create_directories(folder);

    // A name that isn't there yet is used as it is, and nothing is created.
    expectPath(uniqueDownloadPath(folder, "hello.txt"), folder / "hello.txt", "a free name");
    if (fs::exists(folder / "hello.txt")) {
        std::printf("FAIL: uniqueDownloadPath created the file\n");
        ++failures;
    }

    // A name already there gets a number, and the numbers go on from there.
    touch(folder / "hello.txt");
    expectPath(uniqueDownloadPath(folder, "hello.txt"), folder / "hello (1).txt", "a taken name");
    touch(folder / "hello (1).txt");
    expectPath(uniqueDownloadPath(folder, "hello.txt"), folder / "hello (2).txt", "a second taken name");

    // The number goes before the last extension, so a double extension stays together.
    touch(folder / "archive.tar.gz");
    expectPath(uniqueDownloadPath(folder, "archive.tar.gz"), folder / "archive.tar (1).gz", "a double extension");

    // The page can't choose a folder: only the file name is used.
    expectPath(uniqueDownloadPath(folder, "../../escape.txt"), folder / "escape.txt", "a name with folders");

    // A name with nothing usable is saved as "download".
    expectPath(uniqueDownloadPath(folder, ""), folder / "download", "an empty name");
    expectPath(uniqueDownloadPath(folder, ".."), folder / "download", "a parent folder name");

    // The Downloads folder is the user's, and is named Downloads.
    if (downloadsFolder().filename() != "Downloads") {
        std::printf("FAIL: downloadsFolder() is %s\n", downloadsFolder().string().c_str());
        ++failures;
    }

    fs::remove_all(folder);
    if (failures == 0) {
        std::printf("All download checks passed.\n");
        return 0;
    }
    std::printf("%d download check(s) failed.\n", failures);
    return 1;
}
