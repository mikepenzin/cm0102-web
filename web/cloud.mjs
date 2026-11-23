// Accounts and cloud saves (web/README.md, "Cloud saves"): sign in with Google, and the saves of an account in an S3
// store (Tigris on Fly.io). Without its settings the server runs as before: /api/me answers { auth: false } and the
// page shows no account.
//
// Who has cloud saves: CM_CLOUD_FOR=everyone gives them to every signed-in account; else the emails in CM_CLOUD_USERS
// (comma-separated) and the accounts whose record says plan "cloud" until planUntil (accounts/<id>.json in the store:
// set by a Paddle subscription, billing.mjs, or by hand), the emails with a running Buy Me a Coffee membership
// (members/..., billing.mjs) and the accounts an admin gave them to have them: each account's level (planOf, levels.mjs)
// is Squad Player or higher. Everyone else keeps saves in the browser only.
//
// The store holds:
//   accounts/<google id>.json            { sub, email, name, created, seen, plan, planUntil }
//   grants/<google id>.json              { sub, email, by, at, level }: a level an admin gave (the admin page): squad or
//                                        legend; one kept before levels (no level) is squad
//   members/<sha256 of the email>.json   { email, on, until, cancelAt, id, status, level, levelId, eventAt, event, at }:
//                                        the Buy Me a Coffee membership of an email, written by its webhook
//   saves/<google id>/<name, base64url>.gz the save, gzip-compressed by the page; metadata raw-size and saved (ms)
import { randomBytes } from 'node:crypto';
import { createReadStream } from 'node:fs';
import { createGunzip } from 'node:zlib';
import { spool, unspool } from './spool.mjs';
import { GOOGLE, SignInError, googleFinish, googleStart, readCookies, sealer, setCookie } from './auth.mjs';
import { PADDLE_API, accountRef, applyMembership, applySubscription, checkCoffeeSignature, checkPaddleSignature, memberKey, normalEmail,
  openAccountRef, portalUrl } from './billing.mjs';
import { S3Error, s3FromEnv } from './s3.mjs';
import { atLeast, higher, levelsFromEnv, paidLevel, rank } from './levels.mjs';
import { SAVE_HEADER, checkSave } from './saves.js';
import { DATA_SET_FILES } from './dataset.js';
import { zipEntries } from './zip.js';
import { MAX_TACTIC, pruneRemoved, validTactic } from './tacticsync.js';

const SESSION = 'cm_session', OAUTH = 'cm_oauth';
const SESSION_DAYS = 30;
const LAST_COFFEE = 'coffee/last.json';
const LEVEL_NAMES = { free: 'Free', supporter: 'Supporter', squad: 'Squad Player', legend: 'Club Legend' };     // the last notification of Buy Me a Coffee, for the admin page
// What one save may be, compressed and as it is. Long careers make large saves (a player's was 446 MB, 15 November
// 2026), where 64 MB and 256 MB once were the limits, set when the server held each upload in memory. The server now
// streams a save through its disk (spool.mjs), and a page with direct uploads sends it straight to the store. Fly
// secrets CM_CLOUD_MAX_PACKED_MB and CM_CLOUD_MAX_RAW_MB set others.
export const MAX_GZ = 400 << 20;
export const MAX_RAW = 1024 << 20;
export const MAX_TACTICS = 200;            // tactics of an account in the cloud (a tactic file: 1,476 bytes)
export const MAX_SET_ZIP = 48 << 20;       // a data set as a zip (the 3.9.68 data: about 61 MB, some 20 MB zipped)

const b64u = (s) => Buffer.from(s, 'utf8').toString('base64url');
const unb64u = (s) => Buffer.from(s, 'base64url').toString('utf8');

// A data set's name as the store takes it: a folder name of Databases (datasets.js cleanName).
export function validSetName(name) {
  return typeof name === 'string' && name.length >= 1 && name.length <= 60 && !/[\u0000-\u001f\u007f\\/:*?"<>|]/.test(name)
    && name.trim() === name && !name.startsWith('.');
}

// What a zip of a data set holds: null and its size, or why it is not one (the 20 files in one folder).
export function inspectSetZip(zip) {
  let entries;
  try { entries = zipEntries(zip); } catch (e) { return { why: e.message }; }
  const by = new Map();
  for (const e of entries) {
    const i = e.name.lastIndexOf('/'), base = e.name.slice(i + 1).toLowerCase();
    if (DATA_SET_FILES.includes(base)) by.set(base, e);
  }
  if (by.size !== DATA_SET_FILES.length) return { why: `it does not hold the ${DATA_SET_FILES.length} database files` };
  return { why: null, rawSize: [...by.values()].reduce((a, e) => a + e.size, 0) };
}

// A save's name as the store takes it: a .sav file name of the save folder, without a folder part.
export function validSaveName(name) {
  return typeof name === 'string' && name.length >= 5 && name.length <= 120 && /\.sav$/i.test(name)
    && !/[\u0000-\u001f\\/:*?"<>|]/.test(name) && name.trim() === name && name.toLowerCase() !== 'temp_save_file.sav';
}

function send(res, status, body, headers = {}) {
  const text = body === undefined ? '' : JSON.stringify(body);
  res.writeHead(status, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store', ...headers });
  res.end(text);
}

// The body in one Buffer. With a Content-Length (the page's fetch sends one) the Buffer is made at that size and filled,
// so an upload is held once, not twice (its parts and their copy).
function readBody(req, limit) {
  return new Promise((resolveBody, reject) => {
    const length = Number(req.headers['content-length']);
    const whole = Number.isInteger(length) && length >= 0 && length <= limit ? Buffer.allocUnsafe(length) : null;
    const chunks = [];
    let n = 0, over = false;
    req.on('data', (c) => {
      if (over) return;
      if (n + c.length > (whole ? whole.length : limit)) { over = true; chunks.length = 0; return; }
      if (whole) c.copy(whole, n); else chunks.push(c);
      n += c.length;
    });
    req.on('end', () => (over ? reject(Object.assign(new Error('too large'), { status: 413 }))
      : resolveBody(whole ? whole.subarray(0, n) : Buffer.concat(chunks))));
    req.on('error', reject);
  });
}

// What the gzip file holds (a save on disk, spool.mjs): its length and whether it is a saved game of CM 01/02 (the
// header of saves.js), read through without keeping it.
function inspectSave(path, maxRaw) {
  return new Promise((resolveInspect) => {
    const g = createGunzip(), src = createReadStream(path);
    const head = [];
    let total = 0, headLen = 0, settled = false;
    const done = (v) => { if (!settled) { settled = true; src.destroy(); resolveInspect(v); } };
    g.on('data', (c) => {
      total += c.length;
      if (headLen < SAVE_HEADER) { head.push(c); headLen += c.length; }
      if (total > maxRaw) { done({ why: `it is larger than ${maxRaw >> 20} MB` }); g.destroy(); }
    });
    g.on('end', () => {
      const h = Buffer.concat(head);
      done({ why: checkSave(new Uint8Array(h.buffer, h.byteOffset, h.length), total), total });
    });
    g.on('error', () => done({ why: 'the upload is not gzip data' }));
    src.on('error', () => done({ why: 'the upload was not kept' }));
    src.pipe(g);
  });
}

const popupPage = (ok, why) => `<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>${ok ? 'Signed in' : 'Not signed in'}</title><body style="font:16px system-ui;background:#0b0d10;color:#e8eaed;padding:24px">
<p>${ok ? 'Signed in. This window closes by itself.' : `Not signed in: ${String(why).replace(/[&<>"']/g, '')}`}</p>
<script>try { new BroadcastChannel('cm-account').postMessage(${JSON.stringify({ ok, why: ok ? null : String(why) })}); } catch {}
${ok ? 'setTimeout(() => window.close(), 300);' : ''}</script>`;

// paddle: { env, clientToken, priceIds, webhookSecret, apiKey, apiBase } to sell Cloud Saves with Paddle, or null.
// coffee: { secret, url, levels } to sell Cloud Saves as a Buy Me a Coffee membership, or null: the webhook's signing
// secret, the membership page, and the names or ids of the levels that give Cloud Saves (none named: every level).
// price: how the page names the price ("$5 a month").
export function createCloud({ s3, clientId, clientSecret, sessionSecret, publicUrl = null, cloudFor = 'listed', cloudUsers = [],
  upgradeUrl = null, maxSaves = 30, maxSets = 5, maxPacked = MAX_GZ, maxRaw = MAX_RAW, google = GOOGLE, paddle = null, coffee = null, price = '$5 a month', log = (t) => console.log(t),
  onAccountDeleted = () => {}, uploadBudget = 160 << 20, isAdmin = () => false, direct: directWanted = true, origins = ['*'],
  storeTtl = 10 * 60e3, linkStores = [], recheck = [60e3, 5 * 60e3, 30 * 60e3] }) {
  const seal = sealer(sessionSecret);
  // A save may be maxPacked bytes, compressed (MAX_GZ unless a test sets less); the saves list tells the page, so a save
  // larger than that is not sent at all (account.js).
  const mbOf = (n) => Math.round(n / 1048576);
  const tooPacked = `a save may be ${mbOf(maxPacked)} MB at most, compressed`;
  // Direct: the page sends a save or a data set to the store, and gets one from it, with a link that this server signs
  // (s3.presign), so the files do not pass this machine. An upload goes to incoming/<account>/ first; the server then
  // checks it and copies it to its place (the store copies; the bytes do not come here). The store must let the page's
  // origin in (CORS): the server sets the bucket's rules at the start. When that fails, or with direct off, the page
  // goes through this server as before (putSave, getSave).
  // direct: true (set the rules, and go direct when that works), 'given' (the rules were set by hand in the store's
  // console: CM_DIRECT=on) or false (CM_DIRECT=off).
  let direct = false;
  // The store's address that the pages' links name (presign): s3's own, or another address of the same bucket that
  // passed the check (linkStores, { s3, name }: Tigris has two hosts, each with the bucket in the host or the path; on
  // the live site, 17 November 2026, fly.storage.tigris.dev answered the preflight without Access-Control-Allow-Origin).
  let links = s3, linkName = null;
  const candidates = () => [{ s3, name: null }, ...linkStores];
  // The way a page would go, tried by this server at its start: a signed upload link as the pages get them (content-type
  // and content-length signed), and first the browser's preflight for it from the site's own address. On the live site
  // the pages' uploads to the store failed, "the connection to the store failed", where the server had set the rules
  // (15 November 2026): a page cannot tell a refused preflight from a refused signature. The upload carries the headers
  // of a save's (putSave), and the file then comes back with a download link as a page gets it, which must carry
  // Access-Control-Allow-Origin too. null: all of it works; else why not.
  async function checkDirect(store = s3) {
    const key = `meta/direct-check-${randomBytes(6).toString('hex')}.txt`, body = Buffer.from('ok');
    const link = store.presign('PUT', key, { headers: { 'content-type': 'text/plain', 'content-length': body.length,
      'x-amz-meta-raw-size': body.length, 'x-amz-meta-saved': Date.now() } });
    const { 'content-length': _, ...headers } = link.headers;
    const origin = origins.find((o) => o !== '*') ?? null;
    const allowed = (r) => { const a = r.headers.get('access-control-allow-origin'); return a === '*' || a === origin; };
    try {
      if (origin) {
        const pre = await fetch(link.url, { method: 'OPTIONS', headers: { origin, 'access-control-request-method': 'PUT',
          'access-control-request-headers': Object.keys(headers).join(',') } });
        const allow = pre.headers.get('access-control-allow-origin');
        if (!pre.ok || !allowed(pre))
          return `the bucket's CORS rules do not let ${origin} send (the preflight: HTTP ${pre.status}, ${allow ? `allowed ${allow}` : 'no Access-Control-Allow-Origin'})`;
      }
      const put = await fetch(link.url, { method: 'PUT', body, headers: { ...(origin ? { origin } : {}),
        ...Object.fromEntries(Object.entries(headers).map(([k, v]) => [k, String(v)])) } });
      if (!put.ok) return `the store refused a signed upload link (HTTP ${put.status}: ${(await put.text()).replace(/\s+/g, ' ').slice(0, 200)})`;
      const get = await fetch(store.presign('GET', key).url, { headers: origin ? { origin } : {} });
      const back = Buffer.from(await get.arrayBuffer());
      if (!get.ok || !back.equals(body)) return `a signed download link did not bring the file back (HTTP ${get.status})`;
      if (origin && !allowed(get)) return `a download from the store has no Access-Control-Allow-Origin for ${origin}`;
      return null;
    } finally {
      await s3.delete(key).catch(() => {});
    }
  }
  const tryDirect = async (store = s3) => {
    try { return await checkDirect(store); } catch (err) { return `the check of a signed upload link failed (${err.message})`; }
  };
  // What the bucket holds, as its rules come back from the store (for the log of a check that failed).
  async function rulesHeld() {
    try {
      const r = await s3.getCors?.();
      if (r === undefined) return '';
      return r ? `; the bucket holds: origins ${r.origins.join(', ') || 'none'}; methods ${r.methods.join(', ') || 'none'}` : '; the bucket holds no CORS rules';
    } catch (err) { return `; its rules could not be read back (${err.message})`; }
  }
  const after = (ms) => (ms < 60e3 ? `${Math.round(ms / 1000)} s` : `${Math.round(ms / 60e3)} min`);
  // Each address of the store in turn (candidates): the first that passes the check names the pages' links. null when
  // one does, else each one's reason.
  async function findWay() {
    const whys = [];
    for (const c of candidates()) {
      const why = await tryDirect(c.s3);
      if (!why) { links = c.s3; linkName = c.name; direct = true; return null; }
      whys.push(c.name ? `at ${c.name}: ${why}` : why);
    }
    return whys;
  }
  // No address passed: once more after each pause of recheck (rules may take a while to reach every place of the
  // store), and direct goes on at the first that works.
  async function afterFailure(whys) {
    log(`cloud: ${whys[0]}${await rulesHeld()}: saves and data sets go through this server (web/deploy/README.md, "Straight to Tigris")`);
    for (const w of whys.slice(1)) log(`cloud: and ${w}`);
    if (!recheck.length) return;
    log(`cloud: checks again in ${recheck.map(after).join(', ')}`);
    laterChecks(recheck).catch((err) => log(`cloud: the checks of the store stopped: ${err.message}`));
  }
  async function laterChecks() {
    let waited = 0;
    for (const ms of recheck) {
      await new Promise((r) => { const t = setTimeout(r, ms - waited); t.unref?.(); });
      waited = ms;
      if (!await findWay()) {
        log(`cloud: the check works ${after(ms)} after the start${linkName ? ` at ${linkName}` : ''}: saves and data sets now go straight between the browser and the store`);
        sweepIncoming();
        return;
      }
    }
    log(`cloud: the last check failed too: saves and data sets go through this server until the next start`);
  }
  const directReady = (async () => {
    if (!directWanted || !s3.presign) return;
    if (directWanted !== 'given') {
      try { await s3.putCors(origins); } catch (err) {
        log(`cloud: the store's CORS rules were not set (${err.message}): saves and data sets go through this server. Set them in the`
          + ` store's console and CM_DIRECT=on (web/deploy/README.md, "Straight to Tigris")`);
        return;
      }
    }
    const whys = await findWay();
    if (whys) {
      await afterFailure(whys);
      return;
    }
    const from = origins.find((o) => o !== '*');
    log(`cloud: saves and data sets go straight between the browser and the store (${directWanted === 'given' ? 'CORS rules set in the store\'s console, CM_DIRECT=on' : `CORS for ${origins.join(', ')}`}; checked${
      from ? ` from ${from}` : ''}: a signed upload link and the browser's preflight work${linkName ? `; the links name ${linkName}, in place of the configured address, which did not pass` : ''})`);
  })();
  // The store's own id (meta/store.json, made at the first start on a bucket): the lists name it, so a page whose saves
  // went to another bucket (BUCKET_NAME or the key changed) sends them again instead of taking them as removed.
  let storeId = null, storeAt = 0;
  async function theStore() {
    if (storeId && Date.now() - storeAt < storeTtl) return storeId;
    storeAt = Date.now();
    const buf = await s3.getBuffer('meta/store.json');
    if (buf) storeId = JSON.parse(buf.toString()).id;
    else {
      storeId = randomBytes(9).toString('hex');
      await s3.put('meta/store.json', Buffer.from(JSON.stringify({ id: storeId, created: new Date().toISOString() })), { type: 'application/json' });
      log(`cloud: a new store (meta/store.json ${storeId}): pages send their saves and data sets to it again`);
    }
    return storeId;
  }
  const incomingKey = (sub, ext) => `incoming/${sub}/${Date.now()}-${randomBytes(6).toString('hex')}.${ext}`;
  const incomingOf = (sub, key, ext) => typeof key === 'string' && new RegExp(`^incoming/${sub}/\\d+-[0-9a-f]{12}\\.${ext}$`).test(key);
  // Uploads that were never finished: gone after a day (each 6 hours, and at the start).
  async function sweepIncoming() {
    try {
      for (const o of await s3.list('incoming/')) if (Date.parse(o.modified) < Date.now() - 86400e3) await s3.delete(o.key);
    } catch (err) { log(`cloud: incoming uploads not swept: ${err.message}`); }
  }
  if (directWanted && s3.presign) {
    directReady.then(() => { if (direct) sweepIncoming(); });
    setInterval(() => { if (direct) sweepIncoming(); }, 6 * 3600e3).unref?.();
  }
  // The uploads in memory at once (readBody holds each whole, for its check and its signature): a data set, at most
  // MAX_SET_ZIP (a save goes through the disk instead: putSave). Past uploadBudget an upload is turned away with 503 and goes at the next sync, so a
  // few players saving at once cannot run the server's small machine out of memory (web/deploy/README.md).
  let held = 0;
  const hold = (req, res, max) => {
    const n = Math.min(Number(req.headers['content-length']) || max, max);
    if (held + n > uploadBudget && held > 0) {
      req.resume();
      res.writeHead(503, { 'Content-Type': 'application/json', 'Retry-After': '30', 'Cache-Control': 'no-store' });
      res.end(JSON.stringify({ error: 'the server is busy with other uploads: it goes at the next sync' }));
      return null;
    }
    held += n;
    let freed = false;
    return () => { if (!freed) { freed = true; held -= n; } };
  };
  const users = new Set(cloudUsers.map((e) => e.trim().toLowerCase()).filter(Boolean));
  const accounts = new Map();               // google id -> { account, at }: the record, read at most once a minute

  const origin = (req) => publicUrl ?? `${String(req.headers['x-forwarded-proto'] ?? 'http').split(',')[0].trim()}://${req.headers.host}`;
  const secure = (req) => origin(req).startsWith('https:');
  const sessionOf = (req) => seal.open('s', readCookies(req)[SESSION]);
  const accountKey = (sub) => `accounts/${sub}.json`;
  const grantKey = (sub) => `grants/${sub}.json`;
  const saveKey = (sub, name) => `saves/${sub}/${b64u(name)}.gz`;
  const setKey = (sub, name) => `sets/${sub}/${b64u(name)}.zip`;

  async function account(sub, fresh = false) {
    const c = accounts.get(sub);
    if (!fresh && c && Date.now() - c.at < 60000) return c.account;
    const buf = await s3.getBuffer(accountKey(sub));
    const a = buf ? JSON.parse(buf.toString()) : null;
    accounts.set(sub, { account: a, at: Date.now() });
    return a;
  }

  // Cloud Saves an admin gave: grants/<google id>.json, { sub, email, by, at }, a file that only give writes (the sign-in
  // and Paddle's events write the account record, and a record written from an old copy lost a gift kept in it).
  // Kept in memory for a minute, like the record; fresh reads for the admin page and /api/me.
  const grants = new Map();
  async function grant(sub, fresh = false) {
    const c = grants.get(sub);
    if (!fresh && c && Date.now() - c.at < 60000) return c.grant;
    const buf = await s3.getBuffer(grantKey(sub));
    const g = buf ? JSON.parse(buf.toString()) : null;
    grants.set(sub, { grant: g, at: Date.now() });
    return g;
  }

  // The Buy Me a Coffee membership of an email (members/..., billing.mjs), kept in memory for a minute like the record.
  const members = new Map();
  async function member(email, fresh = false) {
    const e = normalEmail(email);
    if (!coffee || !e) return null;
    const c = members.get(e);
    if (!fresh && c && Date.now() - c.at < 60000) return c.member;
    const buf = await s3.getBuffer(memberKey(e));
    const m = buf ? JSON.parse(buf.toString()) : null;
    members.set(e, { member: m, at: Date.now() });
    return m;
  }
  // A Buy Me a Coffee membership: whether it runs now, its site level (levels.mjs, CM_LEVELS), and whether it gives
  // Membership (Squad Player and up).
  const running = (m) => Boolean(m?.on && (!m.until || Date.parse(m.until) > Date.now()));
  const memberLevel = (m) => (coffee ? paidLevel(m, coffee.levels) : { level: 'supporter', known: false });
  const levelGives = (m) => atLeast(memberLevel(m).level, 'squad');
  const memberOn = (m) => running(m) && levelGives(m);

  // An account's level (levels.mjs): the highest of what it pays (a Paddle subscription: squad; a Buy Me a Coffee
  // membership: its level), what an admin gave (grants/), the server's lists (CM_CLOUD_FOR, CM_CLOUD_USERS, a plan set
  // by hand in the record: squad) and being an admin (legend). { level, cloud, from, paid, gift, record, grant,
  // member }: from is 'subscription' | 'membership' while a paid level gives Membership, else 'given' while the level
  // does, else null. fresh: read the store, not the minute's copy.
  async function planOf(user, fresh = false) {
    let a = null, g = null, m = null;
    try { a = await account(user.sub, fresh); } catch (err) { log(`cloud: account ${user.sub} not read: ${err.message}`); }
    const email = String(user.email ?? '').trim().toLowerCase();
    const subOn = a?.plan === 'cloud' && (!a.planUntil || Date.parse(a.planUntil) > Date.now());
    let paid = subOn && a.paddle ? { level: 'squad', known: true, from: 'subscription', until: a.planUntil ?? null, cancelAt: a.paddle?.cancelAt ?? null } : null;
    try { m = await member(user.email, fresh); } catch (err) { log(`cloud: membership of ${user.sub} not read: ${err.message}`); }
    if (running(m)) {
      const pl = memberLevel(m);
      if (!paid || rank(pl.level) > rank(paid.level)) paid = { level: pl.level, known: pl.known, from: 'membership', until: m.until, cancelAt: m.cancelAt };
    }
    try { g = await grant(user.sub, fresh); } catch (err) { log(`cloud: grant ${user.sub} not read: ${err.message}`); }
    const kept = g ?? a?.given ?? null;                               // an admin's gift (a.given: the first version's)
    const gift = kept ? { level: kept.level === 'legend' ? 'legend' : 'squad', by: kept.by ?? null, at: kept.at ?? null } : null;
    const listed = cloudFor === 'everyone' || users.has(email) || (subOn && !a.paddle) ? 'squad' : 'free';
    const admin = isAdmin(email) ? 'legend' : 'free';
    const level = [paid?.level ?? 'free', gift?.level ?? 'free', listed, admin].reduce(higher, 'free');
    const cloud = atLeast(level, 'squad');
    const from = !cloud ? null : paid && atLeast(paid.level, 'squad') ? paid.from : 'given';
    return { level, cloud, from, paid, gift, record: a, grant: kept, member: m };
  }

  // The admin page (server.mjs): an account's level, read fresh, and the level an admin gives or takes back.
  async function planOfSub(sub, fresh = true) {
    const a = await account(sub, fresh);
    if (!a) return null;
    const p = await planOf({ sub, email: a.email }, fresh);
    return { level: p.level, cloud: p.cloud, from: p.from, given: Boolean(p.gift), givenLevel: p.gift?.level ?? null, givenAt: p.gift?.at ?? null,
      until: p.paid?.until ?? null, cancelAt: p.paid?.cancelAt ?? null, paidLevel: p.paid?.level ?? null, paidFrom: p.paid?.from ?? null,
      known: p.paid?.known ?? true, bmcLevel: p.member?.level ?? null, admin: Boolean(isAdmin(String(a.email ?? '').toLowerCase())) };
  }
  // level: 'squad' or 'legend' gives that level, null takes the gift back.
  async function give(sub, level, by) {
    const a = await account(sub, true);
    if (!a) return null;
    if (level) {
      const g = { sub, email: a.email ?? null, by: String(by ?? 'admin').slice(0, 200), at: new Date().toISOString(), level };
      await s3.put(grantKey(sub), Buffer.from(JSON.stringify(g)), { type: 'application/json' });
      grants.set(sub, { grant: g, at: Date.now() });
    } else {
      await s3.delete(grantKey(sub));
      grants.set(sub, { grant: null, at: Date.now() });
      if (a.given) {                            // a gift of the first version, in the record itself
        const { given, ...rest } = a;
        await s3.put(accountKey(sub), Buffer.from(JSON.stringify(rest)), { type: 'application/json' });
        accounts.set(sub, { account: rest, at: Date.now() });
      }
    }
    log(`cloud: account ${sub}: ${level ? `${level} given` : 'the gift taken back'} by an admin`);
    return planOfSub(sub);
  }
  const hasCloud = async (user) => (await planOf(user)).cloud;

  // What the page offers: the price, and how to buy (a Buy Me a Coffee membership, Paddle's checkout, or a link). With
  // a membership to sell, Paddle only keeps the subscriptions bought through it.
  const offer = () => cloudFor === 'everyone'
    ? { price: 'Free with an account', free: true, paddle: null, url: null }
    : coffee ? { price, free: false, url: coffee.url, coffee: true, paddle: null }
    : { price, free: false, url: upgradeUrl,
      paddle: paddle ? { env: paddle.env, token: paddle.clientToken, priceId: paddle.priceIds[0] } : null };

  function finishSignIn(res, mode, ok, why = null) {
    if (mode === 'popup') {
      res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-store' });
      res.end(popupPage(ok, why));
    } else {
      res.writeHead(303, { Location: ok ? '/?account=signed-in' : `/?account=error&why=${encodeURIComponent(why)}`, 'Cache-Control': 'no-store' });
      res.end();
    }
  }

  async function signInStart(req, res, url) {
    const mode = url.searchParams.get('mode') === 'popup' ? 'popup' : 'page';
    const start = googleStart({ clientId, redirectUri: `${origin(req)}/auth/google/callback`, google });
    setCookie(res, OAUTH, seal.seal('o', { state: start.state, verifier: start.verifier, mode }, 600), { maxAge: 600, secure: secure(req) });
    res.writeHead(302, { Location: start.url, 'Cache-Control': 'no-store' });
    res.end();
  }

  async function signInCallback(req, res, url) {
    const o = seal.open('o', readCookies(req)[OAUTH]);
    setCookie(res, OAUTH, null, { secure: secure(req) });
    const mode = o?.mode ?? 'page';
    if (url.searchParams.get('error')) { finishSignIn(res, mode, false, url.searchParams.get('error') === 'access_denied' ? 'cancelled' : url.searchParams.get('error')); return; }
    if (!o || !url.searchParams.get('state') || o.state !== url.searchParams.get('state')) {
      finishSignIn(res, mode, false, 'the sign-in took too long or came from another page: try again');
      return;
    }
    let user;
    try {
      user = await googleFinish({ code: url.searchParams.get('code') ?? '', verifier: o.verifier, clientId, clientSecret,
        redirectUri: `${origin(req)}/auth/google/callback`, google });
    } catch (err) {
      log(`cloud: sign-in failed: ${err.message}`);
      finishSignIn(res, mode, false, err instanceof SignInError ? err.message : 'Google could not be reached');
      return;
    }
    try {
      const had = await account(user.sub, true);
      const now = new Date().toISOString();
      const record = { plan: 'free', planUntil: null, ...had, sub: user.sub, email: user.email, name: user.name,
        created: had?.created ?? now, seen: now };              // a plan and a subscription stay as they were
      await s3.put(accountKey(user.sub), Buffer.from(JSON.stringify(record)), { type: 'application/json' });
      accounts.set(user.sub, { account: record, at: Date.now() });
      if (!had) log(`cloud: new account ${user.sub}`);
    } catch (err) {
      log(`cloud: account ${user.sub} not written: ${err.message}`);      // the sign-in still holds; the plan is read later
    }
    setCookie(res, SESSION, seal.seal('s', user, SESSION_DAYS * 86400), { maxAge: SESSION_DAYS * 86400, secure: secure(req) });
    finishSignIn(res, mode, true);
  }

  async function me(req, res) {
    const user = sessionOf(req);
    if (!user) { send(res, 200, { auth: true, user: null, offer: offer(), maxSaves }); return; }
    const p = await planOf(user, true);              // fresh: a gift or a payment shows at the page's next load
    setCookie(res, SESSION, seal.seal('s', user, SESSION_DAYS * 86400), { maxAge: SESSION_DAYS * 86400, secure: secure(req) });
    send(res, 200, { auth: true, offer: offer(), maxSaves, user: {
      email: user.email, name: user.name, picture: user.picture, level: p.level, cloud: p.cloud, cloudFrom: p.from,
      until: p.from === 'subscription' || p.from === 'membership' ? p.paid.until : null,
      cancelAt: p.from === 'subscription' || p.from === 'membership' ? p.paid.cancelAt ?? null : null,
      manage: Boolean(p.record?.paddle?.customerId), ref: paddle ? accountRef(user.sub, sessionSecret) : null,
      // A membership of a level that gives no Cloud Saves, or one that ended: the page says which.
      member: p.member ? { level: p.member.level, on: memberOn(p.member), until: p.member.until, siteLevel: memberLevel(p.member).level } : null,
      admin: Boolean(isAdmin(String(user.email ?? '').toLowerCase())),   // the page shows the Admin panel button
    } });
  }

  // A notification of Paddle (billing.mjs). Only subscription events change a plan; the others are taken and left.
  async function paddleWebhook(req, res) {
    if (!paddle) { req.resume(); send(res, 404, { error: 'no payments here' }); return; }
    const raw = await readBody(req, 1 << 20);
    const why = checkPaddleSignature(req.headers['paddle-signature'], raw, paddle.webhookSecret);
    if (why) { log(`cloud: Paddle notification refused: ${why}`); send(res, 401, { error: why }); return; }
    let event;
    try { event = JSON.parse(raw.toString()); } catch { send(res, 400, { error: 'not JSON' }); return; }
    if (!String(event.event_type).startsWith('subscription.') || !event.data?.id) { send(res, 200, { ignored: event.event_type }); return; }
    const sub = openAccountRef(event.data.custom_data?.account, sessionSecret);
    if (!sub) { log(`cloud: Paddle ${event.event_type} ${event.data.id}: no account of this server in custom_data`); send(res, 200, { ignored: 'no account' }); return; }
    const prices = (event.data.items ?? []).map((i) => i.price?.id);
    if (!prices.some((id) => paddle.priceIds.includes(id))) {
      log(`cloud: Paddle ${event.event_type} ${event.data.id}: price ${prices.join(',')} is not one of PADDLE_PRICE_ID`);
      send(res, 200, { ignored: 'another price' });
      return;
    }
    const record = await account(sub, true);
    const next = applySubscription(record ?? { sub, email: null, created: new Date().toISOString() }, event);
    if (!next) { send(res, 200, { ignored: 'older than the last event' }); return; }
    await s3.put(accountKey(sub), Buffer.from(JSON.stringify(next)), { type: 'application/json' });
    accounts.set(sub, { account: next, at: Date.now() });
    log(`cloud: account ${sub}: ${event.event_type}, ${event.data.status}, plan ${next.plan}${next.planUntil ? ` until ${next.planUntil}` : ''}`);
    send(res, 200, { ok: true });
  }

  // A notification of Buy Me a Coffee (billing.mjs). Membership events write the member record of the supporter's email;
  // the others (coffees, extras) are taken and left. A test event of the dashboard (live_mode false) is only logged.
  // The last notification (its type and what came of it, no email) is kept for the admin page: coffee/last.json.
  const noteLast = (type, result) => s3.put(LAST_COFFEE, Buffer.from(JSON.stringify({ at: new Date().toISOString(), type: type || null, result })),
    { type: 'application/json' }).catch((err) => log(`cloud: the last Buy Me a Coffee notification not kept: ${err.message}`));
  async function coffeeWebhook(req, res) {
    if (!coffee) { req.resume(); send(res, 404, { error: 'no memberships here' }); return; }
    const raw = await readBody(req, 1 << 20);
    const why = checkCoffeeSignature(req.headers['x-signature-sha256'], raw, coffee.secret);
    if (why) {
      log(`cloud: Buy Me a Coffee notification refused: ${why}`);
      await noteLast(null, `refused: ${why} (BMC_WEBHOOK_SECRET is not the webhook's signing secret)`);
      send(res, 401, { error: why }); return;
    }
    let event;
    try { event = JSON.parse(raw.toString()); } catch { send(res, 400, { error: 'not JSON' }); return; }
    const type = String(event?.type ?? '');
    if (event?.live_mode === false) {
      log(`cloud: Buy Me a Coffee test event ${type}: the signature holds`);
      await noteLast(type, 'a test event: the signature holds');
      send(res, 200, { test: true }); return;
    }
    if (!type.startsWith('membership.') || !event.data?.id) {
      await noteLast(type, 'not a membership (a one-off coffee or an extra): nothing to do');
      send(res, 200, { ignored: type }); return;
    }
    const email = normalEmail(event.data.supporter_email);
    if (!email) {
      log(`cloud: Buy Me a Coffee ${type} ${event.data.id}: no supporter email`);
      await noteLast(type, 'no supporter email in it: nothing to do');
      send(res, 200, { ignored: 'no email' }); return;
    }
    const had = await member(email, true);
    const next = applyMembership(had, event);
    if (!next) { await noteLast(type, 'older than the last event of that membership: nothing to do'); send(res, 200, { ignored: 'older than the last event' }); return; }
    await s3.put(memberKey(email), Buffer.from(JSON.stringify(next)), { type: 'application/json' });
    members.set(email, { member: next, at: Date.now() });
    const pl = memberLevel(next), label = LEVEL_NAMES[pl.level];
    if (!pl.known) log(`cloud: membership ${next.id}: the level ${next.level ?? next.levelId ?? '?'} is not in CM_LEVELS: counted as Supporter`);
    log(`cloud: membership ${next.id} (${next.level ?? 'no level'}: ${label}): ${type}, ${next.status}, Cloud Saves ${memberOn(next) ? `on until ${next.until}` : 'off'}`);
    await noteLast(type, !pl.known ? `the level ${next.level ?? next.levelId ?? '?'} is not in CM_LEVELS: counted as Supporter${running(next) ? '' : ', and it has ended'}`
      : !running(next) ? 'Membership off' : memberOn(next) ? `Membership on (${label})` : `${label}: no Membership at this level`);
    send(res, 200, { ok: true });
  }

  // The admin page (server.mjs): every membership the server was told of, newest first, and the last notification.
  async function memberships() {
    if (!coffee) return { on: false, levels: null, members: [], last: null };
    const keys = (await s3.list('members/')).filter((o) => o.key.endsWith('.json')).map((o) => o.key);
    const list = [];
    for (let i = 0; i < keys.length; i += 8) {                     // 8 records at a time
      list.push(...await Promise.all(keys.slice(i, i + 8).map(async (k) => {
        try { const b = await s3.getBuffer(k); return b ? JSON.parse(b.toString()) : null; } catch { return null; }
      })));
    }
    const last = await s3.getBuffer(LAST_COFFEE).then((b) => (b ? JSON.parse(b.toString()) : null)).catch(() => null);
    return { on: true, levels: coffee.levels, last,
      members: list.filter(Boolean).map((m) => ({ email: m.email, level: m.level, siteLevel: memberLevel(m).level, known: memberLevel(m).known,
        status: m.status, cloud: memberOn(m), levelGives: levelGives(m),
        until: m.until, cancelAt: m.cancelAt, event: m.event, at: m.at })).sort((a, b) => String(b.at).localeCompare(String(a.at))) };
  }

  async function billingPortal(res, user) {
    const record = await account(user.sub, true);
    const customerId = record?.paddle?.customerId;
    if (!customerId) { send(res, 404, { error: 'no subscription on this account' }); return; }
    if (!paddle?.apiKey) { send(res, 501, { error: 'no portal on this server', url: 'https://paddle.net' }); return; }
    const url = await portalUrl({ apiKey: paddle.apiKey, apiBase: paddle.apiBase, customerId, subscriptionId: record.paddle.subscriptionId });
    send(res, 200, { url });
  }

  async function listSaves(user) {
    const objects = (await s3.list(`saves/${user.sub}/`)).filter((o) => o.key.endsWith('.gz'));
    const saves = await Promise.all(objects.map(async (o) => {
      const name = unb64u(o.key.slice(o.key.lastIndexOf('/') + 1, -3));
      const h = await s3.head(o.key);
      if (!h || !validSaveName(name)) return null;
      return { name, etag: h.etag, size: h.size, rawSize: Number(h.metadata['raw-size']) || null,
        saved: Number(h.metadata.saved) || Date.parse(o.modified) || null };
    }));
    return saves.filter(Boolean).sort((a, b) => (b.saved ?? 0) - (a.saved ?? 0));
  }

  // A save through this server goes to a file on its disk, is checked from there and streamed to the store: the
  // machine's memory holds none of it. At most two of the largest on disk at once; past that, 503 and the next sync.
  let spooled = 0;
  async function putSave(req, res, user, name) {
    const length = Number(req.headers['content-length']);
    if (length > maxPacked) { req.resume(); send(res, 413, { error: tooPacked }); return; }
    const n = Math.min(length || maxPacked, maxPacked);
    if (spooled + n > 2 * maxPacked && spooled > 0) {
      req.resume();
      res.writeHead(503, { 'Content-Type': 'application/json', 'Retry-After': '30', 'Cache-Control': 'no-store' });
      res.end(JSON.stringify({ error: 'the server is busy with other uploads: it goes at the next sync' }));
      return;
    }
    spooled += n;
    let file = null;
    try {
      file = await spool(req, maxPacked);
      await storeSave(req, res, user, name, file);
    } finally { spooled -= n; unspool(file); }
  }

  async function storeSave(req, res, user, name, file) {
    const { why, total } = await inspectSave(file.path, maxRaw);
    if (why) { send(res, 422, { error: `${name} was not stored: ${why}` }); return; }
    const key = saveKey(user.sub, name);
    const current = await s3.head(key);
    const base = req.headers['x-base-etag'];
    if (base !== undefined && (base === 'none' ? current !== null : current?.etag !== base)) {
      send(res, 409, { error: 'changed', cloud: current && { etag: current.etag, rawSize: Number(current.metadata['raw-size']) || null,
        saved: Number(current.metadata.saved) || null } });
      return;
    }
    if (!current && (await s3.list(`saves/${user.sub}/`)).length >= maxSaves) {
      send(res, 507, { error: `the cloud holds ${maxSaves} saves at most: remove one first` });
      return;
    }
    const savedAt = Number(req.headers['x-save-time']);
    const saved = Number.isFinite(savedAt) && savedAt > 0 ? Math.floor(savedAt) : Date.now();
    const etag = await s3.putFile(key, file, { type: 'application/gzip', metadata: { 'raw-size': total, saved } });
    send(res, 200, { name, etag, size: file.size, rawSize: total, saved });
  }

  // The direct steps of a save (direct above). ?direct=put (POST { size, rawSize, saved, base }): the checks of an
  // upload before it goes, and a link to send it with. ?direct=done (POST { key, base }): the upload is checked and takes
  // its place. ?direct=get (GET): a link to get it with. With direct off each answers { direct: false }.
  async function directSave(req, res, user, name, how, method) {
    const body = method === 'POST' ? await readBody(req, 4096).then((b) => JSON.parse(b.toString() || '{}')).catch(() => null) : null;
    if (!direct) { send(res, 200, { direct: false }); return; }
    const key = saveKey(user.sub, name);
    if (how === 'get' && method === 'GET') {
      const h = await s3.head(key);
      if (!h) { send(res, 404, { error: `${name} is not in the cloud` }); return; }
      send(res, 200, { direct: true, url: links.presign('GET', key).url, etag: h.etag, size: h.size,
        rawSize: Number(h.metadata['raw-size']) || null, saved: Number(h.metadata.saved) || null });
      return;
    }
    if (method !== 'POST' || !body) { send(res, 400, { error: 'POST a JSON body' }); return; }
    const conflict = async (base) => {
      const current = await s3.head(key);
      if (base !== undefined && (base === 'none' ? current !== null : current?.etag !== base)) {
        send(res, 409, { error: 'changed', cloud: current && { etag: current.etag, rawSize: Number(current.metadata['raw-size']) || null,
          saved: Number(current.metadata.saved) || null } });
        return true;
      }
      if (!current && (await s3.list(`saves/${user.sub}/`)).length >= maxSaves) {
        send(res, 507, { error: `the cloud holds ${maxSaves} saves at most: remove one first` });
        return true;
      }
      return false;
    };
    if (how === 'put') {
      const size = Math.floor(Number(body.size)), rawSize = Math.floor(Number(body.rawSize));
      const saved = Number.isFinite(Number(body.saved)) && Number(body.saved) > 0 ? Math.floor(Number(body.saved)) : Date.now();
      if (!(size > 0) || size > maxPacked) { send(res, 413, { error: tooPacked }); return; }
      if (!(rawSize > 0) || rawSize > maxRaw) { send(res, 422, { error: `${name} was not stored: it is larger than ${maxRaw >> 20} MB` }); return; }
      if (await conflict(body.base ?? undefined)) return;
      const upload = incomingKey(user.sub, 'gz');
      const link = links.presign('PUT', upload, { headers: { 'content-type': 'application/gzip', 'content-length': size,
        'x-amz-meta-raw-size': rawSize, 'x-amz-meta-saved': saved } });
      const { 'content-length': _, ...headers } = link.headers;    // the browser sets the length of the body itself
      send(res, 200, { direct: true, url: link.url, headers: Object.fromEntries(Object.entries(headers).map(([k, v]) => [k, String(v)])), key: upload });
      return;
    }
    if (how === 'done') {
      const upload = body.key;
      if (!incomingOf(user.sub, upload, 'gz')) { send(res, 400, { error: 'not an upload of this account' }); return; }
      const h = await s3.head(upload);
      if (!h) { send(res, 404, { error: 'the upload did not reach the cloud' }); return; }
      const total = Number(h.metadata['raw-size']), saved = Number(h.metadata.saved) || Date.now();
      const head = h.size <= maxPacked ? await gzipHead(upload, SAVE_HEADER) : null;
      const why = h.size > maxPacked ? `it is larger than ${mbOf(maxPacked)} MB, compressed` : head === null ? 'the upload is not gzip data'
        : checkSave(new Uint8Array(head.buffer, head.byteOffset, head.length), total);
      if (why) { await s3.delete(upload); send(res, 422, { error: `${name} was not stored: ${why}` }); return; }
      if (await conflict(body.base ?? undefined)) { await s3.delete(upload); return; }
      await s3.copy(upload, key);
      await s3.delete(upload);
      const now = await s3.head(key);
      send(res, 200, { name, etag: now.etag, size: now.size, rawSize: total, saved });
      return;
    }
    send(res, 400, { error: 'direct=put, done or get' });
  }

  // The first bytes of a gzip object of the store (its header), without the rest: null when it is not gzip data.
  async function gzipHead(key, want) {
    const r = await s3.get(key);
    if (!r) return null;
    return new Promise((resolveHead) => {
      const g = createGunzip(), parts = [];
      let n = 0, settled = false;
      const finish = (v) => { if (settled) return; settled = true; r.destroy(); g.destroy(); resolveHead(v); };
      g.on('data', (c) => { parts.push(c); n += c.length; if (n >= want) finish(Buffer.concat(parts)); });
      g.on('end', () => finish(Buffer.concat(parts)));
      g.on('error', () => finish(null));
      r.on('error', () => finish(null));
      r.pipe(g);
    });
  }

  // The direct steps of a data set, as directSave: put ({ size, saved, base }), done ({ key, base }: the zip is read
  // here for its check, at most MAX_SET_ZIP) and get.
  async function directSet(req, res, user, name, key, how, method) {
    const body = method === 'POST' ? await readBody(req, 4096).then((b) => JSON.parse(b.toString() || '{}')).catch(() => null) : null;
    if (!direct) { send(res, 200, { direct: false }); return; }
    if (how === 'get' && method === 'GET') {
      const h = await s3.head(key);
      if (!h) { send(res, 404, { error: `${name} is not in the cloud` }); return; }
      send(res, 200, { direct: true, url: links.presign('GET', key).url, etag: h.etag, size: h.size, saved: Number(h.metadata.saved) || null });
      return;
    }
    if (method !== 'POST' || !body) { send(res, 400, { error: 'POST a JSON body' }); return; }
    const conflict = async (base) => {
      const current = await s3.head(key);
      if (base !== undefined && (base === 'none' ? current !== null : current?.etag !== base)) { send(res, 409, { error: 'changed' }); return true; }
      if (!current && (await s3.list(`sets/${user.sub}/`)).length >= maxSets) {
        send(res, 507, { error: `the cloud holds ${maxSets} data sets at most: remove one first` });
        return true;
      }
      return false;
    };
    if (how === 'put') {
      const size = Math.floor(Number(body.size));
      const saved = Number.isFinite(Number(body.saved)) && Number(body.saved) > 0 ? Math.floor(Number(body.saved)) : Date.now();
      if (!(size > 0) || size > MAX_SET_ZIP) { send(res, 413, { error: `a data set may be ${MAX_SET_ZIP >> 20} MB at most, zipped` }); return; }
      if (await conflict(body.base ?? undefined)) return;
      const upload = incomingKey(user.sub, 'zip');
      const link = links.presign('PUT', upload, { headers: { 'content-type': 'application/zip', 'content-length': size, 'x-amz-meta-saved': saved } });
      const { 'content-length': _, ...headers } = link.headers;
      send(res, 200, { direct: true, url: link.url, headers: Object.fromEntries(Object.entries(headers).map(([k, v]) => [k, String(v)])), key: upload });
      return;
    }
    if (how === 'done') {
      const upload = body.key;
      if (!incomingOf(user.sub, upload, 'zip')) { send(res, 400, { error: 'not an upload of this account' }); return; }
      const h = await s3.head(upload);
      if (!h) { send(res, 404, { error: 'the upload did not reach the cloud' }); return; }
      const saved = Number(h.metadata.saved) || Date.now();
      const { why, rawSize } = h.size > MAX_SET_ZIP ? { why: `it is larger than ${MAX_SET_ZIP >> 20} MB, zipped` } : inspectSetZip(await s3.getBuffer(upload));
      if (why) { await s3.delete(upload); send(res, 422, { error: `${name} was not stored: ${why}` }); return; }
      if (await conflict(body.base ?? undefined)) { await s3.delete(upload); return; }
      await s3.copy(upload, key, { type: 'application/zip', metadata: { 'raw-size': rawSize, saved } });
      await s3.delete(upload);
      const now = await s3.head(key);
      send(res, 200, { name, etag: now.etag, size: now.size, rawSize, saved });
      return;
    }
    send(res, 400, { error: 'direct=put, done or get' });
  }

  async function getSave(res, user, name) {
    const r = await s3.get(saveKey(user.sub, name));
    if (!r) { send(res, 404, { error: `${name} is not in the cloud` }); return; }
    res.writeHead(200, { 'Content-Type': 'application/gzip', 'Content-Length': r.headers['content-length'], ETag: r.headers.etag,
      'X-Raw-Size': r.headers['x-amz-meta-raw-size'] ?? '', 'X-Saved': r.headers['x-amz-meta-saved'] ?? '', 'Cache-Control': 'no-store' });
    r.pipe(res);
  }

  // The data sets of an account: zips of the 20 database files (datasets.js), metadata raw-size and saved.
  async function dataSets(req, res, user, path, method, url) {
    if (path === '/api/datasets') {
      if (method !== 'GET') { send(res, 405, { error: 'GET only' }); return; }
      const objects = (await s3.list(`sets/${user.sub}/`)).filter((o) => o.key.endsWith('.zip'));
      const sets = (await Promise.all(objects.map(async (o) => {
        const name = unb64u(o.key.slice(o.key.lastIndexOf('/') + 1, -4)), h = await s3.head(o.key);
        return h && validSetName(name) ? { name, etag: h.etag, size: h.size, rawSize: Number(h.metadata['raw-size']) || null, saved: Number(h.metadata.saved) || null } : null;
      }))).filter(Boolean).sort((a, b) => a.name.localeCompare(b.name));
      send(res, 200, { sets, maxSets, store: await theStore() });
      return;
    }
    let name;
    try { name = decodeURIComponent(path.slice('/api/datasets/'.length)); } catch { name = null; }
    if (!validSetName(name)) { req.resume(); send(res, 400, { error: 'not a data set name' }); return; }
    const key = setKey(user.sub, name);
    if (url.searchParams.has('direct')) return directSet(req, res, user, name, key, url.searchParams.get('direct'), method);
    if (method === 'GET') {
      const r = await s3.get(key);
      if (!r) { send(res, 404, { error: `${name} is not in the cloud` }); return; }
      res.writeHead(200, { 'Content-Type': 'application/zip', 'Content-Length': r.headers['content-length'], ETag: r.headers.etag,
        'X-Saved': r.headers['x-amz-meta-saved'] ?? '', 'Cache-Control': 'no-store' });
      r.pipe(res);
      return;
    }
    if (method === 'DELETE') { await s3.delete(key); send(res, 204); return; }
    if (method !== 'PUT') { send(res, 405, { error: 'GET, PUT or DELETE' }); return; }
    if (Number(req.headers['content-length']) > MAX_SET_ZIP) { req.resume(); send(res, 413, { error: `a data set may be ${MAX_SET_ZIP >> 20} MB at most, zipped` }); return; }
    const free = hold(req, res, MAX_SET_ZIP);
    if (!free) return;
    try { await storeSet(req, res, user, name, key); } finally { free(); }
  }

  async function storeSet(req, res, user, name, key) {
    const body = await readBody(req, MAX_SET_ZIP);
    const { why, rawSize } = inspectSetZip(body);
    if (why) { send(res, 422, { error: `${name} was not stored: ${why}` }); return; }
    const current = await s3.head(key), base = req.headers['x-base-etag'];
    if (base !== undefined && (base === 'none' ? current !== null : current?.etag !== base)) { send(res, 409, { error: 'changed' }); return; }
    if (!current && (await s3.list(`sets/${user.sub}/`)).length >= maxSets) {
      send(res, 507, { error: `the cloud holds ${maxSets} data sets at most: remove one first` });
      return;
    }
    const savedAt = Number(req.headers['x-save-time']);
    const saved = Number.isFinite(savedAt) && savedAt > 0 ? Math.floor(savedAt) : Date.now();
    const etag = await s3.put(key, body, { type: 'application/zip', metadata: { 'raw-size': rawSize, saved } });
    send(res, 200, { name, etag, size: body.length, rawSize, saved });
  }

  // The tactics of an account (tacticsync.js): one record, tactics/<sub>.json, { tactics: [{ name, data (base64),
  // mtime }], removed: { name: time } }, written only over the version the page last read (x-base-etag; 409 else), so
  // two devices merge instead of writing over each other.
  const tacticsKey = (sub) => `tactics/${sub}.json`;
  async function tactics(req, res, user, method) {
    const key = tacticsKey(user.sub);
    if (method === 'GET') {
      const r = await s3.get(key);
      if (!r) { send(res, 200, { tactics: [], removed: {}, etag: null, maxTactics: MAX_TACTICS }); return; }
      const chunks = [];
      for await (const c of r) chunks.push(c);
      const kept = JSON.parse(Buffer.concat(chunks).toString());
      send(res, 200, { tactics: kept.tactics ?? [], removed: kept.removed ?? {}, etag: r.headers.etag, maxTactics: MAX_TACTICS });
      return;
    }
    if (method !== 'PUT') { req.resume(); send(res, 405, { error: 'GET or PUT' }); return; }
    let body;
    try { body = JSON.parse((await readBody(req, (MAX_TACTICS + 1) * (MAX_TACTIC * 2) + (1 << 20))).toString()); }
    catch (err) { send(res, err.status === 413 ? 413 : 400, { error: err.status === 413 ? 'too many tactics' : 'not JSON' }); return; }
    const list = Array.isArray(body?.tactics) ? body.tactics : null, removed = body?.removed;
    if (!list || typeof removed !== 'object' || removed === null || Array.isArray(removed)) { send(res, 400, { error: 'tactics and removed, please' }); return; }
    if (list.length > MAX_TACTICS) { send(res, 413, { error: `the cloud keeps ${MAX_TACTICS} tactics at most` }); return; }
    const clean = [];
    for (const t of list) {
      const data = typeof t?.data === 'string' ? Buffer.from(t.data, 'base64') : null;
      if (!data || !validTactic(t?.name, data.length) || !(Number(t.mtime) > 0)) {
        send(res, 422, { error: `${String(t?.name).slice(0, 100)} is not a tactic the cloud keeps` });
        return;
      }
      clean.push({ name: t.name, data: data.toString('base64'), mtime: Math.floor(Number(t.mtime)) });
    }
    // The removals kept are the newest of the last 400 days (pruneRemoved, as the page prunes them before it compares).
    const gone = pruneRemoved(Object.fromEntries(Object.entries(removed).filter(([n, at]) => validTactic(n, 1) && Number(at) > 0)
      .map(([n, at]) => [n, Math.floor(Number(at))])));
    const current = await s3.head(key), base = req.headers['x-base-etag'];
    if (base === undefined || (base === 'none' ? current !== null : current?.etag !== base)) { send(res, 409, { error: 'changed' }); return; }
    const etag = await s3.put(key, Buffer.from(JSON.stringify({ tactics: clean, removed: gone })), { type: 'application/json' });
    send(res, 200, { etag });
  }

  async function deleteAccount(req, res, user) {
    // A subscription that runs on would be paid for an account that is gone: it is canceled first.
    const record = await account(user.sub, true);
    const p = record?.paddle;
    if (p && ['active', 'trialing', 'past_due'].includes(p.status) && !p.cancelAt) {
      send(res, 409, { error: 'Cancel your membership first (Manage subscription), then delete the account.' });
      return;
    }
    const m = await member(user.email, true).catch(() => null);
    if (m?.on && !m.cancelAt && m.status === 'active') {
      send(res, 409, { error: 'Cancel your Buy Me a Coffee membership first (on Buy Me a Coffee), then delete the account.' });
      return;
    }
    if (m) { await s3.delete(memberKey(user.email)); members.delete(normalEmail(user.email)); }
    for (const o of await s3.list(`saves/${user.sub}/`)) await s3.delete(o.key);
    for (const o of await s3.list(`sets/${user.sub}/`)) await s3.delete(o.key);
    for (const o of await s3.list(`incoming/${user.sub}/`)) await s3.delete(o.key);
    await s3.delete(tacticsKey(user.sub));
    await s3.delete(grantKey(user.sub));
    await s3.delete(accountKey(user.sub));
    accounts.delete(user.sub);
    setCookie(res, SESSION, null, { secure: secure(req) });
    log(`cloud: account ${user.sub} deleted`);
    try { await onAccountDeleted(user.sub); } catch (err) { log(`cloud: after the deletion of ${user.sub}: ${err.message}`); }
    send(res, 204);
  }

  async function handle(req, res, url) {
    const path = url.pathname, method = req.method;
    if (path === '/api/paddle/webhook' && method === 'POST') return paddleWebhook(req, res);    // signed by Paddle instead
    if (path === '/api/coffee/webhook' && method === 'POST') return coffeeWebhook(req, res);    // signed by Buy Me a Coffee
    // A change comes only from this site's page: a header that a form or another site cannot send without asking
    // first, and the Origin of this site when the browser names one. The cookies are SameSite=Lax as well.
    if (method !== 'GET' && method !== 'HEAD') {
      const from = req.headers.origin;
      if (req.headers['x-cm'] !== '1' || (from && from !== origin(req))) { req.resume(); send(res, 403, { error: 'not from this site' }); return; }
    }
    if (path === '/api/me' && method === 'GET') return me(req, res);
    if (path === '/auth/google/start' && method === 'GET') return signInStart(req, res, url);
    if (path === '/auth/google/callback' && method === 'GET') return signInCallback(req, res, url);
    if (path === '/auth/logout' && method === 'POST') { setCookie(res, SESSION, null, { secure: secure(req) }); send(res, 204); return; }

    const user = sessionOf(req);
    if (path === '/api/billing/portal' && method === 'POST') {
      if (!user) { send(res, 401, { error: 'not signed in' }); return; }
      return billingPortal(res, user);
    }
    if (path === '/api/account' && method === 'DELETE') {
      if (!user) { send(res, 401, { error: 'not signed in' }); return; }
      return deleteAccount(req, res, user);
    }
    if (path === '/api/tactics') {
      if (!user) { req.resume(); send(res, 401, { error: 'not signed in' }); return; }
      if (!(await hasCloud(user))) { req.resume(); send(res, 403, { error: 'cloud saves are not on this account\'s plan' }); return; }
      return tactics(req, res, user, method);
    }
    if (path === '/api/datasets' || path.startsWith('/api/datasets/')) {
      if (!user) { req.resume(); send(res, 401, { error: 'not signed in' }); return; }
      if (!(await hasCloud(user))) { req.resume(); send(res, 403, { error: 'cloud saves are not on this account\'s plan' }); return; }
      return dataSets(req, res, user, path, method, url);
    }
    if (path === '/api/saves' || path === '/api/saves/' || path.startsWith('/api/saves/')) {
      if (!user) { req.resume(); send(res, 401, { error: 'not signed in' }); return; }
      if (!(await hasCloud(user))) { req.resume(); send(res, 403, { error: 'cloud saves are not on this account\'s plan' }); return; }
      if (path === '/api/saves') {
        if (method !== 'GET') { send(res, 405, { error: 'GET only' }); return; }
        send(res, 200, { saves: await listSaves(user), maxSaves, maxPacked, maxRaw, store: await theStore() });
        return;
      }
      let name;
      try { name = decodeURIComponent(path.slice('/api/saves/'.length)); } catch { name = null; }
      if (!validSaveName(name)) { req.resume(); send(res, 400, { error: 'not a save name' }); return; }
      if (url.searchParams.has('direct')) return directSave(req, res, user, name, url.searchParams.get('direct'), method);
      if (method === 'GET') return getSave(res, user, name);
      if (method === 'PUT') return putSave(req, res, user, name);
      if (method === 'DELETE') { await s3.delete(saveKey(user.sub, name)); send(res, 204); return; }
      send(res, 405, { error: 'GET, PUT or DELETE' });
      return;
    }
    send(res, 404, { error: 'not found' });
  }

  return {
    // The account signed in on this request ({ sub, email, name }), or null: statistics and the admin page.
    userOf: (req) => sessionOf(req),
    // Resolves once the store's CORS rules are set, or found not to be: whether the page may go to the store directly.
    directReady: directReady.then(() => direct),
    // Whether pages go straight to the store now (a later check may turn it on), and the address their links name
    // (null: the configured one).
    isDirect: () => direct,
    linkName: () => linkName,
    // A link of the store at that address (a sent report's save, stats.mjs).
    presign: (method, key, opts) => links.presign(method, key, opts),
    planOf,
    planOfSub,
    give,
    memberships,
    async handle(req, res, url) {
      try {
        await handle(req, res, url);
      } catch (err) {
        if (res.headersSent) { res.destroy(err); return; }
        if (err.status === 413) { send(res, 413, { error: url.pathname.startsWith('/api/datasets') ? `a data set may be ${MAX_SET_ZIP >> 20} MB at most, zipped` : tooPacked }); return; }
        log(`cloud: ${req.method} ${url.pathname}: ${err.message}`);
        send(res, err instanceof S3Error || err.code === 'ECONNREFUSED' || /S3/.test(err.message) ? 502 : 500,
          { error: err instanceof S3Error ? 'the cloud store did not take the request: try again later' : 'the server failed: try again later' });
      }
    },
  };
}

// The other addresses of a Tigris bucket for the pages' links, in the order they are tried after the configured one:
// t3.storage.dev (Tigris's canonical endpoint) with the bucket in the host, then in the path, then
// fly.storage.tigris.dev (which `fly storage create` sets) the other way round from the configured one. Each is
// { s3, name }; none for any other store. All reach the same bucket with the same key (docs.fly.io/tigris).
const T3 = 'https://t3.storage.dev', FLY_TIGRIS = 'https://fly.storage.tigris.dev';
export function linkStoresOf(env = process.env) {
  const now = String(env.AWS_ENDPOINT_URL_S3 ?? '').replace(/\/+$/, '');
  if (now !== T3 && now !== FLY_TIGRIS) return [];
  const path = env.CM_S3_PATH_STYLE === '1';
  const ways = [[T3, false], [T3, true], [FLY_TIGRIS, false], [FLY_TIGRIS, true]].filter(([e, p]) => !(e === now && p === path));
  return ways.map(([endpoint, p]) => ({
    s3: s3FromEnv({ ...env, AWS_ENDPOINT_URL_S3: endpoint, CM_S3_PATH_STYLE: p ? '1' : '0' }),
    name: `${endpoint}${p ? ', the bucket in the path' : ''}`,
  })).filter((w) => w.s3);
}

// The origins the bucket's CORS rules let in: CM_CORS_ORIGINS (comma separated), else CM_PUBLIC_URL and the app's
// fly.dev address (FLY_APP_NAME, which Fly sets), else any (a link of presign is the key to its one object anyway).
export function corsOrigins(env = process.env) {
  const named = env.CM_CORS_ORIGINS ? env.CM_CORS_ORIGINS.split(',') : [env.CM_PUBLIC_URL, env.FLY_APP_NAME && `https://${env.FLY_APP_NAME}.fly.dev`];
  const list = [...new Set(named.map((o) => (o ?? '').trim().replace(/\/+$/, '')).filter(Boolean))];
  return list.length ? list : ['*'];
}

// The cloud of the environment, or null and the settings that are missing.
// isAdmin(email): whether the account may open the admin page (server.mjs: CM_ADMIN_EMAILS).
export function cloudFromEnv(env = process.env, log, { onAccountDeleted, isAdmin } = {}) {
  const missing = [];
  for (const k of ['GOOGLE_CLIENT_ID', 'GOOGLE_CLIENT_SECRET', 'CM_SESSION_SECRET', 'AWS_ENDPOINT_URL_S3', 'AWS_ACCESS_KEY_ID',
    'AWS_SECRET_ACCESS_KEY', 'BUCKET_NAME']) if (!env[k]) missing.push(k);
  if (env.CM_SESSION_SECRET && env.CM_SESSION_SECRET.length < 32) missing.push('CM_SESSION_SECRET (32 characters or more)');
  if (missing.length) return { cloud: null, missing };
  const cloud = createCloud({
    s3: s3FromEnv(env), clientId: env.GOOGLE_CLIENT_ID, clientSecret: env.GOOGLE_CLIENT_SECRET, sessionSecret: env.CM_SESSION_SECRET,
    publicUrl: env.CM_PUBLIC_URL ? env.CM_PUBLIC_URL.replace(/\/+$/, '') : null,
    cloudFor: env.CM_CLOUD_FOR === 'everyone' ? 'everyone' : 'listed', cloudUsers: (env.CM_CLOUD_USERS ?? '').split(','),
    upgradeUrl: env.CM_UPGRADE_URL || null, maxSaves: Number(env.CM_CLOUD_MAX_SAVES) || 30, maxSets: Number(env.CM_CLOUD_MAX_SETS) || 5, log,
    maxPacked: (Number(env.CM_CLOUD_MAX_PACKED_MB) || MAX_GZ >> 20) << 20, maxRaw: (Number(env.CM_CLOUD_MAX_RAW_MB) || MAX_RAW >> 20) << 20,
    price: env.CM_PRICE || '$5 a month', paddle: paddleFromEnv(env), coffee: coffeeFromEnv(env), onAccountDeleted, isAdmin,
    uploadBudget: (Number(env.CM_UPLOAD_BUDGET_MB) || 160) << 20,
    // Direct to the store (createCloud, "direct"): off with CM_DIRECT=off; on without setting the rules with CM_DIRECT=on
    // (they were set in the store's console). The bucket's CORS rules let in the site's own addresses: CM_PUBLIC_URL
    // and the app's fly.dev address, or CM_CORS_ORIGINS (comma separated) in their place.
    direct: env.CM_DIRECT === 'off' ? false : env.CM_DIRECT === 'on' ? 'given' : true,
    origins: corsOrigins(env),
    linkStores: linkStoresOf(env),
  });
  return { cloud, missing };
}

// Paddle's settings, or null (no checkout) when one of the three it needs is missing.
export function paddleFromEnv(env = process.env) {
  if (!env.PADDLE_CLIENT_TOKEN || !env.PADDLE_PRICE_ID || !env.PADDLE_WEBHOOK_SECRET) return null;
  const sandbox = env.PADDLE_ENV === 'sandbox';
  return { env: sandbox ? 'sandbox' : 'production', clientToken: env.PADDLE_CLIENT_TOKEN,
    priceIds: env.PADDLE_PRICE_ID.split(',').map((x) => x.trim()).filter(Boolean), webhookSecret: env.PADDLE_WEBHOOK_SECRET,
    apiKey: env.PADDLE_API_KEY || null, apiBase: PADDLE_API[sandbox ? 'sandbox' : 'production'] };
}

// Buy Me a Coffee's settings, or null (no membership on sale): BMC_WEBHOOK_SECRET, the webhook's signing secret, and
// the membership page, BMC_MEMBERSHIP_URL or else CM_COFFEE_URL's /membership. CM_LEVELS (comma separated): the
// level names or ids, lowest to highest, that are Supporter, Squad Player and Club Legend (levels.mjs); the older
// BMC_CLOUD_LEVELS alone still works.
export function coffeeFromEnv(env = process.env) {
  const page = (env.CM_COFFEE_URL ?? '').trim().replace(/\/+$/, '');
  const url = (env.BMC_MEMBERSHIP_URL ?? '').trim() || (page ? `${page}/membership` : '');
  if (!env.BMC_WEBHOOK_SECRET || !/^https:\/\//.test(url)) return null;
  return { secret: env.BMC_WEBHOOK_SECRET, url, levels: levelsFromEnv(env) };
}
