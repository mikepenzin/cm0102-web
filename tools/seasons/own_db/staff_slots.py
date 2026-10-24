"""Fill empty non-playing staff slots with the database's own free staff, so that a new game with every league
selected fits the original game's 23,000 extra staff rows.

usage: python3 -I staff_slots.py <db folder> <report.tsv> [<target need> [<first season>]]

At a new game, database_generate_extra_non_playing_staff 0x527240 makes a stand-in coach, scout, physio or manager
for every empty slot of the clubs of the selected nations, from 23,000 reserve rows (0x528540). With every league
selected, the game stops at "Database.cpp 1844" (0x527e4c) when it needs more. The 3.9.68 data needs 22,645 by the
estimate of src/recomp_rt/win/staff_reserve.c (need() below counts the same way); the Update Team templates need
23,832 (November 2007) and 23,061 (September 2005), and our databases more (long test of 2026-10-21). Max's answer
(option A): fill empty slots at the lowest-reputation clubs of each nation with that nation's own free non-playing
staff until the need is under the target (22,000 by default, so the 22,500 of the answer holds with margin).

Who: a free non-player (staff +0x5f 1, no club +0x39, no national team +0x24, a non-player row +0x69) whose job
+0x3d is the slot's (8 coach, 9 scout, 10 physio, 5 manager), of the club's nation (+0x1a against club +0x53),
aged 26 to 70 in the game's first season. Nobody with a club moves. Which: the slots go from the lowest club
reputation (+0x80) up, and each takes the free person of its nation and job whose ability (non-player +0x04)
is nearest the median ability of the employed staff of that job at clubs of that reputation (500 a step).
What changes: the person's club, a joined date of 1 July of the first season, a contract to 30 June two years on,
and the contract fields +0x4e..+0x5e of the employed staff member of that job and nation at the nearest club
reputation; the club's slot list (+0xcf manager, +0x19f coaches, +0x1b3 scouts, +0x1cf physios).
The first season is the year of the stored dates: 2001 for the databases stored the Update Team way (the default),
2002 for Summer 2002, whose dates are real (Q12). A format 1 folder (Summer 2002, cmdb1.py) has the non-player row
at +0x99 in place of +0x69, and -2 (as well as -1) for no national team at +0x24; the other fields are at the
same offsets.
"""
import collections
import os
import statistics
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cmdb1  # noqa: E402
from cmdb import Folder, cstr  # noqa: E402
from convert import check_squad_lists  # noqa: E402

LEAGUE_NATIONS = ["Argentina", "Australia", "Belgium", "Brazil", "Croatia", "Denmark", "England", "Finland", "France",
                  "Germany", "Greece", "Holland", "Republic Of Ireland", "Italy", "Japan", "Northern Ireland",
                  "Norway", "Poland", "Portugal", "Russia", "Scotland", "South Korea", "Spain", "Sweden", "Turkey",
                  "United States", "Wales"]
SLOTS = {8: (0x19f, 5), 9: (0x1b3, 7), 10: (0x1cf, 3)}      # job: club list offset, size (loops 1 to 3)
MANAGER = 0xcf
TARGET = 22000
FIRST_SEASON = 2001          # the stored dates (store_update_team_way): the game's own calendar


def i32(b, o):
    return struct.unpack_from("<i", b, o)[0]


def i16(b, o):
    return struct.unpack_from("<h", b, o)[0]


def per_club(rep, k):
    n = (rep + 500) * k // 10000
    return 1 if n < 2 else k if n > k - 1 else n


def nations(F):
    sel, eng = set(), -2
    names = {n.lower() for n in LEAGUE_NATIONS}
    for r in F.rows(4):
        n = cstr(r, 4, 52)
        if n.lower() in names:
            sel.add(i32(r, 0))
            if n == "England":
                eng = i32(r, 0)
    return sel, eng


def need(F):
    """The Select All estimate of staff_reserve.c: (total, {(club id, job): missing})."""
    sel, eng = nations(F)
    missing, loop4 = {}, 0
    for r in F.rows(0):
        rep, nat, cid = i16(r, 0x80), i32(r, 0x53), i32(r, 0)
        if nat == eng and rep <= 500:
            continue
        s = nat in sel
        if s:
            for job, (off, k) in SLOTS.items():
                filled = sum(1 for j in range(k) if i32(r, off + 4 * j) != -1)
                if per_club(rep, k) > filled:
                    missing[(cid, job)] = per_club(rep, k) - filled
        if i32(r, MANAGER) == -1 and ((s and i32(r, 0x57) != -1 and rep != 0) or rep >= 7501):
            loop4 += 1
            if s:
                missing[(cid, 5)] = 1
    for r in F.rows(1):
        if not (i32(r, 0x53) == eng and i16(r, 0x80) <= 500) and i32(r, MANAGER) == -1:
            loop4 += 1
    loops = sum(v for (c, j), v in missing.items() if j != 5)
    return loops + loop4 + (loop4 * 10 + 99) // 100, missing


def put_date(b, o, day, year):
    struct.pack_into("<hH", b, o, day, year)


def main(argv):
    db, report = argv[0], argv[1]
    target = int(argv[2]) if len(argv) > 2 else TARGET
    first = int(argv[3]) if len(argv) > 3 else FIRST_SEASON
    fmt1 = cmdb1.staff_format(db) == 1
    F = cmdb1.open_folder(db) if fmt1 else Folder(db)
    np_link = 0x99 if fmt1 else 0x69
    no_nat = (-1, -2) if fmt1 else (-1,)          # 3.9.68 writes -2 for no national team
    before, missing = need(F)
    clubs = {i32(r, 0): r for r in F.rows(0)}
    staff, npl = F.rows(6), F.rows(9)

    def ability(r):
        n = i32(r, np_link)
        return i16(npl[n], 4) if 0 <= n < len(npl) else 0
    # The employed staff of each job: ability by club reputation, and their contracts by nation.
    curve, contracts = collections.defaultdict(list), collections.defaultdict(list)
    for r in staff:
        c, job = i32(r, 0x39), r[0x3d]
        if r[0x5f] != 1 or c not in clubs or job not in (5, 8, 9, 10):
            continue
        rep = i16(clubs[c], 0x80)
        curve[(job, rep // 500)].append(ability(r))
        contracts[(job, i32(clubs[c], 0x53))].append((rep, bytes(r[0x4e:0x5f])))

    def wanted(job, rep):
        for d in range(0, 21):
            v = curve.get((job, rep // 500 - d)) or curve.get((job, rep // 500 + d))
            if v:
                return statistics.median(v)
        return 0
    pool = collections.defaultdict(list)
    for r in staff:
        y = struct.unpack_from("<H", r, 0x12)[0]
        if (r[0x5f] == 1 and i32(r, 0x39) == -1 and i32(r, 0x24) in no_nat and i32(r, np_link) >= 0
                and r[0x3d] in (5, 8, 9, 10) and first - 70 <= y <= first - 26):
            pool[(i32(r, 0x1a), r[0x3d])].append(i32(r, 0))
    slots = sorted(((i16(clubs[c], 0x80), c, job) for (c, job), n in missing.items() for _ in range(n)),
                   key=lambda t: (t[0], t[1], (5, 8, 10, 9).index(t[2])))
    total, done, lines = before, 0, []
    for rep, c, job in slots:
        if total <= target:
            break
        cand = pool.get((i32(clubs[c], 0x53), job))
        if not cand:
            continue
        w = wanted(job, rep)
        sid = min(cand, key=lambda s: (abs(ability(staff[s]) - w), s))
        cand.remove(sid)
        r, cl = staff[sid], clubs[c]
        struct.pack_into("<i", r, 0x39, c)
        put_date(r, 0x3e, 181, first)
        put_date(r, 0x46, 180, first + 2)
        near = contracts.get((job, i32(cl, 0x53)))
        if near:
            r[0x4e:0x5f] = min(near, key=lambda t: abs(t[0] - rep))[1]
        if job == 5:
            struct.pack_into("<i", cl, MANAGER, sid)
        else:
            off, k = SLOTS[job]
            j = next(j for j in range(k) if i32(cl, off + 4 * j) == -1)
            struct.pack_into("<i", cl, off + 4 * j, sid)
        total -= 1 if job != 5 else 1.1      # a manager also lowers the 10 % margin of loop 4
        done += 1
        lines.append((cstr(cl, 4, 51), rep, job, sid, ability(r), w))
    after, _ = need(F)
    check_squad_lists(F.rows(0), F.rows(6))
    for f, b in F.raw.items():
        with open(os.path.join(db, f), "wb") as h:
            h.write(b)
    with open(report, "w", encoding="utf-8") as h:
        h.write("club\treputation\tjob\tstaff id\tability\twanted ability\n")
        for ln in lines:
            h.write("\t".join(map(str, ln)) + "\n")
    print(f"staff slots: Select All need {before} -> {after} (target {target}), {done} free staff placed")
    if after > target:
        sys.exit(f"staff slots: the need {after} is still above {target}: too few free staff of the nations")


if __name__ == "__main__":
    main(sys.argv[1:])
