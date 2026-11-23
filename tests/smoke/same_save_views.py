#!/usr/bin/env python3
"""Same-save checks of the 0.4.0 screens: a feature must not change the game (spec 2026-10-15-features-0.4.0).

Usage:
    python3 tests/smoke/same_save_views.py <cm0102_recomp> <game folder> [--check compare|print|tactics|match-tactics]
        [--days n] [--dir <fixtures folder>]

Each check plays the same game from start.sav (tests/fixtures) in runs at the same time, goes on holiday
for n game days (default 2) and compares the saved games byte for byte (smoke_run.compare_runs).

    compare   The original's "Compare two chosen players" screen (Seaman and Shaaban, set with the prompts of
              the profile's Action drop-down) with squad_views on and off, in classic and in widescreen mode.
              The squad_views screen must make the same calls that change the game (the training manager's
              call, the random numbers). Its widgets sit in other memory blocks, so the five parts of the
              save that depend on the memory layout (LAYOUT_PARTS, Q152) may differ; no other part may.
    print     The squad printed from the squad screen's Print drop-down as a PDF (the default) and as a File
              (Print To in Game Settings), with the same clicks otherwise. The PDF message box must not
              change the game (Q152): the texts of the box are in runtime memory, not on the game's heap.
    tactics   The tactics screen in its Condition view (Squad, Assistant > Pick best team, Tactics, Ok) with
              match_columns on and off, in classic and in widescreen mode (#48, 0.5.0). The option adds a
              positions label to each row and gives the list new column weights. Both move the game's
              memory blocks (research of slice 3: each alone changes the five parts), so the five parts of
              LAYOUT_PARTS may differ (Q152) between on and off; "on again" must equal "on".
    match-tactics The same in a match (prematch.sav, the Basel friendly of the match check), with a
              substitution at half time, in classic and widescreen (--mode m1,m2 to choose). Not HD: there
              an injury before half time opens the tactics screen, and these steps do not handle it. The game
              is saved after the match (5 days). --shots <folder> keeps the screen of each run.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
sys.path.insert(0, HERE)
import fixtures  # noqa: E402
import smoke_run  # noqa: E402

LAYOUT_PARTS = smoke_run.LAYOUT_PARTS

# Positions for 800x600; "| x y" gives the widescreen position where the game does not scale. The holiday is the one
# of smoke_run.STEPS, with a wait for the manager's menu and for Please Confirm (item 185).
HOLIDAY = [s for s in smoke_run.STEPS[smoke_run.STEPS.index("wait first-news") + 1:]
           if " dump " not in s and not s.endswith(" quit")]

COMPARE = [
    "2500 click 45 165",                    # the manager's menu
    "4000 click 150 156 | 199 203",  # Arsenal Squad
    "wait screen:0x454900",
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
    "3000 click 358 573",                   # Back
    "wait screen:0x870700",
]

TACTICS = [
    "2500 click 45 165",                    # the manager's menu
    "4000 click 150 156 | 199 203",  # Arsenal Squad
    "wait screen:0x454900",
    "1500 click 592 135",                   # Assistant
    "3000 click 540 159 | 928 205",  # Pick best team
    "5000 click 169 527",                   # Tactics: it opens in the Condition view
    "wait screen:0x899ea0",
    "2000 click 705 573",                   # Ok
    "wait screen:0x454900",
]

# Print To is three buttons of Game Settings: PDF at 444 474, File at 522 474, Both at 601 474.
OPTIONS = {"pdf": ["click 45 345", "click 148 397", "click 444 474", "click 662 524"],
           "file": ["click 45 345", "click 148 397", "click 522 474", "click 662 524"]}
PRINT_SQUAD = ["click 45 165", "click 148 156", "click 750 25", "click 700 49"]   # menu, squad, Print, Squad


def print_steps(choice, times=1, boxes=None):
    """The steps of a print run: Print To, then the squad printed times times, then a holiday. The times
    count from the last wait of the load, each after the one before. boxes: how many PDF message boxes show
    (RECOMP_TEST_NO_PDF_BOX=boxes+1); a box that does not show gets no click. None: all."""
    steps, t = [], 2000

    def add(action, gap=1000):
        nonlocal t
        t += gap
        steps.append(f"{t} {action}")

    for a in OPTIONS[choice]:               # the same clicks: only the Print To button differs
        add(a)
    for n in range(times):
        for a in PRINT_SQUAD if n == 0 else PRINT_SQUAD[2:]:     # the squad screen is open for the later prints
            add(a)
        if choice == "pdf":
            if boxes is None or n < boxes:
                add("click 399 330", 3000)  # the message with the path: Ok
            else:
                t += 3000
        else:
            add(f"type squadtxt{n}", 4000)  # the Save Printout dialog: a new name each time, then Save
            add("click 705 573", 3000)
    add("click 45 165", 4000)               # holiday, with the waits of HOLIDAY
    return steps + HOLIDAY[1:]


def run_compare(program, game, days, folder):
    steps = smoke_run.without_intro_click(smoke_run.load_steps(fixtures)) + COMPARE + HOLIDAY + ["400000 quit"]
    failed = 0
    for mode in ("classic", "widescreen"):
        print(f"--- comparison screen, {mode}")
        runs = [("on", {"squad_views": "on"}), ("off", {"squad_views": "off"}), ("on again", {"squad_views": "on"})]
        failed += smoke_run.compare_runs(program, game, mode, runs, steps, days,
                                         os.path.join(folder, "start.sav"), allowed=LAYOUT_PARTS)
    return failed


def run_print(program, game, days, folder):
    """The message box of a PDF print must leave the game as it found it (Q152): the texts of the box are in
    runtime memory, not on the game's heap. The saved game of a PDF print with the second message box equals
    that of one without it (RECOMP_TEST_NO_PDF_BOX=2), after two and three prints.

    The first box of a run is not compared: the game's box code (0x5e8290) keeps a buffer for the pixels
    under the box (0x5e3dd0) that it makes when it first shows, and since 0.4.0 the "CD Not Required" box,
    which used to make it at the load, no longer shows. So the first box moves the game's memory blocks like
    opening a screen (Q152) and changes the five parts that depend on the layout (fifa_rankings, general,
    national_teams, staff history, transfer). Nor is a PDF run compared with a run that does not print, or
    with a File run (the Save Printout dialog is the original's code and changes much more)."""
    base = smoke_run.without_intro_click(smoke_run.load_steps(fixtures))
    save = os.path.join(folder, "start.sav")
    failed = 0
    for times in (2, 3):
        runs = [(f"pdf x{times}", {"_steps": base + print_steps("pdf", times) + ["400000 quit"]}),
                (f"pdf x{times}, no box after the first",
                 {"_steps": base + print_steps("pdf", times, 1) + ["400000 quit"],
                  "_env": {"RECOMP_TEST_NO_PDF_BOX": "2"}}),
                (f"pdf x{times} again", {"_steps": base + print_steps("pdf", times) + ["400000 quit"]})]
        print(f"--- the squad printed {times} times as PDF, with and without the later message boxes, classic")
        failed += smoke_run.compare_runs(program, game, "classic", runs, base, days, save)
    return failed


def run_tactics(program, game, days, folder):
    steps = smoke_run.without_intro_click(smoke_run.load_steps(fixtures)) + TACTICS + HOLIDAY + ["400000 quit"]
    failed = 0
    for mode in ("classic", "widescreen"):
        print(f"--- tactics screen, Condition view, {mode}")
        runs = [("on", {"match_columns": "on"}), ("off", {"match_columns": "off"}),
                ("on again", {"match_columns": "on"})]
        failed += smoke_run.compare_runs(program, game, mode, runs, steps, days,
                                         os.path.join(folder, "start.sav"), allowed={"off": LAYOUT_PARTS})
    return failed


# The in-match Condition view (--check match-tactics): the match check of smoke_run.py (prematch.sav, the
# Basel friendly). At half time the steps open Arsenal Tactics, choose the Condition view, drag Pascal Cygan
# (a substitute) onto Edu (a substitution, so the status column shows "sub"/"on" with the minute), dump the
# screen and close it with Ok. Then the match goes on as in the match check.
MATCH_TACTICS = [
    "1500 click 45 210",                    # Arsenal Tactics
    "wait screen:0x899ea0",
    "1500 click 297 90 | 475 120",   # View
    "3000 click 289 133 | 467 163",  # Condition
    "5000 down 127 373",                    # the number box of Pascal Cygan (row 12)
    "5300 move 127 330",
    "5600 move 127 277",
    "5900 up 127 277",                      # onto Edu (row 7)
    "8000 dump {d}/condition.bmp",
    "9000 click 705 573",                   # Ok
    "wait screen:0x591b70",                 # Please Confirm: Cygan replaces Edu. Return to the match?
    "1000 click 705 573",                   # Yes
    "wait label:Re-start 600000",
]


def run_match_tactics(program, game, days, folder, modes=("classic", "widescreen"), shots=None):
    half = smoke_run.MATCH_STEPS.index("wait label:Re-start 600000") + 1
    failed = 0
    for mode in modes:
        print(f"--- tactics screen in the match, Condition view, {mode}")
        runs = []
        for name, value in (("on", "on"), ("off", "off"), ("on again", "on")):
            tactics = MATCH_TACTICS
            if shots:                       # keep the dump of each run
                out = os.path.join(os.path.abspath(shots), f"{mode}-{name.replace(' ', '-')}")
                os.makedirs(out, exist_ok=True)
                tactics = [t.replace("{d}", out) for t in tactics]
            # The substitution changes the second half, which then has no MATCH_EVENT (item 155).
            steps = smoke_run.without_intro_click(smoke_run.load_steps(fixtures)) + smoke_run.MATCH_STEPS[:half] + \
                tactics + smoke_run.MATCH_STEPS_ORIGINAL[half:]
            runs.append((name, {"match_columns": value, "_steps": steps}))
        failed += smoke_run.compare_runs(program, game, mode, runs, [], days,
                                         os.path.join(folder, smoke_run.MATCH_SAVE), timeout=900,
                                         allowed={"off": LAYOUT_PARTS})
    return failed


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    days = int(argv[argv.index("--days") + 1]) if "--days" in argv else 2
    folder = argv[argv.index("--dir") + 1] if "--dir" in argv else fixtures.DEFAULT_DIR
    which = argv[argv.index("--check") + 1] if "--check" in argv else "all"
    failed = 0
    if which in ("all", "compare"):
        failed += run_compare(program, game, days, folder)
    if which in ("all", "print"):
        failed += run_print(program, game, days, folder)
    if which in ("all", "tactics"):
        failed += run_tactics(program, game, days, folder)
    if which in ("all", "match-tactics"):
        shots = argv[argv.index("--shots") + 1] if "--shots" in argv else None
        modes = argv[argv.index("--mode") + 1].split(",") if "--mode" in argv else ("classic", "widescreen")
        failed += run_match_tactics(program, game, smoke_run.MATCH_DAYS, folder, modes, shots)
    print("same_save_views: " + ("FAIL" if failed else "all checks passed"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
