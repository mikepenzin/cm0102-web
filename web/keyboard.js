// The on-screen keyboard (web/README.md): an iPad or a phone shows its keyboard only for a text field of the page, and
// the game draws its text entries on the canvas. So a Keyboard button focuses a hidden text field, and what is typed
// there goes to the game through the runtime (rt_web_type and rt_web_key in src/recomp_rt/win/user32.c), which hands
// it to the game one character at a time, as a typed key would come.
//
// The keys typed into the field never reach SDL's own keyboard handling (a window listener stops them first), so a
// hardware keyboard on an iPad does not type twice.
const KEYS = { Enter: 13, Escape: 27, Tab: 9 };
const MARK = ' ';            // stays in the field, so that Backspace on an "empty" field is still an input to read

let field = null;

// Before the engine starts: its keyboard listeners come later, so this one runs first.
export function guardKeys() {
  for (const type of ['keydown', 'keypress', 'keyup']) {
    addEventListener(type, (e) => {
      if (!field || e.target !== field) return;
      e.stopImmediatePropagation();
      if (type === 'keydown' && KEYS[e.key]) {
        e.preventDefault();
        window.Module?.ccall?.('rt_web_key', null, ['number'], [KEYS[e.key]]);
      }
    }, true);
  }
}

export function hasTouch() {
  return navigator.maxTouchPoints > 0 || matchMedia('(pointer: coarse)').matches;
}

// Wires the button and the field of index.html once the engine runs.
export function mountKeyboard({ log }) {
  field = document.getElementById('kbd');
  const button = document.getElementById('kbd-open');
  const type = (text) => window.Module.ccall('rt_web_type', null, ['string'], [text]);
  const key = (vk) => window.Module.ccall('rt_web_key', null, ['number'], [vk]);
  const reset = () => { field.value = MARK; field.setSelectionRange(MARK.length, MARK.length); };

  field.addEventListener('input', () => {
    const v = field.value;
    if (v.length < MARK.length) key(8);                       // the mark went: Backspace
    else {
      const text = v.slice(MARK.length);
      for (const part of text.split(/(\n)/)) {
        if (part === '\n') key(13);
        else if (part) type(part);
      }
    }
    reset();
  });
  field.addEventListener('focus', () => { reset(); button.textContent = 'Hide keyboard'; });
  field.addEventListener('blur', () => { button.textContent = 'Keyboard'; });
  // Focus in the tap itself: iPadOS opens the keyboard only for a focus that a tap makes.
  button.addEventListener('pointerdown', (e) => e.preventDefault());   // keep the field focused when it is already
  button.addEventListener('click', () => {
    if (document.activeElement === field) field.blur();
    else { field.focus(); log('page: on-screen keyboard'); }
  });
  button.hidden = !hasTouch();
}
