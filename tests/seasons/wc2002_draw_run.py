#!/usr/bin/env python3
"""The draw of the 2002 World Cup ends in a game whose World Cup pots hold 0xff (open item 166).

Usage:
    python3 tests/seasons/wc2002_draw_run.py <cm0102_recomp> <game folder> --save <in.sav> --keep <folder>
        [--days 6] [--timeout 900] [--keep-faults]

`WorldCup::draw_groups` 0x92e940 draws pot 1 from the pots +0xee before it writes the real 2002 draw. For 2002 nothing
writes the pots (seed_teams 0x92f230 returns at 0x92f240), so they hold what the heap held, and the save keeps them.
One byte of 0xff in the slots 2 to 7 made the pick loop 0x92ea57..0x92ea71 endless, and the game stopped on the
evening of 27 December 2001 (src/recomp_rt/win/original_faults.c, rt_wc2002_pots).

The run loads --save (a game before 27 December 2001 whose pots hold 0xff), waits for Continue Game, goes on holiday
(the manager menu > Go on Holiday > Yes, as tests/smoke/smoke_run.py) and saves --days game days later
(RECOMP_SAVE_AFTER_DAYS, testrun.c), then the program ends. The checks:
    - the log line "recomp: World Cup 2002 draw: <n> pot 1 slots hold 0xff, the pots are cleared"
    - the program ends with code 0 before --timeout seconds and leaves samesave.sav (the day loop got past the draw)
    - no message box
--keep-faults runs with RECOMP_ORIGINAL_FAULTS=keep:wc_pots (give a short --timeout, for example 300). It passes when
the checks find the fault: the line ends in "the pots are kept" and the game stops at the draw (a timeout, no
samesave.sav).

The save: a game of 26 December 2001 whose 2002 pots hold 0xff, kept outside the repo. The run folder holds home/,
user/ (settings.txt, player.sav, samesave.sav) and stderr.txt. The run deletes the game's work files (*.tmp, *.lng)
from user/ at the end.
"""
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import soak_run  # noqa: E402

LINE = "recomp: World Cup 2002 draw: "
STEPS = [
    "1000 click 400 300",                   # the intro
    "8200 click 522 272",                   # settings: Background Changes Off
    "8500 click 662 524",                   # settings: Ok
    "wait setup",
    "1000 click 277 242",                   # Load Game
    "4000 click 445 257",                   # the first save in the list
    "5000 click 705 573",                   # Ok
    "wait continue 300000",                 # a player's save opens on any screen: wait for Continue Game
    "2500 click 45 165",                    # the manager menu
    "4500 click 150 427",                   # Go on Holiday
    "7000 click 705 572",                   # Please Confirm: Yes
]


def main(argv):
    if len(argv) < 2 or "--save" not in argv or "--keep" not in argv:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[0]), os.path.abspath(argv[1])
    save, keep = os.path.abspath(soak_run.arg(argv, "--save")), os.path.abspath(soak_run.arg(argv, "--keep"))
    days, timeout = int(soak_run.arg(argv, "--days", 6)), int(soak_run.arg(argv, "--timeout", 900))
    faults = "--keep-faults" in argv
    user, home = os.path.join(keep, "user"), os.path.join(keep, "home")
    for d in (user, home):
        os.makedirs(d, exist_ok=True)
    for name in ("samesave.sav", "player.sav"):
        if os.path.exists(os.path.join(user, name)):
            os.remove(os.path.join(user, name))
    shutil.copyfile(save, os.path.join(user, "player.sav"))
    with open(os.path.join(user, "settings.txt"), "w") as f:
        f.write("display = classic\nfaster_days = on\n")
    steps = STEPS + [f"{timeout * 1000} quit"]
    env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
               RECOMP_USER_DIR=user, RECOMP_FIXED_CLOCK="1", RECOMP_SCREEN_LOG="1",
               RECOMP_SAVE_AFTER_DAYS=str(days), RECOMP_SCRIPT="; ".join(steps))
    if faults:
        env["RECOMP_ORIGINAL_FAULTS"] = "keep:wc_pots"
    log = os.path.join(keep, "stderr.txt")
    start = time.time()
    with open(log, "w") as out:
        p = subprocess.Popen([program, "--game", game, "-seed", "1"], env=env, stdout=subprocess.DEVNULL, stderr=out)
        try:
            code = p.wait(timeout=timeout + 60)
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait()
            code = "timeout"
    took = time.time() - start
    for name in os.listdir(user):      # the game's work files (up to 1 GB): the run keeps only its saves
        if name.endswith((".tmp", ".lng")):
            os.remove(os.path.join(user, name))
    text = open(log, encoding="latin-1").read().split("\n")
    lines = [x for x in text if x.startswith(LINE)]
    print(f"{'keep:wc_pots' if faults else 'fix'}: code {code} after {took:.0f} s; " +
          (lines[0] if lines else "no World Cup 2002 draw line"))
    boxes = [x.strip() for x in text if "recomp: message box from" in x]
    saved = os.path.exists(os.path.join(user, "samesave.sav"))
    if faults:      # the proof that the checks find the fault
        found = [x for x in lines if x.endswith("the pots are kept")] and code == "timeout" and not saved
        for x in boxes:
            print(f"FAIL: {x}")
        print("PASS: the checks find the fault" if found and not boxes else
              "FAIL: with keep:wc_pots the game did not stop at the draw")
        return 0 if found and not boxes else 1
    fails = []
    if not [x for x in lines if x.endswith("the pots are cleared")]:
        fails.append("no line that the 2002 pots were cleared (the save holds no 0xff, or the fix did not act)")
    if code != 0:
        fails.append(f"the program did not end with code 0 ({code}): the day loop stopped")
    if not saved:
        fails.append(f"no samesave.sav {days} days after the load")
    fails += boxes
    for x in fails:
        print(f"FAIL: {x}")
    if not fails:
        print("PASS")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
