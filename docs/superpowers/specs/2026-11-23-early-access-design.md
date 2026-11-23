# Early access for Club Legends — design

23 November 2026. Approved approach: A, an early-access date per thing, for every new feature.

## Why

The Buy Me a Coffee level **Club Legend** ($10 a month) promises "Early access to new seasons and features, before
everyone else". The first Club Legend joined on 23 November, and the site has no way to keep that promise: every deploy
reaches every player at once, and the only per-player switch is Membership (Cloud Saves, the full Scout).

Success: whenever a new season database or a new feature ships, Club Legends get it at once, everyone else sees it
coming with the date it opens to them, and the developer has nothing to remember for it to open on time.

## Goals

- **Every new feature gets an early-access period**, not only the Library. A new season (a database in the Library)
  is the first case.
- A thing in early access is for **Club Legends only until its date**, then it opens to everyone under its usual rules
  (a Membership feature stays a Membership feature).
- Players who are not Club Legends **see** the thing, marked with the date it opens to them and a way to become a
  Club Legend. Nothing just disappears.
- The admin page sets and changes each date, and can open a thing now.
- Every text a player reads is in the ten languages (skill web-languages).

## Non-goals

- Early versions of the **game** (a new Bosman release, a new engine). One server serves one engine to everyone; that
  would need a second site (approach C, not chosen).
- The other two Club Legend perks: the supporters list and bug reports at the top of the pile.
- Changing who gets Membership, or the Scout's free rows.

## Anti-goals

- A thing locked forever because nobody set or changed its date.
- The Library, which is free and needs no account, turning into a paywall: after its date a database is exactly as
  it is today.
- A Club Legend without early access because of how they got the level (a gift from the admin page).

## Approaches

- **A. An early-access date per thing (chosen).** Each new feature, and each new Library database, carries "Club
  Legends only until <date>". One rule on the server decides who is a Club Legend and whether a thing is still early.
  Small, keeps the promise, and grows with each feature.
- **B. A general switchboard** of features, each with audiences and dates. More flexible than needed for one or two
  things at a time; A's rule lives in one place so it can grow into B later.
- **C. A separate beta site** (a second Fly app) that gets each version first. The only way to give a game change
  early, but for databases and page features it costs a second app and shared saves for nothing more.

## Design

### Who is a Club Legend

A signed-in account is a Club Legend when one of these holds:

- its Buy Me a Coffee membership (members/…, cloud.mjs) is running and its level is in **`CM_EARLY_LEVELS`**, a
  server setting like `BMC_CLOUD_LEVELS` (comma separated, names or ids, any case), default `Club Legend`;
- an admin gave it Club Legend on the admin page (below);
- it is an admin (CM_ADMIN_EMAILS), so the developer sees what Club Legends see.

A player with no account, or a signed-in player who is none of these, is not.

This depends on Buy Me a Coffee's webhook delivering membership events (the problem of 23 November); until it does,
Give is the way.

### Gifts

The admin page's Give (People, an account's row) gives **Membership** or **Club Legend**. A Club Legend gift also
gives Membership (the level includes everything in Squad Player). Take back removes either. The account's row shows
which.

### What is in early access

Two kinds of things, one rule:

1. **A Library database.** Each entry gets `early` (an ISO date, or none). The admin form for adding a database has
   "Club Legends only until" with a date, **14 days ahead by default**, and can be cleared (open to everyone now).
   Editing an entry changes it. Databases already in the Library have none.
2. **A feature of the page.** Features are named in one list in the code, **`web/features.js`**: `{ key, name }` per
   feature, the name in English for the admin page. Building a new feature adds its line there (web/README.md and
   CLAUDE.md say so). The server keeps each feature's date in the store (`early/features.json`, `{ <key>: ISO date }`).
   A key the server sees for the first time gets **14 days from then**, written at once, so no feature waits for the
   developer to remember it. Features already out (Squad Hub, the Scout, Cloud Saves) are not in the list.

A thing is **early** while its date is in the future. After it, early access has no effect at all.

### The server

- One module for the rule (who is a Club Legend, whether a date is still early, the feature dates in the store).
- `/api/me`, for every caller, signed in or not, says `early: { legend: <bool>, features: { <key>: <date> } }`, with
  only the features still early.
- `/api/library` lists every database as today, with `early: <date>` on those still early, and `locked: true` on
  those the caller may not get yet.
- A locked database's zip and its download link are refused with 403 and a message naming the date. Its title card
  stays public (the page shows it on the locked row).
- A feature with its own server route checks the rule there too. A feature that lives only in the page is gated only
  in the page: early access is a perk, not a secret, and a player who forces it open in the browser's tools gets
  nothing the server guards.

### The page

A shared helper turns `/api/me`'s `early` into one of three states for a key or a database: **open** (not early),
**early** (still early, and this player is a Club Legend), **locked** (still early, and this player is not).

- **Early:** the thing works as usual, with a small **Early access** badge, so a Club Legend sees the perk.
- **Locked:** the thing is shown dimmed where it would be, with one line and a link, the same pattern as the Scout's
  dimmed rows:

  ```
  ┌──────────────────────────────────────────────────────────────┐
  │ [card]  Season 2026/27 · October 2026         2026/27   61 MB │
  │         Club Legends have it now · everyone from 7 December   │
  │         [ Become a Club Legend ]                    (dimmed)  │
  └──────────────────────────────────────────────────────────────┘
  ```

  The link opens the Buy Me a Coffee membership page (CM_COFFEE_URL's /membership, as Membership's link does). A
  signed-out player gets the same line and "Sign in" beside the link, since a Club Legend must sign in to get it.
- Each feature decides where its locked line goes (its tab, its button); the line and the link are shared.
- The page asks `/api/me` again when the account changes (as for the Scout), so a new Club Legend sees the thing
  unlocked without a reload.

### The admin page

- A tab **Early access**: each feature of `features.js` and each Library database still early, with its date, a date
  field to change it, and **Open now**. Features and databases already open are listed below, greyed, with the date
  they opened.
- The Library form gets the "Club Legends only until" date (above).
- Members tab: each membership says whether its level counts as Club Legend.

### Wording

New texts (English here; all ten languages): "Early access", "Club Legends have it now · everyone from {date}",
"Become a Club Legend", and the 403 message. The date is shown in the player's language.

The pricing page lists what each level gives; whether it should say early access for Club Legend, and in what words,
is for Max to approve before deploy, like the other legal and pricing pages.

## Left to the builder

- The module and helper names, where the badge and the locked line sit in each place, the store key's exact name.
- Caching: the feature dates are read with the same minute's cache as accounts and memberships.
- Tests in the project's pattern: unit tests for the rule (levels, gifts, admins, dates, first-seen default), the
  Library routes (locked list, 403 on zip and link, open after the date), `/api/me`'s `early`; a browser check for the
  locked row, the badge, the admin tab and a Club Legend gift; the i18n check for the new texts.
