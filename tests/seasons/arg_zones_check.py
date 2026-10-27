#!/usr/bin/env python3
"""Argentine zone check (0.6.0, O259 net B2, O375): in every start from 2001 to 2025 each club of the Argentine
second division has a zone record in the database of the season (club.dat +0x60 above 0) or a row in
docs/research/arg-zones-0.6.0/zones.tsv for that season, club record id and club name.

Usage:
    python3 -I tests/seasons/arg_zones_check.py [--databases <folder>]

The clubs of the division in a start are the clubs of its list in the club moves table (tools/seasons/make_clubs.py);
a start with no list (2001, 2004) uses the clubs that its database holds in the division. The databases come from
test-fixtures (and own-dbs for Summer 2004 and Summer 2006, as in club_ids_check.py); without them the check prints
SKIP and ends with code 77 (ctest skip). The table rows that match no club of a start are listed with --unused.
"""
import csv
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools", "seasons", "own_db"))
import club_ids_check as ids  # noqa: E402
from cmdb import Folder, cstr  # noqa: E402

DIVISION = "Argentine Second Division"
TSV = os.path.join(ROOT, "docs", "research", "arg-zones-0.6.0", "zones.tsv")


def main(argv):
    root = argv[argv.index("--databases") + 1] if "--databases" in argv else ids.DEFAULT_DB
    if not os.path.isdir(root):
        print(f"SKIP  no database folder {root}")
        return 77
    folders, base, missing = ids.database_folders(root)
    if not base:
        print(f"SKIP  no 3.9.68 data in {root}")
        return 77
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "clubs.tsv")
        subprocess.run([sys.executable, "-I", os.path.join(ROOT, "tools", "seasons", "make_clubs.py"), "--out", out],
                       check=True, capture_output=True)
        with open(out, newline="", encoding="cp1252") as f:
            moves = [r.rstrip("\n").split("\t") for r in f.readlines()[2:]]
    table = {}
    with open(TSV, newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            table[(int(r["season"]), int(r["club_id"]))] = r["club"]
    i32 = lambda r, o: struct.unpack_from("<i", r, o)[0]  # noqa: E731
    fails, used = [], set()
    for year in range(2001, 2026):
        if year in missing:
            print(f"note  {year}: no database, not checked")
            continue
        folder = Folder(folders.get(year, base))
        records = {i32(r, 0): r for r in folder.rows(0)}
        listed = [int(m[2]) for m in moves if m[0] == str(year) and m[1] == DIVISION and int(m[2]) >= 0]
        if not any(m[0] == str(year) and m[1] == DIVISION for m in moves):
            names = {i32(r, 0): cstr(r, 4, 51) for r in folder.rows(12)}
            listed = [i for i, r in records.items() if names.get(i32(r, 0x57)) == DIVISION]
        for cid in listed:
            r = records.get(cid)
            if r is None or i32(r, 0x60) > 0:
                continue
            name = cstr(r, 4, 51)
            if table.get((year, cid)) == name:
                used.add((year, cid))
            else:
                fails.append(f"{year}: club {cid} {name!r} of the {DIVISION} has no zone record and no row in zones.tsv")
    for f in fails:
        print("FAIL  " + f)
    if "--unused" in argv:
        for k in sorted(set(table) - used):
            print(f"note  row {k[0]} {k[1]} {table[k]!r} matches no club of the division in a start")
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
