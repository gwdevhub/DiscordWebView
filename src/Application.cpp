#include "Application.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>

#include "DiscordWindow.h"
#include "Host.h"
#include "ProgressWindow.h"
#include "SettingsWindow.h"
#include "Sha256.h"

namespace {

std::filesystem::path behaviourFile() {
    return settingsFolder() / "behaviour.json";
}

// Whether minimising keeps the window in the taskbar. The default is to keep it there.
bool loadKeepInTaskbar() {
    try {
        std::ifstream in(behaviourFile());
        if (!in) {
            return true;
        }
        return Json::parse(in).at("keepInTaskbar").get<bool>();
    } catch (const std::exception&) {
        return true;
    }
}

void saveKeepInTaskbar(bool keepInTaskbar) {
    try {
        std::filesystem::create_directories(behaviourFile().parent_path());
        std::ofstream out(behaviourFile(), std::ios::trunc);
        out << Json{{"keepInTaskbar", keepInTaskbar}}.dump(2) << '\n';
    } catch (const std::exception&) {
        // The choice still applies until the program closes, so a failed save isn't worth stopping for.
    }
}

std::string lowercase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

const char* stageName(UpdateStage stage) {
    switch (stage) {
        case UpdateStage::Checking: return "checking";
        case UpdateStage::Current: return "current";
        case UpdateStage::Available: return "available";
        case UpdateStage::Failed: return "failed";
        case UpdateStage::Idle: break;
    }
    return "idle";
}

}  // namespace

Application::Application() : keepInTaskbar_(loadKeepInTaskbar()) {}

Application::~Application() {
    *alive_ = false;
    progress_.reset();
    settings_.reset();
    discord_.reset();
}

void Application::run() {
    discord_ = std::make_unique<DiscordWindow>(*this);
    discord_->open();
    discord_->restoreFrame();
    showTray();
    // Discord loads hidden, and the loading window covers the wait. The Discord window shows itself once its page is ready.
    showLoading();
    checkForUpdateOnStart();
    runEventLoop();
}

void Application::showTray() {
    trayShown_ = showTrayIcon(
        "Discord", [this] { discord_->present(); },
        {{"Show Discord", [this] { discord_->present(); }}, {"Quit", [this] { discord_->close(); }}});
}

void Application::showLoading() {
    ++jobId_;
    loadingDiscord_ = true;
    progress_ = std::make_unique<ProgressWindow>(
        *this, Json{{"title", "Discord"}, {"heading", "Loading Discord"}, {"note", ""}, {"cancellable", false}});
    progress_->open();
    progress_->present();
}

void Application::discordReady() {
    if (loadingDiscord_) {
        loadingDiscord_ = false;
        dismissProgress();
    }
}

void Application::discordClosed() {
    stopEventLoop();
}

std::string Application::discordNoticeScript() const {
    return std::string("window.gwSetUpdateNotice && window.gwSetUpdateNotice(") + (available_ ? "true" : "false") + ");";
}

void Application::setKeepInTaskbar(bool keepInTaskbar) {
    keepInTaskbar_ = keepInTaskbar;
    saveKeepInTaskbar(keepInTaskbar);
    pushSettingsState();
}

void Application::openSettings() {
    if (!settings_) {
        settings_ = std::make_unique<SettingsWindow>(*this);
        settings_->open();
    }
    settings_->present();
}

Json Application::settingsState() const {
    return Json{
        {"keepInTaskbar", keepInTaskbar_},
        {"trayAvailable", trayShown_},
        {"update", Json{
                       {"stage", stageName(stage_)},
                       {"message", message_},
                       {"available", available_.has_value()},
                       {"busy", busy()},
                   }},
    };
}

bool Application::busy() const {
    return stage_ == UpdateStage::Checking || jobRunning_;
}

void Application::setStage(UpdateStage stage, const std::string& message) {
    stage_ = stage;
    message_ = message;
    pushSettingsState();
}

void Application::setAvailable(const UpdateInfo& update) {
    available_ = update;
    setStage(UpdateStage::Available, "Version r" + std::to_string(update.build) + " is available.");
    pushDiscordNotice();
}

void Application::pushDiscordNotice() {
    if (discord_) {
        discord_->runScript(discordNoticeScript());
    }
}

void Application::pushSettingsState() {
    if (settings_) {
        settings_->emit("state", settingsState());
    }
}

void Application::checkForUpdateOnStart() {
    auto alive = alive_;
    std::thread([this, alive] {
        const auto update = latestUpdate();
        if (!update) {
            return;
        }
        runOnUiThread([this, alive, update = *update] {
            if (*alive) {
                setAvailable(update);
            }
        });
    }).detach();
}

void Application::checkForUpdate() {
    if (busy()) {
        return;
    }
    setStage(UpdateStage::Checking, "Checking for an update...");
    auto alive = alive_;
    std::thread([this, alive] {
        std::optional<UpdateInfo> update;
        std::string error;
        try {
            update = fetchUpdate();
        } catch (const std::exception& failure) {
            error = failure.what();
        }
        runOnUiThread([this, alive, update, error] {
            if (!*alive) {
                return;
            }
            if (!error.empty()) {
                setStage(UpdateStage::Failed, error);
            } else if (update) {
                setAvailable(*update);
            } else {
                setStage(UpdateStage::Current, "You have the latest version (r" + std::to_string(kBuildNumber) + ").");
            }
        });
    }).detach();
}

void Application::updateNow() {
    if (!available_ || busy()) {
        return;
    }
    const UpdateInfo info = *available_;
    if (info.download.empty()) {
        launchUrl(info.url);
        return;
    }
    // Taken before the swap: once the new program is in place, the running file's path is the one that was moved aside.
    const std::filesystem::path current = currentExecutable();
    ProgressJob job;
    job.title = "Updating Discord Web View";
    job.heading = "Downloading r" + std::to_string(info.build);
    job.note = "The program restarts once it's updated.";
    job.cancellable = true;
    job.failure = "The update failed";
    job.work = [info, current](const ProgressReport& report, const std::atomic<bool>& cancel) {
        if (current.empty()) {
            throw std::runtime_error("The program's file couldn't be found.");
        }
        const auto staged = current.parent_path() / (".new-" + current.filename().string());
        // A cancelled job removes what it has downloaded, so nothing is left beside the program.
        const auto stopIfCancelled = [&cancel, &staged] {
            if (cancel.load()) {
                std::error_code ignored;
                std::filesystem::remove(staged, ignored);
                throw std::runtime_error(kCancelledMessage);
            }
        };
        report("Downloading the update...", std::nullopt);
        if (!downloadTo(info.download, staged, cancel)) {
            stopIfCancelled();
            throw std::runtime_error("The update couldn't be downloaded.");
        }
        stopIfCancelled();
        report("Checking the download...", 0.9);
        const auto digest = sha256OfFile(staged);
        if (!digest || *digest != lowercase(info.sha256)) {
            std::error_code ignored;
            std::filesystem::remove(staged, ignored);
            throw std::runtime_error("The download didn't match GitHub's checksum.");
        }
        stopIfCancelled();
        report("Installing the update...", std::nullopt);
        replaceExecutable(current, staged);
    };
    job.done = [this, current](const std::optional<std::string>& failure) {
        if (!failure) {
            restartAfterUpdate(current);
            return;
        }
        // A cancelled update leaves the program as it was, with the update still offered.
        if (*failure != kCancelledMessage) {
            setStage(UpdateStage::Failed, *failure);
        }
    };
    runWithProgress(std::move(job));
}

void Application::restartAfterUpdate(const std::filesystem::path& program) {
    restartProgram(program);
    if (discord_) {
        discord_->close();
    }
}

void Application::runWithProgress(ProgressJob job) {
    const std::uint64_t id = ++jobId_;
    loadingDiscord_ = false;
    cancel_ = std::make_shared<std::atomic<bool>>(false);
    progress_ = std::make_unique<ProgressWindow>(
        *this, Json{{"title", job.title}, {"heading", job.heading}, {"note", job.note}, {"cancellable", job.cancellable}});
    progress_->open();
    progress_->present();
    jobRunning_ = true;
    pushSettingsState();

    auto cancel = cancel_;
    auto alive = alive_;
    std::thread([this, id, alive, cancel, job = std::move(job)]() mutable {
        // Reports reach the progress window on the UI thread, and only while it's the window of this job.
        ProgressReport report = [this, id, alive](const std::string& stage, std::optional<double> fraction) {
            runOnUiThread([this, id, alive, stage, fraction] {
                if (*alive && id == jobId_ && progress_) {
                    progress_->update(stage, fraction);
                }
            });
        };
        std::optional<std::string> failure;
        try {
            job.work(report, *cancel);
        } catch (const std::exception& error) {
            failure = error.what();
        }
        runOnUiThread([this, id, alive, job, failure] {
            if (*alive) {
                finishJob(id, job, failure);
            }
        });
    }).detach();
}

void Application::finishJob(std::uint64_t id, const ProgressJob& job, const std::optional<std::string>& failure) {
    if (id != jobId_) {
        return;
    }
    jobRunning_ = false;
    const bool cancelled = failure && *failure == kCancelledMessage;
    if (progress_) {
        if (failure && !cancelled) {
            // A failure stays on screen until the user closes the window.
            progress_->failed(job.failure + ": " + *failure);
        } else if (!failure && !job.finished.empty()) {
            progress_->finished(job.finished);
        } else {
            dismissProgress();
        }
    }
    if (job.done) {
        job.done(failure);
    }
    pushSettingsState();
}

void Application::cancelProgress() {
    if (cancel_) {
        *cancel_ = true;
    }
}

void Application::dismissProgress() {
    // Closing a running job cancels it, and the window goes once the job has stopped.
    if (jobRunning_ && cancel_) {
        *cancel_ = true;
    }
    if (!progress_) {
        return;
    }
    // The window is removed on a later turn of the event loop, since it may be the one calling.
    const std::uint64_t id = jobId_;
    auto alive = alive_;
    runOnUiThread([this, alive, id] {
        if (*alive && id == jobId_) {
            progress_.reset();
        }
    });
}

