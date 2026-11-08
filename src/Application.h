// The program's windows and what they share: the Discord window, the settings window, the progress window, the tray icon, the saved
// settings, and the update. The windows call back into it, and it pushes state to them.
#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "Progress.h"
#include "UpdateCheck.h"
#include "WebViewWindow.h"

class DiscordWindow;
class SettingsWindow;
class ProgressWindow;

// Where the update check and the update are.
enum class UpdateStage { Idle, Checking, Current, Available, Failed };

// A job the progress window shows while it runs. Done gets the failure, or nothing when the job finished.
struct ProgressJob {
    std::string title;
    std::string heading;
    std::string note;
    bool cancellable = false;
    // The line shown above a failure's reason.
    std::string failure;
    // Shown when the job finishes, and kept on screen until the user closes the window. Empty closes the window at once.
    std::string finished;
    ProgressWork work;
    std::function<void(const std::optional<std::string>& failure)> done;
};

class Application {
public:
    Application();
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Opens the Discord window, which loads Discord directly while a loading window covers it, puts the window where it was, adds the
    // tray icon, checks for an update in the background, and runs until the Discord window closes.
    void run();

    // Settings. The choices are saved as they change.
    bool keepInTaskbar() const { return keepInTaskbar_; }
    bool trayShown() const { return trayShown_; }
    void setKeepInTaskbar(bool keepInTaskbar);
    void openSettings();

    // Called by the Discord window.
    void discordReady();
    void discordClosed();
    // The script that shows or hides the Discord page's update notice, for the state as it is now.
    std::string discordNoticeScript() const;

    // Called by the settings window and the progress window.
    void checkForUpdate();
    void updateNow();
    void cancelProgress();
    void dismissProgress();
    Json settingsState() const;

private:
    void showTray();
    void showLoading();
    void checkForUpdateOnStart();
    void runWithProgress(ProgressJob job);
    void finishJob(std::uint64_t id, const ProgressJob& job, const std::optional<std::string>& failure);
    void restartAfterUpdate(const std::filesystem::path& program);
    void setStage(UpdateStage stage, const std::string& message);
    void setAvailable(const UpdateInfo& update);
    void pushDiscordNotice();
    void pushSettingsState();
    bool busy() const;

    std::unique_ptr<DiscordWindow> discord_;
    std::unique_ptr<SettingsWindow> settings_;
    std::unique_ptr<ProgressWindow> progress_;
    bool keepInTaskbar_ = true;
    bool trayShown_ = false;
    bool loadingDiscord_ = false;
    std::optional<UpdateInfo> available_;
    UpdateStage stage_ = UpdateStage::Idle;
    std::string message_;
    // Whether a job is running, and the id of the latest job or loading window, so one that was replaced reports to nobody.
    bool jobRunning_ = false;
    std::uint64_t jobId_ = 0;
    std::shared_ptr<std::atomic<bool>> cancel_;
    // Cleared when the application is destroyed, so a background thread that finishes after that does nothing.
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
};
