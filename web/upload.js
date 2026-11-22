// What a file given to Saved games is, by what it holds and not by its name (web/README.md, "Saved games"), and what
// to do with it. A player uploaded Matt.zip (a save zipped by another site, 16 November 2026) and was told that only
// .sav files are saved games: the page judged the name alone. sortUpload looks at the first bytes, and into a zip:
// - a saved game of CM 01/02 (its container header, saves.js checkSave) is a save, whatever its name: "Matt", or
//   "Matt.sav.txt" from a phone, becomes Matt.sav;
// - a zip (also one named .sav) gives the saves in it, wherever in its folders, except its older copies (backups/),
//   and its tactics; each is read from the zip only when it is stored (zip.js, zipBlobEntry);
// - a gzip (a report sends a save as .sav.gz) is unpacked and sorted again;
// - a tactic (tactics.js readyTactic) goes to the tactics;
// - anything else is named, with what to do: a RAR or 7-Zip archive, a web page (a download link that gave its page),
//   a data set or one of its files, a picture, a text, an empty file, or a file that is not a saved game of CM 01/02.
import { DATA_SET_FILES } from './dataset.js';
import { SAVE_HEADER, saveProblem } from './saves.js';
import { readyTactic } from './tactics.js';
import { zipBlobEntries, zipBlobEntry } from './zip.js';

const SETS = new Set(DATA_SET_FILES);
const base = (path) => String(path).split(/[\\/]/).pop();
const starts = (b, ...sig) => sig.every((v, i) => b[i] === v);
// What the player reads keeps its English and has key (the English with {placeholders}) and vars, which the page
// translates (i18n.js, which this module never imports: the server imports saves.js, and it this one).
const fill = (key, vars) => key.replace(/\{(\w+)\}/g, (m, k) => (vars && k in vars ? String(vars[k]) : m));
const text = ({ key, vars }) => ({ message: fill(key, vars), key, vars });
// A note of the upload: notes has its English, noteKeys its { key, vars }; why the upload is refused: why (the English),
// whyKey and whyVars.
const note = (out, n) => { const m = text(n); out.notes.push(m.message); out.noteKeys.push({ key: m.key, vars: m.vars }); };
const refuse = (out, why) => { out.why = why.message; out.whyKey = why.key ?? null; out.whyVars = why.vars; return out; };

// The name a save gets: "Matt.sav" stays, "Matt.sav.txt" and "Matt" become "Matt.sav", another extension goes.
export function saveFileName(name) {
  const b = base(name);
  const m = /^(.*?\.sav)(\.[^.]*)*$/i.exec(b);
  if (m) return m[1];
  const stem = b.replace(/\.[^.]{1,5}$/, '') || 'uploaded save';
  return `${stem}.sav`;
}

// What the first bytes say a file is, for one that is no save: a name for the message, or null.
function kindOf(head, name) {
  if (starts(head, 0x52, 0x61, 0x72, 0x21, 0x1a, 0x07)) {
    return text({ key: 'it is a RAR archive, which this page does not open: unpack it first (on a computer, or with an app that opens it on a phone), then upload the .sav file inside, or zip it as a .zip' });
  }
  if (starts(head, 0x37, 0x7a, 0xbc, 0xaf, 0x27, 0x1c)) {
    return text({ key: 'it is a 7-Zip archive, which this page does not open: unpack it first (on a computer, or with an app that opens it on a phone), then upload the .sav file inside, or zip it as a .zip' });
  }
  if (starts(head, 0x89, 0x50, 0x4e, 0x47)) return text({ key: 'it is a picture (PNG), not a saved game' });
  if (starts(head, 0xff, 0xd8, 0xff)) return text({ key: 'it is a picture (JPEG), not a saved game' });
  if (starts(head, 0x47, 0x49, 0x46, 0x38)) return text({ key: 'it is a picture (GIF), not a saved game' });
  if (starts(head, 0x25, 0x50, 0x44, 0x46)) return text({ key: 'it is a PDF document, not a saved game' });
  const start = new TextDecoder('utf-8', { fatal: false }).decode(head.subarray(0, 512));
  if (/^\s*(<!doctype html|<html|<head|<body)/i.test(start)) {
    return text({ key: 'it is a web page, not a save: the site\'s download link gave its page instead of the file. Open the link, let the save itself download, and upload that' });
  }
  if (SETS.has(base(name).toLowerCase()) || /\.dat$/i.test(name)) {
    return text({ key: 'it is a file of a data set (the game\'s database), not a saved game: upload the data set\'s .zip, or all its .dat files together, under Data sets below' });
  }
  const plain = head.length > 0 && head.subarray(0, 512).every((c) => c === 9 || c === 10 || c === 13 || (c >= 0x20 && c !== 0x7f) || c >= 0x80);
  if (plain && /[a-z]{3}/i.test(start)) return text({ key: 'it is a text file, not a saved game' });
  return null;
}

// The upload sorted: { saves: [{ name, size, blob() }], tactics: [{ name, bytes }], notes: [text], why: text or null }.
// why: why nothing in it was taken, with what to do. For the page's translation: noteKeys ([{ key, vars }], one per
// note), whyKey and whyVars.
export async function sortUpload(file, depth = 0) {
  const out = { saves: [], tactics: [], notes: [], noteKeys: [], why: null, whyKey: null, whyVars: undefined };
  const head = new Uint8Array(await file.slice(0, Math.max(SAVE_HEADER, 4096)).arrayBuffer());
  if (file.size === 0) return refuse(out, saveProblem(head, 0));

  if (starts(head, 0x50, 0x4b, 0x03, 0x04) || starts(head, 0x50, 0x4b, 0x05, 0x06)) {           // a zip
    if (!/\.zip$/i.test(file.name)) note(out, { key: '{name} is a zip: the saves in it were taken', vars: { name: file.name } });
    let entries;
    try { entries = await zipBlobEntries(file); } catch (e) { return refuse(out, e); }
    const files = entries.filter((e) => !e.name.endsWith('/'));
    for (const e of files) {
      if (/\.sav$/i.test(e.name) && !/(^|\/)backups\//i.test(e.name)) out.saves.push({ name: base(e.name), size: e.size, blob: () => zipBlobEntry(file, e) });
      else if (/\.(tct|pct)$/i.test(e.name) && e.size < 65536) {
        const t = readyTactic(new Uint8Array(await (await zipBlobEntry(file, e)).arrayBuffer()));
        if (!t.why) out.tactics.push({ name: base(e.name).replace(/\.pct$/i, '.tct'), bytes: t.bytes });
      }
    }
    const setFiles = files.filter((e) => SETS.has(base(e.name).toLowerCase())).length;
    if (setFiles >= 3 && out.saves.length) note(out, { key: '{name} also holds a data set: upload it under Data sets below to play with it', vars: { name: file.name } });
    if (!out.saves.length && !out.tactics.length) {
      const names = files.slice(0, 5).map((e) => base(e.name)).join(', ');
      refuse(out, text(setFiles >= 3
        ? { key: 'it holds a data set (the game\'s database), not a saved game: upload it under Data sets below' }
        : files.length > 5 ? { key: 'it holds no saved game: {names} and {n} more', vars: { names, n: files.length - 5 } }
          : { key: 'it holds no saved game: {names}', vars: { names } }));
    }
    return out;
  }

  if (starts(head, 0x1f, 0x8b) && depth === 0) {                                   // a gzip: unpacked, then sorted again
    let inner;
    try { inner = await new Response(file.stream().pipeThrough(new DecompressionStream('gzip'))).blob(); }
    catch { return refuse(out, text({ key: 'it is packed (gzip) and does not unpack: it is damaged' })); }
    const name = file.name.replace(/\.gz$/i, '') || 'uploaded save.sav';
    const sorted = await sortUpload(new File([inner], name), 1);
    const packed = text({ key: '{name} was packed (gzip): unpacked', vars: { name: file.name } });
    sorted.notes.unshift(packed.message);
    sorted.noteKeys.unshift({ key: packed.key, vars: packed.vars });
    return sorted;
  }

  if (!saveProblem(head, file.size)) {                                                // a save, whatever its name
    const name = saveFileName(file.name);
    if (name !== base(file.name)) note(out, { key: '{file} is a saved game: it is stored as {name}', vars: { file: file.name, name } });
    out.saves.push({ name, size: file.size, blob: async () => file });
    return out;
  }

  if (file.size <= 65536) {                                                         // a tactic
    const t = readyTactic(new Uint8Array(await file.arrayBuffer()));
    if (!t.why) { out.tactics.push({ name: base(file.name).replace(/\.(pct|tct)?$/i, '.tct'), bytes: t.bytes }); return out; }
  }

  return refuse(out, kindOf(head, file.name) ?? saveProblem(head, file.size));
}
