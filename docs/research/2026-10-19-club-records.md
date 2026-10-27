# Real club records for missing top and second tier clubs (0.6.0 slice 15b)

Date 2026-10-19. Original: `clean-3968/cm0102.exe` 3.9.68, virtual addresses. Spec: `docs/specs/2026-10-16-seasons-0.6.0-spec.md` (Q359, Q361, Q362, Q387, Q404). Brief: the slice brief (kept outside the repo). Evidence of the club table: `docs/research/2026-10-19-club-table-users.md` (cited below as "table users"). Research: `docs/research/club-records-0.6.0/`.

## Which clubs

`club-records-0.6.0/rows.tsv` holds the 52 rows of `club-moves-0.6.0/missing.tsv` that stand in a tier 1 or 2 list of a nation of Q359 and Q361, less:

- the MLS rows (Q404: the MLS stays at 12 clubs, so its expansion clubs get no place);
- the B teams (Shamrock Rovers B and II, Vitória de Guimarães B, Dinamo Zagreb II, Hajduk Split II, Asteras Tripolis B): slice 15 fills their places (Q359);
- the rows of "new (gre second, group 2)": no class holds that division (slice 20).

Three of the 52 rows name clubs that the season's own database holds under another name (Q505): Valpovka in 2002/03 and 2003/04 (NK Valpovska Satnica, record 12238, in Croatian Second Division North in both databases) and Međimurje in 2003/04 (NK Cakovec, record 2128 of the October 2003 database, the record that later databases call NK Medjimurje Cakovec). Their rows in `rules-0.6.0/gre-pol-cro-clubs.tsv` now name these records, so slice 15 finds them. The rebuilt Summer 2006 database (Q351, merged 2026-10-20) holds three more: Gençlerbirliği OFTAŞ (record 2938), Sibir Novosibirsk (2512) and Spartak-MZhK Ryazan (8758); `make_club_records.py` leaves out a row whose club the season's database holds. The other 46 rows (34 clubs) become new records (`Seasons/club_records.tsv`).

At run time a row makes no record when the loaded database already holds a club of the division's nation with the record's name or its name in the club moves table (review 1): another database of the year or a Custom data set can hold it. The log names each such row.

The facts of each club are in `club-records-0.6.0/clubs.tsv`, with a source and a second source per row, and `clubs.check.md` says which facts the second source confirms. Q504: a fact with one source stays and its `notes` say so (Glebe Rangers and Warrenpoint Town have no second source that could be read on the day).

## Where the records go: the data load (Option B of the table users doc)

The club table is one block of exactly (clubs + national teams) x 0x245 bytes made by the data load 0x528540 (table users 1), with no spare rows. The national teams must stay the last 2 x nations rows: `database_get_national_team` 0x53b3d0 takes row nation +0x00 + count - 2 x nations, and 0x53b900, 0x53bd10 and 0x5267c0 test `club +0x00 >= count - 2 x nations` (table users 3). So a new club goes before the national teams, and every reference to a national team moves by the number of new clubs K.

A record added after the load (at the start of `database_trim_staff_tables` 0x524d70, where slice 15 moves clubs) needs a larger block, a move of the 426 national team rows, a fix of every pointer into them (staff +0x24, +0x39, club +0xb3..+0xbb, preferences +0x04..+0x18, staff history +0x0a, comp history +0x0a..+0x16, [0xae23b8]), a fix of the club key ids that `set_key_ids` 0x601a40 set at 0x52d8fa, and the +0x244 step of 0x541e60 (0x8282f1), which runs before 0x524d70. A record added before the id pass 0x5315a0 (0x52d1aa) needs only id shifts, and the game's own load, id pass, staff pass, key ids, squad trim, extra staff 0x527240 and both trims then treat it as a club of the files.

**Design: the loader reads a merged copy of the season database.** At the data load of a season game (the wrap of 0x524d50 in `season_dates.c`), before the original load runs, `season_records.c` builds in host memory the files of the season database with the new records added, and the file layer (`files.c`) gives the loader these buffers in place of the files of the data folder. The files on disk stay unchanged (Q225, Q237). After the load the buffers are freed. A saved game holds the clubs in its own tables (`save_database` 0x52dad0 writes the counts and every row, table users 2), so a load of a save needs nothing.

The merged files:

- `club.dat`: the K new records after the file's clubs, ids file count .. file count + K - 1.
- `nat_club.dat`: each national team's id +0x00 plus K.
- Every club id at or above the file club count plus K (the national teams): club +0xb3, +0xb7, +0xbb (in both club files), staff +0x24 and +0x39, the preferences (+0x04..+0x18 of table 22, or file +0x61..+0x75 of the 157-byte staff rows), staff history +0x0a, comp history +0x0a, +0x0e, +0x12, +0x16 (club and nation comp history). This is the list of club fields that the id pass resolves and that `database_trim_club_table` 0x5267c0 fixes when it moves rows (table users 1, step 5).
- `stadium.dat`, `city.dat`: a new stadium per new club, after the file's rows (ids follow). A city that the database holds (same name and nation) is reused, else a new city row.
- `staff.dat` (all parts), the name files and `staff_history.dat`: the players of the new squads (below), after the file's rows. Every table of `staff.dat` gets its new count and offset in `index.dat`.

Why the merged files and not memory edits at 0x52d1aa: the loader's sizing (0x529935..), its readers and its reserve rows then need no change at all, the new rows are file rows for the date shift of slice 11b (it shifts `file_rows` of each table, read from the index, `season_dates.c`), and the code that builds the files can be tested on its own (ctest) against the real databases.

`seasons_db_identify` and the staff reserve estimate read the real files on disk (by path, not through the game's file calls), so the database keeps its identity (Q264, Q271) and its date shift row.

Which games: a season game (`season_game_changes()`: a start year other than 2001, or changing rules on) with rows for its start year. The base game and `RECOMP_SEASON_CODE=off` read the files as they are (Q342). Test switch `RECOMP_CLUB_RECORDS=off`: no records.

## The record

Each new club record is a copy of a template club, then the known fields are set (table users 7: the unknown bytes then hold values that the game already plays with). The template is the club of the target division in the start database whose reputation is the median of the division (the lower of the two middle clubs for an even count, ties by the lower record id).

| Field | Value |
|---|---|
| +0x00 | its row (file count + k) |
| +0x04 name (51), +0x38 short name (26) | `name`, `short_name` of clubs.tsv (Windows-1252). A short name that another club of the nation in the loaded database has becomes the long name cut to 25 characters (review 1); `make_club_records.py --check --databases <folder>` finds such names in the season databases, and the research table holds a distinct short name for each one it found (Ethnikos Piraeus, FK Krasnodar, Atlético Paraná, Iwaki FC, Gwangju FC, Central Coast Mariners) |
| +0x53 nation | the nation of the target division (club_comp +0x5d) |
| +0x57 division | -1. Slice 15's club moves put the club in its division at 0x524d70 (they find it through `season_records_club`, below) |
| +0x5b last division | the target division, so cups that test the last division count it (slice 15 doc, step 5) |
| +0x5f last place | the number of clubs of the division plus 1 (review 2): the place in the last division (data, September 2010 ODB: Chelsea 1, Arsenal 3, Fulham 12) that `UefaSeeding::collect_european_clubs` 0x904040 reads at 0x904185, and the AFC and league cup picks too. After the last club, no place-based pick takes the record |
| +0x69 stadium | the new stadium; +0x6e -1 |
| +0x80 reputation | the median reputation of the clubs of the target division in the start database (Q362) |
| +0x83..+0x97 colours | home: text `colour2`, shirt `colour1`; away: text `colour1`, shirt `colour2`; third: -1 (the pairs of the data: Arsenal White/Red 1, Crawley Town White/Red 1) |
| +0xb3..+0xbb rivals | -1 |
| +0xbf chairman, +0xc3 directors, +0xcf manager, +0xd3 assistant, +0x19f coaches, +0x1b3 scouts, +0x1cf physios, +0x1e0 team | -1. `database_generate_extra_non_playing_staff` 0x527240 fills the coaches, scouts, physios and the manager of a club of a selected nation (table users 7) |
| +0xd7 squad (50) | the new players (below), then -1 |
| +0x244 | 0 (0x541e60 sets it) |

Colours map by name to the colour records of the start database (`colour.dat` names: "Red 1", "Blue 2", ...): Red to Red 1, Maroon to Red 3, Navy Blue to Blue 2, Blue to Blue 4, Sky Blue to Blue 5, Green to Green 1, Yellow to Yellow 1, Orange to Orange 1, Purple to Purple 1, Pink, Gold, Black, White by their names. An unknown colour is White.

The stadium (0x4e bytes): a copy of the template club's stadium; +0x04 name, +0x38 city, +0x3c capacity, +0x40 seats and +0x44 expansion capacity all the capacity (data: Stamford Bridge 42,055 three times), +0x48 -1. An unknown capacity takes the median capacity of the stadiums of the clubs of the target division. The city (0x38 bytes): a copy of the template stadium's city with the name and nation set (its two doubles, inference latitude and longitude, stay those of the template city).

## Squads

`club-records-0.6.0/sources.tsv` (`tools/seasons/find_club_sources.py`, Q503) names the nearest season database that holds each club, the club's record id there and its player count. At a new game the source database is read from its unpacked folder (`Databases/<folder>`, known by its content, `seasons_db_identify`), else from `Seasons/<name>.zip` (unpacked once, `seasons_db_prepare`); with neither, the club gets no players from it.

Each player of the source club (staff +0x39 = the club, a player row at +0x61) is added, except a person whom the start database holds: he stays where the start database has him (spec). A person is held when the start database has the same first, second and common name with the same date of birth after both date shifts, or the same three names, the same nation and a birth year one either way (review 2: the day of birth and the day shifts differ between Update Team editions). What a new staff row gets:

- the staff row (0x6e bytes, or 157 bytes in the 3.9.68 data): names by text (a name row of the start database with the same text, else a new name row), nations by name, club = the new club, job as in the source (player and coach jobs 12, 13 and 15 become 11, player), national club -1, the non-player part removed (+0x69 -1);
- dates: the source database's own shift (slice 11b, `season_dates_shifts_of`) makes them real, and the start database's shift is taken off, so the shift of the load gives the real date again. A contract end that lies before the start becomes 30 June of the year after the start;
- the player row (0x46 bytes) as in the source, with its new id; a preferences row with no favourite clubs or staff (-1), as fill.py of the own databases does;
- the career rows (staff history, 0x11 bytes) of the source before the start season, with the club ids mapped by name and nation to the start database (a club it lacks gives -1).

The staff reserve (`staff_reserve.c`) is sized at the load. The new players are file rows, but 0x527240 makes up to 16 non-playing staff for each new club, so the reserve grows by 16 x K at the data load that adds the records, and only there (review 1: a saved game's load 0x524d10 keeps the reserve it has without them).

**Squad fill (Q509, the lead's answer of 2026-10-20).** The original has no squad fill at a new game (table users 8): no code adds players to a club with too few, and its youth generation `PLAYER_REGEN::regenerate_as_youth_player` 0x7abac0 runs only after the competitions exist, reuses a retired staff row and chooses the club itself (`player_regen_sign_for_club` 0x7a6f10). Many source squads are thin (the Northern Irish clubs 0 to 1 player) and three clubs are in no season database (TŠK Topolovac, Spartak-MZhK Ryazan, Nizhny Novgorod of 2008). So at the start of `database_trim_staff_tables` 0x524d70, after slice 15's club moves, a new club that has a division and fewer than 22 players takes players whom the database has at no club (+0x39 -1), aged 17 to 34 at the start: first two goalkeepers when the squad has fewer (goalkeeper rating, player row +0x0f, 15 or more), then the good ones youngest first, then the others by current ability (player row +0x05; data: Messi 195, Rooney 176). A good one has a current ability at or above the median of the players of the clubs of the club's division in the start database. Players of the club's nation come first; when they run out, players of the other nations of its continent (nation +0x71). These are people of the database: no person is made up. Their joined date becomes 1 July of the start year and their contract ends on 30 June of the year after. A club that still has fewer than 16 players is named in the log ("has N players, fewer than 16") and in the list below. The fill runs only where the records run: a season game (Q152). A record that the class count of its division leaves out (no division after the moves) takes no players (lead decision O37); the log says "has no division: no squad fill". A person joins one club only: he leaves the pool when he is placed, and every player of a source squad is already a staff row of his new club.

Clubs below 16 after the fill: none in the runs of this slice (see Tests).

## Slice 15's club moves

`season_clubs.c` step 1 finds each listed club by record id, name or plain name. A new record has the research name, which can differ from the list's name ("AO Kerkyra" for Kerkyra). So step 1 first asks `season_records_club(season, real name)`, which gives the record id of the new club made for that row. With the club found, the list has no missing club in that place, so no stand-in fills it (Q387 ends for these rows).

## After the trim

`database_trim_club_table` 0x5267c0 writes [0xae23fc][new row] = the row before the trim (table users 1, step 6). The language names (0x5339a0, only when the settings language differs from [0x9b5984], which the port keeps at 0) read the .lng row [0xae23fc][i]. After the trim the wrap sets the entry of each national team to its .lng row (value - K) and of each new club to 0, so a language change can never read past the .lng block. Inference: the port never reaches that code.

## Decisions (slice 15b, recommended answers taken)

- Q500: the Iwaki FC 2025 row stays as slice 15's table has it (the 2026/27 J2 list).
- Q501: "Daegu FC", the club's own name, not the older spelling of the databases.
- Q502: a founded year from the club's own page, other years in the notes. The game has no club field for a founded year (no record field of `include/database.h` or the table users doc holds one), so the column is research only and no record shows it.
- Q503: the squad source is the season database nearest to the row's season that holds the club, the later one on a tie.
- Q504: a fact with one source stays, marked in `notes`.
- Q505: Valpovka (2002/03, 2003/04) and Međimurje (2003/04) are in their season databases; slice 15's clubs file names those records.
- Q506: a record's template is the median-reputation club of its division; unknown bytes keep the template's values.
- Q507: a person whom the start database holds stays where it has him, also when it has him with no club.
- Q508: not used (an earlier fill rule, replaced by Q509).
- Q509: the squad fill above (22 players; two goalkeepers; good unattached players of the nation youngest first, then the others; then the nation's continent; at least 16).

## Tests

Check: `tests/seasons/club_records_run.py <build> <game> --year Y --database <folder> --nations all[,-<row>] [--last Y+1]`. It starts a new game with `RECOMP_RECORDS_SOURCES` (the unpacked databases of `test-fixtures/databases`, known by content) and `RECOMP_RECORDS_DUMP` (the merged files), and checks the log, the merged club.dat (names, ids, the national team ids moved, the squads), and the club lines of the rules report. A record that the class count of its division leaves out (the class takes fewer clubs than the real list; slice 15 keeps the clubs that the database has there first) is printed as "not kept (the class count, slice 18)".

Runs of 2026-10-20 (Select All, start only):

| Season | Database | New clubs | Result |
|---|---|---|---|
| 2002/03 | 3.9.68 (format 1 staff) | Daegu FC, TŠK Topolovac | PASS (before the fill) |
| 2005/06 | September 2005 | 9 | PASS, every squad 22 |
| 2007/08 | November 2007 | 1 | PASS |
| 2008/09 | September 2008 | 7 | PASS |
| 2009/10, 2011/12, 2013/14, 2014/15, 2016/17, 2023/24 | the season databases | 1 to 5 | PASS |
| 2010/11 | September 2010 ODB | 5 | PASS |
| 2020/21, 2022/23 | November 2020, October 2022 | Treaty United, Kerry | PASS without Denmark |
| 2025/26 | December 2025 | Iwaki FC | PASS without Denmark and France |

No club ended below 16 players. Clubs that their class count leaves out until slice 18: the Korean League (10), Greek National A and B (14), Northern Irish First Division (10), Russian First Division 2005/06 (18), J-League 2 (12).

Full seasons (`--last Y+1`): 2008/09 reaches the 2009/10 start, and FK Krasnodar and FK Nizhny Novgorod hold a division there (the end-of-season step runs only for a league whose matches are all played). 2013/14 and 2005/06 stop at world_cup.cpp 1438 (slice 27 fixes it).

Stops that are not this slice (the same with `RECOMP_CLUB_RECORDS=off`): Select All 2020/21 and 2022/23 at den_cup.cpp 75; Select All 2025/26 at fra_lge_cup.cpp 73; a 2013/14 season with Argentina at virtual_staff.cpp 195 (Huracán de Tres Arroyos, no nation and no players in the Autumn 2013 database, becomes a 26th club of the Argentine Second Division after the moves); 2005/06 with the 3.9.68 data (not a game configuration) at Database.cpp 13471.

After the merge of main (slice 18 and the own databases, 2026-10-20): 2002/03 (3.9.68 data, format 1 staff; TŠK Topolovac 22 players from the fill) and 2006/07 (Summer 2006; Wexford Youths in the Irish First Division) PASS.

`tests/seasons/season_core_run.py` (its 2010/11 game holds the 5 new clubs): PASS, the save is the same twice and loads and plays on. Same-save set: see the slice report.

Expected files: the slice 15 start runs of both nation groups (`rules_report_check.py --run ... --start-only --update-expected`, with `RECOMP_RECORDS_SOURCES`) for the 13 seasons with a record and an expected file (2002 to 2020) all PASS. Of each update only the `club_list` lines of the divisions that hold a record of that season are kept (31 lines go); the other lines differ through slices merged since the files were made, and the lead regenerates the full set on main.
