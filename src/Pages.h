// The pages of the program's own windows: settings, progress and loading. Each is a frameless window whose page draws its title bar,
// and each talks to the host through the same bridge the Discord page uses.
#pragma once

#include <string>

namespace pages {

inline constexpr const char* kStyle = R"CSS(
  html, body { margin: 0; height: 100%; overflow: hidden; background: #313338; color: #dbdee1; font: 13px sans-serif; user-select: none; }
  #bar { position: fixed; top: 0; left: 0; right: 0; height: 28px; display: flex; align-items: stretch; background: #1e1f22; }
  #bar .title { flex: 1; min-width: 0; display: flex; align-items: center; padding: 0 10px; font-size: 12px;
    overflow: hidden; white-space: nowrap; text-overflow: ellipsis; cursor: default; }
  #bar button { width: 44px; border: 0; margin: 0; padding: 0; background: transparent; color: #dbdee1; cursor: default; }
  #bar button:hover { background: #da373c; color: #fff; }
  #bar svg { width: 10px; height: 10px; stroke: currentColor; fill: none; stroke-width: 1; }
  #content { position: absolute; top: 28px; left: 0; right: 0; bottom: 0; padding: 16px 20px; box-sizing: border-box; overflow: auto; }
  h2 { margin: 0 0 10px; font-size: 18px; color: #f2f3f5; }
  h3 { margin: 0 0 8px; font-size: 14px; color: #f2f3f5; }
  .section { margin-top: 14px; padding-top: 12px; border-top: 1px solid #3f4147; }
  .option { display: flex; align-items: center; gap: 10px; cursor: pointer; }
  .option input:disabled + span { color: #80848e; }
  p { margin: 8px 0 0; font-size: 13px; line-height: 1.4; color: #b5bac1; }
  .buttons { display: flex; gap: 8px; justify-content: flex-end; flex-wrap: wrap; margin-top: 14px; }
  button.action { padding: 8px 16px; border: 0; border-radius: 3px; background: #4e5058; color: #fff; cursor: pointer; }
  button.action.primary { background: #5865f2; }
  button.action:disabled { opacity: 0.5; cursor: default; }
  .track { height: 6px; margin-top: 14px; border-radius: 3px; background: #1e1f22; overflow: hidden; position: relative; }
  .fill { position: absolute; top: 0; bottom: 0; left: 0; width: 0; background: #5865f2; transition: width 0.2s; }
  .fill.indeterminate { width: 35%; animation: slide 1.2s ease-in-out infinite; }
  @keyframes slide { 0% { left: -35%; } 100% { left: 100%; } }
  .failure { color: #f23f43; }
  .finished { color: #23a559; }
)CSS";

// The bridge and the title bar's close button, shared by every page. The host is called through post; the page draws no frame.
inline constexpr const char* kChromeScript = R"JS(
  const post = (method, args) => {
    try {
      const channel = (window.chrome && window.chrome.webview) || window.webkit.messageHandlers.host;
      channel.postMessage(JSON.stringify({id: Date.now(), method, args}));
    } catch (e) {}
  };
  document.getElementById('title-drag').addEventListener('mousedown', e => {
    if (e.button === 0) post('moveDrag', []);
  });
  document.getElementById('close').addEventListener('click', () => post('close', []));
)JS";

inline constexpr const char* kCloseButton =
    "<button id=\"close\" type=\"button\" aria-label=\"Close\" title=\"Close\">"
    "<svg viewBox=\"0 0 10 10\"><path d=\"M1.5 1.5l7 7M8.5 1.5l-7 7\"/></svg></button>";

// Wraps a page's body and script in a document with the shared style.
inline std::string document(const std::string& title, const std::string& body, const std::string& script) {
    return "<!doctype html><html><head><meta charset=\"utf-8\"><title>" + title + "</title><style>" + kStyle + "</style></head><body>" +
           body + "<script>" + kChromeScript + script + "</script></body></html>";
}

inline std::string settings() {
    const std::string body =
        "<div id=\"bar\"><div class=\"title\" id=\"title-drag\">Settings</div>" + std::string(kCloseButton) + "</div>"
        "<div id=\"content\">"
        "<h2>Settings</h2>"
        "<div class=\"section\"><h3>Window</h3>"
        "<label class=\"option\"><input type=\"checkbox\" id=\"taskbar\"><span>Keep in the taskbar when minimised</span></label>"
        "<p id=\"taskbar-note\"></p></div>"
        "<div class=\"section\"><h3>Updates</h3>"
        "<p id=\"update-status\"></p>"
        "<div class=\"buttons\">"
        "<button class=\"action\" id=\"check\" type=\"button\">Check for update</button>"
        "<button class=\"action primary\" id=\"update-now\" type=\"button\" hidden>Update now</button>"
        "</div></div></div>";
    const std::string script = R"JS(
  const taskbar = document.getElementById('taskbar');
  const taskbarNote = document.getElementById('taskbar-note');
  const status = document.getElementById('update-status');
  const check = document.getElementById('check');
  const updateNow = document.getElementById('update-now');
  taskbar.addEventListener('change', () => post('setTaskbarWhenMinimised', [taskbar.checked]));
  check.addEventListener('click', () => post('checkForUpdate', []));
  updateNow.addEventListener('click', () => post('updateNow', []));
  window.addEventListener('keydown', e => { if (e.key === 'Escape') post('close', []); });
  // The host sends the saved choice, whether a tray icon is available here, and the update state.
  window.hostReceive = ({event, data}) => {
    if (event !== 'state') return;
    taskbar.checked = !!data.keepInTaskbar;
    taskbar.disabled = !data.trayAvailable;
    taskbarNote.textContent = data.trayAvailable
      ? 'When this is unticked, minimising hides the window to the system tray. Click its icon to bring the window back.'
      : 'The system tray isn\'t available on this platform yet, so minimising always uses the taskbar.';
    const update = data.update || {};
    status.textContent = update.message || '';
    const busy = !!update.busy;
    check.disabled = busy;
    updateNow.hidden = !update.available;
    updateNow.disabled = busy;
  };
  post('ready', []);
)JS";
    return document("Settings", body, script);
}

// A job's window: its heading and note, a progress bar (or an animation when there's no known end), the latest stage, and the
// outcome. Cancel is offered while the job runs when it can be cancelled.
inline std::string progress() {
    const std::string body =
        "<div id=\"bar\"><div class=\"title\" id=\"title-drag\">Working</div>" + std::string(kCloseButton) + "</div>"
        "<div id=\"content\">"
        "<h2 id=\"heading\"></h2>"
        "<p id=\"note\"></p>"
        "<div class=\"track\"><div class=\"fill indeterminate\" id=\"fill\"></div></div>"
        "<p id=\"detail\"></p>"
        "<p id=\"failure\" class=\"failure\"></p>"
        "<p id=\"finished\" class=\"finished\"></p>"
        "<div class=\"buttons\">"
        "<button class=\"action\" id=\"cancel\" type=\"button\" hidden>Cancel</button>"
        "<button class=\"action primary\" id=\"done\" type=\"button\" hidden>Done</button>"
        "</div></div>";
    const std::string script = R"JS(
  const title = document.querySelector('#title-drag');
  const heading = document.getElementById('heading');
  const note = document.getElementById('note');
  const fill = document.getElementById('fill');
  const detail = document.getElementById('detail');
  const failure = document.getElementById('failure');
  const finished = document.getElementById('finished');
  const cancel = document.getElementById('cancel');
  const done = document.getElementById('done');
  cancel.addEventListener('click', () => {
    cancel.disabled = true;
    cancel.textContent = 'Cancelling...';
    post('cancel', []);
  });
  done.addEventListener('click', () => post('close', []));
  window.hostReceive = ({event, data}) => {
    if (event !== 'progress') return;
    if (data.phase === 'start') {
      title.textContent = data.title || 'Working';
      heading.textContent = data.heading || '';
      note.textContent = data.note || '';
      cancel.hidden = !data.cancellable;
    } else if (data.phase === 'update') {
      detail.textContent = data.detail || '';
      if (data.fraction === null || data.fraction === undefined) {
        fill.classList.add('indeterminate');
      } else {
        fill.classList.remove('indeterminate');
        fill.style.left = '0';
        fill.style.width = Math.round(data.fraction * 100) + '%';
      }
    } else if (data.phase === 'failed') {
      failure.textContent = data.text || '';
      cancel.hidden = true;
      done.textContent = 'Close';
      done.hidden = false;
    } else if (data.phase === 'finished') {
      finished.textContent = data.text || '';
      fill.classList.remove('indeterminate');
      fill.style.width = '100%';
      cancel.hidden = true;
      done.textContent = 'Done';
      done.hidden = false;
    }
  };
  post('ready', []);
)JS";
    return document("Working", body, script);
}

}  // namespace pages
