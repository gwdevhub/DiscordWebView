// The work a progress window runs: a job on its own thread that reports its stage and how far it has got, and stops when cancelled.

#pragma once

#include <atomic>
#include <functional>
#include <optional>
#include <string>

// The message work throws when the user cancels it, so the progress window ends quietly rather than showing a failure.
inline constexpr const char* kCancelledMessage = "cancelled";

// Reports a stage and how far through the work it is, from 0 to 1. Without a fraction the window shows an animation rather than a bar.
using ProgressReport = std::function<void(const std::string& stage, std::optional<double> fraction)>;

// Work on a job thread. It throws std::runtime_error when it fails, or with kCancelledMessage when the user cancels it.
using ProgressWork = std::function<void(const ProgressReport& report, const std::atomic<bool>& cancel)>;
