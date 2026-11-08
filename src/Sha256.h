// SHA-256, for checking a downloaded update against the checksum GitHub publishes for it.
#pragma once

#include <filesystem>
#include <optional>
#include <string>

// The SHA-256 digest of a file, as lowercase hex. Missing when the file can't be read.
std::optional<std::string> sha256OfFile(const std::filesystem::path& file);
