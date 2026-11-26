// Tests of the Saved games logic (web/saves.js): which files are saves, the check of an upload, its name.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { checkSave, copiesOf, errorDetail, ownerOfCopy, removeOrphanCopies, fileView, isWorkFile, listSaves, lowOnSpace, saveName, savesSignature, skipWorkFiles, spaceText, zipParts } from '../saves.js';

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
    unlink(p) { if (!entries[p]) throw new Error('ENOENT'); delete entries[p]; },
  };
}

test('checkSave: a save container passes; other files are named for what they are', () => {
  assert.equal(checkSave(save()), null);
  assert.equal(checkSave(save(40)), null);
  assert.match(checkSave(new Uint8Array(100)), /too short.*: 1 KB, where a saved game has at least 262 KB/);
  assert.match(checkSave(new Uint8Array(150000)), /: 146 KB, /);
  assert.match(checkSave(new Uint8Array(0)), /the file is empty \(0 bytes\)/);
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

test('fileView gives the file without a copy, or FS.readFile without a memory file system', () => {
  const contents = new Uint8Array([1, 2, 3, 4, 0, 0, 0, 0]);
  const FS = { lookupPath: () => ({ node: { contents, usedBytes: 4 } }), readFile: () => new Uint8Array([9]) };
  const v = fileView(FS, '/user/a.sav');
  assert.deepEqual([...v], [1, 2, 3, 4]);
  assert.equal(v.buffer, contents.buffer);               // the same memory: no copy
  assert.deepEqual([...fileView({ readFile: () => new Uint8Array([9]) }, '/x')], [9]);
});

test('zipParts: Download all in parts that a zip can hold (4 GB), in order', () => {
  const GB = 2 ** 30, f = (name, size) => ({ name, size });
  const six = [1, 2, 3, 4, 5, 6].map((i) => f(`Career ${i}.sav`, 690 * 2 ** 20));     // 4.04 GB: one zip refused it
  const parts = zipParts(six);
  assert.deepEqual(parts.map((p) => p.map((x) => x.name.slice(7, 8)).join('')), ['12345', '6'], 'five of 690 MB fit under 3.5 GB');
  assert.ok(parts.every((p) => p.reduce((a, x) => a + x.size, 0) <= 3.5 * GB));
  assert.deepEqual(zipParts([f('a', 10), f('b', 20)]).length, 1, 'small folders stay one zip');
  assert.deepEqual(zipParts([f('a', 3 * GB), f('huge', 3.8 * GB), f('c', 1 * GB)]).map((p) => p.map((x) => x.name)), [['a'], ['huge'], ['c']],
    'a file larger than a part goes alone');
  assert.deepEqual(zipParts([]), []);
});

test('space: what the saves take, the room left, and when it is low', () => {
  const GB = 2 ** 30, MB = 2 ** 20;
  assert.equal(spaceText({ usage: 3.3 * GB, quota: 15 * GB }, 2.76 * GB, 4), '4 saves, 2.8 GB · this browser allows about 11.7 GB more');
  assert.equal(spaceText(null, 46 * MB, 1), '1 save, 46 MB', 'a browser that does not tell');
  assert.equal(spaceText({ usage: 1 * GB, quota: 1.2 * GB }, 690 * MB, 1), '1 save, 690 MB · this browser allows about 205 MB more');
  assert.equal(lowOnSpace({ usage: 1 * GB, quota: 1.2 * GB }, 690 * MB), true, 'less than twice the largest save: the next save may not fit');
  assert.equal(lowOnSpace({ usage: 3.3 * GB, quota: 15 * GB }, 690 * MB), false);
  assert.equal(lowOnSpace({ usage: 9.9 * GB, quota: 10 * GB }, 1 * MB), true, 'under 300 MB left');
  assert.equal(lowOnSpace(null, 690 * MB), false, 'not told: no warning');
});

test('errorDetail: an error with no message still says what it was', () => {
  assert.equal(errorDetail(new Error('the disk is full')), 'the disk is full');
  const dom = new Error('');
  dom.name = 'UnknownError';
  assert.equal(errorDetail(dom), 'UnknownError');
  assert.equal(errorDetail(Object.assign(new Error('Failed to fetch'), { name: 'TypeError' })), 'TypeError: Failed to fetch');
  class ErrnoError { constructor(errno) { this.name = 'ErrnoError'; this.errno = errno; } }
  assert.equal(errorDetail(new ErrnoError(28)), 'ErrnoError: errno 28');
  assert.equal(errorDetail(new Error('')), 'Error');
  assert.equal(errorDetail(undefined), 'undefined');
});

test('ownerOfCopy: the save an older copy belongs to (the engine\'s .1 to .3, the editor\'s "before edit")', () => {
  assert.equal(ownerOfCopy('Leeds.1.sav'), 'Leeds.sav');
  assert.equal(ownerOfCopy('Chelsea 2014-07-12.3.sav'), 'Chelsea 2014-07-12.sav');
  assert.equal(ownerOfCopy('Leeds (before edit).sav'), 'Leeds.sav');
  assert.equal(ownerOfCopy('Leeds.sav'), null, 'not a name the engine or the editor gives a copy');
  assert.equal(ownerOfCopy('notes.txt'), null);
});

const withCopies = (extra = {}) => fakeFS({
  '/user/Chelsea 2014-07-12.sav': { size: 67, mtime: new Date(9) },
  '/user/backups/Chelsea 2014-07-12.1.sav': { size: 66, mtime: new Date(8) },
  '/user/backups/chelsea 2014-07-12 (before edit).sav': { size: 66, mtime: new Date(8) },
  '/user/backups/Chelsea 2014-06-30.1.sav': { size: 60, mtime: new Date(5) },
  '/user/backups/Chelsea 2014-06-30.2.sav': { size: 60, mtime: new Date(4) },
  '/user/backups/Arsenal.1.sav': { size: 50, mtime: new Date(3) },
  '/user/backups/a copy the player named.sav': { size: 40, mtime: new Date(2) },
  ...extra,
});

test('copiesOf: the older copies of one save, any case', () => {
  const FS = withCopies();
  assert.deepEqual(copiesOf(FS, 'Chelsea 2014-07-12.sav').map((f) => f.name).sort(), ['Chelsea 2014-07-12.1.sav', 'chelsea 2014-07-12 (before edit).sav']);
  assert.deepEqual(copiesOf(FS, 'Wolves.sav'), []);
});

test('removeOrphanCopies: the copies of saves that are gone go; the others, and copies of saves in the cloud, stay', () => {
  const FS = withCopies();
  const r = removeOrphanCopies(FS, { inCloud: (name) => name === 'Arsenal.sav' });
  assert.deepEqual([r.n, r.bytes], [2, 120]);
  assert.deepEqual(listSaves(FS).backups.map((f) => f.name).sort(),
    ['Arsenal.1.sav', 'Chelsea 2014-07-12.1.sav', 'a copy the player named.sav', 'chelsea 2014-07-12 (before edit).sav']);
});

test('removeOrphanCopies: nothing while a save is in progress (the old save is a copy, the new one not named yet)', () => {
  const FS = withCopies({ '/user/temp_save_file.sav': { size: 67, mtime: new Date(10) } });
  assert.deepEqual(removeOrphanCopies(FS).n, 0);
  assert.equal(listSaves(FS).backups.length, 6);
});
