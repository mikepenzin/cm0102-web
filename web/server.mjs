// The web app: serves the browser build of the recompiled game and the player's game files to a page that runs the
// game in the browser (web/README.md). The browser does all the work; this server only hands out files.
//
//   CM_GAME_DIR=<clean files> node web/server.mjs        then open http://127.0.0.1:8080 in Chrome
//
// CM_GAME_DIR  the clean files: the free release with the official patch (tools/clean_files/make_clean_files.py),
//              or the game folder of a Bosman Edition download, which holds the same files
// CM_WEB_BUILD the browser build folder (default build-web): cm0102_recomp.js, .wasm and seasons/*.tsv
// CM_WEB_BUILD_ASYNCIFY  a second build folder, of the Asyncify build (default build-web-asyncify, when it is there):
//              the page of a browser without JSPI (Safari, every iPad browser) runs that one (web/engine.js)
// PORT, HOST   default 8080 and 127.0.0.1
//
// The page gets exactly the files of tools/clean_files/manifest.tsv, each checked for its size, and the season
// tables that the build makes (Seasons/*.tsv, which the runtime reads from the game folder). Nothing else is served.
//
// Hosting (web/deploy): a file with a <file>.br beside it goes out brotli-compressed to a browser that takes br, and a
// request whose ?v= names the file's version (files.json) may be kept by the browser for good: the version changes
// when the file does.
import http from 'node:http';
import { createHash } from 'node:crypto';
import { createReadStream, existsSync, readdirSync, readFileSync, statSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { cloudFromEnv, coffeeFromEnv, paddleFromEnv } from './cloud.mjs';
import { createLibrary } from './library.mjs';
import { createEarly, isEarly, todayUTC, validDay } from './early.mjs';
import { MAX_SENT_SAVE, createStats, statsStore } from './stats.mjs';
import { createNotifier } from './notify.mjs';
import { s3FromEnv } from './s3.mjs';
import { spool, unspool } from './spool.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(HERE, '..');
const SEASON_TABLES = ['rules.tsv', 'names.tsv', 'clubs.tsv', 'club_records.tsv'];
// The page: these files of web/, and nothing else of it. web/deploy/stage.mjs stages the same list, and the server's own.
export const PAGE_FILES = ['index.html', 'loader.js', 'layout.js', 'touch.js', 'keyboard.js', 'saves.js', 'ui.js', 'account.js', 'savedit.js', 'dataset.js', 'datasets.js', 'zip.js', 'editor.js', 'setspanel.js', 'tactics.js', 'report.js', 'play.js', 'release.js', 'progress.js', 'tacticsync.js', 'room.js', 'upload.js', 'lastvisit.js', 'tour.js', 'scout.js', 'shortlist.js', 'features.js', 'early.js', 'i18n.js', 'i18n-es.js', 'i18n-pt.js', 'i18n-it.js', 'i18n-fr.js', 'i18n-de.js', 'i18n-nl.js', 'i18n-da.js', 'i18n-no.js', 'i18n-sv.js', 'disk.js', 'vault.js', 'stats.js', 'engine.js', 'direct.js', 'sw.js',
  'manifest.webmanifest', 'icon.svg', 'icon-180.png', 'icon-192.png', 'icon-512.png', 'privacy.html', 'terms.html', 'refunds.html', 'pricing.html'];
// The server's own files; it also imports saves.js, savedit.js, dataset.js and zip.js of the page (PAGE_FILES).
export const SERVER_FILES = ['server.mjs', 'cloud.mjs', 'levels.mjs', 'early.mjs', 'library.mjs', 'rgn.mjs', 'auth.mjs', 'billing.mjs', 's3.mjs', 'stats.mjs', 'notify.mjs', 'spool.mjs', 'admin.html', 'package.json'];
const PAGES = Object.fromEntries([['/', 'index.html'], ...['privacy', 'terms', 'refunds', 'pricing'].map((p) => [`/${p}`, `${p}.html`]),
  ...PAGE_FILES.map((f) => [`/${f}`, f])]);
// The pages of the site's terms: the seller, the contact and the price come from the settings (CM_SELLER, CM_CONTACT,
// CM_PRICE), as {{SELLER}}, {{CONTACT}} and {{PRICE}} in them; {{MADE_BY}} is " by <seller>" in the footer, or nothing.
const TEMPLATED = new Set(['privacy.html', 'terms.html', 'refunds.html', 'pricing.html']);
// What the page keeps for a start without a connection (web/sw.js): itself, without the service worker's own file.
const SHELL = ['/', ...PAGE_FILES.filter((f) => f !== 'index.html' && f !== 'sw.js').map((f) => `/${f}`)];
const TYPES = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.wasm': 'application/wasm',
  '.webmanifest': 'application/manifest+json', '.svg': 'image/svg+xml', '.png': 'image/png' };

function manifest(path) {
  return readFileSync(path, 'utf8').split('\n').filter((l) => l && !l.startsWith('#')).map((l) => {
    const [sha256, size, file] = l.split('\t');
    return { path: file, size: Number(size), sha256 };
  });
}

// The files the page loads, the ones missing or of the wrong size, and the engine's files: engine and suspend of the
// main build, engines of every build there is (the main one first).
export function inventory({ gameDir, buildDir, asyncifyDir = null, manifestPath = join(ROOT, 'tools/clean_files/manifest.tsv') }) {
  const files = [], missing = [];
  for (const m of manifest(manifestPath)) {
    const disk = join(gameDir, m.path);
    let size = -1;
    try { size = statSync(disk).size; } catch { /* missing */ }
    if (size === m.size) files.push({ path: m.path, size, disk, v: m.sha256.slice(0, 16) });
    else missing.push({ path: m.path, want: m.size, have: size });
  }
  for (const name of SEASON_TABLES) {
    const disk = join(buildDir, 'seasons', name);
    if (existsSync(disk)) {
      const st = statSync(disk);
      files.push({ path: `Seasons/${name}`, size: st.size, disk, v: `${st.size.toString(36)}-${Math.floor(st.mtimeMs).toString(36)}` });
    }
    else missing.push({ path: `Seasons/${name}`, want: null, have: -1, hint: 'build the target seasons_rules' });
  }
  const main = engineIn(buildDir, '/engine/');
  const engines = [main, engineIn(asyncifyDir, '/engine/asyncify/')].filter((e) => e.js && e.wasm);
  return { files, missing, engine: { js: main.js, wasm: main.wasm }, suspend: main.suspend, engines };
}

// The engine of a build folder: its two files, how it suspends (CM0102_WEB_SUSPEND in CMakeCache.txt: jspi or
// asyncify), its version (the time of the .wasm) and the address the page loads it from.
function engineIn(dir, base) {
  const e = { base, dir, js: null, wasm: null, suspend: null, version: null };
  if (!dir) return e;
  for (const ext of ['js', 'wasm']) {
    const disk = join(dir, `cm0102_recomp.${ext}`);
    if (existsSync(disk)) e[ext] = { disk, size: statSync(disk).size, mtime: statSync(disk).mtimeMs };
  }
  try { e.suspend = /CM0102_WEB_SUSPEND:STRING=(\w+)/.exec(readFileSync(join(dir, 'CMakeCache.txt'), 'utf8'))?.[1] ?? null; }
  catch { /* no cache: not a CMake build folder */ }
  if (e.wasm) e.version = Math.floor(e.wasm.mtime).toString(36);
  return e;
}

// immutable: the request named the file's current version, so the browser may keep it for good.
// The release this server serves (/version.json, release.js): the page files and the engines, by name, size and time,
// so a deploy that changes neither keeps it and one that changes either gives a new one. Every machine of a deploy runs
// the same image, so they agree.
function releaseOf(inv) {
  const h = createHash('sha256');
  for (const f of PAGE_FILES) {
    try { const st = statSync(join(HERE, f)); h.update(`${f}:${st.size}:${Math.floor(st.mtimeMs)}\n`); } catch { h.update(`${f}:none\n`); }
  }
  for (const e of inv.engines) h.update(`${e.base}:${e.version}\n`);
  return h.digest('hex').slice(0, 12);
}

function sendFile(req, res, disk, type, immutable = false) {
  const br = /\bbr\b/.test(req.headers['accept-encoding'] ?? '') && existsSync(`${disk}.br`);
  const file = br ? `${disk}.br` : disk;
  const st = statSync(file);
  const etag = `"${st.size.toString(16)}-${Math.floor(st.mtimeMs).toString(16)}${br ? '-br' : ''}"`;
  const headers = { 'Content-Type': type, ETag: etag, Vary: 'Accept-Encoding',
    'Cache-Control': immutable ? 'public, max-age=31536000, immutable' : 'no-cache' };
  if (br) headers['Content-Encoding'] = 'br';
  if (req.headers['if-none-match'] === etag) { res.writeHead(304, headers); res.end(); return; }
  res.writeHead(200, { ...headers, 'Content-Length': st.size });
  if (req.method === 'HEAD') { res.end(); return; }
  createReadStream(file).pipe(res);
}

// The Buy Me a Coffee page (CM_COFFEE_URL), when it is an https address: the page links to it (the bar, Help).
export function coffeeUrl(v) {
  try { const u = new URL(String(v ?? '')); return u.protocol === 'https:' ? u.href.replace(/\/$/, u.pathname === '/' ? '/' : '') : null; }
  catch { return null; }
}
// The YouTube channel of the how-to videos (CM_YOUTUBE_URL), when it is an https address of YouTube: the page links to
// it (its footer, Help).
export function youtubeUrl(v) {
  const href = coffeeUrl(v);
  try { return href && /^(www\.|m\.)?youtube\.com$|^youtu\.be$/.test(new URL(href).hostname) ? href : null; }
  catch { return null; }
}

function sendTemplated(req, res, file, site) {
  const esc = (v) => String(v).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
  const contact = site.contact ? `<a href="mailto:${esc(site.contact)}">${esc(site.contact)}</a>` : 'the contact address on this site\'s project page';
  const body = Buffer.from(readFileSync(file, 'utf8').replaceAll('{{SELLER}}', esc(site.seller || 'the operator of this site'))
    .replaceAll('{{CONTACT}}', contact).replaceAll('{{PRICE}}', esc(site.price || '$5 a month'))
    .replaceAll('{{MADE_BY}}', site.seller ? ` by ${esc(site.seller)}` : ''));
  res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Content-Length': body.length, 'Cache-Control': 'no-cache' });
  res.end(req.method === 'HEAD' ? undefined : body);
}

// The routes of the statistics (stats.mjs): the page's heartbeats and error reports, and the admin page's data. A
// heartbeat or a report comes only from this site's page (the header x-cm, as the cloud wants it). The admin's data goes
// to a signed-in account of CM_ADMIN_EMAILS, or to a request with CM_ADMIN_TOKEN (Authorization: Bearer).
function readJson(req, limit) {
  return new Promise((ok, bad) => {
    const parts = [];
    let n = 0;
    req.on('data', (c) => { n += c.length; if (n > limit) { bad(Object.assign(new Error('too large'), { status: 413 })); req.destroy(); } else parts.push(c); });
    req.on('end', () => { try { ok(JSON.parse(Buffer.concat(parts).toString() || '{}')); } catch { bad(Object.assign(new Error('not JSON'), { status: 400 })); } });
    req.on('error', bad);
  });
}
let reportSpooled = 0;                               // bytes of sent saves on disk now (save-body), at most two of the largest
const json = (res, status, body) => {
  res.writeHead(status, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' });
  res.end(status === 204 ? undefined : JSON.stringify(body));
};
async function statsRoute(req, res, url, { stats, cloud, admins, adminToken, library, early }) {
  const path = url.pathname;
  const user = cloud?.userOf(req) ?? null;
  if (path === '/api/stats/choice') {
    // The player's answer to the question about the statistics, with no id: the count of answers on the admin page.
    if (req.method !== 'POST') { json(res, 405, { error: 'POST only' }); return; }
    const from = req.headers.origin;
    if (req.headers['x-cm'] !== '1' || (from && new URL(from).host !== req.headers.host)) { req.resume(); json(res, 403, { error: 'not from this site' }); return; }
    const body = await readJson(req, 1024);
    if (body.choice !== 'on' && body.choice !== 'off') { json(res, 400, { error: 'choice: on or off' }); return; }
    await stats.choice(body.choice === 'on');
    json(res, 204, null);
    return;
  }
  if (path === '/api/report/send' || path === '/api/report/save' || path === '/api/report/save-done' || path === '/api/report/save-body') {
    // A report the player sends (Send to the developer, report.js), and its save: from this site's page only. The save
    // goes straight to the store when the page may go there (the store's CORS rules: cloud.mjs, direct), else, or when
    // the store does not let the page in after all, through this server (save-body: the packed bytes, ?key=&token=).
    if (req.method !== 'POST') { json(res, 405, { error: 'POST only' }); return; }
    const from = req.headers.origin;
    if (req.headers['x-cm'] !== '1' || (from && new URL(from).host !== req.headers.host)) { req.resume(); json(res, 403, { error: 'not from this site' }); return; }
    if (path === '/api/report/save-body') {
      const length = Number(req.headers['content-length']);
      if (!(length > 0) || length > MAX_SENT_SAVE) { req.resume(); json(res, 413, { error: `a save may be ${MAX_SENT_SAVE >> 20} MB at most, packed` }); return; }
      // The bytes go to a file on disk (spool) only after the report, its token and the length were checked, and from
      // there to the store; the machine's memory holds none of it.
      let held = 0, file = null;
      try {
        const { status = 200, ...r } = await stats.saveBody({ key: url.searchParams.get('key'), token: url.searchParams.get('token') }, async () => {
          if (reportSpooled + length > 2 * MAX_SENT_SAVE) throw Object.assign(new Error('the server is taking other saves: try again in a minute'), { status: 503 });
          reportSpooled += length;
          held = length;
          file = await spool(req, length, 'cm0102-sent-');
          return file;
        }, length);
        if (!held) req.resume();
        json(res, status, r);
      } catch (err) {
        if (!held) req.resume();                         // refused before the body was read
        throw err;
      } finally {
        reportSpooled -= held;
        unspool(file);
      }
      return;
    }
    const body = await readJson(req, path === '/api/report/send' ? 2 << 20 : 4 << 10);
    if (path === '/api/report/send') {
      const { status, ...r } = await stats.send(body, user, req.headers['user-agent']);
      json(res, r.error ? status ?? 400 : 201, r);
    } else {
      const { status = 200, ...r } = path === '/api/report/save' ? await stats.saveLink(body, Boolean(await cloud?.directReady) || Boolean(cloud?.isDirect?.()), cloud?.presign ?? null) : await stats.saveDone(body);
      json(res, status, r);
    }
    return;
  }
  if (path === '/api/stats/beat' || path === '/api/report') {
    if (req.method !== 'POST') { json(res, 405, { error: 'POST only' }); return; }
    const from = req.headers.origin;
    if (req.headers['x-cm'] !== '1' || (from && new URL(from).host !== req.headers.host)) { req.resume(); json(res, 403, { error: 'not from this site' }); return; }
    const body = await readJson(req, path === '/api/report' ? 256 << 10 : 16 << 10);
    if (path === '/api/stats/beat') {
      const why = await stats.beat(body, user, req.headers['user-agent']);
      json(res, why ? 400 : 204, why ? { error: why } : null);
    } else {
      const r = await stats.report(body, user, req.headers['user-agent']);
      json(res, r.error ? 400 : 201, r);
    }
    return;
  }
  const bearer = /^Bearer (.+)$/.exec(req.headers.authorization ?? '')?.[1];
  const admin = (adminToken && bearer === adminToken) || (user && admins.has(String(user.email).toLowerCase()));
  if (!admin) { req.resume(); json(res, user || bearer ? 403 : 401, { error: user || bearer ? 'not an admin' : 'not signed in' }); return; }
  if (path === '/api/admin/stats' && req.method === 'GET') {
    const sum = await stats.summary();
    sum.people = sum.people.map((p) => ({ ...p }));              // copies: the plans below are not statistics
    sum.accountsOn = Boolean(cloud?.planOfSub);
    if (cloud?.planOfSub) {
      const accounts = sum.people.filter((p) => p.kind === 'account' && /^g:/.test(p.id));
      for (let i = 0; i < accounts.length; i += 8) {             // the account records, 8 at a time
        await Promise.all(accounts.slice(i, i + 8).map(async (p) => {
          try { p.plan = await cloud.planOfSub(p.id.slice(2)); } catch { p.plan = null; }
        }));
      }
    }
    json(res, 200, sum);
    return;
  }
  if (path === '/api/admin/early') {
    // Early access (early.mjs): each feature's day; PUT { key, day: 'YYYY-MM-DD' | null } changes one, only from the
    // admin page (or a script with the token).
    if (!early) { req.resume(); json(res, 404, { error: 'early access needs the store (BUCKET_NAME and its key)' }); return; }
    const list = (days) => early.features.map((f) => ({ key: f.key, name: f.name, day: days[f.key] ?? null, early: isEarly(days[f.key]) }));
    if (req.method === 'GET') { json(res, 200, { features: list(await early.dates(true)), today: todayUTC() }); return; }
    if (req.method !== 'PUT') { req.resume(); json(res, 405, { error: 'GET or PUT' }); return; }
    const from = req.headers.origin;
    if (req.headers['x-cm'] !== '1' || (from && new URL(from).host !== req.headers.host)) { req.resume(); json(res, 403, { error: 'not from this site' }); return; }
    const body = await readJson(req, 4 << 10);
    if (body.day !== null && !validDay(body.day)) { json(res, 400, { error: 'day: a day YYYY-MM-DD, or null to open it now' }); return; }
    try { json(res, 200, { features: list(await early.set(String(body.key ?? ''), body.day)), today: todayUTC() }); }
    catch (err) { json(res, err.status ?? 500, { error: err.message }); }
    return;
  }
  if (path === '/api/admin/members' && req.method === 'GET') {
    // The Buy Me a Coffee memberships the server was told of (cloud.mjs, memberships).
    json(res, 200, cloud?.memberships ? await cloud.memberships() : { on: false, levels: [], members: [], last: null });
    return;
  }
  if (path === '/api/admin/cloud' && req.method === 'POST') {
    // Gives an account a level (Squad Player or Club Legend), or takes the gift back. Only from the admin page.
    const from = req.headers.origin;
    if (req.headers['x-cm'] !== '1' || (from && new URL(from).host !== req.headers.host)) { req.resume(); json(res, 403, { error: 'not from this site' }); return; }
    const body = await readJson(req, 4 << 10);
    const sub = /^g:([0-9A-Za-z_-]{1,64})$/.exec(String(body.id ?? ''))?.[1];
    // { id, level: 'squad' | 'legend' | null }; the first version's { id, on } is Squad Player or none.
    const level = 'level' in body ? body.level : typeof body.on === 'boolean' ? (body.on ? 'squad' : null) : undefined;
    if (!sub || ![null, 'squad', 'legend'].includes(level)) { json(res, 400, { error: 'id (g:…) and level (squad, legend or null)' }); return; }
    if (!cloud?.give) { json(res, 404, { error: 'accounts are not set up on this server' }); return; }
    const plan = await cloud.give(sub, level, user?.email ?? 'the admin token');
    json(res, plan ? 200 : 404, plan ? { id: body.id, plan } : { error: 'no such account: it signs in once first' });
    return;
  }
  if (path === '/api/admin/library' || path.startsWith('/api/admin/library/')) {
    // The database library (library.mjs): a change only from the admin page (or a script with the token).
    if (!library) { req.resume(); json(res, 404, { error: 'the library needs the store (BUCKET_NAME and its key)' }); return; }
    const from = req.headers.origin;
    if (req.method !== 'GET' && !(adminToken && bearer === adminToken) && (req.headers['x-cm'] !== '1' || (from && new URL(from).host !== req.headers.host))) {
      req.resume(); json(res, 403, { error: 'not from this site' }); return;
    }
    await library.admin(req, res, url);
    return;
  }
  if (path === '/api/admin/report' && req.method === 'GET') {
    const r = await stats.reportFile(url.searchParams.get('key') ?? '');
    json(res, r ? 200 : 404, r ?? { error: 'no such report' });
    return;
  }
  json(res, 404, { error: 'not found' });
}

// cloud: accounts and cloud saves (cloud.mjs), or null for none. site: { seller, contact, price } for the terms pages.
// stats: the statistics (stats.mjs), or null for none; statsMode: 'optin' (the page asks first: nothing is sent or stored
// for them before a yes) or 'optout' (on unless the player turns them off). admins: the emails of the admin page; adminToken: a token for it instead.
export function createApp({ gameDir, buildDir, asyncifyDir = null, manifestPath, cloud = null, site = {}, stats = null, statsMode = 'optin', admins = [],
  adminToken = null, releaseCacheMs = 10000, library = null, early = null }) {
  let inv = inventory({ gameDir, buildDir, asyncifyDir, manifestPath });
  let released = { at: 0, release: null };              // /version.json's answer and when it was worked out
  const adminSet = new Set(admins.map((e) => e.trim().toLowerCase()).filter(Boolean));
  const token = adminToken && adminToken.length >= 16 ? adminToken : null;
  return http.createServer((req, res) => {
    try {
      const url = new URL(req.url, 'http://x');
      if (stats && (url.pathname === '/api/stats/beat' || url.pathname === '/api/stats/choice' || url.pathname === '/api/report'
        || url.pathname.startsWith('/api/report/') || url.pathname.startsWith('/api/admin/'))) {
        statsRoute(req, res, url, { stats, cloud, admins: adminSet, adminToken: token, library, early }).catch((err) => {
          if (res.headersSent) { res.destroy(err); return; }
          json(res, err.status ?? 500, { error: err.status ? err.message : 'the server failed' });
        });
        return;
      }
      if (url.pathname === '/healthz') {            // Fly's check (web/deploy/fly.toml): the server answers
        res.writeHead(200, { 'Content-Type': 'text/plain', 'Cache-Control': 'no-store' });
        res.end(req.method === 'HEAD' ? undefined : 'ok\n');
        return;
      }
      if (url.pathname === '/admin') { sendFile(req, res, join(HERE, 'admin.html'), TYPES['.html']); return; }
      if (library && (url.pathname === '/api/library' || url.pathname.startsWith('/api/library/'))) {   // for everyone (library.mjs)
        library.handle(req, res, url).catch((err) => {
          if (res.headersSent) { res.destroy(err); return; }
          json(res, err.status ?? 500, { error: err.status ? err.message : 'the server failed' });
        });
        return;
      }
      if (url.pathname.startsWith('/api/') || url.pathname.startsWith('/auth/')) {
        if (cloud) { cloud.handle(req, res, url); return; }
        req.resume();
        if (url.pathname === '/api/me') {                // no accounts: no one is a Club Legend, so early things wait for their day
          (early ? early.early().catch(() => ({})) : Promise.resolve({})).then((features) => json(res, 200, { auth: false, early: { legend: false, features } }));
          return;
        }
        json(res, 404, { error: 'accounts are not set up on this server' });
        return;
      }
      if (req.method !== 'GET' && req.method !== 'HEAD') { res.writeHead(405); res.end(); return; }
      const path = decodeURIComponent(url.pathname), v = url.searchParams.get('v');
      if (PAGES[path] && TEMPLATED.has(PAGES[path])) { sendTemplated(req, res, join(HERE, PAGES[path]), site); return; }
      if (PAGES[path]) { sendFile(req, res, join(HERE, PAGES[path]), TYPES[PAGES[path].slice(PAGES[path].lastIndexOf('.'))]); return; }
      if (path === '/version.json') {
        // Every open page asks every 10 minutes and when it comes back into view: the answer is kept 10 s, not worked
        // out (every game file looked at) for each.
        if (Date.now() - released.at >= releaseCacheMs) {
          inv = inventory({ gameDir, buildDir, asyncifyDir, manifestPath });
          released = { at: Date.now(), release: releaseOf(inv) };
        }
        res.writeHead(200, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' });
        res.end(JSON.stringify({ release: released.release }));
        return;
      }
      if (path === '/files.json') {
        inv = inventory({ gameDir, buildDir, asyncifyDir, manifestPath });   // a rebuild or new files count without a restart
        const body = JSON.stringify({
          files: inv.files.map(({ path: p, size, v: version }) => ({ path: p, size, v: version })),
          totalBytes: inv.files.reduce((a, f) => a + f.size, 0),
          missing: inv.missing,
          engine: Boolean(inv.engine.js && inv.engine.wasm),
          engineBytes: inv.engine.wasm?.size ?? 0,
          engineVersion: inv.engine.wasm ? Math.floor(inv.engine.wasm.mtime).toString(36) : null,
          suspend: inv.suspend,
          engines: inv.engines.map((e) => ({ base: e.base, suspend: e.suspend, version: e.version, bytes: e.wasm.size })),
          shell: SHELL,
          coffee: coffeeUrl(site.coffee),
          youtube: youtubeUrl(site.youtube),                                // the video guides (CM_YOUTUBE_URL)
          maker: site.seller ? String(site.seller).slice(0, 80) : null,
          library: Boolean(library),                                       // Get a database (library.mjs)      // the footer's "Made with ♥ by" (CM_SELLER)
          stats: stats ? { mode: statsMode } : null,
          release: releaseOf(inv),
        });
        res.writeHead(200, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' });
        res.end(body);
        return;
      }
      const asked = /^(\/engine\/(?:asyncify\/)?)cm0102_recomp\.(js|wasm)$/.exec(path);
      const build = asked && inv.engines.find((e) => e.base === asked[1]);
      if (build) {
        sendFile(req, res, build[asked[2]].disk, TYPES[`.${asked[2]}`], v !== null && v === build.version);
        return;
      }
      if (path.startsWith('/game/')) {
        const f = inv.files.find((x) => x.path === path.slice(6));    // only listed files: no path leaves the list
        if (f) { sendFile(req, res, f.disk, 'application/octet-stream', v !== null && v === f.v); return; }
      }
      res.writeHead(404, { 'Content-Type': 'text/plain' });
      res.end('not found');
    } catch (err) {
      res.writeHead(500, { 'Content-Type': 'text/plain' });
      res.end(err.message);
    }
  });
}

// The files of the manifest whose SHA-256 differs from it (a folder that is not the clean files).
export async function verify(inv, manifestPath = join(ROOT, 'tools/clean_files/manifest.tsv')) {
  const { createHash } = await import('node:crypto');
  const want = new Map(manifest(manifestPath).map((m) => [m.path, m.sha256]));
  const bad = [];
  for (const f of inv.files) {
    if (!want.has(f.path)) continue;                    // the season tables come from the build
    const h = createHash('sha256');
    for await (const chunk of createReadStream(f.disk)) h.update(chunk);
    if (h.digest('hex') !== want.get(f.path)) bad.push(f.path);
  }
  return bad;
}

// The folders under dir, up to depth levels down, that hold a cm0102.exe (any case): where a copy of the game folder
// landed when it went one folder deeper than meant.
export function findGameDirs(dir, depth = 3) {
  const found = [];
  const walk = (d, left) => {
    let entries;
    try { entries = readdirSync(d, { withFileTypes: true }); } catch { return; }
    if (entries.some((e) => e.isFile() && e.name.toLowerCase() === 'cm0102.exe')) found.push(d);
    if (left > 0) for (const e of entries) if (e.isDirectory() && !e.name.startsWith('.')) walk(join(d, e.name), left - 1);
  };
  walk(dir, depth);
  return found.sort((x, y) => x.split(/[\\/]/).length - y.split(/[\\/]/).length || x.localeCompare(y));   // the shallowest first
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const gameDir = process.env.CM_GAME_DIR ? resolve(process.env.CM_GAME_DIR) : null;
  if (!gameDir || !existsSync(join(gameDir, 'cm0102.exe'))) {
    if (!gameDir) console.error('Set CM_GAME_DIR to the folder of the clean files (with cm0102.exe and Data). See web/README.md.');
    else if (!existsSync(gameDir)) console.error(`CM_GAME_DIR: ${gameDir} does not exist. See web/README.md, step 1.`);
    else {
      const found = findGameDirs(gameDir).filter((d) => d !== gameDir);
      console.error(found.length ? `CM_GAME_DIR: ${gameDir} has no cm0102.exe, but ${found[0]} has. Use CM_GAME_DIR=${found[0]}`
        : `CM_GAME_DIR: ${gameDir} has no cm0102.exe, and no folder under it has one. Copy the game folder there (web/README.md, step 1).`);
    }
    process.exit(2);
  }
  const buildDir = resolve(process.env.CM_WEB_BUILD ?? join(ROOT, 'build-web'));
  const asyncifyDir = resolve(process.env.CM_WEB_BUILD_ASYNCIFY ?? join(ROOT, 'build-web-asyncify'));
  const inv = inventory({ gameDir, buildDir, asyncifyDir });
  const mb = (n) => `${(n / 1048576).toFixed(0)} MB`;
  console.log(`game files: ${inv.files.length} (${mb(inv.files.reduce((a, f) => a + f.size, 0))}) from ${gameDir}`);
  if (!process.env.CM_WEB_SKIP_VERIFY) {
    const bad = await verify(inv);
    if (bad.length) {
      console.error(`${bad.length} files differ from the clean files (tools/clean_files/manifest.tsv), first: ${bad.slice(0, 5).join(', ')}`);
      process.exit(3);
    }
    console.log('game files: every SHA-256 matches the clean files');
  }
  if (inv.missing.length) console.log(`missing or of another size: ${inv.missing.length}, first: ${inv.missing.slice(0, 5).map((m) => m.path).join(', ')}`);
  console.log(inv.engine.js && inv.engine.wasm ? `engine: ${buildDir}/cm0102_recomp.wasm (${mb(inv.engine.wasm.size)}, ${inv.suspend ?? 'unknown'} build)`
    : `engine: not built in ${buildDir} (web/README.md, step 3)`);
  const second = inv.engines.find((e) => e.dir === asyncifyDir);
  console.log(second ? `second engine, for browsers without JSPI: ${asyncifyDir}/cm0102_recomp.wasm (${mb(second.wasm.size)}, ${second.suspend ?? 'unknown'} build)`
    : 'second engine: none, so Safari and iPad browsers cannot play (web/README.md, "Other browsers")');
  // Statistics: in the bucket when there is one (BUCKET_NAME and the keys), else in memory only. CM_STATS=off: none.
  // A notice to the developer for each report a player sends (notify.mjs): CM_REPORT_WEBHOOK. Its link to the admin page
  // is CM_SITE_URL, else CM_PUBLIC_URL, else the app's address on Fly.
  const notify = createNotifier({ url: process.env.CM_REPORT_WEBHOOK ?? '' });
  const siteUrl = process.env.CM_SITE_URL ?? process.env.CM_PUBLIC_URL ?? (process.env.FLY_APP_NAME ? `https://${process.env.FLY_APP_NAME}.fly.dev` : '');
  const stats = process.env.CM_STATS === 'off' ? null : createStats({ store: statsStore(s3FromEnv(process.env)), notify, site: siteUrl });
  stats?.start();
  const admins = (process.env.CM_ADMIN_EMAILS ?? '').split(',');
  const adminEmails = new Set(admins.map((e) => e.trim().toLowerCase()).filter(Boolean));
  // Early access (early.mjs): the features' days in the bucket.
  const store = s3FromEnv(process.env);
  const early = store ? createEarly({ s3: store, log: (t) => console.log(t) }) : null;
  const { cloud, missing } = cloudFromEnv(process.env, undefined, { onAccountDeleted: (sub) => stats?.forget(sub),
    isAdmin: (email) => Boolean(stats) && adminEmails.has(email), early });
  const someSet = ['GOOGLE_CLIENT_ID', 'GOOGLE_CLIENT_SECRET', 'CM_SESSION_SECRET', 'BUCKET_NAME'].some((k) => process.env[k]);
  const paddle = paddleFromEnv(process.env), coffee = coffeeFromEnv(process.env);
  console.log(cloud ? `accounts and cloud saves: on${process.env.CM_CLOUD_FOR === 'everyone' ? ', for every account'
    : coffee ? `, sold as a Buy Me a Coffee membership (${coffee.levels.names ? `levels ${coffee.levels.names.join(' < ')}: Supporter < Squad Player < Club Legend, CM_LEVELS` : `levels that give Cloud Saves: ${coffee.levels.legacyCloud.join(', ') || 'every level'} (BMC_CLOUD_LEVELS: CM_LEVELS replaces it, web/deploy/README.md)`})${paddle ? '; Paddle keeps its subscriptions' : ''}`
    : paddle ? `, sold with Paddle (${paddle.env})` : ', no payments (BMC_WEBHOOK_SECRET and CM_COFFEE_URL, web/deploy/README.md)'}`
    : `accounts and cloud saves: off${someSet ? `, missing ${missing.join(', ')}` : ''} (web/README.md, "Cloud saves")`);
  const port = Number(process.env.PORT ?? 8080), host = process.env.HOST ?? '127.0.0.1';
  const site = { seller: process.env.CM_SELLER, contact: process.env.CM_CONTACT, price: process.env.CM_PRICE, coffee: process.env.CM_COFFEE_URL, youtube: process.env.CM_YOUTUBE_URL };
  if ((paddle || coffee) && (!site.seller || !site.contact))
    console.log('payments: set CM_SELLER and CM_CONTACT, which the terms, refunds and privacy pages name');
  console.log(stats ? `statistics: on (${process.env.CM_STATS === 'optout' ? 'opt-out' : 'opt-in: the page asks first'}), ${s3FromEnv(process.env) ? 'kept in the bucket' : 'in memory only'}; admin page /admin for ${admins.filter((e) => e.trim()).length} email(s)${process.env.CM_ADMIN_TOKEN ? ' and the token' : ''}`
    : 'statistics: off (CM_STATS=off)');
  if (stats) console.log(notify.on ? 'report notices: on (CM_REPORT_WEBHOOK)' : 'report notices: off (CM_REPORT_WEBHOOK, web/deploy/README.md)');
  // The database library: in the same bucket; its links name the address the pages reach (cloud.mjs presign).
  const library = store ? createLibrary({ s3: store, presign: (m, k, o) => (cloud?.presign ?? store.presign)(m, k, o),
    isDirect: () => Boolean(cloud?.isDirect?.()), log: (t) => console.log(t),
    // The asker's level: a database still early is for Club Legends (early.mjs, levels.mjs).
    levelOf: async (req) => { const u = cloud?.userOf(req); return u ? (await cloud.planOf(u)).level : 'free'; } }) : null;
  console.log(library ? 'database library: on (admin page, Library; players: Saved games, Data sets, Get a database)' : 'database library: off (it needs the bucket)');
  const server = createApp({ gameDir, buildDir, asyncifyDir, cloud, site, stats, statsMode: process.env.CM_STATS === 'optout' ? 'optout' : 'optin', admins,
    adminToken: process.env.CM_ADMIN_TOKEN ?? null, library, early });
  server.listen(port, host, () => console.log(`open http://${host}:${port} in Chrome`));
  // Fly stops a machine with SIGINT (fly.toml kill_signal) or SIGTERM: the statistics are written first.
  for (const sig of ['SIGINT', 'SIGTERM']) process.once(sig, async () => { await stats?.stop(); process.exit(0); });
}
