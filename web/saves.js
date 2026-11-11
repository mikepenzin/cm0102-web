// The Saved games panel (web/README.md): the saves in the user folder, to download as files and to upload back.
//
// The saves stay where the game writes them: the user folder /user, which loader.js keeps in IndexedDB. localStorage
// is not a place for them: it holds a few MB of text per site, and a save holds the whole game world.
//
// The game writes *.sav files in the user folder (src/recomp_rt/win/files.c; the option save_folder can name another
// folder, which the browser build does not use). A save in progress is temp_save_file.sav, renamed when it is done;
// older copies are in backups/ (option save_backups).
export const SAVE_DIR = '/user';
const TEMP = 'temp_save_file.sav';
const HEADER = 0x416ec, ENTRY = 0x10c, MAX_ENTRIES = 1000;     // the save container (tools/save/savfile.py)

const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const mb = (n) => `${(n / 1048576).toFixed(n < 10485760 ? 1 : 0)} MB`;
const when = (d) => d.toLocaleString(undefined, { dateStyle: 'medium', timeStyle: 'short' });

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
export function checkSave(bytes, total = bytes.length) {
  if (total < HEADER || bytes.length < HEADER) return 'it is too short to be a saved game of CM 01/02';
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const version = dv.getInt32(0, true), count = dv.getInt32(8, true);
  if (![1, 2, 3, 4].includes(version) || count < 1 || count > MAX_ENTRIES) return 'it is not a saved game of CM 01/02';
  for (let k = 0; k < count; k++)
    if (dv.getUint32(12 + k * ENTRY, true) > total) return 'the saved game is damaged: a part starts after its end';
  return null;
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
export function readBlobNow(FS, path) {
  let node = null;
  try { node = FS.lookupPath(path).node; } catch { /* not this FS */ }
  return vaultRef && node?.vault === 'out' ? vaultRef.blob(path) : new Blob([fileView(FS, path)]);
}

async function download(FS, f) {
  const url = URL.createObjectURL(new Blob([await readBlob(FS, f.path)], { type: 'application/octet-stream' }));
  const a = document.createElement('a');
  a.href = url;
  a.download = f.name;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 60000);
}

const CLOUD = { cloud: 'In the cloud', up: 'Saving…', here: 'Only here', waiting: 'Not yet' };

// What the browser may do to the saves on its own. kept: the answer of navigator.storage.persist (loader.js): 'kept'
// when the browser agreed to keep the site's storage when the disk runs low. It still deletes it when the player
// clears browsing data, when a private window closes, and Safari (iPhone, iPad, Mac) deletes a site's storage after 7
// days without a visit, unless the page is on the home screen.
let storageKept = 'unknown';
export function setStorageKept(kept) { storageKept = kept; }
export function storageWarning(cloudOn) {
  const when = 'when browsing data is cleared, when a private window closes, and in Safari (iPhone, iPad, Mac) after 7 days without a visit, unless the page is on the home screen';
  if (cloudOn) return `The browser can delete the saves here by itself, without asking: when the disk runs low, ${when}. The saves marked In the cloud then come back at the next start; download the others you want to keep.`;
  if (storageKept === 'kept') return `This browser agreed to keep these saves when the disk runs low, but it still deletes them by itself, without asking, ${when}. Download the saves you want to keep, or use Cloud Saves.`;
  return `The browser can delete these saves by itself, without asking: when the disk runs low, ${when}. Download the saves you want to keep, or use Cloud Saves.`;
}

// Wires the panel of index.html to the engine's file system. sync() writes the user folder to IndexedDB now. cloud():
// the cloud sync of account.js, or null when the account has no cloud saves.
// promo(): a line about Cloud Saves for a player without them, or null; openPromo() shows them. edit(name): opens the
// save editor (editor.js) on a save of the folder.
export function mountSavesPanel({ FS, sync, log, cloud = () => null, promo = () => null, openPromo = () => {}, edit = null }) {
  const dialog = document.getElementById('saves'), list = document.getElementById('saves-list');
  const note = document.getElementById('saves-note'), input = document.getElementById('saves-file');
  const say = (text, bad = false) => { note.textContent = text; note.className = bad ? 'small bad' : 'small'; };
  let shown = { saves: [], backups: [] }, watched = null;

  function render() {
    shown = listSaves(FS);
    const c = cloud();
    document.getElementById('saves-where').textContent = c ? 'Your saves are in this browser and in the cloud.' : 'Your saves stay in this browser.';
    document.getElementById('saves-risk').textContent = storageWarning(Boolean(c));
    const p = promo(), box = document.getElementById('saves-promo');
    box.hidden = !p;
    if (p) { box.firstChild.textContent = `${p} `; box.querySelector('button').onclick = openPromo; }
    const state = (f) => {
      const st = c.status(f.name, f);
      return `<td class="cloud ${st}">${CLOUD[st]}${st === 'cloud' ? ` <button class="link" data-remove="${esc(f.name)}" title="Remove from the cloud; it stays here">Remove</button>` : ''}</td>`;
    };
    const row = (f, i, kind) => `<tr><td>${esc(f.name)}</td><td class="num">${mb(f.size)}</td><td>${esc(when(f.mtime))}</td>
      ${c ? (kind === 'save' ? state(f) : '<td></td>') : ''}<td class="acts">${edit && kind === 'save' ? `<button data-edit="${i}">Edit</button>` : ''}<button data-kind="${kind}" data-i="${i}">Download</button><button class="link" data-delete="${kind}" data-i="${i}">Delete</button></td></tr>`;
    list.innerHTML = shown.saves.length
      ? `<table><tr><th>Save</th><th class="num">Size</th><th>Saved</th>${c ? '<th>Cloud</th>' : ''}<th></th></tr>${shown.saves.map((f, i) => row(f, i, 'save')).join('')}</table>`
      : '<p class="small">No saved games yet. Save in the game (Game Options, Save Game), or upload one.</p>';
    if (shown.backups.length) list.innerHTML += `<details><summary>Older copies (${shown.backups.length})</summary><table>
      ${shown.backups.map((f, i) => row(f, i, 'backup')).join('')}</table></details>`;
    if (c && c !== watched) { watched = c; c.onChange(() => { if (dialog.open) render(); }); }
  }

  document.getElementById('saves-open').onclick = () => { say(''); render(); dialog.showModal(); };
  document.getElementById('saves-close').onclick = () => dialog.close();
  document.getElementById('saves-upload').onclick = () => input.click();
  list.addEventListener('click', async (e) => {
    const ed = e.target.closest('button[data-edit]');
    if (ed && edit) { dialog.close(); edit(shown.saves[Number(ed.dataset.edit)].name); return; }
    const r = e.target.closest('button[data-remove]');
    if (r && cloud()) {
      const name = r.dataset.remove;
      r.disabled = true;
      try { await cloud().remove(name); say(`${name} is no longer in the cloud. It stays in this browser.`); log(`page: ${name} removed from the cloud`); }
      catch (err) { say(`${name} was not removed: ${err.message}`, true); }
      render();
      return;
    }
    const d = e.target.closest('button[data-delete]');
    if (d) {
      const backup = d.dataset.delete === 'backup', f = (backup ? shown.backups : shown.saves)[Number(d.dataset.i)];
      const inCloud = !backup && cloud()?.status(f.name, f) === 'cloud';
      if (!confirm(`Delete ${backup ? 'this older copy of ' : ''}${f.name}${inCloud ? ' here and in the cloud' : ''}? This cannot be undone; Download keeps a copy first.`)) return;
      try {
        if (inCloud) await cloud().remove(f.name);
        FS.unlink(f.path);
        sync();
        say(`${backup ? 'The older copy of ' : ''}${f.name} is deleted.`);
        log(`page: deleted the ${backup ? 'older copy' : 'save'} ${f.name}`);
      } catch (err) { say(`${f.name} was not deleted: ${err.message}`, true); }
      render();
      return;
    }
    const b = e.target.closest('button[data-i]');
    if (!b) return;
    const f = (b.dataset.kind === 'backup' ? shown.backups : shown.saves)[Number(b.dataset.i)];
    try { await download(FS, f); say(`Downloaded ${f.name}.`); log(`page: downloaded the save ${f.name}`); }
    catch (err) { say(`${f.name} could not be read: ${err.message}`, true); }
  });
  input.onchange = async () => {
    const done = [], refused = [];
    for (const file of input.files) {
      const taken = new Set(listSaves(FS).saves.map((f) => f.name.toLowerCase()));
      const name = saveName(file.name, taken);
      if (!name) { refused.push(`${file.name}: only .sav files are saved games`); continue; }
      const bytes = new Uint8Array(await file.arrayBuffer());
      const why = checkSave(bytes);
      if (why) { refused.push(`${file.name}: ${why}`); continue; }
      FS.writeFile(`${SAVE_DIR}/${name}`, bytes);
      done.push(name);
      log(`page: uploaded the save ${name} (${mb(bytes.length)})`);
    }
    input.value = '';
    if (done.length) sync();
    render();
    say([done.length ? `Uploaded ${done.join(', ')}. Open Load Game in the game to play it.` : '', ...refused].filter(Boolean).join('\n'),
      refused.length > 0 && done.length === 0);
  };
  document.getElementById('saves-open').hidden = false;
}
