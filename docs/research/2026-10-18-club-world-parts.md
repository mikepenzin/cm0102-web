# Slice 26 in parts (0.6.0, FIFA and other club competitions)

Date 2026-10-18. The build brief splits slice 26 into lettered parts after the research is read. This doc says what part a does, what is left and what the later parts need. Rules table: `rules-0.6.0/club-world.tsv`. Research of part a: `2026-10-18-club-world-idle.md`.

## Part a: the seasons of the original's competitions (built)

Rule kind `club_world` (`src/recomp_rt/win/season_kind_club_world.c`), registered in `season_change.c`. For each class of the original with a season that the table does not hold, the object stays and its season year +0x40 is the year of its next edition (the state that the original uses between two Club World Championship editions). A competition that the table never holds again waits for the year 2999.

| Class | Seasons with an edition (table) | What the game does now |
|---|---|---|
| World_Club_Champ (FIFA Club World Championship) | 2005/06 to 2024/25, every year | First edition 2005/06; one edition a year, drawn on 1 July (was 5 February, every second year); none after 2024/25 |
| World_Club_Cup (Intercontinental Cup) | to 2004/05 | Waits from 2005/06 for ever |
| Inter_Amer_Cup | none | Waits from the new game |
| Con_Merc_Cup (Merconorte Cup), Conmebol_Merc (Copa Mercosur), Asia_Cup_Winner | 2001/02 only | Wait from 2002/03 |
| Asia_Super_Cup | 2001/02 and 2002/03 | Waits from 2003/04 |
| Asia_Club_Champ | every season but 2022/23 | Skips 2022/23 |
| Oceania_Club_Champ | 2004/05 to 2019/20, 2021/22 on | One edition a year from 2004/05; skips 2020/21 |

Slice 16 offers `season_change_skip()` (the competition keeps its finished season). Part a does not use it: after a gap a skipped League class keeps the start date of its old year, so it never draws again (`2026-10-18-club-world-idle.md`, section 1), and the Oceania Champions Cup and the Asian Club Championship come back after a gap. A competition that waits for its next edition is also not complete, so the pass does not ask it again each season.

Paths: at a new game of a season game (start year not 2001, or changing rules on) the constructors get the year of the first edition; with changing rules on the `before` hook of `start_new_season` moves the year at each season change, and the `after` hook adds one news sentence when the table starts or stops holding a competition. With changing rules off the rules of the start season stay (Q232): a competition that the start season does not hold never comes, and the Club World Championship keeps the form of the start season. The base game and `RECOMP_SEASON_CODE=off` are unchanged (every patch gives the original's value outside a season game). The patches of the start date and the year step (`rt_club_world_month`, `rt_club_world_day`, `rt_club_world_step`, `rt_club_world_annual`, `rt_oceania_step`) also give the original's value when the table has no row of the class (Q248). A fault found by the same-save run rules-on (2026-10-20, a new 2001 game with changing rules on and no rules table): the object kept the original's year 2003 but took the 1 July start date, so byte +0xb6 of `FIFA Club World Championship_104.tmp` (file offset 0x630) was 0xb2 (27 June) against 0x26 (7 February).

Test: `tests/seasons/club_world_check.py` (its docstring gives the runs and checks).

Not covered by the tests: the CONCACAF classes (entry 4 is made only when the United States is selected, and the United States is below row 16 of the Select League(s) screen, so only Select All selects it).

## Left for later parts

### 26b: club counts and formats of the original's competitions

**Built (2026-10-18): the Club World Championship knockout (Form K).** Research: `2026-10-18-club-world-formats.md`. In a season game an edition with 6 or 7 clubs in the table (2005/06 to 2023/24) is one Cup_Stage made on the start date (1 July): for 7 clubs a play-off of the host champion and the OFC winner, quarter-finals with the AFC, CAF and CONCACAF winners, the UEFA and CONMEBOL winners from the semi-finals, the final and the third place match; for 6 clubs the quarter-finals of the AFC, CAF, CONCACAF and OFC winners (2020/21: the host champion in place of the OFC winner). The clubs are the original's picks (`pick_entrants` 0x92a8b0) in round order, except the AFC club: `pick_entrant` 0x92ae60 is called again with the Asian Club Championship record (argument 5), because the original's Asian pick falls back to the Asian Super Cup (0x92aebf), which waits from 2003/04. The matches keep the original's weeks (1 to 12 August) in this branch. Lead decision (2026-10-18, Q293): the real December dates come in part 26c. Left out: the fifth place match (no class of the original has one) and the rule of 2011/12 on of penalties with no extra time in the place matches. Code: the part b block of `season_kind_club_world.c`. Tests: `club_world_check.py` runs 2004-on, 2007 and 2007-load.

The rules report reads the parent's club count +0x3e, which is 0 for Form K, so its `clubs` line for this class says 0 against 6 or 7. That line stays in the expected files: the report needs another count for a class whose clubs are all in a stage (owner of `season_report.c`: slice 12).

**Known leak:** the zeroed parent row (0x3b bytes) of each Form K edition is not freed: the League reset frees the table only with a club count of 1 or more (0x687996). About 1.5 KB over 25 seasons.

**Not built yet in part b (lead, 2026-10-18: all go to part 26c; the December dates and the 32-club form are now built, see 26c below):** the 32-club form of 2024/25, the December dates (Q293), the fifth place match, the club counts of the confederations' competitions (below), and the new competitions (Copa Sudamericana, FIFA Intercontinental Cup, Recopa, Suruga Bank Championship, ACL Two).

- Form G, the 32-club edition of 2024/25 (8 groups of 4 and a knockout from the round of 16, with the Libertadores builders, doc section 4). That season plays the original's 12-club form.

- The Club World Championship host: the original's `HostCountry::redraw_world_club_and_confed_hosts` 0x5fab70 draws a random nation. From 0.6.2 (open item 125) each knockout and group edition takes the host of its row in `club-world.tsv`: place 11 of the picks and the host row (`real_host` in `season_kind_club_world.c`). A random European host had put Barcelona in the 2023 play-off on 12 December, and the clash check moved its Champions League match of 13 December by a week.
- Copa Libertadores: 38 clubs from 2005, 47 from 2017 with a calendar to November, a single final from 2019, no away goals from 2022 (`Conmebol_Seeding` 0x4c5fe0, places at 0x4c6168 and 0x4c617a, `assign_libertadores_places` 0x4c6430).
- CONCACAF: Champions Cup of 8 to 2008 (0x4c11a0), Champions League 2008/09 to 2023, Champions Cup of 27 from 2024; the three Leagues Cup places go to the next MLS clubs in the league table (Q333).
- Asian Club Championship to AFC Champions League from 2002/03 (entrants 0x40b950), autumn to spring from 2023/24, ACL Elite from 2024/25. Australian clubs: Oceania to 2005, the ACL from 2007. Australia's confederation is slice 27's.
- Oceania: 8 clubs (0x798220). The table has more clubs from 2005.

### 26c: December dates and the 32-club form (built 2026-10-19)

**Real dates (Q401, Q293).** `edition_dates` in `season_kind_club_world.c` reads the days of each edition from the `format` text of its row in `club-world.tsv` ("7–16 December 2007", "4–11 February 2021", "14 June – 13 July 2025"). The year of the text minus the row's season is the year offset: 0 for December, 1 for February and for June 2025. With changing rules off the row of the start season stays (Q232), so its days and offset move with the edition's year. The start date fields +0xb6 (day of the year) and +0xb8 (year offset), which the original's start branch tests (0x92a73d..0x92a765), get the day one week before the first match (formats doc 5.4), so the picks read the latest continental winners. The day of the year counts from 0 with the month starts of Date.cpp (0x9b8448, 0x9b8460) and its leap test.

Form K rounds: the first round on the first day, each later round 3 days after the one before, the final and the third place match on the last day. Each draw is 2 days before the first round, else the day after the round before (as the original's final, 0x92975f). The cup round fields are those of the formats doc 3.1 (+0x00 draw day, +0x02 year offset, +0x03 match day, +0x05 year offset).

**Form G, the 32-club Club World Cup of 2024/25.** As the formats doc section 4 recommends: the parent is group A, seven `League_Stage` groups at stage slots 0 to 6 and the knockout at slot 7, built with the Libertadores builders 0x4c37a0, 0x4c3b10 and its advance 0x4c3a80 (knockout 0x4c3d10, 16 clubs) in the order of the Libertadores update 0x4c447d..0x4c45e7. Wraps: `create_schedule` 0x929390 (groups: 3 rounds on the first day and 4 and 9 days later; knockout: round of 16 on day 14, quarter-finals 20, semi-finals 24, the final on the last day, one match each with extra time and penalties, no third place), `advance_stage` 0x929dd0 (to 0x4c3a80), the history thunk (slot 7), and the stage index map of 0x929c70, 0x9298a0, 0x92ad50 (groups 2 to 6 become 1, the knockout 7 becomes 2). +0xbe = 2 and a stage array of 8 slots from the game's operator new.

Picks: the original's 12 and 20 more from `pick_entrant` 0x92ae60, which gives the winner and the runner-up of a continent's record and then clubs of the top reputations (formats doc 5.2). The draw: pot 1 four UEFA and four CONMEBOL clubs, pot 2 eight UEFA clubs, pot 3 four AFC and four CONCACAF clubs, pot 4 four CAF, two CONMEBOL, the OFC club and the host. Each pot is shuffled with the game's random function 0x90cfc0, then the two CONMEBOL clubs of pot 4 move to groups whose pot 1 club is from UEFA.

Tests (`tests/seasons/club_world_check.py`, 2026-10-19): `2007-load` (a save after the quarter-finals on 11 December 2007 loads and ends with places 1 to 3 on 16 December), `2007` (rules off: the editions of 2007 and 2008 both on 7 to 16 December), `2020-on` (4 to 11 February 2021, 3 to 12 February 2022), `2004-on` (11 to 18 December 2005) and `2024-on` (Form G: 32 clubs, 14 June to 13 July 2025, places 1 and 2 on 15 July, when the parent completes) pass. The December editions end with their places while the clubs play league matches in December, so the fixture clash code (Q-26c-6) does not stop them.

**Decisions taken (new small decisions, recommended answers, for Max to confirm):**

- Q-26c-1. The fifth place match stays out. It needs new code in the Cup process path (formats doc 3.4: the Cup routes only the semi-final losers), and the history records places 1 to 3 only.
- Q-26c-2. Form G picks: the real slots (Champions League winners 2021 to 2024, then the ranking, at most two clubs per nation) need older history rows. The game takes each continent's winner, runner-up and then the clubs of the top reputations.
- Q-26c-3. Form G knockout: a random draw of the round of 16 (0x4c3d10 draws with `draw_round`), not the real fixed bracket (group A winner v group B runner-up).
- Q-26c-4. Form G pays no group prize money (the original's 0x92a140 pays 2,500,000 to each club out in the groups; the Libertadores builders pay none).
- Q-26c-5. No set_predicted_positions for Form G (it reads 12 entrants and shuffles the list, 0x92ad36).
- Q-26c-6. A fixture clash of a December edition with a league match: no new code. `FIXTURE_MANAGER::find_fixture_clashes` 0x5ad780 skips one test for this competition's key id (0x5ad92e). The December and February editions of the tests end normally.

### 26c: new competitions (built 2026-10-19, see `2026-10-19-club-cups.md`)

Copa Sudamericana (2002/03 on), Recopa Sudamericana (2003/04 on, with gaps), FIFA Intercontinental Cup (2024/25 on), AFC Champions League Two (2024/25 on), Suruga Bank Championship (2008/09 to 2019/20, Q334). The build brief gives them to the shared new competitions module (`season_comps.c`); no branch holds it on 2026-10-18.

Facts for the module design (read from the data on 2026-10-18; the record index is the position in `club_comp.dat`):

| Record | Index (3.9.68, September 2009, October 2024, December 2025) | Object in the original |
|---|---|---|
| South American CONMEBOL Cup | 60 | none (key id 0x9cf644, sweep-europe 1.1) |
| South American Recopa | 61 | none (key id 0x9cf648, matched at 0x60f799) |
| CONCACAF Cup Winners Cup | 344 | none (key id 0x9cf730) |
| Afro-Asian Champions Cup | 373 | none |
| PrepreLibertadores Cup, PreLibertadores | 218, 219 | unknown |

Inference: a new competition can take a record that the data already holds and no object uses, renamed through `season_change_rename` (the Recopa its own record; the Copa Sudamericana the CONMEBOL Cup record, the competition that it replaced in 2002). That needs no new record in the array at [0xae23d0] and no new key id. An object that waits for ever (part a: the Copa Mercosur, the Merconorte Cup, the Asian Cup Winners' Cup, the Intercontinental Cup, the Inter-American Cup) is saved and loaded like every object, so a kind could give it a new record and a new club picker in place of a new object, vtable and save. The classes read their key id globals, not their record (sweep-europe 2.3), so each reused class needs its club picker and its record reads changed. This is a design choice for the module owner and the lead.

### 26d: substitutes and bench

The `subs_used` and `bench` lines (Club World Championship 5 substitutes from 2020/21, bench 12) are slice 17's kind (substitutes and bench of every part). Part a does not change them.

## For Max

1. Two editions in one season (Recopa in 2006/07, 2008/09, 2014/15 and 2016/17, Copa Sudamericana in 2016/17, separated by `||`). Recommended answer: play both, the second as a second edition of the same object after the first ends. It applies when part c builds these competitions.
2. A competition that ends for good (the Intercontinental Cup after 2004/05, the Inter-American Cup) still shows in the competition menus with its last edition. Recommended answer: keep it in the menus, so its history stays easy to find.
