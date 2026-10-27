# The Nations Leagues and the Euro qualifying paths (0.6.0 slice 28)

Date 2026-10-18. Original: `clean-3968/cm0102.exe` 3.9.68, virtual addresses. Spec: `docs/specs/2026-10-16-seasons-0.6.0-spec.md`, slice 28, Q169, Q248, Q277, Q278, Q288, Q368. Build brief: the build brief (kept outside the repo), slice 28.

Research of this slice:

- `2026-10-18-nations-league-class.md` (named "class doc"): the class `euro_champ_qual` as the base of a Nations League.
- `2026-10-18-national-team-comps.md` ("nations doc"): how the game treats a competition of national teams, the free records.
- `2026-10-18-gold-cup-class.md` ("gold cup doc"): the class `copa_america` for a Gold Cup, and the class for a Finalissima.
- `rules-0.6.0/nations-league-tiers.tsv`: the real league and group of each nation in each edition (two sources a row; both Wikipedia pages, the edition page and the league page).
- `rules-0.6.0/nations-league-dates.tsv`: the draw, matchday, finals and play-off dates of each UEFA edition.

Code: `src/recomp_rt/win/season_nl.h`, `season_nl_rules.c` (the rules, no game memory), `season_nl_table.c` (made by `tools/seasons/make_nl.py`), `season_nl.c` (the game). Tests: `tests/seasons/test_season_nl.c` (ctest `season_nl`), ctest `season_nl_table`, `tests/seasons/season_nl_run.py`.

## The rules (no game memory)

- **Editions.** `make_nl.py` holds one line per edition with the sizes, group counts and moves that the row's text gives, and checks them against the row (season, the total of the clubs column, each size in the format). The 2022-23 editions start on 1 and 2 June 2022, so their season is 2021 (the season of the first match, Q276).
- **Overall ranking.** By league, then group place, then points, goal difference and goals, then nation id (UEFA regulations, overall ranking). Inference: the results against the fourth of a group of four are not left out, because the game's table has no such column. The game reads the points of the table (row +0x19, the overall table of the comparator 0x683466) and leaves goal difference and goals at 0 (their row fields are not read).
- **Leagues of the next edition.** The group winners go up, the last of each group goes down where the edition says so, and the lowest of the ranking where a play-out decides (2 of League C in 2020-21, 1 in 2022-23, 2 in 2024-25; the game plays no play-out). Then each league fills to the size of the next edition from the ranking. The test gives the real 2020-21 leagues from the 2018-19 places.
- **Play-off paths.** Each path takes the group winners of its league that did not qualify, then the other nations of its league by the ranking; the places that a league cannot fill go to the best free nations of the ranking, path A first. Semi-finals 1 v 4 and 2 v 3. The test gives the four real Euro 2020 paths and their semi-finals from the 2018-19 ranking.

## The game

See the head of `season_nl.c` for the design: one `euro_champ_qual` object on the record "Asian Cup Qualifying" in entry 1; the League object is group A1 and the stages are the other groups; the last stage is the finals of four.

### Code sites

| Site | What | Evidence |
|---|---|---|
| wrap 0x57f600 | collect_entrants: nothing for the Nations League | class doc 2, called at 0x57fe35 |
| wrap 0x57dab0 | draw_groups: the groups of the edition into the grid +0xf7 | grid read at 0x57edbe..0x57eddb and 0x57e8a3 |
| wrap 0x57d9d0 | get_group_team_count: the size of each drawn group | called at 0x57e7eb, 0x57ed62 |
| wrap 0x57f920, 0x57fa10 | no runners-up stage or table; +0x30 = the last group | 0x57ff50, 0x57ff9c |
| INSNS 0x57ff34 | `cmp edi, 8`: G - 1 group stages | 0x57ff28..0x57ff37 |
| wrap 0x669ae0 | the record is international (fixture type 1) | nations doc 1, point 4 |
| vtable copy slot 2 | start_new_season: keep the tables, move to the next edition | the pass calls +0x8 at 0x5d5d1c |
| slot 5 | update_for_date: the draw of a new edition, then 0x57fd60 | 0x57fd60 draws only on its draw day (0x57fe0e..0x57fe2d) |
| slot 10 | advance_stage: the finals of four | base update calls +0x28 at 0x6849e0 |
| slot 12 | history from the finals stage, then 0x90fbe0 | Euro thunk 0x40f590; Cup slot 12 0x519a90 |
| slot 15 | round templates: six matchdays, the finals | 0x57d190, league_round_set_date 0x68a160 |
| slots 14, 17, 18, 23, 26, 28, 29, 30, 37 | the League base (0x96d020): no Euro qualification, news, predictions or status 7 | class doc 10 |

### Moving to the next edition

The season pass starts a competition only when `all_stages_complete` (vtable +0xc) is true and vtable +0x2c gives 0 (0x5d5cab..0x5d5d1c). The class's +0x2c is `skip_season_reset` 0x57eb10, which gives 1, so the copy takes the League base 0x48ce70 (gives 0). Entry 1 changes its season only every second year (year step 2, 0x830fb6), so a game that starts in an odd year would meet its update day one year late. So the object's own update moves it to the next edition on the day after its finals end: when `all_stages_complete` is true, it writes the next edition year at +0x40 and the draw day. The flag +0xdd alone is not the end: League::check_complete sets it once the League object's own table is done (0x689bea), before the other stages.

### Cup rounds

A cup round record keeps the day of the year and a year offset from the year argument of `cup_set_round_draw_date` 0x521e60 and `cup_set_round_match_date` 0x521eb0 (offset = the date's year minus that argument, 0x521ea3; stored at +0x02 and +0x05). The Cup_Stage counts the offset from its own year (argument 7). So the finals pass the edition year and an offset of 1, as the class's play-off passes +0x40 and 1 (0x57d494..0x57d4b6).

### The save

No new field. The object saves and loads as every League object (League::save 0x6891c0 writes the stage count +0x2c and the stages up to +0x30 at 0x6898dc..0x6899f1; the constructor returns after a load at 0x57cf28). The module makes the object again at a load from its file (season_comps.c). After an edition ends, start_new_season keeps its tables, so the leagues of the next edition come from them at the next draw, also after a save and a load.

## Euro qualifying

`season_nl_euro.c`. Euro 2020 and Euro 2024 qualifying played their groups from March to November of the year after the campaign year (int-continental-189, -233), because the Nations League took the autumn before. The class plays them from September of the campaign year (template 0x57d190, class doc 4), which would meet the Nations League matchdays. Slice 27 gives these two campaigns ten groups and the top two of each (`season_intl_euroq.c`); this slice owns their dates and the play-off.

- **Matchdays.** The wrap of 0x57d190 gives the group stages of the two campaigns the ten real matchdays (`nations-league-dates.tsv`, events `euro2020_qual_md1..10` and `euro2024_qual_md1..10`, each checked on Wikipedia and a UEFA page), in a season game, for the original object only (the Nations League copy has its own slot).
- **Paths.** The wrap of `create_playoff_stage` 0x57ef50 for the two campaigns builds the paths in place of the class's two-leg ties: four paths (2020) or three (2024) of four nations that did not qualify (the top two of each group and the teams already in the Euro's slots, the 2024 host), by the ranking of the Nations League edition two years before the finals (season_nl_paths; the object still holds those tables in November of the year before the finals). One Cup_Stage of two rounds, semi-finals 1 v 4 and 2 v 3 at the better nation's ground, then the path final, single matches with extra time and penalties, on the real days (26 and 31 March 2020; 21 and 26 March 2024). Link mode 1 pairs the list two by two (0x524a20) and bit 0x200 of the final's flags links ties 2k and 2k + 1 into final tie k (0x5245c3). Only a final winner gets status 1 (0x51af95) and so `add_qualified_team` through the class's slot 17 (0x57d6a0), so the path winners take exactly the places that slice 27's fill held for them.
- **No Nations League tables** (a game that started after the edition): the paths take the best of the nations that did not qualify, by group place and points.
- **The Euro draw.** The Euro's seeding (+0x13e) and draw (+0xb6) days move to two and four days after the path finals, when the paths are built. Both are saved (0x57c5e9, 0x689411; class doc 12.2).
- The real path rules of 2024 also used a draw for some places; the rule here is the ranking (an inference that matches the real Euro 2020 paths, test `season_nl`).
- **Euro 2028** (O320): the real matchdays of 2027, two places for the best-ranked hosts that do not qualify, and a play-off of 2 or 3 paths of 4 or 4 two-leg ties by the host places in use. Sources and decisions: `2026-10-23-euro-2028-qualifying.md`.

## Decisions for Max to confirm

1. **Records.** The UEFA Nations League takes the record "Asian Cup Qualifying" (record 444 in 3.9.68 and in every Update Team database checked), the only nation competition record that no code reads and that has no history (nations doc 5.5). The record gets the name of the table through the module. The CONCACAF Nations League, the Gold Cup and the Finalissima need new records, which the shared module cannot make yet (season_comps open point 1): not built in this slice.
2. **Not a tournament.** The Nations League is not in the tournament lists of `national_teams` (0x76d940 and the eight inline lists): a tournament holds its squad from 10 days before the first match until the team goes out (0x76c31b), which would keep players from their clubs from September to November (nations doc 3.4). It counts as an international competition (0x669ae0, ranking weight 1.1 as the Euro qualifiers).
3. **Play-outs and promotion play-offs** (League C play-outs 2022 and 2024; the A/B and B/C play-offs of 2025 and 2027; the 2024-25 and 2026-27 quarter-finals) are not played. The lowest of the ranking go down in place of the play-out losers; the play-off and quarter-final places keep their league and the finals take the four League A group winners.
4. **The finals.** Semi-finals A1 v A4 and A2 v A3 (no draw), and no third place match.
5. **The 2020-21 finals** play in June 2021 (2 and 6 June), the planned time before Euro 2020 moved to 2021 (Q277).
6. **A game that starts after an edition's league phase** (for example 2021/22, before the 2020-21 finals) starts with the next edition. The finals of the edition before are not played.
7. **Russia** plays to 2020-21 and is left out from 2022-23, as it was in real life (Q293, Q365).
8. **The Euro draw after the play-offs.** The Euro finals of 2020 and 2024 draw two days after the path finals (early April), not in November or December of the year before as in real life: the game's draw cannot hold places for teams that are not known yet.
9. **Paths without Nations League tables** take the best non-qualified nations of the qualifying groups.
10. **After the table (2028-29 on)** the 2026-27 edition is played again every two years (Q248: the rules stay). With changing rules off the edition of the start season is played again every two years (Q232).

## Part b: the CONCACAF Nations League, the Gold Cup and the Finalissima (Q368)

Code: `season_nl.c` (one object per confederation), `season_gold_cup.c` (the Gold Cup and the Finalissima), small blocks in slice 27's `season_intl.c` (the date shift of the stage builder) and `season_intl_copa.c` (the 16-team test, the stage array, the entrants), and the year sites 0x5e06fb and 0x5e1590 in `tools/recomp/seasons.py`, which now call `rt_copa_first_year` and `rt_copa_next_year`. The dates of the CONCACAF editions, the Gold Cups and the Finalissima are in `nations-league-dates.tsv` (editions `concacaf-<edition>`, `goldcup-<year>`, `finalissima-2022`; rows with one source say so in the note).

### The records

Each of the three takes a new record of the shared module (`like`, slice 26): "CONCACAF Nations League" and "CONCACAF Gold Cup" copy the continent, nation and colour ids of "World Cup CONCACAF Qualifying Section"; "Finalissima" copies those of "Confederations Cup". Levels (record +0x69, compared by the officials code at 0x79d447): 14, 17 and 18 (inference: below the World Cup's 20). The wrap of 0x669ae0 gives 1 for the four records of this slice (fixture type 1, the FIFA ranking).

### The CONCACAF Nations League

A second `euro_champ_qual` object with its own vtable copy and the same slots as the UEFA one. The functions of `season_nl.c` take the confederation from the object (`conf_of`). The real leagues of each edition come from `nations-league-tiers.tsv`; a real group that keeps fewer than two nations of the data (Bonaire, Sint Maarten, Saint-Martin, Guadeloupe, French Guiana and Martinique are not FIFA members and some databases lack them) is drawn by pots. The 2019-20 qualifying round is not played: a game that starts in 2018/19 starts with the real 2019-20 leagues.

### The Gold Cup

An object of the class `copa_america` (gold cup doc) on its own record, with a vtable copy that changes no slot (it keeps the object out of slice 27's vtable tests of the Copa America and names it in the rules report). Every Copa America fact of the class that is a direct call or an inline instruction tests the object by its record ([this + 4], written at 0x5e06b9 before every site of the ctor):

| Site | What | Evidence |
|---|---|---|
| 0x5e06fb, 0x5e1590 (seasons.py) | the year: the Gold Cup keeps the odd year that `make` gives (the ctor rounds to an odd year, 0x5e069a..0x5e06f9) and steps to its next edition | gold cup doc 3 |
| stage builder 0x5e12d0, template 0x5e0900 (season_intl.c) | every date moves by the days between 6 July (0x5e09a5) and the edition's first day | gold cup doc 5 |
| copa16 (season_intl_copa.c) | 16 teams from 2019: the shape and bracket of the 2024 Copa America | gold cup doc 5 |
| rt_intl_copa_stages, site 0x5e1595 | the stage array of an edition of another size | 0x5e1595 |
| entrants wrap 0x5e1cd0 | the 12 or 16 strongest CONCACAF national teams | gold cup doc 4 |
| wrap 0x76d940 | a finals competition (squads of 23, no fixture clash) | 0x76d940..0x76d99e |

### The Finalissima

An object of the class `Eur_Super_Cup` (ctor 0x57a100) on its own record, made in season 2021/22 for 1 June 2022. The record is written at 0x57a13d, before the picker (0x57a18e) and the round table (0x57a1d6), so both wraps test it there. The picker 0x57a500 takes the winners (place 0 of 0x4afd80) of the records of the Euro (key 0x9cf7a4) and the Copa America (key 0x9cf7a8). The round table 0x57a3c0 gives draw 1 May, match 1 June, rule flags 0x1 and venue flags 0. Slot 2 of the copy runs the season change hooks and makes no new edition (the class's 0x57a750 steps the year and picks again).

### Decisions of part b (recommended answers taken, for Max to confirm)

11. **CONCACAF League A from 2023-24** plays 4 groups of 4, home and away, like the UEFA leagues; its group winners play the finals of four. The class holds no groups of 6 with 4 matches each, no byes and no quarter-finals. The last of each A group goes down (4, as the real 5th and 6th of two groups of 6). Its matchdays 5 and 6 are those of League B.
12. **The 2019-20 CONCACAF finals** play on the planned days, 4 and 7 June 2020 (Q277). **The 2026-27 finals**: only the window 25 to 28 March 2027 is known, so the semi-finals play on 25 March and the final on 28 March.
13. **Gold Cup entrants** are the 12 or 16 CONCACAF national teams with the highest reputation. No host places (the hosts play their matches at home, decision 19), no qualifying through the Nations League, no guests.
14. **The Gold Cup of 16** (2019 on) takes the bracket of the 2024 Copa America (A1-B2, B1-A2, C1-D2, D1-C2). The Gold Cup keeps the class's third place match (only 2015 had one).
15. **The Gold Cups of 2002 and 2003** (4 groups of 3) are not played: the first Gold Cup of a game is 2005 or later.
16. **The Finalissima** plays only in 2022, with changing rules on or off (2026 was cancelled). It plays at the first team's ground (the Euro winner), not at Wembley: the class has no neutral ground by record. A winner missing from the history (a season database) is replaced by the strongest nation of its continent.
17. **The Confederations Cup** keeps its CONCACAF place (the original's random pick, 0x58cd7c), not the Gold Cup winner.
18. **Euro 2028** (changed by O321): the real matchdays, the two host places and the Nations League play-off, as section "Euro 2028 qualifying" says. The first decision gave it slice 27's fill.
19. **The Gold Cup's hosts**: the first two hosts of each edition from the table; the unknown 2027 host is the United States.
20. **Gold Cup with changing rules off** follows Q398: only when the start season holds a Gold Cup.

### The Euro's team slots after a load

The EuroChamp constructor builds the team slots (+0x14, 6 bytes a slot; malloc 0x57a958, build_team_slots 0x57bd80) only for a new edition: after a load (`League::load` 0x687b10 gives non-zero) it jumps past them (`jne 0x57aa87` at 0x57a943). `load_extra_data` 0x57c480 reads only the pots (+0xee) and the seeding day (+0x13e, +0x140). So after a load +0x14 is 0 while the count +0x36 and the room +0x38 come from the file, and `add_qualified_team` 0x57c120 writes each qualifier at address 0 with no test (0x57c1aa..0x57c1bc). In the 2018 save run (Euro 2020, measured with the test log: slots "at 0x0") other writes overwrote them before the seeding: box "Database..cpp 13126" (national_team 0x53b3d0 with a null nation, from 0x53b4ac) on 1 April 2020, in the loaded game only.

A review game (2003 start, saved in September 2003 during the Euro 2004 qualifying, loaded) seeded Euro 2004 with no box: the fill of slice 27 found its slots. So that Euro had a slot array after the load. Why it had one is not known (inference: its constructor did not load its file and built a new edition). The fault does not need the Nations League: any Euro whose file loads has no slot array.

The World Cup builds its slots before its load (+0x38 = 32 at 0x92bffa, +0x14 at 0x92c00a, load at 0x92c040), so it keeps them.

The repair (`season_euro_slots_after_load`, season_nl_euro.c) runs in every season game from slice 27's Euro slot code (season_intl.c): before `add_qualified_team` 0x57c120 and before the fill at the seeding 0x57c710 and at the draw 0x57c1d0. A Euro with no slot array gets one of its room, with the entries already written at address 0 that hold a national team. The base game keeps the original's code (Q152).

### Euro 2028 qualifying

`season_nl_qualifying` holds Euro 2020, Euro 2024, the 2022 World Cup and Euro 2028 (O321). The first decision for Euro 2028 was slice 27's fill for the places that its groups do not fill, because its format was only in outline. Under O151 and O259 a fill must not act in normal play, so Euro 2028 now has the announced format: the groups on the ten matchdays from March to November 2027, two places for the two best-ranked hosts (England, Republic of Ireland, Scotland, Wales) that do not qualify from the groups, and a play-off in March 2028 from the 2026-27 Nations League: 2 paths of 4, 3 paths of 4 or 4 two-leg ties, by the host places in use. The sources (with URLs and read dates), the decisions and the inferences: `2026-10-23-euro-2028-qualifying.md`.

### The Gold Cup's venue

The class's rounds take venue flags 1 (quarter-finals and semi-finals, 0x5e0c7d, 0x5e0d50), 4 (third place, 0x5e0e11) and 0x80 (final, 0x5e0eb5). The venue code asks `HostCountry::find_hosts` 0x5fa730 with the fixture's record id and year (0x85c5ff..0x85c61c) and plays at the host's stadium (nation +0x80, 0x85c730); with no host it takes the record's nation (+0x5d, 0x85c709). The new record has no host row, so `make` writes one per edition (season_gold_cup.c `gold_host_rows`): the first two hosts of the table's format text (the United States in every edition; Canada in 2015, 2023 and 2025, Costa Rica in 2019; the 2027 host is unknown, so the United States, decision). The test log names the hosts that find_hosts gives and their stadiums ("season gold cup 2019 hosts: ..."); `season_nl_run.py` checks it in runs 2018, 2020 and 2010.

### The Gold Cup with changing rules off (Q398)

With changing rules off a cup that the start season does not hold stays off (Q398). The module's `early` (make an object a season before a held season) therefore acts only with changing rules on. So a 2004/05 start with changing rules off has no Gold Cup (2004/05 holds none), and a 2006/07 start plays the 2007 edition, then every two years.

### National team stats in the club records

`Club_Records::update_records_for_match` 0x444930 asks each player of a national team for his national team stats record (`PlayerStats::get_category_record` 0x7c3920, category 5 when `database_is_national_team` gives 1, 0x445f7b..0x445f96) and stops at the box "club_records.cpp 794" (0x44602d, text "No player stats for %s playing in %s" at 0x98b6ec) when he has none. A player has that record only when his national team called him up. A 2021 start with Greece (October 2021) stopped there with "No player stats for Argentina playing in 2022 Finalissima": Argentina had no other match in that window, so no squad was called up. Making the Finalissima a finals competition (0x76d940) does not call a squad up: tested, the same box. The record is only read after the call (`player_stats_get_record_value` 0x7c2150 at 0x44653f, 0x446690 and 0x4467f7), so in a season game that one call (return address 0x445f9b) gets an empty record of its own in place of 0 (`season_nl_stats.c`): the player counts with no matches and the default rating. Every other call and the base game keep the original's answer (Q152).

### Open item

The Copa America of 12 teams of slice 27 takes Mexico and Canada as guests in every year (0x5e2009, 0x5e1ffe). In 2019 and 2021 its dates meet those of the Gold Cup, so two finals can hold one nation at once. Slice 27 owns the Copa America's guests.
