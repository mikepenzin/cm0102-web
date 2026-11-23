#!/usr/bin/env python3
"""International competitions check (0.6.0 slice 27): the editions, hosts and golden goal of the finals in a
season jump.

Usage:
    python3 tests/seasons/season_intl_run.py <cm0102_recomp> <game folder> [--only <name>,...] [--jobs n]
        [--keep <folder>] [--reuse]

Each run is a season jump of tests/seasons/rules_report_check.py (England, the quick result engine) with the
test log RECOMP_SEASON_INTL_LOG=1. The runs:
    2003    October 2003, 2003/04 to 2004/05, changing rules on (Euro 2004, no golden goal from 2004/05)
    2005    September 2005, 2005/06 to 2006/07, changing rules on (World Cup 2006, Australia to Asia)
    2008    September 2008, 2008/09 to 2009/10, changing rules on (the 2010 inter-confederation play-offs)
    2010    September 2010, 2010/11 to 2011/12, changing rules on (Netherlands Antilles to Curacao in 2011)
    2014    October 2014, 2014/15 to 2015/16, changing rules on (Euro 2016 qualifying in full: a draw of 24
            with no fill)
    2015    October 2015, 2015/16 to 2016/17, changing rules on (Euro 2016 with 24 teams)
    2017    October 2017, 2017/18 to 2018/19, changing rules on (the last Confederations Cup lies before)
    2018    October 2018, 2018/19 to 2019/20, changing rules on (Asian Cup and Africa Cup of Nations of 24 teams)
    2019    October 2019, 2019/20 to 2020/21, changing rules on (Euro 2020 and Copa America 2020 at their
            planned time, the Tokyo Olympics in 2020, Q277, Q366)
    2021    October 2021, 2021/22 to 2022/23, changing rules on (the 2022 World Cup in November, Türkiye)
    2023    October 2023, 2023/24 to 2024/25, changing rules on (Copa America 2024 with 16 teams)
    off     October 2017 with changing rules off: the first editions of the table, then the original's steps

The checks (each run):
    - the jump ends with code 0 and the report holds the end of the last season
    - every year that the game holds for a finals class (world_cup, euro_champ, fifa_confed, olympics,
      goldcup, african_nations, asia_nations, oceania_nations) at the first report is the first edition of
      the table from the start season, after the table on the class's last cycle (the never year 2999 for the
      ended Confederations Cup)
    - on: when the year of a class changes between two reports, the new year is the next edition of the
      table (tools/seasons/make_intl.py); off: the old year plus the original's step
    - no draw after a full qualifying is short (log "short after a full qualifying"; a tie that ends after the
      World Cup draw is not a fault), and the draws of NO_FILL take no team from the fill
    - no inter-confederation play-off is built without a team (log "wc play-off ... v none")
    - a start after the qualifying began takes the real draw and the real play-offs (LINES; O259 A9, A16), and
      every finalist of the real draw is in the data
    - the nation renames of RENAMES and the date moves of MOVES are in the log
    - on: each host row that the runtime writes names the hosts of the table, up to six (log "host")
A run takes about 6 to 8 minutes. --jobs (default 2) runs that many at the same time. --reuse checks the
reports already in the --keep folder again without a new run.
"""
import concurrent.futures
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
sys.path.insert(0, os.path.join(ROOT, "tools", "seasons"))
import make_intl  # noqa: E402
import rules_report_check as rrc  # noqa: E402

DB = "/Users/max/Projects/cm0102/test-fixtures/databases"
RUNS = {
    "2003": (2003, "October 2003", "on"),
    "2005": (2005, "September 2005", "on"),
    "2008": (2008, "September 2008", "on"),
    "2010": (2010, "September 2010 ODB", "on"),
    "2014": (2014, "October 2014", "on"),
    "2015": (2015, "October 2015", "on"),
    "2017": (2017, "October 2017", "on"),
    "2018": (2018, "October 2018", "on"),
    "2019": (2019, "October 2019", "on"),
    "2021": (2021, "October 2021", "on"),
    "2023": (2023, "October 2023", "on"),
    "off": (2017, "October 2017", "off"),
}
# The nation renames each run must log (season_intl.c, int-world rows): the old name and the new.
RENAMES = {
    "2005": [("Yugoslavia", "Serbia and Montenegro"), ("Serbia and Montenegro", "Serbia")],
    "2010": [("Netherlands Antilles", "Cura\xe7ao")],
    "2017": [("Czech Republic", "Czechia"), ("Swaziland", "Eswatini"), ("FYR of Macedonia", "North Macedonia")],
    "2019": [("Czech Republic", "Czechia")],
    "2021": [("Czech Republic", "Czechia"), ("Turkey", "T\xfcrkiye")],
    "off": [("Czech Republic", "Czechia"), ("Swaziland", "Eswatini")],
}
# Renames that must not happen: the database holds Serbia, so the record Yugoslavia stays.
UNRENAMED = {r: [("Yugoslavia", "Serbia and Montenegro")] for r in ("2017", "2019", "2021", "off")}
# The databases from 2019 hold a live "eSwatini" beside a dead "Swaziland" (O188): no rename.
for r in ("2019", "2021"):
    UNRENAMED[r].append(("Swaziland", "Eswatini"))
# A renamed nation plays on (the 2011 rename of the Netherlands Antilles, O306): the record is renamed once, the old
# name is in no later log line, and the run goes on to the end of the next season with no fault about the nation.
PLAYS_ON = {"2010": ("Netherlands Antilles", "Cura\xe7ao")}
# The finals that each run must move to the table's first day (log "moves by").
MOVES = {"2021": [("world_cup", 2022, 161)]}
# Log lines each run must hold (season_intl_euro.c: the Euro of 24 teams).
LINES = {"2015": ["euro_champ 2016 draw of 24 teams in 6 groups", "euro_champ 2016 round of 16"],
         "2018": ["asia_nations 2019 round of 16", "african_nations 2019 round of 16"],
         "2023": ["goldcup 2024: 16 teams", "goldcup 2024 quarter-finals of 16 teams"],
         "2008": ["wc play-off 2010:"],
         "2014": ["euro_champ 2016 draw of 24 teams in 6 groups"],
         # A start after the qualifying began takes the real draw and the real play-offs (O259 A9, A16).
         "2003": ["euro_champ 2004: the real draw, 16 of 16 teams"],
         # (These starts build no Oceania tie, as the main build: its teams come in by the real draw.)
         "2005": ["world_cup 2006: the real draw, 32 of 32 teams", "wc play-off 2006: Bahrain v Trinidad"],
         "2017": ["world_cup 2018: the real draw, 32 of 32 teams", "wc play-off 2018: Australia v Honduras"],
         "2019": ["euro_champ 2020: the real draw, 24 of 24 teams"],
         "2021": ["world_cup 2022: the real draw, 32 of 32 teams", "wc play-off 2022: Australia v Peru"],
         "2023": ["euro_champ 2024: the real draw, 24 of 24 teams"]}
# Finals whose draw must take no team from the fill: the qualifying ran in full in the game.
NO_FILL = {"2008": ["world_cup 2010"], "2014": ["euro_champ 2016"]}
STEPS = {"world_cup": 4, "euro_champ": 4, "fifa_confed": 2, "olympics": 4, "goldcup": 2, "african_nations": 2,
         "asia_nations": 4, "oceania_nations": 2}
# After its table each class continues on its last cycle (0.6.2 decision A, season_intl_rules.c class_cycles); the
# ended Confederations Cup waits for the never year 2999.
CYCLES = {"world_cup": 4, "euro_champ": 4, "fifa_confed": 0, "olympics": 4, "goldcup": 4, "african_nations": 2,
          "asia_nations": 4, "oceania_nations": 4}
NEVER = 2999
BASE_ENV = {"RECOMP_STAFF_RESERVE": "23000", "RECOMP_SEASON_INTL_LOG": "1"}
HOST_LOG = re.compile(r"recomp: season intl: host (\w+) (\d+): (.*)$")


def database_folder(name):
    """The unpacked folder of a season database by its neutral name: some folder names add more words after it,
    which no committed file names."""
    hits = sorted(d for d in os.listdir(DB) if d == name or d.startswith(name + " "))
    if not hits:
        raise SystemExit(f"season_intl_run.py: no database folder {name!r} in {DB}")
    return os.path.join(DB, hits[0])


def table():
    out = {}
    for e in make_intl.editions():
        out.setdefault(e["file"], []).append(e)
    return out


def first(eds, start):
    hit = next((e["year"] for e in eds if e["season"] >= start), None)
    if hit is not None:
        return hit
    last, cycle = eds[-1], CYCLES[eds[-1]["file"]]
    if not cycle:
        return NEVER
    year = last["year"] + cycle
    while year - (last["year"] - last["season"]) < start:
        year += cycle
    return year


def nxt(eds, year):
    hit = next((e["year"] for e in eds if e["year"] > year), None)
    if hit is not None:
        return hit
    last, cycle = eds[-1], CYCLES[eds[-1]["file"]]
    if not cycle:
        return max(year, NEVER)
    return last["year"] + ((year - last["year"]) // cycle + 1) * cycle


def check(name, start, rules, work):
    report = os.path.join(work, "report.tsv")
    if not os.path.exists(report):
        return [f"{name}: no report ({work})"]
    seasons, comps = rrc.read_report(report)
    fails = []
    if not seasons or seasons[-1]["season"] != start + 1 or seasons[-1]["phase"] != "end":
        fails.append(f"{name}: the report does not end with the end of {start + 1}/{(start + 2) % 100:02d}")
    eds = table()
    years = {}                                  # file -> the years of the reports, in order
    for c in comps:
        if c["file"] in STEPS:
            years.setdefault(c["file"], [])
            if not years[c["file"]] or years[c["file"]][-1] != c["year"]:
                years[c["file"]].append(c["year"])
    for f in STEPS:
        seen = years.get(f)
        if not seen:
            fails.append(f"{name}: {f} is not in the report")
            continue
        want = first(eds[f], start)
        if seen[0] != want:
            fails.append(f"{name}: {f} holds {seen[0]} at the start, the table's first edition is {want}")
        for a, b in zip(seen, seen[1:]):
            want = nxt(eds[f], a) if rules == "on" else a + STEPS[f]
            if b != want:
                fails.append(f"{name}: {f} goes from {a} to {b}, expected {want}")
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read().split("\n")
    for old, new in RENAMES.get(name, []):
        if not any(f"season intl: nation {old} is now {new} " in x for x in log):
            fails.append(f"{name}: no rename of {old} to {new} in the log")
    for old, new in UNRENAMED.get(name, []):
        if any(f"season intl: nation {old} is now {new} " in x for x in log):
            fails.append(f"{name}: {old} must keep its name (the database has a later name of the chain)")
    for old, new in ([PLAYS_ON[name]] if name in PLAYS_ON else []):
        at = [i for i, x in enumerate(log) if f"season intl: nation {old} is now {new} " in x]
        if len(at) != 1:
            fails.append(f"{name}: {len(at)} renames of {old} to {new} in the log, expected 1")
        for x in log[(at[0] + 1 if at else len(log)):]:
            if old in x or ("not in the data" in x and new in x):
                fails.append(f"{name}: {new} does not play on after the rename: {x.strip()}")
    for text in LINES.get(name, []):
        if not any(text in x for x in log):
            fails.append(f"{name}: no log line {text!r}")
    for x in log:
        if "season intl: wc play-off" in x and (" v none" in x or x.rstrip().endswith("none v")):
            fails.append(f"{name}: a play-off without a team: {x.strip()}")
    for x in log:
        if "is not in the data" in x and "the finalist" in x:
            fails.append(f"{name}: a finalist of the real draw is missing: {x.strip()}")
    for x in log:
        if "short after a full qualifying" in x:
            fails.append(f"{name}: a short list after a full qualifying: {x.strip()}")
    for comp in NO_FILL.get(name, []):
        if any(f"season intl: {comp}:" in x and " filled" in x and not x.rstrip().endswith(" 0 filled") for x in log):
            fails.append(f"{name}: the {comp} draw took teams from the fill")
    for f, y, days in MOVES.get(name, []):
        if not any(f"season intl: {f} {y} moves by {days} days" in x for x in log):
            fails.append(f"{name}: {f} {y} does not move by {days} days")
    if rules == "on":
        hosts = [m.groups() for m in (HOST_LOG.search(x) for x in log) if m]
        if not hosts:
            fails.append(f"{name}: no host row in the log")
        for f, y, names in hosts:
            e = next((e for e in eds.get(f, []) if e["year"] == int(y)), None)
            # A host row holds up to six hosts (HostCountry pairs 1 to 3; slice 29): the first six of the table.
            # A name of the table may have another spelling in a database ("China PR/China").
            want = [h.split("/") for h in e["hosts"][:6]] if e else None
            got = names.split(";") if names != "-" else []
            if want is not None and (len(got) != len(want) or any(g not in w for g, w in zip(got, want))):
                fails.append(f"{name}: host row {f} {y}: {names}, the table gives {want}")
    return fails


def one(program, game, name, keep, reuse):
    start, database, rules = RUNS[name]
    work = os.path.join(keep, name) if keep else tempfile.mkdtemp(prefix=f"cm0102-season-intl-{name}-")
    if not reuse:
        code, _ = rrc.run(program, game, start + 1, start, database_folder(database), ["England"], [], work, 7200,
                          {"changing_rules": rules}, BASE_ENV)
        if code != 0:
            return [f"{name}: the run ended with code {code} ({work})"]
    return check(name, start, rules, work)


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    only = argv[argv.index("--only") + 1].split(",") if "--only" in argv else list(RUNS)
    jobs = int(argv[argv.index("--jobs") + 1]) if "--jobs" in argv else 2
    keep = os.path.abspath(argv[argv.index("--keep") + 1]) if "--keep" in argv else None
    reuse = "--reuse" in argv
    fails = []
    with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
        for r in pool.map(lambda n: one(program, game, n, keep, reuse), only):
            fails += r
    for f in fails:
        print("FAIL  " + f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
