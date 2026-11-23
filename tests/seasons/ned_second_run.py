#!/usr/bin/env python3
"""Netherlands: a Jong side never goes up from the Eerste Divisie (open item 114, 0.7.0; season_ned_second.c).

Usage:
    python3 tests/seasons/ned_second_run.py <cm0102_recomp> <game folder> [--only on,off,drop,late,old] [--jobs n]
        [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py with Holland alone on the October 2024 database
(changing rules off; the database has Ajax Amsterdam U21 and PSV Eindhoven U21 in the Eerste Divisie, and AZ, FC
Utrecht and Sparta Rotterdam U21 sides in A Lower Division). RECOMP_NED_SECOND_LOG=1 logs the marks, the play-off
clubs, the clubs that go up and down at the season change (HolPrm's chain 0x689c20) and a count of the Jong sides
before and after it. The runs:
    on    RECOMP_TEST_NED_SECOND_TOP=1 (each Jong row 30 points above the best other row of the Eerste Divisie and of
          each period) to 1 June 2026: the Jong sides are marked, no Jong side plays the play-off or goes up, as
          many first teams go up as clubs leave the Eredivisie (at least one), and the Eredivisie has no Jong side
          after the change
    off   the same with RECOMP_NED_SECOND=off, the original: a Jong side goes up or plays the play-off (proves that
          the test sees the fault)
    drop  RECOMP_TEST_NED_SECOND_DROP=1 (the first teams of the Jong sides 30 points below the lowest row of the
          Eredivisie) to 1 June 2026: at the change each Jong side whose first team went down is in A Lower
          Division after the move, another club is in the Eerste Divisie in its place, and the Eerste Divisie keeps
          its size
    late  RECOMP_TEST_NED_SECOND_LATE=1 and TOP=1 to 1 June 2026: no marks until the first change, so a Jong side
          wins the 2024/25 title and the play-off is played as the original plays it (the state of a save made after
          the play-off, in memory). At the change the Jong side's place goes to the best placed club not already up,
          no Jong side goes up, and as many clubs go up as leave the Eredivisie
    old   the state of an old save, made in memory (no save is written or loaded): RECOMP_TEST_NED_SECOND_OLD=1 and
          TOP=1 play as the original until the first change, so a Jong side goes up into the Eredivisie. To 1 June
          2027: at the second change each Jong side leaves the Eredivisie, and the Eredivisie keeps its size
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
ENV = {"RECOMP_NED_SECOND_LOG": "1"}
CENSUS = re.compile(r"recomp: ned second: census (before|after): Eredivisie (\d+) clubs, (\d+) Jong sides; "
                    r"Eerste Divisie (\d+) clubs, (\d+) Jong sides")


def jump(name, program, game, work, env, last):
    code, _ = rrc.run(program, game, last, 2024, os.path.join(DB, "October 2024"), ["Holland"], [], work, 7200, None,
                      dict(ENV, **env))
    fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    fails += [f"{name}: message box {b}" for b in found]
    return fails, log


def changes(log):
    """Each Dutch season change: (census before, census after, lines between)."""
    out, cur = [], None
    for line in log.splitlines():
        m = CENSUS.search(line)
        if m and m.group(1) == "before":
            cur = [tuple(map(int, m.groups()[1:])), None, []]
        elif m and cur:
            cur[1] = tuple(map(int, m.groups()[1:]))
            out.append(cur)
            cur = None
        elif cur and "recomp: ned second:" in line:
            cur[2].append(line)
    return out


def up_lines(lines):
    return [l.split("ned second: ")[1] for l in lines if "goes up to the Eredivisie" in l]


def run_on(program, game, work):
    fails, log = jump("on", program, game, work, {"RECOMP_TEST_NED_SECOND_TOP": "1"}, 2025)
    seen = changes(log)
    if not seen:
        return fails + ["on: no Dutch season change seen"]
    before, after, lines = seen[0]
    print(f"on: change 1: before {before}, after {after} (Eredivisie clubs, Jong sides; Eerste clubs, Jong sides)")
    print(f"on: up: {up_lines(lines)}")
    if "may not go up from the Eerste Divisie" not in log:
        fails.append("on: no Jong row was marked in the Eerste Divisie")
    if "may not go up from a period" not in log:
        fails.append("on: no Jong row was marked in a period table")
    jong_po = [l for l in log.splitlines() if "play-off:" in l and "(a Jong side)" in l]
    if jong_po:
        fails.append(f"on: Jong sides in the play-off: {jong_po}")
    if not [l for l in log.splitlines() if "ned second: play-off:" in l]:
        fails.append("on: no play-off club logged")
    went = [l for l in up_lines(lines) if "(a Jong side)" in l]
    if went:
        fails.append(f"on: Jong sides went up: {went}")
    left = [l for l in lines if "leaves the Eredivisie" in l]
    if not up_lines(lines) or len(up_lines(lines)) != len(left):
        fails.append(f"on: {len(up_lines(lines))} clubs went up and {len(left)} left the Eredivisie, want the same "
                     "number and at least one")
    if after[1]:
        fails.append(f"on: {after[1]} Jong sides in the Eredivisie after the change")
    if before[3] == 0:
        fails.append("on: no Jong side in the Eerste Divisie before the change (the test sees no case)")
    if after[0] != before[0] or after[2] != before[2]:
        fails.append(f"on: division sizes changed: {before} to {after}")
    return fails


def run_off(program, game, work):
    fails, log = jump("off", program, game, work, {"RECOMP_TEST_NED_SECOND_TOP": "1", "RECOMP_NED_SECOND": "off"},
                      2025)
    seen = changes(log)
    if "may not go up" in log:
        fails.append("off: Jong rows were marked with the switch off")
    went = [l for s in seen for l in up_lines(s[2]) if "(a Jong side)" in l]
    print(f"off: Jong sides up: {went}")
    if not went:
        fails.append("off: no Jong side went up with the original (the test does not see the fault)")
    return fails


def run_drop(program, game, work):
    fails, log = jump("drop", program, game, work, {"RECOMP_TEST_NED_SECOND_DROP": "1"}, 2025)
    seen = changes(log)
    if not seen:
        return fails + ["drop: no Dutch season change seen"]
    before, after, lines = seen[0]
    down = [l for l in lines if "goes down from the Eerste Divisie (its first team" in l]
    print(f"drop: change 1: before {before}, after {after}")
    for l in down:
        print("drop: " + l.split("ned second: ")[1])
    if not down:
        fails.append("drop: no Jong side went down with its first team")
    now = re.findall(r"ned second: now (.*) in (.*), (.*) in (.*)$", "\n".join(lines), re.M)
    for side, sdiv, nxt, ndiv in now:
        print(f"drop: now {side} in {sdiv}, {nxt} in {ndiv}")
        if sdiv != "A Lower Division":
            fails.append(f"drop: {side} is in {sdiv} after the move, want A Lower Division")
        if ndiv not in ("Dutch First Division", "Eerste Divisie"):    # the database name, or the season name
            fails.append(f"drop: {nxt} is in {ndiv} after the move, want the Eerste Divisie")
    if len(now) != len(down):
        fails.append(f"drop: {len(down)} Jong sides went down, {len(now)} moves logged")
    if after[0] != before[0] or after[2] != before[2]:
        fails.append(f"drop: division sizes changed: {before} to {after}")
    return fails


def run_old(program, game, work):
    fails, log = jump("old", program, game, work, {"RECOMP_TEST_NED_SECOND_TOP": "1", "RECOMP_TEST_NED_SECOND_OLD": "1"},
                      2026)
    seen = changes(log)
    if len(seen) < 2:
        return fails + [f"old: {len(seen)} Dutch season changes seen, want 2"]
    if not [l for l in up_lines(seen[0][2]) if "(a Jong side)" in l]:
        fails.append("old: no Jong side went up as the original in the first change (no old save made)")
    before, after, lines = seen[1]
    print(f"old: change 2: before {before}, after {after}")
    for l in lines:
        if "goes down from the Eredivisie" in l or "goes up to the Eredivisie" in l:
            print("old: " + l.split("ned second: ")[1])
    if not before[1]:
        fails.append("old: no Jong side in the Eredivisie before change 2 (the test sees no case)")
    if after[1]:
        fails.append(f"old: {after[1]} Jong sides still in the Eredivisie after change 2")
    if after[0] != before[0] or after[2] != before[2]:
        fails.append(f"old: division sizes changed: {before} to {after}")
    went = [l for l in up_lines(lines) if "(a Jong side)" in l]
    if went:
        fails.append(f"old: Jong sides went up: {went}")
    return fails


def run_late(program, game, work):
    fails, log = jump("late", program, game, work, {"RECOMP_TEST_NED_SECOND_TOP": "1",
                                                    "RECOMP_TEST_NED_SECOND_LATE": "1"}, 2025)
    seen = changes(log)
    if not seen:
        return fails + ["late: no Dutch season change seen"]
    before, after, lines = seen[0]
    handed = [l.split("ned second: ")[1] for l in lines if "in place of a Jong side after the play-off" in l]
    print(f"late: change 1: before {before}, after {after}; {handed}")
    if not handed:
        fails.append("late: no place was handed on after the play-off (the case is not seen)")
    went = [l for l in up_lines(lines) if "(a Jong side)" in l]
    if went:
        fails.append(f"late: Jong sides went up: {went}")
    left = [l for l in lines if "leaves the Eredivisie" in l]
    if not up_lines(lines) or len(up_lines(lines)) != len(left):
        fails.append(f"late: {len(up_lines(lines))} clubs went up and {len(left)} left the Eredivisie")
    if after[0] != before[0] or after[2] != before[2]:
        fails.append(f"late: division sizes changed: {before} to {after}")
    return fails


RUNS = {"on": run_on, "off": run_off, "drop": run_drop, "late": run_late, "old": run_old}


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="ned-second-")
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
