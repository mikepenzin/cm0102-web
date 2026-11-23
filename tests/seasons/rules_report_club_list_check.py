#!/usr/bin/env python3
"""ctest of the club_list check of the rules report, with no game (0.6.2 open item 142).

Usage:
    python3 -I tests/seasons/rules_report_club_list_check.py

club_differences() compares the club list of each division of the club moves table with the clubs of that division
at the start of a season game. Slice 15b (cf6e4633, 20 October 2026) gave the new club records (club_record_names)
the name of the division records (division_records), so no division had a record and the check printed no club_list
line at all. This test gives club_differences a made-up data folder, club moves table and start report. It wants the
line of a division with a missing club and another club and the line of a division that the moves leave empty, no
line for a division whose club only a new club record names, and no line in the base game.
Exit code 0 when every case passes.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rules_report_check as rrc  # noqa: E402


def main():
    fails = []
    divisions = {"Italian Serie A": 5, "Italian Serie B": 6, "Norwegian First Division": 7}
    data_clubs = {1: "Alpha", 2: "Beta", 3: "Gamma", 4: "Delta", 9: "Iota"}
    table = [(2014, "Italian Serie A", 1, "Alpha", None), (2014, "Italian Serie A", 2, "Beta", None),
             (2014, "Italian Serie A", 3, "Gamma", None),
             (2014, "Italian Serie B", -1, "Nova", None), (2014, "Italian Serie B", 9, "Iota", None),
             (2014, "Norwegian First Division", 4, "Delta", None),
             (2013, "Italian Serie A", 4, "Delta", None),                  # another season: not compared
             (2001, "Italian Serie A", 3, "Gamma", None)]                  # the base game: not compared either
    new_records = {("Italian Serie B", "Nova"): "Nova FC"}                 # slice 15b: a club the data lacks
    rrc.division_records = lambda folder: dict(divisions)
    rrc.club_records = lambda folder: dict(data_clubs)
    rrc.club_record_names = lambda season: dict(new_records) if season == 2014 else {}
    rrc.make_clubs.build = lambda: (list(table), [])

    seasons = [{"season": 2014, "start": 2014, "rules": "off"}]
    comps = [{"season": 2014, "kind": "league", "clubs": 20, "record": 5, "class": "ItaPrm"},
             {"season": 2014, "kind": "league", "clubs": 22, "record": 6, "class": "ItaFirst"},
             {"season": 2014, "kind": "league", "clubs": 0, "record": 7, "class": "NorFirst"}]
    clubs = [{"season": 2014, "phase": "start", "division": 5, "name": n} for n in ("Alpha", "Beta", "Delta")] + \
            [{"season": 2014, "phase": "start", "division": 6, "name": n} for n in ("Nova FC", "Iota")] + \
            [{"season": 2014, "phase": "end", "division": 5, "name": "Gamma"}]       # an end line: not read

    got = sorted(rrc.club_differences(seasons, comps, clubs, "/no/data"))
    want = sorted([
        (2014, "club_list", "Italian Serie A: 2 of 3 listed clubs that the database holds (3 listed), 1 other "
                            "clubs, 3 in all; not in it: Gamma; others: Delta"),
        (2014, "club_list", "Norwegian First Division: no clubs at the start (NorFirst counts the clubs of the "
                            "data)"),
    ])
    for line in want:
        if line not in got:
            fails.append(f"missing: {line}")
    for line in got:
        if line not in want:
            fails.append(f"not wanted: {line}")
    # The base game moves no club: no line, although its list names Gamma, which the 2001 start does not hold.
    comps01 = [dict(c, season=2001) for c in comps]
    clubs01 = [dict(c, season=2001) for c in clubs]
    base = rrc.club_differences([{"season": 2001, "start": 2001, "rules": "off"}], comps01, clubs01, "/no/data")
    if base:
        fails.append(f"base game: {sorted(base)}")

    for f in fails:
        print("FAIL  " + f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
