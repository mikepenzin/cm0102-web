#!/usr/bin/env python3
"""A league of the size engine plays its real size for one season (0.7.2 league sizes, slice 2; spec
docs/specs/2026-11-13-league-sizes-0.7.2-spec.md, Testing seam 2, D4 and D5). Made from swe_prm_run.py (issue 106).

Usage:
    python3 tests/seasons/league_size_run.py <cm0102_recomp> <game folder> <file> [--year <season>]
        [--last <season> | --to <season>] [--nations all|<name,name,...>|all,-<name>,...] [--keep <folder>] [--reuse]
        [--timeout <seconds>] [--extra conference,regional,c2] [--save-in-split]
    python3 tests/seasons/league_size_run.py --list
    python3 tests/seasons/league_size_run.py --self-test

The run starts a new game in a season (default: the year of the file in ROWS) with all nations (--nations as in
tests/seasons/rules_report_check.py: a list keeps England; "all,-<name>" leaves a nation out), changing rules on,
and plays to the end of --last (default the start year: RECOMP_SEASON_JUMP, the rules report of
tests/seasons/rules_report_check.py). Each edition (+0x40) is checked against its own row of the encoded rules
table (docs/research/2026-10-18-league-rules-encoded.tsv, slice 1): a calendar-year league plays edition Y by the
row of season Y - 1 (its real season is Y), any other league by the row of season Y. The row gives the clubs n and
the meetings m; the rounds are R = m x (n' - 1), n' the count rounded up to even. RECOMP_SIZE_LOG=1
(src/recomp_rt/win/season_sizes.c) writes the lines that the checks read. The checks:
    report   every row of the file in the rules report (start and end of each season) holds the n clubs and m
             meetings of its edition, and m x (n - 1) matches a club once they are set;
    table    a table of n rows when n is more than the class allocates (league_sizes.py HELD);
    dates    a template of R dates for n clubs: the dates ascend, each new date ("+") is a Wednesday or an earlier
             start in steps of 7 days, none is in the largest gap of the class's template (the winter break) or in a
             gap over 14 days, the last date is the class's last date, and with fewer rounds than the class's
             (D5) no date is new and the first date is the class's first;
    played   at the start of the next edition (League::free_table 0x687970) every club of a table of n clubs played
             m x (n - 1) matches, the matches per club (+0xa7);
    expect   the rules report check lists no clubs difference of the file (the first season);
    box      no message box.
A league whose row has a split (slice 7, D7; src/recomp_rt/win/season_split.c) plays its table to N matches a club,
then sections of the row: its template holds R dates and the most rounds of a section, the matches per club are N
and those of the largest section, every club of a finished table played N and the matches of its own section, and
a split line names the row's sections and meetings, the dates of the sections are the last dates of the template
line, and at the end of the table (a split end line, in table order) the places after match N are each place once,
each section's clubs stand together above the next section's, and each club played N and its own section's matches. --save-in-split plays to the first split, saves after the first section date,
loads that save in a new program and plays on (leg0 and leg1 in the work folder): the loaded league goes on after
its split and its table finishes.
A message box ends the run at once (stop_on_box) and fails it. A run takes about 20 to 40 minutes a season. A two-year league ends its table after the end report of its season:
use --last <year + 1> for its played check. --reuse checks the logs in --keep again. Exit code 1 on a failure.
"""
import datetime
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
sys.path.insert(0, os.path.join(ROOT, "tools", "recomp"))
sys.path.insert(0, os.path.join(ROOT, "tools", "seasons"))
import league_sizes  # noqa: E402

ENCODED = os.path.join(ROOT, "docs", "research", "2026-10-18-league-rules-encoded.tsv")

# The runs of each slice: file: (start year, last season, what it shows). Slice 2: the pilots.
ROWS = {
    "nor_prm": (2009, 2010, "Eliteserien, 16 clubs (the class holds 14): a calendar year with a summer break"),
    "nor_prm@2008": (2008, 2012, "Eliteserien 14 in 2008, 16 from 2009 (a forum report: 14 still in 2012)"),
    "por_second": (2014, 2015, "Segunda Liga, 24 clubs (the class holds 18): a template of 46 dates"),
    "por_second@2008": (2008, 2009, "Segunda Liga, 16 clubs: fewer dates, the season ends on its usual day (D5)"),
    "swe_prm": (2025, 2026, "Allsvenskan, 16 clubs (issue 106, also swe_prm_run.py)"),
    # Slice 12: Russia and Turkey.
    "rus_first": (2005, 2006, "Russian First Division, 22 clubs (the class holds 18 and takes at most 18)"),
    "rus_first@2011": (2011, 2012, "Russian First Division 2011-12: 20 clubs, 38 rounds, the first stage only (the split of "
                                   "row 2010, played in 2012, is a gap: a season that spans two game years)"),
    "rus_first@2012": (2012, 2013, "Russian First Division, 17 clubs in 2012/13 (row of a two-year season)"),
    "tur_first": (2020, 2021, "Süper Lig, 21 clubs in 2020/21 (the class holds 18), 42 rounds"),
    "tur_first@2021": (2021, 2022, "Süper Lig, 20 clubs in 2021/22"),
    "tur_second": (2023, 2024, "Turkish second tier, 18 clubs in 2023/24 (the class holds 20): fewer dates"),
    # Slice 9: Italy. All four classes are sequential (flag 0x40): the template holds exactly the rounds.
    "ita_ser_a": (2005, 2006, "Serie A, 20 clubs (the class holds 18): 38 dates of a template of 34"),
    "ita_ser_b@2003": (2003, 2004, "Serie B, 24 clubs (the class holds 20): 46 dates of a template of 38"),
    "ita_ser_b": (2018, 2019, "Serie B, 19 clubs: 38 dates, a rest round each round"),
    "ita_ser_c1a": (2004, 2005, "Serie C1/A, 19 clubs (the class holds 18): 38 dates of a template of 34"),
    "ita_ser_c1a@2012": (2012, 2013, "Serie C1/A 17 and C1/B 16: 34 and 30 dates, the play-out rows of the count"),
    # Slice 8: England and France.
    "eng_conf": (2010, 2011, "Conference, 24 clubs (the class holds 22): a template of 46 dates"),
    "fra_third@2010": (2010, 2011, "National, 21 clubs (the class holds 20): a mirrored template of 42 dates"),
    "fra_third": (2014, 2015, "National, 18 clubs: a mirrored template of 34 dates, the season ends on its usual day "
                              "(D5)"),
    # Slice 7, the split engine (D7): the table to N matches, then sections of the row.
    "nir_prm": (2010, 2011, "IFA Premiership, 12 clubs (the class holds 10): 33 rounds, then sections of 6 and 6"),
    "wel_first": (2012, 2013, "Welsh Premier, 12 clubs: 22 rounds, then sections of 6 and 6 that meet twice"),
    "wel_first@2026": (2026, 2027, "Welsh Premier, 16 clubs: 30 rounds, then sections of 6, 4 and 6"),
    # Open item 250: the Welsh Premier has no chain (WelFirst::start_new_season 0x925cf0); its row's relegation places go
    # to no division and the step brings it to the count of its next row.
    "wel_first@2009": (2009, 2010, "Welsh Premier, 18 clubs, then 12 with a split: 6 down to no division"),
    "wel_first@2025": (2025, 2026, "Welsh Premier, 12 clubs with a split, then 16: 2 down, 6 up from no division"),
    # Slice 17 (D11): the class plays one meeting on 27 dates and a play-off of the top 4. Rows are keyed one year
    # before the real season (row 2003 is the 2004 Série A).
    "bra_nat_first@2003": (2003, 2004, "Série A, 24 clubs in two meetings: 46 dates, no play-off"),
    "bra_nat_first": (2010, 2011, "Série A, 20 clubs in two meetings: 38 dates, no play-off"),
    "bra_nat_second": (2006, 2007, "Série B, 20 clubs in two meetings: 38 dates, no play-off"),
    # Slice 10: the Netherlands, Belgium, Denmark and Finland. Periods scale to the rounds (D6): a periods line each
    # edition. The calendar-year leagues (fin_prm) play edition Y by the row of season Y - 1.
    "hol_first": (2005, 2006, "Eerste Divisie, 20 clubs (the class holds 18): 38 rounds, its four periods scaled"),
    "hol_first@2010": (2010, 2011, "Eerste Divisie, 18 clubs: the class's 34 rounds and periods"),
    "bel_second": (2008, 2009, "Second Division, 19 clubs: 38 rounds, no period winners, an odd count with periods"),
    "bel_second@2016": (2016, 2017, "First Division B, 8 clubs that meet four times: 28 rounds, periods scaled"),
    "bel_second@2022": (2022, 2023, "Challenger Pro League, 12 clubs: 22 rounds, then 6 and 6; the periods take no "
                                     "result after the split"),
    "bel_first": (2010, 2011, "Pro League, 16 clubs: 30 rounds, then Play-off I (points halved), Play-off II in two "
                              "groups of 4 from 0 points, the 15th v 16th in five matches, the 15th starting on 3"),
    "bel_first@2016": (2016, 2017, "First Division A, 16 clubs: 30 rounds, then Play-off I, Play-off II in groups "
                                   "of 4 and 5 (with the 15th) from 0 points; the 16th plays no more"),
    "den_prm": (2017, 2018, "Superliga, 14 clubs in two meetings (the class holds 12 in three): 26 rounds, then 6 and 8"),
    "fin_prm": (2005, 2006, "Veikkausliiga, 14 clubs in two meetings (the class holds 12 in three), 13 in 2006"),
    "fin_prm@2002": (2002, 2003, "Veikkausliiga, 12 clubs, then 14 in 2003: two more clubs of the Ykkonen go up"),
    "fin_prm@2010": (2010, 2011, "Veikkausliiga, 14 clubs, then 12 in 2011: Tampere United and AC Oulu lose their licence"),
    "fin_prm@2019": (2019, 2020, "Veikkausliiga, 12 clubs: 22 rounds, then 6 and 6 that meet once; 2020 one table"),
    # Slice 11: Croatia and Greece (sequential classes, flag 0x40).
    "cro_a2a": (2020, 2021, "Druga HNL, 18 clubs (the class holds 16): 34 rounds, tie-breaks of up to 9 pairs"),
    "cro_a2a@2012": (2012, 2013, "Druga HNL 16 in 2012/13, 12 in three meetings in 2013/14: the regroup of the "
                                 "season change gives the North 12 and the South its 16"),
    "cro_a1": (2010, 2011, "Prva HNL, 16 clubs in two meetings: the class's count"),
    "cro_a1@2013": (2013, 2014, "Prva HNL, 10 clubs in four meetings: 36 rounds on a class of 30 dates"),
    "gre_prm": (2005, 2006, "Super League, 16 clubs (the class holds 14): 30 rounds"),
    "gre_prm@2019": (2019, 2020, "Super League, 14 clubs: 26 rounds, then sections of 6 (2 meetings) and 8 (1)"),
    "gre_second": (2010, 2011, "Beta Ethniki, 18 clubs (the class holds 14): 34 rounds"),
    "gre_second@2014": (2014, 2015, "Football League, 13 clubs (two regional groups in real life, D16): one table "
                                    "of 26 rounds with a rest round"),
    # Slice 11 gap rows 38 to 41 (lead-notes/sizes-gaps.md).
    "cro_a2a@2002": (2002, 2003, "Druga HNL North 12 in 2002/03: 22 rounds, then groups of 6 twice, points kept; "
                                 "the regroup of 2003 gives the North and the South 12 each, by latitude (row 38)"),
    "cro_a2b@2002": (2002, 2003, "Druga HNL South, the same run (--reuse --keep of cro_a2a@2002)"),
    "cro_a2a@2005": (2005, 2006, "Druga HNL North 12 in 2005/06, then one group of 16 in 2006/07, by results; the "
                                 "South does not go up after 2006/07 (row 40)"),
    "cro_a2a@2009": (2009, 2010, "Druga HNL 14 in 2009/10, 16 in 2010/11 by results, and no message box (slice 18 "
                                 "saw cro_a2b 446 at this change)"),
    "cro_a2a@2006": (2006, 2007, "a new game in 2006/07: the South is not held, so its clubs start in A Lower Division "
                                 "and it makes no table and no fixtures (open item 255)"),
    "cro_a2a@2010": (2010, 2011, "a new game in 2010/11 (the lead's 2010 start of open item 255): no South table, the "
                                 "Druga HNL by its row"),
    "cro_a2a@2001": (2001, 2011, "a 2001 start to 2011/12: the groups of 16 and 12, one group from 2006/07, the South "
                                 "idle, and no message box at the 2010/11 change (open item 255, cro_a2b 446)"),
    "gre_prm@2024": (2024, 2025, "Super League 14: 26 rounds, then 4+4+6 twice, only the 5th to 8th halve (row 39)"),
    "gre_second@2023": (2023, 2025, "Super League 2 groups: 12 in 2023/24 (5+6+1), 10 in 2024/25 (5+5), each group "
                                    "splits, points halved (row 41)"),
    "gre_second@2025": (2025, 2026, "Super League 2 groups of 10 in 2025/26 (4+6), then one group of 16 (row 41)"),
    # Slice 14 (D8): calendar-year leagues, edition Y plays row Y - 1. J1 plays halves and the final only in the rows
    # of two stages; its table holds both halves (season_sizes_asia.c).
    "jap_j1": (2005, 2006, "J1, 18 clubs in one table: 34 dates, no halves and no final"),
    "jap_j1@2015": (2015, 2017, "J1, 18 clubs in two halves of 17 rounds and the final, then one table in 2017"),
    "jap_j1@2024": (2024, 2025, "J1, 20 clubs in one table: 38 dates"),
    "jap_j2": (2010, 2011, "J2, 19 clubs in two meetings: 38 dates (the class plays 4 meetings of 12)"),
    "kor_league": (2013, 2014, "K League, 14 clubs: 26 rounds, then sections of 7 and 7 that meet twice"),
    "kor_league@2003": (2003, 2004, "K League, 12 clubs in four meetings: 44 dates, 17 weeks earlier at most, "
                                    "then two stages as one table in two meetings, 12 of the row's 13 (no club to add, D12)"),
    # Gap 45: row 2025 (2026) splits after 33 into 6 and 6 that meet once more, 38 in all (2026 K League 1 page).
    # Gap 47: the K League alone sends its row's places to no division (edition 2024 plays row 2023: 1 down), and the
    # step takes one Korean club with no division back up to 12.
    "kor_league@2024": (2024, 2025, "K League 1, 12 clubs: 1 down to no division, 1 up from no division"),
    "kor_league@2026": (2026, 2026, "K League 1, 12 clubs: 33 rounds, then sections of 6 and 6 that meet once more"),
    # Slice 13: the NIFL Championship (the class holds 10 in 4 meetings on 36 dates) and the League of Ireland First
    # Division (the class takes 9 to 12; rows are keyed one year before the real season, row 2013 is the 2014 season).
    "nir_first": (2016, 2017, "NIFL Championship, 12 clubs: 22 rounds, then sections of 6 and 6 that meet twice"),
    "nir_first@2022": (2022, 2023, "NIFL Championship, 12 clubs: 33 rounds, then sections of 6 and 6 once more"),
    "nir_first@2020": (2020, 2021, "NIFL Championship, 12 clubs: the announced 22 rounds of 2020/21, no split (gap 42)"),
    "ire_first": (2013, 2014, "League of Ireland First Division, 8 clubs in four meetings: 28 dates (D5)"),
    "ire_first@2017": (2017, 2018, "League of Ireland 2017: 3 down, the champion up, no play-off; 8 to 10 (item 251)"),
    "ire_first@2008": (2008, 2009, "League of Ireland 2008: 3 down, the champion up, no play-off; 10 to 12, Premier "
                       "12 to 10 (item 251)"),
    "ire_first@2001": (2001, 2002, "League of Ireland from a 2001 start: 12 and 9, then 3 down and 1 up with the "
                       "play-off, and the size step fills the First Division to the row's 12 from Irish clubs with no "
                       "division (item 260, D12)"),
    "ire_first@2002": (2002, 2005, "League of Ireland from a 2002 start: the calendar move (gap row 2) plays edition 2002 "
                       "as the short season (rows 41 and 42), 2003 by rows 41b and 42b, 2004 by row 2003; 12 and 10 "
                       "in 2005"),
    "ire_first@2003": (2003, 2005, "League of Ireland from a 2003 start: 10 and 12 at the start (item 260), 2004 1 down "
                       "and 3 up with no play-off, 12 and 10 in 2005"),
    "ire_first@2004": (2004, 2005, "League of Ireland from a 2004 start: 10 and 12 at the start, the list of row 2003 "
                       "(item 260), then 12 and 10 in 2005"),
    "nir_first@2001": (2001, 2009, "Irish League from a 2001 start (items 271, 272): the First Division 10, 8, then 12 from "
                                   "2003/04 with clubs of the lower division, 17 in 2008/09 and 14 in 2009/10; the "
                                   "places of each row"),
    "nir_prm@2001": (2001, 2009, "Irish League from a 2001 start, the same run (--reuse --keep of nir_first@2001): "
                                 "no club down in 2001/02, the play-off only in 2002/03, 2 down and no play-off in "
                                 "2007/08"),
    "nir_first@2018": (2018, 2019, "Irish League from a 2018 start (item 288): every section match of 2018/19 is played, "
                                   "so both leagues finish and start 2019/20"),
    "ire_first@2010": (2010, 2012, "League of Ireland from a 2010 start: 2011 0 down and 2 up plus the play-off, each "
                       "club on its own (item 251b); 12 Premier clubs in 2012, and the size step leaves the First Division its "
                       "row's 8 (item 260)"),
    # Slice 15: the Argentine Primera. Edition Y plays row Y (season_sizes_edition): row 2014 is the 2015 season, row
    # 2015 the 2016 tournament, row 2016 the 2016/17 season, row 2024 the 2025 season.
    "arg_prm@2005": (2005, 2006, "Primera 2005/06 (row 2005): the promocion of the 17th and 18th of the averages "
                                 "against the second division's 3rd and 4th, two legs"),
    "arg_prm@2010": (2010, 2011, "Primera 2010/11 (row 2010): the promocion, then row 2011's"),
    "arg_prm@2013": (2013, 2014, "Primera 2013/14 (row 2013, 20 in two halves), then 30 in 2014/15: the size step "
                                 "takes 10 clubs of the second division, which takes clubs of the third"),
    "arg_prm": (2014, 2015, "Primera 2014/15 (row 2014, the 2015 season): one table of 30, 29 rounds and the derby "
                            "round, then row 2015"),
    "arg_prm@2015": (2015, 2016, "Primera 2015/16 (row 2015, the 2016 season): one tournament of two zones of 15, each "
                                 "club's rival twice"),
    "arg_prm@2016": (2016, 2017, "Primera 2016/17 (row 2016): one table of 30 and the derby round, then 28 (the step "
                                 "by the averages)"),
    "arg_prm@2024": (2024, 2025, "Primera 2024/25 (row 2024, the 2025 season): Apertura and Clausura of two zones of "
                                 "15 with the rival and drawn inter-zone rounds"),
    "arg_prm@2025": (2025, 2026, "Primera 2025/26 (row 2025, the 2026 season): the clubs and zones of its list"),
    "arg_prm@2026": (2026, 2027, "Primera 2026/27 (row 2026, the 2027 season): one table of 30 and the derby round in "
                                 "two halves, each with a knockout of 16"),
    # Slice 16 (D10): the Argentine second division. The class plays 25 clubs: an Apertura of one meeting, then three
    # Clausura zones of 8 and a Reducido. Every row but 2001/02 plays one table of its count and meetings, on the dates
    # of both class halves (25 and 14).
    "arg_second": (2010, 2011, "Primera B Nacional 2010/11, 20 clubs in two meetings: 38 dates, no class stages"),
    "arg_second@2016": (2016, 2017, "Primera B Nacional 2016/17, 23 clubs in two meetings: 46 dates, then 25 in one "
                                    "meeting (the size step)"),
    # Item 286: edition Y plays row Y, as the Primera (row 2014 "2015", row 2020 "2021").
    "arg_second@2015": (2015, 2016, "Primera B Nacional row 2015 (the 2016 season): 22 clubs in one meeting, then 23"),
    "arg_second@2021": (2021, 2022, "Primera Nacional row 2021 (the 2022 season): 37 clubs in one meeting, then 37 in "
                                    "two zones as one table"),
    "arg_second@2023": (2023, 2024, "Primera Nacional row 2023 (the 2024 season): 38 clubs in two zones as one table "
                                    "of one meeting, then 36"),
    # Slice 17 (D11): the class plays one meeting on 27 dates and a play-off of the top 4. Rows are keyed one year
    # before the real season (row 2003 is the 2004 Série A).
    "bra_nat_first@2003": (2003, 2004, "Série A, 24 clubs in two meetings: 46 dates, no play-off"),
    "bra_nat_first": (2010, 2011, "Série A, 20 clubs in two meetings: 38 dates, no play-off"),
    "bra_nat_second": (2006, 2007, "Série B, 20 clubs in two meetings: 38 dates, no play-off"),
    # Gap rows 56 and 57: the Série B play-offs of the rules table (season_sizes_bra.c).
    "bra_nat_second@2003": (2003, 2004, "Série B 2003 and 2004, 24 clubs: a play-off of the top 8, the two finalists "
                                        "go up"),
    "bra_nat_second@2025": (2025, 2026, "Série B 2025 (4 up), then 2026: 2 up and the ties 3rd v 6th and 4th v 5th, "
                                        "both winners go up"),
}

# Slice 10 (D6, src/recomp_rt/win/season_sizes_periods.c): leagues whose class plays periods.
PERIOD_FILES = {"hol_first": 4, "bel_second": 3}
# Files whose table holds both halves of a row of halves, so it plays twice the row's meetings (slice 14,
# src/recomp_rt/win/season_sizes_asia.c): J1, and the K League stages as one table.
HALVES_IN_TABLE = {"jap_j1", "kor_league"}

# The extra divisions of the new game screen that a file needs (rules_report_check.EXTRA): the game plays the
# Conference only when the screen selects it.
EXTRA = {"eng_conf": ["conference"]}


class RowSeason(int):
    """A season of a file whose rules hold more than one row (the Irish 2002 rows 41 and 41b, gap row 2): n picks
    the row of that season by its order in the table, as season_rules_row_of (season_rules.c) finds it."""

    def __new__(cls, season, n):
        o = int.__new__(cls, season)
        o.n = n
        return o


_ENC = []


def encoded_rows():
    """The rows of the encoded table, in file order."""
    if not _ENC:
        with open(ENCODED, encoding="utf-8") as f:
            cols = f.readline().rstrip("\n").split("\t")
            _ENC.extend(dict(zip(cols, line.rstrip("\n").split("\t"))) for line in f)
    return _ENC


def encoded_pick(file, season):
    """The encoded row of a file in a season by the after-table rule (the last season before, its first row with
    clubs), or for a RowSeason the n-th row of that season. None when there is none."""
    best, seen = None, {}
    for row in encoded_rows():
        if row.get("comp_file") != file or not row.get("season", "").isdigit() or not row.get("clubs", "").isdigit():
            continue
        s = int(row["season"])
        k = seen.get(s, 0)
        seen[s] = k + 1
        want = season.n if isinstance(season, RowSeason) and s == season else 0
        if s <= season and k == want and (best is None or s > best[0]):
            best = (s, row)
    return best[1] if best else None


def encoded_row(file, season):
    """(clubs, meetings) of the encoded row of a file in a season, by the after-table rule (the last row before)."""
    row = encoded_pick(file, season)
    if not row:
        return (0, 0)
    m = int(row["meetings"]) if row.get("meetings", "").isdigit() else 0
    if file in HALVES_IN_TABLE and row.get("halves") == "1":
        m *= 2
    # Slice 16 (season_sizes_arg_second.c form_of): the second division plays a row of zones as one table of one
    # meeting, a row of halves as one table of twice the meetings; 2001/02 (25 clubs in halves) keeps the class.
    if file == "arg_second" and "+" in row.get("zones", ""):     # zone sizes, "19+19" (item 291: two meetings)
        m = 2
    elif file == "arg_second" and row.get("halves") == "1" and row["clubs"] != "25":
        m *= 2
    return (int(row["clubs"]), m)


def rounds_of(clubs, meetings):
    return meetings * (clubs + (clubs & 1) - 1)


def encoded_split(file, season):
    """The split of the encoded row of a file in a season (the after-table rule): (after, [(size, meetings)]) or
    None for one table or a split the engine does not play (unknown section meetings)."""
    row = encoded_pick(file, season)
    best = (0, row.get("split", "-"), row.get("split_meet", "-")) if row else None
    if not best or not best[1].startswith("after "):
        return None
    head, sizes = best[1].split(",")[0][len("after "):].split(": ")
    meet = best[2].split("+")
    if not all(m.isdigit() for m in meet):
        return None
    return int(head), list(zip(map(int, sizes.split("+")), map(int, meet)))


def split_numbers(sp):
    """(rounds after the split: the most of a section, the fewest and the most matches of a club after it)."""
    rounds = max(m * (n + (n & 1) - 1) for n, m in sp)
    matches = [m * (n - 1) for n, m in sp]
    return rounds, min(matches), max(matches)


def split_dates(log, file):
    """{edition: [dates]} of the split dates lines of a file (the probe at the split, season_sizes.c)."""
    out = {}
    tag = f"recomp: league sizes: {file} split dates "
    for line in log.split("\n"):
        if line.startswith(tag):
            head, _, rest = line[len(tag):].partition(": ")
            if head.isdigit():
                out[int(head)] = [d for d, _a in parse_dates(rest.split()[1:])]
    return out


def split_ends(log, file):
    """{edition: [(place after match N, played)]} in table order, of the split end lines of a file."""
    out = {}
    tag = f"recomp: league sizes: {file} split end "
    for line in log.split("\n"):
        if line.startswith(tag):
            head, _, rest = line[len(tag):].partition(": ")
            try:
                out[int(head)] = [tuple(int(x) for x in w.split(":")) for w in rest.split()]
            except ValueError:
                pass
    return out


# Gap 35: splits whose later sections are groups of places that are not next to each other (season_split.c GROUPS):
# {(file, first season, last season): the section of each place after match N, 0-based}. The Superliga 2016/17 to
# 2019/20: the top 6, then the groups of 7th, 10th, 11th, 14th and of 8th, 9th, 12th, 13th (Wikipedia 2016-17 to
# 2019-20_Danish_Superliga).
GROUPS = {("den_prm", 2016, 2019): [0] * 6 + [1, 2, 2, 1, 1, 2, 2, 1],
          # Gap 34: the Pro League's Play-off II groups of the 7th, 9th, 12th, 14th and the 8th, 10th, 11th, 13th
          # (2016/17 to 2018/19 with the 15th), then the 15th and 16th (en.wikipedia.org 2009-10 to
          # 2015-16_Belgian_Pro_League, 2016-17 to 2018-19_Belgian_First_Division_A).
          ("bel_first", 2009, 2015): [0] * 6 + [1, 2, 1, 2, 2, 1, 2, 1, 3, 3],
          ("bel_first", 2016, 2018): [0] * 6 + [1] * 10}
# Gap 34: the points of each section at the split (season_split.c GROUPS): h halved, k kept, z from 0, b from 0 with
# 3 for the section's best placed club (the 15th of the relegation play-off 2010/11 to 2014/15).
SPLIT_POINTS = {("bel_first", 2009, 2009): "hzzk", ("bel_first", 2010, 2014): "hzzb",
                ("bel_first", 2015, 2015): "hzzk", ("bel_first", 2016, 2018): "hk"}


def split_points_faults(log, file, season_of):
    """Gap 34: the line "<file> split <ed>: points at the split <rules>, <points of place 1> ..." of each split of a
    file with SPLIT_POINTS: its rules, and 0 points in a 'z' section, 3 and 0 in a 'b' section."""
    fails, tag = [], f"recomp: league sizes: {file} split "
    for line in log.split("\n"):
        if not line.startswith(tag) or ": points at the split " not in line:
            continue
        head, _, rest = line[len(tag):].partition(": points at the split ")
        if not head.isdigit():
            continue
        ed = int(head)
        season = season_of(ed)
        want = next((v for (f, a, b), v in SPLIT_POINTS.items() if f == file and a <= season <= b), None)
        rules, _, pts = rest.partition(",")
        if want is None:
            continue
        if rules != want:
            fails.append(f"{file} {ed}: points rules {rules} at the split, want {want}")
            continue
        sec, pts = groups_of(file, season), [int(x) for x in pts.split()]
        if not sec or len(pts) != len(sec):
            fails.append(f"{file} {ed}: points of {len(pts)} places at the split, want {len(sec or [])}")
            continue
        for k, rule in enumerate(rules):
            got = [pts[p] for p in range(len(sec)) if sec[p] == k]
            if rule == "z" and any(got):
                fails.append(f"{file} {ed}: section {k + 1} starts on {got} points, want 0")
            if rule == "b" and got != [3] + [0] * (len(got) - 1):
                fails.append(f"{file} {ed}: section {k + 1} starts on {got} points, want 3 for its best, 0 others")
    return fails


def groups_of(file, season):
    for (f, first, last), sec in GROUPS.items():
        if f == file and first <= season <= last:
            return sec
    return None


def end_faults(name, rows, after, sp, groups=None):
    """The faults of a table at its end after a split: each section holds its count of clubs by their places after
    match N, the table keeps each section together and above the next, and each club played N and its own section's
    matches. The places after match N need not be each place once at the end: League::sort_table 0x6827d0 writes
    each row's place again at index played - 1 at every sort (0x682ada..0x682b0f), so a club that has not yet played
    its first section match (a later kick-off on the day, or a postponed match) gets its place within its section
    at match N again. That place stays in its section, as the order sorts by section first (season_split.c)."""
    fails = []
    places = [p for p, _x in rows]
    total = sum(n for n, _m in sp)
    if len(places) != total or any(not 0 <= p < total for p in places):
        return [f"{name}: the places after match {after} are not places of a table of {total}: {places}"]
    bounds, top = [], 0
    for n, m in sp:
        bounds.append((top, top + n, after + m * (n - 1)))
        top += n
    if groups and len(groups) == total:
        secs = [groups[p] for p in places]
    else:
        secs = [next(k for k, (a, b, _w) in enumerate(bounds) if a <= p < b) for p in places]
    counts = [secs.count(k) for k in range(len(sp))]
    if counts != [n for n, _m in sp]:
        return [f"{name}: the sections hold {counts} clubs by the places after match {after}, want "
                f"{[n for n, _m in sp]}: {places}"]
    if secs != sorted(secs):
        fails.append(f"{name}: the table does not keep the sections in order: {secs}")
    for (p, played), k in zip(rows, secs):
        if played != bounds[k][2]:
            fails.append(f"{name}: the club of place {p + 1} after match {after} played {played}, want {bounds[k][2]}")
    return fails


def splits(log, file):
    """{edition: (after, [sizes], [meetings])} of the split lines of a file, and the editions loaded after one."""
    out, loaded = {}, set()
    tag = f"recomp: league sizes: {file} split "
    for line in log.split("\n"):
        if not line.startswith(tag):
            continue
        head, _, rest = line[len(tag):].partition(": ")
        if not head.isdigit():
            continue
        if rest.startswith("after "):
            p = rest.replace(",", "").split()
            try:
                out[int(head)] = (int(p[1]), [int(x) for x in p[3].split("+")], [int(x) for x in p[5].split("+")])
            except (ValueError, IndexError):
                pass
        elif rest.startswith("loaded"):
            loaded.add(int(head))
    return out, loaded


def split_points_logged(log, file):
    """{edition: points} of the split lines of a file ("kept", "halved", "kept+halved+kept")."""
    out = {}
    tag = f"recomp: league sizes: {file} split "
    for line in log.split("\n"):
        if line.startswith(tag):
            head, _, rest = line[len(tag):].partition(": ")
            m = re.search(r", points (\S+) \(", rest)
            if head.isdigit() and rest.startswith("after ") and m:
                out[int(head)] = m.group(1)
    return out


def encoded_form(file, season):
    """(clubs, meetings, zones, extra, rounds, halves) of the encoded row of a file in a season, by the after-table
    rule; zones is the list of zone counts ([] for none), rounds the encoded rounds (0 when not given)."""
    row = encoded_pick(file, season)
    if not row:
        return (0, 0, [], 0, 0, 0)
    num = lambda k: int(row[k]) if row.get(k, "").isdigit() else 0  # noqa: E731
    zones = [int(z) for z in row.get("zones", "-").split("+") if z.isdigit()]
    return (num("clubs"), num("meetings"), zones, num("extra"), num("rounds"), num("halves"))


def want_counts(file, season, meetings=0):
    """(rounds of the template, matches of a club) of a file's row: m x (n' - 1) and m x (n - 1), plus the derby or
    inter-zone rounds of the row; with zones, those of a zone (each half the same). A row with no meetings plays the
    class's meetings (rules gaps "the class's meetings stay"), which the caller gives from the report, else 2."""
    clubs, m, zones, extra, rounds, halves = encoded_form(file, season)
    if halves and not zones and extra == 1 and m == 1 and clubs % 2 == 0:
        # The split form (arg_prm from 2027, season_sizes_arg.c form_of): one table of n in two halves of n/2 fechas,
        # the derby repeat inside the second. The league is the first half, a League_Stage the second; each has its
        # own template of n/2 dates, and a club plays n/2 matches in each (arg_title_faults checks both together).
        return (clubs // 2, clubs // 2)
    if file == "arg_second" and zones:
        # Item 291 (season_arg_stages.c): two zones of two meetings in one table, the rounds of the larger zone, the
        # matches of the smaller. No inter-zone rounds.
        return (max(2 * (z + (z & 1) - 1) for z in zones), 2 * (min(zones) - 1))
    m = m or meetings or 2
    if zones:
        return (rounds or m * (max(zones) + (max(zones) & 1) - 1) + extra, m * (max(zones) - 1) + extra)
    if extra and clubs % 2 == 0:
        return (m * (clubs - 1) + extra, m * (clubs - 1) + extra)
    return (rounds_of(clubs, m), m * (clubs - 1))


_REAL = {}
_REAL_ID = {}


def real_season(file, season):
    """The real_season text of the rules row of a file and season (make_rules.read_rows), or ""."""
    if not _REAL:
        sys.path.insert(0, os.path.join(ROOT, "tools", "seasons"))
        import make_rules
        for r in make_rules.read_rows():
            if r.get("comp_file") and str(r.get("season", ""))[:4].isdigit():
                _REAL.setdefault((r["comp_file"], int(str(r["season"])[:4])), []).append(r.get("real_season", ""))
                _REAL_ID[r.get("id")] = r.get("real_season", "")
    if isinstance(season, RowSeason):
        row = encoded_pick(file, season)
        if not row or int(row["season"]) != season:
            raise SystemExit(f"league_size_run.py: {file} has no row {season.n} of season {int(season)}")
        return _REAL_ID.get(row.get("id"), "")
    rows = _REAL.get((file, season), [])
    return rows[0] if rows else ""


# Classes that play over two years (August to June) whose rows hold calendar-year seasons from some year: edition Y
# plays row Y, the real season that ends in it (season_sizes.c season_sizes_edition).
TWO_YEAR_CLASSES = {"arg_prm", "arg_second"}   # arg_second: item 286 (season_league.c edition_is_year)

# Classes whose rows follow the Irish calendar move (season_rules.c season_rules_moved: the files "ire_").
MOVED_CLASSES = {"ire_prm", "ire_first"}


def edition_season(file, edition):
    """The season of the row that an edition (+0x40) plays (season_sizes.c season_sizes_edition): row Y when its real
    season begins with Y, else row Y - 1 when its real season begins with Y, else Y. A class of TWO_YEAR_CLASSES
    plays row Y. A class of MOVED_CLASSES (Irish calendar move) can play the second row of season Y - 1, a
    RowSeason."""
    if file in TWO_YEAR_CLASSES:
        return edition
    if file in MOVED_CLASSES:
        # Gap row 2: a size run has changing rules on, so the Irish calendar move can happen (season_calendar_game.c
        # set_moved) and an edition plays the row whose real season begins with it, of its own season or the season
        # before, in table order (season_rules_row_of): edition 2002 row 41 (2002-03, the short season), 2003 row
        # 41b (2003), 2004 row 66 (2004).
        real_season(file, edition)      # fills _REAL_ID
        held = sum(1 for r in encoded_rows() if r.get("comp_file") == file and r.get("season") == str(edition - 1)
                   and r.get("clubs", "").isdigit())
        for key in [edition] + [RowSeason(edition - 1, n) if n else edition - 1 for n in range(held)]:
            t = real_season(file, key)
            if t[:4] == str(edition) and not t[4:5].isdigit():
                return key
    def starts(season):
        t = real_season(file, season)
        return t[:4] == str(edition) and not t[4:5].isdigit()
    if starts(edition):
        return edition
    if starts(edition - 1):
        return edition - 1
    return edition


def parse_dates(words):
    """[(date, added)] of the words of a template line."""
    out = []
    for w in words:
        added = w.endswith("+")
        out.append((datetime.date.fromisoformat(w.rstrip("+")), added))
    return out


def templates(log, file):
    """[(base, class dates, new dates or None)] of the template lines of a file."""
    out, last = [], None
    tag = f"recomp: league sizes: {file} template "
    for line in log.split("\n"):
        if not line.startswith(tag):
            continue
        head, _, rest = line[len(tag):].partition(": ")
        words = rest.split()
        if len(words) < 2 or not head.isdigit():
            continue
        kind = words[0]
        if kind not in ("class", "new"):
            continue
        dates = parse_dates(words[2:])
        if kind == "class":
            last = [int(head), [d for d, _a in dates], None]
            out.append(last)
        elif kind == "new" and last and last[0] == int(head):
            last[2] = dates
    return out


# Earlier starts of 7 days a class may take (season_template.h SEASON_TEMPLATE_EARLIER, season_sizes_bra.c BRA_EARLIER,
# season_sizes_asia_rows.c KOR_EARLIER).
EARLIER = 4
EARLIER_BY_FILE = {"bra_nat_first": 13, "bra_nat_second": 13, "kor_league": 17}


def template_faults(name, cls, new, rounds, earlier=EARLIER):
    """The faults of a new template (D4, D5) against the class's dates. A template with no gap over 7 days has no
    winter break (season_template.c). The class's dates sorted, each once: a class can list a date twice and out of
    order (HolFirst 2005/06: 3 December after 22 October and again after 26 November), and the engine leaves the
    second out."""
    cls = sorted(set(cls))
    fails = []
    if len(new) != rounds:
        return [f"{name}: {len(new)} dates, want {rounds}"]
    days = [d for d, _a in new]
    if any(b <= a for a, b in zip(days, days[1:])):
        fails.append(f"{name}: the dates do not ascend, or two rounds share a date")
    gaps = [(a, b) for a, b in zip(cls, cls[1:])]
    largest = max(((b - a).days for a, b in gaps), default=0)
    if largest <= 7:
        largest = 0
    for d, added in new:
        if not added:
            if d not in cls:
                fails.append(f"{name}: {d} is not a date of the class")
            continue
        if d < cls[0]:
            if (cls[0] - d).days % 7 or (cls[0] - d).days > 7 * earlier:
                fails.append(f"{name}: earlier start {d} is not a week step before {cls[0]}")
            continue
        if d.weekday() != 2:
            fails.append(f"{name}: new date {d} is not a Wednesday")
        for a, b in gaps:
            if a < d < b and ((b - a).days > 14 or (b - a).days == largest):
                fails.append(f"{name}: new date {d} in the gap {a} to {b} ({(b - a).days} days, the winter break)")
    if days and days[-1] != cls[-1]:
        fails.append(f"{name}: last date {days[-1]}, the class's last is {cls[-1]}")
    if rounds < len(cls):
        if any(a for _d, a in new):
            fails.append(f"{name}: a new date with fewer rounds than the class's")
        if days and days[0] != cls[0]:
            fails.append(f"{name}: first date {days[0]}, the class's first is {cls[0]}")
    return fails


def played(log, file):
    """[(edition, clubs, matches, lowest, highest)] of the played lines of a file."""
    out = []
    tag = f"recomp: league sizes: {file} played "
    for line in log.split("\n"):
        if line.startswith(tag):
            try:
                ed, rest = line[len(tag):].split(": ", 1)
                p = rest.replace(",", "").split()
                out.append((int(ed), int(p[0]), int(p[2]), int(p[5]), int(p[7])))
            except (ValueError, IndexError):
                pass
    return out


def calendar_year(file):
    """1 when the encoded rows of a file are of a calendar-year league (column calendar)."""
    with open(ENCODED, encoding="utf-8") as f:
        cols = f.readline().rstrip("\n").split("\t")
        for line in f:
            row = dict(zip(cols, line.rstrip("\n").split("\t")))
            if row.get("comp_file") == file:
                return row.get("calendar") == "calendar"
    return False


def encoded_points(file, season):
    """The points of the split of the encoded row of a file in a season ("kept", "halved", "kept+halved+kept"), by
    the after-table rule; "unknown" plays as kept (season_split.c)."""
    row = encoded_pick(file, season)
    best = (0, row.get("split", "-")) if row else None
    m = re.search(r"points (\S+)$", best[1]) if best else None
    if not m:
        return None
    parts = ["kept" if x == "unknown" else x for x in m.group(1).split("+")]
    return parts[0] if len(set(parts)) == 1 else "+".join(parts)


def south_held(season):
    """1 when the Croatian South row of the season is held (its own row, not the after-table rule)."""
    with open(ENCODED, encoding="utf-8") as f:
        cols = f.readline().rstrip("\n").split("\t")
        for line in f:
            row = dict(zip(cols, line.rstrip("\n").split("\t")))
            if row.get("comp_file") == "cro_a2b" and row.get("season") == str(season):
                return row.get("clubs", "").isdigit() and int(row["clubs"]) >= 2
    return False


def cro_faults(log, start=None):
    """The Croatian second tier (slice 11, gap rows 38 and 40, open item 255): each regroup gives the North its row
    and the South its row (two groups, by latitude), or with one group the North its row by results and the South no
    club (the others to A Lower Division); in a season of one group the South makes no table: a new game sends its
    clubs to A Lower Division (season_sizes_cro.c) and each later season start waits; after a season of one group the
    South has no exchange with the First Division."""
    fails, years = [], []
    for line in log.split("\n"):
        m = re.match(r"recomp: league sizes: cro_a2a regroup (\d+): (\d+) clubs, (\d+) North, (\d+) South, (-?\d+) "
                     r"to A Lower Division, by (results|latitude)", line)
        if not m:
            continue
        y, found, north, south, _lower = (int(x) for x in m.groups()[:5])
        years.append(y)
        one = not south_held(y)
        want_n = min(encoded_row("cro_a2a", y)[0], found)
        # One group: the South is not held, so it keeps no club.
        want_s = 0 if one else min(encoded_row("cro_a2b", y)[0], found - want_n)
        want_how = "results" if one else "latitude"
        if (north, south, m.group(6)) != (want_n, want_s, want_how):
            fails.append(f"cro_a2a regroup {y}: {north} North, {south} South, by {m.group(6)}; want {want_n}, "
                         f"{want_s}, by {want_how}")
    pairs = sum(1 for x in log.split("\n") if re.match(r"recomp: league sizes: cro_a1 \(.*\) down \d+, cro_a2b ", x))
    for y in years:
        if not south_held(y - 1) and f"recomp: league sizes: cro_a2b {y - 1}: not held, no exchange" not in log:
            fails.append(f"cro_a2b {y - 1}: no line of the South's exchange left out")
    for y in years:
        if not south_held(y) and f"recomp: league sizes: cro_a2b {y}: not held, the South waits" not in log:
            fails.append(f"cro_a2b {y}: no line of the South waiting")
    for m in re.finditer(r"recomp: league sizes: cro_a2b \([^)]*\) (\d+): (\d+) clubs in the division", log):
        if not south_held(int(m.group(1))):
            fails.append(f"cro_a2b {m.group(1)}: a table of {m.group(2)} clubs in a season it is not held")
    if start is not None and not south_held(start) and \
            f"recomp: league sizes: cro_a2b {start}: not held, the South's" not in log:
        fails.append(f"cro_a2b {start}: the new game did not send the South's clubs to A Lower Division")
    held = sum(1 for y in years if south_held(y - 1))
    if pairs > held:
        fails.append(f"cro_a2b: {pairs} exchanges with the First Division, want at most {held} (seasons of two groups)")
    return fails


def edition_row(file, edition):
    """(clubs, meetings) of the row that an edition (+0x40) plays (edition_season): a calendar-year league plays
    edition Y by the row of season Y - 1 (its real season is Y), any other league by the row of season Y."""
    return encoded_row(file, edition_season(file, edition))


def edition_counts(file, edition, meetings=0):
    return want_counts(file, edition_season(file, edition), meetings)


def edition_split(file, edition):
    """The split of the row that an edition plays (edition_season), or None."""
    return encoded_split(file, edition_season(file, edition))


def encoded_periods(file, season):
    """The periods column of the encoded row of a file in a season (the after-table rule): an int, -1 unknown."""
    best = None
    with open(ENCODED, encoding="utf-8") as f:
        cols = f.readline().rstrip("\n").split("\t")
        for line in f:
            row = dict(zip(cols, line.rstrip("\n").split("\t")))
            if row.get("comp_file") != file or not row.get("season", "").isdigit():
                continue
            s = int(row["season"])
            if s <= season and row.get("clubs", "").isdigit() and (best is None or s > best[0]):
                best = (s, row.get("periods", "-"))
    return int(best[1]) if best and best[1].isdigit() else -1


def period_lines(log, file):
    """{edition: (rounds, [lengths], winners)} of the periods lines of a file (season_sizes_periods.c)."""
    out = {}
    tag = f"recomp: league sizes: {file} periods "
    for line in log.split("\n"):
        if not line.startswith(tag):
            continue
        try:
            ed, rest = line[len(tag):].split(": ", 1)
            rounds, lengths, winners = rest.split(", ")
            out[int(ed)] = (int(rounds.split()[1]), [int(x) for x in lengths.split()[1].split("+")], winners.split()[1])
        except (ValueError, IndexError):
            pass
    return out


def period_faults(log, file, editions):
    """Each edition of a period class has a periods line: as many periods as the class, each of at least one round,
    adding up to the rounds of its row, and the winners off when its row has no periods (D6)."""
    if file not in PERIOD_FILES:
        return []
    fails, seen = [], period_lines(log, file)
    for ed in sorted(editions):
        if ed not in seen:
            fails.append(f"{file} {ed}: no periods line")
            continue
        rounds, lengths, winners = seen[ed]
        clubs, m = edition_row(file, ed)
        want = rounds_of(clubs, m or 2)
        row = encoded_periods(file, ed - 1 if calendar_year(file) else ed)
        if row >= 2 and row != PERIOD_FILES[file]:
            # Gap 33: the row's own periods, R // P rounds each (the Eerste Divisie 2005/06: six of six in 38, First
            # Division B 2016/17: two of 14); rounds past P x (R // P) are in no period.
            if rounds != want or lengths != [want // row] * row:
                fails.append(f"{file} {ed}: periods of {lengths} in {rounds} rounds, want {row} of {want // row} "
                             f"in {want}")
            # The class names four periods; the fifth and sixth get their own names (season_sizes_period_name).
            for k, name in ((5, "Fifth Period"), (6, "Sixth Period")):
                if row >= k and f"recomp: league sizes: {file} {ed}: period {k} named {name}" not in log:
                    fails.append(f"{file} {ed}: period {k} not named {name}")
        elif rounds != want or sum(lengths) != rounds or len(lengths) != PERIOD_FILES[file] or min(lengths) < 1:
            fails.append(f"{file} {ed}: periods of {lengths} in {rounds} rounds, want {PERIOD_FILES[file]} periods "
                         f"adding up to {want}")
        if winners != ("off" if row == 0 else "on"):
            fails.append(f"{file} {ed}: period winners {winners}, the row has {row} periods")
    return fails


def bel_playoff_faults(log, file, editions):
    """Gap 33: First Division B with two periods (2016/17 to 2019/20): the two period winners play a final, a group of
    two that meets twice; a club that won both periods goes up with no final. The log line of BelSecond::
    create_playoff_group (season_sizes_periods.c): "bel_second play-off <edition>: <clubs> clubs" or "... promoted
    directly"."""
    if file != "bel_second":
        return []
    fails, seen = [], {}
    tag = f"recomp: league sizes: {file} play-off "
    for line in log.split("\n"):
        if line.startswith(tag):
            ed, _, rest = line[len(tag):].partition(": ")
            if ed.isdigit():
                seen[int(ed)] = rest
    for ed in sorted(editions):
        if encoded_periods(file, ed) != 2:
            continue
        rest = seen.get(ed, "")
        if not (rest.startswith("2 clubs") or rest.startswith("promoted directly")):
            fails.append(f"{file} {ed}: play-off '{rest or 'no line'}', want a final of the two period winners")
    return fails


def report_rows(report, file):
    """The rows of a file in the rules report: [(season, phase, edition, clubs, meetings, matches)]."""
    out, cols = [], None
    with open(report, encoding="latin-1") as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            if p[0] == "columns":
                cols = p[1:]
            elif cols and p[0] == "comp" and len(p) == len(cols) + 1:
                row = dict(zip(cols, p[1:]))
                if row["file"] == file:
                    out.append((int(row["season"]), row["phase"], int(row["year"]), int(row["clubs"]),
                                int(row["rounds"]), int(row["matches"])))
    return out


# Slice 17 (D11): leagues whose class plays a play-off that the row's meetings leave out (season_sizes_bra.c).
NO_PLAYOFF = {"bra_nat_first", "bra_nat_second"}


def no_playoff_faults(log, file, meetings):
    """With two meetings: the log says the play-off is off, and the club on top of the table gets the title (code 5
    in the league stage, the base handler 0x686940)."""
    if file not in NO_PLAYOFF or meetings < 2:
        return []
    lines = log.split("\n")
    fails = []
    if not any(x.startswith(f"recomp: league sizes: {file} ") and x.endswith("no play-off") for x in lines):
        fails.append(f"{file}: no log line of a season with no play-off")
    if not any(x.startswith(f"recomp: league sizes: {file} ") and " champion " in x for x in lines):
        fails.append(f"{file}: no champion of the table in the log")
    return fails


# Gap 37: the clubs that lost their Veikkausliiga licence after an edition (season_sizes.c licence_lost).
LICENCE_LOST = {("fin_prm", 2005): {"AC Vantaan Allianssi"}, ("fin_prm", 2010): {"Tampere United", "AC Oulu"}}
DIVISION_NAME = {"fin_prm": "Veikkausliiga"}


def step_faults(log, file):
    """Gap 37: the size step of a division whose division below plays as now (the Veikkausliiga). Real life grew it
    with clubs of the Ykkonen, so no club joins it from no division; after an edition with lost licences, a named club
    that was in it is the one that leaves (a club leaves only to no division). The moves of a step come before its
    line "step: <file> (<name>) <Y>: ...", the change after edition Y - 1."""
    name = DIVISION_NAME.get(file)
    if not name:
        return []
    fails, joined, left, gone = [], [], [], {}
    tag, summary = "recomp: league sizes: step: ", f"recomp: league sizes: step: {file} ("
    for line in log.split("\n"):
        if line.startswith(summary):
            head = line.split(") ", 1)[-1].split(":")[0]
            if not head.isdigit():
                continue
            edition = int(head) - 1
            fails += [f"{file} {edition}: {c} joins from no division" for c in joined]
            if left:
                gone[edition] = left
            joined, left = [], []
        elif line.startswith(tag):
            text = line[len(tag):]
            if text.endswith(f" from no division to {name}"):
                joined.append(text[:-len(f" from no division to {name}")])
            elif text.endswith(f" from {name} to no division"):
                left.append(text[:-len(f" from {name} to no division")])
    for (f, ed), clubs in LICENCE_LOST.items():
        if f == file and ed in gone and not set(gone[ed]) & clubs:
            fails.append(f"{file} {ed}: {gone[ed]} left, not a club that lost its licence ({sorted(clubs)})")
    return fails


def bra_playoff(file, ed):
    """Gap rows 56 and 57: the Série B play-off of edition ed (row ed - 1): "top8" for rows 2001 to 2004 (the two
    finalists of the top 8 go up), "ties" from row 2025 (3rd v 6th and 4th v 5th, both winners go up), else None."""
    if file != "bra_nat_second":
        return None
    row = ed - 1
    return "top8" if 2001 <= row <= 2004 else "ties" if row >= 2025 else None


def playoff_faults(log, file, editions):
    """Each finished edition with a Série B play-off of the rules table: the play-off was drawn on its places, and two
    clubs went up from it (season_sizes_bra.c log lines)."""
    lines = log.split("\n")
    fails = []
    for ed in sorted(editions):
        kind = bra_playoff(file, ed)
        if not kind:
            continue
        head = f"recomp: league sizes: {file} {ed}: "
        drawn = head + "play-off of places " + ("1 to 8" if kind == "top8" else "3 to 6") + " drawn"
        if not any(x.startswith(drawn) for x in lines):
            fails.append(f"{file} {ed}: no play-off of places {'1 to 8' if kind == 'top8' else '3 to 6'}")
        ups = [x for x in lines if x.startswith(head) and " goes up" in x and "no play-off" not in x]
        if len(ups) != 2:
            fails.append(f"{file} {ed}: {len(ups)} clubs went up from the play-off, want 2")
    return fails


_DOWN = {}


def row_down(file, season):
    """The relegation places of the rules row of a file and season (the leading number of its down text), or -1."""
    if not _DOWN:
        sys.path.insert(0, os.path.join(ROOT, "tools", "seasons"))
        import make_rules
        for r in make_rules.read_rows():
            if r.get("comp_file") and str(r.get("season", ""))[:4].isdigit():
                m = re.match(r"\s*(\d+)", str(r.get("down", "")))
                _DOWN[(r["comp_file"], int(str(r["season"])[:4]))] = int(m.group(1)) if m else -1
    return _DOWN.get((file, season), -1)


# A division with no chain (open item 250, season_sizes.c lone_exchange): its row's relegation places go to no
# division at its season change.
# Gap 47: the K League too (no K League 2 in the game). A division alone sends down only as many clubs as its nation's
# clubs with no division can replace (season_size_lone_down), and the log says so when it sends fewer.
LONE = {"wel_first", "kor_league"}


def lone_downs(log, file):
    """{edition: clubs} of the lines of a file's clubs that went down to no division."""
    out = {}
    for m in re.finditer(r"recomp: league sizes: %s \(.*?\) (\d+): .* goes down to no division" % re.escape(file), log):
        out[int(m.group(1))] = out.get(int(m.group(1)), 0) + 1
    return out


def lone_places(log, file):
    """{edition: relegation places} that sizes_rules wrote for a lone file (the last line of each edition). The class
    writes none (0), so an edition with no line shows 0."""
    out = {}
    for m in re.finditer(r"recomp: league sizes: %s \(.*?\) (\d+): relegation places (\d+) to no division"
                         % re.escape(file), log):
        out[int(m.group(1))] = int(m.group(2))
    return out


# Test switch RECOMP_TEST_LONE_HUMAN=1 (season_sizes.c lone_exchange): the worst club of a lone exchange counts as a
# club with a human manager. It stays, the next worst club goes in its place, and the news says so.
def lone_human_faults(log, file, year, last):
    fails = []
    stays = {}
    for m in re.finditer(r"recomp: league sizes: %s \(.*?\) (\d+): (.+?) stays, its manager is a player: (.+?) goes "
                         r"down in its place" % re.escape(file), log):
        stays.setdefault(int(m.group(1)), []).append((m.group(2), m.group(3)))
    downs = {}
    for m in re.finditer(r"recomp: league sizes: %s \(.*?\) (\d+): (.+?) goes down to no division" % re.escape(file),
                         log):
        downs.setdefault(int(m.group(1)), []).append(m.group(2))
    for ed in range(year, last):
        if not downs.get(ed):
            continue
        if not stays.get(ed):
            fails.append(f"{file} {ed}: no club stayed with the test switch RECOMP_TEST_LONE_HUMAN")
            continue
        for kept, went in stays[ed]:
            if kept in downs[ed] or went not in downs[ed]:
                fails.append(f"{file} {ed}: {kept} stays and {went} goes, but the clubs down are {downs[ed]}")
            if not re.search(r"recomp: season change: news \d+/\d+: %s stays in the " % re.escape(kept), log):
                fails.append(f"{file} {ed}: no news line that {kept} stays")
    return fails


def lone_guards(log, file):
    """{edition: clubs that go down} of the guard lines of a file (fewer than its relegation places). The count is
    worked out again from the line's places, pool and counts (season_size_lone_down), so a wrong guard fails: -1."""
    out = {}
    for m in re.finditer(r"recomp: league sizes: %s \(.*?\) (\d+): relegation places (\d+), (\d+) go down: "
                         r"(\d+) clubs with no division can come up, (\d+) clubs to (-?\d+)"
                         r"(?:, the table showed (-?\d+))?" % re.escape(file), log):
        places, down, pool, have, want = (int(m.group(k)) for k in range(2, 7))
        most = 0 if want < 0 else max(0, pool + have - want)
        if m.group(7) is not None and 0 <= int(m.group(7)) < most:
            most = int(m.group(7))     # never more than the table showed (lead decision of 15 November)
        out[int(m.group(1))] = down if down == min(places, most) else -1
    return out


def split_bounds(log, file):
    """{edition: [[(best, worst)] in table order]} of the split bounds lines of a file (season_split.c rt_split_bounds:
    the best and worst places that each row can still reach after the split)."""
    out = {}
    for m in re.finditer(r"recomp: league sizes: %s split bounds (\d+) \d+:((?: \d+-\d+)+)" % re.escape(file), log):
        out.setdefault(int(m.group(1)), []).append([tuple(int(x) for x in w.split("-")) for w in m.group(2).split()])
    return out


def bounds_faults(name, lines, sizes):
    """The faults of the bounds lines of a split: each row's best and worst places lie in the places of its section
    (the rows are in table order, so the sections are the runs of the sizes)."""
    fails = []
    for rows in lines:
        if len(rows) != sum(sizes):
            fails.append(f"{name}: bounds of {len(rows)} rows, want {sum(sizes)}")
            continue
        top = 0
        for size in sizes:
            for i in range(top, top + size):
                best, worst = rows[i]
                if not top + 1 <= best <= worst <= top + size:
                    fails.append(f"{name}: row {i + 1} may reach places {best} to {worst}, its section holds "
                                 f"{top + 1} to {top + size}")
            top += size
    return fails


def encoded_halves(file, season):
    """1 when the encoded row of a file in a season (the after-table rule) plays two halves."""
    row = encoded_pick(file, season)
    return bool(row and row.get("halves") == "1")


def j1_faults(log, file, editions):
    """J1 (slice 14, D8): an edition of a row of halves plays the halves of its count and their final or a double
    winner; an edition of one table plays no half and its top club is the champion."""
    if file != "jap_j1":
        return []
    lines = log.split("\n")
    fails = []
    for ed in sorted(editions):
        tag = f"recomp: league sizes: jap_j1 {ed}: "
        mine = [x[len(tag):] for x in lines if x.startswith(tag)]
        clubs, m = edition_row(file, ed)
        if encoded_halves(file, ed - 1 if calendar_year(file) else ed):
            if not any(x.startswith("halves and the final") for x in mine):
                fails.append(f"jap_j1 {ed}: no log line of a season of halves")
            if any(x.startswith("one table") for x in mine):
                fails.append(f"jap_j1 {ed}: a season of halves played one table")
            half = m * (clubs - 1) // 2
            if half != 15 and not any(x.startswith(f"half of {clubs} clubs, {half} matches") for x in mine):
                fails.append(f"jap_j1 {ed}: no half of {clubs} clubs with {half} matches a club")
            rounds = rounds_of(clubs, m)
            bound = [x for x in mine if x.startswith("second half from round ")]
            if not bound or any(not x.startswith(f"second half from round {rounds // 2 + 1} of {rounds} ") for x in bound):
                fails.append(f"jap_j1 {ed}: second half lines {bound}, want from round {rounds // 2 + 1} of {rounds}")
            if not any(x == "final of the halves" or x == "double winner, no final" for x in mine):
                fails.append(f"jap_j1 {ed}: neither a final of the halves nor a double winner")
        else:
            if not any(x.startswith("one table, no halves and no final") for x in mine):
                fails.append(f"jap_j1 {ed}: no log line of a season of one table")
            if any(x.startswith(("half of", "final of the halves", "second half from", "double winner")) for x in mine):
                fails.append(f"jap_j1 {ed}: a season of one table made a half or a final")
            if not any(x.startswith("champion club ") for x in mine):
                fails.append(f"jap_j1 {ed}: no champion of the table in the log")
    return fails


def shortages(log, file):
    """{edition: clubs} of the editions that the size step left short with a reason (D12: when the database holds
    too few clubs, the division plays with the clubs it has, and the log names it)."""
    out = {}
    pat = re.compile(rf"recomp: league sizes: step: {re.escape(file)} \(.*\) (\d+): (\d+) clubs, the row (\d+): "
                     r"no club of its nation with no division")
    for line in log.split("\n"):
        m = pat.match(line)
        if m and int(m.group(2)) < int(m.group(3)):
            out[int(m.group(1))] = (int(m.group(2)), int(m.group(3)))
    return out
# Gap 43: an exchange whose real places keep both counts. The lower file: (the upper file, the least clubs that move
# each way). League of Ireland 2011 to 2015 (rows wal-nir-irl-266 to 367): the 12th of the Premier Division goes down,
# the First Division champion goes up, and the 11th meets the winner of the First Division play-off, so 1 or 2 clubs
# move each way and the counts stay 12 and 8.
EXCHANGE = {"ire_first": ("ire_prm", 1, 2011, 2014)}


def exchange_faults(log, file, year):
    """The pair lines of the promotion wrap (season_sizes.c, league_promote_and_relegate 0x689c80) between the upper
    file and this one: as many clubs go down as come up, and at least the least count. Only for a run that starts in
    the seasons of EXCHANGE: in a season of a real size change (edition 2017, 12 to 10) the counts differ."""
    if file not in EXCHANGE or not EXCHANGE[file][2] <= year <= EXCHANGE[file][3]:
        return []
    upper, least = EXCHANGE[file][:2]
    pat = re.compile(rf"recomp: league sizes: {upper} \(.*\) down (-?\d+), {file} \(.*\) up (-?\d+), pairs")
    moves = [(int(m.group(1)), int(m.group(2))) for m in map(pat.match, log.split("\n")) if m]
    if not moves:
        return [f"{file}: no exchange line with {upper}"]
    return [f"{file}: {upper} sends {d} down and {file} {u} up, want the same count of {least} or more"
            for d, u in moves if d != u or d < least]


# Open item 251: the League of Ireland seasons whose rows have no play-off between the divisions and change both
# counts (rows wal-nir-irl-66/67, 166/167, 391/392). An edition is a calendar year and plays the row of its real
# season (edition_season), so the key is the edition: (Premier direct places down, First Division places up). Real
# life: 2004 1 down and 3 up (10 to 12, 12 to 10), 2008 and 2017 the 10th to 12th down and the champion up (12 to 10;
# 10 to 12 and 8 to 10), no play-off. The counts of the next edition are the report's check.
IRL_CHANGE = {2004: (1, 3), 2008: (3, 1), 2017: (3, 1)}


def irl_partner_faults(report, file, year):
    """The other Irish division in the report: each edition of the run holds the clubs of its row (edition_row)."""
    other = {"ire_prm": "ire_first", "ire_first": "ire_prm"}.get(file)
    if not other or not os.path.exists(report):
        return []
    seen = {(ed, n) for _s, _p, ed, n, _m, _x in report_rows(report, other)}
    want = {ed: edition_row(other, ed)[0] for ed, _n in seen}
    return [f"{other} {ed}: {n} clubs, want {want[ed]}" for ed, n in sorted(seen)
            if year <= ed and edition_row(other, ed)[0] >= 2 and n != want[ed]]


def irl_place_faults(report):
    """Item 251b review: a League of Ireland edition that plays a row of another real season (edition 2003 plays row
    2003, the real 2004) keeps the class's places: the First Division 1 up and the play-off (IreFirst::set_up_rules
    0x635aa0), not the row's 3 up, and the Premier Division 1 down and the play-off (the real 2003)."""
    if not os.path.exists(report):
        return []
    fails, cols = [], None
    with open(report, encoding="latin-1") as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            if p[0] == "columns":
                cols = p[1:]
                continue
            if p[0] != "comp" or not cols or len(p) < len(cols) + 1:
                continue
            row = dict(zip(cols, p[1:]))
            if row.get("file") not in ("ire_first", "ire_prm") or not row.get("year", "").lstrip("-").isdigit():
                continue
            ed = int(row["year"])
            if real_season(row["file"], edition_season(row["file"], ed))[:4] == str(ed):
                continue
            if row["file"] == "ire_prm":
                # The Premier Division: 1 down and the play-off (IrePrm::set_up_rules 0x63a900, +0xc0 at 0x63a957).
                if (row.get("down"), row.get("playoff_down")) != ("1", "1"):
                    fails.append(f"ire_prm {ed} {row.get('phase')}: down {row.get('down')} and play-off "
                                 f"{row.get('playoff_down')}, want 1 and 1 (a row of another real season)")
                continue
            if (row.get("up"), row.get("playoff_up")) != ("1", "1"):
                fails.append(f"ire_first {ed} {row.get('phase')}: up {row.get('up')} and play-off "
                             f"{row.get('playoff_up')}, want the class's 1 and 1 (a row of another real season)")
    return fails


_NIR = {}


def nir_row(file, season):
    """(up, down, play-off) of the Northern Irish rules row of a file and season: the direct places (the leading
    number of the up and down texts, 0 for none) and 1 when the Premier Division's row of that season has a play-off
    (the play-off of NirFirst::create_playoff_stage 0x78c0a0 is one stage of both divisions), or None."""
    if not _NIR:
        sys.path.insert(0, os.path.join(ROOT, "tools", "seasons"))
        import make_rules
        for r in make_rules.read_rows():
            if r.get("comp_file") in ("nir_prm", "nir_first") and str(r.get("season", ""))[:4].isdigit():
                lead = [re.match(r"\s*(\d+)", str(r.get(k, ""))) for k in ("up", "down")]
                _NIR[(r["comp_file"], int(str(r["season"])[:4]))] = (
                    int(lead[0].group(1)) if lead[0] else 0, int(lead[1].group(1)) if lead[1] else 0,
                    str(r.get("playoffs", "none")) != "none")
    prm = _NIR.get(("nir_prm", season))
    own = _NIR.get((file, season))
    return None if not prm or not own else (own[0], own[1], 1 if prm[2] else 0)


def nir_start_faults(log, year, last):
    """Item 288: both Northern Irish leagues start every season after the first (the season change log of
    RECOMP_SEASON_CHANGE_LOG=1; a run without that log has no lines to read)."""
    if "recomp: season change:" not in log:
        return []
    return [f"{file} does not start {ed}" for ed in range(year + 1, last + 1) for file in ("nir_prm", "nir_first")
            if not re.search(rf"^recomp: season change: {file} \(.*\) starts {ed},", log, re.M)]


def nir_place_faults(report, log=""):
    """Items 271 and 272 (slice 13): every Northern Irish edition in the report holds the places of its row. The
    Premier Division: 0 up, the row's direct places down and the play-off place when the row has one
    (NirPrm::set_up_rules 0x78ed80 writes 1 and 1, +0xc1 at 0x78ede0 and +0xc0 at 0x78edda). The First Division: the
    row's direct places up, the play-off place, and the row's relegation places to the Northern Irish lower division
    (NirFirst::set_up_rules 0x78bd70 writes 1 up and 1 play-off, +0xbe at 0x78bdb0 and +0xbf at 0x78bdb6, and 0
    down). In the log: the clubs that go down from the First Division at each season change are its shown places."""
    if not os.path.exists(report):
        return ["nir: no rules report"]
    fails, cols, shown = [], None, {}
    with open(report, encoding="latin-1") as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            if p[0] == "columns":
                cols = p[1:]
                continue
            if p[0] != "comp" or not cols or len(p) < len(cols) + 1:
                continue
            row = dict(zip(cols, p[1:]))
            file = row.get("file")
            if file not in ("nir_prm", "nir_first") or not row.get("year", "").lstrip("-").isdigit():
                continue
            ed = int(row["year"])
            want = nir_row(file, ed)
            if not want:
                continue
            up, down, po = want
            if file == "nir_prm":
                got, exp = (row.get("up"), row.get("down"), row.get("playoff_down")), ("0", str(down), str(po))
                what = "up, down and play-off down"
            else:
                got, exp = (row.get("up"), row.get("playoff_up"), row.get("down")), (str(up), str(po), str(down))
                what = "up, play-off up and down"
                if row.get("phase") == "end":
                    shown[ed] = int(row.get("down") or 0)
            if got != exp:
                fails.append(f"{file} {ed} {row.get('phase')}: {what} {got}, want {exp}")
    went = {}
    for m in re.finditer(r"recomp: league sizes: nir_first \(.*?\) (\d+): .* goes down to the ", log):
        went[int(m.group(1))] = went.get(int(m.group(1)), 0) + 1
    ends = {int(m.group(1)) for m in re.finditer(r"recomp: league sizes: nir_first played (\d+):", log)}
    for ed in sorted(ends):
        if ed in shown and went.get(ed, 0) != shown[ed]:
            fails.append(f"nir_first {ed}: {went.get(ed, 0)} clubs down to the lower division, the table showed "
                         f"{shown[ed]}")
    return fails


def irl_change_faults(log, file, year, last=None):
    """In a run of ire_prm or ire_first: the play-off line of the First Division (season_sizes_irl.c) for exactly the
    editions of IRL_CHANGE among the editions of the run (the start to last, default the start and the next); and for
    each edition of IRL_CHANGE whose exchange the run plays (the start edition, or one before last), the exchange
    moves the row's places with each club on its own, in a game of any start (item 251b)."""
    if file not in ("ire_prm", "ire_first"):
        return []
    last = max(last or year + 1, year + 1)
    lines = log.split("\n")
    nopo = {int(m.group(1)) for m in (re.match(r"recomp: league sizes: ire_first (\d+): no play-off with the Premier "
                                               r"Division$", x) for x in lines) if m}
    fails = [f"ire_first {ed}: a line of no play-off, the row has one" for ed in sorted(nopo)
             if year <= ed <= last and ed not in IRL_CHANGE]
    pat = re.compile(r"recomp: league sizes: ire_prm \(.*\) down (-?\d+), ire_first \(.*\) up (-?\d+), pairs \d+, .*"
                     r": each club moves on its own$")
    moves = [(int(m.group(1)), int(m.group(2))) for m in map(pat.match, lines) if m]
    for ed in sorted(e for e in IRL_CHANGE if year <= e < last):
        if ed not in nopo:
            fails.append(f"ire_first {ed}: no line of a season with no play-off with the Premier Division")
        if IRL_CHANGE[ed] not in moves:
            fails.append(f"ire_prm {ed}: exchanges {moves}, want {IRL_CHANGE[ed][0]} down and {IRL_CHANGE[ed][1]} up "
                         "with each club on its own")
    return fails


# Gap row 7 (src/recomp_rt/win/season_mls.c): from 2002 the MLS plays one table of the division's clubs, an
# unbalanced schedule of the season's games (season_mls_plan.c, SCHEDULES) and a play-off to a final. The row holds
# the clubs and no meetings, so the generic counts (m x (n - 1) matches, the template of the row, the played lines of
# the size engine) do not apply. Edition 2001 is the original's three groups of 4.
MLS_FIRST = 2002
MLS_PLAN = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "src", "recomp_rt", "win",
                        "season_mls_plan.c")
# 0.7.3 slice 6 (issue 157): clubs fold and join in their real seasons (season_closed.c), so each edition holds the
# clubs of its row. No shortage of the MLS is accepted.
MLS_PLAN_LINE = re.compile(r"recomp: mls: (\d+): (\d+) clubs \(East (\d+), West (\d+)\), (\d+) matches, (\d+) to "
                           r"(\d+) games, (\d+) rounds ")


def mls_games(path=MLS_PLAN):
    """{season: games a club} of the SCHEDULES table of season_mls_plan.c."""
    text = open(path, encoding="utf-8").read()
    block = text.split("SCHEDULES[] = {", 1)[1].split("};", 1)[0]
    return {int(y): int(g) for y, g in re.findall(r"\{(\d{4}), \d+, \d+, \d+, (\d+), \d+\}", block)}


def mls_faults(log, rows, tmpl, file, year, last, row, games):
    """The faults of an MLS run: each edition from 2002 holds the clubs of its row (or the count that the log names
    with its reason), every club plays the season's games, the template holds the plan's rounds, and the play-off
    ends in a final with one club out of each tie."""
    fails = []
    lines = log.split("\n")
    last_known = max(games)

    def games_of(ed):
        return games[min(ed, last_known)]       # editions after the table play its last form (season_mls_schedule)
    for season, phase, ed, n, _meet, matches in rows:
        if ed < MLS_FIRST:
            continue
        clubs = row(ed)[0]
        if n != clubs:
            fails.append(f"{file}: report {season} {phase}, edition {ed}: {n} clubs, want {clubs}")
        elif matches not in (-1, games_of(ed)):
            fails.append(f"{file}: report {season} {phase}, edition {ed}: {matches} matches a club, want "
                         f"{games_of(ed)}")
    plans = {}
    for x in lines:
        m = MLS_PLAN_LINE.match(x)
        if m:
            plans[int(m.group(1))] = [int(v) for v in m.groups()[1:]]
    for ed in range(max(year, MLS_FIRST), last + 1):
        if ed not in plans:
            fails.append(f"{file} {ed}: no plan line")
            continue
        n, east, west, matches, lo, hi, rounds = plans[ed]
        clubs, g = row(ed)[0], games_of(ed)
        if (n, east + west, lo, hi, matches) != (clubs, clubs, g, g, clubs * g // 2):
            fails.append(f"{file} {ed}: plan of {n} clubs ({east} and {west}), {matches} matches, {lo} to {hi} games; "
                         f"want {clubs} clubs, {clubs * g // 2} matches, {g} games each")
        if not any(x.startswith(f"recomp: mls: {ed}: final ") for x in lines):
            fails.append(f"{file} {ed}: no final")
    for base, _cls, new in tmpl:
        if base in plans and (new is None or len(new) != plans[base][6]):
            fails.append(f"{file} template {base}: {len(new) if new else 'the class'} dates, want the plan's "
                         f"{plans[base][6]} rounds")
    fails += [f"{file}: {x[len('recomp: mls: '):]}" for x in lines if x.startswith("recomp: mls: ") and
              (re.search(r" tie \d+: \d+ clubs go on", x) or " not built" in x or ": no plan for " in x or
               ": no room for " in x)]
    return fails


def rules_faults(report, file, short):
    """The rules report differences of the clubs of a file. A short edition (shortages) differs from its row by the
    clubs the database lacks, and the log names it. The report's season of a calendar-year league is the edition
    before (edition_row). Report season 2001 of the MLS also holds edition 2001, the original's groups of 4."""
    import rules_report_check as rrc
    rep_clubs = []
    seasons, comps = rrc.read_report(report, rep_clubs)
    diffs = (set(rrc.differences(seasons, comps, rrc.make_rules.read_rows()))
             | rrc.division_differences(comps, rep_clubs))
    diffs = sorted(d for d in diffs if d[1] in ("clubs", "division_clubs") and file in d[2])

    def named(d):
        ed = d[0] + (1 if calendar_year(file) else 0)
        if ed not in short:
            return False
        m = re.search(r"game ([\d/]+), table (\d+) ", d[2] + " ")
        if not m or int(m.group(2)) != short[ed][1]:
            return False
        ok = {short[ed][0]} | ({4} if file == "usa_mls" and ed == MLS_FIRST else set())
        return all(int(v) in ok for v in m.group(1).split("/"))
    return [f"{file} {d[0]}: rules report difference: {d[2]}" for d in diffs if not named(d)]


def faults(work, file, year, last):
    """The faults of a run, edition by edition: each edition that the report, the template lines and the played
    lines show is checked against its own row (edition_row)."""
    report = os.path.join(work, "report.tsv")
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    lines = log.split("\n")
    fails = [f"{x.strip()}" for x in lines if "recomp: message box from" in x]
    short = shortages(log, file)

    def row(ed):
        clubs, m = edition_row(file, ed)
        return (short[ed][0], m) if ed in short else (clubs, m)

    if edition_row(file, year)[0] < 2:
        return fails + [f"{file} {year}: no row in {os.path.basename(ENCODED)}"]
    # A class of DATA_COUNT allocates the rows of the clubs it counts (IreFirst::create_table 0x635910), so it
    # writes no table line.
    held = league_sizes.DATA_COUNT.get(file, league_sizes.HELD.get(file, 0))
    # The report: every edition of the run holds the clubs and meetings of its row, and the matches of the count.
    rows = report_rows(report, file) if os.path.exists(report) else []
    # The meetings that the game shows for each edition: a row with no meetings plays the class's.
    meet_of = {r[2]: r[4] for r in rows}

    def counts(ed):
        """(rounds, matches of a club) of an edition. The J1 and K League rows of halves or stages are one table of
        both (HALVES_IN_TABLE: row() doubles the meetings) and may hold fewer clubs than the row (D12), so they count
        from row(); the rest from edition_counts (zones and derby rounds; a row with no meetings plays the class's)."""
        if file not in HALVES_IN_TABLE:
            return edition_counts(file, ed, meet_of.get(ed, 0))
        clubs, m = row(ed)
        return (rounds_of(clubs, m or 2), (m or 2) * (clubs - 1))
    if not any(r[0] == year and r[1] == "start" for r in rows):
        fails.append(f"{file} {year}: no row in the start report")
    if file == "usa_mls":
        fails += mls_faults(log, rows, templates(log, file), file, year, last, row, mls_games())
        if os.path.exists(report):
            fails += rules_faults(report, file, short)
        print(f"{file}: editions in the report " + ", ".join(sorted({f"{ed}:{n}" for _s, _p, ed, n, _m, _x in rows}))
              + "; club counts of the database " + ", ".join(f"{ed}:{n} (row {w})" for ed, (n, w) in sorted(short.items())))
        return fails
    for season, phase, ed, n, meet, matches in rows:
        if ed > season + 1:
            continue    # an idle group that waits for its next edition (season_divisions.c gre_swap: year edition - 1)
        clubs, m = row(ed)
        m = m or meet
        if (n, meet) != (clubs, m):
            fails.append(f"{file}: report {season} {phase}, edition {ed}: clubs and meetings {(n, meet)}, want "
                         f"{(clubs, m)}")
        else:
            sp = edition_split(file, ed)
            want = counts(ed)[1] + (split_numbers(sp[1])[2] if sp else 0)
            if matches != -1 and matches != want:
                fails.append(f"{file}: report {season} {phase}, edition {ed}: {matches} matches a club, want {want}")
    fails += [f"{file}: {x.split(': ', 1)[1]}" for x in lines
              if x.startswith(f"recomp: league sizes: {file} template mode ")
              or x.startswith(f"recomp: league sizes: {file} dates: no room")
              or (x.startswith(f"recomp: league sizes: {file} split") and
                  any(w in x for w in ("no dates", "not made", "not each place")))]
    # The templates of each edition.
    tmpl = templates(log, file)
    # The Primera's rows 2001 to 2011 are the class's own form (20 clubs, Apertura and Clausura of 19 rounds): the
    # class's dates stay, so no template line.
    class_form = file == "arg_prm" and all(encoded_form(file, edition_season(file, e))[0] == 20 and
                                           not encoded_form(file, edition_season(file, e))[2] and
                                           edition_season(file, e) <= 2011 for e in range(year, last + 1))
    if not tmpl and not class_form:
        fails.append(f"{file} {year}: no template line")
    for base, cls, new in tmpl:
        clubs, m = row(base)
        sp = edition_split(file, base)
        rounds = counts(base)[0] + (split_numbers(sp[1])[0] if sp else 0)
        if file in MOVED_CLASSES and "short season" in real_season(file, edition_season(file, base)):
            # The short season of the Irish calendar move plans from the class's dates before 1 February of the next
            # year, so the play-off ends before the update day (season_calendar.c season_calendar_move_cut); it always plans
            # again, so the cut comes before the check of the class's count.
            cls = [d for d in cls if d < datetime.date(base + 1, 2, 1)]
        if rounds == len(cls) and not sp:
            continue
        if new is None:
            fails.append(f"{file} template {base}: the class's {len(cls)} dates stay, want {rounds}")
        else:
            fails += template_faults(f"{file} template {base}", cls, new, rounds,
                                     EARLIER_BY_FILE.get(file, EARLIER))
        if clubs > held and not any(x.startswith(f"recomp: league sizes: {file} table of {clubs} rows") and
                                    f" {base}, the class makes" in x for x in lines):
            fails.append(f"{file} {base}: no log line of a table of {clubs} rows (the class allocates {held})")
    # The finished tables: every club played the matches of the count of its edition's row.
    done = played(log, file)
    if not done:
        fails.append(f"{file}: no finished table in the run (a two-year league needs --last {year + 1})")
    made, loaded = splits(log, file)
    news = {base: new for base, _c, new in tmpl if new}
    for base, dates in split_dates(log, file).items():
        tail = [d for d, _a in news.get(base, [])][-len(dates):] if dates else []
        if dates != tail:
            fails.append(f"{file} {base}: the section dates {dates} are not the last dates of the template {tail}")
    for ed, n, matches, lo, hi in done:
        clubs, m = row(ed)
        m = m or meet_of.get(ed, 0) or 2
        sp = edition_split(file, ed)
        if not sp:
            want = counts(ed)[1]
            if n != clubs or not (matches == lo == hi == want):
                fails.append(f"{file} {ed}: {n} clubs, {matches} matches, played {lo} to {hi}; want {clubs} clubs "
                             f"with {want} each")
            continue
        # A split: every club played the table's matches and those of its section (the fewest and the most).
        after = m * (clubs - 1)
        _r, few, most = split_numbers(sp[1])
        if n != clubs or (matches, lo, hi) != (after + most, after + few, after + most):
            fails.append(f"{file} {ed}: {n} clubs, {matches} matches, played {lo} to {hi}; want {clubs} clubs, "
                         f"{after + most} matches, played {after + few} to {after + most}")
        want = (sp[0], [x for x, _m in sp[1]], [y for _x, y in sp[1]])
        if made.get(ed) != want:
            fails.append(f"{file} {ed}: split line {made.get(ed)}, want after, sections and meetings {want}")
        ends = split_ends(log, file)
        if ed not in ends:
            fails.append(f"{file} {ed}: no split end line")
        else:
            fails += end_faults(f"{file} {ed}", ends[ed], sp[0], sp[1],
                                groups_of(file, ed - 1 if calendar_year(file) else ed))
        bounds = split_bounds(log, file)
        if ed not in bounds:
            fails.append(f"{file} {ed}: no split bounds line")
        else:
            fails += bounds_faults(f"{file} {ed}", bounds[ed], [x for x, _m in sp[1]])
    for ed, line in split_points_logged(log, file).items():
        want = encoded_points(file, edition_season(file, ed))
        # Gap 34: a split of SPLIT_POINTS has its own section rules (split_points_faults checks them); the row's
        # "points halved" is its first section only.
        if any(f == file and a <= edition_season(file, ed) <= b for f, a, b in SPLIT_POINTS):
            continue
        if want and line != want:
            fails.append(f"{file} {ed}: split points {line}, want {want}")
    if file in ("cro_a2a", "cro_a2b"):
        fails += cro_faults(log, year)
    if file == "arg_prm":
        fails += arg_faults(log, year)
    if file in LONE:
        downs, guards = lone_downs(log, file), lone_guards(log, file)
        for ed in range(year, last):
            want = guards.get(ed, row_down(file, edition_season(file, ed)))
            if want >= 0 and downs.get(ed, 0) != want:
                fails.append(f"{file} {ed}: {downs.get(ed, 0)} clubs down to no division, want {want}")
            # Lead decision of 15 November: the table shows only the places that the exchange uses.
            shown = lone_places(log, file).get(ed, 0)
            if want >= 0 and shown != downs.get(ed, 0):
                fails.append(f"{file} {ed}: the table shows {shown} relegation places, {downs.get(ed, 0)} go down")
        if os.environ.get("RECOMP_TEST_LONE_HUMAN") == "1":
            fails += lone_human_faults(log, file, year, last)
    fails += no_playoff_faults(log, file, edition_row(file, year)[1] or 2)
    fails += period_faults(log, file, {ed for ed, _n, _x, _l, _h in done})
    fails += step_faults(log, file)
    fails += split_points_faults(log, file, lambda ed: ed - 1 if calendar_year(file) else ed)
    fails += bel_playoff_faults(log, file, {ed for ed, _n, _x, _l, _h in done if _l})
    fails += playoff_faults(log, file, {ed for ed, *_x in done})
    fails += j1_faults(log, file, {ed for ed, *_x in done})
    fails += exchange_faults(log, file, year)
    fails += irl_change_faults(log, file, year, last)
    # Item 260: the other division too, in every Irish run (a 2003 or 2004 start held 12 Premier clubs for 10).
    fails += irl_partner_faults(report, file, year)
    if file in ("ire_prm", "ire_first"):
        fails += irl_place_faults(report)
    if file in ("nir_prm", "nir_first"):
        fails += nir_place_faults(report, log)
        fails += nir_start_faults(log, year, last)
    if os.path.exists(report):
        import rules_report_check as rrc
        rep_clubs = []
        seasons, comps = rrc.read_report(report, rep_clubs)
        diffs = (set(rrc.differences(seasons, comps, rrc.make_rules.read_rows()))
                 | rrc.division_differences(comps, rep_clubs))
        diffs = sorted(d for d in diffs if d[1] in ("clubs", "division_clubs") and own_difference(file, d[2]))
        # The report check reads the row of the game season; an Irish edition is a calendar year and plays the row of
        # its real season (edition 2017 row 2016), so where the two rows differ the edition checks above decide.
        diffs = [d for d in diffs if not (file in ("ire_prm", "ire_first") and edition_season(file, d[0]) != d[0])]
        # A short edition (shortages) differs from its row by the clubs the database lacks, and the log names it.
        # The report's season of a calendar-year league is the edition before (edition_row).
        def named(d):
            ed = d[0] + (1 if calendar_year(file) else 0)
            return ed in short and f"game {short[ed][0]}, table {short[ed][1]} " in d[2] + " "
        fails += [f"{file} {d[0]}: rules report difference: {d[2]}" for d in diffs if not named(d)]
        fails += rules_faults(report, file, short)
    print(f"{file}: editions in the report " + ", ".join(sorted({f"{ed}:{n}" for _s, _p, ed, n, _m, _x in rows})) +
          "; finished tables " + ", ".join(f"{ed}:{n}:{lo}-{hi}" for ed, n, _x, lo, hi in done) +
          "; splits " + ", ".join(f"{ed}:{'+'.join(map(str, v[1]))}" for ed, v in sorted(made.items())) +
          ("; loaded after a split " + ", ".join(map(str, sorted(loaded))) if loaded else ""))
    return fails


def save_in_split(program, game, year, last, db, nations, work, timeout):
    """--save-in-split: leg 0 plays to the first split of a league and saves after the first section date
    (RECOMP_SAVE_AFTER_SPLIT=1, src/recomp_rt/win/season_split.c and testrun.c); leg 1 loads that save (soak_run.load_leg) and plays to
    the end of --last. The logs and reports of both legs join in work for the checks of faults. The exit code of
    the first leg that fails, else 0."""
    import rules_report_check as rrc
    import smoke_run
    import soak_run
    leg0, leg1 = os.path.join(work, "leg0"), os.path.join(work, "leg1")
    code, _r = rrc.run(program, game, last, year, db, nations, [], leg0, timeout, {"changing_rules": "on"},
                       {"RECOMP_SIZE_LOG": "1", "RECOMP_SAVE_AFTER_SPLIT": "1"}, stop_on_box=True)
    save = os.path.join(leg0, "user", smoke_run.SAVE_NAME)
    if code != 0 or not os.path.exists(save):
        print(f"FAIL leg 0: code {code}, save {os.path.exists(save)} ({leg0})")
        return code or 1
    soak_run.LEG_ENV["RECOMP_SIZE_LOG"] = "1"
    o = {"program": program, "game": game, "rules": "on", "database": db, "nations": nations, "extra": [],
         "start": year, "last": last, "seed": 1, "timeout": timeout}
    code, _took, box = soak_run.load_leg(o, leg1, save, 0)
    if box:
        print(f"FAIL leg 1: {box}")
        return code or 1
    with open(os.path.join(work, "stderr.txt"), "w", encoding="latin-1") as out:
        for leg in (leg0, leg1):
            out.write(open(os.path.join(leg, "stderr.txt"), encoding="latin-1").read())
    # The report: leg 1 writes a start report of the season it loads in, which leg 0 already wrote. Its lines of a
    # season and phase that leg 0 holds stay out, so no club counts twice.
    with open(os.path.join(work, "report.tsv"), "w", encoding="latin-1") as out:
        seen = set()
        for k, leg in enumerate((leg0, leg1)):
            path = os.path.join(leg, "report.tsv")
            if not os.path.exists(path):
                continue
            mine = set()
            for line in open(path, encoding="latin-1"):
                p = line.split("\t")
                key = (p[1], p[2]) if len(p) > 2 and p[1].isdigit() else None
                if k and (key in seen or (key is None and line.startswith(("#", "columns")))):
                    continue
                if key:
                    mine.add(key)
                out.write(line)
            seen |= mine
    return code


def save_faults(work, file):
    """The checks of --save-in-split: leg 0 split and saved, leg 1 loaded the league after its split, and a table
    of that edition finished in leg 1 (the played check of faults reads it)."""
    leg0 = open(os.path.join(work, "leg0", "stderr.txt"), encoding="latin-1").read()
    leg1 = open(os.path.join(work, "leg1", "stderr.txt"), encoding="latin-1").read()
    made, _l = splits(leg0, file)
    _m, loaded = splits(leg1, file)
    fails = []
    if not made:
        fails.append(f"{file}: no split in leg 0")
    if "recomp: saved " not in leg0 or f"recomp: league sizes: {file} split " not in leg0 or \
            "a test save after section date 1" not in leg0:
        fails.append(f"{file}: leg 0 did not save after the first section date")
    if not loaded or not set(made) & loaded:
        fails.append(f"{file}: leg 1 loaded no edition after its split (split {sorted(made)}, loaded {sorted(loaded)})")
    if not any(ed in made for ed, *_x in played(leg1, file)):
        fails.append(f"{file}: no table of the split edition finished in leg 1")
    return fails


def rival_twice(season):
    """1 when the research list of a row meets each rival twice and draws no other club (2016, row 2015): no inter-zone
    row of the season (docs/research/arg-zones-0.7.2/primera.tsv) is a draw match."""
    import csv
    path = os.path.join(ROOT, "docs", "research", "arg-zones-0.7.2", "primera.tsv")
    label = f"{season}/{(season + 1) % 100:02d}"
    rows = [r for r in csv.DictReader(open(path, encoding="utf-8"), delimiter="\t")
            if r["season"] == label and r["kind"] == "interzone"]
    return bool(rows) and not any(re.search(r"fecha \d+, draw match", r["note"]) for r in rows)


def own_difference(file, text):
    """1 when a rules report difference ("<file> (<name>), row ...") is one of the file itself, not of a file whose
    name starts with it (tur_second and tur_second_b)."""
    return text.split(" ", 1)[0] == file


def arg_fixtures(log):
    """{(edition, half): [[(home, away)] a round]} of the fixture lines of the Argentine Primera."""
    out = {}
    tag = "recomp: league sizes: arg_prm fixtures "
    for line in log.split("\n"):
        if line.startswith(tag):
            head, _, rest = line[len(tag):].partition(":")
            ed, half = head.split()
            out[(int(ed), half)] = [[tuple(int(x) for x in m.split("-")) for m in r.split()]
                                    for r in rest.split("|")[1:]]
    return out


def arg_round_faults(name, rounds, n):
    fails = []
    for i, r in enumerate(rounds):
        clubs = [c for m in r for c in m]
        if len(r) != n // 2 or len(set(clubs)) != len(clubs):
            fails.append(f"{name} round {i + 1}: {len(r)} matches, {len(clubs) - len(set(clubs))} clubs twice")
    return fails


def arg_down(row):
    """The real relegation of a rules row of arg_prm: (by the averages, by the annual table). Research notes of 15
    November 2026 (RSSSF arg02 to arg2026, es and en Wikipedia); rows 2026 on take the 2027 format."""
    if row <= 2011:
        return 2, 0
    if row <= 2013:
        return 3, 0
    return {2014: (2, 0), 2015: (1, 0), 2016: (4, 0), 2017: (4, 0), 2018: (4, 0), 2019: (0, 0), 2020: (0, 0),
            2021: (2, 0), 2023: (0, 0)}.get(row, (1, 1))


def arg_title_faults(lines, year):
    fails = []
    stages, kos, champs, ended, ko_ids, played, gone = {}, {}, {}, set(), {}, {}, {}
    for x in lines:
        m = re.match(r"recomp: league sizes: arg_prm stages (\d+): row (\d+), (\d+) zone tables of (\d+)", x)
        if m:
            stages[int(m.group(1))] = (int(m.group(2)), int(m.group(3)), int(m.group(4)))
        m = re.match(r"recomp: league sizes: arg_prm (knockout apertura|knockout clausura|final|runners-up match) (\d+): "
                     r"stage (\d+) drawn on day \d+ of \d+, a round every (\d+) days:([^;]*);", x)
        if m:
            # Each tie is "home id:seed-away id:seed".
            ties = [((int(t.split("-")[0].split(":")[0]), int(t.split("-")[0].split(":")[1])),
                     (int(t.split("-")[1].split(":")[0]), int(t.split("-")[1].split(":")[1]))) for t in m.group(5).split()]
            kos[(int(m.group(2)), m.group(1))] = [(h[1], w[1]) for h, w in ties]
            ko_ids[(int(m.group(2)), int(m.group(3)))] = (m.group(1), ties)
        m = re.match(r"recomp: league sizes: arg_prm knockout match (\d+): stage (\d+) round (\d+): (\d+)-(\d+)$", x)
        if m:
            played.setdefault((int(m.group(1)), int(m.group(2))), []).append(
                (int(m.group(3)), int(m.group(4)), int(m.group(5))))
        m = re.match(r"recomp: league sizes: arg_prm champion (\d+) (apertura|clausura): .* \(the (knockout|final)\)", x)
        if m:
            champs[(int(m.group(1)), m.group(2))] = m.group(3)
        m = re.match(r"recomp: league sizes: arg_prm relegation (\d+):", x)
        if m:
            ended.add(int(m.group(1)))
            down = re.findall(r" (\d+):[\d.]+", x.split("by the averages:", 1)[1].split("; lowest", 1)[0])
            down += re.findall(r" (\d+):\d+", x.split("by the annual table:", 1)[1]) if "by the annual table:" in x else []
            gone[int(m.group(1))] = {int(c) for c in down}
        if "league sizes: arg_prm " in x and ("not played" in x or "no knockout" in x or "no final" in x or
                                               "no title stages" in x):
            fails.append("arg_prm: " + x.split("arg_prm ", 1)[1])
    # Art. 14.1.2 of the LPF reglamento 2025 and 2026: no club that goes down at the end of the season plays the
    # Clausura's knockout (season_arg_stages.c bar_relegated).
    for (ed, at), (what, ties) in sorted(ko_ids.items()):
        if what == "knockout clausura":
            barred = sorted({c for t in ties for c, _s in t} & gone.get(ed, set()))
            if barred:
                fails.append(f"arg_prm {ed} knockout clausura: {barred} go down but play it")
    # The clubs the draw left out are the clubs that went down at the end (the averages at the draw are those of the end).
    for x in lines:
        m = re.match(r"recomp: league sizes: arg_prm (\d+): (\d+) .* goes down; not in the Clausura knockout$", x)
        if m and int(m.group(1)) in gone and int(m.group(2)) not in gone[int(m.group(1))]:
            fails.append(f"arg_prm {m.group(1)}: {m.group(2)} left out of the Clausura knockout but did not go down")
    # The matches the knockouts played: round 0 is the drawn ties with the drawn home clubs, the later rounds before
    # the neutral ones have the better seed at home, and a complete knockout of 16 plays 15 matches.
    for (ed, at), (what, ties) in sorted(ko_ids.items()):
        seed = {c: s for t in ties for c, s in t}
        got = played.get((ed, at), [])
        drawn = {(h[0], w[0]) for h, w in ties}
        first = {(h, w) for code, h, w in got if len(ties) == 1 or code == 20}
        if first != drawn:
            fails.append(f"arg_prm {ed} {what}: first round played {sorted(first)}, drawn {sorted(drawn)}")
        table_form = not encoded_form("arg_prm", edition_season("arg_prm", ed))[2]
        home_rule = (120,) if table_form else (120, 130)
        for code, h, w in got:
            if code in home_rule and seed.get(h, 99) > seed.get(w, 99):
                fails.append(f"arg_prm {ed} {what}: round {code} {h} (seed {seed.get(h)}) at home to {w} "
                             f"(seed {seed.get(w)})")
        if ed in ended and len(got) != len(ties) * 2 - 1:
            fails.append(f"arg_prm {ed} {what}: {len(got)} matches, want {len(ties) * 2 - 1}")
    for ed in sorted(ended):
        row = edition_season("arg_prm", ed)
        _c, _m, zones, _e, _r, halves = encoded_form("arg_prm", row)
        if not zones and halves and _e:
            want = [(1, 16), (8, 9), (4, 13), (5, 12), (2, 15), (7, 10), (3, 14), (6, 11)]
            if ed not in stages or stages[ed][1] != 0:
                fails.append(f"arg_prm {ed}: stages {stages.get(ed)}, want no zone tables")
            for half in ("apertura", "clausura"):
                if kos.get((ed, "knockout " + half)) != want:
                    fails.append(f"arg_prm {ed} {half}: knockout seeds {kos.get((ed, 'knockout ' + half))}, want {want}")
                if champs.get((ed, half)) != "knockout":
                    fails.append(f"arg_prm {ed} {half}: no champion of the knockout")
            continue
        if not zones:
            # A season of rows 2002 to 2011 logs its stages line with no zone tables (the promocion's two ties).
            if ed in stages and stages[ed][1] != 0:
                fails.append(f"arg_prm {ed}: zone tables in a season of one table")
            continue
        if ed not in stages or stages[ed][1] != (4 if halves else 2):
            fails.append(f"arg_prm {ed}: zone tables {stages.get(ed)}, want {4 if halves else 2}")
        want_pairs = [(1, 8), (4, 5), (2, 7), (3, 6)] * 2
        for half in (("apertura", "clausura") if halves else ()):
            got = kos.get((ed, "knockout " + half))
            if got != want_pairs:
                fails.append(f"arg_prm {ed} {half}: knockout seeds {got}, want {want_pairs}")
            if champs.get((ed, half)) != "knockout":
                fails.append(f"arg_prm {ed} {half}: no champion of the knockout")
        if not halves:
            if kos.get((ed, "final")) != [(1, 1)] or kos.get((ed, "runners-up match")) != [(2, 2)]:
                fails.append(f"arg_prm {ed}: final {kos.get((ed, 'final'))}, runners-up match "
                             f"{kos.get((ed, 'runners-up match'))}")
            if champs.get((ed, "apertura")) != "final":
                fails.append(f"arg_prm {ed}: no champion of the final")
    return fails


def arg_promo_faults(lines, year, last):
    """The promoción of rows 2002 to 2011 (season_arg_stages.c promo_advance): the 17th and 18th of the averages (the
    two before the last two of the "averages" line) play the second division's two clubs after its direct places,
    the second division's club at home in the first leg and the Primera's in the second; each tie's loser of the
    Primera goes down and its winner goes up at the season change (the "chain" line: status 3 and status 0)."""
    fails, draws, worst, played, moved, chain, ties, ups, ended = [], {}, {}, {}, {}, {}, {}, {}, set()
    for x in lines:
        m = re.match(r"recomp: league sizes: arg_prm promocion (\d+): tie \d+: winner (\d+), loser (\d+)$", x)
        if m:
            ties.setdefault(int(m.group(1)), []).append((int(m.group(2)), int(m.group(3))))
        m = re.match(r"recomp: league sizes: arg_second (\d+): row \d+, .*, (\d+) up, \d+ down, no stages$", x)
        if m:
            ups[int(m.group(1))] = int(m.group(2))
        if re.match(r"recomp: league sizes: arg_prm relegation (\d+):", x):
            ended.add(int(x.split("relegation ")[1].split(":")[0]))
        m = re.match(r"recomp: league sizes: arg_prm promocion (\d+): stage (\d+) drawn on day \d+ of \d+: "
                     r"(\d+):(\d+)-(\d+):\d+ ", x)
        if m:
            # (second division club, Primera club, the second division club's place)
            draws.setdefault(int(m.group(1)), {})[int(m.group(2))] = (int(m.group(3)), int(m.group(5)), int(m.group(4)))
        m = re.match(r"recomp: league sizes: arg_prm averages (\d+), the worst 4 last:((?: \d+:[\d.]+)+)$", x)
        if m:
            worst[int(m.group(1))] = [int(w.split(":")[0]) for w in m.group(2).split()]
        m = re.match(r"recomp: league sizes: arg_prm knockout match (\d+): stage (\d+) round \d+: (\d+)-(\d+)$", x)
        if m:
            played.setdefault((int(m.group(1)), int(m.group(2))), []).append((int(m.group(3)), int(m.group(4))))
        m = re.match(r"recomp: league sizes: arg_prm promocion (\d+): (\d+) .* goes down, (\d+) .* goes up$", x)
        if m:
            moved.setdefault(int(m.group(1)), []).append((int(m.group(2)), int(m.group(3))))
        m = re.match(r"recomp: league sizes: arg_prm chain (\d+): primera:([^;]*); second:(.*)$", x)
        if m:
            down = {int(w.split(":")[0]) for w in m.group(2).split() if w.endswith(":3")}
            up = {int(w.split(":")[0]) for w in m.group(3).split() if w.split(":")[2] in ("0", "5")}
            chain[int(m.group(1))] = (down, up)
    for ed in range(year, last + 1):
        if not 2002 <= edition_season("arg_prm", ed) <= 2011 or ed not in worst:
            continue
        d = draws.get(ed, {})
        if len(d) != 2:
            fails.append(f"arg_prm {ed}: {len(d)} promocion ties drawn, want 2")
            continue
        w = worst[ed]
        want = [w[0], w[1]] if len(w) == 4 else []
        got = [d[k][1] for k in sorted(d)]
        if got != want:
            fails.append(f"arg_prm {ed}: promocion Primera clubs {got}, want the 17th and 18th of the averages {want}")
        places = [d[k][2] for k in sorted(d)]
        if places[1] != places[0] + 1 or (ed in ups and places != [ups[ed] + 1, ups[ed] + 2]):
            fails.append(f"arg_prm {ed}: promocion second division places {places}, want the two after its "
                         f"{ups.get(ed)} direct places")
        pairs = {}
        for k, (sec, prm, _place) in d.items():
            pairs[prm] = sec
            legs = played.get((ed, k), [])
            if legs and legs[0] != (sec, prm):
                fails.append(f"arg_prm {ed} promocion stage {k}: first leg {legs[0]}, want {(sec, prm)}")
            if len(legs) > 1 and legs[1] != (prm, sec):
                fails.append(f"arg_prm {ed} promocion stage {k}: second leg {legs[1]}, want {(prm, sec)}")
            if ed < last and len(legs) != 2:
                fails.append(f"arg_prm {ed} promocion stage {k}: {len(legs)} legs, want 2")
        # Each tie has a result at the season's end, and its two clubs are the drawn pair.
        drawn = {frozenset((sec, prm)) for sec, prm, _p in d.values()}
        if ed in ended:
            t = ties.get(ed, [])
            if len(t) != 2 or {frozenset(x) for x in t} != drawn:
                fails.append(f"arg_prm {ed}: promocion results {t}, want one for each drawn tie {sorted(map(sorted, drawn))}")
            want_moves = sorted((lo, wi) for wi, lo in t if lo in pairs)
            if sorted(moved.get(ed, [])) != want_moves:
                fails.append(f"arg_prm {ed}: promocion moves {sorted(moved.get(ed, []))}, want {want_moves}")
            if ed in chain:
                stay = {lo for wi, lo in t if lo not in pairs}
                if stay & chain[ed][1]:
                    fails.append(f"arg_prm {ed}: second division clubs {sorted(stay & chain[ed][1])} lost the "
                                 f"promocion but go up")
        for lo, wi in moved.get(ed, []):
            if pairs.get(lo) != wi:
                fails.append(f"arg_prm {ed}: promocion {lo} down and {wi} up, not a drawn tie")
            if ed in chain and (lo not in chain[ed][0] or wi not in chain[ed][1]):
                fails.append(f"arg_prm {ed}: promocion {lo} down and {wi} up, but the chain moves {chain[ed]}")
        if ed in chain:
            kept = set(pairs) - {lo for lo, _w in moved.get(ed, [])}
            if kept & chain[ed][0]:
                fails.append(f"arg_prm {ed}: promocion winners {sorted(kept & chain[ed][0])} go down")
        elif ed < last:
            fails.append(f"arg_prm {ed}: no chain line")
    return fails


def arg_faults(log, year):
    """The Argentine Primera (slice 15): each edition with a fixed list plays the form of its row. Zones: the clubs of
    each zone of the zones line meet once in a half, each club meets two clubs of the other zone (its rival and a
    drawn club) once each, the Clausura is the Apertura with the venues reversed, and in the start season every club,
    rival and drawn pair comes from the research list. Derby round: every pair meets once, the derby pairs twice,
    all in the derby round of the list."""
    fails = []
    lines = log.split("\n")
    fixtures = arg_fixtures(log)
    # The zones or derby line that each fixture line follows (an edition's plan is logged just before its lists).
    plan_of = {}
    last_plan = None
    for x in lines:
        m = re.match(r"recomp: league sizes: arg_prm zones (\d+): list (-?\d+), (\d+) clubs of the list, rivals (\d+) "
                     r"of the list, drawn (\d+) of the list, drawn round (\d+); A ([\d ]+); B ([\d ]+);", x)
        if m:
            last_plan = ("zones", int(m.group(3)), int(m.group(4)), int(m.group(5)), int(m.group(6)),
                         [int(c) for c in m.group(7).split()], [int(c) for c in m.group(8).split()])
        m = re.match(r"recomp: league sizes: arg_prm derby (\d+): list (-?\d+), (\d+) pairs of the list, (\d+) by "
                     r"replacement of (\d+), derby round (\d+); pairs ([\d\- ]+);", x)
        if m:
            last_plan = ("derby", int(m.group(3)), int(m.group(6)),
                         [tuple(int(c) for c in p.split("-")) for p in m.group(7).split()],
                         int(m.group(4)), int(m.group(5)))
        m = re.match(r"recomp: league sizes: arg_prm fixtures (\d+) (\w+):", x)
        if m:
            plan_of[(int(m.group(1)), m.group(2))] = last_plan
    seen = set()
    # Halves of one table (2027 on, gap row 49): the Apertura plays the first rounds of the plan and the Clausura the
    # rest, so the season's two lists make the one table and its derby round.
    for (ed, half) in list(fixtures):
        _c, _m, zones_e, extra_e, _r, halves_e = encoded_form("arg_prm", edition_season("arg_prm", ed))
        if half == "clausura" and halves_e and not zones_e and extra_e and (ed, "apertura") in fixtures:
            fixtures[(ed, "apertura")] = fixtures[(ed, "apertura")] + fixtures.pop((ed, "clausura"))
    for (ed, half), rounds in sorted(fixtures.items()):
        season = edition_season("arg_prm", ed)
        clubs, _m, zones, extra, want_rounds, halves = encoded_form("arg_prm", season)
        name = f"arg_prm {ed} {half}"
        seen.add(ed)
        fails += arg_round_faults(name, rounds, clubs)
        split = halves and not zones and extra
        if not split and len(rounds) != want_counts("arg_prm", season)[0]:
            fails.append(f"{name}: {len(rounds)} rounds, want {want_counts('arg_prm', season)[0]}")
        meet = {}
        for r in rounds:
            for h, a in r:
                meet[frozenset((h, a))] = meet.get(frozenset((h, a)), 0) + 1
        plan = plan_of.get((ed, half if half == "apertura" else "apertura"))
        if zones:
            if not plan or plan[0] != "zones":
                fails.append(f"{name}: no zones line before its fixtures")
                continue
            in_list, rivals, drawn, _at, za, zb = plan[1:]
            if len(za) != zones[0] or len(zb) != zones[1]:
                fails.append(f"{name}: zones of {len(za)} and {len(zb)}, want {zones}")
            for grp in (za, zb):
                for i, x in enumerate(grp):
                    for y in grp[i + 1:]:
                        if meet.get(frozenset((x, y)), 0) != 1:
                            fails.append(f"{name}: zone clubs {x} and {y} meet {meet.get(frozenset((x, y)), 0)} times")
            twice = rival_twice(season)
            for x in za + zb:
                other = zb if x in za else za
                inter = sum(meet.get(frozenset((x, y)), 0) for y in other)
                opponents = sum(1 for y in other if meet.get(frozenset((x, y))))
                if inter != extra or opponents != (1 if twice else extra):
                    fails.append(f"{name}: club {x} plays {inter} matches against {opponents} clubs of the other zone, "
                                 f"want {extra} against {1 if twice else extra}")
            if half == "clausura":
                ap = fixtures.get((ed, "apertura"), [])
                if [[(a, h) for h, a in r] for r in ap] != rounds:
                    fails.append(f"{name}: not the Apertura with the venues reversed")
            # A game started in its season: the club moves give the division the clubs of the row's list (D12), so
            # every club, rival and drawn pair comes from the list.
            if ed == year and (in_list, rivals, drawn) != (clubs, zones[0], zones[0]):
                fails.append(f"{name}: {in_list} clubs, {rivals} rivals and {drawn} drawn pairs of the list, want "
                             f"{clubs}, {zones[0]} and {zones[0]} (a game started in its season)")
            if not halves and half == "clausura":
                fails.append(f"{name}: a Clausura in a row of one table")
        elif extra:
            if split and len(rounds) != clubs:
                fails.append(f"{name}: {len(rounds)} rounds in both halves, want {clubs}")
            if not plan or plan[0] != "derby":
                fails.append(f"{name}: no derby line before its fixtures")
                continue
            from_list, at, pairs, replaced, orphans = plan[1:]
            derby = {frozenset(p) for p in pairs}
            # A club of no list takes the partner of the club it replaced (gap row 50): each listed club whose
            # partner is not in the division gets a club of no list.
            if replaced != orphans:
                fails.append(f"{name}: {replaced} derby pairs by replacement, want {orphans} (one for each listed club "
                             f"whose partner is not in the division)")
            ids = {c for r in rounds for m in r for c in m}
            for x in ids:
                for y in ids:
                    if x < y and meet.get(frozenset((x, y)), 0) != 1 + (frozenset((x, y)) in derby):
                        fails.append(f"{name}: clubs {x} and {y} meet {meet.get(frozenset((x, y)), 0)} times")
            if at - 1 < len(rounds) and {frozenset(m) for m in rounds[at - 1]} != derby:
                fails.append(f"{name}: round {at} is not the derby round")
            if ed == season == year and from_list != clubs // 2:
                fails.append(f"{name}: {from_list} derby pairs of the list, want {clubs // 2} (a game started in its "
                             f"season)")
    for ed in range(year, year + 2):
        clubs, _m, zones, extra, _r, _h = encoded_form("arg_prm", edition_season("arg_prm", ed))
        if (zones or extra) and ed not in seen and any(f"arg_prm {ed}:" in x or f" {ed}, the class makes" in x
                                                         for x in lines if "league sizes: arg_prm" in x):
            fails.append(f"arg_prm {ed}: no fixture line of its zones or derby round")
    fails += [f"arg_prm: {x.split(': ', 1)[1]}" for x in lines if "arg_prm fixed list not played" in x]
    # The size step sends down the clubs with the worst averages (promedios): each order line lists the clubs best
    # first with the average of the season just played; it must descend, and each club that the step then sends down
    # from the Primera (before the next order line) is among the last of it (the two that honours sent down are
    # already gone, so 2 more).
    order, down = None, []

    def check_block():
        if order is None:
            return [f"arg_prm: step: {d} goes down with no order by the averages" for d in down]
        out = []
        vals = [v for _n, v in order if v is not None]
        if any(b > a + 1e-6 for a, b in zip(vals, vals[1:])) or (None in [v for _n, v in order] and
                                                                  any(v is not None for _n, v in
                                                                      order[[v for _n, v in order].index(None):])):
            out.append(f"arg_prm: step order not best first by the averages: {order}")
        worst = [n[:50] for n, _v in order[-(len(down) + 2):]] if down else []
        out += [f"arg_prm: step: {d} goes down, not one of the {len(down) + 2} worst averages {worst}"
                for d in down if d[:50] not in worst]
        return out
    for x in lines:
        m = re.match(r"recomp: league sizes: arg_prm step order \d+ by the averages: (.*)$", x)
        if m:
            if order is not None or down:
                fails += check_block()
            order, down = [], []
            for w in m.group(1).split("|"):
                p = w.strip().split(":")
                if len(p) >= 3:
                    v = p[-1].strip()
                    order.append((":".join(p[1:-1]).strip(), None if v.startswith("none") else float(v)))
            continue
        m = re.match(r"recomp: league sizes: step: (.*) from Argentine Premier Division to ", x)
        if m:
            down.append(m.group(1).strip())
    if order is not None or down:
        fails += check_block()
    # Gap row 54: the clubs of each row's real relegation go down, the worst averages first, then the worst club of
    # the annual table that the averages kept (season_sizes_arg.c arg_down).
    # The clubs sent down are not in the division of the next edition (its zones or derby line, which lists every id),
    # unless the next edition grows: then the size step fills it from the Nacional B by the averages, and a club sent
    # down may come back at once (real life: Argentinos Juniors and Colon, down in 2013/14, played the Primera of 30 in
    # 2015). The size of an edition is the length of its step order line, which lists every club.
    next_ids, size = {}, {}
    for x in lines:
        m = re.match(r"recomp: league sizes: arg_prm step order (\d+) by the averages: (.*)$", x)
        if m:
            size[int(m.group(1))] = len([w for w in m.group(2).split("|") if w.strip().count(":") >= 2])
        m = re.match(r"recomp: league sizes: arg_prm zones (\d+): .*?; A ([\d ]+); B ([\d ]+);", x)
        if m:
            next_ids[int(m.group(1))] = {int(c) for c in (m.group(2) + " " + m.group(3)).split()}
        m = re.match(r"recomp: league sizes: arg_prm derby (\d+): .*?; pairs ([\d\- ]+);", x)
        if m:
            next_ids[int(m.group(1))] = {int(c) for p in m.group(2).split() for c in p.split("-")}
    for x in lines:
        m = re.match(r"recomp: league sizes: arg_prm relegation (\d+): row (\d+), (\d+) by the averages:([^;]*); "
                     r"lowest average kept (\S+); (\d+) by the annual table:([^;]*) of (\d+)$", x)
        if not m:
            continue
        ed, row, n_avg, n_tab = int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(6))
        want = arg_down(row)
        if (n_avg, n_tab) != want:
            fails.append(f"arg_prm {ed}: {n_avg} down by the averages and {n_tab} by the annual table, want {want} "
                         f"(row {row})")
        gone = [int(w.split(":")[0]) for w in (m.group(4) + " " + m.group(7)).split()]
        stay = [c for c in gone if c in next_ids.get(ed + 1, set())]
        grows = ed in size and len(next_ids.get(ed + 1, ())) > size[ed]
        if stay and not grows:
            fails.append(f"arg_prm {ed}: {stay} sent down but in the division of {ed + 1}")
        kept = float(m.group(5))
        for w in m.group(4).split():
            if float(w.split(":")[1]) > kept + 1e-6:
                fails.append(f"arg_prm {ed}: {w} goes down by the averages above the lowest average kept {kept}")
        for w in m.group(7).split():
            if int(w.split(":")[1]) < int(m.group(8)) - 1 - n_avg:
                fails.append(f"arg_prm {ed}: {w} goes down by the annual table, not one of its last places")
    # Gap row 48: a zone season has the zone tables of each half and a knockout of 16 after each half (2025 and 2026),
    # or a final of the zone winners and a match of the runners-up (2016); the knockout's winner is the champion.
    fails += arg_title_faults(lines, year)
    fails += arg_promo_faults(lines, year, year + 1)
    if not any("league sizes: arg_prm relegation " in x for x in lines):
        fails.append("arg_prm: no relegation line (the end of a season)")
    print("arg_prm: fixed lists " + ", ".join(f"{ed} {h}: {len(r)} rounds" for (ed, h), r in sorted(fixtures.items())))
    for x in lines:
        if "league sizes: arg_prm " in x and "not in the division:" in x:
            print("arg_prm: " + x.split(": ", 1)[1].split("; ")[0] + "; not in the division:" +
                  x.split("not in the division:", 1)[1].split(";")[0])
    return fails


def self_test():
    """The template checks on made-up templates (no game)."""
    d = datetime.date
    cls = [d(2009, 4, 5), d(2009, 4, 12), d(2009, 5, 31), d(2009, 7, 26), d(2009, 8, 2)]
    good = [(cls[0], False), (d(2009, 4, 8), True), (cls[1], False), (cls[2], False), (cls[3], False),
            (d(2009, 7, 29), True), (cls[4], False)]
    bad_break = [(cls[0], False), (cls[1], False), (cls[2], False), (d(2009, 6, 10), True), (cls[3], False),
                 (cls[4], False)]
    shrink = [(cls[0], False), (cls[2], False), (cls[4], False)]
    late = [(cls[0], False), (cls[1], False), (cls[2], False), (cls[3], False), (cls[4], False),
            (d(2009, 8, 5), True)]
    shared = [(cls[0], False), (cls[0], True), (cls[1], False), (cls[2], False), (cls[3], False), (cls[4], False)]
    thursday = [(cls[0], False), (d(2009, 4, 9), True), (cls[1], False), (cls[2], False), (cls[3], False),
              (cls[4], False)]
    checks = [
        (template_faults("good", cls, good, 7), False),
        (template_faults("shared", cls, shared, 6), True),
        (template_faults("thursday", cls, thursday, 6), True),
        (template_faults("break", cls, bad_break, 6), True),
        (template_faults("shrink", cls, shrink, 3), False),
        (template_faults("late", cls, late, 6), True),
        (template_faults("count", cls, good, 8), True),
        # Gap 37: no club joins the Veikkausliiga from no division; the named club leaves after 2005.
        (step_faults("recomp: league sizes: step: AC Vantaan Allianssi from Veikkausliiga to no division\n"
                     "recomp: league sizes: step: FC Hieno from Ykkonen to Veikkausliiga\n"
                     "recomp: league sizes: step: fin_prm (Veikkausliiga) 2006: 13 clubs, the row 13", "fin_prm"), False),
        (step_faults("recomp: league sizes: step: FC KooTeePee from Veikkausliiga to no division\n"
                     "recomp: league sizes: step: fin_prm (Veikkausliiga) 2006: 13 clubs, the row 13", "fin_prm"), True),
        (step_faults("recomp: league sizes: step: FC KooTeePee from no division to Veikkausliiga\n"
                     "recomp: league sizes: step: fin_prm (Veikkausliiga) 2007: 14 clubs, the row 14", "fin_prm"), True),
        # Gap 35: the Superliga groups after the split (places 7, 10, 11, 14 and 8, 9, 12, 13); the contiguous form
        # of 7 to 10 and 11 to 14 is a fault.
        (end_faults("groups", [(p, 36) for p in range(6)] + [(p, 32) for p in (6, 9, 10, 13, 7, 8, 11, 12)], 26,
                    [(6, 2), (4, 2), (4, 2)], GROUPS[("den_prm", 2016, 2019)]), False),
        (end_faults("contiguous", [(p, 36) for p in range(6)] + [(p, 32) for p in range(6, 14)], 26,
                    [(6, 2), (4, 2), (4, 2)], GROUPS[("den_prm", 2016, 2019)]), True),
        # A class date listed twice, the first out of order (8 March, 26 April, 22 March, 26 April): the Wednesday of
        # 11 March is in a gap of 14 days, not in a false one of 49.
        (template_faults("twice", [d(2009, 3, 1), d(2009, 3, 8), d(2009, 4, 26), d(2009, 3, 22), d(2009, 4, 26)],
                         [(d(2009, 3, 1), False), (d(2009, 3, 8), False), (d(2009, 3, 11), True),
                          (d(2009, 3, 22), False), (d(2009, 4, 26), False)], 5), False),
    ]
    # Gap row 7: the MLS games of the plan table, the plan line and the shortage of a 2023 database.
    games = mls_games()
    if (games.get(2002), games.get(2025)) != (28, 34):
        checks.append((["mls_games"], False))
    mls_log = ("recomp: mls: 2025: 29 clubs, the row 30: the database has no other club of the division\n"
               "recomp: mls: 2025: 29 clubs (East 15, West 14), 493 matches, 34 to 34 games, 37 rounds from day 80 "
               "to 248\nrecomp: mls: 2025: final A v B\n")
    checks += [
        (mls_faults(mls_log, [], [], "usa_mls", 2025, 2025, lambda ed: (29, 0), games), False),
        (mls_faults(mls_log, [], [], "usa_mls", 2025, 2025, lambda ed: (30, 0), games), True),
        (mls_faults(mls_log + "recomp: mls: stage 2, West tie 0: 0 clubs go on (x)\n", [], [], "usa_mls", 2025,
                    2025, lambda ed: (29, 0), games), True),
        (mls_faults(mls_log, [], [], "usa_mls", 2025, 2026, lambda ed: (29, 0), games), True),
        # 0.7.3 slice 6: the log line of a count that differs from the row excuses no edition.
        ([] if shortages(mls_log, "usa_mls") == {} else ["shortage"], False),
        ([] if shortages("recomp: mls: 2002: 12 clubs, the row 10: a club that folds has a human manager\n",
                         "usa_mls") == {} else ["fold"], False),
        (mls_faults(mls_log.replace("2025", "2027"), [(2026, "end", 2027, 29, 0, 34)], [], "usa_mls", 2027, 2027,
                    lambda ed: (29, 0), games), False),
    ]
    # A template of 7-day gaps only (the Brazilian classes) has no winter break: a Wednesday in a gap is good, and the
    # earlier starts go up to the class's limit.
    week = [d(2010, 8, 1), d(2010, 8, 8), d(2010, 8, 15)]
    wed = [(d(2010, 7, 4), True), (d(2010, 8, 1), False), (d(2010, 8, 4), True), (d(2010, 8, 8), False),
           (d(2010, 8, 11), True), (d(2010, 8, 15), False)]
    checks += [
        (template_faults("weeks", week, wed, 6, 13), False),
        (template_faults("weeks limit", week, wed, 6), False),
        (template_faults("weeks far", week, [(d(2010, 4, 25), True)] + wed[1:], 6, 13), True),
    ]
    bad = [(i, f) for i, (f, want) in enumerate(checks) if bool(f) != want]
    line = "recomp: league sizes: nor_prm template 2009: class 2 2009-04-05 2009-04-12"
    line2 = "recomp: league sizes: nor_prm template 2009: new 3 2009-04-05 2009-04-08+ 2009-04-12"
    t = templates(line + "\n" + line2, "nor_prm")
    if not (len(t) == 1 and len(t[0][1]) == 2 and t[0][2][1] == (d(2009, 4, 8), True)):
        bad.append(("templates", t))
    p = played("recomp: league sizes: nor_prm played 2009: 16 clubs, 30 matches, played 30 to 30 (Eliteserien)",
               "nor_prm")
    if p != [(2009, 16, 30, 30, 30)]:
        bad.append(("played", p))
    if (encoded_row("nor_prm", 2009) != (16, 2) or encoded_row("por_second", 2014) != (24, 2) or
            edition_season("arg_prm", 2015) != 2015 or edition_season("arg_prm", 2024) != 2024 or
            edition_row("nor_prm", 2008) != (14, 2) or edition_row("nor_prm", 2009) != (16, 2) or
            edition_row("por_second", 2014) != (24, 2)):
        bad.append(("encoded", encoded_row("nor_prm", 2009), encoded_row("por_second", 2014)))
    # Slice 12: Russia changes from calendar years (edition 2006 plays row 2005, real season 2006) to two-year
    # seasons (edition 2012 plays row 2012, real season 2012–13).
    # Gap row 2: the Irish editions follow the calendar move (season_rules_row_of): 2002 the short season, 2003 the
    # second row of season 2002 (41b, 42b), 2004 row 2003. The short season's template takes the class's dates before
    # the cut.
    ire = [(f, y, edition_row(f, y), edition_counts(f, y)) for f, y in
           (("ire_prm", 2002), ("ire_prm", 2003), ("ire_prm", 2004), ("ire_first", 2002), ("ire_first", 2003),
            ("ire_first", 2004))]
    if [x[2:] for x in ire] != [((10, 3), (27, 27)), ((10, 4), (36, 36)), ((10, 4), (36, 36)),
                                ((12, 2), (22, 22)), ((12, 3), (33, 33)), ((12, 3), (33, 33))]:
        bad.append(("Irish edition rows", ire))
    if (real_season("ire_prm", edition_season("ire_prm", 2003)) != "2003" or
            "short season" not in real_season("ire_first", edition_season("ire_first", 2002)) or
            edition_row("ire_prm", 2001) != (12, 3)):
        bad.append(("Irish real seasons", [real_season(f, edition_season(f, y)) for f in ("ire_prm", "ire_first")
                                           for y in (2001, 2002, 2003)]))
    if (edition_row("rus_first", 2006) != (22, 2) or edition_row("rus_first", 2012) != (17, 2) or
            edition_row("rus_first", 2013) != (19, 2) or edition_row("tur_first", 2020) != (21, 2) or
            edition_row("tur_second", 2023) != (18, 2)):
        bad.append(("edition rows", [(f, y, edition_season(f, y), edition_row(f, y)) for f, y in
                                     (("rus_first", 2006), ("rus_first", 2012), ("rus_first", 2013),
                                      ("tur_first", 2020), ("tur_second", 2023))]))
    # The split of an edition takes the row of edition_season (slices 7 and 12).
    if (edition_split("wel_first", 2026) != encoded_split("wel_first", 2026) or
            edition_split("nir_prm", 2010) != encoded_split("nir_prm", 2010) or edition_split("rus_first", 2006)):
        bad.append(("edition split", "wel_first 2026, nir_prm 2010, rus_first 2006"))
    # The long 2011-12 Russian season (slice 12, lead decision 16 November): edition 2011 plays row 2011 and no
    # split; the split of the first stage row (2010) is a gap.
    if edition_season("rus_first", 2011) != 2011 or edition_split("rus_first", 2011) or encoded_split("rus_first", 2010):
        bad.append(("rus_first 2011", edition_season("rus_first", 2011), edition_split("rus_first", 2011)))
    # A difference of tur_second_b (TFF 2. Lig) is not one of tur_second.
    if (own_difference("tur_second", "tur_second_b (TFF 2. Lig), row rus-tur-239: game 10, table 38 (38)") or
            not own_difference("tur_second", "tur_second (TFF 1. Lig), row rus-tur-1: game 20, table 18 (18)")):
        bad.append(("own difference", "tur_second"))
    # The Clausura knockout bar (art. 14.1.2): a club that goes down must not be in the Clausura's draw.
    bar = ["recomp: league sizes: arg_prm knockout clausura 2025: stage 9 drawn on day 300 of 2026, a round every 7 "
           "days: 11:1-12:8 13:4-14:5;",
           "recomp: league sizes: arg_prm relegation 2025: row 2025, 1 by the averages: 14:0.7000; lowest average "
           "kept 0.8000; 1 by the annual table: 15:30 of 30"]
    got = [f for f in arg_title_faults(bar, 2025) if "go down but play it" in f]
    if got != ["arg_prm 2025 knockout clausura: [14] go down but play it"]:
        bad.append(("clausura bar", got))
    # Slice 9: the Italian rows of the runs.
    for file, season, want in (("ita_ser_a", 2005, (20, 2)), ("ita_ser_b", 2003, (24, 2)), ("ita_ser_b", 2018, (19, 2)),
                               ("ita_ser_c1a", 2004, (19, 2)), ("ita_ser_c1a", 2012, (17, 2)),
                               ("ita_ser_c1b", 2012, (16, 2)), ("ita_ser_a", 2003, (18, 2))):
        if edition_row(file, season) != want:
            bad.append(("encoded", file, season, edition_row(file, season), want))
    import rules_report_check as rrc
    if any(f not in ROWS or x not in rrc.EXTRA for f, xs in EXTRA.items() for x in xs):
        bad.append(("extra", EXTRA))
    if (encoded_split("nir_prm", 2010) != (33, [(6, 1), (6, 1)]) or
            encoded_split("wel_first", 2026) != (30, [(6, 1), (4, 1), (6, 1)]) or
            encoded_split("wel_first", 2012) != (22, [(6, 2), (6, 2)]) or encoded_split("wel_first", 2005) is not None or
            split_numbers([(6, 1), (4, 1), (6, 1)]) != (5, 3, 5) or split_numbers([(6, 2), (6, 2)]) != (10, 10, 10)):
        bad.append(("split", encoded_split("nir_prm", 2010), encoded_split("wel_first", 2026)))
    # Slice 10: the rows, the splits of the hand rows and the periods lines.
    if (edition_row("hol_first", 2005) != (20, 2) or edition_row("bel_second", 2016) != (8, 4) or
            edition_row("fin_prm", 2006) != (13, 2) or edition_row("den_prm", 2017) != (14, 2) or
            encoded_split("bel_first", 2010) != (30, [(6, 2), (4, 2), (4, 2), (2, 5)]) or
            encoded_split("den_prm", 2017) != (26, [(6, 2), (4, 2), (4, 2)]) or
            edition_split("fin_prm", 2019) != (22, [(6, 1), (6, 1)]) or edition_split("fin_prm", 2020) is not None or
            encoded_periods("bel_second", 2008) != 0 or encoded_periods("hol_first", 2005) != 6):
        bad.append(("slice 10 rows", edition_row("hol_first", 2005), edition_row("bel_second", 2016),
                    encoded_split("bel_first", 2010), encoded_split("den_prm", 2017)))
    plog = ("recomp: league sizes: hol_first periods 2005: rounds 38, lengths 6+6+6+6+6+6, winners on\n"
            "recomp: league sizes: hol_first 2005: period 5 named Fifth Period\n"
            "recomp: league sizes: hol_first 2005: period 6 named Sixth Period\n"
            "recomp: league sizes: bel_second periods 2008: rounds 38, lengths 11+14+13, winners off\n"
            "recomp: league sizes: bel_second periods 2009: rounds 38, lengths 11+14+12, winners on")
    if (period_lines(plog, "hol_first") != {2005: (38, [6, 6, 6, 6, 6, 6], "on")} or
            len(period_faults("recomp: league sizes: hol_first periods 2005: rounds 38, lengths 9+9+11+9, winners on",
                              "hol_first", {2005})) != 3 or     # the lengths, and periods 5 and 6 not named
            period_faults(plog, "hol_first", {2005}) or period_faults(plog, "bel_second", {2008}) or
            len(period_faults(plog, "bel_second", {2009})) != 2 or period_faults(plog, "den_prm", {2017}) or
            period_faults("recomp: league sizes: hol_first periods 2001: rounds 34, lengths 8+8+10+8, winners on",
                          "hol_first", {2001}) != ["hol_first 2001: period winners on, the row has 0 periods"] or
            period_faults(plog, "hol_first", {2006}) != ["hol_first 2006: no periods line"] or
            period_faults(plog.replace("Sixth", "Fourth"), "hol_first", {2005}) !=
            ["hol_first 2005: period 6 not named Sixth Period"]):
        bad.append(("periods", period_lines(plog, "hol_first"), period_faults(plog, "bel_second", {2009})))
    sl = splits("recomp: league sizes: wel_first split 2026: after 30, sections 6+4+6, meetings 1+1+1, rounds 5+3+5, "
                "5 dates, points kept (Welsh Premier)\nrecomp: league sizes: wel_first split 2026: loaded after the "
                "split (Welsh Premier)", "wel_first")
    if sl != ({2026: (30, [6, 4, 6], [1, 1, 1])}, {2026}):
        bad.append(("splits", sl))
    good_end = [(0, 35), (2, 35), (1, 35), (5, 35), (4, 35), (3, 35), (6, 33), (7, 33), (9, 33), (8, 33), (10, 35),
                (11, 35), (12, 35), (13, 35), (15, 35), (14, 35)]
    sp16 = [(6, 1), (4, 1), (6, 1)]
    mixed = good_end[:5] + [(6, 33), (3, 35)] + good_end[7:]
    short = good_end[:6] + [(6, 35)] + good_end[7:]
    if (end_faults("good", good_end, 30, sp16) or not end_faults("mixed", mixed, 30, sp16) or
            not end_faults("short", short, 30, sp16) or
            end_faults("again", good_end[:7] + [(6, 33)] + good_end[8:], 30, sp16) or
            not end_faults("crossed", good_end[:5] + [(6, 35)] + good_end[6:], 30, sp16) or
            split_ends("recomp: league sizes: wel_first split end 2026: 0:35 1:33", "wel_first") != {2026: [(0, 35), (1, 33)]}):
        bad.append(("end", end_faults("good", good_end, 30, sp16)))
    # Slice 11: the section meetings of the Greek and Croatian splits (encode_leagues.py SPLIT_MEET_BY_HAND).
    if (encoded_split("gre_prm", 2019) != (26, [(6, 2), (8, 1)]) or
            encoded_split("gre_prm", 2024) != (26, [(4, 2), (4, 2), (6, 2)]) or
            encoded_split("cro_a1", 2004) != (22, [(6, 2), (6, 2)]) or
            encoded_split("gre_second", 2020) != (22, [(6, 1), (6, 1)]) or encoded_split("gre_prm", 2018) is not None or
            encoded_row("cro_a1", 2013) != (10, 4) or encoded_row("cro_a2a", 2020) != (18, 2)):
        bad.append(("encoded cro gre", encoded_split("gre_prm", 2019), encoded_split("gre_prm", 2024)))
    # Slice 11 gap rows 38 to 41: the encoded splits and points, and the Croatian regroup check on made-up logs.
    if (encoded_split("cro_a2b", 2003) != (22, [(6, 2), (6, 2)]) or encoded_points("cro_a2a", 2003) != "kept" or
            encoded_split("gre_second", 2023) != (22, [(5, 2), (6, 2), (1, 0)]) or
            encoded_split("gre_second", 2025) != (18, [(4, 2), (6, 2)]) or
            encoded_points("gre_second", 2024) != "halved" or
            encoded_points("gre_prm", 2024) != "kept+halved+kept" or encoded_points("gre_prm", 2019) != "kept" or
            south_held(2005) is not True or south_held(2006) is not False):
        bad.append(("encoded gap rows", encoded_split("gre_second", 2023), encoded_points("gre_prm", 2024)))
    tag = "recomp: league sizes: "
    two = tag + "cro_a2a regroup 2003: 32 clubs, 12 North, 12 South, 8 to A Lower Division, by latitude\n"
    one = (tag + "cro_a2b 2009: not held, no exchange with the First Division\n" + tag +
           "cro_a2a regroup 2010: 30 clubs, 16 North, 0 South, 14 to A Lower Division, by results\n" + tag +
           "cro_a2b 2010: not held, the South waits\n")
    swapped = one.replace(tag + "cro_a2b 2009: not held, no exchange with the First Division\n",
                          tag + "cro_a1 (Prva HNL) down 0, cro_a2b (Druga HNL South) up 1, pairs 1\n")
    if (cro_faults(two) or cro_faults(one) or len(cro_faults(swapped)) != 2 or
            not cro_faults(two.replace("12 South", "16 South")) or not cro_faults(one.replace("results", "latitude"))
            or not cro_faults(one.replace("0 South", "8 South"))
            or not cro_faults(one.replace(tag + "cro_a2b 2010: not held, the South waits\n", ""))
            or not cro_faults(one + tag + "cro_a2b (Druga HNL South) 2010: 14 clubs in the division, 16 in the table\n")
            or not cro_faults(one + tag + "cro_a2b (Druga HNL South) 2010: 16 clubs in the division, 16 in the "
                              "table\n", 2010)
            or not cro_faults(one, 2010)
            or cro_faults(one + tag + "cro_a2b 2010: not held, the South's 16 clubs start in A Lower Division\n",
                          2010)):
        bad.append(("cro regroup", cro_faults(two), cro_faults(one), cro_faults(swapped)))
    pts = tag + "gre_prm split 2024: after 26, sections 4+4+6, meetings 2+2+2, rounds 6+6+10, 10 dates, points " \
                "kept+halved+kept (Super League)"
    if split_points_logged(pts, "gre_prm") != {2024: "kept+halved+kept"}:
        bad.append(("split points line", split_points_logged(pts, "gre_prm")))
    sh = shortages("recomp: league sizes: step: kor_league (K League) 2004: 12 clubs, the row 13: no club of its "
                   "nation with no division or in a spare division\nrecomp: league sizes: step: kor_league (K League) "
                   "2005: 12 clubs, the row 13\nrecomp: league sizes: step: jap_j2 (J2) 2006: 12 clubs, the row 13: no "
                   "club of its nation with no division or in a spare division\nrecomp: league sizes: step: kor_league (K League) "
                   "2006: 12 clubs, the row 13: the division below has no club to give", "kor_league")
    if sh != {2004: (12, 13)}:
        bad.append(("shortages", sh))
    # Odd sections (the K League 2013 log): a place written again inside its own section passes, one across fails.
    sp7 = [(7, 2), (7, 2)]
    kor_end = [(p, 38) for p in (0, 2, 3, 2, 4, 5, 5, 8, 9, 11, 10, 9, 12, 13)]
    across = [(p, 38) for p in (0, 2, 3, 2, 4, 5, 8, 8, 9, 11, 10, 9, 12, 13)]
    if end_faults("kor", kor_end, 26, sp7) or not end_faults("across", across, 26, sp7):
        bad.append(("end odd", end_faults("kor", kor_end, 26, sp7), end_faults("across", across, 26, sp7)))
    ex = "recomp: league sizes: ire_prm (League of Ireland Premier Division) down %d, ire_first (League of Ireland " \
         "First Division) up %d, pairs 0, expected -1 up -1 down%s"
    if (exchange_faults(ex % (2, 2, ""), "ire_first", 2013) or exchange_faults(ex % (1, 1, ""), "ire_first", 2013) or
            not exchange_faults(ex % (1, 2, ": in pairs"), "ire_first", 2013) or
            not exchange_faults(ex % (0, 1, ": in pairs"), "ire_first", 2013) or
            not exchange_faults(ex % (0, 0, ""), "ire_first", 2013) or not exchange_faults("", "ire_first", 2013) or
            exchange_faults("", "nir_first", 2013) or exchange_faults(ex % (3, 1, ": in pairs"), "ire_first", 2016)):
        bad.append(("exchange", exchange_faults(ex % (1, 2, ": in pairs"), "ire_first", 2013)))
    own = ex % (3, 1, ": each club moves on its own")
    nopo = "recomp: league sizes: ire_first 2017: no play-off with the Premier Division"
    if (irl_change_faults(own + "\n" + nopo, "ire_first", 2017) or irl_change_faults(own + "\n" + nopo, "ire_prm", 2017)
            or len(irl_change_faults("", "ire_prm", 2017)) != 2
            or len(irl_change_faults(ex % (1, 1, ": in pairs") + "\n" + nopo, "ire_first", 2017)) != 1
            or len(irl_change_faults(own, "ire_first", 2017)) != 1
            or len(irl_change_faults(own + "\n" + nopo, "ire_first", 2008)) != 1
            or len(irl_change_faults(nopo.replace("2017", "2013"), "ire_first", 2013)) != 1
            or len(irl_change_faults(nopo.replace("2017", "2009"), "ire_prm", 2008)) != 3
            or irl_change_faults(nopo.replace("2017", "2004"), "ire_prm", 2003)
            or len(irl_change_faults(nopo.replace("2017", "2003"), "ire_prm", 2003)) != 1
            or irl_change_faults(ex % (1, 1, ""), "ire_first", 2013) or irl_change_faults("", "nir_first", 2017)
            # Item 251b: a 2010 start played to 2018 needs the 2017 exchange on its own, and no line in 2008.
            or irl_change_faults(own + "\n" + nopo, "ire_first", 2010, 2018)
            or len(irl_change_faults(ex % (3, 1, ": in pairs") + "\n" + nopo, "ire_first", 2010, 2018)) != 1
            or len(irl_change_faults(own, "ire_first", 2003, 2018)) != 4
            or irl_change_faults(own + "\n" + nopo, "ire_first", 2010, 2017)
            or len(irl_change_faults(nopo.replace("2017", "2012"), "ire_first", 2010, 2018)) != 3):
        bad.append(("irl change", irl_change_faults("", "ire_prm", 2017)))
    if encoded_row("bra_nat_first", 2002) != (24, 2) or encoded_row("bra_nat_second", 2004) != (22, 1):
        bad.append(("encoded bra", encoded_row("bra_nat_first", 2002), encoded_row("bra_nat_second", 2004)))
    off = ("recomp: league sizes: bra_nat_first 2011: 2 meetings, no play-off\n"
           "recomp: league sizes: bra_nat_first 2011: champion club 1234 (Série A)")
    if no_playoff_faults(off, "bra_nat_first", 2) or len(no_playoff_faults("", "bra_nat_first", 2)) != 2 or \
            no_playoff_faults("", "bra_nat_first", 1) or no_playoff_faults("", "nor_prm", 2):
        bad.append(("no play-off", no_playoff_faults(off, "bra_nat_first", 2)))
    top8 = ("recomp: league sizes: bra_nat_second 2003: play-off of places 1 to 8 drawn on day 335, the two finalists\n"
            "recomp: league sizes: bra_nat_second 2003: Club A wins the play-off final and goes up\n"
            "recomp: league sizes: bra_nat_second 2003: place 5 (Club B) goes up, the loser of the play-off final")
    ties = ("recomp: league sizes: bra_nat_second 2026: play-off of places 3 to 6 drawn on day 330, 3rd v 6th\n"
            "recomp: league sizes: bra_nat_second 2026: place 3 (Club C) goes up, the winner of its play-off tie")
    late = ("recomp: league sizes: bra_nat_second 2004: 9 days to the season change, no time for the play-off; places "
            "1 and 2 go up\n"
            "recomp: league sizes: bra_nat_second 2004: place 1 (Club D) goes up, no play-off\n"
            "recomp: league sizes: bra_nat_second 2004: place 2 (Club E) goes up, no play-off")
    if (playoff_faults(top8, "bra_nat_second", {2003}) or len(playoff_faults(ties, "bra_nat_second", {2026})) != 1 or
            len(playoff_faults(late, "bra_nat_second", {2004})) != 2 or
            playoff_faults(top8, "bra_nat_second", {2006, 2025}) or playoff_faults("", "bra_nat_first", {2003}) or
            [bra_playoff("bra_nat_second", y) for y in (2001, 2002, 2005, 2006, 2025, 2026, 2030)] !=
            [None, "top8", "top8", None, None, "ties", "ties"]):
        bad.append(("bra play-off", playoff_faults(top8, "bra_nat_second", {2003}),
                    playoff_faults(ties, "bra_nat_second", {2026})))
    bl = split_bounds("recomp: league sizes: wel_first split bounds 2026 120: 1-2 1-3 3-3 4-5 4-5\n", "wel_first")
    if (bl != {2026: [[(1, 2), (1, 3), (3, 3), (4, 5), (4, 5)]]} or bounds_faults("b", bl[2026], [3, 2]) or
            len(bounds_faults("b", [[(1, 2), (1, 4), (3, 3), (4, 5), (3, 5)]], [3, 2])) != 2 or
            not bounds_faults("b", [[(1, 1)]], [3, 2])):
        bad.append(("bounds", bl))
    ld = lone_downs("recomp: league sizes: wel_first (Welsh Premier League) 2009: Club A goes down to no division\n"
                    "recomp: league sizes: wel_first (Welsh Premier League) 2009: Club B goes down to no division\n",
                    "wel_first")
    if ld != {2009: 2} or row_down("wel_first", 2009) != 6 or row_down("wel_first", 2025) != 2:
        bad.append(("lone", ld, row_down("wel_first", 2009), row_down("wel_first", 2025)))
    lg = lone_guards("recomp: league sizes: kor_league (K League) 2012: relegation places 2, 1 go down: 0 clubs with "
                     "no division can come up, 13 clubs to 12\nrecomp: league sizes: kor_league (K League) 2013: "
                     "relegation places 2, 2 go down: 0 clubs with no division can come up, 12 clubs to 12\nrecomp: league "
                     "sizes: kor_league (K League) 2014: relegation places 1, 0 go down: 3 clubs with no division can "
                     "come up, 12 clubs to 12, the table showed 0\n", "kor_league")
    lp = lone_places("recomp: league sizes: kor_league (K League 1) 2024: relegation places 1 to no division, the class "
                     "gave 0\nrecomp: league sizes: kor_league (K League 1) 2024: relegation places 0 to no division, "
                     "the class gave 1\n", "kor_league")
    hf = lone_human_faults("recomp: league sizes: kor_league (K League) 2013: A stays, its manager is a player: C goes "
                           "down in its place\nrecomp: league sizes: kor_league (K League) 2013: B goes down to no "
                           "division\nrecomp: league sizes: kor_league (K League) 2013: C goes down to no division\n"
                           "recomp: season change: news 2014/15: A stays in the K League: its manager is a player.\n",
                           "kor_league", 2013, 2014)
    hf2 = lone_human_faults("recomp: league sizes: kor_league (K League) 2013: A goes down to no division\n",
                            "kor_league", 2013, 2014)
    if lp != {2024: 0} or hf or len(hf2) != 1:
        bad.append(("lone places and humans", lp, hf, hf2))
    if (lg != {2012: 1, 2013: -1, 2014: 0} or row_down("kor_league", 2012) != 2 or row_down("kor_league", 2023) != 1 or
            row_down("kor_league", 2025) != 0 or edition_season("kor_league", 2024) != 2023):
        bad.append(("lone kor", lg, row_down("kor_league", 2012), row_down("kor_league", 2023),
                    row_down("kor_league", 2025), edition_season("kor_league", 2024)))
    # Slice 14: the table of a row of halves holds both halves (twice the row's meetings).
    if (encoded_row("jap_j1", 2014) != (18, 2) or encoded_row("jap_j1", 2004) != (18, 2) or
            encoded_row("kor_league", 2005) != (14, 2) or encoded_row("jap_j2", 2009) != (19, 2) or
            edition_row("jap_j1", 2015) != (18, 2) or not encoded_halves("jap_j1", 2014) or
            encoded_halves("jap_j1", 2004)):
        bad.append(("encoded asia", encoded_row("jap_j1", 2014), encoded_row("kor_league", 2005)))
    one = ("recomp: league sizes: jap_j1 2005: one table, no halves and no final (row 2004)\n"
           "recomp: league sizes: jap_j1 2005: champion club 1234 (the top of the table)")
    halves = ("recomp: league sizes: jap_j1 2015: halves and the final (row 2014), 3 stages\n"
              "recomp: league sizes: jap_j1 2015: half of 18 clubs, 17 matches a club (the class makes 15)\n"
              "recomp: league sizes: jap_j1 2015: second half from round 18 of 34 (the class's 16)\n"
              "recomp: league sizes: jap_j1 2015: final of the halves")
    if (not j1_faults(halves.replace("round 18", "round 16"), "jap_j1", {2015}) or
            not j1_faults(halves.replace("final of the halves", "final of the halves not made"), "jap_j1", {2015}) or
            j1_faults(halves.replace("final of the halves", "double winner, no final"), "jap_j1", {2015})):
        bad.append(("j1 bound", j1_faults(halves.replace("round 18", "round 16"), "jap_j1", {2015})))
    if (j1_faults(one, "jap_j1", {2005}) or j1_faults(halves, "jap_j1", {2015}) or
            len(j1_faults(halves.replace("2015", "2005"), "jap_j1", {2005})) != 3 or
            len(j1_faults(one.replace("2005", "2015"), "jap_j1", {2015})) != 5 or j1_faults("", "nor_prm", {2009})):
        bad.append(("j1", j1_faults(one, "jap_j1", {2005}), j1_faults(halves, "jap_j1", {2015})))
    for b in bad:
        print("FAIL self-test", b)
    # Gap 34: the points at the split of the Pro League 2010/11 (places 1 to 16 after match 30).
    pts = "recomp: league sizes: bel_first split 2010: points at the split hzzb, " + " ".join(
        str(x) for x in [31, 29, 27, 25, 23, 21] + [0] * 8 + [3, 0])
    zero_bad = pts.replace(" 0 0 0 0 0 0 0 0 3 0", " 0 0 9 0 0 0 0 0 3 0")
    no_bonus = pts[:-4] + " 0 0"
    if (split_points_faults(pts, "bel_first", lambda e: e) or not split_points_faults(zero_bad, "bel_first", lambda e: e)
            or not split_points_faults(no_bonus, "bel_first", lambda e: e)
            or not split_points_faults(pts.replace("hzzb", "hhhh"), "bel_first", lambda e: e)):
        bad.append(("split points", split_points_faults(pts, "bel_first", lambda e: e)))
    print(("FAIL" if bad else "PASS") + " league_size_run self-test")
    return 1 if bad else 0


def main(argv):
    if "--list" in argv:
        for k, (y, last, what) in ROWS.items():
            print(f"{k.split('@')[0]}\t--year {y} --last {last}\t{what}")
        return 0
    if "--self-test" in argv:
        return self_test()
    if len(argv) < 3:
        print(__doc__)
        return 2
    import rules_report_check as rrc
    if os.environ.get("RECOMP_TEST_LONE_HUMAN") == "1":
        os.environ["RECOMP_SEASON_CHANGE_LOG"] = "1"    # the news lines of lone_human_faults (season_change.c)
    program, game, file = os.path.abspath(argv[0]), os.path.abspath(argv[1]), argv[2]
    year0, last0, _w = ROWS.get(file, (2025, None, ""))
    year = int(rrc.arg(argv, "--year", str(year0)))
    last = int(rrc.arg(argv, "--last", rrc.arg(argv, "--to", str(last0 or year))))
    keep = rrc.arg(argv, "--keep", None)
    timeout = int(rrc.arg(argv, "--timeout", "7200"))
    work = os.path.abspath(keep) if keep else tempfile.mkdtemp(prefix=f"cm0102-size-{file}-{year}-")
    if "--reuse" not in argv:
        db = rrc.run_database(argv, year, os.path.join(work, "owndb"))
        words = [x.strip().lower().replace("_", " ") for x in rrc.arg(argv, "--nations", "all").split(",")]
        rows = rrc.NATION_ROWS + rrc.NATION_ROWS_BELOW
        names = [next(n for n in rows if n.lower() == w.lstrip("-")) for w in words if w != "all"]
        nations = (["all"] + names) if words[0] == "all" else names
        if nations[0] != "all" and "England" not in nations:
            nations = ["England"] + nations
        extra = EXTRA.get(file, []) + [x for x in rrc.arg(argv, "--extra", "").split(",") if x]
        if "--save-in-split" in argv:
            code = save_in_split(program, game, year, last, db, nations, work, timeout)
        else:
            code, _report = rrc.run(program, game, last, year, db, nations, extra, work, timeout,
                                    {"changing_rules": "on"}, {"RECOMP_SIZE_LOG": "1"}, stop_on_box=True)
        if code != 0:
            print(f"FAIL {file} {year}: the run ended with code {code} ({work})")
            return 1
    fails = faults(work, file, year, last)
    if "--save-in-split" in argv:
        fails += save_faults(work, file)
    for f in fails:
        print("FAIL", f)
    print(("FAIL" if fails else "PASS") + f" {file} {year} to {last} ({work})")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
