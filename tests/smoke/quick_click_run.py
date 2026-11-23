#!/usr/bin/env python3
"""Counts clicks on the Tactics button that do not open the tactics screen (0.7.2, forum report of 13 November
2026: "I sometimes click on tactics and it won't click into it").

    python3 tests/smoke/quick_click_run.py <cm0102_recomp> <game folder> [--mode classic|widescreen]
        [--kind tap|gap:MS|click] [--times n] [--out folder]

It is a check to run by hand, not a ctest: one run loads start.sav (tests/fixtures/fixtures.py) and takes about
two minutes. The run opens the Arsenal squad screen, then n times clicks Tactics (169 527), waits up to 3 s for
the tactics screen 0x899ea0, and clicks its Ok back to the squad screen. The clicks go through SDL as from a mouse
or a trackpad (script steps tap, sdldown and sdlup of user32.c), so they take the path of a real click:
    tap      the down and the up in one batch of SDL events (a trackpad tap, or any click while a frame is long)
    gap:MS   the up MS ms after the down
    click    the click of the other scripts (its up waits for the game, item 154): the control
A click that does not open the screen is a miss (RECOMP_SCRIPT_SOFT_WAIT=1 goes on after the wait). The run then
clicks Ok of the squad screen's place 705 573, which on the squad screen is empty space, and goes on.

Pass rule: 0 misses. --no-hold runs the same build without the fix (RECOMP_PRESS_HOLD=0), to show the fault. The output names the misses and
the stderr is in --out.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fixtures"))
import smoke_run  # noqa: E402
import fixtures  # noqa: E402

SQUAD = "0x454900"
TACTICS = "0x899ea0"


def click_steps(kind, x, y):
    if kind == "tap":
        return [f"1500 tap {x} {y}"]
    if kind == "click":
        return [f"1500 click {x} {y}"]
    gap = int(kind.split(":")[1])
    return [f"1500 sdldown {x} {y}", f"{1500 + gap} sdlup {x} {y}"]


def steps(kind, times):
    out = smoke_run.without_intro_click(smoke_run.load_steps(fixtures)) + [
        "2500 click 45 165",                    # the manager's menu
        "4000 click 150 156 | 199 203",  # Arsenal Squad
        f"wait screen:{SQUAD}",
    ]
    for _ in range(times):
        out += click_steps(kind, 169, 527) + [  # Tactics
            f"wait screen:{TACTICS} 3000",
            "1500 click 705 573",                   # Ok (on the squad screen: empty space)
            f"wait screen:{SQUAD} 5000",
        ]
    return out + ["2000 quit"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("program")
    ap.add_argument("game")
    ap.add_argument("--mode", default="classic", choices=sorted(smoke_run.SIZES))
    ap.add_argument("--kind", default="tap")
    ap.add_argument("--times", type=int, default=20)
    ap.add_argument("--out", default=None)
    ap.add_argument("--no-hold", action="store_true", help="RECOMP_PRESS_HOLD=0: the build without the fix")
    a = ap.parse_args()
    save = os.path.join(fixtures.DEFAULT_DIR, "start.sav")
    with tempfile.TemporaryDirectory(prefix="cm0102-quick-click-") as work:
        user, home = os.path.join(work, "user"), os.path.join(work, "home")
        os.makedirs(user)
        os.makedirs(home)
        shutil.copyfile(save, os.path.join(user, "start.sav"))
        fixtures.write_settings(user, a.mode, [])
        env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
                   RECOMP_USER_DIR=user, RECOMP_FIXED_CLOCK="1", RECOMP_SCRIPT_SOFT_WAIT="1", RECOMP_SCREEN_LOG="1", RECOMP_CLICK_LOG="1",
                   **({"RECOMP_PRESS_HOLD": "0"} if a.no_hold else {}),
                   RECOMP_SCRIPT="; ".join(smoke_run.scaled_steps(a.mode, work, steps(a.kind, a.times))))
        r = subprocess.run([a.program, "--game", a.game, "-seed", "1"], env=env, capture_output=True, text=True,
                           errors="replace", timeout=1800)
    if a.out:
        os.makedirs(a.out, exist_ok=True)
        with open(os.path.join(a.out, f"stderr-{a.mode}-{a.kind.replace(':', '-')}{'-nohold' if a.no_hold else ''}.txt"), "w") as f:
            f.write(r.stderr)
    opened = len(re.findall(rf"^recomp: script reached screen:{TACTICS}", r.stderr, re.M))
    missed = len(re.findall(rf"^recomp: script missed screen:{TACTICS}", r.stderr, re.M))
    lost = len(re.findall(rf"^recomp: script missed screen:{SQUAD}", r.stderr, re.M))
    print(f"{a.mode} {a.kind}{' no hold' if a.no_hold else ''}: {opened} opened, {missed} missed of {a.times} (code {r.returncode})")
    if lost or opened + missed != a.times or r.returncode != 0:
        print(r.stderr[-2000:])
        print("FAIL: the run left the squad screen or did not end")
        return 1
    return 0 if missed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
