# Release tests from 0.7.1
Status: Ready

## Problem

The 0.7.0 run set took several days for each pass. It held a Tier A game from 2001 to 2300, three full-world games from that game's saves of 2101, 2201 and 2291, Tier A games of seven starts with three seeds to 2100, and three date jumps. After each fix the affected rows played all ten legs again. Most faults that the long rows found came decades into a game, and the release waited for each one.

The run set also missed a fault on the first day. A new game with Select All and every extra division stopped at "swe_second..cpp 656" on the 3.9.60 data, and at line 643 on the 3.9.65 data (open item 224). No test set the extra divisions, and the run set played only the 3.9.68 data or a season database.

## Solution

From 0.7.1 a release passes a fixed release set that 10 games on the Mac finish in about 12 hours or less. It follows the rule that every soak plays through a World Cup and a European Championship with every season change between. It covers every start season tile, every database, changing rules on and off, Select All with every extra division, a date jump, two Tier A games to the 2060s and one network game.

A fix gets a short, fixed check. The runs to 2300 move to a background soak that plays only while the Mac is idle and does not hold a release.

## User stories

1. As Max, I want the release tests to end within about 12 hours, so that a release does not wait days for long runs.
2. As Max, I want every start season tile in the release set, so that a fault in a season database shows before the players find it.
3. As a player who starts in 2001/02 on the default 3.9.60 data with every extra division, I want the new game to start, so that I can play the lower divisions (item 224).
4. As a player on the 3.9.65 data with the Swedish extra division, I want the new game to start (item 224, second box).
5. As a player with changing rules off, I want club cups and national matches to keep apart, so that my club is not short of players (items 157 to 162).
6. As a player who plays thirty or sixty years, I want the competitions to keep going, so that my long game has World Cups and finals (items 183, 194, 201, 218).
7. As a player in a far year, I want the season change of 2100/01 to work, so that the non-leap year 2100 does not stop the game.
8. As a player of online play, I want a host and its joiners to see the same game through the lobby and the relay (items 212, 213, 216).
9. As the lead, I want a fixed per-fix check, so that each fix is retested the same way and quickly.
10. As the lead, I want the long runs to keep going in the background, so that faults in 2070, 2096 or 2222 still come to light for the next release (items 167, 182, 198, 223).
11. As the lead, I want one seed per row that changes each release, so that one release is repeatable and later releases try other games.
12. As the lead, when a fault shows in the background soak during a release, I want it on the list of the next release, so that the current release does not wait for it.

## Decisions

Settled with Max on 12 November 2026 (R1 to R11, all yes).

### End point

A soak starts in season Y and plays to about 1 September after the later of the next World Cup and the next European Championship that it reaches. A tournament played in June or July of year Y comes before a start in Y and does not count. The November 2022 World Cup counts for a 2022 start. Euro 2020 is played in 2021. Each soak saves and loads each July.

| Start | End | Start | End | Start | End |
|---|---|---|---|---|---|
| 2001 | Sep 2004 | 2010 | Sep 2014 | 2019 | Jan 2023 |
| 2002 | Sep 2006 | 2011 | Sep 2014 | 2020 | Jan 2023 |
| 2003 | Sep 2006 | 2012 | Sep 2016 | 2021 | Sep 2024 |
| 2004 | Sep 2008 | 2013 | Sep 2016 | 2022 | Sep 2024 |
| 2005 | Sep 2008 | 2014 | Sep 2018 | 2023 | Sep 2026 |
| 2006 | Sep 2010 | 2015 | Sep 2018 | 2024 | Sep 2028 |
| 2007 | Sep 2010 | 2016 | Sep 2021 | 2025 | Sep 2028 |
| 2008 | Sep 2012 | 2017 | Sep 2021 | 2026 | Sep 2030 |
| 2009 | Sep 2012 | 2018 | Jan 2023 | | |

The 2019 and 2020 end points are not used in the release set (those tiles are tile rows). They stay in the table for gate_run.py.

### The release set (R1 to R8)

The full world is a new game with Select All, the manager on holiday and the quick result engine. Each full-world row gets the history checker and the health checks on every save. It does not get the Tier B limits, because those need decades of saves. Each tile plays the database that it gives on the start screen: 3.9.60 for 2001, our own Summer 2002, 2004 and 2006 databases, and the season databases for the other years. Changing rules are on unless a row says off.

| Rows | Settings | Seasons | Protects against |
|---|---|---|---|
| 8 spine soaks: 2001, 2004, 2008, 2012, 2016, 2021, 2024, 2026 (R1) | full world, plain Select All, each to its end point. Together they play every season from 2001/02 to 2029/30 once, in the real calendar. | about 33 | faults of the running seasons: item 187 (2020, a squad with one player in 11 slots), 131 (Date box, 2024), 206 (a Champions League place from 7th, 2031 class), 214 and 215 (CONCACAF windows, 2023 and 2024), 197 (2025 Club World Cup), 219 (a Mexican Libertadores winner, 2001 and 2004), 217 (24-team finals) |
| 18 tile rows: 2002, 2003, 2005, 2006, 2007, 2009, 2010, 2011, 2013, 2014, 2015, 2017, 2018, 2019, 2020, 2022, 2023, 2025 (R1, R2) | full world, Select All with all six extra divisions, a new game through the first season change with the July save and load, to 1 September | about 22 | first-season faults of each database: items 100 and 222 (first-season European entrants), 189 and 176 (rows held twice at the start), 197 (missing history at the start), 123 (a box at the new game), 224 (an extra division that the data does not fit) |
| 3 database rows: 2001 on 3.9.60, 3.9.65 and 3.9.68 (R3) | full world, Select All with all six extra divisions. 3.9.65 and 3.9.68 play to Sep 2004. 3.9.60 plays to Sep 2002, because its spine soak plays on to 2004. | about 8 | item 224 (line 656 on 3.9.60, line 643 on 3.9.65), item 123 (a box at the new game on 3.9.68) |
| 2 rules-off soaks: 2001 on 3.9.68 to Sep 2004, 2016 to Sep 2021 (R4) | full world, plain Select All, changing rules off | about 8 | items 157 to 162 (club cups on national match days, rules off), item 102 (match_eng box, rules off) |
| 1 date jump: jump2098 (R6) | as in 0.7.0, through the season change of 2100/01 | 3 | the non-leap year 2100, item 169 (World Cup 2099 not written) |
| 2 Tier A rows: 2001 to 2060, 2026 to 2066 (R5) | Tier A, all nations, legs of 10 seasons, a checkpoint save each decade | 99 Tier A seasons | decade faults: items 201 (2033), 183 (2035), 218 (2039), 102 (2052), 194 (2054). Their checkpoint saves are the start of a fix replay. |
| 1 network game: release test 1 (R7) | two and four games on this Mac through the real lobby and relay | short | items 212, 213, 216 |

The six extra divisions are the English Conference, the German Regionalliga, Serie C2, the Spanish Segunda B, the Portuguese Second Division B and the Swedish Second Division. Select All sets none of them. The first three are clicks on the Select League(s) screen. The other three come from the test switch RECOMP_TEST_EXTRA, as rules_report_check.py does it now for Spain and Portugal.

Budget. A full-world season takes about 63 minutes with 10 games on the Mac (pass 6: b2025 played 4 seasons in 15103 s, b2001 10 seasons in 38211 s). A Tier A season takes about 7 minutes (a2001s2 100 seasons in 40908 s). The full-world rows need about 77 game-hours and the Tier A rows about 12. Ten games for 12 hours give 120. The longest rows are the Tier A row from 2001 at about 7 hours and the 2016 spine soak at about 5.5 hours. The first release run measures the real time (slice 4).

Seeds (R8). Each row plays one seed. The seed changes each release and stays fixed within it, so a fix replay plays the same game. The seed comes from the release version: 0.7.1 gives 71. The run set takes it as an argument.

The other release checks do not change: the smoke tests in both modes, the same-save set, the rules report, the screen and data sweeps, the Windows and Linux checks and online release tests 2 to 4. Full play with the match engine stays outside this 12-hour set as it is now (R11).

### The per-fix check (R9)

Each fix gets these checks, in this order:
1. The fix's replay: the save before the fault, played with the fixed build to the end of that row. The fix's log line must fire.
2. The same-save set against main.
3. h-r2001-e: the rules report of the base game from 2001 to 2027 with England.
4. One fresh Opus review. A second review runs only when the first finds a real problem.
5. A full rerun of every release row whose saves or logs show the fix's trigger. The fix session gives a scan for the trigger, as for items 223 and 224. When no scan is possible, every row with the same start, database and rules setting reruns.

Release rows are 7 hours or less, so full reruns stay within a night. Every failure is still fixed and retested before the release.

### The background soak (R10)

Two games play only while the Mac is idle, from the release build on:
- a Tier A game from 2001 to 2300, with its checkpoint save each decade
- a full-world game from 2025 to 2100

They are not in --verify and do not hold a release. A fault that they find goes on the open items of the next release, with its save. Items 167 (2074), 223 (2070), 198 (2096) and 182 (2222) were in reach only of runs this long.

The 0.7.0 rows that leave the release set: the Tier A rows of seven starts with three seeds to 2100, a2001esb, a2001nation, a2001to2300 as a release row, jump2198, jump2298, b2001, b2025, late2101, late2201 and late2291. The weekly-market rows bw2001 and bw2025 and the long Tier B rows b2001long and b2025long leave the background set. The scripts keep the Tier B and late tiers, so a later release can use them for one fix.

## Testing

The seam is run_set.py with run_set.tsv: its ctest future_run_set tests the script without a game, and its --set quick plays short games. A good test checks that a row with a database and extra divisions starts the right new game, ends at its end point and passes --verify. It does not check how run_set.py builds its command lines.

Similar tests that exist: rules_report_check.py --extra (the clicks of the three screen extras and RECOMP_TEST_EXTRA), gate_run.py (soaks to the World Cup and Euro end point), and the item 224 sweep runs in the open items.

## Out of scope

- Full play with the match engine (R11).
- A network game that plays through a season change. No script for it exists yet.
- Changes to the history checker or to history_known.tsv.
- Speed changes to the game.

## Open decisions

None.

## Slices

### 1. Database and extra divisions in run_set rows
Blocked by: none
Delivers: a run_set.tsv row can name its database (3.9.60, 3.9.65, 3.9.68 or the tile default) and set all six extra divisions. Its end can be the end point of its start, and the seed can come from an argument.
Merge danger: two-way door. Blast radius: test scripts only. A wrong row setting tests the wrong game and gives a false pass, so the acceptance checks read the save.
- [x] ctest future_run_set covers the new columns and the end point table.
- [x] A quick row with 3.9.60 and every extra division makes a new game whose save holds the Swedish Second Division part (club_comp 108) and the Conference, Regionalliga, Serie C2, Segunda B and Portuguese Second Division B.
- [x] A quick row with the 3.9.68 data plays 3.9.68, checked by the database signature in the save.
- [x] --seed sets the seed of every row.

### 2. The release set and the background set
Blocked by: 1
Delivers: run_set.tsv holds the 34 release rows and the 2 background rows of this spec. The 0.7.0 rows are removed. --verify checks the new release set.
Merge danger: two-way door. Blast radius: the release check. A missing row lets a release pass with a gap, so the acceptance check counts the rows against this spec.
- [x] The release set has 8 spine soaks, 18 tile rows, 3 database rows, 2 rules-off soaks, jump2098 and 2 Tier A rows, each with the start, database, extras and end of this spec.
- [x] The background set has the Tier A game to 2300 and the full-world game from 2025 to 2100.
- [x] ctest future_run_set passes. --set quick passes (7 of 7, after items 227, 228 and 231; t072, 13 November 2026).

### 3. TESTING.md and the release script
Blocked by: 2
Delivers: docs/TESTING.md describes the release set, the per-fix check and the background soak. The release script runs run_set.py --verify on the new set with the release's seed and online release test 1.
Merge danger: two-way door. Blast radius: the release process. A release script that checks the wrong set blocks or passes a release by mistake.
- [x] TESTING.md has no reference to the 0.7.0 long rows as release checks.
- [x] A dry run of the release script reads the new set.

### 4. First timed release run
Blocked by: 3
Delivers: one full run of the release set on main, with the time of each row. Each failure goes on the open items of 0.7.1.
Merge danger: two-way door. Blast radius: none in the code. A run longer than 12 hours goes back to Max with the measured times before any row changes.
- [ ] Every row has a result and a time.
- [ ] The wall time of the whole set is recorded. If it is over 12 hours, Max gets the times and a proposal.
