# Scout to shortlist — design

25 November 2026. Asked by the user: after the Wonderkids preset, let a player found in the Scout go onto a human
manager's in-game shortlist; with two or more managers in a save, choose whose.

## Why

The Scout finds players across a saved game, but the game only acts on them through its own screens. Putting a found
player on the manager's Short List carries the find into the game: when the save loads, the player is waiting there to
be watched, scouted or bid for.

## Goals

- Add a player from the Scout (and from a player's detail panel in Squad Hub) to a human manager's Short List in the
  saved game; take him off again.
- With two or more human managers in the save, choose the manager; one manager needs no choice.
- The save stays a save the game loads exactly as its own: entries built as the game builds them.
- Every text in the ten languages.

## Non-goals

- The other nine lists of a manager (Player Search, Staff Search, scouts).
- Sending scouts, bids, or anything the game does from the shortlist.
- Changing the game's code: this is a change of the saved file only.

## Anti-goals

- A save that the game cannot load, or loads with a broken list.
- A player shown on a shortlist in Squad Hub that the game does not show.

## Approaches

- **Edit the save's shortlist (chosen).** Squad Hub already reads and writes saves; the shortlist is `scout_manager.dat`,
  whose layout is read from the game's own save, load and add code (`docs/research/2026-11-25-shortlist-in-save.md`).
- A file the game imports (a saved short list, `PLAYER_SEARCH::save_to_file` 0x7b3660): the player would have to load it
  in the game by hand, and the format has its own header and owner checks. More steps for the player.
- A change of the game's code that reads a list from the page at load: game code for something the save holds already.

## Design

- **Model** (`web/savedit.js` or a module beside it): parse `scout_manager.dat` as the research note says; list the human
  managers (name, staff id); for a manager, the staff ids on his Short List (entries without the removed bit); add a
  player (build the entry from his staff row and the game date in `general.dat` at 0xf85, insert by staff id, count + 1,
  capacity + 250 when needed; a removed entry of him gets its bit cleared instead); remove a player (set the removed bit,
  as `contains_player` 0x7af600 reads it). Write the member back through the save file's own writer, so Save the game
  file writes it with the other changes.
- **Page** (Squad Hub, a saved game only): a "+ Shortlist" button on each Scout result and in the player panel;
  "On shortlist ✓" when he is there, a click takes him off. With two or more managers, "Shortlist for: [manager ▾]" at
  the top of the Scout, remembered for the save. The change counts as a change of the save (the Save button and its
  count), like an edit of a player.
- **Who:** members (Squad Player and up, as the full Scout). A free player sees the button with the Membership
  invitation. A new feature of the page: a line in `web/features.js` (key `shortlist`), so Club Legends have it first,
  14 days; gated with `earlyState` and `lockedHtml`.
- **Proof:** unit tests on the Hertha save's layout rebuilt in a fake save (parse to the byte, add, remove, capacity,
  sort, round trip); the browser check of the Scout; and the real engine loads an edited save and its Short List shows
  the added player.

## Left to the builder

The module's name, where the button sits in a row, the remembered manager's storage key.
