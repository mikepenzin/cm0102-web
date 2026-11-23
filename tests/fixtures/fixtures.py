#!/usr/bin/env python3
"""Saved test games: make them once, then start feature checks from them instead of a new game.

Usage:
    python3 tests/fixtures/fixtures.py make <cm0102_recomp> <game folder> [--dir <fixtures folder>] [--all]
    python3 tests/fixtures/fixtures.py play <cm0102_recomp> <game folder> <save name> <steps file> <out folder>
        [--dir <fixtures folder>] [--mode classic|widescreen] [--set key=value]... [--days n]

The saves stay outside git, because they hold the original's data. The default folder is
/Users/max/Projects/cm0102/test-fixtures (FIXTURES_DIR in the environment changes it).

make: plays a new game with the steps of the smoke run (tests/smoke/smoke_run.py), -seed 1 and the
test clock, to the first news screen, and saves it there as start.sav: Test Smoke, Arsenal, Saturday
14.7.01. The steps wait for the screens, so the make takes about a minute after the database load.
With --all the new game loads every league (Select All on Select League(s), England 01/02 on Select Start
Season) and is saved as allleagues.sav: Test Smoke, Arsenal, 14.7.01. The load takes about six minutes.

Saves in the folder:
    start.sav     made by "make"
    allleagues.sav  made by "make --all" (tests/smoke/player_search_run.py --big)
    prematch.sav  Arsenal before the friendly at Basel on Friday 20.7.01, with a bid to answer. It was
                  made by hand in a test session on 2026-10-15 (no script makes it yet). Put it in the
                  folder by hand. The match check (tests/smoke/smoke_run.py --match) plays from it.

play: copies the save into a new empty user folder, writes settings.txt (the mode and each --set line),
loads the save with wait steps and then runs the steps of the file. Each line of the file is a script
step (src/recomp_rt/win/user32.c and waits.c) with positions for the mode's screen size; "{d}" stands for
the out folder. The times of the first steps count from the moment the loaded game shows its first news
screen. The run uses the test clock and -seed 1, so two runs of the same steps play the same game.
With --days n the runtime saves the game n game days after the load as <out>/samesave.sav and ends
(RECOMP_SAVE_AFTER_DAYS). Screen dumps (.bmp) get a .png copy.

Example, an option must not change the game: play the same steps twice with --set match_columns=on and
--set match_columns=off and --days 6, then compare the two samesave.sav files byte for byte.
"""
import os
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "smoke"))
import smoke_run  # noqa: E402

DEFAULT_DIR = os.environ.get("FIXTURES_DIR", "/Users/max/Projects/cm0102/test-fixtures")

# Load a save from the first start, positions for 800x600. The save list of the Load Game screen shows
# the saves of the user folder in name order. With one save, it is the first row.
LOAD_STEPS = [
    "1000 click 400 300",                   # the intro: a click ends the logos
    smoke_run.SETTINGS_WAIT,                # the settings screen: the clicks count from it (item 155)
    "8200 click 522 272",                   # settings: Background Changes Off
    "8500 click 662 524",                   # settings: Ok
    "wait setup",
    "1000 click 277 242",                   # Load Game
    "4000 click 445 257",                   # the first save in the list
    "5000 click 705 573",                   # Ok
    "wait first-news",
]


def run(program, game, mode, user, steps, env_extra, timeout):
    home = os.path.join(os.path.dirname(user), "home")
    os.makedirs(home, exist_ok=True)
    env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
               RECOMP_USER_DIR=user, RECOMP_FIXED_CLOCK="1", RECOMP_SCRIPT="; ".join(steps), **env_extra)
    start = time.time()
    # errors="replace": the game writes Latin-1 names (for example a club name with 0xf3) into its log.
    r = subprocess.run([program, "--game", game, "-seed", "1"], env=env, capture_output=True, text=True,
                       errors="replace", timeout=timeout)
    return r, time.time() - start


# The data set of the base game (Q152). The option's default is 3.9.60 since 0.6.1 (item 11), so a test that
# names no data set pins 3.9.68.
BASE_DATABASE = "3.9.68"


def write_settings(user, mode, options):
    with open(os.path.join(user, "settings.txt"), "w") as f:
        f.write(f"display = {mode}\n")
        if not any(key == "database" for key, _ in options):
            f.write(f"database = {BASE_DATABASE}\n")
        for key, value in options:
            f.write(f"{key} = {value}\n")


# make --all: the steps of Select League(s) and Select Team for a game of every league (800x600). The load
# of every league takes minutes, so the waits after it are long.
ALL_LEAGUES = [
    "2500 click 592 155",                   # Select All
    "4500 click 705 572",                   # Next
    "wait screen:0x81e8d0",                 # Select Start Season
    "3000 click 217 285",                   # England 01/02
    "5000 click 705 572",                   # Next: the database load
]
ALL_TEAM = [
    "1000 click 155 190",                   # Select Team: Arsenal, first row (the Nation menu covers the header)
    "wait screen:0x454900",
    "1000 click 722 14",                    # Take Control
]


def make(program, game, folder, all_leagues=False):
    os.makedirs(folder, exist_ok=True)
    steps = [s for s in smoke_run.STEPS if "dump" not in s]
    steps = steps[:steps.index("wait first-news") + 1] + ["600000 quit"]
    name = "start.sav"
    if all_leagues:
        name = "allleagues.sav"
        k = steps.index("2500 click 705 572")
        steps[k:k + 1] = ALL_LEAGUES
        k = steps.index("1000 click 155 163")
        steps[k:k + 3] = ALL_TEAM
        steps = [s + " 1500000" if s in ("wait typing", "wait first-news") else s for s in steps]
    with tempfile.TemporaryDirectory(prefix="cm0102-fixture-") as work:
        user = os.path.join(work, "user")
        os.makedirs(user)
        write_settings(user, "classic", [])
        r, took = run(program, game, "classic", user, smoke_run.scaled_steps("classic", work, steps),
                      {"RECOMP_SAVE_AFTER_DAYS": "0"}, 2400 if all_leagues else 900)
        save = os.path.join(user, smoke_run.SAVE_NAME)
        if r.returncode != 0 or not os.path.exists(save):
            print(r.stderr[-2000:])
            print(f"FAIL: no saved game (code {r.returncode})")
            return 1
        shutil.copyfile(save, os.path.join(folder, name))
    print(f"saved {os.path.join(folder, name)} in {took:.0f} s")
    return 0


def play(program, game, folder, name, steps_file, out, mode, options, days):
    save = os.path.join(folder, name)
    if not os.path.exists(save):
        print(f"no saved game {save}: see the docstring")
        return 2
    out = os.path.abspath(out)
    os.makedirs(out, exist_ok=True)
    extra = []
    for line in open(steps_file):
        line = line.strip()
        if line and not line.startswith("#"):
            extra.append(line.format(d=out))
    steps = smoke_run.scaled_steps(mode, out, LOAD_STEPS) + extra
    if not any("quit" in s.split()[:2] for s in extra):
        steps.append("1800000 quit")
    env = {"RECOMP_SAVE_AFTER_DAYS": str(days)} if days is not None else {}
    with tempfile.TemporaryDirectory(prefix="cm0102-play-") as work:
        user = os.path.join(work, "user")
        os.makedirs(user)
        shutil.copyfile(save, os.path.join(user, name))
        write_settings(user, mode, options)
        r, took = run(program, game, mode, user, steps, env, 2400)
        with open(os.path.join(out, "stderr.txt"), "w") as f:
            f.write(r.stderr)
        result = os.path.join(user, smoke_run.SAVE_NAME)
        if os.path.exists(result):
            shutil.copyfile(result, os.path.join(out, smoke_run.SAVE_NAME))
    for f in sorted(os.listdir(out)):
        if f.endswith(".bmp") and shutil.which("sips"):
            subprocess.run(["sips", "-s", "format", "png", os.path.join(out, f), "--out",
                            os.path.join(out, f[:-4] + ".png")], capture_output=True)
    print(f"code {r.returncode} after {took:.0f} s, log in {os.path.join(out, 'stderr.txt')}")
    if days is not None and not os.path.exists(os.path.join(out, smoke_run.SAVE_NAME)):
        print("FAIL: no saved game")
        return 1
    return 0 if r.returncode == 0 else 1


def main(argv):
    if len(argv) < 4 or argv[1] not in ("make", "play"):
        print(__doc__)
        return 2
    args = argv[2:]
    opts = {"--dir": DEFAULT_DIR, "--mode": "classic", "--days": None}
    sets = []
    all_leagues = "--all" in args
    args = [a for a in args if a != "--all"]
    rest = []
    i = 0
    while i < len(args):
        if args[i] == "--set":
            key, _, value = args[i + 1].partition("=")
            sets.append((key.strip(), value.strip()))
            i += 2
        elif args[i] in opts:
            opts[args[i]] = args[i + 1]
            i += 2
        else:
            rest.append(args[i])
            i += 1
    if opts["--mode"] not in smoke_run.SIZES:
        print(f"unknown mode {opts['--mode']}: use classic or widescreen")
        return 2
    if argv[1] == "make":
        if len(rest) != 2:
            print(__doc__)
            return 2
        return make(os.path.abspath(rest[0]), os.path.abspath(rest[1]), opts["--dir"], all_leagues)
    if len(rest) != 5:
        print(__doc__)
        return 2
    days = int(opts["--days"]) if opts["--days"] is not None else None
    return play(os.path.abspath(rest[0]), os.path.abspath(rest[1]), opts["--dir"], rest[2], rest[3], rest[4],
                opts["--mode"], sets, days)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
