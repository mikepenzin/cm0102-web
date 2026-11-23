#!/usr/bin/env python3
"""Competition history of the official data sets in later start years (0.6.2 open item 149).

Usage:
    python3 tests/seasons/history_official_run.py <cm0102_recomp> <game folder> <folder of databases>
        [--only <run>,...] [--jobs n]

The official data sets (3.9.60, 3.9.65, 3.9.68) hold the history of 2001 in every start year. Before the fix, a
new game of such data in 2003 or later held no row of the label years 2002 to the start (no World Cup 2002, no
Euro 2004, no OFC Nations Cup 2002 or 2004). The fill now writes the researched rows of
docs/research/history-0.6.0/plan-official.tsv that hold for the start year. Each run is a new game (England, seed
1) to the first news screen with the test log RECOMP_HISTORY_LOG (season_history.c). The runs:
    3.9.68-2003, -2004, -2006, -2010, -2014  the game's own 3.9.68 data (2006, 2010 and 2014: World Cup finals in
                                             July of the start year)
    3.9.65-2004, 3.9.60-2004, 3.9.60-2002    the official versions from the folder of databases
    Summer 2004                             a season database in its own season (plan.tsv): the rows it had
The checks, at the database load and at the first turn (the game then keeps the rows of the selected nations,
0x539ae0, so only the competitions it kept are checked there):
    plan    for each competition below, every label year from 2001 to the start year that the plan of that
            database fills for this start year holds exactly the planned first and second, and no other row.
    real    the winners of FACTS (public records, independent of the research table) for each edition that ended
            before the start: Euro 2004 Greece and the OFC Nations Cups of 2002 and 2004 in a 2004 game, and so on.
At most --jobs (2) games run at the same time.
"""
import collections
import concurrent.futures
import csv
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import season_dates_run as sdr  # noqa: E402
import season_history_run as shr  # noqa: E402
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "seasons"))
from make_history_table import label_of  # noqa: E402  the label the game keeps (the Irish short season: 2002)

HIST = os.path.join(ROOT, "docs", "research", "history-0.6.0")
EURO, OFC, WC = "European Football Championship", "Oceania Nations Cup", "FIFA World Cup"
COMPS = shr.COMPS + [EURO, OFC]
# Real winners and the first start year that holds each edition. A new game's first day is about 10 July. The
# World Cup and the Euro count in the start year of a July final too: the game does not play them then (history_fill.py,
# JULY_FINALS). The OFC Nations Cup 2004 (final 12 October 2004) counts from 2004 by the plan's calendar-year rule, as
# in Summer 2004. Sources: en.wikipedia.org, 2002 to 2014 FIFA World Cup, UEFA Euro 2004, 2008 and 2012, 2002, 2004
# and 2008 OFC Nations Cup; the Premier League and UEFA Champions League finals of each season.
FACTS = {  # competition: {label: (part of the winner's name, first start year that holds it)}
    WC: {2002: ("Brazil", 2002), 2006: ("Italy", 2006), 2010: ("Spain", 2010), 2014: ("Germany", 2014)},
    EURO: {2004: ("Greece", 2004), 2008: ("Spain", 2008), 2012: ("Spain", 2012)},
    OFC: {2002: ("New Zealand", 2003), 2004: ("Australia", 2004), 2008: ("New Zealand", 2009)},
    shr.EPL: {2002: ("Arsenal", 2002), 2003: ("Manchester United", 2003), 2004: ("Arsenal", 2004),
              2005: ("Chelsea", 2005), 2006: ("Chelsea", 2006), 2009: ("Manchester United", 2009)},
    "European Champions Cup": {2004: ("Porto", 2004), 2009: ("Barcelona", 2009)},
}
RUNS = [("3.9.68-2003", "3.9.68", 2003), ("3.9.68-2004", "3.9.68", 2004), ("3.9.68-2006", "3.9.68", 2006),
        ("3.9.68-2010", "3.9.68", 2010), ("3.9.68-2014", "3.9.68", 2014), ("3.9.65-2004", "3.9.65", 2004),
        ("3.9.60-2004", "3.9.60", 2004), ("3.9.60-2002", "3.9.60", 2002), ("Summer 2004", "Summer 2004", 2004)]


def planned(db, start):
    """{competition: {label: set of (first, second)}} that the plan fills for database db in start year start."""
    want = collections.defaultdict(lambda: collections.defaultdict(set))
    for fn in ("plan.tsv", "plan-official.tsv"):
        with open(os.path.join(HIST, fn), newline="", encoding="utf-8") as f:
            for r in csv.DictReader(f, delimiter="\t"):
                if r["database"] != db:
                    continue
                first = int(r["start_year"])
                last = int(r["last_start"]) if r.get("last_start") else (first if "last_start" not in r else 9999)
                if not first <= start <= last:
                    continue
                c = want[r["competition"]]
                if r["action"] == "clear":
                    c.setdefault(int(r["label"]), set())
                else:
                    c[label_of(r)].add((r["first"], r["second"]))
    return want


def check(start, got, want, turn):
    bad, n = [], 0
    for comp in COMPS:
        rows = got.get(comp, {})
        if comp not in want or (turn and not rows):
            continue
        for y in range(shr.FIRST, start + 1):
            if y not in want[comp]:
                continue
            n += 1
            have = rows.get(y, [])
            if set(have) != want[comp][y] or len(have) != len(want[comp][y]):
                bad.append(f"{comp} {y}: holds {sorted(set(have)) or 'nothing'}, the plan {sorted(want[comp][y]) or 'nothing'}")
    for comp, facts in FACTS.items():
        rows = got.get(comp, {})
        if turn and not rows:
            continue
        for y, (part, from_start) in facts.items():
            if start < from_start:
                continue
            n += 1
            first = [a for a, b in rows.get(y, [])]
            if len(first) != 1 or part.lower() not in first[0].lower():
                bad.append(f"{comp} {y}: winner {first or 'none'}, real {part}")
    return n, bad


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2
    program, game, dbs_dir = (os.path.abspath(a) for a in argv[1:4])
    jobs_n = int(argv[argv.index("--jobs") + 1]) if "--jobs" in argv else 2
    only = argv[argv.index("--only") + 1].split(",") if "--only" in argv else None
    dbs = sdr.databases(dbs_dir)
    work = tempfile.mkdtemp(prefix="history-official-")
    failed, runs = 0, [r for r in RUNS if not only or r[0] in only]
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs_n) as ex:
        futs = {}
        for label, db, year in runs:
            folder = None
            if db != "3.9.68":
                if db not in dbs:
                    print(f"FAIL  {label}: {db} is not in {dbs_dir}")
                    failed += 1
                    continue
                folder = sdr.neutral_copy(work, db, dbs[db][1])
            env = {"RECOMP_HISTORY_LOG": ";".join(COMPS)}
            futs[ex.submit(sdr.play, program, game, work, label, year, folder, env)] = (label, db, year)
        for f in concurrent.futures.as_completed(futs):
            label, db, year = futs[f]
            save, log, secs = f.result()
            got, turn = shr.parse(log, "load"), shr.parse(log, "turn")
            if not turn or not save:
                print(f"FAIL  {label}: no history log of the first turn or no save ({secs:.0f} s)\n{log[-1500:]}")
                failed += 1
                continue
            want = planned(db, year)
            n, bad = check(year, got, want, False) if got else (0, ["no history log at the load: no fill ran"])
            n2, bad2 = check(year, turn, want, True)
            bad += ["at the first turn: " + b for b in bad2]
            fill = next((x[24:] for x in log.splitlines() if x.startswith("recomp: season history: ") and " rows written" in x), "no fill line")
            print(f"{'ok  ' if not bad else 'FAIL'}  {label}: {n} checks at the load, {n2} at the first turn; {fill} ({secs:.0f} s)")
            for b in bad:
                print("      " + b)
            failed += bool(bad)
    print(f"{len(runs)} runs, {failed} failed; work folder {work}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
