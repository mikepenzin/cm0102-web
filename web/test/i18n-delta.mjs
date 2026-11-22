// What the translation tables lack after an English text changed (web/README.md, "Languages"; the brief:
// i18n-brief.md): the texts missing from them, with the files each is in and the other-form of a plural, and the
// entries whose English left the page. Measured on the German table; every table has the same entries.
//   node web/test/i18n-delta.mjs [out.json]          (no file: a summary on the terminal)
import { writeFileSync } from 'node:fs';
import { pageStrings, pagePlurals, tables, checkTables } from './i18n-strings.mjs';

const strings = pageStrings(), plurals = pagePlurals();
const r = checkTables(strings, await tables(), plurals).get('de');
const out = { missing: r.missing.map((en) => ({ en, files: [...strings.get(en)], ...(plurals.has(en) ? { other: plurals.get(en) } : {}) })), stale: r.stale };
if (process.argv[2]) writeFileSync(process.argv[2], JSON.stringify(out, null, 1));
console.log(`${out.missing.length} missing, ${out.stale.length} stale${process.argv[2] ? `: ${process.argv[2]}` : ''}`);
if (!process.argv[2]) {
  for (const m of out.missing) console.log(`+ ${m.en.slice(0, 110)}`);
  for (const s of out.stale) console.log(`- ${s.slice(0, 110)}`);
}
