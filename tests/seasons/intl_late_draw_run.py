#!/usr/bin/env python3
"""A finals draw that lies before the new game's day (0.6.2 open item 19, src/recomp_rt/win/season_intl.c late_draw).

Usage:
    python3 tests/seasons/intl_late_draw_run.py <cm0102_recomp> <game folder> [--keep <folder>]

The run makes a 2002/03 game (the own database Summer 2002, all nations, seed 1, changing rules on, Tier A) and plays
40 days from its start date (13 July 2002). The OFC Nations Cup of 2002 plays from 5 July 2002; the table moves the
class's first group day (10 November) by -128 days and its draw (5 March) with it, to 28 October 2001, before the
game is made. The class's update draws on the exact day of its draw date, so the draw never came, the cup never ended
and the next edition came in 2016 (run set pass 2, a2002s1). The database: own-dbs/Summer 2002.zip, unpacked in the
keep folder.

The checks: the program ends with code 0 and shows no message box; the save holds a 2002 row of the OFC Nations Cup
with a winner and a runner-up.
"""
import os
import struct
import sys
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools", "save"))
import soak_run  # noqa: E402
import savfile  # noqa: E402

OWN_ZIP = "/Users/max/Projects/cm0102/own-dbs/Summer 2002.zip"
DAYS = 40
HIST = 26                               # nation_comp_history.dat rows, as tests/seasons/cup_third_place_run.py reads


def rows(save, name):
    sav = savfile.Sav(save)
    comps = sav.read_member("nation_comp.dat")
    ids = set()
    for i in range(len(comps) // 0x6b):
        rec = comps[i * 0x6b:(i + 1) * 0x6b]
        if rec[4:4 + 51].split(b"\0")[0].decode("latin-1").startswith(name):     # the data's name or the renamed one
            ids.add(struct.unpack_from("<i", rec, 0)[0])
    data = sav.read_member("nation_comp_history.dat")
    out = []
    for i in range(len(data) // HIST):
        _, cid, year, a, b, c, _ = struct.unpack_from("<iiHiiii", data, i * HIST)
        if cid in ids:
            out.append((year, a, b))
    return out


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-late-draw-")
    if not os.path.exists(OWN_ZIP):
        print(f"FAIL  missing: {OWN_ZIP}")
        return 1
    database = os.path.join(keep, "owndb", "Summer 2002")
    if not os.path.isdir(database):
        with zipfile.ZipFile(OWN_ZIP) as z:
            z.extractall(os.path.join(keep, "owndb"))
    os.environ["RECOMP_TIER_A"] = "1"
    o = {"program": program, "game": game, "rules": "on", "database": database, "seed": 1, "start": 2002,
         "last": 2012, "nations": ["all"], "extra": [], "timeout": 3600}
    code, took, box = soak_run.new_game_leg(o, os.path.join(keep, "leg"), DAYS)
    fails = []
    if code != 0 or box:
        fails.append(f"the run ended with code {code}{', box ' + box if box else ''} ({keep})")
    save = os.path.join(keep, "leg", "user", "samesave.sav")
    if not os.path.exists(save):
        fails.append("no save after the days")
    else:
        got = [r for r in rows(save, "OFC") + rows(save, "Oceania Nations") if r[0] == 2002]
        if not got:
            fails.append("the save has no 2002 row of the OFC Nations Cup (its draw never came)")
        elif min(got[0][1:]) < 0:
            fails.append(f"the 2002 row of the OFC Nations Cup lacks a nation: {got[0]}")
        else:
            print(f"ok    2002 row of the OFC Nations Cup: {got[0][1:]} (run {took:.0f} s)")
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
