# The UEFA league phase, 2024/25 on (0.6.0 slice 25)

Date 2026-10-19. Original: `clean-3968/cm0102.exe` 3.9.68, virtual addresses. The facts of the real competitions are in the section "Slice 25" of `2026-10-18-uefa-access-and-round-names.md` and in `rules-0.6.0/uefa.tsv` rows uefa-185 to uefa-216. Earlier slices: `2026-10-17-uefa-formats.md` (22), `2026-10-18-uefa-2009-2020.md` (23), `2026-10-18-uefa-2021-2023.md` (24).

Code: `season_uefa_league_draw.c` (the draw, no game memory), `season_uefa_league.c` (the stage in the game), `season_uefa.c` (Champions League), `season_uefa_cup.c` (Europa League and Conference League), `season_uefa_seeding.c` (European Performance Spots), `season_uefa_qual.c` (36 clubs), `tools/seasons/make_uefa_table.py` (the rows).

## What the game plays

| Change point | Seasons | What the game plays |
|---|---|---|
| U10 | 2024/25 on | The Champions League, the Europa League and the Conference League each have a league phase: one table of 36 clubs. A club plays 8 matches (6 in the Conference League). Places 1 to 8 go to the round of 16, places 9 to 24 to two-legged knockout phase play-offs, places 25 to 36 are out. |
| U10 | 2024/25 on | Two European Performance Spots: one more league phase place for each of the two nations with the best season coefficient of the season before. |
| Q370 | 2024/25 on | The play-off round is named "Knockout Phase Play-offs" ("KO Play-offs" in the abbreviated set). |
| Q365 | 2027/28 | Russia stays suspended. |

The base game, a game with `RECOMP_SEASON_CODE=off` and every season before 2024/25 keep what they had. After 2027/28 the rules of 2027/28 stay (Q248).

## The table

`make_uefa_table.py` reads to 2027/28 (`LAST` 2027):

- The round "League phase 36" is the `Groups` round of the row, "R16" is `KO`. The league phase is one group of 36 that meets once, with 24 places through (`uc_groups` 1, `uc_group_clubs` 36, `uc_through` 24; the same for the Conference League).
- `cl_league`, `uc_league`, `ecl_league`: the matches of a club ("one table of 36, 8 matches each"), 0 for a group stage.
- The Europa League and Conference League rows write the main path "LP" in some seasons and "MP" in others. Both are MP (Q541).
- The 2027/28 rows give no rounds list for the Europa League and the Conference League, no tie rules for the Champions League and no Super Cup date. These take the values of 2026/27 (Q542). The access list of 2027/28 is read from its row.
- `ECL_DROPS` of 2024/25: the Champions League Q1 losers and the Europa League Q1, Q2, Q3 and play-off losers go to the Conference League (no preliminary round any more).

## The league phase stage (season_uefa_league.c)

The original makes a group with `League_Stage::League_Stage` 0x68b100 (thiscall, 22 arguments, `ret 0x58` at 0x68b4c1), as `draw_phase1_groups` does at 0x5858b6. The arguments, from the constructor's code:

| Argument | Field | Evidence |
|---|---|---|
| 0 parent | +0x49, +0x4a copied from it | 0x68b22f..0x68b261 |
| 1 clubs | +0x3e | 0x68b141 |
| 2 member list | one 0x687430 call per club | 0x68b367..0x68b3ab |
| 3 meetings | +0x3c | 0x68b14d |
| 4 record | +4 | 0x68b173 |
| 5 round table | copied to a new block at +0xba (+0xa9 x 0x41 bytes) | 0x68b26f..0x68b2d9 |
| 6 match days | +0xa9 | 0x68b158 |
| 7, 8, 9 | +0xc2, +0xc3, +0xc4 | 0x68b166, 0x68b19d, 0x68b17f |
| 10 order bytes, 11 places bytes | +0xc5..+0xc8, +0xbe..+0xc1 | 0x68b1a5, 0x68b1be |
| 12 year, 13 stage, 14 name | +0x40, +0x44, +0x3a | 0x68b1e4, 0x68b206, 0x68b211 |
| 15, 16, 17, 18 | +0x51, +0x42, +0xab, +0xd9 | 0x68b185, 0x68b21d, 0x68b23a, 0x68b241 |
| 19 matches of a club | +0xa7 | 0x68b215, 0x68b228 |
| 20, 21 | +0x28, +0xe1 | 0x68b264, 0x68b267 |

With a round table the constructor calls `League::create_fixtures` 0x6835c0 at 0x68b3b9.

`create_fixtures` has two paths. The round robin at 0x683ac9 cannot give 8 matches of 35 possible opponents. The other path reads a fixed list at +0xea when the year word +2 of its first entry equals +0x40 (0x68382e..0x68383d):

- The list is an array of 16-byte entries made with new[]: the count is the dword before the first entry. It is deleted at 0x683aa1..0x683abe (the vector destructor 0x9462bb with the empty element destructor 0x5a53c0, then operator delete 0x944c94), and +0xea is set to 0 (0x683abe).
- Entry d x (clubs / 2) + i is match i of match day d (0x6838ef..0x683906, `shl eax, 4`).
- Entry +8 is the home club id, +0xc the away club id. They are compared with the club id of each table row (rows at +0xb1, 0x3b bytes each, 0x68392f..0x68395a). `fixed_fixture_set_by_club_names` 0x68aff0 writes the ids at +8 and +0xc in the same way for the Italian lists.
- The date of each match is the date of its match day in the round table (0x6839f8..0x683a10).
- Each fixture goes to the game's fixture list through 0x5aa680 (0x683a55), as in the round robin.

So `season_uefa_league_make` gives the constructor a round table of 8 (or 6) match days and a fixed list of the draw's 144 (or 108) matches. The wrap of 0x6835c0 writes the list to +0xea of the one object that is being made, just before the original reads it. Every other league runs as it is: the wrap writes nothing when no league phase stage is being made.

The end of the stage. `League::check_complete` 0x689ad0 wants the played count of each table row (row +0x11) to reach +0xa7 (0x689b47..0x689b58). With -1 there it uses (clubs - 1) x meetings (0x689b1d..0x689b36), which would be 35. The league phase passes its match count as argument 19.

The other arguments are those of the original's group draws (3, 1, 8, year, stage, name, 0xf, 2, 0, 8, then 0 and 2), apart from the order bytes. The groups pass 4, 1, 2, 0: code 4 starts a head-to-head pass before the sort (0x682850..0x682877), the sort compares points (+0x19, comparator 0x683410), and for clubs level on points code 1 compares goal difference (row +0x15 minus +0x17) and code 2 goals scored (+0x15) (0x68a535..0x68a556). The league phase ranks level clubs by goal difference, then goals scored, with no head-to-head (regulations 2024/25, Art. 18), so it passes 1, 2, 0, 0. The next criteria of Art. 18 (away goals, wins, away wins) have no code in the game. The places byte is 8: the clubs of places 1 to 8 go straight to the round of 16 (inference: the byte marks the places that go through, as 2 in a group of 4).

The match days are the real days of each published season, and a later season takes those of the last published season, moved to the same weekday (Q547, update of 26 October 2026): the Champions League on Wednesdays, the Europa League and Conference League on Thursdays. A matchday over several days takes its Wednesday or Thursday.

| Season | Champions League | Europa League | Conference League |
|---|---|---|---|
| 2024/25 and before | 18 Sep, 2 Oct, 23 Oct, 6 Nov, 27 Nov, 11 Dec, 22 Jan, 29 Jan | 26 Sep, 3 Oct, 24 Oct, 7 Nov, 28 Nov, 12 Dec, 23 Jan, 30 Jan | 3 Oct, 24 Oct, 7 Nov, 28 Nov, 12 Dec, 19 Dec |
| 2025/26 | 17 Sep, 1 Oct, 22 Oct, 5 Nov, 26 Nov, 10 Dec, 21 Jan, 28 Jan | 25 Sep, 2 Oct, 23 Oct, 6 Nov, 27 Nov, 11 Dec, 22 Jan, 29 Jan | 2 Oct, 23 Oct, 6 Nov, 27 Nov, 11 Dec, 18 Dec |
| 2026/27 and later | 9 Sep, 14 Oct, 21 Oct, 4 Nov, 25 Nov, 9 Dec, 20 Jan, 27 Jan | 17 Sep, 15 Oct, 22 Oct, 5 Nov, 26 Nov, 10 Dec, 21 Jan, 28 Jan | 15 Oct, 22 Oct, 5 Nov, 26 Nov, 10 Dec, 17 Dec |

Sources: 2025/26 and 2026/27 from the uefa.com fixture pages of each competition, with a second source for each: tntsports.co.uk (2025/26 Champions League), laligaexpert.com (2025/26 Europa League and Conference League), si.com (2026/27 Champions League) and flashscore.com (2026/27 Europa League and Conference League), all read 26 October 2026. UEFA published its own Europa League and Conference League days for both seasons, so none of them is taken from the Champions League week.

## The draw (season_uefa_league_draw.c)

The clubs are sorted by the European seed (0x586f60: the entry code, then reputation), as the original's group pots (Q544, as Q411: the game keeps no club coefficient). With 8 matches the pots are 4 of 9; each club meets two clubs of each pot, one at home and one away. With 6 matches the pots are 6 of 6; each club meets one club of each pot, and of the pots 1 and 2, 3 and 4, 5 and 6 it plays one at home and one away (regulations 2024/25, Art. 16).

- Two clubs of one nation (club +0x53) do not meet, and a club meets at most two clubs of one nation (Art. 16.02). After 200 tries with no draw the second rule is dropped, after 400 the first too (as Art. 16.03 lets UEFA change them). The ctest draws 480 leagues: all but a few keep both rules; the few that drop a rule have 5 or 6 clubs of one nation.
- The matches go into match days, each club once a day, by a search with restarts.
- The rule of no more than two home or away matches in a row (Art. 17.02) is not kept (Q544).
- The random numbers come from the game's random number function 0x90cfc0, so the same clicks give the same draw.

## The knockout stage

After the league phase the object holds a Cup_Stage of 24 clubs in stage 1 (constructor 0x522e00, 15 arguments, as slice 24's knockout round play-offs):

- List slots 0 to 15: the 8 play-off ties of the bracket (Art. 19.02): places 9 or 10 against 23 or 24, 11 or 12 against 21 or 22, 13 or 14 against 19 or 20, 15 or 16 against 17 or 18, which of each pair by draw. The seeded club (9 to 16) plays the second leg at home. Round 0 keeps these ties (+0xd 0x200).
- List slots 16 to 23: places 1 to 8. The round of 16 is a seeded draw (+0xd 0x400, tier byte 2 for places 1 to 8 and 1 for the play-off clubs): each club of places 1 to 8 meets a play-off winner, of another nation when a draw is found (+0xb 4). The quarter-finals and semi-finals are the original's random draws (Q543). The real bracket after the play-offs is not followed.
- Places 25 to 36 are out (+0x1db -1, as 0x5862fe).

Champions League: the round table has 5 rounds: code 10 (the play-offs, 16 clubs), code 20 (the round of 16), then the original's quarter-finals, semi-finals and final (create_round_table stage 12, codes 0x78, 0x82, 0x96). Dates: the play-off draw on 31 January and first legs on 12 February, the round of 16 draw on 21 February and first legs on 5 March, the second legs a week later. Europa League and Conference League: slice 24's table with the play-offs (codes 30, 40, 0x78, 0x82, 0x96).

## The Champions League class (season_uefa.c)

The object holds 2 stages (+0x2c 2). The wraps of slice 22 know this layout:

- `cl_set_format` sets 2 for a row with `cl_league`.
- `rt_uefa_cl_ko_offset` (the two reads of the knockout slot, 0x584ad3 and 0x584cf2) gives slot 1.
- `cl_round_table` (0x583c50): stage 0 the league phase table (8 match days, name 0x41f, league 1), stage 1 the knockout table. Comp::build_match_date_list 0x48d500 asks for each stage.
- `cl_advance_stage` (0x5851d0): at stage -1 the league phase draw in place of `draw_phase1_groups` 0x585290, at stage 0 the knockout draw.
- `cl_rank_teams` (0x586900): bands of the 5 knockout rounds, then places 25 to 36 (one band of 12), then the qualifying rounds.
- `map_stage_arg`: stage 1 is the original's knockout 12 for set_club_progress 0x586c00, build_fate_news_text 0x584550 and update_staff_records_for_status 0x584ae0.

The league phase draw takes its clubs as the original does (0x585358..0x5853ba): list slots 0 to 15 and every entry with byte +5 = 1. `season_uefa_qual.c` now wants 36 there in a league phase season (the row's `Groups` round). Each club gets a coefficient point and the group stage money of the original, 1,800,000 with its news (0x5857ea..0x58582a, code 0x449). The play-off losers (entry byte +5 = 2) go to the Europa League from its slot count - 0x18, as 0x5859fa..0x585a86, and the Europa League ranks its teams (vtable +0x5c). At the knockout draw each of the 24 clubs gets a point and the original's phase 2 money, 1,200,000 (Q545: the original's amounts, not the real ones).

## The Europa League and Conference League (season_uefa_cup.c)

`uc_format` gives the league phase format: 1 group of 36, 24 through, 16 in the play-offs, 8 or 6 match days. A cup with a league phase holds 2 stages, so `groups_layout` now allows 2 to 13. The advance (0x5a8f60) draws the league phase at stage -1 and the knockout at stage 0. The clubs of the league phase come from `uc_group_entrants` (slice 23: the play-off winners and every club of the slots after the rounds). The 8 slots of the Champions League thirds stay at the end of the entry table and stay empty: no thirds come in a league phase season, and no Europa League thirds go to the Conference League (`ecl_take_el_thirds` finds no 8 groups).

## European Performance Spots (season_uefa_seeding.c, Q540)

`roll_coefficients` 0x903b50 runs before the places at each season: it shifts the six season values of each nation (doubles at nation +0xe0 to +0x108) and writes the points of the season before divided by its clubs entered to +0x108 (`2026-10-16-sweep-europe.md` 3.2). At a new game there is no shift, so +0x108 holds the database's value of the season before. The two nations with the highest +0x108 each get one more Champions League league phase place, after the title holders and before the Europa League places: their best-placed club with no place yet (Art. 3.07). The other clubs of the nation then move down one place, as Art. 3.08 says, because the Europa League places come after. The nation's league table mark (+0xc) counts the place. A log line names each nation, its rank, its value and the club.

## News (Q258)

With changing rules on, the kind `uefa` writes for 2024/25: "The Champions League group stage is now a league phase: one table of 36 clubs who play 8 matches each. Places 1 to 8 go to the round of 16, places 9 to 24 to knockout phase play-offs." and "The Europa League and the Conference League now also have a league phase of 36 clubs, with 8 and 6 matches each." No group stage sentence comes across the change.

## Decisions (recommended answers taken; for Max to confirm)

- Q540: the European Performance Spots go to the two nations with the best season coefficient of the season before in the game (nation +0x108, from the game's points; at a new game the database's value), not to the real nations.
- Q541: the main path of the Europa League and Conference League is MP, also where `uefa.tsv` writes LP.
- Q542: the 2027/28 formats that the sources do not give (Europa League and Conference League rounds, Champions League tie rules, Super Cup date) take the 2026/27 values.
- Q543: the knockout phase play-offs follow the bracket of the regulations; the round of 16 is a seeded draw of places 1 to 8 against the play-off winners, and the quarter-finals and semi-finals are random draws, as the game's knockouts are.
- Q544: the league phase pots use the European seed (entry code, then reputation), as Q411. The holder is not put first in pot 1, and the match days do not follow the home and away run rule of Art. 17.02.
- Q545: the money of the league phase and knockout phase is the original's group and phase 2 money.
- Q546: the places byte of the league phase table is 8 (places 1 to 8 marked as through).
- Q547: the match days and knockout dates of 2024/25 serve every season from 2024/25. Update of 25 October 2026 (O259, lead 4, Max's approval of all decisions): the knockout dates are the real dates of each season wherever UEFA has published them (2024/25, 2025/26 and the 2026/27 match dates), and a later season takes the last published season's dates moved to the same weekdays, as Q542 does for formats. The table and its sources: `docs/research/rules-0.6.0/uefa-ko-dates.tsv`, `docs/research/2026-10-25-uefa-ko-dates.md`. Update of 26 October 2026 (A18, lead 4, O378): the league phase match days are also the real days of each published season (2024/25, 2025/26, 2026/27), and later seasons take the 2026/27 days on the same weekdays. From 2026 FIFA's September window is a double window (21 September to 6 October 2026, 18 September to 3 October 2028), and the 2024/25 days put matchdays 1 and 2 in it: in a 2025 test game Lyon v Hajduk Split (20 September 2028) moved for its players' CONCACAF qualifiers.

## The stage name

The stage name id of the league phase is 0x41f, "Group A" in the original's name table (0x4b9a44). The round name function 0x4b4850 names stage ids too, and the screen wraps of `season_round_names_game.c` set its competition. The round names table takes round codes up to 990 only, so the wrap asks `season_uefa_stage_name`: id 0x41f of a competition whose row has a league phase reads "League Phase" (regulations 2024/25, Art. 13.02, Q370). The rules report shows it ("European Champions Cup League Phase").

## Tests and results (2026-10-20)

- ctest `season_uefa`, `season_uefa_table`, `season_uefa_league`, `season_round_names`, `season_round_names_table`: pass.
- `tests/seasons/uefa_2024_run.py` with `build/cm0102_recomp`: `2024`, `2025`, `change`, `load`, `repeat` pass. The `load` run saves on day 120 (league phase) and day 215 (13 February, after the play-off draw; the run checks that the first program drew the knockout before it saved), and each loaded game plays to the final. Each league phase table has 36 clubs with 8 (Champions League, Europa League) or 6 (Conference League) matches played by every club, the play-off ties are the pairs of the bracket, and each cup has a winner. The draws kept both nation rules in every run.
- Rules reports: `--year 2024 --database "October 2024" --last 2025` gives `rules_report_expected/2024-england.tsv`, `--year 2025 --database "December 2025" --last 2026` gives `2025-england.tsv`. The league phase format lines are gone (the check reads "League phase 36, KO play-offs 16, R16 16"). Left: the bench and substitutes counts and the competition names of the original, which other slices own.
- The club lines of `2025-england.tsv` are all of 2026/27: the game holds 82 Champions League, 77 Europa League and 164 Conference League clubs, the 2026/27 rows 81, 76 and 165. The run has changing rules off, so 2026/27 plays by the rules of the start season 2025/26 (Q232), whose rows give 82, 77 and 164. The counts of 2024/25 and 2025/26 equal their rows.
- The table order: level clubs rank by goal difference, then goals scored (order bytes 1, 2, 0, 0; see "The league phase stage"). A log line counts the level pairs of each league phase table and those out of that order; the test wants none out of order.
- `--year 2025 --last 2027` stopped at the box "Database..cpp 13126" in May 2028 (`NationalTeams::select_squad`, Euro 2028 finals); main's Euro 2028 fix is merged. The full jump from 2001 with changing rules on stopped at the same box on day 247 of 2015 in the original's Champions League group draw; slice 23's draw guard (293456d, merged in 476cf2b) removes it. After both, the full jump stops on day 345 of 2009 at "world_cup..cpp 1438" (the World Cup box of a moved season, slice 27). None of these runs code of this slice.
- Same-save (Q342) with start.sav, prematch.sav, a new game and yeovil.sav (7 days): every run equal to normal, `season-code-off` too, except `rules-on`, whose save also differs in `FIFA Club World Championship_104.tmp` (slice 26's competition; this slice changes nothing in a 2001/02 game).

## Open points

- The Euro 2028 and 2015 boxes above (other slices).
- The `rules-on` difference in `FIFA Club World Championship_104.tmp` (slice 26).
