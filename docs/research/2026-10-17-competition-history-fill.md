# Competition history between 2001 and the start season (slice 11c)

Date 2026-10-17. Slice 11c of `docs/specs/2026-10-16-seasons-0.6.0-spec.md` (Q347). Research process: the `league-research` skill. Brief of the research agents: the history research brief (kept outside the repo). Tool: `tools/seasons/history_gaps.py`. Research table: `docs/research/history-0.6.0/<part>.tsv`, checks `<part>.check.tsv`.

## 1. The history rows

### Record

`club_comp_history.dat` and `nation_comp_history.dat` hold rows of 26 bytes (reader 0x550230 `db_files_read_comp_history`, record size from the `_malloc` in the loader 0x528540). In memory both files are one table at [0xae23d4] with the count [0xae237c]. The nation rows are the last [0xae2384] rows, and [0xae23dc] points at the first of them (0x527f60 sets it to base + (count - nation count) * 26).

| Offset | Size | In the file | In memory after the id pass | Evidence |
|---|---|---|---|---|
| +0x00 | 4 | row id | row id | 0x527f60 writes the row number in new rows (0x527fd9) |
| +0x04 | 4 | competition id: club competitions, then nation competitions after the count of club_comp.dat | pointer: [0xae23d0] + id * 0x6b, or 0 for a negative id | id pass 0x532c11 to 0x532c38 |
| +0x08 | 2 | year (the label year) | the same | 0x4afce0 writes [0xae2c92], the game year, at +0x08 (0x4afd62) |
| +0x0a | 4 | first: club id (clubs, then national sides) | pointer: [0xae23b4] + id * 0x245, or 0 | 0x532c42 to 0x532c63; 0x4afd6a stores the winner |
| +0x0e | 4 | second | pointer, or 0 | 0x532c6d to 0x532c8e; 0x4afd71 |
| +0x12 | 4 | third | pointer, or 0 (also 0 for an id above the club count [0xae235c]) | 0x532c98 to 0x532cc7; 0x4afd78 |
| +0x16 | 4 | host | pointer, or 0 (the same check) | 0x532cd1 to 0x532d00; 0x4afd7b |

The id pass starts with `xor ebp, ebp` (0x5315ae), so a negative id becomes a null pointer. A row with a null competition is a free row: 0x4afce0 reuses the free rows at the end of the table before it grows the table by 20 rows with 0x527f60. The winners tab 0x4991f0 draws winner, runner-up, third and host. Data: club competitions use the first two places; the nation competitions and the Olympic Games use all four (3.9.68: FIFA World Cup 1998 France, Brazil, Croatia, host France).

### Label years

The label year is the year that the row holds. The convention depends on the competition (3.9.68 rows):

- Most competitions: the year where the season ends. English Premier Division 2002 Arsenal is 2001/02.
- Calendar-year competitions: the calendar year. Swedish Premier Division 2001 Hammarby is the 2001 season. The 3.9.68 data holds the 2001 season of calendar-year leagues although a 2001/02 game starts in July 2001.
- Argentine Premier Division: two rows per label year. Label 2001 holds the Apertura 2001 (Racing Club) and the Clausura 2002 (River Plate).

The research table gives the convention of each competition (part `.md` files).

### After the shift of slice 11b

For the databases before 2010/11 the shift of the competition history is 0. The database writes the last champions into the row of 2001, and slice 11b moves only that row to the season before the start, for the competitions that the database updated (`databases-date-shifts-last-rows.tsv`). So each competition holds its rows up to 2000 or 2001, then at most one row at the start year. All label years from 2002 to the start year minus 1 are missing, and the start year too for the competitions that the database did not update.

### Gaps per database

`history_gaps.py` lists every competition with history rows and the label years from 2002 to the start year that it lacks (`docs/research/history-0.6.0/gaps.tsv`; `targets.tsv` gives the label years to research over all databases). The start year of the 3.9.68 data on the 2002/03 tile is 2002.

| Database | Start | Competitions with history | Label years missing (all competitions) |
|---|---|---|---|
| 3.9.68 (2002/03 tile) | 2002 | 333 | 237 |
| October 2003 | 2003 | 333 | 562 |
| September 2005 | 2005 | 302 | 1081 |
| November 2007 | 2007 | 190 | 1076 |
| February 2008 (extra) | 2007 | 191 | 1081 |
| September 2008 | 2008 | 207 | 1346 |
| December 2008 (extra) | 2008 | 167 | 1103 |
| March 2009 (extra) | 2008 | 200 | 1300 |
| September 2009 | 2009 | 168 | 1245 |
| February 2010 ODB (extra) | 2009 | 182 | 1301 |
| April 2010 SIM (extra) | 2009 | 168 | 1242 |
| June 2010 (extra) | 2009 | 182 | 1301 |

Over all databases, 347 competitions (by kind, name and nation) hold history rows. The missing counts include competitions that ended before 2002 (for example the European Cup Winners Cup); the research marks those `not held`.

## 2. The research

One Opus agent per part researched every competition and label year of `targets.tsv` (by the history research brief, kept outside the repo). A second agent checked every filled field against a page of another publisher (`<part>.check.tsv`, `.check.md`). A reconcile agent settled the disagreements with a third source (`reconcile.tsv`, `reconcile.md`).

| Part | Rows | held yes | not held | unknown |
|---|---|---|---|---|
| england | 108 | 102 | 6 | 0 |
| scot-wal-nir | 136 | 127 | 9 | 0 |
| ireland | 134 | 42 | 92 | 0 |
| ger-aut-sui-lux | 90 | 76 | 14 | 0 |
| fra-bel-ned | 179 | 163 | 16 | 0 |
| italy | 82 | 82 | 0 | 0 |
| esp-por | 101 | 88 | 13 | 0 |
| nordic-1 (Sweden) | 151 | 117 | 34 | 0 |
| nordic-2 (Norway, Denmark, Finland) | 132 | 116 | 16 | 0 |
| east-europe-1 (Poland, Croatia, Czech Republic) | 112 | 93 | 19 | 0 |
| east-europe-2 (Russia, Yugoslavia, Greece, Turkey) | 146 | 135 | 11 | 0 |
| brazil | 196 | 151 | 45 | 0 |
| americas (Argentina, Mexico, United States) | 63 | 63 | 0 | 0 |
| asia-1 (Japan, Korea, China) | 128 | 118 | 10 | 0 |
| asia-2 (Hong Kong, India, Indonesia, Malaysia, Singapore, Thailand) | 106 | 94 | 6 | 6 |
| aus-rsa (Australia, South Africa) | 66 | 59 | 7 | 0 |
| intl-clubs-1 (Europe, South America, CONCACAF, world) | 162 | 108 | 54 | 0 |
| intl-clubs-2 (Africa, Asia, Arab, CIS, Gulf, Oceania) | 87 | 46 | 41 | 0 |
| nations | 72 | 22 | 50 | 0 |

Check pass: 4,861 fields agree, 15 disagree (10 without Brazil, 5 in Brazil), 584 have no second source. The reconcile pass settled 6 of the 10. The open points are in section 4. `single-source.tsv` lists every single-source field, the 2000/01 pass included (665: 384 are the month of the last match, `ended`, 96 a winner, 110 a runner-up and 75 a `not held`). The 2000/01 checks: 529 agree, 1 disagrees (point 5 of section 4), 81 have no second source. They are mostly lower divisions, regional groups and small cups that only one publisher lists. They stay in the table as single-source facts.

### Conventions found

The label year follows the 3.9.68 data of each competition. The part `.md` files give the 3.9.68 rows that prove each one. Main groups:

- The year where the season ends: most leagues and cups of Europe, the European Champions Cup and the UEFA Cup.
- The calendar year: the leagues of Scandinavia, Brazil, Japan, Korea, China, Russia, the United States, the Irish leagues from 2003, most continental club cups outside Europe.
- The year where the season starts: Croatia, Greece, Turkey, the Australian National Soccer League, the Scottish League Challenge Cup, the Conference Cup, the Irish League Cup, the Hong Kong Senior Shield and the Thai Premier League.
- The year of the match: the super cups and the Charity Shield.
- Two rows per label year: the Argentine and Mexican first divisions (label Y holds the Apertura of Y and the Clausura of Y + 1), the Inter-Toto Cup (one row per joint winner), the Danish Second Division from 2006 (East and West), the Irish leagues and FAI Cup for 2003 (the 2002/03 transition season and the 2003 season).

### Names

The rows name each club as the 3.9.68 data does. `tools/seasons/history_fill.py` finds each name in each database: the full name, then `aliases.tsv` (86 lines, renamed and merged clubs, each with a source), then the short name, then the club of the same id in the 3.9.68 data when the database gives it a similar name (`matched-by-id.tsv`, reviewed; `not-same.tsv` holds 3 pairs that are other clubs). `unmatched.tsv` lists the names that no club of a database holds (clubs that the database does not have, for example Airdrieonians or small Swedish, Croatian and Chinese clubs). A row whose winner is not in the database is left out, and the database keeps its own rows of that year. `no-winner.tsv` lists each such row per database: slice 15 adds real clubs that a database lacks as new club records (Q335), and after slice 15 these rows can be filled. A runner-up, third or host that is not in the database stays empty.

## 3. In the game

Code: `src/recomp_rt/win/season_history.c` and `season_history.h`, the table `season_history_table.c` (made by `tools/seasons/make_history_table.py` from `docs/research/history-0.6.0/plan.tsv`, which `history_fill.py --write` makes). `ctest -R season_history_table` checks that the C table is the one that `plan.tsv` gives.

### Which rows

For a database before 2010/11 and a new game in its own season (start year S):

1. The competitions are those that hold history rows in the database. `skip.tsv` lists a competition whose name means another competition in one database.
2. For each such competition and each label year from 2001 to S that the research covers (2001 only from the 2000/01 pass, Q354), the rows of the database with that label year are cleared. Most of those rows hold the result of another season (the part `.md` files list them): for example the last row that slice 11b moves to S often holds the season before (Swedish Premier Division, September 2005: Malmö FF, the 2004 champion, at 2005).
3. The researched rows with `held = yes` are written when the edition ended before July of S, or when it is a calendar-year edition of the year S that ends in September or later (as the 3.9.68 data holds the 2001 season of the calendar-year leagues in a game that starts in July 2001). So a super cup or a Libertadores final of July or August S stays out. Inference: the game plays that edition itself in its first season; no address shows it for each competition.

`fill-report.tsv` gives the counts per database.

### How

`season_dates.c` calls `season_history_fill` right after the shift of slice 11b, in the same place (after the load of the database files 0x524d50, and again when the game is made in another year than the load: `season_dates_at_create`). It:

- finds the competitions by name among the competition pointers of the history rows (+0x04, name at +0x04 of the competition);
- finds the clubs by full name in the club table [0xae23b4] (count [0xae235c], the clubs and then the national sides), the club of the right nation (+0x53, name at +0x04 of the nation) when several share a name;
- clears a row by setting its competition to 0, the free row of the original (0x4afce0 reuses free rows; the id pass 0x532c1f writes 0 for a negative competition id, so the Update Team databases already hold such rows);
- writes the new rows into the cleared rows first, then into new rows from the original's own grow function 0x527f60 (`rt_callback`, listed in `tools/recomp/callbacks.py` with its first bytes), which reallocates the table, adds blank rows and moves the nation part pointer [0xae23dc];
- keeps the old bytes of each changed row and the old row count, so `season_history_undo` gives them back before a new shift (`shift_for`), and drops them in `season_dates_forget`;
- keeps the database rows of a competition and year when the planned row of that year has no winner in memory (the year does not become empty).

The grow can move the table: in the test runs 0x945667 moved it each time (September 2005: from 7055a390 to 7029c830). The shift of slice 11b journals the history years by address, so its restore (`season_dates.c`, `restore`) moves each journaled address inside the old table to the same row of the new table. The test switch `RECOMP_DATES_RESHIFT=<year>` makes the shift again for another year and back, as `season_dates_at_create` does when a game is made in another year than the load. The run `reshift` of the test shows 103 history years restored at the new place, and a save equal to the save of one shift.

The nation part pointer [0xae23dc]. After the grow it is base + (count - nation count) * 0x1a, as the original's grow sets it (0x527f8d to 0x527fb2) and as the original's prune sets it after it drops rows (0x539b49 to 0x539b70, called at the creation of the game, 0x828371). The original therefore never keeps the club and nation rows split by content after a grow or a prune. The readers: 0x52a01b, 0x52be64 and 0x52d1e8 are inside the loader 0x528540 (function map), so they run before the fill. 0x53005d, 0x5300ec and 0x530172 are inside the save writer 0x52dad0, which runs after the creation and its prune. No reader runs between the fill and the creation.

The game then treats the rows as its own: at the creation of the game it keeps only the rows of the selected nations (0x539ae0, function map; seen in the test: the Scottish rows are in memory after the load and gone at the first turn of an England-only game). A saved game holds the rows; its load does not run the fill.

Nothing changes for a start in 2001, for a start year that is not the season of the database (the table has rows for that season only), for a Custom database, and with `RECOMP_SEASON_CODE=off`.

### Test

`tests/seasons/season_history_run.py <program> <game> <test-fixtures/databases> [--extras] [--jobs 3]`: a new game in the season of each database before 2010/11 (and 3.9.68 on the 2002/03 tile). The test log `RECOMP_HISTORY_LOG=<competition>;...` prints the rows from 1999 on right after the fill and at the first turn. The checks: the English Premier Division, English FA Cup, European Champions Cup, Scottish Premier Division, Norwegian Premier Division, Italian Serie A and FIFA World Cup hold exactly the planned rows for every label year from 2001 to the start year (at the load; at the first turn for the competitions that the game kept), and each of them holds the real winner of a list of public results written in the test, independent of the research table. Also `off` (September 2005 with `RECOMP_SEASON_CODE=off`: no row written), `again` (September 2009 twice: equal saves), `reshift` (section 3, How) and `load` (the September 2005 save loads with the same rows, runs no fill and plays 3 days to a save).

### Rows per database

From `fill-report.tsv` (plan after the fix round of 2026-10-17, with the 2000/01 rows of Q354). "Added" are label years that the database lacked, "corrected" are label years where the database held another result, "agree" are database rows that the research confirms. "No winner" rows are left out and the database keeps its rows of that year (`no-winner.tsv`).

| Database | Start | Rows written | Added | Corrected | Agree | No winner | Database rows cleared |
|---|---|---|---|---|---|---|---|
| 3.9.68 (2002/03 tile) | 2002 | 416 | 160 | 16 | 240 | 0 | 262 |
| October 2003 | 2003 | 686 | 490 | 67 | 129 | 4 | 198 |
| September 2005 | 2005 | 1115 | 944 | 80 | 91 | 21 | 184 |
| November 2007 | 2007 | 1019 | 929 | 36 | 54 | 21 | 101 |
| February 2008 (extra) | 2007 | 1025 | 935 | 38 | 52 | 22 | 101 |
| September 2008 | 2008 | 1306 | 1205 | 52 | 49 | 23 | 117 |
| December 2008 (extra) | 2008 | 1018 | 943 | 33 | 42 | 21 | 86 |
| March 2009 (extra) | 2008 | 1271 | 1172 | 52 | 47 | 23 | 114 |
| September 2009 | 2009 | 1255 | 1166 | 43 | 46 | 15 | 102 |
| February 2010 ODB (extra) | 2009 | 1324 | 1175 | 66 | 83 | 16 | 164 |
| April 2010 SIM (extra) | 2009 | 1257 | 1169 | 73 | 15 | 13 | 128 |
| June 2010 (extra) | 2009 | 1322 | 1173 | 67 | 82 | 18 | 164 |

## 4. Max's answers and open points

Max answered on 2026-10-17 (spec, Q352 to Q357), and the build follows each answer:

- Q352: the researched rows replace the database rows of the label years 2002 to the start year (and 2001 where the 2000/01 pass has a row).
- Q353: the calendar-year season of the start year is added.
- Q354: the 2000/01 pass: `y2001-a.tsv` and `y2001-b.tsv` (233 rows for the 230 competitions of `targets-2001.tsv` that lack the label 2001 in at least one database after the shift), with their checks. Of 165 of those competitions with a 3.9.68 row at 2001, 163 agree with the research; the research corrects Italian Serie B (second Piacenza) and the Polish Super Cup (second Polonia Warszawa).
- Q355: titles taken away later show the result on the pitch: Serie A 2004/05 and 2005/06 Juventus (second AC Milan), the Chinese league 2003 Shanghai Shenhua, the Polish Cup 2005 Dyskobolia.
- Q356: Dutch Premier Division 2005/06: PSV first, AZ second (the table). Ajax was 4th; the play-off decided a Champions League place.
- Q357: the Austrian First Division and the Swiss National Division A rows carry the year where the season ends.

Open points (the rows stay as researched):

1. Mappings by inference: English tier 6 from 2004/05 (Northern Premier is Conference North, Southern is Conference South, Isthmian not held), the Swedish Second and Third Divisions from 2006, the Copa Sudamericana in the Copa Mercosur rows, the CAF Confederation Cup in the African CAF Cup rows, the AFC Cup not in the Asian Cup Winners' Cup rows, the Belgian Third Division not held from 2002 (A and B have their own rows).
2. Facts with one source each: Malaysian Premier One 2003 last month, Indian Chennai League 2003 winner, Santa Catarina 2002 runner-up (Criciúma or Avaí), Bahia 2002 (the row holds the Supercampeonato winner Vitória; the state FA championship went to Palmeiras do Nordeste, who are not in the data).
3. Oceania Nations Cup 2004 and 2008: no third-place match, so third is empty.
4. A label year with two researched rows (Argentina, Mexico, the Inter-Toto Cup) where one winner is not in the database: the database rows of that year stay beside the other researched row.
5. Portuguese Second League 2000/01, second: Varzim (RSSSF) or Vitória de Setúbal (Wikipedia); three clubs had 64 points. The row keeps Varzim.

## 5. Test results (2026-10-17, build of this branch)

With `G=/Users/max/Projects/cm0102/clean-3968`, `MAIN=/Users/max/Projects/cm0102/cm0102-mac/build/cm0102_recomp` and `R050=/Users/max/Projects/cm0102/wt-ref-0.5.0/build/cm0102_recomp`:

| Command | Result |
|---|---|
| `python3 tests/smoke/smoke_run.py build/cm0102_recomp $G --same-save --runs normal,again,season-code-off --jobs 3 --ref-build $MAIN --ref-050 $R050` | PASS: again, season-code-off and ref-build equal normal; text-0.5.0 equals ref-0.5.0 |
| the same with `--save start.sav` | PASS (the same four lines) |
| the same with `--save prematch.sav` | PASS |
| the same with `--save yeovil.sav --days 7` | PASS |
| `python3 tests/seasons/season_history_run.py build/cm0102_recomp $G /Users/max/Projects/cm0102/test-fixtures/databases --extras --jobs 3` | 15 runs, 0 failed: the 12 databases, off, reshift (103 history years restored at the new place, equal save), again, load |
| `python3 tests/seasons/season_dates_run.py build/cm0102_recomp $G /Users/max/Projects/cm0102/test-fixtures/databases --only 2002,2003,2005,2007,2008,2009,2010 --jobs 3` (first round) | all passed |
| `ctest` in `build/` | all passed, `season_history_table` included |
