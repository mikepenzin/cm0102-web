// The vault: saves, older copies and data sets kept out of the page's memory (web/README.md, "Where the saves live").
//
// The engine's file system keeps every file in memory (Emscripten's MEMFS), and IDBFS reads the whole user folder into
// it at the start. A player with many saves ran the tab out of memory ("Array buffer allocation failed"). The vault
// keeps these files in an IndexedDB store of its own, as Blobs, which the browser keeps on disk:
// - the user folder holds a placeholder for each one: a file with its real size and time and no contents, so the game
//   lists it as before;
// - when the game opens a placeholder to read it, nothing is loaded: each read of the game takes the pieces of the Blob
//   it needs (4 MB each, the last dozen kept), so a save is never in memory whole. instantiate below makes the engine's
//   open and read Suspending imports under JSPI, so they can wait for the browser's storage. The game opens a save about
//   100 times during a load, and the vault once read the whole file at every open: with a player's saves of 690 MB a
//   load took 99 s and 5.6 GB of memory in the tab, and the Load Game list 17 s for four of them (15 November 2026).
//   A file opened to be written (and not emptied) still comes into memory whole, and goes again when it is closed
//   unchanged;
// - a file that the game or the page writes stays in memory until it is closed and has not changed for STABLE ms,
//   then it goes into the vault and becomes a placeholder;
// - a renamed file keeps its record (the record moves to the new name before anything new is stored under the old
//   one), a deleted file loses it;
// - the page removes only the records it knows (mounted or stored by it): another tab's are not its own. Two tabs of
//   the game removed each other's new saves every 2 seconds that way, and a Download of such a save then gave an empty
//   file (loader.js now lets one tab play at a time; this is the second guard).
// The game files (/game) can be placeholders too (gameFile, loader.js): a file the browser keeps in its cache storage
// is read in pieces from there, as a save is from the vault, and the 467 MB of game files are no longer in the page's
// memory (the game reads 18 MB of them on the way to its first screen; the tab's JS heap was 497 MB, 21 November
// 2026). Such a placeholder has a source (node.vaultSource: its Blob, from the cache) and no record: nothing of the
// vault's store, rounds or IDBFS touches it.
// The rest of the user folder (settings.txt, game.cfg, the fitted .lng files, the notes of the data sets) stays with
// IDBFS. Without JSPI (an Asyncify build, an old browser) the vault is off and the page works as before; files that an
// earlier visit put in the vault then come back into the user folder (materialize).

export const VAULT_DB = 'cm0102-vault';
// Version 2 shuts out the pages from before one tab at a time (loader.js, myTurn): such a page takes no turn, and its
// rounds removed the records of other tabs. Opening version 2 asks its connection to close; the page then cannot open
// version 1 again, so it reads and writes nothing more here. A page so old that it keeps its connection open holds the
// opening up (blocked) until it is closed or loaded again: onBlocked says so.
export const VAULT_VERSION = 2;
const STORE = 'files';
const IDBFS_STORE = 'FILE_DATA', IDBFS_VERSION = 21;   // Emscripten's IDBFS: one database per mount point
const O_TRUNC = 512, O_ACCMODE = 3;                     // Emscripten's flags (musl); O_RDONLY is 0
const ENOENT = 44, EIO = 29;
export const PIECE = 4 << 20;                           // a save is read from the storage in pieces of this size
// A file that comes as a stream (a cloud download, adoptStream) is stored as part records of PIECE bytes, written one at
// a time as it comes, and a main record that names them ({ size, mtime, partsId, parts }): an iPhone could not hold a
// 446 MB cloud save as one Blob, and iOS closed the tab at every start (18 November 2026). A part's key starts with PART
// (a file's key starts with /), so records() and the rounds never take a part for a file; mount() removes the parts
// that no record names any more (a copy shares its parts, a rename keeps them).
export const PART = '~part:';
const partKey = (id, i) => `${PART}${id}:${String(i).padStart(6, '0')}`;
const KEEP_PIECES = 12;                                 // the pieces kept in memory, the last used (48 MB)
// A placeholder: the file's contents are in the browser's storage, not in its node ('lazy' while the game reads it in
// pieces). The page reads such a file with bytes, view or blob.
export const isAway = (node) => node?.vault === 'out' || node?.vault === 'lazy';
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

// onBlocked(true): another page's connection holds the opening up, which goes on once it closes; onBlocked(false): it
// went on. Without onBlocked a blocked opening fails.
function openDB(idb, name, version, upgrade, onBlocked = null) {
  return new Promise((ok, bad) => {
    let r, blocked = false;
    try { r = idb.open(name, version); } catch (e) { bad(e); return; }
    r.onupgradeneeded = (e) => upgrade(r.result, e.target.transaction);
    r.onsuccess = () => { if (blocked) onBlocked(false); ok(r.result); };
    r.onerror = () => bad(r.error);
    r.onblocked = () => {
      if (!onBlocked) { bad(Object.assign(new Error(`the browser's storage ${name} is in use by another tab`), { key: 'the browser\'s storage {name} is in use by another tab', vars: { name } })); return; }
      if (!blocked) { blocked = true; onBlocked(true); }
    };
  });
}

// FS: the engine's FS (Module.FS), or a function that gives it (the engine is instantiated before its FS is there).
// log(text): the engine log. idb: indexedDB (a test can pass another).
export function createVault({ FS: fsOrGetter, log = () => {}, idb = globalThis.indexedDB, stable = STABLE, onBlocked = null }) {
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
  const connect = () => opening ??= openDB(idb, VAULT_DB, VAULT_VERSION, (d) => {
    if (!d.objectStoreNames.contains(STORE)) d.createObjectStore(STORE);
  }, onBlocked).then((d) => {
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
  // A write counts once its transaction committed. A request's success comes before the commit, and the commit can
  // still fail (a full disk, or the site's room in the browser: QuotaExceededError); the vault once dropped a save's
  // contents from memory on the request's success, and a failed commit then lost the save.
  const committed = (s, r) => new Promise((ok, bad) => {
    const t = s.transaction;
    const no = () => bad(t.error ?? r.error ?? new DOMException('the browser\'s storage did not keep the write', 'AbortError'));
    t.oncomplete = () => ok(r.result);
    t.onabort = no;
    t.onerror = no;
  });
  const put = (key, value) => request('readwrite', (s) => committed(s, s.put(value, key)));
  const del = (key) => request('readwrite', (s) => committed(s, s.delete(key)));
  const keys = () => request('readonly', (s) => done(s.getAllKeys()));
  // A record's Blob: its own, or its parts joined (a Blob of the storage's own, read from disk; never in memory whole).
  async function blobOf(rec) {
    if (rec.blob) return rec.blob;
    if (!rec.partsId) return new Blob([]);
    const parts = rec.parts ? await request('readonly', (s) => done(s.getAll(IDBKeyRange.bound(partKey(rec.partsId, 0), partKey(rec.partsId, rec.parts - 1))))) : [];
    if (parts.length !== rec.parts) {
      throw Object.assign(new Error(`the browser's storage has ${parts.length} of the ${rec.parts} parts of this file`),
        { key: 'the browser\'s storage has {have} of the {parts} parts of this file', vars: { have: parts.length, parts: rec.parts } });
    }
    return new Blob(parts.map((x) => x.blob));
  }
  // A placeholder's Blob: a game file's from its source (the cache), a vault file's from its record.
  async function sourceOf(node) {
    if (node.vaultSource) return node.vaultSource();
    const rec = await get(node.vaultKey);
    if (!rec) throw Object.assign(new Error(`the browser's storage has no ${node.vaultKey}`), { key: 'the browser\'s storage has no {name}', vars: { name: node.vaultKey } });
    return blobOf(rec);
  }
  const dropParts = (id) => request('readwrite', (s) => committed(s, s.delete(IDBKeyRange.bound(`${PART}${id}:`, `${PART}${id}:\uffff`))));
  const newPartsId = () => `${Date.now().toString(36)}${Math.random().toString(36).slice(2, 8)}`;
  const writing = new Set();                    // the ids of parts being written, whose main record is not there yet

  // A file of the user folder into the storage as parts of PIECE bytes, each a Blob of its own written on its own:
  // a Blob of the whole file was a second copy of it in the tab's memory, beside the file (an iPhone closed the tab
  // after the game saved, 21 November 2026). still(): the file is still the version being stored; when it changed
  // meanwhile, its parts go and the call gives null (the next round stores the new version). Else { partsId, parts }.
  async function putParts(bytes, size, still) {
    const id = newPartsId();
    writing.add(id);
    let n = 0;
    try {
      for (let at = 0; at < size || n === 0; at += PIECE, n++) {
        if (!still()) { await dropParts(id); return null; }
        await put(partKey(id, n), { part: true, blob: new Blob([bytes.subarray(at, Math.min(size, at + PIECE))]) });
      }
      return { partsId: id, parts: n };
    } catch (e) {
      await dropParts(id).catch(() => {});
      throw e;
    } finally { writing.delete(id); }
  }

  const lookup = (path) => { try { return FS.lookupPath(path).node; } catch { return null; } };
  const pathOf = (node) => { try { return FS.getPath(node); } catch { return null; } };
  const alive = (node) => { const p = pathOf(node); return p && lookup(p) === node ? p : null; };
  const inUse = (node) => FS.streams.some((s) => s && s.node === node);
  // The node is whole in memory again, or a file of its own: its reads are MEMFS's, and its Blob is not kept (the
  // record may change; the next open takes it anew, and with it other pieces).
  const release = (node) => { if (node.vaultOps) node.stream_ops = node.vaultOps; node.vaultBlob = null; };
  const evict = (node) => { node.contents = new Uint8Array(0); node.vault = 'out'; release(node); };

  // ---- Reading in pieces ----
  const pieces = new Map();                     // "<blob id>:<index>" -> bytes; in the order of use, the oldest first
  let blobIds = 0;
  // The reads of a node that the game reads in pieces: from the pieces readForGame brought. A stream opened so keeps
  // these after the node changed (loaded whole, rewritten): they then read as MEMFS does.
  function pieceRead(stream, buffer, offset, length, position) {
    const node = stream.node;
    if (node.vault !== 'lazy') return node.vaultOps.read(stream, buffer, offset, length, position);
    const n = Math.max(0, Math.min(length, node.usedBytes - position));
    let done = 0;
    while (done < n) {
      const at = position + done, i = Math.floor(at / PIECE), piece = pieces.get(`${node.vaultBlobId}:${i}`);
      if (!piece) throw FS.ErrnoError ? new FS.ErrnoError(EIO) : Object.assign(new Error('a piece of the file was not read first'), { errno: EIO });
      const from = at - i * PIECE, take = Math.min(n - done, piece.length - from);
      buffer.set(piece.subarray(from, from + take), offset + done);
      done += take;
    }
    return done;
  }
  const readInPieces = (node, blob) => {
    node.vaultBlob = blob;
    node.vaultBlobId = ++blobIds;
    node.vaultOps ??= node.stream_ops;
  };
  const lazy = (node) => { node.stream_ops = { ...node.vaultOps, read: pieceRead }; node.vault = 'lazy'; };
  // The game opens a placeholder (flags: those of open). To read: nothing is loaded. To write: the whole file, as load.
  // null when the open can go on at once, else a promise that it waits for.
  function openForGame(node, flags) {
    if (!isAway(node)) return null;
    if (flags & O_TRUNC) { release(node); node.vault = undefined; return null; }     // rewritten whole
    if ((flags & O_ACCMODE) !== 0) return load(node);
    if (node.vault === 'lazy') return null;
    if (node.vaultBlob) { lazy(node); return null; }
    const take = async () => {
      if (node.vault !== 'out') return;
      const blob = await sourceOf(node);
      if (node.vault !== 'out') return;
      readInPieces(node, blob);
      lazy(node);
    };
    // A game file needs nothing of the vault's store, so it does not wait behind a round of it.
    return node.vaultSource ? take() : serial(take);
  }
  // Before a read of the game at position, of length bytes: the pieces it needs. null when they are all there.
  function readForGame(node, position, length) {
    if (node.vault !== 'lazy') return null;
    const end = Math.min(node.usedBytes, position + length);
    if (position >= end) return null;
    const first = Math.floor(position / PIECE), last = Math.floor((end - 1) / PIECE), id = node.vaultBlobId, missing = [];
    for (let i = first; i <= last; i++) {
      const k = `${id}:${i}`, piece = pieces.get(k);
      if (piece) { pieces.delete(k); pieces.set(k, piece); } else missing.push(i);
    }
    if (!missing.length) return null;
    const blob = node.vaultBlob, size = node.usedBytes;
    return (async () => {
      for (const i of missing) pieces.set(`${id}:${i}`, new Uint8Array(await blob.slice(i * PIECE, Math.min(size, (i + 1) * PIECE)).arrayBuffer()));
      for (const k of [...pieces.keys()]) {             // the oldest go, but none of this read
        if (pieces.size <= KEEP_PIECES) break;
        const [kid, ki] = k.split(':').map(Number);
        if (!(kid === id && ki >= first && ki <= last)) pieces.delete(k);
      }
    })();
  }
  // After a close (or an open that failed): a node read in pieces with no stream left is a plain placeholder again.
  // Its Blob and its pieces stay for the next open.
  function settle(node) {
    if (node?.vault === 'lazy' && !inUse(node)) { node.stream_ops = node.vaultOps; node.vault = 'out'; }
  }

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
      c.onsuccess = () => {
        const cur = c.result;
        if (!cur) { ok(out); return; }
        if (!(typeof cur.key === 'string' && cur.key.startsWith(PART))) out.push({ key: cur.key, size: cur.value.size, mtime: cur.value.mtime, partsId: cur.value.partsId ?? null });
        cur.continue();
      };
      c.onerror = () => bad(c.error);
    }));
  }

  // The parts that no record names (a deleted or replaced file, a download that stopped): removed at the start.
  async function dropUnnamedParts(named) {
    const ids = await request('readonly', (s) => done(s.getAllKeys(IDBKeyRange.bound(PART, `${PART}\uffff`))));
    const gone = new Set(ids.map((k) => k.slice(PART.length, k.lastIndexOf(':'))).filter((id) => !named.has(id) && !writing.has(id)));
    for (const id of gone) await dropParts(id);
    return gone.size;
  }

  // A placeholder for each record: the game sees the file, its size and its time; its contents stay in the vault.
  // The placeholder of a record in the user folder: the file's name, size and time, no contents.
  function placeholder(key, size, mtime) {
    FS.mkdirTree(key.slice(0, key.lastIndexOf('/')));
    FS.writeFile(key, new Uint8Array(0));
    const node = lookup(key);
    node.usedBytes = size;
    node.mtime = node.atime = node.ctime = mtime;
    node.vaultKey = key; node.vaultMtime = mtime; node.vaultSize = size;
    evict(node);
    managed.add(node);
  }
  async function mount() {
    let n = 0, bytes = 0;
    const all = await records();
    await dropUnnamedParts(new Set(all.map((r) => r.partsId).filter(Boolean)));
    for (const r of all) {
      mine.add(r.key);
      if (lookup(r.key)) continue;              // a file there already (an older page put it in IDBFS): it wins
      placeholder(r.key, r.size, r.mtime);
      n++; bytes += r.size;
    }
    return { files: n, bytes };
  }
  // A file from outside (an upload, a cloud download) straight into the vault: its Blob is stored as it is (the browser
  // copies it, not the page) and the user folder gets its placeholder, so it is never in the page's memory. A file of
  // that name there is replaced. In one step of the vault's queue: a round between the two would remove the record.
  function adopt(path, blob, mtime = Date.now()) {
    return serial(async () => {
      // A file renamed away from this name whose record the round has not moved yet (the save editor keeps the save as
      // it was under backups/, then stores the edited one here): its record moves first, or the round would later give
      // it this new file's bytes.
      for (const other of [...managed]) if (other.vaultKey === path) { const q = alive(other); if (q && q !== path) await rekey(other, q); }
      await put(path, { size: blob.size, mtime, blob });
      mine.add(path);
      const old = lookup(path);
      if (old) managed.delete(old);
      placeholder(path, blob.size, mtime);
    });
  }

  // A file that comes as a stream (a cloud download) straight into the vault, in parts of PIECE bytes written as it comes:
  // never more than a part of it in the page's memory (an iPhone, 18 November 2026: a 446 MB cloud save as one Blob,
  // and iOS closed the tab). check(head, size): why the file is refused (its first bytes, at most a part, and its size),
  // or null. Resolves to why, or null once it is stored and the user folder has its placeholder.
  async function adoptStream(path, stream, mtime = Date.now(), check = () => null) {
    const id = newPartsId();
    writing.add(id);
    const reader = stream.getReader();
    let buf = new Uint8Array(PIECE), fill = 0, n = 0, size = 0, head = null;
    const flush = async () => {
      if (!fill) return;
      const part = buf.subarray(0, fill);
      if (!head) head = part.slice();
      await put(partKey(id, n), { part: true, blob: new Blob([part]) });
      n++;
      fill = 0;
    };
    try {
      for (;;) {
        const { value, done: end } = await reader.read();
        if (end) break;
        let at = 0;
        while (at < value.length) {
          const take = Math.min(PIECE - fill, value.length - at);
          buf.set(value.subarray(at, at + take), fill);
          fill += take; at += take; size += take;
          if (fill === PIECE) await flush();
        }
      }
      await flush();
      buf = null;
      const why = check(head ?? new Uint8Array(0), size);
      if (why) { await dropParts(id); writing.delete(id); return why; }
    } catch (e) {
      reader.cancel().catch(() => {});
      await dropParts(id).catch(() => {});
      writing.delete(id);
      throw e;
    }
    try {
      await serial(async () => {
        for (const other of [...managed]) if (other.vaultKey === path) { const q = alive(other); if (q && q !== path) await rekey(other, q); }
        await put(path, { size, mtime, partsId: id, parts: n });
        mine.add(path);
        const old = lookup(path);
        if (old) managed.delete(old);
        placeholder(path, size, mtime);
      });
    } finally { writing.delete(id); }
    return null;
  }

  // A file of the vault copied under another name, as a record of its own: the browser copies the Blob, not the page,
  // and the file keeps its name all the while. The save editor's older copy: it once renamed the save away and stored
  // the edited one after, and a cloud sync in between found the save missing and brought the cloud's back over the
  // edit. Resolves to false for a file whose bytes are not in the vault yet (written, not stored): the caller copies it.
  function copy(from, to) {
    return serial(async () => {
      const node = lookup(from);
      if (!node || !managed.has(node)) return false;
      if (node.vault === 'in' && (node.mtime !== node.vaultMtime || node.usedBytes !== node.vaultSize)) return false;
      if (node.vaultKey !== from) await rekey(node, from);
      const rec = await get(from);
      if (!rec) return false;
      for (const other of [...managed]) if (other.vaultKey === to) { const q = alive(other); if (q && q !== to) await rekey(other, q); }
      await put(to, { ...rec });
      mine.add(to);
      const old = lookup(to);
      if (old) managed.delete(old);
      placeholder(to, rec.size, rec.mtime);
      return true;
    });
  }

  // Without JSPI: the records become files of the user folder again (in memory, as before the vault).
  async function materialize() {
    let n = 0;
    for (const r of await records()) {
      if (lookup(r.key)) continue;
      const rec = await get(r.key);
      FS.mkdirTree(r.key.slice(0, r.key.lastIndexOf('/')));
      FS.writeFile(r.key, new Uint8Array(await (await blobOf(rec)).arrayBuffer()));
      const node = lookup(r.key);
      node.mtime = node.atime = node.ctime = r.mtime;
      n++;
    }
    return n;
  }

  // The contents of a placeholder into its node. The node stays pinned (not given up) until unpin.
  function load(node) {
    if (!isAway(node)) return Promise.resolve();
    if (loading.has(node)) return loading.get(node);
    const p = serial(async () => {
      if (!isAway(node)) return;
      const bytes = new Uint8Array(await (await sourceOf(node)).arrayBuffer());
      if (!isAway(node)) return;
      node.contents = bytes;
      node.usedBytes = bytes.length;
      node.vaultMtime = node.mtime; node.vaultSize = bytes.length;
      release(node);
      node.vault = 'in';
    }).finally(() => loading.delete(node));
    loading.set(node, p);
    return p;
  }

  // Module.instantiateWasm: the engine's open (__syscall_openat) and read (fd_read: the runtime reads with read(),
  // host/posix.c) can wait for the browser's storage. Returns false when this engine or browser cannot (no JSPI): the
  // caller then instantiates as Emscripten does.
  let gameOpens = 0;                            // the engine's own opens in progress: FS.open lets those through
  let gameFiles = 0;                            // game files made placeholders (gameFile)
  function instantiate(info, receive, wasmUrl) {
    const env = info?.env, orig = env?.__syscall_openat;
    const wasi = info?.wasi_snapshot_preview1 ?? env, origRead = wasi?.fd_read;
    if (typeof WebAssembly.Suspending !== 'function' || typeof orig !== 'function') return false;
    const decoder = new TextDecoder();
    const cstr = (ptr) => {
      const b = new Uint8Array(memory.buffer);
      let e = ptr;
      while (b[e]) e++;
      return decoder.decode(b.subarray(ptr, e));
    };
    env.__syscall_openat = new WebAssembly.Suspending((dirfd, path, flags, varargs) => {
      if (!memory || (!active && !gameFiles)) return orig(dirfd, path, flags, varargs);
      let p = cstr(path);
      if (!p.startsWith('/')) p = `${FS.cwd().replace(/\/$/, '')}/${p}`;
      const node = (active && p.startsWith('/user/')) || (gameFiles && p.startsWith('/game/')) ? lookup(p) : null;
      if (!node || !isAway(node)) return orig(dirfd, path, flags, varargs);
      const open = () => {
        gameOpens++;
        try { return orig(dirfd, path, flags, varargs); } finally { gameOpens--; settle(node); }
      };
      const wait = openForGame(node, flags);
      if (!wait) return open();
      node.vaultPin = (node.vaultPin || 0) + 1;
      return wait.then(open, (e) => { log(`page: ${e.message}`); return -ENOENT; }).finally(() => { node.vaultPin--; });
    });
    if (typeof origRead === 'function') {
      const read = new WebAssembly.Suspending((fd, iov, iovcnt, pnum) => {
        const stream = (active || gameFiles) && memory ? FS.streams[fd] : null, node = stream?.node;
        if (node?.vault !== 'lazy') return origRead(fd, iov, iovcnt, pnum);
        const words = new Uint32Array(memory.buffer);
        let length = 0;
        for (let i = 0; i < iovcnt >>> 0; i++) length += words[((iov >>> 0) + 8 * i + 4) >>> 2];
        const wait = readForGame(node, stream.position, length);
        return wait ? wait.then(() => origRead(fd, iov, iovcnt, pnum), (e) => { log(`page: ${node.vaultKey}: ${e.message}`); return EIO; })
          : origRead(fd, iov, iovcnt, pnum);
      });
      wasi.fd_read = read;
      if (env !== wasi && env.fd_read === origRead) env.fd_read = read;
    }
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
      if (active) settle(node);
      return r;
    };
    FS.open = (path, flags, mode) => {
      if (active && typeof path === 'string') {
        const node = lookup(path);
        if (isAway(node) && !gameOpens) {
          const f = typeof flags === 'string' ? FS.modeStringToFlags(flags) : flags;
          if (f & O_TRUNC) { release(node); node.vault = undefined; }
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
        const size = node.usedBytes, mtime = node.mtime, bytes = node.contents ?? new Uint8Array(0);
        const parts = await putParts(bytes, size, () => lookup(p) === node && node.mtime === mtime && node.usedBytes === size && node.contents === bytes);
        if (!parts) continue;                                       // changed while it was stored: the next round
        await put(p, { size, mtime, ...parts });
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
      // The parts of the versions just replaced, and of the files just removed, when no other record names them.
      if (stored || removed) await dropUnnamedParts(new Set((await records()).map((r) => r.partsId).filter(Boolean)));
      return { stored, removed };
    });
  }

  // For the page: the file's bytes (a copy the caller may change) or a Blob (for a download or an upload), without
  // bringing a placeholder's contents into the engine's memory. A placeholder whose record is gone fails: its node has
  // no contents, and a Download once made an empty file of it.
  // A game file (gameFile) as a placeholder of the engine's file system: its size, and source() for its Blob when the
  // game or the page opens it. Not one of the vault's records: no round stores, moves or removes it.
  function gameFile(path, size, source) {
    FS.mkdirTree(path.slice(0, path.lastIndexOf('/')));
    FS.writeFile(path, new Uint8Array(0));
    const node = lookup(path);
    node.usedBytes = size;
    node.vaultSource = source;
    node.vaultSize = size;
    evict(node);
    gameFiles++;
  }
  async function record(node, path) {
    const rec = await serial(() => get(node.vaultKey));
    if (!rec) {
      const name = path.slice(path.lastIndexOf('/') + 1);
      throw Object.assign(new Error(`the browser's storage no longer has ${name}`), { key: 'the browser\'s storage no longer has {name}', vars: { name } });
    }

    return rec;
  }
  async function bytes(path) {
    const node = lookup(path);
    if (isAway(node) && node.vaultSource) return new Uint8Array(await (await node.vaultSource()).arrayBuffer());
    if (isAway(node)) return new Uint8Array(await (await blobOf(await record(node, path))).arrayBuffer());
    return FS.readFile(path);
  }
  async function view(path) {
    const node = lookup(path);
    if (isAway(node)) return bytes(path);
    const n = node ?? FS.lookupPath(path).node;
    return n.contents instanceof Uint8Array ? n.contents.subarray(0, n.usedBytes ?? n.contents.length) : FS.readFile(path);
  }
  async function blob(path) {
    const node = lookup(path);
    if (isAway(node) && node.vaultSource) return node.vaultSource();
    if (isAway(node)) return blobOf(await record(node, path));
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
    openForGame, readForGame, settle, adopt, adoptStream, copy, dropUnnamedParts, gameFile,
    get gameFiles() { return gameFiles; },
    held: () => [...managed].filter((n) => n.vault === 'in').length,
  };
}
