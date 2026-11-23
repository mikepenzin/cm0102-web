#!/usr/bin/env python3
"""A round set up before it was drawn (0.6.2 open item 88, src/recomp_rt/win/season_cup_round_setup.c).

Usage:
    python3 tests/seasons/cup_round_setup_run.py <cm0102_recomp> <game folder> [--keep <folder>]

The run loads a save of 10 January 2008 (a base game: 2001 start, changing rules off, base data, all nations, seed 2,
played in Tier A to that day) and plays 30 days in Tier A. In that game a group match of the 2008 Oceania Champions
Cup moves from 12 to 19 January, so the semi-final stage is made on 19 January, after its own days. The same day the
third-place round is drawn and set up, and the set-up of the final, which was not drawn yet, is passed over: the final
never comes, the cup never ends and no later edition starts. The save: RECOMP_ROUND_SETUP_SAVE, else
test-fixtures/ofc-final-2008.sav; with neither the run fails.

The checks: the program ends with code 0 and shows no message box of the game; the log says that the final was drawn
before its set-up; the new save holds the 2008 row of the Oceania Champions Cup (competition 109) with a winner, a
runner-up and a third.
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
COMP = 109                              # the Oceania Champions Cup record (club_comp.dat)
YEAR = 2008
DAYS = 30
HIST = 26                               # club_comp_history.dat, as tests/seasons/cup_third_place_run.py reads it
LINE = "recomp: cup: Oceania Champions Cup round 1 is drawn before its set-up"


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
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-round-setup-")
    save = os.environ.get("RECOMP_ROUND_SETUP_SAVE") or os.path.join(FIXTURES, "ofc-final-2008.sav")
    if not os.path.exists(save):
        print(f"FAIL  no save: {save}")
        return 1
    os.environ["RECOMP_TIER_A"] = "1"           # the game of the save was played in Tier A (open item 88)
    os.environ.pop("RECOMP_ORIGINAL_FAULTS", None)   # the fix acts unless that switch is keep
    o = {"program": program, "game": game, "rules": "off", "database": None, "seed": 2, "last": 2012,
         "timeout": 3600}
    code, took, box = soak_run.load_leg(o, keep, save, DAYS)
    fails = []
    if code != 0 or box:
        fails.append(f"the run ended with code {code}{', box ' + box if box else ''} ({keep})")
    log = open(os.path.join(keep, "stderr.txt"), encoding="latin-1").read()
    if LINE not in log:
        fails.append("no line of the final drawn before its set-up in the log")
    new = os.path.join(keep, "user", "samesave.sav")
    if not os.path.exists(new):
        fails.append("no save after the days")
    else:
        got = [r for r in rows(new, COMP) if r[0] == YEAR]
        if not got:
            fails.append(f"the save has no {YEAR} row of the Oceania Champions Cup (the cup never ended)")
        elif min(got[0][1:]) < 0:
            fails.append(f"the {YEAR} row of the Oceania Champions Cup lacks a club: {got[0]}")
        else:
            print(f"ok    {YEAR} row of the Oceania Champions Cup: clubs {got[0][1:]} (run {took:.0f} s)")
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
