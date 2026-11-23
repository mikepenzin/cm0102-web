#!/usr/bin/env python3
"""The gap rows of tools/seasons/history_fill.py (0.6.2 open items 97, 98, 170 and 197).

Usage:
    python3 -I tests/seasons/history_fill_gap_check.py [--databases <folder of database folders>]

Runs gap_rows on the October 2026 data alone and checks the plan rows it makes: the 2026 World Cup, the Africa Cup
of Nations 2025 and the Argentine Apertura 2026 (Belgrano, before the start) are added, and the 2025 Argentine rows
that the database holds get no researched row. Item 170: the Africa Cup of Nations rows that a database of 2010/11 or
later lacks are added when the season the game plays them in (Q277) is before the start: 2021 for October 2021, 2021
and 2023 for October 2023, 2019 for October 2019; April 2024 holds both and gets none. The researched rows of 2021 and
2023 count before a start in 2021 and 2023 (kept). Item 197: each database keeps the FIFA Club World Cup editions to
the one before it was made; the missing edition before the start is added: 2025 (Chelsea / Paris Saint-Germain) for
December 2025 and May 2026, 2017 for October 2018, 2023 for October 2024; October 2026 holds 2025 and gets none. The
2025 row counts before a start in 2025 only (the rules table plays it in 2024/25), so a 2001 game, which plays the
2025 edition itself, gets no row of it. The OFC Nations Cup (0.7.3, release set full2): September 2010 ODB
gets 2008 and October 2016 gets 2016; September 2012 holds 2012 and gets none. The databases are in test-fixtures, outside the repo, so this check is
not in ctest. Exit code 1 on a failure.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "seasons"))
import history_fill as hf  # noqa: E402
import history_gaps as hg  # noqa: E402

DATABASES = "/Users/max/Projects/cm0102/test-fixtures/databases"


def data_folder(path):
    if os.path.isfile(os.path.join(path, "index.dat")):
        return path
    subs = [os.path.join(path, n) for n in sorted(os.listdir(path)) if os.path.isfile(os.path.join(path, n, "index.dat"))]
    return subs[0] if len(subs) == 1 else path


def gap_plan(root, name, start):
    """{(competition, label)} of the rows that gap_rows adds for one database."""
    shifts = {r["database"]: r for r in hg.read_tsv(hg.SHIFTS)}
    plan, report = [], []
    hf.gap_rows(name, start, data_folder(os.path.join(root, name)), hf.research_rows(),
                hg.Data(os.path.join(root, "Original 3.9.68")), hf.aliases(), plan, report,
                int(shifts[name]["comp_history"]), hf.nation_shifts(hg.SHIFTS).get(name, (0, {})))
    return {(c[3], c[4]) for c in (p.split("\t") for p in plan) if c[0] == "add"}


def main(argv):
    root = argv[argv.index("--databases") + 1] if "--databases" in argv else DATABASES
    added = gap_plan(root, "October 2026", 2026)
    bad = []
    for want in (("FIFA World Cup", "2026"), ("African Cup of Nations", "2025"), ("Argentine Premier Division", "2026")):
        if want not in added:
            bad.append(f"no plan row for {want[0]} {want[1]}")
    if ("Argentine Premier Division", "2025") in added:
        bad.append("a researched row for the Argentine Premier Division 2025, which the database holds")
    acn = "African Cup of Nations"
    for name, start, want in (("October 2021", 2021, {"2021"}), ("October 2023", 2023, {"2021", "2023"}),
                              ("October 2019", 2019, {"2019"}), ("April 2024", 2023, set())):
        got = {label for comp, label in gap_plan(root, name, start) if comp == acn}
        if got != want:
            bad.append(f"{name}: {acn} rows added {sorted(got)}, want {sorted(want)}")
    cwc = "FIFA Club World Championship"
    for name, start, want in (("December 2025", 2025, {"2025"}), ("May 2026", 2025, {"2025"}),
                              ("October 2018", 2018, {"2017"}), ("October 2024", 2024, {"2023"}),
                              ("October 2026", 2026, set())):
        got = {label for comp, label in gap_plan(root, name, start) if comp == cwc}
        if got != want:
            bad.append(f"{name}: {cwc} rows added {sorted(got)}, want {sorted(want)}")
    ofc = "Oceania Nations Cup"
    for name, start, want in (("September 2010 ODB", 2010, {"2008"}), ("October 2016", 2016, {"2016"}),
                              ("September 2012", 2012, set())):
        got = {label for comp, label in gap_plan(root, name, start) if comp == ofc}
        if got != want:
            bad.append(f"{name}: {ofc} rows added {sorted(got)}, want {sorted(want)}")
    plan = []
    shifts = {r["database"]: r for r in hg.read_tsv(hg.SHIFTS)}
    hf.gap_rows("December 2025", 2025, data_folder(os.path.join(root, "December 2025")), hf.research_rows(),
                hg.Data(os.path.join(root, "Original 3.9.68")), hf.aliases(), plan, [],
                int(shifts["December 2025"]["comp_history"]), hf.nation_shifts(hg.SHIFTS).get("December 2025", (0, {})))
    final = [p.split("\t")[7:11] for p in plan if p.split("\t")[3:5] == [cwc, "2025"]]
    if final != [["Chelsea", "England", "Paris Saint-Germain", "France"]]:
        bad.append(f"December 2025: {cwc} 2025 is {final}, want Chelsea / Paris Saint-Germain")
    cw = {r["label"]: r for r in hf.research_rows() if r["competition"] == cwc}
    if "2025" not in cw or not hf.kept(cw["2025"], 2025) or hf.kept(cw["2025"], 2024) or hf.kept(cw["2025"], 2001):
        bad.append(f"the researched {cwc} 2025 row does not count from a start in 2025 only")
    rows = {r["label"]: r for r in hf.research_rows() if r["competition"] == acn}
    for label, start in (("2021", 2021), ("2023", 2023), ("2019", 2019)):
        if label not in rows or not hf.kept(rows[label], start):
            bad.append(f"the researched {acn} {label} row does not count before a start in {start}")
    if "2025" in rows and hf.kept(rows["2025"], 2025):
        bad.append(f"the researched {acn} 2025 row counts before a start in 2025")
    print(("FAIL  " + "; ".join(bad)) if bad else f"ok    history fill gap rows: {len(added)} added for October 2026")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
