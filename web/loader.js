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

// Every listed file, read as a stream so the bar moves inside the large ones (language.ldb is 195 MB).
async function download(files, onBytes) {
  const out = new Map();
  let next = 0;
  async function one() {
    while (next < files.length) {
      const f = files[next++];
      const r = await fetch(`/game/${f.path.split('/').map(encodeURIComponent).join('/')}`);
      if (!r.ok) throw new Error(`${f.path}: HTTP ${r.status}`);
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
  if (info.suspend !== 'asyncify' && typeof WebAssembly.Suspending !== 'function') {
    fail('This browser cannot run this build', 'The engine is built with JSPI: use Chrome 137 or later, or build it with -DCM0102_WEB_SUSPEND=asyncify.');
    return;
  }
  status('Ready');
  overlay(`<h1>CM 01/02, running in your browser</h1>
    <p>${info.files.length} game files (${mb(info.totalBytes)}) and the engine (${mb(info.engineBytes)}) load into this tab,
    and the game runs here. Nothing is installed.</p><button class="primary" id="go">Start the game</button>`);
  await new Promise((r) => { $('go').onclick = r; });     // one click first: the browser allows sound only after one
  overlay(`<h1>Loading the game files</h1><div class="bar"><i id="bar"></i></div><div class="small" id="bytes"></div>`);
  status('Loading the game files…');
  const t0 = performance.now();
  let got = 0;
  let files;
  try {
    files = await download(info.files, (n) => {
      got += n;
      $('bar').style.width = `${(100 * got / info.totalBytes).toFixed(1)}%`;
      $('bytes').textContent = `${mb(got)} of ${mb(info.totalBytes)}`;
    });
  } catch (e) {
    fail('A game file did not load', String(e.message ?? e));
    return;
  }
  log(`page: ${info.files.length} files, ${mb(info.totalBytes)}, in ${((performance.now() - t0) / 1000).toFixed(1)} s`);
  overlay(`<h1>Starting the engine</h1><p>The browser compiles the engine (${mb(info.engineBytes)}). The first start can take a minute.</p>`);
  status('Starting the engine…');
  runEngine(files, info);
}

start();
