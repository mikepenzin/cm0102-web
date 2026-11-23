#!/usr/bin/env python3
"""Reach check of the slice 6 fallbacks (0.7.2 league sizes, gap rows 19 and 20): over every row of the rules table
and every season database, no real season reaches them.

Usage:
    python3 -I tests/seasons/cup_east_reach_check.py [--databases <folder>]

The fallbacks are in src/recomp_rt/win/season_cup_sizes_east.c:
    cro   CroCup::collect_entrants 0x517bb0 takes every club of its three divisions with no bound (0x517c71..0x517d19)
          into 48 places (0x517bc5). Over 48, the wrap leaves out second-tier clubs. Check: the rows of the Prva HNL
          and the Druga HNL North, plus the Druga HNL South (its row, or its class cap of 16 in the "na" seasons, when
          the game keeps it and its clubs only leave), are 48 or less in every season.
    kor   KorLeagueCup::select_teams 0x66cf20 fills 10 pointers (push 0xa at 0x66cf2a). A K League under 10 leaves a
          group short and the game stops at a message box. Check: every K League row is 10 or more.
    gre   GreCup::collect_entrants 0x5e9a80 needs 44 less the two divisions as lower picks (rt_gre_cup_lower), from
          at most 100 clubs of the Greek Lower Division (0x5e9b2d..0x5e9b5a). When that pool is short, the wrap joins
          Greek clubs of no cup division, and with none to join the game stops at box gre_cup.cpp 802 (0x5e9c24).
          Check: in every season database the Greek Lower Division holds the most picks that any season needs, after
          the clubs that a growth of the two divisions above it takes from it, and after the second group of the
          Greek second tier (gre_fill, src/recomp_rt/win/season_divisions.c, up to GRE_GROUP_MAX 14 clubs from the
          Lower Division), taken off for every season from the first group season on.
The game plays a row's count up to the class cap (CAPS of tools/recomp/league_sizes.py). A count of "unknown" is the
class count at a new game (class_size, src/recomp_rt/win/season_clubs.c) and the last count during a game, so the
check takes the worse of the two. The check takes the side that is worst for each fallback: the most clubs for the
Croatian Cup, the fewest for the K League and the Greek Cup.

The database part needs the season databases of test-fixtures (as club_ids_check.py). Without them the row part
runs alone, and the check ends with code 77 (ctest skip) when the rows pass.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools", "seasons"))
sys.path.insert(0, os.path.join(ROOT, "tools", "seasons", "own_db"))
sys.path.insert(0, os.path.join(ROOT, "tools", "recomp"))
import league_sizes  # noqa: E402
import make_rules  # noqa: E402

YEARS = range(2001, 2028)
CRO_PLACES, KOR_PLACES, GRE_PLACES = 48, 10, 44
CRO_SOUTH_CAP = 16            # CroA2b::create_table 0x516dd0: 16 rows (mov word ptr [esi + 0x3e], 0x10 at 0x516de2)
GRE_POOL_MAX = 100            # cmp bl, 0x64 at 0x5e9b3e
GRE_GROUP_MAX = 14            # GreSecond's table of 14 rows (malloc(0x33a) at 0x5ebcbb), season_divisions.c
GRE_LOWER = "Greek Lower Division"
GRE_TOP = ("Greek National A Division", "Greek National B Division")


def class_counts(comp_file):
    """(smallest, largest) count of the class in the original."""
    for _c, f, _u, stores in league_sizes.CLASSES:
        if f == comp_file:
            return min(n for _a, _r, n in stores), league_sizes.CAPS[f]
    raise SystemExit(f"cup_east_reach_check.py: no class {comp_file}")


def counts(rows, comp_file):
    """{year: [(fewest, most) clubs that the game plays for each row of the year]}: a count up to the class cap,
    "unknown" as the last count or the class count, "na" as None."""
    low, cap = class_counts(comp_file)
    by_year = {}
    for r in rows:
        if r["comp_file"] == comp_file and r["season"] in YEARS:
            by_year.setdefault(r["season"], []).append(r["clubs"])
    out, last = {}, None
    for y in YEARS:
        vals = []
        for c in by_year.get(y, []) or ["unknown"]:
            if isinstance(c, int):
                last = min(c, cap)
                vals.append((last, last))
            elif c == "unknown":
                vals.append((min(low, last if last is not None else low), cap))
            else:
                vals.append(None)
        out[y] = vals
    return out


def row_part(rows):
    fails, notes = [], []
    a1, a2a, a2b = counts(rows, "cro_a1"), counts(rows, "cro_a2a"), counts(rows, "cro_a2b")
    worst = 0
    most = lambda vals: sum(v[1] for v in vals if v)  # noqa: E731
    for y in YEARS:
        south = most(a2b[y]) if any(a2b[y]) else CRO_SOUTH_CAP
        total = most(a1[y]) + most(a2a[y]) + south
        worst = max(worst, total)
        if total > CRO_PLACES:
            fails.append(f"cro {y}: {total} clubs in the three divisions, over {CRO_PLACES}")
    notes.append(f"cro: at most {worst} clubs of {CRO_PLACES} places")
    kor = counts(rows, "kor_league")
    least = min(min(v[0] for v in kor[y] if v) for y in YEARS if any(kor[y]))
    for y in YEARS:
        if any(v is None or v[0] < KOR_PLACES for v in kor[y]):
            fails.append(f"kor {y}: K League counts {kor[y]}, under {KOR_PLACES}")
    notes.append(f"kor: at least {least} clubs for {KOR_PLACES} places")
    prm, second = counts(rows, "gre_prm"), counts(rows, "gre_second")
    need, group_from = {}, None
    for y in YEARS:
        if len(second[y]) > 1 and group_from is None:
            group_from = y
        top = min(v[0] for v in prm[y] if v) + min(v[0] for v in second[y] if v)
        need[y] = (max(1, GRE_PLACES - top), max(v[1] for v in prm[y] if v) + max(v[1] for v in second[y] if v),
                   GRE_GROUP_MAX if group_from is not None else 0)
    notes.append(f"gre: at most {max(n[0] for n in need.values())} lower picks in a season, the second group from "
                 f"{group_from}")
    return fails, notes, need


def database_part(need, root):
    import club_ids_check as ids
    if not os.path.isdir(root):
        return None, [f"gre: no database folder {root}, the database part did not run"]
    folders, base, missing = ids.database_folders(root)
    if not base:
        return None, [f"gre: no 3.9.68 data in {root}, the database part did not run"]
    fails, worst, notes = [], None, []
    for start in sorted(set(folders) | {2001}):
        if start in missing:
            notes.append(f"gre: no database for the start {start}, not checked")
            continue
        db = ids.Database(folders.get(start, base))
        greek = [d for (_l, _s, n, _t, d) in db.clubs.values() if n == "Greece"]
        lower = min(GRE_POOL_MAX, greek.count(GRE_LOWER))
        top_db = sum(greek.count(d) for d in GRE_TOP)
        for y in YEARS:
            if y < start:
                continue
            picks, top_most, group = need[y]
            pool = lower - max(0, top_most - top_db) - group
            if worst is None or pool - picks < worst[0]:
                worst = (pool - picks, start, y, pool, picks)
            if pool < picks:
                fails.append(f"gre: start {start}, season {y}: {pool} lower clubs for {picks} picks")
    note = notes + [f"gre: databases from {min(set(folders) | {2001})} to {max(set(folders) | {2001})}"]
    if worst:
        note.append(f"gre: least room {worst[0]} (start {worst[1]}, season {worst[2]}: {worst[3]} lower clubs for "
                    f"{worst[4]} picks)")
    return fails, note


def main(argv):
    import club_ids_check as ids
    root = argv[argv.index("--databases") + 1] if "--databases" in argv else ids.DEFAULT_DB
    rows = make_rules.read_rows()
    fails, notes, need = row_part(rows)
    f2, n2 = database_part(need, root)
    for n in notes + n2:
        print("note ", n)
    for f in fails + (f2 or []):
        print("FAIL ", f)
    if fails or f2:
        print("FAIL")
        return 1
    if f2 is None:
        print("SKIP  the rows pass, the database part did not run")
        return 77
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
