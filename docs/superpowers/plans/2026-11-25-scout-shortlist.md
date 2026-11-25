# Scout to Shortlist Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a Scout player to (and take him off) a human manager's in-game Short List in a saved game, choosing the
manager when the save has two or more.

**Architecture:** A module `web/shortlist.js` reads and edits the save's `scout_manager.dat` as the game's own code
does (research note `docs/research/2026-11-25-shortlist-in-save.md`); Squad Hub's Scout and player panel call it; the
change is saved with the save's other edits.

**Spec:** `docs/superpowers/specs/2026-11-25-scout-shortlist-design.md`

## Global Constraints

- Entries exactly as `PLAYER_SEARCH::add_player` 0x7af720 builds them; capacity ≥ count always (+250 when full); sorted
  by staff id; removal sets bit 1 of +0x20.
- Members only (Squad Player and up; the Scout's `access()` is 'full'); a feature of `web/features.js`, key
  `shortlist`, Club Legends first.
- A save without `scout_manager.dat`, or with no human manager, shows no shortlist control.
- Ten languages (skill web-languages).

## Review Focus

- A manager whose Short List is empty (capacity 0): the first add sets capacity 250.
- A list at its capacity: the add raises it by 250, and the saved member still parses to the byte.
- A player removed then added again: one entry, bit cleared, no duplicate.
- A save with a club report table in a search (+0x190 above -1): parsing does not lose its 0x51a8 bytes.
- The save written, then read again: unchanged searches byte for byte.

---

### Task 1: The model

**Files:** create `web/shortlist.js`, `web/test/shortlist.test.js`; modify `web/test/fakes.mjs` (`fakeSaveGame`
option `managers`: a `scout_manager.dat` with ten searches per manager, list 0 holding given staff ids, and a
`general.dat` long enough for the game date at 0xf85).

**Produces:**
- `scoutManager(game) -> { managers: [{ id, name }], has(managerId, staffId) -> bool, ids(managerId) -> Set,
  add(managerId, staffId) -> bool, remove(managerId, staffId) -> bool } | null` (null: no `scout_manager.dat` or no
  human manager). `add` and `remove` change the member and call `game.file.changed('scout_manager.dat')`.
- `parseSearches(bytes)` and `writeSearches(parsed)` (round trip byte for byte), exported for the tests.

- [ ] Tests first: round trip of a fake member byte for byte; managers listed with names; add (entry bytes as the
  research note's table, sorted place, count, capacity +250 when full, first add on an empty list), remove (bit set,
  `has` false), add again (bit cleared, one entry), a search with a club report table kept; SaveGame written and read
  again keeps the change. Run `node --test test/shortlist.test.js` (in web/): FAIL, then implement, then PASS.
- [ ] The real save (not committed): the same parse on `Hertha BSC 2011-12-11` gives 160 searches, Klaus 140-149, Mike
  150-159, Pirozzi on Klaus's list, Mason on Mike's; round trip identical.
- [ ] Commit.

### Task 2: The page

**Files:** `web/editor.js` (the control and buttons), `web/features.js` (`{ key: 'shortlist', name: 'Scout to
shortlist' }`), `web/index.html` (CSS), `web/i18n-*.js`, `web/test/scout-check.mjs`, `web/test/i18n-check.mjs` if
needed.

- [ ] Browser check first (scout-check, a member, a fake save with two managers): "Shortlist for" shows both; "+
  Shortlist" on a result adds him (the button says "On shortlist ✓", the save counts a change); a click again takes him
  off; the other manager's list is untouched; saved and opened again, he is there. A free player: the button invites to
  Membership. A save with one manager: no picker. Run: FAIL, implement, PASS.
- [ ] Early access: the feature's day (early.js); a member who is not a Club Legend while it is early sees the locked
  line in place of the buttons.
- [ ] Translations; `i18n-check`, `npm test`, scout-check, editor-check.
- [ ] Commit.

### Task 3: Proof with the real engine

- [ ] An edited Hertha save (Mike adds one more player) loads in the native engine, runs a day, saves; the new save's
  Mike list 0 holds the added player (the engine read our entry and kept it).
- [ ] Docs: web/README.md (Squad Hub, the shortlist), the research note's "As built" line.
- [ ] Commit and push.
