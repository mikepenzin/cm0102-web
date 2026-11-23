#!/usr/bin/env python3
"""A club in two key ids keeps both after the trim of the club table (open item 295, original-faults group club_keys).

Usage:
    python3 tests/seasons/club_key_twins_run.py <cm0102_recomp> <game folder> <out folder> [--keep-faults] [--original-data]

A new game up to its first start report: a season game on the October 2026 database, or with --original-data the base
game on the 3.9.68 data (a 2001 start with changing rules off), which must stay the original.
remap_club_key_id 0x60ba80 rewrites only the first key id of a row, so the second key ids of C.D. Logrones,
C.D. Logrones B, C.D. Badajoz and C.D. Badajoz B kept their old rows after database_trim_club_table 0x5267c0.

The checks, on the log of RECOMP_SEASON_LOG and RECOMP_CLUB_KEY_TEST:
    - October 2026: the two key ids of each club hold one row, and each second key id moves to its known row
    - October 2026: no club on an old row has a B side past the end of the club table
    - --original-data: no second key id moves, so the two key ids of each club hold different rows
    - --keep-faults (RECOMP_ORIGINAL_FAULTS=keep:club_keys), October 2026: the club on row 3230 has that B side, so the
      test can see the fault
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
import rules_report_check as rrc  # noqa: E402

DB = "/Users/max/Projects/cm0102/test-fixtures/databases/October 2026/October 2026 Data Update"
NATIONS = ["England", "Italy", "Spain", "France", "Germany", "Holland", "Portugal", "Belgium"]
MOVES = {0x9d04c0: (1215, 827), 0x9d04c4: (1216, 828), 0x9d04c8: (3230, 1901), 0x9d04cc: (6679, 3316)}


def faults_of(log, keep, original):
    moved = {int(k, 16): (int(a), int(b)) for k, a, b in re.findall(r"recomp: club key (0x[0-9a-f]+) follows row (\d+) to row (\d+)", log)}
    moved = {k: v for k, v in moved.items() if k in MOVES}
    pairs = re.findall(r"club key test: keys 0x[0-9a-f]+ 0x[0-9a-f]+ hold rows (-?\d+) (-?\d+)", log)
    past = re.findall(r"club key test: row (\d+) '([^']*)' has a B side past the table", log)
    faults = [] if "club key test: done" in log and len(pairs) == 4 else ["the key test did not run"]
    if original:
        if moved:
            faults.append(f"the base game moved second key ids: {sorted(hex(k) for k in moved)}")
        faults += [f"the base game holds rows {a} and {b} in one row's key ids" for a, b in pairs if a == b]
        return faults
    if keep:
        if moved:
            faults.append(f"second key ids moved: {sorted(hex(k) for k in moved)}")
        if not any(row == "3230" for row, _ in past):
            faults.append("row 3230 has no B side past the table")
        return faults
    faults += [f"the key ids of one club hold rows {a} and {b}" for a, b in pairs if a != b]
    faults += [f"row {row} {name} has a B side past the table" for row, name in past]
    if moved != MOVES:
        faults.append(f"second key ids that moved: { {hex(k): v for k, v in moved.items()} }")
    return faults


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    if len(args) != 3:
        print(__doc__)
        return 2
    keep, original = "--keep-faults" in argv, "--original-data" in argv
    program, game, out = (os.path.abspath(a) for a in args)
    env = {"RECOMP_CLUB_KEY_TEST": "1", "RECOMP_SEASON_LOG": "1"}
    if keep:
        env["RECOMP_ORIGINAL_FAULTS"] = "keep:club_keys"
    year = 2001 if original else 2026
    code, _ = rrc.run(program, game, year, year, None if original else DB, NATIONS, [], out, 1200,
                      {} if original else {"changing_rules": "on"}, env, start_only=True)
    with open(os.path.join(out, "stderr.txt"), encoding="latin-1") as f:
        log = f.read()
    for line in re.findall(r"recomp: club key test: .*", log):
        print(line)
    faults = faults_of(log, keep, original) + ([] if code == 0 else [f"the run ended with code {code}"])
    for x in faults:
        print("FAIL ", x)
    print("ok" if not faults else f"FAIL  {len(faults)} checks")
    return 1 if faults else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
