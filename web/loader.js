// The page side of the browser build (web/README.md): it downloads the game files, puts them into the engine's file
// system at /game, keeps the user folder /user (saves, settings.txt) in the browser, and starts the recompiled game
// on the canvas. The game runs in this tab; the server only hands out files.
const $ = (id) => document.getElementById(id);
const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const mb = (n) => `${(n / 1048576).toFixed(n < 10485760 ? 1 : 0)} MB`;
const CONNECTIONS = 6;

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

function fail(title, detail) {
  status(title);
  const last = lines.slice(-8).join('\n');
  overlay(`<h1>${esc(title)}</h1><p>${esc(detail)}</p>${last ? `<pre class="small" style="white-space:pre-wrap;margin:0">${esc(last)}</pre>` : ''}`, 'error');
}

addEventListener('error', (e) => log(`page: ${e.message}`));
addEventListener('unhandledrejection', (e) => log(`page: ${e.reason?.stack ?? e.reason}`));

// The game files stay in the browser's cache storage under their versioned address, so a later visit reads them
// from this computer. Only pages on https or on this computer (127.0.0.1) have cache storage; elsewhere every visit
// downloads them.
const CACHE = 'cm0102-game-files';
const fileUrl = (f) => `/game/${f.path.split('/').map(encodeURIComponent).join('/')}?v=${encodeURIComponent(f.v ?? '')}`;

async function openCache() {
  try { return self.caches ? await caches.open(CACHE) : null; } catch { return null; }
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
async function download(files, cache, onBytes, counts) {
  const out = new Map();
  let next = 0;
  async function one() {
    while (next < files.length) {
      const f = files[next++];
      const url = fileUrl(f);
      let r = cache ? await cache.match(url) : undefined, storing = null;
      const fromCache = Boolean(r);
      if (!r) {
        r = await fetch(url);
        if (!r.ok) throw new Error(`${f.path}: HTTP ${r.status}`);
        if (cache) storing = cache.put(url, r.clone()).catch((e) => log(`page: ${f.path} was not kept in the browser: ${e}`));
      }
      const buf = new Uint8Array(f.size), reader = r.body.getReader();
      let at = 0;
      for (;;) {
        const { done, value } = await reader.read();
        if (done) break;
        if (at + value.length > f.size) throw new Error(`${f.path}: more than ${f.size} bytes`);
        buf.set(value, at);
        at += value.length;
        onBytes(value.length);
      }
      if (at !== f.size) throw new Error(`${f.path}: ${at} of ${f.size} bytes`);
      if (storing) await storing;
      counts[fromCache ? 'cached' : 'fetched'] += f.size;
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

function resumeAudio() {
  const ctx = window.Module?.SDL2?.audioContext;
  if (ctx && ctx.state === 'suspended') ctx.resume();
}

function runEngine(files, info) {
  const canvas = $('canvas');
  canvas.addEventListener('contextmenu', (e) => e.preventDefault());      // the game uses the right button
  canvas.addEventListener('mousedown', () => { canvas.focus(); resumeAudio(); });
  $('fullscreen').onclick = () => canvas.requestFullscreen?.();
  let syncing = false;
  const sync = () => {
    const FS = window.Module?.FS;
    if (syncing || !FS) return;
    syncing = true;
    FS.syncfs(false, (err) => { syncing = false; if (err) log(`page: the user folder was not saved: ${err}`); });
  };
  window.Module = {
    canvas,
    arguments: ['--game', '/game'],
    print: log,
    printErr: log,
    locateFile: (name) => `/engine/${name}?v=${info.engineVersion}`,
    preRun: [() => {
      const M = window.Module, FS = M.FS;
      install(FS, files);
      files.clear();
      FS.mkdirTree('/user');
      FS.mount(M.IDBFS || FS.filesystems.IDBFS, {}, '/user');   // newer Emscripten has no FS.filesystems
      M.ENV.RECOMP_USER_DIR = '/user';                  // main.c: every write of the game goes there
      M.ENV.HOME = '/user';
      M.addRunDependency('user-folder');
      FS.syncfs(true, (err) => {                         // the saves of earlier visits, back from the browser
        if (err) log(`page: the user folder was not read: ${err}`);
        M.removeRunDependency('user-folder');
      });
    }],
    onRuntimeInitialized: () => {
      overlay('');
      status('Running in this tab');
      $('fullscreen').hidden = false;
      canvas.focus();
      setInterval(sync, 15000);
      addEventListener('pagehide', sync);
      document.addEventListener('visibilitychange', () => { if (document.hidden) sync(); });
      log(`page: main(${window.Module.arguments.join(' ')})`);
      try {
        const r = window.Module.callMain(window.Module.arguments);
        if (r && typeof r.then === 'function') r.catch((e) => { if (e?.name !== 'ExitStatus') fail('The game stopped', String(e)); });
      } catch (e) {
        if (e?.name !== 'ExitStatus') fail('The game stopped', String(e));
      }
    },
    onExit: (code) => {
      sync();
      if (code === 0) { status('The game ended'); overlay('<h1>The game ended</h1><p>Reload the page to play again. Saves stay in this browser.</p>'); }
      else fail(`The game ended with code ${code}`, 'The engine log below says why. Saves stay in this browser.');
    },
    onAbort: (what) => fail('The game stopped', String(what)),
  };
  const s = document.createElement('script');
  s.src = `/engine/cm0102_recomp.js?v=${info.engineVersion}`;
  s.onerror = () => fail('The engine did not load', 'The server did not give /engine/cm0102_recomp.js.');
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
  if (info.suspend !== 'asyncify' && (typeof WebAssembly.Suspending !== 'function' || typeof WebAssembly.promising !== 'function'))
    problems.push('The engine needs JSPI, a WebAssembly feature that this browser lacks. Chrome 137 or later on a computer has it.');
  try {                                              // the engine's own memory plan, on a memory that is then dropped
    const m = new WebAssembly.Memory({ initial: ENGINE_START_PAGES, maximum: ENGINE_MAX_PAGES });
    m.grow(ENGINE_NEED_PAGES - ENGINE_START_PAGES);
  } catch (e) {
    problems.push(`This browser does not give a page the ${Math.round(ENGINE_NEED_PAGES * PAGE / 1048576)} MB of memory that the engine needs (${e.message}).`);
  }
  if (typeof indexedDB === 'undefined') warnings.push('This browser keeps no data for the page (a private window?): saves are lost when the tab closes.');
  if (matchMedia('(pointer: coarse)').matches && !matchMedia('(any-pointer: fine)').matches)
    warnings.push('This device has no mouse. The game is made for a mouse and a large screen.');
  return { problems, warnings };
}

function blocked(problems, note = 'Nothing was downloaded.') {
  status('This browser cannot run the game');
  overlay(`<h1>This browser cannot run the game</h1><ul>${problems.map((p) => `<li>${esc(p)}</li>`).join('')}</ul>
    <p>It runs in Chrome 137 or later on a Mac, Windows or Linux computer. ${esc(note)}</p>`, 'error');
}

// The engine file itself, compiled once before the game files download: a browser that lacks a WebAssembly feature the
// engine uses fails here, after the engine's download and before the 460 MB of game files.
async function engineCompiles(info) {
  try {
    await WebAssembly.compileStreaming(fetch(`/engine/cm0102_recomp.wasm?v=${info.engineVersion}`));
    return null;
  } catch (e) {
    return e.message ?? String(e);
  }
}

async function start() {
  overlay('');
  status('Checking the files…');
  let info;
  try { info = await (await fetch('/files.json')).json(); }
  catch (e) { fail('The server does not answer', String(e)); return; }
  if (!info.engine) { fail('The engine is not built', 'Build it first: web/README.md, step 3.'); return; }
  if (info.missing.length) {
    fail(`${info.missing.length} game files are missing or of another size`,
      `First: ${info.missing.slice(0, 6).map((m) => m.path).join(', ')}. CM_GAME_DIR must hold the clean files (web/README.md, step 1).`);
    return;
  }
  const { problems, warnings } = requirements(info);
  if (problems.length) { log(`page: blocked: ${problems.join(' ')}`); blocked(problems); return; }
  const cache = await openCache();
  const [have, kept] = await Promise.all([cachedBytes(cache, info.files), keepStorage()]);
  log(`page: browser storage ${kept}; ${mb(have)} of the game files already in this browser`);
  const toGet = info.totalBytes - have;
  status('Ready');
  overlay(`<h1>CM 01/02, running in your browser</h1>
    <p>${toGet > 0 ? `${mb(toGet)} of game files download into this browser${have ? ` (${mb(have)} are here already)` : ''},`
      : 'The game files are in this browser already,'} and the game runs in this tab. Nothing is installed.</p>
    ${warnings.length ? `<ul>${warnings.map((w) => `<li>${esc(w)}</li>`).join('')}</ul>` : ''}
    <button class="primary" id="go">Start the game</button>`);
  await new Promise((r) => { $('go').onclick = r; });     // one click first: the browser allows sound only after one
  overlay(`<h1>Checking the engine</h1><p>The browser compiles the engine once (${mb(info.engineBytes)}) to check that it can run it.</p>`);
  status('Checking the engine…');
  const compileError = await engineCompiles(info);
  if (compileError) {
    log(`page: the engine does not compile here: ${compileError}`);
    blocked([`This browser cannot compile the engine: ${compileError}`], 'The game files were not downloaded.');
    return;
  }
  overlay(`<h1>Loading the game files</h1><div class="bar"><i id="bar"></i></div><div class="small" id="bytes"></div>`);
  status('Loading the game files…');
  const t0 = performance.now();
  const counts = { cached: 0, fetched: 0 };
  let got = 0;
  let files;
  try {
    files = await download(info.files, cache, (n) => {
      got += n;
      $('bar').style.width = `${(100 * got / info.totalBytes).toFixed(1)}%`;
      $('bytes').textContent = `${mb(got)} of ${mb(info.totalBytes)}`;
    }, counts);
  } catch (e) {
    fail('A game file did not load', String(e.message ?? e));
    return;
  }
  log(`page: ${info.files.length} files in ${((performance.now() - t0) / 1000).toFixed(1)} s: ${mb(counts.cached)} from this browser, ${mb(counts.fetched)} downloaded`);
  pruneCache(cache, info.files).catch((e) => log(`page: old game files not removed: ${e}`));
  overlay(`<h1>Starting the engine</h1><p>The browser compiles the engine (${mb(info.engineBytes)}). The first start can take a minute.</p>`);
  status('Starting the engine…');
  runEngine(files, info);
}

start();
