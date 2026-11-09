// A stand-in for Emscripten's cm0102_recomp.js in the page check (page-check.cjs): the part of the Module contract
// that web/loader.js uses (locateFile, preRun, FS, ENV, run dependencies, onRuntimeInitialized, callMain), with no
// game behind it. It records what the loader gave it in window.__fake and draws one line on the canvas.
(() => {
  const M = window.Module;
  const files = new Map(), dirs = new Set(['/']), mounts = [];
  const mkdirTree = (p) => {
    let cur = '';
    for (const part of p.split('/').filter(Boolean)) { cur += `/${part}`; dirs.add(cur); }
  };
  M.FS = {
    mkdirTree,
    writeFile(path, data, opts) {
      if (!dirs.has(path.slice(0, path.lastIndexOf('/')) || '/')) throw new Error(`no folder for ${path}`);
      files.set(path, { size: data.length, own: Boolean(opts?.canOwn), zero: data.length ? data[0] === 0 : true });
    },
    mount(type, opts, point) {
      if (!type || type.name !== 'IDBFS') throw new Error('mount without IDBFS');
      if (!dirs.has(point)) throw new Error(`mount point ${point} missing`);
      mounts.push(point);
    },
    syncfs(populate, cb) { setTimeout(() => cb(null), 5); },
  };
  // As newer Emscripten: IDBFS is an exported runtime method (web/web.cmake), and FS has no filesystems table.
  M.IDBFS = { name: 'IDBFS' };
  M.ENV = {};
  let deps = 0, started = false;
  M.addRunDependency = () => { deps++; };
  M.removeRunDependency = () => { if (--deps === 0) init(); };
  M.callMain = (args) => {
    const all = [...files.values()];
    window.__fake = {
      args, files: files.size, bytes: all.reduce((a, f) => a + f.size, 0), owned: all.every((f) => f.own),
      mounts, env: { ...M.ENV }, exe: files.has('/game/cm0102.exe'), rules: files.has('/game/Seasons/rules.tsv'),
      wasmType: window.__wasmType,
    };
    const c = M.canvas, g = c.getContext('2d');
    c.width = 800; c.height = 600;
    g.fillStyle = '#123'; g.fillRect(0, 0, 800, 600);
    g.fillStyle = '#fff'; g.font = '20px sans-serif';
    g.fillText(`stand-in engine: ${files.size} files, main(${args.join(' ')})`, 40, 300);
  };
  function init() {
    if (started) return;
    started = true;
    M.onRuntimeInitialized();
  }
  fetch(M.locateFile('cm0102_recomp.wasm')).then((r) => {
    window.__wasmType = r.headers.get('content-type');
    for (const f of M.preRun) f();
    if (deps === 0) init();
  });
})();
