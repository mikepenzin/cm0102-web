# UEFA club competition formats by season (0.6.0 slice 22)

Date 2026-10-17. Original: `clean-3968/cm0102.exe` 3.9.68, virtual addresses. Code: `src/recomp_rt/win/season_uefa.h`, `season_uefa.c` (Champions League, news, report), `season_uefa_cup.c` (UEFA Cup and Intertoto Cup), `season_uefa_lookup.c`, `season_uefa_table.c` (made by `tools/seasons/make_uefa_table.py`), `tools/recomp/uefa.py` (constant sites). Tests: ctest `season_uefa`, `season_uefa_table`, and the rules report (`tests/seasons/rules_report_check.py`, kind `format`).

## What the slice does

| Change point | Seasons | What the game plays |
|---|---|---|
| U1 | 2003/04 on | Champions League: one group stage of 8 groups of 4, then a knockout round of 16 (in place of the second group stage and the quarter-final draw of the original) |
| U2 | 2004/05 on | UEFA Cup: Q1, Q2 and a first round of 80, then 8 groups of 5 that meet once, then a round of 32 with the top three of each group and the 8 Champions League group thirds, then R16, QF, SF and the final |
| U3 | 2006 to 2008 | Intertoto Cup: 3 rounds, the 11 winners of the third round enter the UEFA Cup second qualifying round |

The format of a season comes from the table, not from tests of years. `season_uefa_row(S)` gives the row: none for the base game (start year 2001, changing rules off) and with `RECOMP_SEASON_CODE=off`; with changing rules on the row of S; with changing rules off the row of the start year (Q232). After the last row (2008/09 in this slice) the last row applies (Q248). Slices 23 to 25 raise `LAST` in the tool and add their seasons.

## The table

`tools/seasons/make_uefa_table.py` reads `docs/research/rules-0.6.0/uefa.tsv`: for each season the `european_cup`, `uefa_cup` and `intertoto_cup` rows. It takes the "Rounds (clubs):" list of the format (names Q1, Q2, Q3, QR, PO, R1 to R4, Groups, Groups 2, KO), the UEFA Cup group text ("N groups of M, single|double round robin") and the Intertoto round count and winners ("the 11 third-round winners enter the UEFA Cup second qualifying round"). A row it cannot read stops it. The ctest `season_uefa_table` checks that the committed C file is the one the table gives.

## Champions League (U1)

The original (EuropeanCup, vtable 0x96a1d0, Cup base) holds 13 stages: array +0xc, count +0x2c = 13 (0x583820), current +0x30 (-1 is the qualifying rounds of the Cup base). `advance_stage` 0x5851d0 draws phase 1 into stages 0 to 7 (0x585290), phase 2 into 8 to 11 (0x585ab0) and the quarter-finals into 12 (0x586330).

Why the new knockout stage is in slot 8 and not in slot 12: Cup base code walks the stages up to +0x30 with no NULL test (Cup::update_for_date 0x51f8da and 0x51f957, Cup::check_complete 0x521e2b, Cup::save_cup_to_disk 0x521573, Club_History::record_comp_season 0x44313b, the competition screens 0x49d49b, 0x48fe5d, 0x4950fc, menubar 0x764661), and Club_History reads a stage's +0x44 as its slot (0x443051). So the format holds 9 stages: groups in 0 to 7, the knockout (Cup_Stage, 16 clubs, 4 rounds, +0x44 = 8) in 8. The save keeps +0x2c and +0x30 (0x521535, 0x521541; load 0x520c48, 0x520c56), so a count of 9 marks the format in a loaded game.

The format is chosen when the class asks for its qualifying round table (create_round_table 0x583c50 with stage -1: from the constructor at 0x5838d4 and from set_up_rules 0x858e70 inside start_new_season 0x584d60, after the year moves). Wraps for a count of 9:

- `advance_stage`: at stage 8 the runtime draws the round of 16: each group runner-up meets the winner of another group, of another nation when a draw is found (club +0x53, the field 0x585bab compares); the runner-up is first in the tie (inference: the second club plays the second leg at home). Each club gets what phase 2 gave: bookings reset (0x7e9180), a coefficient point (0x9058b0), 1,200,000 (0x586065). The group thirds go to the UEFA Cup and the fourths are out, as 0x586282..0x58630b.
- The round table of the knockout: the original's 3 rounds (create_round_table stage 12) with a round of 16 before them, made from the quarter-final's record: code 20 ("Second Round": codes 10 to 150 are taken, 0x4b4850), flag 0x200 of +0xd (the ties are the list's pairs; the draw 0x51c410 skips the random draw at 0x51c5e4), 16 clubs, all new. First legs on 20 February, second legs 14 days later (+0x22), the weeks of the original's second group stage (0x58415a). The quarter-final loses flag 0x200, so its draw is random (map_fixture_tree 0x518dc6).
- `rank_teams` 0x586900 (predicted positions by bands, the clubs sorted by strength 0x4c08f0): the knockout bands come from the 4-round table, and the 2 bands of 4 of phase 2 (0x586aef) go.
- The stage argument 8 of `set_club_progress` 0x586c00, `build_fate_news_text` 0x584550 and `update_staff_records_for_status` 0x584ae0 becomes 12, the knockout of the original.
- The two reads of the knockout slot with offset 0x30 (0x584ad3 in record_season_honours, 0x584cf2 in the staff records) read `rt_uefa_cl_ko_offset` (0x20 for 9 stages).

## UEFA Cup (U2)

The original (Uefa_Cup, vtable 0x970790) is a Cup of one stage (+0x2c = 0 at 0x902344). Its entry table +0xa7 has 145 slots (0x902ba7): 118 seeded clubs (0x902cb0), qualifying clubs first (sort 0x522190 by entry byte = club +0x1df), then 3 slots for the Intertoto winners (count - 0x1b, 0x633deb), 16 for the Champions League third qualifying round losers (count - 0x18, 0x585a02) and 8 for the Champions League group thirds (count - 8, 0x586286). A round takes +0x1c new clubs from slot +0x1e.

The format keeps the entry table. Its round table (wrap of 0x9025c0, stage -1, from the constructor 0x9023cd and set_up_rules 0x9030b0) has Q1 (code 250), Q2 (260) and R1 (10). R1 and Q2 come from the row; Q1 is set so the three rounds take every slot before the thirds: Q1/2 + Q2/2 + R1 = count - 8. With 145 slots that gives Q1 50, Q2 64, R1 80 (2004/05), Q1 52 with Q2 62 (2005/06). The row's own Q1 count (62 to 74) needs another number of seeded clubs, which the original's place brackets fix at 118: see "Not done". When the UEFA Cup has clubs of the original's extra round (+0xb2, 0x902c85) the original format stays.

Then the object holds 9 stages, as the Champions League: 8 League_Stage groups (0x68b100: 5 clubs, 1 meeting, 5 match days on 21 October to 16 December; create_fixtures 0x6835c0 adds a resting place for an odd count, 0x683c1b) and a Cup_Stage of 32 clubs and 5 rounds (codes 30, 40, 0x78, 0x82, 0x96; 17 February to the final on 18 May). The group draw takes the 40 R1 winners (entry byte +5 = 1, as 0x585383), sorted by seed (0x586f60) into 5 pots of 8, one club of each pot a group, no two clubs of a nation in a group when a draw is found. The knockout draw: the 8 winners and 8 runners-up are seeded; the 8 group thirds and the 8 Champions League thirds (from their slots) meet them, not their own group, not their nation when a draw is found. The fourths and fifths are out (+0x1db = -1).

Wraps: slot 10 of the UEFA Cup is the shared empty function 0x5a8f60 (a `ret` of many vtables); its wrap acts only for a UEFA Cup object with 9 stages. Cup::load_from_disk 0x51fc00 makes the stage array of a UEFA Cup first (the constructor makes none), in a season game only. Cup::record_season_history 0x519a90 forwards a UEFA Cup with groups to its knockout stage, as 0x584ad0 does. Slot 17 0x9030e0 calls the Cup base 0x519890, which does nothing for a cup with stages (0x5198a3); the wrap clears +0x2c for that call, so the title holder (0x519974) and the staff records stay. A round index past the round table is not read there (0x5199e0).

## Intertoto Cup (U3)

The original (Intertoto_Cup, vtable 0x96bb8c) plays 5 rounds (0x633300): 40, 32 and 24 clubs in rounds 1 to 3 with 40, 12 and 8 new clubs of its 60 entrants. In this format the first three rounds are the cup: 40, 36 and 22 clubs with 40, 16 and 4 new clubs; the 11 winners of the third round (dates of the original: 17 and 24 July) go to the last 11 new slots of the UEFA Cup's Q2. The UEFA Cup's entry table grows to 153 slots (161 are allocated, 0x902b9d): the wrap of build_entrants 0x902b90 moves the seeded clubs after Q1 and the 20 seeded Q2 clubs up by 11 and makes the list +0x14 153 entries long (0x902dff makes it count entries). The hand-over (update_for_date 0x633d40) reads `rt_uefa_uic_back` and `rt_uefa_uic_winners` at 0x633deb and 0x633df3. With 153 slots the sizes are Q1 66, Q2 64 (20 seeded, 11 winners), R1 80.

## News (Q258)

The kind `uefa` of the season change writes one sentence for each change of format in a season with changing rules on, for example "The Champions League now has one group stage of 8 groups and a knockout round of 16."

## Rules report

`season_report.c` writes a `uefa` line for the Champions League and the UEFA Cup at each report: stage count, current stage, clubs of the group stages in stages 0 to 7 and after them, clubs and rounds of the knockout stage. The check compares the end line of each season with the "Rounds (clubs):" list of the row (kind `format`).

## Entry counts (Q369)

Up to 2008/09 the game keeps the original's place brackets by rank (`UefaSeeding::set_place_counts` 0x903cc0, `assign_champions_league_places` 0x904210, built from registers at 0x904219..0x9042ce) and the entrant counts the classes check (Champions League 71 at 0x584faa and the fill check 0x5851a1, UEFA Cup 118 seeded at 0x902cb0, Intertoto 60 at 0x6336db). The real counts (Champions League 72 to 76, UEFA Cup Q1 62 to 74, Intertoto 49 to 50) stay as expected differences of the rules report. Slice 23 owns the real access list from 2009/10 (Q369).

## Round names (Q370)

The real names of the new rounds, by season, with a source:

| Round | Seasons | Real name | Source |
|---|---|---|---|
| Champions League, first knockout round | 2003/04 on | Round of 16 | rules table row uefa-18 ("round of 16"); https://en.wikipedia.org/wiki/2003%E2%80%9304_UEFA_Champions_League_knockout_stage (section "Round of 16"); the same in 2009/10, https://en.wikipedia.org/wiki/2009%E2%80%9310_UEFA_Champions_League_knockout_phase |
| UEFA Cup, first knockout round | 2004/05 to 2008/09 | Round of 32 | https://en.wikipedia.org/wiki/2004%E2%80%9305_UEFA_Cup_knockout_stage (section "Round of 32") |
| UEFA Cup, second knockout round | 2004/05 to 2008/09 | Round of 16 | the same page (section "Round of 16") |
| UEFA Cup, first and second qualifying rounds | 2004/05 to 2008/09 | First qualifying round, Second qualifying round | rules table rows uefa-29 to uefa-61 (Q1, Q2) |

How the game shows them (`season_round_names.c`, `.h`, `season_round_names_game.c`; rows in `docs/research/round-names.tsv`, one per name with its source, made into `season_round_names_table.c` by `tools/seasons/make_round_names.py`. Slices 23 to 29 add rows). The round codes stay as they are, so no compare of a round code changes. The round name function 0x4b4850, cdecl (buffer, size, id, flag), writes the name of a code into the caller's buffer: flag 0 the long set (0x4b48d2, "Quarter Final"), flag 3 the plural set (0x4b6937, "Quarter Finals"), any other flag the abbreviated set (0x4b7a84, "Qtr Final", "2nd Rnd", "Qual.1 Leg 2"); code + 1 to + 5 name the replays, legs and decider. The runtime wraps it: in a season game, with the round's competition known, `season_round_name(comp_file, code, season, set)` gives the real name of the set, with the words of a replay, leg or decider added as the original's names write them, into the same buffer with its size. The season is the one of the formats: the competition's own season with changing rules on, the start year with changing rules off (Q232). A set or season with no row, a row that ends a name ("-" in the table), a replay or decider of a code from 160 (codes such as 251 and 252 give the original's box), the base game and `RECOMP_SEASON_CODE=off` get the original's text. The abbreviated rows are "Rnd of 16" and "Rnd of 32"; "Qual.1" and "Qual.2" stay.

The callers (23 calls in 12 functions) only put the text on a screen, into a news text or into a printout. None compares it or keeps it in a save: the competition screens 0x491380 (0x491570, 0x491a71), 0x492980 (0x492b3a), 0x495050 (0x495202), the league progress screen 0x49b990 (0x49babb), the cup tie news 0x4b0b80 (0x4b0e68), the group news 0x4b4590 (0x4b45d0, 0x4b46fd), the labels 0x4bfa30 (0x4bfb27, 0x4bfb4c, 0x4bfbac, 0x4bfbd1, 0x4bfc21, 0x4bfc46) and 0x4bffc0 (0x4c0016, 0x4c0030, 0x4c0083, 0x4c00b4), the live league table 0x7280e0 (0x7284ce, 0x7285e4), NewsItem::write_text 0x77bc80 (0x77fad9), the printouts 0x7d8120 (0x7d82e9) and SpaSecondB news 0x857e20 (0x857f0a). The name function knows no competition, so the functions that know theirs give it for their calls: the label 0x4bfa30 (a competition record), the competition screens 0x491380, 0x492980, 0x493ee0 (fixtures), 0x4947a0 (schedule), 0x495050, the printouts 0x7d8120, and the progress texts Comp::write_progress_text 0x48cab0 and EuropeanCup::write_progress_text 0x586fa0 (two of the 23 callers of 0x4bffc0). These keep the original's name, because they know no competition: the cup tie news 0x4b0b80, the group news 0x4b4590, the league progress screen 0x49b990, the live league table 0x7280e0, NewsItem::write_text 0x77bc80, and the callers of 0x4bffc0 that read the competition from screen parameters or records: comp_screens_draw_comp_screen 0x48fcd0, build_day_fixtures_screen 0x717db0, refresh_latest_scores_rows 0x724520, the stage menu 0x49d930 and the record texts 0x4a5690 and 0x4a7f50. The context goes back to the one before after each call, also when the call unwinds.

## Other limits

1. Dates: the group stages of both cups keep the original's weeks where they can (Champions League groups to 7 November). Update of 26 October 2026 (A18, lead 4): in a season game the Champions League groups of 2002/03 to 2023/24, the UEFA Cup groups of 2007/08 and the Europa League and Conference League groups of 2009/10 to 2023/24 take their real days (`season_uefa_days.c`, sources in `docs/research/2026-10-26-uefa-group-dates.tsv` and `2026-10-26-uefa-group-dates-el.tsv`); ctest season_uefa_days checks every running season against the FIFA days.
2. The UEFA Cup's stage objects are not freed when a game ends (the class's destructor 0x9024d0 knows no stages). The game's memory of that game is freed with it.
3. The Intertoto Cup after 2008 is slice 23's (with slice 16's interface to idle a competition).

## Boxes that are not from this slice

Two season databases stop with a box of the original in a rules report run, with the main build too: October 2003 at `EuroChamp::set_predicted_positions` 0x57c9f0 (comp_stats.cpp 1664, a club pointer of 0, February 2004) and September 2005 at world_cup.cpp 1438 (0x92f2b1, December 2005). The checks of this slice use November 2007, the own Summer 2006 database and the 3.9.68 data from 2002.
