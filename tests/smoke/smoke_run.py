#!/usr/bin/env python3
"""Smoke run: the recompiled game plays the start screen, a new game and some days without a person.

Usage:
    python3 tests/smoke/smoke_run.py <cm0102_recomp> <game folder> <reference folder> [--mode classic|widescreen]
        [--database 3.9.60|3.9.65|3.9.68] [--record] [--tolerance n]
    python3 tests/smoke/smoke_run.py <cm0102_recomp> <game folder> --same-save [--mode classic|widescreen] [--days n] [--ref-build <program>]
        [--ref-050 <0.5.0 program>] [--ref-setup <name>] [--jobs n] [--runs a,b] [--custom-db <folder>]
        [--save start.sav|prematch.sav]
    python3 tests/smoke/smoke_run.py <cm0102_recomp> <game folder> --match [--days n] [--speed very-fast|fastest] [--ref-build <program>] [--sleep-danger]

--tolerance n: a colour channel that differs by n or less counts as equal. The Linux build differs from the
Mac reference pictures by 1 in some channels (the compilers round floating point differently), so its
smoke run uses --tolerance 1.

Same-save check (--same-save): the run plays the same new game once per run in SAME_SAVE_RUNS, with -seed 1, the test
clock (RECOMP_FIXED_CLOCK=1) and the same steps to "Go on Holiday". The runtime saves the game n game
days (default 14) after the first news screen (RECOMP_SAVE_AFTER_DAYS) and stops. The runs:
    normal   every option at its default, faster_days = off
    again    the same as normal: the runs are repeatable
    faster   faster_days = on: faster days changes no game decision
    silent   sound = off: sound changes no game decision
    off      every switch option off: no option changes a game decision
    sleep-off  RECOMP_IDLE_SLEEP=0: the idle sleep (#58) changes no game decision
    rules-on, rules-off  changing_rules (0.6.0 slice 11): a new game with it on has the season part
               bosman_season.dat in its save; from slice 17 it also plays by the rules table of 2001/02 (RULES_ON_PARTS). Every new-game run has
               changing_rules = off unless it sets it (new_game_runs), so the new game is the base game.
    kinds-base  RECOMP_SEASON_CHANGE_TEST=write (0.6.0 slice 16): a rule kind that writes the game in every
               hook changes nothing in the base game, because the base game calls no kind
Each run prints its count of idle sleeps (RECOMP_IDLE_COUNT=1). The check fails when the normal run
has none, here and in the match check.
Each save must be byte for byte the same as the save of "normal". A failed check names the parts of
the save file that differ. The runs play at the same time, about two minutes in all. A later option
that must not change the game adds a run to SAME_SAVE_RUNS.

With --save, each run loads that saved test game (tests/fixtures/fixtures.py) instead of a new game, then
takes the same Go on Holiday steps. The game is saved n game days after the load.

Search check (--same-save --save <name> --search): the runs open the player search and close it before
the holiday: search-off, search-on and search-on-again (full_player_search). Each save is compared with
the save of search-off.

Options box check (--same-save --save <name> --options): the runs open the Bosman Edition Options box from the
side bar before the holiday (0.5.0 slice 6). options and options-again switch Faster holidays off and on and
close the box. options-tabs clicks the tabs Game, Graphics and Game, options-rules switches the default of
Changing rules off and on (0.6.0 slice 13: the open game keeps its own value), and options-backups switches
Save backups off and on, so the box is built as often. Each save
must be the same as the save of options. The runs give the box the same save folder path end
(OPTIONS_USER_SUB).

Match check (--match): the same as the same-save check, but each run loads the saved test game
prematch.sav (tests/fixtures/fixtures.py) and plays the friendly at Basel on Friday 20.7.01 to the end.
The steps answer the Henry bid (Reject), go to the match, set the commentary speed to Very Fast and
kick off (--speed fastest: the Fastest step of #31 instead). At half time they restart the match. After the match, Continue Game and one
Space go on to Saturday 21.7.01 AM, where the runtime saves the game, 5 game days (default) after
the load. Later days are not reached: the Bergkamp rumour of 21.7.01 needs an answer. The runs are
in MATCH_RUNS and play at the same time, each in about two minutes. The mode is widescreen only,
because the match screens of the option match_columns draw only in widescreen.

The run uses a new empty save folder (RECOMP_USER_DIR is a temporary folder), so the game first shows its
settings. Before the start it writes settings.txt with the mode (default: classic). It saves five
screens. With --record it writes them to <reference folder>/<mode>. Without it, it compares them with
the reference pictures there. The reference pictures stay outside git, because they show the
original's screens.

With --database, settings.txt also chooses the data set of the new game, and the reference pictures are in
<reference folder>/<mode>-<data set>. The game folder must hold the data set folders (Data-3.9.60 and
Data-3.9.65, as in a download). Without it, the run uses the default 3.9.68 and <reference folder>/<mode>.

Widescreen mode draws at 1280x800 and the runtime scales every area by 1280/800 across and 800/600
down, so the run scales each click and the date box by the same factors.

Screens and checks:
    1 settings   the first-start settings screen: up to 20% of pixels differ (random background)
    2 setup      the Setup Game start screen: exact match
    3 leagues    Select League(s): exact match
    4 news       the first news screen of the new game, Saturday 14.7.01 AM: exact match
    5 holiday    30 s after "Go on Holiday": the date box differs from screen 4, so days passed

The runs use the SDL dummy audio driver: the sounds take their real time, without noise. A click ends
the intro and its sounds early, so the settings screen shows by 8 s.

The run sets Background Changes to Off on the settings screen, so screens 2 to 4 repeat. The days
pass on holiday, because Continue Game stops at random events (an offer for a player) that need an
answer. The news of the days is random, so screen 5 is only checked for a new date. In 30 s the
game passes about two weeks, with more than five days in every run so far.
"""
import concurrent.futures
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time

# Positions for classic mode, 800x600. A "wait <state>" step holds the later steps until the game
# reaches the state (src/recomp_rt/win/waits.c). The times count in ms from the window opening, and
# after a wait from the moment the state was reached. A wait that does not end in 120 s ends the run
# with code 3 and names the state the game was in.
STEPS = [
    "1000 click 400 300",                   # the intro: a click ends the logos and their sounds (INTRO_CLICK)
    "wait screen:0x823950",                 # the settings screen of a first start (SETTINGS_WAIT)
    "8000 dump {d}/1-settings.bmp",
    "8200 click 522 272",                   # settings: Background Changes Off
    "8500 click 662 524",                   # settings: Ok
    "wait setup",
    "2000 dump {d}/2-setup.bmp",
    "2500 click 277 177",                   # Start New Game
    "wait leagues",
    "2000 dump {d}/3-leagues.bmp",
    "2500 click 705 572",                   # Next (England is selected)
    "wait typing",
    "1000 type Test",
    "3000 click 560 310",
    "4000 type Smoke",
    "7000 click 705 572",                   # Enter Name: Next
    "wait screen:0x821900",
    "1000 click 705 572",                   # Select Nationality: Next
    "wait screen:0x822260",
    "1000 click 155 163",                   # Select Team: Arsenal
    "wait screen:0x454900",
    "1000 click 722 14",                    # Take Control
    "wait screen:0x822aa0",
    "1000 click 705 572",                   # Please Confirm: Yes
    "wait screen:0x81ff10",
    "1000 click 705 572",                   # Manager Status: Finish
    "wait first-news",
    "2000 dump {d}/4-news.bmp",
    "2500 click 45 165",                    # manager menu (the manager's name)
    "4500 click 150 427 | 199 474",  # Go on Holiday (popup items keep their size in widescreen and HD)
    "7000 click 705 572",                   # Please Confirm: Yes
    "37000 dump {d}/5-holiday.bmp",
    "38000 quit",
]
DATE_BOX = (0, 15, 90, 55)  # x0, y0, x1, y1 of the day and date in the left bar
SCREENS = [("1-settings", 0.20), ("2-setup", 0.0), ("3-leagues", 0.0), ("4-news", 0.0), ("5-holiday", None)]


SIZES = {"classic": (800, 600), "widescreen": (1280, 800)}
# The modes that can have their own position after "|": "x y | widescreen x y".
OWN_POSITIONS = ["widescreen"]


def scaled_steps(mode, shots, steps=STEPS):
    """The steps with their x y positions scaled from 800x600 to the mode's screen size."""
    w, h = SIZES[mode]
    out = []
    for step in steps:
        # "| x y": the widescreen position, where it does not scale.
        step, *own = step.split("|")
        parts = step.format(d=shots).split()
        if mode in OWN_POSITIONS and len(own) > OWN_POSITIONS.index(mode):
            parts[2:4] = own[OWN_POSITIONS.index(mode)].split()
        elif parts[0] != "wait" and parts[1] in ("click", "move", "down", "up", "rclick"):
            parts[2] = str(int(parts[2]) * w // 800)
            parts[3] = str(int(parts[3]) * h // 600)
        out.append(" ".join(parts))
    return out


def scaled_box(mode):
    w, h = SIZES[mode]
    x0, y0, x1, y1 = DATE_BOX
    return (x0 * w // 800, y0 * h // 600, x1 * w // 800, y1 * h // 600)


def read_bmp(path):
    """Pixels of an uncompressed 24- or 32-bit BMP as (width, height, bytes per pixel, rows top down)."""
    data = open(path, "rb").read()
    offset = struct.unpack_from("<I", data, 10)[0]
    w, h = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0] // 8
    stride = (w * bpp + 3) & ~3
    rows = [data[offset + y * stride: offset + y * stride + w * bpp] for y in range(abs(h))]
    if h > 0:
        rows.reverse()
    return w, abs(h), bpp, rows


def differing_share(a, b, box=None, tol=0):
    wa, ha, pa, ra = read_bmp(a)
    wb, hb, pb, rb = read_bmp(b)
    if (wa, ha, pa) != (wb, hb, pb):
        return 1.0
    x0, y0, x1, y1 = box or (0, 0, wa, ha)
    diff = 0
    for y in range(y0, y1):
        x, z = ra[y][x0 * pa:x1 * pa], rb[y][x0 * pa:x1 * pa]
        if x != z:
            diff += sum(1 for i in range(0, len(x), pa)
                        if any(abs(c - d) > tol for c, d in zip(x[i:i + pa], z[i:i + pa])))
    return diff / ((x1 - x0) * (y1 - y0))


# The runs of the same-save check: a name and the option lines of settings.txt (after the mode).
SAME_SAVE_RUNS = [
    ("normal", {"faster_days": "off"}),
    ("again", {"faster_days": "off"}),
    ("faster", {"faster_days": "on"}),
    ("silent", {"faster_days": "off", "sound": "off"}),
    ("off", {"space_to_continue": "off", "shortcuts": "off", "sharp_picture": "off", "sound": "off",
             "clickable_names": "off", "attribute_colours": "off", "faster_days": "off",
             "match_columns": "off", "condition_colours": "off", "save_backups": "off", "remember_window": "off",
             "squad_views": "off", "attribute_groups": "off", "foreign_colours": "off",
             "full_player_search": "off", "changing_rules": "off"}),
    ("foreign-on", {"faster_days": "off", "foreign_colours": "on"}),
    ("foreign-off", {"faster_days": "off", "foreign_colours": "off"}),
    ("search-on", {"faster_days": "off", "full_player_search": "on"}),
    ("search-off", {"faster_days": "off", "full_player_search": "off"}),
    # Changing rules (0.6.0 slice 11): a new game with it on gets the season part bosman_season.dat in its save
    # and must equal the game with it off in every other part (RULES_PART). A loaded old save has it off.
    ("rules-on", {"faster_days": "off", "changing_rules": "on"}),
    ("rules-off", {"faster_days": "off", "changing_rules": "off"}),
    # Idle CPU (#58) has no option: a test-only variable turns the sleep of an idle pass off.
    ("sleep-off", {"faster_days": "off", "_env": {"RECOMP_IDLE_SLEEP": "0"}}),
    # Q342: the season code off (slice 11 year sites, the season part, the Choose Season code) gives the save of
    # normal, a 2001/02 game with changing rules off. From slice 13 the base game of 0.6.0 is the reference.
    ("season-code-off", {"faster_days": "off", "_env": {"RECOMP_SEASON_CODE": "off"}}),
    # The season change (0.6.0 slice 16) calls no rule kind in the base game (2001, changing rules off): with the
    # test kind that writes the game in every hook (RECOMP_SEASON_CHANGE_TEST=write) the save is that of normal.
    ("kinds-base", {"faster_days": "off", "_env": {"RECOMP_SEASON_CHANGE_TEST": "write"}}),
    # The fixes of two error boxes of the original (call_up_news.c, euro_finals.c) act only where the original
    # stops at the box. A test-only variable turns them off: the save must equal normal.
    ("box-fixes-off", {"faster_days": "off", "_env": {"RECOMP_BOX_FIXES": "off"}}),
    # The play-off kind (0.6.0 slice 19) runs only in a season game: with its test switch off the base game save
    # is that of normal.
    ("playoffs-off", {"faster_days": "off", "_env": {"RECOMP_PLAYOFFS": "off"}}),
    # The new divisions (0.6.0 slice 20) act only in a season game: with their test switch off the base game save
    # is that of normal.
    ("new-divisions-off", {"faster_days": "off", "_env": {"RECOMP_NEW_DIVISIONS": "off"}}),
    # The Turkish groups of Summer 2002 act only with that database: with their test switch off the save of the
    # 3.9.68 data is that of normal.
    ("turkish-groups-off", {"faster_days": "off", "_env": {"RECOMP_TURKISH_GROUPS": "off"}}),
]

# --ref-050 <0.5.0 program> (Q342): the run ref-0.5.0 plays the 0.5.0 build, and the run text-0.5.0 plays this
# build with the Setup Game text of 0.5.0 (test variable RECOMP_SETUP_TEXT=0.5.0) and the season code off. Its
# save must equal the save of ref-0.5.0, so the new Setup Game text is the only thing that moves memory.
# ref-0.5.0 itself is not compared with normal. --ref-build stays free for the main build.
REF_050 = None
# --ref-setup <name> (with --ref-build): the run setup-<name> plays this build with RECOMP_SETUP_TEXT=<name>, the
# Setup Game of the reference build (0.6.0-preview, or menu-b for the strip of batch S; database.c), and is compared
# with ref-build.
REF_SETUP = None
TEXT_050_RUN = ("text-0.5.0", {"faster_days": "off", "_against": "ref-0.5.0",
                               "_env": {"RECOMP_SETUP_TEXT": "0.5.0", "RECOMP_SEASON_CODE": "off"}})

# The match check (--match): the steps after the load of prematch.sav (fixtures.LOAD_STEPS), positions
# for 800x600 as in STEPS. The times after a wait count from the moment it was reached. Each Space has
# a wait after it, so a busy machine does not lose one. Screens: news 0x7881e0, bid negotiation
# 0x8e9760, reject confirm 0x8efda0, fixtures and results 0x717db0, squad 0x454900, "Proceed to the
# match?" 0x7260f0, match 0x718e90 (RECOMP_SCREEN_LOG=1 prints them). A match event that stops the
# match, for example a red card that opens the tactics screen 0x899ea0, needs more steps: Ok, then
# Resume (label:Resume) at the same place as Kick Off.
MATCH_SAVE = "prematch.sav"
MATCH_STEPS = [
    "1500 press space",                     # news of 16.7.01: the next item
    "3500 press space",                     # the Henry bid: it blocks Continue Game
    "5000 click 172 230",                   # Respond
    "wait screen:0x8e9760",
    "1000 click 502 573",                   # Reject
    "wait screen:0x8efda0",
    "1000 click 705 573",                   # Yes
    "wait news",
    "500 press space",                      # Continue Game: fixtures of 18.7.01
    "wait day:18.7.01",
    "500 press space",                      # results of 18.7.01, the same screen
    "1500 move 400 30",
    "wait continue",
    "500 press space",                      # fixtures of 20.7.01
    "wait day:20.7.01",
    "500 press space",
    "wait screen:0x454900",
    "500 press space",
    "wait screen:0x7260f0",
    "1000 click 705 573",                   # Proceed to the match? Yes
    "wait match",
    "1000 click 45 254",                    # Commentary Speed
    "2500 click 120 255 | 192 344",  # Very Fast (widescreen and HD; the classic position is not checked)
    "4000 click 45 120",                    # Kick Off Match
    "wait label:Re-start 600000",           # half time
    "1000 click 45 120",                    # Re-start Match
    "wait label:Continue 600000",           # full time: Continue Game on the match screen
    "1000 click 45 120",                    # Continue Game: results of 20.7.01
    "wait continue",
    "500 press space",                      # the day goes on to 21.7.01, where the runtime saves
    "600000 quit",
]
# The runs of the match check, as SAME_SAVE_RUNS. Each pair of an option must give the save of normal.
MATCH_RUNS = [
    ("normal", {}),
    ("again", {}),
    ("columns-on", {"match_columns": "on"}),
    ("columns-off", {"match_columns": "off"}),
    ("colours-on", {"attribute_colours": "on"}),
    ("colours-off", {"attribute_colours": "off"}),
    ("condition-on", {"condition_colours": "on"}),
    ("condition-off", {"condition_colours": "off"}),
    ("foreign-on", {"foreign_colours": "on"}),
    ("foreign-off", {"foreign_colours": "off"}),
    ("search-on", {"full_player_search": "on"}),
    ("search-off", {"full_player_search": "off"}),
    ("rules-on", {"changing_rules": "on"}),
    ("rules-off", {"changing_rules": "off"}),
    ("sleep-off", {"_env": {"RECOMP_IDLE_SLEEP": "0"}}),
    ("season-code-off", {"_env": {"RECOMP_SEASON_CODE": "off"}}),
    ("box-fixes-off", {"_env": {"RECOMP_BOX_FIXES": "off"}}),
]
MATCH_DAYS = 5      # 16.7.01 to 21.7.01
# --match --sleep-danger adds this run: the idle sleep without its time rules (RECOMP_IDLE_SLEEP=force), so
# it also sleeps while background matches play. It must FAIL: it shows that the match check catches a sleep
# that changes the match. Not in MATCH_RUNS, because it fails.
SLEEP_DANGER_RUN = ("sleep-force", {"_env": {"RECOMP_IDLE_SLEEP": "force"}})

SAVE_NAME = "samesave.sav"
INTRO_CLICK = "1000 click 400 300"
# The settings screen (build 0x823950) of a first start. Its clicks wait for it: on a slow machine (the
# Windows build under wine in an emulated container) the screen came after 8 s, the clicks missed it, and
# Background Changes stayed on, so every later screen had another background picture. A test whose user
# folder already has game.cfg sees no settings screen and leaves out this wait with the settings clicks.
SETTINGS_WAIT = "wait screen:0x823950"
INTRO_WAIT = 20000  # ms


def save_parts(path):
    """The parts of a saved game: {name: (stored bytes, size, check word)}. The file starts with three
    words (version, flags, count) and a table of count rows of 268 bytes: offset, size, a name of 256
    bytes and a check word. The stored bytes of a part run from its offset to the next offset."""
    data = open(path, "rb").read()
    count = struct.unpack_from("<I", data, 8)[0]
    rows = []
    for i in range(count):
        o = 12 + i * 268
        offset, size = struct.unpack_from("<II", data, o)
        name = data[o + 8:o + 264].split(b"\0")[0].decode("latin-1")
        rows.append((offset, name, size, struct.unpack_from("<I", data, o + 264)[0]))
    rows.sort()
    parts = {}
    for i, (offset, name, size, check) in enumerate(rows):
        end = rows[i + 1][0] if i + 1 < len(rows) else len(data)
        parts[name] = (data[offset:end], size, check)
    return parts


def without_intro_click(steps):
    """The steps without the click that ends the intro, and the steps before the first wait 20 s later.

    With sound the intro takes about 19 s, without it 5.5 s, but both runs load and free the same
    sounds. A click would skip the second sound in one run only, and the memory of the two games would
    differ. The steps before the first wait count from the window opening, so only they move."""
    steps = [s for s in steps if s != INTRO_CLICK]
    first_wait = next(i for i, s in enumerate(steps) if s.startswith("wait "))
    return [f"{int(s.split(' ', 1)[0]) + INTRO_WAIT} {s.split(' ', 1)[1]}" for s in steps[:first_wait]] + \
        steps[first_wait:]


# --ref-build <program>: one more run, with the options of the first run, plays another build (for example the
# build of main). For a change with no option, such as the crash guards: its save must equal the first run's.
REF_BUILD = None
# --jobs n: at most n games at the same time (default: all runs at once). --runs a,b,...: only the first run
# and these runs, for a short check on a busy machine. An unknown name or fewer than two runs stops the check.
JOBS = 0
ONLY_RUNS = None
# --custom-db <folder> (0.6.0, staff reserve): each run copies the folder into Databases of its user folder and
# plays it as a custom data set (database = folder:<name>), for example the 3.9.68 data against the 0.5.0 build.
CUSTOM_DB = None


def compare_runs(program, game, mode, runs, steps, days, save=None, timeout=600, allowed=(), required=None,
                 text_050=False):
    """Plays the runs at the same time, each with its option lines, and compares each saved game with
    the save of the first run. save: a saved game to copy into each user folder first. allowed: names of
    parts that may differ (an accepted difference, for example the parts that depend on the memory layout of
    the game, Q152); a difference in any other part fails. allowed may also be a dict {run name: parts}: then
    only those runs may differ, each in its own parts. required: {run name: part}: that run's save must hold
    the part (0.6.0 slice 11: rules-on of a new game has the season part)."""
    if ONLY_RUNS:
        unknown = [n for n in ONLY_RUNS if n not in [r[0] for r in runs]]
        if unknown:
            raise SystemExit(f"--runs: no run named {', '.join(unknown)} in this check")
        runs = [r for i, r in enumerate(runs) if i == 0 or r[0] in ONLY_RUNS]
        if len(runs) + bool(REF_BUILD) + bool(REF_050 and text_050) < 2:
            raise SystemExit("--runs: fewer than two runs are left, so nothing is compared")
    if REF_BUILD:
        runs = list(runs) + [("ref-build", dict(runs[0][1], _program=REF_BUILD))]
        if REF_SETUP:
            # This build with the Setup Game of the reference build (test variable RECOMP_SETUP_TEXT): its save
            # must equal the save of ref-build, so Setup Game is the only thing that moves memory.
            env = dict(runs[0][1].get("_env", {}), RECOMP_SETUP_TEXT=REF_SETUP)
            runs = list(runs) + [("setup-" + REF_SETUP, dict(runs[0][1], _env=env, _against="ref-build"))]
    if REF_050 and text_050:
        runs = list(runs) + [("ref-0.5.0", dict(runs[0][1], _program=REF_050, _against="")),
                             (TEXT_050_RUN[0], dict(runs[0][1], **TEXT_050_RUN[1]))]
    against = {name: options.get("_against") for name, options in runs}

    def play(name, options, work):
        steps_of_run = options.get("_steps", steps)        # a run may have its own steps and environment
        extra_env = options.get("_env", {})
        run_program = options.get("_program", program)     # and its own build (--ref-build)
        user_sub = options.get("_user_sub", "")              # and a folder between the run and its user folder
        options = {k: v for k, v in options.items() if k not in ("_steps", "_env", "_program", "_user_sub", "_against")}
        run = os.path.join(work, name)
        user, home, shots = (os.path.join(run, d) for d in (os.path.join(user_sub, "user"), "home", "shots"))
        for d in (user, home, shots):
            os.makedirs(d)
        if save:
            shutil.copyfile(save, os.path.join(user, os.path.basename(save)))
        if CUSTOM_DB:      # --custom-db: every run plays this folder as a custom data set
            shutil.copytree(CUSTOM_DB, os.path.join(user, "Databases", os.path.basename(CUSTOM_DB)))
        with open(os.path.join(user, "settings.txt"), "w") as f:
            f.write(f"display = {mode}\n")
            for key, value in options.items():
                f.write(f"{key} = {value}\n")
            if CUSTOM_DB:
                f.write(f"database = folder:{os.path.basename(CUSTOM_DB)}\n")
        env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
                   RECOMP_USER_DIR=user,
                   RECOMP_FIXED_CLOCK="1", RECOMP_SAVE_AFTER_DAYS=str(days), RECOMP_IDLE_COUNT="1",
                   RECOMP_SCRIPT="; ".join(scaled_steps(mode, shots, steps_of_run)), **extra_env)
        start = time.time()
        r = subprocess.run([run_program, "--game", game, "-seed", "1"], env=env, capture_output=True, text=True,
                           timeout=timeout)
        return r, os.path.join(user, SAVE_NAME), time.time() - start

    # The runs play at the same time. The test clock and the wait steps make each run the same on a
    # busy machine.
    saves = {}
    with tempfile.TemporaryDirectory(prefix="cm0102-same-save-") as work:
        with concurrent.futures.ThreadPoolExecutor(min(len(runs), JOBS) if JOBS else len(runs)) as pool:
            jobs = [(name, pool.submit(play, name, options, work)) for name, options in runs]
            failed_runs = 0
            for name, job in jobs:
                r, path, took = job.result()
                if r.returncode != 0 or not os.path.exists(path):
                    print(r.stderr[-2000:])
                    print(f"FAIL  {name}: no saved game (code {r.returncode}) after {took:.0f} s")
                    failed_runs += 1
                    continue
                m = re.search(r"recomp: idle sleeps (\d+)", r.stderr)
                sleeps = int(m.group(1)) if m else None
                print(f"run   {name}: saved after {days} game days in {took:.0f} s, idle sleeps {sleeps}")
                saves[name] = path
                # The idle sleep (#58) must have run in the normal run, or the check proves nothing.
                if name == "normal" and not sleeps:
                    print(f"FAIL  {name}: the idle sleep never ran (idle sleeps {sleeps})")
                    failed_runs += 1
            if failed_runs:
                print("FAIL")
                return 1
        failed = 0
        first = runs[0][0]
        want = open(saves[first], "rb").read()
        for name, _ in runs[1:]:
            if required and name in required and required[name] not in save_parts(saves[name]):
                failed += 1
                print(f"FAIL  {name}: the save has no part {required[name]}")
                continue
            if against.get(name) == "":
                continue                                    # a reference run for another run (ref-0.5.0)
            base = against.get(name) or first
            if open(saves[name], "rb").read() == open(saves[base], "rb").read():
                print(f"ok    {name}: the save is the same as {base}")
                continue
            a, b = save_parts(saves[base]), save_parts(saves[name])
            differ = sorted(k for k in set(a) | set(b) if a.get(k) != b.get(k))
            ok_parts = allowed.get(name, ()) if isinstance(allowed, dict) else allowed
            if differ and set(differ) <= set(ok_parts):
                print(f"ok    {name}: the save differs from {base} only in accepted parts: {', '.join(differ)}")
                continue
            failed += 1
            print(f"FAIL  {name}: the save differs from {base} in: {', '.join(differ) or 'the file header'}")
    print("PASS" if not failed else "FAIL")
    return 1 if failed else 0


# --search: after the load, open the player search (Player & Staff Search, Player Search tab: every player
# of the list, no filter), close it with Cancel, then go on holiday. The runs open the same screen, so a
# difference between them comes from the option alone (0.5.0 slice 5).
SEARCH_STEPS = [
    "2500 click 45 165",                    # the manager's menu
    "4000 click 190 302",                   # Player & Staff Search
    "wait screen:0x808690",
    "3000 click 359 573",                   # Cancel
    "wait screen:0x7881e0",
]
SEARCH_RUNS = [
    ("search-off", {"faster_days": "off", "full_player_search": "off"}),
    ("search-on", {"faster_days": "off", "full_player_search": "on"}),
    ("search-on-again", {"faster_days": "off", "full_player_search": "on"}),
]


# --options: after the load, open the Bosman Edition Options box (Game Options, then its item), switch
# Faster holidays off and on on the Game tab, close the box, then go on holiday. Positions as in
# tests/smoke/options_box_run.py. The drop-down item keeps its size, so it has a widescreen position.
OPTIONS_OPEN = ["1000 click 45 344", "wait popup", "1000 click 175 417 | 225 525", "wait options"]
OPTIONS_CLOSE = ["5500 click 444 524", "wait screen:0x7881e0"]
OPTIONS_STEPS = OPTIONS_OPEN + [
    "1000 click 584 155",                   # the Game tab
    "2500 click 452 367",                   # Faster holidays: Off
    "4000 click 371 367",                   # Faster holidays: On
] + OPTIONS_CLOSE


# The Game tab of the box shows the end of the save folder path, and the text that the game draws changes
# later days (a run folder "same-4" gave another save than "same-0" with the same clicks, also with
# clickable_names = off). So the runs put a long folder of one name before the user folder: the box then
# shows the same path end in every run and mode.
OPTIONS_USER_SUB = "options-box-runs-" + "x" * 100


def options_runs(load, holiday):
    """The runs of the options box check, each with its own steps."""
    switch = load + OPTIONS_STEPS + holiday
    tabs = load + OPTIONS_OPEN + ["1000 click 584 155", "2500 click 304 155", "4000 click 584 155"] + OPTIONS_CLOSE + \
        holiday
    rules = load + OPTIONS_OPEN + ["1000 click 584 155", "2500 click 452 437", "4000 click 371 437"] + \
        OPTIONS_CLOSE + holiday
    backups = load + OPTIONS_OPEN + ["1000 click 584 155", "2500 click 452 402", "4000 click 371 402"] + \
        OPTIONS_CLOSE + holiday

    def run(name, steps):
        return (name, {"faster_days": "on", "_steps": steps, "_user_sub": OPTIONS_USER_SUB})
    return ([run("options", switch), run("options-again", switch), run("options-tabs", tabs),
             run("options-rules", rules), run("options-backups", backups)],)


# The season part of a save (0.6.0 slice 11, src/recomp_rt/win/season.c). A save of a new game with
# changing_rules = on has it, and only that part may differ from the same game with it off.
RULES_PART = "bosman_season.dat"
# Slice 17: with changing rules on, the rules of 2001/02 from the rules table apply to a new game (named
# substitutes, English windows, foreign player caps: Q232), so the game plays differently and these parts of the
# save differ as well. These four are the parts that differ in the new-game run of 2026-10-18 (all four, no
# other: transfer.dat, clubs and staff are equal), so a fifth part that differs fails the check. The base game
# (changing_rules off) is compared as before.
# contract.dat joins them with Setup Game menu B (O107, Max's play test of 0.6.0, accepted overnight for Max's
# morning check): the same build with the menu of the preview (RECOMP_SETUP_TEXT=0.6.0-preview) differs only in
# the four parts and bosman_season.dat, and a heap trace of Setup Game is the same with changing rules on and
# off, so the rules-on game's own memory move meets the new menu's layout. Only the new-game check uses this.
# O323: a 2001 game with changing rules on holds the South American section of the 2006 cycle from its first day,
# and that cycle's league ends on its real last matchday, 12 October 2005 (season_intl_qual.c, end_by_october), so
# the section's part differs too.
RULES_ON_PARTS = ("fifa_rankings.dat", "general.dat", "national_teams.dat", "staff history.tmp", "contract.dat",
                  "World Cup South American Qualifying Section_440.tmp")
# O128 (the standard of O107): in a new game the runs off and foreign-off may differ in club records.dat. The same
# build with the menu of the preview (RECOMP_SETUP_TEXT=0.6.0-preview) equals normal in both, so the Setup Game
# top row's layout (Q422) meets the memory move that those options already make (foreign_colours draws names in
# another colour); it is not decision code. Only those two runs of the new-game check use this.
NEW_GAME_LAYOUT_PARTS = ("club records.dat",)


# A saved test game that opens on another screen than the news ends its load at another wait (0.6.0 slice 11).
# yeovil.sav (a copy of Max's save, 20.8.02) opens on the squad screen: the load ends when Continue Game shows.
LOAD_END = {"yeovil.sav": "wait continue"}


def new_game_runs(runs):
    """The runs of a new game: changing_rules = off unless a run sets it, so that the new game is the base
    game (2001/02, 3.9.68, changing rules off) and its save has no season part, as a save of 0.5.0."""
    return [(name, dict({"changing_rules": "off"}, **options)) for name, options in runs]


def same_save(program, game, mode, days, save_name=None, search=False, options=False):
    # No click ends the intro (without_intro_click).
    if save_name is None:
        steps = [s for s in STEPS if "5-holiday" not in s and "quit" not in s]
        return compare_runs(program, game, mode, new_game_runs(SAME_SAVE_RUNS),
                            without_intro_click(steps) + ["400000 quit"], days,
                            allowed={"rules-on": (RULES_PART,) + RULES_ON_PARTS, "off": NEW_GAME_LAYOUT_PARTS,
                                     "foreign-off": NEW_GAME_LAYOUT_PARTS},
                            required={"rules-on": RULES_PART}, text_050=True)
    # --save: load the saved test game, then the same Go on Holiday steps as the new game.
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fixtures"))
    import fixtures  # noqa: E402  (fixtures.py imports this file)
    save = os.path.join(fixtures.DEFAULT_DIR, save_name)
    if not os.path.exists(save):
        print(f"FAIL: no saved test game {save} (tests/fixtures/fixtures.py says where it comes from)")
        return 1
    holiday = STEPS[STEPS.index("wait first-news") + 1:]
    holiday = [s for s in holiday if "dump" not in s and "quit" not in s]
    if search:
        steps = without_intro_click(fixtures.LOAD_STEPS) + SEARCH_STEPS + holiday + ["400000 quit"]
        return compare_runs(program, game, mode, SEARCH_RUNS, steps, days, save)
    if options:
        groups = options_runs(without_intro_click(fixtures.LOAD_STEPS), holiday + ["400000 quit"])
        codes = [compare_runs(program, game, mode, runs, runs[0][1]["_steps"], days, save) for runs in groups]
        return max(codes)
    load = list(fixtures.LOAD_STEPS)
    if save_name in LOAD_END:
        load[load.index("wait first-news")] = LOAD_END[save_name]
    steps = without_intro_click(load) + holiday + ["400000 quit"]
    return compare_runs(program, game, mode, SAME_SAVE_RUNS, steps, days, save, text_050=True)


# --speed fastest: the Fastest item (#31, commentary.c), one row above Very Fast.
SPEED_CLICKS = {"very-fast": "2500 click 120 255 | 192 344",
                "fastest": "2500 click 120 242 | 192 323"}


def match(program, game, days, speed="very-fast", sleep_danger=False):
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fixtures"))
    import fixtures  # noqa: E402  (fixtures.py imports this file)
    save = os.path.join(fixtures.DEFAULT_DIR, MATCH_SAVE)
    if not os.path.exists(save):
        print(f"FAIL: no saved test game {save} (tests/fixtures/fixtures.py says where it comes from)")
        return 1
    # No click ends the intro (without_intro_click): with the click, the runs on a busy machine played
    # different matches.
    steps = without_intro_click(fixtures.LOAD_STEPS) + [
        SPEED_CLICKS[speed] if s.startswith("2500 click 120 255") else s for s in MATCH_STEPS]
    runs = MATCH_RUNS + ([SLEEP_DANGER_RUN] if sleep_danger else [])
    return compare_runs(program, game, "widescreen", runs, steps, days, save, timeout=900)


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2
    global REF_BUILD, REF_050, REF_SETUP, JOBS, ONLY_RUNS, CUSTOM_DB
    if "--custom-db" in argv[3:]:
        CUSTOM_DB = os.path.abspath(argv[argv.index("--custom-db") + 1])
    if "--jobs" in argv[3:]:
        JOBS = int(argv[argv.index("--jobs") + 1])
        if JOBS < 1:
            print("--jobs needs a number of 1 or more")
            return 2
    if "--runs" in argv[3:]:
        ONLY_RUNS = argv[argv.index("--runs") + 1].split(",")
    if "--ref-build" in argv[3:]:
        REF_BUILD = os.path.abspath(argv[argv.index("--ref-build") + 1])
    if "--ref-050" in argv[3:]:
        REF_050 = os.path.abspath(argv[argv.index("--ref-050") + 1])
    if "--ref-setup" in argv[3:]:
        REF_SETUP = argv[argv.index("--ref-setup") + 1]
    mode = "classic"
    if "--mode" in argv[3:]:
        mode = argv[argv.index("--mode") + 1]
    if mode not in SIZES:
        print(f"unknown mode {mode}: use classic or widescreen")
        return 2
    if "--match" in argv[3:]:
        days = int(argv[argv.index("--days") + 1]) if "--days" in argv[3:] else MATCH_DAYS
        speed = argv[argv.index("--speed") + 1] if "--speed" in argv[3:] else "very-fast"
        if speed not in SPEED_CLICKS:
            print(f"unknown speed {speed}: use very-fast or fastest")
            return 2
        return match(os.path.abspath(argv[1]), os.path.abspath(argv[2]), days, speed, "--sleep-danger" in argv[3:])
    if "--same-save" in argv[3:]:
        days = int(argv[argv.index("--days") + 1]) if "--days" in argv[3:] else 14
        save_name = argv[argv.index("--save") + 1] if "--save" in argv[3:] else None
        return same_save(os.path.abspath(argv[1]), os.path.abspath(argv[2]), mode, days, save_name, "--search" in argv[3:],
                         "--options" in argv[3:])
    program, game, ref = os.path.abspath(argv[1]), os.path.abspath(argv[2]), os.path.abspath(argv[3])
    record = "--record" in argv[4:]
    tol = int(argv[argv.index("--tolerance") + 1]) if "--tolerance" in argv[4:] else 0
    database = argv[argv.index("--database") + 1] if "--database" in argv[4:] else None
    ref = os.path.join(ref, mode + (f"-{database}" if database else ""))
    with tempfile.TemporaryDirectory(prefix="cm0102-smoke-") as work:
        shots = os.path.join(work, "shots")
        home = os.path.join(work, "home")
        os.mkdir(shots)
        os.mkdir(home)
        user = os.path.join(work, "user")  # RECOMP_USER_DIR: the same on Mac and Windows
        os.makedirs(user)
        with open(os.path.join(user, "settings.txt"), "w") as f:
            f.write(f"display = {mode}\n")
            if database:
                f.write(f"database = {database}\n")
        env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
                   RECOMP_SCRIPT="; ".join(scaled_steps(mode, shots)))
        env["RECOMP_USER_DIR"] = user
        # The season tables (names, rules) of the build, as the season checks read them
        # (rules_report_check.py). Without them the game reads <game folder>/Seasons: a download has it, the
        # clean files do not, and the news screen of the two showed other names ("Premier Division").
        table = os.path.join(os.path.dirname(program), "seasons", "rules.tsv")
        if os.path.exists(table) and "RECOMP_RULES_TABLE" not in env:
            env["RECOMP_RULES_TABLE"] = table
        r = subprocess.run([program, "--game", game], env=env, capture_output=True, text=True, timeout=300)
        if r.returncode != 0:
            print(r.stderr[-2000:])
            print(f"FAIL: the game ended with code {r.returncode}")
            return 1
        os.makedirs(ref, exist_ok=True)
        failed = 0
        for name, allowed in SCREENS:
            got = os.path.join(shots, name + ".bmp")
            want = os.path.join(ref, name + ".bmp")
            if not os.path.exists(got):
                print(f"FAIL  {name}: no screen dump")
                failed += 1
                continue
            if allowed is None:
                # The date must move on from the first news screen.
                moved = differing_share(got, os.path.join(shots, "4-news.bmp"), scaled_box(mode))
                ok = moved > 0.02
                print(f"{'ok   ' if ok else 'FAIL '} {name}: {moved:.0%} of the date box changed since 4-news")
            elif record:
                ok = True
                print(f"saved {want}")
            elif not os.path.exists(want):
                ok = False
                print(f"FAIL  {name}: no reference picture {want} (run with --record)")
            else:
                share = differing_share(got, want, tol=tol)
                ok = share <= allowed
                print(f"{'ok   ' if ok else 'FAIL '} {name}: {share:.1%} of pixels differ (allowed {allowed:.0%})")
            if record or not ok:
                try:
                    shutil.copyfile(got, want if record else os.path.join(ref, name + ".fail.bmp"))
                except OSError:
                    print(f"      cannot write {name}.fail.bmp (read-only reference folder)")
            failed += not ok
    print("PASS" if not failed else "FAIL")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
