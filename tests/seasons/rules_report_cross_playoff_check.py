#!/usr/bin/env python3
"""ctest of the places check for a play-off between two divisions, with no game (0.6.2 open item 259).

Usage:
    python3 -I tests/seasons/rules_report_cross_playoff_check.py

The Tippeligaen and the First Division play a play-off between them (NorFirst 0x790b00, NorPrm 0x792700). No club
surely moves through it, so the rows count the direct places only: nor-fin-62 (2007/08, the real 2008) says 14th
(1) down with the 13th in the play-off, nor-fin-63 says 1st-3rd (3) up with the 4th. The check took 0 down and the
play-off as 1 down, and a 2008 start relegated nobody (season_sizes.c set_places read the row in the same way). This
test gives differences() the real rows with made-up game places of 2007/08. It wants a line for 0 down and the
play-off and for 2 up and the play-off, and no line for 1 down and 3 up with the play-offs.
Exit code 0 when every case passes.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rules_report_check as rrc  # noqa: E402


def league(file, record, phase, year, up, playoff_up, playoff_down, down, above, below, name):
    return {"season": 2007, "phase": phase, "slot": f"t{record}", "record": record, "file": file, "class": "?",
            "kind": "league", "vtable": "0x0", "year": year, "clubs": 14 if file == "nor_prm" else 16, "rounds": 2,
            "subs_used": 3, "bench": 7, "matches": 0, "up": up, "playoff_up": playoff_up,
            "playoff_down": playoff_down, "down": down, "flags": "0x3", "above": above, "below": below, "name": name}


def place_lines(prm, first):
    """The up and down lines of the two divisions for (up, playoff up, playoff down, down) each."""
    comps = []
    for phase, year in (("start", 2007), ("end", 2008)):
        comps.append(league("nor_prm", 315, phase, year, *prm, -1, 316, "Tippeligaen"))
        comps.append(league("nor_first", 316, phase, year, *first, 315, -1, "Adeccoligaen"))
    seasons = [{"season": 2007, "phase": "start", "date": "", "start": 2007, "rules": "on"},
               {"season": 2007, "phase": "end", "date": "", "start": 2007, "rules": "on"}]
    rows = [r for r in rrc.make_rules.read_rows() if r["id"] in ("nor-fin-62", "nor-fin-63")]
    return sorted(text for _, kind, text in rrc.differences(seasons, comps, rows) if kind in ("up", "down"))


def main():
    fails = []
    # Before the fix: the Tippeligaen 0 down and the play-off, the First Division 2 up and the play-off.
    got = place_lines((0, 0, 1, 0), (2, 1, 0, 4))
    want = ["nor_first (Adeccoligaen), row nor-fin-63: game 2 (+1 play-off places), table 3 (1st-3rd (3))",
            "nor_prm (Tippeligaen), row nor-fin-62: game 0 (+1 play-off places), table 1 (14th (1))"]
    for line in want:
        if line not in got:
            fails.append(f"missing: {line}")
    # The rows' direct places with the play-offs: no up or down line (the First Division's 4 down to the Second
    # Division against the row's 2 is another fault, so its down places are the row's here).
    got = place_lines((0, 0, 1, 1), (3, 1, 0, 2))
    if got:
        fails.append(f"not wanted: {got}")
    for f in fails:
        print("FAIL  " + f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
