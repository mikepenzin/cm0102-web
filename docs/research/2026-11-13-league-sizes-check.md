# League sizes: check of the October research (0.7.2)

Date 2026-11-13. Original: `clean-3968/cm0102.exe` 3.9.68, virtual addresses. Checked: `docs/research/2026-10-18-league-sizes.md`, sections "Leagues whose size change the code cannot hold" and "Smaller counts that a cup does not allow", and the rows of `docs/research/rules-0.6.0/*.tsv` for those files. Spec: `docs/specs/2026-11-13-league-sizes-0.7.2-spec.md`.

Six research agents checked the club counts of every season (2001/02 to 2027/28) against two publishers (Wikipedia season pages, RSSSF, worldfootball.net, Transfermarkt, the league's own site) and read the code sites in the original. Two more agents read the code for the shared engine and for the effects on cups, places, club moves and saves. Each address below was read in the original.

## Club counts

Every `clubs` value of the rules table for these files agrees with two publishers, with one exception:

- `tur_second` 2023/24: the table holds 19. Yeni Malatyaspor withdrew before the season and played no match, so 18 clubs played 34 rounds (Transfermarkt, RSSSF).

Future rows with no announcement: `ita_ser_a`, `ita_ser_b`, `ita_ser_c1a`, `ita_ser_c1b` 2027/28 (rows ita-444, 445, 451, 452) hold an inferred 20. The rows of `arg_prm` and the Brazilian leagues for 2026/27 on hold `unknown`. The MLS 2027/28 count of 30 has one source.

Calendar-year leagues: the research table keys by the row's start year. As real seasons, J1 grows past 16 in 2005, J2 past 12 in 2006, the K League past 10 in 2003, the Eliteserien reaches 16 in 2009 and the Allsvenskan in 2008.

## Code sites: corrections

- The "table" addresses of the research are the table allocations (`push` rows x 0x3b). The count stores are separate: arg_prm 0x405552, eng_conf 0x56ecc2, fra_third 0x5c05e2, jap_j1 0x6634f2, jap_j2 0x665042, kor_league 0x66bbb2, nir_first 0x78bc62, nir_prm 0x78ec72, aus_nsl 0x411b82. A larger count needs both.
- Meetings per pair are class constants that the research does not list: AusNsl 3 (0x411d24), JapJ1 2 (0x66362d), JapJ2 4 (0x665149), KorLeague 3 (0x66bcf1), NirFirst 4 (0x78bd80), NirPrm 4 (0x78ed92), FinPrm 3 (0x595ca2), DenPrm 3 (0x553a0d), MLS 4 (0x9086cb). For DenPrm and FinPrm the limit is the meetings, not D 33: the real 14-club seasons play two meetings.
- Date template allocations: JapJ1 0x662e68, JapJ2 0x6647b5, KorLeague 0x66b165, AusNsl 0x4111d8 (finals 0x411654), FraThird 0x5bfe65 (38 dates, so a count of 21 needs a larger block). PorSecond also writes D 34 at 0x7d16bb in `set_up_rules`.
- Sequential classes: ItaSerC1a (flag 0x40 at 0x64b7e5) and ItaSerC1b (0x64f9ae) as well as ItaSerA and ItaSerB. `league_next_schedule_round` 0x68a030 is also sequential below 8 clubs (0x68a071).
- Periods: HolFirst limits 0x5f65ce, 0x5f65da, 0x5f65e6 and lengths 0x5f6bd6, 0x5f6be2 (0x5f6c16 is the +0xd9 flag argument). BelSecond lengths 0x4205b9, 0x4205cb beside the limits 0x42012e, 0x42013a.
- ArgSecond: the groups of 8 are at 0x408412, 0x4084ba, 0x408562 (0x4083d5 is the +0xd9 flag). The zones read rows 1 to 24 (0x408233..0x40838a). The box of line 85 is called at 0x406dde.
- Brazil: the play-off takes the top 4 (0x42aa00..0x42aaf4, 0x42ce80..0x42cf74), not 8. Série B is a double round robin from 2006, Série A from 2003.
- PolFirst stores the constant 8 (0x7c945a), so it takes a fixed 16 clubs. It does not count the clubs of the data (research lines 22 and 133).
- FinFirst: the Ykkönen was two zones of 9 in 2002 and one division from 2003 (14, 13, 10, 12, 10). DenSecond: group totals 28, 32, 24 and 28, one division of 12 from 2021/22.
- Serie C1 regroup: `ItaSerA::start_new_season` 0x641700 calls 0x642630 at 0x641743 after both branches (`2026-10-20-ita-serie-c.md` says 0x641fc0 does).

## Sites that depend on the count, missed by the research

| Site | What breaks |
|---|---|
| ArgPrm `init_relegation_averages` 0x405ef0 (last write 0x406441) | 20 fixed rows |
| ArgSecond `init_relegation_averages` 0x4093f0, `set_predicted_positions` 0x40a022, 0x40a0c5, zone list 0x4081d0 | exactly 25 clubs |
| ItaSerA tie-break zone array, 18 entries (0x642880, 0x641a70) | stack bytes read at 20 |
| ItaSerB tie-break zone array, 20 entries (0x647050, 0x646bb0) | read past the array at 22 and 24 |
| Serie C1 regroup 0x642630: takes up to 36 (0x64269c), writes 36 (0x64275f) | 16 and 17 a group write empty slots |
| C1 play-outs read rows 13 to 16 (0x64bc2c, 0x64fdec) | fixed rows |
| RusFirst take loop, stack array of 18 (0x7ea77d) | more than 18 clubs |
| RusFirst::create_table with fewer clubs than +0x3e (0x7ea7a0..0x7ea7a3) | empty table, no box |
| CroA2a tie-break stage 0x5160f0, stack array of 16 | more than 16 clubs |
| BelThird row 15 of the Belgian Second Division (0x423a80) | a wrong club at 19 |
| JapJ1 stage count 3 (0x663745), final (0x663a50, `Cup_Stage` of 2 at 0x663b32) | halves and final every season |

## Cups

| Cup | Fact | Effect |
|---|---|---|
| `NirCup` 0x78ac90 | Premiership and First Division from place 12 of 32, no bound (0x78ad81..0x78adb0) | the two must hold exactly 20, and every real season from 2003/04 has 24 to 29 |
| `EngFaCup` 0x56fab0 | the Conference into a stack array of 22, no bound (store 0x56fd98) | 24 overwrites the lower-club array |
| `CroCup` 0x517bb0 | 48 slots, no bound (0x517c71..0x517d08) | a second tier of 18 writes past the list |
| `GreCup` 0x5e9a80 | 44 places, second + top at least 28 (box gre_cup.cpp 802, 0x5e9c24) | larger Greek divisions leave top clubs out |
| `RusCup` 0x7e99a0 | First Division from slot 31 down (0x7e9bc2) | holds 20 and 22, 17 leaves a slot empty |
| `TurCup` 0x8fcb90 | Süper Lig from slot 63 down, second tier from 26 | 19 to 21 overwrite second-tier slots |
| `BelFaCup` 0x41d8d0 | 112 clubs: 44 fourth-tier plus 68 | smaller Belgian divisions leave it short |
| `WelLgeCup` 0x927010, `KorLeagueCup` 0x66cf20, `ScoLgeCup` 0x7f0400, `HolCup` 0x5f4820 | fixed lists | write past the list when a division grows |

`season_cup_entrants.c` (slice 21 of 0.6.0) fills only empty places of cup lists. No cup follows the division sizes yet. European and continental places go by position (0x904040, history via 0x4afd80), so any count works.

## Engine facts

- Every class allocates its table inline before its count store (the push is 7 bytes or less before it). FraFirst, GerFirst, TurFirst, PolSecond, ScoPrm, IreFirst and IrePrm size it from +0x3e.
- `League::save_to_temp_file` 0x6891c0 writes +0x3e, +0xa7, +0xa9, the template and the table. `League::load_from_temp_file` 0x687b10 sizes everything from the save. The only version test is [0xdd7110] against 0x3e013 (0x687fc5). A save of any count loads.
- `init_table_row` 0x687430 allocates +0xa7 bytes of results a row (0x687570..0x687578), so +0xa7 below the real matches risks a heap overflow.
- `check_complete` 0x689ad0 reads "played" as a signed byte (0x689b51), and `League_Stage` loops rows with a signed byte (0x68b3a0): at most 127.
- The league table screen 0x491380 and the fixtures screen 0x493ee0 take their rows from +0x3e and scroll. No screen has a fixed row count.
- `would_result_clinch_title` runs only for 24 dates or more (`cmp word [edi + 0xa9], 0x18` at 0x6854cb).
- Issue 106 (Allsvenskan 16) is the model: `GROWN` in `tools/recomp/league_sizes.py`, `rt_league_table_bytes`, and a `create_schedule` wrap that adds midweek rounds. It has no upper limit on the gap, so a midweek round can fall in a winter break.

## Formats that one table of N in a double round robin cannot hold

- arg_prm: halves of one meeting to 2013/14, one table of one meeting 2014/15 to 2023/24, zones of 15 with inter-zone rounds and a knockout of 16 in 2024/25 and 2025/26.
- arg_second: Apertura and Clausura, single round robins, zones of 16 to 19.
- Brazil: Série B single round robin with group stages 2002 to 2005, play-off of 3rd to 6th from 2026.
- MLS: two conferences, an unbalanced schedule and play-offs every season.
- jap_j1: halves and final to 2004 and in 2015 and 2016 only. jap_j2: 4 meetings to 2007, 3 in 2008 and 2009. kor_league: 3 or 4 meetings, stages 2004 to 2006, a top-six play-off 2007 to 2011, a split from 2012.
- Splits: Northern Irish Premiership (33 + 6 and 6), First Division, Welsh Premier (22 + 6 and 6, from 2026/27 sections of 6, 4 and 6), Danish Superliga from 2016/17, Finnish Veikkausliiga 2002 and from 2019, Greek Super League from 2019/20, Belgian First Division with Play-off I and halved points.
- Periods: Eerste Divisie (layouts change by season), Belgian Second Division (no periods 2007/08 to 2009/10).
- Regional groups: Greek second tier, Croatian second tier 2002/03 to 2005/06.

## Rules table cells to correct

From the checks (sources in the agents' notes, carried into slice 1 of the spec):

- tur_second 2023/24 clubs 18.
- ita 2027/28 rows: clubs `unknown`.
- bel_second 2007/08 to 2009/10: no period titles, a final round instead. 2020/21 down 0. 2001/02 and 2002/03 down: licence relegations only.
- bel_first 2020/21 and 2021/22 split "9th to 18th". 2022/23 and 2025/26 play-off points halved.
- hol_first 2001/02 to 2003/04: no period winners. 2019/20: 0 up and 0 down. The period layouts by season.
- fin_prm 2020: no split. 2022: the play-off is the 11th against the winner of an Ykkönen ladder.
- kor_league 2013: split after 26 rounds (no longer an inference).
- MLS 2007 and 2008 play-off rule: sources disagree, the qualified clubs are the same.
- nir_first 2020/21: no source shows a season. nir_first 2026/27 split has one source.
