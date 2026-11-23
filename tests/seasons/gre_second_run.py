#!/usr/bin/env python3
"""Greece: the B sides of Super League clubs follow the real rule (open item 242, 0.7.2; season_gre_second.c).

Usage:
    python3 tests/seasons/gre_second_run.py <cm0102_recomp> <game folder> [--only on,off,rule2020,old] [--jobs n]
        [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py with Greece alone on the October 2021 database
(changing rules off, so the rule of the start year 2021 holds), from the 2021 start to 1 June 2023, so the game plays
the Greek season change of summer 2022 (GrePrm::start_new_season 0x5eaf50: the chain 0x689c20, then 0x5eb120). The
database holds AEK Athens B, Olympiacos Piraeus B, PAOK Thessaloniki B and Panathinaikos Athens B in the Lower
Division, with their first teams in the Super League. RECOMP_GRE_SECOND_LOG=1 logs the census of each change and the
clubs that move. Test switches: RECOMP_TEST_GRE_SECOND_REP=1 gives each B side of the Lower Division the highest
reputation before 0x5eb120, so the original takes them into the second tier first; RECOMP_TEST_GRE_SECOND_UP=1 gives
the best placed B side of each second-tier table a promotion place, and RECOMP_TEST_GRE_SECOND_DOWN=1 the lowest placed
one a relegation place, before the chain, so the cases come about with any results. The runs:
    on       the shipped code (rule of 2021): the new game puts the four B sides into the second tier, no B side goes
             up to the Super League, the B side of status 3 stays in the second tier (2021/22 and 2022/23: no
             relegation for B sides) and another club goes down in its place, and no B side of a Super League club is
             held out of the second tier
    off      RECOMP_GRE_SECOND=off, the original: a B side goes up to the Super League and one goes down (proves that
             the test sees the fault)
    rule2020 the shipped code with RECOMP_TEST_GRE_SECOND_YEAR=2020 (no B side in the national divisions). The new
             game places the four B sides in the second tier by the named lists of 2021/22 (season_clubs.c), so the
             game is as a save of an older build: at the change every B side leaves the second tier for the Lower
             Division, the second tier keeps its size, and no B side enters it
    old      RECOMP_TEST_GRE_SECOND_OLD=1 plays the change of 2022 as the original (a B side goes up into the Super
             League, as a game of an older build), to 1 June 2024: at the change of 2023 the B side leaves the Super
             League and the Super League keeps its size
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
ENV = {"RECOMP_TEST_GRE_SECOND_REP": "1", "RECOMP_GRE_SECOND_LOG": "1", "RECOMP_TEST_GRE_SECOND_UP": "1",
       "RECOMP_TEST_GRE_SECOND_DOWN": "1"}
TAG = "recomp: gre second: "
CENSUS = re.compile(TAG + r"census (before|after): Super League (\d+) clubs, (\d+) B sides; second tier (\d+) clubs, "
                    r"(\d+) B sides; Lower Division (\d+) clubs, (\d+) B sides")


def jump(name, program, game, work, env, last=2022):
    code, _ = rrc.run(program, game, last, 2021, os.path.join(DB, "October 2021"), ["Greece"], [], work, 7200, None,
                      dict(ENV, **env))
    fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    fails += [f"{name}: message box {b}" for b in found]
    return fails, log


def changes(log):
    """The log of each Greek season change: (census before, census after, lines between)."""
    out, cur = [], None
    for line in log.splitlines():
        m = CENSUS.search(line)
        if m and m.group(1) == "before":
            cur = [tuple(map(int, m.groups()[1:])), None, []]
        elif m and cur:
            cur[1] = tuple(map(int, m.groups()[1:]))
            out.append(cur)
            cur = None
        elif cur and line.startswith(TAG):
            cur[2].append(line)
    return out


def b_moves(lines, word, to):
    return [l for l in lines if f" goes {word} to {to} (a B side)" in l]


def first_change(name, log):
    seen = changes(log)
    if not seen:
        return None, [f"{name}: no Greek season change seen"]
    before, after, lines = seen[0]
    print(f"{name}: change 1: before {before}, after {after} (Super League clubs, B sides; second tier clubs, B "
          f"sides; Lower Division clubs, B sides)")
    tests = [l for l in lines if "test:" in l]
    print(f"{name}: test cases: {tests}")
    return seen[0], []


def run_on(program, game, work):
    fails, log = jump("on", program, game, work, {})
    ch, f = first_change("on", log)
    if not ch:
        return fails + f
    before, after, lines = ch
    if before[3] == 0:
        fails.append("on: no B side in the second tier before the change (the test sees no case)")
    if not any("test:" in l for l in lines):
        fails.append("on: the test switches planted no case")
    up = b_moves(lines, "up", "the Super League")
    if up or after[1]:
        fails.append(f"on: B sides in the Super League after the change: {after[1]} ({up})")
    if not any("in place of a second team at the season change" in l for l in lines):
        fails.append("on: no place was handed on from a B side at the season change")
    down = b_moves(lines, "down", "the Lower Division")
    if down:
        fails.append(f"on: B sides went down to the Lower Division in a season with no relegation for them: {down}")
    if not any("may not go down from" in l for l in lines):
        fails.append("on: no B side was kept from going down (the test sees no case)")
    if before[5] and not any("may enter the second tier (rule of 2021" in l for l in lines):
        fails.append("on: no B side of the Lower Division was allowed into the second tier")
    held = [l for l in lines if "stays in the Lower Division" in l]
    if held:
        fails.append(f"on: B sides of Super League clubs were held out of the second tier: {held}")
    if after[0] != before[0]:
        fails.append(f"on: the Super League changed size: {before[0]} to {after[0]}")
    return fails


def run_off(program, game, work):
    fails, log = jump("off", program, game, work, {"RECOMP_GRE_SECOND": "off"})
    ch, f = first_change("off", log)
    if not ch:
        return fails + f
    before, after, lines = ch
    if not after[1] and not b_moves(lines, "up", "the Super League"):
        fails.append("off: no B side went up to the Super League (the test does not see the fault)")
    if not b_moves(lines, "down", "the Lower Division"):
        fails.append("off: no B side went down to the Lower Division (the test does not see the fault)")
    if any("may not go up" in l or "stays in the Lower Division" in l for l in lines):
        fails.append("off: the rule acted with the switch off")
    return fails


def run_rule2020(program, game, work):
    """To 1 June 2024: change 1 (2022) sends the B sides of the second tier down, change 2 (2023) holds the B sides of
    the Lower Division out although each has the highest reputation."""
    fails, log = jump("rule2020", program, game, work, {"RECOMP_TEST_GRE_SECOND_YEAR": "2020"}, last=2023)
    seen = changes(log)
    if len(seen) < 2:
        return fails + [f"rule2020: {len(seen)} Greek season changes seen, want 2"]
    for k, (before, after, lines) in enumerate(seen[:2], 1):
        print(f"rule2020: change {k}: before {before}, after {after}")
        entered = b_moves(lines, "up", "the second tier") + b_moves(lines, "up", "the Super League")
        if entered:
            fails.append(f"rule2020: change {k}: B sides went up: {entered}")
        if after[3] or after[1]:
            fails.append(f"rule2020: change {k}: B sides in the second tier {after[3]}, Super League {after[1]}")
        if after[2] != before[2] or after[0] != before[0]:
            fails.append(f"rule2020: change {k}: sizes changed: {before} to {after}")
    before, after, lines = seen[0]
    if not before[3]:
        fails.append("rule2020: no B side in the second tier before change 1 (the test sees no case)")
    if not b_moves(lines, "down", "the Lower Division"):
        fails.append("rule2020: no B side went down from the second tier at change 1")
    before, after, lines = seen[1]
    if not before[5]:
        fails.append("rule2020: no B side in the Lower Division before change 2 (the test sees no case)")
    if not [l for l in lines if "stays in the Lower Division" in l]:
        fails.append("rule2020: no B side of the Lower Division was held out at change 2")
    return fails


def run_old(program, game, work):
    fails, log = jump("old", program, game, work, {"RECOMP_TEST_GRE_SECOND_OLD": "1"}, last=2023)
    seen = changes(log)
    if len(seen) < 2:
        return fails + [f"old: {len(seen)} Greek season changes seen, want 2"]
    if not seen[0][1][1]:
        fails.append("old: no B side went up into the Super League as the original in the first change (no old save)")
    before, after, lines = seen[1]
    print(f"old: change 2: before {before}, after {after}")
    if not before[1]:
        fails.append("old: no B side in the Super League before change 2 (the test sees no case)")
    if after[1]:
        fails.append(f"old: {after[1]} B sides still in the Super League after change 2")
    if after[0] != before[0]:
        fails.append(f"old: the Super League changed size: {before[0]} to {after[0]}")
    if not b_moves(lines, "down", "the second tier"):
        fails.append("old: no B side went down from the Super League")
    return fails


RUNS = {"on": run_on, "off": run_off, "rule2020": run_rule2020, "old": run_old}


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="gre-second-")
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
