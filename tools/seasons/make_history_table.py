#!/usr/bin/env python3
"""Makes src/recomp_rt/win/season_history_table.c from docs/research/history-0.6.0/plan.tsv (0.6.0 slice 11c).

Usage:
    python3 -I tools/seasons/make_history_table.py           writes the C table
    python3 -I tools/seasons/make_history_table.py --check [--stamp <file>]
        exits 1 when the C table is not the one plan.tsv gives, or a row is not valid; writes the stamp file
        when all is right (the build runs it before cm0102_recomp)

A row is not valid when its winner is empty, its label year is outside 2001 to the start year of its database,
a name is longer than 50 bytes in Latin-1 (the game keeps 51 bytes with the end byte), or it is a duplicate.

plan.tsv comes from tools/seasons/history_fill.py --write. For each database before 2010/11 it holds the
competition and label years whose history rows the game clears (`clear`), and the researched rows that it adds
(`add`), with the club names as that database writes them. The names go into C as Latin-1 bytes, the encoding of
the game data.

plan-fixes.tsv (0.7.3) holds hand-kept rows in the columns of plan.tsv plus `sources`: data faults of a season
database of 2010/11 or later (two rows of one label where real history has one, the newer row one year early: 0.6.2
open items 58 and 59), each cleared and written again from its sources. history_fill.py does not write it, so a
--write run keeps it. A row with an empty source is not valid.

plan-official.tsv (0.6.2 open item 149) holds the rows of the official data sets (3.9.60, 3.9.65, 3.9.68) for every
later start year: start_year is the first start year of a row and last_start its last (empty: every later one).
Each data set becomes one entry that holds from its first start year on (`last` NO_LAST), and each row carries
its range (`from`, `to`). The rows of plan.tsv hold for their start year only (`to` 0: the entry's own year).
"""
import csv
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
PLAN = os.path.join(ROOT, "docs", "research", "history-0.6.0", "plan.tsv")
PLAN_OFFICIAL = os.path.join(ROOT, "docs", "research", "history-0.6.0", "plan-official.tsv")
PLAN_FIXES = os.path.join(ROOT, "docs", "research", "history-0.6.0", "plan-fixes.tsv")
NO_LAST = 0xffff        # a row of plan-official.tsv with no last start year: every later start year
OUT = os.path.join(ROOT, "src", "recomp_rt", "win", "season_history_table.c")


def c_str(s):
    b = s.encode("latin-1")
    out = []
    for ch in b:
        if ch in (0x22, 0x5c):
            out.append("\\" + chr(ch))
        elif 0x20 <= ch < 0x7f and ch != 0x3f:      # '?' is escaped: no trigraphs
            out.append(chr(ch))
        else:
            out.append("\\%03o" % ch)
    return '"' + "".join(out) + '"'


FIRST_LABEL = 2001     # 2000/01 (Q354) to the start year
NAME_MAX = 50


def validate(rows):
    """The texts of the rows that are not valid."""
    bad, seen = [], set()
    for i, r in enumerate(rows, 2):
        where = f"plan.tsv line {i} ({r['database']}, {r['competition']} {r['label']})"
        start, label = int(r["start_year"]), int(r["label"])
        if not FIRST_LABEL <= label <= start:
            bad.append(f"{where}: label outside {FIRST_LABEL} to {start}")
        names = [r["competition"]] + ([r[f] for f in ("first", "second", "third", "host")] if r["action"] == "add" else [])
        for n in names:
            try:
                b = n.encode("latin-1")
            except UnicodeEncodeError:
                bad.append(f"{where}: {n!r} is not Latin-1")
                continue
            if len(b) > NAME_MAX:
                bad.append(f"{where}: {n!r} is longer than {NAME_MAX} bytes")
        if r["action"] == "add" and not r["first"]:
            bad.append(f"{where}: no winner")
        if "sources" in r and not r["sources"]:
            bad.append(f"{where}: no source")
        key = (r["action"], r["database"], r["competition"], r["label"], r.get("part", ""), r.get("first", ""),
               r.get("second", ""))
        if key in seen:
            bad.append(f"{where}: duplicate row")
        seen.add(key)
    return bad


def read_plan(path):
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    bad = validate(rows)
    if bad:
        raise SystemExit(f"{os.path.basename(path)} has rows that are not valid:\n  " + "\n  ".join(bad[:50]))
    return rows


# The League of Ireland moved to calendar years in 2003 (docs/research/2026-11-15-ireland-calendar-2003.md, decision 8;
# season_calendar_label in src/recomp_rt/win/season_calendar.c). The game writes the short season 2002/03 under the
# label 2002 and the 2003 season under 2003, so a researched row of part "2002/03" takes the label 2002 too. Then the
# start-year clear of a 2003 start meets one data row of 2003, the real 2003 season, and the game's own 2003 edition
# replaces it (src/recomp_rt/win/season_history.c, the wrap of 0x4afce0).
IRISH_MOVE = ("Irish Premier Division", "Irish First Division", "Irish Senior Challenge Cup")


def label_of(r):
    """The label year that the game keeps for a plan row."""
    if r["action"] == "add" and r["competition"] in IRISH_MOVE and r.get("part") == "2002/03" and r["label"] == "2003":
        return 2002
    return int(r["label"])


def span(r):
    """(from, to) of a row: 0, 0 for a row of plan.tsv; the start years of a row of plan-official.tsv."""
    if "last_start" not in r:
        return 0, 0
    first = int(r["start_year"])
    last = int(r["last_start"]) if r["last_start"] else NO_LAST
    if last < first:
        raise SystemExit(f"plan-official.tsv: {r['database']} {r['competition']} {r['label']}: last start before first")
    return first, last


def irish_faults(rows):
    """The data sets whose table would hold two researched rows of label 2003 for a competition of IRISH_MOVE: the
    start-year clear of a 2003 start then keeps both, and the game's own 2003 edition becomes a third row."""
    seen, bad = {}, []
    for r in rows:
        if r["action"] == "add" and r["competition"] in IRISH_MOVE and label_of(r) == 2003:
            k = (r["database"], r["start_year"], r["competition"])
            if k in seen:
                bad.append(f"{k[0]} {k[1]} {k[2]}: two rows of label 2003")
            seen[k] = 1
    return bad


def make():
    dbs = {}
    for r in read_plan(PLAN):
        k = (r["database"], int(r["start_year"]), 0)
        dbs.setdefault(k, {"clear": [], "add": []})[r["action"]].append(r)
    for r in read_plan(PLAN_FIXES):                 # hand-kept data fixes, after the rows of history_fill.py
        k = (r["database"], int(r["start_year"]), 0)
        dbs.setdefault(k, {"clear": [], "add": []})[r["action"]].append(r)
    official = read_plan(PLAN_OFFICIAL) if os.path.isfile(PLAN_OFFICIAL) else []
    bad = irish_faults(read_plan(PLAN) + official)
    if bad:
        raise SystemExit("the Irish short season keeps label 2003:\n  " + "\n  ".join(bad))
    firsts = {}
    for r in official:
        firsts[r["database"]] = min(firsts.get(r["database"], 9999), int(r["start_year"]))
    for r in official:
        k = (r["database"], firsts[r["database"]], NO_LAST)
        dbs.setdefault(k, {"clear": [], "add": []})[r["action"]].append(r)
    lines = ["/* Made by tools/seasons/make_history_table.py from docs/research/history-0.6.0/plan.tsv. Do not edit. */",
             "/* The competition history of the season databases before 2010/11 (0.6.0 slice 11c, Q347). */",
             '#include "season_history.h"', ""]
    table = []
    for i, ((db, start, last), v) in enumerate(dbs.items()):
        lines.append(f"static const SeasonHistClear clears_{i}[] = {{")
        for r in v["clear"]:
            lines.append(f"    {{{c_str(r['competition'])}, {int(r['label'])}, {', '.join(map(str, span(r)))}}},")
        lines.append("};")
        lines.append(f"static const SeasonHistRow rows_{i}[] = {{")
        for r in v["add"]:
            places = ", ".join(c_str(r[f]) for f in ("first", "second", "third", "host"))
            nations = ", ".join(c_str(r[f + "_nation"]) for f in ("first", "second", "third", "host"))
            lines.append(f"    {{{c_str(r['competition'])}, {label_of(r)}, {{{places}}}, {{{nations}}}, "
                         f"{', '.join(map(str, span(r)))}}},")
        lines.append("};")
        table.append(f"    {{{c_str(db)}, {start}, {last}, clears_{i}, {len(v['clear'])}, rows_{i}, {len(v['add'])}}},")
    lines.append("const SeasonHistDb season_history_dbs[] = {")
    lines += table
    lines.append("};")
    lines.append(f"const int season_history_dbs_count = {len(table)};")
    return "\n".join(lines) + "\n"


def main(argv):
    text = make()
    if "--check" in argv:
        old = open(OUT, encoding="latin-1").read() if os.path.isfile(OUT) else ""
        if old != text:
            print(f"{OUT} is not the table that {PLAN} and {PLAN_OFFICIAL} give: run tools/seasons/make_history_table.py")
            return 1
        print("season history table matches plan.tsv and plan-official.tsv")
        if "--stamp" in argv:
            stamp = argv[argv.index("--stamp") + 1]
            os.makedirs(os.path.dirname(stamp), exist_ok=True)
            with open(stamp, "w") as f:
                f.write("ok\n")
        return 0
    with open(OUT, "w", encoding="latin-1", newline="\n") as f:
        f.write(text)
    print("wrote", OUT)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
