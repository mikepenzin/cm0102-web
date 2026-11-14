// The tactics in Saved games: download the player's tactics, upload others (from the PC game or from a friend), delete
// them. The game keeps them as it does on a PC: Save As on the tactics screen writes <name>.tct in the folder tactics
// of the user folder, and Load lists that folder (a run of the game, 2026-11-12: Save As made
// user/tactics/4-4-2X.tct, 1,476 bytes, and Load Formation listed it).
//
// The file (docs/research/2026-10-16-tactics-inventory.md, the loader 0x5b5d10): a version word at offset 0, from
// 0x98EC54 to 0x98EC5E; the file is big-endian when its last 4 bytes are 0xF1FFFFF1 (0x5b5e2b). The presets that ship
// in Data (*.pct) store the version plus 0x075BCD15 ("obfuscated"), and that is the only byte they differ in from the
// same tactic saved by the game (442_default.pct against a Save As of 4-4-2). The tactics screen's Load reads the plain
// version only: a preset copied as a .tct gave "Unable To Load". So an upload of such a file (a .pct, or a .tct made
// from one) gets the plain version, and then loads (a run of the game: "Diamond Converted" from 442_diamond_default.pct).
export const TACTICS_DIR = '/user/tactics';
const VERSION_FROM = 0x98ec54, VERSION_TO = 0x98ec5e, OBFUSCATION = 0x075bcd15, BIG_ENDIAN = 0xf1fffff1;
const SMALLEST = 0x400, LARGEST = 0x10000;            // the shipped ones are 1,428 to 1,476 bytes

const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const kb = (n) => `${Math.max(1, Math.round(n / 1024))} KB`;
const when = (d) => d.toLocaleString(undefined, { dateStyle: 'medium', timeStyle: 'short' });
const known = (v) => v >= VERSION_FROM && v <= VERSION_TO;

// The tactic files of the folder, by name.
export function listTactics(FS) {
  let names;
  try { names = FS.readdir(TACTICS_DIR); } catch { return []; }
  const out = [];
  for (const name of names) {
    if (!/\.tct$/i.test(name)) continue;
    try {
      const st = FS.stat(`${TACTICS_DIR}/${name}`);
      if (FS.isFile(st.mode)) out.push({ name, path: `${TACTICS_DIR}/${name}`, size: st.size, mtime: new Date(st.mtime) });
    } catch { /* gone meanwhile */ }
  }
  return out.sort((a, b) => a.name.localeCompare(b.name, undefined, { sensitivity: 'base', numeric: true }));
}

// A tactic to store: { bytes } (the plain version, converted when the file had the presets' one), or { why }.
export function readyTactic(bytes) {
  if (bytes.length < SMALLEST) return { why: 'it is too short to be a tactic of CM 01/02' };
  if (bytes.length > LARGEST) return { why: 'it is too large to be a tactic of CM 01/02' };
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const little = dv.getUint32(bytes.length - 4, true) !== BIG_ENDIAN;
  const v = dv.getUint32(0, little);
  if (known(v)) return { bytes };
  const plain = (v - OBFUSCATION) >>> 0;
  if (!known(plain)) return { why: 'it is not a tactic of CM 01/02' };
  const out = bytes.slice();
  new DataView(out.buffer).setUint32(0, plain, little);
  return { bytes: out, converted: true };
}

// The name an uploaded file gets in the folder (a preset's .pct becomes .tct, the game's Load lists only those), or
// null when it is not a tactic file.
export function tacticName(fileName, taken) {
  let base = String(fileName).split(/[\\/]/).pop().replace(/[\u0000-\u001f:*?"<>|]/g, '_').trim();
  if (/\.pct$/i.test(base)) base = `${base.slice(0, -4)}.tct`;
  if (!/\.tct$/i.test(base) || base.length <= 4) return null;
  if (!taken.has(base.toLowerCase())) return base;
  const stem = base.slice(0, -4);
  for (let i = 2; ; i++) if (!taken.has(`${stem} (${i}).tct`.toLowerCase())) return `${stem} (${i}).tct`;
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

// Wires the Tactics section of the Saved games dialog (index.html, #tactics) to the engine's file system. sync() writes
// the user folder to the browser's storage.
export function mountTacticsPanel({ FS, sync, log }) {
  const list = document.getElementById('tactics-list'), note = document.getElementById('tactics-note');
  const input = document.getElementById('tactics-file');
  const say = (text, bad = false) => { note.textContent = text; note.className = bad ? 'small bad' : 'small'; };
  let shown = [];

  function render() {
    shown = listTactics(FS);
    list.innerHTML = shown.length
      ? `<table><tr><th>Tactic</th><th class="num">Size</th><th>Saved</th><th></th></tr>${shown.map((f, i) => `<tr>
        <td>${esc(f.name.slice(0, -4))}</td><td class="num">${kb(f.size)}</td><td>${esc(when(f.mtime))}</td>
        <td class="acts"><button data-tactic="${i}">Download</button><button class="link" data-tactic-delete="${i}">Delete</button></td></tr>`).join('')}</table>`
      : '<p class="small">No tactics yet. On the tactics screen use Tactics, Save As, or upload one.</p>';
  }

  document.getElementById('saves-open').addEventListener('click', () => { say(''); render(); });
  document.getElementById('tactics-upload').onclick = () => input.click();
  list.addEventListener('click', (e) => {
    const del = e.target.closest('button[data-tactic-delete]');
    if (del) {
      const f = shown[Number(del.dataset.tacticDelete)];
      if (!confirm(`Delete the tactic ${f.name.slice(0, -4)}? This cannot be undone; Download keeps a copy first.`)) return;
      try { FS.unlink(f.path); sync(); say(`The tactic ${f.name.slice(0, -4)} is deleted.`); log(`page: deleted the tactic ${f.name}`); }
      catch (err) { say(`${f.name} was not deleted: ${err.message}`, true); }
      render();
      return;
    }
    const b = e.target.closest('button[data-tactic]');
    if (!b) return;
    const f = shown[Number(b.dataset.tactic)];
    try {
      saveLink(new Blob([FS.readFile(f.path)], { type: 'application/octet-stream' }), f.name);
      say(`Downloaded ${f.name}. On a PC it goes in the folder tactics of the game (or of the Bosman Edition's user folder).`);
      log(`page: downloaded the tactic ${f.name}`);
    } catch (err) { say(`${f.name} could not be read: ${err.message}`, true); }
  });
  input.onchange = async () => {
    const done = [], refused = [];
    try { FS.mkdirTree(TACTICS_DIR); } catch { /* there already */ }
    for (const file of input.files) {
      const name = tacticName(file.name, new Set(listTactics(FS).map((f) => f.name.toLowerCase())));
      if (!name) { refused.push(`${file.name}: only .tct files (or a preset's .pct) are tactics`); continue; }
      const t = readyTactic(new Uint8Array(await file.arrayBuffer()));
      if (t.why) { refused.push(`${file.name}: ${t.why}`); continue; }
      FS.writeFile(`${TACTICS_DIR}/${name}`, t.bytes);
      done.push(name.slice(0, -4));
      log(`page: uploaded the tactic ${name}${t.converted ? ' (a preset\'s file, made loadable)' : ''}`);
    }
    input.value = '';
    if (done.length) sync();
    render();
    say([done.length ? `Uploaded ${done.join(', ')}. On the tactics screen use Tactics, Load to use it.` : '', ...refused].filter(Boolean).join('\n'),
      refused.length > 0 && done.length === 0);
  };
  document.getElementById('tactics').hidden = false;
  return { render };
}
