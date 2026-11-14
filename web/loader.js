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
import { mountTacticsPanel } from './tactics.js';
import { createSetSync, listDataSets } from './datasets.js';
import { LEGAL_LINKS, createCloudSync, mountAccount, plansHtml } from './account.js';
import { guardKeys, hasTouch, mountKeyboard } from './keyboard.js';
import { mountLayout } from './layout.js';
import { mountMacRightButton, mountTouch } from './touch.js';
import { guardLeave, isStandalone, keepAwake, mountBusy, toast } from './ui.js';
import { mountReport, watchSaveInPlay } from './report.js';

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
let report = null, saveInPlay = () => null;          // Send to the developer (report.js), when the server takes reports
function fail(title, detail) {
  status(title);
  stats?.report(`${title}: ${detail}`, lines.slice(-60), 'fail');
  const last = lines.slice(-8).join('\n');
  // The memory of a tab is only given back for sure when the tab is closed (a page that loaded again in the same tab
  // once kept the game before it: letEngineGo), so that is the first thing to try.
  const memory = OUT_OF_MEMORY.test(String(detail)) ? `<p>The browser ran out of memory for the game. <b>Close this tab
    and open the game in a new one</b>: loading the page again in this same tab does not give its memory back. If it
    happens again, close the other tabs of this site, or quit the browser and open it again. Your saves stay in this
    browser.</p>` : '';
  overlay(`<h1>${esc(title)}</h1><p>${esc(detail)}</p>${memory}${last ? `<pre class="small" style="white-space:pre-wrap;margin:0 0 14px">${esc(last)}</pre>` : ''}
    <div class="row"><button class="primary" id="fail-again">Start again</button>${report ? '<button id="fail-report">Send to the developer</button>' : ''}<span class="small">Your saves stay in this browser.</span></div>`, 'error');
  if ($('fail-report')) $('fail-report').onclick = () => report.open({ message: `${title}: ${detail}`, where: 'the error box' });
  // The way on: the page loads again, after the user folder went to the browser's storage.
  const again = $('fail-again');
  if (again) again.onclick = () => {
    running = false; leaving = true;
    const go = () => location.reload();
    storeEverything().then(() => letEngineGo()).then(go, go);
    setTimeout(go, 15000);                             // a storage that does not answer
  };
}

// The game's own fatal box, the last one before an exit: the runtime logs it (waits.c: "recomp: message box from
// <caller>: Error: v3.9.68 <source>..cpp <line>"), and the game then exits with -1. The player saw that box; the error
// box of the page and its report name it, so the check that failed can be found. A player's save that would not load
// gave only "The game ended with code -1" (14 November 2026).
function fatalBox() {
  for (let i = lines.length - 1; i >= 0; i--) {
    const m = /^recomp: message box from (0x[0-9a-f]+): Error: (.+)$/.exec(lines[i]);
    if (m) return { site: m[1], text: m[2] };
  }
  return null;
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
// One tab at a time (myTurn, below): another tab took the game. turnGone at once, handedOver after this tab's last store;
// from then on nothing of this tab writes the browser's storage.
let turnGone = false, handedOver = false;
let keyboard = null;
guardKeys();
guardLeave(() => (running || storing) && !leaving);
const canvas = $('canvas');
const layout = mountLayout({ app: $('app'), bar: $('bar'), stage: $('stage'), canvas, chip: $('type-chip'), isTouch: TOUCH });
// The Right click button (touch devices): the next tap on the game is a click of the right button. A long press does
// the same, but a finger that wanders turns it into a drag, and on the tactics board a drag moves the player.
const setRightButton = (on) => $('right-click').setAttribute('aria-pressed', String(on));
const touch = mountTouch({ canvas, stage: $('stage'), fx: $('fx'), onFirstTouch: resumeAudio, onTap: (x, y) => keyboard?.onTap(x, y),
  onRightUsed: () => setRightButton(false) });
$('right-click').onclick = () => {
  const on = !touch.rightArmed;
  touch.armRight(on);
  setRightButton(on);
  if (on) toast('Tap the game: that tap is a right click', { ms: 2500 });
};
mountBusy($('busy'));

function resumeAudio() {
  const ctx = window.Module?.SDL2?.audioContext;
  if (ctx && ctx.state === 'suspended') ctx.resume();
}

canvas.addEventListener('contextmenu', (e) => e.preventDefault());      // the game uses the right button
mountMacRightButton({ canvas });                     // a Mac: Control and a click (or a drag) is the right button's
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
// A phone's or tablet's file picker turns the accept list into file types it knows; .sav and .dat are none of them, and
// a downloaded zip often has a general type, so the picker could grey out the very file. The page checks each file
// itself (saves.js, setspanel.js), so on a touch device the picker shows every file.
if (TOUCH) for (const id of ['saves-file', 'sets-file', 'tactics-file']) $(id).removeAttribute('accept');
// Full screen with no room beside the picture: the page's buttons sit over its top right corner, where the game has
// buttons too (a player's Transfer tab, for one). The fold button folds them into a small tab on the right edge
// (index.html, #app.bar-folded), and the choice stays for later visits.
const FOLD_KEY = 'cm0102-bar-folded';
function setBarFolded(on, remember = true) {
  $('app').classList.toggle('bar-folded', on);
  const label = on ? 'Show the buttons' : 'Hide these buttons';
  $('bar-fold').setAttribute('aria-expanded', String(!on));
  $('bar-fold').setAttribute('aria-label', label);
  $('bar-fold').title = label;
  if (remember) try { if (on) localStorage.setItem(FOLD_KEY, '1'); else localStorage.removeItem(FOLD_KEY); } catch { /* storage off */ }
}
try { if (localStorage.getItem(FOLD_KEY)) setBarFolded(true, false); } catch { /* storage off: open */ }
$('bar-fold').onclick = () => setBarFolded(!$('app').classList.contains('bar-folded'));
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

// The browser can close a site's IndexedDB connection by itself (Android's Chrome does). Emscripten's IDBFS keeps the
// closed one in IDBFS.dbs, so every later write of the user folder fails with "The database connection is closing".
// The page drops it, and the next write opens a new one.
const closedStorage = (err) => /InvalidStateError|connection is closing|AbortError|UnknownError/i.test(`${err?.name ?? ''} ${err?.message ?? err}`);
function forgetUserStorage() {
  const M = window.Module, idbfs = M?.IDBFS || M?.FS?.filesystems?.IDBFS;
  if (!idbfs?.dbs) return false;
  for (const k of Object.keys(idbfs.dbs)) {
    try { idbfs.dbs[k].close(); } catch { /* closed already */ }
    delete idbfs.dbs[k];
  }
  return true;
}

// Everything of the user folder into the browser's storage now, the files written a moment ago too (the vault leaves
// those for its next round otherwise): for Restart Game and Exit, after which the page goes or the game is over. The
// promise ends when both writes did (or failed: the log says so). Leaving the page meanwhile asks first (storing).
let storing = 0;
function storeEverything() {
  const FS = window.Module?.FS;
  if (!FS || handedOver) return Promise.resolve();
  storing++;
  return stillMine().then((mine) => {
    if (mine) return storeAll(FS);
    stopHere(false);
    return undefined;
  }).finally(() => { storing--; });
}
function storeAll(FS) {
  const vaultDone = vault?.active ? vaultRound({ force: true }) : null;
  const folderDone = new Promise((done) => FS.syncfs(false, (err) => {
    if (err && closedStorage(err) && forgetUserStorage()) {
      FS.syncfs(false, (again) => { if (again) log(`page: the user folder was not saved: ${again}`); done(); });
      return;
    }
    if (err) log(`page: the user folder was not saved: ${err}`);
    done();
  }));
  return Promise.all([vaultDone, folderDone]);
}

// One round of the vault (vault.persist). A save it could not store stays in this tab's memory and is tried again at
// the next round; the player is told once, until a round works again: the next visit would not have that save.
let vaultFailing = false;
async function vaultRound(options) {
  if (!(await stillMine())) { stopHere(false); return undefined; }
  return vault.persist(options).then(() => {
    if (!vaultFailing) return;
    vaultFailing = false;
    log('page: the vault stores again');
    toast('Your saves are in this browser\'s storage again');
  }, (e) => {
    log(`page: the vault: ${e?.name ?? 'Error'}: ${e?.message ?? e}`);
    if (vaultFailing) return;
    vaultFailing = true;
    toast(`A save did not reach this browser's storage${/Quota/i.test(e?.name ?? '') ? ': the disk, or the room the browser gives this site, is full' : ''}. It stays in this tab: keep the tab open and Download the save.`,
      { kind: 'bad', ms: 15000 });
  });
}

// Writes the user folder to IndexedDB. A call while a write runs makes one more write after it, so nothing is missed.
let syncing = false, syncAgain = false, reopened = false;
function sync() {
  const FS = window.Module?.FS;
  if (!FS || handedOver) return;
  if (vault?.active) vaultRound();
  if (syncing) { syncAgain = true; return; }
  syncing = true;
  stillMine().then((mine) => {
    if (!mine || handedOver) { syncing = false; if (!mine) stopHere(false); return; }
    writeFolder(FS);
  });
}
function writeFolder(FS) {
  FS.syncfs(false, (err) => {
    syncing = false;
    if (err && !reopened && closedStorage(err) && forgetUserStorage()) {
      reopened = true;                                 // once per failing run: a second failure is reported
      log('page: the browser closed the storage of the user folder; the page opens it again');
      syncAgain = true;
    } else if (err) {
      log(`page: the user folder was not saved: ${err}`);
      toast('The save did not reach this browser\'s storage', { kind: 'bad', ms: 6000 });
    } else reopened = false;
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

// Letting the engine go when the page goes. With JSPI the game never returns: between frames its stack is parked
// inside emscripten_sleep. The browser keeps a parked stack alive after the page is gone, and with it the engine, its
// memory and the 467 MB of game files (measured in Chromium, 2026-11-12: about 515 MB more in the tab's process at
// each load of the page, until "Array buffer allocation failed" or the tab died). So the page wraps the engine's
// sleep: when the page is about to go (Restart Game, Start again, a reload, a closed tab), the sleep in progress
// ends with an error, the game's stack unwinds through main, and nothing stays parked.
// The engine's own sleep still runs inside (web/web.cmake builds it; Emscripten wraps it in WebAssembly.Suspending,
// which is how the page finds it: suspendingMade).
class EngineLetGo extends Error {
  constructor() { super('the page let the engine go'); this.name = 'EngineLetGo'; }
}
const engine = { letGo: false, wake: null, ended: Promise.resolve(), stoppable: false };
const suspendingMade = new WeakMap();                // each WebAssembly.Suspending the engine makes -> its function
function watchSuspending() {
  const Real = WebAssembly.Suspending;
  if (typeof Real !== 'function') return () => {};
  function Suspending(fn) {
    const s = new Real(fn);
    suspendingMade.set(s, fn);
    return s;
  }
  Suspending.prototype = Real.prototype;
  try { WebAssembly.Suspending = Suspending; } catch { return () => {}; }
  return () => { try { WebAssembly.Suspending = Real; } catch { /* stays wrapped: harmless */ } };
}
// The imports of the engine (instantiateWasm): its sleep becomes one the page can end.
function stoppableSleep(imports, unwatch) {
  const Real = Object.getPrototypeOf(imports?.env?.emscripten_sleep ?? {})?.constructor;
  const sleep = suspendingMade.get(imports?.env?.emscripten_sleep);
  unwatch();
  if (typeof sleep !== 'function' || typeof Real !== 'function') return false;
  imports.env.emscripten_sleep = new Real((ms) => new Promise((resolve, reject) => {
    if (engine.letGo) { reject(new EngineLetGo()); return; }
    const wake = () => reject(new EngineLetGo());
    engine.wake = wake;
    sleep(ms).then((v) => { if (engine.wake === wake) engine.wake = null; resolve(v); }, reject);
  }));
  engine.stoppable = true;
  return true;
}
// Ends the game for good. The promise ends when the game's stack is gone (at once when it never ran or cannot be ended).
function letEngineGo() {
  if (!engine.letGo) {
    engine.letGo = true;
    running = false;
    const wake = engine.wake;
    engine.wake = null;
    wake?.();
  }
  return engine.stoppable ? engine.ended : Promise.resolve();
}
// Any other way out of the page (a reload, a closed tab, another address): the sleep ends inside this event, and the
// stack unwinds before the page is gone. A page that comes back from the browser's back/forward cache has no game.
addEventListener('pagehide', () => { letEngineGo(); });
addEventListener('pageshow', (e) => { if (e.persisted && engine.letGo) location.reload(); });

function runEngine(files, info) {
  const wasmUrl = engineUrl(info.chosen, 'cm0102_recomp.wasm');
  // The vault opens files lazily only on a JSPI engine in a browser with WebAssembly.Suspending; otherwise (or turned
  // off) it is there only to give back what an earlier visit put in it (prepareVault: restore).
  const lazy = vaultWanted() && info.chosen.suspend === 'jspi' && typeof WebAssembly.Suspending === 'function';
  vault = typeof indexedDB !== 'undefined' ? createVault({ FS: () => window.Module?.FS, log, onBlocked: olderPageBlocks }) : null;
  let hooked = false;
  const plainInstantiate = (imports, receive) => WebAssembly.instantiateStreaming(fetch(wasmUrl), imports).then((r) => receive(r.instance))
    .catch(async () => receive((await WebAssembly.instantiate(await (await fetch(wasmUrl)).arrayBuffer(), imports)).instance));
  const jspi = info.chosen.suspend === 'jspi' && typeof WebAssembly.Suspending === 'function';
  const unwatch = jspi ? watchSuspending() : () => {};
  window.Module = {
    canvas,
    ...(jspi ? { instantiateWasm: (imports, receive) => {
      if (!stoppableSleep(imports, unwatch)) log('page: this engine\'s sleep cannot be ended: a reload of the page keeps its memory');
      hooked = vault && lazy ? vault.instantiate(imports, receive, wasmUrl) : false;
      if (!hooked) plainInstantiate(imports, receive);
      return {};
    } } : {}),
    arguments: ['--game', '/game'],
    // Restart Game (the game's restart_game 0x68dbc0, WinExec in kernel32.c, host_restart in host/web.c): the user
    // folder goes to the browser's storage, then the page loads again, which starts the game as a visit does.
    rtRestart: () => {
      running = false;
      log('page: Restart Game: the saves go to the browser\'s storage, then the page loads again');
      overlay('<h1>Restarting…</h1><p>Your saves go to this browser\'s storage first.</p>');
      // Not before the saves are stored: a large save on a phone can take longer than the 8 s this once allowed, and
      // the reload then lost it. A minute is the most the page waits for a storage that does not answer.
      let gone = false;
      const go = () => { if (gone) return; gone = true; leaving = true; location.reload(); };
      const letGo = () => letEngineGo().then(go, go);    // the game's stack ends first, so the old page's memory is freed
      storeEverything().then(letGo, letGo);
      setTimeout(() => { if (!gone) log('page: Restart Game: the storage did not answer in a minute; the page loads again'); go(); }, 60000);
    },
    print: log,
    printErr: log,
    locateFile: (name) => engineUrl(info.chosen, name),
    preRun: [() => {
      const M = window.Module, FS = M.FS;
      saveInPlay = watchSaveInPlay(FS);                // which save a report sends by default
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
      if (turnGone) return;                            // another tab took the game while this one started
      overlay('');
      status('');
      running = true;
      canvas.dataset.engine = '1';
      $('fullscreen').hidden = !layout.canFullscreen || isStandalone();
      $('right-click').hidden = !TOUCH;
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
        if (handedOver) return;
        if (vault?.active) vaultRound();
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
      mountSavesPanel({ FS, sync, log, report: () => report, cloud: () => cloud, promo: () => (cloud ? null : account.promo()),
        openPromo: () => { $('saves').close(); account.open(); }, edit: (name) => editor.openSave(name) });
      mountTacticsPanel({ FS, sync, log });
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
      const onHide = () => { if (handedOver) return; if (vault?.active) vaultRound({ force: true }); sync(); };
      addEventListener('pagehide', onHide);
      document.addEventListener('visibilitychange', () => { if (document.hidden) onHide(); });
      primeOffline(info).catch((e) => log(`page: not kept for a start without a connection: ${e}`));
      log(`page: main(${M.arguments.join(' ')})`);
      try {
        const quiet = (e) => e?.name === 'ExitStatus' || e?.name === 'EngineLetGo';   // an exit, or the page's own stop
        const r = M.callMain(M.arguments);
        if (r && typeof r.then === 'function') {
          engine.ended = r.then(() => {}, () => {});
          r.catch((e) => { if (!quiet(e)) { running = false; logStack(e); fail('The game stopped', String(e)); } });
        }
      } catch (e) {
        if (e?.name !== 'ExitStatus' && e?.name !== 'EngineLetGo') { running = false; logStack(e); fail('The game stopped', String(e)); }
      }
    },
    onExit: (code) => {
      running = false;
      // Exit right after a save: the save is stored before the box says the game ended (closing the tab meanwhile
      // asks first), where an ordinary write would leave a file of the last moment for later.
      const stored = storeEverything();
      if (code === 0) {
        status('Saving…');
        overlay('<h1>The game ended</h1><p>Your saves go to this browser\'s storage…</p>');
        stored.finally(() => { status('The game ended'); overlay('<h1>The game ended</h1><p>Reload the page to play again. Your saves stay in this browser.</p>'); });
      } else {
        const box = fatalBox();
        fail(`The game ended with code ${code}`, box
          ? `The game stopped on an error of its own: "${box.text}" (at ${box.site}). Send to the developer, with the save you loaded, so it can be looked at. Your saves stay in this browser.`
          : 'Help, Engine log says why. Your saves stay in this browser.');
      }
    },
    onAbort: (what) => { if (engine.letGo) return; running = false; logStack(what); fail('The game stopped', String(what)); },
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

// An iPhone or an iPad, by name, or null: every browser there is WebKit inside (Chrome too), so the answer is the
// system's version, not another browser. An iPad asks for the desktop site and says Mac, with a touch screen.
function appleMobile() {
  const ua = navigator.userAgent;
  if (/iPhone|iPod/.test(ua)) return 'iPhone';
  if (/iPad/.test(ua) || (/Macintosh/.test(ua) && navigator.maxTouchPoints > 1)) return 'iPad';
  return null;
}

function requirements(info) {
  const problems = [], warnings = [];
  if (typeof WebAssembly !== 'object' || typeof WebAssembly.instantiate !== 'function') {
    problems.push('This browser has no WebAssembly, which the engine is made of.');
    return { problems, warnings };
  }
  if (info.chosen.suspend !== 'asyncify' && !hasJspi())
    problems.push(appleMobile()
      ? `The engine needs JSPI, a WebAssembly feature that this ${appleMobile()} does not have yet. ${appleMobile() === 'iPad' ? 'iPadOS' : 'iOS'} 27 has it, in every browser (Safari and Chrome alike): update in Settings, General, Software Update, then open the game again.`
      : 'The engine needs JSPI, a WebAssembly feature that this browser lacks, and this server has no engine for other browsers. Chrome 137 or later has it, and Safari 27 (iOS and iPadOS 27, in every browser of an iPhone or iPad).');
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
    <p>${appleMobile() ? `It runs on an ${appleMobile()} with ${appleMobile() === 'iPad' ? 'iPadOS' : 'iOS'} 27 or later, and on a computer in Chrome 137 or later or Safari 27 or later.`
      : 'It runs in Chrome 137 or later, and in Safari 27 or later: on an iPhone or iPad with iOS or iPadOS 27, in Safari or Chrome.'} ${esc(note)}</p>`, 'error');
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

// ---- One tab at a time ----
// Every tab of the game writes the same storage: the user folder (IDBFS) and the vault. A write of one tab removed what
// another had stored meanwhile: a new save went that way, and its Download was an empty file, which an upload refuses as
// "too short to be a saved game". So one tab plays at a time. The tab that runs the game holds a Web Lock; a tab that wants to start
// asks it to stop (BroadcastChannel), and it ends its game, puts its saves in the storage and lets the lock go. A
// browser without Web Locks starts as before.
const TURN = 'cm0102-game';
const TURN_WAIT = 90000;                               // for the other tab to stop: its storing can take a while
let turnRelease = null, startAbort = null, turnClient = null;
const turnChannel = (() => { try { return new BroadcastChannel(TURN); } catch { return null; } })();
const timeoutSignal = (ms) => { const c = new AbortController(); setTimeout(() => c.abort(new DOMException('timed out', 'TimeoutError')), ms); return c.signal; };
const within = (p, ms) => Promise.race([Promise.resolve(p).catch(() => {}), new Promise((r) => setTimeout(r, ms))]);

// true when this tab has the turn (options: those of navigator.locks.request), false when another tab kept it.
function takeTurn(options = {}) {
  if (typeof navigator.locks?.request !== 'function') return Promise.resolve(true);
  return new Promise((ok, bad) => {
    let granted = false;
    navigator.locks.request(TURN, options, (lock) => {
      if (!lock) { ok(false); return undefined; }
      granted = true;
      ok(true);
      navigator.locks.query?.().then(({ held }) => { turnClient = held.find((l) => l.name === TURN)?.clientId ?? null; }).catch(() => {});
      return new Promise((release) => { turnRelease = release; });
    }).catch((e) => {
      if (granted) { turnRelease = null; stopHere(false); return; }   // taken without asking: Play here anyway
      if (e?.name === 'AbortError' || e?.name === 'TimeoutError') ok(false); else bad(e);
    });
  });
}

// This tab stops for another one: the game ends, the saves go to the storage (store), then nothing of this tab writes
// there again and the lock goes.
async function stopHere(store) {
  if (turnGone) return;
  turnGone = true;
  log(`page: the game moves to another tab${store ? ': it stops here, and the saves go to the browser\'s storage first' : ''}`);
  running = false;
  if (!store) handedOver = true;                       // the other tab plays already: not one more write from here
  startAbort?.abort();
  status('The game moved to another tab');
  overlay(`<h1>The game moves to another tab</h1><div class="row"><i class="spin"></i>${store ? 'Your saves go to this browser\'s storage first.' : 'The game here stops.'}</div>`);
  await within(letEngineGo(), 10000);
  if (store) await within(storeEverything(), 60000);
  handedOver = true;
  turnRelease?.();
  turnRelease = null;
  for (const id of ['saves-open', 'editor-open', 'kbd-open', 'right-click']) if ($(id)) $(id).hidden = true;   // they would change nothing stored
  for (const d of document.querySelectorAll('dialog[open]')) d.close();
  overlay(`<h1>The game moved to another tab</h1><p>${store ? 'It runs in the other tab now, with your saves.'
    : 'Another tab took it without waiting for this one: what this tab had not put in the browser\'s storage yet is not there.'}
    You can close this one.</p>
    <div class="row"><button class="primary" id="turn-back">Play here</button></div>`);
  $('turn-back').onclick = () => { leaving = true; location.reload(); };
}
turnChannel?.addEventListener('message', (e) => { if (e.data?.type === 'take' && turnRelease) stopHere(true); });

// Whether this tab still has the turn, as the browser says now. A tab whose turn another took without asking (Play here
// anyway, while this one was paused) learns it from its lock request only later: each write of the storage asks first.
async function stillMine() {
  if (handedOver) return false;                        // not before: a hand-over stores the saves first, with the turn
  if (!turnClient || typeof navigator.locks?.query !== 'function') return true;
  try {
    const { held } = await navigator.locks.query();
    return held.some((l) => l.name === TURN && l.clientId === turnClient);
  } catch { return true; }
}

// A page of the game from before one tab at a time runs in another tab and keeps the saves' storage open (vault.js,
// VAULT_VERSION): this one waits until that tab is closed or loaded again, and says so.
function olderPageBlocks(on) {
  if (on) {
    log('page: an older page of the game, in another tab, keeps the saves\' storage open: waiting until it is closed or loaded again');
    status('The game is open in another tab');
    overlay(`<h1>The game is open in another tab</h1><p>An older page of the game runs in another tab or window of this
      browser. Close that tab or load it again: the game then goes on here by itself. Two at once would delete each other's
      saves.</p><div class="row"><i class="spin"></i>Waiting for the other tab…</div>`);
  } else {
    log('page: the other tab let the saves\' storage go');
    status(running ? '' : 'Starting the engine…');
    overlay(running ? '' : '<h1>Starting the engine</h1><div class="row"><i class="spin"></i>The first start can take a minute.</div>');
  }
}

// Before the game files load: true when this tab may run the game. A Restart's reload can come before the page it
// replaces is gone, so the first try waits a moment.
async function myTurn() {
  try {
    if (await takeTurn({ signal: timeoutSignal(1500) })) return true;
  } catch (e) { log(`page: one tab at a time did not work here (${e?.message ?? e}): the game starts`); return true; }
  log('page: the game runs in another tab of this browser');
  status('The game is open in another tab');
  overlay(`<h1>The game is open in another tab</h1>
    <p>CM 01/02 runs in another tab or window of this browser. One may play at a time: two would delete each other's
    saves from the browser's storage.</p>
    <p class="small">Play here stops the game in the other tab, after it puts its saves in the browser's storage. What was
    not saved in the game is lost there, as when that tab closes.</p>
    <div class="row"><button class="primary" id="turn-take">Play here</button></div>`);
  await new Promise((r) => { $('turn-take').onclick = r; });
  for (;;) {
    status('Stopping the game in the other tab…');
    overlay('<h1>Stopping the game in the other tab</h1><div class="row"><i class="spin"></i>Its saves go to the browser\'s storage first.</div>');
    turnChannel?.postMessage({ type: 'take' });
    let got = false;
    try { got = await takeTurn({ signal: timeoutSignal(TURN_WAIT) }); } catch (e) { log(`page: ${e?.message ?? e}`); }
    if (got) { log('page: the other tab stopped; the game runs here'); return true; }
    log(`page: the other tab did not stop in ${TURN_WAIT / 1000} s`);
    status('The other tab did not stop');
    overlay(`<h1>The other tab did not stop</h1><p>It does not answer: the browser may have paused it. Close the other tab
      or window of this site, then press Try again. Play here anyway starts the game here at once; the other tab then stops
      when it wakes, without storing what it had not stored yet.</p>
      <div class="row"><button class="primary" id="turn-again">Try again</button><button id="turn-steal">Play here anyway</button></div>`, 'error');
    const choice = await new Promise((r) => { $('turn-again').onclick = () => r('again'); $('turn-steal').onclick = () => r('steal'); });
    if (choice === 'steal') {
      log('page: the game runs here; the other tab stops when it wakes');
      if (await takeTurn({ steal: true })) return true;
    }
  }
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
  if (info.stats) {
    report = mountReport({ FS: () => window.Module?.FS ?? null, lines: () => lines, who: () => stats?.who?.() ?? {}, inPlay: () => saveInPlay(), log });
    $('help-report').hidden = false;
    $('help-report-open').onclick = () => { $('help').close(); report.open({ where: 'Help' }); };
  }
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
      ${TOUCH ? '<p class="small">On a touch screen: tap to click, long press for the right button (keep the finger down to drag with it), two fingers to scroll. Help has the list.</p>' : ''}
      <div class="row"><button class="primary" id="go">Start the game</button><span class="free-note">Free · no account, no payment</span></div>
      ${plansForStart(await Promise.race([account.ready, new Promise((r) => setTimeout(r, 1500))]))}`);
    if ($('go-plans')) $('go-plans').onclick = (e) => { e.preventDefault(); account.open(); };
    await new Promise((r) => { $('go').onclick = r; });
  }
  if (!await myTurn()) return;
  overlay(`<h1>${toGet > 0 ? 'Loading the game files' : 'Starting'}</h1><div class="bar"><i id="bar-fill"></i></div><div class="small" id="bytes"></div>`);
  status('Loading the game files…');
  const t0 = performance.now();
  const counts = { cached: 0, fetched: 0 };
  const abort = new AbortController();
  startAbort = abort;                                  // another tab takes the game meanwhile: the download stops
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
    if (turnGone) return;
    if (!compileError) {
      // Not a dropped connection when the browser had no memory for the file: fail() says what to do then.
      fail('A game file did not load', OUT_OF_MEMORY.test(String(e?.message ?? e)) ? `${e.message ?? e}.`
        : `${e.message ?? e}. The connection may have dropped. The game files that arrived stay in this `
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
  if (turnGone) return;
  overlay('<h1>Starting the engine</h1><div class="row"><i class="spin"></i>The first start can take a minute.</div>');
  status('Starting the engine…');
  runEngine(files, info);
}

start();
