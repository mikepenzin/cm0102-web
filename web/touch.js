// Touch input (web/README.md, "On an iPad or another touch device"): what a finger does on the game, as the mouse
// that the game knows.
//
//   a tap                      a click of the left button
//   a drag                     the left button held and moved (scroll bars, the tactics board)
//   a long press               a click of the right button
//   a tap with two fingers     a click of the right button
//   a drag with two fingers    the mouse wheel (lists scroll with the fingers)
//
// SDL's browser backend has its own touch handling: it turns every touch into the left button at once, so it has no
// right button and no wheel. This file takes the touches of the canvas before SDL sees them (a capture listener on
// the window, made before the engine loads) and sends mouse and wheel events to the canvas in their place. SDL reads
// those as it reads a real mouse (SDL_emscriptenevents.c: mousemove and mousedown on the canvas, mouseup on the
// document, wheel on the canvas; a button event carries no place, so a move goes first). A mouse or a trackpad on a
// touch device sends mouse events of its own, which pass through untouched.
export const SLOP = 10;          // CSS pixels a finger may wander and still be a tap
export const LONG_MS = 550;      // a press this long without a move is a right click
export const CLICK_MS = 80;      // from a made button down to its up: the game sees them in different input polls
export const WHEEL_PX = 36;      // finger travel for one step of the wheel

export function mountTouch({ canvas, stage, fx, onFirstTouch = () => {}, onTap = () => {} }) {
  let mode = 'idle';              // idle | pending | drag | scroll | done
  let first = null;               // the first finger: {id, x0, y0, x, y, t0}
  let longTimer = 0, upTimer = 0, pendingUp = null, started = false;
  let scroll = null;              // {y, acc, moved, t0}

  const send = (type, x, y, button, buttons) => canvas.dispatchEvent(new MouseEvent(type, {
    bubbles: true, cancelable: true, view: window, clientX: x, clientY: y, screenX: x, screenY: y, button, buttons }));
  const wheel = (x, y, deltaY) => canvas.dispatchEvent(new WheelEvent('wheel', {
    bubbles: true, cancelable: true, view: window, clientX: x, clientY: y, deltaY, deltaMode: 0 }));

  function ping(x, y, kind) {
    if (!fx) return;
    const r = stage.getBoundingClientRect();
    const el = document.createElement('i');
    el.className = `ping ${kind}`;
    el.style.left = `${x - r.left}px`;
    el.style.top = `${y - r.top}px`;
    el.addEventListener('animationend', () => el.remove());
    fx.appendChild(el);
    while (fx.childElementCount > 6) fx.firstElementChild.remove();
  }

  // The up of a made click, now: before another press, or when its time has come.
  function flushUp() {
    clearTimeout(upTimer);
    if (pendingUp) { const p = pendingUp; pendingUp = null; send('mouseup', p.x, p.y, p.button, 0); }
  }

  function click(x, y, button) {
    flushUp();
    send('mousemove', x, y, 0, 0);
    send('mousedown', x, y, button, button === 2 ? 2 : 1);
    pendingUp = { x, y, button };
    upTimer = setTimeout(flushUp, CLICK_MS);
  }

  const find = (list, id) => { for (const t of list) if (t.identifier === id) return t; return null; };
  const centre = (touches) => {
    let x = 0, y = 0;
    for (const t of touches) { x += t.clientX; y += t.clientY; }
    return { x: x / touches.length, y: y / touches.length };
  };

  function onStart(e) {
    if (e.target !== canvas) return;
    e.preventDefault();
    e.stopImmediatePropagation();
    if (!started) { started = true; onFirstTouch(); }
    const t = e.changedTouches[0];
    if (mode === 'idle') {
      flushUp();
      first = { id: t.identifier, x0: t.clientX, y0: t.clientY, x: t.clientX, y: t.clientY, t0: performance.now() };
      mode = 'pending';
      send('mousemove', t.clientX, t.clientY, 0, 0);          // the pointer is under the finger: hover, and the wheel's place
      longTimer = setTimeout(() => {
        if (mode !== 'pending') return;
        mode = 'done';
        ping(first.x, first.y, 'right');
        click(first.x, first.y, 2);
      }, LONG_MS);
    } else if (mode === 'pending' && e.touches.length === 2) {
      clearTimeout(longTimer);
      const c = centre(e.touches);
      scroll = { y: c.y, acc: 0, moved: false, t0: first.t0 };
      mode = 'scroll';
      send('mousemove', c.x, c.y, 0, 0);
    }
  }

  function onMove(e) {
    if (mode === 'idle' || (e.target !== canvas && mode === 'done')) return;
    if (e.target === canvas) { e.preventDefault(); e.stopImmediatePropagation(); }
    if (mode === 'pending' || mode === 'drag') {
      const t = find(e.touches, first.id);
      if (!t) return;
      first.x = t.clientX;
      first.y = t.clientY;
      if (mode === 'pending' && Math.hypot(t.clientX - first.x0, t.clientY - first.y0) > SLOP) {
        clearTimeout(longTimer);
        mode = 'drag';
        send('mousemove', first.x0, first.y0, 0, 0);
        send('mousedown', first.x0, first.y0, 0, 1);
      }
      if (mode === 'drag') send('mousemove', t.clientX, t.clientY, 0, 1);
    } else if (mode === 'scroll' && e.touches.length >= 2) {
      const c = centre(e.touches);
      scroll.acc += c.y - scroll.y;
      scroll.y = c.y;
      while (Math.abs(scroll.acc) >= WHEEL_PX) {
        const down = scroll.acc > 0;                            // the fingers go down: the list follows, as the wheel turned up
        scroll.acc -= down ? WHEEL_PX : -WHEEL_PX;
        scroll.moved = true;
        wheel(c.x, c.y, down ? -100 : 100);
      }
    }
  }

  function onEnd(e) {
    if (mode === 'idle') return;
    if (e.target === canvas) { e.preventDefault(); e.stopImmediatePropagation(); }
    clearTimeout(longTimer);
    const cancelled = e.type === 'touchcancel';
    if (mode === 'pending' && find(e.changedTouches, first.id)) {
      if (!cancelled) {
        onTap(first.x, first.y);                                // inside the touch's own handler: it may open the keyboard
        ping(first.x, first.y, 'tap');
        click(first.x, first.y, 0);
      }
    } else if (mode === 'drag' && find(e.changedTouches, first.id)) {
      send('mouseup', first.x, first.y, 0, 0);
    } else if (mode === 'scroll' && !scroll.moved && !cancelled && performance.now() - scroll.t0 < 400) {
      const c = centre([...e.touches, ...e.changedTouches]);
      ping(c.x, c.y, 'right');
      click(c.x, c.y, 2);
      scroll.moved = true;                                      // once: the second finger's lift is not another click
    }
    mode = e.touches.length === 0 ? 'idle' : mode === 'scroll' ? 'scroll' : 'done';
    if (mode === 'idle') { first = null; scroll = null; }
  }

  const opts = { capture: true, passive: false };
  addEventListener('touchstart', onStart, opts);
  addEventListener('touchmove', onMove, opts);
  addEventListener('touchend', onEnd, opts);
  addEventListener('touchcancel', onEnd, opts);
  // Safari's own pinch: it would zoom the page under the game.
  document.addEventListener('gesturestart', (e) => e.preventDefault());
}
