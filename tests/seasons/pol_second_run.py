#!/usr/bin/env python3
"""Poland: reserve sides follow the real rule (open item 243, 0.7.2; season_pol_second.c).

Usage:
    python3 tests/seasons/pol_second_run.py <cm0102_recomp> <game folder> [--only on2018,off2018,...] [--jobs n]
        [--keep <folder>] [--check <name>=<stderr.txt>]

Each run is a season jump of tests/seasons/rules_report_check.py with Poland alone (changing rules off, so the rule
of the start year holds), to the end of the season after the start (the jump of a year Y ends on 1 June of Y + 1, and the Polish change
comes later, on day 170), over the Polish season change of June (PolFirst::start_new_season 0x7c9740, which calls
pol_first_promote_lower_division_clubs 0x7ca030). RECOMP_TEST_POL_SECOND_REP=1 gives each reserve side of the Polish
Lower Division the highest reputation, so the original would send four of them up. RECOMP_POL_SECOND_LOG=1 logs the
clubs held, the clubs that go up and down, and a census before and after each change. The runs:
    on2018   October 2018, the 2018 start (the rule of 2008 to 2018/19): no reserve side goes up into the Second
             Division, four other clubs go up, at least one reserve side is held, and after the change no reserve side
             plays the Second Division or the Ekstraklasa
    off2018  the same with RECOMP_POL_SECOND=off (the original): reserve sides go up (the test sees the fault)
    place2018 on2018 with RECOMP_TEST_POL_SECOND_PLACE=1: two reserve sides take places of the Second Division before
             the call, as a game of an older build; each goes down, and the Second Division keeps its size
    on2024   October 2024, the 2024 start, to 1 June 2027 (the rule from 2019/20): only reserve sides whose
             first team plays the Ekstraklasa go up, those of other clubs are held, each reserve side of the Second
             Division gets the mark 0xfc in 2025/26, and no reserve side reaches the Ekstraklasa
    off2024  the same with RECOMP_POL_SECOND=off: a reserve side of a club below the Ekstraklasa goes up
--check <name>=<file> runs the checks of a run over a saved stderr.txt (test first: the on checks over an off log).
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
ENV = {"RECOMP_TEST_POL_SECOND_REP": "1", "RECOMP_POL_SECOND_LOG": "1"}
T = "recomp: pol second: "
CENSUS = re.compile(T + r"census (before|after) \(season (\d+), rule of (\d+)\): Ekstraklasa (\d+) clubs, (\d+) "
                    r"reserve sides; Second Division (\d+) clubs, (\d+) reserve sides; Lower Division (\d+) clubs, "
                    r"(\d+) reserve sides")
UP = re.compile(T + r"(.*) goes up to the Second Division(?: \(a reserve side; first team (.*), (Ekstraklasa|below "
                r"the Ekstraklasa)\))?$", re.M)
HELD = re.compile(T + r"(.*) stays in the Polish Lower Division \(rule of (\d+): (.*)\)")
DOWN = re.compile(T + r"(.*) goes down from the Second Division \(rule of (\d+): (.*)\)")


def changes(log):
    """Each change: (census before, census after, lines between)."""
    out, cur = [], None
    for line in log.splitlines():
        m = CENSUS.search(line)
        if m and m.group(1) == "before":
            cur = [tuple(map(int, m.groups()[1:])), None, []]
        elif m and cur:
            cur[1] = tuple(map(int, m.groups()[1:]))
            out.append(cur)
            cur = None
        elif cur and T in line:
            cur[2].append(line)
    return out


def check_on2018(log, name="on2018"):
    fails = []
    seen = changes(log)
    if not seen:
        return [f"{name}: no Polish season change seen"]
    before, after, lines = seen[0]
    text = "\n".join(lines)
    ups = UP.findall(text)
    res = [u[0] for u in ups if u[1]]
    print(f"{name}: census before {before}, after {after}; up {[u[0] for u in ups]}; held "
          f"{len(HELD.findall(text))}")
    if res:
        fails.append(f"{name}: reserve sides went up to the Second Division: {res}")
    if len(ups) != 4:
        fails.append(f"{name}: {len(ups)} clubs went up to the Second Division, want 4")
    if not HELD.findall(text):
        fails.append(f"{name}: no reserve side was held in the Lower Division")
    if after[3] or after[5]:
        fails.append(f"{name}: after the change {after[3]} reserve sides in the Ekstraklasa, {after[5]} in the Second "
                     f"Division")
    if after[4] != before[4]:
        fails.append(f"{name}: the Second Division had {before[4]} clubs before, {after[4]} after")
    return fails


def check_off2018(log, name="off2018"):
    seen = changes(log)
    if not seen:
        return [f"{name}: no Polish season change seen"]
    ups = UP.findall("\n".join(seen[0][2]))
    if not [u for u in ups if u[1]]:
        return [f"{name}: no reserve side went up with the switch off (the test does not see the fault)"]
    return []


def check_place2018(log, name="place2018"):
    fails = []
    placed = re.findall(T + r"test: (.*) takes the place of (.*) in the Second Division", log)
    seen = changes(log)
    if not seen:
        return [f"{name}: no Polish season change seen"]
    before, after, lines = seen[0]
    down = [d[0] for d in DOWN.findall("\n".join(lines))]
    print(f"{name}: placed {placed}, census before {before}, after {after}, down {down}")
    if len(placed) != 2:
        fails.append(f"{name}: {len(placed)} reserve sides placed in the Second Division, want 2")
    for p, _ in placed:
        if p not in down:
            fails.append(f"{name}: {p} did not go down from the Second Division")
    if after[5]:
        fails.append(f"{name}: {after[5]} reserve sides still in the Second Division")
    if after[4] != before[4]:
        fails.append(f"{name}: the Second Division had {before[4]} clubs before, {after[4]} after")
    return fails


def check_on2024(log, name="on2024"):
    fails = []
    seen = changes(log)
    if not seen:
        return [f"{name}: no Polish season change seen"]
    if len(seen) < 2:
        fails.append(f"{name}: {len(seen)} Polish season changes seen, want 2")
    text = "\n".join(seen[0][2])
    ups = UP.findall(text)
    res = [(u[0], u[2]) for u in ups if u[1]]
    held = HELD.findall(text)
    print(f"{name}: change 1: census {seen[0][0]} to {seen[0][1]}; reserve sides up {res}; held "
          f"{[(h[0], h[2]) for h in held]}")
    wrong = [r for r, lvl in res if lvl != "Ekstraklasa"]
    if wrong:
        fails.append(f"{name}: reserve sides of clubs below the Ekstraklasa went up: {wrong}")
    if not res:
        fails.append(f"{name}: no reserve side of an Ekstraklasa club went up (the allowed case is not seen)")
    if not any("below the Ekstraklasa" in h[2] for h in held):
        fails.append(f"{name}: no reserve side of a club below the Ekstraklasa was held (the barred case is not seen)")
    if any("in the top two tiers" in h[2] for h in held):
        fails.append(f"{name}: the rule of 2008 to 2018/19 held a reserve side in 2024")
    marks = re.findall(T + r"(.*) may not go up from the Second Division", log)
    print(f"{name}: marked {sorted(set(marks))}; change 2: census {seen[1][0] if seen[1:] else None} to "
          f"{seen[1][1] if seen[1:] else None}")
    if not marks:
        fails.append(f"{name}: no reserve side of the Second Division got the mark 0xfc")
    for b, a, _ in seen[:2]:
        if b[3] or a[3]:
            fails.append(f"{name}: reserve sides in the Ekstraklasa: census {b} to {a}")
    return fails


def check_off2024(log, name="off2024"):
    seen = changes(log)
    if not seen:
        return [f"{name}: no Polish season change seen"]
    ups = UP.findall("\n".join(seen[0][2]))
    if not [u for u in ups if u[1] and u[2] != "Ekstraklasa"]:
        return [f"{name}: no reserve side of a club below the Ekstraklasa went up with the switch off"]
    return []


def jump(name, program, game, work, env, last, year, db):
    code, _ = rrc.run(program, game, last, year, os.path.join(DB, db), ["Poland"], [], work, 7200, None,
                      dict(ENV, **env))
    fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    fails += [f"{name}: message box {b}" for b in found]
    return fails, log


RUNS = {
    "on2018": ({}, 2019, 2018, "October 2018", check_on2018),
    "off2018": ({"RECOMP_POL_SECOND": "off"}, 2019, 2018, "October 2018", check_off2018),
    "place2018": ({"RECOMP_TEST_POL_SECOND_PLACE": "1"}, 2019, 2018, "October 2018", check_place2018),
    "on2024": ({}, 2026, 2024, "October 2024", check_on2024),
    "off2024": ({"RECOMP_POL_SECOND": "off"}, 2025, 2024, "October 2024", check_off2024),
}


def run_one(name, program, game, work):
    env, last, year, db, check = RUNS[name]
    fails, log = jump(name, program, game, work, env, last, year, db)
    return fails + check(log, name)


def main(argv):
    checks = [a.split("=", 1) for i, a in enumerate(argv) if i and argv[i - 1] == "--check"]
    if checks:
        fails = []
        for name, path in checks:
            kind = re.sub(r"-.*", "", name)
            fails += RUNS[kind][4](open(path, encoding="utf-8", errors="replace").read(), name)
        for f in fails:
            print("  " + f)
        print("PASS" if not fails else "FAIL")
        return 0 if not fails else 1
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="pol-second-")
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        futures = {n: ex.submit(run_one, n, program, game, os.path.join(keep, n)) for n in only}
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
