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
import { whenCell } from './saves.js';
import { onLanguage, t } from './i18n.js';
export const TACTICS_DIR = '/user/tactics';
const VERSION_FROM = 0x98ec54, VERSION_TO = 0x98ec5e, OBFUSCATION = 0x075bcd15, BIG_ENDIAN = 0xf1fffff1;
const SMALLEST = 0x400, LARGEST = 0x10000;            // the shipped ones are 1,428 to 1,476 bytes

const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
const kb = (n) => `${Math.max(1, Math.round(n / 1024))} KB`;
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

// A tactic to store: { bytes } (the plain version, converted when the file had the presets' one), or { why, key }: why
// in English, key the same for the page's translation (i18n.js).
const refusal = ({ key }) => ({ why: key, key });
export function readyTactic(bytes) {
  if (bytes.length < SMALLEST) return refusal({ key: 'it is too short to be a tactic of CM 01/02' });
  if (bytes.length > LARGEST) return refusal({ key: 'it is too large to be a tactic of CM 01/02' });
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const little = dv.getUint32(bytes.length - 4, true) !== BIG_ENDIAN;
  const v = dv.getUint32(0, little);
  if (known(v)) return { bytes };
  const plain = (v - OBFUSCATION) >>> 0;
  if (!known(plain)) return refusal({ key: 'it is not a tactic of CM 01/02' });
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
// cloud(): the tactics sync of Cloud Saves (tacticsync.js), or null.
export function mountTacticsPanel({ FS, sync, log, cloud = () => null }) {
  const list = document.getElementById('tactics-list'), note = document.getElementById('tactics-note');
  const input = document.getElementById('tactics-file');
  const say = (text, bad = false) => { note.textContent = text; note.className = bad ? 'small bad' : 'small'; };
  const why = (e) => t(e.key ?? e.message, e.vars);
  let shown = [];

  function render() {
    shown = listTactics(FS);
    list.innerHTML = shown.length
      ? `<table><tr><th>${esc(t('Tactic'))}</th><th class="num">${esc(t('Size'))}</th><th>${esc(t('Saved'))}</th><th></th></tr>${shown.map((f, i) => `<tr>
        <td>${esc(f.name.slice(0, -4))}</td><td class="num">${kb(f.size)}</td>${whenCell(f.mtime)}
        <td class="acts"><div class="btns"><button data-tactic="${i}">${esc(t('Download'))}</button><button class="link danger" data-tactic-delete="${i}">${esc(t('Delete'))}</button></div></td></tr>`).join('')}</table>`
      : `<p class="small">${esc(t('No tactics yet. On the tactics screen use Tactics, Save As, or upload one.'))}</p>`;
  }

  document.getElementById('saves-open').addEventListener('click', () => { say(''); render(); });
  document.getElementById('tactics-upload').onclick = () => input.click();
  list.addEventListener('click', (e) => {
    const del = e.target.closest('button[data-tactic-delete]');
    if (del) {
      const f = shown[Number(del.dataset.tacticDelete)];
      if (!confirm(t('Delete the tactic {name}? This cannot be undone; Download keeps a copy first.', { name: f.name.slice(0, -4) }))) return;
      try { FS.unlink(f.path); sync(); cloud()?.removed(f.name); say(t('The tactic {name} is deleted.', { name: f.name.slice(0, -4) })); log(`page: deleted the tactic ${f.name}`); }
      catch (err) { say(t('{name} was not deleted: {why}', { name: f.name, why: why(err) }), true); }
      render();
      return;
    }
    const b = e.target.closest('button[data-tactic]');
    if (!b) return;
    const f = shown[Number(b.dataset.tactic)];
    try {
      saveLink(new Blob([FS.readFile(f.path)], { type: 'application/octet-stream' }), f.name);
      say(t('Downloaded {name}. On a PC it goes in the folder tactics of the game (or of the Bosman Edition\'s user folder).', { name: f.name }));
      log(`page: downloaded the tactic ${f.name}`);
    } catch (err) { say(t('{name} could not be read: {why}', { name: f.name, why: why(err) }), true); }
  });
  input.onchange = async () => {
    const done = [], refused = [];
    try { FS.mkdirTree(TACTICS_DIR); } catch { /* there already */ }
    for (const file of input.files) {
      const name = tacticName(file.name, new Set(listTactics(FS).map((f) => f.name.toLowerCase())));
      if (!name) { refused.push(t('{file}: only .tct files (or a preset\'s .pct) are tactics', { file: file.name })); continue; }
      const tc = readyTactic(new Uint8Array(await file.arrayBuffer()));
      if (tc.why) { refused.push(t('{file}: {why}', { file: file.name, why: t(tc.key ?? tc.why) })); continue; }
      FS.writeFile(`${TACTICS_DIR}/${name}`, tc.bytes);
      done.push(name.slice(0, -4));
      log(`page: uploaded the tactic ${name}${tc.converted ? ' (a preset\'s file, made loadable)' : ''}`);
    }
    input.value = '';
    if (done.length) { sync(); cloud()?.run(); }
    render();
    say([done.length ? t('Uploaded {names}. On the tactics screen use Tactics, Load to use it.', { names: done.join(', ') }) : '', ...refused].filter(Boolean).join('\n'),
      refused.length > 0 && done.length === 0);
  };
  document.getElementById('tactics').hidden = false;
  // A tactic that came through Saved games (upload.js): shown here, and to the cloud.
  document.addEventListener('cm-tactics-changed', () => { render(); cloud()?.run(); });
  onLanguage(() => render());                         // the list in the new language
  return { render };
}
