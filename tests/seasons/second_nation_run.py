#!/usr/bin/env python3
"""No capped player in the squad of his second nation (0.7.3 slice 16, issue 141).

Usage:
    python3 tests/seasons/second_nation_run.py <cm0102_recomp> <game folder> <out folder> [--days n]

A new game on the October 2026 database with the leagues of eight nations, on holiday for DAYS days, past the
international matches of September. A capped player in the squad of his second nation loses his first nation at his
first match there (0x76b7a9..0x76b7b9). The original's pick never calls him up (0x76cefc), and neither may the fill of
a short national side (season_national_fill.c).

The checks, on the save against the database:
    - no player with caps in the database sits in a national side of another nation than his first nation
    - a player with caps in the database keeps his first nation (an uncapped player may change it, as in the original)
<out folder>/well-known.tsv lists the 500 players of the highest world reputation with the result of each.
"""
import collections
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
for p in (HERE, os.path.join(HERE, "..", "smoke"), os.path.join(ROOT, "tools", "save"),
          os.path.join(ROOT, "tools", "seasons", "own_db")):
    sys.path.insert(0, p)
import cmdb  # noqa: E402
import rules_report_check as rrc  # noqa: E402
import savfile  # noqa: E402
import season_core_run as scr  # noqa: E402

DB = "/Users/max/Projects/cm0102/test-fixtures/databases/October 2026/October 2026 Data Update"
NATIONS = ["England", "Italy", "Spain", "France", "Germany", "Holland", "Portugal", "Belgium"]
STAFF, CLUB, NATION, NAME = 110, 581, 290, 60
DAYS = 100


def database_rows():
    """{(first, second, birth day): (first nation, caps, world reputation, shown name)} of the database."""
    f = cmdb.Folder(DB)
    first, second, common = cmdb.names(f, 13), cmdb.names(f, 14), cmdb.names(f, 15)
    players = {struct.unpack_from("<i", r, 0)[0]: struct.unpack_from("<h", r, 13)[0] for r in f.rows(10)}
    out, twins = {}, collections.Counter()
    for r in f.rows(6):
        _, a, b, c = struct.unpack_from("<iiii", r, 0)
        world = players.get(struct.unpack_from("<i", r, 0x61)[0])
        if world is None:
            continue
        key = (first.get(a, ""), second.get(b, ""), struct.unpack_from("<h", r, 0x10)[0])
        twins[key] += 1
        out[key] = (struct.unpack_from("<i", r, 0x1a)[0], r[0x22], world, common.get(c) or f"{key[0]} {key[1]}")
    return {k: v for k, v in out.items() if twins[k] == 1}      # two players of one name and birth day: the shifted year cannot tell them apart


def names_of(sav, member):
    d = sav.read_member(member)
    return {struct.unpack_from("<i", d, i * NAME + 0x33)[0]: d[i * NAME:i * NAME + 51].split(b"\0")[0].decode("latin-1")
            for i in range(len(d) // NAME)}


def check(save, out):
    sav = savfile.Sav(save)
    n = sav.read_member("nation.dat")
    nations = {struct.unpack_from("<i", n, i * NATION)[0]: n[i * NATION + 4:i * NATION + 55].split(b"\0")[0].decode("latin-1")
               for i in range(len(n) // NATION)}
    sides = {}
    for member in ("club.dat", "nat_club.dat"):
        d = sav.read_member(member)
        for i in range(len(d) // CLUB):
            sides[struct.unpack_from("<i", d, i * CLUB)[0]] = struct.unpack_from("<i", d, i * CLUB + 0x53)[0]
    first, second = names_of(sav, "first_names.dat"), names_of(sav, "second_names.dat")
    data = database_rows()
    staff = sav.read_member("staff.dat")
    seen, faults, count = {}, [], collections.Counter()
    for i in range(len(staff) // STAFF):
        r = staff[i * STAFF:(i + 1) * STAFF]
        a, b = struct.unpack_from("<ii", r, 4)
        row = data.get((first.get(a, ""), second.get(b, ""), struct.unpack_from("<h", r, 0x10)[0]))
        if not row:
            continue
        home, caps, _, name = row
        now, side = struct.unpack_from("<i", r, 0x1a)[0], struct.unpack_from("<i", r, 0x24)[0]
        result = "ok"
        if now != home and caps:
            result = f"first nation {nations.get(now)}, in the database {nations.get(home)}"
        elif caps and side in sides and sides[side] != home:
            result = f"{caps} caps for {nations.get(home)}, in the squad of {nations.get(sides[side])}"
        count["players"] += 1
        seen[name] = result
        if result != "ok":
            faults.append(f"{name}: {result}")
    top = sorted(data.values(), key=lambda v: -v[2])[:500]
    with open(os.path.join(out, "well-known.tsv"), "w", encoding="utf-8") as f:
        f.write("player\tworld reputation\tresult\n")
        for _, _, world, name in top:
            f.write(f"{name}\t{world}\t{seen.get(name, 'not in this game')}\n")
    held = sum(1 for v in top if v[3] in seen)
    print(f"{count['players']} players read, {held} of the 500 best known, {len(faults)} faults")
    return faults


def main(argv):
    days = int(argv[argv.index("--days") + 1]) if "--days" in argv else DAYS
    args = [a for i, a in enumerate(argv[1:]) if a != "--days" and argv[i] != "--days"]
    if len(args) != 3:
        print(__doc__)
        return 2
    program, game, out = (os.path.abspath(a) for a in args)
    os.makedirs(out, exist_ok=True)
    steps = rrc.league_steps(NATIONS, []) + ["3000000 quit"]
    save, log, _ = scr.play(program, game, out, "new", {"changing_rules": "on"}, steps, days,
                            {"RECOMP_START_SEASON": "2026"}, database=DB, timeout=3600)
    with open(os.path.join(out, "new.log"), "w", encoding="utf-8", errors="replace") as f:
        f.write(log)
    if not save:
        print("FAIL  no save")
        return 1
    faults = check(save, out)
    for x in faults[:40]:
        print("FAIL ", x)
    print("ok" if not faults else f"FAIL  {len(faults)} players")
    return 1 if faults else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
