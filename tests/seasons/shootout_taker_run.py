#!/usr/bin/env python3
"""A shootout slot with no taker takes the best player left (0.6.2 item 188, src/recomp_rt/win/season_match_grid.c).

Usage:
    python3 tests/seasons/shootout_taker_run.py <cm0102_recomp> <game folder> [--keep <folder>]

A human side's tactics hold taker -1 for a slot when the taker chooser (0x8a9370) finds fewer than 11 players on the
pitch. The original copies it into the shootout order (0x6be663), and the shootout step then takes an object that is
no match player (0x6be931..0x6be96a). The run is one leg of tests/seasons/full_play_run.py (a 2021 start, Brazil,
365 days with the match engine on, with the shootouts of the Copa do Brasil) with RECOMP_TEST_TAKER_GAP=1, which
sets slot 10 of each side to -1 after the original fills the order, and RECOMP_CLUB_LOG=1, which logs each order after the fix.

The checks: the full-play leg passes, the log holds at least one test line and one shootout order, and no
order holds -1.
"""
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ORDER = re.compile(r"recomp: match grid: shootout of .*; order((?: -?\d+){22})$", re.M)


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = os.path.abspath(argv[argv.index("--keep") + 1]) if "--keep" in argv else tempfile.mkdtemp(
        prefix="cm0102-shootout-taker-")
    env = dict(os.environ, RECOMP_TEST_TAKER_GAP="1", RECOMP_CLUB_LOG="1")
    run = subprocess.run([sys.executable, os.path.join(HERE, "full_play_run.py"), program, game, "--keep", keep,
                          "--start", "2021", "--seasons", "1", "--nations", "Brazil"], env=env,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    print(run.stdout.strip().splitlines()[-1] if run.stdout.strip() else "(no output from the full-play run)")
    log_path = os.path.join(keep, "leg00", "stderr.txt")
    log = open(log_path, encoding="latin-1").read() if os.path.exists(log_path) else ""
    tests = log.count("recomp: match grid: taker gap test")
    orders = [list(map(int, m.group(1).split())) for m in ORDER.finditer(log)]
    gaps = [o for o in orders if -1 in o]
    print(f"{tests} test lines, {len(orders)} shootout orders, {len(gaps)} with -1")
    fails = []
    if run.returncode != 0:
        fails.append("the full-play leg failed (" + keep + ")")
    if not tests or not orders:
        fails.append("no shootout with the test switch in " + log_path)
    if gaps:
        fails.append(f"{len(gaps)} orders still hold -1, first: {gaps[0]}")
    for f in fails:
        print("FAIL  " + f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
