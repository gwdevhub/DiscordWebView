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
    #gw-titlebar { position: fixed; top: 0; left: 0; right: 0; height: ${BAR}px; z-index: 2147483647; display: flex;
      align-items: stretch; background: #1e1f22; color: #dbdee1; font: 12px sans-serif; user-select: none; cursor: default; }
    #gw-titlebar .gw-title { flex: 1; min-width: 0; display: flex; align-items: center; padding: 0 10px; white-space: nowrap;
      overflow: hidden; text-overflow: ellipsis; }
    #gw-titlebar button { width: 44px; border: 0; margin: 0; padding: 0; background: transparent; color: #dbdee1; cursor: default; }
    #gw-titlebar button:hover { background: #35373c; }
    #gw-titlebar button.gw-close:hover { background: #da373c; color: #fff; }
    #gw-titlebar svg { width: 10px; height: 10px; stroke: currentColor; fill: none; stroke-width: 1; }
    .gw-edge { position: fixed; z-index: 2147483646; }
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
  button('Minimise', 'M1 5.5h8', 'minimize');
  button('Maximise', 'M1.5 1.5h7v7h-7z', 'toggleMaximize');
  button('Close', 'M1.5 1.5l7 7M8.5 1.5l-7 7', 'close', 'gw-close');
  document.body.appendChild(bar);

  // Adds a button to the bar that opens a newer release's page. The host calls it when a newer release exists, and again on each
  // page load, since the bar is rebuilt with the page.
  window.gwShowUpdate = href => {
    if (document.getElementById('gw-update')) return;
    const b = document.createElement('button');
    b.id = 'gw-update';
    b.type = 'button';
    b.textContent = 'Update available';
    b.title = 'Open the download page for the newer version';
    b.style.cssText = 'width: auto; padding: 0 10px; color: #f0b232; font: 12px sans-serif;';
    b.addEventListener('click', () => post('openExternal', [href]));
    bar.insertBefore(b, bar.querySelector('button'));
  };

  // Resize zones, numbered as the host's ResizeEdge: NW, N, NE, W, E, SW, S, SE. The top edge is the bar's, so only its corners.
  const zones = [
    [0, 'left:0;top:0;width:6px;height:6px;cursor:nwse-resize'],
    [2, 'right:0;top:0;width:6px;height:6px;cursor:nesw-resize'],
    [3, 'left:0;top:6px;bottom:6px;width:6px;cursor:ew-resize'],
    [4, 'right:0;top:6px;bottom:6px;width:6px;cursor:ew-resize'],
    [5, 'left:0;bottom:0;width:6px;height:6px;cursor:nesw-resize'],
    [6, 'left:6px;right:6px;bottom:0;height:6px;cursor:ns-resize'],
    [7, 'right:0;bottom:0;width:6px;height:6px;cursor:nwse-resize'],
  ];
  for (const [edge, place] of zones) {
    const z = document.createElement('div');
    z.className = 'gw-edge';
    z.style.cssText = place;
    z.addEventListener('mousedown', e => {
      if (e.button === 0) post('resizeDrag', [edge]);
    });
    document.body.appendChild(z);
  }

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
