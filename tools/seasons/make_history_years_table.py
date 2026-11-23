#!/usr/bin/env python3
"""Makes src/recomp_rt/win/season_history_years_table.c from docs/research/history-years/wrong-rows.tsv (0.7.2
open item 233).

Usage:
    python3 -I tools/seasons/make_history_years_table.py            write season_history_years_table.c
    python3 -I tools/seasons/make_history_years_table.py --check    exit 1 when season_history_years_table.c is not
                                                                    the file that wrong-rows.tsv gives

wrong-rows.tsv (tools/seasons/history_years_check.py --write) lists each competition history row of a database
whose year is wrong after the shift of its database, with the right year and its reference. The C table holds,
per database, each such row of its file (club or nation history, the row, its competition id and its year in the
file) and the years to add on top of the shift (right year minus shown year), or 0 for a row to clear (right
year 0 in wrong-rows.tsv: a row that matches no real edition, on a year that a corrected row takes). A database name that
databases-signatures.tsv does not know, a row without a reference, or a difference of 0 (without right year 0) or more than 3 years stops
the tool with exit code 1.
"""
import collections
import csv
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SRC = os.path.join(ROOT, "docs", "research", "history-years", "wrong-rows.tsv")
SIGS = os.path.join(ROOT, "docs", "research", "rules-0.6.0", "databases-signatures.tsv")
OUT = os.path.join(ROOT, "src", "recomp_rt", "win", "season_history_years_table.c")


def read(path):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def fail(text):
    raise SystemExit("make_history_years_table.py: " + text)


def text():
    known = {r["database"] for r in read(SIGS)}
    by = collections.defaultdict(list)
    for r in read(SRC):
        if r["database"] not in known:
            fail(f"{r['database']!r} is not in databases-signatures.tsv")
        if not r["reference"]:
            fail(f"{r['database']} {r['competition']} {r['data_year']}: no reference")
        clear = r["right_year"] == "0"                  # a row of no real edition: cleared (delta 0)
        delta = 0 if clear else int(r["right_year"]) - int(r["shown_year"])
        if not clear and (not delta or abs(delta) > 3):
            fail(f"{r['database']} {r['competition']} {r['data_year']}: difference {delta}")
        by[r["database"]].append((r["kind"] == "nation", int(r["file_row"]), int(r["competition_id"]),
                                  int(r["data_year"]), delta))
    lines = [
        "/* The competition history rows whose year the shift leaves wrong (season_dates.h, open item 233). Made by",
        " * tools/seasons/make_history_years_table.py from docs/research/history-years/wrong-rows.tsv, which gives each",
        " * row with its right year and reference. Do not edit by hand: change the table and run the tool. */",
        '#include "season_dates.h"',
        "",
    ]
    names = []
    for i, db in enumerate(sorted(by)):
        rows = sorted(by[db])
        names.append((db, f"fix_{i}", len(rows)))
        lines.append(f"static const SeasonYearFix fix_{i}[] = {{    /* {db} */")
        for nation, row, comp, year, delta in rows:
            lines.append(f"    {{{int(nation)}, {delta}, {year}, {comp}, {row}}},")
        lines.append("};")
    lines.append("")
    lines.append("const SeasonYearFixes season_year_fixes[] = {")
    for db, var, n in names:
        lines.append(f'    {{"{db}", {var}, {n}u}},')
    if not names:
        lines.append("    {0, 0, 0},")
    lines.append("};")
    lines.append(f"const int season_year_fixes_count = {len(names)};")
    return "\n".join(lines) + "\n"


def main(argv):
    t = text()
    if "--check" in argv:
        have = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
        if have != t:
            print(f"{OUT} is not the table that {SRC} gives: run tools/seasons/make_history_years_table.py")
            return 1
        print("season_history_years_table.c matches wrong-rows.tsv")
        return 0
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(t)
    print(f"wrote {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
