// Early access (web/README.md, "Levels and early access"): a new feature of the page (features.js) or a new database of
// the library (library.mjs) is for Club Legends only until its day, then for everyone under its usual rules.
//
// A day is a calendar day, YYYY-MM-DD, and opens at its 00:00 UTC. The store holds the features' days:
//   early/features.json   { <key>: 'YYYY-MM-DD' }
// A key seen for the first time gets today + 14 days, written at once, so no feature waits for someone to remember it;
// not when the store could not be read (then the feature counts as open until a read works).
import { FEATURES } from './features.js';

export const EARLY_DAYS = 14;
const KEY = 'early/features.json';

export const todayUTC = (now = Date.now()) => new Date(now).toISOString().slice(0, 10);
export const addDays = (day, n) => new Date(Date.parse(`${day}T00:00:00Z`) + n * 86400e3).toISOString().slice(0, 10);
export const validDay = (s) => typeof s === 'string' && /^\d{4}-\d{2}-\d{2}$/.test(s) && !Number.isNaN(Date.parse(`${s}T00:00:00Z`))
  && new Date(Date.parse(`${s}T00:00:00Z`)).toISOString().slice(0, 10) === s;
// Whether a thing with this day is still early: its day has not begun (UTC).
export const isEarly = (day, now = Date.now()) => Boolean(day) && day > todayUTC(now);

// now(): the clock (tests). dates(): every feature's day; early(): those still early; set(key, day | null): a new
// day, or null to open it today.
export function createEarly({ s3, features = FEATURES, now = () => Date.now(), log = () => {} }) {
  let cache = null, cachedAt = 0;
  async function read(fresh) {
    if (!fresh && cache && now() - cachedAt < 60000) return cache;
    let stored;
    try {
      const buf = await s3.getBuffer(KEY);
      stored = buf ? JSON.parse(buf.toString()) : {};
    } catch (err) { log(`early access: the days not read (${err.message}): every feature counts as open`); return null; }
    const unseen = features.filter((f) => !validDay(stored[f.key]));
    if (unseen.length) {
      for (const f of unseen) stored[f.key] = addDays(todayUTC(now()), EARLY_DAYS);
      await s3.put(KEY, Buffer.from(JSON.stringify(stored)), { type: 'application/json' });
      log(`early access: ${unseen.map((f) => f.key).join(', ')} for Club Legends until ${addDays(todayUTC(now()), EARLY_DAYS)}`);
    }
    cache = stored;
    cachedAt = now();
    return stored;
  }
  const known = (stored) => Object.fromEntries(features.filter((f) => stored?.[f.key]).map((f) => [f.key, stored[f.key]]));
  return {
    features,
    async dates(fresh = false) { return known(await read(fresh)); },
    async early(fresh = false) { return Object.fromEntries(Object.entries(known(await read(fresh))).filter(([, d]) => isEarly(d, now()))); },
    async set(key, day) {
      if (!features.some((f) => f.key === key)) throw Object.assign(new Error('no such feature'), { status: 404 });
      if (day !== null && !validDay(day)) throw Object.assign(new Error('day: a day YYYY-MM-DD, or null to open it now'), { status: 400 });
      const stored = await read(true);
      if (!stored) throw Object.assign(new Error('the store did not answer'), { status: 503 });
      stored[key] = day ?? todayUTC(now());
      await s3.put(KEY, Buffer.from(JSON.stringify(stored)), { type: 'application/json' });
      cache = stored;
      cachedAt = now();
      log(`early access: ${key} ${day ? `for Club Legends until ${day}` : 'open to everyone now'}`);
      return known(stored);
    },
  };
}
