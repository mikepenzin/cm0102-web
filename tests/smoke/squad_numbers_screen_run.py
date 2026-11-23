#!/usr/bin/env python3
"""The squad numbers screen with numbers 1 to 99 (GitHub issue 140, 0.7.3 slice 14).

Usage:
    python3 tests/smoke/squad_numbers_screen_run.py <cm0102_recomp> <game folder> <out folder>

Loads start.sav, takes number 1 from its holder and opens the screen of the human club (RECOMP_TEST_SQUAD_NUMBERS=screen,
squad_numbers.c). Real mouse steps drag number button 75 onto that player's name (the handler sets it at 0x47ba3c).
    on      squad_numbers_99 = on. The screen has 99 buttons, and the player holds 75 after the drop.
    classic the same in the classic display.
    off     squad_numbers_99 = off, with a player on 75 as in a save made with the option on. The original strips draw,
            the screen opens with no error box and the player keeps 75.
The pass rule is the log lines above, no error box and code 0. The pictures stay in the out folder.
"""
import os
import re
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fixtures"))
import fixtures  # noqa: E402
import smoke_run  # noqa: E402

PLAYER_ROW = (528, 273)         # the name of the player who held number 1 (Seaman), 800x600 positions
BUTTON_75 = (766, 503)          # number 75: the last column of the first row of the lower strip
HELD = re.compile(r"squad numbers screen: number (\d+) has staff (\d+)")
CLEARED = re.compile(r"squad numbers screen: staff (\d+) has no number")
KEPT = re.compile(r"squad numbers screen: staff (\d+) has number 75")
SHOWN = re.compile(r"squad numbers screen: staff (\d+) shows number (\d+)")
failures = []


def check(ok, text):
    print(("ok   " if ok else "FAIL ") + text)
    if not ok:
        failures.append(text)


def steps(drag=True):
    x0, y0 = BUTTON_75
    x1, y1 = PLAYER_ROW
    # The screen opens over the first news screen, so the steps wait for it in place of "wait first-news".
    start = [s for s in fixtures.LOAD_STEPS if s != "wait first-news"] + ["wait screen:0x475520", "2000 dump {d}/1-open.bmp"]
    if not drag:
        return start + ["5000 dump {d}/2-open.bmp", "6000 quit"]
    return start + [
        f"2500 move {x0} {y0}",
        f"3000 down {x0} {y0}",
        f"3500 move {x0 + 20} {y0 + 20}",
        f"4000 move {x1} {y1}",
        f"4500 up {x1} {y1}",
        "7000 dump {d}/2-dropped.bmp",
        "8000 quit",
    ]


def play(program, game, out, name, value, mode="widescreen"):
    folder = os.path.join(out, name)
    os.makedirs(folder, exist_ok=True)
    work = tempfile.mkdtemp(prefix="cm0102-squad-screen-")
    user = os.path.join(work, "user")
    os.makedirs(user)
    shutil.copyfile(os.path.join(fixtures.DEFAULT_DIR, "start.sav"), os.path.join(user, "start.sav"))
    fixtures.write_settings(user, mode, [("changing_rules", "off"), ("squad_numbers_99", value)])
    env = {"RECOMP_SCREEN_LOG": "1", "RECOMP_TEST_SQUAD_NUMBERS": "screen"}
    r, _ = fixtures.run(program, game, mode, user, smoke_run.scaled_steps(mode, folder, steps(value == "on")), env, 600)
    with open(os.path.join(folder, "stderr.txt"), "w") as f:
        f.write(r.stderr)
    shutil.rmtree(work, ignore_errors=True)
    check(r.returncode == 0, f"{name}: the game ends with code 0 (code {r.returncode})")
    check("message box from" not in r.stderr, f"{name}: no error box")
    check("squad numbers screen: opened 1" in r.stderr, f"{name}: the screen opens")
    return r.stderr


def main(argv):
    if len(argv) != 4:
        print(__doc__)
        return 2
    program, game, out = argv[1:]
    log = play(program, game, out, "on", "on")
    check("squad numbers screen: 99 buttons" in log, "on: the screen draws 99 buttons")
    cleared = CLEARED.findall(log)
    check(len(cleared) == 1 and ("75", cleared[0]) in HELD.findall(log), f"on: staff {cleared} holds 75 after the drop")
    log = play(program, game, out, "classic", "on", "classic")
    cleared = CLEARED.findall(log)
    check(len(cleared) == 1 and ("75", cleared[0]) in HELD.findall(log), f"classic: staff {cleared} holds 75 after the drop")
    log = play(program, game, out, "off", "off")
    check("squad numbers screen: 99 buttons" not in log, "off: the runtime draws no button")
    kept = KEPT.findall(log)
    check(len(kept) == 1 and (kept[0], "75") in SHOWN.findall(log), f"off: staff {kept} keeps 75 on the open screen")
    print("squad numbers screen:", "FAILED" if failures else "passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
