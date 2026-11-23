#!/usr/bin/env python3
"""The arrow menu of a competition history screen lists the competition's own group (issue 163).

Usage:
    python3 tests/seasons/comp_menu_run.py <cm0102_recomp> <game folder> --keep <folder> [--year 2025]
        [--database <folder>] [--changing-rules on|off] [--show]

The run is the start-only run of tests/seasons/rules_report_check.py with RECOMP_COMP_MENU_TEST=1. At the first start
report the runtime (rt_comp_menu_test, src/recomp_rt/win/screen_sweep.c) opens and builds the history screen of each
competition and prints the items of the arrow menu that comp_screens_add_related_comps_menu 0x49d070 added.

The checks: the run ends with code 0 and the test ends, each world club competition of the game has a menu, no such
menu holds a European competition, and each menu holds the competition itself. Two menus are left out of that last
check. No league table entry holds the Friendly record, so the original gives it the fallback entry 3. The base game
(2001, changing rules off) stays the original, whose Inter-American Cup has the same fallback (0x6695ac). --show prints
every menu.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LINE = re.compile(r"recomp: comp menu: (.*?) \[(.*?)\]: (.*)$", re.M)
WORLD = ("Intercontinental Cup", "Club World", "Suruga")
EUROPE = ("Champions League", "Champions Cup", "European Cup", "UEFA", "Europa League", "Intertoto", "Conference League",
          "Super Cup")


def is_world(name):
    return any(w in name for w in WORLD)


def main(argv):
    if len(argv) < 3 or "--keep" not in argv:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = os.path.abspath(argv[argv.index("--keep") + 1])
    year = argv[argv.index("--year") + 1] if "--year" in argv else "2025"
    base_game = year == "2001" and "--changing-rules" in argv and argv[argv.index("--changing-rules") + 1] == "off"
    cmd = [sys.executable, os.path.join(HERE, "rules_report_check.py"), "--run", program, game, "--year", year,
           "--start-only", "--keep", keep]
    for flag in ("--database", "--changing-rules"):
        if flag in argv:
            cmd += [flag, argv[argv.index(flag) + 1]]
    run = subprocess.run(cmd, env=dict(os.environ, RECOMP_COMP_MENU_TEST="1"), stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True)
    log = os.path.join(keep, "stderr.txt")
    text = open(log, encoding="latin-1").read() if os.path.exists(log) else ""
    fails = []
    if "run: code 0 " not in run.stdout:
        fails.append("the start-only run did not end with code 0 (" + log + ")")
    if "recomp: comp menu: done" not in text:
        fails.append("the menu test did not end (" + log + ")")
    menus = [(n, s, i.split("|")) for n, s, i in LINE.findall(text)]
    world = [m for m in menus if is_world(m[0])]
    own_group_exempt = {"Friendly"} | ({"Inter-American Cup"} if base_game else set())
    if not world:
        fails.append("no world club competition among the menus")
    for name, short, items in menus:
        if "--show" in argv or is_world(name):
            print(f"{name}: {' | '.join(items)}")
        if short not in items and name not in own_group_exempt:
            fails.append(f"{name}: the menu does not hold {short}")
    for name, _, items in world:
        wrong = [i for i in items if any(e in i for e in EUROPE) and not is_world(i)]
        if wrong:
            fails.append(f"{name}: the menu lists {', '.join(wrong)}")
    if "run: code 0 " not in run.stdout:
        print(run.stdout[-1500:])
    for f in fails:
        print("FAIL  " + f)
    print(f"{len(menus)} menus read")
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
