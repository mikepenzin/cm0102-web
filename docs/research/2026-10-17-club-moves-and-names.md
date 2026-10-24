# Club moves and names at a new game (0.6.0 slice 15)

Date 2026-10-17. Original: `clean-3968/cm0102.exe` 3.9.68, virtual addresses. Spec: `docs/specs/2026-10-16-seasons-0.6.0-spec.md` (Q225, Q226, Q237, Q238, Q244, Q279, Q290, Q335). Code: `tools/seasons/make_names.py`, `tools/seasons/make_clubs.py`, `src/recomp_rt/win/season_names.c` and `.h`, `src/recomp_rt/win/season_clubs.c` and `.h`, `src/recomp_rt/win/injury_init.c`, the club lines of `season_report.c`, the club and name checks of `tests/seasons/rules_report_check.py` and `tests/seasons/season_core_run.py`. Lists: `docs/research/club-moves-0.6.0/lists.tsv` (each club list and what became of it) and `missing.tsv` (the real clubs that no database record matches).

## Which games

Only a season game gets names and club moves: a game whose start year is not 2001, or whose changing rules are on. The base game (2001/02, changing rules off) and the test switch `RECOMP_SEASON_CODE=off` keep the data as it is.

## Names (Q226, Q238)

`make_names.py` turns the `name` column of the rules table into `Seasons/names.tsv` (4,713 rows): season, source file of the class, object number, long name, short name. The rows of a file in a season are those that `rules_report_check.py` pick_row takes, in the same order, so the k-th object of a file gets the k-th name.

The shown name is the research name with four changes, so that a screen shows a plain name:

- a part in brackets at the end goes, unless it names a confederation or a play-off ("Primera División (Liga BBVA)" gives "Primera División");
- a name in quotes at the end goes;
- the year of an edition goes ("2012 Africa Cup of Nations" gives "Africa Cup of Nations"; "UEFA Euro 2012" gives "UEFA European Championship"), because the object of a competition can wait for another edition than the row's;
- a name over 51 characters is cut after a whole word.

The short name (26 characters) is the long name when it fits, else a shorter form ("FIFA World Cup qualification (UEFA)" gives "WC qualifying (UEFA)", "Football League Championship" gives "Championship"). `make_names.py --list` prints them all.

Where the names go: the competition record holds the long name at +4 (52 bytes) and the short name at +56 (27 bytes) (`2026-10-13-database-inventory.md`). The objects are those of the rules report: `g_comp_object_table` [0xadadfc], then the 35 league table entries at 0xb63d60; the class of an object comes from its vtable (`season_comp_classes`).

When: at the end of `build_comp_object_table` 0x8386f0. A new game calls it at 0x829640 in `create_game` 0x828110, after the competitions are made and before the run to the start date (0x5cc850 at 0x829fd9); a load calls it at 0x82c5f6 in `load_game_data` 0x82a9c0. A new game takes the names of the start season. A load takes the start season with changing rules off, else the season of the game date (a season starts on 1 July); slice 16 gives later names through `season_names_apply`.

The save: the original finds each competition by its long name at every load (`match_club_comp_key_id` 0x60efd0, called from `set_key_ids` 0x601a40 at 0x52d8fa in `load_database` 0x528540). Every long name goes in through `season_change_rename` of slice 16, which keeps the name in the data by record index, gives it to that match (its wrap of 0x60efd0) and writes it in the season part (version 2). The short name is not matched and goes in directly. A load takes the names from the season part when it holds them (the part is read before the load runs, `load_game` 0x82a9c0 in season.c, and so before 0x82c5f6), and the names of the season of the game date only when it holds none. A save of a season game holds the season names, and its part the database names; `season_core_run.py` checks both, and a load and a load of a later-version part play on.

With the season names in the data a league writes its temp file as "<long name>_<id>.tmp" (`League::save_to_temp_file` 0x6891c0, sprintf at 0x689234). A name with an accent ("Copa América") is code page 1252, which macOS refuses as a file name, so the game stopped at league.cpp 5095. `win_host_path` (files.c) now turns a path part that is not UTF-8 into UTF-8 (`cp1252_to_utf8` in cp1252.h: valid UTF-8 sequences stay, other bytes are code page 1252, and the five bytes 1252 does not define take their Latin-1 code points; ctest `path_utf8`). This is a fix of the base game too, not under the season switch: on main a Select All 3.9.68 new game on macOS stopped at its first save at league.cpp 5095 (fopen at 0x689282), because the Brazilian state championships (São Paulo, Gaúcho, Goiás, Paraná, ids 67, 254, 260, 266) have such names. With the fix the save holds their parts with UTF-8 names (the save keeps member names as the host listing gives them, so valid UTF-8 must stay as it is). `tests/seasons/path_utf8_run.py` checks it. A path that failed before now opens, so no base game that worked before changes.

Slice 16 calls the names through its names kind (`season_kind_names.c`): `season_names_rename_object(obj, season, ...)` takes the object number, so the two objects of a file (ger_regional North and South) get their own names, and the news names only a change of the long name. `season_names_get_object(file, season, n)` and `season_names_get(file, season)` give NULL outside a season game, in 2001, with no row, or when the object has that name now.

## Club moves (Q225, Q237, Q244, Q279, Q290)

### The table

`make_clubs.py` turns the 16 clubs files `rules-0.6.0/<part>-clubs.tsv` into `Seasons/clubs.tsv` (20,830 rows): season, database division (long name), club record id, club record name, real name. One list is the rows of one part, season and division label. The database division of each list comes from the table `TARGETS` of the tool, one line per part and label. A first version took the database division that most clubs of a list held; that is wrong for many lists (most of the real Série B clubs of 2008 sit in the database's first division, most J2 clubs of 2010 in the Japanese Administrative Division), so the tool now uses the fixed table. 994 lists are in the table and 359 are left out (`make_clubs.py --check`):

| Part | Lists left out | Reason |
|---|---|---|
| den-swe | 40 | Swedish Division 1 Norra and Södra, Ettan: the real groups do not match the six database divisions |
| ger | 18 | 3. Liga: no class in the original (slice 20) |
| gre-pol-cro | 7 | the new second Greek group: no class (slice 20) |
| hol-bel | 8 | Belgian First Amateur Division and National Division 1: the mapping of Q292 leaves Third Division B empty, which the code cannot hold (slice 18) |
| ita | 36 | the single Serie C and Lega Pro groups: no class (slice 20) |
| nor-fin | 22 | Ykkönen as one group and Ykkösliiga: the database has North and South |
| por | 75 | the Segunda Divisão series, Campeonato Nacional de Seniores and Campeonato de Portugal series (groups that do not match the three database zones), Liga 3 (slice 20) |
| rus-tur | 57 | Turkish Category B groups (slice 18) |
| sco | 53 | cup entrants, guests and the pyramid play-off (slice 21) |
| spa | 20 | the Segunda B subgroups of 2020/21 and Primera Federación (slices 18 and 20) |

Four clubs stand in two lists of one season (Brazil 2008, Portugal 2009 to 2011, see `lists.tsv`). The first list keeps them. The research should check these four rows.

### The moves

At a new game in a season game, in memory, at the start of `database_trim_staff_tables` 0x524d70 (called once, at 0x828389 in `create_game`): after the load (0x8282a7, and the date shift of slice 11b at its end) and before the trims of the staff and club tables (0x524d70, `database_trim_club_table` 0x5267c0 at 0x828395) and before the competitions are made. A first version ran after the club trim: there the clubs of the nations that the player did not select are already cut down to a few per nation, so the divisions of those nations looked broken. The database files stay unchanged. A loaded game needs nothing: its save holds the moved clubs.

The division of a club is the pointer at club +0x57 to its club_comp record (`resolve_database_indexes` 0x5315a0 at 0x531625). A league takes its clubs by it, in the order of the club table, and stops at its count: `EngConf::create_table` 0x56ecb0 sets 22 at 0x56ecc2 and takes the clubs whose division is the key id [0x9cf69c] (0x56ed54..0x56ed7f). With fewer clubs the table is not made (0x56ed95..0x56edab). So each division keeps the count that its class takes, not the count of the database. `CLASS_SIZE` in `season_clubs.c` holds the count of each class with a fixed count, with the address of the write of +0x3e (58 classes). Many differ from the databases, for example:

| Class | Count | Databases | Evidence |
|---|---|---|---|
| GreSecond (Greek National B Division) | 14 | 16 | `GreSecond::create_table` 0x5ebcb0, 14 at 0x5ebcc2 |
| ItaSerA (Italian Serie A) | 18 | 20 from 2004 | 0x641522 |
| FraFirst (French First Division) | 18 in 2001/02, else 20 | 18 | `FraFirst::create_table` 0x5bc2b0: +0x40 == 2001 at 0x5bc2bd, 18 at 0x5bc2c5, 20 at 0x5bc2cd. Every season database holds 18, so a later start stopped at fra_first.cpp 108 |
| PolSecond (Polish Second Division) | 20 in 2001/02, else 18 | 20 | `PolSecond::create_table` 0x7cbdf0: 0x7cbdfd, 20 at 0x7cbe03, 18 at 0x7cbe14 |

The classes that count the clubs of the data (IrePrm 0x63a740, IreFirst 0x635910, PolFirst 0x7c9450, NorFirst 0x7909a0) or that take groups (BraNatThird 0x42dc90, UsaMls 0x908560, FraLower 0x5be230, SpaLower 0x8533d0, the Belgian, Portuguese, Spanish, Swedish, Italian C2 and Turkish group classes) keep the database count. Other year branches on the count take one count in every start from 2001: `GerRegional::create_table` 0x5dd7f0 (19 only when +0x40 is 2000, else 18), `ScoPrm::create_table` 0x7f2820 (10 only in 1999, else 12).

A division of another count with no list in a season (Polish Second Division in 2004/05 and 2006/07: 20 clubs in the database, 18 in the class) is cut to its count too: the last clubs of the table leave it, as the class would leave them out. The 2004/05 run logs "Polish Second Division takes 18 clubs, the database gives it 20" and the division holds 18 at the start.

The steps for each season:

1. Each row finds its club: the record id with the same name and the nation of the division, else the club of that name in that nation (an extra database of a season can hold other ids), else the club of that nation with the same plain name (below), else the place is that of a missing club.
2. Each division of a list keeps its listed clubs up to the count of its class: the clubs that the database has there first, then the others in the order of the list.
3. A club that the database has in a listed division and that no list keeps there leaves the division (no division, Q290).
4. Every division of the nations with a list (or a division of another count) is cut or filled to its count, from the highest rank (club_comp +0x69) down. First the clubs that the database had there and that no list keeps. Then two kinds of place:
   - the place of a listed club that the database lacks (Q359): a club of the same nation that the database holds in no division, no national team, and no reserve or B side unless the list lacks one. This holds below the top two tiers, and in every tier for the place of a missing reserve or B side. In the top two tiers the place of another missing club stays open for slice 15b, except in a class with a fixed count (below);
   - any other place (a club that moved up, or a class that takes more clubs than the real division has, slice 18): first a club that left a division of a higher rank (step 3; no reserve or B side, and no move between two divisions of one rank such as German Regional North and South), then the nation's clubs with no division, then the clubs of the spare divisions "A Lower Division" and "A Premier Division" (key ids 0x9cf764 and 0x9cf6b8, set at 0x610153 and 0x60fb91 in 0x60efd0), then the clubs of lower divisions that no list keeps. A lower division that gives a club fills in its turn.
   Inside each group the order is that of the club table. A missing club's place takes no club of another division (Max's condition to Q359); when no club with no division is left, the place stays open.
5. A moved club gets its database division as the division of the last season (+0x5b, 0x5315a0 at 0x531657). `FraLgeCup::collect_entrants` 0x5bd160 needs exactly 54 entrants: 14 French National clubs whose last division is set and is not CFA (0x5bd20f..0x5bd249; 16 when +0x40 is 2001), then Ligue 2 and Ligue 1. Without this step the 2002/03 start stopped at fra_lge_cup.cpp 73 (0x5bca3a).

**Clubs that leave.** Before 2026-10-18 a club that left its division (step 3) kept no division. A 2019/20 start with Sweden then hung: `SweCup::build_club_pool` 0x88da20 fills a pool of 244 (0x88da33) from the Swedish Second Division (key [0x9cf7c8]), the Swedish Lower Division ([0x9cf6ac], "Swedish Lower Division" at 0x60fb28), the First and the Premier Division, and with fewer clubs the cup code loops for ever (a sample showed `SweCup::build_club_pool` and the random function 0x90cfc0). The same start with `RECOMP_CLUB_MOVES=off` reaches the game. Now a club that left is the first choice for the other places of its nation, so the clubs of the nation stay in its divisions; the 2019/20 start reaches the game. The `left` column of `fills.tsv` counts them.

**Stand-ins in the top two tiers (decision for Max).** Max's answer to Q359 says to leave the top and second tier places of missing clubs open for slice 15b. A class with a fixed count does not make its table with fewer clubs (0x56ed95..0x56edab) and its constructor stops at its assert (fra_first.cpp 108 is one case), so an open place in such a division stops the new game. In those divisions (Russian First Division, Northern Irish First Division, J-League 2, Korean League, Portuguese Second League, Argentine Second Division and others in `CLASS_SIZE`) the place takes a club with no division of the same nation, and the log calls it a stand-in (`stand-in` column of `fills.tsv`). The lead's interim decision (2026-10-18, Max asleep): the stand-ins are accepted until slice 15b, and slice 15b must replace every stand-in with the real club record.

**The plain name.** The research name and the database name of a club can differ a little ("Operário Futebol Clube Ltda" and "Operário Futebol Clube"). The plain form keeps the letters of Windows-1252 without accents, in lower case, as words of letters and digits, and leaves out the words that only say "club" (fc, futebol, clube, ltda, ...). When one club of the nation has the plain form of the research name, the row takes it. When more than one has it ("Operário Futebol Clube" and "Operário (SC)" in 2005/06 both give "operario"), the row takes the one whose full words begin the research name or begin with it. `season_clubs.c` (`plain_words`, `word_prefix`) and `make_clubs.py` (`plain_name`, `plain_match`) use the same rule; the check uses the Python one.

**The French CFA and Lower Division.** The game plays no league of these two divisions: French CFA has a stub object (FraCfaStubComp) and the French Lower Division a league entry with no club count (+0x3e is 0). Before the start the game moves the clubs of the divisions that it does not play to "A Lower Division": a base game start of 2001/02 (`RECOMP_SEASON_CODE=off`, France selected) shows French CFA and French Lower Division with 0 clubs and A Lower Division with 558. So both show 0 clubs at every start, with or without the club moves, and the check compares no list of a league with no club count. The moves of these two lists still choose which clubs the database holds there; the 54 French League Cup entrants depend on the French National list (step 5).

`RECOMP_CLUB_DUMP=<file>` (test only) writes each club's id, name and division after the moves, one line per club. `RECOMP_CLUB_LOG=1` writes a line per list: real clubs, clubs found in the database, clubs kept, the clubs filled from each pool, stand-ins, open places, the class count and the database count, and a line per missing club. `fills.tsv` (see Tests) collects the list lines of every run.

### MLS

`usa_mls_get_club_division` 0x909ec0 knows the 12 MLS clubs of the original by their key ids (0x9d037c..0x9d03a8, compares at 0x909ee5..0x909f6b) and stops at usa_mls.cpp 1819 (0x909f96, box at 0x909fbe) for any other club in the American Major League. A 2010/11 start with the real MLS list stopped there. Slice 18 (`season_mls.c`) gives every other club of the division a free group place, so the MLS lists are in the table now. The class takes 12 clubs (three groups of 4), so a longer real list keeps 12 of its clubs (those that the database has there first) and a shorter one is filled from the pool.

### Real clubs that the database lacks (Q335, Q359)

1,645 rows of the clubs files have no record id in the research. A row with no record id goes into the table with club id -1 and the real name, and the game looks for the club by its plain name in the nation of the division (step 1). `make_clubs.py --missing docs/research/club-moves-0.6.0/missing.tsv --databases <folders>` leaves out the rows whose plain name finds one club in the season's database (29 rows, Operário Futebol Clube of 2005/06 among them), so `missing.tsv` holds 1,616 rows. The places of these clubs are filled as step 4 says (Q359); the top and second tier clubs become real records in slice 15b.

## Tests

Seam: the rules report (`tests/seasons/rules_report_check.py`). New: at each season start the report writes the clubs of each division (club lines: division record, club id, club name), and `--start-only` ends the run after the first start report (`RECOMP_RULES_REPORT_STOP=start`, about 3 minutes with 15 nations). In a season game the check compares the clubs of each division with a list of the table with the listed clubs that the database holds (kind `club_list`, by name: the club trim gives clubs new ids). A listed club is found by its record name, else by `plain_match`. The difference line names each listed club that is not in the division and each other club in it, so a wrong fill or a changed fill is a new difference. A league with no club count (+0x3e is 0, the French Lower Division) is not compared. Expected files: `rules_report_expected/<year>-<nations>-start.tsv`.

### Runs of 2026-10-17

`--start-only` runs of every season with a database (2002/03 with the 3.9.68 data, then the 20 season databases), each with two nation groups: (a) England and the first 15 rows of the Select League(s) screen, Argentina to Norway with France; (b) Select All without those 15 (the other 11 nations and England). 44 runs, 3 at a time, about 3 minutes each. All 44 pass against their expected files. Every run with France selected now reaches the game: before this slice each one stopped at fra_first.cpp 108 (a run of 2009/10 with `RECOMP_CLUB_MOVES=off` still stops there).

- Names: no `name` difference is left in any run. Each season renames 29 to 117 records (log line "names of").
- Club lists: 766 lists were checked (a list counts when its league is in the report), 169 of them exact. The other 597 are `club_list` lines of the expected files: the real count differs from the count of the class (slice 18), or real clubs lack a record.
- 2009/10 group (a) stopped at a box "Disk Space Error" (FILE_LLIST::add_node 0x58e340, file_llist.cpp 161) with an older table. The cause is a bug of the original at the first injury pass, fixed for season games in `injury_init.c` (see "The Disk Space Error" below). The CFA 2 list of 2009/10 is in the table again.

Commands (from the repo, `G=/Users/max/Projects/cm0102/clean-3968`, `D=/Users/max/Projects/cm0102/test-fixtures/databases`):

- `python3 tests/seasons/rules_report_check.py --run build/cm0102_recomp $G --year 2010 --database "$D/September 2010 ODB" --nations argentina,australia,belgium,brazil,croatia,denmark,finland,france,germany,greece,holland,italy,japan,northern_ireland,norway --start-only`
- the same with `--nations all,-argentina,-australia,-belgium,-brazil,-croatia,-denmark,-finland,-france,-germany,-greece,-holland,-italy,-japan,-northern_ireland,-norway`
- for 2002 no `--database` (the 3.9.68 data of the game folder).

Other checks of 2026-10-17, after the merge of main (slices 11b and 11c):

| Command | Result |
|---|---|
| `python3 tests/seasons/season_core_run.py build/cm0102_recomp $G --jobs 3` | PASS: the base game has no names and no club moves; the 2010/11 game renames 29 records and moves clubs, its save holds the database names, it is the same save twice, and the save loads, gives the names again and plays 3 days |
| `python3 tests/smoke/smoke_run.py build/cm0102_recomp $G --same-save --save start.sav --jobs 3` | PASS, `season-code-off` equal |
| the same with `--save prematch.sav`, without `--save` (new game), and `--save yeovil.sav --days 7` | PASS each, `season-code-off` equal |
| `cd build && ctest` | 17 of 17 pass, `season_names_table` and `season_clubs_table` among them |

## The Disk Space Error

`Injury::init_records` 0x62cf70 (called once at a new game, from `Injury::Injury` 0x62a680 at 0x62a873) allocates one 31-byte record per staff (0x62cf91) and fills them in the order of the staff table (0x62d036..0x62d056: +0xe, +0x14 and +0x18 are -1). About one player in a hundred gets an injury or illness in the same loop (`Injury::add_injury` 0x62d250 at 0x62d0df). An illness can spread (`Injury::add_illness_outbreak` 0x62e820, called at 0x62dbfd, calls add_injury at 0x62eb6e) to a player of a later staff row, whose record is still fresh memory, all zero. add_injury then reads the current injury +0xe as 0, not -1 (0x62d3f1), and writes it to the injury history through `FILE_LLIST::add_node` 0x58e340 with the offsets +0x14 and +0x18, both 0. With offsets that are not -1, add_node opens `injury_history.tmp` "r+b" (0x58e5c1); the file does not exist yet, so the game stops at the box (0x58e601, line 161). Debug runs of the 2009/10 start that stopped showed the illness of staff row 19736 reaching row 19744 (record +0x14 already 0) and no CreateFileA call for the file before the failing open. The club moves only change which players and clubs take part, so any season start can meet it.

The original never gets past this case for an illness type: for other types add_injury ends at 0x62dc82 and returns -1 with no injury. The fix (`injury_init.c`, season games only): while init_records runs, an add_injury call for a staff row after the row of the loop returns -1 at once, as at 0x62dc82. That player gets no injury, and the loop fills his record later as before, so no injury of the injury list (this +4, written at 0x62d4bc) names a record that the loop then clears. The same 2009/10 start with the older table reaches the game with the fix (log line "injury records: staff row 19744 reached from row 19736 before its turn: no injury"). The base game keeps the code of the original.

## Open

- Q359 and Q360 are answered (Max, 2026-10-17): lower tiers and B teams filled from the database (step 4), top and second tier clubs as records in slice 15b; names without sponsors and edition years.
- Stand-ins in the top two tiers (see "The moves"): accepted by the lead until slice 15b, which removes every one. Max may confirm.
- Our own 2004/05 database: a start with Germany selected stops at ger_lge_cup.cpp 285 (`GerLgeCup::collect_entrants` 0x5dc700 needs 6 clubs whose last division +0x5b is the German First Division with a last place 1 to 6, check at 0x5dc750..0x5dc777). It stops with `RECOMP_CLUB_MOVES=off` too: the database gives the Bundesliga clubs "A Lower Division" (record 357) as their last division. That belongs to the database follow-up (`fm2005-work`), not to this slice. So 2004/05 has an expected file for group (b) only.
- Slice 18: every list whose real count differs from the count of its class (`club_list` lines of the expected files), the MLS, the Belgian mapping of Q292, the Swedish, Finnish, Portuguese, Spanish and Turkish groups, Ligue 1 with 18 clubs from 2023/24 (the class takes 20, so two clubs of the pool fill it).
- Slice 20: 3. Liga, the single Serie C, the second Greek group, Liga 3, Primera Federación.
- Slice 21: the Scottish cup entrants, guests and pyramid play-off, and the academy sides of `eng-academy.tsv` (Q336): cup entrants, not divisions.
- Wembley for a 2003 to 2006 start (slice 11 table 3): not in this slice.
- Slice 11c (`history-0.6.0/no-winner.tsv`): 10 of its 197 rows name a club of `missing.tsv` (Ersboda SK in six databases, Dalkurd FF in four). They become fillable only when the new club records come in.
- Known test problem: the load check of `season_change_run.py` ("the save did not load and play on") presses Space about 400 times with no save; it passed once after 41 presses. It fails on main too (2026-10-18, 423ec0f): the game stays on the news screen after the Next Unread clicks. Not this slice.
