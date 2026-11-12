// The page side of the browser build (web/README.md): it downloads the game files, puts them into the engine's file
// system at /game, keeps the user folder /user (saves, settings.txt) in the browser, and starts the recompiled game
// on the canvas. The game runs in this tab; the server only hands out files.
//
// Around the game: layout.js sizes the canvas, touch.js turns touches into the mouse, keyboard.js routes the keys and
// runs the on-screen keyboard, saves.js is the Saved games panel, account.js the account and the cloud saves, ui.js
// has the messages and the small helpers.
import { SAVE_DIR, listSaves, mountSavesPanel, savesSignature, setStorageKept, skipWorkFiles, storageWarning, useVault } from './saves.js';
import { needsChoice, setStatsAllowed, startStats, statsAllowed } from './stats.js';
import { createVault } from './vault.js';
import { engineUrl, hasJspi, pickEngine } from './engine.js';
import { mountEditor } from './editor.js';
import { mountSetsPanel } from './setspanel.js';
import { createSetSync, listDataSets } from './datasets.js';
import { LEGAL_LINKS, createCloudSync, mountAccount, plansHtml } from './account.js';
import { guardKeys, hasTouch, mountKeyboard } from './keyboard.js';
import { mountLayout } from './layout.js';
import { mountTouch } from './touch.js';
import { guardLeave, isStandalone, keepAwake, mountBusy, toast } from './ui.js';

const $ = (id) => document.getElementById(id);
const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const mb = (n) => `${(n / 1048576).toFixed(n < 10485760 ? 1 : 0)} MB`;
const CONNECTIONS = 6;
const TOUCH = hasTouch();

const lines = [];
function log(text) {
  lines.push(String(text));
  if (lines.length > 3000) lines.splice(0, lines.length - 3000);
  $('log').textContent = lines.join('\n');
}

function status(text) { $('status').textContent = text; }

function overlay(html, kind = '') {
  $('overlay').innerHTML = html ? `<div class="box ${kind}">${html}</div>` : '';
  $('overlay').style.display = html ? '' : 'none';
}

// For the statistics (stats.js): how many saves, older copies and data sets the user folder holds, and their size.
function folderState() {
  const FS = window.Module?.FS;
  if (!FS?.readdir) return null;
  const { saves, backups } = listSaves(FS);
  const sets = listDataSets(FS, SAVE_DIR);
  const of = (list) => ({ n: list.length, bytes: list.reduce((a, f) => a + (f.size ?? 0), 0) });
  return { saves: of(saves), older: of(backups), sets: of(sets) };
}

// The choice in Help: send the statistics or not (stats.js keeps it in localStorage).
// The statistics: the player's answer in Help, and the question (the consent card) while there is none (stats.js).
function mountStatsChoice(info) {
  const box = $('help-stats'), input = $('help-stats-on'), card = $('consent');
  if (!box || !info.stats) return;
  const mode = info.stats.mode;
  box.hidden = false;
  input.checked = statsAllowed(mode);
  const answer = (on) => {
    setStatsAllowed(on, { tell: true });
    input.checked = on;
    if (card) card.hidden = true;
    log(`page: statistics ${on ? 'allowed' : 'not allowed'}`);
    if (on) stats?.allowed();
  };
  input.onchange = () => answer(input.checked);
  if (card && needsChoice(mode)) {
    card.hidden = false;
    $('consent-yes').onclick = () => answer(true);
    $('consent-no').onclick = () => answer(false);
  }
}

// What the user folder keeps in memory, for the log: the page holds all of it while the game runs.
function userFolderSizes(FS) {
  const sum = { saves: 0, older: 0, sets: 0, other: 0, stored: 0 };
  const walk = (dir, kind) => {
    let names = [];
    try { names = FS.readdir(dir).filter((n) => n !== '.' && n !== '..'); } catch { return; }
    for (const n of names) {
      const p = `${dir}/${n}`;
      let st;
      try { st = FS.stat(p); } catch { continue; }
      if (FS.isDir(st.mode)) walk(p, dir === SAVE_DIR && n === 'backups' ? 'older' : dir === SAVE_DIR && n === 'Databases' ? 'sets' : kind);
      else if ((() => { try { return FS.lookupPath(p).node.vault === 'out'; } catch { return false; } })()) sum.stored += st.size;  // a placeholder
      else sum[kind === 'other' && /\.sav$/i.test(n) ? 'saves' : kind] += st.size;
    }
  };
  walk(SAVE_DIR, 'other');
  const all = sum.saves + sum.older + sum.sets + sum.other;
  return `user folder in memory: ${mb(all)} (saves ${mb(sum.saves)}, older copies ${mb(sum.older)}, data sets ${mb(sum.sets)}, other ${mb(sum.other)}); in the browser's storage only: ${mb(sum.stored)}`;
}

// The browser had no memory for the page: it keeps the game files and everything of the user folder (saves, older
// copies, data sets) in memory while the game runs.
const OUT_OF_MEMORY = /Array buffer allocation failed|out of memory|Cannot allocate|could not allocate/i;
let stats = null;                                    // stats.js, once files.json told the server's mode
function fail(title, detail) {
  status(title);
  stats?.report(`${title}: ${detail}`, lines.slice(-60), 'fail');
  const last = lines.slice(-8).join('\n');
  const memory = OUT_OF_MEMORY.test(String(detail)) ? `<p>The browser ran out of memory for the game. The page keeps the game
    files and all your saves, older copies and data sets in memory while it runs. Close other tabs, reload the page, and
    in Saved games download and delete the saves and older copies you no longer play.</p>` : '';
  overlay(`<h1>${esc(title)}</h1><p>${esc(detail)}</p>${memory}${last ? `<pre class="small" style="white-space:pre-wrap;margin:0 0 14px">${esc(last)}</pre>` : ''}
    <div class="row"><button class="primary" id="fail-again">Start again</button><span class="small">Your saves stay in this browser.</span></div>`, 'error');
  // The way on: the page loads again, after the user folder went to the browser's storage.
  const again = $('fail-again');
  if (again) again.onclick = () => { running = false; leaving = true; sync(); setTimeout(() => location.reload(), 600); };
}

// The stack of an error, in the log: for a stop of the engine it names the game's functions (the build keeps their
// names, web/web.cmake --profiling-funcs: f_<address> is the original's function at that address), so the error
// report says where the game was. Chrome keeps 10 frames of a stack unless told more; the game's own frames are many.
const STACK_LINES = 30;
try { Error.stackTraceLimit = STACK_LINES; } catch { /* not Chrome: the browser keeps its own number */ }
function logStack(e) {
  const lines = String(e?.stack ?? '').split('\n').map((l) => l.trim()).filter(Boolean);
  for (const l of lines.slice(0, STACK_LINES)) log(`page: at ${l.replace(/^at /, '')}`);
}

addEventListener('error', (e) => { log(`page: ${e.message}`); logStack(e.error); });
addEventListener('unhandledrejection', (e) => log(`page: ${e.reason?.stack ?? e.reason}`));

// ---- Before the engine: the listeners that must run ahead of SDL's, and the layout. ----

let running = false, leaving = false;
let keyboard = null;
guardKeys();
guardLeave(() => running && !leaving);
const canvas = $('canvas');
const layout = mountLayout({ app: $('app'), bar: $('bar'), stage: $('stage'), canvas, chip: $('type-chip'), isTouch: TOUCH });
mountTouch({ canvas, stage: $('stage'), fx: $('fx'), onFirstTouch: resumeAudio, onTap: (x, y) => keyboard?.onTap(x, y) });
mountBusy($('busy'));

function resumeAudio() {
  const ctx = window.Module?.SDL2?.audioContext;
  if (ctx && ctx.state === 'suspended') ctx.resume();
}

canvas.addEventListener('contextmenu', (e) => e.preventDefault());      // the game uses the right button
// A click of a real mouse gives the keys to the game. Not a click that touch.js made, and not while the on-screen
// keyboard's field has the focus: taking the focus from it would close the keyboard at every tap.
canvas.addEventListener('mousedown', (e) => {
  if (e.isTrusted && document.activeElement !== $('kbd')) canvas.focus({ preventScroll: true });
  resumeAudio();
});

// The page's own buttons: after one is used, the keys belong to the game again.
const backToGame = () => { if (!document.querySelector('dialog[open]') && document.activeElement !== $('kbd')) canvas.focus({ preventScroll: true }); };
$('bar').addEventListener('click', (e) => { if (e.target.closest('button') && !e.target.closest('#kbd-open')) setTimeout(backToGame); });
for (const d of document.querySelectorAll('dialog')) d.addEventListener('close', backToGame);
$('help-open').onclick = () => $('help').showModal();

// ---- The account and the cloud saves (account.js). ----

const account = mountAccount({ log, toast, isRunning: () => running, isStandalone, leave: () => { leaving = true; } });
account.ready.then((me) => { $('help-cloud').hidden = !me?.auth; });
let cloud = null, cloudBehind = false, watcher = null, setSync = null;

// The sync of the account's saves, when it has cloud saves. replace: the saves of the cloud may replace the ones here
// (before the game runs). Resolves to the jobs done, or null.
async function startCloud(FS, { replace, onStep }) {
  await account.ready;
  const me = account.me;                         // as now: a sign-in since the page opened counts
  cloud?.stop();
  cloud = null;
  setSync?.stop();
  setSync = null;
  account.setCloud(null);
  if (!me?.user?.cloud) return null;
  if (typeof CompressionStream !== 'function') { log('page: no CompressionStream in this browser: no cloud saves'); return null; }
  cloud = createCloudSync({ FS, sync, log, toast, account: me.user.email, onWrote: () => watcher?.rebase() });
  account.setCloud(cloud);
  setSync = createSetSync({ FS, root: SAVE_DIR, sync, log, toast, account: me.user.email });   // runs once the game is up
  const jobs = await cloud.run({ replace, onStep, signal: onStep?.signal });
  cloudBehind = jobs === null || jobs.failed.length > 0;
  const came = jobs?.filter((j) => j.do === 'down' && !jobs.failed.includes(j.name)).length ?? 0;
  if (jobs === null && !onStep?.signal?.aborted) toast('The cloud was not reached: the saves here are used, and go up later', { kind: 'bad', ms: 7000 });
  else if (jobs?.failed.length) toast(notYet(jobs.failed), { kind: 'bad', ms: 7000 });
  else if (came) toast(`${came} saved game${came > 1 ? 's' : ''} came from the cloud`);
  return jobs;
}

// The saves that the cloud did not take this time (the server or its store failed them): they go at the next sync.
const notYet = (names) => `${names.length === 1 ? `${names[0]} is` : `${names.length} saves are`} not in the cloud yet: the cloud did not take ${names.length === 1 ? 'it' : 'them'} this time. ${names.length === 1 ? 'It goes' : 'They go'} up in a minute.`;

// After a save: up to the cloud. A sync that failed (no connection) or left a save behind is tried again each minute.
function cloudAfterSave(quiet = false) {
  if (!cloud || cloud.stopped) return;
  cloud.run({ replace: false }).then((jobs) => {
    cloudBehind = jobs === null || jobs.failed.length > 0;
    if (jobs === null) { if (!quiet) toast('Not in the cloud yet: no connection. It goes up later.', { kind: 'bad', ms: 6000 }); }
    else if (jobs.failed.length) { if (!quiet) toast(notYet(jobs.failed), { kind: 'bad', ms: 6000 }); }
    else if (jobs.some((j) => j.do !== 'down')) toast('Saved to the cloud');
  });
}
setInterval(() => { if (cloudBehind && running) cloudAfterSave(true); }, 60000);   // the try again says nothing till it works
account.onAccountChange((me) => {
  const FS = window.Module?.FS;
  if (!running || !FS) return;                   // before the game, the start does it
  if (!me?.user?.cloud) { cloud?.stop(); cloud = null; setSync?.stop(); setSync = null; account.setCloud(null); return; }
  startCloud(FS, { replace: false }).then(() => setSync?.run());   // signed in while the game runs: new saves come in, none is replaced
});
// The start box tells the plans: free, or Cloud Saves. Signing in leaves the page, which is fine before the game.
function plansForStart(me) {
  if (!me?.auth) return '';
  const links = `<p class="small legal">${LEGAL_LINKS}</p>`;
  if (me.user?.cloud) return `<p class="small">Signed in as ${esc(me.user.email)} · Cloud Saves are on: your saves from other devices come in when the game starts.</p>`;
  if (me.user) return `<p class="small">Signed in as ${esc(me.user.email)} · Free plan. Cloud Saves (${esc(me.offer.price)}) keep your saves on all your
    devices: <a href="#" id="go-plans">see Cloud Saves</a>.</p>${links}`;
  // Signed out: the game is free, and Cloud Saves is a note under it, not a plan to pick.
  return `<div class="addon"><b>Optional: Cloud Saves</b> · ${esc(me.offer.price)}. Your saves on every computer and iPad,
    backed up after each save. <a href="/auth/google/start?mode=page" id="go-signin">Sign in with Google</a> to get it, or to
    bring the cloud saves you have. Playing needs no account.</div>${links}`;
}
// Once in this browser, after its first save: a player without Cloud Saves hears that they exist.
function cloudHintOnce() {
  const hint = account.promo();
  if (!hint) return;
  try { if (localStorage.getItem('cm-cloud-hint')) return; localStorage.setItem('cm-cloud-hint', '1'); } catch { return; }
  setTimeout(() => toast(`${hint} See ${account.me?.user ? 'your name' : 'Sign in'}, top right.`, { ms: 9000 }), 2500);
}
$('help-close').onclick = () => $('help').close();
$('fullscreen').onclick = () => layout.toggleFullscreen();
layout.onFullscreen((on) => {
  $('fullscreen').setAttribute('aria-pressed', String(on));
  $('fullscreen').setAttribute('aria-label', on ? 'Leave full screen' : 'Full screen');
  $('fullscreen').querySelector('span').textContent = on ? 'Leave full screen' : 'Full screen';
});

// ---- The game files in the browser. ----

// The game files stay in the browser's cache storage under their versioned address, so a later visit reads them
// from this device. Only pages on https or on this computer (127.0.0.1) have cache storage; elsewhere every visit
// downloads them.
const CACHE = 'cm0102-game-files';
const SHELL = 'cm0102-shell';            // the page and the engine, for a start without a connection (sw.js)
const fileUrl = (f) => `/game/${f.path.split('/').map(encodeURIComponent).join('/')}?v=${encodeURIComponent(f.v ?? '')}`;

async function openCache(name = CACHE) {
  try { return self.caches ? await caches.open(name) : null; } catch { return null; }
}

// The bytes of the listed files that are in the cache already.
async function cachedBytes(cache, files) {
  if (!cache) return 0;
  let n = 0;
  for (const f of files) if (await cache.match(fileUrl(f))) n += f.size;
  return n;
}

// Removes the cached files that the list no longer names (an older version of a file).
async function pruneCache(cache, files) {
  if (!cache) return;
  const wanted = new Set(files.map((f) => new URL(fileUrl(f), location.href).href));
  for (const req of await cache.keys()) if (!wanted.has(req.url)) await cache.delete(req);
}

// Asks the browser to keep this site's storage (saves and game files) when the disk runs low.
async function keepStorage() {
  try {
    if (!navigator.storage?.persist) return 'unknown';
    if (await navigator.storage.persisted() || await navigator.storage.persist()) return 'kept';
    return 'may be cleared when the disk is nearly full';
  } catch {
    return 'unknown';
  }
}

// Every listed file, from the cache or the server, read as a stream so the bar moves inside the large ones
// (language.ldb is 195 MB). counts.cached and counts.fetched get the bytes of each kind.
// A game file that did not come (the connection dropped, the transfer was cut short, the server failed) is asked for
// again after these pauses before the start stops; a phone on mobile data loses its connection now and then.
const FILE_TRIES = [2000, 5000, 10000];

async function download(files, cache, onBytes, counts, signal) {
  const out = new Map();
  let next = 0;
  // The body of r into an array of the file's size; got(n) counts the bytes as they come.
  async function readAll(r, f, fromCache, got = () => {}) {
    const buf = new Uint8Array(f.size), reader = r.body.getReader();
    let at = 0;
    for (;;) {
      const { done, value } = await reader.read();
      if (done) break;
      if (at + value.length > f.size) throw new Error(`${f.path}: more than ${f.size} bytes`);
      buf.set(value, at);
      at += value.length;
      onBytes(value.length, fromCache);
      got(value.length);
    }
    if (at !== f.size) throw Object.assign(new Error(`${f.path}: ${at} of ${f.size} bytes`), { again: true });
    return buf;
  }
  async function one() {
    while (next < files.length) {
      const f = files[next++];
      const url = fileUrl(f);
      const kept = cache ? await cache.match(url) : undefined;
      let buf;
      if (kept) buf = await readAll(kept, f, true);
      else {
        for (let i = 0; ; i++) {
          let got = 0;
          try {
            const r = await fetch(url, { signal });
            if (!r.ok) throw Object.assign(new Error(`${f.path}: HTTP ${r.status}`), { again: r.status >= 500 });
            const storing = cache ? cache.put(url, r.clone()).catch((e) => log(`page: ${f.path} was not kept in the browser: ${e}`)) : null;
            buf = await readAll(r, f, false, (n) => { got += n; });
            if (storing) await storing;
            break;
          } catch (e) {
            if (got) onBytes(-got, false);                 // the bytes of the try that failed come again
            const again = e.name !== 'AbortError' && (e instanceof TypeError || e.again);
            if (!again || i >= FILE_TRIES.length || signal?.aborted) throw e;
            log(`page: ${f.path}: ${e.message}; asked for again in ${FILE_TRIES[i] / 1000} s`);
            await new Promise((ok) => setTimeout(ok, FILE_TRIES[i]));
          }
        }
      }
      counts[kept ? 'cached' : 'fetched'] += f.size;
      out.set(f.path, buf);
    }
  }
  await Promise.all(Array.from({ length: CONNECTIONS }, one));
  return out;
}

// The files into the engine's file system, without a copy (the file system keeps the arrays).
function install(FS, files) {
  for (const [path, data] of files) {
    const full = `/game/${path}`;
    FS.mkdirTree(full.slice(0, full.lastIndexOf('/')));
    FS.writeFile(full, data, { canOwn: true });
  }
}

// After a good start: the page and the engine into the cache that sw.js answers from when there is no connection.
async function primeOffline(info) {
  const cache = await openCache(SHELL);
  if (!cache || !('serviceWorker' in navigator)) return;
  const engine = ['js', 'wasm'].map((ext) => engineUrl(info.chosen, `cm0102_recomp.${ext}`));
  const wanted = [...(info.shell ?? []), '/files.json', ...engine];
  for (const req of await cache.keys()) {
    const u = new URL(req.url);
    if (u.pathname.startsWith('/engine/') && !engine.includes(u.pathname + u.search)) await cache.delete(req);
  }
  for (const url of wanted) {
    try {
      if (url.startsWith('/engine/') && await cache.match(url)) continue;
      const r = await fetch(url, { cache: 'no-cache' });
      if (r.ok) await cache.put(url, r);
    } catch { /* offline now: the cache keeps what it has */ }
  }
}

// ---- The engine. ----

// Writes the user folder to IndexedDB. A call while a write runs makes one more write after it, so nothing is missed.
let syncing = false, syncAgain = false;
function sync() {
  const FS = window.Module?.FS;
  if (!FS) return;
  if (vault?.active) vault.persist().catch((e) => log(`page: the vault: ${e?.message ?? e}`));
  if (syncing) { syncAgain = true; return; }
  syncing = true;
  FS.syncfs(false, (err) => {
    syncing = false;
    if (err) { log(`page: the user folder was not saved: ${err}`); toast('The save did not reach this browser\'s storage', { kind: 'bad', ms: 6000 }); }
    if (syncAgain) { syncAgain = false; sync(); }
  });
}

// The vault (vault.js): with JSPI the engine's open can wait, so saves, older copies and data sets stay in the browser's
// storage until the game opens one, not in the page's memory. vaultMode: 'on', 'restore' (the vault cannot run here;
// files that an earlier visit put in it come back into the user folder) or 'off'.
let vault = null, vaultMode = 'off', vaultOpened = false;

async function prepareVault(hooked) {
  if (!vault) return;
  try {
    await vault.open();
    vaultOpened = true;
    if (!hooked) { vaultMode = 'restore'; return; }
    const moved = await vault.migrateFrom('/user');
    if (moved) log(`page: ${moved} files of the user folder moved to the browser's storage (the vault)`);
    vault.wrapFS();
    vaultMode = 'on';
  } catch (e) {
    log(`page: the vault is off: ${e?.message ?? e}`);
    vault.stop();
    vaultMode = vaultOpened ? 'restore' : 'off';
  }
}

async function finishVault(FS) {
  if (!vault) return;
  if (vaultMode === 'on') {
    try {
      const m = await vault.mount();
      vault.active = true;
      useVault(vault);
      log(`page: the vault: ${m.files} files (${mb(m.bytes)}) stay in the browser's storage until the game opens them`);
      return;
    } catch (e) {
      log(`page: the vault is off: ${e?.message ?? e}`);
      vault.stop();
      vaultMode = 'restore';
    }
  }
  if (vaultMode === 'restore') {
    try {
      const n = await vault.materialize();
      if (n) {
        log(`page: ${n} files came back from the vault into the user folder (this engine cannot open them from there)`);
        FS.syncfs(false, (err) => { if (!err) vault.clear(); });
      }
    } catch (e) { log(`page: the vault's files did not come back: ${e?.message ?? e}`); }
  }
}

// The vault is on, unless asked to be off: ?vault=0 in the address, or localStorage cm0102-vault = off (a way back if a
// browser does not take it; the files already in it then come back into the user folder).
function vaultWanted() {
  if (/[?&]vault=0\b/.test(location.search)) return false;
  try { return localStorage.getItem('cm0102-vault') !== 'off'; } catch { return true; }
}

function runEngine(files, info) {
  const wasmUrl = engineUrl(info.chosen, 'cm0102_recomp.wasm');
  // The vault opens files lazily only on a JSPI engine in a browser with WebAssembly.Suspending; otherwise (or turned
  // off) it is there only to give back what an earlier visit put in it (prepareVault: restore).
  const lazy = vaultWanted() && info.chosen.suspend === 'jspi' && typeof WebAssembly.Suspending === 'function';
  vault = typeof indexedDB !== 'undefined' ? createVault({ FS: () => window.Module?.FS, log }) : null;
  let hooked = false;
  const plainInstantiate = (imports, receive) => WebAssembly.instantiateStreaming(fetch(wasmUrl), imports).then((r) => receive(r.instance))
    .catch(async () => receive((await WebAssembly.instantiate(await (await fetch(wasmUrl)).arrayBuffer(), imports)).instance));
  window.Module = {
    canvas,
    ...(vault && lazy ? { instantiateWasm: (imports, receive) => {
      hooked = vault.instantiate(imports, receive, wasmUrl);
      if (!hooked) plainInstantiate(imports, receive);
      return {};
    } } : {}),
    arguments: ['--game', '/game'],
    // Restart Game (the game's restart_game 0x68dbc0, WinExec in kernel32.c, host_restart in host/web.c): the user
    // folder goes to the browser's storage, then the page loads again, which starts the game as a visit does.
    rtRestart: () => {
      running = false;                                   // no question before the page goes
      log('page: Restart Game: the page loads again');
      const FS = window.Module.FS;
      const go = () => location.reload();
      Promise.all([vault?.active ? vault.persist({ force: true }).catch(() => {}) : null,
        new Promise((done) => FS.syncfs(false, () => done()))]).then(go, go);
      setTimeout(go, 8000);                              // at the latest
    },
    print: log,
    printErr: log,
    locateFile: (name) => engineUrl(info.chosen, name),
    preRun: [() => {
      const M = window.Module, FS = M.FS;
      install(FS, files);
      files.clear();
      FS.mkdirTree('/user');
      const idbfs = M.IDBFS || FS.filesystems.IDBFS;           // newer Emscripten has no FS.filesystems
      if (!skipWorkFiles(idbfs, (p) => Boolean(vault?.owns(p)))) log('page: this engine\'s IDBFS has no sets: the working files go to IndexedDB too');
      FS.mount(idbfs, {}, '/user');
      M.ENV.RECOMP_USER_DIR = '/user';                  // main.c: every write of the game goes there
      M.ENV.HOME = '/user';
      M.addRunDependency('user-folder');
      prepareVault(hooked).then(() => FS.syncfs(true, async (err) => {   // the saves of earlier visits, back from the browser
        if (err) log(`page: the user folder was not read: ${err}`);
        await finishVault(FS);                           // the placeholders, before the cloud sync and the game list them
        // The display mode of the settings file, for an engine that does not tell the size of its picture.
        try { if (/^\s*display\s*=\s*classic\b/m.test(FS.readFile('/user/settings.txt', { encoding: 'utf8' }))) layout.setPicture(800, 600); }
        catch { /* no settings file yet: widescreen */ }
        // The saves made on other devices, before the game lists them.
        if ((await account.ready)?.user?.cloud) {
          const abort = new AbortController();
          overlay(`<h1>Getting your cloud saves</h1><div class="row"><i class="spin"></i><span id="cloud-step">Asking the cloud…</span></div>
            <p style="margin:14px 0 0"><button id="cloud-skip" hidden>Start without them</button></p>`);
          status('Getting your cloud saves…');
          const skip = setTimeout(() => { if ($('cloud-skip')) { $('cloud-skip').hidden = false; $('cloud-skip').onclick = () => abort.abort(); } }, 5000);
          const onStep = (t) => { if ($('cloud-step')) $('cloud-step').textContent = t; };
          onStep.signal = abort.signal;
          try { await startCloud(FS, { replace: true, onStep }); }
          catch (e) { log(`page: cloud saves: ${e?.stack ?? e}`); }       // the game starts all the same
          clearTimeout(skip);
          overlay('<h1>Starting the engine</h1><div class="row"><i class="spin"></i>The first start can take a minute.</div>');
          status('Starting the engine…');
        }
        log(`page: ${userFolderSizes(FS)}`);
        M.removeRunDependency('user-folder');
      }));
    }],
    onRuntimeInitialized: () => {
      const M = window.Module, FS = M.FS;
      overlay('');
      status('');
      running = true;
      canvas.dataset.engine = '1';
      $('fullscreen').hidden = !layout.canFullscreen || isStandalone();
      canvas.focus({ preventScroll: true });
      setInterval(sync, 15000);
      if (vault?.active && typeof vault.exports?.rt_web_choose_data_set === 'function' && typeof WebAssembly.promising === 'function') {
        M._rt_web_choose_data_set = WebAssembly.promising(vault.exports.rt_web_choose_data_set);
        M.chooseAsync = true;                            // datasets.js: ccall's async form
      }
      // A save file that was added or changed, and has stayed the same for 2 seconds (the game finished writing it),
      // goes to IndexedDB at once, not at the next 15-second write.
      // A save that the cloud sync wrote is not a save of the game: the watcher takes it as seen.
      let lastSaves = savesSignature(FS), seen = lastSaves;
      watcher = { rebase() { lastSaves = seen = savesSignature(FS); } };
      setInterval(() => {
        if (vault?.active) vault.persist().catch((e) => log(`page: the vault: ${e?.message ?? e}`));
        const now = savesSignature(FS);
        if (now !== lastSaves && now === seen) {
          if (now.length >= lastSaves.length) { toast('Saved in this browser'); if (!cloud) cloudHintOnce(); }
          lastSaves = now;
          sync();
          cloudAfterSave();
        }
        seen = now;
      }, 2000);
      const editor = mountEditor({ FS, sync, log, toast, dir: SAVE_DIR, isRunning: () => running,
        onSetChange: (name, what) => (what === 'deleted' ? setSync?.deleted(name) : setSync?.run()) });
      $('editor-open').hidden = false;
      $('editor-open').onclick = () => editor.open();
      const sets = mountSetsPanel({ FS, root: SAVE_DIR, sync, log, toast, module: () => window.Module, cloud: () => setSync,
        edit: (name) => editor.openSet(name), create: () => editor.open(), changed: () => setSync?.run() });
      $('sets').hidden = false;
      $('saves-open').addEventListener('click', () => sets.render());
      setSync?.run();                                   // the cloud's data sets come in while the game starts
      mountSavesPanel({ FS, sync, log, cloud: () => cloud, promo: () => (cloud ? null : account.promo()),
        openPromo: () => { $('saves').close(); account.open(); }, edit: (name) => editor.openSave(name) });
      keyboard = mountKeyboard({ log, layout, toast });
      // What the engine says about itself, a few times a second: the size of its picture, and the text entry that takes
      // keys (user32.c). An engine built before these calls has neither; the page then works without them.
      const picture = M._rt_web_picture, typingRow = M._rt_web_typing_row;
      if (typeof picture === 'function' || typeof typingRow === 'function') {
        const poll = setInterval(() => {
          try {
            const v = typeof picture === 'function' ? picture() : 0;
            if (v) layout.setPicture(v >>> 16, v & 0xffff);
            if (keyboard && typeof typingRow === 'function') keyboard.setTypingRow(typingRow());
          } catch (e) {
            clearInterval(poll);
            log(`page: the engine's state calls stopped: ${e}`);
          }
        }, 200);
      }
      keepAwake();
      const onHide = () => { if (vault?.active) vault.persist({ force: true }).catch(() => {}); sync(); };
      addEventListener('pagehide', onHide);
      document.addEventListener('visibilitychange', () => { if (document.hidden) onHide(); });
      primeOffline(info).catch((e) => log(`page: not kept for a start without a connection: ${e}`));
      log(`page: main(${M.arguments.join(' ')})`);
      try {
        const r = M.callMain(M.arguments);
        if (r && typeof r.then === 'function') r.catch((e) => { if (e?.name !== 'ExitStatus') { running = false; logStack(e); fail('The game stopped', String(e)); } });
      } catch (e) {
        if (e?.name !== 'ExitStatus') { running = false; logStack(e); fail('The game stopped', String(e)); }
      }
    },
    onExit: (code) => {
      running = false;
      sync();
      if (code === 0) { status('The game ended'); overlay('<h1>The game ended</h1><p>Reload the page to play again. Your saves stay in this browser.</p>'); }
      else fail(`The game ended with code ${code}`, 'Help, Engine log says why. Your saves stay in this browser.');
    },
    onAbort: (what) => { running = false; logStack(what); fail('The game stopped', String(what)); },
  };
  const s = document.createElement('script');
  s.src = engineUrl(info.chosen, 'cm0102_recomp.js');
  s.onerror = () => fail('The engine did not load', `The server did not give ${info.chosen.base}cm0102_recomp.js.`);
  document.body.appendChild(s);
}

// What the engine needs of the browser. Each hard requirement that fails is a reason the game cannot run, and the page
// stops before it downloads anything. The warnings are things that make it worse, not impossible.
const PAGE = 65536;
const ENGINE_START_PAGES = 12288;                    // -sINITIAL_MEMORY=768 MB (web/web.cmake)
const ENGINE_NEED_PAGES = 0x74000000 / PAGE;         // host/web.c grows the memory to the guest heap at 0x70000000 + 64 MB
const ENGINE_MAX_PAGES = 65536;                      // -sMAXIMUM_MEMORY=4GB

function requirements(info) {
  const problems = [], warnings = [];
  if (typeof WebAssembly !== 'object' || typeof WebAssembly.instantiate !== 'function') {
    problems.push('This browser has no WebAssembly, which the engine is made of.');
    return { problems, warnings };
  }
  if (info.chosen.suspend !== 'asyncify' && !hasJspi())
    problems.push('The engine needs JSPI, a WebAssembly feature that this browser lacks, and this server has no engine for other browsers. Chrome 137 or later has it, and Safari 27 (iOS and iPadOS 27, in every browser of an iPhone or iPad).');
  try {                                              // the engine's own memory plan, on a memory that is then dropped
    const m = new WebAssembly.Memory({ initial: ENGINE_START_PAGES, maximum: ENGINE_MAX_PAGES });
    m.grow(ENGINE_NEED_PAGES - ENGINE_START_PAGES);
  } catch (e) {
    problems.push(`This browser does not give a page the ${Math.round(ENGINE_NEED_PAGES * PAGE / 1048576)} MB of memory that the engine needs (${e.message}).`);
  }
  if (typeof indexedDB === 'undefined') warnings.push('This browser keeps no data for the page (a private window?): saves are lost when the tab closes.');
  return { problems, warnings };
}

function blocked(problems, note = 'Nothing was downloaded.') {
  status('This browser cannot run the game');
  overlay(`<h1>This browser cannot run the game</h1><ul>${problems.map((p) => `<li>${esc(p)}</li>`).join('')}</ul>
    <p>It runs in Chrome 137 or later, and in Safari 27 or later: on an iPad with iPadOS 27, in Safari or Chrome. ${esc(note)}</p>`, 'error');
}

// The engine file itself, compiled once while the game files download: a browser that lacks a WebAssembly feature the
// engine uses fails here, and the download of the game files stops.
async function engineCompiles(info) {
  try {
    await WebAssembly.compileStreaming(fetch(engineUrl(info.chosen, 'cm0102_recomp.wasm')));
    return null;
  } catch (e) {
    return e.message ?? String(e);
  }
}

// The speed of the download over the last few seconds, and the time left at that speed.
function meter(total) {
  const marks = [];
  return (got) => {
    const now = performance.now();
    marks.push([now, got]);
    while (marks.length > 2 && now - marks[0][0] > 4000) marks.shift();
    const [t0, g0] = marks[0];
    const rate = now - t0 > 400 ? (got - g0) / ((now - t0) / 1000) : 0;
    if (!(rate > 0)) return '';                       // none yet, or a try that failed was taken back
    const left = (total - got) / rate;
    return ` · ${mb(rate)}/s · ${left < 90 ? `about ${Math.max(1, Math.round(left))} s left` : `about ${Math.round(left / 60)} min left`}`;
  };
}

async function start() {
  overlay('<div class="row"><i class="spin"></i>Checking the files…</div>');
  status('Checking the files…');
  let info;
  try { info = await (await fetch('/files.json')).json(); }
  catch (e) { fail('The server does not answer', String(e)); return; }
  info.chosen = pickEngine(info);
  if (!info.chosen) { fail('The engine is not built', 'Build it first: web/README.md, step 4.'); return; }
  log(`page: engine ${info.chosen.base} (${info.chosen.suspend ?? 'unknown'} build, version ${info.chosen.version})${hasJspi() ? '' : '; this browser has no JSPI'}`);
  if (info.missing.length) {
    fail(`${info.missing.length} game files are missing or of another size`,
      `First: ${info.missing.slice(0, 6).map((m) => m.path).join(', ')}. CM_GAME_DIR must hold the clean files (web/README.md, step 3).`);
    return;
  }
  stats = startStats({ info, log, isRunning: () => running, state: folderState,
    extra: () => ({ engine: `${info.chosen.suspend ?? 'unknown'} ${info.chosen.version}`, vault: Boolean(vault?.active), cloud: Boolean(account.me?.user?.cloud) }) });
  mountStatsChoice(info);
  const { problems, warnings } = requirements(info);
  if (problems.length) { log(`page: blocked: ${problems.join(' ')}`); stats.report(`blocked: ${problems.join(' ')}`, [], 'requirements'); blocked(problems); return; }
  if ('serviceWorker' in navigator && window.isSecureContext) navigator.serviceWorker.register('/sw.js').catch((e) => log(`page: no service worker: ${e}`));
  const cache = await openCache();
  const [have, kept] = await Promise.all([cachedBytes(cache, info.files), keepStorage()]);
  log(`page: browser storage ${kept}; ${mb(have)} of the game files already in this browser`);
  setStorageKept(kept);
  $('help-storage').textContent = ` ${storageWarning(false)}`;
  const toGet = info.totalBytes - have;
  if (toGet > 0 || warnings.length) {
    // The first visit, or part of the files: one click before a large download.
    status('Ready');
    overlay(`<h1>CM 01/02, in your browser</h1>
      <p>${toGet > 0 ? `${mb(toGet)} of game files download into this browser once${have ? ` (${mb(have)} are here already)` : ''}.`
        : 'The game files are in this browser already.'} The game then runs in this tab, on your device. Nothing is installed.</p>
      <p class="small">Your saved games stay in this browser too. Saved games downloads them, one by one or all at once, so you
      keep copies: a browser may delete a site's data by itself.</p>
      ${warnings.length ? `<ul>${warnings.map((w) => `<li>${esc(w)}</li>`).join('')}</ul>` : ''}
      ${TOUCH ? '<p class="small">On a touch screen: tap to click, long press for the right button, two fingers to scroll. Help has the list.</p>' : ''}
      <div class="row"><button class="primary" id="go">Start the game</button><span class="free-note">Free · no account, no payment</span></div>
      ${plansForStart(await Promise.race([account.ready, new Promise((r) => setTimeout(r, 1500))]))}`);
    if ($('go-plans')) $('go-plans').onclick = (e) => { e.preventDefault(); account.open(); };
    await new Promise((r) => { $('go').onclick = r; });
  }
  overlay(`<h1>${toGet > 0 ? 'Loading the game files' : 'Starting'}</h1><div class="bar"><i id="bar-fill"></i></div><div class="small" id="bytes"></div>`);
  status('Loading the game files…');
  const t0 = performance.now();
  const counts = { cached: 0, fetched: 0 };
  const abort = new AbortController();
  const speed = meter(info.totalBytes);
  let got = 0, compileError = null, files;
  // The engine is checked while the files load; if this browser cannot compile it, the download stops.
  const compiled = engineCompiles(info).then((err) => { if (err) { compileError = err; abort.abort(); } });
  try {
    files = await download(info.files, cache, (n) => {
      got += n;
      $('bar-fill').style.width = `${(100 * got / info.totalBytes).toFixed(1)}%`;
      $('bytes').textContent = `${mb(got)} of ${mb(info.totalBytes)}${toGet > 0 ? speed(got) : ''}`;
    }, counts, abort.signal);
    await compiled;
  } catch (e) {
    await compiled;
    if (!compileError) {
      fail('A game file did not load', `${e.message ?? e}. The connection may have dropped. The game files that arrived stay in this `
        + 'browser: Start again goes on with the rest.');
      return;
    }
  }
  if (compileError) {
    log(`page: the engine does not compile here: ${compileError}`);
    blocked([`This browser cannot compile the engine: ${compileError}`], 'The download of the game files was stopped.');
    return;
  }
  log(`page: ${info.files.length} files in ${((performance.now() - t0) / 1000).toFixed(1)} s: ${mb(counts.cached)} from this browser, ${mb(counts.fetched)} downloaded`);
  pruneCache(cache, info.files).catch((e) => log(`page: old game files not removed: ${e}`));
  overlay('<h1>Starting the engine</h1><div class="row"><i class="spin"></i>The first start can take a minute.</div>');
  status('Starting the engine…');
  runEngine(files, info);
}

start();
