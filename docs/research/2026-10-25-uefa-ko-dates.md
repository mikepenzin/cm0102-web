# The UEFA knockout dates of each season (O259, 25 October 2026)

## The fault

The knockout round tables that 0.6.0 builds had the same dates in every season:

- UEFA Cup, Europa League and Conference League (`season_uefa_cup.c`, `ko_table`, from 2004/05): the 2004/05 dates. The round of 32 on 17 February, the round of 16 on 10 March, the quarter-finals on 7 April, the semi-finals on 28 April and 5 May, the final on 18 May.
- Champions League with a round of 16 (`season_uefa.c`, `ko_table`, 2003/04 to 2023/24): the round of 16 drawn on 22 November with first legs on 20 February. The quarter-finals, semi-finals and final kept the original's 2001/02 dates (create_round_table stage 12, 0x58437f..0x584409).
- Champions League knockout phase (`league_ko_table`, from 2024/25): the 2024/25 play-off and round of 16 dates (Q547), the original's dates after them.

The 2004/05 dates fall on the Russian Cup's semi-final and final days (RusCup::create_round_table 0x7e9530: semi-finals on day 124, the final on day 137). In a 2008 game (September 2008 database, Select All, changing rules on) the Europa League semi-final Juventus v Zenit went onto day 124 of 2010, the day of the cup semi-final Zenit v Sibir Novosibirsk. The original's clash rule moved the cup match (FIXTURE_MANAGER::add_fixture 0x5aa680, find_fixture_clashes 0x5ad780, find_new_fixture_date 0x5ab510, move_nearby_club_fixtures 0x5ae9d0), first to day 131, then day 138 after the second leg, then day 145 after the final Zenit v Marseille on day 139. The cup final went to day 152, past the 1 June start of the next edition (dae353f9, rus-cup-june-lead4.md). The original UEFA Cup dates (0x9028e1..0x902b16: semi-finals from 5 April, final 16 May) miss the Russian Cup's.

## The table

`docs/research/rules-0.6.0/uefa-ko-dates.tsv`: one row per competition (cl, uc, ecl), season and round (po, r16, qf, sf, final), with the draw, the first day of the first leg and of the second leg, a uefa.com source and a Wikipedia source. Champions League 2003/04 to 2027/28 (104 rows), UEFA Cup and Europa League 2004/05 to 2027/28 (120), Conference League 2021/22 to 2027/28 (35). `tools/seasons/make_uefa_dates.py` writes `src/recomp_rt/win/season_uefa_dates_table.c`; ctest `season_uefa_dates_table` checks that the file is the table's.

Rules of the tool:

- The final's draw is the semi-finals' draw.
- The game draws each round from the winners of the round before (Cup::draw_round 0x51c410 on the round's draw date). UEFA draws later rounds with an earlier one (the quarter-finals and semi-finals together; from 2024/25 every round with the round of 16). Such a draw is put on the day after the second leg of the round before (118 rows; the tool prints them).
- 2019/20: the quarter-finals and semi-finals were single matches in August 2020, and the Europa League round of 16 second legs were in August. The game plays two legs a week or two apart, so such a round (no second leg, or more than 35 days to it) is left out with the later rounds of its season. Those rounds keep the format's dates.
- A round with no row keeps the dates of its format.

Rows with one source (the note says so): Europa League 2005/06 and 2006/07 round of 32 and round of 16 (Wikipedia only), and all of 2027/28 in each competition (no UEFA calendar yet: the 2026/27 dates moved to the same weekdays, the finals from UEFA Circular Letter 44/2025 through Wikipedia). Draw dates of many seasons come from Wikipedia only (the notes say which). Conflicts between the two sources are in the notes; the uefa.com date is used.

## The code

`season_uefa_dates.c`, `season_uefa_ko_dates`: after each table is built, the round's draw date (+0, +2), first leg (+3, +5) and days to the second leg (+0x22), from the row of its season. The dates are made with Date::Date 0x549f70 with weekday -1 (the real date; another weekday moves it, 0x54a0f2). The time (+6) and venue (+0x13) stay. Each table writes a log line "knockout dates of N of M rounds from the dates table". These tables exist only in the season formats, so the base game keeps the original's dates (Q152).

## Q547

Updated (lead 4, 25 October 2026): the knockout dates are real per season wherever UEFA has published them, and later seasons take the last published season's dates moved to the same weekdays (as Q542). The league phase match days stay those of 2024/25.
