#!/usr/bin/env python3
"""A crash while the season part goes into a save leaves the old save whole (0.6.1 item 10).

Usage:
    python3 tests/seasons/save_crash_run.py <cm0102_recomp> <game folder> --keep <folder> [--days 3]

In 0.6.0 the runtime added the season part (season.c add_part) after the original's save had deleted the old
file and renamed temp_save_file.sav to its name. A crash in that step (on Linux, the 4-byte overrun of items 4
and 10 made glibc stop the program in free()) left only a save of renamed records without its part, and its load
stopped at the box "index.cpp 5809". The player's autosave of July 2002 was such a file.

The steps, in a 2001/02 game of England with changing rules on (a season game, so every save has a part):
    1. a new game, saved after --days days as samesave.sav (testrun.c), then the program ends
    2. a new program loads that save and saves again after --days days under the same name, with
       RECOMP_TEST_PART_CRASH=1: the program stops (abort) inside add_part
    3. the file samesave.sav must still be the save of step 1, byte for byte, with its season part
    4. a new program loads samesave.sav: no message box (the 0.6.0 build shows "index.cpp 5809" here when the
       game holds renamed records), and the game goes on to a human turn
    5. the crash left temp_save_file.sav: the renamed records with no season part, as the player's file. A new
       program loads it: the load refuses it with the box "This save is incomplete. Load an earlier save." and
       never shows "index.cpp 5809"
The pass rule: all five steps as above. The run folder holds one folder per step with its stderr.txt.
"""
import filecmp
import os
import shutil
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import soak_run  # noqa: E402

PART = b"bosman_season.dat"


def has_part(path):
    """1 when the save container holds a row of the season part (season.c, the layout at its top)."""
    with open(path, "rb") as f:
        head = f.read(12 + 1000 * 0x10c)
    count = struct.unpack_from("<I", head, 8)[0]
    return any(head[12 + k * 0x10c + 8:12 + k * 0x10c + 8 + len(PART) + 1] == PART + b"\0" for k in range(count))


def main(argv):
    if len(argv) < 3 or "--keep" not in argv:
        print(__doc__)
        return 2
    o = {"program": os.path.abspath(argv[1]), "game": os.path.abspath(argv[2]), "start": 2001, "last": 2001,
         "seed": 1, "nations": ["England"], "database": None, "rules": "on", "timeout": 1800, "extra": []}
    keep = os.path.abspath(soak_run.arg(argv, "--keep"))
    days = int(soak_run.arg(argv, "--days", 3))
    if os.path.exists(keep):
        print(f"save_crash_run.py: {keep} exists; use a new --keep folder")
        return 2
    os.makedirs(keep)
    fails = []

    code, took, box = soak_run.new_game_leg(o, os.path.join(keep, "1-new"), days)
    first = os.path.join(keep, "1-new", "user", "samesave.sav")
    print(f"1 new game: code {code}, {took:.0f} s, box {box}")
    if code != 0 or box or not os.path.exists(first) or not has_part(first):
        print("FAIL  step 1 made no save with a season part")
        return 1
    kept = os.path.join(keep, "first.sav")
    shutil.copyfile(first, kept)

    # Step 2: soak_run.load_leg copies the save in as soak.sav; the test save writes samesave.sav, so the save
    # of step 1 goes in under that name too and the second save replaces it.
    work = os.path.join(keep, "2-crash")
    os.makedirs(os.path.join(work, "user"))
    shutil.copyfile(kept, os.path.join(work, "user", "samesave.sav"))
    os.environ["RECOMP_TEST_PART_CRASH"] = "1"
    try:
        code, took, box = soak_run.load_leg(o, work, kept, days)
    finally:
        del os.environ["RECOMP_TEST_PART_CRASH"]
    left = os.path.join(work, "user", "samesave.sav")
    print(f"2 load and save with the crash switch: code {code}, {took:.0f} s, box {box}")
    if code == 0:
        fails.append("step 2: the program did not stop in the part (the switch did not act)")
    if not os.path.exists(left):
        fails.append("step 3: samesave.sav is gone")
    elif not filecmp.cmp(left, kept, shallow=False):
        fails.append(f"step 3: samesave.sav is not the save of step 1 (season part {'in' if has_part(left) else 'missing'})")

    if os.path.exists(left):
        code, took, box = soak_run.load_leg(o, os.path.join(keep, "4-load"), left, 1)
        print(f"4 load of samesave.sav: code {code}, {took:.0f} s, box {box}")
        if box or code != 0:
            fails.append(f"step 4: the load gives {box or 'code %s' % code}")
    temp = os.path.join(work, "user", "temp_save_file.sav")
    if not os.path.exists(temp) or has_part(temp):
        fails.append("step 5: the crash left no temp_save_file.sav without a season part")
    else:
        cut = os.path.join(keep, "cut.sav")
        shutil.copyfile(temp, cut)
        code, took, box = soak_run.load_leg(o, os.path.join(keep, "5-cut"), cut, 1)
        print(f"5 load of the file without its part: code {code}, {took:.0f} s, box {box}")
        if not box or "This save is incomplete" not in box or "5809" in box:
            fails.append(f"step 5: the load gives {box or 'code %s' % code}, not the box of an incomplete save")
    for f in fails:
        print("FAIL ", f)
    print("FAIL" if fails else "PASS")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
