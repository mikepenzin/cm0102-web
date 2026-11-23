# Online games page
Status: Ready

Interview with Max on 13 November 2026 (grilling, questions 1 to 12). Max chose mockup C, the matchday style.

## Problem

A player who wants to play online must start the game and open Join Network Game to see if anyone is playing. Nobody can see from outside the game how much online play there is. Max decided not to sample the lobby from his Mac (13 November 2026). The lobby keeps only the games that are open now, so there is no record of earlier play.

Hosts may not know that an online game with no game password is open to anyone who sees it in the game list.

## Solution

The site moves to its own domain, `https://cm0102bosman.co.uk`. A new page, `/online.html`, shows the open online games, each with a Join button and its invite link. Below the list, the page shows stats about online play: what is on now, today, the last 7 days and all time. A note above the list says that a game with no game password is open to anyone.

The lobby keeps a small set of counters for each UTC day. They hold no names of people and no game codes. A new endpoint serves them to the page.

## User stories

1. As a player, I want to see the open online games on the site, so that I know if there is a game to join before I start Bosman Edition.
2. As a player, I want a Join button on each game, so that one click opens Bosman Edition and joins that game.
3. As a player, I want to copy a game's invite link, so that I can send it to a friend.
4. As a player, I want to see which clubs people manage in each game, so that I can pick a game with a club free that I want.
5. As a player, I want to see the places left in each game, so that I do not try to join a full game.
6. As a player with an older or newer version, I want a game on another version to say "Needs 0.7.1" (the host's version), so that I know which download joins it.
7. As a player, I want a lock and "Password needed" on a game with a game password, so that I know I must ask the host for it.
8. As a host, I want to read that a game with no game password is open to anyone, and where to set the password, so that I keep my game to my friends.
9. As a host, I want the page to show no host name and no manager name, so that my name stays in the game only.
10. As a visitor, I want stats that are fun to read (games started, the busiest moment, hours played, the furthest season, the clubs people manage most), so that I see that people play online.
11. As a visitor, I want the list and the "now" figures to update while the page is open, so that I do not have to reload.
12. As a visitor on a phone, I want the page to fit the screen, so that I can read it without sideways scroll.
13. As a visitor when no game is open, I want "No open games right now." and how to host one, so that the page is not empty.
14. As a visitor when the online service is full for the day or down, I want the page to say so, so that I do not think the page is broken.
15. As a visitor who turned off motion, I want the vidiprinter to show its lines at once, so that nothing moves.
16. As a player with the old github.io address, I want it to send me to the new domain, so that my bookmark still works.
17. As Max, I want the stats to cost little against the free plan, so that online play does not hit the daily limit sooner.

## Decisions

### Domain (Q1, Q10, Q11)

- The whole site moves to `https://cm0102bosman.co.uk` with a GitHub Pages custom domain. The DNS records go in the Cloudflare zone `cm0102bosman.co.uk`, which already carries `online.cm0102bosman.co.uk` for the Worker. HTTPS is enforced. GitHub sends the old `maxevans60.github.io/cm0102-bosman-edition/` address to the new one.
- Links to the site in the README, the invite link page and the release notes change to the new domain.
- The page is `https://cm0102bosman.co.uk/online.html`. It reads the lobby at `https://online.cm0102bosman.co.uk` from the browser.

### Lobby API (contract version 1, additions only)

- `GET /v1/games` and the new `GET /v1/stats` send `Access-Control-Allow-Origin: https://cm0102bosman.co.uk`. No other origin, no credentials, no other endpoint. A preflight is not needed, because both are simple GET requests.
- `GET /v1/stats` gives the stats below as JSON with `X-Bosman-Api: 1`. The Worker keeps the answer in memory for 60 s. Errors use the existing codes (`full_today`, `unavailable`).
- The game does not call `/v1/stats`. No game build changes (Q8).

### Stats the lobby keeps (Q2, Q3)

The lobby Durable Object keeps one stats row for each UTC day, in its own storage, and one all-time row. It updates them in memory and writes the changed rows at most once a minute. It keeps the rows forever. A row holds:

- Games started: a register with no `code` and `hostToken`. A register again of an open game, after an eviction or `unknown_game`, does not count.
- Most games open at once, and the minute when it happened.
- Most managers online at once (the sum of `managersOnline` over the open games), and the minute.
- Hours played by human managers: at each heartbeat, `managersOnline` times the seconds since the game's last heartbeat, with at most 60 s for one heartbeat.
- Clubs that people manage: each club name in `humanClubs` counts once for each game in which it appears, on the day it first appears in that game. The page shows the top 5 of all time.
- Start seasons and databases: counted once for each game started.
- The furthest season: the highest season reached by the game date (`day`) of any game. A date from July onwards is in the season that starts that year.

The rows hold no game name, no host name, no manager name, no game code, no game id and no internet address (Q3: no manager names). Club and database names have already passed the word filter.

"Now" comes from the open games: the number of open games, the managers online and the human clubs.

Stats start on the day the Worker change deploys. The page says "since <date>".

### What the page shows (Q5, Q6, Q7, Q12, mockup C)

The layout follows mockup C (`online-c.html` in `lead-notes/online-games-mockup/`, outside the repo, matchday style): a LIVE NOW scoreboard, then OPEN GAMES as a fixture list, then THE VIDIPRINTER with the stats lines, then the clubs table and the records table.

- Scoreboard: open games, managers online, clubs managed by people.
- The note above the list, word for word: "If you host an online game with no game password, anyone can join it. Set a game password on the Network tab to keep it to your friends."
- Each game row: start season, game date, game name, human clubs, managers online, places left, a Join button (`cm0102://join/<code>`) and a Copy invite link button (`https://online.cm0102bosman.co.uk/join/<code>`). No host name.
- A game with a game password shows a lock and "Password needed". It stays in the list (Q6).
- A game whose version is not the newest release shows "Needs <its version>" (Q12). The page holds the newest version as text, which the release slice of each release updates with the rest of the site text.
- Vidiprinter lines: games started and hours played for today, the last 7 days and all time, the busiest moment of each, and the furthest season. With reduced motion, the lines show at once.
- Clubs table: the top 5 clubs with the number of games. Records table: furthest season, top start season, top database, and the share of games on each.
- Times show in UK time (Europe/London), to the minute.
- Empty list: "No open games right now." and one line on how to host one.
- `full_today`: "Online games are full today. They open again at 00:00 UTC." Any other failure: "The online games are not available right now." The rest of the page stays.

### Freshness and cost (Q4)

- The page reads `/v1/games` and `/v1/stats` when it opens, then every 60 s while the tab is visible. It stops when the tab is hidden.
- One open tab costs about 2 Worker requests a minute and at most 1 lobby request a minute for the stats. The list already has the Worker's 2 s cache.

### Nav (Q9)

The site header gets an "Online" link to `/online.html`. The "New in 0.7.0" section gets a link to it too.

### Release (Q8)

This ships apart from the game versions, with no game build and no version number. The Worker deploys first, then the site. Each deploy needs Max's yes.

## Testing

- Seam 1, the lobby: the Worker tests in `server/test/` (Vitest in workerd). New tests call the Worker as the page does and check `/v1/stats`: games started (a register again does not count), peaks with their minute, hours played with the 60 s cap, clubs counted once for each game, the furthest season across July, the day change at 00:00 UTC, the write cap of once a minute, the stats after an eviction, and the CORS header on `/v1/games` and `/v1/stats` only. Similar tests: `lobby.test.ts`, `robust.test.ts`.
- `privacy.test.ts` gets the stats rows: no host name, game name, manager name, code, game id or test address in storage or in the `/v1/stats` answer.
- Seam 2, the page: there is no site test suite. A headless browser check loads `online.html` against a local Worker (`wrangler dev`) with fake games: full list, empty list, `full_today`, password game, older version, 375 px width with no sideways scroll, reduced motion. Screenshots about 800 px wide.

## Out of scope

- A lobby sampler on Max's Mac (Max, 13 November 2026). `online-stats/stats.py` stays a manual tool.
- A private game that does not show in the list. The game has no such setting. The game password is the way to keep a game to friends.
- Host names, manager names and any chart of play over time.
- Changes to the game, its game list or the invite link page, apart from the site address in its links.
- A light mode for the site.

## Open decisions

None.

## Slices

### 1. Site on cm0102bosman.co.uk
Blocked by: none
Delivers: the site opens at `https://cm0102bosman.co.uk` with HTTPS, and the old github.io address sends visitors there. Links in the README and the invite link page use the new address.
Merge danger: two-way door (the custom domain and the DNS records can be removed), but live DNS and a page every visitor sees. Blast radius: the site is down or shows a certificate error until DNS settles. The game and online play do not depend on it. Max sets the DNS records and the custom domain, or says yes before an agent does.
- [ ] `site/CNAME` holds `cm0102bosman.co.uk`. The Pages workflow keeps it in the published site.
- [ ] The apex and `www` records point to GitHub Pages. `online.cm0102bosman.co.uk` still answers `/v1/health`.
- [ ] HTTPS enforced. The old address gives a redirect to the new one.
- [ ] README and invite link page links use the new address. Worker tests pass.

### 2. Lobby stats and `/v1/stats`
Blocked by: none (it can run beside slice 1)
Delivers: the lobby keeps the daily and all-time stats rows, and `GET /v1/stats` serves them with CORS for `https://cm0102bosman.co.uk`, as does `GET /v1/games`. Not deployed.
Merge danger: one-way door for the stored rows once deployed (a later change of their form needs a migration or loses the history). Blast radius: a fault in the lobby stops online play for every player, so the review rule applies. Max approves the deploy.
- [ ] Every Testing seam 1 test passes, plus the full `server/` suite and `npm run check`.
- [ ] The privacy tests cover the stats rows and the answer.
- [ ] Server README: the endpoint, the rows, the write cost and the CORS rule.

### 3. The online games page
Blocked by: 2 (it reads `/v1/stats`). It can start against a local Worker before slice 1 is live.
Delivers: `site/online.html` in the matchday style of mockup C, with live data, the note, Join and Copy invite link, the error and empty states, reduced motion, and the nav links.
Merge danger: two-way door (a site page). Blast radius: a broken page on the public site. The rest of the site and the game are not touched. Customers see it, so the review rule applies.
- [ ] Every Testing seam 2 state shows as in the spec, at 1280 px and 375 px.
- [ ] No host name or manager name in the page or its requests.
- [ ] The page stops its 60 s reads while the tab is hidden.
- [ ] Copy passes the humanizer check. British English.

### 4. Deploy
Blocked by: 1, 2, 3
Delivers: the Worker deploys, `/v1/stats` answers on the live lobby, then the site deploys with the page.
Merge danger: one-way door (stats history starts, and the stored row form is fixed). Blast radius: online play for every player. Only after Max's yes.
- [ ] Live `/v1/health`, `/v1/games` and `/v1/stats` answer. The CORS header shows for the site origin only.
- [ ] A game hosted through the live lobby shows on the page and counts in today's stats.
- [ ] The page works on a phone at the new domain.
