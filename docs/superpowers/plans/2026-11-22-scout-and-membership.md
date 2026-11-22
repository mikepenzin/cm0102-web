# The Scout, and Membership — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A Scout tab in the Editor that finds a saved game's players by age, position, potential, ability and
attributes — whole for members, the first 3 for free players — and the paid plan renamed Membership (Cloud Saves
and the Scout), in all ten languages.

**Architecture:** A DOM-free module `web/scout.js` builds an index of a saved game's players once (from
`SaveGame`, savedit.js) and answers filters fast; it also decides access (member, free, or a server where nothing
can be bought) and the free cut. `web/editor.js` draws the Scout tab over its player list and opens a result in its
form. The rename is text: page, tour, admin, legal pages; then the nine tables.

**Tech Stack:** plain ES modules in the browser and Node; node:test; Playwright checks on the stand-in engine.

**Spec:** docs/superpowers/specs/2026-11-22-scout-and-membership-design.md

## Global Constraints

- Work on main (Max's standing instruction), commit and push per task.
- The Scout is on a saved game only; a data set shows no Scout tab.
- Members see up to 100 matches; free players the first 3; the hidden matches' data never enters the page (DOM,
  attributes, titles, aria-labels).
- Sort: PA, then CA (both high first), then name. Position counts a rating of 15 or more. Attribute values are the
  shown 1–20 values (`shown()`, savedit.js; the game's 0x543540, cited in savedit.js's header).
- Age in a saved game: staff +0x18 (savedit.js `read`; rewritten daily by 0x53ab90, called at 0x5cc850,
  docs/research/2026-10-17-season-date-shifts.md).
- Whole Scout when: `me.user.cloud`; or no accounts (`!me.auth`); or nothing can be bought (offer not `free` and with
  no `coffee`, no `paddle`, no `url`). Unknown `me` (offline, not answered yet): the last answer kept in localStorage
  `cm0102-member` ('1' or '0'); none kept: free.
- Free invitation: "The Scout is part of Membership: {count} more players match." with the Account dialog's own
  button for the account as it is: signed out "Sign in"; signed in "Become a member"; `offer.free` "Sign in with
  Google, free". It opens the Account dialog.
- Texts through `t`/`tn`/`data-t` (web/README.md, "Languages"); `savedit.js` never imports i18n.js.
- The legal pages' changed wording is listed for Max to approve before a deploy (the final report).
- Domain words: GLOSSARY.md.

## Review Focus

- A hidden match's name or numbers reaching the page through a title, an aria-label, a data- attribute or a stale
  list (the Players list while the Scout shows). Pinned in Task 2 (`scout-check.mjs`: no hidden name anywhere in
  `document.documentElement.outerHTML`).
- A member offline, or `/api/me` failing, cut to 3. Pinned in Task 1 (`scoutAccess` with `me` null and the kept
  answer) and Task 2 (`/api/me` failing in the browser check).
- A large save (60,000 players and more) freezing the page while typing a filter. Pinned in Task 1 (index and a
  search of 60,000 players within budget in Node) and Task 2 (filters wait a pause before searching).
- A language change while the Scout is open leaving it half English. Pinned in Task 4 (the sweep opens the Scout).
- A player with odd numbers (PA below 0, a missing club, an age of 0) breaking a filter or a row. Pinned in Task 1.

---

### Task 1: The Scout's model (`web/scout.js`)

**Files:**
- Create: `web/scout.js`, `web/test/scout.test.js`
- Modify: `web/test/fakes.mjs` only if a fake needs an option (e.g. `age` per player, `positions`)
- Modify: `web/server.mjs` PAGE_FILES (add `scout.js`)

**Interfaces:**
- Produces:
  - `scoutIndex(game: SaveGame) -> Index` — every player of `game.players()` with `{ row, name, club, pos, age, ca,
    pa, positions: number[12], values: Uint8Array }` (values in `ATTRIBUTES` order, the shown 1–20). Built once.
  - `scout(index, filters) -> { rows, total }` — filters `{ ageMin, ageMax, position, paMin, caMin, attrs }`
    (`position`: index 0..11 into savedit's `POSITIONS` or null; `attrs`: up to 3 `{ name, min }`; blank fields
    null). `rows`: the matches sorted, at most 100, each `{ row, name, club, pos, age, ca, pa, values: { [name]: n } }`
    with only the filtered attributes in `values`; `total`: all matches.
  - `scoutAccess(me, kept) -> 'full' | 'free'` and `keptAccess(me) -> '1' | '0' | null` (what to keep in localStorage).
  - `freeCut(result, access) -> { rows, held }` — `rows`: all (full) or the first 3 (free); `held`: how many matches
    are not shown (`total - rows.length`).

- [ ] **Step 1: Write the failing tests** (`web/test/scout.test.js`, on `fakeSaveGame` with players made for it):
  - `scout: age, position, PA, CA and attributes filter; sorted by PA, then CA, then name; at most 100` — five
    players (ages 18, 19, 21, 25, 30; PA 190, 170, 170, 150, 120; one GK) → `{ ageMax: 21 }` gives the three
    youngest in PA order with the CA tie broken; `{ position: 6 }` (Attacker) only those rating 15+ there;
    `{ attrs: [{ name: 'Finishing', min: 15 }] }` only those showing 15+ and `values` `{ Finishing: n }`; 150
    matching players → `rows.length === 100`, `total === 150`.
  - `scout: odd players do not break it` — PA -1 is left out by `paMin` and kept with no PA filter; no club gives
    `club: ''`; age 0 is left out by `ageMin: 16`.
  - `scoutAccess: member, no accounts, nothing to buy, free, and the kept answer` —
    `{ auth: true, user: { cloud: true } }` → full; `{ auth: false }` → full;
    `{ auth: true, user: null, offer: { price: 'x', free: false } }` → full;
    `{ auth: true, user: null, offer: { coffee: true, url: 'https://…' } }` → free; `null` with kept '1' → full,
    with kept '0' or null → free; `keptAccess` gives '1' for a member, '0' for a known non-member, null for null.
  - `freeCut: 3 for free, all for full, held counted` — a result of 47 → free `{ rows: 3, held: 44 }`, full
    `{ rows: 47, held: 0 }`; a result of 2 → free `{ rows: 2, held: 0 }`.
  - `scout is fast: a save of 60,000 players indexed and searched within budget` — a fake save of 60,000 players:
    `scoutIndex` under 1500 ms and a 3-attribute search under 150 ms in Node (record both in the test's message).
- [ ] **Step 2: Run** `node --test web/test/scout.test.js` — FAIL (no module).
- [ ] **Step 3: Implement** `web/scout.js` (no DOM, no i18n.js): the index reads each player once (`game.read(row)` or
  the same bytes directly, as long as values equal `read`'s `attrs`); a search is a filter over typed arrays and a
  sort of the matches only.
- [ ] **Step 4: Run** the test file, then `ls web/test/*.test.js | xargs node --test` — PASS.
- [ ] **Step 5: Commit** "Scout: the model (filters, access, the free cut)".

### Task 2: The Scout tab in the Editor

**Files:**
- Modify: `web/index.html` (the tabs over `.ed-list`, the Scout panel: filters, results, the invitation; CSS for a
  laptop and a phone), `web/editor.js` (the tab, the search with a pause, opening a result, Back on a phone, the
  Players list emptied while the Scout shows, `onLanguage` redraw), `web/loader.js` (pass `access: () =>
  scoutAccess(account.me, kept)` and `invite: () => account.open()` to `mountEditor`; keep `cm0102-member` from each
  `/api/me` answer), `web/account.js` (expose the last `me` if it does not already)
- Create: `web/test/scout-check.mjs` (the editor check's server and fake save, plus Playwright routes for `/api/me`);
  add it to `.github/workflows/web.yml` next to editor-check

**Interfaces:**
- Consumes: Task 1's `scoutIndex`, `scout`, `scoutAccess`, `keptAccess`, `freeCut`.
- Produces: `mountEditor({ …, access = () => 'full', invite = () => {} })`.

- [ ] **Step 1: Write the failing browser check** `scout-check.mjs`:
  - a save with 10 players open in the Editor shows tabs Players and Scout; a data set open shows no Scout tab;
  - `/api/me` routed to a free account with a Buy Me a Coffee offer: Scout with no filter shows 3 full rows, dimmed
    placeholder rows, "7 more players match" and a "Become a member" button that opens the Account dialog; none of
    the 7 hidden players' names is in `document.documentElement.outerHTML`; the Players list is empty while the
    Scout shows;
  - a filter (Finishing ≥ 15) narrows the rows; a result opens the player's form; on a phone (iPhone 13) Back returns
    to the results with the filter kept;
  - `/api/me` routed to a member (`user.cloud: true`): all 10 rows, no invitation; then `/api/me` failing (route
    abort) after a member answer: still all 10 (the kept answer);
  - signed out (`user: null`): the invitation's button reads "Sign in";
  - no page errors.
- [ ] **Step 2: Run** `NODE_PATH=$(npm root -g) node web/test/scout-check.mjs` — FAIL.
- [ ] **Step 3: Implement** the tab, the panel and the wiring. The placeholder rows are blank shapes (no text). A
  search runs 250 ms after the last change of a filter. The index is built when the Scout tab is first opened on a
  save (a "Working…" line while it builds) and dropped when the save closes.
- [ ] **Step 4: Run** scout-check, editor-check, i18n-check and the unit tests — PASS.
- [ ] **Step 5: Commit** "Scout: the tab in the Editor; free players see 3".

### Task 3: Membership, the rename (English)

**Files:**
- Modify: `web/account.js` (the plans: the paid plan's title "Membership", its list "Cloud Saves: your saves on every
  computer and iPad, backed up after each save, up to {n} saves" and "Scout: find players by age, potential and
  attributes"; both buy buttons "Become a member · {price}"; every sentence that calls the paid plan "Cloud Saves"),
  `web/tour.js` (the account-open step: title "Membership", `cloudStepText` saying what Membership holds and how to
  become a member; the editor-open step mentions the Scout), `web/loader.js` (the start box's add-on note and the
  cloud hint), `web/index.html` (Help's Cloud saves part), `web/admin.html` (labels that count paying accounts:
  "Members"), `web/pricing.html`, `web/terms.html`, `web/refunds.html`, `web/privacy.html` (the plan's name and what
  it holds), and any server text in `web/cloud.mjs`/`web/billing.mjs` that the page shows to a player
- Test: the existing unit tests, `cloud-check.mjs`, `tour-check.mjs`, `page-check.cjs` — update the expected texts

**Interfaces:**
- Consumes: nothing new. Produces: the English that Task 4 translates.

- [ ] **Step 1: Change the tests' expected English first** (e.g. cloud-check's "Get Cloud Saves" → "Become a member",
  tour-check's step titles, pricing page tests) — run them — FAIL.
- [ ] **Step 2: Rename** by this rule: the paid plan and its purchase are "Membership" / "a member" / "Become a
  member"; the feature (saves in the cloud, its sync messages, "Cloud Saves are on") stays "Cloud Saves". The server's
  stored flag `cloud` and the environment names do not change.
- [ ] **Step 3: Run** the unit tests, cloud-check, tour-check, page-check, i18n-check (English strings may be missing
  from the tables now: allowed until Task 4) — PASS.
- [ ] **Step 4: Write** the legal pages' changed sentences, before and after, into the task's report for Max.
- [ ] **Step 5: Commit** "Membership: Cloud Saves and the Scout".

### Task 4: The nine languages

**Files:**
- Modify: `web/i18n-{es,pt,it,fr,de,nl,da,no,sv}.js`, `web/test/i18n-check.mjs` (the sweep opens the Editor on the
  fake save and its Scout tab as a free player), `web/test/screens.mjs` (a shot of the Scout, free, on each device)
- Test: `I18N_STRICT=1 node --test web/test/i18n.test.js`, i18n-check, screens in every language

- [ ] **Step 1: Extend** i18n-check's sweep and screens.mjs to the Scout — run — FAIL (new English untranslated).
- [ ] **Step 2: Translate** every missing and stale entry in the nine tables (the brief of the languages pass:
  scratchpad `i18n/BRIEF.md`, with the list of new and changed English; one helper per language; "Membership" and
  "Scout" a short natural name, the same everywhere in a language; entries whose English left the page removed).
- [ ] **Step 3: Run** the strict check, i18n-check, and screens.mjs in all nine languages ("everything fits") — PASS.
- [ ] **Step 4: Commit** "i18n: the Scout and Membership in nine languages".

### Task 5: Docs

**Files:**
- Modify: `web/README.md` (a section "The Scout" after "Edit a saved game"; "Cloud saves" says Membership holds them;
  the tests list gains scout.test.js and scout-check.mjs), `GLOSSARY.md` (Membership, Scout)

- [ ] **Step 1: Write** the docs; run every check once: unit, page, cloud, editor, scout, tour, i18n (strict) — PASS.
- [ ] **Step 2: Commit** "Docs: the Scout and Membership".
