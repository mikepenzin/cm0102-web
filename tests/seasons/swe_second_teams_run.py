#!/usr/bin/env python3
"""Sweden: no second team or combined team goes up into the Second Division (open item 244, 0.7.2;
season_swe_second.c). (tests/seasons/swe_second_run.py is the test of open item 224, the group sizes.)

Usage:
    python3 tests/seasons/swe_second_teams_run.py <cm0102_recomp> <game folder> [--only on,off,old] [--jobs n]
        [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py with Sweden alone and its extra division
(RECOMP_TEST_EXTRA=sweden), changing rules off, on the June 2010 database from the 2010 start. The database holds
Friska Viljor FC Lag 2 (a second team) and Friska Viljor FC 2/Arnäs IF (a combined team) in the Lower Division.
With changing rules off the rule of the start year, 2010 (SvFF TB 2010), holds in every season: they may not be
picked into the Second Division. RECOMP_TEST_SWE_SECOND_REP=1 gives them the highest reputation before
SweSecond::create_relegation_play_off_stage 0x892bb0, so the original would pick them. RECOMP_SWE_SECOND_LOG=1 logs
the picks, the clubs that go up at the season start (0x890b40) and a census of the barred clubs. The runs:
    on    the shipped code, to 1 June 2012 (the Swedish seasons 2010 and 2011, and the season starts of 2011 and
          2012; the original picked the planted clubs in 2011, not in 2010, in the off run): the barred clubs
          stay out of the picks, no barred club goes up, 12 or more clubs go up, and the Second Division holds no
          barred club after the start
    off   RECOMP_SWE_SECOND=off, the original's list: a barred club is picked (proves that the test sees the fault;
          the checks of "on" must fail on this log)
    old   RECOMP_TEST_SWE_SECOND_OLD=2 and RECOMP_TEST_SWE_SECOND_HOLD=1 (a barred club takes the middle club's points
          in its group, so the table does not send it down), to 1 June 2013: the original until the season start of
          2012 is done (a game of
          an older build, so a barred club goes up when it is picked for the direct places or wins the play-off),
          then the fix: at the season start of 2013 every barred club of the Second Division goes down and a club of
          its group stays up, so the Second Division keeps its size
All runs end with code 0 and show no message box of the game.
"""
import concurrent.futures
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rules_report_check as rrc  # noqa: E402
import uefa_formats_run as ufr  # noqa: E402

DB = "/Users/max/Projects/cm0102/test-fixtures/databases"
ENV = {"RECOMP_TEST_SWE_SECOND_REP": "1", "RECOMP_SWE_SECOND_LOG": "1", "RECOMP_TEST_EXTRA": "sweden"}
CENSUS = re.compile(r"recomp: swe second: census (before|after): Second Division (\d+) clubs, (\d+) barred; "
                    r"Lower Division (\d+) barred")


def jump(name, program, game, work, env, last):
    code, _ = rrc.run(program, game, last, 2010, os.path.join(DB, "June 2010"), ["Sweden"], [], work, 10800, None,
                      dict(ENV, **env))
    fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    fails += [f"{name}: message box {b}" for b in found]
    if "extra division of sweden on" not in log:
        fails.append(f"{name}: the Swedish extra division was not set")
    return fails, log


def starts(log):
    """Each Swedish season start (0x890b40): (census before, census after, lines between)."""
    out, cur = [], None
    for line in log.splitlines():
        m = CENSUS.search(line)
        if m and m.group(1) == "before":
            cur = [tuple(map(int, m.groups()[1:])), None, []]
        elif m and cur:
            cur[1] = tuple(map(int, m.groups()[1:]))
            out.append(cur)
            cur = None
        elif cur and "recomp: swe second:" in line:
            cur[2].append(line)
    return out


def check_on(name, log):
    fails = []
    out = re.findall(r"recomp: swe second: (.*) stays out of the picks \(season (\d+), rule of (\d+)", log)
    picked = re.findall(r"recomp: swe second: pick (.*) \((up|play-off)\) \(a second team\)", log)
    up = re.findall(r"recomp: swe second: (.*) goes up to the Second Division( \(a second team\))?", log)
    seen = starts(log)
    print(f"{name}: held out {out}, barred picks {picked}, {len(up)} clubs went up, starts "
          f"{[(b, a) for b, a, _ in seen]}")
    if not out:
        fails.append(f"{name}: no barred club was held out of the picks")
    if picked:
        fails.append(f"{name}: barred clubs were picked: {picked}")
    if [n for n, s in up if s]:
        fails.append(f"{name}: barred clubs went up: {[n for n, s in up if s]}")
    if len(up) < 12:
        fails.append(f"{name}: {len(up)} clubs went up to the Second Division, want 12 or more")
    if not seen:
        fails.append(f"{name}: no Swedish season start seen")
    elif seen[-1][1][1]:
        fails.append(f"{name}: {seen[-1][1][1]} barred clubs in the Second Division after the start")
    return fails


def run_on(program, game, work):
    fails, log = jump("on", program, game, work, {}, 2011)
    return fails + check_on("on", log)


def run_off(program, game, work):
    fails, log = jump("off", program, game, work, {"RECOMP_SWE_SECOND": "off"}, 2011)
    picked = re.findall(r"recomp: swe second: pick (.*) \((up|play-off)\) \(a second team\)", log)
    if not picked:
        fails.append("off: no barred club was picked with the original's list (the test does not see the fault)")
    red = check_on("off (checks of on)", log)
    print("off: the checks of on on this log: " + ("; ".join(red) if red else "none fail"))
    if not red:
        fails.append("off: the checks of on pass on the log of the original (they do not see the fault)")
    return fails


def run_old(program, game, work):
    env = {"RECOMP_TEST_SWE_SECOND_OLD": "2", "RECOMP_TEST_SWE_SECOND_HOLD": "1"}
    fails, log = jump("old", program, game, work, env, 2012)
    seen = starts(log)
    if len(seen) < 3:
        return fails + [f"old: {len(seen)} Swedish season starts seen, want 3"]
    print(f"old: starts {[(b, a) for b, a, _ in seen]}")
    if not any("goes up to the Second Division (a second team)" in l for s in seen[:2] for l in s[2]):
        fails.append("old: no barred club went up as the original in the first two starts (no old save made)")
    before, after, lines = seen[2]
    if not before[1]:
        fails.append("old: no barred club in the Second Division before the third start (the test sees no case)")
    if after[1]:
        fails.append(f"old: {after[1]} barred clubs still in the Second Division after the third start")
    if after[0] != before[0]:
        fails.append(f"old: the Second Division held {before[0]} clubs before the start and {after[0]} after")
    if not any("goes down from the Second Division" in l for l in lines):
        fails.append("old: no barred club went down from the Second Division")
    if not any("stays up in its group" in l for l in lines):
        fails.append("old: no club stayed up in place of a barred club")
    if [l for l in lines if "(a second team)" in l and "goes up" in l]:
        fails.append("old: a barred club went up at the third start")
    return fails


RUNS = {"on": run_on, "off": run_off, "old": run_old}


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="swe-second-")
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        futures = {n: ex.submit(RUNS[n], program, game, os.path.join(keep, n)) for n in only}
        fails = []
        for n in only:
            got = futures[n].result()
            print(f"{n}: {'PASS' if not got else 'FAIL'}")
            fails += got
    for f in fails:
        print("  " + f)
    print("PASS" if not fails else "FAIL")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
