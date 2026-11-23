#!/usr/bin/env python3
"""Croatia's match squad limit by season (GitHub issue 135).

Usage:
    python3 tests/seasons/croatia_non_eu_run.py <cm0102_recomp> <game folder> [--only <name>,...] [--keep <folder>]
        [--reuse]

The original gives every Croatian competition a limit of 4 foreign players in the match squad
(CROATIA_RULES::fill_squad_restrictions 0x518620 writes 4 at +0 of the restriction record), so a player of the EU
counts too. From 2013/14 the HNS counts only players not of the EU or EEA (HNS Glasnik 28/2013, Prva HNL
propozicije article 16): 6 to 2016/17, 8 in 2017/18 and 2018/19, 6 from 2019/20, 5 from 2026/27
(docs/research/rules-0.6.0/gre-pol-cro.tsv). Under season rules the kind's wrap of the lookup 0x8cf6b0
(season_kind_nation.c) moves the limit to the non-EU slots (season_nation_squad_record).

Each run makes a new game with England and Croatia, changing_rules on and RECOMP_SEASON_NATION_LOG=1, and plays
the start season. The runs:
    on2020   November 2020, 2020/21: Croatian top records become 6 non-EU (+3 or +5), with no foreign slot;
             lower records become the game's 4 as non-EU
    on2010   September 2010 ODB, 2010/11: Croatian records keep 4 foreign and no non-EU slot
Both runs also check that Croatian lookups were logged and that no message box came up.
"""
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
import rules_report_check as rrc  # noqa: E402
import uefa_formats_run as ufr  # noqa: E402

DB = "/Users/max/Projects/cm0102/test-fixtures/databases"
# name: (start year, database, the kind of the Croatian record after the lookup, the top number)
RUNS = {
    "on2020": (2020, os.path.join(DB, "November 2020"), "non-eu", 6),
    "on2010": (2010, os.path.join(DB, "September 2010 ODB"), "foreign", 4),
}
LINE = re.compile(r"season nation: Croatia (\S+) (\d+) (top|lower) squad record ((?:[0-9a-f]{2} ){5}[0-9a-f]{2}) -> "
                  r"((?:[0-9a-f]{2} ){5}[0-9a-f]{2})")


def check_record(name, kind, top_n, level, before, after):
    b = [int(x, 16) for x in before.split()]
    a = [int(x, 16) for x in after.split()]
    foreign, non_eu = [v for v in a[:3] if v != 0xff], [v for v in a[3:] if v != 0xff]
    want = top_n if level == "top" else 4
    if kind == "non-eu":
        ok = not foreign and non_eu == [want] and [v for v in b[:3] if v != 0xff] == [4]
    else:
        ok = not non_eu and foreign == [want]
    return [] if ok else [f"{name}: {level} record {before} -> {after}, expected {want} {kind}"]


def check_run(name, program, game, keep, reuse):
    year, database, kind, top_n = RUNS[name]
    work = os.path.join(keep, name) if keep else tempfile.mkdtemp(prefix=f"cm0102-i135-{name}-")
    if not (reuse and os.path.exists(os.path.join(work, "stderr.txt"))):
        code, _ = rrc.run(program, game, year, year, database, ["England", "Croatia"], [], work, 7200,
                          {"changing_rules": "on"}, {"RECOMP_SEASON_NATION_LOG": "1"})
        if code != 0:
            return [f"{name}: the run ended with code {code} ({work})"]
    fails = []
    found, _ = ufr.boxes(work)
    fails += [f"{name}: message box {b}" for b in found]
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read().split("\n")
    lines = [m for m in (LINE.search(x) for x in log) if m]
    levels = {m.group(3) for m in lines}
    if "top" not in levels:
        fails.append(f"{name}: no Croatian top division lookup in the log ({work})")
    for m in lines:
        if int(m.group(2)) != year:
            fails.append(f"{name}: a lookup in season {m.group(2)}, expected {year}")
        fails += check_record(name, kind, top_n, m.group(3), m.group(4), m.group(5))
    print(f"{name}: {len(lines)} Croatian lookups logged ({', '.join(sorted(levels))}); {len(fails)} problems ({work})")
    for m in lines[:3]:
        print(f"    {m.group(1)} {m.group(3)}: {m.group(4)} -> {m.group(5)}")
    return fails


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    keep = os.path.abspath(rrc.arg(argv, "--keep")) if "--keep" in argv else None
    fails = []
    for n in only:                       # one game at a time
        fails += check_run(n, program, game, keep, "--reuse" in argv)
    for t in fails:
        print("FAIL  " + t)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
