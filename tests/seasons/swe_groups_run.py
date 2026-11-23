#!/usr/bin/env python3
"""Sweden: the third tier plays its real groups (issue 106; season_swe_groups.c).

Usage:
    python3 tests/seasons/swe_groups_run.py <cm0102_recomp> <game folder> [--only s2004,s2025] [--jobs n]
        [--keep <folder>] [--databases <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py with Sweden alone and its extra division (the test
switch RECOMP_TEST_EXTRA=sweden), changing rules on:
    s2004  the short proof: a 2004/05 start on Summer 2004: six groups of 12 in 2005, then 2 x 14 to 2008, the
           season in which the Allsvenskan grows to 16 and the Superettan must stay at 16
    s2001  a 2001/02 start played to 2019: six groups of 12 to the Swedish season of 2005, then 2 x 14, then 2 x 16
    s2025  a 2025/26 start on the December 2025 database played to 2027: 2 x 16 from the new game on
    off    a 2025/26 start with changing rules off: the six groups of the class stay, with no line of the groups
The game logs one line for each edition of two groups. The check compares each line with the row of that edition in
the rules table, and wants a line for every edition that the row plays in two groups and for no other edition.
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

LINE = re.compile(r"recomp: swe groups: edition (\d+): Norra (\d+) clubs, Sodra (\d+) clubs \(row 2 x (\d+)\), "
                  r"Superettan (\d+) clubs")
SPLIT = re.compile(r"2 groups of (\d+)")
RUNS = {"s2004": (2004, 2007, "on"), "s2001": (None, 2019, "on"), "s2025": (2025, 2027, "on"), "off": (2025, 2026, "off")}


def group_sizes():
    """{edition: clubs of a group} of the rows in two groups. A Swedish edition is the real season of its row."""
    out = {}
    for r in rrc.make_rules.read_rows():
        m = SPLIT.match(r["split_text"]) if r["comp_file"] == "swe_second" else None
        if m:
            out[int(r["real_season"])] = int(m.group(1))
    return out


def superettan_clubs():
    """{edition: clubs of the Superettan} of the rules table."""
    return {int(r["real_season"]): int(r["clubs"]) for r in rrc.make_rules.read_rows() if r["comp_file"] == "swe_first"}


def check(name, log, first, last, rules):
    fails, seen = [], {}
    for m in LINE.finditer(log):
        seen[int(m.group(1))] = tuple(int(g) for g in m.groups()[1:])
    want = {y: n for y, n in group_sizes().items() if first <= y <= last} if rules == "on" else {}
    for y, n in sorted(want.items()):
        if y not in seen:
            fails.append(f"{name}: edition {y}: no line of the groups, the row plays 2 x {n}")
        elif seen[y][:3] != (n, n, n):
            fails.append(f"{name}: edition {y}: Norra {seen[y][0]}, Sodra {seen[y][1]}, the row 2 x {n}")
        elif seen[y][3] != superettan_clubs().get(y):
            fails.append(f"{name}: edition {y}: Superettan {seen[y][3]} clubs, the row {superettan_clubs().get(y)}")
    fails += [f"{name}: edition {y}: a line of the groups, the row plays none" for y in sorted(set(seen) - set(want))]
    print(f"{name}: editions in two groups: " + (", ".join(f"{y} {seen[y][0]}+{seen[y][1]}" for y in sorted(seen)) or "none"))
    return fails


def run(name, program, game, work, databases):
    year, last, rules = RUNS[name]
    if year in rrc.OWN_DATABASES:
        database = rrc.own_database(year, rrc.OWN_DBS, work + "-db")
    else:
        database = rrc.season_database(year, databases) if year else None
    code, _ = rrc.run(program, game, last, year, database, ["Sweden"], [], work, 14400, {"changing_rules": rules},
                      {"RECOMP_TEST_EXTRA": "sweden"})
    fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    if "rules table: none" in log:
        fails.append(f"{name}: the game read no rules table")
    fails += [f"{name}: message box {b}" for b in found]
    if "extra division of sweden on" not in log:
        fails.append(f"{name}: the Swedish extra division was not set")
    # A jump plays the season `last` to its end, so the deal of the edition after it is in the log too.
    return fails + check(name, log, (year or 2001) + 1, last + 1, rules)


def main():
    args = sys.argv[1:]
    if len(args) < 2:
        raise SystemExit(__doc__)
    program, game = os.path.abspath(args[0]), os.path.abspath(args[1])

    def opt(flag, default=None):
        return args[args.index(flag) + 1] if flag in args else default

    only = opt("--only", "s2004").split(",")
    base = opt("--keep") or tempfile.mkdtemp(prefix="cm0102-swe-groups-")
    databases = os.path.abspath(opt("--databases", rrc.DATABASES))
    fails = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=int(opt("--jobs", "1"))) as pool:
        futures = [pool.submit(run, n, program, game, os.path.join(base, n), databases) for n in only]
        for f in concurrent.futures.as_completed(futures):
            fails += f.result()
    for f in fails:
        print("FAIL", f)
    print("PASS" if not fails else f"FAIL ({len(fails)})")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
