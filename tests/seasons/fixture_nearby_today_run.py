#!/usr/bin/env python3
"""The nearby move never takes a match of today or before, or to today or before (0.6.2 open item 194,
src/recomp_rt/win/fixture_season_end.c).

Usage:
    python3 tests/seasons/fixture_nearby_today_run.py <cm0102_recomp> <game folder> [--keep <folder>]

The fault (run set pass 4, game a2002s2): on day 195 of 2054 a Conference League draw made a chain of clash and
nearby moves, and FIXTURE_MANAGER::move_nearby_club_fixtures 0x5ae9d0 took that day's Scottish League Cup match to
day 194. Its remove freed today's match while the day still held it, a re-added second leg was stored in the freed
block and overwritten, and on day 197 the first leg found no second leg: the box "Cup..cpp 1278" (0x51a672). That
chain depends on every fixture of the save (a change to the Under 21 days, item 163b, no longer makes it), so the
test makes the case itself: with RECOMP_TEST_NEARBY_TODAY=1 the game calls 0x5ae9d0, at the first add of a fixture
on a day with a match of today, for a copy of that match dated two days later, as an add on that day would. The
original then moves the match of today one day earlier.

The run loads a save of day 201 of 2052 (a 2002 game, the own database Summer 2002, seed 2, changing rules on, Tier
A; the save of leg 4 of run set game a2002s2 in pass 4) and plays 10 days. The save: RECOMP_NEARBY_TODAY_SAVE, else
test-fixtures/nearby-today-2052.sav; the database: own-dbs/Summer 2002.zip, unpacked in the keep folder.

The checks: the program ends with code 0 and shows no message box, the test call ran, the match of today stays on
today, and the log has the line of the match that the nearby move leaves on today.
"""
import os
import re
import sys
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import soak_run  # noqa: E402

FIXTURES = "/Users/max/Projects/cm0102/test-fixtures"
OWN_ZIP = "/Users/max/Projects/cm0102/own-dbs/Summer 2002.zip"
DAYS = 10
CALLED = "recomp: test nearby today: a match of "
KEPT = "recomp: test nearby today: the match of today stays on today"
STAYS = re.compile(r"recomp: fixtures: a match of .* stays on day (\d+) of (\d+) \(the nearby move gave day \d+ of "
                   r"\d+, today is day \1 of \2\)")


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-nearby-today-")
    save = os.environ.get("RECOMP_NEARBY_TODAY_SAVE") or os.path.join(FIXTURES, "nearby-today-2052.sav")
    for need in (save, OWN_ZIP):
        if not os.path.exists(need):
            print(f"FAIL  missing: {need}")
            return 1
    database = os.path.join(keep, "owndb", "Summer 2002")
    if not os.path.isdir(database):
        with zipfile.ZipFile(OWN_ZIP) as z:
            z.extractall(os.path.join(keep, "owndb"))
    os.environ["RECOMP_TIER_A"] = "1"           # the game of the save was played in Tier A
    os.environ["RECOMP_TEST_NEARBY_TODAY"] = "1"
    o = {"program": program, "game": game, "rules": "on", "database": database, "seed": 2, "last": 2063,
         "timeout": 3600}
    code, took, box = soak_run.load_leg(o, os.path.join(keep, "leg"), save, DAYS)
    fails = []
    if code != 0 or box:
        fails.append(f"the run ended with code {code}{', box ' + box if box else ''} ({keep})")
    log = open(os.path.join(keep, "leg", "stderr.txt"), encoding="latin-1").read()
    if CALLED not in log:
        fails.append("the test call of the nearby move did not run")
    elif KEPT not in log:
        fails.append("the nearby move took the match of today off today")
    elif not STAYS.search(log):
        fails.append("no line of the match of today that the nearby move leaves on today")
    if not fails:
        print(f"ok    the nearby move leaves today's match on today (run {took:.0f} s)")
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
