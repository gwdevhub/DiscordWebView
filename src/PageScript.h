// Injected into Discord's page: a title bar and resize edges, the way GW Launcher draws its own frame, and routing of links that
// leave Discord to the default browser. It runs again on each tick and does nothing once it's in place.
#pragma once

inline constexpr const char* kPageScript = R"JS(
(() => {
  const install = () => {
  // The window starts on about:blank, which must not count as the page being ready.
  if (!/^https?:$/.test(location.protocol) || window.__gwDiscordFrame) return;
  window.__gwDiscordFrame = true;

  const BAR = 28;
  // WebView2 (Windows) and WebKitGTK (Linux) expose the host under different names.
  const bridge = () => (window.chrome && window.chrome.webview) || window.webkit.messageHandlers.host;
  const post = (method, args) => {
    try {
      bridge().postMessage(JSON.stringify({id: Date.now(), method, args}));
    } catch (e) {}
  };
  window.hostReceive = window.hostReceive || (() => {});

  const style = document.createElement('style');
  style.textContent = `
    html { height: 100%; }
    body { margin-top: ${BAR}px !important; height: calc(100vh - ${BAR}px) !important; overflow: hidden !important; }
    #app-mount { height: calc(100vh - ${BAR}px) !important; }
    /* One below the library's resize strips (2147483647), so they stay on top of the bar's top edge. */
    #gw-titlebar { position: fixed; top: 0; left: 0; right: 0; height: ${BAR}px; z-index: 2147483646; display: flex;
      align-items: stretch; background: #1e1f22; color: #dbdee1; font: 12px sans-serif; user-select: none; cursor: default; }
    #gw-titlebar .gw-title { flex: 1; min-width: 0; display: flex; align-items: center; padding: 0 10px; white-space: nowrap;
      overflow: hidden; text-overflow: ellipsis; }
    #gw-titlebar button { width: 44px; border: 0; margin: 0; padding: 0; background: transparent; color: #dbdee1; cursor: default; }
    #gw-titlebar button:hover { background: #35373c; }
    #gw-titlebar button.gw-close:hover { background: #da373c; color: #fff; }
    #gw-titlebar svg { width: 10px; height: 10px; stroke: currentColor; fill: none; stroke-width: 1; }
    #gw-overlay { position: fixed; top: 0; right: 0; bottom: 0; left: 0; z-index: 2147483646; display: flex; align-items: center;
      justify-content: center; background: rgba(0, 0, 0, 0.6); font: 14px sans-serif; color: #dbdee1; }
    #gw-overlay.gw-hidden { display: none; }
    #gw-overlay .gw-dialog { width: 380px; max-width: calc(100vw - 32px); padding: 20px 24px; border-radius: 6px; background: #313338;
      box-shadow: 0 8px 24px rgba(0, 0, 0, 0.5); }
    #gw-overlay h2 { margin: 0 0 14px; font-size: 18px; color: #f2f3f5; }
    #gw-overlay .gw-option { display: flex; align-items: center; gap: 10px; cursor: pointer; }
    #gw-overlay .gw-option input:disabled { cursor: default; }
    #gw-overlay .gw-option:has(input:disabled) { color: #80848e; cursor: default; }
    #gw-overlay p { margin: 10px 0 0; font-size: 13px; line-height: 1.4; color: #b5bac1; }
    #gw-overlay .gw-actions { display: flex; justify-content: flex-end; margin-top: 18px; }
    #gw-overlay .gw-actions button { padding: 8px 18px; border: 0; border-radius: 3px; background: #5865f2; color: #fff; cursor: pointer; }
    #gw-overlay .gw-section { margin-top: 16px; padding-top: 12px; border-top: 1px solid #3f4147; }
    #gw-overlay h3 { margin: 0 0 8px; font-size: 14px; color: #f2f3f5; }
    #gw-overlay .gw-buttons { display: flex; gap: 8px; flex-wrap: wrap; margin-top: 10px; }
    #gw-overlay .gw-buttons button { padding: 8px 14px; border: 0; border-radius: 3px; background: #4e5058; color: #fff; cursor: pointer; }
    #gw-overlay .gw-buttons #gw-update-now { background: #5865f2; }
    #gw-overlay button:disabled { opacity: 0.5; cursor: default; }
  `;
  document.documentElement.appendChild(style);

  const bar = document.createElement('div');
  bar.id = 'gw-titlebar';
  const title = document.createElement('div');
  title.className = 'gw-title';
  title.textContent = document.title || 'Discord';
  title.addEventListener('mousedown', e => {
    if (e.button !== 0) return;
    post(e.detail === 2 ? 'toggleMaximize' : 'moveDrag', []);
  });
  bar.appendChild(title);

  const button = (label, path, method, cls) => {
    const b = document.createElement('button');
    b.type = 'button';
    b.setAttribute('aria-label', label);
    b.title = label;
    if (cls) b.className = cls;
    b.innerHTML = `<svg viewBox="0 0 10 10"><path d="${path}"/></svg>`;
    b.addEventListener('click', () => post(method, []));
    bar.appendChild(b);
  };
  // Opens the window behaviour dialog, which is built below.
  const behaviour = document.createElement('button');
  behaviour.type = 'button';
  behaviour.id = 'gw-behaviour';
  behaviour.setAttribute('aria-label', 'Settings');
  behaviour.title = 'Settings';
  behaviour.innerHTML = '<svg viewBox="0 0 10 10"><path d="M1.5 1.5h7v5h-7z M1 8.5h8"/></svg>';
  bar.appendChild(behaviour);
  button('Minimise', 'M1 5.5h8', 'minimize');
  button('Maximise', 'M1.5 1.5h7v7h-7z', 'toggleMaximize');
  button('Close', 'M1.5 1.5l7 7M8.5 1.5l-7 7', 'close', 'gw-close');
  document.body.appendChild(bar);

  // The settings dialog. Window: ticked, minimising keeps the window in the taskbar; unticked, it hides the window to the system tray.
  // Updates: the host reports the state, and asks for a check or an update through the buttons.
  const overlay = document.createElement('div');
  overlay.id = 'gw-overlay';
  overlay.className = 'gw-hidden';
  overlay.innerHTML = '<div class="gw-dialog" role="dialog" aria-modal="true" aria-labelledby="gw-settings-title">' +
    '<h2 id="gw-settings-title">Settings</h2>' +
    '<div class="gw-section">' +
      '<h3>Window</h3>' +
      '<label class="gw-option"><input type="checkbox" id="gw-taskbar"> Keep in the taskbar when minimised</label>' +
      '<p id="gw-behaviour-note"></p>' +
    '</div>' +
    '<div class="gw-section">' +
      '<h3>Updates</h3>' +
      '<p id="gw-update-status"></p>' +
      '<div class="gw-buttons">' +
        '<button type="button" id="gw-check-update">Check for update</button>' +
        '<button type="button" id="gw-update-now" hidden>Update now</button>' +
      '</div>' +
    '</div>' +
    '<div class="gw-actions"><button type="button" id="gw-settings-done">Done</button></div>' +
    '</div>';
  document.body.appendChild(overlay);
  const taskbar = overlay.querySelector('#gw-taskbar');
  const note = overlay.querySelector('#gw-behaviour-note');
  const status = overlay.querySelector('#gw-update-status');
  const checkButton = overlay.querySelector('#gw-check-update');
  const updateNowButton = overlay.querySelector('#gw-update-now');
  const openSettings = () => { overlay.classList.remove('gw-hidden'); taskbar.focus(); };
  const closeSettings = () => overlay.classList.add('gw-hidden');
  behaviour.addEventListener('click', openSettings);
  overlay.querySelector('#gw-settings-done').addEventListener('click', closeSettings);
  overlay.addEventListener('click', e => { if (e.target === overlay) closeSettings(); });
  document.addEventListener('keydown', e => {
    if (e.key === 'Escape' && !overlay.classList.contains('gw-hidden')) closeSettings();
  });
  taskbar.addEventListener('change', () => post('setTaskbarWhenMinimised', [taskbar.checked]));
  checkButton.addEventListener('click', () => post('checkForUpdate', []));
  updateNowButton.addEventListener('click', () => post('updateNow', []));
  // The host calls this on each page load with the saved choice, and whether a tray icon is available on this platform.
  window.gwSetBehaviour = (keepInTaskbar, trayAvailable) => {
    taskbar.checked = !!keepInTaskbar;
    taskbar.disabled = !trayAvailable;
    note.textContent = trayAvailable
      ? 'When this is unticked, minimising hides the window to the system tray. Click its icon to bring the window back.'
      : 'The system tray isn\'t available on this platform yet, so minimising always uses the taskbar.';
  };
  // The host calls this with the update state: the stage, a message, and whether a newer release is known. The bar's notice shows
  // while one is known, and opens the settings dialog.
  window.gwSetUpdateState = state => {
    const s = state || {};
    status.textContent = s.message || '';
    const busy = s.stage === 'checking' || s.stage === 'downloading';
    checkButton.disabled = busy;
    updateNowButton.hidden = !s.available;
    updateNowButton.disabled = busy;
    const notice = document.getElementById('gw-update');
    if (s.available && !notice) {
      const b = document.createElement('button');
      b.id = 'gw-update';
      b.type = 'button';
      b.textContent = 'Update available';
      b.title = 'Open settings to update';
      b.style.cssText = 'width: auto; padding: 0 10px; color: #f0b232; font: 12px sans-serif;';
      b.addEventListener('click', openSettings);
      bar.insertBefore(b, bar.querySelector('button'));
    } else if (!s.available && notice) {
      notice.remove();
    }
  };

  // Keeps the bar's title in step with the page's.
  new MutationObserver(() => { title.textContent = document.title || 'Discord'; })
    .observe(document.head || document.documentElement, {subtree: true, childList: true, characterData: true});

  // Only links that ask for a new window (target="_blank") are handled here. Web addresses on other sites open in the default browser.
  // Addresses on Discord's own site load in this window, even when the page asks for a new one: this window is the app, so a new
  // window would be a second copy of it. Every other link is left to the app.
  const webUrl = href => {
    try {
      const url = new URL(href, location.href);
      if (url.protocol === 'http:' || url.protocol === 'https:') return url;
    } catch (e) {}
    return null;
  };
  document.addEventListener('click', e => {
    if (e.button !== 0) return;
    const anchor = e.target && e.target.closest ? e.target.closest('a[href]') : null;
    if (!anchor || anchor.target !== '_blank') return;
    const url = webUrl(anchor.href);
    if (!url) return;
    if (url.hostname !== location.hostname) {
      e.preventDefault();
      e.stopPropagation();
      post('openExternal', [url.href]);
    } else {
      // Same-site links stay in this window. The click isn't stopped, so Discord's own router still handles it and changes the
      // channel without a page load.
      anchor.removeAttribute('target');
    }
  }, true);
  window.open = (href, name, features) => {
    const url = webUrl(href === undefined ? '' : String(href));
    if (url && url.hostname === location.hostname) {
      location.assign(url.href);
    } else if (url) {
      post('openExternal', [url.href]);
    }
    return null;
  };

  // F12 opens the developer tools.
  document.addEventListener('keydown', e => {
    if (e.key === 'F12') {
      e.preventDefault();
      post('openDevTools', []);
    }
  }, true);

  // Tells the host the frame is in place: the window's onPageReady runs for it.
  post('ready', []);
  };
  // The page is injected as its navigation completes, and that can come before the document has a body to build the bar in.
  if (document.body) install();
  else document.addEventListener('DOMContentLoaded', install, {once: true});
})();
)JS";
