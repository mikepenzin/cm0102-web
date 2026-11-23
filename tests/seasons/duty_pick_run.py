#!/usr/bin/env python3
"""A club whose players are on international duty fields 11 in a friendly (open item 262;
src/recomp_rt/win/season_pick_rotation.c).

Usage:
    python3 tests/seasons/duty_pick_run.py <cm0102_recomp> <game folder> --keep <folder> [--save <in.sav>]
        [--keep-fault]

The save: --save, else RECOMP_DUTY_PICK_SAVE, else test-fixtures/duty-2027-italy.sav. It is the save of a 2026 start
(Italy, changing rules on, seed 1, the base data; the after-table run of session sz09, made with its build before the
fix of youth_club_lists.c) 430 days after the start, before the fault. So it stands for a save of 0.7.0 or 0.7.1. Guiseley then has all 50 squad slots filled, most of them with youths of the under-21 squads of the
Republic of Ireland, Wales and Northern Ireland. A few days later it plays a friendly while 38 of them are on
international duty (NationalTeams::get_international_duty_club 0x7757d0). The pick MATCH_MAN::pick_team 0x6dbc50
leaves them out and gives 10 players, and init_match 0x6b7f20 stops at match_eng.cpp 652 (0x6b8aca).

The run loads the save on holiday (tests/seasons/soak_run.py load_leg) and plays 40 days. The checks:
    - no message box, and the program ends with code 0 (the save after 40 days)
    - the log has the line "recomp: duty pick: Guiseley fields 11, not 10, in the friendly match"
--keep-fault runs with RECOMP_TEST_DUTY_PICK=original (the original's pick): both checks must then fail, with the box
match_eng..cpp 652. This is the proof that the run finds the fault.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import soak_run  # noqa: E402

FIXTURES = "/Users/max/Projects/cm0102/test-fixtures"
DATABASE = os.path.join(FIXTURES, "databases", "October 2026")
LINE = "recomp: duty pick: Guiseley fields 11, not 10, in the friendly match"
DAYS = 40


def main(argv):
    if len(argv) < 4 or "--keep" not in argv:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = os.path.abspath(soak_run.arg(argv, "--keep"))
    save = os.path.abspath(soak_run.arg(argv, "--save", os.environ.get("RECOMP_DUTY_PICK_SAVE")
                                        or os.path.join(FIXTURES, "duty-2027-italy.sav")))
    fault = "--keep-fault" in argv
    os.environ.pop("RECOMP_TEST_DUTY_PICK", None)
    if fault:
        os.environ["RECOMP_TEST_DUTY_PICK"] = "original"
    work = os.path.join(keep, "fault" if fault else "fixed")
    os.makedirs(work)
    soak_run.wait_for_room()
    o = {"program": program, "game": game, "rules": "on", "seed": 1, "last": 2028, "database": DATABASE,
         "timeout": 1800}
    code, t, box = soak_run.load_leg(o, work, save, DAYS)
    log = open(os.path.join(work, "stderr.txt"), "rb").read().decode("latin-1")
    failures = 0

    def check(cond, text):
        nonlocal failures
        print(("ok    " if cond else "FAIL  ") + text, flush=True)
        failures += not cond

    check(code == 0 and not box, f"{DAYS} days played and saved, no message box ({code}, {t:.0f} s, {box})")
    check(LINE in log, "the log has the line: " + LINE)
    for line in log.splitlines():
        if "duty pick:" in line or "reserve pick:" in line or "team pick of" in line:
            print("      " + line[:200])
    for name in ("soak.sav", "samesave.sav"):            # about 50 MB each
        p = os.path.join(work, "user", name)
        if os.path.exists(p):
            os.remove(p)
    print(("PASS" if not failures else f"FAIL ({failures})") + (" (fault kept)" if fault else ""))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
