// The first-visit tour (web/README.md, "The first-visit tour"): once per browser, after the game has started, a card
// beside each of the page's own buttons says what it is for. A new player from a Facebook group or the video guides
// sees a row (or, on a phone held sideways, a column) of icons around the game, and one could not find the Editor.
// Help's "Show the tour" runs it again. Nothing of the game changes; the card covers a corner of the game until it
// ends. It does not hold the game: a tap on the game still reaches it, and Escape ends the tour while the card has the
// focus (keyboard.js's guard keeps the card's keys from the game).

export const TOUR_KEY = 'cm0102-tour-seen';

// The steps, in order. A step whose button is not shown (the Right click button on a computer, the bar folded away in
// full screen) is left out.
export const TOUR_STEPS = [
  { target: 'saves-open', title: 'Saved games',
    text: 'Your saves live in this browser. Download keeps a copy on your device; Upload brings one in, also from the desktop game. Databases for a new game are here too.' },
  { target: 'editor-open', title: 'Editor',
    text: 'Change the players of a saved game, or make a database of your own for new games.' },
  { target: 'right-click', title: 'Right click',
    text: 'Tap it, then tap the game: that tap is a right click (a player\'s instructions on the tactics screen). A long press on the game does it too.' },
  { target: 'help-open', title: 'Help',
    text: 'The gestures, the video guides, and Send to the developer when something goes wrong.' },
];

const GAP = 10;                 // between the card and its button
const EDGE = 8;                 // the card keeps this far from the window's edges

// Where the card goes for a button at r (a DOMRect) in a window of vw x vh: below the button when the bar is a row and
// there is room, else beside it, on the side with more room. column: the bar is a column (a phone held sideways, full
// screen beside the game), where a card below its button would cover the buttons under it.
export function placeCard(r, card, vw, vh, { column = false } = {}) {
  const clampX = (x) => Math.max(EDGE, Math.min(x, vw - card.w - EDGE));
  const clampY = (y) => Math.max(EDGE, Math.min(y, vh - card.h - EDGE));
  if (!column && r.bottom + GAP + card.h <= vh - EDGE && r.top < vh / 3) {
    return { left: clampX(r.right - card.w), top: r.bottom + GAP, side: 'below' };
  }
  const leftRoom = r.left - GAP - EDGE, rightRoom = vw - r.right - GAP - EDGE;
  if (leftRoom >= card.w || leftRoom >= rightRoom) return { left: Math.max(EDGE, r.left - GAP - card.w), top: clampY(r.top + r.height / 2 - card.h / 2), side: 'left' };
  return { left: Math.min(r.right + GAP, vw - card.w - EDGE), top: clampY(r.top + r.height / 2 - card.h / 2), side: 'right' };
}

const shown = (el) => Boolean(el) && !el.hidden && el.offsetParent !== null && el.getClientRects().length > 0;

// storage: localStorage or a stand-in (it may throw: a private window, and then the tour shows at each visit's start).
// onPageKey: keyboard.js's, for the keys of the card.
export function mountTour({ doc = document, win = window, storage = null, steps = TOUR_STEPS, onPageKey = null, onEnd = () => {} } = {}) {
  let card = null, at = 0, list = [], ring = null, unhookKey = null;
  const seen = () => { try { return storage?.getItem(TOUR_KEY) === '1'; } catch { return false; } };
  const markSeen = () => { try { storage?.setItem(TOUR_KEY, '1'); } catch { /* not kept: it shows again next visit */ } };

  function build() {
    card = doc.createElement('div');
    card.id = 'tour';
    card.setAttribute('role', 'dialog');
    card.setAttribute('aria-modal', 'false');
    card.setAttribute('aria-labelledby', 'tour-title');
    card.innerHTML = `<p class="tour-count" id="tour-count"></p><h3 id="tour-title"></h3><p id="tour-text"></p>
      <div class="row"><button id="tour-skip" class="link">Skip the tour</button><span class="spacer"></span>
      <button id="tour-back">Back</button><button id="tour-next" class="primary">Next</button></div>`;
    doc.body.appendChild(card);
    card.querySelector('#tour-skip').onclick = () => end();
    card.querySelector('#tour-back').onclick = () => go(at - 1);
    card.querySelector('#tour-next').onclick = () => (at + 1 < list.length ? go(at + 1) : end());
  }

  function place() {
    if (!card || !list[at]) return;
    const el = doc.getElementById(list[at].target);
    if (!shown(el)) { list = list.filter((s) => shown(doc.getElementById(s.target))); if (!list.length) { end(); return; } go(Math.min(at, list.length - 1)); return; }
    const r = el.getBoundingClientRect(), bar = el.parentElement?.getBoundingClientRect();
    const column = Boolean(bar) && bar.height > bar.width;
    const p = placeCard(r, { w: card.offsetWidth, h: card.offsetHeight }, win.innerWidth, win.innerHeight, { column });
    card.style.left = `${Math.round(p.left)}px`;
    card.style.top = `${Math.round(p.top)}px`;
    card.dataset.side = p.side;
  }

  function go(i) {
    at = Math.max(0, Math.min(i, list.length - 1));
    const step = list[at];
    ring?.classList.remove('tour-target');
    ring = doc.getElementById(step.target);
    ring.classList.add('tour-target');
    card.querySelector('#tour-count').textContent = `${at + 1} of ${list.length}`;
    card.querySelector('#tour-title').textContent = step.title;
    card.querySelector('#tour-text').textContent = step.text;
    card.querySelector('#tour-back').hidden = at === 0;
    card.querySelector('#tour-next').textContent = at + 1 < list.length ? 'Next' : 'Done';
    place();
    card.querySelector('#tour-next').focus({ preventScroll: true });
  }

  // Escape in the card ends the tour.
  const onKey = (e) => {
    if (card && e.key === 'Escape' && e.target instanceof Node && card.contains(e.target)) { e.preventDefault(); end(); }
  };

  function end() {
    if (!card) return;
    ring?.classList.remove('tour-target');
    ring = null;
    card.remove();
    card = null;
    win.removeEventListener('resize', place);
    unhookKey?.();
    unhookKey = null;
    markSeen();
    onEnd();
  }

  // Runs the tour now (Help's "Show the tour"). Resolves nothing; returns whether it started.
  function start() {
    if (card) return true;
    list = steps.filter((s) => shown(doc.getElementById(s.target)));
    if (!list.length) return false;            // no button to show (the bar folded away): try again at the next visit
    build();
    win.addEventListener('resize', place);
    unhookKey = onPageKey?.(onKey) ?? null;
    go(0);
    return true;
  }

  return {
    start,
    end,
    // The first visit: the tour once, if this browser has not seen it.
    maybeStart() { return seen() ? false : start(); },
    get open() { return Boolean(card); },
    get seen() { return seen(); },
    relayout: place,
  };
}
