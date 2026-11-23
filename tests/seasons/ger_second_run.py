#!/usr/bin/env python3
"""Germany: a reserve side plays German Regional (the Regionalliga, from 2008/09 the 3. Liga) only while its first
team plays the Bundesliga or the 2. Bundesliga, and never goes up into the 2. Bundesliga (open item 239, 0.7.2;
season_ger_second.c).

Usage:
    python3 tests/seasons/ger_second_run.py <cm0102_recomp> <game folder> [--only on,off,on2005,off2005,old] [--jobs n]
        [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py with Germany and its extra division (German
Regional), changing rules off, over one German season change (GerFirst::start_new_season 0x5db4a0 in summer). The
test switch RECOMP_TEST_GER_SECOND_REP=1 gives every reserve, youth or third side of the pool (A Lower Division or no
division) the highest reputation, so the original's exchange 0x5db980 takes them first, those whose first team plays
German Regional or below too. RECOMP_TEST_GER_SECOND_UP=1 gives the best placed side of German Regional's table a
promotion status before the change, as an old save. RECOMP_GER_SECOND_LOG=1 writes a census before and after the
change. The runs:
    on       October 2017, a 2017 start over the changes of summer 2018 and 2019 (the 3. Liga rule; the first
             change brings sides into the 3. Liga, the second makes the old-save case): after each change no side
             plays the 2. Bundesliga or the Bundesliga, none plays German Regional where its first team plays German
             Regional or below, at least one barred side went back to A Lower Division with a club in its place, a
             promotion place was handed on, and at least one side whose first team plays the top two stays
    off      the same with RECOMP_GER_SECOND=off: barred sides play German Regional or a side goes up (the fault)
    on2005   September 2005, a 2005 start over the changes of summer 2006 and 2007 (the Regionalliga rule, North
             and South): the same checks as on
    off2005  the same with RECOMP_GER_SECOND=off
    old      as on, but RECOMP_TEST_GER_SECOND_OLD=2 plays as the original until the change of summer 2019 is done
             (an old save: sides go up into the 2. Bundesliga), to 1 June 2021: at the change of summer 2020 every
             side leaves the 2. Bundesliga, each in exchange for a club of status 3 that stays up
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
ENV = {"RECOMP_TEST_GER_SECOND_REP": "1", "RECOMP_TEST_GER_SECOND_UP": "1", "RECOMP_GER_SECOND_LOG": "1"}
CENSUS = re.compile(r"recomp: ger second: census (before|after) \(season (\d+), rule of (\d+)\): Bundesliga (\d+) "
                    r"sides, 2\. Bundesliga (\d+) sides, German Regional (\d+) sides, (\d+) of them barred there")


def jump(name, program, game, work, env, year, db, changes=1):
    code, _ = rrc.run(program, game, year + changes, year, os.path.join(DB, db), ["Germany"], ["regional"], work, 7200,
                      None, dict(ENV, **env))
    fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    fails += [f"{name}: message box {b}" for b in found]
    return fails, log


def census(log):
    """(before, after) of the first German season change: tuples (season, rule, top, second, regional, barred)."""
    got = {}
    for m in CENSUS.finditer(log):
        got.setdefault(m.group(1), tuple(map(int, m.groups()[1:])))
    return got.get("before"), got.get("after")


def checks_on(name, log):
    """The checks of an on run (they fail on the log of an off run)."""
    fails = []
    before, after = census(log)
    if not before:
        return [f"{name}: no German season change seen"]
    seen_after = [tuple(map(int, m.groups()[1:])) for m in CENSUS.finditer(log) if m.group(1) == "after"]
    print(f"{name}: census before {before}, after {seen_after} (season, rule, Bundesliga, 2. Bundesliga, German "
          f"Regional, barred there)")
    if not seen_after:
        return fails + [f"{name}: no census after the change (GerRegional::create_table not reached)"]
    for k, a in enumerate(seen_after, 1):
        if a[2] or a[3]:
            fails.append(f"{name}: sides in the Bundesliga ({a[2]}) or the 2. Bundesliga ({a[3]}) after change {k}")
        if a[5]:
            fails.append(f"{name}: {a[5]} sides play German Regional after change {k} where their first team plays "
                         f"it or below")
    if not re.search(r"ger second: .* goes down to A Lower Division .* takes its place", log):
        fails.append(f"{name}: no barred side went back to A Lower Division (the test sees no case)")
    if re.search(r"ger second: .* goes down to A Lower Division .*; no club takes its place", log):
        fails.append(f"{name}: a place of German Regional stayed empty")
    if not re.search(r"ger second: test: .* takes status 0 from", log):
        fails.append(f"{name}: the test gave no side a promotion place (the old-save case is not made)")
    if not re.search(r"ger second: .* takes status \d+ in place of a second team at the season change", log):
        fails.append(f"{name}: no promotion place was handed on")
    if not re.search(r"ger second: .* plays German Regional, allowed", log):
        fails.append(f"{name}: no side whose first team plays the top two stays in German Regional")
    return fails


def run_on(name, year, db, changes):
    def go(program, game, work):
        fails, log = jump(name, program, game, work, {}, year, db, changes)
        return fails + checks_on(name, log)
    return go


def run_off(name, year, db, changes):
    def go(program, game, work):
        fails, log = jump(name, program, game, work, {"RECOMP_GER_SECOND": "off"}, year, db, changes)
        red = checks_on(name, log)
        print(f"{name}: the on checks on this log: {len(red)} fail")
        for r in red:
            print(f"    {r}")
        afters = [tuple(map(int, m.groups()[1:])) for m in CENSUS.finditer(log) if m.group(1) == "after"]
        if len(afters) < changes:
            fails.append(f"{name}: {len(afters)} census lines after a change, want {changes}")
        elif not any(a[3] or a[5] for a in afters):
            fails.append(f"{name}: no side in the 2. Bundesliga and none barred in German Regional with the switch "
                         f"off (the test does not see the fault)")
        return fails
    return go


def run_old(program, game, work):
    fails, log = jump("old", program, game, work, {"RECOMP_TEST_GER_SECOND_OLD": "2"}, 2017, "October 2017", 3)
    seen = {k: [tuple(map(int, m.groups()[1:])) for m in CENSUS.finditer(log) if m.group(1) == k]
            for k in ("before", "after")}
    print(f"old: census {seen}")
    if len(seen["before"]) < 3 or len(seen["after"]) < 3:
        return fails + ["old: fewer than 3 German season changes seen"]
    if not seen["before"][2][3]:
        fails.append("old: no side in the 2. Bundesliga before change 3 (the old save is not made)")
    if seen["after"][2][2] or seen["after"][2][3]:
        fails.append(f"old: sides in the top two after change 3: {seen['after'][2]}")
    if not re.search(r"ger second: .* goes down from the 2\. Bundesliga, .* stays up", log):
        fails.append("old: no side went down from the 2. Bundesliga in exchange for a club")
    return fails


RUNS = {
    "on": run_on("on", 2017, "October 2017", 2),
    "off": run_off("off", 2017, "October 2017", 2),
    "on2005": run_on("on2005", 2005, "September 2005 Helvete", 2),
    "off2005": run_off("off2005", 2005, "September 2005 Helvete", 2),
    "old": run_old,
}


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="ger-second-")
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
