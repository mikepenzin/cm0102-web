# The Scout, and Membership

Date: 22 November 2026. Decided with Max in the conversation of that day.

## Intent

Players asked for a way to find players: "search by filtering, like age and potential of a specific ability". The
game hides potential ability (PA) and scatters what it shows over many screens; a player looking for a wonderkid
striker for his career has no way to ask "under 21, PA 150 or more, Finishing 15 or more".

Max wants it to be part of what paying players get. The paid plan, so far "Cloud Saves", becomes **Membership**:
Cloud Saves and the Scout. Free players see the Scout work — the first 3 players it finds — and the rest dimmed
behind an invitation to become a member. Every text in the page's languages, as the rest of the page.

## Goals

- A **Scout** tab in the Editor, beside its player list: filters, and the players who match, best first. A player
  of the results opens in the Editor's form on the right, as one picked from the list does.
- The filters: age from–to; a position (a player who rates 15 or more there); minimum potential ability (PA) and
  current ability (CA); up to three attributes, each with a minimum, on the 1–20 scale the game shows.
- On a saved game only, as of its last save (Max, 22 November 2026): scouting is for a career in play. Not on a data
  set or the game's own database.
- Members see every match (up to 100, sorted by PA, then CA). Free players see the first 3 in full; below them,
  dimmed placeholder rows, the count ("44 more players match") and the invitation with the Account dialog's own
  button. The hidden players' data is never put into the page.
- The plan is renamed **Membership** everywhere the page, the tour, the admin page and the legal pages speak of the
  paid plan; "Cloud Saves" stays the name of the feature inside it. The plans, the Account dialog and the tour say
  what Membership holds: Cloud Saves and the Scout.
- English and the nine languages (web/README.md, "Languages"): the new and changed texts in all ten tables, and the
  checks of the languages work (strict table check, i18n-check, screens in every language) pass.

## Non-goals

- The running game: the Scout reads the stored save, not the game's memory. A save made a minute ago is what it sees.
- Changing a player from the Scout: the Editor's form does that, as now.
- A server-side gate. The save lives in the browser, so the 3-player limit is the browser's (a shop window, not a
  vault); the server is not sent saves to scout them.
- New prices, a new payment level, or changes at Paddle or Buy Me a Coffee: the same price and the same
  membership; Max renames the product on their sites himself.
- Nation, club, contract and value filters; sorting by other columns; saving a search. Later, if asked for.
- The game's own Language setting, the legal pages in other languages (they stay English, as now).

## Anti-goals

- A paying member who loses anything: every member keeps Cloud Saves as they are and gains the Scout, with no action.
- A member locked out: offline, or before /api/me answers, a member who was a member at the last answer keeps the
  full Scout.
- The real names or numbers of the hidden matches anywhere in the page (in the DOM, a title, an aria-label): a look at
  the page's source must not give the list away.
- A Scout that makes the Editor slower to open, or a search that freezes the page: a search on the 2011 database
  (the largest in the library) answers within about a second on a phone, and the page stays responsive.
- English left in the Scout or in a renamed text in another language.

## Constraints

- The Editor's model of a saved game, `web/savedit.js`, already lists every player
  (`players()`: name, club, nation, born, CA, PA, position text) and read one (`read(row)`: all attributes, positions).
  The shown 1–20 value of an attribute in a saved game is what the game's 0x543540 returns (staff, player data,
  number 1..31), as savedit.js's `shown()` gives it; its header cites the functions.
- Age: in a saved game staff +0x18 is the age on the game's date (savedit.js, `read`), rewritten every day of the
  game (0x53ab90, called at 0x5cc850; docs/research/2026-10-17-season-date-shifts.md).
- Positions: twelve ratings (GK, SW, D, DM, M, AM, F, WB, and the sides) as the Editor shows them.
- Membership as the page knows it: `/api/me` (`account.js`, `me.user.cloud`), and the offer (`me.offer`: Buy Me a
  Coffee, Paddle, or free with an account). The server's flag stays `cloud` (a stored field of every account): a
  member is an account with it. No server change is needed for the gate.
- A server with no accounts (`me.auth` false), or one where nothing can be bought (an offer with no Buy Me a Coffee,
  no Paddle and no link: the Account dialog's "not on sale yet", as on a server that gives Cloud Saves to a list of
  emails, `CM_CLOUD_USERS`), has no one who could pay: the Scout is whole there (the executor's call). With "free with
  an account" (`offer.free`) the invitation is to sign in.
- Translations: English is the key (`t`, `tn`, `data-t`); the shared module `savedit.js` carries
  `key`/`vars` on errors and never import `i18n.js`.
- The legal pages are English and binding: their changed wording (Membership, what it holds) is Max's to read and
  approve before the deploy.
- Domain words: GLOSSARY.md (add Membership and Scout there).

## Approaches

- **A. A Scout tab in the Editor** (chosen by Max). The Editor already opens a save or a data set and lists its
  players; the Scout is a second tab over that list. No new button in the bar, which is full on a phone.
- B. A Scout button in the bar, opening on the save in play: easiest to find, but an eighth button where a phone has
  room for seven.
- C. A Scout button on each save in Saved games: clear about which save it reads, but two taps deep and not on a
  data set.

## Decided

- **Where:** the Editor, once a saved game is open (a data set shows no Scout tab): two tabs over the left
  column, "Players" (the list and search as now) and "Scout". On a phone the Scout's filters and results take the column; a result opens the
  form as a picked player does, and Back returns to the results with the filters kept.
- **Filters:** age from and to (blank: any); position (any, or one of the twelve, counting a rating of 15 or more);
  minimum PA and minimum CA; up to three rows of attribute + minimum (the attributes the Editor shows for a saved
  game, in its order and its names). A search runs when a filter changes (after a short pause while typing).
- **Results:** sorted by PA, then CA, then name; at most 100, with "and N more" when there are more. Each row: name,
  age, club (or "No club"), the position text, CA/PA, and the values of the chosen attributes.
- **Free players:** the first 3 rows in full. Then up to 5 dimmed placeholder rows made of blank shapes (no data),
  the count of the rest, and the invitation: "The Scout is part of Membership: {count} more players match." with
  the Account dialog's own button for the account as it is: signed out, "Sign in"; signed in, "Become a member";
  free with an account, "Sign in with Google, free". The button opens the Account dialog. The filters work in full
  for everyone; only the rows are held back. While the Scout tab shows, the Players list is emptied from the
  page (drawn again when its tab is chosen), so the page holds no player the Scout held back (the executor's call;
  the Players tab shows CA and PA by name, as now, one search at a time).
- **Members:** whoever `/api/me` says has `user.cloud`; kept in localStorage as the last answer, so an offline member
  keeps the full Scout; a later answer that says no ends it.
- **Membership, the rename:** the paid plan is "Membership" and its holder "a member" in every page text, the tour,
  the plans, the Account dialog, the admin page and the legal pages; "Cloud Saves" names the feature inside it. The
  Paddle and Buy Me a Coffee buttons both read "Become a member". The plans list Membership's two parts: Cloud Saves
  (your saves on every device, backed up after each save, up to N saves) and the Scout (find players by age,
  potential and attributes). The tour's Cloud Saves step becomes the Membership step; its Editor step mentions the
  Scout.
- **Languages:** every new and changed English text in the nine tables (machine translation, as the rest; a label
  named in a sentence matches that label's own translation). "Scout" and "Membership" are translated as a short
  natural name, the same everywhere in a language.
- **Docs:** web/README.md, a section "The Scout", and "Cloud saves" says Membership holds them; GLOSSARY.md gains
  Membership and Scout.

## Left to the builder

- The exact layout of the filter rows, on a laptop and a phone, within the Editor's look (web/README.md, "Look and
  feel").
- How the attribute values of every player are read and kept for a search (all at once when the tab opens, or as
  needed), as long as the speed anti-goal holds; measure on the 2011 database.
- Where the scouting logic lives (a new module, say `web/scout.js`, with no DOM, used by the Editor; testable in
  Node on fakes.mjs's saved games).
- The wording of the new texts, in the page's plain style, and of the legal pages' changes, for Max to read.
- The tests: unit tests of the filters and the free cut (no hidden player's data in what the page renders), the
  Editor check with the Scout on a saved game (and none on a data set), the language checks, the screens in every language.
