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
export function isStandalone() {
  return matchMedia('(display-mode: standalone)').matches || matchMedia('(display-mode: fullscreen)').matches ||
    navigator.standalone === true;
}
