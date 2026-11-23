# Real league sizes (0.7.2)
Status: Ready

Max, 13 November 2026: real league sizes go into release 0.7.2. This fixes the league structure of the existing leagues to match real life. It is not "more leagues" (issue 86). Research: `docs/research/2026-10-18-league-sizes.md` (the October slice 18 doc) and `docs/research/2026-11-13-league-sizes-check.md` (the check of 13 November). The decisions below are lead decisions of 13 November 2026, taken with the answer most faithful to real life (the lead's instruction: no questions to Max).

## Problem

In a season game each league plays the club count of its class in the original, up to the class's largest count. 23 leagues hold more clubs in real life (Argentina 30, Serie A 20, Serie B 24, the Eerste Divisie 20, J1 20, J2 22, the K League 16 and others). Divisions that a cup needs never go below the class count, so the Welsh Premier plays 18 when the real league has 12 and the Belgian First Division plays 18 when it has 16. The class also fixes the meetings, so J2 plays four meetings and the Northern Irish leagues play four when the real leagues play two or three. Argentina plays two halves of 20 every season, where the real league had one table of 30 and later two zones of 15. In 0.6.2 we told players that real sizes would come next.

## Solution

A season game plays every league of the rules table at its real club count, with its real meetings, and with its halves, zones, splits and periods where the real league has them. The base game stays the original. A new shared engine in our own runtime code builds the table and the fixture dates of a league of any size from the rules row. The cups take the real divisions. Promotion and relegation bring a division to its new size at the season change.

## User stories

1. As a player of a 2016/17 game, I want Serie A to hold 20 clubs and 38 rounds, so that the season matches real life.
2. As a player of a 2025/26 game, I want the Argentine Primera to hold 30 clubs in two zones of 15 with the inter-zone rounds, so that Argentina plays as it does now.
3. As a player of a 2010 game, I want J2 to play 2 meetings of 19 clubs, so that the season has 36 rounds and not 72.
4. As a player of a 2012/13 game, I want the Welsh Premier to hold 12 clubs and split into two groups of 6 after 22 rounds, so that the league plays its real format.
5. As a player of a game with changing rules on from 2001, I want Serie A to grow from 18 to 20 at the 2004/05 season change, so that the league follows its history.
6. As a player who loads a 0.7.1 save, I want the game to go on, and the leagues to take their real sizes at the next season change with changing rules on.
7. As a player in a league of 24, I want the cups of my nation to take every club of the real divisions, so that no cup leaves out or doubles a club.
8. As a player of the base game, I want nothing to change, so that 2001/02 plays as the original.
9. As a player of a calendar-year league that grows, I want no match in the winter break, so that the season keeps its real shape.
10. As a player of a league that shrinks, I want the season to end on its usual date, so that the season does not stop weeks early.
11. As a player of a 2009/10 game, I want the Belgian First Division to hold 16 clubs and play Play-off I with halved points, so that the title is decided as in real life.
12. As a player of a Northern Irish game, I want the cup to take the real Premiership and First Division, so that a count above 20 does not break it.
13. As a tester, I want one season test per league that shows the real count and the rounds played, so that each slice proves its leagues.

## Decisions

All decisions are lead decisions, 13 November 2026.

D1. Scope. Every league of the research table "Leagues whose size change the code cannot hold", the smaller counts that the cups blocked, the Argentine second division and the Brazilian Série A and B. The Allsvenskan is done by issue 106 and moves into the engine's row form. The Ekstraklasa format stays with issue 92. Not in scope: the MLS (D17), regional groups of one division (D16), new divisions or nations.

D2. Structure in scope: the club count, the meetings per pair, halves (Apertura and Clausura, two stages), zones of one division (Argentina), splits after N rounds into sections (with points kept or halved), and periods. A play-off that decides the title, a European place or a final after the table plays only where the original or the play-off kind of 0.6.0 slice 19 already plays it. Elsewhere it plays in its nearest form, the 0.6.0 rule: the place goes to the next club of the table.

D3. Meetings come from the rules table. The encoder (`encode_leagues.py`) gets a `meetings` column for every league row, read from the `format` text (single 1, double 2, triple 3, four 4), with hand-encoded rows where the text is unclear. With no value the class's meetings stay.

D4. Date template. The engine builds exactly R dates, R = meetings x (n' - 1) with n' the count rounded up to even. It takes the class's own dates first. Extra rounds go on the Wednesday of a gap of 7 days or more, spread evenly, never in the winter break (the largest gap of the template, and any gap over 14 days). When the gaps run out, the season starts earlier in steps of 7 days. It never ends after the class's last date. Mirrored classes (flag 0x80, the French classes) get the same number of new dates in each half. When no room is left, the class keeps its count for that season and the log names it. The season test of the league then fails, so the slice fixes it.

D5. Fewer clubs. The engine drops dates evenly and keeps the first and the last date, so the season ends on its usual day.

D6. Periods. On or off by the row (the Belgian Second Division has none in 2007/08 to 2009/10). The period limits and lengths of the class scale to R, rounded, so that the lengths add up to R.

D7. Splits. One runtime split for any class: after N rounds, sections of the row's sizes, the row's meetings in the sections, points kept or halved. The rules come from the `split` encoding of 0.6.0 slice 19 ("after N: A+B, points P"). The Scottish Premier keeps its own split code, because its real format never changes from 12 clubs and 33 + 5.

D8. Halves. On or off by the row (`split` = stages). JapJ1 plays halves and the final only in the 2002 to 2004 and 2015 to 2016 seasons, and one table in the others. The 2026 special J1 season plays one table of its row. Argentina plays halves only where the row says so.

D9. Argentina. The Primera plays its row's form each season: halves of one meeting, one table of one meeting (2014/15 to 2023/24), or two zones of 15 with two inter-zone rounds a half (2024/25, 2025/26, and later seasons by the after-table rule). The zone lists come from two sources for each real season. A promoted club takes the zone of the club it replaces. The title knockout of 16 is not played: the champion of each half is the club with the most points of both zones (D2). The relegation averages take the real count.

D10. Argentine second division. It plays its row's count and form with the zone code of D9. The functions that need exactly 25 clubs take the real count. Its promotion play-off (Reducido) plays where the class's stages fit the count, else the direct places of the row (D2).

D11. Brazil. Série A plays two meetings from 2003 and Série B from 2006, with no play-off of the top 4 in those seasons. The Série B play-off of 2026 on plays in its nearest form (D2).

D12. Club moves at a new game. The class cap goes. Each division fills to its row's count from the research club lists, then from the nation's clubs with no division in the database. Reserve, youth and university sides are never added unless the real league holds them. When the database holds too few clubs, the division plays with the clubs it has, and the log names it.

D13. Size step at the season change, with changing rules on. After promotion and relegation, a division with fewer clubs than its new row takes the best-placed clubs of the division below that did not go up. A division with more takes the worst-placed clubs down to the division below. With changing rules off the start season's sizes stay (Q232).

D14. Cups take the real divisions in a season game. Each cup that fills fixed places or a fixed list gets its list size and its counts at run time from the divisions (as `rt_cup_room` does). The `FLOORS` of `league_sizes.py` go. The base game keeps every cup as the original.

D15. Promotion and relegation places of a grown division come from its row. The English Conference exchange (`EngPrm::exchange_conference_clubs_unplayed` 0x574e60, 1 for 1 at 0x575423..0x57548d) takes the row's places. The German split of 36 (0x5dbc3b) and other chains outside the size table keep their pairs.

D16. Regional groups of one division (the Greek second tier, the Croatian second tier 2002/03 to 2005/06), the Korean stages of 2004 to 2006, the Finnish Ykkönen as one division and the Danish 2nd Divisions play as now. They need a zone for each club that the research does not hold, or new division objects.

D17. The MLS keeps 12 clubs in three groups of 4. Its conferences, unbalanced schedule and play-offs need a new league structure, which is its own spec.

D18. Saves. A 0.7.1 save loads in 0.7.2 and its leagues take their row sizes at the next season change with changing rules on (D13). A season-game save of 0.7.2 does not load safely in 0.7.1. The changelog says so. Nothing blocks it.

D19. Base game. Every new runtime function returns the original's value and allocates nothing outside a season game (rule Q152). `RECOMP_LEAGUE_SIZES=off` keeps the original counts in a season game, as now.

D20. Rules table corrections. The cells of the check (research doc of 13 November, "Rules table cells to correct") are corrected with their sources. A future row with no announcement holds `unknown`, and the after-table rule then keeps the last known count.

D21. The engine table. `GROWN` becomes one row per class in `league_sizes.py`: the file, its cap, the `create_schedule` address and its mode (shuffled, sequential, mirrored, halves, periods). The tool finds the table allocation next to the class's count store and stops the build when it finds none. `make_size_wraps.py` writes one `create_schedule` wrap per row. Sites that a later slice already patches (the Serie C pushes of `new_divisions.py`) are not patched twice.

D22. Fixed arrays and rows of the classes (the Serie A and Serie B tie-break arrays, the Serie C1 regroup and play-out rows, the RusFirst take loop, the CroA2a tie-break array, BelThird row 15, the Argentine averages) are replaced by runtime code that takes the count, in the slice of their league.

D23. Both directions, in the right year (Max, 13 November 2026). Every change of a division's club count in the rules table plays in the season it happened, up or down, for every league in the game, also those whose new count the class already holds. In the season before a change, the division plays that season's real promotion and relegation places from its row (for example four down when the league goes from 20 to 18), so the step comes through the real places. The size step of D13 is the fallback only when the real places do not reach the new count (a start season, a database with too few clubs). Slice 3 owns this. Slice 18 checks every count change of the rules table from 2001 to 2027: each one shows in the save of its season, and the season before shows its real places.

## Testing

Three seams, all existing:

1. The same-save set with rules off (`tests/smoke/smoke_run.py --jobs 1`, skill `same-save-check`): the base game and the 2001 start stay byte for byte the same. Every slice that touches game code runs it.
2. A season game per league: one shared script, `tests/seasons/league_size_run.py`, made from issue 106's `swe_prm_run.py`. For a file and a start year it plays one season (and with `--to` the next season change) and checks the club count of the row, the rounds (meetings x (n' - 1)), that every club plays its matches, that no two rounds share a date, that no round falls in the winter break, that the season completes and that the next season starts. Each league slice adds its rows to the script's table and to the ctest list.
3. The rules report (`tests/seasons/rules_report_check.py`): the `clubs` lines of the grown leagues leave the expected files, and new `meetings` and `rounds` columns are compared with the rules table.

Each slice also follows the per-fix check of the 0.7.2 brief where it changes rules (the h-r2001-e rules report of England from 2001 to 2027).

## Out of scope

- New leagues, divisions or nations (issue 86).
- The MLS structure (D17).
- Regional groups of one division, the Korean stages, the Ykkönen and the Danish 2nd Divisions (D16).
- Title, European and final play-offs that the code does not play (D2).
- Calendar moves (Ireland 2003, Russia 2011/12, Argentina 2016/17, Japan 2026).
- The A-League schedules of 26 or 27 games and its finals.
- Scottish split changes (D7).

## Open decisions

None.

## Slices

Shared files: `tools/recomp/league_sizes.py`, `tools/seasons/make_size_wraps.py`, `src/recomp_rt/win/season_sizes.c`, the expected files of the rules report, `CMakeLists.txt`. Each league slice adds its own block of rows to `GROWN`, its own runtime file for class code (`season_sizes_<nation>.c`) and its own ctest lines. Cup slices add `season_cup_sizes_<group>.c`. The lead keeps both sides at merge.

### 1. Rules data for structure
Blocked by: none
Delivers: the encoded rules give `meetings`, halves, zones, splits and periods for every league row. The rules table cells of the check are corrected. The game reads `meetings` from the row. The rules report prints meetings and rounds, and the check compares them.
Merge danger: two-way door. Blast radius: the rules table and the expected files. No game change without a later slice.
- [x] `encode_leagues.py` writes `meetings` for every league row, with hand rows where the text is unclear, and its test covers each form
- [x] The cells of "Rules table cells to correct" are fixed with sources and `rules_lint.py` passes
- [x] The rules report shows meetings and rounds, the check compares them, and the expected files are made again and read

### 2. Shared engine: table and dates of any size
Blocked by: 1, and issue 106 on main
Delivers: `GROWN` holds one row per class with the found table site and a generated `create_schedule` wrap. The engine builds the table and the template of D4 and D5 for shuffled, sequential and mirrored classes, sets +0xa7 to the real matches, and takes the meetings of the row. The Allsvenskan moves into the row form. Pilots: the Eliteserien (16) and the Portuguese Segunda Liga (up to 24, D also at 0x7d16bb).
Merge danger: one-way door for saves (a save holds the grown table). Blast radius: every league of a season game.
- [x] `league_size_run.py` passes for nor_prm 2009 and por_second 2014, and swe_prm_run.py still passes
- [x] A calendar-year league gets no round in its winter break, and a shrunk league ends on its usual date
- [x] The same-save set is equal with rules off

### 3. Club moves and the size step
Blocked by: 2
Delivers: a new game fills each division to its row (D12). At a season change with changing rules on, each division reaches its new row's count (D13). A 0.7.1 save reaches the row at its next season change (D18).
Merge danger: one-way door for saves. Blast radius: club lists of every division in a season game.
- [x] A 2001 game with changing rules on across 2004/05 gives Serie A 20 in 2004/05
  Slice 9 adds ita_ser_a to the grown classes. `tests/seasons/size_step_run.py italy2001` passes: Serie A has 18 clubs to 2003/04 and 20 from 2004/05. The size step and D23 are also proven by Allsvenskan 14 to 16 in 2008 and Ligue 1 20 to 18 in 2023/24 (`size_step_run.py` grow and shrink).
- [x] A 0.7.1 save of a 2015 game loads and reaches the row sizes at the next season change
- [x] Each division short of clubs in a database is named in the log, and the same-save set is equal

### 4. Cups of Britain and Ireland follow the divisions
Blocked by: 2
Delivers: `EngFaCup` (the Conference array of 22), `EngCcCup`, `WelCup`, `WelLgeCup`, `NirCup` (exactly 20 today), `ScoLgeCup` and `ScoChalCup` take the real divisions. Their floors go.
Merge danger: two-way door. Blast radius: the cups of these nations.
- [x] A 2012/13 game with a Welsh Premier of 12 and a 2015/16 game with a Conference of 24 play every cup round (live: Welsh Premier 12 and the 2015/16 Conference of the database, `tests/seasons/cup_sizes_uk_run.py`; a Conference of 24: host test `season_cup_sizes_uk`, the cap holds it at 22 until slice 8)
- [x] A Northern Irish game with 24 to 29 clubs in the two divisions plays the Irish Cup (host test `season_cup_sizes_uk` at 24 and 29; live n2002 at 18, the caps hold 10 and 10 until slice 13)
- [x] The same-save set is equal (rules off, against the lead-072-base build e27b094e)

### 5. Cups of western Europe follow the divisions
Blocked by: 2
Delivers: `FraCup`, `FraLgeCup`, the CFA fill, `ItaCCup`, the Coppa Italia positions (Serie A 20 leaves 15th to 17th out today), `HolCup`, `BelFaCup` (112 today) and `DenCup` take the real divisions. Their floors go.
Merge danger: two-way door. Blast radius: the cups of these nations.
- [x] A season game per nation with a grown and a shrunk division plays every cup round (`tests/seasons/cup_size_run.py` runs the shrunk divisions: bel2016, den2020, fra2024, ita2018, hol2005. The grown divisions need slices 8, 9 and 10. `test_season_cup_west.c` checks their cup counts, and those slices' runs check the cups.)
- [x] The same-save set is equal

### 6. Cups of eastern Europe and Asia follow the divisions
Blocked by: 2
Delivers: `GreCup`, `CroCup` (48, no bound), `RusCup`, `TurCup`, `PolLgeCup`, `JapJCup`, `JapEmpCup` and `KorLeagueCup` take the real divisions. Their floors go.
Merge danger: two-way door. Blast radius: the cups of these nations.
- [x] A season game per nation with a grown and a shrunk division plays every cup round (`tests/seasons/cup_sizes_run.py`: grown-2010, shrunk-2012, poland-2007, mixed-2020; sizes above the class caps through the seam `RECOMP_CUP_SIZE_TEST` until slices 11, 12 and 14; no K League under 10 in the rules)
- [x] The same-save set is equal (start.sav, prematch.sav, new game and match pass on 2d203e4e; the reference build is this branch without slice 6, because main lacks the lead base)

### 7. Split engine
Blocked by: 2, 4
Delivers: the runtime split of D7 on any class. Pilots: the Northern Irish Premiership (12, 3 meetings in place of 4: 33 matches, then sections of 6 and 6 that meet once more, 38 matches; gap fix, sources in docs/research/2026-11-15-split-points.md) and the Welsh Premier (12, 22 + 6 and 6, and 16 with sections of 6, 4 and 6 in 2026/27).
Merge danger: one-way door for saves (a save holds the section stages). Blast radius: leagues with a split row.
- [x] `league_size_run.py` passes for nir_prm 2010 and wel_first 2012 and 2026, with the sections and their rounds
- [x] A save during the split loads and the season completes
- [x] The same-save set is equal

### 8. England and France
Blocked by: 2, 3, 4, 5
Delivers: the Conference at 24 with the row's exchange places (D15), League One by its row, the National at 21 (2010/11, a template block of 42) and at 17 and 18.
Merge danger: two-way door. Blast radius: the English and French pyramids.
- [x] `league_size_run.py` passes for eng_conf 2010 and fra_third 2010 and 2014
- [x] A 2005 game with changing rules on through 2007 moves the row's Conference places (`size_step_run.py conference`)
- [x] The same-save set is equal

### 9. Italy
Blocked by: 2, 3, 5
Delivers: Serie A 20, Serie B 22, 24 and 19, Serie C1 19, 16 and 17, with the tie-break arrays, the C1 regroup and the play-out rows taking the count (D22), and the expected counts of 0x642476 and 0x642478 from the places.
Merge danger: two-way door. Blast radius: the Italian pyramid and the Serie C groups.
- [x] `league_size_run.py` passes for ita_ser_a 2005, ita_ser_b 2003 and 2018, ita_ser_c1a 2004 and 2012
- [x] A 2001 game with changing rules on to 2005 changes sizes with no fault line
- [x] The same-save set is equal

### 10. Netherlands, Belgium, Denmark, Finland
Blocked by: 2, 3, 5, 7
Delivers: the Eerste Divisie at 20 with periods by row (D6), the Belgian Second Division at 19 and 8 to 17 with BelThird's row taking the count, the Belgian First Division at 16 with Play-off I and halved points, the Superliga at 14 with 2 meetings and its split, the Veikkausliiga at 14 with 2 meetings and its splits.
Merge danger: two-way door. Blast radius: these four pyramids.
- [x] `league_size_run.py` passes for hol_first 2005 and 2010, bel_second 2008 and 2016, bel_first 2010, den_prm 2017, fin_prm 2005 and 2019
- [x] The same-save set is equal (start.sav, new game, prematch.sav and match pass on d8c240c1 against the lead base build 6f4ef2f7)

### 11. Croatia and Greece
Blocked by: 2, 3, 6, 7
Delivers: the Croatian second tier at 18 (the tie-break array takes the count) and 12 to 15, the Croatian First Division at 10 and 12 with its meetings, the Greek Super League at 16 and 18 with its split from 2019/20, the Greek second tier up to 21 and down to 10.
Merge danger: two-way door. Blast radius: these two pyramids.
- [x] `league_size_run.py` passes for cro_a2a 2020, cro_a1 2010, gre_prm 2005 and 2019, gre_second 2010 and 2014
- [x] The same-save set is equal

### 12. Russia and Turkey
Blocked by: 2, 3, 6
Delivers: the Russian First Division at 20 and 22 (the take loop and its array take the count) and 17, the Süper Lig at 19 to 21, the Turkish second tier at 17 to 19.
Merge danger: two-way door. Blast radius: these two pyramids.
- [x] `league_size_run.py` passes for rus_first 2005 and 2012, tur_first 2020 and 2021, tur_second 2023 (session sz12, 16 Nov, build of d651b66e: all five PASS)
- [x] `league_size_run.py` passes for rus_first 2011: the long 2011–12 season plays its first stage only, 20 clubs and 38 rounds, inside its game year. The split of row 2010, played in 2012, is a gap: a season that spans two game years (lead decision 16 Nov) (session sz12, 16 Nov, build of 9bfd7c91: PASS, 20 clubs, 38 rounds)
- [x] The same-save set is equal (session sz12, 16 Nov: 9bfd7c91 against lead-072-base bf80a5e5, start, prematch, new and match PASS)

### 13. Northern Ireland and Ireland
Blocked by: 4, 7
Delivers: the Northern Irish First Division at its row's count with its meetings and splits, and the Irish First Division at 8 (2012 to 2017).
Merge danger: two-way door. Blast radius: the two Irish pyramids.
- [x] `league_size_run.py` passes for nir_first 2016 and 2022 and ire_first 2013
- [x] The same-save set is equal

### 14. Japan and Korea
Blocked by: 2, 3, 6, 7
Delivers: J1 at 18 and 20 with halves and the final only in their seasons (D8), J2 at up to 22 with its row's meetings, the K League at 12 to 16 with its meetings and its split.
Merge danger: two-way door. Blast radius: the Japanese and Korean leagues.
- [x] `league_size_run.py` passes for jap_j1 2005 and 2015, jap_j2 2010, kor_league 2013
- [x] The same-save set is equal

### 15. Argentine Primera: halves and zones
Blocked by: 2, 3
Delivers: halves on and off by row, one table of 30, two zones of 15 with two inter-zone rounds a half (D9), zone lists from two sources, the averages at the real count.
Merge danger: one-way door for saves (zone stages in the save). Blast radius: Argentina.
- [x] `league_size_run.py` passes for arg_prm 2014, 2016 and 2024, with the zones and the inter-zone rounds
- [x] A save in the second half of 2024/25 loads and completes the season
- [x] The same-save set is equal

### 16. Argentine second division
Blocked by: 15
Delivers: the second division at its row's count and form (D10) with the zone code of slice 15, and the functions that need 25 taking the count.
Merge danger: two-way door. Blast radius: Argentina's second tier and its promotion.
- [x] `league_size_run.py` passes for arg_second 2010 and 2016
  Halves and zones rows play as one table (no zone lists in any source), open item 252.
- [x] The same-save set is equal

### 17. Brazil
Blocked by: 2
Delivers: Série A at 24 and 20 in two meetings from 2003, Série B in two meetings from 2006, no top-4 play-off in those seasons (D11).
Merge danger: two-way door. Blast radius: Brazil.
- [x] `league_size_run.py` passes for bra_nat_first 2003 and 2010 and bra_nat_second 2006
- [x] The same-save set is equal

### 18. Full check
Blocked by: 1 to 17
Delivers: the rules report of every nation from 2001 to 2027 with changing rules on, a start in each nation group in 2010, 2016 and 2025, and the October research doc marked with what 0.7.2 holds.
Merge danger: two-way door. Blast radius: none (tests and docs).
- [ ] The rules report check passes for every nation group with no `clubs` line of a grown league left
- [ ] The release set rows of the 0.7.2 brief pass
- [ ] The same-save set is equal against the 0.7.1 build

## Parallel work

After slice 1, slice 2 runs alone. After slice 2, slices 3, 4, 5, 6 and 17 can run at the same time. Slice 7 starts when 4 is done, slice 15 when 3 is done. Then slices 8, 9, 10, 11, 12, 13, 14 and 16 can run at the same time as their blockers finish, because each touches its own nations. Slice 18 runs last.
