// Tests of the Saved games logic (web/saves.js): which files are saves, the check of an upload, its name.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { checkSave, isWorkFile, listSaves, saveName, savesSignature, skipWorkFiles } from '../saves.js';

// A save container as tools/save/savfile.py reads it: version, count, the entries, then the parts.
function save(parts = 1, extra = 16) {
  const header = 0x416ec, bytes = new Uint8Array(header + extra), dv = new DataView(bytes.buffer);
  dv.setInt32(0, 2, true);
  dv.setInt32(8, parts, true);
  for (let k = 0; k < parts; k++) dv.setUint32(12 + k * 0x10c, header, true);
  return bytes;
}

// The part of Emscripten's FS that saves.js uses.
function fakeFS(entries) {
  const FILE = 0o100644, DIR = 0o040755;
  return {
    readdir(dir) {
      const names = new Set(['.', '..']);
      for (const p of Object.keys(entries)) if (p.startsWith(`${dir}/`)) names.add(p.slice(dir.length + 1).split('/')[0]);
      if (names.size === 2 && !Object.keys(entries).some((p) => p.startsWith(`${dir}/`))) throw new Error('ENOENT');
      return [...names];
    },
    stat(p) {
      if (entries[p]) return { mode: FILE, size: entries[p].size, mtime: entries[p].mtime };
      if (Object.keys(entries).some((k) => k.startsWith(`${p}/`))) return { mode: DIR, size: 0, mtime: new Date(0) };
      throw new Error('ENOENT');
    },
    isFile: (mode) => (mode & 0o170000) === 0o100000,
  };
}

test('checkSave: a save container passes; other files are named for what they are', () => {
  assert.equal(checkSave(save()), null);
  assert.equal(checkSave(save(40)), null);
  assert.match(checkSave(new Uint8Array(100)), /too short/);
  const notASave = save();
  new DataView(notASave.buffer).setInt32(0, 7, true);
  assert.match(checkSave(notASave), /not a saved game/);
  const damaged = save();
  new DataView(damaged.buffer).setUint32(12, 0x7fffffff, true);
  assert.match(checkSave(damaged), /damaged/);
  const empty = save();
  new DataView(empty.buffer).setInt32(8, 0, true);
  assert.match(checkSave(empty), /not a saved game/);
});

test('saveName: a .sav keeps its name, a taken name gets a number, other files are refused', () => {
  const taken = new Set(['arsenal 2001-08-12.sav', 'arsenal 2001-08-12 (2).sav']);
  assert.equal(saveName('Leeds 2002-01-01.sav', taken), 'Leeds 2002-01-01.sav');
  assert.equal(saveName('Arsenal 2001-08-12.sav', taken), 'Arsenal 2001-08-12 (3).sav');
  assert.equal(saveName('C:\\fakepath\\Wolves.SAV', new Set()), 'Wolves.SAV');
  assert.equal(saveName('a:b?.sav', new Set()), 'a_b_.sav');
  assert.equal(saveName('temp_save_file.sav', new Set()), 'uploaded save.sav', 'never the name of a save in progress');
  assert.equal(saveName('notes.txt', new Set()), null);
  assert.equal(saveName('.sav', new Set()), null);
});

test('listSaves: the saves and the older copies, newest first, without a save in progress', () => {
  const FS = fakeFS({
    '/user/settings.txt': { size: 10, mtime: new Date(1) },
    '/user/Arsenal 2001-08-12.sav': { size: 9000000, mtime: new Date(2000) },
    '/user/Leeds.sav': { size: 8000000, mtime: new Date(3000) },
    '/user/temp_save_file.sav': { size: 100, mtime: new Date(4000) },
    '/user/backups/Leeds.1.sav': { size: 7000000, mtime: new Date(1000) },
  });
  const { saves, backups } = listSaves(FS);
  assert.deepEqual(saves.map((f) => f.name), ['Leeds.sav', 'Arsenal 2001-08-12.sav']);
  assert.deepEqual(backups.map((f) => f.name), ['Leeds.1.sav']);
  const before = savesSignature(FS);
  assert.equal(savesSignature(FS), before, 'the same files: the same signature');
  assert.notEqual(savesSignature(fakeFS({ '/user/Leeds.sav': { size: 8000001, mtime: new Date(3000) } })), before);
});

test('the working files (*.tmp) stay out of IndexedDB, and earlier copies there go', () => {
  const calls = [];
  const set = (type, paths) => ({ type, entries: Object.fromEntries(paths.map((p) => [p, { timestamp: new Date(1) }])) });
  const IDBFS = {
    getLocalSet: (mount, cb) => cb(null, set('local', ['/user/a.sav', '/user/staff history.tmp', '/user/backups/a.sav'])),
    getRemoteSet: (mount, cb) => cb(null, set('remote', ['/user/old.TMP', '/user/d.sav'])),
    reconcile: (src, dst, cb) => { calls.push([src.type, Object.keys(src.entries).sort(), dst.type, Object.keys(dst.entries).sort()]); cb(null); },
  };
  assert.equal(skipWorkFiles(IDBFS), true);
  let done = 0;
  IDBFS.syncfs({}, false, (err) => { assert.equal(err, null); done++; });
  IDBFS.syncfs({}, true, (err) => { assert.equal(err, null); done++; });
  assert.equal(done, 2);
  // To IndexedDB: the local set without the .tmp file; the remote set keeps old.TMP, so reconcile removes it there.
  assert.deepEqual(calls[0], ['local', ['/user/a.sav', '/user/backups/a.sav'], 'remote', ['/user/d.sav', '/user/old.TMP']]);
  // From IndexedDB: no .tmp file comes back.
  assert.deepEqual(calls[1], ['remote', ['/user/d.sav'], 'local', ['/user/a.sav', '/user/backups/a.sav']]);
  assert.equal(isWorkFile('/user/x.tmp'), true);
  assert.equal(isWorkFile('/user/x.sav'), false);
  assert.equal(skipWorkFiles({ syncfs() {} }), false);
});
