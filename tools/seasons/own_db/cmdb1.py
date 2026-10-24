"""Read a CM 01/02 data folder with the format 1 staff table (index version 1, 157 bytes per row).

The 3.9.68 data and the Xbox game's data store the staff table in format 1. The Update Team databases store format
2 (110 bytes per row), which cmdb.py reads. cmdb.py refuses format 1 and stays as it is. Data only: the folder is
untrusted input, so every read is checked and a folder of another shape stops the script.

Format 1 staff row (found in the data and proved by the player links): ids of the first, second and common name at
4, 8 and 12; date of birth at 0x10; nation at 0x1a; club at 0x39; job at 0x3d; joined at 0x3e; contract end at
0x46; flags at 0x5f (bit 2 player, bit 1 non-player); row id in the player table (10) at 0x91; row id in the
non-player table (9) at 0x99. Player row (70 bytes): current and potential ability as i16 at 5 and 7, three
reputations as i16 at 9, 12 position bytes at 0x0f, 42 attribute bytes from 0x1b.
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


class Folder1(cmdb.Folder):
    """cmdb.Folder for a folder whose staff table is in format 1. Same rows() and table() calls."""

    def __init__(self, path):
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
