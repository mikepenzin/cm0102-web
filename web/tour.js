// The first-visit tour (web/README.md, "The first-visit tour"): once per browser, after the game has started, a card
// beside each of the page's own buttons says what it is for. A new player from a Facebook group or the video guides
// sees a row (or, on a phone held sideways, a column) of icons around the game, and one could not find the Editor.
// Help's "Show the tour" runs it again. Nothing of the game changes; the card covers a corner of the game until it
// ends. It does not hold the game: a tap on the game still reaches it, and Escape ends the tour while the card has the
// focus (keyboard.js's guard keeps the card's keys from the game).

import { onLanguage, priceText, t } from './i18n.js';

export const TOUR_KEY = 'cm0102-tour-seen';

// The steps, in order. A step whose button is not shown is left out: Keyboard and Right click show on a touch device
// only, Full screen where the browser offers it and the page is not full already (not on an iPhone), Sign in where
// the server has accounts. A text may be a function, read when its step shows (Cloud Saves: cloudStepText).
// Each step keeps its English title and text; words() gives them in the page's language, read when the step shows
// (at import the page is English, so the English here is t's own).
const step = (target, words) => ({ target, ...words(), words });
export const TOUR_STEPS = [
  step('saves-open', () => ({ title: t('Saved games'),
    text: t('Your saves live in this browser. Download keeps a copy on your device; Upload brings one in, also from the desktop game. Databases for a new game are here too.') })),
  step('account-open', () => ({ title: t('Membership'), text: t('Membership: Cloud Saves keep your saves on all your devices, and the Scout finds players.') })),
  step('editor-open', () => ({ title: t('Squad Hub'),
    text: t('Change the players of a saved game, or make a database of your own for new games. In a saved game, the Scout finds players by age, potential and attributes.') })),
  step('kbd-open', () => ({ title: t('Keyboard'),
    text: t('Opens your keyboard to type in the game: a manager\'s name, a search. When the game waits for text, a "Tap to type" sign shows beside the entry too.') })),
  step('right-click', () => ({ title: t('Right click'),
    text: t('Tap it, then tap the game: that tap is a right click (a player\'s instructions on the tactics screen). A long press on the game does it too.') })),
  step('fullscreen', () => ({ title: t('Full screen'),
    text: t('The game fills the screen. These buttons go beside it, or into a small tab at the edge (›) when there is no room. The same button leaves full screen.') })),
  step('help-open', () => ({ title: t('Help'),
    text: t('The gestures, the video guides, and Send to the developer when something goes wrong. "Show the tour" there runs this tour again.') })),
];

// What the Membership step says, for the account as the page knows it (account.js, /api/me): what Membership holds,
// and how to turn them on from here, in the words of the Account dialog's own button. price: " ($5 a month)" or none.
export function cloudStepText(me) {
  const what = t('Membership holds Cloud Saves, which keep your saves on all your devices (each save goes up right after you save it, and your other devices get it when they start), and the Scout, which finds players in Squad Hub.');
  if (me?.user?.cloud) return t('You are a member ({email}): Cloud Saves are on, each save goes up right after you save it and your other devices get it when they start, and the Scout in Squad Hub shows all its matches. This button shows your membership.', { email: me.user.email });
  const offer = me?.offer;
  if (offer?.free) return `${what} ${t('Sign in here with Google, and you are a member, free.')}`;
  const price = offer?.price ? ` (${priceText(offer.price)})` : '';
  let how;
  if (me?.user) {
    how = offer?.coffee
      ? t('To become a member, open this button, press "Become a member"{price} and join on Buy Me a Coffee with the same email: it turns on by itself a moment after. Until then your saves stay in this browser.', { price })
      : t('To become a member, open this button, press "Become a member"{price} and pay: it turns on a moment after. Until then your saves stay in this browser.', { price });
  } else {
    how = offer?.coffee
      ? t('To become a member, open this button and sign in with Google, then press "Become a member"{price} and join on Buy Me a Coffee with the same email: it turns on by itself a moment after. Without it your saves stay in this browser.', { price })
      : t('To become a member, open this button and sign in with Google, then press "Become a member"{price} and pay: it turns on a moment after. Without it your saves stay in this browser.', { price });
  }
  return `${what} ${how}`;
}

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
      <div class="row"><button id="tour-skip" class="link"></button><span class="spacer"></span>
      <button id="tour-back"></button><button id="tour-next" class="primary"></button></div>`;
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

  // The card's words for the step at, in the page's language.
  function words() {
    const step = list[at];
    const w = step.words?.() ?? step;
    card.querySelector('#tour-count').textContent = t('{at} of {count}', { at: at + 1, count: list.length });
    card.querySelector('#tour-title').textContent = w.title;
    card.querySelector('#tour-text').textContent = typeof step.text === 'function' ? step.text() : w.text;
    card.querySelector('#tour-skip').textContent = t('Skip the tour');
    card.querySelector('#tour-back').textContent = t('Back');
    card.querySelector('#tour-back').hidden = at === 0;
    card.querySelector('#tour-next').textContent = at + 1 < list.length ? t('Next') : t('Done');
  }

  function go(i) {
    at = Math.max(0, Math.min(i, list.length - 1));
    ring?.classList.remove('tour-target');
    ring = doc.getElementById(list[at].target);
    ring.classList.add('tour-target');
    words();
    place();
    card.querySelector('#tour-next').focus({ preventScroll: true });
  }

  // A language chosen while the card is up: the card in it at once.
  onLanguage(() => { if (card && list[at]) { words(); place(); } });

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
