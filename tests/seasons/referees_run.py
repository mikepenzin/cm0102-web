#!/usr/bin/env python3
"""Referee ranges outside the officials table (src/recomp_rt/win/referees.c, tools/recomp/referees.py).

Usage:
    python3 tests/seasons/referees_run.py <cm0102_recomp> <game folder> --save <a .sav> --keep <folder> [--days 21]

The save: a game whose off.dat holds a nation with a continent and a referee range past the officials table, for
example the save of the report of 25 November 2026 (a 2025/26 game with changing rules on, 1,317 officials: Martinique,
Guadeloupe, Bonaire, French Guiana, Saint-Martin and Sint Maarten in North America with first slot 1000000). The
browser stopped in its match day of late September 2026 with "memory access out of bounds" in
OfficialsManager::choose_match_official 0x79c7b0. The script reads the bad ranges of the save first and stops when
there is none (nothing to test).

Two runs, each loads the save, goes on holiday and plays --days days (fixtures.play):
- default: the load gives each such nation the range of its continent ("recomp: referees: X takes the referees of
  Y"), the picker's test never acts, and the new save holds no range past the table;
- RECOMP_REFEREES=guard (no repair): the picker's test acts ("recomp: referees: the range from slot ...") and the
  game plays on.
Each run must end with code 0 and save.
"""
import os
import shutil
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path[:0] = [os.path.join(ROOT, "tests", "fixtures"), os.path.join(ROOT, "tools", "save")]
import fixtures  # noqa: E402
import savfile  # noqa: E402

HOLIDAY_STEPS = ["2500 click 45 165", "4500 click 150 427 | 199 474", "7000 click 705 572"]
TAKES = "recomp: referees: "
GUARD = "recomp: referees: the range from slot"


def bad_ranges(path):
    """[(nation name, count, first)]: the nations with a continent whose referee range reaches past the table."""
    sav = savfile.Sav(path)
    off, officials, nations = (sav.read_member(m) for m in ("off.dat", "officials.dat", "nation.dat"))
    rows = struct.unpack_from("<I", off, 0)[0]
    at = 4 + rows * 17                                 # the stats rows (load_official_stats 0x79ba19..0x79ba35)
    blocks = struct.unpack_from("<I", off, at)[0]      # then the nation blocks (0x79ba5f..0x79ba85)
    size = min(rows, len(officials) // 0x2b)
    out = []
    for i in range(min(blocks, len(nations) // 0x122)):
        count, first = struct.unpack_from("<ii", off, at + 4 + i * 0x102 + 4)
        continent = struct.unpack_from("<i", nations, i * 0x122 + 0x71)[0]
        if continent >= 0 and (count < 0 or first < 0 or first + max(count, 1) > size):
            out.append((nations[i * 0x122 + 4:i * 0x122 + 0x36].split(b"\0")[0].decode("latin-1"), count, first))
    return out


def one_run(program, game, keep, save, days, guard):
    work = os.path.join(keep, "guard" if guard else "repair")
    os.makedirs(work, exist_ok=True)
    name = os.path.basename(save)
    dest = os.path.join(work, name)
    if not os.path.exists(dest):
        shutil.copyfile(save, dest)
    steps = os.path.join(work, "steps.txt")
    with open(steps, "w") as f:
        f.write("\n".join(HOLIDAY_STEPS) + "\n")
    if guard:
        os.environ["RECOMP_REFEREES"] = "guard"
    else:
        os.environ.pop("RECOMP_REFEREES", None)
    out = os.path.join(work, "out")
    code = fixtures.play(program, game, work, name, steps, out, "classic", [], days)
    with open(os.path.join(out, "stderr.txt"), encoding="latin-1") as f:
        lines = [ln.rstrip("\n") for ln in f if ln.startswith(TAKES)]
    fails = [] if code == 0 else [f"{os.path.basename(work)}: the run failed (code {code})"]
    guarded = [ln for ln in lines if ln.startswith(GUARD)]
    taken = [ln for ln in lines if not ln.startswith(GUARD)]
    for ln in lines:
        print(f"  {os.path.basename(work)}: {ln}")
    if guard and not guarded:
        fails.append("guard: the picker's test never acted")
    if not guard:
        if not taken:
            fails.append("repair: no nation took a range at the load")
        if guarded:
            fails.append("repair: the picker's test acted after the repair")
        result = os.path.join(out, "samesave.sav")
        if os.path.exists(result) and bad_ranges(result):
            fails.append(f"repair: the new save still has ranges past the table: {bad_ranges(result)}")
    return fails


def main(argv):
    if len(argv) < 3 or "--save" not in argv or "--keep" not in argv:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    save = os.path.abspath(argv[argv.index("--save") + 1])
    keep = os.path.abspath(argv[argv.index("--keep") + 1])
    days = int(argv[argv.index("--days") + 1]) if "--days" in argv else 21
    bad = bad_ranges(save)
    print(f"{os.path.basename(save)}: {len(bad)} nations with a continent and a range past the table: {bad}")
    if not bad:
        print("FAIL: the save has no range past the table, so it tests nothing")
        return 1
    fails = one_run(program, game, keep, save, days, guard=False) + one_run(program, game, keep, save, days, guard=True)
    for f in fails:
        print("FAIL", f)
    print("PASS" if not fails else f"FAIL: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
