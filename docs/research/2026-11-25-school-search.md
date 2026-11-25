# The "School" keyword of the player search

25 November 2026. Read from the clean 3.9.68 `cm0102.exe` (sha256 `0c78e7da…bff86`, `tools/clean_files/manifest.tsv`).

## The request

A player of the browser version types `School` in the player search to hunt for young players and regens, and gets too
many pages. John Locke's community "School search" patch changed the keyword to list players aged 19 or younger with
Potential Ability of at least 180 (as the player and the community describe it; the patch's bytes were not found).

## Where the keyword is

`SearchEngine::find_staff` 0x805310, thiscall (SearchEngine* this, const char* text, int non_players), `ret 8`. The
only reference to the string `"School"` (0xa7df24) is at 0x805546 in it. Before the keywords it checks the person table
`[0xae23bc]`, the result buffer `this+0x34` and the text, each with an error box when missing (0x80531b to 0x805404).
Then it sets the count `this+0x38` to 0 (0x805407) and compares the text with the keywords in turn, each with an exact,
case-sensitive byte compare: `No Club` (0x80541a), `Recurring` (0x8054b6), `School` (0x805546), `Retired` (0x8055e2),
`Retiring` (0x80568b), `Sedgeford` (0x805734). Other text is a name search.

The `School` branch (0x80557a to 0x8055d7):

- It walks every person record: the table `[0xae23bc]`, `[0xae2364]` records of 0x6e bytes.
- It keeps a record whose kind byte `+0x5f` is 3, or, with `non_players` set, 1, or, without it, 2.
- It keeps it only while the byte `+0x18` is below 18: `cmp byte [ebp+0x18], 0x12; jge` (signed). In a running game
  `+0x18` is the person's age (the save editor reads it as such: `web/savedit.js`, `read`).
- Each kept record's address goes to the result buffer (`[this+0x34]`, a pointer each), and `this+0x38` counts them.
- It returns with `eax` = `[0xae2364]` (the last value loaded), `ret 8`.

## Potential Ability

A person's `+0x61` holds the address of the player record (0 for a non-player; the runtime reads it so in
`season_loan_keepers.c`, `groups.c`, `season_national_fill.c`). In the player record `+5` is Current Ability and `+7`
Potential Ability, 16-bit: the same offsets as `player.dat` in a saved game (`web/savedit.js`, `players`), and the game
reads a 16-bit value at `+7` of the `+0x61` record in 39 places (for example 0x4c845e `cmp word [ecx+7], 0x64`,
0x4d1a21, 0x4d3cee).

## A change that matches the patch

Wrap `find_staff` 0x805310 (RT_WRAP): when the text is exactly `School`, and the checks of the start pass (table,
buffer, text), do the branch's walk with the new rule, and return as it does (`eax` = `[0xae2364]`, `ret 8`); any other
text goes to the original. The walk, the kinds and the result buffer stay as above; only the test changes:

- a player search (`non_players` 0): age (`+0x18`) 19 or less, and a player record (`+0x61`) whose Potential Ability
  (`+7`) is 180 or more;
- a staff search (`non_players` set): the original rule (under 18), since a non-player has no Potential Ability.

A search lists people and changes no game decision, so a same-save run must give the same save with the change.
