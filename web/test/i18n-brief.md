# Translating the page: the brief for a translator

The brief given to each translator (a person, or a helper agent: one per language, or one for a few languages when
the change is small). Read with web/README.md, "Languages". Make the list of texts to translate with
`node web/test/i18n-delta.mjs <out.json>`, give the translator this file and that list, and check each table with
`node web/test/i18n-table-check.mjs <code>` (it must print zeros and exit 0).

## The page

The site runs the old football game Championship Manager 01/02 in a browser tab. Around the game is our own page: a
start box, a bar of buttons, Saved games, Account (Membership: Cloud Saves and the Scout), Help, Squad Hub (a saved
game's players: view and change them, scout them; data sets for new games), a first-visit tour, messages. Its English
goes through `t('…')`, `tn(n, { one, other })` and `data-t` in index.html (web/i18n.js). The English text is the key.
A translator writes the table of one language: `web/i18n-<code>.js`, and reads index.html and the code when a text's
meaning is unclear (the list names the files each text is in).

## Input

The list from `i18n-delta.mjs`: `missing` (entries `{ en, files, other? }`: `other` marks a plural, `en` is its
one-form, `other` its many-form) and `stale` (English no longer on the page: remove those entries).

## Output: web/i18n-<code>.js, nothing else

Write the translations as a JSON object (English → translation) and make the file with a small node script, so the
quoting is right. Keys and strings are JSON.stringify'd, one entry per line:

```
// <Language name in English> (<code>): the page's text in <language> (web/i18n.js). English is the key.
export default {
  "English": "Translation",
  "{n} save, {size}": { "one": "…", "other": "…" },
};
```

A plural gets an object with every plural category of the language: es, pt, it, fr: one, many, other ("many" is for
round millions: the same text as other); de, nl, da, no, sv: one, other.

## Rules

- Every `{placeholder}` exactly as in the English (same names; it may move). `{n}` in each plural form.
- Markup (`<b>`, `<a href=…>`, `<kbd>`, …) exactly as in the English; only the words between the tags change.
- A page label named inside a sentence uses that label's own translation from the table, word for word: Saved games,
  Databases, Tactics, Older copies, Download, Upload, Play, Use, Squad Hub, Players, Scout, Help, Sign in, Keyboard,
  Right click, Full screen, Send to the developer, Show the tour, Become a member, Save the game file, and the others
  that are `data-t` buttons or headings in index.html or button texts in the code.
- The game's own screens, menus and buttons stay English, as the game shows them (the game itself is English): "Tactics,
  Load" on the tactics screen, Restart Game, Save Game, Exit Game, Continue, Options, Load Game. The text says when it
  means the game's: "on the tactics screen", "in the game", "the game's …".
- "CM 01/02", "Google", "Buy Me a Coffee", "Paddle", "iPad", "iPhone", "Safari", "Chrome", file names, ".sav", ".tct",
  "Update Team" stay as they are. Membership, member, Cloud Saves, Squad Hub and Scout keep the words the table
  already uses for them (look them up in the table before translating).
- `{price}` is the price as a whole ("$5 a month"); in the tour it is " ($5 a month)" with its own leading space and
  brackets: put it right after the button name. `{amount} a month`: translate the period.
- "(English)" follows links to the legal pages, which are in English: "in English", briefly.
- Football words as the language's players say them; attribute names as that language's football games name them.
- Address the player as games in that language do: es tú, pt você (Brazilian Portuguese), it tu, fr vous, de du, nl je,
  da du, no du (Bokmål), sv du. Plain, short, friendly; the English is plain on purpose.
- Labels must fit: the bar's labels, tabs and dialog buttons at most about a third longer than the English
  (`node web/test/screens.mjs <folder> <code>` fails on a label that is cut or leaves the window).

## Report

The check's line per language, and the words chosen for any new name, with any text the translator was unsure of.
