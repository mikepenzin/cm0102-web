// The keyboard (web/README.md): which keys go to the game, and the on-screen keyboard of a touch device.
//
// Keys. SDL listens for keys on the window, so it would also take what is typed into the page's own parts. A capture
// listener made before the engine loads lets a key through to SDL only when the game should have it: not while a
// dialog of the page is open, and not from a button or a field of the page.
//
// The on-screen keyboard. An iPad shows its keyboard only for a text field of the page, and the game draws its text
// entries on the canvas. So the page has a hidden text field; what is typed there goes to the game through the
// runtime (rt_web_type and rt_web_key in src/recomp_rt/win/user32.c), one character at a time, as typed keys come.
// The runtime also says where a text entry of the game takes keys (rt_web_typing_row). With that the page offers the
// keyboard where it is needed: a "Tap to type" chip at the entry, a tap on the entry itself opens the keyboard, the
// picture moves so the entry stays above the keyboard (layout.js), and the keyboard closes when the entry is done.
// The device only opens its keyboard from inside a tap, which is why the page cannot open it by itself.
const KEYS = { Enter: 13, Escape: 27, Tab: 9 };
const MARK = ' ';            // stays in the field, so that Backspace on an "empty" field is still an input to read
const ROW_REACH = 30;        // rows of the picture around the entry's middle that count as a tap on the entry

let field = null;

const pageOwns = (target) => {
  if (document.querySelector('dialog[open]')) return true;
  return target instanceof Element && target.closest('button, a, input, textarea, select, summary, [contenteditable]') !== null;
};

// Before the engine starts: its keyboard listeners come later, so this one runs first.
export function guardKeys() {
  for (const type of ['keydown', 'keypress', 'keyup']) {
    addEventListener(type, (e) => {
      if (field && e.target === field) {
        e.stopImmediatePropagation();
        if (type === 'keydown' && KEYS[e.key]) {
          e.preventDefault();
          window.Module?.ccall?.('rt_web_key', null, ['number'], [KEYS[e.key]]);
        }
      } else if (pageOwns(e.target)) {
        e.stopImmediatePropagation();                         // the page's own part takes the key; the game does not
      }
    }, true);
  }
}

export function hasTouch() {
  return navigator.maxTouchPoints > 0 || matchMedia('(pointer: coarse)').matches;
}

// Wires the button, the chip and the field of index.html once the engine runs. Returns null with an engine built
// before the keyboard calls.
export function mountKeyboard({ log, layout, toast = () => {} }) {
  const M = window.Module;
  const button = document.getElementById('kbd-open'), chip = document.getElementById('type-chip');
  if (typeof M.ccall !== 'function' || typeof M._rt_web_type !== 'function' || typeof M._rt_web_key !== 'function') {
    // An engine built before these calls: a button would type nothing, so there is none.
    if (hasTouch()) log('page: this engine has no keyboard calls (rt_web_type); build it again for the Keyboard button (web/README.md, step 4)');
    return null;
  }
  field = document.getElementById('kbd');
  const label = button.querySelector('span');
  const type = (text) => M.ccall('rt_web_type', null, ['string'], [text]);
  const key = (vk) => M.ccall('rt_web_key', null, ['number'], [vk]);
  const reset = () => { field.value = MARK; field.setSelectionRange(MARK.length, MARK.length); };
  const focused = () => document.activeElement === field;
  let forTyping = false;       // the keyboard is open for a text entry of the game: it closes when the entry is done
  let hinted = false;

  const open = () => { forTyping = layout.typingRow >= 0; field.focus({ preventScroll: true }); };
  const close = () => field.blur();

  field.addEventListener('input', () => {
    const v = field.value;
    if (v.length < MARK.length) key(8);                       // the mark went: Backspace
    else {
      for (const part of v.slice(MARK.length).split(/(\n)/)) {
        if (part === '\n') key(13);
        else if (part) type(part);
      }
    }
    reset();
  });
  field.addEventListener('focus', () => {
    reset();
    label.textContent = 'Hide keyboard';
    button.setAttribute('aria-pressed', 'true');
    layout.setFieldFocused(true);
    log('page: on-screen keyboard');
  });
  field.addEventListener('blur', () => {
    label.textContent = 'Keyboard';
    button.setAttribute('aria-pressed', 'false');
    forTyping = false;
    layout.setFieldFocused(false);
  });
  // Focus in the tap itself: the device opens its keyboard only for a focus that a tap makes.
  for (const el of [button, chip]) el.addEventListener('pointerdown', (e) => e.preventDefault());   // the field keeps the focus
  button.addEventListener('click', () => (focused() ? close() : open()));
  chip.addEventListener('click', open);
  button.hidden = !hasTouch();

  return {
    // The runtime's word on the text entry, a few times a second (loader.js).
    setTypingRow(row) {
      const was = layout.typingRow;
      layout.setTypingRow(row);
      if (row >= 0 && focused()) forTyping = true;
      if (row < 0 && was >= 0 && focused() && forTyping) close();
      if (row >= 0 && was < 0 && hasTouch() && !focused() && !hinted) {
        hinted = true;
        toast('Tap the text box, or Tap to type, for the keyboard', { ms: 4000 });
      }
    },
    // A tap on the game (touch.js), still inside the touch's handler: on the entry that takes keys, it opens the keyboard.
    onTap(clientX, clientY) {
      if (layout.typingRow < 0 || focused()) return;
      const p = layout.pictureAt(clientX, clientY);
      if (p && Math.abs(p.y - layout.typingRow) <= ROW_REACH) open();
    },
    close,
  };
}
