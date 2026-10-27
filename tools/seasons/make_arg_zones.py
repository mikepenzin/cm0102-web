#!/usr/bin/env python3
"""The zone fill table of the Argentine clubs (0.6.0, O259 net B2, O375): season_arg_zones_table.c.

Usage:
    python3 -I tools/seasons/make_arg_zones.py --check              the C file matches the table, the table is clean
    python3 -I tools/seasons/make_arg_zones.py --write              write the C file from the table
    python3 -I tools/seasons/make_arg_zones.py --derive [<folder>]  write the rows that the season databases give
                                                                    (keeps the rows with a web source)

Input: docs/research/arg-zones-0.6.0/zones.tsv, columns season, club_id, club, zone, source.
A club of the Argentine second division needs a zone record (club +0x60: Argentine Metropolitan Zone or Argentine
Interior Zone) because ArgSecond::fill_league_table 0x407cd0 counts only clubs that have one (0x407d7f). Some season
databases hold a club that the real second division has with no zone record (club.dat +0x60 = -1) and give it the zone
in a later database: Boca Unidos in September 2009 and 2010, Crucero del Norte in September 2012, Gimnasia y Esgrima
de Mendoza in October 2014. A row of the table is such a club in the database of that season (the club record id and
the long name there) and its zone. The club moves (season_clubs.c) write the zone record of a club that a list moves
into a division and that has none, when the season, the id and the name match a row.
    - source "database <name>": the zone that the same record id has in that later database (derive);
    - source with a web address: the zone from two public sources, no later database gives the club a zone. The cell
      holds both addresses with the date read, for example "https://...; https://...; read 2026-10-27".
The check stops (exit code 1) on a bad row: a season outside 2001 to 2099, a club id that is not a number, a zone that
is not Metropolitan or Interior, a name that Windows-1252 cannot hold or that is longer than 51 characters, a missing
source, a web source without two addresses and a read date, or the same (season, club id) twice.
"""
import csv
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
TSV = os.path.join(ROOT, "docs", "research", "arg-zones-0.6.0", "zones.tsv")
OUT = os.path.join(ROOT, "src", "recomp_rt", "win", "season_arg_zones_table.c")
COLUMNS = ["season", "club_id", "club", "zone", "source"]
ZONES = {"Metropolitan": "Argentine Metropolitan Zone", "Interior": "Argentine Interior Zone"}
LIMIT = 51


def fail(text):
    sys.stderr.write("make_arg_zones.py: " + text + "\n")
    sys.exit(1)


def read():
    with open(TSV, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    seen = set()
    for n, r in enumerate(rows, 2):
        where = f"{TSV} line {n}"
        if list(r) != COLUMNS or None in r.values():
            fail(f"{where}: the columns are {COLUMNS}")
        try:
            season, cid = int(r["season"]), int(r["club_id"])
        except ValueError:
            fail(f"{where}: season and club_id are numbers")
        if not 2001 <= season <= 2099 or cid < 0:
            fail(f"{where}: season {season} or club id {cid} out of range")
        if r["zone"] not in ZONES:
            fail(f"{where}: the zone is Metropolitan or Interior")
        try:
            name = r["club"].encode("cp1252")
        except UnicodeEncodeError:
            fail(f"{where}: the name has a character that Windows-1252 lacks")
        if not name or len(name) > LIMIT:
            fail(f"{where}: the name is empty or longer than {LIMIT}")
        src = r["source"]
        if not src:
            fail(f"{where}: no source")
        if not src.startswith("database "):
            if len(re.findall(r"https?://\S+", src)) < 2 or not re.search(r"read 20\d\d-\d\d-\d\d", src):
                fail(f"{where}: a web source holds two addresses and 'read yyyy-mm-dd'")
        if (season, cid) in seen:
            fail(f"{where}: season {season} club {cid} twice")
        seen.add((season, cid))
        r["season"], r["club_id"] = season, cid
    rows.sort(key=lambda r: (r["season"], r["club_id"]))
    return rows


def c_string(text):
    out = []
    for b in text.encode("cp1252"):
        out.append(chr(b) if 32 <= b < 127 and chr(b) not in '"\\?' else f"\\{b:03o}")
    return '"' + "".join(out) + '"'


def text():
    rows = read()
    lines = ["/* Made by tools/seasons/make_arg_zones.py from docs/research/arg-zones-0.6.0/zones.tsv. Do not edit. The",
             " * names are in code page 1252, as the game holds them. */",
             '#include "season_arg_zones.h"', "",
             "const SeasonArgZone season_arg_zones[] = {"]
    for r in rows:
        lines.append(f"    {{{r['season']}, {r['club_id']}, {c_string(r['club'])}, '{r['zone'][0]}'}},")
    lines += ["};", f"const int season_arg_zone_count = {len(rows)};", ""]
    return "\n".join(lines)


def derive(folder):
    sys.path.insert(0, os.path.join(ROOT, "tests", "seasons"))
    sys.path.insert(0, os.path.join(ROOT, "tools", "seasons", "own_db"))
    import difflib
    import club_ids_check as c
    from cmdb import Folder, cstr, norm
    i32 = lambda r, o: struct.unpack_from("<i", r, o)[0]  # noqa: E731
    folders, base, _ = c.database_folders(folder)
    by_year = {}
    for year in range(2001, 2026):
        path = folders.get(year, base)
        f = Folder(path)
        arg = [i32(r, 0) for r in f.rows(4) if cstr(r, 4, 51) == "Argentina"][0]
        zone_name = {i32(r, 0): cstr(r, 4, 51) for r in f.rows(12)}
        by_year[year] = (os.path.basename(path), {i32(r, 0): (cstr(r, 4, 51), i32(r, 0x60)) for r in f.rows(0)
                                                  if i32(r, 0x53) == arg}, zone_name)
    keep = []
    if os.path.exists(TSV):
        with open(TSV, newline="", encoding="utf-8") as f:
            keep = [r for r in csv.DictReader(f, delimiter="\t") if not r["source"].startswith("database ")]
    out = []
    for year in range(2001, 2025):
        for cid, (name, zone) in by_year[year][1].items():
            if zone > 0:
                continue
            for later in range(year + 1, 2026):
                other = by_year[later][1].get(cid)
                if not other or other[1] <= 0:
                    continue
                if difflib.SequenceMatcher(None, norm(name), norm(other[0])).ratio() < 0.6:
                    break
                zn = by_year[later][2].get(other[1], "")
                kind = next((k for k, v in ZONES.items() if v == zn), None)
                if not kind:
                    fail(f"record {cid} in {by_year[later][0]}: zone record {other[1]} is {zn!r}")
                out.append({"season": year, "club_id": cid, "club": name, "zone": kind,
                            "source": f"database {by_year[later][0]}"})
                break
    rows = sorted(out + keep, key=lambda r: (int(r["season"]), int(r["club_id"])))
    os.makedirs(os.path.dirname(TSV), exist_ok=True)
    with open(TSV, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, COLUMNS, delimiter="\t", lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    print(f"wrote {TSV}: {len(out)} rows from databases, {len(keep)} with a web source")


def main(argv):
    if "--derive" in argv:
        i = argv.index("--derive")
        derive(argv[i + 1] if len(argv) > i + 1 else "/Users/max/Projects/cm0102/test-fixtures/databases")
        return 0
    want = text()
    if "--write" in argv:
        with open(OUT, "w", newline="\n") as f:
            f.write(want)
        print(f"wrote {OUT}")
        return 0
    have = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
    if have != want:
        fail(f"{OUT} is out of date: run make_arg_zones.py --write")
    print("make_arg_zones.py: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
