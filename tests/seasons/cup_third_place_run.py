#!/usr/bin/env python3
"""The third-place tie whose loser comes late (0.6.2 open item 73, src/recomp_rt/win/season_cup_third_place.h).

Usage:
    python3 tests/seasons/cup_third_place_run.py <cm0102_recomp> <game folder> [--keep <folder>]

The run loads a save of 12 May 2021 (a 2013 game, Autumn 2013 data, all nations, seed 1, changing rules on), plays
60 days on holiday and saves (11 July 2021). In that game the second semi-final of the 2020/21 AFC Champions League
moves from 27 May to 2 June (its clubs' players have AFC World Cup qualifiers on 28 May), after the day the final and
the third place are set up (28 May). The save: RECOMP_THIRD_PLACE_SAVE, else test-fixtures/afc-third-place-2021.sav;
with neither the run fails.

The checks: the program ends with code 0 and shows no message box of the game; the log says that the third-place tie
got its fixture after its clubs came; the new save holds the 2021 row of the AFC Champions League (competition 110)
with a winner, a runner-up and a third. Without the fix the third-place match never comes, the cup never ends and
the save has no 2021 row (the 2020/21 edition holds the label year 2021).
"""
import os
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools", "save"))
import soak_run  # noqa: E402
import savfile  # noqa: E402

FIXTURES = "/Users/max/Projects/cm0102/test-fixtures"
DATABASE = os.path.join(FIXTURES, "databases", "Autumn 2013")
COMP = 110                              # the AFC Champions League record (club_comp.dat)
YEAR = 2021
DAYS = 60
HIST = 26                               # club_comp_history.dat: +0 id, +4 competition, +8 u16 year, +0xa first,
                                        # +0xe second, +0x12 third, +0x16 host (tests/seasons/future/history_check.py)
LINE = "recomp: cup: AFC Champions League third place tie 0: its clubs came after its set-up, fixture made"


def rows(save, comp):
    data = savfile.Sav(save).read_member("club_comp_history.dat")
    out = []
    for i in range(len(data) // HIST):
        _, cid, year, a, b, c, _ = struct.unpack_from("<iiHiiii", data, i * HIST)
        if cid == comp:
            out.append((year, a, b, c))
    return out


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-third-place-")
    save = os.environ.get("RECOMP_THIRD_PLACE_SAVE") or os.path.join(FIXTURES, "afc-third-place-2021.sav")
    if not os.path.exists(save):
        print(f"FAIL  no save: {save}")
        return 1
    o = {"program": program, "game": game, "rules": "on", "database": DATABASE, "seed": 1, "last": 2023,
         "timeout": 3600}
    code, took, box = soak_run.load_leg(o, keep, save, DAYS)
    fails = []
    if code != 0 or box:
        fails.append(f"the run ended with code {code}{', box ' + box if box else ''} ({keep})")
    log = open(os.path.join(keep, "stderr.txt"), encoding="latin-1").read()
    if LINE not in log:
        fails.append("no line of the late third-place fixture in the log")
    new = os.path.join(keep, "user", "samesave.sav")
    if not os.path.exists(new):
        fails.append("no save after the days")
    else:
        got = [r for r in rows(new, COMP) if r[0] == YEAR]
        if not got:
            fails.append(f"the save has no {YEAR} row of the AFC Champions League (the cup never ended)")
        elif min(got[0][1:]) < 0:
            fails.append(f"the {YEAR} row of the AFC Champions League lacks a club: {got[0]}")
        else:
            print(f"ok    {YEAR} row of the AFC Champions League: clubs {got[0][1:]} (run {took:.0f} s)")
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
