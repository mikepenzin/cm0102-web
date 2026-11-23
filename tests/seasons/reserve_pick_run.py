#!/usr/bin/env python3
"""A reserve side whose club has many players away fields 11 (open item 237; src/recomp_rt/win/season_pick_rotation.c).

Usage:
    python3 tests/seasons/reserve_pick_run.py <cm0102_recomp> <game folder> --keep <folder> [--save <in.sav>]
        [--keep-fault]

The save: --save, else RECOMP_RESERVE_PICK_SAVE, else test-fixtures/reserve-2014-day262.sav. It is the save of a 2014
start (October 2014, all nations, changing rules on, seed 1; the rules report jump of session sz02) on day 262 of
2014, at the human turn before the fault. GNK Dinamo Zagreb then has all 50 squad slots filled. On day 269 of 2014,
18 of those players were away on loan (16 of them the database's loans, item 229) and 4 injured or recovering, and
the pick kept 14 back for the first team, so its side for the Reserve competition (record 87) had 10 players:
init_match 0x6b7f20 stopped at match_eng.cpp 612 (0x6b8973).

The run loads the save on holiday (the rules report jump, tests/seasons/soak_run.py load_leg) and plays 14 days. The
checks:
    - no message box, and the program ends with code 0 (the save after 14 days)
    - the log has the line "recomp: reserve pick: GNK Dinamo Zagreb keeps 7 players back for its first team, not 14,
      and fields 14, not 10, in the Reserve match"
--keep-fault runs with RECOMP_TEST_RESERVE_PICK=original (the original's pick): both checks must then fail, with
the box match_eng..cpp 612. This is the proof that the run finds the fault.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import soak_run  # noqa: E402

FIXTURES = "/Users/max/Projects/cm0102/test-fixtures"
DATABASE = os.path.join(FIXTURES, "databases", "October 2014")
LINE = ("recomp: reserve pick: GNK Dinamo Zagreb keeps 7 players back for its first team, not 14, and fields 14, not 10, "
        "in the Reserve match")


def main(argv):
    if len(argv) < 4 or "--keep" not in argv:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = os.path.abspath(soak_run.arg(argv, "--keep"))
    save = os.path.abspath(soak_run.arg(argv, "--save", os.environ.get("RECOMP_RESERVE_PICK_SAVE")
                                        or os.path.join(FIXTURES, "reserve-2014-day262.sav")))
    fault = "--keep-fault" in argv
    os.environ.pop("RECOMP_TEST_RESERVE_PICK", None)
    if fault:
        os.environ["RECOMP_TEST_RESERVE_PICK"] = "original"
    work = os.path.join(keep, "fault" if fault else "fixed")
    os.makedirs(work)
    soak_run.wait_for_room()
    o = {"program": program, "game": game, "rules": "on", "seed": 1, "last": 2014, "database": DATABASE,
         "timeout": 1800}
    code, t, box = soak_run.load_leg(o, work, save, 14)
    log = open(os.path.join(work, "stderr.txt"), "rb").read().decode("latin-1")
    failures = 0

    def check(cond, text):
        nonlocal failures
        print(("ok    " if cond else "FAIL  ") + text, flush=True)
        failures += not cond

    check(code == 0 and not box, f"14 days played and saved, no message box ({code}, {t:.0f} s, {box})")
    check(LINE in log, "the log has the line: " + LINE)
    for line in log.splitlines():
        if "reserve pick:" in line or "team pick of" in line:
            print("      " + line[:200])
    for name in ("soak.sav", "samesave.sav"):            # about 90 MB each
        p = os.path.join(work, "user", name)
        if os.path.exists(p):
            os.remove(p)
    print(("PASS" if not failures else f"FAIL ({failures})") + (" (fault kept)" if fault else ""))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
