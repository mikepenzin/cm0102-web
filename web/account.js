// The account and the cloud saves on the page (web/README.md, "Cloud saves"). The server (cloud.mjs) signs in with
// Google and keeps the saves of an account in its store; this file shows the account, and keeps the saves of the user
// folder and the cloud the same:
//
// - Before the game starts, the saves made on other devices come into the user folder, so Load Game lists them.
// - After each save, the new or changed save goes up.
// - A save changed on both sides since they were last the same (two devices at once) is kept twice, nothing is lost:
//   the other device's version under "<name> (other device).sav", this device's under its own name, in both places.
// - A save removed from the cloud (in Saved games) stays in this browser and does not go up again, unless it changes.
// - While the game runs, a save of the user folder is never replaced from the cloud: only new ones come in.
//
// What was last the same is kept in the user folder itself (.cloud-sync.json): per save, the cloud's ETag and the size
// and time of the local file at that moment.
import { SAVE_DIR, listSaves, readBlobNow, saveName, storeSaveStream, storedFirst } from './saves.js';
import { downDirect, upDirect } from './direct.js';
import { toDisk } from './disk.js';
import { language, onLanguage, priceText, t } from './i18n.js';

const STATE = `${SAVE_DIR}/.cloud-sync.json`;
const CHANGE = { 'x-cm': '1' };                   // what the server wants on a change (cloud.mjs)
const $ = (id) => document.getElementById(id);
const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const at = (name) => `/api/saves/${encodeURIComponent(name)}`;
// Dates in the page's language, or the browser's own format when the page is in English.
const dateLocale = () => (language() === 'en' ? undefined : language());
const when = (time) => new Date(time).toLocaleString(dateLocale(), { dateStyle: 'medium', timeStyle: 'short' });

// The account as the server sees it: { auth: false } (no accounts on this server), { auth: true, user: null } (signed
// out), or the user; null when the server could not be asked (no connection).
export async function fetchMe(timeoutMs = 8000) {
  try {
    const r = await fetch('/api/me', { cache: 'no-store', signal: AbortSignal.timeout(timeoutMs) });
    return r.ok ? await r.json() : null;
  } catch {
    return null;
  }
}

const PAUSES = [3000, 10000];   // the pauses before a request of the cloud saves is tried again
// A save packed for the cloud, and a cloud save unpacked: through a file on the browser's disk (disk.js), not whole in the
// page's memory. { blob, done, onDisk }; done() once the bytes are kept elsewhere.
const gzip = (bytes) => toDisk(new Blob([bytes]).stream().pipeThrough(new CompressionStream('gzip')));

// Cloud saves that this device does not bring down by itself: the last try closed the tab (an iPhone, 18 November 2026:
// iOS closed it while a 446 MB save came at the start, and again at each start after). The next start leaves such a
// save in the cloud (loader.js, from the sudden-stop mark of lastvisit.js), and the Account dialog has it with "Get it
// here". { name: since (ms) } in localStorage, for this browser.
const KEPT_KEY = 'cm0102-cloud-kept';
export function keptInCloud() { try { return JSON.parse(localStorage.getItem(KEPT_KEY) ?? '{}') ?? {}; } catch { return {}; } }
export function keepInCloud(name, on = true) {
  const k = keptInCloud();
  if (on) k[name] = Date.now(); else delete k[name];
  try { localStorage.setItem(KEPT_KEY, JSON.stringify(k)); } catch { /* not kept: it comes down as before */ }
}

// A name for the other device's version of name, free among the names taken (lower case).
export function otherDeviceName(name, taken) {
  return saveName(`${name.slice(0, -4)} (other device).sav`, taken);
}

// A large save goes up at most every BIG_EVERY_MS, and when the tab is left (force): a player who saves a game of
// 690 MB every few game days would otherwise send it again at each save (about 300 MB packed, minutes on a home
// connection), and the sync of the others would wait behind it. Its first upload goes at once.
export const BIG_SAVE = 100 << 20;
export const BIG_EVERY_MS = 10 * 60000;

// The plan of one sync: what to do with each save, from the local files, the cloud's list and the state. A save changed
// both here and in the cloud: the one here goes up over the cloud's when it is the newer (its time against the cloud's
// saved); else both are kept, the cloud's as "… (other device)".
// local: Map name -> { size, mtime }, cloud: Map name -> { etag, rawSize, saved }, state: name -> { etag, size, mtime,
// removed, upAt, sending }. held: the large saves that wait (BIG_EVERY_MS). Each job has why, for the log.
//
// A cloud copy that this device sent itself is not a change made elsewhere: an upload can go through after its tab
// closed (a save sent when the tab is left), and the page then never records its etag. The next start took the cloud's
// new etag for another device's save and brought it down as "… (other device)", at every reload (an iPhone, 21 November
// 2026). The copy is this device's when its length and save time are those of the file here, or of the version this
// device was sending (state.sending, written before an upload starts).
export function planSync(local, cloud, state, { replace = true, now = Date.now(), force = false } = {}) {
  const jobs = [], settle = {}, forget = [], held = [];
  for (const name of new Set([...local.keys(), ...cloud.keys(), ...Object.keys(state)])) {
    const l = local.get(name), c = cloud.get(name), e = state[name];
    const localChanged = Boolean(l) && (!e || e.size !== l.size || e.mtime !== l.mtime);
    // The same length and the same save time to the millisecond (the upload names the file's own mtime as saved).
    const isCopyOf = (v) => Boolean(v && c) && c.rawSize === v.size && c.saved === v.mtime;
    if (!l && !c) { forget.push(name); continue; }
    if (!c) {                                                     // only here
      if (e && !localChanged) { if (!e.removed) settle[name] = { ...e, removed: true }; continue; }   // removed from the cloud
      jobs.push({ do: 'up', name, base: 'none', why: 'only here' });
      continue;
    }
    if (!l) { jobs.push({ do: 'down', name, why: 'only in the cloud' }); continue; }        // a new save from elsewhere
    if (!e) {
      // Both, never synced here (the first sign-in on a device with a copy): the same save when its length and time agree.
      if (c.rawSize === l.size && Math.abs((c.saved ?? 0) - l.mtime) < 2000) settle[name] = { etag: c.etag, size: l.size, mtime: l.mtime };
      else if (l.mtime > (c.saved ?? 0)) jobs.push({ do: 'up', name, base: c.etag, why: 'here and in the cloud, never synced on this device; the save here is the newer' });
      else jobs.push({ do: 'both', name, cloud: c, why: 'here and in the cloud, never synced on this device; the cloud\'s is the newer' });
      continue;
    }
    const cloudChanged = c.etag !== e.etag;
    if (cloudChanged && isCopyOf(l)) { settle[name] = { etag: c.etag, size: l.size, mtime: l.mtime }; continue; }
    if (cloudChanged && isCopyOf(e.sending)) {                    // this device's upload; the save here changed since
      jobs.push({ do: 'up', name, base: c.etag, why: 'changed here since this device sent the cloud\'s copy' });
      continue;
    }
    if (!localChanged && !cloudChanged) continue;
    if (!localChanged) { if (replace) jobs.push({ do: 'down', name, why: 'changed in the cloud, not here' }); continue; }
    if (!cloudChanged) {
      if (!force && l.size >= BIG_SAVE && e.upAt && now >= e.upAt && now - e.upAt < BIG_EVERY_MS) { held.push(name); continue; }
      jobs.push({ do: 'up', name, base: e.etag, why: 'changed here' });
      continue;
    }
    // Changed on both sides: the newer save wins when it is the one here (it goes up over the cloud's; the store keeps
    // the cloud's earlier versions). When the cloud's is the newer, both are kept, so the game played here is not lost.
    if (l.mtime > (c.saved ?? 0)) { jobs.push({ do: 'up', name, base: c.etag, why: 'changed here and in the cloud; the save here is the newer' }); continue; }
    jobs.push({ do: 'both', name, cloud: c, why: 'changed here and in the cloud; the cloud\'s is the newer' });
  }
  return { jobs, settle, forget, held };
}

export function createCloudSync({ FS, sync, log, toast, account, onWrote = () => {} }) {
  let state = { account, saves: {} };
  try {
    const kept = JSON.parse(FS.readFile(STATE, { encoding: 'utf8' }));
    if (kept.account === account && kept.saves) state = kept;
  } catch { /* the first sync on this browser */ }
  let chain = Promise.resolve(), busy = new Set(), held = new Set(), stopped = false, full = false, last = null, count = null, max = null;
  let limits = { packed: 0, raw: 0 };            // what the cloud takes of one save (the saves list), 0: not told
  const listeners = new Set();
  const changed = () => { for (const f of listeners) f(); };
  const keep = () => { FS.writeFile(STATE, JSON.stringify(state)); sync(); };
  const inVault = (name) => { try { return Boolean(FS.lookupPath(`${SAVE_DIR}/${name}`).node?.vault); } catch { return false; } };
  const stat = (name) => {
    const st = FS.stat(`${SAVE_DIR}/${name}`);
    return { size: st.size, mtime: +new Date(st.mtime) };
  };
  // An answer of 500 and up (the server, or the cloud store behind it, did not take it) can be tried again.
  const fail = (what, r) => Object.assign(new Error(`${what}: HTTP ${r.status}`), { again: r.status >= 500 });

  // A request that the server or the store did not take, or that lost its connection, is tried twice more after a
  // pause; then the job waits for the next sync (each minute: loader.js).
  async function retrying(what, request, signal, pauses = PAUSES) {
    for (let i = 0; ; i++) {
      try {
        const r = await request();
        if (r.status >= 500 && i < pauses.length) throw fail(what, r);
        return r;
      } catch (err) {
        const again = err.again || err instanceof TypeError;    // TypeError: fetch lost the connection
        if (!again || i >= pauses.length || signal?.aborted) throw err;
        log(`page: cloud saves: ${err.message}; again in ${pauses[i] / 1000} s`);
        await new Promise((ok, no) => {
          const t = setTimeout(ok, pauses[i]);
          signal?.addEventListener('abort', () => { clearTimeout(t); no(signal.reason); }, { once: true });
        });
      }
    }
  }

  // A save the cloud does not take for its size: it stays here, the sync goes on with the others, and it is not sent
  // again until it changes (state: tooBig with its size and time; planSync then leaves it). A player's log had a 413
  // that stopped the whole sync, so the saves after it never went up, and the sync, failed as a whole, packed and sent
  // that save again each minute (15 November 2026).
  // why: the English, for the log; shown: the same in the page's language (a reason from the server stays as it came).
  function tooBig(name, before, why, shown = why) {
    const e = state.saves[name];
    if (!(e?.tooBig && e.size === before.size && e.mtime === before.mtime)) {
      toast(t('{name} is too large for Cloud Saves ({why}): it stays in this browser. Download it in Saved games to keep a copy.', { name, why: shown }), { kind: 'bad', ms: 12000 });
      log(`page: cloud saves: ${name} is too large for the cloud (${why}): it stays here`);
    }
    state.saves[name] = { etag: e?.etag ?? null, size: before.size, mtime: before.mtime, tooBig: true };
    return { tooBig: true };
  }

  async function up(name, base, signal) {
    await storedFirst(FS, `${SAVE_DIR}/${name}`);              // a save just written: read the stored copy, not memory
    const before = stat(name);
    let bytes = readBlobNow(FS, `${SAVE_DIR}/${name}`);       // with the stat above: the same version of the file
    if (bytes instanceof Promise) bytes = await bytes;          // a placeholder: from the browser's storage
    const rawSize = bytes.size ?? bytes.length;
    const mb = (n) => `${(n / 1048576).toFixed(0)} MB`;
    if (limits.raw && rawSize > limits.raw) {
      return tooBig(name, before, `${mb(rawSize)}; the cloud takes ${mb(limits.raw)}`, t('{size}; the cloud takes {most}', { size: mb(rawSize), most: mb(limits.raw) }));
    }
    // The version going up, kept before it goes: if the tab closes after the store took it but before its answer came, the
    // next start knows the cloud's copy for this device's own (planSync), not another device's.
    state.saves[name] = { ...(state.saves[name] ?? {}), sending: { size: rawSize, mtime: before.mtime } };
    keep();
    const packed = await gzip(bytes), body = packed.blob;       // a Blob: the same body for each try
    bytes = null;
    try { return await send(name, base, signal, before, body, rawSize); } finally { await packed.done(); }
  }
  async function send(name, base, signal, before, body, rawSize) {
    const mb = (n) => `${(n / 1048576).toFixed(0)} MB`;
    if (limits.packed && body.size > limits.packed) {
      return tooBig(name, before, `${mb(body.size)} packed; the cloud takes ${mb(limits.packed)}`, t('{size} packed; the cloud takes {most}', { size: mb(body.size), most: mb(limits.packed) }));
    }
    // Straight to the store (direct.js), or through the server when that is off.
    let way = 'through the server';                              // for the log: which way the file went
    const r = await retrying(`${name} up`, async () => {
      const d = await upDirect({ url: at(name), body, base, signal, log, fields: { rawSize, saved: before.mtime } });
      way = d ? 'straight to the store' : 'through the server';
      return d ?? fetch(at(name), { method: 'PUT', body, signal, headers: { ...CHANGE, 'x-base-etag': base, 'x-save-time': String(before.mtime) } });
    }, signal);
    if (r.status === 409) return { conflict: (await r.json()).cloud };
    if (r.status === 413 || r.status === 422) {                 // the server's own word on its size (an older page's limit)
      const why = (await r.json().catch(() => ({}))).error ?? `HTTP ${r.status}`;
      if (r.status === 413 || /larger than/.test(why)) return tooBig(name, before, why);
    }
    if (r.status === 507) {
      if (!full) toast((await r.json()).error, { kind: 'bad', ms: 8000 });
      full = true;
      return {};
    }
    if (r.status === 401 || r.status === 403) { stopped = true; return {}; }
    if (!r.ok) throw fail(`${name} up`, r);
    const j = await r.json();
    state.saves[name] = { etag: j.etag, size: rawSize, mtime: before.mtime, upAt: Date.now() };
    log(`page: ${name} is in the cloud (${(j.size / 1048576).toFixed(1)} MB compressed, ${way})`);
    return { up: true };
  }

  // The cloud's save into the user folder as file (its own name, or another one).
  async function down(name, file, signal) {
    let link = null;                                            // the store's link, its ETag and time (direct.js)
    const r = await retrying(`${name} down`, async () => {
      link = await downDirect({ url: at(name), signal, log });
      return link?.response ?? fetch(at(name), { cache: 'no-store', signal });
    }, signal);
    if (!r.ok) throw fail(`${name} down`, r);
    const etag = link?.etag ?? r.headers.get('etag'), saved = Number(link?.saved ?? r.headers.get('x-saved')) || Date.now();
    // Unpacked as it comes and stored in parts (storeSaveStream, vault.adoptStream): never whole in the page's memory. An
    // iPhone kept the whole save in memory, and iOS closed the tab at each start (18 November 2026).
    const why = await storeSaveStream(FS, file, r.body.pipeThrough(new DecompressionStream('gzip')), { mtime: saved });
    if (why) { log(`page: ${name} from the cloud was not taken: ${why}`); return null; }
    log(`page: ${name} came from the cloud (${(stat(file).size / 1048576).toFixed(1)} MB, ${link?.response === r ? 'straight from the store' : 'through the server'}, ${inVault(file) ? 'into the browser\'s storage in parts' : 'unpacked whole'})`);
    onWrote();
    return { etag, saved, ...stat(file) };
  }

  // Changed on both sides: the cloud's version comes in under another name and goes up under it, then this device's
  // version goes up under the name.
  async function keepBoth(name, cloudNow, cloudNames, signal) {
    const taken = new Set([...listSaves(FS).saves.map((f) => f.name), ...Object.keys(state.saves), ...cloudNames].map((n) => n.toLowerCase()));
    const other = otherDeviceName(name, taken);
    const theirs = await down(name, other, signal);
    if (!theirs) return;
    state.saves[other] = { etag: null, size: theirs.size, mtime: theirs.mtime };
    const o = await up(other, 'none', signal);
    if (o.conflict) return;                       // that name came up elsewhere meanwhile: the next sync sorts it out
    const mine = await up(name, theirs.etag ?? cloudNow?.etag ?? 'none', signal);
    if (mine.up) toast(t('{name} was also saved on another device. That version is kept as {other}.', { name, other }), { ms: 9000 });
    log(`page: ${name} changed here and elsewhere: the other version is ${other}`);
  }

  // One sync with the cloud. replace: a save of the user folder may be replaced by a newer one of the cloud (not
  // while the game runs). onStep(text) tells what it does.
  async function once({ replace, signal, force = false, onStep = () => {} }) {
    if (stopped) return;
    const r = await fetch('/api/saves', { cache: 'no-store', signal });
    if (r.status === 401 || r.status === 403) { stopped = true; changed(); return; }
    if (!r.ok) throw fail('the cloud list', r);
    const { saves, maxSaves, store, maxPacked = 0, maxRaw = 0 } = await r.json();
    limits = { packed: maxPacked, raw: maxRaw };
    // Another store than the last sync's (its bucket changed): what went up before is not there, and it goes up again
    // rather than count as removed from the cloud ("Only here").
    if (store && state.store !== store) {
      if (state.store) log('page: cloud saves: the cloud store is another one: the saves here go up to it again');
      state.saves = {};                         // each save as on a first sign-in: the same in both places, or up
      state.store = store;
    }
    count = saves.length;
    max = maxSaves;
    const local = new Map(listSaves(FS).saves.map((f) => [f.name, { size: f.size, mtime: +f.mtime }]));
    const cloud = new Map(saves.map((s) => [s.name, s]));
    const plan = planSync(local, cloud, state.saves, { replace, force });
    for (const job of plan.jobs) log(`page: cloud saves: ${job.name}: ${job.do === 'both' ? 'kept twice' : job.do} (${job.why})`);
    for (const n of Object.keys(plan.settle)) if (state.saves[n]?.etag !== plan.settle[n].etag && !plan.settle[n].removed) log(`page: cloud saves: ${n}: the cloud has this very file (sent from here), nothing to move`);
    held = new Set(plan.held);
    Object.assign(state.saves, plan.settle);
    for (const n of plan.forget) delete state.saves[n];
    let i = 0;
    const failed = [], kept = [], keptHere = keptInCloud();
    for (const job of plan.jobs) {
      i++;
      if (full && job.do === 'up' && job.base === 'none') continue;
      if (job.do !== 'up' && keptHere[job.name]) { kept.push(job.name); continue; }   // left in the cloud on this device
      busy.add(job.name);
      changed();
      const step = { name: job.name, i, n: plan.jobs.length };
      onStep(job.do === 'down' ? t('Getting {name} ({i} of {n})', step) : t('Saving {name} ({i} of {n})', step), job);
      try {
        if (job.do === 'down') {
          const got = await down(job.name, job.name, signal);
          if (got) state.saves[job.name] = { etag: got.etag, size: got.size, mtime: got.mtime };
        } else if (job.do === 'up') {
          const u = await up(job.name, job.base, signal);
          if (u.conflict) await keepBoth(job.name, u.conflict, cloud.keys(), signal);
        } else {
          await keepBoth(job.name, job.cloud, cloud.keys(), signal);
        }
      } catch (err) {
        // One save that did not go (the server or the store failed it, after the tries): the others still go, and it
        // waits for the next sync. A lost connection or a stop ends the sync.
        if (err.name === 'AbortError' || err.name === 'TimeoutError' || err instanceof TypeError || !err.again) throw err;
        log(`page: cloud saves: ${err.message}; it goes at the next sync`);
        failed.push(job.name);
      } finally {
        busy.delete(job.name);
        keep();
        changed();
      }
      if (stopped) break;
    }
    if (plan.jobs.length || Object.keys(plan.settle).length || plan.forget.length) keep();
    last = Date.now();
    if (plan.jobs.some((j) => j.do !== 'down')) count = null;     // the count changed; the next list tells it
    changed();
    // failed: the names that go at the next sync; held: the large saves that wait their time (BIG_EVERY_MS); kept: the
    // cloud saves left in the cloud on this device (keepInCloud)
    return Object.assign(plan.jobs.filter((j) => !kept.includes(j.name)), { failed, held: plan.held, kept });
  }

  // Runs one sync after the ones before it. Resolves to the jobs (with failed: the saves that wait for the next sync),
  // or null when it failed as a whole (no connection, or the cloud's list not read).
  function run(opts = {}) {
    const job = chain.then(() => once(opts)).then((jobs) => jobs ?? Object.assign([], { failed: [], held: [], kept: [] })).catch((err) => {
      if (err.name === 'AbortError' || err.name === 'TimeoutError') return null;
      log(`page: cloud saves: ${err.message ?? err}`);
      return null;
    });
    chain = job;
    return job;
  }

  return {
    run,
    stop() { stopped = true; changed(); },
    get stopped() { return stopped; },
    onChange(f) { listeners.add(f); return () => listeners.delete(f); },
    info: () => ({ last, count, max, full }),
    // How a save of the user folder stands: 'cloud', 'up' (on its way), 'later' (a large save that waits its time),
    // 'here' (removed from the cloud), 'big' (too large for the cloud) or 'waiting'.
    status(name, file) {
      if (busy.has(name)) return 'up';
      if (held.has(name)) return 'later';
      const e = state.saves[name];
      if (!e) return 'waiting';
      if (e.tooBig && e.size === file.size && e.mtime === +file.mtime) return 'big';
      if (e.removed) return 'here';
      return e.size === file.size && e.mtime === +file.mtime ? 'cloud' : 'waiting';
    },
    // Whether the cloud keeps a save of this name (from this device or another): its older copies here stay.
    keeps(name) { const e = state.saves[name]; return Boolean(e && !e.removed); },
    async remove(name) {
      const r = await fetch(at(name), { method: 'DELETE', headers: CHANGE });
      if (!r.ok && r.status !== 404) throw fail(`${name} remove`, r);
      const f = listSaves(FS).saves.find((s) => s.name === name);
      state.saves[name] = f ? { etag: null, size: f.size, mtime: +f.mtime, removed: true } : undefined;
      if (!f) delete state.saves[name];
      full = false;
      count = count === null ? null : Math.max(0, count - 1);
      keep();
      changed();
    },
  };
}

// The two plans, side by side: what the page shows a player who has no Cloud Saves (account dialog, start box).
// The two plans. The free game is the one everybody has (the green border): players read a highlighted plan as the one
// they must take, and the $5 card looked like the only way in. Cloud Saves is marked an optional add-on.
// In the page's language (i18n.js), the price as the server gives it.
export function plansHtml(offer, maxSaves = 30) {
  return `<div class="plans">
    <div class="plan on"><span class="tag">${esc(t('Included · no account'))}</span><h4>${esc(t('The game'))}</h4><div class="price">${esc(t('Free'))}</div><ul>
      <li>${esc(t('The whole game, in your browser'))}</li><li>${esc(t('Your saves stay in this browser'))}</li><li>${esc(t('Download and upload saves as files'))}</li></ul></div>
    <div class="plan pro"><span class="tag">${esc(t('Optional add-on'))}</span><h4>${esc(t('Membership'))}</h4><div class="price">${esc(priceText(offer?.price))}</div><ul>
      <li>${esc(t('Cloud Saves: your saves on every computer and iPad, backed up right after each save, up to {n} in the cloud', { n: maxSaves }))}</li>
      <li>${esc(t('The Scout: find players by age, potential and attributes in your saved games'))}</li></ul></div>
  </div>`;
}
// The site's terms, by their English names (the pages are in English): LEGAL_LINKS as they are, legalLinks() with
// "(English)" after them in another language.
export const LEGAL_LINKS = ['pricing', 'terms', 'refunds', 'privacy'].map((p) =>
  `<a href="/${p}" target="_blank" rel="noopener">${p[0].toUpperCase()}${p.slice(1)}</a>`).join(' · ');
export const legalLinks = () => (language() === 'en' ? LEGAL_LINKS : `${LEGAL_LINKS} <span>${esc(t('(English)'))}</span>`);
const day = (time) => new Date(time).toLocaleDateString(dateLocale(), { dateStyle: 'medium' });

// Paddle.js, loaded when a player buys, not before (no script of Paddle's on the page until then).
let paddleLoad = null;
function loadPaddle(cfg, onEvent) {
  paddleLoad ??= new Promise((resolve, reject) => {
    const s = document.createElement('script');
    s.src = 'https://cdn.paddle.com/paddle/v2/paddle.js';
    s.onload = () => {
      try {
        if (cfg.env === 'sandbox') window.Paddle.Environment.set('sandbox');
        window.Paddle.Initialize({ token: cfg.token, eventCallback: onEvent });
        resolve(window.Paddle);
      } catch (e) { paddleLoad = null; reject(e); }
    };
    s.onerror = () => { paddleLoad = null; s.remove(); reject(Object.assign(new Error('the checkout did not load'), { key: 'the checkout did not load' })); };
    document.head.appendChild(s);
  });
  return paddleLoad;
}

// The Account button and dialog: the plans, the sign-in, buying and managing Membership (Cloud Saves and the Scout). isRunning: whether a game
// runs (then the sign-in must not leave the page); leave(): the page is about to be left on purpose.
export function mountAccount({ log, toast, isRunning, isStandalone, leave }) {
  const dialog = $('account'), body = $('account-body'), note = $('account-note'), button = $('account-open');
  let me = null, cloud = null;
  const listeners = new Set();
  const say = (text, bad = false) => { note.textContent = text; note.className = bad ? 'small bad' : 'small'; };
  const buyable = () => Boolean(me?.offer?.coffee || me?.offer?.paddle || me?.offer?.url);

  function render() {
    const u = me?.user, offer = me?.offer;
    button.hidden = !me?.auth;
    button.classList.toggle('on', Boolean(u?.cloud));
    button.querySelector('span').textContent = u ? (u.name || u.email).split(/[\s@]/)[0].slice(0, 14) : t('Sign in');
    button.setAttribute('aria-label', u ? t('Account: {email}', { email: u.email }) : t('Sign in'));
    if (!me?.auth) { body.innerHTML = ''; return; }
    const legal = `<p class="small legal">${legalLinks()}</p>`;
    if (!u) {
      body.innerHTML = `<p>${esc(t('Play for free, or become a member: Cloud Saves keep your saved games on all your devices, and the Scout finds players for your career.'))}</p>
        ${plansHtml(offer, me.maxSaves)}
        <p class="small">${esc(t('Membership needs a Google account: signing in is how your saves know they are yours, on any device. Signing in alone changes nothing else in the game.'))}</p>
        <div class="row"><button class="primary" id="account-signin">${esc(t('Sign in with Google'))}</button></div>${legal}`;
      $('account-signin').onclick = signIn;
      return;
    }
    const signedIn = `<p>${u.cloud ? t('Signed in as <b>{email}</b> · Member', { email: esc(u.email) }) : t('Signed in as <b>{email}</b> · Free plan', { email: esc(u.email) })}</p>`;
    // The account's level (levels.mjs) and what it gives.
    const levelText = { legend: t('Club Legend: early access to new seasons and features, Cloud Saves and the full Scout.'),
      squad: t('Squad Player: Cloud Saves and the full Scout.'), supporter: t('Supporter: thank you for the support.') }[u.level];
    const level = levelText ? `<p class="small"><b>${esc(levelText)}</b></p>` : '';
    const own = `<div class="row"><button id="account-signout">${esc(t('Sign out'))}</button><button id="account-delete">${esc(t('Delete account…'))}</button>
      ${u.admin ? `<button id="account-admin">${esc(t('Admin panel'))}</button>` : ''}</div>`;
    if (u.cloud) {
      const info = cloud?.info();
      const term = u.cloudFrom === 'subscription' || u.cloudFrom === 'membership'
        ? (u.cancelAt ? esc(t('Canceled: your membership ends on {date}.', { date: day(u.cancelAt) }))
          : u.until ? esc(t('Renews by itself; paid until {date}.', { date: day(u.until) })) : '')
        : offer?.free ? '' : esc(t('Given to this account.'));
      // Saves left in the cloud on this device (keepInCloud): bringing one here closed the tab last time.
      const keptNames = Object.keys(keptInCloud());
      const kept = keptNames.length ? `<p class="small">${esc(keptNames.length > 1
        ? t('Left in the cloud on this device: the last try to bring them here closed the tab, most likely for its memory. The game here runs without them.')
        : t('Left in the cloud on this device: the last try to bring it here closed the tab, most likely for its memory. The game here runs without it.'))}</p>
        ${keptNames.map((n, i) => `<div class="row"><span class="small">${esc(n)}</span><button data-kept="${i}">${esc(t('Get it here'))}</button></div>`).join('')}` : '';
      const member = u.cloudFrom === 'membership' && offer?.url
        ? `<div class="row"><button id="account-member">${esc(t('Your membership'))}</button>
          <span class="small">${esc(t('on Buy Me a Coffee: change, pause or cancel it there'))}</span></div>` : '';
      body.innerHTML = `${signedIn}${level}<p class="small">${esc(info?.count != null
        ? t('Cloud Saves are on. Each save you make here goes to the cloud, and when the game starts, this browser gets the saves you made elsewhere. {count} of {max} saves are in the cloud.', { count: info.count, max: info.max })
        : t('Cloud Saves are on. Each save you make here goes to the cloud, and when the game starts, this browser gets the saves you made elsewhere.'))}</p>
        <p class="small">${term} ${cloud?.stopped ? esc(t('Cloud saves stopped: sign in again.')) : info?.last ? esc(t('Last sync: {when}.', { when: when(info.last) })) : ''}</p>
        ${u.manage ? `<div class="row"><button id="account-manage">${esc(t('Manage subscription'))}</button><span class="small">${esc(t('cancel, card, invoices'))}</span></div>` : ''}
        ${kept}${member}${own}${legal}`;
      if (u.manage) $('account-manage').onclick = manage;
      for (const b of body.querySelectorAll('button[data-kept]')) b.onclick = () => {
        const name = keptNames[Number(b.dataset.kept)];
        if (!confirm(t('Bring {name} to this device? The last try closed the tab; if it closes again, the game leaves it in the cloud once more.', { name }))) return;
        keepInCloud(name, false);
        log(`page: cloud saves: ${name}: brought here again, as asked`);
        render();
        cloud?.run?.({ replace: false });
      };
      if (member) $('account-member').onclick = () => window.open(offer.url, '_blank', 'noopener');
    } else {
      body.innerHTML = `${signedIn}${level}<p class="small">${esc(t('Your saves stay in this browser, and Saved games downloads them as files.'))}</p>
        ${plansHtml(offer, me.maxSaves)}
        ${u.member && !u.member.on && u.member.level && offer?.coffee
          ? `<p class="small bad">${esc(t('Your Buy Me a Coffee membership ({level}) is not a level that gives Membership here: change its level on Buy Me a Coffee.', { level: u.member.level }))}</p>` : ''}
        ${!buyable() ? `<p class="small">${esc(t('Membership is not on sale yet.'))}</p>`
          : offer.coffee ? `<div class="row"><button class="primary" id="account-buy">${esc(t('Become a member · {price}', { price: priceText(offer.price) }))}</button></div>
          <p class="small">${t('You become a member on Buy Me a Coffee: join with this account\'s email, <b>{email}</b>, and Cloud Saves and the Scout turn on by themselves. Renews every month or year; cancel any time. A full refund within 14 days of any payment. Joined with another email? Send us a report (Help, Send to the developer) and we will link it.', { email: esc(u.email) })}</p>`
          : `<div class="row"><button class="primary" id="account-buy">${esc(t('Become a member · {price}', { price: priceText(offer.price) }))}</button></div>
          <p class="small">${esc(t('Renews every month; cancel any time. A full refund within 14 days of any payment. Payment by Paddle.'))}</p>`}
        ${own}${legal}`;
      if (buyable()) $('account-buy').onclick = buy;
    }
    $('account-signout').onclick = signOut;
    $('account-delete').onclick = deleteAccount;
    // The statistics in a tab of their own, so the game here goes on (CM_ADMIN_EMAILS lists the admins).
    if (u.admin) $('account-admin').onclick = () => window.open('/admin', '_blank', 'noopener');
  }

  function signIn() {
    if (!isRunning()) { leave(); location.href = '/auth/google/start?mode=page'; return; }
    const w = isStandalone() ? null : window.open('/auth/google/start?mode=popup', 'cm-sign-in', 'popup,width=500,height=680');
    if (w) { say(t('Finish the sign-in in the small window.')); return; }
    // An app on the home screen, or a blocked window: the sign-in leaves the page, so the game must be saved first.
    body.querySelector('.row').innerHTML = `<button class="primary" id="account-signin-leave">${esc(t('Saved: sign in now'))}</button>`;
    say(t('Signing in here leaves this page and starts it again. Save your game in the game first.'));
    $('account-signin-leave').onclick = () => { leave(); location.href = '/auth/google/start?mode=page'; };
  }

  // After the payment: Paddle tells the server (a webhook), the page asks until Cloud Saves are on.
  async function waitForCloud() {
    toast(t('Payment done. Turning on your membership…'), { ms: 5000 });
    for (let i = 0; i < 40; i++) {
      await new Promise((r) => setTimeout(r, i < 10 ? 1500 : 3000));
      await refresh();
      if (me?.user?.cloud) { toast(t('You are a member: Cloud Saves are on, and the Scout shows all its matches.'), { ms: 7000 }); log('page: Cloud Saves on after the payment'); return; }
    }
    toast(t('The payment is done. Your membership turns on within a few minutes: reload the page if it does not.'), { kind: 'bad', ms: 10000 });
  }

  // While the player joins on Buy Me a Coffee (another tab): Buy Me a Coffee tells the server, and the page asks every
  // few seconds for 20 minutes, and at once when the player comes back to this tab. legend: the player joins as a Club
  // Legend (an early thing's Become a Club Legend, early.js): the wait ends at that level, not at Membership.
  let memberWait = null;
  function waitForMember(legend = false) {
    if (memberWait) { if (legend) memberWait.legend = true; return; }
    const until = Date.now() + 20 * 60e3;
    const check = async () => {
      if (!memberWait) return;
      const had = Boolean(me?.user?.cloud);
      await refresh();
      if (memberWait.legend ? me?.user?.level === 'legend' : me?.user?.cloud) {
        stop();
        if (!had) toast(t('Thank you for joining! You are a member: Cloud Saves are on, and the Scout shows all its matches.'), { ms: 8000 });
        log(`page: ${me.user.level === 'legend' ? 'Club Legend' : 'Cloud Saves on'} after joining`);
      } else if (Date.now() > until) stop();
    };
    const seen = () => { if (document.visibilityState === 'visible') check(); };
    const timer = setInterval(check, 5000);
    const stop = () => { clearInterval(timer); document.removeEventListener('visibilitychange', seen); memberWait = null; };
    document.addEventListener('visibilitychange', seen);
    memberWait = { stop, legend };
  }

  async function buy() {
    const offer = me?.offer;
    if (offer?.coffee) {
      window.open(offer.url, '_blank', 'noopener');
      say(t('Join on Buy Me a Coffee with {email}. Your membership turns on here a moment after.', { email: me.user.email }));
      log('page: membership page opened');
      waitForMember();
      return;
    }
    if (!offer?.paddle) { if (offer?.url) window.open(offer.url, '_blank', 'noopener'); return; }
    say(t('Opening the checkout…'));
    try {
      const Paddle = await loadPaddle(offer.paddle, (ev) => {
        if (ev?.name === 'checkout.completed') waitForCloud();
        if (ev?.name === 'checkout.error') log(`page: checkout error: ${JSON.stringify(ev.data ?? ev).slice(0, 300)}`);
      });
      dialog.close();                          // the dialog would cover Paddle's overlay
      Paddle.Checkout.open({
        items: [{ priceId: offer.paddle.priceId, quantity: 1 }],
        customer: { email: me.user.email },
        customData: { account: me.user.ref },
        settings: { displayMode: 'overlay', theme: 'dark', allowLogout: false },
      });
      log('page: checkout opened');
    } catch (e) {
      say(t('The checkout did not open: {why}. Try again in a moment.', { why: t(e.key ?? e.message, e.vars) }), true);
      log(`page: checkout: ${e.message}`);
    }
  }

  async function manage() {
    const w = window.open('about:blank', '_blank');            // now, while the click counts; the address follows
    const r = await fetch('/api/billing/portal', { method: 'POST', headers: CHANGE }).catch(() => null);
    const j = await r?.json().catch(() => ({})) ?? {};
    const url = j.url ?? (r?.status === 501 ? 'https://paddle.net' : null);
    if (url && w) { w.location.href = url; return; }
    w?.close();
    if (url) location.href = url;
    else say(t('The subscription page did not open: {why}. paddle.net has it too.', { why: j.error ?? t('no connection') }), true);
  }

  async function signOut() {
    await fetch('/auth/logout', { method: 'POST', headers: CHANGE }).catch(() => {});
    cloud?.stop();
    await refresh();
    say(t('Signed out. Your saves stay in this browser.'));
  }

  async function deleteAccount() {
    if (!confirm(t('Delete your account and every save of it in the cloud? The saves in this browser stay.'))) return;
    const r = await fetch('/api/account', { method: 'DELETE', headers: CHANGE }).catch(() => null);
    if (!r?.ok) { say((await r?.json().catch(() => null))?.error ?? t('The account was not deleted: try again.'), true); return; }
    cloud?.stop();
    await refresh();
    say(t('Your account and its cloud saves are deleted. The saves in this browser stay.'));
    log('page: account deleted');
  }

  // What the listeners follow: the account, Membership, and the level (a Squad Player who becomes a Club Legend keeps
  // Membership, and an early thing unlocks).
  const key = (m) => `${m?.user?.email ?? ''}|${Boolean(m?.user?.cloud)}|${m?.user?.level ?? ''}|${Boolean(m?.early?.legend)}`;
  async function refresh() {
    const before = key(me);
    me = (await fetchMe()) ?? me;
    render();
    if (key(me) !== before) for (const f of listeners) f(me);
    return me;
  }

  const open = () => { say(''); render(); dialog.showModal(); };
  onLanguage(() => render());                    // the button's label, and the dialog when it is open
  button.onclick = open;
  $('account-close').onclick = () => dialog.close();
  // The small sign-in window tells this page when it is done (cloud.mjs, popupPage).
  try {
    new BroadcastChannel('cm-account').onmessage = async (e) => {
      if (!e.data?.ok) { say(t('Not signed in: {why}', { why: e.data?.why ?? t('unknown') }), true); return; }
      await refresh();
      if (me?.user) toast(t('Signed in as {email}', { email: me.user.email }));
    };
  } catch { /* no BroadcastChannel: the dialog's next opening shows the account */ }
  // Back from the sign-in that left the page.
  const q = new URLSearchParams(location.search);
  if (q.has('account')) {
    const ok = q.get('account') === 'signed-in', why = q.get('why');
    history.replaceState(null, '', '/');
    if (!ok) setTimeout(() => toast(t('Not signed in: {why}', { why: why ?? t('unknown') }), { kind: 'bad', ms: 8000 }));
  }

  const ready = refresh();
  if (q.get('account') === 'signed-in') {
    ready.then((m) => {
      if (m?.user) toast(m.user.cloud ? t('Signed in as {email}. Cloud Saves are on.', { email: m.user.email }) : t('Signed in as {email}', { email: m.user.email }), { ms: 6000 });
    });
  }
  return {
    ready,                                       // resolves to the account, once asked
    get me() { return me; },
    open,
    // A line for a player who could have Cloud Saves and has not (Saved games, after a save), or null.
    promo() {
      if (!me?.auth || me.user?.cloud || me.offer?.free === undefined) return null;
      return me.offer.free ? t('Sign in with Google to keep your saves on all your devices too, for free.')
        : t('Membership keeps your saves on all your devices too, and its Scout finds players: {price}.', { price: priceText(me.offer.price) });
    },
    setCloud(c) { cloud = c; c?.onChange(() => { if (dialog.open) render(); }); render(); },
    onAccountChange(f) { listeners.add(f); },
    // The player opened Become a Club Legend (early.js): ask the server until the level comes.
    waitForLegend() { waitForMember(true); },
  };
}
