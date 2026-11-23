#!/usr/bin/env python3
"""The two legs of a tie stay in order when the season change rule moves one (0.6.2 open item 90,
src/recomp_rt/win/fixture_season_end.c).

Usage:
    python3 tests/seasons/fixture_leg_order_run.py <cm0102_recomp> <game folder> [--keep <folder>]

The run loads a save of day 163 of 2055 (a 2006 game, the own database Summer 2006, seed 2, changing rules on, Tier
A; run set game a2006s2) and plays 10 days in Tier A. The Segunda División play-off final (two legs, drawn on day 162)
has its first leg on day 165 and its second on day 169, the day before the season change of the league (day 170). A
clash moves the first leg, and the season change rule puts it on day 169, the last free day before day 170; the
second leg, added next, meets a clash too, and the rule kept it on day 169. Both legs on one day: the game processes
one as the first leg, finds no second leg after it and stops with the box "Cup..cpp 1278" (0x51a672; run set pass 2,
a2006s2, day 169 of 2055). Whether the box comes depends on the order in which the day's matches are played, so the
check is the dates the rule gives the legs. The save: RECOMP_LEG_ORDER_SAVE, else test-fixtures/segunda-legs-2055.sav;
the database: own-dbs/Summer 2006.zip, unpacked in the keep folder.

The checks: the program ends with code 0 and shows no message box; the log has no line of a Segunda División match
kept on day 169 (the day of its first leg), and has the line of the second leg moved after its first leg.
"""
import os
import sys
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import soak_run  # noqa: E402

FIXTURES = "/Users/max/Projects/cm0102/test-fixtures"
OWN_ZIP = "/Users/max/Projects/cm0102/own-dbs/Summer 2006.zip"
DAYS = 10
KEPT = "a match of Segunda Divisi\xf3n stays on day 169 of 2055"
MOVED = "a match of Segunda Divisi\xf3n goes to day 173 of 2055 after its season change on day 170 (its first leg is on " \
        "day 169 of 2055"


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-leg-order-")
    save = os.environ.get("RECOMP_LEG_ORDER_SAVE") or os.path.join(FIXTURES, "segunda-legs-2055.sav")
    for need in (save, OWN_ZIP):
        if not os.path.exists(need):
            print(f"FAIL  missing: {need}")
            return 1
    database = os.path.join(keep, "owndb", "Summer 2006")
    if not os.path.isdir(database):
        with zipfile.ZipFile(OWN_ZIP) as z:
            z.extractall(os.path.join(keep, "owndb"))
    os.environ["RECOMP_TIER_A"] = "1"           # the game of the save was played in Tier A
    o = {"program": program, "game": game, "rules": "on", "database": database, "seed": 2, "last": 2056,
         "timeout": 3600}
    code, took, box = soak_run.load_leg(o, os.path.join(keep, "leg"), save, DAYS)
    fails = []
    if code != 0 or box:
        fails.append(f"the run ended with code {code}{', box ' + box if box else ''} ({keep})")
    log = open(os.path.join(keep, "leg", "stderr.txt"), encoding="latin-1").read()
    if KEPT in log:
        fails.append("a Segunda Divisi\xf3n leg was kept on day 169, the day of its first leg")
    if MOVED not in log:
        fails.append("no line of the second leg moved after its first leg")
    if not fails:
        print(f"ok    the play-off legs stay in order (run {took:.0f} s)")
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
