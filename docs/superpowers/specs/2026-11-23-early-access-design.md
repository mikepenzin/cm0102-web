# Levels and early access for Club Legends — design

23 November 2026. Approved approach: A, an early-access date per thing, for every new feature. Each account has a
level, and the admin can give Squad Player or Club Legend.

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
- **Every account has a level** (Free, Supporter, Squad Player, Club Legend), and the site treats them apart: the
  admin page shows it, the admin can give Squad Player or Club Legend, the player sees their own.
- Every text a player reads is in the ten languages (skill web-languages).

## Non-goals

- Early versions of the **game** (a new Bosman release, a new engine). One server serves one engine to everyone; that
  would need a second site (approach C, not chosen).
- The other two Club Legend perks: the supporters list and bug reports at the top of the pile.
- Changing what a player can do today: who gets Cloud Saves and the full Scout stays as it is, and the Scout's free
  rows stay 3.

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

### Levels

Every account has one level, each including everything below it:

| Level | Buy Me a Coffee | What it gets on the site |
|---|---|---|
| Free | — | what everyone gets today (the Scout's first 3 rows, the Library) |
| Supporter | $3 | the same as Free; shown as a thank-you, no site feature (my call) |
| Squad Player | $5 | Membership: Cloud Saves and the full Scout |
| Club Legend | $10 | Squad Player, plus early access |

A player with no account is Free. "Membership" stays the name of what Squad Player and up get (the plan's name on the
page, the pricing page); the level is shown beside it.

**Where an account's level comes from**, the highest of:

- its Buy Me a Coffee membership (members/…, cloud.mjs), while it runs: its level name, read against **`CM_LEVELS`**,
  a server setting listing the Buy Me a Coffee level names (or ids) from lowest to highest, default
  `Supporter,Squad Player,Club Legend`, any case. A level name not in the list counts as Supporter (paid, unknown) and
  the server logs it; the Members tab flags it.
- a gift from the admin (below);
- a Paddle subscription or a gift of the first version (both give Membership today): Squad Player;
- an admin (CM_ADMIN_EMAILS): Club Legend, so the developer sees what Club Legends see;
- the server's `CM_CLOUD_FOR=everyone` or `CM_CLOUD_USERS` (emails), which give Cloud Saves today: Squad Player.

`CM_LEVELS` replaces `BMC_CLOUD_LEVELS`, which said which levels give Cloud Saves: now that is "Squad Player and up".
A server that still has `BMC_CLOUD_LEVELS` and no `CM_LEVELS` keeps working as before (those levels are Squad
Player, the others Supporter) and logs that `CM_LEVELS` replaces it. The code asks for "Squad Player and up" or "Club
Legend", never a Buy Me a Coffee name, so renaming a level there is a change of `CM_LEVELS` only.

This depends on Buy Me a Coffee's webhook delivering membership events (the problem of 23 November); until it does,
a gift is the way.

### Gifts

The admin page's **Give** (People, an account's row) gives **Squad Player** or **Club Legend**, offered on any
account whose level is below Club Legend: a choice of the two, then the same confirm as today. **Take back** removes
the gift. A gift is kept where gifts are kept now (`grants/<sub>.json`), with its level; a gift kept before levels
existed is Squad Player. A gift and a paid membership: the higher counts, and the row shows both ("Squad Player · paid
· to 26 Dec, Club Legend · gift").

### Where the level shows

- **Admin, People:** the Membership column shows the level and where it comes from (paid, gift, admin), with the
  date a paid one runs to; the chips filter by level (Free, Supporter, Squad Player, Club Legend) in place of the one
  "Members" chip; sorting by Membership sorts by level.

  ```
  terry…@gmail.com    Squad Player · paid · to 26 Dec
  sig…@gmail.com      Club Legend · gift                 [Take back]
  someone@…           Free                               [Give ▾]
  ```
- **Admin, Members:** each Buy Me a Coffee membership with the site level its name maps to, and a warning when the
  name is not in `CM_LEVELS`.
- **Admin, Overview:** the Members card counts Squad Player and up, as now; a line under it splits it by level.
- **The player's account dialog:** their level and what it gives, for example "Club Legend: early access, Cloud
  Saves and the full Scout", or "Supporter: thank you for the support". A Free or Supporter player keeps today's offer
  of Membership.
- `/api/me` gives the level (`user.level`: `free`, `supporter`, `squad`, `legend`); `user.cloud` stays, for the
  pages and checks that read it.

### What is in early access

Two kinds of things, one rule:

1. **A Library database.** Each entry gets `early` (an ISO date, or none). The admin form for adding a database has
   "Club Legends only until" with a date, **14 days ahead by default**, and can be cleared (open to everyone now).
   Editing an entry changes it. Databases already in the Library have none.
2. **A feature of the page.** Features are named in one list in the code, **`web/features.js`**: `{ key, name }` per
   feature, the name in English for the admin page. Building a new feature adds its line there (web/README.md and
   CLAUDE.md say so). The server keeps each feature's date in the store (`early/features.json`, `{ <key>: ISO date }`).
   A key the server sees for the first time gets **14 days from then**, written at once (not when reading the store
failed: then the feature counts as open until the next read), so no feature waits for the
   developer to remember it. Features already out (Squad Hub, the Scout, Cloud Saves) are not in the list.

A thing is **early** while its date is in the future. After it, early access has no effect at all.

**A date is a calendar day** (`YYYY-MM-DD`), and the thing opens at the start of that day, 00:00 UTC, so "everyone from
7 December" is true everywhere from the morning of the 7th (in the UK, at midnight). The store holds the day; the
admin's field is a date field; Open now writes today; the first-seen default is today + 14 days. The page shows the
day in the player's language. (My call.)

### The server

- One module for the rule (who is a Club Legend, whether a date is still early, the feature dates in the store).
- `/api/me`, for every caller, signed in or not (and in server.mjs's answer for a server without accounts: no one is
  a Club Legend there, so early things are locked to all until their date), says `early: { legend: <bool>, features: { <key>: <date> } }`, with
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
is for Max to approve before deploy, like the other legal and pricing pages. Only that change waits: it goes in its own
commit, so the rest can be deployed without it. (My call.)

## Left to the builder

- The module and helper names, where the badge and the locked line sit in each place, the store key's exact name.
- Caching: the feature dates are read with the same minute's cache as accounts and memberships.
- Tests in the project's pattern: unit tests for the rule (levels, gifts, admins, dates, first-seen default), the
  Library routes (locked list, 403 on zip and link, open after the date), `/api/me`'s `early`; a browser check for the
  locked row, the badge, the admin tab and a Club Legend gift; the i18n check for the new texts.
