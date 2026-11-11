// Usage statistics of the page (web/README.md, "Statistics and the admin panel"; the server's side: stats.mjs).
//
// A heartbeat each minute while the game runs: the seconds played since the last one, and what the user folder holds
// (saves, older copies, data sets: their number and size). A second counts as played when the tab is visible and the
// player clicked, typed or touched in the last 5 minutes. The browser has a random id of its own (localStorage
// cm0102-anon): no name, no email and no IP address go with it; a signed-in account goes as the account. The player can
// turn the statistics off in Help (localStorage cm0102-stats = off); with the server's opt-in mode they are off until
// turned on. Error reports (a stop of the game) go with the last lines of the engine log, a few per visit.

const BEAT = 60000, TICK = 5000, IDLE = 5 * 60000, MAX_REPORTS = 3;
const ANON_KEY = 'cm0102-anon', CHOICE_KEY = 'cm0102-stats';

const get = (k) => { try { return localStorage.getItem(k); } catch { return null; } };
const set = (k, v) => { try { localStorage.setItem(k, v); } catch { /* storage off: the choice lasts this visit */ } };
const uuid = () => (crypto.randomUUID ? crypto.randomUUID() : '10000000-1000-4000-8000-100000000000'
  .replace(/[018]/g, (c) => (Number(c) ^ (crypto.getRandomValues(new Uint8Array(1))[0] & (15 >> (Number(c) / 4)))).toString(16)));

// mode: 'optout' or 'optin' (the server's, files.json), or null: no statistics on this server.
export function statsAllowed(mode) {
  if (!mode) return false;
  const c = get(CHOICE_KEY);
  return mode === 'optin' ? c === 'on' : c !== 'off';
}
export function setStatsAllowed(on) { set(CHOICE_KEY, on ? 'on' : 'off'); }

// info: files.json; state(): { saves, older, sets } as { n, bytes }; extra(): { engine, vault, cloud }; isRunning().
export function startStats({ info, state, extra = () => ({}), isRunning = () => true, log = () => {} }) {
  const mode = info?.stats?.mode ?? null;
  let anon = get(ANON_KEY);
  if (!anon || !/^[0-9a-f-]{36}$/.test(anon)) { anon = uuid(); set(ANON_KEY, anon); }
  const session = uuid();
  let played = 0, lastInput = Date.now(), reports = 0;
  const seen = () => { lastInput = Date.now(); };
  for (const t of ['pointerdown', 'keydown', 'touchstart', 'wheel']) addEventListener(t, seen, { capture: true, passive: true });
  const post = (path, body, keepalive = false) => fetch(path, {
    method: 'POST', credentials: 'same-origin', keepalive,
    headers: { 'content-type': 'application/json', 'x-cm': '1' }, body: JSON.stringify(body),
  });

  async function beat(keepalive = false) {
    if (!statsAllowed(mode)) { played = 0; return; }
    const seconds = played;
    let s = null;
    try { s = state(); } catch { /* the folder is not there yet */ }
    try {
      const r = await post('/api/stats/beat', { anon, session, seconds, state: s, ...extra() }, keepalive);
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
      post('/api/report', { anon, session, message: String(message).slice(0, 500), log: lines.slice(-60), where, ...extra() }, true)
        .catch((e) => log(`page: the error report was not sent: ${e.message}`));
    },
    get mode() { return mode; },
  };
}
