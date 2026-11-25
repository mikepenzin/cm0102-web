# A scout request in a saved game

25 November 2026. Read from the clean 3.9.68 `cm0102.exe` and checked on the same real save as the shortlist note
(`docs/research/2026-11-25-shortlist-in-save.md`): "Hertha BSC 2011-12-11", two human managers (Klaus Kastelmann at
Hertha BSC, club 4232; Mike Penzin at Liverpool, 5583), game date 11 March 2012. Asked by the user: from the Scout,
send the club's scouts to watch a player, as the game's own "scout him" does.

## Where it is

`scout_manager.dat`, after the 160 searches (`SCOUT_MANAGER::save` 0x7f8020, from 0x7f8193):

| Bytes | What | Code |
|---|---|---|
| 4 | the count of scout requests (this+0x64) | 0x7f819a |
| count × 17 | the requests (this+0x5c), one record each, below | 0x7f81b3 |
| 16 | one byte for each of the sixteen human manager places: the list he last chose (this+0x4c) | 0x7f81c5 |

These are the 377 "rest" bytes of the Hertha save: 4 + 21 × 17 + 16. `SCOUT_MANAGER::load` 0x7f7bb0 reads them back the
same way and sets the array's room to the count (0x7f7e95, `mov [edi+0x60], eax`; 0x7f7ea6 allocates count × 17).

## One request (17 bytes; `SCOUT_MANAGER::add_scout_request` 0x7f9290)

| Offset | Size | What | Code |
|---|---|---|---|
| +0x00 | 4 | the club's id: the asking manager's club | 0x7f94c0 |
| +0x04 | 4 | the player's staff id | 0x7f94ad |
| +0x08 | 8 | the day it was asked: the game date (0xae2c90, saved in `general.dat` at 0xf85), copied by `Date::operator=` 0x417c10 | 0x7f94d9 |
| +0x10 | 1 | the reports sent so far: 0 | 0x7f94f2 |

The array is kept sorted, player id first, then club id (comparator 0x7fc270; `add_scout_request` re-sorts with
0x945333 after its append at 0x7f9504, and so does `daily_update` at 0x7f8b85). `has_scout_request` 0x7f8d70 finds a
request with a binary search on that order (0x946464 with 0x7fc270), so **a request written out of order is not found
by the game**. When the array is full, `add_scout_request` grows it by 10 (0x7f9409; the first one, 0x7f9373, makes room
for 10).

The Hertha save: 21 requests, all of club 4232 (Klaus's Hertha), none of Liverpool; players from 259 to 134764 in
order; days 13 February to 3 March 2012 (leap flag 1: 2012 is a leap year); reports 0 on every one.

## How the game makes one

`staff_screens_handle_scout_request_action` 0x88c360: on Yes in the dialog `staff_screens_build_scout_request_dialog`
0x88c100 builds (assess the player now, or watch him when the scouts are free), it calls `add_scout_request` with the
player shown and club 0, which `add_scout_request` takes as the current human manager's club (the human manager record's club pointer,
+0x39, 0x7f92e7). No other check before the add: the dialog itself is what the game offers on the player's screen.

## What the game does with it (`SCOUT_MANAGER::daily_update` 0x7f8750)

Each day, for each request (0x7f8806 onward):

- dropped (0x7f8b0e, the last record moved into its place, then the array re-sorted) when the club is no longer a human
  manager's (the check 0x600a50), the player is now at that club (0x7f886d), the request is a year old (365 days,
  0x7f88c1), or three reports were sent (+0x10 ≥ 3, 0x7f8908);
- waiting while fewer than 30 days have passed since +0x08 (0x7f8926);
- then the manager's ten lists are asked in turn (`get_search` 0x7f6cd0, 0x7b7490): a scout able to watch the player
  writes a report (0x7b62a0), up to three, sent as news (`send_scout_reports` 0x7f9890).

## Adding a request to a saved game, as `add_scout_request` does

1. The club: the chosen human manager's club, read in the save from his staff row (Klaus's row gives 4232, the
   club of all 21 Hertha requests). A manager without a club cannot ask.
2. If a request of that club for that player is there already, nothing (the game's dialog adds a second one; the
   page does not, so one player is asked once).
3. Build the 17 bytes: club id, player id, the game date from `general.dat` at 0xf85, 0 reports.
4. Insert it in order (player id, then club id); count + 1. The array's room is the count at load, so nothing else.
5. Write `scout_manager.dat` back; the searches and the 16 last-list bytes as they are.

Taking a request back is `remove_scout_request` 0x7f8ee0: the record goes and the gap closes, order kept.

A player at the manager's own club: the game drops that request the next day (0x7f886d), so the page does not offer it.

## As built

`web/shortlist.js` (`clubOf`, `requested`, `canRequest`, `request`, `unrequest`), its tests in
`web/test/shortlist.test.js`, the page in `web/editor.js` (the eye button and "+ Scout"). On the Hertha save: the 21
requests read, Klaus's club 4232 and Mike's 5583, the member written back byte for byte, a request added and taken back
leaves it as it was.
