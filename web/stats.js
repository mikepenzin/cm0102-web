// Usage statistics of the page (web/README.md, "Statistics and the admin panel"; the server's side: stats.mjs).
//
// A heartbeat each minute while the game runs: the seconds played since the last one, and what the user folder holds
// (saves, older copies, data sets: their number and size). A second counts as played when the tab is visible and the
// player clicked, typed or touched in the last 5 minutes. The browser has a random id of its own (localStorage
// cm0102-anon): no name, no email and no IP address go with it; a signed-in account goes as the account. Error reports
// (a stop of the game) go with the last lines of the engine log, a few per visit.
//
// Consent. With the server's opt-in mode (the default) nothing is sent and no id is stored until the player allows it:
// the page asks once (needsChoice, the consent card of loader.js), and the answer stays in localStorage cm0102-stats
// (on or off). No thanks sends only that answer, with no id, so the admin page can tell how many say no. Turning them
// off later (Help, Statistics) deletes the browser's id. With the opt-out mode they are on until turned off.

const BEAT = 60000, TICK = 5000, IDLE = 5 * 60000, MAX_REPORTS = 3;
const ANON_KEY = 'cm0102-anon', CHOICE_KEY = 'cm0102-stats';

const get = (k) => { try { return localStorage.getItem(k); } catch { return null; } };
const set = (k, v) => { try { localStorage.setItem(k, v); } catch { /* storage off: the choice lasts this visit */ } };
const drop = (k) => { try { localStorage.removeItem(k); } catch { /* storage off */ } };
const uuid = () => (crypto.randomUUID ? crypto.randomUUID() : '10000000-1000-4000-8000-100000000000'
  .replace(/[018]/g, (c) => (Number(c) ^ (crypto.getRandomValues(new Uint8Array(1))[0] & (15 >> (Number(c) / 4)))).toString(16)));
const post = (path, body, keepalive = false) => fetch(path, {
  method: 'POST', credentials: 'same-origin', keepalive,
  headers: { 'content-type': 'application/json', 'x-cm': '1' }, body: JSON.stringify(body),
});

let visitChoice = null;                              // the answer of this visit, when localStorage keeps none

// The player's answer: 'on', 'off' or null (not asked yet).
export function statsChoice() {
  const c = get(CHOICE_KEY) ?? visitChoice;
  return c === 'on' || c === 'off' ? c : null;
}

// mode: 'optin' or 'optout' (the server's, files.json), or null: no statistics on this server.
export function statsAllowed(mode) {
  if (!mode) return false;
  const c = statsChoice();
  return mode === 'optin' ? c === 'on' : c !== 'off';
}

// Whether the page asks: the server wants consent and the player has not answered.
export const needsChoice = (mode) => mode === 'optin' && statsChoice() === null;

// The player's answer. Off deletes the browser's id. tell: send the answer to the server (no id with it), for the count
// of answers on the admin page.
export function setStatsAllowed(on, { tell = false } = {}) {
  visitChoice = on ? 'on' : 'off';
  set(CHOICE_KEY, visitChoice);
  if (!on) drop(ANON_KEY);
  if (tell) post('/api/stats/choice', { choice: visitChoice }, true).catch(() => {});
}

// info: files.json; state(): { saves, older, sets } as { n, bytes }; extra(): { engine, vault, cloud }; isRunning().
export function startStats({ info, state, extra = () => ({}), isRunning = () => true, log = () => {} }) {
  const mode = info?.stats?.mode ?? null;
  const session = uuid();
  let played = 0, lastInput = Date.now(), reports = 0;
  // The browser's id: made only once the statistics are allowed.
  const anonId = () => {
    let anon = get(ANON_KEY);
    if (!anon || !/^[0-9a-f-]{36}$/.test(anon)) { anon = uuid(); set(ANON_KEY, anon); }
    return anon;
  };
  const seen = () => { lastInput = Date.now(); };
  for (const t of ['pointerdown', 'keydown', 'touchstart', 'wheel']) addEventListener(t, seen, { capture: true, passive: true });

  async function beat(keepalive = false) {
    if (!statsAllowed(mode)) { played = 0; return; }
    const seconds = played;
    let s = null;
    try { s = state(); } catch { /* the folder is not there yet */ }
    try {
      const r = await post('/api/stats/beat', { anon: anonId(), session, seconds, state: s, ...extra() }, keepalive);
      if (r.ok) played = Math.max(0, played - seconds);
    } catch { /* offline: the seconds go with the next one */ }
  }

  setInterval(() => {
    if (isRunning() && document.visibilityState === 'visible' && Date.now() - lastInput < IDLE) played += TICK / 1000;
  }, TICK);
  setInterval(() => beat(), BEAT);
  setTimeout(() => beat(), 3000);                    // the visit counts at once
  addEventListener('pagehide', () => { if (played) beat(true); });

  return {
    // A stop of the game: what it said and the last lines of the log.
    report(message, lines = [], where = '') {
      if (!statsAllowed(mode) || reports >= MAX_REPORTS) return;
      reports++;
      post('/api/report', { anon: anonId(), session, message: String(message).slice(0, 500), log: lines.slice(-60), where, ...extra() }, true)
        .catch((e) => log(`page: the error report was not sent: ${e.message}`));
    },
    // The player allowed them just now: the visit counts at once.
    allowed() { beat(); },
    get mode() { return mode; },
  };
}
