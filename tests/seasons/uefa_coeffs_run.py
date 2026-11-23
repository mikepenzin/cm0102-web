#!/usr/bin/env python3
"""The real UEFA coefficients of every season database and extra database (0.6.2 open item 106).

Usage:
    python3 tests/seasons/uefa_coeffs_run.py <cm0102_recomp> <game folder> [--only <name>,...] [--keep <folder>]

At the database load the game writes the real six seasons (S-6)/(S-5) to (S-1)/S at nation +0xe0..+0x108 of the
nations of the table (src/recomp_rt/win/season_coeffs.c, docs/research/2026-11-03-database-coefficients.tsv).

Each run starts a game on one database (season database or extra database) in its own start year S with England
(rules_report_check.py), saves one day after the start and ends. The checks: code 0 and no message box; the log line
of the fill (every row written, none kept; on the zips of our own databases of 4 November 2026 every row held
already, on their earlier zips every row written); in the save, the six values of every nation of the table are the
real ones; the first ten UEFA nations by the totals of the first season (+0xe0..+0x100) are in the order of UEFA's
real ranking of year S - 1, and by the totals of the first season change (+0xe8..+0x108, roll_coefficients 0x903b50)
in the order of the real ranking of year S (RANKS, kassiesa.net country rankings, read 3 November 2026). From 2024
on the two European Performance Spots of the first season go to the two best values at +0x108 (2024: Italy and
Germany; 2025: England and Spain, as in the real seasons). The 2025 runs also check that Spain has five Champions
League clubs in 2025/26: four by its rank and one European Performance Spot. A run takes 1 to 3 minutes.
"""
import csv
import os
import re
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
import rules_report_check as rrc  # noqa: E402
import uefa_formats_run as ufr  # noqa: E402
import history_check as hc  # noqa: E402

DB = "/Users/max/Projects/cm0102/test-fixtures/databases"
OWN = "/Users/max/Projects/cm0102/own-dbs"             # our own databases, as zips
TSV = os.path.join(HERE, "..", "..", "docs", "research", "2026-11-03-database-coefficients.tsv")
# run: (database, its folder in DB or zip in OWN, start year, more environment). reshift: the test switch of
# season_dates.c makes the shift for 2030 and then for 2025 again, so the fill is undone and made again (three
# fills in the log, the values as after one).
RUNS = {"s2002": ("Summer 2002", "Summer 2002.zip", 2002, {}),
        "sep2005": ("September 2005", "September 2005 Helvete", 2005, {}),
        "s2006": ("Summer 2006", "Summer 2006.zip", 2006, {}),
        "sep2008": ("September 2008", "September 2008", 2008, {}),
        "sep2011": ("September 2011 ODB", "September 2011 ODB", 2011, {}),
        "oct2019": ("October 2019", "October 2019", 2019, {}),
        "nov2020": ("November 2020", "November 2020", 2020, {}),
        "oct2021": ("October 2021", "October 2021", 2021, {}),
        "oct2022": ("October 2022", "October 2022", 2022, {}),
        "oct2023": ("October 2023", "October 2023", 2023, {}),
        "oct2024": ("October 2024", "October 2024", 2024, {}),
        "dec2025": ("December 2025", "December 2025", 2025, {}),
        "may2026": ("May 2026", "May 2026", 2025, {}),
        "oct2026": ("October 2026", "October 2026/October 2026 Data Update", 2026, {}),
        "dec2008": ("December 2008", "December 2008", 2008, {}),
        "mar2009": ("March 2009", "March 2009", 2008, {}),
        "jun2010": ("June 2010", "June 2010", 2009, {}),
        "mar2012": ("March 2012 ODB", "March 2012 ODB", 2011, {}),
        "mar2020": ("March 2020", "March 2020", 2019, {}),
        "apr2021": ("April 2021", "April 2021", 2020, {}),
        "apr2022": ("April 2022", "April 2022", 2021, {}),
        "apr2023": ("April 2023", "April 2023", 2022, {}),
        "apr2024": ("April 2024", "April 2024", 2023, {}),
        "oct2003": ("October 2003", "October 2003 Sawczuk", 2003, {}),
        "s2004": ("Summer 2004", "Summer 2004.zip", 2004, {}),
        "nov2007": ("November 2007", "November 2007", 2007, {}),
        "feb2008": ("February 2008", "February 2008", 2007, {}),
        "sep2009": ("September 2009", "September 2009", 2009, {}),
        "apr2010": ("April 2010 SIM", "April 2010 SIM", 2009, {}),
        "feb2010": ("February 2010 ODB", "February 2010 ODB", 2009, {}),
        "sep2010": ("September 2010 ODB", "September 2010 ODB", 2010, {}),
        "dec2010": ("December 2010 SIM", "December 2010 SIM", 2010, {}),
        "feb2011": ("February 2011 ODB", "February 2011 ODB", 2010, {}),
        "apr2011": ("April 2011 SIM", "April 2011 SIM", 2010, {}),
        "mar2012s": ("March 2012 SIM", "March 2012 SIM", 2011, {}),
        "sep2012": ("September 2012", "September 2012", 2012, {}),
        "mar2013": ("March 2013", "March 2013", 2012, {}),
        "aut2013": ("Autumn 2013", "Autumn 2013", 2013, {}),
        "mar2014": ("March 2014", "March 2014", 2013, {}),
        "oct2014": ("October 2014", "October 2014", 2014, {}),
        "apr2015": ("April 2015", "April 2015", 2014, {}),
        "oct2015": ("October 2015", "October 2015", 2015, {}),
        "apr2016": ("April 2016", "April 2016", 2015, {}),
        "oct2016": ("October 2016", "October 2016", 2016, {}),
        "apr2017": ("April 2017", "April 2017", 2016, {}),
        "oct2017": ("October 2017", "October 2017", 2017, {}),
        "mar2018": ("March 2018", "March 2018", 2017, {}),
        "oct2018": ("October 2018", "October 2018", 2018, {}),
        "mar2019": ("March 2019", "March 2019", 2018, {}),
        # the zips of our own databases before 4 November 2026: the load writes every row
        "s2002old": ("Summer 2002", "old/Summer 2002 (2026-11-04, before the real coefficients).zip", 2002, {}),
        "s2004old": ("Summer 2004", "old/Summer 2004 (2026-11-04, before the real coefficients).zip", 2004, {}),
        "s2006old": ("Summer 2006", "old/Summer 2006 (2026-11-04, before the real coefficients).zip", 2006, {}),
        "reshift": ("December 2025", "December 2025", 2025, {"RECOMP_DATES_RESHIFT": "2030"})}
# UEFA's real association ranking of each year, first ten (kassiesa.net/uefa/data/method<n>/crank<year>.html)
RANKS = {
    2001: ["Spain", "Italy", "England", "Germany", "France", "Holland", "Turkey", "Greece", "Russia", "Portugal"],
    2002: ["Spain", "Italy", "England", "Germany", "France", "Greece", "Holland", "Turkey", "Portugal", "Russia"],
    2003: ["Spain", "Italy", "England", "Germany", "France", "Greece", "Portugal", "Holland", "Scotland", "Turkey"],
    2004: ["Spain", "England", "Italy", "Germany", "France", "Portugal", "Greece", "Holland", "Czech Republic",
           "Turkey"],
    2005: ["Spain", "England", "Italy", "France", "Germany", "Portugal", "Holland", "Greece", "Belgium", "Scotland"],
    2006: ["Spain", "Italy", "England", "France", "Germany", "Portugal", "Holland", "Greece", "Russia", "Romania"],
    2007: ["Spain", "England", "Italy", "France", "Germany", "Portugal", "Romania", "Holland", "Russia", "Scotland"],
    2008: ["England", "Spain", "Italy", "France", "Germany", "Russia", "Romania", "Portugal", "Holland", "Scotland"],
    2009: ["England", "Spain", "Italy", "Germany", "France", "Russia", "Ukraine", "Holland", "Romania", "Portugal"],
    2010: ["England", "Spain", "Italy", "Germany", "France", "Russia", "Ukraine", "Romania", "Portugal", "Holland"],
    2011: ["England", "Spain", "Germany", "Italy", "France", "Portugal", "Russia", "Ukraine", "Holland", "Turkey"],
    2012: ["England", "Spain", "Germany", "Italy", "Portugal", "France", "Russia", "Holland", "Ukraine", "Greece"],
    2013: ["Spain", "England", "Germany", "Italy", "Portugal", "France", "Ukraine", "Russia", "Holland", "Turkey"],
    2014: ["Spain", "England", "Germany", "Italy", "Portugal", "France", "Russia", "Holland", "Ukraine", "Belgium"],
    2015: ["Spain", "England", "Germany", "Italy", "Portugal", "France", "Russia", "Ukraine", "Holland", "Belgium"],
    2016: ["Spain", "Germany", "England", "Italy", "Portugal", "France", "Russia", "Ukraine", "Belgium", "Holland"],
    2017: ["Spain", "Germany", "England", "Italy", "France", "Russia", "Portugal", "Ukraine", "Belgium", "Turkey"],
    2018: ["Spain", "England", "Italy", "Germany", "France", "Russia", "Portugal", "Ukraine", "Belgium", "Turkey"],
    2019: ["Spain", "England", "Italy", "Germany", "France", "Russia", "Portugal", "Belgium", "Ukraine", "Turkey"],
    2020: ["Spain", "England", "Germany", "Italy", "France", "Portugal", "Russia", "Belgium", "Ukraine", "Holland"],
    2021: ["England", "Spain", "Italy", "Germany", "France", "Portugal", "Holland", "Russia", "Belgium", "Austria"],
    2022: ["England", "Spain", "Italy", "Germany", "France", "Portugal", "Holland", "Austria", "Scotland", "Russia"],
    2023: ["England", "Spain", "Germany", "Italy", "France", "Holland", "Portugal", "Belgium", "Scotland", "Austria"],
    2024: ["England", "Italy", "Spain", "Germany", "France", "Holland", "Portugal", "Belgium", "Turkey",
           "Czech Republic"],
    2025: ["England", "Italy", "Spain", "Germany", "France", "Holland", "Portugal", "Belgium", "Czech Republic",
           "Turkey"],
    2026: ["England", "Italy", "Spain", "Germany", "France", "Portugal", "Holland", "Belgium", "Turkey",
           "Czech Republic"],
}


# the names that the season code gives in later start years (season_intl_rules.c): the names of RANKS
LATER_NAMES = {"Türkiye": "Turkey", "Czechia": "Czech Republic", "Netherlands": "Holland",
               "North Macedonia": "FYR of Macedonia", "Ireland": "Republic of Ireland"}


def ranks(nations, first):
    """[(name)] of the UEFA nations of a save by the total of five doubles from nation +first, best first. Equal
    totals go by the last of the five seasons, as UEFA ranks them (Belgium and Scotland in 2005, the Czech Republic
    and Scotland in 2024)."""
    rows = sorted(((-sum(v), -v[4], name) for name, v in
                   ((name, struct.unpack_from("<5d", r, first)) for name, r in nations.items())))
    return [name for _, _, name in rows]


def database_folder(where, work):
    if not where.endswith(".zip"):
        return os.path.join(DB, where)
    import zipfile
    out = os.path.join(work, "db")
    with zipfile.ZipFile(os.path.join(OWN, where)) as z:
        z.extractall(out)
    inner = [os.path.join(out, d) for d in os.listdir(out) if os.path.isdir(os.path.join(out, d))]
    return inner[0] if len(inner) == 1 else out


def check(name, database, where, start, env, work):
    import smoke_run
    fails = []
    os.makedirs(work, exist_ok=True)
    code, _ = rrc.run(PROGRAM, GAME, start, start, database_folder(where, work), ["England"], [], work, 3600, None,
                      dict({"RECOMP_SAVE_AFTER_DAYS": "1"}, **env))
    if code != 0:
        fails.append(f"{name}: the game ended with code {code}")
    found, log = ufr.boxes(work)
    fails += [f"{name}: message box {b}" for b in found]
    with open(TSV, encoding="utf-8") as f:
        want = [r for r in csv.DictReader(f, delimiter="\t") if r["database"] == database]
    if not want:
        fails.append(f"{name}: no row of {database} in {TSV}")
    # our own databases hold the real values from 4 November 2026 (tools/seasons/own_db/coeffs.py): the rows fix a
    # copy of an earlier zip, and on these zips the fill finds every value real already
    line = (f"season coeffs: {database}: the real six seasons written for 0 nations, 0 kept, some held them already"
            if where.endswith(".zip") and not where.startswith("old/") else
            f"season coeffs: {database}: the real six seasons written for {len(want)} nations, 0 kept")
    fills = 3 if "RECOMP_DATES_RESHIFT" in env else 1
    if log.count(line) != fills or log.count("season coeffs:") != fills:
        fails.append(f"{name}: not {fills} log lines {line!r} and no other")
    save = os.path.join(work, "user", smoke_run.SAVE_NAME)
    if not os.path.exists(save):
        return fails + [f"{name}: no save ({save})"]
    g = hc.Game(save)
    nat = g.sav.read("nation.dat")
    recs = {hc.s32(nat, i * hc.NATION): nat[i * hc.NATION:(i + 1) * hc.NATION] for i in range(len(nat) // hc.NATION)}
    for r in want:
        rec = recs.get(int(r["nation_id"]))
        v = list(struct.unpack_from("<6d", rec, 0xe0)) if rec else None
        if v != [float(x) for x in r["real"].split()]:
            fails.append(f"{name}: {r['nation']} has {v} at +0xe0..+0x108, not {r['real']}")
    uefa = {LATER_NAMES.get(hc.text(rec, 4), hc.text(rec, 4)): rec for rec in recs.values()
            if g.continents.get(hc.s32(rec, 0x71)) == "UEFA"}
    if "Serbia" in uefa:
        uefa.pop("Yugoslavia", None)        # the record of a former nation
    for first, year in ((0xe0, start - 1), (0xe8, start)):
        got, real = ranks(uefa, first)[:10], RANKS[year][:10]
        if got != real:
            fails.append(f"{name}: the first ten by the totals of +0x{first:x}: {got}, not UEFA's {year}: {real}")
    if start >= 2024:
        eps = re.findall(rf"European Performance Spot {start}/{(start + 1) % 100:02d}: rank (\d+) \(([0-9.]+) at "
                         r"\+0x([0-9a-f]+)\)", log)
        best = sorted((struct.unpack_from("<d", r, 0x108)[0] for r in uefa.values()), reverse=True)[:2]
        if sorted((float(v), at) for _, v, at in eps) != sorted((v, "108") for v in best):
            fails.append(f"{name}: the European Performance Spots are {eps}, not the best two at +0x108 {best}")
    if start == 2025:
        spain = [k for k, (n, _) in g.nations.items() if n == "Spain"][0]
        cl = {k for k, c in g.comps.items() if c["name"] in ("European Champions Cup", "UEFA Champions League")}
        clubs = sorted(g.clubs[c][0] for c, (_, _, mark, _) in g.divisions.items()
                       if mark in cl and g.clubs[c][1] == spain)
        if len(clubs) != 5:
            fails.append(f"{name}: Spain has {len(clubs)} Champions League clubs in 2025/26, not 5: {clubs}")
        print(f"{name}: Spain's Champions League clubs: {', '.join(clubs)}")
    return fails


def main(argv):
    global PROGRAM, GAME
    if len(argv) < 2:
        print(__doc__)
        return 2
    PROGRAM, GAME = os.path.abspath(argv[0]), os.path.abspath(argv[1])
    only = rrc.arg(argv, "--only", "").split(",") if "--only" in argv else list(RUNS)
    if any(n not in RUNS for n in only):
        print(f"--only: the runs are {', '.join(RUNS)}")
        return 2
    keep = rrc.arg(argv, "--keep")
    fails = []
    for name in only:
        work = os.path.join(keep, name) if keep else tempfile.mkdtemp(prefix=f"cm0102-coeffs-{name}-")
        f = check(name, *RUNS[name], work)
        print(f"{'PASS' if not f else 'FAIL'}  {name} ({work})")
        fails += f
    for f in fails:
        print(f"FAIL  {f}")
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
