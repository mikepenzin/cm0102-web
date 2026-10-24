"""Cases for the Summer 2002 build on format 1 folders made here: the writer of cmdb1.py, the steps of
xbox0203/build_2002.py and the format 1 merge of duplicates.py. Usage: python3 -I test_build_2002.py

The folders are synthetic (invented people and clubs). No game data is in the repo.
"""
import csv
import datetime
import os
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "xbox0203"))
import build_2002 as b2  # noqa: E402
import cmdb1  # noqa: E402
import duplicates  # noqa: E402
from test_format1 import cs, name_row, nation_row  # noqa: E402

D = datetime.date
SQUAD, MANAGER = 0xd7, 0xcf


def club_row(i, name, division, squad=(), manager=-1):
    r = bytearray(581)
    struct.pack_into("<i", r, 0, i)
    r[4:55] = cs(name, 51)
    r[0x38:0x52] = cs(name, 26)
    struct.pack_into("<iii", r, 0x53, 0, division, division)
    for o in range(0x9b, 0x1e0 + 80, 4):
        struct.pack_into("<i", r, o, -1)
    for j, t in enumerate(squad):
        struct.pack_into("<i", r, SQUAD + 4 * j, t)
    struct.pack_into("<i", r, MANAGER, manager)
    return r


def comp_row(i, name):
    r = bytearray(107)
    struct.pack_into("<i", r, 0, i)
    r[4:55] = cs(name, 51)
    struct.pack_into("<i", r, 0x5d, 0)
    return r


def staff_row(i, first, second, dob, club, job, flags, pl, npl, joined=D(2000, 7, 1), cend=D(2003, 6, 30), fav=()):
    r = bytearray(157)
    struct.pack_into("<iiii", r, 0, i, first, second, -1)
    struct.pack_into("<hH", r, 0x10, *dob)
    struct.pack_into("<i", r, 0x1a, 0)
    struct.pack_into("<i", r, 0x24, -2)
    struct.pack_into("<i", r, 0x39, club)
    r[0x3d] = job
    cmdb1.put_date(r, 0x3e, joined)
    cmdb1.put_date(r, 0x46, cend)
    r[0x5f] = flags
    for j in range(6):
        struct.pack_into("<i", r, 0x61 + 4 * j, -1)
        struct.pack_into("<i", r, 0x79 + 4 * j, fav[j] if j < len(fav) else -1)
    struct.pack_into("<i", r, 0x91, pl)
    struct.pack_into("<i", r, 0x99, npl)
    return r


def player_row(i, ca, pa):
    r = bytearray(70)
    struct.pack_into("<i", r, 0, i)
    struct.pack_into("<hh", r, 5, ca, pa)
    struct.pack_into("<hhh", r, 9, 100, 90, 80)
    return r


def np_row(i, fav=()):
    r = bytearray(68)
    struct.pack_into("<i", r, 0, i)
    for j in range(8):
        struct.pack_into("<i", r, 0x23 + 4 * j, fav[j] if j < len(fav) else -1)
    return r


FIRST = ["Jo", "Andy", "Zed", "Ann"]
SECOND = ["Pair", "Evans", "Last", "Boss"]


def people(xbox=False):
    """Staff rows of the template, or of the Xbox copy (Zed at club 0 and better ratings, Ann a free manager)."""
    zed_club, zed_joined = (0, D(2002, 7, 15)) if xbox else (1, D(2000, 7, 1))
    return [
        staff_row(0, 0, 0, (61, 1979), 0, 11, 2, 0, -1),                       # Jo Pair, 3 March 1979
        staff_row(1, 0, 0, (62, 1979), -1, 11, 2, 1, -1),                      # Jo Pair again, one day later
        staff_row(2, 1, 1, (328, 1975), 1, 11, 2, 2, -1),                      # Andy Evans, 1975
        staff_row(3, 1, 1, (90, 1978), 1, 11, 2, 3, -1),                       # Andy Evans, 1978: two people
        staff_row(4, 3, 3, (100, 1950), -1 if xbox else 0, 5, 1, -1, 0, fav=(5,)),   # Ann Boss, manager
        staff_row(5, 2, 2, (200, 1980), zed_club, 11, 2, 4, -1, joined=zed_joined, cend=D(2005, 6, 30)),
    ]


def build(path, xbox=False):
    st = people(xbox)
    pl = [player_row(i, 150 if xbox and i == 4 else 100, 160) for i in range(5)]
    staff_file = b"".join(st) + bytes(np_row(0, fav=(5,))) + b"".join(pl)
    squads = {0: [0], 1: [2, 3, 5]} if not xbox else {0: [0, 5], 1: [2, 3]}
    clubs = [club_row(0, "Florentia Viola" if xbox else "Fiorentina", 1, squads[0], -1 if xbox else 4),
             club_row(1, "Second Town", 2 if xbox else 1, squads[1])]
    comps = [comp_row(0, "Cup"), comp_row(1, "Top League"), comp_row(2, "Second League")]
    hist = b"".join(struct.pack("<iiHiBBB", n, s, 2000 + n, 0, 0, 10, 2) for n, s in ((0, 1), (1, 5)))
    award = bytearray(58)
    struct.pack_into("<i", award, 0x0a + 8, 5)
    struct.pack_into("<i", award, 0x0a + 24, -1)
    struct.pack_into("<i", award, 0x0a + 40, -1)
    files = {"nation.dat": nation_row(0, "Italy"), "club.dat": b"".join(clubs), "nat_club.dat": b"",
             "first_names.dat": b"".join(name_row(n, i) for i, n in enumerate(FIRST)),
             "second_names.dat": b"".join(name_row(n, i) for i, n in enumerate(SECOND)),
             "common_names.dat": b"", "staff.dat": staff_file, "club_comp.dat": b"".join(comps),
             "staff_history.dat": hist, "staff_comp_history.dat": bytes(award),
             "club_comp_history.dat": b"", "nation_comp_history.dat": b""}
    n = len(st)
    entries = [(4, "nation.dat", 1, 0, 2), (0, "club.dat", 2, 0, 2), (1, "nat_club.dat", 0, 0, 2),
               (13, "first_names.dat", 4, 0, 1), (14, "second_names.dat", 4, 0, 1),
               (15, "common_names.dat", 0, 0, 1), (6, "staff.dat", n, 0, 1), (9, "staff.dat", 1, n * 157, 2),
               (10, "staff.dat", 5, n * 157 + 68, 2), (12, "club_comp.dat", 3, 0, 2),
               (17, "staff_history.dat", 2, 0, 1), (18, "staff_comp_history.dat", 1, 0, 1),
               (19, "club_comp_history.dat", 0, 0, 1), (20, "nation_comp_history.dat", 0, 0, 1)]
    ib = bytearray(8)
    for tid, f, cnt, off, ver in entries:
        ib += cs(f, 51) + struct.pack("<iiii", tid, cnt, off, ver)
    files["index.dat"] = bytes(ib)
    os.makedirs(path)
    for f, b in files.items():
        with open(os.path.join(path, f), "wb") as h:
            h.write(b)


def write_changes(path):
    """A change list with one move (Zed Last to club 0), one rating change and the two answers of kinds 5, 6."""
    os.makedirs(path)
    move = dict(person="Zed Last", born="-", id_3968=5, id_xbox=5, club_3968="Second Town", club_xbox="Fiorentina",
                new_club_id=0, joined_3968="-", joined_xbox="-", cend_3968="-", cend_xbox="-", job_3968="-",
                job_xbox="-", summer_2002="yes", take_job="", rule="clear match", decision="applied")
    release = dict(move, person="Ann Boss", id_3968=4, id_xbox=4, new_club_id=-1, take_job="yes")
    tables = {
        "1-moves": [move], "2-releases": [release], "3-dates": [],
        "4-ratings": [dict(person="Jo Pair", id_3968=0, id_xbox=0, ca_xbox=100, pa_xbox=160, decision="held"),
                      dict(person="Zed Last", id_3968=5, id_xbox=5, ca_xbox=150, pa_xbox=160, decision="applied")],
        "5-leagues": [dict(club="Second Town", id_3968=1, new_division_id=2, division_3968="Top League",
                           division_xbox="Second League", decision="applied")],
        "6-names": [dict(item="club name", id_3968=0, id_xbox=0, value_3968="Fiorentina / Fiorentina",
                         value_xbox="Florentia Viola / Florentia Viola", decision="applied")]}
    for k, rows in tables.items():
        with open(os.path.join(path, k + ".tsv"), "w", newline="", encoding="utf-8") as f:
            fields = list(rows[0]) if rows else ["decision"]
            w = csv.DictWriter(f, fields, delimiter="\t", lineterminator="\n")
            w.writeheader()
            w.writerows(rows)


def i32(r, o):
    return struct.unpack_from("<i", r, o)[0]


def stops(fn):
    try:
        fn()
    except SystemExit:
        return True
    return False


def writer_cases(t):
    bad = []
    r = bytearray(10)
    if not stops(lambda: cmdb1.put(r, "<i", 8, 1)) or not stops(lambda: cmdb1.put(r, "<i", -1, 1)):
        bad.append(("put outside the row",))
    if not stops(lambda: cmdb1.put_text(r, 0, 4, "abcd")) or not stops(lambda: cmdb1.put_bytes(r, 8, b"xyz")):
        bad.append(("text or bytes outside the field",))
    cmdb1.put_text(r, 0, 6, "ab")
    if bytes(r[:6]) != b"ab\0\0\0\0":
        bad.append(("put_text", bytes(r)))
    for d in (D(1977, 8, 17), D(2002, 1, 1), D(2002, 12, 31), D(2004, 6, 30)):
        if cmdb1.to_date(*cmdb1.from_date(d)) != d:
            bad.append(("date round trip", d, cmdb1.from_date(d)))
    if cmdb1.from_date(None) != (0, 1900) or cmdb1.from_date(D(2004, 2, 29)) != cmdb1.from_date(D(2004, 2, 28)):
        bad.append(("no date or leap day", cmdb1.from_date(None)))
    build(os.path.join(t, "w"))
    F = cmdb1.open_folder(os.path.join(t, "w"))
    if cmdb1.staff_format(os.path.join(t, "w")) != 1 or not stops(lambda: cmdb1.write_folder(F, os.path.join(t, "w"))):
        bad.append(("staff_format, or write_folder over a folder",))
    if not stops(lambda: cmdb1.write_folder(F, os.path.join(t, "w2"), {"club.dat": 1})):
        bad.append(("write_folder with a changed length",))
    cmdb1.write_folder(F, os.path.join(t, "w3"))
    for f in os.listdir(os.path.join(t, "w")):
        if open(os.path.join(t, "w", f), "rb").read() != open(os.path.join(t, "w3", f), "rb").read():
            bad.append(("write_folder changed", f))
    return bad


def build_cases(t):
    """The steps of build_2002 on the synthetic template and Xbox copy."""
    bad = []
    build(os.path.join(t, "tpl"))
    build(os.path.join(t, "xbox"), xbox=True)
    write_changes(os.path.join(t, "changes"))
    F, X = cmdb1.open_folder(os.path.join(t, "tpl")), cmdb1.open_folder(os.path.join(t, "xbox"))
    log = []
    changed = b2.apply_staff(F, X, os.path.join(t, "changes"), log)
    b2.apply_ratings(F, X, os.path.join(t, "changes"), log)
    b2.fix_lists(F, changed, log)
    b2.apply_divisions(F, os.path.join(t, "changes"), log)
    b2.apply_names(F, X, os.path.join(t, "changes"), log)
    st, cl, pl = F.rows(6), F.rows(0), F.rows(10)
    if changed != {5: 1, 4: 0} or i32(st[5], 0x39) != 0 or i32(st[4], 0x39) != -1:
        bad.append(("staff clubs", changed, i32(st[5], 0x39), i32(st[4], 0x39)))
    if cmdb1.date_at(st[5], 0x3e) != D(2002, 7, 15) or cmdb1.date_at(st[5], 0x46) != D(2005, 6, 30):
        bad.append(("dates", cmdb1.date_at(st[5], 0x3e)))
    if [i32(cl[0], SQUAD + 4 * j) for j in range(3)] != [0, 5, -1] or i32(cl[0], MANAGER) != -1:
        bad.append(("club 0 lists", [i32(cl[0], SQUAD + 4 * j) for j in range(3)], i32(cl[0], MANAGER)))
    if [i32(cl[1], SQUAD + 4 * j) for j in range(3)] != [2, 3, -1]:
        bad.append(("club 1 squad", [i32(cl[1], SQUAD + 4 * j) for j in range(3)]))
    if struct.unpack_from("<h", pl[4], 5)[0] != 150 or struct.unpack_from("<h", pl[0], 5)[0] != 100:
        bad.append(("ratings",))
    if i32(cl[1], 0x57) != 2 or cmdb1.cmdb.cstr(cl[0], 4, 51) != "Florentia Viola":
        bad.append(("division or name", i32(cl[1], 0x57), cmdb1.cmdb.cstr(cl[0], 4, 51)))
    # A change list for other data, and a move into a full list, stop the build.
    G = cmdb1.open_folder(os.path.join(t, "tpl"))
    struct.pack_into("<i", G.rows(6)[5], 4, 0)                       # staff 5 is now "Jo Last"
    if not stops(lambda: b2.apply_staff(G, X, os.path.join(t, "changes"), [])):
        bad.append(("no stop for a change list of other data",))
    H = cmdb1.open_folder(os.path.join(t, "tpl"))
    for j in range(50):
        struct.pack_into("<i", H.rows(0)[0], SQUAD + 4 * j, 0)
    if not stops(lambda: b2.fix_lists(H, {5: 1}, [])):
        bad.append(("no stop for a full squad list",))
    return bad, F


GROUP = "Turkish 2. Division Category B Group "


def group_folder(path, xbox):
    """A copy of the synthetic template with a colour and a continent table and the three free rows of build step 6
    (ids 3 to 5); the Xbox copy has them too, then the three group rows (ids 6 to 8)."""
    build(path, xbox)
    files = {"colour.dat": struct.pack("<i", 0) + cs("White", 54),
             "continent.dat": struct.pack("<i", 0) + cs("Europe", 194)}
    comps = open(os.path.join(path, "club_comp.dat"), "rb").read()
    comps += b"".join(bytes(comp_row(3 + k, name)) for k, name in enumerate(b2.FREE_ROWS))
    if xbox:
        comps += b"".join(bytes(comp_row(6 + k, GROUP + c)) for k, c in enumerate("ABC"))
    files["club_comp.dat"] = comps
    ib = bytearray(open(os.path.join(path, "index.dat"), "rb").read())
    for i in range(8, len(ib), 67):
        if struct.unpack_from("<i", ib, i + 51)[0] == 12:
            struct.pack_into("<i", ib, i + 55, len(comps) // 107)
    ib += cs("colour.dat", 51) + struct.pack("<iiii", 2, 1, 0, 1)
    ib += cs("continent.dat", 51) + struct.pack("<iiii", 3, 1, 0, 1)
    files["index.dat"] = bytes(ib)
    for f, data in files.items():
        with open(os.path.join(path, f), "wb") as h:
            h.write(data)
    if not xbox:                                    # 3.9.68 leaves the group tag of these clubs at -2
        club = bytearray(open(os.path.join(path, "club.dat"), "rb").read())
        for i in range(len(club) // 581):
            struct.pack_into("<i", club, 581 * i + 0x60, -2)
        open(os.path.join(path, "club.dat"), "wb").write(bytes(club))


def group_cases(t):
    """Step 6 (slice 5, Q43): the three Xbox group rows fill the free rows with their ids, no row is added, the member
    clubs get their group tag and their divisions stay; a club that has a tag, or a free row that a club points at,
    stops it; --no-turkish-groups leaves it out."""
    bad = []
    group_folder(os.path.join(t, "g-tpl"), False)
    group_folder(os.path.join(t, "g-xbox"), True)
    ch = os.path.join(t, "g-changes")
    os.makedirs(ch)
    rows = [dict(item="competition", id_3968=-1, id_xbox=6 + k, value_3968="-", value_xbox=f"{GROUP}{c} (1 clubs)",
                 rule="Q29", decision="applied") for k, c in enumerate("ABC")]
    rows += [dict(item=f"member of {GROUP}A", id_3968=0, id_xbox=0, value_3968="-", value_xbox="Fiorentina",
                  rule="Q29", decision="applied"),
             dict(item=f"member of {GROUP}B", id_3968=1, id_xbox=1, value_3968="-", value_xbox="Second Town",
                  rule="Q29", decision="applied")]
    with open(os.path.join(ch, "6-names.tsv"), "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, list(rows[0]), delimiter="\t", lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    F, X = cmdb1.open_folder(os.path.join(t, "g-tpl")), cmdb1.open_folder(os.path.join(t, "g-xbox"))
    ids = b2.apply_groups(F, X, ch, False, [])
    comps = {i32(r, 0): cmdb1.cmdb.cstr(r, 4, 51) for r in F.rows(12)}
    if ids != [3, 4, 5] or F.idx[12]["count"] != 6 or [comps.get(i) for i in (3, 4, 5)] != [GROUP + c for c in "ABC"]:
        bad.append(("group rows", ids, comps))
    cl = F.rows(0)
    if [i32(cl[0], 0x60), i32(cl[1], 0x60)] != [3, 4] or [i32(cl[0], 0x57), i32(cl[1], 0x57)] != [1, 1]:
        bad.append(("group tags", [i32(c, 0x60) for c in cl], [i32(c, 0x57) for c in cl]))
    G = cmdb1.open_folder(os.path.join(t, "g-tpl"))
    struct.pack_into("<i", G.rows(0)[1], 0x60, 2)                    # club 1 already has a group
    if not stops(lambda: b2.apply_groups(G, X, ch, False, [])):
        bad.append(("no stop for a club with a group tag",))
    J = cmdb1.open_folder(os.path.join(t, "g-tpl"))
    struct.pack_into("<i", J.rows(0)[1], 0x5b, 4)                    # club 1's last division is a free row
    if not stops(lambda: b2.apply_groups(J, X, ch, False, [])):
        bad.append(("no stop for a free row with a reference",))
    H = cmdb1.open_folder(os.path.join(t, "g-tpl"))
    if b2.apply_groups(H, X, ch, True, []) != [] or H.idx[12]["count"] != 6:
        bad.append(("--no-turkish-groups changed the data",))
    return bad


def merge_cases(t, F):
    """duplicates.merge_folder on format 1: Jo Pair merges, the two Andy Evans stay, Zed Last (the last row) takes
    the dropped place, and every reference to him follows."""
    bad = []
    report = os.path.join(t, "merge.tsv")
    n, cur, kept = duplicates.merge_folder(F, 6, 6, report, duplicates.kept_apart)
    st, cl = F.rows(6), F.rows(0)
    if n != 1 or cur != {0: 0, 1: 0, 2: 2, 3: 3, 4: 4, 5: 1} or [(a, b) for a, b, _ in kept] != [(2, 3)]:
        bad.append(("merge result", n, cur, kept))
    if F.idx[6]["count"] != 5 or any(i32(r, 0) != k for k, r in enumerate(st)):
        bad.append(("staff ids are not row numbers", [i32(r, 0) for r in st]))
    if i32(st[1], 4) != 2 or i32(st[1], 0x39) != 0:
        bad.append(("the last row did not move into the dropped place", i32(st[1], 4)))
    if [i32(cl[0], SQUAD + 4 * j) for j in range(3)] != [0, 1, -1]:
        bad.append(("squad list after the merge", [i32(cl[0], SQUAD + 4 * j) for j in range(3)]))
    if i32(st[4], 0x79) != 1 or i32(F.rows(9)[0], 0x23) != 1 or i32(F.rows(18)[0], 0x0a + 8) != 1:
        bad.append(("favourites or award", i32(st[4], 0x79), i32(F.rows(9)[0], 0x23), i32(F.rows(18)[0], 0x12)))
    hist = [i32(r, 4) for r in F.rows(17)]
    if hist != [0, 1]:                                  # Jo's row goes to the keeper 0, Zed's follows him to 1
        bad.append(("careers", hist))
    out = os.path.join(t, "merged")
    cmdb1.write_folder(F, out)
    kept_tsv = os.path.join(t, "kept.tsv")
    b2.write_kept(kept_tsv, cmdb1.open_folder(out), kept)
    saved = sys.stdout
    sys.stdout = open(os.devnull, "w")
    try:
        ok, without = duplicates.check(out, kept_tsv), duplicates.check(out)
        with open(os.path.join(t, "stale.tsv"), "w") as h:
            h.write("0\t4\n2\t3\n")
        stale = duplicates.check(out, os.path.join(t, "stale.tsv"))
    finally:
        sys.stdout.close()
        sys.stdout = saved
    if (ok, without, stale) != (0, 1, 1):
        bad.append(("check with the kept list, without it, with a stale pair", ok, without, stale))
    return bad


def build2(path):
    """A format 2 folder (110-byte staff rows, preferences table 22), for the merge path of the 2004/05 and 2006/07
    builds: Jo Pair three times, Zed Last twice, the two Andy Evans (merged here, as format 2 keeps no pair apart)."""
    def staff(i, f, sn, dob, club, job, flags, pl, pref, npl):
        r = bytearray(110)
        struct.pack_into("<iiii", r, 0, i, f, sn, -1)
        struct.pack_into("<hH", r, 0x10, *dob)
        struct.pack_into("<ii", r, 0x1a, 0, 0)
        struct.pack_into("<i", r, 0x24, -1)
        struct.pack_into("<i", r, 0x39, club)
        r[0x3d], r[0x5f] = job, flags
        struct.pack_into("<iii", r, 0x61, pl, pref, npl)
        return r

    def pref(i, fav=()):
        r = bytearray(52)
        struct.pack_into("<i", r, 0, i)
        for j in range(6):
            struct.pack_into("<i", r, 4 + 4 * j, -1)
            struct.pack_into("<i", r, 0x1c + 4 * j, fav[j] if j < len(fav) else -1)
        return r
    st = [staff(0, 0, 0, (61, 1979), 0, 11, 2, 0, 0, -1), staff(1, 0, 0, (62, 1979), -1, 11, 2, 1, 1, -1),
          staff(2, 1, 1, (328, 1975), 1, 11, 2, 2, 2, -1), staff(3, 1, 1, (90, 1978), 1, 11, 2, 3, 3, -1),
          staff(4, 3, 3, (100, 1950), 0, 5, 1, -1, 4, 0), staff(5, 2, 2, (200, 1980), 1, 11, 2, 4, 5, -1),
          staff(6, 0, 0, (61, 1979), 1, 11, 2, 5, 6, -1), staff(7, 2, 2, (201, 1980), -1, 11, 2, 6, 7, -1)]
    prefs = [pref(i, (7,) if i == 4 else ()) for i in range(8)]
    npr = np_row(0, fav=(7,))
    clubs = [club_row(0, "Home", 1, [0], 4), club_row(1, "Away", 1, [2, 3, 5, 6])]
    hist = b"".join(struct.pack("<iiHiBBB", n, sn, 2000 + n, 0, 0, 1, 0) for n, sn in ((0, 1), (1, 7), (2, 6)))
    award = bytearray(58)
    struct.pack_into("<i", award, 0x0a + 8, 7)
    struct.pack_into("<i", award, 0x0a + 24, -1)
    struct.pack_into("<i", award, 0x0a + 40, -1)
    files = {"nation.dat": nation_row(0, "X"), "club.dat": b"".join(clubs), "nat_club.dat": b"",
             "first_names.dat": b"".join(name_row(n, i) for i, n in enumerate(FIRST)),
             "second_names.dat": b"".join(name_row(n, i) for i, n in enumerate(SECOND)), "common_names.dat": b"",
             "staff.dat": b"".join(st) + bytes(npr) + bytes(70 * 7) + b"".join(prefs),
             "staff_history.dat": hist, "staff_comp_history.dat": bytes(award)}
    n = 8
    entries = [(4, "nation.dat", 1, 0, 2), (0, "club.dat", 2, 0, 2), (1, "nat_club.dat", 0, 0, 2),
               (13, "first_names.dat", 4, 0, 1), (14, "second_names.dat", 4, 0, 1), (15, "common_names.dat", 0, 0, 1),
               (6, "staff.dat", n, 0, 2), (9, "staff.dat", 1, n * 110, 2), (10, "staff.dat", 7, n * 110 + 68, 2),
               (22, "staff.dat", 8, n * 110 + 68 + 490, 1), (17, "staff_history.dat", 3, 0, 1),
               (18, "staff_comp_history.dat", 1, 0, 1)]
    ib = bytearray(8)
    for tid, f, cnt, off, ver in entries:
        ib += cs(f, 51) + struct.pack("<iiii", tid, cnt, off, ver)
    files["index.dat"] = bytes(ib)
    os.makedirs(path)
    for f, b in files.items():
        with open(os.path.join(path, f), "wb") as h:
            h.write(b)


def format2_cases(t):
    """The format 2 merge (duplicates.merge) gives what it gave before the format 1 work (checked once against the
    old code): four rows merged, ids equal to row numbers, and the favourites, award and careers of Zed's second
    row (7, the last row) follow him to his keeper 5."""
    bad = []
    db = os.path.join(t, "f2")
    build2(db)
    saved = sys.stdout
    sys.stdout = open(os.devnull, "w")
    try:
        n = duplicates.merge(db, db, db, os.path.join(t, "f2.tsv"))
    finally:
        sys.stdout.close()
        sys.stdout = saved
    F = duplicates.Folder(db)
    st, prefs = F.rows(6), F.rows(22)
    if n != 4 or F.idx[6]["count"] != 4 or [i32(r, 0) for r in st] != [0, 1, 2, 3]:
        bad.append(("format 2 merge", n, [i32(r, 0) for r in st]))
    if [i32(r, 0x65) for r in st] != [0, 1, 2, 3] or F.idx[22]["count"] != 4:
        bad.append(("format 2 preferences rows", [i32(r, 0x65) for r in st], F.idx[22]["count"]))
    names = [(i32(r, 4), i32(r, 0x39)) for r in st]
    zed = names.index((2, 1))
    ann = names.index((3, 0))
    if i32(prefs[ann], 0x1c) != zed or i32(F.rows(9)[0], 0x23) != zed or i32(F.rows(18)[0], 0x12) != zed:
        bad.append(("format 2 references", names, i32(prefs[ann], 0x1c), i32(F.rows(18)[0], 0x12)))
    if sorted(i32(r, 4) for r in F.rows(17)) != sorted([0, zed]):
        bad.append(("format 2 careers", [i32(r, 4) for r in F.rows(17)]))
    return bad


def main():
    bad = []
    with tempfile.TemporaryDirectory() as t:
        bad += writer_cases(t)
        more, F = build_cases(t)
        bad += more
        bad += merge_cases(t, F)
        bad += group_cases(t)
        bad += format2_cases(t)
    for x in bad:
        print("FAIL", x)
    print("PASS" if not bad else f"FAILED: {len(bad)}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
