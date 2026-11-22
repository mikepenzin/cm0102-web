// Small parts of the page around the game (web/README.md, "The page"): messages, the sign that the game is busy, the
// question before the page is left, and the screen kept awake.

// A short message at the bottom of the game. The same text again only moves its time on.
export function toast(text, { ms = 2600, kind = '' } = {}) {
  const host = document.getElementById('toasts');
  if (!host) return;
  let el = [...host.children].find((c) => c.textContent === text);
  if (!el) {
    el = document.createElement('div');
    el.className = `toast ${kind}`;
    el.textContent = text;
    host.appendChild(el);
    while (host.childElementCount > 3) host.firstElementChild.remove();
  }
  clearTimeout(el.timer);
  el.timer = setTimeout(() => el.remove(), ms);
}

// The busy sign. The game runs on the page's own thread; while it works without asking for input (a database load,
// a long day), the page cannot run any script, so it cannot decide to show a sign then. So the sign is an animation
// that would fade in after BUSY_MS, and a timer sets it back to its start four times a second. While the page's
// thread is free the sign never shows; when the thread is held, the timer stops and the browser's compositor, which
// goes on without the thread, plays the animation to its end.
export const BUSY_MS = 1500;
export function mountBusy(el) {
  if (!el || !el.animate) return;
  const anim = el.animate([{ opacity: 0 }, { opacity: 0, offset: 0.9 }, { opacity: 1 }], { duration: BUSY_MS, fill: 'forwards' });
  setInterval(() => { anim.currentTime = 0; }, 250);
}

// Asks before the page is left while a game runs: what was played since the last save would be lost. It also keeps the
// unload away from SDL (a capture listener made before the engine): some versions of SDL tell the game to quit at this
// point, and the player may choose to stay.
export function guardLeave(running) {
  addEventListener('beforeunload', (e) => {
    e.stopImmediatePropagation();
    if (!running()) return;
    e.preventDefault();
    e.returnValue = '';
  }, true);
}

// Keeps the screen on while the game shows: a holiday of the manager can run for minutes with no touch.
export function keepAwake() {
  if (!navigator.wakeLock) return;
  const ask = () => { if (document.visibilityState === 'visible') navigator.wakeLock.request('screen').catch(() => {}); };
  document.addEventListener('visibilitychange', ask);
  ask();
}

// 1 while the page runs as an installed app (from the home screen or the dock), without the browser's bars.
// Full screen is not offered where the page is full already: an iPhone or iPad home-screen app (navigator.standalone),
// or an app in the display mode fullscreen. A computer's app window (standalone) can still go full screen: it once got
// no button either, and a player's Mac app had none.
// The bar's buttons with their names when they fit in one row, else icons only (class compact), else icons closer
// together (class tight), else without the title (class bare): a phone held sideways had the Full screen button past
// the edge. Again at each resize and each button shown or hidden. Not in full screen,
// where the bar has rules of its own.
export function fitBar(bar, app) {
  let queued = false;
  const fit = () => {
    queued = false;
    bar.classList.remove('compact', 'tight', 'bare');
    if (app.classList.contains('fs')) return;
    const over = () => bar.scrollWidth > bar.clientWidth + 1;
    if (over()) bar.classList.add('compact');
    if (over()) bar.classList.add('tight');
    if (over()) bar.classList.add('bare');
  };
  const soon = () => { if (!queued) { queued = true; requestAnimationFrame(fit); } };
  new ResizeObserver(soon).observe(bar);
  new MutationObserver(soon).observe(bar, { subtree: true, attributes: true, attributeFilter: ['hidden'], childList: true });
  new MutationObserver(soon).observe(app, { attributes: true, attributeFilter: ['class'] });
  fit();
  return fit;
}

export function isFullAlready() {
  return matchMedia('(display-mode: fullscreen)').matches || navigator.standalone === true;
}

export function isStandalone() {
  return matchMedia('(display-mode: standalone)').matches || matchMedia('(display-mode: fullscreen)').matches ||
    navigator.standalone === true;
}

// The engine's sleeps while the tab is hidden. The game hands the page time through emscripten_sleep: a sleep of 0 ms
// at least every 12 ms of its work (host/web.c, rt_web_yield) and every 100 ms of a day's processing (rt_web_breathe),
// one of 1 ms while it waits for the player. Emscripten waits with setTimeout, and Chrome runs a hidden tab's timers
// once a second, and after five minutes once a minute (its intensive throttling of timer chains): a player who moved to
// another window found the game's days stopped until he came back (22 November 2026). A dedicated worker's timers are
// not slowed so, and its message wakes the page at once: while the tab is hidden, the page sleeps through it. The
// worker is made at the first hidden sleep; in view the engine's own sleep runs, as before.
export function workerTimer(makeWorker = () => new Worker(URL.createObjectURL(new Blob(
  ['onmessage = (e) => setTimeout(() => postMessage(e.data.id), e.data.ms);'], { type: 'text/javascript' })))) {
  let worker = null, next = 0;
  const waiting = new Map();
  return (ms) => {
    if (worker === null) {
      try {
        worker = makeWorker();
        worker.onmessage = (e) => { const done = waiting.get(e.data); waiting.delete(e.data); done?.(); };
      } catch { worker = false; }                     // no worker here: the engine's own sleep
    }
    if (!worker) return null;
    return new Promise((resolve) => { const id = ++next; waiting.set(id, resolve); worker.postMessage({ id, ms: Math.max(0, ms | 0) }); });
  };
}
