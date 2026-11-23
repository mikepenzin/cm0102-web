# Levels and Early Access Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every account has a level (Free, Supporter, Squad Player, Club Legend); the admin gives Squad Player or Club
Legend; new features and Library databases are Club Legends only until their date.

**Architecture:** A pure module `web/levels.mjs` maps Buy Me a Coffee level names to site levels; `cloud.mjs` computes
each account's level and keeps gifts with a level; `web/early.mjs` keeps the feature dates in the store and decides
"still early"; the Library adds a date per database and refuses locked downloads; the page reads `early` from
`/api/me` and `/api/library` through one helper (`web/early.js`).

**Tech Stack:** Node 22 ESM server, `node:test` unit tests, Playwright browser checks against `test/demo-site.mjs`,
the page's i18n (`t`, `tn`, tables `web/i18n-<code>.js`).

**Spec:** `docs/superpowers/specs/2026-11-23-early-access-design.md`

## Global Constraints

- Levels, lowest first: `free`, `supporter`, `squad`, `legend`. Each includes everything below it.
- `CM_LEVELS` lists Buy Me a Coffee level names (or ids), lowest to highest, any case; default `Supporter,Squad Player,Club Legend`.
- A paid level name not in `CM_LEVELS` counts as `supporter`, is logged, and is flagged on the Members tab.
- `BMC_CLOUD_LEVELS` without `CM_LEVELS`: its levels are `squad`, other paid levels `supporter`; the server logs that `CM_LEVELS` replaces it.
- Squad Player and up = Membership (Cloud Saves, the full Scout) exactly as `user.cloud` is today; `user.cloud` stays in `/api/me`.
- Admins (CM_ADMIN_EMAILS) are `legend`. Paddle subscriptions, first-version gifts, `CM_CLOUD_FOR=everyone`, `CM_CLOUD_USERS` are `squad`. A gift kept before levels is `squad`.
- Give offers `squad` or `legend`, on any account below `legend`; never `supporter`. The higher of gift and paid counts.
- Early dates are calendar days `YYYY-MM-DD`; a thing opens at 00:00 UTC of its day. Default 14 days after today (UTC).
- A feature key first seen by the server gets today + 14 days, written at once; not written when the store read failed (then the feature counts as open).
- After its day, early access has no effect at all.
- Texts a player reads: in all ten languages, with the tables pushed with the English (skill web-languages). English texts:
  `Early access`, `Club Legends have it now · everyone from {date}`, `Become a Club Legend`, `Sign in`,
  `Club Legends get this database until {date}`; level lines in the account dialog (Task 6).
- The pricing page change goes in its own commit, for Max to approve (not deployed without him).
- No email in logs or in `coffee/last.json`.

## Review Focus

- An account with a Buy Me a Coffee Club Legend membership **and** a Squad Player gift: level `legend`, row shows both — tested in Task 2.
- A Library database whose day is today: open to all from 00:00 UTC today, not locked — tested in Task 4.
- A signed-out player and a server with no accounts (`/api/me` `{ auth: false }`): locked, never an error — tested in Tasks 3 and 4.
- Taking back a Club Legend gift from someone who also pays Squad Player: level falls to `squad`, Cloud Saves stay — tested in Task 2.
- A Club Legend who joins while the page is open: the locked row unlocks without a reload — tested in Task 6 (early-check).

---

### Task 1: Levels

**Files:**
- Create: `web/levels.mjs`
- Test: `web/test/levels.test.js`

**Interfaces:**
- Produces:
  - `LEVELS = ['free', 'supporter', 'squad', 'legend']`
  - `rank(level: string) -> number` (index in LEVELS; unknown → 0)
  - `atLeast(level: string, min: string) -> boolean`
  - `higher(a: string, b: string) -> string`
  - `levelsFromEnv(env) -> { names: string[] | null, legacyCloud: string[] | null }` — `names` from `CM_LEVELS` (lower-cased, trimmed, empty dropped; default `['supporter','squad player','club legend']` when neither setting is there), `legacyCloud` from `BMC_CLOUD_LEVELS` only when `CM_LEVELS` is absent (then `names` is null)
  - `paidLevel(member: { level, levelId }, cfg) -> { level: 'supporter'|'squad'|'legend', known: boolean }` — with `names`: position 0 → supporter, 1 → squad, 2+ → legend, matched by name or `String(levelId)`; not found → `{ level: 'supporter', known: false }`. With `legacyCloud`: in the list (or the list empty) → squad, else supporter, `known: true`.

- [ ] **Step 1: Write the failing tests** (`levels.test.js`):
  - `rank, atLeast, higher`: `atLeast('legend','squad') === true`, `atLeast('supporter','squad') === false`, `higher('squad','legend') === 'legend'`, `rank('nonsense') === 0`.
  - `levelsFromEnv`: `{}` → names `['supporter','squad player','club legend']`; `{ CM_LEVELS: ' Fan , Pro ,VIP,' }` → `['fan','pro','vip']`; `{ BMC_CLOUD_LEVELS: 'Squad Player,Club Legend' }` → `{ names: null, legacyCloud: ['squad player','club legend'] }`; both set → `CM_LEVELS` wins, `legacyCloud: null`.
  - `paidLevel`: default names: `Supporter` → supporter, `squad PLAYER` → squad, `Club Legend` → legend, `{ level: 'Gold', levelId: 9 }` → `{ level: 'supporter', known: false }`, `{ level: 'x', levelId: 2 }` with `CM_LEVELS='1,2,3'` → squad; legacy `['squad player']`: `Club Legend` → supporter, `Squad Player` → squad; legacy `[]` → squad.
- [ ] **Step 2:** `cd web && node --test test/levels.test.js` — Expected: FAIL, cannot find `../levels.mjs`.
- [ ] **Step 3:** Implement `web/levels.mjs` with the exports above.
- [ ] **Step 4:** same command — Expected: PASS.
- [ ] **Step 5:** Commit `web/levels.mjs web/test/levels.test.js` — "Levels: the site's four levels and Buy Me a Coffee's names".

### Task 2: Each account's level; gifts with a level

**Files:**
- Modify: `web/cloud.mjs` (planOf, planOfSub, give, me, memberships, coffeeWebhook's log/last line, coffeeFromEnv, the header comment), `web/server.mjs` (`/api/admin/cloud` body, start-up log line)
- Test: `web/test/coffee.test.js`, `web/test/stats.test.js`, `web/test/cloud.test.js`

**Interfaces:**
- Consumes: Task 1.
- Produces:
  - `coffeeFromEnv(env) -> { secret, url, levels: { names, legacyCloud } } | null` (replaces the old `levels: string[]`); `createCloud({ coffee })` reads it. Admins: `createCloud` gets `isAdmin(email)` (it has it today) and uses it for the level.
  - `planOf(user, fresh) -> { level, cloud: atLeast(level,'squad'), from, paid: { level, until, cancelAt, known } | null, gift: { level, by, at } | null, record, member }` — `from` keeps its values (`'subscription'|'membership'|'given'|null`) for the paths that read it.
  - Gift file `grants/<sub>.json`: `{ sub, email, by, at, level: 'squad'|'legend' }`; missing `level` → `'squad'`.
  - `give(sub, level: 'squad'|'legend'|null, by) -> planOfSub result | null` (null level takes the gift back).
  - `planOfSub(sub) -> { level, cloud, from, given: boolean, givenLevel, givenAt, until, paidLevel, known }`.
  - `/api/me` `user.level` (one of LEVELS); `user.member` gains `siteLevel`.
  - `memberships()` rows gain `siteLevel` and `known`; `levelGives` stays as `atLeast(siteLevel,'squad')`.
  - `POST /api/admin/cloud { id, level }` with `level` in `'squad'|'legend'|null`; the old `{ id, on }` still works (`on: true` → `'squad'`, `false` → null).

- [ ] **Step 1: Write the failing tests:**
  - `coffee.test.js` "levels: a Club Legend, a Squad Player, a Supporter, an unknown level": hooks for four emails (levels `Club Legend`, `Squad Player`, `Supporter`, `Gold`), each signs in; `/api/me` `user.level` = `legend`, `squad`, `supporter`, `supporter`; `user.cloud` = true, true, false, false; `memberships()` row for `Gold` has `known: false`; the last notification's `result` for `Gold` matches `/not in CM_LEVELS/`.
  - `coffee.test.js` "a gift and a paid level: the higher counts; taking the gift back leaves the paid one": the `Squad Player` member gets `{ level: 'legend' }` by `POST /api/admin/cloud` (admin token) → `/api/me` level `legend`; `{ level: null }` → `squad`, `cloud` still true.
  - `coffee.test.js` "BMC_CLOUD_LEVELS alone still works": a second app with `coffee.levels = levelsFromEnv({ BMC_CLOUD_LEVELS: 'Squad Player,Club Legend' })`: `Club Legend` member → `squad`, `Supporter` → `supporter`.
  - `stats.test.js` route test: `give({ cookie: 'admin', 'x-cm': '1' }, { id: 'g:2', level: 'legend' })` → 200 and the stub's `give` called with `'legend'`; `{ id: 'g:2', level: 'supporter' }` → 400; `{ id: 'g:2', on: true }` → called with `'squad'`.
  - `cloud.test.js`: an admin email signs in → `user.level === 'legend'`; a gift file without `level` → `squad`; `CM_CLOUD_FOR=everyone` → `squad`.
- [ ] **Step 2:** `cd web && node --test test/coffee.test.js test/stats.test.js test/cloud.test.js` — Expected: the new tests FAIL (`user.level` undefined, 400 on `level`).
- [ ] **Step 3:** Implement in `cloud.mjs` and `server.mjs` per the Interfaces. The webhook's log and `coffee/last.json` result name the site level (`Membership on (Club Legend)`, `… not in CM_LEVELS: counted as Supporter`). `server.mjs`'s start-up line says `levels Supporter < Squad Player < Club Legend (CM_LEVELS)` or the legacy note.
- [ ] **Step 4:** `cd web && npm test` — Expected: all pass (165 before this plan + the new ones).
- [ ] **Step 5:** Commit — "Levels for every account; the admin gives Squad Player or Club Legend (server)".

### Task 3: Early access: feature dates and `/api/me`

**Files:**
- Create: `web/features.js` (the list), `web/early.mjs` (the rule and the store)
- Modify: `web/server.mjs` (create it, `/api/me` field also in the `{ auth: false }` answer, admin routes), `web/cloud.mjs` (`me` adds `early`)
- Test: `web/test/early.test.js`

**Interfaces:**
- Consumes: Task 2's `planOf` level (`legend` = Club Legend).
- Produces:
  - `web/features.js`: `export const FEATURES = []` — entries `{ key: string, name: string }`, `key` `/^[a-z0-9-]{1,40}$/`. Starts empty (features already out are not listed). The page and the server both import it.
  - `web/early.mjs`:
    - `todayUTC(now = Date.now()) -> 'YYYY-MM-DD'`, `addDays(day, n) -> day`, `validDay(s) -> boolean`
    - `isEarly(day: string|null, now = Date.now()) -> boolean` — true while `day > todayUTC(now)`
    - `createEarly({ s3, features = FEATURES, log }) -> { dates(fresh?) -> Promise<{ [key]: day }>, set(key, day|null) -> Promise<dates>, early(now?) -> Promise<{ [key]: day }> }` — store `early/features.json`; `dates()` writes today+14 for keys it has not seen (only after a successful read, `null` buffer counts as success); a minute's cache; `set(key, null)` writes today (open now). `early()` returns only keys still early.
  - `/api/me` (signed in, signed out, and `{ auth: false }`): `early: { legend: boolean, features: { [key]: day } }`.
  - `GET /api/admin/early` → `{ features: [{ key, name, day, early }], today }`; `PUT /api/admin/early { key, day: 'YYYY-MM-DD'|null }` (admin page header `x-cm: 1` as `/api/admin/cloud` checks) → the same list.

- [ ] **Step 1: Write the failing tests** (`early.test.js`, with `fakeS3` from `test/fakes.mjs`):
  - "a day opens at 00:00 UTC": `isEarly('2026-12-07', Date.parse('2026-12-06T23:59:59Z')) === true`, `isEarly('2026-12-07', Date.parse('2026-12-07T00:00:00Z')) === false`, `isEarly(null) === false`.
  - "a feature first seen gets 14 days, written once": features `[{ key: 'x', name: 'X' }]`, now 2026-11-23 → `dates()` `{ x: '2026-12-07' }`; a second `createEarly` on the same store returns the same day the next day.
  - "a failed read writes nothing": an s3 whose `getBuffer` throws → `dates()` resolves `{}`, no `put`.
  - "set: a day, or open now": `set('x', '2027-01-01')` then `early()` has it; `set('x', null)` → `early()` `{}`.
  - "/api/me says early": with the demo-style app (as `coffee.test.js` builds it, features `[{ key: 'x', name: 'X' }]`): signed out → `{ legend: false, features: { x: <day> } }`; an admin signed in → `legend: true`.
  - "the admin routes": GET lists `x` with `early: true`; PUT without `x-cm` → 403; PUT `{ key: 'x', day: null }` → `early: false`; PUT `{ key: 'nope' }` → 404; PUT `{ key: 'x', day: '7 Dec' }` → 400.
- [ ] **Step 2:** `cd web && node --test test/early.test.js` — Expected: FAIL, cannot find `../early.mjs`.
- [ ] **Step 3:** Implement. `createApp` takes `early` (null: `/api/me`'s `early` is `{ legend: false, features: {} }`); `server.mjs` main creates it when the store is set. Add `early.mjs` and `features.js` to `SERVER_FILES` / the page files the stage copies (check `stage.mjs`'s lists and `stage.test.js`).
- [ ] **Step 4:** `cd web && npm test` — Expected: all pass.
- [ ] **Step 5:** Commit — "Early access: feature dates in the store, /api/me says what is early".

### Task 4: Library databases with an early date

**Files:**
- Modify: `web/library.mjs` (`entryFields`, `handle`, `createLibrary` options), `web/server.mjs` (pass `levelOf`)
- Test: `web/test/library.test.js`

**Interfaces:**
- Consumes: Task 3's `isEarly`, `validDay`; Task 2's level (through `levelOf`).
- Produces:
  - Index entries gain `early: 'YYYY-MM-DD' | null` (missing = null). `entryFields(body)` accepts `early` (`null`, `''` → null; else `validDay` or `why: 'early: a day YYYY-MM-DD, or none'`); `done` and `PATCH` store it.
  - `createLibrary({ …, levelOf = async (req) => 'free' })`.
  - `GET /api/library` entries: as today, plus `early` (only while `isEarly`) and `locked: true` when early and the caller is below `legend`.
  - A locked zip (`GET /api/library/<id>`) and link (`?direct=get`) → `403 { error: 'Club Legends get this database until {date}', key: 'Club Legends get this database until {date}', vars: { date } }`; the card stays public.
  - The admin list (`GET /api/admin/library`) has `early` on every entry as stored.

- [ ] **Step 1: Write the failing tests** (`library.test.js`, in its own pattern):
  - "an early database: listed, locked for a free player, open for a Club Legend": an entry with `early` = today + 3; `levelOf` returns `'free'` → list has `locked: true`, zip 403, `?direct=get` 403 with `vars.date`; card 200; `levelOf` → `'legend'` → no `locked`, zip 200.
  - "its day opens it to all": `early` = `todayUTC()` → no `early`, no `locked` in the list, zip 200 for `'free'`.
  - "the admin sets and clears the day": `done` with `early: '2027-01-01'` stores it; PATCH `early: null` clears it; PATCH `early: 'soon'` → 400.
- [ ] **Step 2:** `cd web && node --test test/library.test.js` — Expected: the new tests FAIL.
- [ ] **Step 3:** Implement. In `server.mjs`, `levelOf = async (req) => { const u = cloud?.userOf(req); return u ? (await cloud.planOf(u)).level : 'free'; }` (export `planOf` from `createCloud` if it is not).
- [ ] **Step 4:** `cd web && npm test` — Expected: all pass.
- [ ] **Step 5:** Commit — "Library: a database can be Club Legends only until its day".

### Task 5: The admin page

**Files:**
- Modify: `web/admin.html`
- Test: create `web/test/early-check.mjs` (admin half here; the page half in Task 6)

**Interfaces:**
- Consumes: Task 2 (`planOfSub` fields, `POST /api/admin/cloud { id, level }`), Task 3 (`/api/admin/early`), Task 4 (library `early`).
- Produces: the admin UI only.

What it shows (spec, "Where the level shows" and "The admin page"):
- People: Membership column `Club Legend · gift`, `Squad Player · paid · to 26 Dec`, `Supporter · paid`, `Free`, `Club Legend · admin`; both when gift and paid (`Squad Player · paid · to 26 Dec, Club Legend · gift`). Give on any account below `legend`: a small menu (`<select>` or two buttons) with Squad Player and Club Legend, then the confirm `Give Club Legend to {email}? It counts from the player's next page load.`; Take back when a gift is there. Chips: All, Accounts, Anonymous, Free, Supporter, Squad Player, Club Legend, With errors, Seen in 7 days (the level chips count accounts). Sort by Membership sorts by level rank, then paid before gift.
- Members tab: a Level column (the site level the name maps to), `not in CM_LEVELS: counted as Supporter` in `warn` when `known` is false; the setup note names `CM_LEVELS` (or the legacy setting).
- Overview: Members card counts Squad Player and up (as now), sub-line `{n} Club Legends · {n} Squad Players · {n} Supporters`.
- Tab **Early access** (`#early`): features (`FEATURES` from the server's list) and Library databases, each with its day, a date field, Save and **Open now**; those still early first, the open ones below, greyed, "opened {day}". A database's day is changed through the library's PATCH.
- Library form: "Club Legends only until" date field, default today + 14 (UTC) for a new database, empty = open to all; Edit shows the stored day.

- [ ] **Step 1: Write the failing check** `early-check.mjs` (pattern of `scout-check.mjs`: `startDemoSite({ adminToken })`, `check(name, ok, detail)`, last line `early check: all ok`), admin part:
  - a Squad Player membership hooked for `a@example.com` and that account signed in once (the demo site's fake Google): People shows `Squad Player · paid`; Give → Club Legend → row `Club Legend · gift` and `Squad Player · paid`; chip Club Legend counts 1; Take back → `Squad Player · paid`.
  - Members tab: a `Gold` membership shows `not in CM_LEVELS`.
  - Early access tab: lists a database added with day today+3 as early; Open now → it moves to the open list.
  - Library form: the date field holds today + 14 for a new database.
- [ ] **Step 2:** `NODE_PATH=$(npm root -g) node web/test/early-check.mjs > early.log 2>&1; grep FAIL early.log` — Expected: FAIL lines (no level text, no tab).
- [ ] **Step 3:** Implement in `admin.html`. The demo site needs `features` and `early` wired (`test/demo-site.mjs`: create `early` on its fake store, pass `levelOf` to the library, admins as today).
- [ ] **Step 4:** same command — Expected: `early check: all ok` for the admin part; `cloud-check.mjs` (admin data, Give) still all ok.
- [ ] **Step 5:** Commit — "Admin: levels in People and Members, Give a level, the Early access tab".

### Task 6: The game page

**Files:**
- Create: `web/early.js` (page helper)
- Modify: `web/setspanel.js` (locked rows, badge, 403), `web/account.js` (level lines), `web/loader.js` (wiring), `web/index.html` (CSS for `.early-badge`, `.lib-locked`), `web/i18n-*.js` (nine tables)
- Test: `web/test/early-check.mjs` (page half), `web/test/i18n-check.mjs`, `web/test/early.test.js` (helper)

**Interfaces:**
- Consumes: Task 3 (`/api/me` `early`, `FEATURES`), Task 4 (library `early`, `locked`, 403 `key`/`vars`), Task 2 (`user.level`).
- Produces:
  - `web/early.js`:
    - `earlyState(me, key) -> 'open' | 'early' | 'locked'` (from `me.early.features[key]` and `me.early.legend`; no `me` or no `early` → `'open'`)
    - `entryState(entry) -> 'open' | 'early' | 'locked'` (a Library entry: `locked` → locked; `early` → early; else open)
    - `earlyDate(day) -> string` (the day in the player's language, as account.js's `day`, read as UTC)
    - `lockedHtml(day, { signedIn, offer }) -> string` — one line `Club Legends have it now · everyone from {date}` and the link `Become a Club Legend` (`offer.url`, new tab), with `Sign in` before it when `signedIn` is false; empty link part when there is no `offer.url`
    - `badgeHtml() -> string` — `<span class="early-badge">Early access</span>`
  - `mountSetsPanel({ …, early: () => ({ signedIn, offer }), signIn })`; it re-asks `/api/library` when the account changes (`refresh()` exposed and called from loader's `account.onAccountChange`).
  - Account dialog lines by `user.level`:
    - legend: `Club Legend: early access to new seasons and features, Cloud Saves and the full Scout.`
    - squad: `Squad Player: Cloud Saves and the full Scout.`
    - supporter: `Supporter: thank you for the support.` (and today's offer of Membership stays)
    - The existing `· Member` / `· Free plan` line stays.

- [ ] **Step 1: Write the failing tests:**
  - `early.test.js` "the page helper": `earlyState(null,'x') === 'open'`; `earlyState({ early: { legend: false, features: { x: '2099-01-01' } } }, 'x') === 'locked'`; with `legend: true` → `'early'`; `entryState({ locked: true, early: 'd' }) === 'locked'`; `lockedHtml('2026-12-07', { signedIn: false, offer: { url: 'https://b/m' } })` contains `everyone from`, `Become a Club Legend`, `Sign in`, `https://b/m`.
  - `early-check.mjs` page part: a database early (today + 3) in the demo library; a free player sees it in Saved games → Databases dimmed with `Club Legends have it now · everyone from` and no Get button; the Club Legend (gift from Task 5's part) sees **Get** and the `Early access` badge and gets it; a free player who becomes Club Legend while the dialog is open (gift + `new BroadcastChannel('cm-account').postMessage({ ok: true })`) sees Get without a reload; the account dialog of the Club Legend says `Club Legend: early access`.
  - `i18n-check.mjs`: the locked line and the badge in German have no English left (its sweep with `DE:…:ED` marks), the date in German form.
- [ ] **Step 2:** run `early-check.mjs` and `i18n-check.mjs` (commands as in Task 5, Step 2) — Expected: FAIL lines.
- [ ] **Step 3:** Implement; translate the new texts into the nine tables (skill web-languages: `web/test/i18n-brief.md`, `i18n-delta.mjs`, `i18n-table-check.mjs`); a 403 `key` is shown through `t(key, vars)`.
- [ ] **Step 4:** `cd web && npm test`; `early-check.mjs`, `i18n-check.mjs`, `scout-check.mjs`, `cloud-check.mjs` — Expected: all pass, `… check: all ok`.
- [ ] **Step 5:** Commit — "Page: early access in the Library, the level in the account dialog, ten languages".

### Task 7: Docs, CI, the pricing draft

**Files:**
- Modify: `web/README.md` (a section "Levels and early access": the four levels, where a level comes from, `CM_LEVELS`, gifts, early days, `web/features.js`: a new feature adds its line, and gates its entry point with `earlyState`/`lockedHtml`), `web/deploy/README.md` (`CM_LEVELS` in the Buy Me a Coffee secrets line, replacing `BMC_CLOUD_LEVELS`; the Members and Early access tabs), `CLAUDE.md` (one line: a new page feature gets an entry in `web/features.js`, see web/README.md "Levels and early access"), `.claude/skills/web-checks/SKILL.md` (row for `early-check.mjs`), `.github/workflows/web.yml` (run `early-check.mjs` after `scout-check.mjs`)
- Modify (own commit): `web/pricing.html` — Club Legend and its early access, drafted for Max; plus its nine translations if the page is translated

- [ ] **Step 1:** Write the docs and CI lines.
- [ ] **Step 2:** `cd web && npm test` and the whole list of checks (skill web-checks) — Expected: all pass.
- [ ] **Step 3:** Commit — "Docs and CI: levels and early access"; push.
- [ ] **Step 4:** Draft the pricing change; commit separately — "Pricing page: Club Legend's early access (for Max to approve)"; push. Tell the user it waits for Max.
