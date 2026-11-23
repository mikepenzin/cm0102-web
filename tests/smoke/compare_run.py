#!/usr/bin/env python3
"""Comparison check (spec 2026-10-15-features-0.4.0, slice 6): the polygon on the comparison screen and the
Compare players tab of the Team views screen.

Usage:
    python3 tests/smoke/compare_run.py <cm0102_recomp> <game folder> <out folder> [--dir <fixtures folder>]
        [--mode classic|widescreen]... [--language n]

One run from start.sav (Arsenal, 14.7.01) for each mode (classic and widescreen when no --mode is given),
at the same time. The tour:
    - own squad > Assistant > Team views > Compare players (the first two of the squad) > the first name
      box > Seaman > the second name box > (classic: More players > the box again) > Shaaban > Back;
    - Seaman's profile > Action > Compare Players > Set as 1st player > Yes > Back > Shaaban's profile >
      Set as 2nd player > Yes > Compare two chosen players (the comparison screen) > Back > the manager's
      menu > Compare two chosen players (side bar item 0x434) > the first name box (the profile);
    - Liverpool's Compare tab, then the worst case for space on the comparison screen (WORST): Van Bronckhorst
      against the goalkeeper Arphexad of another club.
Screen dumps of each step in <out>/<mode>. --language n plays the tour in language n of game.cfg (1 English to
10 Swedish, as tests/smoke/groups_language_run.py counts them): the goalkeepers come from the position ratings,
not from the positions text in that language (0.6.1 item 8).

The runtime writes a line for each build of a comparison (RECOMP_VIEWS_LOG=1), and "recomp: comparison
screen built" for each build of the original's screen:
    recomp: compare staff <s1> <s2> keepers <0|1> columns <1|2> rows <n> attr1 <31 values> attr2 <31>
        axes1 <8 tenths> axes2 <8 tenths> marks <n>

Checks:
    - every axis value is the average of its shown attributes, rounded half up to tenths (Q110, Q117), with
      the goalkeeper axes when both players are goalkeepers
    - every attribute value is 0 (hidden) to 20
    - the Compare tab opens on two outfield players of the squad, the drop-downs change both sides, and two
      goalkeepers get the goalkeeper axes
    - the prompts choose the same two goalkeepers for the comparison screen, which opens from the profile
      and from the side bar, and its name box opens the profile
    - a goalkeeper against an outfield player: each side lists its own player's attributes (the goalkeeper's
      side Handling and Reflexes, not Tackling; the other side the reverse), and the polygon has the outfield axes
    - no hidden attribute of another club's player is shown: the Compare tab of Liverpool's Team views screen
      holds every flagged attribute as 0 (hidden_check.py)
    - after the Compare tab, the Best XI tab has no pixel of the polygon colours (4b dump), while the Compare
      tab's dump has them
    - no text of the tour is cut by its box (RECOMP_TEXT_CUT_LOG=1): the names, the lines under them, the
      legend and the heading fit in both modes (Tousled12, 2026-10-21)
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fixtures"))
import fixtures  # noqa: E402
import smoke_run  # noqa: E402
import hidden_check  # noqa: E402
import lng_fit_run  # noqa: E402

LANGUAGE = None                         # --language n: game.cfg in language n

# Positions for 800x600; "| x y" gives the widescreen position where the game does not scale
# (popups).
TOUR = [
    "2500 click 45 165",                    # the manager's menu
    "4000 click 150 156 | 199 203",  # Arsenal Squad
    "wait screen:0x454900",
    "1500 click 592 135",                   # Assistant
    "3000 click 540 241 | 928 288",  # Team views
    "5000 click 675 97",                    # Compare players: Vieira and Henry, the first two of the squad
    "7000 dump {d}/1-compare-tab.bmp",
    "8000 click 160 137 | 220 180",  # the first name box
    "9500 dump {d}/2-squad-list.bmp",
    "10500 click 160 327 | 220 370",  # David Seaman
    "12500 click 610 137 | 1080 180",  # the second name box
    # Classic: More players (page 2). Widescreen and HD: Rami Shaaban on page 1.
    "14000 click 610 538 | 1140 641",
    "16000 click 610 137 | 1080 180",  # the second name box again
    "17500 dump {d}/3-second-list.bmp",
    # Classic: Rami Shaaban on page 2. Widescreen and HD: a click beside the list closes it.
    "18500 click 720 223 | 712 365",
    "20500 dump {d}/4-two-keepers.bmp",
    "21500 click 215 97",                   # the Best XI tab: no polygon may stay on it
    "24000 dump {d}/4b-best-xi-after-compare.bmp",
    "25000 click 358 573",                  # Back
    "wait screen:0x454900",
    # The original's prompts on the profile's Action drop-down > Compare Players.
    "1500 click 210 234",                   # David Seaman
    "wait screen:0x870700",
    "1500 click 722 14",                    # Action
    "3000 move 700 267 | 1170 275",
    "3500 move 695 267 | 1166 275",  # Compare Players
    "4500 move 530 269 | 1001 277",
    "5000 click 530 269 | 1001 277",  # Set as 1st player in comparison
    "wait screen:0x8f4210",
    "1500 click 704 572",                   # Yes
    "wait screen:0x870700",
    "1500 click 358 573",                   # Back
    "wait screen:0x454900",
    "1500 click 545 234",                   # Rami Shaaban
    "wait screen:0x870700",
    "1500 click 722 14",                    # Action
    "3000 move 700 267 | 1170 275",
    "3500 move 695 267 | 1166 275",  # Compare Players
    "4500 move 530 290 | 1001 298",
    "5000 click 530 290 | 1001 298",  # Set as 2nd player in comparison
    "wait screen:0x8f4210",
    "1500 click 704 572",                   # Yes
    "wait screen:0x870700",
    "1500 click 722 14",                    # Action
    "3000 move 700 267 | 1170 275",
    "3500 move 695 267 | 1166 275",  # Compare Players
    "4500 move 520 310 | 1001 318",
    "5000 click 520 310 | 1001 318",  # Compare two chosen players
    "wait screen:0x876520",
    "2000 dump {d}/5-comparison-screen.bmp",
    "3000 click 358 573",                   # Back
    "wait screen:0x870700",
    "1500 click 45 165",                    # the manager's menu
    "3000 click 150 323 | 199 370",  # Compare two chosen players (side bar item 0x434)
    "wait screen:0x876520",
    "2000 dump {d}/6-from-side-bar.bmp",
    "3000 click 200 88 | 220 117",  # the first name box: a link to the profile
    "wait screen:0x870700",
    "2000 dump {d}/7-name-link.bmp",
    # Liverpool's Compare tab: another club's players, so the game hides some attributes.
    "1500 click 45 165",                    # the manager's menu
    "4000 click 150 156 | 199 203",  # Arsenal Squad
    "wait screen:0x454900",
    "1500 click 584 527",                   # Premier Division
    "wait screen:0x48fcd0",
    "1500 click 215 462",                   # Liverpool
    "3500 click 169 97",                    # Squad
    "5500 click 592 135",                   # Assistant
    "7000 click 540 159 | 928 205",  # Team views, the only item
    "9000 click 675 97",                    # Compare players
    "11000 dump {d}/8-other-club-compare.bmp",
]

# Classic only (a popup's rows are at other places in the other modes): a Liverpool reserve of whom the game
# hides 11 attributes (John Welsh), from page 2 of the first drop-down of the start.sav.
HIDDEN_CLASSIC = [
    "12000 click 160 137",                  # the first name box
    "13500 click 160 538",                  # More players (font 2 name boxes since 0.6.0: one row fewer)
    "15000 click 160 137",                  # the first name box again
    "16500 click 160 265",                  # John Welsh (one row lower since the page holds one name less)
    "18000 dump {d}/9-hidden-attributes.bmp",
]

# The worst case for space (Tousled12, 2026-10-21): Liverpool's goalkeeper Pegguy Arphexad as the 2nd player
# (another club's profile has a shorter Action drop-down) and Arsenal's Giovanni Van Bronckhorst as the 1st, on
# the comparison screen: the longest heading of the save, a long name in the legend and a goalkeeper's side.
WORST = [
    "22000 click 358 573",                  # Back from Liverpool's Team views screen
    "wait screen:0x454900",
    "1500 click 228 208",                   # Pegguy Arphexad
    "wait screen:0x870700",
    "1500 click 722 14",                    # Action
    "3000 move 700 183 | 1170 191",
    "3500 move 695 183 | 1166 191",  # Compare Players
    "4500 move 530 206 | 1001 214",
    "5000 click 530 206 | 1001 214",  # Set as 2nd player in comparison
    "wait screen:0x8f4210",
    "1500 click 704 572",                   # Yes
    "wait screen:0x870700",
    "1500 click 45 165",                    # the manager's menu
    "4000 click 150 156 | 199 203",  # Arsenal Squad
    "wait screen:0x454900",
    "1500 click 595 346",                   # Giovanni Van Bronckhorst
    "wait screen:0x870700",
    "1500 click 722 14",                    # Action
    "3000 move 700 267 | 1170 275",
    "3500 move 695 267 | 1166 275",  # Compare Players
    "4500 move 530 269 | 1001 277",
    "5000 click 530 269 | 1001 277",  # Set as 1st player in comparison
    "wait screen:0x8f4210",
    "1500 click 704 572",                   # Yes
    "wait screen:0x870700",
    "1500 click 722 14",                    # Action
    "3000 move 700 267 | 1170 275",
    "3500 move 695 267 | 1166 275",  # Compare Players
    "4500 move 520 310 | 1001 318",
    "5000 click 520 310 | 1001 318",  # Compare two chosen players
    "wait screen:0x876520",
    "2000 dump {d}/10-worst-case.bmp",
]

# The text box writes "recomp: text cut ..." for a text wider than its box (RECOMP_TEXT_CUT_LOG=1, names.c).
CUT = re.compile(r"recomp: text cut .*")

# The attributes of each axis (src/recomp_rt/win/compare_layout.c), by the getter's numbers 1 to 31.
A = {n: i + 1 for i, n in enumerate(
    "acceleration aggression agility anticipation balance bravery creativity crossing decisions determination "
    "dribbling finishing flair handling heading influence jumping long_shots marking off_the_ball pace passing "
    "positioning reflexes set_pieces stamina strength tackling teamwork technique work_rate".split())}
OUTFIELD = [["tackling", "marking", "positioning"], ["strength", "stamina", "agility", "balance"],
            ["pace", "acceleration"], ["passing", "creativity", "flair"],
            ["finishing", "off_the_ball", "long_shots"], ["technique", "dribbling", "crossing"],
            ["heading", "jumping"], ["decisions", "anticipation", "work_rate", "teamwork", "determination"]]
KEEPER = [["reflexes", "agility"], ["handling"], ["jumping", "bravery"], ["positioning", "influence"],
          ["passing", "technique"], ["pace", "acceleration"], ["strength", "stamina", "balance"],
          ["decisions", "anticipation", "work_rate", "teamwork", "determination"]]

LINE = re.compile(r"recomp: compare staff (-?\d+) (-?\d+) keepers (\d) columns (\d) rows (\d+) attr1 ([\d ]+) "
                  r"attr2 ([\d ]+) axes1 ([-\d ]+) axes2 ([-\d ]+) marks (\d+) sides (\d) (\d) "
                  r"hidden1 ([01 ]+) hidden2 ([01 ]+) listed1 ([01 ]+) listed2 ([01 ]+)")


def run(program, game, folder, mode, out):
    os.makedirs(out, exist_ok=True)
    work = tempfile.mkdtemp(prefix="cm0102-compare-")
    user = os.path.join(work, "user")
    os.makedirs(user)
    shutil.copyfile(os.path.join(folder, "start.sav"), os.path.join(user, "start.sav"))
    fixtures.write_settings(user, mode, [])
    load = fixtures.LOAD_STEPS
    if LANGUAGE:
        if not lng_fit_run.language_cfg(program, game, user, LANGUAGE):
            return 1, "no game.cfg"
        load = [s for s in load if s not in lng_fit_run.SETTINGS + [smoke_run.SETTINGS_WAIT]]
    tour = TOUR + (HIDDEN_CLASSIC if mode == "classic" else []) + WORST
    steps = smoke_run.scaled_steps(mode, out, load) + smoke_run.scaled_steps(mode, out, tour)
    steps.append("20000 quit" if mode == "classic" else "13000 quit")
    r, took = fixtures.run(program, game, mode, user, steps, {"RECOMP_SCREEN_LOG": "1", "RECOMP_VIEWS_LOG": "1",
                                                            "RECOMP_TEXT_CUT_LOG": "1"},
                           1200)
    with open(os.path.join(out, "stderr.txt"), "w") as f:
        f.write(r.stderr)
    for f in sorted(os.listdir(out)):
        if f.endswith(".bmp") and shutil.which("sips"):
            subprocess.run(["sips", "-s", "format", "png", os.path.join(out, f), "--out",
                            os.path.join(out, f[:-4] + ".png")], capture_output=True)
    return r.returncode, r.stderr


def builds(log):
    """The comparisons in log order, without repeats of the same build."""
    out = []
    for m in LINE.finditer(log):
        b = {"staff": (int(m.group(1)), int(m.group(2))), "keepers": int(m.group(3)), "columns": int(m.group(4)),
             "rows": int(m.group(5)), "attr": ([int(v) for v in m.group(6).split()],
                                               [int(v) for v in m.group(7).split()]),
             "axes": ([int(v) for v in m.group(8).split()], [int(v) for v in m.group(9).split()]),
             "marks": int(m.group(10)), "sides": (int(m.group(11)), int(m.group(12))),
             "hidden": ([int(v) for v in m.group(13).split()], [int(v) for v in m.group(14).split()]),
             "listed": ([int(v) for v in m.group(15).split()], [int(v) for v in m.group(16).split()])}
        if not out or out[-1] != b:
            out.append(b)
    return out


def axis_value(names, attr):
    shown = [attr[A[n] - 1] for n in names if attr[A[n] - 1]]
    if not shown:
        return -1
    return (sum(shown) * 20 + len(shown)) // (2 * len(shown))


def check_values(name, b):
    problems = []
    axes = KEEPER if b["keepers"] else OUTFIELD
    for side in (0, 1):
        attr = b["attr"][side]
        if len(attr) != 31 or any(not 0 <= v <= 20 for v in attr):
            problems.append(f"{name}: player {side + 1} has values outside 0 to 20: {attr}")
            continue
        want = [axis_value(names, attr) for names in axes]
        if b["axes"][side] != want:
            problems.append(f"{name}: player {side + 1} axes {b['axes'][side]}, expected {want}")
    return problems


HANDLING, REFLEXES, TACKLING = A["handling"], A["reflexes"], A["tackling"]


def check_sides(name, bs):
    """A goalkeeper against an outfield player: each side has the layout of its own player (item 6)."""
    mixed = [b for b in bs if b["sides"] in ((1, 0), (0, 1))]
    if not mixed:
        return [f"{name}: no comparison of a goalkeeper against an outfield player in the log"]
    problems = []
    for b in mixed:
        for side in (0, 1):
            listed, keeper = b["listed"][side], b["sides"][side]
            if bool(listed[HANDLING - 1]) != bool(keeper) or bool(listed[REFLEXES - 1]) != bool(keeper):
                problems.append(f"{name}: side {side + 1} (keeper {keeper}) lists Handling/Reflexes "
                                f"{listed[HANDLING - 1]}/{listed[REFLEXES - 1]}")
            if bool(listed[TACKLING - 1]) == bool(keeper):
                problems.append(f"{name}: side {side + 1} (keeper {keeper}) lists Tackling {listed[TACKLING - 1]}")
        if b["keepers"]:
            problems.append(f"{name}: a goalkeeper against an outfield player has the goalkeeper axes")
    return problems


def colour_pixels(path, rgb, tol=6, middle=False):
    """The pixels of a dump within tol of an RGB colour in each channel; middle: only in the middle panel of the
    comparison (x 35 % to 65 %, y 22 % to 90 % of the screen), away from the heading and the name boxes."""
    w, h, bpp, rows = smoke_run.read_bmp(path)
    n = 0
    for y, row in enumerate(rows):
        if middle and not 0.22 * h <= y <= 0.9 * h:
            continue
        for i in range(0, len(row), bpp):
            if middle and not 0.35 * w <= i // bpp <= 0.65 * w:
                continue
            if (abs(row[i + 2] - rgb[0]) <= tol and abs(row[i + 1] - rgb[1]) <= tol and
                    abs(row[i] - rgb[2]) <= tol):
                n += 1
    return n


# Player 1 and player 2: the original's green [0xae31aa] and red [0xae31a8] (compare.c). The original's pitch
# lines on the Best XI tab are the same green, so only red tells that no polygon stays there.
POLYGON_COLOURS = ((0, 130, 0), (255, 0, 0))


def check_polygon_gone(name, folder):
    """The Compare tab's dump holds the polygon colours; the Best XI tab after it holds none."""
    problems = []
    tab, after = os.path.join(folder, "4-two-keepers.bmp"), os.path.join(folder, "4b-best-xi-after-compare.bmp")
    if not (os.path.exists(tab) and os.path.exists(after)):
        return [f"{name}: the dumps of the Compare tab and the Best XI tab after it are missing"]
    for rgb in POLYGON_COLOURS:
        shown, stale = colour_pixels(tab, rgb, middle=True), colour_pixels(after, rgb, middle=True)
        if shown < 50:
            problems.append(f"{name}: the Compare tab has {shown} pixels of {rgb}, expected the polygon")
        if rgb == (255, 0, 0) and stale > 4:
            problems.append(f"{name}: the Best XI tab after the Compare tab has {stale} pixels of {rgb}")
    return problems


def check(name, code, log, folder=None):
    problems = []
    if code != 0:
        problems.append(f"{name}: exit code {code}")
    bs = builds(log)
    if not bs:
        return problems + [f"{name}: no comparison in the log"]
    for b in bs:
        problems += check_values(name, b)
        for side in (0, 1):
            for k, (v, h) in enumerate(zip(b["attr"][side], b["hidden"][side])):
                if h and v:
                    problems.append(f"{name}: player {side + 1} shows {v} for attribute {k + 1}, which the game hides")
    problems += check_sides(name, bs)
    if name == "classic" and not any(any(h) for b in bs for h in b["hidden"]):
        problems.append(f"{name}: no comparison of a player with a hidden attribute (another club's)")
    if folder:
        problems += check_polygon_gone(name, folder)
    first = bs[0]
    two_keepers = [b for b in bs if b["keepers"]]
    keepers = two_keepers[-1] if two_keepers else bs[-1]
    if first["keepers"] or first["staff"][0] < 0 or first["staff"][1] < 0 or first["staff"][0] == first["staff"][1]:
        problems.append(f"{name}: the Compare tab did not open on two outfield players: {first['staff']}")
    if not keepers["keepers"] or first["staff"][0] in keepers["staff"] or first["staff"][1] in keepers["staff"]:
        problems.append(f"{name}: the drop-downs did not choose two goalkeepers: {keepers['staff']}")
    screens = log.count("recomp: comparison screen built")
    if screens < 3:
        problems.append(f"{name}: the comparison screen was built {screens} times, expected 3 (profile, side bar, "
                        "worst case)")
    for cut in CUT.findall(log):
        problems.append(f"{name}: {cut}")
    tail = log[log.find("recomp: comparison screen built") - 2000:] if screens else ""
    shown = [b for b in builds(tail) if b["keepers"]]
    if not shown or shown[-1]["staff"] != keepers["staff"] or not shown[-1]["keepers"]:
        problems.append(f"{name}: the comparison screen does not show the two chosen goalkeepers "
                        f"{keepers['staff']}: {[b['staff'] for b in shown]}")
    built = [m.start() for m in re.finditer("recomp: comparison screen built", log)]
    if len(built) < 2 or "screen 0x870700" not in log[built[1]:built[2] if len(built) > 2 else len(log)]:
        problems.append(f"{name}: the name box of the comparison screen did not open the profile")
    return problems


def main(argv):
    global LANGUAGE
    args = argv[1:]
    if "--language" in args:
        i = args.index("--language")
        LANGUAGE = int(args[i + 1])
        del args[i:i + 2]
    folder = fixtures.DEFAULT_DIR
    modes = []
    while "--dir" in args or "--mode" in args:
        flag = "--dir" if "--dir" in args else "--mode"
        i = args.index(flag)
        if flag == "--dir":
            folder = args[i + 1]
        else:
            modes.append(args[i + 1])
        del args[i:i + 2]
    if len(args) != 3:
        print(__doc__)
        return 2
    modes = modes or ["classic", "widescreen"]
    program, game, out = (os.path.abspath(a) for a in args)
    with ThreadPoolExecutor(len(modes)) as pool:
        futures = {m: pool.submit(run, program, game, folder, m, os.path.join(out, m)) for m in modes}
        results = {m: f.result() for m, f in futures.items()}
    problems = []
    for m in modes:
        problems += check(m, *results[m], os.path.join(out, m))
    for p in problems:
        print("FAIL:", p)
    print("compare: " + ("FAIL" if problems else "all checks passed") + f" (dumps in {out})")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
