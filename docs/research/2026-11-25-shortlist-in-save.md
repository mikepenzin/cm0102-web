# A human manager's shortlist in a saved game

25 November 2026. Read from the clean 3.9.68 `cm0102.exe` and checked on a real save: "Hertha BSC 2011-12-11", two
human managers (Klaus Kastelmann, staff id 158853, Hertha BSC; Mike Penzin, 158854, Liverpool), game date 11 March
2012, where Klaus shortlisted Mauro Pirozzi (83025) and Mike Phil Mason (94240).

## Where it is

The member `scout_manager.dat` (`SCOUT_MANAGER::save` 0x7f8020): an int32 count of searches (160: ten for each of 16
human manager places), then each search as `PLAYER_SEARCH::save` 0x7b2360 writes it, then the scout requests and other
fields (377 bytes in this save). A manager's ten searches are his lists in order; `scout_manager_write_list_name`
0x7fc150 names list 0 **Short List**, then Player Search, Staff Search and the scouts. In the save, searches 140 to 149
belong to Klaus and 150 to 159 to Mike; Pirozzi is in Klaus's list 0 and Mason in Mike's.

## One search, as 0x7b2360 writes it

Fields of the search (offset in the 0x2b0-byte object, bytes): +0x00 4 (the manager's staff id), +0x04 1 (the list
number 0 to 9), +0x05 0x16b, +0x174 0x18, +0x190 4 (a club: -1 for none), +0x18c 4, +0x194 1, +0x195 4, +0x199 4,
+0x19d 4, +0x1a1 4, +0x1ad 8, +0x1a5 8, +0x1b5 1, +0x1b6 8, +0x1be 8, +0x1c6 40 (ten int32), **+0x1f2 4 (the capacity
of the result buffer)**, **+0x1f6 4 (the count of results)**, then count results of 45 bytes, then +0x1fa 0x99,
+0x293 0x11, +0x2a4 4, +0x2a8 4, +0x2ac 4, and, when +0x190 is above -1 (and its table is there), a club report table
of 0x51a8 bytes. All 160 searches of the save parse to the byte with this.

`PLAYER_SEARCH::load` 0x7b17e0 allocates capacity × 45 bytes (0x7b1e68) and reads count entries into it (0x7b1f24):
**the capacity must never be below the count.** `add_result` 0x7af870 grows it by 250 (0xfa) when the count reaches it.

## One result (45 bytes; `search_result_copy` 0x7af580)

| Offset | Size | What | From |
|---|---|---|---|
| +0x00 | 4 | first name id | the person's row (+0x04), `record` key 0x7e4920 |
| +0x04 | 4 | second name id | row +0x08 |
| +0x08 | 4 | common name id | row +0x0c |
| +0x0c | 4 | the person's staff id | row +0x00; **the sort key** (comparator 0x7bce50) |
| +0x10 | 8 | date of birth | row +0x10 |
| +0x18 | 8 | the date added: the game date | 0xae2c90, saved in `general.dat` at 0xf85 |
| +0x20 | 1 | flags; bit 1 = removed (`contains_player` 0x7af600) | 0 |
| +0x21 | 4, 4 | +0x21, +0x25 | 0 |
| +0x29 | 1, 1 | +0x29, +0x2a | 0 |
| +0x2b | 1 | not set by `add_player`; mostly 0 in the save (other values from other code) | 0 |
| +0x2c | 1 | the manager's note rating, 0xff with no note (0x795750, 0x7afa07) | 0xff (all 342 entries of the two shortlists) |

A date is int16 day of the year, int16 year, int32 1 in a leap year (`web/savedit.js`, `readDate`, `writeDate`).

The results are kept sorted by staff id (`add_result` re-sorts with 0x945333 and 0x7bce50 after each insert; both
shortlists of the save are sorted). The result entry was checked against Pirozzi's and Mason's rows: names, id and date
of birth are the row's own bytes.

## Adding a player to a manager's shortlist, as `PLAYER_SEARCH::add_player` 0x7af720 does

1. Find the manager's list 0: the search whose +0x00 is the manager's staff id and +0x04 is 0.
2. If the player is there (by staff id), clear the removed bit of his entry; else build the entry as above and insert
   it in staff id order.
3. Count + 1; when the count passes the capacity, capacity + 250.
4. Write `scout_manager.dat` back (the other members as they are).

The human managers of a save: the distinct +0x00 of the searches that are a staff id (not -1); their names from
`staff.dat`.
