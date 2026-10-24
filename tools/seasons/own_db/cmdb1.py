"""Read a CM 01/02 data folder with the format 1 staff table (index version 1, 157 bytes per row).

The 3.9.68 data and the Xbox game's data store the staff table in format 1. The Update Team databases store format
2 (110 bytes per row), which cmdb.py reads. cmdb.py refuses format 1 and stays as it is. Data only: the folder is
untrusted input, so every read is checked and a folder of another shape stops the script.

Format 1 staff row (found in the data and proved by the player links): ids of the first, second and common name at
4, 8 and 12; date of birth at 0x10; nation at 0x1a; club at 0x39; job at 0x3d; joined at 0x3e; contract end at
0x46; flags at 0x5f (2 player, 1 non-player, 3 both); row id in the player table (10) at 0x91; row id in the
non-player table (9) at 0x99. Player row (70 bytes): current and potential ability as i16 at 5 and 7, three
reputations as i16 at 9, 12 position bytes at 0x0f, 42 attribute bytes from 0x1b. The game's load (convert_staff
in src/cm3/database.c, 0x52caa1) also reads the national team at 0x24 and the preferences inline: six club ids at
0x61 and six staff ids (favourite people) at 0x79. Format 1 has no preferences table (22).

The writer (put, put_date, write_folder) changes rows in place with the same bounds checks. It adds no row and
changes no count: a change that needs a new row stops the script.
"""
import datetime
import os
import struct

import cmdb

STAFF_SIZE = 157
PLAYER_SIZE = 70
CUM = [0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334]   # CM day of year: the common-year table
NO_DATE_YEAR = 1900       # a year of 1900 or less means no date


def need(cond, msg):
    if not cond:
        raise SystemExit("cmdb1: " + msg)


def u(fmt, r, o):
    """struct.unpack_from with a bounds check (a negative offset would read from the end)."""
    need(0 <= o and o + struct.calcsize(fmt) <= len(r), f"read {fmt} at {o:#x} is outside the row")
    return struct.unpack_from(fmt, r, o)


def to_date(day, year):
    """datetime.date of a CM date (day of year from 0 in the common-year table, year), or None for no date."""
    if year <= NO_DATE_YEAR:
        return None
    need(0 <= day <= 365 and year <= 9999, f"date with day {day} and year {year} is not a date")
    day = min(day, 364)                 # the data has day 365 (a leap year's last day) as 31 December
    m = max(i for i in range(12) if CUM[i] <= day)
    return datetime.date(year, m + 1, day - CUM[m] + 1)


def date_at(r, o):
    day, year = u("<hH", r, o)
    return to_date(day, year)


def from_date(d):
    """(day, year) of a datetime.date in the common-year table, the reverse of to_date. None gives no date (day 0,
    year 1900). 29 February is 28 February, as convert.cm_date writes it: the table has no leap day."""
    if d is None:
        return 0, NO_DATE_YEAR
    need(NO_DATE_YEAR < d.year <= 9999, f"{d} is outside the years of the data")
    return CUM[d.month - 1] + min(d.day, 28 if d.month == 2 else 31) - 1, d.year


def put(r, fmt, o, *vals):
    """struct.pack_into with the bounds check of u()."""
    need(0 <= o and o + struct.calcsize(fmt) <= len(r), f"write {fmt} at {o:#x} is outside the row")
    struct.pack_into(fmt, r, o, *vals)


def put_date(r, o, d):
    """Writes the day and year of d at o (the 4 bytes after them stay)."""
    put(r, "<hH", o, *from_date(d))


def put_bytes(r, o, b):
    need(0 <= o and o + len(b) <= len(r), f"write of {len(b)} bytes at {o:#x} is outside the row")
    r[o:o + len(b)] = b


def put_text(r, o, n, text):
    """A text field of n bytes: the Latin-1 text, a 0 and zeros to the end. The text must fit with its 0."""
    b = text.encode("latin1")
    need(len(b) < n, f"{text!r} does not fit in {n} bytes")
    put_bytes(r, o, b + bytes(n - len(b)))


def staff_view(F):
    """cmdb.staff_view for a format 1 folder: the same dict per staff row. np is the player row (0x91) as in
    cmdb.staff_view (whose np is format 2's player link at 0x61), -1 for a row with no player data."""
    fn, sn, cn = cmdb.names(F, 13), cmdb.names(F, 14), cmdb.names(F, 15)
    clubs = {u("<i", r, 0)[0]: cmdb.cstr(r, 4, 51) for t in (0, 1) for r in F.rows(t)}
    out = []
    for r in F.rows(6):
        i, f, s, c = u("<iiii", r, 0)
        out.append(dict(id=i, first=fn.get(f, ""), second=sn.get(s, ""), common=cn.get(c, "") if c > 0 else "",
                        dob=u("<hH", r, 0x10), nation=u("<i", r, 0x1a)[0], club=u("<i", r, 0x39)[0],
                        joined=u("<hH", r, 0x3e), cend=u("<hH", r, 0x46), kind=r[0x5f],
                        np=u("<i", r, 0x91)[0] if r[0x5f] & 2 else -1,
                        pl=u("<i", r, 0x99)[0] if r[0x5f] & 1 else -1))
    return out, clubs


class Folder1(cmdb.Folder):
    """cmdb.Folder for a folder whose staff table is in format 1. Same rows() and table() calls."""

    def __init__(self, path):
        need(os.path.isfile(os.path.join(path, "index.dat")), f"{path}: there is no index.dat")
        super().__init__(path)
        ib = self.raw.get("index.dat")
        need(ib is not None and len(ib) >= 8 and (len(ib) - 8) % 67 == 0,
             f"{path}: index.dat is not a header of 8 bytes and entries of 67 bytes")
        need(6 in self.idx, f"{path}: index.dat has no staff table")
        need(self.idx[6]["ver"] == 1, f"{path}: staff table has version {self.idx[6]['ver']}, not 1")

    def table(self, tid):
        """The table's file, offset, row size and count, checked against the file length."""
        need(tid in self.idx, f"{self.path}: index.dat has no table {tid}")
        e = self.idx[tid]
        size = STAFF_SIZE if tid == 6 else cmdb.REC[tid]
        need(e["file"] in self.raw, f"{self.path}: index.dat names {e['file']!r}, which the folder lacks")
        b = self.raw[e["file"]]
        need(e["count"] >= 0 and e["off"] >= 0 and e["off"] + e["count"] * size <= len(b),
             f"{self.path}: table {tid} ({e['count']} rows of {size} bytes at {e['off']}) runs past the end of "
             f"{e['file']}")
        return b, e["off"], size, e["count"]


def open_folder(path):
    need(os.path.isdir(path), f"{path} is not a folder")
    return Folder1(path)


def staff_format(path):
    """1 or 2: the version of the staff table in index.dat of a data folder."""
    ib = open(os.path.join(path, "index.dat"), "rb").read()
    need(len(ib) >= 8 and (len(ib) - 8) % 67 == 0, f"{path}: index.dat is not a header of 8 bytes and entries of 67")
    for i in range(8, len(ib), 67):
        tid, _, _, ver = struct.unpack_from("<iiii", ib, i + 51)
        if tid == 6:
            need(ver in (1, 2), f"{path}: staff table has version {ver}")
            return ver
    raise SystemExit(f"cmdb1: {path}: index.dat has no staff table")


def write_folder(F, path, sizes=None):
    """Writes every file of F into the new folder path. sizes {file: length} are the lengths each file must keep."""
    need(not os.path.exists(path), f"{path} exists: the out folder must be new")
    for f, b in (sizes or {}).items():
        need(len(F.raw[f]) == b, f"{f} changed its length from {b} to {len(F.raw[f])}")
    os.makedirs(path)
    for f in sorted(F.raw):
        with open(os.path.join(path, f), "wb") as h:
            h.write(F.raw[f])
