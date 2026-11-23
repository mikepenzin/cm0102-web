#!/usr/bin/env python3
"""Open folder check (0.6.1 item 5): the Open folder buttons of the options box and of Load Game.

Usage:
    python3 tests/smoke/open_folder_run.py <cm0102_recomp> <game folder> <out folder> [--dir <fixtures folder>]

Each run starts the game with RECOMP_OPEN_FOLDER_PROGRAM set to a script that writes its arguments to a file, in
place of open or xdg-open (host_open_folder, src/recomp_rt/host/posix.c). From the start screen it opens the
Bosman Edition Options box, the Game tab, clicks Open folder on the Save folder row, closes the box, opens Load
Game and clicks Open folder there. Two runs at a time.

Runs:
    default     classic, no save_folder: both buttons open the user folder.
    custom      widescreen, save_folder = ~/My CM Saves 'x' (with the "~" form and a quote): both buttons open
                that folder under the run's home folder. start.sav is in it, so Load Game lists it.
    picked      classic: Change picks another folder first (RECOMP_PICK_FOLDER). The box's button opens the
                picked folder (the folder of the next start, as the row shows it); Load Game opens the folder
                in use.
    none        classic, RECOMP_OPEN_FOLDER_PROGRAM names no program: the game logs "failed" and the row shows
                the note "The folder could not be opened".

Checks: each button gives one start of the program with one argument, the expected folder. Pictures (with .png
copies) in <out>/<run>/: game.bmp (the Game tab), game-open.bmp (after the click), load.bmp and load-open.bmp.
"""
import os
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import fixtures  # noqa: E402
import smoke_run  # noqa: E402

# Positions for 800x600; the box and Load Game scale as every area does. Box rows: y = 192 + 35 * row, and the
# Save folder row is row 8 of the Game tab: Change at x 380, Open folder at 473. The Load Game button: 275 532.
OPEN_BOX = ["1500 click 277 425", "3500 click 584 155", "4500 dump {d}/game.bmp"]
BOX_OPEN_FOLDER = ["5000 click 473 472", "6500 dump {d}/game-open.bmp"]
# The picked run: Change first, then Open folder (the times of a step count from the last wait).
BOX_CHANGE_THEN_OPEN = ["5000 click 380 472", "5600 dump {d}/game-picked.bmp", "6000 click 473 472",
                        "6600 dump {d}/game-open.bmp"]
CLOSE_THEN_LOAD = ["7000 click 444 524", "wait setup", "1000 click 277 242", "4000 dump {d}/load.bmp",
                   "4500 click 275 532", "6000 dump {d}/load-open.bmp", "7000 quit"]
SETUP = fixtures.LOAD_STEPS[:fixtures.LOAD_STEPS.index("wait setup") + 1]

FAKE_OPEN = """#!/bin/sh
echo "count $# arg $1" >> "$OPEN_LOG"
"""


def png(folder):
    for f in sorted(os.listdir(folder)):
        if f.endswith(".bmp") and shutil.which("sips"):
            subprocess.run(["sips", "-s", "format", "png", os.path.join(folder, f), "--out",
                            os.path.join(folder, f[:-4] + ".png")], capture_output=True)


def main(argv):
    args = argv[1:]
    folder = fixtures.DEFAULT_DIR
    if "--dir" in args:
        i = args.index("--dir")
        folder = args[i + 1]
        del args[i:i + 2]
    if len(args) != 3:
        print(__doc__)
        return 2
    program, game, out = (os.path.abspath(p) for p in args)
    work = tempfile.mkdtemp(prefix="cm0102-open-folder-")
    fake = os.path.join(work, "fake-open.sh")
    with open(fake, "w") as f:
        f.write(FAKE_OPEN)
    os.chmod(fake, 0o755)

    def run(name, mode, settings, steps, program_env=fake, extra_env=None, save_in=None):
        user = os.path.join(work, name, "user")
        home = os.path.join(work, name, "home")      # fixtures.run uses this home folder
        shots = os.path.join(out, name)
        os.makedirs(user)
        os.makedirs(home)
        os.makedirs(shots, exist_ok=True)
        fixtures.write_settings(user, mode, settings)
        target = save_in(home) if save_in else user
        os.makedirs(target, exist_ok=True)
        shutil.copyfile(os.path.join(folder, "start.sav"), os.path.join(target, "start.sav"))
        log = os.path.join(work, name, "open.log")
        env = dict({"RECOMP_OPEN_FOLDER_PROGRAM": program_env, "OPEN_LOG": log}, **(extra_env or {}))
        r, _ = fixtures.run(program, game, mode, user, smoke_run.scaled_steps(mode, shots, SETUP + steps), env, 600)
        with open(os.path.join(shots, "stderr.txt"), "w") as f:
            f.write(r.stderr)
        png(shots)
        lines = open(log).read().splitlines() if os.path.exists(log) else []
        return r, lines, user, home

    custom_dir = lambda home: os.path.join(home, "My CM Saves 'x'")
    picked = os.path.join(work, "Picked Folder")
    os.makedirs(picked)
    plans = {
        "default": ("classic", [], OPEN_BOX + BOX_OPEN_FOLDER + CLOSE_THEN_LOAD, {}),
        "custom": ("widescreen", [("save_folder", "~/My CM Saves 'x'")], OPEN_BOX + BOX_OPEN_FOLDER + CLOSE_THEN_LOAD,
                   {"save_in": custom_dir}),
        "picked": ("classic", [], OPEN_BOX + BOX_CHANGE_THEN_OPEN + CLOSE_THEN_LOAD,
                   {"extra_env": {"RECOMP_PICK_FOLDER": picked}}),
        "none": ("classic", [], OPEN_BOX + BOX_OPEN_FOLDER + CLOSE_THEN_LOAD,
                 {"program_env": "cm0102-no-such-file-manager"}),
    }
    with ThreadPoolExecutor(max_workers=2) as pool:
        jobs = {name: pool.submit(run, name, mode, settings, steps, **kw)
                for name, (mode, settings, steps, kw) in plans.items()}
        results = {name: job.result() for name, job in jobs.items()}

    ok = True

    def check(passed, text):
        nonlocal ok
        print(("PASS " if passed else "FAIL ") + text)
        ok = ok and passed

    r, lines, user, home = results["default"]
    check(r.returncode == 0, f"default: exit code {r.returncode}")
    check(lines == [f"count 1 arg {user}"] * 2, f"default: both buttons open the user folder: {lines}")

    r, lines, user, home = results["custom"]
    want = custom_dir(home)
    check(r.returncode == 0, f"custom: exit code {r.returncode}")
    check(lines == [f"count 1 arg {want}"] * 2, f"custom: both buttons open {want}: {lines}")

    r, lines, user, home = results["picked"]
    check(r.returncode == 0, f"picked: exit code {r.returncode}")
    check(lines == [f"count 1 arg {picked}", f"count 1 arg {user}"],
          f"picked: the box opens the picked folder, Load Game the folder in use: {lines}")

    r, lines, user, home = results["none"]
    check(r.returncode == 0, f"none: exit code {r.returncode}")
    check(not lines and r.stderr.count(f"recomp: open folder {user}: failed") == 2,
          "none: both buttons log that the file manager could not start")

    shutil.rmtree(work, ignore_errors=True)
    print("PASS" if ok else "FAIL", f"(pictures in {out})")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
