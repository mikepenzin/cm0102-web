#!/usr/bin/env python3
"""France: a reserve side does not go up from the CFA into the National (open item 116, 0.7.0; season_fra_second.c).

Usage:
    python3 tests/seasons/fra_second_run.py <cm0102_recomp> <game folder> [--only on,off] [--jobs n] [--keep <folder>]
        [--last <year>]

Each run is a season jump of tests/seasons/rules_report_check.py with France alone on the November 2020 database
(changing rules off, 12 reserve sides in the CFA), from the 2020 start to the end of the season that starts in
--last (default 2021, to 1 June 2022), so the game plays the French season change (FraFirst::start_new_season 0x5bc4d0, which calls
FraFirst::exchange_cfa_national_clubs 0x5bc780). The 12 reserve sides of the database's French CFA keep the CFA
zone in A Lower Division, so they are in the list. The test switch RECOMP_TEST_FRA_SECOND_REP=1 gives each listed
French reserve side the highest reputation, so the original would send four of them up.
RECOMP_FRA_SECOND_LOG=1 logs the reserve sides held back and the clubs that go up. The runs:
    on    the shipped code: no reserve side goes up, four other clubs go up, and reserve sides stay in the CFA
    off   RECOMP_FRA_SECOND=off, the original's list: reserve sides go up (proves that the test sees the fault)
    old1  the state of an old save, made in memory (no save is written or loaded): RECOMP_TEST_FRA_SECOND_OLD=1
          plays as the original until the season change of 2021 is done, so reserve sides go up into the National,
          and RECOMP_TEST_FRA_SECOND_NO_MARK=1 lets the 2021/22 table give them places as the original (as a save
          made after the table was complete). To 1 June 2023: at the change of 2022 every reserve side leaves the
          National, the three divisions keep their sizes, and no reserve side goes up
    old2  the same with the marks: the 2021/22 season plays with the fix, so no reserve side holds a place
    old3  RECOMP_TEST_FRA_SECOND_OLD=2 with RECOMP_TEST_FRA_SECOND_PROMOTE=1: as the original to the change of 2022,
          which also gives the reserve sides of the National its promotion places, so reserve sides go up into
          Ligue 2. To 1 June 2024: the 2022/23 season marks them in Ligue 2, and at the change of 2023 every reserve
          side leaves Ligue 2 and the National, with the three divisions at their sizes
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
ENV = {"RECOMP_TEST_FRA_SECOND_REP": "1", "RECOMP_FRA_SECOND_LOG": "1"}
LAST = 2021


CENSUS = re.compile(r"recomp: fra second: census (before|after): Ligue 1 (\d+) clubs, (\d+) reserve sides; Ligue 2 "
                    r"(\d+) clubs, (\d+) reserve sides; the National (\d+) clubs, (\d+) reserve sides")


def changes(log):
    """Each French season change: (census before, census after, the log lines between)."""
    out, cur = [], None
    for line in log.splitlines():
        m = CENSUS.search(line)
        if m and m.group(1) == "before":
            cur = [tuple(map(int, m.groups()[1:])), None, []]
        elif m and cur:
            cur[1] = tuple(map(int, m.groups()[1:]))
            out.append(cur)
            cur = None
        elif cur and "recomp: fra second:" in line:
            cur[2].append(line)
    return out


def jump(name, program, game, work, env, last=None):
    code, _ = rrc.run(program, game, last or LAST, 2020, os.path.join(DB, "November 2020"), ["France"], [], work, 7200, None,
                      dict(ENV, **env))
    fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    fails += [f"{name}: message box {b}" for b in found]
    up = re.findall(r"recomp: fra second: (.*) goes up to the National( \(a reserve side\))?", log)
    stays = re.findall(r"recomp: fra second: (.*) stays in the CFA", log)
    listed = re.findall(r"recomp: fra second: listed reserve side (.*), reputation", log)
    print(f"{name}: {len(up)} clubs up ({sum(1 for _, s in up if s)} reserve sides), {len(stays)} held back, "
          f"{len(listed)} reserve sides listed")
    if not listed:
        fails.append(f"{name}: no reserve side in the list of 0x5bc780 (no season change seen, or no case)")
    return fails, up, stays


def run_on(program, game, work):
    fails, up, stays = jump("on", program, game, work, {})
    seconds = [n for n, s in up if s]
    if seconds:
        fails.append(f"on: reserve sides went up to the National: {', '.join(seconds)}")
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    seen = changes(log)
    first = [l for l in seen[0][2] if "goes up to the National" in l] if seen else []
    if len(first) != 4:
        fails.append(f"on: {len(first)} clubs went up to the National in the first change, want 4")
    if not stays:
        fails.append("on: no reserve side stayed in the CFA")
    return fails


def run_off(program, game, work):
    fails, up, stays = jump("off", program, game, work, {"RECOMP_FRA_SECOND": "off"})
    if stays:
        fails.append(f"off: {len(stays)} reserve sides were held back with the switch off")
    if not [n for n, s in up if s]:
        fails.append("off: no reserve side went up with the original's list (the test does not see the fault)")
    return fails


def run_old(name, program, game, work, no_mark, promote=False):
    n = 2 if promote else 1
    env = {"RECOMP_TEST_FRA_SECOND_OLD": str(n), **({"RECOMP_TEST_FRA_SECOND_NO_MARK": "1"} if no_mark else {}),
           **({"RECOMP_TEST_FRA_SECOND_PROMOTE": "1"} if promote else {})}
    fails, _, _ = jump(name, program, game, work, env, last=LAST + n)
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    seen = changes(log)
    if len(seen) < n + 1:
        return fails + [f"{name}: {len(seen)} French season changes seen, want {n + 1}"]
    if not any("goes up to the National (a reserve side)" in l for l in seen[0][2]):
        fails.append(f"{name}: no reserve side went up as the original in the first change (no old save made)")
    before, after, lines = seen[n]
    print(f"{name}: change {n + 1}: before {before}, after {after} (Ligue 1, Ligue 2, the National: clubs, reserve "
          f"sides)")
    if promote and before[3] == 0:
        fails.append(f"{name}: no reserve side in Ligue 2 before change {n + 1} (the Ligue 2 case is not seen)")
    if promote and not any("may not go up from Ligue 2" in l for l in log.splitlines()):
        fails.append(f"{name}: no reserve side was marked in Ligue 2's table")
    if promote and not any("goes on from the National to the CFA" in l for l in lines):
        fails.append(f"{name}: no reserve side went on from the National to the CFA (the Ligue 2 path is not seen)")
    if not promote and before[5] == 0:
        fails.append(f"{name}: no reserve side in the National before change 2 (the test sees no case)")
    if after[1] or after[3] or after[5]:
        fails.append(f"{name}: reserve sides above the CFA after change {n + 1}: {after}")
    if (after[0], after[2], after[4]) != (before[0], before[2], before[4]):
        fails.append(f"{name}: division sizes changed: {before} to {after}")
    went = [l for l in lines if "(a reserve side)" in l]
    if went:
        fails.append(f"{name}: reserve sides went up: {went}")
    if not any("goes down from the National" in l for l in lines):
        fails.append(f"{name}: no reserve side went down from the National")
    lost = [l for l in lines if "stays in" in l and "one more season" in l]
    if lost:
        fails.append(f"{name}: reserve sides kept for one more season: {lost}")
    marks = [l for l in log.splitlines() if "may not go up from the National" in l]
    if no_mark and marks:
        fails.append(f"{name}: {len(marks)} marks with RECOMP_TEST_FRA_SECOND_NO_MARK=1")
    if not no_mark and not marks:
        fails.append(f"{name}: no reserve side was marked in the National's table (the mark is not seen)")
    shifted = [l for l in lines if "in place of a second team" in l]       # information: depends on the table
    print(f"{name}: {len(shifted)} places handed on at the season change")
    return fails


RUNS = {"on": run_on, "off": run_off,
        "old1": lambda p, g, w: run_old("old1", p, g, w, True),
        "old2": lambda p, g, w: run_old("old2", p, g, w, False),
        "old3": lambda p, g, w: run_old("old3", p, g, w, False, promote=True)}


def main(argv):
    global LAST
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    LAST = int(rrc.arg(argv, "--last", str(LAST)))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="fra-second-")
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
