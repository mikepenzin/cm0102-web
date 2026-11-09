// The layout of the game on the page (web/README.md, "The page").
//
// The canvas gets the shape of the game's picture (1280x800, or 800x600 in classic mode), as large as the stage
// allows, with its edges on device pixels. SDL's browser backend takes the size of a canvas that CSS sizes: at each
// resize event of the window it sets the canvas's own pixels to the CSS size times the device pixel ratio
// (SDL_emscriptenevents.c, Emscripten_HandleResize), and the runtime then scales the picture to exactly that
// (user32.c, update_scaling). So the picture fills the canvas with no bars, sharp on a high-DPI screen, and this file
// only has to keep the CSS size right and tell SDL when it changed.
//
// On a touch device the on-screen keyboard covers the lower part of the page. The page then keeps the picture at its
// size and moves it up so that the text entry being typed in stays in view (the runtime gives its row), or, when it
// does not know the row, shrinks the picture into the part that is left.

// The canvas size for a picture of pw x ph in a space of availW x availH CSS pixels: the largest whole multiple of
// the picture's shape (8 by 5 for 1280x800, 4 by 3 for 800x600) that fits. SDL takes the CSS size cut to whole
// numbers as its window, and that times the device pixel ratio as the canvas's own pixels; with whole multiples of
// the shape both keep the picture's shape exactly, so the runtime scales the picture with no bar of even one pixel.
// bw x bh: the canvas's own pixels that SDL will set.
const gcd = (a, b) => (b ? gcd(b, a % b) : a);
export function fitPicture(pw, ph, availW, availH, dpr = 1) {
  if (!(pw > 0 && ph > 0)) return null;
  const g = gcd(pw, ph), ux = pw / g, uy = ph / g;
  const k = Math.floor(Math.min(availW / ux, availH / uy));
  if (k < 1) return null;
  const cssW = ux * k, cssH = uy * k;
  return { cssW, cssH, bw: Math.floor(cssW * dpr + 1e-6), bh: Math.floor(cssH * dpr + 1e-6) };
}

// How far to move a picture of height cssH (top at top0) so that the row at rowY of it shows in the top visibleH
// pixels: 0 or a negative number. The row lands a little above the middle of what is visible; the picture never
// leaves a gap under its bottom edge.
export function shiftForRow(top0, cssH, rowY, visibleH) {
  const y = top0 + rowY;
  if (y < visibleH - 48) return 0;                           // in view already, clear of the keyboard's edge
  const wanted = visibleH * 0.42 - y;
  const most = Math.min(0, visibleH - (top0 + cssH));         // the picture's bottom at the visible bottom
  return Math.round(Math.max(wanted, most));
}

export function mountLayout({ app, bar, stage, canvas, chip, isTouch }) {
  const vv = window.visualViewport;
  const state = {
    picture: { w: 1280, h: 800 },
    typingRow: -1,              // the row of the text entry that takes keys, or -1
    fieldFocused: false,        // the page's hidden text field has the focus (the on-screen keyboard is asked for)
    box: null,                  // the last layout: {left, top, cssW, cssH, bw, bh, shift}
    listeners: [],
  };
  let resizeQueued = false, lastSync = 0;

  const fullHeight = () => document.documentElement.clientHeight;
  // The part of the page's height that the on-screen keyboard (or anything else over the page) takes.
  const keyboardInset = () => (vv ? Math.max(0, fullHeight() - (vv.height + vv.offsetTop)) : 0);
  const keyboardOpen = () => isTouch && state.fieldFocused && keyboardInset() > 80;

  function tellSdl() {
    if (resizeQueued) return;
    resizeQueued = true;
    requestAnimationFrame(() => {
      resizeQueued = false;
      lastSync = performance.now();
      const e = new Event('resize');
      e.fromLayout = true;                                    // not a change of the window: the layout below skips it
      window.dispatchEvent(e);
    });
  }

  function layout() {
    // Keep the app where the visible part of the page is (the keyboard or a pinch can move it).
    const ox = vv ? vv.offsetLeft : 0, oy = vv ? vv.offsetTop : 0;
    app.style.transform = ox || oy ? `translate(${ox}px, ${oy}px)` : '';
    const dpr = window.devicePixelRatio || 1;
    const availW = stage.clientWidth, availH = stage.clientHeight;
    const kb = keyboardOpen();
    const visibleH = kb ? Math.max(120, availH - keyboardInset()) : availH;
    const follow = kb && state.typingRow >= 0;                // keep the size, move the row into view
    const fit = fitPicture(state.picture.w, state.picture.h, availW, follow || !kb ? availH : visibleH, dpr);
    if (!fit) return;
    const snap = (v) => Math.round(v * dpr) / dpr;
    const left = snap((availW - fit.cssW) / 2);
    const top = snap(((kb && !follow ? visibleH : availH) - fit.cssH) / 2);
    const shift = follow ? shiftForRow(top, fit.cssH, state.typingRow / state.picture.h * fit.cssH, visibleH) : 0;
    const old = state.box;
    state.box = { left, top, ...fit, shift, visibleH };
    const s = stage.style;
    s.setProperty('--cw', `${fit.cssW}px`);
    s.setProperty('--ch', `${fit.cssH}px`);
    s.setProperty('--cl', `${left}px`);
    s.setProperty('--ct', `${top}px`);
    s.setProperty('--shift', `${shift}px`);
    s.setProperty('--covered', `${availH - visibleH}px`);     // what the on-screen keyboard hides: messages stay above it
    if (chip) {
      const showChip = isTouch && state.typingRow >= 0 && !state.fieldFocused;
      chip.hidden = !showChip;
      if (showChip) {
        const y = top + state.typingRow / state.picture.h * fit.cssH;
        s.setProperty('--chip-y', `${Math.round(Math.min(Math.max(y, 28), availH - 28))}px`);
        s.setProperty('--chip-x', `${Math.round(left + fit.cssW - 12)}px`);
      }
    }
    if (!old || old.bw !== fit.bw || old.bh !== fit.bh) tellSdl();
    for (const fn of state.listeners) fn(state.box);
  }

  // SDL sets the canvas's pixels only at a resize event. If they are not what the layout asked for (the window was
  // made after the last event, or another size was set meanwhile), say so again, at most once a second.
  setInterval(() => {
    const b = state.box;
    if (!b || !canvas.dataset.engine) return;
    if ((Math.abs(canvas.width - b.bw) > 1 || Math.abs(canvas.height - b.bh) > 1) && performance.now() - lastSync > 900) tellSdl();
  }, 1000);

  addEventListener('resize', (e) => { if (!e.fromLayout) layout(); });
  addEventListener('orientationchange', layout);
  vv?.addEventListener('resize', layout);
  vv?.addEventListener('scroll', layout);
  new ResizeObserver(layout).observe(stage);
  // A change of the device pixel ratio (the window moved to another screen, or the page was zoomed).
  const watchRatio = () => matchMedia(`(resolution: ${window.devicePixelRatio}dppx)`).addEventListener('change', () => { layout(); watchRatio(); }, { once: true });
  watchRatio();

  // Full screen: the whole app, so the page's own buttons stay with the game.
  const fsElement = () => document.fullscreenElement ?? document.webkitFullscreenElement ?? null;
  const canFullscreen = Boolean(app.requestFullscreen || app.webkitRequestFullscreen);
  async function toggleFullscreen() {
    try {
      if (fsElement()) await (document.exitFullscreen ? document.exitFullscreen() : document.webkitExitFullscreen());
      else await (app.requestFullscreen ? app.requestFullscreen({ navigationUI: 'hide' }) : app.webkitRequestFullscreen());
    } catch { /* the browser said no: nothing changes */ }
  }
  function onFullscreenChange() {
    const on = Boolean(fsElement());
    app.classList.toggle('fs', on);
    // In full screen Escape belongs to the game, not to the browser (Chrome: a long press of Escape still leaves).
    if (on) navigator.keyboard?.lock?.(['Escape']).catch(() => {});
    else navigator.keyboard?.unlock?.();
    for (const fn of fsListeners) fn(on);
    layout();
  }
  const fsListeners = [];
  document.addEventListener('fullscreenchange', onFullscreenChange);
  document.addEventListener('webkitfullscreenchange', onFullscreenChange);

  layout();
  return {
    layout,
    get box() { return state.box; },
    get picture() { return state.picture; },
    get typingRow() { return state.typingRow; },
    keyboardOpen,
    canFullscreen,
    toggleFullscreen,
    isFullscreen: () => Boolean(fsElement()),
    onFullscreen: (fn) => fsListeners.push(fn),
    onLayout: (fn) => state.listeners.push(fn),
    setPicture(w, h) {
      if (w > 0 && h > 0 && (w !== state.picture.w || h !== state.picture.h)) { state.picture = { w, h }; layout(); }
    },
    setTypingRow(row) { if (row !== state.typingRow) { state.typingRow = row; layout(); } },
    setFieldFocused(on) { if (on !== state.fieldFocused) { state.fieldFocused = on; layout(); } },
    // The place in the picture (its own pixels) of a point of the page, or null when it is outside the picture.
    pictureAt(clientX, clientY) {
      const r = canvas.getBoundingClientRect();
      if (clientX < r.left || clientX >= r.right || clientY < r.top || clientY >= r.bottom) return null;
      return { x: (clientX - r.left) / r.width * state.picture.w, y: (clientY - r.top) / r.height * state.picture.h };
    },
  };
}
