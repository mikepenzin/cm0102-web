# First-season branches and fixed 2001/02 dates (0.6.0 slice 11)

Date 2026-10-17. Original: `clean-3968/cm0102.exe` 3.9.68, virtual addresses. Code: `tools/recomp/seasons.py` (the year sites) and `src/recomp_rt/win/season.c` (the start year). Sources: `2026-10-16-sweep-season-end.md` sections 3.1 to 3.3 (the site list), `2026-10-16-season-change-points.md` section 2, and a new read of each site's code (two Opus research passes, checked against the code for the sites below).

Y is the start year of the game (`rt_season_year`). "+0x40" is the competition season year. "Game year" is the word at 0xae2c92. "Run-up" is `game_run_to_start_date` 0x5cc850, which plays each day from 1 January Y to the start date. Every competition constructor gets the game year as its year (for example 0x8353f9 for Serie A), so in a game that starts in Y each first edition has year Y or the next edition after Y.

## How a site moves

- **MOVE:** the site compares with Y (Y + 1, Y + 2) in place of 2001 (2002, 2003). The branch means "the first season of the game".
- **KEEP:** the site keeps 2001. The 2001 path is a real 2001/02 fact (a size, a count, a date of that real season), and the other path is right for a later season. In a game that starts in Y > 2001, +0x40 is never 2001, so the game takes the later-season path. This is the rule of the brief for a structure branch that would break a later season.
- **SKIP:** a whole instruction is replaced so that the 2001 data is never used in another start year.
- With Y = 2001 every MOVE and SKIP site gives the original's result. The same-save checks against the 0.5.0 build prove it for the base game.
- The rules table wins over a moved branch (Q239). Slice 11 does not read the table. The values for later seasons below are for slices 15, 18 and 21.

## Table 1: start date

| Address | Function | What it does | Decision |
|---|---|---|---|
| 0x81b93c (push 2001) | `setup_run_front_end` 0x81b890, `Date::set` at 0x81b949 | Game date = 1 January 2001 when the start screen opens | MOVE: 1 January Y |
| 0x8304f1 (push 2001) | `free_game_data` 0x82ff40, `Date::set` at 0x8304fe | The same after a game ends | MOVE |
| 0x83058a (cmp), 0x830597, 0x8305db (push 2001) | `setup_pick_start_date` 0x830520 | A European start date after 1 September of the first season becomes 1 September (0x8305d9..0x830609) | MOVE: the compare and both dates |

`key_nation_build_table` 0x667150 builds each league table entry from the game year. England: start = Date(10 August, game year) (0x667dc6..0x667e00), the nearest Saturday; `create_game` takes off 4 weeks (0x828480). So a 2010 start begins on a day from 10 to 16 July 2010. The clamp never acts on England. A 2010 game in the check shows its first news screen on day 190 of 2010 (10 July when day 0 is 1 January; the check accepts any July day).

The Select Start Season screen 0x81e8d0 writes a split season as `%s %02d/%02d` (0xa8049c) with year % 100 and (year + 1) % 10 (`mov ebx, 10` at 0x81e9c9). 2010 showed "10/01". The constant is now 100: "10/11". For 2001 to 2008 both give the same text.

## Table 2: first-season branches of leagues and cups

| Address | File, function | Test | 2001 path | Other path | Decision | Later seasons (rules table) |
|---|---|---|---|---|---|---|
| 0x4224ec | bel_third `BelThird::fill_league_table` | +0x40 == 2001 | Moves the clubs of Third Division A and B (0x9cf5a4, 0x9cf5a8) into this division (0x422518..0x42253a), then the table of 16 | The table of 16 | MOVE | A database merge, needed in the first season of each game. Size 16 on both paths (0x4224f2). hol-bel: 16 per group to 2008/09, 18 from 2009/10, one group of 16 from 2016/17. Slice 18 |
| 0x5db23e | ger_first `GerFirst::create_table` | +0x40 == 2001 | Moves the clubs of the four regional divisions (0x9cf554..0x9cf560) into German Regional (0x9cf564) (0x5db26f..0x5db2b5) | The table of 18 | MOVE | Merge. Size 18 on both paths. Regionalliga groups change in 2008/09 and 2012/13. Slice 18 |
| 0x7d240c | por_second_b `PorSecondB::create_table` | +0x40 == 2001 | Moves the clubs of II Divisao B Norte, Centro, Sul (0x9cf614..0x9cf61c) | The table of 20 | MOVE | Merge. por: 3 zones to 2004/05, 4 series of 14 from 2005/06. Slice 18 |
| 0x856cdc | spa_second_b `SpaSecondB::load_group_clubs` | +0x40 == 2001 | Moves the clubs of Segunda B groups 1 to 4 (0x9cf62c..0x9cf638) | The table of 20 | MOVE | Merge. spa: 4 groups of 20 to 2019/20, new tiers from 2021/22. Slice 18 |
| 0x8fddce | tur_first `TurFirst::create_table` | +0x40 == 2001 | Moves the clubs of 2 Division Category B groups 1 to 5 (0x9cf8e4..0x9cf8f4) into 0x9cf8e0 | The table of 18 | MOVE | Merge. Size 18 on both paths. rus-tur: 18 to 2019/20, 21 in 2020/21, then 20, 19, 20, 19, 18. Slice 18 |
| 0x56fc5f | eng_fa_cup `EngFaCup::collect_entrants` | +0x40 == 2001 | The FA Trophy winner and runner-up from the database history (0x56fcac..0x56fd00) | Club 0 when it is in the Conference, else none (0x56fc81 loop; inference: an original fault) | MOVE | History of the database. FA Cup replays and exemptions: slice 21 |
| 0x5bb6a7 | fra_first `FraFirst::create_schedule` | +0x40 == 2001 | 34 rounds | 38 rounds | KEEP | fra: 18 clubs in 2001/02, 20 from 2002/03 to 2022/23, 18 from 2023/24. Moved, a 20-club database would get 34 rounds. Slice 18 for 2023/24 on |
| 0x5bc2bd | fra_first `FraFirst::create_table` | +0x40 == 2001 | Size 18 | Size 20 | KEEP | As above. Moved, two clubs of a 20-club database would be left out |
| 0x5bc41c | fra_first `FraFirst::set_up_rules` | +0x40 == 2001 | 2 down | 3 down | KEEP | fra down: 3 to 2015/16, 2 and a play-off 2016/17 to 2021/22, 4 in 2022/23, 2 and a play-off from 2023/24. Slice 18 |
| 0x5bc8ae | fra_first `FraFirst::exchange_cfa_national_clubs` | +0x40 == 2001 | 6 up from CFA | 4 up | KEEP | fra_cfa up 4 from 2002/03, later changes. Slice 18 |
| 0x5bf0aa | fra_second `FraSecond::set_up_rules` | +0x40 == 2001 | 4 up, 2 down | 3 up, 3 down | KEEP | Ligue 2 of 18 from 2024/25. Slice 18 |
| 0x5c070a | fra_third `FraThird::set_up_rules` | +0x40 == 2001 | 4 up, 4 down | 3 up, 4 down | KEEP | National size 18 from 2013/14, 17 in 2017/18 and 2024/25. Slice 18 |
| 0x5d9d31 | ger_first `GerFirst::create_schedule` | +0x40 == 2001 | A dated 2001/02 round template (push 2001 at 0x5d9d3b) | The generic template | KEEP | 18 clubs each season. Real calendars: slice 18 |
| 0x7cb4b8, 0x7cbdfd | pol_second create_schedule, create_table | +0x40 == 2001 | 38 rounds, 20 clubs | 34 rounds, 18 clubs | KEEP | gre-pol-cro pol_second: 18 from 2002/03, later changes. Slice 18 |
| 0x7cbf4f | pol_second `PolSecond::set_up_rules` | +0x40 == 2001 | 6 down | 4 down | KEEP | 6, 4, 2, 3, then 4 to 2012/13, 3 in 2013/14. Slice 18 |
| 0x66b1f1 | kor_league `KorLeague::create_schedule` | +0x40 == 2002 | A late first round after the 2002 World Cup (push 2002 at 0x66b1fb) | Generic template | KEEP | Later World Cup years may need a late start too: slices 18 and 27 |
| 0x421513 | bel_super `BelSuper::select_teams` | +0x40 == 2001 | Swaps the two clubs | No swap: the champion hosts | KEEP | The reason for the 2001 swap is unknown. The other path fits hol-bel bel_super. Slice 21 checks the host of each season |
| 0x5bab83, 0x5bac2f, 0x5bace5 | fra_cup `FraCup::collect_entrants` | +0x40 - 2001 | Cap 184 for the lower clubs | Cap 182 | KEEP | The cap must be 222 - (Ligue 1 + Ligue 2 clubs): 182 to 2022/23, 184 in 2023/24, 186 from 2024/25. Slice 21 |
| 0x5bd237 | fra_lge_cup `FraLgeCup::collect_entrants` | +0x40 - 2001 | First National pass capped at 16 | Capped at 14 | KEEP | Inference: in later seasons two Ligue 1 clubs can miss the cup. Slice 21 |
| 0x5bf6c6 | fra_super `FraSuper::create_round_table` | +0x40 == 2001 | A fixed 2001 venue (push 2001 at 0x5bf6d3) | A random venue, the same date with +0x40 | KEEP | Slice 21 |
| 0x5d92b4 | ger_cup `GerCup::create_round_table` | +0x40 == 2001 | The 2002 final date (push 2001 at 0x5d92bd) | Another final date with +0x40 | KEEP | Slice 21 |
| 0x7ca7c3, 0x7cad3f | pol_lge_cup | +0x40 == 2001 | 7 rounds, 36 clubs | 6 rounds, 34 clubs | KEEP | The cup was not held 2002/03 to 2005/06 and from 2009/10. Slice 21 must remove or rebuild it |

The pushes of 2001 and 2002 inside the KEEP branches (0x5bf6d3, 0x5d92bd, 0x5d9d3b, 0x66b1fb) run only when +0x40 equals that year, so they stay.

## Table 3: awards, friendlies, fixtures, screens, finance, transfers

| Address | File, function | Test | 2001 path | Other path | Decision |
|---|---|---|---|---|---|
| 0x5836c4 | european_awards `EuropeanAwards::daily_update` | date year == 2001 | Skip | Awards in May | MOVE: no awards in the run-up |
| 0x632e30 | international_awards `InternationalAwards::daily_update` | date year == 2001 | Skip | Awards | MOVE |
| 0x79e8a0 | old_international_awards `OldInternationalAwards::daily_update` | date year == 2001 | Skip | Awards | MOVE |
| 0x5fa89b | host_country `HostCountry::daily_update` | date year == 2001 | Skip | Picks the European final hosts on 1 June | MOVE: the first hosts come from the database |
| 0x5c3065 | friendly `Friendly::plan_preseason_tournaments_and_tours` | game year == 2001 | Skip the plan | Plans the pre-season of other clubs | MOVE |
| 0x931ae9 | year_ratings `YEAR_RATINGS::collect_best_players` | game year == 2001 | Minimum 15 | Minimum 25 | MOVE: half a year of ratings (inference) |
| 0x59b56b | fin_prm `finance_get_days_to_season_start` | mov 2002 | Threshold 2002 for split-season leagues | Start year + 1 | MOVE: Y + 1 |
| 0x6eb28d | match_pl `MATCH_PLAYER::setup` | Japan and game year < 2002 | Skip the league stats | Read them | MOVE: Y + 1 (Japan plays calendar years) |
| 0x5aee52 | fix_man `FIXTURE_MANAGER::get_club_first_fixture_year` | 2001 + bit 0 | First calendar year of a club's fixtures | | MOVE: Y + bit 0. The club dates table is saved in `club_dates.tmp`, so a save needs Y: the season part gives it |
| 0x5af218, 0x5af243, 0x5af288, 0x5af2b9 | fix_man `fix_man_record_club_fixture_years` | fixture year == 2001 | Bit 0 = 0 | Bit 0 = 1 | MOVE |
| 0x45582d, 0x45583c (push) | club_screens `club_screen_build_fragment_454950` | season year == 2001 | The range starts on 1 January 2001 | The nation's update day | MOVE, the push too |
| 0x45f4e9 | club_screens `club_screen_draw_fixtures_tab` | season year == 2001 | No step back a year | Step back before the update day | MOVE |
| 0x45f979, 0x45f980 (push) | club_screens `club_screen_draw_fixtures_tab` | season year == 2001 | The range starts on 1 January 2001 | The nation's update day | MOVE, the push too |
| 0x483322 | club_screens `handle_club_screen_action` | friendly date year < 2001 | An unset date gets the game date + 7 | The date as set | MOVE |
| 0x8c7aa9 | transfer_manager `TRANSFER_MANAGER::is_work_permit_granted` | game year == 2001 | A fixed grant for two named players of one Scottish club of the 2001 data | Normal rules | KEEP |
| 0x8595ad, 0x8595b3 | spain_rules `SPAIN_RULES::fill_squad_restrictions` | season 2001 or 2002 | Non-EU limit 3 | 2 from 2003 | KEEP: a real-year rule. Nation rules slice (17) |
| 0x85b83d (+ push 2003 at 0x85b897), 0x85c407 | stadium `Stadium::daily_update`, `set_fixture_venue` | year < 2003 | England's national stadium is closed | Open from 1 June 2003 | KEEP: a real-year event (Wembley). It reopened in 2007: a 2003 to 2006 start needs slice 15 |

## Table 4: league table entry year +0x1b

| Address | Function | Decision |
|---|---|---|
| 0x830fb0, 0x8312b0, 0x83161e, 0x8318c2, 0x831b69, 0x831e7c, 0x8320c9, 0x8371d1 | `create_other_int_comps`, `create_world_club_comps`, `create_european_club_comps`, `create_north_american_club_comps`, `create_south_american_club_comps`, `create_asian_club_comps`, `create_oceania_club_comps`, `create_scottish_comps` (mov word [esi+0x1b], 2001) | MOVE |

Entry +0x1b is the first season year of a league table entry. The other national create functions write the game year there (for example `create_french_comps` 0x833ff0: game year at 0x8340c6, stored at 0x8343ab). Its one reader is `UefaSeeding::assign_uefa_cup_places` 0x904580 at 0x904951: it skips a place when +0x1b equals a competition's +0x40 (0x904966), so the first season's UEFA Cup places come from the data. Moved, a game in Y takes the data places in season Y, as the original does in 2001.

## Table 5: international and club editions

| Address | File, function | Test | What the start in Y gives | Decision |
|---|---|---|---|---|
| 0x58b385 | fifa_confed `FifaConfed::FifaConfed` | year == 2001 | First odd year after Y (no edition in the run-up) | MOVE |
| 0x58cd13 | fifa_confed `pick_continental_champion` | edition < 2002 | Never true: the edition is the first odd year after the start year, 2003 or later (0x58b36f..0x58b390) | KEEP, no later slice: dead code in every start year |
| 0x92dfc4, 0x92eb46, 0x92f240 | world_cup `set_up_team_list`, `draw_groups`, `seed_teams` | edition == 2002 | Edition = Y + 1 rounded up to a World Cup year (0x92bf8c..0x92bfec): 2014 for Y = 2010. The 2002 branches never run | KEEP |
| 0x797042 | oceania_club_champ constructor | year < 2001 | No effect for Y >= 2001 | KEEP |
| 0x7988e9, 0x798918 | oceania_nations constructor | year < 2002 then 2002 | Y rounded up to an even year. Y = 2010 gives a 2010 edition on 5 March, in the run-up; there was no real 2010 edition | KEEP, cycle: slice 27 |
| 0x9291b2 | world_club_champ constructor | year < 2003 | First odd year from Y | KEEP |
| 0x92af4e, 0x92b24b | world_club_champ `pick_entrant` | edition == 2001 | First edition: winners of every continent from the history | MOVE |
| 0x92b970, 0x92ba9c | world_club_cup `pick_qualifiers` | edition == 2001 | First edition: champions from the history | MOVE |
| 0x632540, 0x63266c | inter_amer_cup `select_teams` | edition == 2001 | First edition: champions from the history | MOVE |
| 0x5e1d65 | goldcup `GoldCup::set_up_team_list` (the Copa America) | edition == 2001 | The fixed 2001 field. Y = 2010 gives 2011 | KEEP. Slice 27: the fields and years of 2015, 2016, 2019, 2021 and 2024 |
| 0x4c64fe | conmebol_seeding `assign_libertadores_places` | game year > 2002 | Brazil's places from the cup history for Y >= 2003 | KEEP. Slice 26: the Libertadores places of each season |
| 0x91f486 | wc_oceania_league `update_for_date` | year == 2001 | A campaign that began before the game starts at once in the first year | MOVE |

The four MOVE sites that read the history need the history files of the database. The September 2010 ODB was not checked for them.

## Table 6: UEFA seeding

| Address | Function | Test | Decision | Reason |
|---|---|---|---|---|
| 0x9041ec | `UefaSeeding::collect_european_clubs` | +0x439 == 2001 | MOVE | Reads euro.cfg in the first season. The September 2010 ODB has its own euro.cfg with the 2010/11 entrants. From 0.6.2 (open item 100) only a euro.cfg of the start season: the database's own file, or the game's Data file in a 2001/02 start (rt_euro_cfg_year) |
| 0x903b6f | `UefaSeeding::roll_coefficients` | game year == 2001 | MOVE | The first season keeps the coefficients of nation.dat as they are |
| 0x9038af | `UefaSeeding::pick_english_intertoto_club` | game year == 2001 | MOVE | No volunteer list in the first season |
| 0x9059b2 | `uefa_check_registration_deadlines` | game year == 2001 | MOVE | The 1 February deadline of the run-up belongs to a season the game does not play |
| 0x903e1a, 0x903e32, 0x903e64, 0x903e73, 0x903e8c | `UefaSeeding::set_place_counts` | game year == 2001 | KEEP | The real 2001 Intertoto quotas |
| 0x903eee | `UefaSeeding::set_place_counts` | game year == 2001 | KEEP | The real 2001/02 fair play nations |

The access list of places (0x903d30..0x903db4) is the 2001 list. Later access lists and the end of the Intertoto and fair play places: slices 22 to 26.

## Table 7: fixed 2001/02 data (S sites)

| Data | Where | What the code does without it | Decision |
|---|---|---|---|
| Italian fixed fixture lists, 2,216 calls of `fixed_fixture_set_by_club_names` 0x68aff0 in the builders 0x642a70, 0x647230, 0x64be90, 0x650100, 0x6545c0, 0x658750, 0x65c8e0 | Each constructor calls its builder, then `League::create_fixtures` 0x6835c0 (Serie A 0x6407c5, 0x6407cc) | `create_fixtures` takes the fixed list only when the year of its first entry equals +0x40 (0x68382e..0x68383d, `jne 0x683ac9`). Otherwise the generic round robin at 0x683ac9, which the original uses from 2002/03 on. The constructor sets the same state as `start_new_season` 0x641700 (+0x40 = Y at 0x64071d, the same `set_up_rules` 0x641640 and `create_table` 0x641510) | No change: a game in Y takes the generic path by itself. The 2,216 name searches still run (the list stays unused) |
| World Cup qualifying 2002 data, Europe (`load_2002_campaign` 0x91b760, 162 calls of 0x91db60) and South America (0x9216d0, 40 calls of 0x922350) | The constructors call it when Y is not before the campaign (0x91846f, 0x9204b4) | The campaign is Y rounded up to a qualifying year, at least 2004. In a 2001 game it is 2004, so the original never loads this data. For Y = 2004, 2005, 2008, 2009 and so on it would load the 2002 data into a later campaign, and its asserts (0x91dbc4, 0x9223ab) want dates in 2000 and 2001 | SKIP: the jumps at 0x918473 and 0x9204b8 are taken when Y is not 2001 |
| Fixed dates of the first-season branches (fra_super, ger_cup, ger_first, kor_league) | Table 2 | The other path | KEEP |
| Update days of the league table entries (33 pushes in 0x667150, 35 in the `create_*_comps`) | `Date::Date` 0x549f70 with 2001 | Only the day of the year is kept (0x667e66) | No change. Inference: in a leap start year the update day is one day early |
| The new FIFA transfer rules date, 31 August 2001 (0x524cd2) | `init_transfer_rule_change_date` 0x524cd0 | | No change: a real-year rule, in force from the first day of a later start |
| Award update day (0x413385, 0x4140e4), club history dates (0x44426f, 0x444296), contract offer date (0x4dc134), 23 July 2003 (0x69e84d) | | | No change in slice 11. Their meaning is in the sweep. A check of these in a later start is left for slice 12 (the rules report) |

## Left for later slices

- **Slice 18 (league sizes):** every KEEP of table 2 for seasons where the rules table differs from the later-season path (Ligue 1 18 from 2023/24, Ligue 2 18 from 2024/25, the Polish second tier, the Belgian, German, Portuguese, Spanish and Turkish group tiers when a later database names other group divisions).
- **Slice 15 (club moves):** Wembley for a 2003 to 2006 start (table 3).
- **Slice 21 (cups):** the Coupe de France and Coupe de la Ligue caps, the Polish League Cup, the FA Trophy exemption in the FA Cup.
- **Slices 22 to 26 (UEFA and FIFA):** the Intertoto quotas, fair play places and the access list of table 6; the Club World Championship and Club World Cup cycle.
- **Slices 27 to 29 (international):** the Oceania Nations Cup cycle (a 2010 start plays a 2010 edition in the run-up), the Confederations Cup cycle, a start inside a running World Cup qualifying campaign (Europe and South America from 2004; CONCACAF, Africa and Asia were not read), the late start of the K-League in World Cup years.

## Not years

Menubar.cpp 0x7647f7, 0x76488d, 0x764f7e (menu ids), news.cpp 0x78164b, 0x781671, 0x781697, 0x7835fb, 0x783602, 0x785102, 0x78510d, 0x785118 (news type ids, inference) and the glyph code at 0x93e9b4 compare ids, not years. They stay. Setup.cpp 0x833ca3 (`create_english_comps` 0x833750, named in the sweep with the +0x1b sites) is `mov word [esi+0x1b], ax`: it stores a register (ax, loaded from a local at 0x833c9a), not a constant. The other national create functions store the game year in the same way (`create_french_comps`: game year at 0x8340c6, stored at 0x8343ab). Inference: the local holds the game year, so the site moves with the start year already. It stays.

## Checks

- Base game (Y = 2001): `smoke_run.py --same-save` against the 0.5.0 build on start.sav, prematch.sav, Yeovil (7 days) and a new game: equal saves.
- Y = 2010 with the September 2010 ODB (`tests/seasons/season_core_run.py`): first news screen in July 2010, the fixture store holds only 2010 and 2011, 14 days of holiday, the same save twice.
- Y = 2010 with England and Italy and the September 2010 ODB: first news screen in July 2010, the fixture store holds only 2010 and 2011, so the Italian fixed lists put no fixture in 2001 or 2002.
- Every league selected (Select All) does not reach the game with the September 2010 ODB: the assert Database.cpp 1844 (box at 0x527e9e in `database_generate_extra_non_playing_staff` 0x527240, called from `database_trim_staff_tables` 0x524d70 in `create_game`) stops it in 2001 and in 2010 alike. So the cause is that database with every nation, not the start year. Slices 14 and 18 must look at it.
- Every league selected with the 3.9.68 data and Y = 2010 stops at the assert of `FraFirst::FraFirst` 0x5bb3ad (line 108): `FraFirst::create_table` 0x5bc2b0 needs exactly the size of its path (0x5bc3c0), and the later-season path wants 20 clubs where the 3.9.68 data has 18. A season database of 2002/03 to 2022/23 has 20. **A season database with 18 Ligue 1 clubs (2023/24 on) will stop the same way when France is selected, until slice 18 takes the size from the rules table.** Other leagues with a fixed size in `create_table` can stop the same way: slice 18 must check each league against each season database.

## Unknowns

- Why the Belgian Super Cup swaps its clubs in 2001 (0x421513).
- The meaning of nation byte +0x11c bit 4 in the FA Cup (0x56fc56), and of YEAR_RATINGS +0xc (0x931b02).
- Whether the history files of each season database hold the winners that the MOVE sites of table 5 read.
