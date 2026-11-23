#!/usr/bin/env python3
"""The matches of each team in the OFC Nations Cup's group table after an edition of another size (0.6.2 open item 45,
src/recomp_rt/win/season_intl_ofc.c season_intl_ofc_rules).

Usage:
    python3 tests/seasons/ofc_team_matches_run.py <cm0102_recomp> <game folder> [--keep <folder>]

League +0xa7 holds the matches of each team of a League table. The League constructor sets it to -1 (0x682263), and
the table code works it out once from the rows and the meetings, (rows - 1) x meetings (0x684817..0x684830,
0x6827ef..0x68280f). Nothing else sets it, so an OFC object that goes from an edition of 8 (groups of 4: 3 matches)
to one of 6 (groups of 3: 2 matches) kept the 3 of the edition before, and the 2004 edition then stopped after its
groups (no knockout, no end of the edition); the home-and-away group of 2008 (4 rows, 2 meetings: 6) kept it too.

One new game, Tier A, England's leagues (the national sides of every nation play), seed 1, changing rules on: the
3.9.68 database, a 2001/02 start, 2720 days (to December 2008), past the 2002 edition (8 teams, November 2002 in a
2001 game), the 2004 edition (6 teams, from 31 May 2004) and the 2008 edition (one group of 4, home and away, to
November 2008). With RECOMP_SEASON_INTL_LOG=1 the class
logs at each edition's end its group A rows, meetings and +0xa7 (season_intl_ofc_honours).

The checks: the program ends with code 0 and shows no message box; the log names the editions 2002, 2004 and 2008; in
each,
+0xa7 is (rows - 1) x meetings.
"""
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import soak_run  # noqa: E402

LINE = re.compile(r"oceania_nations (\d+): group A (-?\d+) rows, (-?\d+) meetings, (-?\d+) matches each")
YEARS = (2002, 2004, 2008)


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-ofcmatches-")
    os.environ["RECOMP_TIER_A"] = "1"
    os.environ["RECOMP_SEASON_INTL_LOG"] = "1"
    o = {"program": program, "game": game, "rules": "on", "database": None, "seed": 1, "start": 2001, "last": 2011,
         "nations": ["England"], "extra": [], "timeout": 5400}
    work = os.path.join(keep, "leg")
    code, took, box = soak_run.new_game_leg(o, work, 2720)
    fails = []
    if code != 0 or box:
        fails.append(f"the run ended with code {code}{', box ' + box if box else ''} ({work})")
    seen = {}
    with open(os.path.join(work, "stderr.txt"), errors="replace") as f:
        for line in f:
            m = LINE.search(line)
            if m:
                seen[int(m.group(1))] = tuple(int(x) for x in m.groups()[1:])
    print(f"      editions: {seen} (run {took:.0f} s)")
    for year in YEARS:
        if year not in seen:
            fails.append(f"{year}: no log line of the edition's end")
            continue
        rows, meetings, each = seen[year]
        if each != (rows - 1) * meetings:
            fails.append(f"{year}: {each} matches each in a group of {rows} with {meetings} meetings, not "
                         f"{(rows - 1) * meetings}")
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
