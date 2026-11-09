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

// A short text that changes whenever a save file is added, changed or removed (the watcher of loader.js).
export function savesSignature(FS) {
  const { saves, backups } = listSaves(FS);
  return [...saves, ...backups].map((f) => `${f.path}:${f.size}:${+f.mtime}`).join('|');
}

// null when bytes look like a saved game of CM 01/02 (its container header), else why not.
export function checkSave(bytes) {
  if (bytes.length < HEADER) return 'it is too short to be a saved game of CM 01/02';
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const version = dv.getInt32(0, true), count = dv.getInt32(8, true);
  if (![1, 2, 3, 4].includes(version) || count < 1 || count > MAX_ENTRIES) return 'it is not a saved game of CM 01/02';
  for (let k = 0; k < count; k++)
    if (dv.getUint32(12 + k * ENTRY, true) > bytes.length) return 'the saved game is damaged: a part starts after its end';
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

function download(FS, f) {
  const url = URL.createObjectURL(new Blob([FS.readFile(f.path)], { type: 'application/octet-stream' }));
  const a = document.createElement('a');
  a.href = url;
  a.download = f.name;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 60000);
}

// Wires the panel of index.html to the engine's file system. sync() writes the user folder to IndexedDB now.
export function mountSavesPanel({ FS, sync, log }) {
  const dialog = document.getElementById('saves'), list = document.getElementById('saves-list');
  const note = document.getElementById('saves-note'), input = document.getElementById('saves-file');
  const say = (text, bad = false) => { note.textContent = text; note.className = bad ? 'small bad' : 'small'; };
  let shown = { saves: [], backups: [] };

  function render() {
    shown = listSaves(FS);
    const row = (f, i, kind) => `<tr><td>${esc(f.name)}</td><td class="num">${mb(f.size)}</td><td>${esc(when(f.mtime))}</td>
      <td><button data-kind="${kind}" data-i="${i}">Download</button></td></tr>`;
    list.innerHTML = shown.saves.length
      ? `<table><tr><th>Save</th><th class="num">Size</th><th>Saved</th><th></th></tr>${shown.saves.map((f, i) => row(f, i, 'save')).join('')}</table>`
      : '<p class="small">No saved games yet. Save in the game (Game Options, Save Game), or upload one.</p>';
    if (shown.backups.length) list.innerHTML += `<details><summary>Older copies (${shown.backups.length})</summary><table>
      ${shown.backups.map((f, i) => row(f, i, 'backup')).join('')}</table></details>`;
  }

  document.getElementById('saves-open').onclick = () => { say(''); render(); dialog.showModal(); };
  document.getElementById('saves-close').onclick = () => dialog.close();
  document.getElementById('saves-upload').onclick = () => input.click();
  list.addEventListener('click', (e) => {
    const b = e.target.closest('button[data-i]');
    if (!b) return;
    const f = (b.dataset.kind === 'backup' ? shown.backups : shown.saves)[Number(b.dataset.i)];
    try { download(FS, f); say(`Downloaded ${f.name}.`); log(`page: downloaded the save ${f.name}`); }
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
