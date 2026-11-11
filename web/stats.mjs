// Usage statistics and error reports (web/README.md, "Statistics and the admin panel").
//
// The page sends a heartbeat each minute while the game runs (stats.js): the seconds played since the last one (the tab
// visible, an input in the last 5 minutes), and what its user folder holds (saves, older copies, data sets). A person
// is an account (signed in with Google: "g:" and Google's id) or, without one, a browser: "a:" and a random id that
// the page keeps in localStorage. No IP address and no name of an anonymous player is kept. When a browser signs in,
// its anonymous record points at the account, and the account lists the browser.
//
// Everything lives in one object of the store (stats/stats.json), kept in memory and written every FLUSH ms when it
// changed and when the server stops. Fly may run two machines of the app, so a write first reads the object again
// and adds this machine's changes since its last read to what is there (merge): the counts of both machines stay.
// Error reports go to reports/<day>/<time>-<random>.json, and the last ones stay listed in the object.

const KEY = 'stats/stats.json';
const FLUSH = 120000;
const KEEP_DAYS = 400;              // daily totals and people idle this long go (the privacy page says 13 months)
const KEEP_REPORTS = 200;           // reports listed in the object
const REPORT_DAYS = 90;             // report files older than this go (the privacy page says so)
const MAX_BEAT_SECONDS = 180;       // a beat counts at most this much play: the page sends one each minute
const ANON = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/;

const day = (t) => new Date(t).toISOString().slice(0, 10);
const count = (v) => {
  const n = Math.floor(Number(v?.n)), b = Math.floor(Number(v?.bytes));
  return { n: Number.isFinite(n) && n >= 0 ? Math.min(n, 1e6) : 0, bytes: Number.isFinite(b) && b >= 0 ? Math.min(b, 1e13) : 0 };
};
const text = (v, max) => String(v ?? '').replace(/[\u0000-\u001f]/g, ' ').slice(0, max);
const clone = (v) => JSON.parse(JSON.stringify(v));
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
const PERSON_COUNTS = ['seconds', 'sessions', 'days', 'reports'];
const DAY_COUNTS = ['seconds', 'sessions', 'new', 'reports'];

// The object as the store holds it now (theirs), with this machine's changes since it last read it (from base to
// mine) added. A count adds this machine's part to theirs; any other field of a person comes from the record that saw
// the person last. What this machine removed (an account deleted, a person or a day pruned) goes.
export function mergeStats(base, mine, theirs) {
  const out = clone(theirs);
  out.people ??= {};
  out.days ??= {};
  const notNew = [];                         // new here, but the other machine had them first: counted new once
  for (const id of new Set([...Object.keys(mine.people), ...Object.keys(base.people ?? {})])) {
    const m = mine.people[id], b = base.people?.[id], t = out.people[id];
    if (!m) { delete out.people[id]; continue; }
    if (b && same(m, b)) continue;                          // not changed here: theirs as it is
    if (!t) { out.people[id] = m; continue; }
    if (!b) notNew.push(day(m.first));
    const p = (m.last ?? 0) >= (t.last ?? 0) ? { ...t, ...m } : { ...m, ...t };
    for (const f of PERSON_COUNTS) p[f] = (t[f] ?? 0) + (m[f] ?? 0) - (b?.[f] ?? 0);
    p.first = Math.min(t.first ?? m.first, m.first ?? t.first);
    if (m.devices || t.devices) p.devices = [...new Set([...(t.devices ?? []), ...(m.devices ?? [])])].slice(-20);
    const account = m.account !== b?.account ? m.account : t.account;
    if (account) p.account = account; else delete p.account;
    out.people[id] = p;
  }
  for (const k of new Set([...Object.keys(mine.days), ...Object.keys(base.days ?? {})])) {
    const m = mine.days[k], b = base.days?.[k], t = out.days[k];
    if (!m) { delete out.days[k]; continue; }
    const d = { ...(t ?? {}), active: Math.max(t?.active ?? 0, m.active ?? 0) };
    for (const f of DAY_COUNTS) d[f] = (t?.[f] ?? 0) + (m[f] ?? 0) - (b?.[f] ?? 0);
    out.days[k] = d;
  }
  for (const k of notNew) if (out.days[k]?.new > 0) out.days[k].new--;
  const today = [mine.today, out.today].filter((x) => x?.day).sort((a, b) => (a.day < b.day ? 1 : -1))[0] ?? mine.today;
  out.today = { day: today.day, ids: [...new Set([...(mine.today?.day === today.day ? mine.today.ids : []),
    ...(out.today?.day === today.day ? out.today.ids : [])])] };
  const kept = new Set((mine.reports ?? []).map((r) => r.key));
  const seen = new Set((base.reports ?? []).map((r) => r.key).filter((k) => !kept.has(k)));   // removed here (90 days)
  out.reports = [...(mine.reports ?? []), ...(out.reports ?? [])].filter((r) => !seen.has(r.key) && seen.add(r.key))
    .sort((a, b) => (a.at < b.at ? 1 : -1)).slice(0, KEEP_REPORTS);
  out.version = 1;
  return out;
}

// The browser and the system of a User-Agent, in a few words: no version past the major one, no device model.
export function browserOf(ua = '') {
  const s = String(ua);
  const os = /iPad/.test(s) ? 'iPad' : /iPhone/.test(s) ? 'iPhone' : /Android/.test(s) ? 'Android' : /Mac OS X|Macintosh/.test(s)
    ? (/Mobile\//.test(s) && /Safari/.test(s) && !/Chrome|CriOS/.test(s) ? 'iPad' : 'Mac') : /Windows/.test(s) ? 'Windows' : /CrOS/.test(s) ? 'ChromeOS'
      : /Linux/.test(s) ? 'Linux' : 'other';
  const m = (re) => s.match(re)?.[1];
  const name = m(/Edg\/(\d+)/) ? `Edge ${m(/Edg\/(\d+)/)}` : m(/OPR\/(\d+)/) ? `Opera ${m(/OPR\/(\d+)/)}` : m(/Firefox\/(\d+)/) ? `Firefox ${m(/Firefox\/(\d+)/)}`
    : m(/(?:Chrome|CriOS)\/(\d+)/) ? `Chrome ${m(/(?:Chrome|CriOS)\/(\d+)/)}` : m(/Version\/(\d+)[.\d]* .*Safari/) ? `Safari ${m(/Version\/(\d+)[.\d]* .*Safari/)}` : 'other';
  return `${name} · ${os}`;
}

// store: { get(key) -> Buffer|null, put(key, Buffer) } or null (in memory only). now(): the time (tests).
export function createStats({ store = null, log = (t) => console.log(t), now = () => Date.now(), flushMs = FLUSH } = {}) {
  let data = { version: 1, people: {}, days: {}, today: { day: day(now()), ids: [] }, reports: [] };
  let dirty = false, timer = null, prunedReports = 0;
  let base = clone(data);                    // the object as this machine last read or wrote it (mergeStats)
  const todayIds = new Set();

  const ready = (async () => {
    if (!store) return;
    try {
      const buf = await store.get(KEY);
      if (buf) {
        const d = JSON.parse(buf.toString());
        data = { ...data, ...d };
        base = clone(data);
        if (data.today?.day === day(now())) for (const id of data.today.ids ?? []) todayIds.add(id);
      }
    } catch (e) { log(`stats: not read: ${e.message}`); }
  })();

  const dayOf = (k) => (data.days[k] ??= { active: 0, seconds: 0, sessions: 0, new: 0, reports: 0 });

  // The day turned: yesterday's count of active people stays, today's list starts again.
  function rollDay(t) {
    const k = day(t);
    if (data.today.day === k) return;
    if (data.today.day) dayOf(data.today.day).active = todayIds.size;
    todayIds.clear();
    data.today = { day: k, ids: [] };
  }

  function person(id, t, fields) {
    let p = data.people[id];
    if (!p) {
      // saves, older and sets come with the first beat that reports the folder (the admin page reads them as 0 till then)
      p = data.people[id] = { id, first: t, last: t, seconds: 0, sessions: 0, days: 0, lastDay: null, reports: 0 };
      dayOf(day(t)).new++;
    }
    Object.assign(p, fields);
    return p;
  }

  // One heartbeat. user: { sub, email, name } when signed in, else null. Returns null, or why it was not taken.
  async function beat(body, user, ua = '') {
    await ready;
    const anon = String(body?.anon ?? '');
    if (!ANON.test(anon)) return 'no browser id';
    const t = now();
    rollDay(t);
    const id = user ? `g:${user.sub}` : `a:${anon}`;
    const p = person(id, t, user ? { kind: 'account', email: text(user.email, 200), name: text(user.name, 100) } : { kind: 'anon' });
    const k = day(t);
    let seconds = Math.floor(Number(body.seconds));
    seconds = Number.isFinite(seconds) && seconds > 0 ? Math.min(seconds, MAX_BEAT_SECONDS) : 0;
    const session = text(body.session, 40);
    if (session && session !== p.session) { p.session = session; p.sessions++; dayOf(k).sessions++; }
    if (p.lastDay !== k) { p.lastDay = k; p.days++; }
    p.last = t;
    p.seconds += seconds;
    dayOf(k).seconds += seconds;
    if (body.state) { p.saves = count(body.state.saves); p.older = count(body.state.older); p.sets = count(body.state.sets); }
    if (ua) p.browser = browserOf(ua);
    p.engine = text(body.engine, 40);
    p.vault = Boolean(body.vault);
    if (user) {
      p.cloud = Boolean(body.cloud);
      p.devices = [...new Set([...(p.devices ?? []), `a:${anon}`])].slice(-20);
      const a = data.people[`a:${anon}`];
      if (a && a.account !== id) a.account = id;          // the browser's play before the sign-in belongs to the account
    }
    if (!todayIds.has(id)) { todayIds.add(id); data.today.ids.push(id); }
    dirty = true;
    return null;
  }

  // An error report of the page: kept as a file of its own; the last ones are listed.
  async function report(body, user, ua = '') {
    await ready;
    const anon = String(body?.anon ?? '');
    if (!ANON.test(anon)) return { error: 'no browser id' };
    const t = now();
    rollDay(t);
    const id = user ? `g:${user.sub}` : `a:${anon}`;
    const r = {
      at: new Date(t).toISOString(), id, message: text(body.message, 500), engine: text(body.engine, 40), browser: browserOf(ua),
      where: text(body.where, 200), log: (Array.isArray(body.log) ? body.log : []).slice(-60).map((l) => text(l, 400)),
    };
    const key = `reports/${day(t)}/${t}-${Math.random().toString(36).slice(2, 8)}.json`;
    if (store) {
      try { await store.put(key, Buffer.from(JSON.stringify(r, null, 1))); } catch (e) { log(`stats: report not written: ${e.message}`); }
    }
    data.reports.unshift({ at: r.at, id, message: r.message, browser: r.browser, engine: r.engine, key });
    data.reports.length = Math.min(data.reports.length, KEEP_REPORTS);
    dayOf(day(t)).reports++;
    if (data.people[id]) data.people[id].reports = (data.people[id].reports ?? 0) + 1;
    dirty = true;
    return { key };
  }

  async function reportFile(key) {
    if (!/^reports\/\d{4}-\d{2}-\d{2}\/\d+-[a-z0-9]+\.json$/.test(key) || !store) return null;
    const buf = await store.get(key);
    return buf ? JSON.parse(buf.toString()) : null;
  }

  // An account that was deleted: its record goes, and its browsers no longer point at it.
  async function forget(sub) {
    await ready;
    const id = `g:${sub}`;
    delete data.people[id];
    for (const p of Object.values(data.people)) if (p.account === id) delete p.account;
    dirty = true;
  }

  function prune(t) {
    const oldest = t - KEEP_DAYS * 86400000, oldestDay = day(oldest);
    for (const [id, p] of Object.entries(data.people)) if (p.last < oldest) delete data.people[id];
    for (const k of Object.keys(data.days)) if (k < oldestDay) delete data.days[k];
  }

  // Report files older than REPORT_DAYS go, once a day.
  async function pruneReports(t) {
    if (!store?.list || !store?.delete || t - prunedReports < 86400000) return 0;
    prunedReports = t;
    const oldest = day(t - REPORT_DAYS * 86400000);
    let n = 0;
    try {
      for (const o of await store.list('reports/')) {
        const d = /^reports\/(\d{4}-\d{2}-\d{2})\//.exec(o.key)?.[1];
        if (d && d < oldest) { await store.delete(o.key); n++; }
      }
    } catch (e) { log(`stats: old reports not removed: ${e.message}`); }
    data.reports = data.reports.filter((r) => r.at.slice(0, 10) >= oldest);
    return n;
  }

  async function flush() {
    await pruneReports(now());
    if (!dirty || !store) return false;
    const t = now();
    rollDay(t);
    prune(t);
    data.today.ids = [...todayIds];
    dirty = false;
    let theirs = null;
    try {
      const buf = await store.get(KEY);
      theirs = buf ? JSON.parse(buf.toString()) : null;
    } catch (e) { dirty = true; log(`stats: not read before the write: ${e.message}`); return false; }
    // From here to the put nothing waits, so a beat that comes meanwhile lands in the merged object.
    const merged = theirs ? mergeStats(base, data, theirs) : data;
    data = merged;
    base = clone(merged);
    todayIds.clear();
    if (data.today?.day === day(now())) for (const id of data.today.ids ?? []) todayIds.add(id);
    try { await store.put(KEY, Buffer.from(JSON.stringify(merged))); return true; } catch (e) {
      base = theirs ? clone(theirs) : clone({ ...merged, people: {}, days: {}, reports: [] });   // the changes count again next time
      dirty = true;
      log(`stats: not written: ${e.message}`);
      return false;
    }
  }

  // What the admin page shows: totals, the days, the people (with their linked browsers) and the last reports.
  async function summary() {
    await ready;
    const t = now();
    rollDay(t);
    const people = Object.values(data.people);
    const persons = people.filter((p) => !p.account);    // a browser that signed in counts with its account
    const since = (days) => persons.filter((p) => p.last >= t - days * 86400000).length;
    const sum = (f) => persons.reduce((a, p) => a + f(p), 0);
    const days = Object.entries(data.days).sort(([a], [b]) => (a < b ? -1 : 1)).slice(-90)
      .map(([k, v]) => ({ day: k, ...v, active: k === data.today.day ? todayIds.size : v.active }));
    if (!days.some((d) => d.day === data.today.day)) days.push({ day: data.today.day, active: todayIds.size, seconds: 0, sessions: 0, new: 0, reports: 0 });
    return {
      at: new Date(t).toISOString(),
      totals: {
        people: persons.length, accounts: persons.filter((p) => p.kind === 'account').length, anonymous: persons.filter((p) => p.kind === 'anon').length,
        cloud: persons.filter((p) => p.cloud).length,
        activeToday: todayIds.size, active7: since(7), active30: since(30),
        // hours: every record, so a browser's play before it signed in counts too
        hours: people.reduce((a, p) => a + p.seconds, 0) / 3600, saves: sum((p) => p.saves?.n ?? 0), savesBytes: sum((p) => p.saves?.bytes ?? 0),
        older: sum((p) => p.older?.n ?? 0), olderBytes: sum((p) => p.older?.bytes ?? 0), sets: sum((p) => p.sets?.n ?? 0),
        setsBytes: sum((p) => p.sets?.bytes ?? 0), reports7: data.reports.filter((r) => Date.parse(r.at) >= t - 7 * 86400000).length,
      },
      days,
      people: people.sort((a, b) => b.last - a.last),
      reports: data.reports.slice(0, 100),
    };
  }

  function start() {
    if (!store || timer) return;
    timer = setInterval(() => { flush(); }, flushMs);
    timer.unref?.();
  }
  async function stop() { if (timer) clearInterval(timer); timer = null; await flush(); }

  return { ready, beat, report, reportFile, forget, flush, summary, start, stop, get data() { return data; } };
}

// The store of an S3 client (s3.mjs): getBuffer and put.
export function statsStore(s3) {
  if (!s3) return null;
  return { get: (key) => s3.getBuffer(key), put: (key, buf) => s3.put(key, buf, { type: 'application/json' }),
    list: (prefix) => s3.list(prefix), delete: (key) => s3.delete(key) };
}
