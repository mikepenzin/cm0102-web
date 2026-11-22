// One translation table against the page's English (web/README.md, "Languages"; the brief: i18n-brief.md): missing
// and stale entries, placeholders, markup, plural forms, empty strings. Prints a JSON line and the first missing
// texts; exits 1 on any of them.
//   node web/test/i18n-table-check.mjs de
import { pageStrings, pagePlurals, tables, checkTables } from './i18n-strings.mjs';

const code = process.argv[2];
const table = (await tables()).get(code);
if (!table) { console.error(`no table web/i18n-${code}.js`); process.exit(2); }
const strings = pageStrings(), plurals = pagePlurals();
const r = checkTables(strings, new Map([[code, table]]), plurals).get(code);
const tags = (s) => [...String(s).matchAll(/<\/?[a-z][^>]*>/gi)].map((m) => m[0]).join('');
const markup = [], empty = [];
for (const [en, tr] of Object.entries(table)) {
  if (plurals.has(en)) continue;                                 // forms: checkTables
  if (typeof tr !== 'string' || !tr.trim()) { empty.push(en); continue; }
  if (tags(tr) !== tags(en)) markup.push(`${en} → ${tr}`);
}
console.log(JSON.stringify({ code, entries: Object.keys(table).length, missing: r.missing.length, stale: r.stale, placeholders: r.placeholders,
  markupDiffers: markup, pluralWithoutAllForms: r.forms, empty }));
for (const m of r.missing.slice(0, 10)) console.log('missing:', m);
process.exit(r.missing.length || r.stale.length || r.placeholders.length || r.forms.length || markup.length || empty.length ? 1 : 0);
