#!/usr/bin/env python3
"""UEFA Cup outcome check (issue 119): a winner of the last round of the UEFA Cup's own round table goes on to the
group stage. It does not get the cup winners record and the news "Fans celebrate winning the UEFA Cup".

Usage:
    python3 tests/seasons/uefa_cup_outcome_run.py <cm0102_recomp> <game folder> [--save-build <cm0102_recomp>]
        [--save <file>] [--days n] [--keep <folder>]

The runs:
    save      a new game in 2006/07 (RECOMP_START_SEASON=2006, our own database Summer 2006, England, Arsenal,
              changing rules off), Go on Holiday, saved n game days after the start (--days, default 70): between
              the first leg of the UEFA Cup First Round (14 September 2006) and the second leg (28 September). The
              save is made by --save-build (default the program under test), so a save of the main build is the
              save before the fault. --save gives a save made before and skips this run.
    fixed     the program loads the save and plays 21 days on holiday (RECOMP_SAVE_AFTER_DAYS=21)
    original  the same with RECOMP_TEST_UC_OUTCOME=original: the code before issue 119
The checks: each run ends with code 0 and shows no message box of the game. fixed has a line "UEFA Cup 2006/07:
<club> won round <code> of the round table: goes on, outcome 0 (not the cup winner)" for each of the 40 winners
of the First Round (src/recomp_rt/win/season_uefa_cup.c, the wrap of 0x9030e0) and no line of the old code.
original has 40 lines "... outcome 1, the cup winners record 0x86a800" and no line of the fix (the fault).
A run takes about 5 to 10 minutes.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import rules_report_check as rrc  # noqa: E402

YEAR = 2006
WINNERS = 40                                    # the First Round of 2004/05 to 2008/09: 80 clubs, the 40 of the groups
FIRST_LEG, SECOND_LEG = 256, 270                # 14 and 28 September 2006, days of the year from 0 (GAME_DATE)
FIXED = re.compile(r"recomp: uefa: UEFA Cup 2006/07: (.*) won round (\d+) of the round table: goes on, outcome 0")
OLD = re.compile(r"recomp: uefa: UEFA Cup 2006/07: (.*) won round (\d+) of the round table: outcome 1, the cup "
                 r"winners record")
BOX = re.compile(r"message box from 0x[0-9a-f]+: (.*)")
DATE = re.compile(r"recomp: game date (\d+)/(\d+) at a human turn")
SAVED = re.compile(r"recomp: saved .* after (\d+) game days")


def make_save(program, game, days, work):
    """The save of the save run, or a list of failures."""
    import smoke_run
    unpack = tempfile.mkdtemp(prefix="cm0102-uc-outcome-db-")
    try:
        db = rrc.own_database(YEAR, rrc.OWN_DBS, unpack)
        code, _ = rrc.run(program, game, YEAR, YEAR, db, ["England"], [], work, 3600, None,
                          {"RECOMP_SAVE_AFTER_DAYS": str(days)})
    finally:
        shutil.rmtree(unpack, ignore_errors=True)
    save = os.path.join(work, "user", smoke_run.SAVE_NAME)
    if code != 0 or not os.path.exists(save):
        return None, [f"save: no saved game (code {code}, {work})"]
    with open(os.path.join(work, "stderr.txt"), encoding="latin-1") as f:
        log = f.read()
    first, saved = DATE.search(log), SAVED.search(log)
    day = int(first.group(1)) + int(saved.group(1)) if first and saved else -1
    if not (first and saved and int(first.group(2)) == YEAR and FIRST_LEG < day < SECOND_LEG):
        return None, [f"save: the save is not between the two legs of the First Round (day {day} of {YEAR}, "
                      f"want {FIRST_LEG + 1} to {SECOND_LEG - 1}; change --days)"]
    print(f"save: day {day} of {YEAR} ({save})")
    return save, []


def play(program, game, save, work, env):
    """Loads the save and plays 21 days on holiday; returns the code and the log."""
    import custom_database_run as cdr
    import fixtures
    import smoke_run
    user = os.path.join(work, "user")
    os.makedirs(user, exist_ok=True)
    shutil.copyfile(save, os.path.join(user, "uefa.sav"))
    unpack = tempfile.mkdtemp(prefix="cm0102-uc-outcome-db-")
    try:
        db = rrc.own_database(YEAR, rrc.OWN_DBS, unpack)
        cdr.copy_data(db, os.path.join(user, "Databases", os.path.basename(db)))
    finally:
        shutil.rmtree(unpack, ignore_errors=True)
    fixtures.write_settings(user, "classic", [("database", f"folder:{os.path.basename(db)}"), ("faster_days", "on")])
    steps = fixtures.LOAD_STEPS + ["3600000 quit"]
    try:
        r, _ = fixtures.run(program, game, "classic", user, smoke_run.scaled_steps("classic", work, steps),
                            dict({"RECOMP_SEASON_JUMP": str(YEAR), "RECOMP_SAVE_AFTER_DAYS": "21"}, **env), 3600)
    except subprocess.TimeoutExpired:
        return "timeout", ""
    with open(os.path.join(work, "stderr.txt"), "w") as f:
        f.write(r.stderr)
    return r.returncode, r.stderr


def check(name, code, log, want, other):
    fails = [] if code == 0 else [f"{name}: the game ended with code {code}"]
    fails += [f"{name}: message box {b}" for b in BOX.findall(log)]
    got, wrong = want.findall(log), other.findall(log)
    clubs = sorted({c for c, _ in got})
    if len(got) != WINNERS or len(clubs) != WINNERS:
        fails.append(f"{name}: {len(got)} lines for {len(clubs)} clubs, want {WINNERS}")
    if wrong:
        fails.append(f"{name}: {len(wrong)} lines of the other code")
    print(f"{name}: {len(got)} lines, rounds {sorted({int(r) for _, r in got})}, {len(wrong)} of the other code")
    return fails


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    opt = lambda n, d=None: argv[argv.index(n) + 1] if n in argv else d  # noqa: E731
    keep = opt("--keep")
    work = os.path.abspath(keep) if keep else tempfile.mkdtemp(prefix="cm0102-uc-outcome-")
    os.makedirs(work, exist_ok=True)
    save = opt("--save")
    fails = []
    if save:
        save = os.path.abspath(save)
    else:
        save, fails = make_save(os.path.abspath(opt("--save-build", program)), game, int(opt("--days", "70")),
                                os.path.join(work, "save"))
    if save:
        code, log = play(program, game, save, os.path.join(work, "fixed"), {})
        fails += check("fixed", code, log, FIXED, OLD)
        code, log = play(program, game, save, os.path.join(work, "original"), {"RECOMP_TEST_UC_OUTCOME": "original"})
        fails += check("original", code, log, OLD, FIXED)
    for f in fails:
        print("FAIL", f)
    if not fails:
        print("PASS")
        if not keep:
            shutil.rmtree(work, ignore_errors=True)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
