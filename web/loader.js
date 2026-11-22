// The page side of the browser build (web/README.md): it downloads the game files, puts them into the engine's file
// system at /game, keeps the user folder /user (saves, settings.txt) in the browser, and starts the recompiled game
// on the canvas. The game runs in this tab; the server only hands out files.
//
// Around the game: layout.js sizes the canvas, touch.js turns touches into the mouse, keyboard.js routes the keys and
// runs the on-screen keyboard, saves.js is the Saved games panel, account.js the account and the cloud saves, ui.js
// has the messages and the small helpers.
import { SAVE_DIR, listSaves, mountSavesPanel, savesSignature, setStorageKept, skipWorkFiles, spaceState, storageWarning, useVault } from './saves.js';
import { needsChoice, setStatsAllowed, startStats, statsAllowed } from './stats.js';
import { createVault, isAway } from './vault.js';
import { engineUrl, hasJspi, pickEngine } from './engine.js';
import { mountEditor } from './editor.js';
import { mountSetsPanel } from './setspanel.js';
import { mountTacticsPanel } from './tactics.js';
import { createSetSync, listDataSets } from './datasets.js';
import { createCloudSync, keepInCloud, legalLinks, mountAccount, plansHtml } from './account.js';
import { inviteKind, keptAccess, scoutAccess } from './scout.js';
import { guardKeys, hasTouch, mountKeyboard, onPageKey } from './keyboard.js';
import { TOUR_STEPS, cloudStepText, mountTour } from './tour.js';
import { mountLayout } from './layout.js';
import { mountMacRightButton, mountTouch } from './touch.js';
import { fitBar, guardLeave, isFullAlready, isStandalone, keepAwake, mountBusy, toast, workerTimer } from './ui.js';
import { mountVisitMark, suddenStopText } from './lastvisit.js';
import { storageCopiesFiles, sweepDisk } from './disk.js';
import { mountReport, watchSaveInPlay } from './report.js';
import { mountPlay } from './play.js';
import { watchRelease } from './release.js';
import { watchProgress } from './progress.js';
import { createTacticSync } from './tacticsync.js';
import { reserveSaveRoom } from './room.js';
import { LANGUAGES, LANG_KEY, language, onLanguage, pickLanguage, priceText, setLanguage, t, tn } from './i18n.js';

const $ = (id) => document.getElementById(id);
const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const mb = (n) => `${(n / 1048576).toFixed(n < 10485760 ? 1 : 0)} MB`;
// A text that the page both shows and logs or reports: { key: its English with {placeholders}, vars } (a var may be
// such a text too: an error's), or a plain string (an error's own message, the engine's words). english() gives it for
// the log, the statistics and Send to the developer; shown() in the page's language, for the player (a function: its
// text now). An error of the page's own carries key and vars as well (errorMsg).
const fillIn = (s, vars) => (vars ? s.replace(/\{(\w+)\}/g, (m, k) => (k in vars ? String(vars[k]) : m)) : s);
const isMsg = (m) => Boolean(m) && typeof m === 'object' && typeof m.key === 'string';
const varsOf = (vars, f) => (vars ? Object.fromEntries(Object.entries(vars).map(([k, v]) => [k, isMsg(v) ? f(v) : v])) : vars);
const english = (m) => (isMsg(m) ? fillIn(m.key, varsOf(m.vars, english)) : String(m));
const shown = (m) => (typeof m === 'function' ? m() : isMsg(m) ? t(m.key, varsOf(m.vars, shown)) : String(m));
const errorMsg = (e) => (isMsg(e) ? { key: e.key, vars: e.vars } : String(e?.message ?? e));
const CONNECTIONS = 6;
const TOUCH = hasTouch();

// The page's language (i18n.js): ?lang=<code> (checks and screenshots), else the one chosen in Help, else the browser's
// when the game speaks it, else English. Only a choice made in Help is kept; the start box shows in that language.
const langParam = new URLSearchParams(location.search).get('lang');
const keptLang = (() => { try { return localStorage.getItem(LANG_KEY); } catch { return null; } })();
await setLanguage(pickLanguage({ stored: langParam ?? keptLang, browser: navigator.languages ?? [navigator.language] }), { keep: false });
{
  const select = $('help-lang');
  for (const { code, name } of LANGUAGES) select.add(new Option(name, code));
  select.value = language();
  select.addEventListener('change', () => {
    setLanguage(select.value, { storage: (() => { try { return localStorage; } catch { return null; } })() }).then((code) => { select.value = code; });
  });
}

const lines = [];
let progress = null;                                   // the game's long jobs in the bar (progress.js)
// The mark of a visit that loads or runs the game (lastvisit.js): the next visit finds it when the system closed the tab.
const visit = mountVisitMark({ storage: (() => { try { return localStorage; } catch { return null; } })() });
addEventListener('pagehide', (e) => visit.end({ hide: e.persisted }));
addEventListener('pageshow', (e) => { if (e.persisted) visit.resume(); });

function log(text) {
  progress?.line(String(text));
  const stage = /^recomp: progress: (.+)$/.exec(String(text));
  if (stage) visit.step(/ took [\d.]+ s$/.test(stage[1]) ? 'in the game' : stage[1]);   // the game's long jobs: "Loading game data"…
  lines.push(String(text));
  if (lines.length > 3000) lines.splice(0, lines.length - 3000);
  $('log').textContent = lines.join('\n');
}

// The bar's status: a text, or text() for one in the page's language, written again when the language changes.
let statusText = null;
function status(text) {
  statusText = typeof text === 'function' ? text : null;
  $('status').textContent = statusText ? statusText() : text;
}

// The box over the game: html, or html() for one in the page's language, drawn again when the language changes.
// on: { id: handler } for the box's buttons and links, by their ids, so that they still work when it is drawn again.
let overlayNow = null, overlayOn = {};
function drawOverlay(html, kind) {
  $('overlay').innerHTML = html ? `<div class="box ${kind}">${html}</div>` : '';
  $('overlay').style.display = html ? '' : 'none';
}
function overlay(html, kind = '', on = {}) {
  overlayNow = typeof html === 'function' ? { html, kind } : null;
  overlayOn = on;
  drawOverlay(overlayNow ? html() : html, kind);
}
$('overlay').addEventListener('click', (e) => {
  const el = e.target instanceof Element ? e.target.closest('button[id], a[id]') : null;
  if (el && Object.hasOwn(overlayOn, el.id)) overlayOn[el.id](e);
});
// The box while the engine starts.
const startingBox = () => `<h1>${t('Starting the engine')}</h1><div class="row"><i class="spin"></i>${t('The first start can take a minute.')}</div>`;
onLanguage(() => {
  if (statusText) $('status').textContent = statusText();
  if (overlayNow) drawOverlay(overlayNow.html(), overlayNow.kind);
});

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
      else if ((() => { try { return isAway(FS.lookupPath(p).node); } catch { return false; } })()) sum.stored += st.size;  // a placeholder
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
// title and detail: texts as english() and shown() take them; the statistics and the report get their English.
function fail(title, detail) {
  visit.end();                                       // the page stopped and says why: not a sudden stop
  const titleEn = english(title), detailEn = english(detail);
  status(() => shown(title));
  stats?.report(`${titleEn}: ${detailEn}`, lines.slice(-60), 'fail');
  const last = lines.slice(-8).join('\n');
  // The memory of a tab is only given back for sure when the tab is closed (a page that loaded again in the same tab
  // once kept the game before it: letEngineGo), so that is the first thing to try.
  const memory = () => (OUT_OF_MEMORY.test(detailEn) ? `<p>${t('The browser ran out of memory for the game.')} ${
    t('{close}: loading the page again in this same tab does not give its memory back.', { close: `<b>${t('Close this tab and open the game in a new one')}</b>` })} ${
    t('If it happens again, close the other tabs of this site, or quit the browser and open it again.')} ${t('Your saves stay in this browser.')}</p>` : '');
  overlay(() => `<h1>${esc(shown(title))}</h1><p>${esc(shown(detail))}</p>${memory()}${last ? `<pre class="small" style="white-space:pre-wrap;margin:0 0 14px">${esc(last)}</pre>` : ''}
    <div class="row"><button class="primary" id="fail-again">${t('Start again')}</button>${report ? `<button id="fail-report">${t('Send to the developer')}</button>` : ''}<span class="small">${t('Your saves stay in this browser.')}</span></div>`, 'error', {
    'fail-report': () => report?.open({ message: `${titleEn}: ${detailEn}`, shown: () => `${shown(title)}: ${shown(detail)}`, where: 'the error box' }),
    // The way on: the page loads again, after the user folder went to the browser's storage.
    'fail-again': () => {
      running = false; leaving = true;
      const go = () => location.reload();
      storeEverything().then(() => letEngineGo()).then(go, go);
      setTimeout(go, 15000);                             // a storage that does not answer
    },
  });
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
// Safari's and an iPhone's stacks hold no message ("start@…/loader.js:1100:17"), so the message goes first.
const errorText = (err) => { const m = err?.message ? `${err.name ?? 'Error'}: ${err.message}` : String(err); return err?.stack && !err.stack.includes(m) ? `${m}\n${err.stack}` : err?.stack ?? m; };
addEventListener('unhandledrejection', (e) => log(`page: ${errorText(e.reason)}`));

// ---- Before the engine: the listeners that must run ahead of SDL's, and the layout. ----

let running = false, leaving = false;
// One tab at a time (myTurn, below): another tab took the game. turnGone at once, handedOver after this tab's last store;
// from then on nothing of this tab writes the browser's storage.
let turnGone = false, handedOver = false;
let keyboard = null;
// The keys of a tab list (WAI-ARIA tabs): Left and Right go to the tab before or after, Home and End to the first and the
// last, among the tabs offered (one whose section is hidden is not). On the window before guardKeys, whose listener stops
// every key of an open dialog there, so that the game gets none of them.
function savesTabKeys(e) {
  const list = e.target instanceof Element ? e.target.closest('#saves .tabs') : null;
  if (!list) return;
  const tabs = [...list.querySelectorAll('[data-tab]')].filter((b) => getComputedStyle(b).display !== 'none');
  const at = tabs.indexOf(e.target.closest('[data-tab]'));
  const to = at < 0 ? undefined : { ArrowRight: at + 1, ArrowLeft: at - 1, Home: 0, End: tabs.length - 1 }[e.key];
  if (to === undefined) return;
  e.preventDefault();
  const b = tabs[(to + tabs.length) % tabs.length];
  showSavesTab(b.dataset.tab);
  b.focus();
}
addEventListener('keydown', savesTabKeys, true);
guardKeys();
guardLeave(() => (running || storing) && !leaving);
const canvas = $('canvas');
const layout = mountLayout({ app: $('app'), bar: $('bar'), stage: $('stage'), canvas, chip: $('type-chip'), isTouch: TOUCH });
// The Right click button (touch devices): the next tap on the game is a click of the right button. A long press does
// the same, but a finger that wanders turns it into a drag, and on the tactics board a drag moves the player.
const setRightButton = (on) => $('right-click').setAttribute('aria-pressed', String(on));
const touch = mountTouch({ canvas, stage: $('stage'), fx: $('fx'), pictureWidth: () => layout.picture.w, onFirstTouch: resumeAudio, onTap: (x, y) => keyboard?.onTap(x, y),
  onRightUsed: () => setRightButton(false) });
$('right-click').onclick = () => {
  const on = !touch.rightArmed;
  touch.armRight(on);
  setRightButton(on);
  if (on) toast(t('Tap the game: that tap is a right click'), { ms: 2500 });
};
mountBusy($('busy'));
fitBar($('bar'), $('app'));                            // icons only when the buttons do not fit in one row

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
// Not while the tour's card is up: Help's "Show the tour" closes Help, whose close event comes after the card took the focus.
const backToGame = () => { if (!document.querySelector('dialog[open]') && document.activeElement !== $('kbd') && !$('tour')) canvas.focus({ preventScroll: true }); };
$('bar').addEventListener('click', (e) => { if (e.target.closest('button') && !e.target.closest('#kbd-open')) setTimeout(backToGame); });
for (const d of document.querySelectorAll('dialog')) d.addEventListener('close', backToGame);
$('help-open').onclick = () => $('help').showModal();
// Saved games in tabs (index.html): Saves, Tactics, Databases. The tab chosen stays for the visit.
function showSavesTab(tab) {
  const d = $('saves');
  d.dataset.tab = tab;
  for (const b of d.querySelectorAll('.tabs [data-tab]')) {
    b.setAttribute('aria-selected', String(b.dataset.tab === tab));
    b.tabIndex = b.dataset.tab === tab ? 0 : -1;       // one stop in the Tab order; the arrows go between the tabs
  }
}
showSavesTab('saves');
$('saves-open').addEventListener('click', () => showSavesTab('saves'));     // Saved games opens on the saves
$('saves').querySelector('.tabs').addEventListener('click', (e) => { const b = e.target.closest('[data-tab]'); if (b) showSavesTab(b.dataset.tab); });


// ---- The account and the cloud saves (account.js). ----

const account = mountAccount({ log, toast, isRunning: () => running, isStandalone, leave: () => { leaving = true; } });
account.ready.then((me) => { $('help-cloud').hidden = !me?.auth; });
// The Scout (scout.js): whether this browser's account is a member, kept from each answer of /api/me, so a member
// offline keeps the whole Scout.
const MEMBER_KEY = 'cm0102-member';
const keptMember = () => { try { return localStorage.getItem(MEMBER_KEY); } catch { return null; } };
const keepMember = (me) => { const k = keptAccess(me); if (k) try { localStorage.setItem(MEMBER_KEY, k); } catch { /* not kept */ } };
account.ready.then(keepMember);
account.onAccountChange(keepMember);
let cloud = null, cloudBehind = false, watcher = null, setSync = null, tacticSync = null;

// The sync of the account's saves, when it has cloud saves. replace: the saves of the cloud may replace the ones here
// (before the game runs). Resolves to the jobs done, or null.
async function startCloud(FS, { replace, onStep }) {
  await account.ready;
  const me = account.me;                         // as now: a sign-in since the page opened counts
  cloud?.stop();
  cloud = null;
  setSync?.stop();
  setSync = null;
  tacticSync?.stop();
  tacticSync = null;
  account.setCloud(null);
  if (!me?.user?.cloud) return null;
  if (typeof CompressionStream !== 'function') { log('page: no CompressionStream in this browser: no cloud saves'); return null; }
  cloud = createCloudSync({ FS, sync, log, toast, account: me.user.email, onWrote: () => watcher?.rebase() });
  account.setCloud(cloud);
  setSync = createSetSync({ FS, root: SAVE_DIR, sync, log, toast, account: me.user.email });   // runs once the game is up
  tacticSync = createTacticSync({ FS, sync, log, account: me.user.email });
  tacticSync.run();                                     // the tactics of the other devices, before the game lists them
  const jobs = await cloud.run({ replace, onStep, signal: onStep?.signal });
  cloudBehind = jobs === null || jobs.failed.length > 0 || jobs.held?.length > 0;
  const came = jobs?.filter((j) => j.do === 'down' && !jobs.failed.includes(j.name)).length ?? 0;
  if (jobs === null && !onStep?.signal?.aborted) toast(t('The cloud was not reached: the saves here are used, and go up later'), { kind: 'bad', ms: 7000 });
  else if (jobs?.failed.length) toast(notYet(jobs.failed), { kind: 'bad', ms: 7000 });
  else if (came) toast(tn(came, { one: '{n} saved game came from the cloud', other: '{n} saved games came from the cloud' }));
  return jobs;
}

// The saves that the cloud did not take this time (the server or its store failed them): they go at the next sync.
const notYet = (names) => (names.length === 1
  ? t('{name} is not in the cloud yet: the cloud did not take it this time. It goes up in a minute.', { name: names[0] })
  : tn(names.length, { one: '{n} save is not in the cloud yet: the cloud did not take it this time. It goes up in a minute.',
    other: '{n} saves are not in the cloud yet: the cloud did not take them this time. They go up in a minute.' }));

// After a save: up to the cloud. A sync that failed (no connection) or left a save behind is tried again each minute,
// and so is one that held a large save back (account.js, BIG_EVERY_MS: at most every 10 minutes). force: the tab is
// left, so a held save goes now.
let heldSaid = false;
function cloudAfterSave(quiet = false, force = false) {
  if (!cloud || cloud.stopped) return;
  cloud.run({ replace: false, force }).then((jobs) => {
    cloudBehind = jobs === null || jobs.failed.length > 0 || jobs.held?.length > 0;
    if (jobs === null) { if (!quiet) toast(t('Not in the cloud yet: no connection. It goes up later.'), { kind: 'bad', ms: 6000 }); }
    else if (jobs.failed.length) { if (!quiet) toast(notYet(jobs.failed), { kind: 'bad', ms: 6000 }); }
    else if (jobs.some((j) => j.do !== 'down')) toast(t('Saved to the cloud'));
    else if (jobs.held?.length && !quiet && !heldSaid) {
      heldSaid = true;
      toast(t('{name} goes to the cloud within 10 minutes: a large save goes up at most every 10 minutes, and when you leave this tab.', { name: jobs.held[0] }), { ms: 9000 });
    }
  });
}
setInterval(() => { if (cloudBehind && running) cloudAfterSave(true); }, 60000);   // the try again says nothing till it works
setInterval(() => { if (running) tacticSync?.runIfChanged(); }, 60000);            // a tactic the game saved goes up
account.onAccountChange((me) => {
  const FS = window.Module?.FS;
  if (!running || !FS) return;                   // before the game, the start does it
  if (!me?.user?.cloud) { cloud?.stop(); cloud = null; setSync?.stop(); setSync = null; account.setCloud(null); return; }
  startCloud(FS, { replace: false }).then(() => setSync?.run());   // signed in while the game runs: new saves come in, none is replaced
});
// The start box tells the plans: free, or Cloud Saves. Signing in leaves the page, which is fine before the game.
function plansForStart(me) {
  if (!me?.auth) return '';
  const links = `<p class="small legal">${legalLinks()}</p>`;
  if (me.user?.cloud) return `<p class="small">${t('Signed in as {email} · Cloud Saves are on: your saves from other devices come in when the game starts.', { email: esc(me.user.email) })}</p>`;
  if (me.user) return `<p class="small">${t('Signed in as {email} · Free plan. Cloud Saves ({price}) keep your saves on all your devices: {link}.', {
    email: esc(me.user.email), price: esc(priceText(me.offer.price)), link: `<a href="#" id="go-plans">${t('see Cloud Saves')}</a>` })}</p>${links}`;
  // Signed out: the game is free, and Cloud Saves is a note under it, not a plan to pick.
  return `<div class="addon"><b>${t('Optional: Cloud Saves')}</b> · <span>${t('{price}. Your saves on every computer and iPad, backed up after each save.', { price: esc(priceText(me.offer.price)) })} ${
    t('{signin} to get it, or to bring the cloud saves you have.', { signin: `<a href="/auth/google/start?mode=page" id="go-signin">${t('Sign in with Google')}</a>` })} ${
    t('Playing needs no account.')}</span></div>${links}`;
}
// The page's footer: "Made with ♥ by <CM_SELLER>" (files.json maker), or without the name.
const heart = () => `<svg viewBox="0 0 24 24" aria-label="${t('love')}" role="img"><path d="M12 21.2 10.6 20C5.4 15.3 2 12.3 2 8.5 2 5.4 4.4 3 7.5 3c1.7 0 3.4.8 4.5 2.1C13.1 3.8 14.8 3 16.5 3 19.6 3 22 5.4 22 8.5c0 3.8-3.4 6.8-8.6 11.5L12 21.2Z"/></svg>`;
const madeWith = (maker) => (maker ? t('Made with {heart} by {maker}', { heart: heart(), maker: esc(maker) }) : t('Made with {heart}', { heart: heart() }));
// Once in this browser, after its first save: a player without Cloud Saves hears that they exist.
function cloudHintOnce() {
  const hint = account.promo();
  if (!hint) return;
  try { if (localStorage.getItem('cm-cloud-hint')) return; localStorage.setItem('cm-cloud-hint', '1'); } catch { return; }
  setTimeout(() => toast(account.me?.user ? t('{hint} See your name, top right.', { hint }) : t('{hint} See Sign in, top right.', { hint }), { ms: 9000 }), 2500);
}
$('help-close').onclick = () => $('help').close();
// The first-visit tour (tour.js): once in this browser after the game has started, and from Help at any time.
const tour = mountTour({ storage: (() => { try { return localStorage; } catch { return null; } })(), onPageKey,
  steps: TOUR_STEPS.map((s) => (s.target === 'account-open' ? { ...s, text: () => cloudStepText(account.me) } : s)),
  onEnd: () => { if (!document.querySelector('dialog[open]')) canvas.focus({ preventScroll: true }); } });
$('help-tour').onclick = () => { $('help').close(); tour.start(); };
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
  const label = on ? t('Show the buttons') : t('Hide these buttons');
  $('bar-fold').setAttribute('aria-expanded', String(!on));
  $('bar-fold').setAttribute('aria-label', label);
  $('bar-fold').title = label;
  if (remember) try { if (on) localStorage.setItem(FOLD_KEY, '1'); else localStorage.removeItem(FOLD_KEY); } catch { /* storage off */ }
}
try { if (localStorage.getItem(FOLD_KEY)) setBarFolded(true, false); } catch { /* storage off: open */ }
$('bar-fold').onclick = () => setBarFolded(!$('app').classList.contains('bar-folded'));
const fullscreenLabel = (on) => {
  $('fullscreen').setAttribute('aria-label', on ? t('Leave full screen') : t('Full screen'));
  $('fullscreen').querySelector('span').textContent = on ? t('Leave full screen') : t('Full screen');
};
layout.onFullscreen((on) => {
  $('fullscreen').setAttribute('aria-pressed', String(on));
  fullscreenLabel(on);
});
// The labels the page set, in a language chosen later.
onLanguage(() => {
  setBarFolded($('app').classList.contains('bar-folded'), false);
  fullscreenLabel(layout.isFullscreen());
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
// Game files out of the page's memory (vault.js, gameFile): with a JSPI engine and the browser's cache storage, a file
// larger than this stays in the cache, and the game reads it from there in pieces when it opens it. Smaller ones (272
// files, 1.8 MB) come into memory: each lazy file costs a look in the cache at its first open.
const LAZY_MIN = 64 << 10;
// A lazy game file in the list download gives: where the cache keeps it, and its size.
const lazyFile = (f) => ({ lazy: true, url: fileUrl(f), size: f.size });
const isLazy = (v) => Boolean(v?.lazy);

async function download(files, cache, onBytes, counts, signal, { lazy = false } = {}) {
  const out = new Map();
  let next = 0;
  // The body of r into an array of the file's size; got(n) counts the bytes as they come.
  async function readAll(r, f, fromCache, got = () => {}) {
    // An empty file: the browser's cache gives its copy with no body at all (the stand-in game's empty .dat files made a
    // second visit stop on "Cannot read properties of null (reading 'getReader')").
    if (!r.body) {
      if (f.size === 0) return new Uint8Array(0);
      throw Object.assign(new Error(`${f.path}: no body, where ${f.size} bytes were wanted`), { again: !fromCache,
        key: '{path}: no body, where {size} bytes were wanted', vars: { path: f.path, size: f.size } });
    }
    const buf = new Uint8Array(f.size), reader = r.body.getReader();
    let at = 0;
    for (;;) {
      const { done, value } = await reader.read();
      if (done) break;
      if (at + value.length > f.size) throw Object.assign(new Error(`${f.path}: more than ${f.size} bytes`), { key: '{path}: more than {size} bytes', vars: { path: f.path, size: f.size } });
      buf.set(value, at);
      at += value.length;
      onBytes(value.length, fromCache);
      got(value.length);
    }
    if (at !== f.size) throw Object.assign(new Error(`${f.path}: ${at} of ${f.size} bytes`), { again: true, key: '{path}: {got} of {size} bytes', vars: { path: f.path, got: at, size: f.size } });
    return buf;
  }
  async function one() {
    while (next < files.length) {
      const f = files[next++];
      const url = fileUrl(f);
      const kept = cache ? await cache.match(url) : undefined;
      const leave = lazy && cache && f.size > LAZY_MIN;     // stays in the cache: not read now
      let buf;
      if (kept && leave) { onBytes(f.size, true); buf = lazyFile(f); }
      else if (kept) buf = await readAll(kept, f, true);
      else if (leave) {
        // Into the cache as it comes (the response as the server sent it: a new Response with its headers and the
        // decoded body was refused, "Cache.put() encountered a network error"), counted on the way by reading the other
        // copy, whose pieces are dropped: nothing of the file stays in the page's memory.
        for (let i = 0; ; i++) {
          let got = 0;
          try {
            const r = await fetch(url, { signal });
            if (!r.ok) throw Object.assign(new Error(`${f.path}: HTTP ${r.status}`), { again: r.status >= 500 });
            const storing = cache.put(url, r.clone());
            const reader = r.body.getReader();
            for (;;) {
              const { done, value } = await reader.read();
              if (done) break;
              got += value.length;
              onBytes(value.length, false);
            }
            await storing;
            if (got !== f.size) {
              await cache.delete(url);
              throw Object.assign(new Error(`${f.path}: ${got} of ${f.size} bytes`), { again: true, key: '{path}: {got} of {size} bytes', vars: { path: f.path, got, size: f.size } });
            }
            buf = lazyFile(f);
            break;
          } catch (e) {
            if (got) onBytes(-got, false);
            const again = e.name !== 'AbortError' && (e instanceof TypeError || e.again);
            if (!again || i >= FILE_TRIES.length || signal?.aborted) throw e;
            log(`page: ${f.path}: ${e.message}; asked for again in ${FILE_TRIES[i] / 1000} s`);
            await new Promise((ok) => setTimeout(ok, FILE_TRIES[i]));
          }
        }
      } else {
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

// The files into the engine's file system, without a copy (the file system keeps the arrays). A lazy file (download,
// LAZY_MIN) becomes a placeholder that reads from the cache (vault.gameFile); the files must all be in memory when the
// vault could not hook the engine's open and read.
function install(FS, files, gameFile) {
  let lazy = 0, lazyBytes = 0;
  for (const [path, data] of files) {
    const full = `/game/${path}`;
    FS.mkdirTree(full.slice(0, full.lastIndexOf('/')));
    if (isLazy(data)) {
      const { url, size } = data;
      gameFile(full, size, async () => {
        const r = await gameCache?.match(url);
        if (!r) {
          throw Object.assign(new Error(`the browser's cache no longer has the game file ${path}: load the page again`),
            { key: 'the browser\'s cache no longer has the game file {path}: load the page again', vars: { path } });
        }
        return r.blob();
      });
      lazy++; lazyBytes += size;
    } else FS.writeFile(full, data, { canOwn: true });
  }
  return { lazy, lazyBytes };
}
let gameCache = null;                                 // the cache that the lazy game files are read from
// The lazy files of the list into memory, from the cache: the way back when the engine's open could not be hooked.
async function bringIn(files) {
  for (const [path, data] of files) {
    if (!isLazy(data)) continue;
    const r = await gameCache?.match(data.url);
    if (!r) throw Object.assign(new Error(`the browser's cache no longer has the game file ${path}`), { key: 'the browser\'s cache no longer has the game file {path}', vars: { path } });
    files.set(path, new Uint8Array(await r.arrayBuffer()));
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
// The user folder was read from IndexedDB (preRun). Until then the folder in memory is empty or half read, and a write
// of it would remove from IndexedDB every file it lacks (IDBFS's reconcile): a hand-over while a tab was still starting
// did that (a review, 14 November 2026). After a read that failed it stays false: nothing of this visit is written
// there, so the stored files stay.
let folderRead = false;
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
  const folderDone = !folderRead ? null : new Promise((done) => FS.syncfs(false, (err) => {
    if (err && closedStorage(err) && forgetUserStorage()) {
      FS.syncfs(false, (again) => { if (again) log(`page: the user folder was not saved: ${again}`); done(); });
      return;
    }
    if (err) log(`page: the user folder was not saved: ${err}`);
    done();
  }));
  return Promise.all([vaultDone, folderDone]);
}

// Little room left in the browser for saves (saves.js, lowOnSpace): the Saved games button gets a mark, and the player
// is told once a visit, before a save fails for it. Checked at the start and after each save.
let toldLowSpace = false, lowSpace = false;
const lowSpaceTitle = () => { $('saves-open').title = lowSpace ? t('Little room left in this browser for saves') : ''; };
onLanguage(() => { if (lowSpace) lowSpaceTitle(); });
function checkSpace() {
  const FS = window.Module?.FS;
  if (!FS) return;
  spaceState(FS).then((sp) => {
    $('saves-open').classList.toggle('low', sp.low);
    lowSpace = sp.low;
    lowSpaceTitle();
    if (sp.low && !toldLowSpace) {
      toldLowSpace = true;
      log(`page: little room left for saves: ${sp.estimate.quota - sp.estimate.usage} bytes allowed, the largest save has ${sp.biggest}`);
      toast(t('Little room is left in this browser for saves. Saved games shows what they take: download the ones to keep, then delete some.'), { kind: 'bad', ms: 12000 });
    }
  }).catch(() => {});
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
    toast(t('Your saves are in this browser\'s storage again'));
  }, (e) => {
    log(`page: the vault: ${e?.name ?? 'Error'}: ${e?.message ?? e}`);
    if (vaultFailing) return;
    vaultFailing = true;
    toast(/Quota/i.test(e?.name ?? '')
      ? t('A save did not reach this browser\'s storage: the disk, or the room the browser gives this site, is full. It stays in this tab: keep the tab open and Download the save.')
      : t('A save did not reach this browser\'s storage. It stays in this tab: keep the tab open and Download the save.'), { kind: 'bad', ms: 15000 });
  });
}

// Writes the user folder to IndexedDB. A call while a write runs makes one more write after it, so nothing is missed.
let syncing = false, syncAgain = false, reopened = false;
function sync() {
  const FS = window.Module?.FS;
  if (!FS || handedOver || !folderRead) return;
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
      toast(t('The save did not reach this browser\'s storage'), { kind: 'bad', ms: 6000 });
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
    if (turnGone) return;                                // handed over while it started: it moves nothing
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
  if (!vault || turnGone) return;                      // handed over while it started: it writes nothing
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
        // Only over a folder that was read (folderRead): the vault empties once they are stored.
        if (folderRead) FS.syncfs(false, (err) => { if (!err) vault.clear(); });
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
// The engine's sleeps while the tab is hidden (ui.js, workerTimer): a hidden tab's timers are slowed to once a second, then
// once a minute, and the game's days stopped while the player was in another window.
const hiddenSleep = workerTimer();

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
    const slept = (document.hidden && hiddenSleep(ms)) || sleep(ms);
    slept.then((v) => { if (engine.wake === wake) engine.wake = null; resolve(v); }, reject);
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
      overlay(() => `<h1>${t('Restarting…')}</h1><p>${t('Your saves go to this browser\'s storage first.')}</p>`);
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
      reserveSaveRoom(FS, { when: () => Boolean(vault?.active) });   // a save being written gets its room at once (room.js)
      const lazyOnes = [...files.values()].filter(isLazy).length;
      if (lazyOnes && !hooked) {
        // The vault did not hook this engine: the game cannot read the cache in pieces, so every file comes in whole.
        log(`page: the engine's reads could not be hooked: the ${lazyOnes} large game files come into memory`);
        M.addRunDependency('game-files');
        bringIn(files).then(() => { install(FS, files); files.clear(); M.removeRunDependency('game-files'); },
          (e) => fail({ key: 'A game file did not load' }, { key: '{error}.', vars: { error: errorMsg(e) } }));
      } else {
        const { lazy, lazyBytes } = install(FS, files, (...a) => vault.gameFile(...a));
        files.clear();
        if (lazy) {
          useVault(vault);                               // the page's reads of /game (the editor) go through the vault
          log(`page: ${lazy} game files (${mb(lazyBytes)}) stay in the browser's cache until the game opens them`);
        }
      }
      FS.mkdirTree('/user');
      const idbfs = M.IDBFS || FS.filesystems.IDBFS;           // newer Emscripten has no FS.filesystems
      if (!skipWorkFiles(idbfs, (p) => Boolean(vault?.owns(p)))) log('page: this engine\'s IDBFS has no sets: the working files go to IndexedDB too');
      FS.mount(idbfs, {}, '/user');
      M.ENV.RECOMP_USER_DIR = '/user';                  // main.c: every write of the game goes there
      M.ENV.HOME = '/user';
      M.addRunDependency('user-folder');
      prepareVault(hooked).then(() => FS.syncfs(true, async (err) => {   // the saves of earlier visits, back from the browser
        if (err) log(`page: the user folder was not read (${err}): nothing of this visit is written to it, so the stored files stay`);
        else folderRead = true;
        if (turnGone) return;                            // handed over while it started: the start ends here
        await finishVault(FS);                           // the placeholders, before the cloud sync and the game list them
        // The display mode of the settings file, for an engine that does not tell the size of its picture.
        try { if (/^\s*display\s*=\s*classic\b/m.test(FS.readFile('/user/settings.txt', { encoding: 'utf8' }))) layout.setPicture(800, 600); }
        catch { /* no settings file yet: widescreen */ }
        // The saves made on other devices, before the game lists them.
        if ((await account.ready)?.user?.cloud) {
          const abort = new AbortController();
          let step = null, skipShown = false;            // the cloud's last step (its own words), and the skip button shown
          overlay(() => `<h1>${t('Getting your cloud saves')}</h1><div class="row"><i class="spin"></i><span id="cloud-step">${step === null ? t('Asking the cloud…') : esc(step)}</span></div>
            <p style="margin:14px 0 0"><button id="cloud-skip"${skipShown ? '' : ' hidden'}>${t('Start without them')}</button></p>`, '', { 'cloud-skip': () => abort.abort() });
          status(() => t('Getting your cloud saves…'));
          const skip = setTimeout(() => { skipShown = true; if ($('cloud-skip')) $('cloud-skip').hidden = false; }, 5000);
          const onStep = (text, job) => {
            step = text;
            if ($('cloud-step')) $('cloud-step').textContent = text;
            // The cloud save that comes now: a tab closed meanwhile leaves it in the cloud at the next start.
            if (job && job.do !== 'up') visit.step(`getting ${job.name} from the cloud`, { cloudFile: job.name });
            else visit.step('sending saves to the cloud');
          };
          onStep.signal = abort.signal;
          try { await startCloud(FS, { replace: true, onStep }); }
          catch (e) { log(`page: cloud saves: ${e?.stack ?? e}`); }       // the game starts all the same
          clearTimeout(skip);
          overlay(startingBox);
          status(() => t('Starting the engine…'));
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
      visit.step('in the game');
      canvas.dataset.engine = '1';
      $('fullscreen').hidden = !layout.canFullscreen || isFullAlready();
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
          if (now.length >= lastSaves.length) { toast(t('Saved in this browser')); if (!cloud) cloudHintOnce(); }
          checkSpace();
          lastSaves = now;
          sync();
          cloudAfterSave();
        }
        seen = now;
      }, 2000);
      const editor = mountEditor({ FS, sync, log, toast, dir: SAVE_DIR, isRunning: () => running,
        access: () => scoutAccess(account.me, keptMember()), inviteKind: () => inviteKind(account.me), invite: () => account.open(),
        onSetChange: (name, what) => (what === 'deleted' ? setSync?.deleted(name) : setSync?.run()) });
      $('editor-open').hidden = false;
      $('editor-open').onclick = () => editor.open();
      const sets = mountSetsPanel({ FS, root: SAVE_DIR, sync, log, toast, module: () => window.Module, cloud: () => setSync,
        edit: (name) => editor.openSet(name), create: () => editor.open(), changed: () => setSync?.run(), library: Boolean(info.library) });
      $('sets').hidden = false;
      $('saves-open').addEventListener('click', () => sets.render());
      setSync?.run();                                   // the cloud's data sets come in while the game starts
      $('help-tour-row').hidden = false;
      // The tour waits a moment for the bar's last buttons and the layout; not over a dialog or a box the page shows.
      setTimeout(() => { if (running && !handedOver && !document.querySelector('dialog[open]') && !$('overlay').innerHTML.trim()) tour.maybeStart(); }, 1500);
      // Play (play.js): a save's Play button; with a game in play, through Restart Game's path.
      const player = mountPlay({ module: () => window.Module, log, toast, restart: () => window.Module.rtRestart(), isRunning: () => running });
      mountSavesPanel({ FS, sync, log, report: () => report, cloud: () => cloud, promo: () => (cloud ? null : account.promo()),
        openPromo: () => { $('saves').close(); account.open(); }, edit: (name) => editor.openSave(name), play: player,
        t, tn, onLanguage });
      mountTacticsPanel({ FS, sync, log, cloud: () => tacticSync });
      // A new version on the server (release.js): a button in the bar loads it the way Restart Game does, the saves
      // stored first; with a game in play the page asks first. The page never loads it by itself.
      watchRelease({ current: info?.release, onNew: (release) => {
        log(`page: a new version is out (${release}; this page runs ${info.release})`);
        $('update').hidden = false;
        toast(t('A new version of the game is out. Press New version at the top when it suits you: your saves are stored first.'), { ms: 10000 });
      } });
      $('update').onclick = () => {
        if (!running) { leaving = true; location.reload(); return; }
        // An engine without rt_web_in_game (before Play) cannot say: it asks, as for a game in play.
        const inGame = typeof M._rt_web_in_game === 'function' ? M._rt_web_in_game() : true;
        if (inGame && !confirm(t('Load the new version now? The game in play closes: what you did since your last save is lost.'))) return;
        M.rtRestart();
      };
      checkSpace();
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
      // The game's long jobs (a new game's stages) in the bar's status, with their share and time (progress.js).
      if (typeof M._rt_web_progress === 'function') {
        progress = watchProgress({ read: () => M._rt_web_progress(), set: status });
        const ticks = setInterval(() => {
          if (!running) { clearInterval(ticks); return; }  // stopped, failed or handed over: its message stays in the bar
          try { progress.tick(); } catch (e) { clearInterval(ticks); log(`page: the engine's progress stopped: ${e}`); }
        }, 250);
      }
      keepAwake();
      const onHide = () => {
        if (handedOver) return;
        if (vault?.active) vaultRound({ force: true });
        sync();
        if (cloudBehind && running) cloudAfterSave(true, true);         // a large save held back goes now
      };
      addEventListener('pagehide', onHide);
      document.addEventListener('visibilitychange', () => { if (document.hidden) onHide(); });
      primeOffline(info).catch((e) => log(`page: not kept for a start without a connection: ${e}`));
      log(`page: main(${M.arguments.join(' ')})`);
      try {
        const quiet = (e) => e?.name === 'ExitStatus' || e?.name === 'EngineLetGo';   // an exit, or the page's own stop
        const r = M.callMain(M.arguments);
        if (r && typeof r.then === 'function') {
          engine.ended = r.then(() => {}, () => {});
          r.catch((e) => {
            if (quiet(e)) return;
            running = false;
            if (e === undefined || e === null) log(`page: the engine's run ended with ${e} and no error (no stack: the browser gave none)`);
            logStack(e);
            fail({ key: 'The game stopped' }, e === undefined || e === null
              ? { key: 'the engine ended without an error message. Send to the developer: the log says what it did last.' } : String(e));
          });
        }
        player.resume();                                 // a Play that restarted the game: the save it asked for
      } catch (e) {
        if (e?.name !== 'ExitStatus' && e?.name !== 'EngineLetGo') { running = false; logStack(e); fail({ key: 'The game stopped' }, String(e)); }
      }
    },
    onExit: (code) => {
      running = false;
      visit.end();
      // Exit right after a save: the save is stored before the box says the game ended (closing the tab meanwhile
      // asks first), where an ordinary write would leave a file of the last moment for later.
      const stored = storeEverything();
      if (code === 0) {
        status(() => t('Saving…'));
        overlay(() => `<h1>${t('The game ended')}</h1><p>${t('Your saves go to this browser\'s storage…')}</p>`);
        stored.finally(() => {
          status(() => t('The game ended'));
          overlay(() => `<h1>${t('The game ended')}</h1><p>${t('Reload the page to play again. Your saves stay in this browser.')}</p>`);
        });
      } else {
        const box = fatalBox();
        fail({ key: 'The game ended with code {code}', vars: { code } }, box
          ? { key: 'The game stopped on an error of its own: "{text}" (at {site}). Send to the developer, with the save you loaded, so it can be looked at. Your saves stay in this browser.',
            vars: { text: box.text, site: box.site } }
          : { key: 'Help, Engine log says why. Your saves stay in this browser.' });
      }
    },
    // The engine stopped itself (Emscripten's abort: a C abort(), an assert, a runtime check). A bare abort() comes
    // with no message and no stack (a player's Firefox reported only "The game stopped: undefined"): the stack of this
    // moment is logged instead, which names the game's and the runtime's functions that called it.
    onAbort: (what) => {
      if (engine.letGo) return;
      running = false;
      const told = what !== undefined && what !== null && String(what) !== '';
      log(`page: the engine stopped itself (abort)${told ? `: ${String(what)}` : ' without a message'}`);
      logStack(what?.stack ? what : new Error('abort'));
      fail({ key: 'The game stopped' }, told ? String(what) : { key: 'the engine stopped itself (abort) without a message. Send to the developer: the log says where.' });
    },
  };
  const s = document.createElement('script');
  s.src = engineUrl(info.chosen, 'cm0102_recomp.js');
  s.onerror = () => fail({ key: 'The engine did not load' }, { key: 'The server did not give {file}.', vars: { file: `${info.chosen.base}cm0102_recomp.js` } });
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

// Each problem and warning is a text as english() and shown() take them: the log and the statistics get the English.
function requirements(info) {
  const problems = [], warnings = [];
  if (typeof WebAssembly !== 'object' || typeof WebAssembly.instantiate !== 'function') {
    problems.push({ key: 'This browser has no WebAssembly, which the engine is made of.' });
    return { problems, warnings };
  }
  if (info.chosen.suspend !== 'asyncify' && !hasJspi())
    problems.push(appleMobile()
      ? { key: 'The engine needs JSPI, a WebAssembly feature that this {device} does not have yet. {system} 27 has it, in every browser (Safari and Chrome alike): update in Settings, General, Software Update, then open the game again.',
        vars: { device: appleMobile(), system: appleMobile() === 'iPad' ? 'iPadOS' : 'iOS' } }
      : { key: 'The engine needs JSPI, a WebAssembly feature that this browser lacks, and this server has no engine for other browsers. Chrome 137 or later has it, and Safari 27 (iOS and iPadOS 27, in every browser of an iPhone or iPad).' });
  try {                                              // the engine's own memory plan, on a memory that is then dropped
    const m = new WebAssembly.Memory({ initial: ENGINE_START_PAGES, maximum: ENGINE_MAX_PAGES });
    m.grow(ENGINE_NEED_PAGES - ENGINE_START_PAGES);
  } catch (e) {
    problems.push({ key: 'This browser does not give a page the {size} MB of memory that the engine needs ({error}).',
      vars: { size: Math.round(ENGINE_NEED_PAGES * PAGE / 1048576), error: e.message } });
  }
  if (typeof indexedDB === 'undefined') warnings.push({ key: 'This browser keeps no data for the page (a private window?): saves are lost when the tab closes.' });
  return { problems, warnings };
}

function blocked(problems, note = { key: 'Nothing was downloaded.' }) {
  status(() => t('This browser cannot run the game'));
  const device = appleMobile(), system = device === 'iPad' ? 'iPadOS' : 'iOS';
  overlay(() => `<h1>${t('This browser cannot run the game')}</h1><ul>${problems.map((p) => `<li>${esc(shown(p))}</li>`).join('')}</ul>
    <p>${device ? t('It runs on an {device} with {system} 27 or later, and on a computer in Chrome 137 or later or Safari 27 or later.', { device, system })
      : t('It runs in Chrome 137 or later, and in Safari 27 or later: on an iPhone or iPad with iOS or iPadOS 27, in Safari or Chrome.')} ${esc(shown(note))}</p>`, 'error');
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
    return ` · ${mb(rate)}/s · ${left < 90 ? t('about {n} s left', { n: Math.max(1, Math.round(left)) }) : t('about {n} min left', { n: Math.round(left / 60) })}`;
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
  visit.end();
  turnGone = true;
  log(`page: the game moves to another tab${store ? ': it stops here, and the saves go to the browser\'s storage first' : ''}`);
  running = false;
  if (!store) handedOver = true;                       // the other tab plays already: not one more write from here
  startAbort?.abort();
  status(() => t('The game moved to another tab'));
  overlay(() => `<h1>${t('The game moves to another tab')}</h1><div class="row"><i class="spin"></i>${store ? t('Your saves go to this browser\'s storage first.') : t('The game here stops.')}</div>`);
  await within(letEngineGo(), 10000);
  // An engine whose sleep cannot be ended (Asyncify) plays on behind the box: at least its sound stops.
  try { window.Module?.SDL2?.audioContext?.suspend?.(); } catch { /* no sound */ }
  // The lock stays until the store is done, however long it takes: the asking tab must not read the storage while this
  // one still writes it (a review found the 60 s cap that was here). A store that never ends: the asking tab offers
  // Play here anyway after its own wait.
  if (store) await storeEverything().catch((e) => log(`page: the store before the hand-over: ${e?.message ?? e}`));
  handedOver = true;
  turnRelease?.();
  turnRelease = null;
  for (const id of ['saves-open', 'editor-open', 'kbd-open', 'right-click']) if ($(id)) $(id).hidden = true;   // they would change nothing stored
  for (const d of document.querySelectorAll('dialog[open]')) d.close();
  overlay(() => `<h1>${t('The game moved to another tab')}</h1><p>${store ? t('It runs in the other tab now, with your saves.')
    : t('Another tab took it without waiting for this one: what this tab had not put in the browser\'s storage yet is not there.')}
    ${t('You can close this one.')}</p>
    <div class="row"><button class="primary" id="turn-back">${t('Play here')}</button></div>`, '', { 'turn-back': () => { leaving = true; location.reload(); } });
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
  if (turnGone) return;                                // this tab handed over meanwhile: its box says so
  if (on) {
    log('page: an older page of the game, in another tab, keeps the saves\' storage open: waiting until it is closed or loaded again');
    status(() => t('The game is open in another tab'));
    overlay(() => `<h1>${t('The game is open in another tab')}</h1><p>${t('An older page of the game runs in another tab or window of this browser.')} ${
      t('Close that tab or load it again: the game then goes on here by itself.')} ${t('Two at once would delete each other\'s saves.')}</p><div class="row"><i class="spin"></i>${t('Waiting for the other tab…')}</div>`);
  } else {
    log('page: the other tab let the saves\' storage go');
    status(running ? '' : () => t('Starting the engine…'));
    overlay(running ? '' : startingBox);
  }
}

// Before the game files load: true when this tab may run the game. A Restart's reload can come before the page it
// replaces is gone, so the first try waits a moment.
async function myTurn() {
  try {
    if (await takeTurn({ signal: timeoutSignal(1500) })) return true;
  } catch (e) { log(`page: one tab at a time did not work here (${e?.message ?? e}): the game starts`); return true; }
  log('page: the game runs in another tab of this browser');
  await new Promise((r) => {
    status(() => t('The game is open in another tab'));
    overlay(() => `<h1>${t('The game is open in another tab')}</h1>
    <p>${t('CM 01/02 runs in another tab or window of this browser.')} ${t('One may play at a time: two would delete each other\'s saves from the browser\'s storage.')}</p>
    <p class="small">${t('Play here stops the game in the other tab, after it puts its saves in the browser\'s storage.')} ${
      t('What was not saved in the game is lost there, as when that tab closes.')}</p>
    <div class="row"><button class="primary" id="turn-take">${t('Play here')}</button></div>`, '', { 'turn-take': () => r() });
  });
  for (;;) {
    status(() => t('Stopping the game in the other tab…'));
    overlay(() => `<h1>${t('Stopping the game in the other tab')}</h1><div class="row"><i class="spin"></i>${t('Its saves go to the browser\'s storage first.')}</div>`);
    turnChannel?.postMessage({ type: 'take' });
    let got = false;
    try { got = await takeTurn({ signal: timeoutSignal(TURN_WAIT) }); } catch (e) { log(`page: ${e?.message ?? e}`); }
    if (got) { log('page: the other tab stopped; the game runs here'); return true; }
    log(`page: the other tab did not stop in ${TURN_WAIT / 1000} s`);
    const choice = await new Promise((r) => {
      status(() => t('The other tab did not stop'));
      overlay(() => `<h1>${t('The other tab did not stop')}</h1><p>${t('It does not answer: the browser may have paused it.')} ${
        t('Close the other tab or window of this site, then press Try again.')} ${
        t('Play here anyway starts the game here at once; the other tab then stops when it wakes, without storing what it had not stored yet.')}</p>
      <div class="row"><button class="primary" id="turn-again">${t('Try again')}</button><button id="turn-steal">${t('Play here anyway')}</button></div>`, 'error',
      { 'turn-again': () => r('again'), 'turn-steal': () => r('steal') });
    });
    if (choice === 'steal') {
      log('page: the game runs here; the other tab stops when it wakes');
      if (await takeTurn({ steal: true })) return true;
    }
  }
}

async function start() {
  overlay(() => `<div class="row"><i class="spin"></i>${t('Checking the files…')}</div>`);
  status(() => t('Checking the files…'));
  let info;
  try { info = await (await fetch('/files.json')).json(); }
  catch (e) { fail({ key: 'The server does not answer' }, String(e)); return; }
  info.chosen = pickEngine(info);
  if (!info.chosen) { fail({ key: 'The engine is not built' }, { key: 'Build it first: web/README.md, step 4.' }); return; }
  log(`page: engine ${info.chosen.base} (${info.chosen.suspend ?? 'unknown'} build, version ${info.chosen.version})${hasJspi() ? '' : '; this browser has no JSPI'}`);
  if (info.missing.length) {
    fail({ key: '{n} game files are missing or of another size', vars: { n: info.missing.length } },
      { key: 'First: {files}. CM_GAME_DIR must hold the clean files (web/README.md, step 3).', vars: { files: info.missing.slice(0, 6).map((m) => m.path).join(', ') } });
    return;
  }
  stats = startStats({ info, log, isRunning: () => running, state: folderState,
    extra: () => ({ engine: `${info.chosen.suspend ?? 'unknown'} ${info.chosen.version}`, vault: Boolean(vault?.active), cloud: Boolean(account.me?.user?.cloud) }) });
  mountStatsChoice(info);
  // The page's footer, with the video guides (CM_YOUTUBE_URL), again in a language chosen later.
  const footer = () => {
    $('made').innerHTML = madeWith(info.maker);
    if (info.youtube) $('made').insertAdjacentHTML('beforeend', ` · <a href="${esc(info.youtube)}" target="_blank" rel="noopener">${t('Video guides')}</a>`);
  };
  footer();
  onLanguage(footer);
  // The video guides: in the footer and in Help.
  if (info.youtube) {
    for (const a of document.querySelectorAll('a.youtube')) a.href = info.youtube;
    $('help-videos').hidden = false;
  }
  // Buy me a coffee (CM_COFFEE_URL on the server): in the bar and in Help.
  if (info.coffee) {
    for (const a of document.querySelectorAll('a.coffee')) a.href = info.coffee;
    $('coffee-open').hidden = false;
    $('help-coffee').hidden = false;
  }
  if (info.stats) {
    report = mountReport({ FS: () => window.Module?.FS ?? null, lines: () => lines, who: () => stats?.who?.() ?? {}, inPlay: () => saveInPlay(), log });
    $('help-report').hidden = false;
    $('help-report-open').onclick = () => { $('help').close(); report.open({ where: 'Help' }); };
    $('help-machine-report').hidden = false;
    $('help-machine-report').onclick = () => { $('help').close(); report.open({ where: 'Help, a better word' }); };
  }
  const { problems, warnings } = requirements(info);
  sweepDisk().then((n) => { if (n) log(`page: ${n} unfinished file${n > 1 ? 's' : ''} of an earlier visit removed from the browser's disk`); });
  // How this browser's storage keeps a file's Blob (disk.js): the way cloud saves and uploads are stored follows it.
  storageCopiesFiles().then((c) => log(c ? 'page: the browser\'s storage copies files: cloud saves are unpacked on disk, not in memory'
    : 'page: the browser\'s storage keeps references to files: a save is copied by the browser before it is stored'));
  // The last visit stopped without the page closing: the system closed the tab, most likely for its memory (iOS: "A
  // problem repeatedly occurred"). The player hears it before the next start, the log and the statistics have it.
  // A mark of a tab that plays now is no sudden stop: it is read only when no tab holds the game (a closed tab's lock goes
  // with it).
  const otherPlays = await Promise.resolve(navigator.locks?.query?.()).then((q) => Boolean(q?.held?.some((l) => l.name === TURN)), () => false);
  const lastStop = otherPlays ? null : visit.takeLastStop();
  if (lastStop) {
    log(`page: the last visit stopped without the page closing, while ${lastStop.step} (${lastStop.device ?? 'a computer'}, ${lastStop.engine ?? '?'} engine)`);
    stats.report(`sudden stop: the last visit stopped while ${lastStop.step}`, [], 'sudden stop');
    const device = appleMobile(), name = lastStop.cloudFile;
    // A cloud save that came when the tab closed: it stays in the cloud on this device, or each start would close the
    // tab again on it (an iPhone, 18 November 2026: a 446 MB save, "A problem repeatedly occurred").
    if (name) {
      keepInCloud(name);
      log(`page: cloud saves: ${name} stays in the cloud on this device; Account has it ("Get it here")`);
    }
    warnings.unshift(() => (name
      ? `${suddenStopText(lastStop, { device })} ${t('{name} stays in the cloud on this device for now, so the game can start; Account has it, with "Get it here".', { name })}`
      : suddenStopText(lastStop, { device })));
  }
  if (problems.length) {
    const said = problems.map(english).join(' ');
    log(`page: blocked: ${said}`); stats.report(`blocked: ${said}`, [], 'requirements'); blocked(problems); return;
  }
  if ('serviceWorker' in navigator && window.isSecureContext) navigator.serviceWorker.register('/sw.js').catch((e) => log(`page: no service worker: ${e}`));
  const cache = await openCache();
  const [have, kept] = await Promise.all([cachedBytes(cache, info.files), keepStorage()]);
  log(`page: browser storage ${kept}; ${mb(have)} of the game files already in this browser`);
  setStorageKept(kept);
  // In a language other than English, Help says the words are a machine's, and asks for better ones.
  $('help-machine').hidden = language() === 'en';
  onLanguage(() => { $('help-machine').hidden = language() === 'en'; });
  $('help-storage').textContent = ` ${storageWarning(false, t)}`;
  onLanguage(() => { $('help-storage').textContent = ` ${storageWarning(false, t)}`; });
  const toGet = info.totalBytes - have;
  if (toGet > 0 || warnings.length) {
    // The first visit, or part of the files: one click before a large download.
    status(() => t('Ready'));
    const me = await Promise.race([account.ready, new Promise((r) => setTimeout(r, 1500))]);
    await new Promise((r) => {
      overlay(() => `<h1>${t('CM 01/02, in your browser')}</h1>
      <p>${toGet > 0 ? (have ? t('{size} of game files download into this browser once ({here} are here already).', { size: mb(toGet), here: mb(have) })
        : t('{size} of game files download into this browser once.', { size: mb(toGet) }))
        : t('The game files are in this browser already.')} ${t('The game then runs in this tab, on your device.')} ${t('Nothing is installed.')}</p>
      <p class="small">${t('Your saved games stay in this browser too.')} ${
        t('Saved games downloads them, one by one or all at once, so you keep copies: a browser may delete a site\'s data by itself.')}</p>
      ${warnings.length ? `<ul>${warnings.map((w) => `<li>${esc(shown(w))}</li>`).join('')}</ul>` : ''}
      ${TOUCH ? `<p class="small">${t('On a touch screen: tap to click, long press for the right button (keep the finger down to drag with it), two fingers to scroll.')} ${t('Help has the list.')}</p>` : ''}
      <div class="row"><button class="primary" id="go">${t('Start the game')}</button><span class="free-note">${t('Free · no account, no payment')}</span></div>
      ${plansForStart(me)}`, '', { go: () => r(), 'go-plans': (e) => { e.preventDefault(); account.open(); } });
    });
  }
  if (!await myTurn()) return;
  visit.begin({ step: toGet > 0 ? 'downloading the game files' : 'reading the game files', device: appleMobile() ?? undefined, engine: info.chosen.suspend ?? undefined });
  overlay(() => `<h1>${toGet > 0 ? t('Loading the game files') : t('Starting')}</h1><div class="bar"><i id="bar-fill"></i></div><div class="small" id="bytes"></div>`);
  status(() => t('Loading the game files…'));
  const t0 = performance.now();
  const counts = { cached: 0, fetched: 0 };
  const abort = new AbortController();
  startAbort = abort;                                  // another tab takes the game meanwhile: the download stops
  const speed = meter(info.totalBytes);
  let got = 0, compileError = null, files;
  // The engine is checked while the files load; if this browser cannot compile it, the download stops.
  const compiled = engineCompiles(info).then((err) => { if (err) { compileError = err; abort.abort(); } });
  // Large game files stay in the cache (LAZY_MIN) when the game can read them from there: a JSPI engine, the vault
  // wanted, and a cache. runEngine falls back to memory if its hook does not take.
  const lazyGame = Boolean(cache) && vaultWanted() && info.chosen.suspend === 'jspi' && typeof WebAssembly.Suspending === 'function';
  gameCache = cache;
  try {
    files = await download(info.files, cache, (n) => {
      got += n;
      if (!$('bar-fill')) return;                       // the box shows something else now (a file failed)
      $('bar-fill').style.width = `${(100 * got / info.totalBytes).toFixed(1)}%`;
      $('bytes').textContent = `${t('{got} of {total}', { got: mb(got), total: mb(info.totalBytes) })}${toGet > 0 ? speed(got) : ''}`;
    }, counts, abort.signal, { lazy: lazyGame });
    await compiled;
  } catch (e) {
    await compiled;
    if (turnGone) return;
    if (!compileError) {
      // Not a dropped connection when the browser had no memory for the file: fail() says what to do then.
      fail({ key: 'A game file did not load' }, OUT_OF_MEMORY.test(String(e?.message ?? e)) ? { key: '{error}.', vars: { error: errorMsg(e) } }
        : { key: '{error}. The connection may have dropped. The game files that arrived stay in this browser: Start again goes on with the rest.', vars: { error: errorMsg(e) } });
      return;
    }
  }
  if (compileError) {
    log(`page: the engine does not compile here: ${compileError}`);
    blocked([{ key: 'This browser cannot compile the engine: {error}', vars: { error: compileError } }], { key: 'The download of the game files was stopped.' });
    return;
  }
  log(`page: ${info.files.length} files in ${((performance.now() - t0) / 1000).toFixed(1)} s: ${mb(counts.cached)} from this browser, ${mb(counts.fetched)} downloaded`);
  pruneCache(cache, info.files).catch((e) => log(`page: old game files not removed: ${e}`));
  if (turnGone) return;
  overlay(startingBox);
  status(() => t('Starting the engine…'));
  visit.step('starting the engine');
  runEngine(files, info);
}

// A start that stops on an error before the game: the page and its scripts may be of two versions (a page from the
// service worker's copy, a script from a newer server), so it loads again once, fresh; a second time it says so,
// instead of "Checking the files…" for good.
const START_RELOAD = 'cm0102-start-reload';
start().catch((err) => {
  log(`page: the start stopped: ${errorText(err)}`);
  if (running) return;
  let last = 0;
  try { last = Number(sessionStorage.getItem(START_RELOAD)) || 0; } catch { /* no storage: no reload */ last = Date.now(); }
  if (Date.now() - last > 5 * 60e3) {
    try { sessionStorage.setItem(START_RELOAD, String(Date.now())); } catch { /* ignore */ }
    log('page: loading the page again, once');
    location.reload();
    return;
  }
  fail({ key: 'The page did not start' }, { key: '{error}. Load the page again; if it stays, send the Engine log from Help.', vars: { error: errorMsg(err) } });
});
