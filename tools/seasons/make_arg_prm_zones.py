#!/usr/bin/env python3
"""Makes src/recomp_rt/win/season_sizes_arg_table.c: the zones, the inter-zone pairs and the derby rounds of the
Argentine Primera (0.7.2 league sizes, slice 15; spec docs/specs/2026-11-13-league-sizes-0.7.2-spec.md, D9).

Usage:
    python3 -I tools/seasons/make_arg_prm_zones.py [--out <file>] [--check]

Input: docs/research/arg-zones-0.7.2/primera.tsv (two sources a row, its README.md) and, for the database name of a
derby partner, docs/research/rules-0.6.0/arg-usa-clubs.tsv. The season column is the rules row (2024/25 is the 2025
season, the row of season 2024). Only the Apertura rows go in: the Clausura has the same zones, and its inter-zone
matches are those of the Apertura with the venues reversed (the README), which the runtime makes.

Each club is written with its database name (db_club, the long name of club.dat +4), which the runtime compares with
the clubs of the division. --check exits with code 1 when the committed file differs (ctest arg_prm_zones).
"""
import argparse
import csv
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
TSV = os.path.join(ROOT, "docs", "research", "arg-zones-0.7.2", "primera.tsv")
CLUBS = os.path.join(ROOT, "docs", "research", "rules-0.6.0", "arg-usa-clubs.tsv")
OUT = os.path.join(ROOT, "src", "recomp_rt", "win", "season_sizes_arg_table.c")
# Derby partners that no row of primera.tsv names as its club, or names by another season's database name: (season,
# short name): the real name of the clubs file. They come before the names of the club column.
ALIASES = {
    ("2014/15", "Crucero del Norte"): "Crucero del Norte",
    ("2014/15", "Quilmes"): "Quilmes Atlético Club",
    ("2016/17", "Quilmes"): "Quilmes Atlético Club",
    # The 2025 rows name Sarmiento by the database name of 2025; the databases of 2014 and 2016 hold another.
    ("2014/15", "Sarmiento (J)"): "Sarmiento de Junín",
    ("2016/17", "Sarmiento (J)"): "Sarmiento de Junín",
}


def c_str(s):
    b = s.encode("cp1252")
    if len(b) > 51:
        raise SystemExit(f"make_arg_prm_zones.py: name longer than 51 bytes: {s}")
    return '"' + "".join(chr(x) if 32 <= x < 127 and chr(x) not in '"\\' else f"\\{x:03o}" for x in b) + '"'


def row_season(text):
    m = re.fullmatch(r"(\d{4})/\d{2}", text)
    return int(m.group(1)) if m else None


def match_kind(r):
    """rival or draw: the words after the row's first fecha ("fecha 8, draw match"), else None."""
    m = re.search(r"fecha \d+, (rival|draw) match", r["note"])
    return m.group(1) if m else None


def make():
    rows = list(csv.DictReader(open(TSV, encoding="utf-8"), delimiter="\t"))
    db = {}
    for r in csv.DictReader(open(CLUBS, encoding="utf-8"), delimiter="\t"):
        if r["division"].startswith("Primera") and r["db_club"]:
            db[(r["season"], r["club"])] = r["db_club"]
    zones, pairs = [], []
    # The database name of each short name of the file (its club column), and of the partners that are never in it
    # (ALIASES, each checked against the Primera list of its season in the clubs file).
    names = {}
    for r in rows:      # the season's own name first (row_key), then any season's
        names[(r["season"].split("-")[0], r["club"])] = r["db_club"]
        names.setdefault(r["club"], r["db_club"])
    for (season, short), real in ALIASES.items():
        if (season, real) not in db:
            raise SystemExit(f"make_arg_prm_zones.py: alias {season} {short}: no club {real} in the clubs file")
        names[(season, short)] = db[(season, real)]
    for r in rows:
        if r["kind"] == "zone" and row_season(r["season"]) is not None:
            # The 2016 sources number the zones (Zona 1 and Zona 2).
            zones.append((row_season(r["season"]), {"1": "A", "2": "B"}.get(r["zone"], r["zone"]), r["db_club"]))
    # A season of rival matches only (2016: each pair twice): the fecha that holds most of them is the round where every
    # club meets its rival (fecha 12), which the runtime plays as its round of drawn opponents, each the rival.
    full = {}
    for season in {r["season"] for r in rows if r["kind"] == "interzone"}:
        inter = [r for r in rows if r["season"] == season and r["kind"] == "interzone"]
        if any(match_kind(r) == "draw" for r in inter):
            continue
        fechas = [int(re.search(r"fecha (\d+)", r["note"]).group(1)) for r in inter]
        best = max(set(fechas), key=fechas.count)
        if fechas.count(best) * 2 >= len({frozenset((r["club"], r["partner"])) for r in inter}):
            full[season] = best
    for r in rows:
        s = row_season(r["season"])
        if s is None or r["kind"] not in ("interzone", "derby"):
            continue
        partner = names.get((r["season"].split("-")[0], r["partner"])) or names.get(r["partner"])
        if not partner:
            raise SystemExit(f"make_arg_prm_zones.py: {r['season']} {r['partner']}: no database name")
        m = re.search(r"fecha (\d+)", r["note"])
        if not m:
            raise SystemExit(f"make_arg_prm_zones.py: {r['season']} {r['club']}: no fecha in the note")
        kind = "derby" if r["kind"] == "derby" else match_kind(r)
        if kind == "rival" and full.get(r["season"]) == int(m.group(1)):
            kind = "draw"
        if r["kind"] == "interzone" and not match_kind(r):
            raise SystemExit(f"make_arg_prm_zones.py: {r['season']} {r['club']}: neither a rival nor a draw match")
        pairs.append((s, kind, int(m.group(1)), r["db_club"], partner))
    for s in sorted({z[0] for z in zones}):
        a = sum(1 for z in zones if z[0] == s and z[1] == "A")
        b = sum(1 for z in zones if z[0] == s and z[1] == "B")
        if a != b:
            raise SystemExit(f"make_arg_prm_zones.py: row {s}: zones of {a} and {b}")
    kinds = {"rival": "SEASON_ARG_RIVAL", "draw": "SEASON_ARG_DRAW", "derby": "SEASON_ARG_DERBY"}
    out = ["/* The zones, inter-zone pairs and derby rounds of the Argentine Primera (season_sizes_arg.h). Made by",
           " * tools/seasons/make_arg_prm_zones.py from docs/research/arg-zones-0.7.2/primera.tsv: do not edit by hand. */",
           '#include "season_sizes_arg.h"', "",
           "const SeasonArgZone season_arg_prm_zones[] = {"]
    out += [f"    {{{s}, '{z}', {c_str(n)}}}," for s, z, n in zones]
    out += ["};", f"const int season_arg_prm_zone_count = {len(zones)};", "",
            "const SeasonArgPair season_arg_pairs[] = {"]
    out += [f"    {{{s}, {kinds[k]}, {f}, {c_str(h)}, {c_str(a)}}}," for s, k, f, h, a in pairs]
    out += ["};", f"const int season_arg_pair_count = {len(pairs)};"]
    return "\n".join(out) + "\n"


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args(argv)
    text = make()
    if a.check:
        old = open(a.out, encoding="utf-8").read() if os.path.exists(a.out) else ""
        if old != text:
            print(f"make_arg_prm_zones.py: {a.out} is not up to date", file=sys.stderr)
            return 1
        print("make_arg_prm_zones.py: up to date")
        return 0
    with open(a.out, "w", encoding="utf-8") as f:
        f.write(text)
    print(f"make_arg_prm_zones.py: wrote {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
