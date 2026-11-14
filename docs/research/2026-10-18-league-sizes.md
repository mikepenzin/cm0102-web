# League sizes, promotion and relegation in existing divisions (0.6.0 slice 18)

Date 2026-10-18. Original: `clean-3968/cm0102.exe` 3.9.68, virtual addresses. Spec: `docs/specs/2026-10-16-seasons-0.6.0-spec.md` (Q232, Q239, Q247, Q248, Q272, Q281, Q290, Q331, Q359). Code: `tools/recomp/league_sizes.py`, `tools/seasons/make_size_wraps.py`, `src/recomp_rt/win/season_sizes.c` and `.h`, `season_size_wraps.c`, `season_mls.c`, the class counts of `season_clubs.c`. Tests: the rules report (`tests/seasons/rules_report_check.py`).

## Which games

Only a season game gets any of this: a start year that is not 2001, or changing rules on (`season_game_changes`, slice 15). The base game and `RECOMP_SEASON_CODE=off` keep the original's counts: every patched store gives its constant, and every wrap calls the original only.

## The rules of a season

A league takes the row of its source file (`comp_file`) in the rules table. A new game, and every season of a game with changing rules off, takes the row of the start season (Q232). With changing rules on, a later season takes the row of the edition that the object holds (+0x40, s16; `start_new_season` adds 1, for example 0x574c10): the row whose real season begins with that year, else the row of that season. A calendar-year league holds edition Y + 1 in the row of season Y (MLS row 2001/02 is the 2002 season, real season "2002"), so the real season is the key. After the last row the last row stays (Q248).

## Club counts

Each league class of one division stores its club count in `create_table` with one instruction, `mov word ptr [reg + 0x3e], imm` (`League::check_complete` 0x689b3d reads +0x3e), and takes the clubs whose division (club +0x57) is its key id, in the order of the club table, up to that count (`EngConf::create_table` 0x56ecb0 at 0x56ed54..0x56ed7f). With fewer clubs the table is not made (0x56ed95..0x56edab) and the constructor stops at its assert (fra_first.cpp 108 for FraFirst). A class with a year branch has one store per path (FraFirst 0x5bc2c5 and 0x5bc2cd, PolSecond 0x7cbe03 and 0x7cbe14, ScoPrm 0x7f2835 and 0x7f283d, NorFirst 0x7909b1 and 0x790a73).

`tools/recomp/league_sizes.py` lists the 56 classes and their stores (a list made from the translated code of each `create_table`; the build checks each instruction). Each store calls `rt_league_clubs(object, constant)`:

- the base game: the constant;
- a season game: the clubs that the division holds now (the clubs whose +0x57 is the object's record, object +4). A new game fills each division to the count of its start season row (the club moves of slice 15 take `season_sizes_clubs` in place of the class count), so the count is the table's count. In a later season the game's own results decide who moves (Q247): a division holds the count that promotion and relegation give it, which is the table's count when the places of the table are right. When the division holds another count than the row, the log says so (`recomp: league sizes:`).

IreFirst (0x63591f), IrePrm (0x63a74f) and PolFirst (0x7c9469) store a register: they count the clubs of the data already, so only their places change.

Not in the list: the classes of groups (DenSecond, whose real division is groups from 2005/06, BelThird, FraLower, SpaLower, SpaSecondB, PorSecondB, SweSecond, TurSecondB, ItaSerC2a to c, BraNatThird, GerRegional), FinFirst (the North and South groups of the database are two objects of one class with 10 each at 0x593a82, while the table holds one Ykkönen from 2002), and UsaMls (below).

## Promotion and relegation places

`set_up_rules` (vtable +0x8c) of each class writes +0xbe (promotion places, `League::has_automatic_promotion` 0x684790), +0xbf (promotion play-off places), +0xc0 (relegation play-off places) and +0xc1 (relegation places, `League::has_relegation` 0x6847b0); most classes write registers, some constants (EngFirst 0x572dce +0xbf 4). The constructor and `start_new_season` call it. `season_size_wraps.c` (made by `tools/seasons/make_size_wraps.py`, ctest `season_size_wraps`) wraps the `set_up_rules` of the 56 classes: after the original, `season_sizes_rules` sets the places of the row. A row's `up` or `down` can hold the winner of a play-off (the rules report reads it so: the check accepts the direct places, the direct places and one, or the direct and play-off places). So the runtime leaves a value that the check accepts, and else sets the direct places to the row's count, less one when the class has play-off places. The play-offs themselves are slice 19.

## Moves between divisions

**Review fix (2026-10-18).** A pair of size-table divisions whose places differ moves its clubs on their own only with changing rules on and when both divisions stay within their floor and cap after the moves (the upper with the clubs it holds now, the lower before its own exchange below). Else the runtime sets pairs, also for the French chain, which passes none (`FraFirst::start_new_season` pushes 0 at 0x5bc4f1, `League::process_promotion_relegation_chain` passes it at 0x689c59). So with changing rules off the start season's sizes stay (Q232), and a change that needs a division above its cap for a moment stays in pairs: in a game across 2023/24 Ligue 1 keeps 20, because 18 would put 22 clubs in Ligue 2 until its exchange with the National, which keeps 20. A 2023/24 start has the 18. Before this fix the French moves ran on their own and Ligue 2 held 22 clubs in a 20-row table. The check now reads the clubs of each division at every season start (`division_clubs`), so such a gap fails.


At the season change a top league moves the clubs down its chain of lower leagues: `League::process_promotion_relegation_chain` 0x689c20 or the class's own chain (`EngPrm::process_promotion_relegation_chain_body` 0x5752f0, `ItaSerA::process_promotion_relegation_chain` 0x642410 and others). Each pair of divisions goes through `league_promote_and_relegate` 0x689c80 (stdcall: upper, lower, expire contracts, pairwise, expected up, expected down):

- the status byte of a table row (+0x37 of a 0x3b-byte row of the table at +0xb1) says what happens to the club: 3 relegated (set at 0x68525e), 0xff none; the lower league's rows of status 0 or 5 go up (0x689d47..0x689d4e, 0x689efd..0x689f03). Inference from those reads: 0 and 5 are the promotion statuses;
- in pairs (pairwise, 0x689cf9) a relegated club moves only with a promoted partner, so both sizes stay. Most callers pass pairwise 1 (BelFirst 0x41eef8, ItaSerA 0x642483, SpaFirst 0x850747 and others);
- the callers can pass the expected counts, compared with the moves at 0x689f47 and 0x689fbb (asserts league.cpp 5596 and 5602): ItaSerA passes 4 and 4 for Serie A and B (0x642476, 0x642478). Every other caller of the original passes 0xff (none). The two pushes are whole-instruction sites (`tools/recomp/league_sizes.py`, CUP_INSNS): in a season game they pass the moves of the places of the season, Serie A's +0xc1 down and Serie B's +0xbe up, each plus 1 for a play-off of +0xc0 or +0xbf places (`rt_promote_expected`, O260 row A20). The jump from 2001 with changing rules on (England, Italy and Serie C2): the old build passes 4 and 4 against moves of 3 and 3 in each season change from 2004 to 2015; the fixed build passes the counts of the moves in each of them (in 2003, in pairs, 3 and 3), with no fault line. A 2016 start with changing rules off: 3 and 3 in 2016/17 and 2017/18. (That jump stops at the 2015 edition of the Italian leagues, on both builds: a fault of its own.)

So a size change of the table (Ligue 1 2022/23: 4 down, Ligue 2 2 up) cannot happen with the original's calls. The runtime wraps 0x689c80: in a season game, when the relegated rows of the upper league and the promoted rows of the lower league differ in number, the clubs move on their own (pairwise 0) and the expected counts become the smaller one when the wrap sets pairs. A count that still differs from the moves is a fault of the statuses: the log names it ("league sizes: fault") and the check is dropped (0xff) so that the season goes on. With equal numbers the call is the original's.

## MLS

`usa_mls_get_club_division` 0x909ec0 knows the 12 MLS clubs of the original by their key ids (0x9d037c..0x9d03a8, compares at 0x909ee5..0x909f6b) and stops at usa_mls.cpp 1819 (push 0x71b at 0x909f96, box 0x5e8290 at 0x909fbe) for any other club whose division is the American Major League (key id [0x9cf590], 0x909edd). `usa_mls_get_club_all_star_conference` 0x90a000 does the same for the All-Star conferences (stop at usa_mls.cpp 1859, push 0x743 at 0x90a0d6). The groups are 0x413, 0x47b and 0x414 with 4 clubs each (`UsaMls::create_table` 0x908560 stores 4 at 0x908572; `create_second_division_stage` 0x908fc0, `create_third_division_stage` 0x9091e0), the conferences 0x413 and 0x414 with 6.

All 49 Update Team databases hold exactly the 12 clubs of the original in the division (a read of each club.dat; New York appears as "NY/NJ Metrostars" and matches its key id). So only the club moves of a season game put other clubs there. `season_mls.c` wraps both functions: a known club or a club of another division goes to the original; an unknown club of the MLS takes the group with the fewest clubs that has fewer than 4 (counting the division in club table order, the known clubs first), and for the middle group the conference with fewer clubs. A club after the 12th gets 0, the value of a club that is not in the MLS. The MLS lists of the clubs files are in the club moves table now, and the class keeps its 12 places (`season_clubs.c`). Before the fix the 2010/11 start of the second nation group stopped at the box "usa_mls..cpp 1819".

## What else depends on the count

A research pass read every function of the 60 classes (notes of 2026-10-18; the facts below are those this slice relies on, each read again in the code):

- `start_new_season` calls `set_up_rules`, which calls `create_schedule` (vtable +0x3c) with -1: a date template of D dates, D a constant of the class (EngPrm `mov word ptr [eax], 0x26` at 0x57369a), written to +0xa9. Then `create_table` stores the count, then `League::create_fixtures` 0x6835c0 makes meetings x (n' - 1) rounds (n' the count rounded up to even; an odd count gives each club a bye, 0x683c1b..0x683c41). Each round takes a date of the template (`league_next_schedule_round` 0x68a030). In almost every class D is the rounds of its own count. A smaller count leaves the last dates empty. A larger count needs more dates than the template holds: a sequential class (flag 0x40, or fewer than 2 meetings) reads past the template, the others put two rounds on one date.
- Most classes allocate the table for their own count before the store (`push 0x49c` at 0x574a2b, 20 rows of 0x3b bytes, for EngPrm). Seven size it from +0x3e (FraFirst 0x5bc2d3, GerFirst 0x5db2d1, TurFirst 0x8fde69, IreFirst 0x63599d, IrePrm, PolSecond 0x7cbe2d, ScoPrm 0x7f2843).
- Matches per club +0xa7: `League::check_complete` 0x689ad0 sets it to (n - 1) x meetings only when it is -1 (0x689b1d..0x689b36) and calls the league complete when every row has played that many. `League::League` sets -1 (0x682263), and only nine classes set -1 again each season (FraFirst 0x5bc2d7, IreFirst 0x635923, IrePrm 0x63a753, NorFirst 0x790a80, PolSecond 0x7cbe09, GrePrm 0x5eaf2a, GreSecond 0x5ebe78, FraThird 0x5c078e, RusFirst 0x7ea8a2). In every other class a smaller count in a later season would never be complete, so `start_new_season` would not run again. `rt_league_clubs` writes -1 to +0xa7 when the count differs from the class's constant. A class that sets its own value keeps it: `ScoPrm::set_up_rules` writes 38 (33 rounds and the split) at 0x7f29d5 before `create_table` runs (0x7f2b68, then 0x7f2ba6), and a first form of this slice reset it to -1 every time, so `check_complete` counted (12 - 1) x 3 = 33 and the season could end before the split games (review finding).
- The French classes play the second half from the middle of the template (flag 0x80 of 0x68a030 at 0x68a090; flag 0x83 set at 0x5bc480, 0x5bf138, 0x5c0798). So their template must have exactly the rounds of the count: `rt_league_dates` replaces D at FraFirst 0x5bb6b4 (`mov ebp, 0x26`; the 2001 path takes 0x22 at 0x5bb6ad), FraSecond 0x5be65a and FraThird 0x5bfe6a (`mov word ptr [eax], 0x26`) with the rounds of the clubs that the division holds. The allocation of 38 dates (`push 0x9a6`) stays.

So a class holds any count from 8 up to its own largest constant (`CAPS` in `league_sizes.py`). The club moves of a new game fill a division to its row's count, capped there. `rt_league_clubs` gives at most that count. The wrap of 0x689c80 lets clubs move on their own only when both divisions stay within 8 and their largest count after the moves; else the clubs move in pairs, as in the original.

## Cups

A club that leaves a division at a new game goes to no division, as slice 15 does. A first form of this slice put it in "A Lower Division" (where the databases keep a nation's clubs with no league). The staff trim of `create_game` then left it with no players, and the cups take the clubs of that division: a 2022/23 game with an 18-club National stopped in January 2023 at virtual_staff.cpp 195 (`VIRTUAL_STAFF::fill_fixture_squads` 0x90d2f0 wants 15 players of 50 slots at club +0xd7, 0x90d43a..0x90d469) with SC Toulon, a club cut from the National, in a cup tie. So that form is gone.

A domestic cup with a fixed entrant count takes the clubs of whole divisions, so a smaller division can leave it short (a research pass of 2026-10-18 read every cup entrant function; the sites below were read again). The constructor then shows a box: the 2010/11 start of the second nation group stopped at por_cup.cpp 74 (box at 0x7cdb19) with a Portuguese Premier League and Second League of 16, then at wel_cup.cpp 70 and wel_lge_cup.cpp 743 with a Welsh Premier of 12. In a later season the callers of most cups accept the short list (`jge` after the call, for example `EngCcCup` in `update_for_date` 0x56cba0), so the cup plays with stale heap data and no box.

Cups that take a capped number of lower clubs and then every club of the top divisions get a cap of the entrants less the clubs of those divisions (`rt_cup_room`, `rt_cup_add`; the original's cap in the base game):

| Cup | Entrants | Cap site | Top divisions |
|---|---|---|---|
| `PorCup::collect_entrants` 0x7ce220 | 164 (0x7ce234) | `cmp edi, 0x300` at 0x7ce2f8 and 0x7ce3a6 | [0x9cf610], [0x9cf60c] |
| `WelCup::collect_entrants` 0x924ab0 | 32 (0x924ac4) | `cmp eax, 0x54` at 0x924b81 | [0x9cf94c] |
| `DenCup::collect_entrants` 0x551240 | 92 (0x551254) | `cmp eax, 0x120` at 0x55131c | [0x9cf5b4], [0x9cf5b0], [0x9cf5ac] |
| `FraLgeCup::collect_entrants` 0x5bd160 | 54 (0x5bd174) | `cmp eax, 0x60` at 0x5bd2ac | [0x9cf5d0], [0x9cf5cc] |
| `FraCup::collect_entrants` 0x5baa50 | 222 (0x5baa64) | `add ebx, 0xb8` at 0x5bab90 and 0x5bac3c, `add eax, 0xb8` at 0x5bacf0 | [0x9cf5d0], [0x9cf5cc] |

The French League Cup already holds a Ligue 1 of 18 (16 National clubs, 20 and 18), as in 2001/02. Its cap matters from 2024/25, when Ligue 2 has 18 too.

The cap only helps when the lower division holds the clubs that the room asks for. **Fix of 2026-10-19.** A 2020/21 start (November 2020 database, group (a)) stopped at the new game at den_cup.cpp 75 (push 0x4b at 0x550a62, after `DenCup::collect_entrants` 0x551240 returns 0). The database gives the Danish First Division 16 clubs, its row 12. The club moves cut 4 clubs to no division, so the room rose to 52 lower clubs of [0x9cf764] and the database holds fewer: the cup stayed short of 92. The 2012/13 to 2019/20 starts passed, because their databases give the First Division 12 clubs. So the club moves leave the Danish First Division the clubs of the database, up to its class count of 16, when its row has fewer (`season_sizes_keeps_database`). Its real count then stays a `clubs` line of the expected files. With `RECOMP_LEAGUE_SIZES=off` the start did not stop.

**Final round (2026-10-20), all-nation start-only runs on the 2021 to 2025 databases.** Two more shortages showed, and the 2022 to 2024 starts and the group (b) starts passed:

- Danish Cup, 2021/22 (October 2021 database), den_cup.cpp 75. That database keeps only 2 Danish clubs in A Lower Division ([0x9cf764], the division `DenCup::collect_entrants` reads at 0x5512fa with the Danish nation check at 0x551302..0x55131a). The club moves took both into the First Division (the step that takes clubs of the spare divisions), and the cup was short. Main does not stop there; the branch with `RECOMP_LEAGUE_SIZES=off` did. The club moves now take no Danish club out of the spare divisions (`season_clubs.c`, the nation at [0xae23a8] + [0x9cf2c0] x 0x122). Lead decision O7 keeps the First Division at its database count of 16.
- French League Cup, 2025/26 (December 2025 database), fra_lge_cup.cpp 73 (0x5bcb21). `FraLgeCup::collect_entrants` 0x5bd160 takes up to 14 National clubs whose last division (+0x5b) is set and is not the CFA [0x9cf5d8] (`add edx, 0x10` at 0x5bd244, edx -2 after 2001), then National clubs from the CFA up to the room (0x5bd2ac). With Ligue 1 and Ligue 2 at 18 the room is 18, and the National held too few clubs from the CFA. `rt_fra_lge_first` (a whole-instruction site at 0x5bd244; `cmp edi, edx` at 0x5bd247 sets the flags again) lets the first loop take the places that clubs from the CFA cannot fill. With enough of them the limit is the original's.
- French League Cup, 2025/26 (October 2026 Data Update, a Custom database), the same stop at fra_lge_cup.cpp 73 (0x5bca3a, box 0x5bcb21). Both loops of `collect_entrants` skip a National club whose last division (+0x5b) is not set (0x5bd222..0x5bd227, 0x5bd2c4..0x5bd2c9). The database has 19 National clubs: 15 from another division, 2 from the CFA, 2 with none, so 17 of the room of 18 and 53 entrants. The wrap of 0x5bd160 (`season_sizes.c`; callers the constructor at 0x5bca0a and `start_new_season` 0x5bd420) runs only when the clubs with a last division cannot fill the room: it gives as many clubs without one as the room needs their own division as the last one for the time of the call, in the order of the club table, and puts 0 back after. A run with England, France, Germany, Holland, Italy and Spain (start Spain) made the game and played to the first human turn.
- French National background league, 2025/26 (October 2026 Data Update). `FraLower::FraLower` 0x5bd600 fills a stack array of 40 (0x28 at 0x5bd6e9) with every National club ([0x9cf5d4], 0x5bd6db..0x5bd6ed) and up to 20 French clubs (+0x53 [0x9cf300]) of A Lower Division [0x9cf764] with no zone record (+0x60), the cap `cmp word ptr [esp + 0x10], 0x14` at 0x5bd71d. It shuffles only the clubs it took (0x4afb40 at 0x5bd758) and makes two tables of 20 (`FraLower::create_table` 0x5be230 at 0x5bd769, for +0 and +0x50), which reads each place as a club (0x5be2d7). With a National of 19 the last place kept a stack value. Natively that address read zeros and the game went on; in the browser the guest addresses 0x21000000..0x5f000000 hold the page's own data, and the game stopped later with "memory access out of bounds" (Chrome) or a crashed tab (headless Chromium), after the Champions League draws. A native run with those addresses unreadable stopped in 0x5be230 (called from 0x5bd600, from `FraFirst::FraFirst` 0x5bb160). In a season game `rt_fra_lower_cap` (`season_fra_cfa.c`, a whole-instruction site in `tools/recomp/league_sizes.py`) makes the cap 40 less the National's clubs (21 here); the base game keeps 20. The same run then made the game with no stray access.

The Portuguese and Welsh cups did not stop in any of these runs.

**Coppa Italia (2026-10-20).** A 2010/11 start (September 2010 database, group (a) without France, `--last 2010`) stopped on day 330 of 2010 at Cup.cpp 1187 (`Cup::process_fixture` 0x51a150: `Cup::find_tie` 0x519c90 returns -1 at 0x51a336). The Coppa Italia had drawn Foggia twice. `ItaCup::select_entrants` 0x63faf0 fills 32 places: its first loop takes the Serie A clubs, the clubs relegated from Serie A, and the clubs whose last division (+0x5b) is a Serie C1 group with last position (+0x5f) 3, 4 or 5 (0x63fc69..0x63fc9b). While the list is short, its second loop takes C1 clubs by last position p from 5 up (`mov byte ptr [esp + 0x13], 5` at 0x63fb16, 0x63fcf9..0x63fd30), so a club of last position 5 comes in twice. The club moves change the Italian divisions, so the first loop found fewer clubs. In a season game the second loop now starts at 6 (`rt_ita_cup_first_position`, a whole-instruction site); the base game keeps 5.

The other cups fill fixed places with every club of a division, need the sum of whole divisions, or sort a fixed number. A smaller division breaks them: `EngCcCup` 0x56ccc0 and `EngFaCup` 0x56fab0 (England), `WelLgeCup` 0x927010 (Wales), `BelFaCup` 0x41d8d0 (Belgium), `GreCup` 0x5e9a80 (the Greek second tier), `CroCup` 0x517bb0, `RusCup` 0x7e99a0, `TurCup` 0x8fcb90, `NirCup` 0x78ac90, `ItaCCup` 0x63dfe0 (Serie C1), `HolCup` 0x5f4820, `JapJCup` 0x665a00 and `JapEmpCup` 0x6624f0, `KorLeagueCup` 0x66cf20, `ScoLgeCup` 0x7f0400 and `ScoChalCup` 0x7ed010, `PolLgeCup` 0x7cad30, and `FraCup` 0x5baa50 for the National (it takes the whole National, then lower clubs up to 182: with a National of 18 two more lower clubs with almost no players came in, and a 2022/23 game with changing rules on stopped in January 2023 at virtual_staff.cpp 195; with the National at 20 it does not). So the divisions that these cups read never go below their class's count (`FLOORS` in `league_sizes.py`). Making those cups follow the sizes is slice 21's work (domestic cups).

A floor division keeps the class's places too. In a 2022/23 game with changing rules on, League Two got the table's 2 relegation places while its exchange with the Conference (not selected; `EngPrm::exchange_conference_clubs_unplayed` 0x574e60) brought 1 club back, so it held 23 clubs in 2023/24. So the English, Welsh, Belgian, Croatian, Russian, Turkish, Northern Irish, Dutch, Japanese, Korean, Scottish, Polish second tier, Greek second tier, Serie C1 and National places stay the original's, and their `up` and `down` lines stay in the expected files.

The places of a division change only towards a neighbour that is a league of the size table with no floor (the Irish and Polish classes that count their data are such neighbours: their moves stay in pairs, so their places change and their counts do not): `up` when the division above (league +0x1c, the record id; EngFirst::set_up_rules stores it at 0x572dea) is one, `down` when the division below (+0x20, 0x572df6) is one. The object of a record is g_comp_object_table [0xadadfc] at the record id (0x689c42). Elsewhere the exchange is the class's own code with fixed counts: slice 19 found a 2016/17 game (October 2016, England, Italy, France, Germany, changing rules off) that stopped in June 2017 at ger_first.cpp 1049 (box at 0x5dbcd9 in 0x5dbbd0, which splits exactly 36 Regionalliga clubs, `cmp bl, 0x24` at 0x5dbc3b and 0x5dbc7e), because the 2. Bundesliga had 2 relegation places against the 4 promotions of the Regionalliga. At a new game the constructors' `set_up_rules` run before [0xadadfc] is filled (`build_comp_object_table` 0x8386f0 at 0x829640), so the sizes kind sets the places of the start season again in its `new_game` hook.

**Decisions taken (the lead, 2026-10-18).** Q282: a 2019/20 start plays League One with 23 clubs (Bury removed), so the floor of League One is 23 in that season. The MLS keeps 12 clubs for 0.6.0 (groups of 4 at 0x908572, fixtures at 0x9086e0); the expansion clubs of slice 15b get no MLS place. Max may change both.

## Leagues whose size change the code cannot hold

**Decision taken (the lead, 2026-10-18, on Max's behalf; Max may change it):** a class keeps its own largest count, and a division that a cup needs keeps at least its class count, until a later slice builds date templates and tables of any size (and slice 21 the cups). Recommended follow-up: one slice that makes the date template and the table of each simple class at run time from the count (the class's own dates spread over the season), which holds every larger count of the first table below except the classes with halves, periods or groups.

These rows of the table have more clubs than the class holds. The division keeps the class's largest count; the other real clubs stay where the club moves of slice 15 put them (the division below, or no division). Each row stays a `clubs` line of the expected files.

| File | Class holds | Seasons with more | Largest real count | Sites (table, dates) |
|---|---|---|---|---|
| arg_prm | 20 | 2014/15 to 2025/26 | 30 | table 0x40554b, 19 dates a half 0x4045aa, 0x404d9a (Apertura and Clausura) |
| bel_second | 18 | 2007/08 to 2009/10 | 19 | table 0x41fd7b, D 34 at 0x41f37e, periods 0x42012e, 0x42013a |
| cro_a2a | 16 | 2020/21 | 18 | table 0x515cbb, D 30 at 0x51561d |
| den_prm | 12 | 2016/17 to 2019/20 | 14 | table 0x5538db, D 33 at 0x55293a |
| eng_conf | 22 | 2006/07 to 2027/28 | 24 | table 0x56ecbb, D 42 at 0x56de9d |
| fin_prm | 12 | 2002/03 to 2009/10 | 14 | table 0x595b7b, D 33 at 0x5954ea |
| fra_third | 20 | 2010/11 | 21 | table 0x5c05db, D 38 at 0x5bfe6a |
| gre_prm | 14 | 2002/03 to 2018/19 | 18 | table 0x5ead6b, D 26 at 0x5ea70a |
| gre_second | 14 | 2002/03 to 2027/28 | 21 | table 0x5ebcbb, D 30 at 0x5eb5aa |
| hol_first | 18 | 2003/04 to 2026/27 | 20 | table 0x5f624b, D 34 at 0x5f57ca, periods 0x5f65ce..0x5f6c16 |
| ita_ser_a | 18 | 2004/05 to 2027/28 | 20 | table 0x64151b, D 34 at 0x6409ed (sequential) |
| ita_ser_b | 20 | 2003/04 to 2017/18 | 24 | table 0x64678b, D 38 at 0x645b2d (sequential) |
| ita_ser_c1a | 18 | 2004/05 | 19 | table 0x64b67b, D 34 at 0x64adbd |
| jap_j1 | 16 | 2004 to 2026 seasons | 20 | table 0x6634eb, D 30 at 0x662e6d, halves 0x66395a, 0x663d36 |
| jap_j2 | 12 | 2005 to 2026 seasons | 22 | table 0x66503b, D 44 at 0x6647ba |
| kor_league | 10 | 2002 to 2026 seasons | 16 | table 0x66bbab, D 27 at 0x66b16a |
| nir_first | 10 | 2003/04 to 2027/28 | 17 | table 0x78bc5b, D 36 at 0x78b34d |
| nir_prm | 10 | 2002/03 to 2026/27 | 16 | table 0x78ec6b, D 36 at 0x78e36a |
| nor_prm | 14 | 2008 to 2027 seasons | 16 | table 0x7925eb, D 26 at 0x791813 |
| por_second | 18 | 2012/13 to 2017/18 | 24 | table 0x7d152d, D 34 at 0x7d0c7a |
| rus_first | 18 | 2002/03 to 2021/22 | 22 | table 0x7ea6cc, D 36 at 0x7ea25a, stack array of 18 at 0x7ea77d |
| swe_prm | 14 | 2007 to 2027 seasons | 16 | table 0x89043b, D 26 at 0x88feea |
| tur_first | 18 | 2020/21 to 2024/25 | 21 | D 34 at 0x8fd25a |

Also not held, for other reasons:

- **MLS** (10 to 30 clubs): three groups of 4 with their own fixture code (`UsaMls::create_season_fixtures` 0x9086e0, +0xa7 32 at 0x9086d1). It keeps 12 clubs.
- **Brazilian Série A and B** (26 to 20 clubs, a double round robin from 2003): one meeting (0x42a5af, 0x42cb83) and D 27 (0x429b8d, 0x42c17d), with a play-off of the top 8. A smaller count plays a single round robin and the play-off (format: slice 19).
- **Polish First Division** (two groups of 8: seed list 0x7c9464..0x7c94b2, 16 slots at 0x7c950e, D 14 at 0x7c8e34): the count comes from the data and stays.
- **Argentine Second Division** (Primera B Nacional; 20 to 38 clubs): the class takes exactly 25. It adds club [0x9d020c] as the 25th when it finds 24 (0x407dc0, 0x407e06, 0x407e0d), and its zone stages read fixed rows (row 18 at 0x4082fd, groups of 8 at 0x4083d5). A 2010/11 start with 20 clubs stopped at arg_second.cpp 85 (box at 0x406de3). Out of the size table: it keeps 25 and the original's places.
- **Danish 2nd Divisions** (one class of 16; the real tier is groups of 28 to 32 clubs from 2005/06, and its rows give the places of the groups): out of the size table, so it keeps 16 and the original's places.
- **Finnish First Division** (two groups of 10 in the database, one Ykkönen of 13 or 14 from 2002) and the group divisions of Belgium, Spain, Portugal, Sweden, Turkey, Italy (C2), Brazil (Série C) and Germany (Regionalliga): their groups are the database's.
- **Irish First Division and Premier Division** count the clubs of the data and take only 9 to 12 (0x63596f, 0x635979) and 10 or 12 (0x63a79f, 0x63a7a5): their counts stay those of the database.
- **AusNsl** already plays 39 rounds on 24 dates in the original (D at 0x4111dd).

**Smaller counts that a cup does not allow.** These divisions keep their class count, and the real count stays a `clubs` line of the expected files:

- English Second Division 23 (2019/20);
- Welsh Premier 17 (2003/04, 2006/07) and 12 (2010/11 on; 18 kept);
- Belgian First Division 16 (2009/10 to 2019/20, 2023/24 to 2025/26) and Second Division 8 to 17 (2015/16 to 2026/27);
- Greek second tier 10 to 13 (2014/15, 2019/20, 2020/21, 2023/24 to 2025/26);
- Croatian First Division 10 and 12, Second Division 12 to 15;
- Russian First Division 17 (2012/13);
- Turkish Second Division 17 to 19;
- Northern Irish First Division 8 (2002/03);
- French National 17 and 18 (2013/14 on; 20 kept);
- Serie C1 groups 16 and 17 (2012/13, 2013/14).

Serie B 19 (2018/19) is allowed: the Italian Cup takes positions (`ItaCup::select_entrants` 0x63faf0).

A smaller count works in every class of the size table (unused dates at the end of the template). Classes with periods or halves (HolFirst, BelSecond, JapJ1, ScoPrm split) keep their period limits; with fewer rounds the last period is shorter (inference, not tested).

## Tests

Final runs of 2026-10-18 on the branch with main 423ec0f merged (`G=/Users/max/Projects/cm0102/clean-3968`, `D=/Users/max/Projects/cm0102/test-fixtures/databases`). The expected files lose the counts and places that the slice applies and get the MLS list (12 of 18 clubs kept).

| Command | Result |
|---|---|
| `python3 tests/seasons/rules_report_check.py --run build/cm0102_recomp $G --year 2010 --database "$D/September 2010 ODB" --nations all,-argentina,-australia,-belgium,-brazil,-croatia,-denmark,-finland,-france,-germany,-greece,-holland,-italy,-japan,-northern_ireland,-norway --start-only` | PASS. Before the MLS fix: box usa_mls..cpp 1819 |
| the same with `--nations argentina,australia,belgium,brazil,croatia,denmark,finland,france,germany,greece,holland,italy,japan,northern_ireland,norway` | PASS |
| `... --year 2022 --database "$D/October 2022" --nations france --last 2023 --changing-rules on` | PASS: Ligue 1 relegates 4, Ligue 2 promotes 2, and Ligue 1 holds 18 clubs on 34 dates in 2023/24 (`changing-2022-england-france.tsv`) |
| `... --year 2016 --last 2017 --database "$D/October 2016" --nations england,italy,france,germany` | PASS through June 2018. Before: ger_first.cpp 1049 (slice 19's find), then league.cpp 5596 |
| `... --year 2023 --database "$D/October 2023" --nations france --last 2023` (before main 423ec0f) | stopped on day 40 of 2024 at comp_stats.CPP 1664 (Euro 2024 list), also with `RECOMP_LEAGUE_SIZES=off`; main 423ec0f fixes that box; not run again |
| `cd build && ctest` | 29 of 29 pass |

Test switches: `RECOMP_LEAGUE_SIZES=off` (original counts and places in a season game), `RECOMP_SIZE_LOG=1` (a line for each count, place, date count, cup cap and pair move).

Runs of 2026-10-19 on the branch with main 841d270 merged, made again after the merge of main 2425739 (slice 27): every expected file that slices 15 and 18 change, made again with `--update-expected` (the job list of the state file, `--year X --nations <group> --start-only` for each start file, and the longer runs). Each new line was read: the club lists of the MLS (Q404 keeps 12) and of the divisions the code cannot hold, and one 2024 `format` line of the UEFA Cup (slice 25's league phase). The lines that go are counts, places and names that slices 15, 17 and 18 now apply. After the merge of slice 27, its international competitions add 33 `clubs`, `bench`, `idle` and `not_held` lines (a tournament now held, before its draw) and remove 96 `not_held`, 25 `idle` and the OFC club `up` lines. The same-save set (start.sav, prematch.sav, a new game, yeovil.sav with 7 days, `--ref-build` main 2425739) and `season_core_run.py` (season-again) pass.

**Box of the group (b) 2010 run past the split (not this slice).** `--year 2010 --last 2010` with group (b) stops on day 211 of 2010 at Database.cpp 13471 (or fix_man.cpp 3272 in other runs), also with `RECOMP_LEAGUE_SIZES=off` and on main 841d270 alone. `database_is_national_team` 0x53b900 gets a null club (push 0x349f at 0x53b937) from `FIXTURE_MANAGER::find_fixture_clashes` 0x5ad952, which reads the home club (+0x1c) of a Copa Merconorte group game (`con_merc_cup`; away Independiente Santa Fe). `ConMercCup::select_invited_clubs` 0x4c2560 takes two clubs of the MLS from `comp_util_get_latest_placed_club` 0x4afd80 (0x4c28b3, 0x4c28c6) when the MLS object exists; one place came back null (inference: the latest history row of the MLS holds no club in that place). The rules table holds the Merconorte as not held after 2001 (club-world-25 on), so the cup should not run in 2010: that is slice 26 (Q389).

Not run here (the lead runs them in review): the same-save set (Q342), the full jump to 2027/28, and other seasons.
