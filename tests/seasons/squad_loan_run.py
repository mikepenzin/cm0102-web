#!/usr/bin/env python3
"""A human move of a player on loan inside a linked pair keeps him in one squad slot of each side (issue 159, open item
280, original-faults group squad_loans).

Usage:
    python3 tests/seasons/squad_loan_run.py <cm0102_recomp> <game folder> [--keep <folder>] [--keep-faults]

The run is the start-only run of tests/seasons/rules_report_check.py (a new 2001 game, the report of the first day,
then the program ends) with RECOMP_SQUAD_LOAN_TEST=1. The runtime (rt_squad_loan_test, src/recomp_rt/win/original_faults.c)
loans a player between a senior club and its linked B side. The human manager then takes each side in turn and uses the move
item of the staff profile's Action menu, through the screen's own handler 0x888260.

The test makes one loan from the senior side to its B side, and one against the link of club_get_linked_b_team 0x540a50.
The data holds no one-contract player of a true B side, so the second loan is between two clubs that the game's fixed
table links, whatever their names. The checks: the run ends with code
0, both loans ran with four moves each, the Action menu offers no move for such a loan, and each side holds the player
in one slot after every move. --keep-faults runs with RECOMP_ORIGINAL_FAULTS=keep:squad_loans: it passes only when the
menu offers the move for each loan and a side then holds him in two slots, which proves that the test finds the fault.
"""
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
LOAN = re.compile(r"recomp: squad loan test: staff (\d+) loaned from club (\d+) to club (\d+), slots (\d+) and (\d+)")
MOVE = re.compile(r"recomp: squad loan test: staff (\d+) move (\d+) by the manager of club (\d+): item (-?\d+), "
                  r"slots (\d+) and (\d+)")


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = os.path.abspath(argv[argv.index("--keep") + 1]) if "--keep" in argv else tempfile.mkdtemp(
        prefix="cm0102-squad-loan-")
    keep_faults = "--keep-faults" in argv
    env = dict(os.environ, RECOMP_SQUAD_LOAN_TEST="1")
    if keep_faults:
        env["RECOMP_ORIGINAL_FAULTS"] = "keep:squad_loans"
    run = subprocess.run([sys.executable, os.path.join(HERE, "rules_report_check.py"), "--run", program, game,
                          "--year", "2001", "--start-only", "--keep", keep], env=env,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    log = os.path.join(keep, "stderr.txt")
    text = open(log, errors="replace").read() if os.path.exists(log) else ""
    fails = []
    if "run: code 0 " not in run.stdout:
        fails.append("the start-only run did not end with code 0 (" + log + ")")
    if "recomp: message box from" in text:
        fails.append("the game showed a message box (" + log + ")")
    loans, moves = LOAN.findall(text), [tuple(map(int, m)) for m in MOVE.findall(text)]
    if len(loans) != 2:
        fails.append(f"{len(loans)} loans, expected one in each direction")
    for staff, _, _, first, second in loans:
        if (first, second) != ("1", "1"):
            fails.append(f"staff {staff}: after the loan the sides hold {first} and {second} slots, expected 1 and 1")
    if len(moves) != 4 * len(loans):
        fails.append(f"{len(moves)} moves for {len(loans)} loans, expected 4 each")
    for staff, _, _, _, _ in loans:
        offered = any(m[0] == int(staff) and m[3] >= 0 for m in moves)
        if keep_faults and not offered:
            fails.append(f"staff {staff}: the fault is kept and no manager of either side had the move item")
        if not keep_faults and offered:
            fails.append(f"staff {staff}: the Action menu offers a move that the game does not make")
    for staff, number, club, item, first, second in moves:
        print(f"staff {staff} move {number} by the manager of club {club}: item {item}, slots {first} and {second}")
    doubled = any(m[4] > 1 or m[5] > 1 for m in moves)
    if keep_faults and not doubled:
        fails.append("the fault is kept and no side holds the player twice: the test does not find the fault")
    if not keep_faults:
        fails += [f"staff {m[0]} move {m[1]}: slots {m[4]} and {m[5]}, expected 1 and 1"
                  for m in moves if (m[4], m[5]) != (1, 1)]
    if "run: code 0 " not in run.stdout:
        print(run.stdout[-1500:])
    for f in fails:
        print("FAIL  " + f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
