# Data set editor: changing and adding players in the database files (2026-11-10)

Facts for the data set editor of the browser build (`web/dataset.js`, `web/editor.js`). Sources: the 20 database files
of the clean files (each checked against `tools/clean_files/manifest.tsv`), `cm0102.exe` of the clean files, the hand
port of the loader (`src/cm3/database.c`, `database_links.c`, built as `port_driver` and run on the files), and the
reading tools of `tools/seasons/own_db`. The game itself was not run for this note.

## How the game takes a data set

- A custom data set is a folder of `Databases` in the save folder that holds all 20 database files
  (`src/recomp_rt/win/dbfolders.h`); Setup Game lists it under Database, and the option `database = folder:<name>`
  makes the load read each Data file from it (`files.c`, `data_dir_find`).
- A folder is taken for a known season database only when the SHA-256 of its club.dat and staff.dat match one
  (`seasons_db_identify`, `seasons_db.c`), so an edited copy of the 3.9.68 data is a custom data set of its own.
- A name with a month and a four-digit year is a dated name (`dbfolders_date`): the editor refuses such names, so that
  an edited copy is not listed as a season.

## The files

`index.dat` is 8 bytes and 22 entries of 0x43 bytes: file name (0x33 bytes), table +0x33, count +0x37, offset +0x3b,
format +0x3f (read_index, 0x528734..0x528834). In the 3.9.68 data staff.dat holds four tables one after another:

| Table | Rows | Row | Offset |
|---|---|---|---|
| 6 staff, format 1 | 169,342 | 157 | 0 |
| 8 | 0 | | 26,586,694 |
| 9 non-players, format 2 | 30,865 | 68 | 26,586,694 |
| 10 players, format 2 | 139,769 | 70 | 28,685,514 |

The other tables are files of their own from offset 0: club.dat (13,214 rows of 581), nat_club.dat (426), nation.dat
(213 of 290), first_names.dat (44,281 of 60), second_names.dat (104,395), common_names.dat (13,256). The sizing of the
load (0x52990e..0x529cc0) makes each table's array from the count, and the read loop (0x52b37c) reads the rows from the
offset. So a new row goes at the end of its table with the counts and the offsets of the later tables of the same file
changed in `index.dat`.

## Ids are rows

The id pass (FUN_005315a0, `database_links.c`) turns each id into a pointer to the row of that number (negative: none).
In the data, every row's id is its row: 169,342 of 169,342 staff, 139,769 of 139,769 players, every name. A new
person's id is the staff count, his player row the player count.

## A player's rows

The format 1 staff row as convert_staff (0x52caa1) reads it, with what the 138,477 player-only rows (flags 2) of 3.9.68
hold:

| Field | Offset | In the data | New player |
|---|---|---|---|
| id | +0x00 | the row | the row |
| first, second, common name | +0x04, +0x08, +0x0c | name rows; common 0 for 121,080 (row 0 is empty) | the names; 0 for none |
| born, year | +0x10, +0x18 | a Date.cpp date, the year | as given |
| nation, second nation | +0x1a, +0x1e | second -2 for 132,017 | as given; -2 for none |
| caps, goals | +0x22, +0x23 | 0 for most | 0 |
| national team, job | +0x24, +0x28 | -2 and 0 for nearly all | -2, 0 |
| dates | +0x29, +0x31 | all zero; 31.1900 for nearly all | the same |
| club, job | +0x39, +0x3d | -1 for 25,372 free players; job 11 for 138,923 | as given; 11 (player) |
| joined, contract end | +0x3e, +0x46 | no date (31, 1900) for 111,615 contracts | 1 July 2001; as given |
| two ints | +0x4e, +0x52 | 0 for 124,617 and 124,649 | 0 |
| personality | +0x56..+0x5d | 0..20, 0 for most; Determination +0x58 (0x543540 case 10) | 0, Determination as given |
| +0x5e, flags, +0x60 | | 1 for 122,683; 2; 0 | 1, 2, 0 |
| favourite clubs, people | +0x61..+0x90 | 12 ints | -1 |
| player row, +0x95, non-player row | +0x91, +0x95, +0x99 | +0x95 is -1 in all 169,342 rows | the new player row, -1, -1 |

The player row (70 bytes): id, CA +0x05 (0..190, 0 for 36,218), PA +0x07 (-2..197; -1 and -2 for 4,283), three i16 at
+0x09..+0x0e (0 for most), positions +0x0f..+0x1a (0..20), attributes +0x1b..+0x44 (0..20; 0 for 4,884,838 of the
5,870,298 bytes, and every attribute 0 for 74,185 players). "0 (random) - 20 (best)" is the hint of SI's editor
(goalkeeper attributes note). A new player row is zeros, with the id and the fields given.

Name rows: the name, the row as id +0x33, a nation +0x37, a count byte +0x3b (1 for 30,855 of the first names, more
for shared names). A new name gets the player's nation and count 1; a name that exists is shared and its count goes up
(to 255 at most).

## Clubs

The squad list of a club (+0xd7, 50 ids) holds every player of the club: 113,105 of 113,105 players with a club are in
their club's list. Empty slots are -1 and come after the used ones in every club; the longest list has 48. The 20
selected players (+0x1e0) are empty in the data. The staff pass (0x52d20a) takes a player job without player data out
of the squad (check_job, job kinds 0x52daa0), and the computer's team pick stops with "match_man..cpp 1225" when one
player is in a list twice (`check_squad_lists` in `tools/seasons/own_db/convert.py`). The editor keeps the lists
packed: a player who leaves is taken out and the slots after him move up, a player who joins takes the first empty
slot, and a club with 50 players takes no more.

## Checks of an edited copy

An edited copy of the 3.9.68 files (Del Piero changed, a new player added to Juventus, one more until the list was
full, one refused) was:

- read by the loader port (`db_read` 1), with the counts up by 2 staff, 2 players, one first name, one surname and
  one common name; then linked (`db_resolve`) and passed (`db_staff_pass`) with no error. After the pass the new rows
  link to their player rows, Juventus, job 11, their names, and Juventus's squad holds them;
- read by `tools/seasons/own_db/cmdb1.py` with the right names, clubs and dates;
- passed by `check_squad_lists`.

## Choosing a data set from the page

The Choose button of Choose Season (choose_apply, `season_screen.c`) sets the options `database = folder:<name>` and
`season`, calls `win_data_set_changed` and `season_set_new_game`. The runtime reads settings.txt once, at the start
(`options_load`, `display.c`), so the page cannot choose by writing the file while the game runs. The browser build's
`rt_web_choose_data_set(folder, year)` (`season_screen.c`, `__EMSCRIPTEN__` only) sets the options and calls
`win_data_set_changed` for the Custom tile, after the same checks (the season range, `folder_fault`, `dbfolders_path`).
For a season database that the game knows, `season_screen_year` takes the database's own season, whatever the option
says.

It does not call `season_set_new_game` itself. With no game on, that function moves the game date to 1 January of
the start year with `rt_callback` to Date::set (0x54a130), code of the game. The page calls between two frames,
outside the game's turn, where JSPI cannot suspend: the first build of the call stopped with "trying to suspend
without WebAssembly.promising". The second build stopped the same way in `options_set`: settings.txt is written
with `host_sync`, which is `fsync`, and Emscripten's `fd_sync` syncs the browser's storage (IDBFS) and waits for it
(the stack of the error: `host_sync` < `options_set` < `rt_web_choose_data_set`). In the glue only
`emscripten_sleep` and `fd_sync` can suspend. While the page calls, `host_web_page_call` (host.h) makes `host_sync`
only flush; the page syncs the folder after the call (`setspanel.js`). The call marks the choice instead, and the wrap of Start New Game (0x81ce00,
`start_new_game` in `season_dates.c`) applies it first (`season_screen_apply_page_choice`). 0x81ce00 is the
function that Setup Game's input (0x81bd70) calls for item 1 at 0x81c929; it is also called at 0x81f764 and
0x764164. It reads the year of the game date at 0x81cf37 (key_nation_build_table 0x667150), after the wrap. Setup
Game's menu build (database.c, the wrap of 0x5ed300) applies the options too, at each build.

## Not done

Clubs, non-playing staff, the personality bytes other than Determination, and the three i16 of the player row are not
edited. A player is added only to data in format 1 (the game's own data and copies of it); Update Team data sets keep
staff in format 2 and preferences in table 22, which a new row would need too. Data sets go to other browsers as
zips (Download, Upload) and, with Cloud Saves, through the cloud.
