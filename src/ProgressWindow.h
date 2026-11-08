// The progress window: a job's heading, its stage and how far it has got, with a cancel button when the job can be cancelled. The
// loading window is the same window with no job behind it.
#pragma once

#include <optional>
#include <string>

#include "WebViewWindow.h"

class Application;

class ProgressWindow : public WebViewWindow {
public:
    // start holds the title, heading, note and whether the job can be cancelled.
    ProgressWindow(Application& app, Json start);

    void update(const std::string& stage, std::optional<double> fraction);
    void failed(const std::string& text);
    void finished(const std::string& text);

    std::string html() const override;
    std::string title() const override;

protected:
    // A page that's loaded again is brought up to date with the job's start and its latest event.
    void onPageReady() override;
    // Closing the window dismisses it: a running job is cancelled, and the window goes once the job has stopped.
    bool allowClose() override;

private:
    void send(const Json& event);

    Application& app_;
    Json start_;
    Json latest_;
};
