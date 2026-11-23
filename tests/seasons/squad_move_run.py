#!/usr/bin/env python3
"""A player moved twice to the linked side holds one squad slot there (open item 187, original-faults group
squad_moves, tools/recomp/original_faults.py at 0x4cc03d).

Usage:
    python3 tests/seasons/squad_move_run.py <cm0102_recomp> <game folder> [--keep <folder>] [--keep-faults]

The run is the start-only run of tests/seasons/rules_report_check.py (a new 2001 game, the report of the first day,
then the program ends) with RECOMP_SQUAD_MOVE_TEST=1. At that report the runtime (rt_squad_move_test,
src/recomp_rt/win/original_faults.c) takes the first senior club with a linked B side that does not link back to it (as
Hertha BSC Amateure in item 187, so the placement of a player at the B side comes from bit 0x40 of his contract) and
a player of the senior club with one contract and the placement 4 (at the senior club of a linked side, get_squad_placement 0x4cbb60), calls
CONTRACT_MANAGER::move_to_reserves 0x4cc0f0 twice, as the squad assessment does, and prints the placement before and
after the first move and how many squad slots of each side hold him.

The checks: the run ends with code 0, the test line is there, the placement after the first move is 3 (on the B side),
and the B side holds the player in one slot and the senior club in none. --keep-faults runs with
RECOMP_ORIGINAL_FAULTS=keep:squad_moves: it passes only when the line shows the fault (placement 4 after the move and
two slots on the B side).
"""
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
LINE = re.compile(r"recomp: squad move test: club (\d+) B side (\d+) staff (\d+) placement (\d+) then (\d+), "
                  r"B side slots (\d+), club slots (\d+)")


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = os.path.abspath(argv[argv.index("--keep") + 1]) if "--keep" in argv else tempfile.mkdtemp(
        prefix="cm0102-squad-move-")
    keep_faults = "--keep-faults" in argv
    env = dict(os.environ, RECOMP_SQUAD_MOVE_TEST="1")
    if keep_faults:
        env["RECOMP_ORIGINAL_FAULTS"] = "keep:squad_moves"
    run = subprocess.run([sys.executable, os.path.join(HERE, "rules_report_check.py"), "--run", program, game,
                          "--year", "2001", "--start-only", "--keep", keep], env=env,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    log = os.path.join(keep, "stderr.txt")
    text = open(log, errors="replace").read() if os.path.exists(log) else ""
    found = LINE.search(text)
    fails = []
    if "run: code 0 " not in run.stdout:
        fails.append("the start-only run did not end with code 0 (" + log + ")")
    if not found:
        fails.append("no squad move test line in " + log)
    else:
        club, b, staff, before, after, b_slots, club_slots = map(int, found.groups())
        print(f"club {club}, B side {b}, staff {staff}: placement {before} then {after}, "
              f"B side slots {b_slots}, club slots {club_slots}")
        want = (4, 2) if keep_faults else (3, 1)
        if (after, b_slots) != want or (not keep_faults and club_slots != 0):
            fails.append(f"placement {after} and {b_slots} B side slots, expected {want[0]} and {want[1]}"
                         + ("" if keep_faults else " and no club slot"))
    for f in fails:
        print("FAIL  " + f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
