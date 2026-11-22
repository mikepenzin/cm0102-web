# The Page in the Game's Languages: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The page around the game in Spanish, Portuguese, Italian, French, German, Dutch, Danish, Norwegian and Swedish (English stays the floor), and the browser's language counted in the anonymous statistics.

**Architecture:** A small module `web/i18n.js` (`t`, `tn`, `apply`, `setLanguage`) with one translation table per language (`web/i18n-<code>.js`, English text as the key). The page's scripts call `t`/`tn`; `index.html` marks its static text with `data-t`. Logs, reports and modules the server imports stay English; errors from those modules carry `key` and `vars` for the page to translate at display.

**Tech Stack:** Plain ES modules in the browser (no build step), `Intl.PluralRules`, Node's test runner, Playwright checks.

**Spec:** `docs/superpowers/specs/2026-11-22-page-languages-design.md`

## Global Constraints

- Languages: `en` (floor), `es`, `pt`, `it`, `fr`, `de`, `nl`, `da`, `no`, `sv`; `nb` and `nn` count as `no`.
- Choice: localStorage `cm0102-lang`, else the first match in `navigator.languages`, else `en`. A change applies at once, no reload.
- A missing translation shows the English; never a key, an empty control or a throw.
- Logs (`page: …`, `recomp: …`), `stats.report`, Send to the developer, the admin page and the legal pages stay English.
- zip.js, savedit.js, dataset.js, saves.js and upload.js never import i18n.js (the server imports them).
- The game's own button names stay English, in quotes.
- No outside script, no text sent to a third party; the translation files are page files (PAGE_FILES), cached by the service worker.
- Statistics: `lang` = `navigator.language`, at most 16 characters; no re-consent; the consent box and `/privacy` name it.

## Review Focus

- A sentence built from pieces left untranslated half-way ("Saved" + name): every converted message is one template with `{placeholders}`. Pinned in Task 4 by the browser check's "no English left" sweep.
- German and Dutch length breaking the bar, the tabs, the phone cards or the tour card. Pinned in Task 6 (screens per language, overflow check).
- A language change while a dialog is open leaving it half English. Pinned in Task 3 (switch with Saved games open).
- A save name, a club name or a player's text passing through `t` and being "translated" or treated as HTML. Pinned in Task 1 (`t` returns an unknown string as it is; `data-t-html` only from `index.html`).
- An English sentence edited later leaving its translations stale. Pinned in Task 2 (stale-entry check).

---

### Task 1: The i18n module

**Files:**
- Create: `web/i18n.js`
- Test: `web/test/i18n.test.js`

**Interfaces:**
- Produces: `LANGUAGES` (`[{ code, name }]`, ten, each name in its own language); `pickLanguage({ stored, browser }) -> code`; `useTable(code, table)` (for tests and `setLanguage`); `t(english, vars?) -> string`; `tn(count, { one, other }, vars?) -> string` (`{n}` is the count; the table's entry for `one` is an object of plural categories); `apply(root)`; `async setLanguage(code, { load }?)` (`load(code)` returns the table, default `import(\`./i18n-${code}.js\`)`); `onLanguage(fn)`; `language()`.

- [ ] **Step 1: Write the failing tests** in `web/test/i18n.test.js`: `pickLanguage({ stored: 'de' })` → `de`; `({ stored: 'xx', browser: ['pt-BR', 'en'] })` → `pt`; `({ browser: ['nb-NO'] })` → `no`; `({ browser: ['ja'] })` → `en`. With `useTable('de', { 'Saved {name}': '{name} gespeichert' })`: `t('Saved {name}', { name: 'A' })` → `A gespeichert`; `t('Unknown <b>x</b>')` → the same string; `t('Saved {name}')` keeps `{name}` when no var. `tn` with Polish-like categories through a table `{ '{n} save': { one: '{n} Spielstand', other: '{n} Spielstände' } }` for counts 0, 1, 2, 21 in `de`; with no entry, English `one`/`other` by `Intl.PluralRules('en')`.
- [ ] **Step 2: Run** `node --test web/test/i18n.test.js` — FAIL (no module).
- [ ] **Step 3: Implement** `web/i18n.js`. `apply(root)`: `[data-t]` → `textContent = t(el.dataset.t ??= el.textContent.trim())`; `[data-t-html]` the same with `innerHTML` (its English kept in the attribute); `[data-t-attr]` lists attribute names, each translated from its English value kept in `data-t-<name>`. `setLanguage` sets `document.documentElement.lang`, stores `cm0102-lang` (try/catch), calls `apply(document)` and the listeners. No DOM access at import.
- [ ] **Step 4: Run** the test — PASS. Run all unit tests: `ls web/test/*.test.js | xargs node --test` — pass.
- [ ] **Step 5: Commit** "i18n: the page's translation module".

### Task 2: The translation check

**Files:**
- Create: `web/test/i18n-strings.mjs` (shared extraction)
- Modify: `web/test/i18n.test.js`

**Interfaces:**
- Produces: `pageStrings() -> Map<english, Set<file>>` (literal first arguments of `t(`/`tn(` in `web/*.js` but zip.js, savedit.js, dataset.js, saves.js, upload.js, server files; `tn` contributes its `one` and `other` texts; `data-t`/`data-t-html` texts and `data-t-attr` values of `index.html`; `key:` literals of thrown errors in the five shared modules); `tables() -> Map<code, object>` (imports `web/i18n-*.js`).

- [ ] **Step 1: Tests**: for each table, every entry's `{placeholders}` equal its English's (fail); no entry whose English is not in `pageStrings()` (fail, names it); missing entries printed per language, and fail only with `I18N_STRICT=1`.
- [ ] **Step 2: Run** with no tables yet — PASS (nothing to check), prints nothing missing.
- [ ] **Step 3: Commit** "i18n: the check of the translation tables".

### Task 3: Language choice on the page

**Files:**
- Modify: `web/loader.js` (start), `web/index.html` (Help: a Language `<select id="help-lang">`), `web/server.mjs` (PAGE_FILES: `i18n.js` and `i18n-<code>.js` for the nine)
- Create: `web/i18n-es.js` … `web/i18n-sv.js` as `export default {}`
- Test: `web/test/page-check.cjs` (a new block)

**Interfaces:**
- Consumes: Task 1.
- Produces: the page calls `await setLanguage(pickLanguage({ stored, browser: navigator.languages }))` before the start box shows; `?lang=<code>` in the address overrides (for checks and screenshots); dialogs register `onLanguage(render)`.

- [ ] **Step 1: Test** (page-check): with `?lang=de` and a stub table `{ 'How to play here': 'So spielst du hier' }` injected through `i18n-de.js` served by the check, Help's title reads German; the Help select shows ten languages, the current one selected; choosing English with Saved games open turns its heading back to English without a reload (`performance.navigation` unchanged, `window.__fake` the same engine).
- [ ] **Step 2: Run** page-check — FAIL.
- [ ] **Step 3: Implement**; `stage.test.js` still passes with the new PAGE_FILES.
- [ ] **Step 4: Run** page-check and unit tests — PASS.
- [ ] **Step 5: Commit** "i18n: the page picks its language; Help has the choice".

### Task 4: The page's text through t

**Files:**
- Modify: `web/index.html` (every visible text: `data-t`, `data-t-html`, `data-t-attr`), `web/loader.js`, `web/ui.js`, `web/tour.js`, `web/keyboard.js`, `web/touch.js` (none to the game), `web/account.js`, `web/saves.js` (page-side messages only, not its thrown errors), `web/setspanel.js`, `web/tactics.js`, `web/editor.js`, `web/report.js`, `web/play.js`, `web/release.js`, `web/progress.js`, `web/lastvisit.js`, `web/datasets.js`, `web/tacticsync.js`; `web/zip.js`, `web/savedit.js`, `web/dataset.js`, `web/upload.js` (thrown errors gain `key` and `vars`, message unchanged)
- Test: `web/test/page-check.cjs`, `web/test/cloud-check.mjs`, `web/test/editor-check.mjs`, `web/test/tour-check.mjs`, unit tests

**Interfaces:**
- Consumes: Tasks 1–3.
- Produces: `fail(title, detail)` and the other shown-and-logged calls keep English for the log and report and show `t(...)`; `say(text)` callers pass `t(...)`; errors shown use `t(e.key ?? e.message, e.vars)`.

- [ ] **Step 1: Test** (page-check, a new block): with `?lang=de` and a table made by prefixing every English of `pageStrings()` with `DE:`, the start box, the bar's labels and titles, Saved games (three tabs), Account, Help, the tour and three toasts (saved, cloud hint, an upload refused) show only `DE:` texts outside an allow-list (save names, sizes, dates, the game's button names in quotes, legal link names, the brand). The engine log has no `DE:`.
- [ ] **Step 2: Run** — FAIL.
- [ ] **Step 3: Convert** file by file; each message one template with placeholders; the links to the legal pages keep their English names, with " (English)" after them (`t('(English)')`) when the page is not English; run the existing checks after each file group (page, cloud, editor, tour, unit) — they must pass in English unchanged.
- [ ] **Step 4: Run** the new block and all checks — PASS. `node --test web/test/i18n.test.js` lists every string as missing in nine languages (expected).
- [ ] **Step 5: Commit** per file group ("i18n: index.html", "i18n: loader and the bar", "i18n: Saved games", "i18n: Account", "i18n: the editors", "i18n: errors of the shared modules carry their key").

### Task 5: The translations and the statistics

**Files:**
- Modify: `web/i18n-es.js` … `web/i18n-sv.js`; `web/stats.js` (beat `lang`), `web/stats.mjs` (person `lang`, summary `languages`), `web/admin.html` (Languages list), `web/index.html` (consent text), `web/privacy.html`, `web/README.md`
- Test: `web/test/stats.test.js`, `web/test/i18n.test.js` with `I18N_STRICT=1`

- [ ] **Step 1: Test** (stats.test.js): beats with `lang` `pt-BR`, `pt-PT`, `de`, and none, from four people within 30 days and one older → `summary().languages` is `[{ lang: 'pt', people: 2 }, { lang: 'de', people: 1 }, { lang: '?', people: 1 }]`; a `lang` of 40 characters is kept as 16.
- [ ] **Step 2: Run** — FAIL; implement; PASS.
- [ ] **Step 3: Write** the nine tables, and Help's line for languages other than English: "Translated by machine: tell us a better word." with the report button (a `data-t` paragraph shown when `language() !== 'en'`); (every string of `pageStrings()`, football words as each language's players use them; `{placeholders}` kept; plural entries with that language's categories).
- [ ] **Step 4: Run** `I18N_STRICT=1 node --test web/test/i18n.test.js` — PASS (no gaps, no stale, placeholders equal).
- [ ] **Step 5: Commit** "i18n: Spanish … Swedish" and "Statistics: the browser's language".

### Task 6: Screens in each language, docs

**Files:**
- Modify: `web/test/screens.mjs` (a language argument: `node web/test/screens.mjs <folder> de`), `web/test/page-check.cjs` (overflow check), `web/README.md` (a section "Languages"), `CHANGELOG`-style note in README only

- [ ] **Step 1: Test** (page-check): for `de` and `nl` on a laptop and a phone held upright and sideways, no element of the bar, the tabs, the dialog footers, the phone cards or the tour card has `scrollWidth > clientWidth + 1` (text cut) or leaves the window.
- [ ] **Step 2: Run** — fix what overflows (shorter wording in that table first; CSS only where a word cannot shrink).
- [ ] **Step 3: Run** screens.mjs for all nine languages; look at the laptop and phone shots of each.
- [ ] **Step 4: Run** every check: unit, page, cloud, editor, tour, vault — PASS.
- [ ] **Step 5: Commit** "i18n: the screens in every language; README".
