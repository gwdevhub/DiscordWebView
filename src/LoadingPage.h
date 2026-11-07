// The page shown while Discord loads. It reports ready at once, so the window appears straight away, then replaces itself with
// the Discord page. The address is passed as a JavaScript string literal, already quoted.
#pragma once

#include <string>

inline std::string loadingPage(const std::string& quotedUrl) {
    return R"HTML(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Discord</title>
<style>
  html, body { height: 100%; margin: 0; background: #313338; color: #dbdee1; font: 14px sans-serif; overflow: hidden;
    user-select: none; cursor: default; }
  #drag { position: fixed; top: 0; left: 0; right: 0; height: 28px; }
  #wrap { height: 100%; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 16px; }
  .spinner { width: 32px; height: 32px; border: 3px solid #4e5058; border-top-color: #5865f2; border-radius: 50%;
    animation: spin 0.9s linear infinite; }
  @keyframes spin { to { transform: rotate(360deg); } }
</style>
</head>
<body>
<div id="drag"></div>
<div id="wrap"><div class="spinner"></div><div>Loading Discord&hellip;</div></div>
<script>
(() => {
  const bridge = () => (window.chrome && window.chrome.webview) || window.webkit.messageHandlers.host;
  const post = (method, args) => {
    try {
      bridge().postMessage(JSON.stringify({id: Date.now(), method, args}));
    } catch (e) {}
  };
  document.getElementById('drag').addEventListener('mousedown', e => {
    if (e.button === 0) post('moveDrag', []);
  });
  // The host shows the window once a page reports ready, so the loading screen appears at once.
  post('ready', []);
  // Then the Discord page takes this one's place. A failure to load it is reported by the host.
  location.replace()HTML" + quotedUrl + R"HTML();
})();
</script>
</body>
</html>
)HTML";
}
