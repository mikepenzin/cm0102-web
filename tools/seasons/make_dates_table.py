#!/usr/bin/env python3
"""Makes src/recomp_rt/win/season_dates_table.c from the date shift tables (0.6.0 slice 11b).

Usage:
    python3 -I tools/seasons/make_dates_table.py            write season_dates_table.c
    python3 -I tools/seasons/make_dates_table.py --check    exit 1 when season_dates_table.c is not the file that
                                                            the tables would give

The tables are docs/research/rules-0.6.0/databases-date-shifts.tsv (the decided shifts of each known
database), databases-date-shifts-last-rows.tsv (the competitions whose 2001 history row moves) and
databases-date-shifts-careers.tsv (the staff rows whose career keeps its years) and
databases-date-shifts-nation-comps.tsv (the nation competitions with a shift of their own). tools/seasons/
measure_date_shifts.py makes them from the databases. Every database name must be a row of
databases-signatures.tsv. A value that is not a small whole number, or a name the signatures table does not
know, stops the tool with exit code 1.
"""
import csv
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
RULES = os.path.join(ROOT, "docs", "research", "rules-0.6.0")
OUT = os.path.join(ROOT, "src", "recomp_rt", "win", "season_dates_table.c")
FIELDS = ["birth", "joined", "contract", "officials", "staff_history", "comp_history", "last_rows", "nation_history"]
HEX = re.compile(r"^[0-9a-f]*$")


def read(name):
    with open(os.path.join(RULES, name), newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def fail(text):
    raise SystemExit("make_dates_table.py: " + text)


def text():
    known = {r["database"] for r in read("databases-signatures.tsv")}
    last = {r["database"]: [int(x) for x in r["competition_ids"].split()] for r in read("databases-date-shifts-last-rows.tsv")}
    kept = {r["database"]: (int(r["staff_rows"]), r["kept_bitmap"]) for r in read("databases-date-shifts-careers.tsv")}
    nation = {}
    for r in read("databases-date-shifts-nation-comps.tsv"):
        cid, s = int(r["competition_id"]), int(r["shift"])
        if not 0 <= cid < 32768 or not -30 <= s <= 60:          # int16_t pairs in the C table
            fail(f"{r['database']}: bad nation competition row {cid} {s}")
        nation.setdefault(r["database"], []).append((cid, s))
    lines = [
        "/* The date shifts of the known databases (season_dates.h). Made by tools/seasons/make_dates_table.py from",
        " * docs/research/rules-0.6.0/databases-date-shifts*.tsv. Do not edit by hand: change the tables and run the tool. */",
        '#include "season_dates.h"',
        "",
    ]
    rows = []
    for r in read("databases-date-shifts.tsv"):
        name = r["database"]
        if name not in known:
            fail(f"{name!r} is not in databases-signatures.tsv")
        vals = []
        for k in FIELDS:
            if not re.match(r"^-?\d+$", r[k]) or not -30 <= int(r[k]) <= 60:
                fail(f"{name}: bad {k} {r[k]!r}")
            vals.append(int(r[k]))
        ident = re.sub(r"[^A-Za-z0-9]", "_", name)
        ids = last.get(name, [])
        if any(not 0 <= i < 65536 for i in ids):
            fail(f"{name}: a competition id out of range")
        if ids:
            lines.append(f"static const uint16_t last_{ident}[] = {{")
            for i in range(0, len(ids), 16):
                lines.append("    " + ", ".join(str(x) for x in ids[i:i + 16]) + ",")
            lines.append("};")
        nrows, bits = kept.get(name, (0, ""))
        if not HEX.match(bits) or len(bits) != 2 * ((nrows + 7) // 8):
            fail(f"{name}: the career bitmap does not fit {nrows} rows")
        if nrows:
            lines.append(f"static const char kept_{ident}[] =")
            for i in range(0, len(bits), 120):
                lines.append(f'    "{bits[i:i + 120]}"' + (";" if i + 120 >= len(bits) else ""))
        comps = nation.get(name, [])
        if comps:
            lines.append(f"static const int16_t nation_{ident}[] = {{")
            lines.append("    " + ", ".join(f"{c}, {s}" for c, s in comps) + ",")
            lines.append("};")
        rows.append((name, vals, f"last_{ident}" if ids else "0", len(ids), nrows, f"kept_{ident}" if nrows else "0",
                     len(comps), f"nation_{ident}" if comps else "0"))
    names = {r[0] for r in rows}
    for name in sorted((set(last) | set(kept) | set(nation)) - names):
        print(f"make_dates_table.py: warning: {name!r} has last rows, kept careers or nation competition shifts but "
              "no row in databases-date-shifts.tsv: dropped", file=sys.stderr)
    with_season = {r["database"] for r in read("databases-signatures.tsv") if r["data_season"] != "none"}
    for name in sorted(with_season - names):
        print(f"make_dates_table.py: warning: {name!r} of databases-signatures.tsv has no shift row: "
              "the game moves none of its dates", file=sys.stderr)
    lines += ["", "const SeasonDateShifts season_date_shifts[] = {"]
    for name, vals, lname, lcount, nrows, kname, ncount, nname in rows:
        lines.append(f'    {{"{name}", {", ".join(map(str, vals))}, {lcount}, {lname}, {nrows}u, {kname}, {ncount}, {nname}}},')
    lines += ["};", "", f"const int season_date_shifts_count = {len(rows)};", ""]
    return "\n".join(lines)


def main(argv):
    want = text()
    if "--check" in argv:
        have = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
        if have != want:
            print("make_dates_table.py: season_dates_table.c is not the file that the tables give. "
                  "Run tools/seasons/make_dates_table.py.", file=sys.stderr)
            return 1
        print("season_dates_table.c matches the date shift tables")
        return 0
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(want)
    print(f"wrote {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
