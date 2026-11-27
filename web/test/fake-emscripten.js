// A stand-in for Emscripten's cm0102_recomp.js in the page check (page-check.cjs): the part of the Module contract
// that web/loader.js and web/saves.js use (locateFile, preRun, FS, IDBFS, ENV, run dependencies,
// onRuntimeInitialized, callMain), with no game behind it. It records what the loader gave it in window.__fake, draws
// one line on the canvas, and writes one saved game into the user folder as a game would.
(() => {
  const M = window.Module;
  const files = new Map(), dirs = new Set(['/']), mounts = [];
  const FILE = 0o100644, DIR = 0o040755;
  const mkdirTree = (p) => {
    let cur = '';
    for (const part of p.split('/').filter(Boolean)) { cur += `/${part}`; dirs.add(cur); }
  };
  const enoent = (p) => Object.assign(new Error(`ENOENT ${p}`), { errno: 44 });
  window.__syncs = { load: 0, store: 0, opened: 0 };
  M.FS = {
    mkdirTree,
    mkdir(p) { if (dirs.has(p)) throw Object.assign(new Error(`EEXIST ${p}`), { errno: 20 }); dirs.add(p); },
    unlink(p) { if (!files.delete(p)) throw enoent(p); },
    rename(from, to) {                                   // as Emscripten's: the file takes the new name, over one there
      const f = files.get(from);
      if (!f) throw enoent(from);
      if (!dirs.has(to.slice(0, to.lastIndexOf('/')) || '/')) throw enoent(to);
      files.delete(from);
      files.set(to, f);
    },
    rmdir(p) { if ([...files.keys()].some((f) => f.startsWith(`${p}/`))) throw new Error(`ENOTEMPTY ${p}`); if (!dirs.delete(p)) throw enoent(p); },
    writeFile(path, data, opts) {
      if (!dirs.has(path.slice(0, path.lastIndexOf('/')) || '/')) throw new Error(`no folder for ${path}`);
      files.set(path, { data, size: data.length, own: Boolean(opts?.canOwn), mtime: new Date() });
    },
    readFile(path) {
      if (!files.has(path)) throw enoent(path);
      return files.get(path).data;
    },
    readdir(dir) {
      if (!dirs.has(dir)) throw enoent(dir);
      const names = new Set(['.', '..']);
      for (const p of [...files.keys(), ...dirs]) if (p.startsWith(`${dir}/`)) names.add(p.slice(dir.length + 1).split('/')[0]);
      return [...names];
    },
    stat(path) {
      if (files.has(path)) return { mode: FILE, size: files.get(path).size, mtime: files.get(path).mtime };
      if (dirs.has(path)) return { mode: DIR, size: 0, mtime: new Date(0) };
      throw enoent(path);
    },
    isFile: (mode) => (mode & 0o170000) === 0o100000,
    isDir: (mode) => (mode & 0o170000) === 0o040000,
    mount(type, opts, point) {
      if (!type || type.name !== 'IDBFS') throw new Error('mount without IDBFS');
      if (!dirs.has(point)) throw new Error(`mount point ${point} missing`);
      mounts.push(point);
    },
    // As IDBFS: a write keeps its connection in IDBFS.dbs; one the browser closed fails as Chrome's does.
    // The order of the folder's syncs in window.__syncLog: 'read' and 'read done' (IDBFS into memory), 'store' (memory
    // into IDBFS, which removes from IndexedDB what memory lacks). window.__slowRead (ms) makes the read slow, as a
    // large folder's is.
    syncfs(populate, cb) {
      window.__syncs[populate ? 'load' : 'store']++;
      (window.__syncLog ??= []).push(populate ? 'read' : 'store');
      // window.__userSeed: { path: bytes } of files the stored user folder holds (a visit before this one).
      if (populate) {
        setTimeout(() => {
          for (const [path, size] of Object.entries(window.__userSeed ?? {})) {
            mkdirTree(path.slice(0, path.lastIndexOf('/')));
            files.set(path, { data: new Uint8Array(size), size, own: false, mtime: new Date() });
          }
          // window.__seedSettings: a settings.txt where the player turned Save backups on again after the page's turn.
          if (window.__seedSettings) files.set('/user/settings.txt', { data: '# The browser turned save_backups off once (older copies took the browser\'s storage).\nsave_backups = on\n', size: 120, own: false, mtime: new Date() });
          window.__syncLog.push('read done'); cb(null);
        }, window.__slowRead ?? 5);
        return;
      }
      const dbs = M.IDBFS.dbs;
      if (!populate && dbs['/user']?.closed) {
        setTimeout(() => cb(new DOMException("Failed to execute 'transaction' on 'IDBDatabase': The database connection is closing.", 'InvalidStateError')), 5);
        return;
      }
      if (!populate && !dbs['/user']) { dbs['/user'] = { closed: false, close() { this.closed = true; } }; window.__syncs.opened++; }
      // window.__slowStore (ms): a store as slow as a large folder's on a phone; window.__storeDoneAt says when it ended.
      setTimeout(() => { window.__syncLog.push('store done'); window.__storeDoneAt = Date.now(); cb(null); }, window.__slowStore ?? 5);
    },
  };
  // As newer Emscripten: IDBFS is an exported runtime method (web/web.cmake), and FS has no filesystems table.
  M.IDBFS = { name: 'IDBFS', dbs: {} };
  M.ENV = {};
  let deps = 0, started = false;
  M.addRunDependency = () => { deps++; };
  M.removeRunDependency = () => { if (--deps === 0) init(); };
  M.callMain = (args) => {
    window.__fakeAt = Date.now();
    const all = [...files].filter(([p]) => p.startsWith('/game/')).map(([, f]) => f);   // the game files (the user folder has the page's own)
    window.__fake = {
      args, files: all.length, bytes: all.reduce((a, f) => a + f.size, 0), owned: all.every((f) => f.own),
      mounts, env: { ...M.ENV }, exe: files.has('/game/cm0102.exe'), rules: files.has('/game/Seasons/rules.tsv'),
      wasmType: window.__wasmType,
    };
    // One saved game, as the game writes it (a save container: version 2, one part), and a save in progress. A save of
    // that name in the user folder already (the cloud check brings one) stays as it is.
    const save = new Uint8Array(0x416ec + 32), dv = new DataView(save.buffer);
    dv.setInt32(0, 2, true);
    dv.setInt32(8, 1, true);
    dv.setUint32(12, 0x416ec, true);
    save.set(new TextEncoder().encode('Harbour City'), 0x416ec);
    if (!files.has('/user/Harbour City 2001-07-14.sav')) files.set('/user/Harbour City 2001-07-14.sav', { data: save, size: save.length, own: false, mtime: new Date() });
    files.set('/user/temp_save_file.sav', { data: new Uint8Array(10), size: 10, own: false, mtime: new Date() });
    window.__fakeSave = Array.from(save.subarray(0x416ec - 4));
    const c = M.canvas, g = c.getContext('2d');
    c.width = 800; c.height = 600;
    g.fillStyle = '#123'; g.fillRect(0, 0, 800, 600);
    g.fillStyle = '#fff'; g.font = '20px sans-serif';
    g.fillText(`stand-in engine: ${files.size} files, main(${args.join(' ')})`, 40, 300);
    // As a JSPI build: main never returns; it parks in emscripten_sleep between frames (the wasm of the page
    // check's JSPI case is a loop of emscripten_sleep(5)).
    if (parked) {
      window.__main = WebAssembly.promising(parked.exports.main)();
      window.__parked = true;
      return window.__main;
    }
    return undefined;
  };
  // A JSPI build gets its imports through the loader's instantiateWasm, the sleep as a WebAssembly.Suspending.
  let parked = null;
  window.__sleeps = 0;
  const jspi = typeof M.instantiateWasm === 'function' && typeof WebAssembly.Suspending === 'function';
  // The runtime's keyboard calls (rt_web_type, rt_web_key in user32.c), recorded; and SDL's window key listener,
  // which counts the keys it sees (keyboard.js must keep the keys of its field away from it).
  window.__typed = [];
  window.__sdlKeys = 0;
  // Play (play.c): rt_web_play's names in window.__played, and its answer window.__playAnswer (-1: on its way);
  // rt_web_in_game says window.__inGame (a game in play).
  window.__played = [];
  M.ccall = (name, ret, types, args) => {
    if (name === 'rt_web_type') window.__typed.push(['type', args[0]]);
    else if (name === 'rt_web_key') window.__typed.push(['key', args[0]]);
    else if (name === 'rt_web_play') { window.__played.push(args[0]); return window.__playAnswer ?? -1; }
    else throw new Error(`no export ${name}`);
    return null;
  };
  // An engine built before the keyboard calls has neither export (the page check sets __oldEngine for that case).
  if (!window.__oldEngine) {
    M._rt_web_type = M._rt_web_key = M._rt_web_play = () => {};
    M._rt_web_in_game = () => (window.__inGame ? 1 : 0);
    M._rt_web_progress = () => window.__progress ?? -1;          // display.c: the share done of a long job, -1: none
    M._rt_web_typing_row = () => window.__typingRow ?? -1;     // user32.c: the row of a text entry that takes keys, -1: none
  }
  function init() {
    if (started) return;
    started = true;
    addEventListener('keydown', () => { window.__sdlKeys++; });
    M.onRuntimeInitialized();
  }
  fetch(M.locateFile('cm0102_recomp.wasm')).then((r) => {
    window.__wasmType = r.headers.get('content-type');
    for (const f of M.preRun) f();
    if (jspi) {
      deps++;
      const imports = { env: { emscripten_sleep: new WebAssembly.Suspending((ms) => { window.__sleeps++; return new Promise((ok) => setTimeout(ok, ms)); }) } };
      M.instantiateWasm(imports, (instance) => { parked = instance; M.removeRunDependency('wasm'); });
    }
    if (deps === 0) init();
  });
})();
