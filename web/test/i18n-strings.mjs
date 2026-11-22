// The page's English, and its translation tables (web/i18n-<code>.js), for the check in i18n.test.js and the page
// check's sweep. What a player reads goes through t('…'), tn(n, { one: '…', other: '…' }) or index.html's data-t,
// data-t-html and data-t-attr; errors of the modules the server imports carry their English as key: '…'.
import { readFileSync, readdirSync, existsSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const WEB = join(dirname(fileURLToPath(import.meta.url)), '..');
const norm = (s) => s.trim().replace(/\s+/g, ' ');
// A string literal in '…', "…" or `…` (no ${…}: a message is one template with {placeholders}).
const LIT = String.raw`'((?:[^'\\]|\\.)*)'|"((?:[^"\\]|\\.)*)"|\`((?:[^\`\\$]|\\.)*)\``;
const unescape = (s) => s.replace(/\\(.)/g, '$1');
const litOf = (m, i) => unescape(m[i] ?? m[i + 1] ?? m[i + 2]);

// The English of t('…') and of tn's one-form, and of key: '…', in a script's text.
export function stringsOf(js) {
  const out = new Set();
  for (const m of js.matchAll(new RegExp(String.raw`\bt\(\s*(?:${LIT})`, 'g'))) out.add(litOf(m, 1));
  for (const m of js.matchAll(new RegExp(String.raw`\btn\([^,]+,\s*\{\s*one:\s*(?:${LIT})`, 'g'))) out.add(litOf(m, 1));
  for (const m of js.matchAll(new RegExp(String.raw`\bkey:\s*(?:${LIT})`, 'g'))) out.add(litOf(m, 1));
  return out;
}

// The plural texts of a script: Map of tn's one-form -> its other-form (a table keys a plural by its one-form).
export function pluralsOf(js) {
  const out = new Map();
  for (const m of js.matchAll(new RegExp(String.raw`\btn\([^,]+,\s*\{\s*one:\s*(?:${LIT})\s*,\s*other:\s*(?:${LIT})`, 'g'))) out.set(litOf(m, 1), litOf(m, 4));
  return out;
}

// Every plural text of the page: Map one-form -> other-form.
export function pagePlurals(web = WEB) {
  const out = new Map();
  for (const f of readdirSync(web).filter((n) => n.endsWith('.js') && !n.startsWith('i18n-') && !NOT_PAGE.has(n))) {
    const js = readFileSync(join(web, f), 'utf8');
    if (!SHARED.has(f)) { for (const [one, other] of pluralsOf(js)) out.set(one, other); continue; }
    // The shared modules' counts: { key: '…', other: '…', n } (saves.js, textOf).
    for (const m of js.matchAll(new RegExp(String.raw`\bkey:\s*(?:${LIT})\s*,\s*other:\s*(?:${LIT})`, 'g'))) out.set(litOf(m, 1), litOf(m, 4));
  }
  return out;
}

// The English of index.html's marked text: data-t on a leaf element (its text), data-t-html (its inner HTML),
// data-t-attr (the named attributes' values). Whitespace as apply() makes it.
export function htmlStrings(html) {
  const out = new Set();
  for (const m of html.matchAll(/<(\w+)\b([^>]*)>([^<]*)<\/\1>/g)) if (/\sdata-t(?=[\s>=]|$)/.test(` ${m[2]}`) && norm(m[3])) out.add(norm(m[3]));
  for (const m of html.matchAll(/<(\w+)\b([^>]*\sdata-t-html\b[^>]*)>([\s\S]*?)<\/\1>/g)) out.add(norm(m[3]));
  for (const m of html.matchAll(/<\w+\b([^>]*\sdata-t-attr="([^"]+)"[^>]*)>/g)) {
    for (const name of m[2].split(',').map((s) => s.trim())) {
      const v = new RegExp(String.raw`\s${name}="([^"]*)"`).exec(m[1]);
      if (v && norm(v[1])) out.add(norm(v[1]));
    }
  }
  return out;
}

// The modules the server imports too: their thrown errors' key: '…' only.
const SHARED = new Set(['zip.js', 'savedit.js', 'dataset.js', 'saves.js', 'upload.js']);
const NOT_PAGE = new Set(['sw.js']);

// Every English text of the page: Map english -> Set of files.
export function pageStrings(web = WEB) {
  const all = new Map();
  const add = (s, f) => { if (!all.has(s)) all.set(s, new Set()); all.get(s).add(f); };
  for (const f of readdirSync(web).filter((n) => n.endsWith('.js') && !n.startsWith('i18n-') && !NOT_PAGE.has(n))) {
    const text = readFileSync(join(web, f), 'utf8');
    const found = SHARED.has(f) ? new Set([...text.matchAll(new RegExp(String.raw`\bkey:\s*(?:${LIT})`, 'g'))].map((m) => litOf(m, 1))) : stringsOf(text);
    for (const s of found) add(s, f);
  }
  const html = join(web, 'index.html');
  if (existsSync(html)) for (const s of htmlStrings(readFileSync(html, 'utf8'))) add(s, 'index.html');
  return all;
}

// The translation tables: Map code -> table.
export async function tables(web = WEB) {
  const out = new Map();
  for (const f of readdirSync(web).filter((n) => /^i18n-[a-z]{2}\.js$/.test(n)).sort()) {
    out.set(f.slice(5, 7), (await import(pathToFileURL(join(web, f)).href)).default ?? {});
  }
  return out;
}

const marks = (s) => [...String(s).matchAll(/\{(\w+)\}/g)].map((m) => m[1]).sort().join(',');

// Per language: missing (English with no entry), stale (entries whose English is not on the page), placeholders (an
// entry whose {placeholders} differ from its English's; a plural entry, each of its forms).
export function checkTables(strings, tablesByCode) {
  const report = new Map();
  for (const [code, table] of tablesByCode) {
    const missing = [...strings.keys()].filter((s) => !(s in table));
    const stale = Object.keys(table).filter((s) => !strings.has(s));
    const placeholders = [];
    for (const [en, tr] of Object.entries(table)) {
      if (typeof tr === 'string') { if (marks(tr) !== marks(en)) placeholders.push(`${en} → ${tr}`); continue; }
      for (const [cat, form] of Object.entries(tr ?? {})) if (marks(form) !== marks(en)) placeholders.push(`${en} (${cat}) → ${form}`);
    }
    report.set(code, { missing, stale, placeholders });
  }
  return report;
}
