#include "ProgressWindow.h"

#include "Application.h"
#include "Pages.h"

ProgressWindow::ProgressWindow(Application& app, Json start)
    : WebViewWindow(WindowOptions{
          .title = start.value("title", std::string("Working")),
          .width = 420,
          .height = 200,
          .resizable = false,
      }),
      app_(app),
      start_(std::move(start)) {
    start_["phase"] = "start";
    auto& methods = handler();
    methods.on("moveDrag", [](WebViewWindow& window, const Json&, const Reply& reply) {
        window.beginMove();
        reply.result(nullptr);
    });
    methods.on("cancel", [](WebViewWindow& window, const Json&, const Reply& reply) {
        static_cast<ProgressWindow&>(window).app_.cancelProgress();
        reply.result(nullptr);
    });
    methods.on("close", [](WebViewWindow& window, const Json&, const Reply& reply) {
        static_cast<ProgressWindow&>(window).app_.dismissProgress();
        reply.result(nullptr);
    });
}

void ProgressWindow::update(const std::string& stage, std::optional<double> fraction) {
    send(Json{
        {"phase", "update"},
        {"fraction", fraction ? Json(*fraction) : Json(nullptr)},
        {"detail", stage},
    });
}

void ProgressWindow::failed(const std::string& text) {
    send(Json{{"phase", "failed"}, {"text", text}});
}

void ProgressWindow::finished(const std::string& text) {
    send(Json{{"phase", "finished"}, {"text", text}});
}

void ProgressWindow::send(const Json& event) {
    latest_ = event;
    emit("progress", event);
}

std::string ProgressWindow::html() const {
    return pages::progress();
}

std::string ProgressWindow::title() const {
    return start_.value("title", std::string("Working"));
}

void ProgressWindow::onPageReady() {
    emit("progress", start_);
    if (!latest_.is_null()) {
        emit("progress", latest_);
    }
}

bool ProgressWindow::allowClose() {
    app_.dismissProgress();
    return false;
}
