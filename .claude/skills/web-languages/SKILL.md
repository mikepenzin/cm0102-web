---
name: web-languages
description: Changes the browser page's text in all ten languages (English and the game's nine) without leaving English behind or breaking CI. Use for "add a text", "change the wording", "rename a button", "translate", "i18n", "a new feature's texts", "the table check failed", "stale entries", "missing translations", "the page in German", "I18N_STRICT", or any edit to web/*.js or web/index.html that changes what a player reads.
---

# The page's languages

The page around the game (web/) speaks English and es, pt, it, fr, de, nl, da, no, sv. web/README.md, "Languages", has
the design; web/i18n.js the code; web/i18n-<code>.js the tables. The English text is the key.

## Writing a text

- In page code: `t('Saved {name}', { name })`; a count: `tn(n, { one: '{n} save', other: '{n} saves' })`; in
  web/index.html: `data-t` (an element's text), `data-t-html` (a sentence with a link or bold), `data-t-attr="title,aria-label"`.
- One sentence is one text with `{placeholders}`, never English pieces joined together.
- Text drawn by code is drawn again when the language changes: register with `onLanguage(...)` (look at a neighbour).
- The modules the server imports too (zip.js, savedit.js, dataset.js, saves.js, upload.js) never import i18n.js:
  their errors carry `key: '…'` and `vars` beside the English message; saves.js gets the page's `t` and `tn`.
- A player's or a save's own text (names, clubs, position codes, library entries) is not translated: mark the element
  `data-raw` so the language sweep leaves it.
- A label named in a sentence must use the same words as the label itself.
- The log, the report to the developer and the server's answers stay English. Legal pages (pricing, terms, refunds,
  privacy) are English and binding: list every changed sentence for Max to approve before a deploy.
- Names: GLOSSARY.md (Membership, Cloud Saves, Scout, Squad Hub).

## After changing English

1. `node web/test/i18n-delta.mjs` lists the texts missing from the tables and the stale ones.
2. Translate them: web/test/i18n-brief.md is the brief (one helper per language for a large change; one helper for
   all nine when a few texts changed). Give it the list: `node web/test/i18n-delta.mjs <file.json>`.
3. Each table: `node web/test/i18n-table-check.mjs <code>` prints zeros and exits 0.
4. `I18N_STRICT=1 node --test web/test/i18n.test.js` passes (no missing, no stale, placeholders and plural forms right).
5. `NODE_PATH=$(npm root -g) node web/test/i18n-check.mjs`: the sweep (any English outside a translated text fails).
6. A change to a label: `NODE_PATH=$(npm root -g) node web/test/screens.mjs <folder> <code>` for de, nl and fr at least
   ("everything fits"); look at the shots.
7. Commit the English and the tables, and push them together: the English alone leaves stale entries, and CI on main
   (`npm test`) goes red.

## Pitfalls met

- A check that waits for an exact English sentence (cloud-check, tour-check, scout-check, billing.test.js) fails when
  that sentence is reworded: grep `web/test/` for the old words before pushing, and run the checks the web-checks skill
  names.
- `tn` with a plain string in a table falls back to English: a plural entry is `{ one, …, other }` with every category
  of the language (es, pt, it, fr: one, many, other).
- The sweep's stand-in table marks texts `DE:…:ED`; a new kind of data on the screen needs `data-raw`, not an allow-list
  entry.
