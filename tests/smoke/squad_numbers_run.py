#!/usr/bin/env python3
"""Squad numbers 1 to 99 (GitHub issue 140, 0.7.3 slice 13).

Usage:
    python3 tests/smoke/squad_numbers_run.py <cm0102_recomp> <game folder> <out folder>

Runs, on a new game of the 3.9.68 database with England:
    on      squad_numbers_99 = on. The new game keeps database numbers from 51 to 99.
    off     squad_numbers_99 = off. The new game has no number above 50.
    carry   a game made with the option on is saved, then loads with the option off and plays on for 7 game days.
    season  a game with the option on plays 120 game days, with league matches of players numbered above 50.

In "on" and "off" the runtime's checks run at the first human turn (RECOMP_TEST_SQUAD_NUMBERS=1, squad_numbers.c):
the setter takes numbers up to 99, assign_club_squad_numbers 0x859fe0 and allocate_squad_number 0x85a470 keep them at
a computer club and at the human club, and a signing at a club with 1 to 50 taken gets 51 with the option on and no
number with it off. The pass rule is no failed check, no error box and the counts below.
"""
import os
import re
import shutil
import sys
import tempfile
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fixtures"))
import fixtures  # noqa: E402
import smoke_run  # noqa: E402

COUNT = re.compile(r"squad numbers test: option (on|off), (\d+) players above 50, highest (\d+)")
NEW_GAME = smoke_run.STEPS[:smoke_run.STEPS.index("wait first-news") + 1]
HOLIDAY = smoke_run.STEPS[smoke_run.STEPS.index("wait first-news") + 1:]
HOLIDAY = [s for s in HOLIDAY if "dump" not in s and "quit" not in s]
failures = []
MAX_LOAD = 24         # no new test game while the 1-minute load of the Mac is this or more


def check(ok, text):
    print(("ok   " if ok else "FAIL ") + text)
    if not ok:
        failures.append(text)


def play(program, game, out, name, steps, options, env, save=None):
    while os.getloadavg()[0] >= MAX_LOAD:
        time.sleep(15)
    folder = os.path.join(out, name)
    os.makedirs(folder, exist_ok=True)
    work = tempfile.mkdtemp(prefix="cm0102-squad-")
    user = os.path.join(work, "user")
    os.makedirs(user)
    if save:
        shutil.copyfile(save, os.path.join(user, "start.sav"))
    fixtures.write_settings(user, "widescreen", [("changing_rules", "off")] + options)
    env = dict({"RECOMP_SCREEN_LOG": "1"}, **env)
    r, _ = fixtures.run(program, game, "widescreen", user, smoke_run.scaled_steps("widescreen", folder, steps), env, 1500)
    with open(os.path.join(folder, "stderr.txt"), "w") as f:
        f.write(r.stderr)
    result = os.path.join(user, smoke_run.SAVE_NAME)
    kept = os.path.join(folder, smoke_run.SAVE_NAME)
    if os.path.exists(result):
        shutil.copyfile(result, kept)
    shutil.rmtree(work, ignore_errors=True)
    return r.returncode, r.stderr, kept


def counts(log):
    return [(m[0], int(m[1]), int(m[2])) for m in COUNT.findall(log)]


def checks(name, code, log):
    check(code == 0, f"{name}: the game ends with code 0 (code {code})")
    check("squad numbers test: done, 0 failures" in log, f"{name}: every runtime check passes")
    for line in log.splitlines():
        if "squad numbers test: FAIL" in line:
            print("     " + line)
    check("message box from" not in log, f"{name}: no error box")


def main(argv):
    if len(argv) != 4:
        print(__doc__)
        return 2
    program, game, out = argv[1:]
    steps = NEW_GAME + ["600000 quit"]
    test = {"RECOMP_TEST_SQUAD_NUMBERS": "1"}

    code, log, _ = play(program, game, out, "on", steps, [("squad_numbers_99", "on")], test)
    checks("on", code, log)
    c = counts(log)
    check(bool(c) and c[0][1] > 0 and 50 < c[0][2] <= 99, f"on: the new game keeps numbers above 50 {c[:1]}")

    code, log, _ = play(program, game, out, "off", steps, [("squad_numbers_99", "off")], test)
    checks("off", code, log)
    c = counts(log)
    check(bool(c) and c[0][1] == 0, f"off: the new game has no number above 50 {c[:1]}")

    code, log, save = play(program, game, out, "carry-save", NEW_GAME + HOLIDAY + ["600000 quit"],
                           [("squad_numbers_99", "on")], {"RECOMP_SAVE_AFTER_DAYS": "1"})
    check(os.path.exists(save), f"carry: the game with the option on saves (code {code})")
    if os.path.exists(save):
        steps = list(fixtures.LOAD_STEPS) + HOLIDAY + ["600000 quit"]
        code, log, after = play(program, game, out, "carry-load", steps, [("squad_numbers_99", "off")],
                                {"RECOMP_SAVE_AFTER_DAYS": "7", "RECOMP_TEST_SQUAD_NUMBERS": "count"}, save)
        c = counts(log)
        check(os.path.exists(after), f"carry: the game with the option off plays 7 days and saves (code {code})")
        check("message box from" not in log, "carry: no error box")
        check(bool(c) and c[0][1] > 0, f"carry: the load keeps the numbers above 50 {c[:1]}")
        check(bool(c) and c[-1][1] > 0 and c[-1][2] <= 99, f"carry: they are still there at the end {c[-1:]}")
        check(bool(c) and max(x[1] for x in c) <= c[0][1], f"carry: the option off adds no number above 50 {c[0]} {c[-1:]}")
    code, log, save = play(program, game, out, "season", NEW_GAME + HOLIDAY + ["1200000 quit"],
                           [("squad_numbers_99", "on")], {"RECOMP_SAVE_AFTER_DAYS": "120"})
    check(os.path.exists(save), f"season: 120 game days with the option on end in a save (code {code})")
    check("message box from" not in log, "season: no error box")
    print("squad numbers:", "FAILED" if failures else "passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
