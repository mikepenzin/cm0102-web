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
- 2019/20: the rows hold UEFA's calendar from before the COVID-19 break (UEFA stopped its matches from 15 March 2020), as the game has no break: Champions League round of 16 second legs 10 March, quarter-finals 7 and 14 April, semi-finals 28 April and 5 May, final 30 May; Europa League round of 16 second legs 19 March, quarter-finals 9 and 16 April, semi-finals 30 April and 7 May, final 27 May (the uefa.com calendar pages, which now show the August plan, and the Wikipedia footnotes "originally scheduled", O378 review). The tool keeps its rule for a two-leg round with no second leg or more than 35 days to it (left out with the later rounds of its season); no row meets it now.
- The game's season ends on 1 June (the end of season report and the season's history), so a final on 1 June or later is played on 31 May (Champions League 2014/15, 2016/17, 2018/19, 2022/23, 2023/24, 2026/27, Conference League 2022/23, 2026/27; the tool prints them). The 2018/19 Champions League final of 1 June had no winner at the end report (uefa_access_run 2018, load2018).
- A round with no row keeps the dates of its format: a season before the table (the Conference League before 2021/22, the formats before 2003/04 and 2004/05).
- A season after the table (2028/29 on): `season_uefa_ko_date` takes the last row of the competition and round and moves each date 364 days a year (the weekday stays), 7 days more each time it has come more than 3 days before the same day of the month, and a final after 31 May is played on 31 May (O378 review). ctest `season_uefa_dates` checks the rows, 2027/28 to 2060/61, the weekdays, the 31 May rule and the order of the rounds.

A draw on a day before the last group match day (a first knockout round drawn by UEFA before the groups end) stays. Cup::update_for_date 0x51f890 draws in a loop (0x51fa07..0x51faa6): the round in +0xac, the next one to draw, is drawn (Cup::draw_round 0x51c410 at 0x51fa7d) on the first update day on or after its draw date (0x51fa40..0x51fa6d), and the loop goes on to the next round while its date is past too. The knockout stage's table is made for the stage the cup starts after its group stage, so its first round is drawn on the first update of that stage, with the group winners known. A later round's draw is never before the round before it ends (the rule above), so the loop never draws a round before its clubs are known.

Rows with one source (the note says so): Europa League 2005/06 and 2006/07 round of 32 and round of 16 (Wikipedia only), and all of 2027/28 in each competition (no UEFA calendar yet: the 2026/27 dates moved to the same weekdays, the finals from UEFA Circular Letter 44/2025 through Wikipedia). Draw dates of many seasons come from Wikipedia only (the notes say which). Conflicts between the two sources are in the notes; the uefa.com date is used.

## The code

`season_uefa_dates.c`, `season_uefa_ko_dates`: after each table is built, the round's draw date (+0, +2), first leg (+3, +5) and days to the second leg (+0x22), from the row of its season. The dates are made with Date::Date 0x549f70 with weekday -1 (the real date; another weekday moves it, 0x54a0f2). The time (+6) and venue (+0x13) stay. The first build of a season's table writes a log line "knockout dates of N of M rounds from the dates table" (once per competition and season: rank_teams and the Schedule tab build the table again). These tables exist only in the season formats, so the base game keeps the original's dates (Q152).

## Q547

Updated (lead 4, 25 October 2026): the knockout dates are real per season wherever UEFA has published them, and later seasons take the last published season's dates moved to the same weekdays (as Q542). The league phase match days stay those of 2024/25.
