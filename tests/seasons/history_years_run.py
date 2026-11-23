#!/usr/bin/env python3
"""Competition history rows in their real years in the season databases (0.7.2 open item 233, forum report of
13 November 2026: "the FIFA World Cup on the 2018/19 and 2020/21 databases shows France winning the 2018 World Cup
in 2017").

Usage:
    python3 tests/seasons/history_years_run.py <cm0102_recomp> <game folder> <folder of databases> [--jobs n]
        [--keep <folder>]

The World Cup of the report is right since 0.7.0 (0.6.2 slice 7, the nation rows' own shift, 7ee42cd8): a 0.6.0 or
0.6.1 game showed it a year early, and a save keeps its rows. Other rows are wrong in the data itself: the Update
Team databases hold them one year off against the other rows of their competition, so the shift of the database
leaves them wrong (tools/seasons/history_years_check.py lists every such row with its reference,
docs/research/history-years/wrong-rows.tsv). The game corrects them at the database load (season_dates.c, the
year fix table).

For each run, a new game in the season of the database, England, seed 1, to the first news screen. The test log
RECOMP_HISTORY_LOG (season_history.c) gives every row from 1999 on of the competitions below at the creation of the
game, before the game keeps the rows of the selected nations. The checks: each expected row (an independent list
of real results, sources below) is at its year, and no row of the same first club and second club is at another
year next to it; the log line of the year fix names the database and finds every row of its table.

A row with no year (Belgian Second Division KSK Beveren / Beerschot, which no season had) must be gone: the load
clears it. Sources of the expected rows: Belgian Second Division, RSSSF https://www.rsssf.org/tablesb/belg2champ.html; Norwegian champions, RSSSF https://www.rsssf.org/tablesn/noorkamp.html; Swedish Cup
finals, RSSSF https://www.rsssf.org/tabless/zwedcuphist.html; League of Ireland champions and runners-up,
https://en.wikipedia.org/wiki/List_of_League_of_Ireland_champions (2000/01 Bohemians, 2001/02 Shelbourne); World
Cup finals, https://www.rsssf.org/tablesw/worldcup.html.
At most --jobs (2) games run at the same time.
"""
import concurrent.futures
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import season_dates_run as sdr  # noqa: E402

NOR, SWE, IRL, WC, BEL = ("Norwegian Premier Division", "Swedish Cup", "Irish Premier Division", "FIFA World Cup",
                          "Belgian Second Division")
# (competition, year, first, second): names as the database writes them (first word enough, see match()).
RUNS = {
    "October 2018": (2018, [
        (WC, 2014, "Germany", "Argentina"), (WC, 2018, "France", "Croatia"),
        (NOR, 2004, "Rosenborg", "lerenga"), (NOR, 2005, "lerenga", "IK Start"),
        (NOR, 2006, "Rosenborg", "SK Brann"), (NOR, 2007, "SK Brann", "Stab"),
        (NOR, 2009, "Rosenborg", "Molde"),
        (SWE, 2002, "Djurgardens", "AIK"), (SWE, 2003, "Elfsborg", "Assyriska"),
        (SWE, 2007, "Kalmar", "teborg"), (SWE, 2009, "AIK", "teborg"),
    ]),
    "December 2025": (2025, [
        (WC, 2022, "Argentina", "France"),
        (NOR, 2005, "lerenga", "IK Start"), (NOR, 2007, "SK Brann", "Stab"),
        (IRL, 2001, "Bohemian", "Shelbourne"), (IRL, 2002, "Shelbourne", "Shamrock"),
    ]),
    "October 2014": (2014, [
        (BEL, 2009, "Sint", "Lierse"), (BEL, 2010, "Lierse", "Lommel"), (BEL, 2012, "Charleroi", "Waasland"),
        (BEL, 2013, "Oostende", "Mouscron"), (BEL, None, "KSK Beveren", "Beerschot"),
    ]),
    "October 2026": (2026, [
        (WC, 2022, "Argentina", "France"),
        (NOR, 2005, "lerenga", "IK Start"), (SWE, 2002, "Djurg", "AIK"),
        (IRL, 2001, "Bohemian", "Shelbourne"),
    ]),
}


def match(want, got):
    return got.lower().startswith(want.lower()) or want.lower() in got.lower()


def parse(log):
    """{competition: [(year, first, second)]} of the create log."""
    out = {}
    for m in re.finditer(r"recomp: history log create: ([^:]+):(.*)", log):
        rows = []
        for part in m.group(2).split(";"):
            p = re.match(r"\s*(\d+) (.*?) / (.*)", part)
            if p:
                rows.append((int(p.group(1)), p.group(2).strip(), p.group(3).strip()))
        out[m.group(1)] = rows
    return out


def check(db, log, want):
    fails = []
    got = parse(log)
    for comp, year, a, b in want:
        rows = got.get(comp, [])
        if year is None:            # a row of no real edition: cleared at the load
            if any(match(a, r[1]) and match(b, r[2]) for r in rows):
                fails.append(f"{comp} {a} / {b}: still there")
            continue
        here = [r for r in rows if r[0] == year and match(a, r[1]) and match(b, r[2])]
        off = [r for r in rows if r[0] != year and abs(r[0] - year) == 1 and match(a, r[1]) and match(b, r[2])]
        if not here or off:
            fails.append(f"{comp} {year} {a} / {b}: " + (f"at {', '.join(str(r[0]) for r in off)}" if off else "missing"))
    m = re.search(r"season dates: (\d+) competition history years corrected for (.+?) \(open item 233\), (\d+) rows", log)
    if not m or m.group(2) != db or m.group(3) != "0":
        fails.append("the year fix line " + (f"says {m.group(0)}" if m else "is missing"))
    return fails


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2
    program, game, dbs_dir = (os.path.abspath(a) for a in argv[1:4])
    jobs = int(argv[argv.index("--jobs") + 1]) if "--jobs" in argv else 2
    work = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="history-years-")
    os.makedirs(work, exist_ok=True)
    dbs = sdr.databases(dbs_dir)
    for sub in sorted(os.listdir(dbs_dir)):        # a folder that holds the database folder (October 2026 Data Update)
        p = os.path.join(dbs_dir, sub)
        if os.path.isdir(p) and not os.path.exists(os.path.join(p, "index.dat")):
            for name, v in sdr.databases(p).items():
                dbs.setdefault(name, v)
    env = {"RECOMP_HISTORY_LOG": ";".join((NOR, SWE, IRL, WC, BEL))}
    failed = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        futs = {}
        for db, (year, want) in RUNS.items():
            if db not in dbs:
                print(f"FAIL  {db}: not in {dbs_dir}")
                failed += 1
                continue
            folder = sdr.neutral_copy(work, db, dbs[db][1])
            futs[ex.submit(sdr.play, program, game, work, db.replace(" ", "_"), year, folder, env)] = (db, want)
        for f in concurrent.futures.as_completed(futs):
            db, want = futs[f]
            save, log, secs = f.result()
            with open(os.path.join(work, db.replace(" ", "_") + ".log"), "w", encoding="latin-1") as out:
                out.write(log)
            fails = check(db, log, want) + ([] if save else ["no save"])
            print(f"{'ok  ' if not fails else 'FAIL'}  {db}: {len(want)} rows in their years"
                  + "".join(f"\n      {x}" for x in fails) + f" ({secs:.0f} s)")
            failed += bool(fails)
    print(f"logs: {work}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
