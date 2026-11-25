# Scout a player from Squad Hub — design

25 November 2026. Asked by the user after the shortlist: "Can we add same player to scout request? As in game?"

## Why

The shortlist puts a found player in front of the manager; a scout request sends the club's scouts to watch him, so
reports on him arrive in the game as news, as when the manager asks "scout him" on the player's screen.

## Goals

- From a Scout result (and the player's panel in Squad Hub), ask the chosen manager's club to scout a player; take the
  request back with a second tap.
- The request exactly as the game's `add_scout_request` 0x7f9290 makes it, in the game's order
  (`docs/research/2026-11-25-scout-request-in-save.md`), so the game finds it and reports on it.
- One choice of manager for both buttons: "Shortlist for" becomes "Manager".
- Ten languages.

## Non-goals

- Making reports appear sooner, or changing the game's 30-day wait, three reports or one-year end.
- The scouts' own assignments (their lists).

## Anti-goals

- A request the game cannot find (out of order) or a save it cannot load.
- A request the game would drop the next day: no button for a player already at the manager's club, none for a
  manager without a club.

## Design

- **Model** (`web/shortlist.js`, beside the shortlist): parse the bytes after the searches as count, 17-byte records,
  16 last-list bytes; `clubOf(manager)` from his staff row +0x39; `requested(m, id)`, `request(m, id)` (club id, player
  id, the game date from `general.dat` 0xf85, 0 reports, inserted in order player id then club id; none when there
  already, at his own club, or no club), `unrequest(m, id)` (the record goes, the order kept, as
  `remove_scout_request` 0x7f8ee0). Written back with the rest of `scout_manager.dat`, byte for byte otherwise.
- **Page:** an eye button after the star on each Scout row, and "+ Scout" / "Scouting ✓" in the player panel. A change
  of the save (Save counts it). The manager select is labelled "Manager".
- **Who:** members, as the shortlist; a feature of `web/features.js`, key `scout-request`, Club Legends first.
- **Proof:** unit tests on the Hertha layout in a fake save; scout-check in the browser; the real engine loads the
  edited Hertha save.
