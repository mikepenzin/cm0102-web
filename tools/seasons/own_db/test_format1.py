"""Cases for the format 1 reader (cmdb1.py, xbox0203/read_xbox0203.py) on a folder made here. usage: python3 -I test_format1.py

The folder is synthetic: two nations, two clubs, a player (Thierry Henry, 181/189), a manager and a bad copy of the
folder for each check that must stop the script. No game data is in the repo.
"""
import copy
import datetime
import os
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "xbox0203"))
import cmdb  # noqa: E402
import read_xbox0203 as rx  # noqa: E402


def cs(text, n):
    return text.encode("latin1").ljust(n, b"\0")


def name_row(text, i):
    r = bytearray(60)
    r[0:51] = cs(text, 51)
    struct.pack_into("<i", r, 0x33, i)
    return bytes(r)


def club_row(i, name, nat):
    r = bytearray(581)
    struct.pack_into("<i", r, 0, i)
    r[4:55] = cs(name, 51)
    r[0x38:0x52] = cs(name, 26)
    struct.pack_into("<i", r, 0x53, nat)
    return bytes(r)


def nation_row(i, name):
    r = bytearray(290)
    struct.pack_into("<i", r, 0, i)
    r[4:55] = cs(name, 51)
    r[0x38:0x52] = cs(name, 26)
    return bytes(r)


def staff_row(i, first, second, common, dob, nat, club, job, joined, cend, flags, pl, npl):
    r = bytearray(157)
    struct.pack_into("<iiii", r, 0, i, first, second, common)
    struct.pack_into("<hH", r, 0x10, *dob)
    struct.pack_into("<i", r, 0x1a, nat)
    struct.pack_into("<i", r, 0x39, club)
    r[0x3d] = job
    struct.pack_into("<hH", r, 0x3e, *joined)
    struct.pack_into("<hH", r, 0x46, *cend)
    r[0x5f] = flags
    struct.pack_into("<ii", r, 0x91, pl, 0)
    struct.pack_into("<i", r, 0x99, npl)
    return bytes(r)


def player_row(ca, pa):
    r = bytearray(70)
    struct.pack_into("<hh", r, 5, ca, pa)
    struct.pack_into("<hhh", r, 9, 100, 150, 190)
    r[0x0f:0x1b] = bytes(range(1, 13))
    r[0x1b:0x45] = bytes(range(100, 142))
    return bytes(r)


def hist_row(i, comp, year, clubs):
    r = bytearray(26)
    struct.pack_into("<iiH", r, 0, i, comp, year)
    struct.pack_into("<4i", r, 10, *clubs)
    return bytes(r)


def build(path, version=1, bad=None):
    """Write a format 1 folder. `bad` names one fault: club, name, flags, player, date, short, history."""
    names13, names14, names15 = [name_row("Thierry", 0), name_row("Alex", 1)], [name_row("Henry", 0), name_row(
        "Ferguson", 1)], [name_row("Titi", 0)]
    clubs0 = [club_row(0, "Arsenal", 0), club_row(1, "Manchester United", 0)]
    staff = [staff_row(0, 0, 0, 0, (228, 1977), 0, 99 if bad == "club" else 0, 11, (214, 1999), (180, 2004), 2, 0, -1),
             staff_row(1, 1, 1, -1, (364, 1941), 0, 1, 5, (287, 1986), (180, 2005), 1, -1, 0)]
    if bad == "name":
        staff[0] = staff_row(0, 9, 0, 0, (228, 1977), 0, 0, 11, (214, 1999), (180, 2004), 2, 0, -1)
    if bad == "flags":
        staff[1] = staff_row(1, 1, 1, -1, (364, 1941), 0, 1, 5, (287, 1986), (180, 2005), 0, -1, 0)
    if bad == "player":
        staff[0] = staff_row(0, 0, 0, 0, (228, 1977), 0, 0, 11, (214, 1999), (180, 2004), 2, 5, -1)
    if bad == "date":
        staff[0] = staff_row(0, 0, 0, 0, (900, 1977), 0, 0, 11, (214, 1999), (180, 2004), 2, 0, -1)
    staff_file = b"".join(staff) + b"".join([bytes(68)]) + player_row(181, 189)
    if bad == "short":
        staff_file = staff_file[:-30]
    ch = [hist_row(0, 7, 2002, (0, 1, -1, -1))]
    if bad == "history":
        ch = [hist_row(0, 7, 2002, (0, 1, -5, -1))]
    files = {"nation.dat": nation_row(0, "England"), "club.dat": b"".join(clubs0), "nat_club.dat": b"",
             "first_names.dat": b"".join(names13), "second_names.dat": b"".join(names14),
             "common_names.dat": b"".join(names15), "staff.dat": staff_file,
             "club_comp_history.dat": b"".join(ch), "nation_comp_history.dat": hist_row(0, 3, 2002, (0, -1, -1, -1))}
    entries = [  # table id, file, count, offset, version
        (4, "nation.dat", 1, 0, 2), (0, "club.dat", 2, 0, 2), (1, "nat_club.dat", 0, 0, 2),
        (13, "first_names.dat", 2, 0, 1), (14, "second_names.dat", 2, 0, 1), (15, "common_names.dat", 1, 0, 1),
        (6, "staff.dat", 2, 0, version), (9, "staff.dat", 1, 2 * 157, 2), (10, "staff.dat", 1, 2 * 157 + 68, 2),
        (19, "club_comp_history.dat", 1, 0, 1), (20, "nation_comp_history.dat", 1, 0, 1)]
    ib = bytearray(8)
    for tid, f, cnt, off, ver in entries:
        ib += cs(f, 51) + struct.pack("<iiii", tid, cnt, off, ver)
    files["index.dat"] = bytes(ib)
    os.makedirs(path)
    for f, b in files.items():
        with open(os.path.join(path, f), "wb") as h:
            h.write(b)


def stops(path):
    try:
        rx.facts(path)
    except SystemExit:
        return True
    return False


def main():
    bad = []
    with tempfile.TemporaryDirectory() as t:
        build(os.path.join(t, "ok"))
        out, dangling = rx.facts(os.path.join(t, "ok"))
        P, R = out["people_all"], out["ratings"]
        want = [
            (P[0]["fn"], P[0]["sn"], P[0]["cn"], P[0]["dob"], P[0]["club"], P[0]["job"], P[0]["role"]),
            (P[1]["sn"], P[1]["cn"], P[1]["dob"], P[1]["club"], P[1]["job"], P[1]["role"]),
        ]
        have = [
            ("Thierry", "Henry", "Titi", datetime.date(1977, 8, 17), 0, 1, "player"),
            ("Ferguson", None, datetime.date(1941, 12, 31), 1, 16, "staff"),
        ]
        bad += [(a, b) for a, b in zip(want, have) if a != b]
        if (R[0]["ca"], R[0]["pa"], R[0]["rep"]) != (181, 189, (100, 150, 190)) or list(R) != [0]:
            bad.append(("ratings", R))
        if R[0]["pos"]["Goalkeeper"] != 1 or R[0]["attrs"]["Acceleration"] != 100 or "Versatility" not in R[0]["attrs"]:
            bad.append(("positions or attributes", R[0]))
        if (P[0]["joined"], P[0]["cend"]) != (datetime.date(1999, 8, 3), datetime.date(2004, 6, 30)):
            bad.append(("dates", P[0]["joined"], P[0]["cend"]))
        if out["comphist"] != [(7, 2002, (0, 1, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff)),
                               (3, 2002, (0, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff))]:
            bad.append(("comphist", out["comphist"]))
        if [h[:4] for h in out["clubhist"]] != [(7, 0, 2002, 1), (7, 1, 2002, 2)] or out["teammap"] != {0: 0, 1: 1}:
            bad.append(("clubhist or teammap", out["clubhist"], out["teammap"]))
        if set(out["clubs"][0]) != {"long", "short", "id2", "off", "nxt", "nat"}:
            bad.append(("club keys", out["clubs"][0]))
        for fault in ("club", "name", "flags", "player", "date", "short", "history"):
            build(os.path.join(t, fault), bad=fault)
            if not stops(os.path.join(t, fault)):
                bad.append(("no stop for", fault))
        build(os.path.join(t, "v2"), version=2)
        if not stops(os.path.join(t, "v2")):
            bad.append(("no stop for", "staff table version 2"))
    # cmdb.py still refuses format 1 and still reads the table size 110 of format 2.
    if cmdb.REC[6] != 110:
        bad.append(("cmdb.REC[6] changed", cmdb.REC[6]))
    for x in bad:
        print("FAIL", x)
    print("PASS" if not bad else f"FAILED: {len(bad)}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
