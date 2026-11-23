#!/usr/bin/env python3
"""Portugal's non-EU signing limit by season (GitHub issue 121).

Usage:
    python3 tests/seasons/portugal_non_eu_run.py <cm0102_recomp> <game folder> [--only <name>,...] [--keep <folder>]
        [--reuse]

The original refuses a signing that takes a Portuguese club past 6 non-EU players in the Liga, 5 in the Liga de
Honra and 4 below (PORTUGAL_RULES::can_sign_player 0x7d3ef0, can_make_offer 0x7d4040). The Liga general assembly
of 30 June 2006 ended that limit in the Liga and the Liga de Honra from 2006/07 (docs/research/rules-0.6.0/por.tsv).

The Liga clubs of the databases hold few non-EU players (0 to 3 at the 2010 start), so a season of AI transfers
does not reach a 7th. Each run makes a new game with Portugal, changing_rules on, RECOMP_SEASON_NATION_LOG=1 and
the test switch RECOMP_TEST_PT_SIGN=1, and stops after the first start report. At the new game the switch fills the
Liga club with the most non-EU players up to 6 with non-EU players of other clubs, asks the rules object's
can_sign_player (vtable +0x10) about a 7th on day 200 (in the transfer window), and puts the squad back
(season_kind_nation.c, portugal_test_sign). The runs:
    on2010   September 2010 ODB, 2010/11: the club holds 6, the check answers 1 (the signing may go on)
    on2001   the base game, 2001/02: the club holds 6, the check answers 0 with code 0x15 (the Portuguese limit)
Both runs also check that the check went through 0x7d3ef0 and that no message box came up.
"""
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
import rules_report_check as rrc  # noqa: E402
import uefa_formats_run as ufr  # noqa: E402

DB = "/Users/max/Projects/cm0102/test-fixtures/databases"
# name: (start year, database, expected answer, expected code)
RUNS = {
    "on2010": (2010, os.path.join(DB, "September 2010 ODB"), 1, None),
    "on2001": (2001, None, 0, 0x15),
}
TEST = re.compile(r"season nation: Portugal test (\d+): (.+) held (\d+) non-EU \(had (\d+), (\d+) found\), "
                  r"can_sign_player 0x([0-9a-f]+) for one more answers (\d+) code 0x([0-9a-f]+)")


def check_run(name, program, game, keep, reuse):
    year, database, answer, code_want = RUNS[name]
    work = os.path.join(keep, name) if keep else tempfile.mkdtemp(prefix=f"cm0102-i121-{name}-")
    if not (reuse and os.path.exists(os.path.join(work, "stderr.txt"))):
        code, _ = rrc.run(program, game, year, year if year != 2001 else None, database, ["Portugal"], [], work, 7200,
                          {"changing_rules": "on"}, {"RECOMP_SEASON_NATION_LOG": "1", "RECOMP_TEST_PT_SIGN": "1"},
                          start_only=True)
        if code != 0:
            return [f"{name}: the run ended with code {code} ({work})"]
    fails = []
    found, _ = ufr.boxes(work)
    fails += [f"{name}: message box {b}" for b in found]
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read().split("\n")
    tests = [m for m in (TEST.search(x) for x in log) if m]
    if len(tests) != 1:
        return fails + [f"{name}: {len(tests)} test lines in the log, expected 1 ({work})"]
    m = tests[0]
    season, club, held, had, n_found = int(m.group(1)), m.group(2), int(m.group(3)), int(m.group(4)), int(m.group(5))
    fn, got, code = int(m.group(6), 16), int(m.group(7)), int(m.group(8), 16)
    if season != year:
        fails.append(f"{name}: the test ran in season {season}, expected {year}")
    if held != 6:
        fails.append(f"{name}: {club} held {held} non-EU players for the test, expected 6")
    if fn != 0x7d3ef0:
        fails.append(f"{name}: the rules object's can_sign_player is 0x{fn:x}, expected 0x7d3ef0")
    if got != answer or (code_want is not None and code != code_want):
        fails.append(f"{name}: the check of a 7th non-EU player answers {got} code 0x{code:x}, expected {answer}"
                     + (f" code 0x{code_want:x}" if code_want is not None else ""))
    # The base cap +0x13 stays: 255 there refuses every offer for a foreign player (0x8e8800, signed compare).
    caps = [x for x in log if re.search(r"season nation: Portugal foreign cap", x)]
    if caps:
        fails.append(f"{name}: the foreign cap of Portugal was changed: {caps[0].strip()}")
    print(f"{name}: {club} had {had} non-EU players, held {held} for the test ({n_found} found elsewhere); "
          f"a 7th: answer {got} code 0x{code:x}; {len(fails)} problems ({work})")
    return fails


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    keep = os.path.abspath(rrc.arg(argv, "--keep")) if "--keep" in argv else None
    fails = []
    for n in only:                       # one game at a time
        fails += check_run(n, program, game, keep, "--reuse" in argv)
    for t in fails:
        print("FAIL  " + t)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
