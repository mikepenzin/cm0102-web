# Future play to 2300 (0.6.2)
Status: Ready

## Problem

Players keep a game going for a long time, some of them to 2300. A forum player went to 2097 on 0.6.0 with the 3.9.68 database and changing rules on. They found these faults:
- After about 2030 the World Cup, Euros and Olympics are no longer played, and their draw screens give dates in 2199 or 2200.
- Some finals stop much earlier.
- History screens show some years twice and some rows with no host.
- European nations win the "Asian Cup Quals".
- The Suruga Bank final has the same two clubs every year.

Our tests did not find these faults, because the long games stopped at 2027/28 and checked only the leagues. A code inventory and an audit then found 35 faults in all. Leagues lose their rules in 2037, cups fall back to the start year's rules in 2100, saves and new games after 2099 are refused, most national teams lose their manager for good, money above £2.1 billion shows wrong, the 256th cap resets to 0, and the save fails at 999 parts. Full list: the open items of 0.6.2 (kept outside the repo).

## Solution

Every competition, league and rule continues to 2300 and later. A competition that reaches the end of its rules table goes on with its last cycle and last rules (the after-table rule). Hosts come from the real announcements, then from the original game's host rule. Saves from 0.6.0 and 0.6.1 with competitions stuck at the idle year move them to their next edition at load. Long-term faults that are in the original game are fixed too.

New tests reach far years quickly:
- Unit tests of the calendar code, stepped to 2300.
- A Tier A run, which plays only the competitions to a far year.
- A Tier B run, which plays the full world to check player pools, managers, money, save size and speed.
- A date jump, which starts a new game in 2098, 2198 or 2298.
- The history checker, which reads a save and checks the history rows and the entrants of every competition.

The run set of these tests becomes a release check. 0.6.2 ships only when the checker finds no fault to 2300.

## User stories

1. As a player in 2097, I want a World Cup every four years, so that my national team career goes on.
2. As a player, I want the draw screen of a competition to give its next real draw date, not a date in 2199 or 2200.
3. As a player, I want the Euros, Olympics, Africa Cup of Nations, Asian Cup, Copa América, Gold Cup and OFC Nations Cup to go on after their last known edition, on their last cycle.
4. As a player, I want each final to have a host in its history row, so that the history screen is complete.
5. As a player after 2030, I want hosts that change, from the original game's host rule, and the hosts already announced (World Cup 2034 Saudi Arabia, Euro 2032 Italy and Turkey, Olympics 2028 Los Angeles and 2032 Brisbane).
6. As a player, I want a host pair that names a nation that no longer exists to be drawn again from the other pairs.
7. As a player, I want the qualifiers after 2030 to fill the number of places the finals have, with the host places taken into account.
8. As a player with a 0.6.0 or 0.6.1 save past 2030, I want my stuck competitions to start again when I load the save in 0.6.2.
9. As a player, I want each year to appear once in a history list, and in the right place.
10. As a player, I want only Asian nations in the Asian Cup qualifiers, and the UEFA Nations League to be a European competition.
11. As a player, I want the Suruga Bank finalists to be that year's J.League Cup winner and Copa Sudamericana winner, up to its end in 2019.
12. As a player, I want the Copa Sudamericana, the Recopa, the FIFA Intercontinental Cup and AFC Champions League Two to go on every year after 2027.
13. As a player, I want the Club World Cup every four years from 2029, with 32 clubs.
14. As a player, I want the UEFA Super Cup in a different city each year after 2027.
15. As a player, I want promotion, relegation, play-offs, substitutes and foreign player rules to keep their last known form after the rules table ends, with no end year.
16. As a player, I want the right clubs and nations to enter each competition by its access rule, in every year.
17. As a player managing a national team that hosts or qualifies, I want the finals to work as they do for an AI nation.
18. As a player, I want every national team to get a new manager when its manager retires.
19. As a player at a very rich club, I want the right balance on the club info screen and AI clubs that keep buying.
20. As a player in a late-start game, I want caps above 255 to count.
21. As a player, I want my manager's age to be right after 2100.
22. As a player in 2232 and later, I want my game to save.
23. As a player in a long game, I want each game day to take no longer than it did in the first seasons.
24. As a player, I want saves and new games after 2099 to work.
25. As the developer, I want a test that reaches 2300 in hours, not weeks, so that faults are found before players find them.
26. As the developer, I want a test that starts in a far year, so that a fault tied to a year number is found in minutes.
27. As the developer, I want a release check that fails on any history, entrant or world-health fault to 2300, so that these faults do not come back.

## Decisions

Settled in three grilling rounds with Max on 30 October 2026.

After-table rules:
- After its last table edition, each international final continues on its last cycle, with no end year (decision A).
- Hosts after the table come from the original game's host rule. The original keeps host rows that roll on by a fixed step: 32 years for the World Cup, Euros and Olympics, and 16 for the Africa Cup of Nations. Five years before each edition, the host is drawn from three candidate pairs (Q4).
- Announced editions and hosts are added to the tables first, and each fact is checked against a source (Q5).
- A drawn pair that names a nation that no longer exists is drawn again from the other pairs (Q6).
- Where the original rule does not fit (the Copa América and Asian Cup rows, whose roll does not match their cycle, the Gold Cup and OFC Nations Cup with no original rows, and a row whose three pairs all name nations that no longer exist), the host rotates in order through the nations that hosted that competition in the table and still exist (Q32).
- The World Cup qualifiers take the place counts of the continued edition, so 48-team finals get 48 qualifiers and hosts (fault 32).
- Every league, play-off, substitute, foreign player and cup rule keeps its last table row, with no end year and no fall-back to the start year (Q1).
- The Copa Sudamericana, Recopa, FIFA Intercontinental Cup and AFC Champions League Two continue every year in their last known format (Q2).
- The Club World Cup is played every four years from 2029, with 32 clubs (Q3).
- The Suruga Bank keeps its end in 2019, and only its finalists are fixed (Q7).
- The UEFA Super Cup venue changes every year after 2027. It is drawn from past final venues (Q28).
- Russia's UEFA suspension stays in force to 2300 (Q29).
- The Confederations Cup stays ended and never reaches the idle year.
- No competition uses the idle year after 0.6.2.

Saves:
- A save from 0.6.0 or 0.6.1 with a competition waiting for the idle year moves it to its next edition after the save's date, at load (Q8).
- A qualifying campaign that a save of 0.6.0 or 0.6.1 made in the original's shape for an edition after the table keeps that shape to its end; its next campaign takes the new shape (open item 58). The load repair marks it, and the season part of the save keeps the marks as version 5 (the caps and goals fields, then u32 count and u32 record id and u32 campaign year per mark). A save without the field has no mark (lead, 31 October 2026). The finals of that campaign take the teams it qualifies after the load, and the fill gives their other places by reputation. A class at the idle year is repaired only in a save of an old build (its ended Confederations Cup waits at 2200); a class stuck in an edition more than a year past (a stall of 0.6.1) is repaired in any save.
- Saves, season parts and news parts accept years up to at least 2300 and news in more than 200 seasons.
- Caps go in a wider field, kept in the season part of the save. The original byte stops at 255 (Q25).
- The save keeps each type of data in one part, merged across seasons, so the part count stops growing. Older saves still load (Q26). A type is the name of a temporary file with each run of digits as '#', such as `fixtures_#.tmp`; a type with two files or more is one merged part, a type with one file stays a part of its own. The data files (`.dat`) are one part each already.

Original game faults, fixed in every game, base game included (Q23, Q24, Q27):
- Every national team opens a vacancy when its manager retires.
- The club info screen and the AI transfer budget use the full 64-bit balance.
- The human manager's age comes from the birth date.
- Every national team with no manager gets one at the first month change in play, also a team that had none at the start of the game. A nation that no longer exists in that year gets no manager (Q43).
- The top wage fix acts from a balance of about £1.65 billion, where the original's 32-bit sum already wraps (Q44).
- The human manager's age stops at 127, the largest age the game's byte holds, in place of the age from the birth date (Q45).
- Every money text that can pass £2,147,483,647 is traced and shows the full amount, news items included (Q46).
- History rows of editions after the start date are cleared, so each edition has one row; the game's own row replaces the data row of the same edition (Q48, 30 October 2026). A new base game then differs from the original in its history rows and in the first-season holders that the game reads from them (the 2001 Charity Shield: Manchester United, not Arsenal, against Liverpool).
- The same-save set under Q48 (slice 7, 31 October 2026): a loaded 2001 save differs only in nation_comp_history.dat and index.dat, the 3.9.68 row Copa América 2001 Colombia / Mexico, which goes when the game writes its own 2001 final (29 July). The new game differs in its history rows and all that follows the changed holders. The run rules-on differs from normal through one row only (a bisect of the 128 rows of 2002): the Asian Cup Winners' Cup 2002 Al Hilal. Without it the newest holder is Al Shabab (2001), and with changing rules on the season code makes the Asian Super Cup edition (season_kind_club_world.c), whose Cup Winners' Cup entrant is that holder. Accepted by the lead for Max (31 October 2026).
- Every final writes its host into its history row in a season game (slice 5, open item 18), also the finals whose class asks for no host (the Africa Cup of Nations, Asian Cup, Copa América, Gold Cup, Confederations Cup and OFC Nations Cup). An expected season-game difference: a new 2001 game with changing rules on plays the Copa América 2001 final (29 July) in its first days, so its row names Colombia; a row diff of the two new-game saves with and without slice 5 shows this one host field and nothing else (1 November 2026). The same-save set accepts host fields that were empty (smoke_run.py HOST_ROWS: rules-on against history-q48-off, rules-on-q48 against a reference build before slice 5).

The same game (Q152): every game code change keeps the same saves in the same-save set, unless the change acts only after a table end year, after 2099 or above £2.1 billion, which the set does not reach. The speed fixes give the same results as the code they replace (Q18).

Year limits:
- The season code has no year limit for rules, hooks and saves. The latest start year of Choose Season, the date jump limit and the rule lookups are separate values. Tests run to 2301 to prove there is no limit at 2300.
- One slice owns the format of the season part of the save. Slices 1, 12 and 13 change it in that order, each with its own part version.

Test switches (Q9, Q30):
- The Tier A switch and the date jump switch are in the normal build. The program reads them once at start, and both are off by default. The same-save set proves that they change nothing when off.
- A Tier A run keeps the calendar, draws, entrants, league tables, season changes, history, finances, retirements, new players, contracts, the filling of manager vacancies, and save and load. It skips the AI transfer market, background matches, new injuries and the sacking of managers (Q16, changed by Q31: without retirements and new players, ages wrap and the under-21 and Olympic squads empty). Each match gets an instant result from the two sides' reputation, so the stronger side wins more often (Q16). Tier A has no AI sales. So a Tier A step, which is not original code, acts when an AI club fills all 50 squad slots. The club lets its weakest players over 21 go until 37 remain (open item 78, approved 1 November 2026). Tier A has no AI signings either. So a second Tier A step lets an AI club of a played league with fewer than 15 players make its own shortlist and bids, and a Tier A year takes about a fifth longer (open item 92, approved 2 November 2026: a Tier A game must keep squads like the real game).
- Contracts stay (slice 3 build, approved 31 October 2026). With them skipped, a player stayed at his club after his contract ended and could not play, and the game stopped when a club could not field 11 (Aston Villa, August 2028, in an England game from 2025). The vacancy pass stays because retirements and season changes open vacancies, and only it fills them. Without it, more clubs and national teams would have no manager each year.
- Tier A counts as the same as the normal game over 10 years when they agree on editions, dates, hosts and entrant rules. Winners can differ (Q10). Two windows: one from a 2013 start, inside the tables, and one from a 2098 date jump, after the tables.
- Checks that depend on real results, such as the same finalists in a row, count only in Tier B.

The history checker:
- It reads a save, writes the history rows of every competition, and checks them against the expected rule of each competition (the competitions file in the tests).
- It finds a year held twice, a missing host, a winner from the wrong confederation, the same two finalists more than two editions in a row, a gap in a cycle, a missing edition, and a next draw date more than one cycle ahead.
- It lists every edition's entrants with their route in (league place, cup winner, title holder, host, qualifier) and checks them against that year's access rule (Q13).
- It checks nation names and confederations against the year.
- For Tier B it reports these each decade: squad sizes for each nation, manager counts, club balances, save size and time per game day. It fails on any overflow or empty squad (Q17).

The run set (Q11, Q14, Q15, Q22):
- Tier A: start seasons 2001, 2002, 2006, 2013, 2021, 2025 and 2026 (Q56) with changing rules on, and 2001 with changing rules off. All nations, three seeds each, to 2100.
- Tier A, also: a 2001 game with England, Spain and Brazil only, and one game where the manager runs a national team.
- One Tier A run to 2300 every night.
- Date jumps to 2098, 2198 and 2298.
- Tier B: a few long games in the background, with checkpoint saves each decade. It fails when the save would pass the 4 GB file limit before 2300, when a game day in a later decade takes more than twice as long as in the first seasons, or when a national team has no manager for more than one season.
- 0.6.2 ships only when the checker finds no fault to 2300 in the whole run set. The run set joins the release checks of every later release (Q12).

The 2026/27 tile (Max, 1 November 2026, Q51 to Q56):
- Q51: the option defaults stay 3.9.60 and 2001/02 (the 0.6.1 choice), so a new install still starts on 2001/02. Only the Custom tile's default year moves from 2025 to 2026.
- Q52: 2025/26 keeps December 2025. Q53: October 2026 ships in the download like the other Update Team databases.
- Q54: every 2026/27 rules row is checked against a source and against the database's divisions, and corrected where it differs.
- Q55: a 2026/27 game starts after the 2026 World Cup, and its history holds the real result.
- Q56: the run set adds a 2026 start with three seeds.

## Testing

Seams, approved by Max:
1. The history checker on a save. This is the main seam. A fix is proved by a Tier A run or a date jump past its year, then the checker on the save. The test fails on main and passes on the fix branch.
2. Unit tests to 2300 of the calendar code that reads no game memory, in ctest (the future tests of slice 0, 462bc8a9). A known-fault list keeps ctest green, and each fix removes its faults from the list.
3. The same-save set for every game code change (skill same-save-check).

Similar tests that exist: the start-season soaks, the long games, full play and the gate runs (docs/TESTING.md), the history duplicates check, and the rules report checks.

## Out of scope

- Real competitions after the announced ones. We do not invent formats, such as a new Club World Cup shape or a new Nations League. The last cycle continues.
- New nations after the data, and nations that split or join after 2026.
- Weekday and date display formats, apart from the faults above.
- Online play (0.7.0).

## Open decisions

None. Two decisions from the review were settled by Max on 30 October 2026 (Q31, Q32), see Decisions.

## Slices

Each slice is one fresh session in its own worktree, with two fresh Opus review agents on its diff. No merge or push without Max's yes. The merge of the history checker branch (claude/future-2-history) comes before slice 2. Every check "to 2300" also needs slices 1 and 13, because the save fails at 999 parts in about 2232.

First wave, in parallel: 1, 2, 11, 14 and 17. Slices 12 and 13 follow slice 1 in that order, because all three change the season part of the save.

### 1. Remove the year limits, and add the date jump switch
Blocked by: none
Delivers: season hooks, cup rules, cup groups, new cups, saves, season parts and news parts work after 2099 with no upper year limit, and with more than 200 news seasons. The latest start year, the date jump limit and the rule lookups are separate values. New games start up to 2300 under the date jump switch. Owner of the season part format for this release. Faults 6, 10, 23, 24, 25, 31 and 35.
Merge danger: two-way door. Blast radius: every season game after 2099, and the load of season parts.
- [x] A date jump to 2098 plays past July 2101, with a save and load after the 2100/01 season change, and no box.
- [x] A date jump to 2298 plays through the 2300/01 season change, with a save and load, and no box.
- [x] Unit tests to 2301 show no rule falls back to the start year and no hook stops.
- [x] A 0.6.1 save loads.
- [x] Same-save set PASS.

### 2. History checker: entrants, nation names and world health
Blocked by: none (after the merge of the checker branch)
Delivers: the checker lists entrants with their route in and checks them against the year's access rule. It checks nation names and confederations by year, counts the under-21 and under-23 players of each nation, and writes the Tier B world-health report.
Merge danger: two-way door, tests only. Blast radius: false passes or false faults in the checker.
- [x] On existing saves, the checker finds Yugoslavia after 2006 in a 2001 game, and every fault the forum player reported.
- [x] A fixture save with a wrong entrant fails the entrant check.
- [x] The world-health report reads squad sizes, under-21 and under-23 counts, managers, balances and save size from a save.

### 3. Tier A switch
Blocked by: 1, 2
Delivers: a test switch that skips the work in the decisions above and gives instant results from reputation. It measures the time per year.
Merge danger: two-way door. Off by default. Blast radius: none for players if the same-save set passes.
- [x] 10 years from a 2013 start and 10 years from a 2098 date jump, each in Tier A and in the normal game, agree on editions, dates, hosts and entrant rules.
- [ ] Each nation keeps eligible under-21 and under-23 players in 2050 and 2100 of a Tier A run.
- [x] Same-save set PASS with the switch off.
- [x] The time per year is measured and reported.

### 4a. International finals continue
Blocked by: 1, 2, 3
Delivers: every international final and its qualifiers continue on the last cycle. The 48-team qualifiers fill 48 places. The Confederations Cup stays ended. A start after 2200 has no final in the past. Faults 1, 13, 15, 32 and 34.
Merge danger: two-way door. Blast radius: every international competition.
- [ ] A Tier A run to 2100 gives every final on its cycle, and the checker finds no gap.
- [x] Unit tests: no class returns the idle year.

### 4b. Repair of saves stuck at the idle year
Blocked by: 4a
Delivers: a save with a competition waiting for the idle year moves it to its next edition at load (Q8).
Merge danger: one-way door, because the repair changes old saves at load. Blast radius: every international competition in loaded saves.
- [x] Fixture: a 2098 date jump made on the 0.6.1 build (its finals wait for 2200). After a load in 0.6.2, it plays the next World Cup, Euros and Olympics.

### 5. Hosts
Blocked by: 4a
Delivers: hosts from the announced editions, then from the original host rule, with the rotation of Q32 where the original rule does not fit. Every final writes its host into its history row. Euro 2020 and 2028 and the Gold Cup after 2031 have hosts. Faults 3 (hosts), 12, 14 and 18.
Merge danger: two-way door. Blast radius: hosts and automatic host places.
- [ ] The checker finds no final without a host to 2300.
- [x] A host drawn as a nation that no longer exists is drawn again (unit test), including the case where all pairs name such nations.
- [x] The host of each announced edition matches its source.

### 6. Finals that stop before their table ends
Blocked by: 2
Delivers: the Copa América, Africa Cup of Nations and Asian Cup go on in a 2001 game. Faults 2 and 19. The cause is unknown, so the first session finds the cause and says whether the fix needs a second session.
Merge danger: two-way door. Blast radius: the classes that the cause touches.
- [ ] Long game seed 6 (2001 start) to 2030: the checker finds every edition of the three finals.

### 7. History rows
Blocked by: 2
Delivers: one row per year at the start, and history years in their right places for every database. Faults 3 (duplicates), 17 and 20.
Merge danger: two-way door. Blast radius: the history screens of every start.
- [ ] The checker finds no year held twice in the starts of the run set.
- [x] The 2023 and 2010 starts show the World Cup in its real years.

### 8. The Nations League and the Asian Cup qualifiers
Blocked by: 2, 3
Delivers: the UEFA Nations League has its own European record. The Asian Cup qualifiers hold only Asian nations. Saves with the wrong record are repaired at load. Faults 4 and 21.
Merge danger: one-way door, because the repair changes saves at load. Blast radius: both competitions and their history.
- [ ] The checker finds no nation from the wrong confederation in either competition to 2100. (Slice 8: none in a game from 2018 to April 2030, before slice 3. A game to 2100 waits for the Tier A switch. A 2098 date jump makes no Nations League, a separate fault.)
- [x] A 0.6.1 save with the fault loads and plays both competitions correctly. (Run "repair" of tests/seasons/season_nl_run.py. No code plays the Asian Cup qualifiers: their record is the Nations League's.)

### 9a. Club cups after the table
Blocked by: 1, 2, 3
Delivers: the Copa Sudamericana, Recopa, FIFA Intercontinental Cup and AFC Champions League Two continue every year. The Club World Cup is played every four years from 2029. No club cup stops in 2101. The 2027 Libertadores row gets its places text. Faults 7 and 11.
Merge danger: two-way door. Blast radius: continental and world club cups.
- [ ] Tier A to 2100: the checker finds every edition with the right entrants. Not run: slice 3 is not on main. Normal game in its place: a 2098 date jump to August 2101 gives every edition of the Sudamericana, Recopa, FIFA Intercontinental Cup and ACL Two from 2098 to 2101 and the Club World Cup of 2101, with no club cup entrant fault (run-logs/0.6.2-s9a/j2098).

### 9b. Suruga Bank finalists and the Super Cup venue
Blocked by: 2
Delivers: the Suruga Bank finalists are the year's J.League Cup and Copa Sudamericana winners to 2019. The UEFA Super Cup venue changes each year after 2027. Faults 5 and 22, Q28.
Merge danger: two-way door. Blast radius: these two competitions.
- [x] Long game to 2019: each Suruga Bank final has that year's two winners (checked in Tier B or a normal game, because it depends on results).
- [x] Tier A to 2040: no two Super Cups in a row in one venue.

### 10. Leagues and domestic cups after the table
Blocked by: 1, 2, 3
Delivers: leagues, play-offs and Brazilian state leagues keep their last rules with no end year. Faults 8 and 9.
Merge danger: two-way door. Blast radius: promotion and relegation in every league.
- [x] Tier A to 2100: league sizes, promotion and play-off places stay as in the last table row. Run as a date jump from 2098 to 2100 (slice 3 not on main then): every division keeps the places and sizes of 2027/28 in a 2025 start, Série A and B too after the fix of open item 63.
- [x] Unit tests to 2301: no lookback limit ends a rule.

### 11. Original faults: national managers, money, manager age
Blocked by: none
Delivers: every national team gets a new manager, money above £2.1 billion is right on every screen and in AI budgets, and the manager's age is right after 2100. Faults 26, 27, 28 and 33.
Merge danger: two-way door. The base game changes at the first month change in play (1 August in a 2001 game): about 95 national teams of the 3.9.68 data have no manager from the start and get one then (Q43). The same-save set of a new game, start.sav, prematch.sav and the match does not reach that point, so it must PASS. The 14 days of yeovil.sav (20 August 2002) pass 1 September: there 78 national teams get a vacancy, an accepted change (Q43), and the run original-faults-keep of yeovil.sav keeps only the money and age faults (item 72). A loaded save with a national team that has no manager gets one at the next month change. That is an expected change. Blast radius: national team jobs, money screens and AI transfer budgets.
- [x] A save with a retired national manager of a small nation opens a vacancy. A save where a national team already has no manager fills it at the next month change.
- [x] Saves edited to balances of £2.5 billion, £5 billion and £12 billion show the right balance, and the club still bids.
- [x] Every screen that formats money through the 32-bit Cash object is traced and fixed.
- [x] Same-save set PASS.

### 12. Caps in a wider field
Blocked by: 1
Delivers: caps count above 255. Fault 30.
Merge danger: one-way door, because the save holds a new field. Blast radius: caps and first-cap history rows.
- [x] A player edited to 255 caps gets 256 at the next cap, and no second "first cap" row.
- [x] A 0.6.1 save loads with the right caps.

### 13. Save parts merged across seasons
Blocked by: 12
Delivers: the part count of a save stops growing. Fault 29.
Merge danger: one-way door, because the save format changes. Blast radius: every save and load. Old saves must load. Every save made after this slice has merged parts (a new game keeps fixtures_2001.tmp and fixtures_2002.tmp from its first day), and 0.6.0, 0.6.1 and earlier 0.6.2 builds cannot load it: their load stops at the box "Cup..cpp 5246". The changelog of 0.6.2 must say so.
- [x] A save with 990 parts saves again and loads.
- [x] 0.6.0 and 0.6.1 saves load.
- [x] Same-save set PASS.

### 14. Speed fixes
Blocked by: none
Delivers: the new-player sort, the transfer shortlist search, the retirement search and the FIFA day steps run faster with the same results. Fault 16.
Merge danger: two-way door. Blast radius: every game, if a result changes.
- [x] Same-save set PASS.
- [x] Time per game day of the second year drops, measured against the profile of 30 October 2026.

### 15. Run set and release check
Blocked by: 1, 2, 3, 13
Delivers: one script runs the whole run set in parallel within the game limit, keeps checkpoint saves, runs the checker on each save, applies the Tier B limits, and joins the release script.
Merge danger: two-way door, tests only. Blast radius: release checks.
- [x] The script runs the run set and writes one pass or fail per game.
- [x] The release script stops on any checker fault.

### 16. Full run set, every finding fixed, release 0.6.2
Blocked by: all
Delivers: the run set passes to 2300 with no fault, and 0.6.2 is released.
Merge danger: the release itself. Max's yes is needed for each push and for the publish.
- [ ] The whole run set passes.
- [ ] Release skill steps done, with Max's yes.

### 17. Date span audit
Blocked by: none
Delivers: a check of the callers of the original's day-span function that the audit did not read (201 of 215). A 16-bit span wraps after about 60 years, so a span from an old date can be wrong from about 2061. Each caller gets a verdict, and each fault goes to the open items with a fix slice.
Merge danger: two-way door, read only.
- [ ] Every caller has a verdict with its address.

### 18. Save size growth (only if Tier B confirms it)
Blocked by: 15
Delivers: if a Select All save grows about 25 MB a season, it reaches the 4 GB file limit in about 2160. Tier B measures the growth. If the growth holds, this slice finds what grows and keeps the save under the limit to 2300.
Merge danger: one-way door if it changes the save format. Blast radius: every save.
- [ ] Tier B save size stays under 4 GB to 2300 at the measured rate.

### 19. Nation names, Kazakhstan and the Netherlands Antilles
Blocked by: none
Delivers: the national sides of a renamed nation take the nation's name, in a new game, at each season point and at the load of an older save. Kazakhstan moves from the AFC to UEFA from the 2002/03 season. The dead Netherlands Antilles plays no competition beside Curaçao (fault 51, already fixed on main). Every UEFA member plays the World Cup qualifying of 2006 to 2022, apart from the hosts (fault 57). A qualifier that moves to a FIFA day keeps the other matchday of its window 3 days away and no club tie of its players within a day (fault 93). Faults 42, 43, 51, 57 and 93.
Merge danger: two-way door. The load of a 0.6.1 save gives its old side names the nation's names, and that changes only names. Blast radius: the names that the fixtures, results and history screens show for six national sides, and the international competitions of 2001 and 2002 starts, where Kazakhstan now plays the UEFA qualifiers and no longer the AFC ones, and the European World Cup qualifying of 2006 to 2022, whose groups now hold 5 or 6 nations.
- [x] The checker finds no side-name and no nation-confed fault in a 2001 start with changing rules on, played past the 2002 season point, and in a 2025 start on the December 2025 data.
- [x] The checker finds no entrant-dead fault in the CONCACAF Nations League of a 2022 start played to November 2023. Fault 51 is not on main: only saves of builds before 29b9dc82 show it, so this slice changes no code for it.
- [x] history_check_test.py: the UEFA moves of nations.tsv match the season code.
- [x] Same-save set PASS.
- [x] Fault 57: a 2001 game has 51 European nations in the 2006 qualifying, a 2013 start 52 in 2018 (all of its data less Russia, the host) and a 2017 start 55 in 2022. The checker finds no entrant-missing fault there. history_check_test.py tests the rule.
- [x] Fault 57: same-save set against main 99bf45fb: start, prematch, new game, yeovil and match equal main (ref-build). Yeovil fails only original-faults-keep, as main does (open item 72).
- [x] Fault 93: the two qualifiers of a FIFA window lie 3 days apart or more, inside the window, with no club tie of their players within a day. Run set game a2013s3 played again from its July 2023 save: main stops with the box match_eng.cpp 612 on 22 November 2029 (West Bromwich Albion fields 9); the fix plays the 2030 UEFA qualifying on 22/25 March, 7/10 June, 27/30 September, 6/9 October and 15/18 November 2029 and its leg to 2034 with no box. The 2098 date jump (normal game) holds the 2102 UEFA qualifying in its windows. The checker's duty-clash finds main's clashes of 22 November 2029 and none on the fix; history_check_test.py plants one and three cases that are not faults.
- [x] Fault 93: same-save set against main 1901ffa5: start, prematch, new game and yeovil PASS; the match check PASS for normal, again, host-loops-off and ref-build (its first pass lost host-loops-off to a wait timeout under load, so those runs ran again at a load under 16).

### 20. A youth pool for every nation
Blocked by: none
Delivers: every nation with 23 players or more keeps players under 21 and under 23 (Q35). Open item 52.
- The cause, read in code: a new player (regenerate_as_youth_player 0x7abac0) takes the nation of a retired player. Some nations are not loaded and had 349 players or fewer at the start (+0x88, counted once by 0x524d70). Their new players are 23 to 27 in most cases (0x7abe87..0x7abeee). Their young players grow up and none follow.
- The fix, in every game: a short nation gets the young ages of the original. Short means fewer than 1 in 20 players under 21, or fewer than 1 in 10 under 23. A nation that will have none under 21 or none under 23 within a year gets the next new player of a nation that is not short.
Merge danger: two-way door. The base game changes from the first new player of a nation that is short or runs out. In an England-only 2001 game (start.sav) that is 24 March 2002, a new player for Croatia. In the 2025 start of the gate runs it is the first new player after the July 2027 save. No nation is short or runs out at the start of a 2001 game, so the same-save set must PASS. Blast radius: the age and nation of new players.
- [x] Unit test: the rule cases, and a 300-year model where no nation runs out.
- [x] The 2041 forum save played one year: main has 27 no-youth faults, the fix none.
- [x] An England-only 2001 game to July 2009: main has 15 no-youth faults, the fix none. A 2025 start from July 2027 to May 2028: main has 21, the fix none.
- [x] Same-save set PASS against main.

### 21. A 2026/27 tile with the October 2026 database
Blocked by: none
Delivers: a 2026/27 tile on Choose Season with the Update Team's October 2026 database (posted on 31 October 2026), bundled in the download like the other season databases (Q51 to Q56, Max, 1 November 2026). 2025/26 keeps December 2025 (Q52). The 2026/27 rules rows are checked against a source and against the new database's divisions, and corrected where they differ (Q54). A 2026/27 game starts after the 2026 World Cup, and its history holds the real result: Spain, Argentina, England, hosts Canada, Mexico and United States (Q55). The run set gets a 2026 start with three seeds (Q56).
- The changes follow the 2025/26 tile: the signature row (make_db_table.py), the measured date shifts (measure_date_shifts.py), the history fill row (history_fill.py, with the 2026 World Cup as a researched row), 26 seasons in place of 25, the screen's labels, the option's values, the release zips and the tests that name the last season.
Merge danger: two-way door. Only a game that starts in 2026/27 takes the new code paths. The 2001 game and the other 25 tiles keep their tables: the new rows are added, and no row of another database changes. The rules rows that Q54 corrects also change games of earlier seasons that reach 2026/27 with changing rules on. Blast radius: the Choose Season screen, the season option, the release zips, and the 2026/27 rules of every league.
- [x] A 2026/27 new game with changing rules on and with changing rules off starts with no box and plays one season (England, 11 July 2026 to 1 June 2027; and the full world with changing rules on to 28 July 2027).
- [x] The rules report of the 2026/27 start matches its expected file (2026-england, changing-2026-england to 2027/28, changing-2026-all-start). Every line is a kind that the 2025/26 start of main also has.
- [x] The history of the 2026/27 start holds the 2026 World Cup (Spain, Argentina, England), and the game plays no 2026 finals (its World Cup waits for 2030 from the first day).
- [x] The history checker finds no new fault in the 2026/27 game (run set row quick2026): no fault at all. The October 2026 database also lacks the Africa Cup of Nations 2025 row, which the game now adds (Morocco, the CAF Appeal Board result). The three Argentine titles of 2025 in the database are no fault (the AFA's Campeón de Liga).
- [x] Every 2026/27 rules row is checked against a cited source (with the date read) and the database's divisions, and corrected where it differs. A row that no source confirms keeps its value and says so in its notes.
- [x] The run set holds a 2026 start with three seeds.
- [x] Same-save set PASS for the 2001 game against main 962d9986: start, prematch, new game and match (the match check at load 6.8).
- [x] ctest passes (70 of 70).
