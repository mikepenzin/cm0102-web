// The vault: saves, older copies and data sets kept out of the page's memory (web/README.md, "Where the saves live").
//
// The engine's file system keeps every file in memory (Emscripten's MEMFS), and IDBFS reads the whole user folder into
// it at the start. A player with many saves ran the tab out of memory ("Array buffer allocation failed"). The vault
// keeps these files in an IndexedDB store of its own, as Blobs, which the browser keeps on disk:
// - the user folder holds a placeholder for each one: a file with its real size and time and no contents, so the game
//   lists it as before;
// - when the game opens a placeholder, the open waits while the page reads the Blob into it (instantiate below makes
//   the engine's open a Suspending import under JSPI), and when the game closes it unchanged, the contents go again;
// - a file that the game or the page writes stays in memory until it is closed and has not changed for STABLE ms,
//   then it goes into the vault and becomes a placeholder;
// - a renamed file keeps its record (the record moves to the new name before anything new is stored under the old
//   one), a deleted file loses it;
// - the page removes only the records it knows (mounted or stored by it): another tab's are not its own. Two tabs of
//   the game removed each other's new saves every 2 seconds that way, and a Download of such a save then gave an empty
//   file (loader.js now lets one tab play at a time; this is the second guard).
// The rest of the user folder (settings.txt, game.cfg, the fitted .lng files, the notes of the data sets) stays with
// IDBFS. Without JSPI (an Asyncify build, an old browser) the vault is off and the page works as before; files that an
// earlier visit put in the vault then come back into the user folder (materialize).

export const VAULT_DB = 'cm0102-vault';
const STORE = 'files';
const IDBFS_STORE = 'FILE_DATA', IDBFS_VERSION = 21;   // Emscripten's IDBFS: one database per mount point
const O_TRUNC = 512;                                    // Emscripten's flags (musl)
const ENOENT = 44;
const STABLE = 1500;

// The files that live in the vault: saves (*.sav) at the top of the user folder, every file in backups/ (older
// copies) and the files of each data set folder in Databases/ (not its notes, which start with a dot). The save in
// progress, temp_save_file.sav, is renamed when the game has written it, so it never goes into the vault.
export function isVaultFile(path) {
  return /^\/user\/[^/]+\.sav$/i.test(path) || /^\/user\/backups\/[^/]+$/.test(path)
    || /^\/user\/Databases\/[^/]+\/[^./][^/]*$/.test(path);
}
const isTemp = (path) => /^\/user\/temp_save_file\.sav$/i.test(path);

const done = (r) => new Promise((ok, bad) => { r.onsuccess = () => ok(r.result); r.onerror = () => bad(r.error); });

function openDB(idb, name, version, upgrade) {
  return new Promise((ok, bad) => {
    let r;
    try { r = idb.open(name, version); } catch (e) { bad(e); return; }
    r.onupgradeneeded = (e) => upgrade(r.result, e.target.transaction);
    r.onsuccess = () => ok(r.result);
    r.onerror = () => bad(r.error);
    r.onblocked = () => bad(new Error(`the browser's storage ${name} is in use by another tab`));
  });
}

// FS: the engine's FS (Module.FS), or a function that gives it (the engine is instantiated before its FS is there).
// log(text): the engine log. idb: indexedDB (a test can pass another).
export function createVault({ FS: fsOrGetter, log = () => {}, idb = globalThis.indexedDB, stable = STABLE }) {
  const getFS = typeof fsOrGetter === 'function' ? fsOrGetter : () => fsOrGetter;
  const FS = new Proxy({}, { get: (_, k) => getFS()[k], set: (_, k, v) => { getFS()[k] = v; return true; } });
  let db = null, active = false, filtering = false, memory = null, exportsOf = null;
  const managed = new Set();                    // the nodes with a record in the vault (placeholders and loaded files)
  const mine = new Set();                       // the records this page mounted or stored: the only ones it removes
  const loading = new Map();
  let chain = Promise.resolve();
  const serial = (fn) => { const p = chain.then(fn); chain = p.catch(() => {}); return p; };

  // The browser can close the connection by itself (Android's Chrome does, for example when it frees memory or the
  // site's storage is cleared): onclose fires, and every transaction after it fails with "The database connection is
  // closing". The vault then opens it again at its next request, and a request that met the closing tries once more.
  let opening = null;
  const connect = () => opening ??= openDB(idb, VAULT_DB, 1, (d) => {
    if (!d.objectStoreNames.contains(STORE)) d.createObjectStore(STORE);
  }).then((d) => {
    const gone = () => { if (db === d) db = null; };
    d.onclose = () => { gone(); log('page: the vault: the browser closed its storage; it opens again at the next use'); };
    d.onversionchange = () => { d.close(); gone(); };
    db = d;
    return d;
  }).finally(() => { opening = null; });
  const closing = (e) => e?.name === 'InvalidStateError' || e?.name === 'AbortError' || e?.name === 'UnknownError';
  async function request(mode, ask) {
    for (let tries = 0; ; tries++) {
      const d = db ?? await connect();
      try {
        return await ask(d.transaction([STORE], mode).objectStore(STORE));
      } catch (e) {
        if (tries || !closing(e)) throw e;
        if (db === d) db = null;
      }
    }
  }
  const get = (key) => request('readonly', (s) => done(s.get(key)));
  const put = (key, value) => request('readwrite', (s) => done(s.put(value, key)));
  const del = (key) => request('readwrite', (s) => done(s.delete(key)));
  const keys = () => request('readonly', (s) => done(s.getAllKeys()));

  const lookup = (path) => { try { return FS.lookupPath(path).node; } catch { return null; } };
  const pathOf = (node) => { try { return FS.getPath(node); } catch { return null; } };
  const alive = (node) => { const p = pathOf(node); return p && lookup(p) === node ? p : null; };
  const inUse = (node) => FS.streams.some((s) => s && s.node === node);
  const evict = (node) => { node.contents = new Uint8Array(0); node.vault = 'out'; };

  async function open() {
    await connect();
  }

  // The vault's files that IDBFS of an earlier page kept: each one moves to the vault, one at a time, so that only one
  // file is in memory at once, and leaves IDBFS's store. Returns how many moved.
  async function migrateFrom(mount) {
    const src = await openDB(idb, mount, IDBFS_VERSION, (d, t) => {     // as IDBFS opens it (getDB)
      const s = d.objectStoreNames.contains(IDBFS_STORE) ? t.objectStore(IDBFS_STORE) : d.createObjectStore(IDBFS_STORE);
      if (!s.indexNames.contains('timestamp')) s.createIndex('timestamp', 'timestamp', { unique: false });
    });
    let moved = 0;
    try {
      const all = await done(src.transaction([IDBFS_STORE], 'readonly').objectStore(IDBFS_STORE).getAllKeys());
      for (const key of all.filter((k) => typeof k === 'string' && isVaultFile(k))) {
        const entry = await done(src.transaction([IDBFS_STORE], 'readonly').objectStore(IDBFS_STORE).get(key));
        if (entry?.contents) {
          const mtime = +new Date(entry.timestamp);
          await put(key, { size: entry.contents.length, mtime, blob: new Blob([entry.contents]) });
        }
        await done(src.transaction([IDBFS_STORE], 'readwrite').objectStore(IDBFS_STORE).delete(key));
        moved++;
      }
    } finally { src.close(); }
    filtering = true;                           // from now on IDBFS leaves the vault's files alone (owns)
    return moved;
  }

  async function records() {
    return request('readonly', (s) => new Promise((ok, bad) => {
      const out = [];
      const c = s.openCursor();
      c.onsuccess = () => { const cur = c.result; if (!cur) { ok(out); return; } out.push({ key: cur.key, size: cur.value.size, mtime: cur.value.mtime }); cur.continue(); };
      c.onerror = () => bad(c.error);
    }));
  }

  // A placeholder for each record: the game sees the file, its size and its time; its contents stay in the vault.
  async function mount() {
    let n = 0, bytes = 0;
    for (const r of await records()) {
      mine.add(r.key);
      if (lookup(r.key)) continue;              // a file there already (an older page put it in IDBFS): it wins
      FS.mkdirTree(r.key.slice(0, r.key.lastIndexOf('/')));
      FS.writeFile(r.key, new Uint8Array(0));
      const node = lookup(r.key);
      node.usedBytes = r.size;
      node.mtime = node.atime = node.ctime = r.mtime;
      node.vaultKey = r.key; node.vaultMtime = r.mtime; node.vaultSize = r.size;
      evict(node);
      managed.add(node);
      n++; bytes += r.size;
    }
    return { files: n, bytes };
  }

  // Without JSPI: the records become files of the user folder again (in memory, as before the vault).
  async function materialize() {
    let n = 0;
    for (const r of await records()) {
      if (lookup(r.key)) continue;
      const rec = await get(r.key);
      FS.mkdirTree(r.key.slice(0, r.key.lastIndexOf('/')));
      FS.writeFile(r.key, new Uint8Array(await rec.blob.arrayBuffer()));
      const node = lookup(r.key);
      node.mtime = node.atime = node.ctime = r.mtime;
      n++;
    }
    return n;
  }

  // The contents of a placeholder into its node. The node stays pinned (not given up) until unpin.
  function load(node) {
    if (node.vault !== 'out') return Promise.resolve();
    if (loading.has(node)) return loading.get(node);
    const p = serial(async () => {
      if (node.vault !== 'out') return;
      const rec = await get(node.vaultKey);
      if (!rec) throw new Error(`the browser's storage has no ${node.vaultKey}`);
      const bytes = new Uint8Array(await rec.blob.arrayBuffer());
      if (node.vault !== 'out') return;
      node.contents = bytes;
      node.usedBytes = bytes.length;
      node.vaultMtime = node.mtime; node.vaultSize = bytes.length;
      node.vault = 'in';
    }).finally(() => loading.delete(node));
    loading.set(node, p);
    return p;
  }

  // Module.instantiateWasm: the engine's open (__syscall_openat) waits for a placeholder's contents. Returns false
  // when this engine or browser cannot (no JSPI): the caller then instantiates as Emscripten does.
  function instantiate(info, receive, wasmUrl) {
    const env = info?.env, orig = env?.__syscall_openat;
    if (typeof WebAssembly.Suspending !== 'function' || typeof orig !== 'function') return false;
    const decoder = new TextDecoder();
    const cstr = (ptr) => {
      const b = new Uint8Array(memory.buffer);
      let e = ptr;
      while (b[e]) e++;
      return decoder.decode(b.subarray(ptr, e));
    };
    env.__syscall_openat = new WebAssembly.Suspending((dirfd, path, flags, varargs) => {
      if (!active || !memory) return orig(dirfd, path, flags, varargs);
      let p = cstr(path);
      if (!p.startsWith('/')) p = `${FS.cwd().replace(/\/$/, '')}/${p}`;
      const node = p.startsWith('/user/') ? lookup(p) : null;
      if (!node || node.vault !== 'out') return orig(dirfd, path, flags, varargs);
      if (flags & O_TRUNC) { node.vault = undefined; return orig(dirfd, path, flags, varargs); }  // rewritten whole
      node.vaultPin = (node.vaultPin || 0) + 1;
      return load(node).then(() => orig(dirfd, path, flags, varargs), (e) => { log(`page: ${e.message}`); return -ENOENT; })
        .finally(() => { node.vaultPin--; });
    });
    const go = (r) => { memory = r.instance.exports.memory; exportsOf = r.instance.exports; receive(r.instance, r.module); };
    WebAssembly.instantiateStreaming(fetch(wasmUrl), info).then(go).catch(async () => {
      const bytes = await (await fetch(wasmUrl)).arrayBuffer();
      go(await WebAssembly.instantiate(bytes, info));
    });
    return true;
  }

  // The engine's FS: a closed file that the game only read gives its contents up; a written one becomes a file of
  // its own (persist takes it); a page read of a placeholder fails loudly (use bytes or blob).
  function wrapFS() {
    const close = FS.close, openFn = FS.open;
    FS.close = (stream) => {
      const node = stream?.node;
      const r = close(stream);
      if (active && node?.vault === 'in' && !node.vaultPin) {
        if (node.mtime !== node.vaultMtime || node.usedBytes !== node.vaultSize) node.vault = undefined;
        else if (!inUse(node)) evict(node);
      }
      return r;
    };
    FS.open = (path, flags, mode) => {
      if (active && typeof path === 'string') {
        const node = lookup(path);
        if (node?.vault === 'out') {
          const f = typeof flags === 'string' ? FS.modeStringToFlags(flags) : flags;
          if (f & O_TRUNC) node.vault = undefined;
          else throw new Error(`${path} is in the browser's storage, not in memory: the page must read it with the vault`);
        }
      }
      return openFn(path, flags, mode);
    };
  }

  // The vault's files of the user folder in memory: the ones the game or the page wrote.
  function written() {
    const out = [];
    const files = (dir) => { try { return FS.readdir(dir).filter((n) => n !== '.' && n !== '..').map((n) => `${dir}/${n}`); } catch { return []; } };
    const isFile = (p) => { try { return FS.isFile(FS.stat(p).mode); } catch { return false; } };
    const sets = files('/user/Databases').filter((p) => !isFile(p));
    for (const p of [...files('/user'), ...files('/user/backups'), ...sets.flatMap(files)]) {
      if (!isVaultFile(p) || isTemp(p) || !isFile(p)) continue;
      const node = lookup(p);
      if (node && !node.vault) out.push([p, node]);
    }
    return out;
  }

  // A managed node whose record is under another name (a rename) gets its record moved first.
  async function rekey(node, path) {
    const old = node.vaultKey;
    if (old === path) return;
    const rec = await get(old);
    if (rec) { await put(path, rec); mine.add(path); }
    node.vaultKey = path;
    if (![...managed].some((m) => m !== node && m.vaultKey === old)) { await del(old); mine.delete(old); }
  }

  // One pass: renamed files keep their records; written files go into the vault (and out of memory when closed);
  // records that no file points at go. force: also files that changed less than `stable` ms ago (the tab is closing).
  function persist({ force = false } = {}) {
    if (!active) return Promise.resolve({ stored: 0, removed: 0 });
    return serial(async () => {
      let stored = 0, removed = 0;
      for (const node of [...managed]) {
        const p = alive(node);
        if (!p) { managed.delete(node); continue; }
        if (node.vaultKey !== p) await rekey(node, p);
      }
      for (const [p, node] of written()) {
        if (inUse(node) || (!force && Date.now() - node.mtime < stable)) continue;
        for (const other of [...managed]) if (other !== node && other.vaultKey === p) { const q = alive(other); if (q && q !== p) await rekey(other, q); }
        const size = node.usedBytes, mtime = node.mtime;
        await put(p, { size, mtime, blob: new Blob([node.contents ? node.contents.subarray(0, size) : new Uint8Array(0)]) });
        mine.add(p);
        stored++;
        if (lookup(p) !== node || node.mtime !== mtime || node.usedBytes !== size || node.vault) continue;   // changed meanwhile
        node.vaultKey = p; node.vaultMtime = mtime; node.vaultSize = size;
        managed.add(node);
        if (!inUse(node) && !node.vaultPin) evict(node); else node.vault = 'in';
      }
      const wanted = new Set([...managed].map((n) => n.vaultKey));
      const there = new Set(await keys());
      for (const k of [...mine]) {
        if (wanted.has(k)) continue;
        mine.delete(k);
        if (there.has(k)) { await del(k); removed++; }
      }
      return { stored, removed };
    });
  }

  // For the page: the file's bytes (a copy the caller may change) or a Blob (for a download or an upload), without
  // bringing a placeholder's contents into the engine's memory. A placeholder whose record is gone fails: its node has
  // no contents, and a Download once made an empty file of it.
  async function record(node, path) {
    const rec = await serial(() => get(node.vaultKey));
    if (!rec) throw new Error(`the browser's storage no longer has ${path.slice(path.lastIndexOf('/') + 1)}`);
    return rec;
  }
  async function bytes(path) {
    const node = lookup(path);
    if (node?.vault === 'out') return new Uint8Array(await (await record(node, path)).blob.arrayBuffer());
    return FS.readFile(path);
  }
  async function view(path) {
    const node = lookup(path);
    if (node?.vault === 'out') return bytes(path);
    const n = node ?? FS.lookupPath(path).node;
    return n.contents instanceof Uint8Array ? n.contents.subarray(0, n.usedBytes ?? n.contents.length) : FS.readFile(path);
  }
  async function blob(path) {
    const node = lookup(path);
    if (node?.vault === 'out') return (await record(node, path)).blob;
    const n = node ?? FS.lookupPath(path).node;
    return new Blob([n.contents instanceof Uint8Array ? n.contents.subarray(0, n.usedBytes ?? n.contents.length) : FS.readFile(path)]);
  }

  // The records into the user folder's IDBFS store took place (materialize, then a sync): the vault empties, so that
  // a later page with the vault on does not bring back files that were deleted meanwhile.
  const clear = () => serial(() => request('readwrite', (s) => done(s.clear())));

  // IDBFS leaves the vault's files alone once they moved to the vault (migrateFrom), placeholders above all: their
  // contents are not in memory.
  const owns = (path) => {
    if (!filtering) return false;
    if (isVaultFile(path)) return true;
    const node = lookup(path);
    return Boolean(node?.vault);
  };

  // The vault off again (a step of the start failed): IDBFS takes all files, the page reads them from memory.
  const stop = () => { active = false; filtering = false; };

  return {
    get active() { return active; },
    get exports() { return exportsOf; },
    stop,
    set active(on) { active = Boolean(on); },
    open, migrateFrom, mount, materialize, clear, instantiate, wrapFS, persist, bytes, view, blob, owns, records,
    held: () => [...managed].filter((n) => n.vault === 'in').length,
  };
}
