// The Saved games panel (web/README.md): the saves in the user folder, to download as files and to upload back.
//
// The saves stay where the game writes them: the user folder /user, which loader.js keeps in IndexedDB. localStorage
// is not a place for them: it holds a few MB of text per site, and a save holds the whole game world.
//
// The game writes *.sav files in the user folder (src/recomp_rt/win/files.c; the option save_folder can name another
// folder, which the browser build does not use). A save in progress is temp_save_file.sav, renamed when it is done;
// older copies are in backups/ (option save_backups).
import { ownable, toStoredBlob } from './disk.js';
import { zipOfBlobs } from './zip.js';
import { sortUpload } from './upload.js';
import { TACTICS_DIR, listTactics, tacticName } from './tactics.js';
import { isAway } from './vault.js';

export const SAVE_DIR = '/user';
const TEMP = 'temp_save_file.sav';
const HEADER = 0x416ec, ENTRY = 0x10c, MAX_ENTRIES = 1000;     // the save container (tools/save/savfile.py)

const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const mb = (n) => `${(n / 1048576).toFixed(n < 10485760 ? 1 : 0)} MB`;
const when = (d) => d.toLocaleString(undefined, { dateStyle: 'medium', timeStyle: 'short' });
// The date of a list's row: the day over the time, never broken (Saved games, Tactics, Data sets). In the page's
// language (the lang of the page, which i18n.js sets), or the browser's own format when the page is in English.
const dateLocale = () => {
  const l = typeof document === 'object' ? document.documentElement?.lang : '';
  return l && l !== 'en' ? l : undefined;
};
export const whenCell = (d) => {
  const t = new Date(d), locale = dateLocale();
  return `<td class="when">${esc(t.toLocaleDateString(locale, { dateStyle: 'medium' }))}<br><span class="small">${esc(t.toLocaleTimeString(locale, { timeStyle: 'short' }))}</span></td>`;
};
const size = (n) => (n >= 2 ** 30 ? `${(n / 2 ** 30).toFixed(1)} GB` : `${Math.max(1, Math.round(n / 2 ** 20))} MB`);

// The text a player reads, in the page's language. This module never imports i18n.js (the server imports this one): the
// page passes its t and tn (loader.js); without them it is English. A text is { key, vars }: key is the English with
// {placeholders}, written key: '…' so the check of the tables finds it here (web/test/i18n-strings.mjs); a var may be a
// text too (a reason inside a sentence); { key, other, n, vars } is a count with its noun (tn's one and other forms).
const fill = (s, v) => s.replace(/\{(\w+)\}/g, (m, k) => (v && k in v ? String(v[k]) : m));
const englishT = (s, v) => fill(s, v);
const englishTn = (n, forms, v = {}) => fill(n === 1 ? forms.one : forms.other, { ...v, n });
function textOf(m, t = englishT, tn = englishTn) {
  if (m === null || typeof m !== 'object') return String(m ?? '');
  const vars = m.vars && Object.fromEntries(Object.entries(m.vars).map(([k, v]) => [k, v && typeof v === 'object' && 'key' in v ? textOf(v, t, tn) : v]));
  return 'other' in m ? tn(m.n, { one: m.key, other: m.other }, vars) : t(m.key, vars);
}
// An error as a text: its key and vars (zip.js, upload.js, dataset.js), else its English message as it is. An error
// with an empty message (a DOMException of the browser's storage; the engine's FS.ErrnoError has none) gives its name
// and errno: an upload on Windows was refused with "it was not stored: " and nothing after it (24 November 2026).
export function errorDetail(e) {
  if (e === null || typeof e !== 'object') return String(e);
  const message = typeof e.message === 'string' ? e.message.trim() : '';
  const name = e.name && e.name !== 'Error' ? String(e.name) : '';
  const errno = Number.isInteger(e.errno) ? `errno ${e.errno}` : '';
  const text = [name, message, errno].filter(Boolean).join(': ');
  return text || e.constructor?.name || 'an error with no message';
}
const errText = (e) => ({ key: e?.key ?? (typeof e?.message === 'string' && e.message.trim() ? e.message : errorDetail(e)),
  vars: e?.vars });
// A reason a file is refused: its English (message), key and vars.
const reason = (m) => ({ message: textOf(m), key: m.key, vars: m.vars });

// Download all in parts. A zip holds 4 GB (zip.js), and six saves of 690 MB pass that: one zip was refused, with no way
// to take fewer. The files in their order, in groups of at most limit bytes; a file larger than that goes alone.
export const PART_LIMIT = 3.5 * 2 ** 30;
export function zipParts(files, limit = PART_LIMIT) {
  const parts = [];
  let sum = 0;
  for (const f of files) {
    if (!parts.length || sum + f.size > limit) { parts.push([]); sum = 0; }
    parts.at(-1).push(f);
    sum += f.size;
  }
  return parts;
}

// What the saves take and the room left. estimate: navigator.storage.estimate()'s answer ({ usage, quota }: what the
// browser allows this site, which a nearly full disk can still refuse), or null when the browser does not tell.
const roomLeft = (estimate) => (estimate && estimate.quota > 0 ? Math.max(0, estimate.quota - (estimate.usage ?? 0)) : null);
// t, tn: the page's translation (i18n.js), else English.
export function spaceText(estimate, bytes, count, t = englishT, tn = englishTn) {
  const head = { key: '{n} save, {size}', other: '{n} saves, {size}', n: count, vars: { size: size(bytes) } }, room = roomLeft(estimate);
  return textOf(room === null ? head : { key: '{saves} · this browser allows about {room} more', vars: { saves: head, room: size(room) } }, t, tn);
}
// Low: less room than two saves of the largest size (a save is written beside the one it replaces), or than 300 MB.
export function lowOnSpace(estimate, biggest) {
  const room = roomLeft(estimate);
  return room !== null && room < Math.max(2 * biggest, 300 * 2 ** 20);
}
// The saves and older copies of the user folder against the browser's room: { count, bytes, biggest, estimate, low }.
export async function spaceState(FS) {
  const { saves, backups } = listSaves(FS), all = [...saves, ...backups];
  let estimate = null;
  try { estimate = await navigator.storage.estimate(); } catch { /* not told */ }
  const biggest = Math.max(0, ...all.map((f) => f.size));
  return { count: saves.length, bytes: all.reduce((a, f) => a + f.size, 0), biggest, estimate, low: lowOnSpace(estimate, biggest) };
}

function files(FS, dir) {
  let names;
  try { names = FS.readdir(dir); } catch { return []; }
  const out = [];
  for (const name of names) {
    if (!/\.sav$/i.test(name) || name.toLowerCase() === TEMP) continue;
    try {
      const st = FS.stat(`${dir}/${name}`);
      if (FS.isFile(st.mode)) out.push({ name, path: `${dir}/${name}`, size: st.size, mtime: new Date(st.mtime) });
    } catch { /* gone meanwhile */ }
  }
  return out.sort((a, b) => b.mtime - a.mtime);
}

export function listSaves(FS) { return { saves: files(FS, SAVE_DIR), backups: files(FS, `${SAVE_DIR}/backups`) }; }

// The game's working files: the parts of the game in play (*.tmp, written at a load and at each save, tens of MB in a
// big game) live only while the game runs, and a save packs them into its .sav. IDBFS would copy each changed one into
// IndexedDB at every sync, all of them in one transaction that holds the copies in memory until it ends. That also
// ran during a load, while the game wrote them. A path of the user folder that the sync leaves out:
export const isWorkFile = (path) => /\.tmp$/i.test(path);

// Makes IDBFS (Emscripten's IndexedDB file system) leave the working files out: a sync to IndexedDB does not store
// them and removes the ones an earlier page stored; a read from IndexedDB does not bring them back. The rest is
// IDBFS.syncfs as Emscripten has it (getLocalSet, getRemoteSet, reconcile). An IDBFS without those parts is left as it
// is. Returns true when the sync was changed.
// skip(path): more paths to leave out (the vault's files, vault.js); called at each sync.
export function skipWorkFiles(IDBFS, skip = () => false) {
  if (!IDBFS || typeof IDBFS.getLocalSet !== 'function' || typeof IDBFS.getRemoteSet !== 'function'
    || typeof IDBFS.reconcile !== 'function') return false;
  const drop = (set) => { for (const path of Object.keys(set.entries)) if (isWorkFile(path) || skip(path)) delete set.entries[path]; };
  IDBFS.syncfs = (mount, populate, callback) => {
    IDBFS.getLocalSet(mount, (err, local) => {
      if (err) return callback(err);
      IDBFS.getRemoteSet(mount, (err2, remote) => {
        if (err2) return callback(err2);
        drop(local);                    // never stored, never removed here
        if (populate) drop(remote);     // not read back; a sync to IndexedDB removes them there (not in the local set)
        IDBFS.reconcile(populate ? remote : local, populate ? local : remote, callback);
      });
    });
  };
  return true;
}

// A short text that changes whenever a save file is added, changed or removed (the watcher of loader.js).
export function savesSignature(FS) {
  const { saves, backups } = listSaves(FS);
  return [...saves, ...backups].map((f) => `${f.path}:${f.size}:${+f.mtime}`).join('|');
}

// null when bytes look like a saved game of CM 01/02 (its container header), else why not. bytes may be only the start
// of the file (at least its header, SAVE_HEADER bytes) when total gives the whole length: the server checks so.
export const SAVE_HEADER = HEADER;
// A file too short says its size: an empty one (0 bytes) is a copy that never got the save, such as a Download of a
// save whose stored copy another tab of the game had removed (vault.js).
// saveProblem: the same as { message (the English), key, vars }, for the page's translation; checkSave: the English.
export function saveProblem(bytes, total = bytes.length) {
  if (total === 0) return reason({ key: 'the file is empty (0 bytes): the copy or download that made it did not get the saved game' });
  if (total < HEADER || bytes.length < HEADER) {
    return reason({ key: 'it is too short to be a saved game of CM 01/02: {size} KB, where a saved game has at least {least} KB (most have tens of MB)',
      vars: { size: Math.max(1, Math.round(total / 1024)), least: Math.round(HEADER / 1024) } });
  }
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const version = dv.getInt32(0, true), count = dv.getInt32(8, true);
  if (![1, 2, 3, 4].includes(version) || count < 1 || count > MAX_ENTRIES) return reason({ key: 'it is not a saved game of CM 01/02' });
  for (let k = 0; k < count; k++)
    if (dv.getUint32(12 + k * ENTRY, true) > total) return reason({ key: 'the saved game is damaged: a part starts after its end' });
  return null;
}
export function checkSave(bytes, total = bytes.length) { return saveProblem(bytes, total)?.message ?? null; }

// A save from outside (an uploaded File, a cloud download: a Blob) into the save folder under name. It is checked by its
// header alone. With the vault it goes to the browser's storage as it is (vault.adopt) and is never in the page's memory:
// the page once read the whole file and copied it into the user folder, 3.4 GB in the tab for a save of 690 MB (15
// November 2026). Without the vault it is a file in memory, as before. Resolves to null, or to why it is not a save
// (storeSaveProblem: as saveProblem gives it, for the page's translation).
export async function storeSave(FS, name, blob, opts) { return (await storeSaveProblem(FS, name, blob, opts))?.message ?? null; }
export async function storeSaveProblem(FS, name, blob, { mtime = Date.now(), owned = false } = {}) {
  const why = saveProblem(new Uint8Array(await blob.slice(0, HEADER).arrayBuffer()), blob.size);
  if (why) return why;
  const path = `${SAVE_DIR}/${name}`;
  // The vault keeps the Blob's bytes, not a reference to a file they came from (disk.js, ownable: an uploaded save, in
  // Chromium, was a reference to the player's file, and broke when that file was deleted). owned: made for it already.
  if (vaultRef?.active) await vaultRef.adopt(path, owned ? blob : await ownable(blob), mtime);
  else {
    FS.writeFile(path, new Uint8Array(await blob.arrayBuffer()), { canOwn: true });   // the file system takes the array, no copy
    try { FS.utime(path, mtime, mtime); } catch { /* an older file system: the time of the copy */ }
  }
  return null;
}

// A save that comes as a stream (a cloud download, unpacked as it comes) into the save folder under name. With the
// vault it goes in parts as it comes (vault.adoptStream): never whole in the page's memory, which an iPhone could not
// hold for a 446 MB save (18 November 2026). Without it, as storeSave. Resolves to why it was refused, or null.
export async function storeSaveStream(FS, name, stream, { mtime = Date.now() } = {}) {
  const path = `${SAVE_DIR}/${name}`;
  if (vaultRef?.active && typeof vaultRef.adoptStream === 'function')
    return vaultRef.adoptStream(path, stream, mtime, (head, size) => checkSave(head.subarray(0, Math.min(head.length, HEADER)), size));
  const kept = await toStoredBlob(stream);
  try { return await storeSave(FS, name, kept.blob, { mtime, owned: kept.owned }); } finally { await kept.done(); }
}

// A copy of a file of the user folder under another name (the save editor's older copy). A file of the vault is copied
// there as a record (vault.copy: the browser copies it, not the page); any other is read and written. The file keeps its
// name all the while.
export async function copyFile(FS, from, to) {
  if (vaultRef?.active && await vaultRef.copy(from, to)) return;
  FS.writeFile(to, await readBytes(FS, from));
}

// The name an uploaded file gets in the save folder, or null when it is not a .sav file.
export function saveName(fileName, taken) {
  let base = String(fileName).split(/[\\/]/).pop().replace(/[\u0000-\u001f:*?"<>|]/g, '_').trim();
  if (!/\.sav$/i.test(base) || base.length <= 4) return null;
  if (base.toLowerCase() === TEMP) base = 'uploaded save.sav';
  if (!taken.has(base.toLowerCase())) return base;
  const stem = base.slice(0, -4);
  for (let i = 2; ; i++) if (!taken.has(`${stem} (${i}).sav`.toLowerCase())) return `${stem} (${i}).sav`;
}

// The bytes of a file of the engine's file system without a copy: a view of the array where the memory file system keeps
// the file (it can be larger than the file). FS.readFile would first make a copy of the whole file, 75 MB for a big save,
// at a time when the page's memory can be short. Use the view at once (new Blob copies it); a later write can change
// it. A file system without such an array gives FS.readFile's copy.
export function fileView(FS, path) {
  try {
    const node = FS.lookupPath(path).node, c = node?.contents;
    if (c instanceof Uint8Array) return c.subarray(0, node.usedBytes ?? c.length);
  } catch { /* below */ }
  return FS.readFile(path);
}

// The vault (vault.js) when it is on: saves, older copies and data sets are then placeholders in the user folder, and
// the page reads them with these. Without it they read from the engine's memory, as before.
let vaultRef = null;
export function useVault(vault) { vaultRef = vault; }
// The file's bytes, a copy that the caller may change (the editors).
export async function readBytes(FS, path) { return vaultRef ? vaultRef.bytes(path) : FS.readFile(path); }
// The file's bytes for reading only (a zip): no copy of a file in memory.
export async function readView(FS, path) { return vaultRef ? vaultRef.view(path) : fileView(FS, path); }
// The file as a Blob, for a download or an upload: a placeholder's comes from the browser's storage, not memory.
export async function readBlob(FS, path) { return vaultRef ? vaultRef.blob(path) : new Blob([fileView(FS, path)]); }
// The same at once for a file in memory (a Blob made now, so it matches a stat taken just before), a promise only for a
// placeholder, whose contents do not change while it waits.
// A save the game has just written (in memory, not yet in the vault): the vault stores it now, and the reader then
// takes the stored copy. The cloud upload once read it at once, while the vault was storing it: a Blob of the whole
// file made from memory, a third copy of the save in the tab beside the file and the vault's (an iPhone closed the tab
// after a save, 21 November 2026). Without the vault, or when the store fails, the reader takes it from memory as before.
export async function storedFirst(FS, path) {
  if (!vaultRef?.active) return;
  let node = null;
  try { node = FS.lookupPath(path).node; } catch { return; }
  if (isAway(node)) return;
  try { await vaultRef.persist({ force: true }); } catch { /* it stays in memory: read from there */ }
}

export function readBlobNow(FS, path) {
  let node = null;
  try { node = FS.lookupPath(path).node; } catch { /* not this FS */ }
  return vaultRef && isAway(node) ? vaultRef.blob(path) : new Blob([fileView(FS, path)]);
}

// Every file to keep, as the save folder holds them: the saves, backups/ (older copies), tactics/ (the game's tactics,
// tactics.js) and Databases/ (data sets).
function allFiles(FS) {
  const out = [];
  const walk = (dir, rel) => {
    let names = [];
    try { names = FS.readdir(dir).filter((n) => n !== '.' && n !== '..'); } catch { return; }
    for (const n of names) {
      const p = `${dir}/${n}`, r = rel ? `${rel}/${n}` : n;
      let st;
      try { st = FS.stat(p); } catch { continue; }
      if (FS.isDir(st.mode)) walk(p, r);
      else out.push({ path: p, name: r, size: st.size });
    }
  };
  for (const f of listSaves(FS).saves) out.push({ path: f.path, name: f.name, size: f.size });
  walk(`${SAVE_DIR}/backups`, 'backups');
  walk(`${SAVE_DIR}/tactics`, 'tactics');
  walk(`${SAVE_DIR}/Databases`, 'Databases');
  return out;
}

const saveLink = (blob, name) => {
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = name;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 120000);
};

async function download(FS, f) { saveLink(new Blob([await readBlob(FS, f.path)], { type: 'application/octet-stream' }), f.name); }

const CLOUD = { cloud: { key: 'In the cloud' }, up: { key: 'Saving…' }, later: { key: 'Goes up soon' }, here: { key: 'Only here' },
  waiting: { key: 'Not yet' }, big: { key: 'Too large for the cloud' } };

// What the browser may do to the saves on its own. kept: the answer of navigator.storage.persist (loader.js): 'kept'
// when the browser agreed to keep the site's storage when the disk runs low. It still deletes it when the player
// clears browsing data, when a private window closes, and Safari (iPhone, iPad, Mac) deletes a site's storage after 7
// days without a visit, unless the page is on the home screen. t: the page's translation (i18n.js), else English.
let storageKept = 'unknown';
export function setStorageKept(kept) { storageKept = kept; }
export function storageWarning(cloudOn, t = englishT) {
  if (cloudOn) {
    return textOf({ key: 'The browser can delete the saves here by itself, without asking: when the disk runs low, when browsing data is cleared, when a private window closes, and in Safari (iPhone, iPad, Mac) after 7 days without a visit, unless the page is on the home screen. The saves marked In the cloud then come back at the next start; download the others you want to keep.' }, t);
  }
  if (storageKept === 'kept') {
    return textOf({ key: 'This browser agreed to keep these saves when the disk runs low, but it still deletes them by itself, without asking, when browsing data is cleared, when a private window closes, and in Safari (iPhone, iPad, Mac) after 7 days without a visit, unless the page is on the home screen. Download the saves you want to keep, or use Cloud Saves.' }, t);
  }
  return textOf({ key: 'The browser can delete these saves by itself, without asking: when the disk runs low, when browsing data is cleared, when a private window closes, and in Safari (iPhone, iPad, Mac) after 7 days without a visit, unless the page is on the home screen. Download the saves you want to keep, or use Cloud Saves.' }, t);
}

// Wires the panel of index.html to the engine's file system. sync() writes the user folder to IndexedDB now. cloud():
// the cloud sync of account.js, or null when the account has no cloud saves.
// promo(): a line about Cloud Saves for a player without them, or null; openPromo() shows them. edit(name): opens the
// save editor (editor.js) on a save of the folder.
// report(): Send to the developer (report.js), or null: a message of the panel that says something went wrong offers it,
// with the files of an upload that were refused. play: Play (play.js), or null for an engine that cannot open a save
// from the page.
// t, tn, onLanguage: the page's translation (i18n.js; this module does not import it, the server imports this one).
// Without them the panel is English. The report and the log get the English.
export function mountSavesPanel({ FS, sync, log, cloud = () => null, promo = () => null, openPromo = () => {}, edit = null, report = () => null, play = null,
  t = englishT, tn = englishTn, onLanguage = () => {} }) {
  const dialog = document.getElementById('saves'), list = document.getElementById('saves-list');
  const note = document.getElementById('saves-note'), input = document.getElementById('saves-file');
  const tx = (m) => textOf(m, t, tn);
  const lines = (m) => (Array.isArray(m) ? m : [m]).filter(Boolean);
  // m: a text ({ key, vars }), or a list of them, one a line. Shown in the page's language; the report gets the English.
  let said = null;
  const say = (m, bad = false, files = []) => {
    said = { m, bad, files };
    note.textContent = lines(m).map(tx).join('\n');
    note.className = bad ? 'small bad' : 'small';
    const r = bad || files.length ? report() : null;
    if (!r) return;
    const b = document.createElement('button');
    b.className = 'link';
    b.id = 'saves-report';
    b.textContent = tx({ key: 'Send to the developer' });
    b.onclick = () => { dialog.close(); r.open({ message: lines(m).map((x) => textOf(x)).join('\n'), where: 'Saved games', files }); };
    note.append(' ', b);
  };
  let shown = { saves: [], backups: [] }, watched = null;

  function render() {
    shown = listSaves(FS);
    const c = cloud();
    document.getElementById('saves-where').textContent = tx(c ? { key: 'Your saves are in this browser and in the cloud.' } : { key: 'Your saves stay in this browser.' });
    document.getElementById('saves-risk').textContent = storageWarning(Boolean(c), t);
    const p = promo(), box = document.getElementById('saves-promo');
    box.hidden = !p;
    if (p) { box.firstChild.textContent = `${p} `; box.querySelector('button').onclick = openPromo; }
    const state = (f) => {
      const st = c.status(f.name, f);
      return `<td class="cloud ${st}">${esc(tx(CLOUD[st]))}${st === 'cloud' ? ` <button class="link" data-remove="${esc(f.name)}" title="${esc(tx({ key: 'Remove from the cloud; it stays here' }))}">${esc(tx({ key: 'Remove' }))}</button>` : ''}</td>`;
    };
    const row = (f, i, kind) => `<tr><td>${esc(f.name)}</td><td class="num">${mb(f.size)}</td>${whenCell(f.mtime)}
      ${c ? (kind === 'save' ? state(f) : '<td></td>') : ''}<td class="acts"><div class="btns">${play?.can() && kind === 'save' ? `<button class="primary" data-play="${i}" title="${esc(tx({ key: 'Load this save in the game' }))}">${esc(tx({ key: 'Play' }))}</button>` : ''}${edit && kind === 'save' ? `<button data-edit="${i}">${esc(tx({ key: 'Edit' }))}</button>` : ''}<button data-kind="${kind}" data-i="${i}">${esc(tx({ key: 'Download' }))}</button><button class="link danger" data-delete="${kind}" data-i="${i}">${esc(tx({ key: 'Delete' }))}</button></div></td></tr>`;
    list.innerHTML = shown.saves.length
      ? `<table><tr><th>${esc(tx({ key: 'Save' }))}</th><th class="num">${esc(tx({ key: 'Size' }))}</th><th>${esc(tx({ key: 'Saved' }))}</th>${c ? `<th>${esc(tx({ key: 'Cloud' }))}</th>` : ''}<th></th></tr>${shown.saves.map((f, i) => row(f, i, 'save')).join('')}</table>`
      : `<p class="small">${esc(tx({ key: 'No saved games yet. Save in the game (Game Options, Save Game), or upload one.' }))}</p>`;
    // The older copies with what they take: the game keeps three of each save, 2 GB for one save of 690 MB.
    if (shown.backups.length) list.innerHTML += `<details><summary>${esc(tx({ key: 'Older copies ({n}, {size})', vars: { n: shown.backups.length, size: size(shown.backups.reduce((a, f) => a + f.size, 0)) } }))}</summary>
      <p class="small">${esc(tx({ key: 'The game keeps the last three copies of each save here, in case a save goes wrong.' }))}
      <button class="link" id="backups-remove">${esc(tx({ key: 'Remove all older copies' }))}</button></p><table>
      ${shown.backups.map((f, i) => row(f, i, 'backup')).join('')}</table></details>`;
    spaceState(FS).then((sp) => {
      const el = document.getElementById('saves-space');
      const space = spaceText(sp.estimate, sp.bytes, sp.count, t, tn);
      el.textContent = sp.count || sp.bytes ? (sp.low
        ? tx({ key: '{space}. That is little: a save may not fit. Download the saves you want to keep, then delete some here or remove the older copies.', vars: { space } })
        : space) : '';
      el.className = sp.low ? 'small warn' : 'small';
    });
    if (c && c !== watched) { watched = c; c.onChange(() => { if (dialog.open) render(); }); }
  }

  document.getElementById('saves-open').onclick = () => { say(''); render(); dialog.showModal(); };
  document.getElementById('saves-close').onclick = () => dialog.close();
  document.getElementById('saves-upload').onclick = () => input.click();
  // One zip of everything (zipOfBlobs: the files of the vault go from disk, not through memory).
  // More than a zip holds: a part at each press (zipParts), the button says which is next.
  let nextPart = 0, partsNow = 1;
  const allButton = document.getElementById('saves-all');
  const allLabel = () => { allButton.textContent = tx(nextPart ? { key: 'Download part {part} of {parts}', vars: { part: nextPart + 1, parts: partsNow } } : { key: 'Download all' }); };
  allButton.onclick = async (e) => {
    const b = e.currentTarget;
    const everything = allFiles(FS);
    if (!everything.length) { say({ key: 'There is nothing to download yet.' }); return; }
    const parts = zipParts(everything), part = Math.min(nextPart, parts.length - 1), of = parts.length > 1 ? ` (${part + 1} of ${parts.length})` : '';
    let files = parts[part];
    b.disabled = true;
    let done = 0, shown = -1;
    try {
      // A file the browser's storage no longer has stays out, and the note names it: the others still go in the zip.
      const entries = [], lost = [];
      for (const f of files) {
        try { entries.push({ name: f.name, blob: await readBlob(FS, f.path), size: f.size }); }
        catch (err) { lost.push(f.name); log(`page: download all: ${f.name} left out: ${err.message}`); }
      }
      if (!entries.length) {
        throw Object.assign(new Error(`the browser's storage no longer has ${lost.join(', ')}`), { key: 'the browser\'s storage no longer has {names}', vars: { names: lost.join(', ') } });
      }
      files = entries;
      const total = files.reduce((a, f) => a + f.size, 0);
      const zip = await zipOfBlobs(entries, { onBytes: (n) => {
        done += n;
        const pct = Math.floor((done / total) * 100);
        if (pct !== shown) { shown = pct; say({ key: 'Packing {n} files ({size}): {pct}%', vars: { n: files.length, size: mb(total), pct } }); }
      } });
      const day = new Date().toISOString().slice(0, 10);
      saveLink(zip, `CM 01-02 saves ${day}${of}.zip`);
      nextPart = part + 1 < parts.length ? part + 1 : 0;
      partsNow = parts.length;
      const counts = { n: files.length, size: mb(total), part: part + 1, parts: parts.length };
      say([parts.length > 1
        ? { key: 'Downloaded {n} files ({size}) as part {part} of {parts}: the saves, the older copies (backups), the tactics and the data sets (Databases), as the save folder holds them.', vars: counts }
        : { key: 'Downloaded {n} files ({size}) as one zip: the saves, the older copies (backups), the tactics and the data sets (Databases), as the save folder holds them.', vars: counts },
      nextPart ? { key: 'A zip holds 4 GB: press Download part {part} of {parts} for the next one.', vars: { part: nextPart + 1, parts: parts.length } } : null,
      lost.length ? { key: 'Left out, because the browser\'s storage no longer has them: {names}.', vars: { names: lost.join(', ') } } : null], lost.length > 0);
      log(`page: downloaded all${of}: ${files.length} files, ${mb(total)}`);
    } catch (err) { say({ key: 'The zip was not made: {why}', vars: { why: errText(err) } }, true); }
    allLabel();
    b.disabled = false;
  };
  list.addEventListener('click', async (e) => {
    if (e.target.closest('#backups-remove')) {
      const old = shown.backups, bytes = old.reduce((a, f) => a + f.size, 0);
      if (!confirm(tx({ key: 'Remove all {n} older copies ({size})? The saves themselves stay. This cannot be undone.', vars: { n: old.length, size: size(bytes) } }))) return;
      let gone = 0;
      for (const f of old) { try { FS.unlink(f.path); gone++; } catch (err) { log(`page: ${f.path} was not removed: ${err.message}`); } }
      sync();
      say({ key: 'Removed {n} older copies: {size} free again.', vars: { n: gone, size: size(bytes) } });
      log(`page: removed ${gone} older copies (${size(bytes)})`);
      render();
      return;
    }
    const pl = e.target.closest('button[data-play]');
    if (pl && play) {
      const f = shown.saves[Number(pl.dataset.play)];
      if (f && play.play(f.name)) dialog.close();
      return;
    }
    const ed = e.target.closest('button[data-edit]');
    if (ed && edit) { dialog.close(); edit(shown.saves[Number(ed.dataset.edit)].name); return; }
    const r = e.target.closest('button[data-remove]');
    if (r && cloud()) {
      const name = r.dataset.remove;
      r.disabled = true;
      try { await cloud().remove(name); say({ key: '{name} is no longer in the cloud. It stays in this browser.', vars: { name } }); log(`page: ${name} removed from the cloud`); }
      catch (err) { say({ key: '{name} was not removed: {why}', vars: { name, why: errText(err) } }, true); }
      render();
      return;
    }
    const d = e.target.closest('button[data-delete]');
    if (d) {
      const backup = d.dataset.delete === 'backup', f = (backup ? shown.backups : shown.saves)[Number(d.dataset.i)];
      const inCloud = !backup && cloud()?.status(f.name, f) === 'cloud';
      const ask = backup ? { key: 'Delete this older copy of {name}? This cannot be undone; Download keeps a copy first.', vars: { name: f.name } }
        : inCloud ? { key: 'Delete {name} here and in the cloud? This cannot be undone; Download keeps a copy first.', vars: { name: f.name } }
          : { key: 'Delete {name}? This cannot be undone; Download keeps a copy first.', vars: { name: f.name } };
      if (!confirm(tx(ask))) return;
      try {
        if (inCloud) await cloud().remove(f.name);
        FS.unlink(f.path);
        sync();
        say(backup ? { key: 'The older copy of {name} is deleted.', vars: { name: f.name } } : { key: '{name} is deleted.', vars: { name: f.name } });
        log(`page: deleted the ${backup ? 'older copy' : 'save'} ${f.name}`);
      } catch (err) { say({ key: '{name} was not deleted: {why}', vars: { name: f.name, why: errText(err) } }, true); }
      render();
      return;
    }
    const b = e.target.closest('button[data-i]');
    if (!b) return;
    const f = (b.dataset.kind === 'backup' ? shown.backups : shown.saves)[Number(b.dataset.i)];
    try { await download(FS, f); say({ key: 'Downloaded {name}.', vars: { name: f.name } }); log(`page: downloaded the save ${f.name}`); }
    catch (err) { say({ key: '{name} could not be read: {why}', vars: { name: f.name, why: errText(err) } }, true); }
  });
  // Each file by what it is (upload.js): its saves, its tactics, or why neither, with what to do.
  input.onchange = async () => {
    const done = [], tacticsDone = [], notes = [], refused = [], refusedFiles = [];
    for (const file of input.files) {
      let sorted;
      try { sorted = await sortUpload(file); } catch (err) { sorted = { saves: [], tactics: [], notes: [], noteKeys: [], why: err.message, whyKey: err.key, whyVars: err.vars }; }
      notes.push(...sorted.notes.map((n, i) => sorted.noteKeys?.[i] ?? { key: n }));
      if (sorted.why) {
        refused.push({ key: '{file}: {why}', vars: { file: file.name, why: { key: sorted.whyKey ?? sorted.why, vars: sorted.whyVars } } });
        refusedFiles.push(file);
        continue;
      }
      for (const one of sorted.saves) {
        const taken = new Set(listSaves(FS).saves.map((f) => f.name.toLowerCase()));
        const name = saveName(one.name, taken);
        const label = one.name === file.name ? file.name : { key: '{name} (in {zip})', vars: { name: one.name, zip: file.name } };
        if (one.size > 50 << 20) say({ key: 'Storing {name} ({size})…', vars: { name: label, size: mb(one.size) } });
        let why;
        try { why = await storeSaveProblem(FS, name, await one.blob()); } catch (err) {
          why = { key: 'it was not stored: {why}', vars: { why: errText(err) } };
          log(`page: the upload of ${one.name} (${mb(one.size)}) was not stored: ${errorDetail(err)}`);
        }
        if (why) { refused.push({ key: '{file}: {why}', vars: { file: label, why } }); refusedFiles.push(file); continue; }
        done.push(name);
        log(`page: uploaded the save ${name} (${mb(one.size)})${one.name === file.name ? '' : ` from ${file.name}`}`);
      }
      for (const tc of sorted.tactics) {
        try { FS.mkdirTree(TACTICS_DIR); } catch { /* there */ }
        const name = tacticName(tc.name, new Set(listTactics(FS).map((f) => f.name.toLowerCase())));
        FS.writeFile(`${TACTICS_DIR}/${name}`, tc.bytes);
        tacticsDone.push(name.slice(0, -4));
        log(`page: uploaded the tactic ${name} through Saved games (from ${file.name})`);
      }
    }
    input.value = '';
    if (done.length || tacticsDone.length) sync();
    render();
    if (tacticsDone.length) document.dispatchEvent(new CustomEvent('cm-tactics-changed'));   // tactics.js: the list, the cloud
    say([done.length ? (play?.can() ? { key: 'Uploaded {names}. Press Play to open it in the game.', vars: { names: done.join(', ') } }
      : { key: 'Uploaded {names}. Open Restore Saved Game in the game to play it.', vars: { names: done.join(', ') } }) : null,
    tacticsDone.length ? { key: '{names} is a tactic: it is under Tactics now (on the tactics screen: Tactics, Load).',
      other: '{names} are tactics: they are under Tactics now (on the tactics screen: Tactics, Load).', n: tacticsDone.length, vars: { names: tacticsDone.join(', ') } } : null,
    ...notes, ...refused],
    refused.length > 0 && done.length === 0 && tacticsDone.length === 0, refusedFiles);
  };
  // Another language: what is shown again in it.
  onLanguage(() => {
    allLabel();
    if (said) say(said.m, said.bad, said.files);
    if (dialog.open) render();
  });
  document.getElementById('saves-open').hidden = false;
}
