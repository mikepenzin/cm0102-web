# The page in the game's languages (design)

Asked by Max, 22 November 2026.

## Intent

Max sees players from other countries. The game itself speaks nine languages (its Game Settings, Language: the
`Data/*.lng` files: English, Spanish, French, German, Dutch, Italian, Danish, Norwegian, Portuguese), but the page
around it (the start box, the bar, Saved games, the editor, Account, Help, the tour, every message) is English only.
(The game's files hold ten `.lng` files, `swe.lng` among them: language index 10, 0x535930..0x535a60,
docs/research/2026-10-14-start-new-game-inventory.md. So Swedish too.)
The page should speak the player's language where the game can too. To learn which languages the players really
use, the anonymous statistics also count the browser's language.

## Goals

- The page's own text in the nine other languages of the game: `es`, `pt`, `it`, `fr`, `de`, `nl`, `da`, `no`, `sv`.
- The language is the browser's when it is one of these (its first match in `navigator.languages`; `nb` and `nn`
  count as `no`), else English. Help has a Language choice (the nine), kept in localStorage (`cm0102-lang`); it
  changes the page at once, with no reload (the game keeps running).
- A sentence with no translation yet shows in English: a new feature never breaks a language.
- Plurals by each language's own rules (`Intl.PluralRules`).
- The statistics: each beat carries the browser's language (`navigator.language`, at most 16 characters); the admin
  page shows the people of the last 30 days by language (the primary subtag: `pt-BR` counts as `pt`), most first. The
  consent box and the privacy page say that the browser's language is counted. Players who said yes before are not
  asked again (Max, 22 November 2026): the language is coarse, goes with the same random id, and names no one.

## Non-goals

- The game: it has its own Language setting. (Later, maybe: a first start sets the game's Language to the page's.)
- The legal pages (`/terms`, `/privacy`, `/refunds`, `/pricing`): English, as written; a wrong translation of a term
  is a liability. The page's links to them keep their English names, with "(English)" after them in the other
  languages.
- The admin page, the README and other docs, the server's log, the engine log (`page: …` and `recomp: …` lines):
  for the developer, English.
- Texts the server writes into its answers (for example "X was not stored: it is larger than 400 MB"): English in this
  pass; the page shows them as they come. A later pass can turn them into codes the page translates.
- Languages the game does not speak.

## Anti-goals

- A missing or broken translation that leaves a button empty, shows a key, or throws: English is the floor.
- A longer translation (German, Dutch) that breaks a layout: the bar's labels, the tabs, the dialog footers, the phone
  cards and the tour card are checked in each language on a laptop and a phone (`test/screens.mjs`).
- Any text, save name or player data sent to a third party; any script from another site; anything that stops the
  page working offline (the service worker caches the translation files as it caches the page).

## Approaches

- **A. The English sentence as the key** (chosen): `t('Saved in this browser')` in the scripts, `data-t` on the
  page's static text, one file per language mapping the English text to its translation. The code stays readable and
  English stays where it is; a sentence not translated shows in English. Changing an English sentence leaves its
  translations behind: the check below names them.
- B. Named keys (`t('toast.saved')`) with English in a file of its own: the usual way of large apps, but every string
  is renamed, the code reads worse, and a missing key shows a name.
- C. A copy of the page per language, made by a script: the scripts still need A or B for their messages.
- Add-ons that translate the live page (Weglot, Localize; Google's website translator takes no new sites): a monthly
  bill, the page's text (save names among it) sent to a third party, an outside script. The browser's own Translate
  keeps working for anyone meanwhile (the page does not forbid it).
- Library: i18next (the plain-JavaScript counterpart of Angular's ngx-translate) was weighed against a small module of
  the page's own; Max chose the small module. The translation files are the same either way, so i18next can come later.

## How

**`web/i18n.js`** (new):
- `LANGUAGES`: `[{ code: 'en', name: 'English' }, { code: 'es', name: 'Español' }, …]` (each in its own language).
- `pickLanguage({ stored, browser })`: the stored choice if it is one of `LANGUAGES`, else the first of
  `browser` (`navigator.languages`) whose primary subtag is one of them (`nb`, `nn` → `no`), else `'en'`.
- `async setLanguage(code)`: loads `i18n-<code>.js` (an ES module whose default export is the table) unless English,
  sets `document.documentElement.lang`, translates the page's marked text (`apply(document)`), stores the choice, and
  tells the listeners (`onLanguage(fn)`), which re-render what they show.
- `t(english, vars)`: the table's text for `english`, or `english`; `{name}` in it replaced by `vars.name`.
- `tn(count, forms, vars)`: `forms` is `{ one: '…', other: '…' }` in English; the table holds the same key with each
  language's plural categories (`{ one, few, many, other }` as `Intl.PluralRules(code).select(count)` names them);
  `{n}` is the count.
- `apply(root)`: each element with `data-t` gets its translated text (`textContent`; the English is its own content,
  kept in `data-t` on first use, so the element can go back to English). `data-t-html` the same, as HTML, for the few
  texts with a link or bold (from the page's own files only, never from a player). `data-t-attr="title,aria-label,
  placeholder"`: those attributes, by their English values.

**Translation files**: `web/i18n-es.js`, `web/i18n-pt.js`, … (flat names, as every page file: they join `PAGE_FILES`
in `server.mjs`, so the stage, the server and the service worker carry them). Each is `export default { "English":
"translation", … }`. Plural entries hold objects.

**The page's text**: every sentence a player reads goes through `t`, `tn` or `data-t`. A sentence now built from
pieces ("Saved " + name + " in the cloud") becomes one sentence with placeholders ("Saved {name} in the cloud"), so
each language can order it. Text that names the game's own buttons or screens (Restore Saved Game, Game Options)
keeps the game's English name, in quotes: the game starts in English for everyone, so that is what the player sees
on the screen (decided after the builder check).

**Logs and reports stay English** (decided after the builder check). What the player reads is translated at the
point it is shown; what goes to the log, to `stats.report` and to Send to the developer keeps the English, so the
admin page and the reports stay readable and grouped. Where one text does both (`fail(title, detail)` in loader.js,
the `blocked` problems, an error shown by `say` and logged), the call keeps the English and its variables, and the
display translates them. Errors thrown in modules the server imports too (zip.js, savedit.js, dataset.js, saves.js,
upload.js) keep their English `message` and gain `key` and `vars` (the English template and its values); the page
shows `t(e.key ?? e.message, e.vars)`. Those modules never import i18n.js.

**The translations**: written by Claude from the English, in the usual football words of each language. (The game's
`.lng` files hold only the names of clubs, nations, competitions, cities and stadiums,
docs/research/2026-10-13-database-inventory.md; the game's own screen text is in `language.ldb`, whose format is not
researched; not needed here.) A
native speaker among the players should read each language before it is announced; until then Help says "Translated
by machine: tell us a better word" with the report button.

**Checks** (`web/test/i18n.test.js`):
- every English string passed to `t`/`tn` in the page's scripts (found by a parse of the literal first argument)
  and every `data-t` text in `index.html` is in each language's table, or is listed as missing. The regular test
  (CI) prints the missing ones and does not fail on them, so a change that adds an English sentence can go in before
  its translations; `I18N_STRICT=1` fails on any gap, run before a language is announced (decided after the builder
  check);
- every `{placeholder}` of an English text is in its translation, and none more;
- no entry of a table whose English is no longer in the page (left behind by an edited sentence);
- `pickLanguage` cases; plurals for each language (`tn` with 0, 1, 2, 5, 21).

A browser check: the page in German on a laptop and a phone (`?lang=de` or the stored choice): the start box, Saved
games, the editor, Account, Help and the tour have no English text left outside the listed exceptions, and nothing
overflows (`screens.mjs` with a language argument, for the review).

**The statistics** (`stats.js`, `stats.mjs`, `admin.html`): the beat's `lang` field; the person's `lang`; the summary's
`languages: [{ lang, people }]` of the last 30 days; the admin page's list. Players who said no to statistics send
nothing, as now.

## Left to the builder

- The order of the files (the static text of `index.html` first, then the scripts by how much a player sees them).
- The exact Help control (a `<select>` is enough).
- Where a re-render on a language change needs more than `apply(document)` (an open dialog renders anew).
