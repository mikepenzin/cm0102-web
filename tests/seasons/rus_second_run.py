#!/usr/bin/env python3
"""Russia: second teams play below their club (open item 120, 0.7.0; season_rus_second.c).

Usage:
    python3 tests/seasons/rus_second_run.py <cm0102_recomp> <game folder> [--only on,off,parent,parent-off,old]
        [--jobs n] [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py with Russia alone on the October 2016 database
(changing rules off), from the 2016 start. With this start the Russian season changes at the end of each
December (RusPrm::start_new_season 0x7eb800: the chain 0x689c20, then the zone exchange 0x7ebe20), so the runs
to 1 June 2018 play the changes of December 2016 and 2017. In that database Dinamo Moscow-2, Sibir-2 and Kuban
Krasnodar-2 play in the zones while their clubs play in the First Division. RECOMP_RUS_SECOND_LOG=1 logs the moves.
The runs:
    on          RECOMP_TEST_RUS_SECOND_REP=1 (the second teams of the zones get the highest reputations, the barred
                ones the highest) and RECOMP_TEST_RUS_SECOND_PTS=1 (the second teams of the First Division and the
                Premier League get 500 points): at each change three clubs go up from the zones, no barred
                second team goes up to the First Division, at least one stays in its zone, the second teams of
                the First Division get the mark 0xfc, none reaches the Premier League, and the sizes stay
    off         the same with RECOMP_RUS_SECOND=off (the original): a barred second team goes up to the First
                Division and second teams go up to the Premier League (proves that the test sees the fault)
    parent      RECOMP_TEST_RUS_SECOND_REP=allowed (the second teams that may go up get the highest reputation) and
                RECOMP_TEST_RUS_SECOND_PARENT_DOWN=1 (at the second change the club
                of a second team of the First Division goes down from the Premier League): the second team goes down
                from the First Division and no barred second team stays there
    parent-off  the same with the original: the second team stays in the First Division with its club
    old         the state of an old save, made in memory: RECOMP_TEST_RUS_SECOND_OLD=2 plays as the original until
                the change of December 2017 is done, with the reputation and points switches, so second teams go up
                into the First Division and then the Premier League, and RECOMP_TEST_RUS_SECOND_NO_MARK=1 (no mark,
                as a save made after the table was complete). To 1 June 2019: at the change of December 2018 the places
                of the second teams of the First Division go on to the next clubs, every second team leaves the
                Premier League and the sizes stay
    old-all     the same with RECOMP_TEST_RUS_SECOND_ALL_DOWN=1 (every second team of the First Division goes down
                at the zone exchange): at the change of December 2018 more than three go down, a club of the zones
                takes the place of each one beyond the three rows, every second team of the two divisions goes down
                (those that may go up from the zones still do), and the sizes stay
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
ENV = {"RECOMP_RUS_SECOND_LOG": "1"}
TAG = "recomp: rus second: "
CENSUS = re.compile(r"recomp: rus second: census (before|after): Premier League (\d+) clubs, (\d+) second teams; "
                    r"First Division (\d+) clubs, (\d+) second teams")


def jump(name, program, game, work, env, last=2017):
    code, _ = rrc.run(program, game, last, 2016, os.path.join(DB, "October 2016"), ["Russia"], [], work, 7200, None,
                      dict(ENV, **env))
    fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    fails += [f"{name}: message box {b}" for b in found]
    return fails, log


def changes(log):
    """Each Russian season change: (census before, census after, the lines between)."""
    out, cur = [], None
    for line in log.splitlines():
        m = CENSUS.search(line)
        if m and m.group(1) == "before":
            cur = [tuple(map(int, m.groups()[1:])), None, []]
        elif m and cur:
            cur[1] = tuple(map(int, m.groups()[1:]))
            out.append(cur)
            cur = None
        elif cur and TAG in line:
            cur[2].append(line)
    return out


def first_change(name, log, fails, n=1):
    seen = changes(log)
    if len(seen) < n:
        fails.append(f"{name}: {len(seen)} Russian season changes seen, want {n}")
        return None
    before, after, lines = seen[n - 1]
    print(f"{name}: change {n}: before {before}, after {after} (Premier clubs, second teams; First clubs, second "
          f"teams)")
    if (before[0], before[2]) != (after[0], after[2]):
        fails.append(f"{name}: division sizes changed: {before} to {after}")
    return before, after, lines


def ups(name, lines, fails, change):
    """Three clubs go up from the zones at each change (the original's three places, 0x7ec0be)."""
    up = [l for l in lines if re.search(r"goes up to the First Division( \(a (barred )?second team\))?$", l)]
    extra = [l for l in lines if "goes up to the First Division in place of" in l]
    if len(up) != 3 + len(extra):
        fails.append(f"{name}: change {change}: {len(up)} clubs went up from the zones, want {3 + len(extra)}: {up}")


def run_on(program, game, work):
    fails, log = jump("on", program, game, work, {"RECOMP_TEST_RUS_SECOND_REP": "1", "RECOMP_TEST_RUS_SECOND_PTS": "1"})
    held = 0
    for n in (1, 2):
        got = first_change("on", log, fails, n)
        if not got:
            return fails
        _, after, lines = got
        ups("on", lines, fails, n)
        bad = [l for l in lines if "(a barred second team)" in l]
        if bad:
            fails.append(f"on: change {n}: barred second teams went up or stayed: {bad}")
        held += len([l for l in lines if re.search(r"rus second: .* stays in (?!the First Division|the Premier League)", l)])
        if n == 2:
            if not got[0][3]:
                fails.append("on: no second team in the First Division before change 2 (the promotion case is not seen)")
            if after[1]:
                fails.append(f"on: {after[1]} second teams in the Premier League after change 2")
    if not held:
        fails.append("on: no barred second team stayed in its zone (the zone case is not seen)")
    if "may not go up from the First Division" not in log:
        fails.append("on: no second team of the First Division got the mark (the promotion case is not seen)")
    return fails


def run_off(program, game, work):
    fails, log = jump("off", program, game, work, {"RECOMP_TEST_RUS_SECOND_REP": "1", "RECOMP_TEST_RUS_SECOND_PTS": "1",
                                                   "RECOMP_RUS_SECOND": "off"})
    barred_up = 0
    for n in (1, 2):
        got = first_change("off", log, fails, n)
        if not got:
            return fails
        ups("off", got[2], fails, n)
        barred_up += len([l for l in got[2] if "goes up to the First Division (a barred second team)" in l])
        if n == 2 and not got[1][1]:
            fails.append("off: no second team went up to the Premier League with the original (change 2)")
    if not barred_up:
        fails.append("off: no barred second team went up with the original (the test does not see the fault)")
    return fails


def run_parent(name, program, game, work, off):
    env = {"RECOMP_TEST_RUS_SECOND_PARENT_DOWN": "1", "RECOMP_TEST_RUS_SECOND_REP": "allowed",
           **({"RECOMP_RUS_SECOND": "off"} if off else {})}
    fails, log = jump(name, program, game, work, env)
    got = first_change(name, log, fails, 2)
    if got:
        _, _, lines = got
        down = [re.search(r"\(its second team (.*)\)$", l).group(1) for l in lines
                if "test: " in l and "goes down from the Premier League (its second team" in l]
        if not down:
            fails.append(f"{name}: no club was sent down by the test switch (no case)")
            return fails
        stays = [l for l in lines if l.endswith(f"{down[0]} stays in the First Division (a barred second team)")]
        if off and not stays:
            fails.append(f"{name}: no second team stayed with its club with the original (the test does not see "
                         f"the fault)")
        if not off:
            if stays:
                fails.append(f"{name}: barred second teams stayed in the First Division: {stays}")
            if not [l for l in lines if l.endswith(f"{down[0]} went down from the First Division")]:
                fails.append(f"{name}: {down[0]} did not go down from the First Division")
    return fails


def run_old(program, game, work, name="old", all_down=False):
    env = {"RECOMP_TEST_RUS_SECOND_OLD": "2", "RECOMP_TEST_RUS_SECOND_REP": "1", "RECOMP_TEST_RUS_SECOND_PTS": "1",
           "RECOMP_TEST_RUS_SECOND_NO_MARK": "1", **({"RECOMP_TEST_RUS_SECOND_ALL_DOWN": "1"} if all_down else {})}
    fails, log = jump(name, program, game, work, env, last=2018)
    if all_down:
        got = first_change(name, log, fails, 3)
        if got:
            before, after, lines = got
            ups(name, lines, fails, 3)
            if before[3] + before[1] <= 3:
                fails.append(f"{name}: {before[3] + before[1]} second teams to send down, want more than 3 (no case)")
            went = [l for l in lines if l.endswith("went down from the First Division")]
            if after[1] or len(went) != before[3] + before[1]:
                fails.append(f"{name}: {len(went)} second teams went down, want {before[3] + before[1]}; "
                             f"{after[1]} left in the Premier League")
            if [l for l in lines if "stays in the First Division" in l]:
                fails.append(f"{name}: a second team stayed in the First Division")
            if not [l for l in lines if "goes up to the First Division in place of" in l]:
                fails.append(f"{name}: no club took the place of a fourth second team (the path is not seen)")
        return fails
    first = first_change("old", log, fails, 2)
    if first and not first[1][1]:
        fails.append("old: no second team went up to the Premier League as the original (no old save made)")
    got = first_change("old", log, fails, 3)
    if got:
        before, after, lines = got
        if not before[1]:
            fails.append("old: no second team in the Premier League before change 3 (no case)")
        if after[1]:
            fails.append(f"old: {after[1]} second teams still in the Premier League after change 3")
        if not [l for l in lines if l.endswith("goes down from the Premier League")]:
            fails.append("old: no second team of a high place went down from the Premier League (no case)")
        if not [l for l in lines if "in place of a second team at the season change" in l]:
            fails.append("old: no place was handed on at the season change (the no-mark path is not seen)")
    return fails


RUNS = {"on": run_on, "off": run_off,
        "parent": lambda p, g, w: run_parent("parent", p, g, w, False),
        "parent-off": lambda p, g, w: run_parent("parent-off", p, g, w, True),
        "old": run_old,
        "old-all": lambda p, g, w: run_old(p, g, w, "old-all", True)}


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="rus-second-")
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
