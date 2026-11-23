#!/usr/bin/env python3
"""Smoke run: the recompiled game plays the start screen, a new game and some days without a person.

Usage:
    python3 tests/smoke/smoke_run.py <cm0102_recomp> <game folder> <reference folder> [--mode classic|widescreen]
        [--database 3.9.60|3.9.65|3.9.68] [--record] [--tolerance n]
    python3 tests/smoke/smoke_run.py <cm0102_recomp> <game folder> --same-save [--mode classic|widescreen] [--days n] [--ref-build <program>]
        [--ref-050 <0.5.0 program>] [--ref-setup <name>] [--jobs n] [--runs a,b] [--custom-db <folder>]
        [--save start.sav|prematch.sav|yeovil.sav]
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
    history-keep-all  RECOMP_HISTORY_SEASONS=0: the trim of the club records history and the fixtures to 50
               seasons (open item 192, history_trim.c) changes no game decision
    rules-on, rules-off  changing_rules (0.6.0 slice 11): a new game with it on has the season part
               bosman_season.dat in its save, and it changes the parts of RULES_ON_PARTS. The new-game rules-on runs
               with no rules table (item 257, NO_TABLE). Every new-game run has changing_rules = off unless it sets it
               (new_game_runs), so the new game is the base game.
    host-loops-off  RECOMP_QSORT=original, RECOMP_HOT=original, RECOMP_FTOL=original, RECOMP_CALL_CACHE=0: the
               host copies of hot code of the original (qsort6.c, hot_loops.c, _ftol in crt.c) and the cache of
               calls through an address (rt.c, 0.6.2 item 110) change no game decision
    files-off  RECOMP_FOLDER_CACHE=0, RECOMP_KEEP_FILES=0: the kept folder names of the path lookup
               (folder_names.c) and the kept history file of the season change and of a new game (files.c, the
               scopes 0x7c1350, 0x7bfd50 and 0x5aff00, item 151) change nothing
    rules-on-again  (new game only) the same as rules-on, compared with rules-on in every part: the season part
               is repeatable too (it once held unset host bytes after each changed name)
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
Changing rules off and on (0.6.0 slice 13: the open game keeps its own value), options-backups switches
Save backups off and on, and options-network clicks the tabs Network, Game and Network (0.7.0 slice 6: the
game password field and the list of human managers online), so the box is built as often. Each save
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
import contextlib
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
# with code 3 and names the state the game was in. Each click that opens a screen or a popup has a
# wait for it before the next click (items 154 and 185), so a slow machine cannot put a click before the
# screen that takes it: the manager's menu (popup) and Please Confirm of the holiday (0x6b2d80).
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
    "wait popup",
    "1000 click 150 427 | 199 474",  # Go on Holiday (popup items keep their size in widescreen and HD)
    "wait screen:0x6b2d80",                 # Please Confirm
    "1000 click 705 572",                   # Please Confirm: Yes
    "31000 dump {d}/5-holiday.bmp",
    "32000 quit",
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
        elif parts[0] != "wait" and parts[1] in ("click", "move", "down", "up", "rclick", "tap", "sdldown", "sdlup"):
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
# The groups of original_faults.c. The groups callups (open item 143), confed (open items 157 to 159) and club_regulars
# (open item 160: a squad pick of the first days can be new) can act in a new game or its first days, and the group
# match_numbers acts on each match day (open item 155, match_numbers.c), and squad_moves (open item 187) acts at each
# squad assessment of a club with a linked B side (Shortlist_Manager::assess_club_squad 0x83cb40), and squad_loans (open item 280) at each loan to a
# linked B side (sign_loan_contract 0x4d01e0), so the runs original-faults-keep keep only the others. Group
# callup_counts (open item 300) acts only in a season game, so the base game keeps the original with it on.
ORIGINAL_FAULT_GROUPS = ["managers", "money", "age", "olympics", "ire_super", "nsl", "holiday", "callups", "wc_pots",
                         "confed", "club_regulars", "match_numbers", "squad_moves",
                         "squad_loans", "callup_counts"]
# Group confed moves the dates of the Confederations Cup 2003 that a new game of 2001/02 builds (its stage builder
# 0x58bdb0 runs at the creation), in the base game and with changing rules on alike.
ACTING_GROUPS = ("callups", "confed", "club_regulars", "match_numbers", "squad_moves", "squad_loans")
KEEP_IN_RUNS = "keep:" + ",".join(g for g in ORIGINAL_FAULT_GROUPS if g not in ACTING_GROUPS)


def with_set_keep(env):
    """A run's environment with RECOMP_ORIGINAL_FAULTS of the whole set (keep or keep:<groups>) added to the run's own
    keep list, so that a set with keep:callups keeps that group in every run, original-faults-keep too."""
    outer, inner = os.environ.get("RECOMP_ORIGINAL_FAULTS", ""), env.get("RECOMP_ORIGINAL_FAULTS")
    if inner is None or not (outer == "keep" or outer.startswith("keep:")):
        return env
    if "keep" in (outer, inner):
        return dict(env, RECOMP_ORIGINAL_FAULTS="keep")
    groups = [g for g in (outer[5:] + "," + inner[5:]).split(",") if g]
    return dict(env, RECOMP_ORIGINAL_FAULTS="keep:" + ",".join(dict.fromkeys(groups)))


SAME_SAVE_RUNS = [
    ("normal", {"faster_days": "off"}),
    ("again", {"faster_days": "off"}),
    ("faster", {"faster_days": "on"}),
    ("silent", {"faster_days": "off", "sound": "off"}),
    ("off", {"space_to_continue": "off", "shortcuts": "off", "sharp_picture": "off", "sound": "off",
             "clickable_names": "off", "attribute_colours": "off", "faster_days": "off",
             "match_columns": "off", "condition_colours": "off", "save_backups": "off", "remember_window": "off",
             "squad_views": "off", "attribute_groups": "off", "foreign_colours": "off",
             "full_player_search": "off", "changing_rules": "off", "online_play": "off",
             "squad_numbers_99": "off"}),
    ("foreign-on", {"faster_days": "off", "foreign_colours": "on"}),
    ("foreign-off", {"faster_days": "off", "foreign_colours": "off"}),
    ("search-on", {"faster_days": "off", "full_player_search": "on"}),
    ("search-off", {"faster_days": "off", "full_player_search": "off"}),
    # Changing rules (0.6.0 slice 11): a new game with it on gets the season part bosman_season.dat in its save
    # and must equal the game with it off in every other part (RULES_PART). A loaded old save has it off.
    # From 0.6.2 (Q48) the history rows of editions after the start go in every game, which changes the first-season
    # holders and so the play of a new game: rules-on plays with that fix off and is compared with history-q48-off
    # (q48_runs below).
    ("rules-on", {"faster_days": "off", "changing_rules": "on"}),
    ("rules-off", {"faster_days": "off", "changing_rules": "off"}),
    # Online play (0.7.0 slice 4): acts only in network games, so a single-player game must not change.
    ("online-on", {"faster_days": "off", "online_play": "on"}),
    # Squad numbers 1 to 99 (issue 140): off is the original. On is compared only in a saved game (SQUAD_ON_RUN).
    ("squad99-off", {"faster_days": "off", "squad_numbers_99": "off"}),
    ("online-off", {"faster_days": "off", "online_play": "off"}),
    # Idle CPU (#58) has no option: a test-only variable turns the sleep of an idle pass off.
    ("sleep-off", {"faster_days": "off", "_env": {"RECOMP_IDLE_SLEEP": "0"}}),
    # Open item 192 has no option: the trim to 50 seasons acts in every game, and keeps all with this variable.
    ("history-keep-all", {"faster_days": "off", "_env": {"RECOMP_HISTORY_SEASONS": "0"}}),
    # Q342: the season code off (slice 11 year sites, the season part, the Choose Season code) gives the save of
    # normal, a 2001/02 game with changing rules off. From slice 13 the base game of 0.6.0 is the reference.
    ("season-code-off", {"faster_days": "off", "_env": {"RECOMP_SEASON_CODE": "off"}}),
    # Q48 (0.6.2 slice 7): the reference built from the decision, the game with the history fix off
    # (RECOMP_HISTORY_AFTER_START=off, season_history.c). See q48_runs.
    ("history-q48-off", {"faster_days": "off", "_env": {"RECOMP_HISTORY_AFTER_START": "off"}}),
    # The season change (0.6.0 slice 16) calls no rule kind in the base game (2001, changing rules off): with the
    # test kind that writes the game in every hook (RECOMP_SEASON_CHANGE_TEST=write) the save is that of normal.
    ("kinds-base", {"faster_days": "off", "_env": {"RECOMP_SEASON_CHANGE_TEST": "write"}}),
    # The host copies of hot code of the original (qsort6.c, hot_loops.c, 0.6.1; _ftol, 0.6.2 item 110) and the
    # call cache (rt.c, item 110) have no option: the recompiled code and the plain search give the same save.
    ("host-loops-off", {"faster_days": "off", "_env": {"RECOMP_QSORT": "original", "RECOMP_HOT": "original",
                                                       "RECOMP_FTOL": "original", "RECOMP_CALL_CACHE": "0"}}),
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
    # The date jump switch (0.6.2 slice 1, Q30) only raises the latest start year of a new game to 2300: with it on,
    # a game of the normal start years gives the save of normal.
    ("date-jump-on", {"faster_days": "off", "_env": {"RECOMP_DATE_JUMP": "1"}}),
    ("files-off", {"faster_days": "off", "_env": {"RECOMP_FOLDER_CACHE": "0", "RECOMP_KEEP_FILES": "0"}}),
    # The fixes of faults of the original (0.6.2 slice 11, original_faults.c) act in every game, but only when a
    # small nation's national team loses or misses its manager (at the latest at the first month change in play),
    # for money above 2,147,483,647 (the top wage from about 1.65 billion), at an age above 127 and at an Olympic
    # squad pick inside a senior squad's window, at the season change of the Australian NSL (19 July from 2002,
    # outside the 14 days of each saved game), and for a club on holiday with "Reject all offers" or "below asking
    # price" (open item 132; these runs go on holiday with every setting off), and at the draw of the 2002 World Cup
    # on 27 December 2001 (open item 166): in these runs none of that happens, so with their test switch off the save
    # is that of normal. A saved game whose days pass a month change keeps only the other groups (FAULTS_ACT). The
    # senior call-ups (group callups, open item 143) act at a senior pick that meets a player abroad, which can come
    # in the first days of a game (in a game with every league,
    # Argentina's pick on 14.7.01 refuses 60 players with the original's rule), so this run keeps every other group
    # (KEEP_IN_RUNS). Its proof is a set with RECOMP_ORIGINAL_FAULTS=keep:callups for every run against --ref-build
    # (with_set_keep), and tests/seasons/callups_run.py. The background match numbers (group match_numbers, open item
    # 155) act on every match day, so this run keeps them too. Their proof is the match set with
    # RECOMP_ORIGINAL_FAULTS=keep:match_numbers against --ref-build, and tests/smoke/match_timing_run.py.
    ("original-faults-keep", {"faster_days": "off", "_env": {"RECOMP_ORIGINAL_FAULTS": KEEP_IN_RUNS}}),
    # The merged parts of a save (0.6.2 slice 13, save_parts.c) change only how the temporary files go into the
    # save: with their test switch off the save holds the same files as normal.
    ("save-parts-keep", {"faster_days": "off", "_env": {"RECOMP_SAVE_PARTS": "keep"}}),
    # GitHub issue 136 (0.7.2, season_intl_cups.c): the Asian Cup entrants after the hosts are the best sides by
    # reputation, in every game (Max, 13 November 2026: changing rules on and off). The planned difference is the
    # entrant list of the Asian Cup (asia_nations +0xee) from its draw, which comes first in the year of the 2004
    # edition, outside the 14 days of each saved game. The fix keeps the original's random numbers, so with its test
    # switch on the original's pick the save is that of normal.
    ("asian-pick-off", {"faster_days": "off", "_env": {"RECOMP_ASIAN_PICK": "original"}}),
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

# The parts of a saved game that depend on where the game's memory blocks sit (Q152): any change of memory
# layout, even opening one of the original's own screens, changes them after a few days. Research of slices 3
# and 7 (docs/research/2026-10-15-attribute-groups.md) found these five and no others.
LAYOUT_PARTS = ("fifa_rankings.dat", "general.dat", "national_teams.dat", "staff history.tmp", "transfer.dat")

# The match check (--match): the steps after the load of prematch.sav (fixtures.LOAD_STEPS), positions
# for 800x600 as in STEPS. The times after a wait count from the moment it was reached. Each Space that
# stands for Continue Game has "wait continue" before it and a wait after it, so a busy machine does not
# lose one: the date of a new day comes before the screen of that day offers Continue Game, and a Space
# without Continue Game goes to the game as a plain key, which does not continue (item 154; the runtime
# then prints "Space finds no Continue Game"). Screens: news 0x7881e0, bid negotiation 0x8e9760, reject
# confirm 0x8efda0, fixtures and results 0x717db0, squad 0x454900, "Proceed to the match?" 0x7260f0,
# match 0x718e90 (RECOMP_SCREEN_LOG=1 prints them). A match event that stops the
# match, for example a red card that opens the tactics screen 0x899ea0, needs more steps: Ok, then
# Resume (label:Resume) at the same place as Kick Off. Since item 155 the background matches take their own
# numbers, and the Basel friendly has such an event early in the second half on every machine: MATCH_EVENT.
MATCH_EVENT = [
    "wait screen:0x899ea0",                 # the event opens the tactics screen
    "1000 click 705 573",                   # Ok
    "wait label:Resume",
    "1000 click 45 120",                    # Resume Match
]
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
    "wait continue",
    "500 press space",                      # Continue Game: fixtures of 18.7.01
    "wait day:18.7.01",
    "wait continue",
    "500 press space",                      # results of 18.7.01, the same screen
    "1500 move 400 30",
    "wait continue",
    "500 press space",                      # fixtures of 20.7.01
    "wait day:20.7.01",
    "wait continue",
    "500 press space",
    "wait screen:0x454900",
    "wait continue",
    "500 press space",
    "wait screen:0x7260f0",
    "1000 click 705 573",                   # Proceed to the match? Yes
    "wait match",
    "1000 click 45 254",                    # Commentary Speed
    "wait popup",
    "1000 click 120 255 | 192 344",  # Very Fast (widescreen and HD; the classic position is not checked)
    "wait label:Kick Off",                  # the popup is closed: the left bar shows its buttons again
    "1000 click 45 120",                    # Kick Off Match
    "wait label:Re-start 600000",           # half time
    "1000 click 45 120",                    # Re-start Match
] + MATCH_EVENT + [
    "wait label:Continue 600000",           # full time: Continue Game on the match screen
    "1000 click 45 120",                    # Continue Game: results of 20.7.01
    "wait continue",
    "500 press space",                      # the day goes on to 21.7.01, where the runtime saves
    "600000 quit",
]
# The match check of a run that keeps the group match_numbers: the original's matches have no MATCH_EVENT.
_EVENT_AT = MATCH_STEPS.index(MATCH_EVENT[0])
MATCH_STEPS_ORIGINAL = MATCH_STEPS[:_EVENT_AT] + MATCH_STEPS[_EVENT_AT + len(MATCH_EVENT):]
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
    ("online-on", {"online_play": "on"}),
    ("online-off", {"online_play": "off"}),
    ("squad99-on", {"squad_numbers_99": "on"}),
    ("squad99-off", {"squad_numbers_99": "off"}),
    ("sleep-off", {"_env": {"RECOMP_IDLE_SLEEP": "0"}}),
    ("history-keep-all", {"_env": {"RECOMP_HISTORY_SEASONS": "0"}}),
    ("season-code-off", {"_env": {"RECOMP_SEASON_CODE": "off"}}),
    ("box-fixes-off", {"_env": {"RECOMP_BOX_FIXES": "off"}}),
    ("host-loops-off", {"_env": {"RECOMP_QSORT": "original", "RECOMP_HOT": "original", "RECOMP_FTOL": "original",
                                 "RECOMP_CALL_CACHE": "0"}}),
    ("original-faults-keep", {"_env": {"RECOMP_ORIGINAL_FAULTS": KEEP_IN_RUNS}}),
    ("save-parts-keep", {"_env": {"RECOMP_SAVE_PARTS": "keep"}}),
    ("files-off", {"_env": {"RECOMP_FOLDER_CACHE": "0", "RECOMP_KEEP_FILES": "0"}}),
]
# A saved game holds no number above 50 and no full squad, so the option changes nothing in it. A new game differs by
# plan: it keeps the database numbers from 51 to 99.
SQUAD_ON_RUN = ("squad99-on", {"faster_days": "off", "squad_numbers_99": "on"})
MATCH_DAYS = 5      # 16.7.01 to 21.7.01
# --match --sleep-danger adds this run: the idle sleep without its time rules (RECOMP_IDLE_SLEEP=force), so
# it also sleeps while background matches play. It keeps the original's shared random stream (group match_numbers,
# item 155): with the fix, the count of background steps no longer changes the watched match. It must FAIL: it
# shows that the match check catches a sleep that changes the match. Not in MATCH_RUNS, because it fails.
SLEEP_DANGER_RUN = ("sleep-force", {"_env": {"RECOMP_IDLE_SLEEP": "force",
                                             "RECOMP_ORIGINAL_FAULTS": "keep:match_numbers"}})

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


def save_files(path):
    """The files of a saved game as the game reads them: {name: (decoded bytes, size)}. A merged part (0.6.2 slice
    13, tools/save/savfile.py) gives each file it holds, so a save with merged parts and one without compare by
    their contents."""
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "save"))
    import savfile
    sav = savfile.Sav(path)
    return {m.name: (sav.read_member(m.name), m.size) for m in sav.members
            if not m.name.lower().endswith(savfile.MERGED)}


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
# A season data set that the game knows (seasons_table.c, for example December 2025: role SEASON, 2025) starts in
# its own season (season_screen_year, season_screen.c), so its normal game is a season game and the runs of
# BASE_GAME_RUNS do not apply to it (see there).
CUSTOM_DB = None

# Item 153 (0.6.2): runs whose claim holds for the base game only, the game with start year 2001 and changing
# rules off. With --custom-db the check first proves that the normal game is in the season that the data set gives
# (expected_season, or the season of the loaded save with --save), then leaves these runs out (n/a) when that
# season is not the base game:
# - season-code-off: RECOMP_SEASON_CODE=off starts every game in 2001 and reads and writes no season part. Its
#   claim (season_code_on, season.c, Q342) is that a base game with the switch gives the same save as without it.
#   A season game with the switch is a game in another year.
# - kinds-base: the season change calls the rule kinds in every season game (season_game_changes, season_names.c;
#   docs/research/2026-10-17-season-change.md) and in no base game. With the test kind that writes the game in
#   every hook (RECOMP_SEASON_CHANGE_TEST=write) a season game changes by design.
BASE_GAME_RUNS = ("season-code-off", "kinds-base")
BASE_GAME = (2001, 0)       # SEASON_FIRST_YEAR (season.h), changing rules off
# The known data sets (the source of seasons_table.c, tools/seasons/make_db_table.py) and the roles that start a
# new game in the data set's own season (season_screen_year, season_screen.c).
SIGNATURES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "docs", "research", "rules-0.6.0",
                          "databases-signatures.tsv")
OWN_SEASON_ROLES = ("season database", "extra version")


def season_text(season):
    year, rules = season
    return f"{year}/{(year + 1) % 100:02d} with changing rules {'on' if rules else 'off'}"


def season_of_save(path):
    """(start year, changing rules) of a saved game, from the fields +8 and +12 of the part bosman_season.dat
    (season.c). The game writes the part in every season game (season.c, the test before the part writer), so a
    save without it is the base game."""
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "save"))
    import savfile
    sav = savfile.Sav(path)
    try:
        part = sav.read_member("bosman_season.dat")
    except KeyError:
        return BASE_GAME
    return struct.unpack_from("<II", part, 8)


def expected_season(folder, table=SIGNATURES):
    """(start year, changing rules) of a new game of the runs on the data set folder: the season of a known season
    data set, else 2001. The runs write no season line (season 2001/02 by default) and changing_rules = off
    (new_game_runs). A data set is known by the sizes and SHA-256 of club.dat and staff.dat, as seasons_db_identify
    (seasons_db.c) knows it."""
    import csv
    import hashlib
    files = {n.lower(): os.path.join(folder, n) for n in os.listdir(folder)}
    if "club.dat" not in files or "staff.dat" not in files:
        return BASE_GAME
    sizes = {k: os.path.getsize(files[k]) for k in ("club.dat", "staff.dat")}
    digests = {}
    with open(table, newline="") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            if int(r["club_size"]) != sizes["club.dat"] or int(r["staff_size"]) != sizes["staff.dat"]:
                continue
            if not digests:
                for k in ("club.dat", "staff.dat"):
                    with open(files[k], "rb") as g:
                        digests[k] = hashlib.sha256(g.read()).hexdigest()
            if r["club_sha256"] == digests["club.dat"] and r["staff_sha256"] == digests["staff.dat"]:
                if r["role"] in OWN_SEASON_ROLES:
                    return (int(r["data_season"][:4]), 0)
                return BASE_GAME
    return BASE_GAME


def base_game_only(name, season):
    """None when the run is compared, or the reason it is not: a run of BASE_GAME_RUNS with --custom-db when the
    season of the game (season_want of compare_saves, already checked against the normal save) is not the base
    game."""
    if not CUSTOM_DB or name not in BASE_GAME_RUNS or season is None or tuple(season) == BASE_GAME:
        return None
    return (f"the custom data set plays a season game ({season_text(season)}), and this run is defined for the "
            f"base game only (BASE_GAME_RUNS)")



# RECOMP_SMOKE_KEEP=<folder>: a check that fails (a difference, no saved game, a timeout or an error) keeps its
# work folder in that folder and prints the path. Each run's folder then also holds its stderr.txt. A check that
# passes deletes its folder. Without the switch the work folder goes at the end, as before.
KEEP_FAILED = os.environ.get("RECOMP_SMOKE_KEEP")


@contextlib.contextmanager
def work_folder(prefix, result):
    """A work folder for one check. result: a dict; the check sets result["failed"] before the end. A check that
    ends with no result (an exception or an early return) counts as failed."""
    if KEEP_FAILED:
        os.makedirs(KEEP_FAILED, exist_ok=True)
    work = tempfile.mkdtemp(prefix=prefix, dir=KEEP_FAILED or None)
    try:
        yield work
    finally:
        if KEEP_FAILED and result.get("failed", True):
            print(f"kept  the work folder of the failed check: {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)


def compare_runs(program, game, mode, runs, steps, days, save=None, timeout=600, allowed=(), required=None,
                 text_050=False, rows=None, ref_steps=None):
    """Plays the runs at the same time, each with its option lines, and compares each saved game with
    the save of the first run. save: a saved game to copy into each user folder first. allowed: names of
    parts that may differ (an accepted difference, for example the parts that depend on the memory layout of
    the game, Q152); a difference in any other part fails. allowed may also be a dict {run name: parts}: then
    only those runs may differ, each in its own parts. required: {run name: part}: that run's save must hold
    the part (0.6.0 slice 11: rules-on of a new game has the season part). rows: {run name: keys}: that run's
    accepted difference must also be exactly those nation history rows (q48_rows_ok), or with keys HOST_ROWS only
    the host fields of its nation history rows (host_rows_ok)."""
    if ONLY_RUNS:
        unknown = [n for n in ONLY_RUNS if n not in [r[0] for r in runs]]
        if unknown:
            raise SystemExit(f"--runs: no run named {', '.join(unknown)} in this check")
        keep = set(ONLY_RUNS) | {r[1].get("_against") for r in runs if r[0] in ONLY_RUNS}     # and their references
        runs = [r for i, r in enumerate(runs) if i == 0 or r[0] in keep]
        if len(runs) + bool(REF_BUILD) + bool(REF_050 and text_050) < 2:
            raise SystemExit("--runs: fewer than two runs are left, so nothing is compared")
    if REF_BUILD:
        # ref_steps: the steps of the reference build when they differ from this build's (the match check, item 155)
        runs = list(runs) + [("ref-build", dict(runs[0][1], _program=REF_BUILD, **({"_steps": ref_steps} if ref_steps
                                                                                   else {})))]
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
        extra_env = with_set_keep(options.get("_env", {}))
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
            elif "database" not in options:
                f.write("database = 3.9.68\n")     # the base game (Q152); the default is 3.9.60 since 0.6.1
        # RECOMP_SCREEN_LOG=1 prints the text and the stack of a game box (0x5e8290, waits.c); RECOMP_NO_BOX logs only
        # the system box. It only writes the log (item 202).
        env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
                   RECOMP_SCREEN_LOG="1", RECOMP_USER_DIR=user,
                   RECOMP_FIXED_CLOCK="1", RECOMP_SAVE_AFTER_DAYS=str(days), RECOMP_IDLE_COUNT="1",
                   RECOMP_SCRIPT="; ".join(scaled_steps(mode, shots, steps_of_run)), **extra_env)
        start = time.time()
        args = [run_program, "--game", game, "-seed", "1"]
        proc = subprocess.Popen(args, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                                errors="replace")
        try:
            try:
                out, err = proc.communicate(timeout=min(SAMPLE_AFTER, timeout))
            except subprocess.TimeoutExpired:
                if timeout <= SAMPLE_AFTER:
                    raise
                sample_run(name, proc.pid, run)
                out, err = proc.communicate(timeout=timeout - SAMPLE_AFTER)
        except subprocess.TimeoutExpired:
            proc.kill()
            out, err = proc.communicate()
            if KEEP_FAILED:
                with open(os.path.join(run, "stderr.txt"), "w", encoding="latin-1", errors="replace") as f:
                    f.write(err or "")
            raise subprocess.TimeoutExpired(args, timeout, out, err)
        r = subprocess.CompletedProcess(args, proc.returncode, out, err)
        if KEEP_FAILED:
            with open(os.path.join(run, "stderr.txt"), "w", encoding="latin-1", errors="replace") as f:
                f.write(r.stderr)
        return r, os.path.join(user, SAVE_NAME), time.time() - start

    # The runs play at the same time. The test clock and the wait steps make each run the same on a
    # busy machine.
    saves = {}
    result = {}
    with work_folder("cm0102-same-save-", result) as work:
        with concurrent.futures.ThreadPoolExecutor(min(len(runs), JOBS) if JOBS else len(runs)) as pool:
            jobs = [(name, pool.submit(play, name, options, work)) for name, options in runs]
            failed_runs = 0
            for (name, job), (_n, options) in zip(jobs, runs):
                r, path, took = job.result()
                if r.returncode != 0 or not os.path.exists(path):
                    print(r.stderr[-2000:])
                    for line in re.findall(r"^recomp: message box (?:from|stack).*$", r.stderr, re.M):
                        print(f"box   {name}: {line}")       # the game box of the run, if any (item 202)
                    print(f"FAIL  {name}: no saved game (code {r.returncode}) after {took:.0f} s")
                    failed_runs += 1
                    continue
                # Item 257: a run that asks for no rules table must have read none.
                if options.get("_env", {}).get("RECOMP_RULES_TABLE") == NO_TABLE and \
                        "recomp: rules table: none" not in r.stderr:
                    print(f"FAIL  {name}: the game read a rules table, though the run asks for none")
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
        # A run with no save fails the check, but the runs that saved are still compared (item 185): one run with no
        # save must not hide a difference in the others.
        first = runs[0][0]
        if first not in saves:
            print(f"FAIL  {first} has no save, so no run is compared")
            result["failed"] = True
            print("FAIL")
            return 1
        saved_runs, dropped = [runs[0]], 0
        for name, options in runs[1:]:
            base = against.get(name) or first
            if name not in saves:
                continue
            if base not in saves:
                print(f"FAIL  {name}: not compared, its reference {base} has no save")
                dropped += 1
                continue
            saved_runs.append((name, options))
        season_want = None
        if CUSTOM_DB:
            season_want = season_of_save(save) if save else expected_season(CUSTOM_DB)
        failed = failed_runs + dropped
        if len(saved_runs) > 1 or not failed:          # with every other run lost, there is nothing to compare
            failed += compare_saves(saved_runs, saves, against, allowed, required, rows, season_want)
        result["failed"] = bool(failed)
    print("PASS" if not failed else "FAIL")
    return 1 if failed else 0


# A run of a check that still plays after SAMPLE_AFTER s gets a stack sample (item 185). The runs save after 60 to
# 100 s, under load in up to about 200 s. In round Z the silent run of two checks played its holiday days and then
# ran for 390 s with no save and no idle sleep (a busy loop that still pumps the script) until its quit step. The
# sample names the loop: the function names of the recompiled game (f_00xxxxxx, o_00xxxxxx) give the address of the
# original's code. macOS only (the tool "sample"); elsewhere the line says so.
SAMPLE_AFTER = 300


def sample_run(name, pid, folder):
    """Prints the heaviest stack of the game pid to the check log, and keeps the whole sample in folder."""
    if not shutil.which("sample"):
        print(f"slow  {name}: still playing after {SAMPLE_AFTER} s (no sample tool here)", flush=True)
        return
    path = os.path.join(folder, "sample.txt")
    subprocess.run(["sample", str(pid), "5", "-file", path], capture_output=True)
    lines = open(path, errors="replace").read().splitlines() if os.path.exists(path) else []
    graph = [ln.rstrip() for ln in lines[next((i for i, ln in enumerate(lines) if ln.startswith("Call graph")), 0):]
             if "Thread_" in ln or "o_00" in ln or "f_00" in ln][:60]
    print(f"slow  {name}: still playing after {SAMPLE_AFTER} s, its stack (5 s sample, {path}):", flush=True)
    for ln in graph:
        print(f"      {ln}")


def compare_saves(runs, saves, against, allowed=(), required=None, rows=None, season_want=None):
    """Compares the save of each run with the save of its reference (compare_runs) and prints a line for each run.
    Returns the count of failures. season_want: with --custom-db, the (start year, changing rules) that the normal
    game must have (expected_season). A check in which no run is compared fails."""
    failed = 0
    first = runs[0][0]
    if all(against.get(name) == "" for name, _ in runs[1:]):
        print("FAIL  no run is compared (each run left is a reference)")
        return 1
    if season_want is not None:         # --custom-db: the normal game must be in the season of the data set
        season_got = season_of_save(saves[first])
        if season_got != season_want:
            print(f"FAIL  {first}: the game is in {season_text(season_got)}, the data set gives {season_text(season_want)}")
            failed += 1
    compared = 0
    want = open(saves[first], "rb").read()
    for name, _ in runs[1:]:
        if required and name in required and required[name] not in save_parts(saves[name]):
            failed += 1
            print(f"FAIL  {name}: the save has no part {required[name]}")
            continue
        if against.get(name) == "":
            continue                                    # a reference run for another run (ref-0.5.0)
        why = base_game_only(name, season_want)
        if why:
            print(f"n/a   {name}: {why}")
            continue
        compared += 1
        base = against.get(name) or first
        if open(saves[name], "rb").read() == open(saves[base], "rb").read():
            print(f"ok    {name}: the save is the same as {base}")
            continue
        a, b = save_parts(saves[base]), save_parts(saves[name])
        merged = any(k.lower().endswith(".parts") for k in list(a) + list(b))
        if merged:      # a save of 0.6.2 or later with merged parts (slice 13): compare the files
            a, b = save_files(saves[base]), save_files(saves[name])
        differ = sorted(k for k in set(a) | set(b) if a.get(k) != b.get(k))
        if merged and open(saves[base], "rb").read(8) != open(saves[name], "rb").read(8):
            differ.append("the file header (version and flags)")
        if merged and not differ:
            print(f"ok    {name}: the save holds the same files as {base} (merged parts)")
            continue
        ok_parts = allowed.get(name, ()) if isinstance(allowed, dict) else allowed
        if differ and set(differ) <= set(ok_parts) and rows and name in rows and rows[name] == HOST_ROWS and \
                not host_rows_ok(saves[base], saves[name]):
            failed += 1
            print(f"FAIL  {name}: the save differs from {base} in more than the hosts of its history rows")
            continue
        if differ and set(differ) <= set(ok_parts) and rows and name in rows and rows[name] != HOST_ROWS and \
                not q48_rows_ok(saves[base], saves[name], rows[name]):
            failed += 1
            print(f"FAIL  {name}: the save differs from {base} in other history rows than {rows[name]}")
            continue
        if differ and set(differ) <= set(ok_parts):
            print(f"ok    {name}: the save differs from {base} only in accepted parts: {', '.join(differ)}")
            continue
        failed += 1
        print(f"FAIL  {name}: the save differs from {base} in: {', '.join(differ) or 'the file header'}")
    if not compared:
        print("FAIL  no run is compared (each run left is n/a or a reference)")
        failed += 1
    return failed


# --search: after the load, open the player search (Player & Staff Search, Player Search tab: every player
# of the list, no filter), close it with Cancel, then go on holiday. The runs open the same screen, so a
# difference between them comes from the option alone (0.5.0 slice 5).
SEARCH_STEPS = [
    "2500 click 45 165",                    # the manager's menu
    "wait popup",
    "1000 click 190 302",                   # Player & Staff Search
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
    network = load + OPTIONS_OPEN + ["1000 click 680 155", "2500 click 584 155", "4000 click 680 155"] + \
        OPTIONS_CLOSE + holiday

    def run(name, steps):
        return (name, {"faster_days": "on", "_steps": steps, "_user_sub": OPTIONS_USER_SUB})
    return ([run("options", switch), run("options-again", switch), run("options-tabs", tabs),
             run("options-rules", rules), run("options-backups", backups), run("options-network", network)],)


# The season part of a save (0.6.0 slice 11, src/recomp_rt/win/season.c). A save of a new game with
# changing_rules = on has it, and only that part may differ from the same game with it off.
RULES_PART = "bosman_season.dat"
# Slice 17: a new game with changing rules on plays differently, so these parts of the save differ as well. These
# four are the parts that differ in the new-game run of 2026-10-18 (all four, no other: transfer.dat, clubs and staff
# are equal), so a fifth part that differs fails the check. That run, and every rules-on run of a new game since, has
# no rules table (the game folder has no Seasons/, and the set did not set RECOMP_RULES_TABLE), so no row of the
# table applies: the substitutes and nation rows (season_subs_rule.c, file_nation of season_kind_nation.c) read it.
# Item 257 makes that fixed: the new-game rules-on runs with NO_TABLE. The base game (changing_rules off) is compared
# as before.
# contract.dat joins them with Setup Game menu B (O107, Max's play test of 0.6.0, accepted overnight for Max's
# morning check): the same build with the menu of the preview (RECOMP_SETUP_TEXT=0.6.0-preview) differs only in
# the four parts and bosman_season.dat, and a heap trace of Setup Game is the same with changing rules on and
# off, so the rules-on game's own memory move meets the new menu's layout. Only the new-game check uses this.
# O323: a 2001 game with changing rules on holds the South American section of the 2006 cycle from its first day,
# and that cycle's league ends on its real last matchday, 12 October 2005 (season_intl_qual.c, end_by_october), so
# the section's part differs too.
RULES_ON_PARTS = ("fifa_rankings.dat", "general.dat", "national_teams.dat", "staff history.tmp", "contract.dat",
                  "World Cup South American Qualifying Section_440.tmp")
# The run rules-on is compared with rules-off, so its season part is never compared with another. rules-on-again
# plays the same game and must equal rules-on in every part, the season part too: before 2026-11-11 the part held
# the host heap's bytes after each changed name and differed between two runs with the same clicks
# (docs/research/2026-11-11-season-part-unset-bytes.md).
RULES_AGAIN_RUN = ("rules-on-again", {"faster_days": "off", "changing_rules": "on", "_against": "rules-on"})
# O128 (the standard of O107): in a new game the runs off and foreign-off may differ in club records.dat. The same
# build with the menu of the preview (RECOMP_SETUP_TEXT=0.6.0-preview) equals normal in both, so the Setup Game
# top row's layout (Q422) meets the memory move that those options already make (foreign_colours draws names in
# another colour); it is not decision code. Only those two runs of the new-game check use this.
NEW_GAME_LAYOUT_PARTS = ("club records.dat",)


# Q48 (0.6.2 slice 7, decided 30 and 31 October 2026): the history rows of editions after the start go in every game,
# and the game's own row replaces the data row of the same edition. That is a change of the game by decision, so
# some runs that equalled normal before compare with history-q48-off, the same game with the fix off, instead:
# - season-code-off must equal history-q48-off in every part (the season code off includes the fix off).
# - New game: rules-on plays with the fix off too and must equal history-q48-off up to the rules-on parts. With the
#   fix on it differs from normal through the Asian Cup Winners' Cup holder (Q48 in the spec), so the shipped
#   default (changing rules on, fix on) is the run rules-on-q48, which equals the same run of --ref-build
#   (rules-on-q48-ref); without --ref-build it is no check. A loaded save has changing rules off: there rules-on
#   keeps the fix and must equal normal, as before.
# - history-q48-off itself: in start.sav and prematch.sav it differs from normal only in index.dat and
#   nation_comp_history.dat, and there only in the rows of Q48_SAVE_ROWS: the 3.9.68 row Copa America 2001, which goes
#   when the game writes its own 2001 final on 29 July. In any other saved game it must equal normal. In a new game it
#   is the reference only: its difference from normal is the whole decision (first-season holders).
Q48_REFERENCE = "history-q48-off"
# Item 257: the value of RECOMP_RULES_TABLE that the new-game rules-on runs with. The game reads a table that does not
# exist as no table (season_rules_init), and the other season files beside it (season_data_path) do not exist either.
# A path with a "/" and no folder, so that the files beside it do not resolve against the working folder on Windows.
NO_TABLE = "/cm0102-no-rules-table/rules.tsv"
Q48_SAVE_PARTS = ("index.dat", "nation_comp_history.dat")
Q48_COPA_2001 = (450, 2001)                 # (competition id, label year) of the row that goes
Q48_SAVE_ROWS = {"start.sav": [Q48_COPA_2001], "prematch.sav": [Q48_COPA_2001]}


# The new-game pair rules-on and history-q48-off runs with original-faults group club_regulars off (lead, 7 November
# 2026, open item 160). The two games split after some days in the original too (item 184: by 28 days with the group
# off); the group's marks keep more players at their clubs, which brings that split inside the 14 days of the check.
# season-code-off, which a new game compares with history-q48-off, runs with it off too. The group stays on in every
# other run. Group callup_counts (open item 300) acts only in a season game, so it acts in rules-on and not in
# history-q48-off. The pair runs with it off too.
PAIR_KEEP = {"RECOMP_ORIGINAL_FAULTS": "keep:club_regulars,callup_counts"}


# Issue 145 and 165 (0.7.3 slice 15, docs/research/history-rows/): in a new 2001 game with changing rules on, the
# corrected rows write the 8 Greek Super Cup rows of 1980 to 1996 (the lists part of apply_rows, season_history.c).
# The history table grows by 8 rows for them (0x527f60), and the game then plays differently from the 0.7.2 build.
# The mechanism is not known. The difference is the planned change of play of docs/research/history-rows/README.md. With the lists part off (RECOMP_HISTORY_ROWS=fhc) rules-on-q48 equals
# rules-on-q48-ref, and with only the lists part on it differs (lead bisect point 5795241a, 19 November 2026). So
# rules-on-q48 runs with the lists part off, and tests/seasons/history_rows_run.py (run r2001) checks the rows.
HISTORY_ROWS_NO_LISTS = {"RECOMP_HISTORY_ROWS": "fhc"}
# Open item 295 (0.7.3 slice 16, original_faults.c rt_club_key_twins): at the trim of the club table
# (database_trim_club_table 0x5267c0) every key id of a club follows its row. The fix acts only in a season game, and
# a new 2001 game with changing rules on is one, so rules-on-q48 takes it and the 0.7.2 build does not. The game then
# plays differently. This is the planned fix of the item. The lead bisect of 19 November 2026 (lists part off) finds
# the first difference at merge 7f0ef429 (item 295), and rules-on-q48 equals rules-on-q48-ref with the group off. So
# rules-on-q48 runs with group club_keys off, and tests/seasons/club_key_twins_run.py checks the fix. Group
# callup_counts (open item 300) also acts only in a season game, and the 0.7.2 build has no such fix, so rules-on-q48
# runs with it off too, and tests/seasons/callup_counts_run.py checks the fix.
CLUB_KEYS_OFF = {"RECOMP_ORIGINAL_FAULTS": "keep:club_keys,callup_counts"}


def q48_runs(runs, new_game):
    """The runs with the Q48 comparisons (above)."""
    out = []
    for name, o in runs:
        if name == "season-code-off":
            o = dict(o, _against=Q48_REFERENCE)
            if new_game:                          # compared with history-q48-off, so with the same groups
                o = dict(o, _env=dict(o.get("_env", {}), **PAIR_KEEP))
        elif name == "rules-on" and new_game:
            q = dict(o, _env=dict(o.get("_env", {}), **HISTORY_ROWS_NO_LISTS, **CLUB_KEYS_OFF))
            out.append(("rules-on-q48", dict(q, _against="rules-on-q48-ref" if REF_BUILD else "")))
            if REF_BUILD:
                out.append(("rules-on-q48-ref", dict(q, _program=REF_BUILD, _against="")))
            # Item 257: the pair is defined with no rules table (RULES_ON_PARTS). With a table, changing rules on
            # applies the 2001/02 rows at the new game (club moves, new competitions, names), so rules-on runs with
            # none (NO_TABLE) whatever the set's environment holds. history-q48-off keeps the set's table. So with a
            # table set, a difference of the pair is either the rules-on change or the base game reading the table.
            o = dict(o, _env=dict(o.get("_env", {}), RECOMP_HISTORY_AFTER_START="off", RECOMP_RULES_TABLE=NO_TABLE,
                                  **PAIR_KEEP),
                     _against=Q48_REFERENCE)
        elif name == "rules-on-again" and new_game:   # the same game as rules-on (above), to compare with it
            o = dict(o, _env=dict(o.get("_env", {}), RECOMP_HISTORY_AFTER_START="off", RECOMP_RULES_TABLE=NO_TABLE,
                                  **PAIR_KEEP))
        elif name == Q48_REFERENCE and new_game:
            o = dict(o, _env=dict(o.get("_env", {}), **PAIR_KEEP), _against="")
        out.append((name, o))
    return out


def history_rows(path):
    """The nation competition history rows of a save: [(competition id, label year, row bytes)], free rows
    (a negative competition id, as a cleared row is saved) left out."""
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "save"))
    import savfile
    b = savfile.Sav(path).read_member("nation_comp_history.dat")
    return sorted((struct.unpack_from("<i", b, i * 26 + 4)[0], struct.unpack_from("<H", b, i * 26 + 8)[0],
                   b[i * 26 + 4:i * 26 + 26]) for i in range(len(b) // 26)
                  if struct.unpack_from("<i", b, i * 26 + 4)[0] >= 0)


def q48_rows_ok(base, path, keys):
    """1 when the save at path holds the nation history rows of base plus exactly one row of each key, and no other
    difference (the rows of the decision, Q48_SAVE_ROWS)."""
    a, b = history_rows(base), history_rows(path)
    for r in a:
        if r in b:
            b.remove(r)
        else:
            return 0
    return sorted((c, y) for c, y, _ in b) == sorted(keys)


# 0.6.2 slice 5 (open item 18, spec Decisions): in a season game every final writes its host into its history row,
# also the finals whose class asks for no host (Cup::record_season_honours 0x519c2f). A new 2001 game with changing
# rules on plays the final of the Copa America 2001 (29 July) in its first days, so its row names Colombia, while
# the game with changing rules off (the base game, and history-q48-off) keeps no host there. So rules-on may differ
# from history-q48-off in index.dat and nation_comp_history.dat, and rules-on-q48 from rules-on-q48-ref (a reference
# build before slice 5), only in the host fields of history rows that had no host.
HOST_ROWS = "hosts"
HOST_ROW_PARTS = ("index.dat", "nation_comp_history.dat")
# Item 103 (0.6.2, lead decision 3 November 2026): at a new game season_intl_rows_clean sets host fields 3 to 6 of
# each fixed host row to -1 (the original leaves them uncleared; it reads only the first two). So with changing rules
# on the bytes of HostCountry.tmp differ from the base game and from a reference build before item 103.
HOST_TABLE_PART = "HostCountry.tmp"


def host_rows_ok(base, path):
    """1 when the nation history rows of path are those of base, but for host fields (+0x16) that base leaves empty."""
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "save"))
    import savfile
    a = savfile.Sav(base).read_member("nation_comp_history.dat")
    b = savfile.Sav(path).read_member("nation_comp_history.dat")
    if len(a) != len(b):
        return 0
    for i in range(0, len(a), 26):
        x, y = a[i:i + 26], b[i:i + 26]
        if x[:0x16] + x[0x1a:] != y[:0x16] + y[0x1a:]:
            return 0
        if x[0x16:0x1a] != y[0x16:0x1a] and struct.unpack_from("<i", x, 0x16)[0] != -1:
            return 0
    return 1


# A saved test game that opens on another screen than the news ends its load at another wait (0.6.0 slice 11).
# yeovil.sav (a copy of Max's save, 20.8.02) opens on the squad screen: the load ends when Continue Game shows.
LOAD_END = {"yeovil.sav": "wait continue"}

# The Load Game screen (build 0x58f7c0, RECOMP_SCREEN_LOG=1).
LOAD_SCREEN_WAIT = "wait screen:0x58f7c0"


def load_steps(fixtures):
    """fixtures.LOAD_STEPS with a wait before each click on a new screen (item 185): the settings screen of a first
    start (SETTINGS_WAIT) and the Load Game screen. The list in fixtures.py stays as it is: other tests leave out its
    settings clicks for a user folder that has game.cfg, where no settings screen shows. The checks play the intro
    without a click (without_intro_click), 19 s with sound, so the settings wait has 600 s: on a slow machine
    (slow_machine_run.py, a game stopped 9 tenths of the time) the intro takes more than the 120 s of a wait."""
    steps = list(fixtures.LOAD_STEPS)
    if SETTINGS_WAIT in steps:                      # fixtures.LOAD_STEPS waits for it since item 155
        steps[steps.index(SETTINGS_WAIT)] = SETTINGS_WAIT + " 600000"
    else:
        steps.insert(steps.index("8200 click 522 272"), SETTINGS_WAIT + " 600000")
    at = steps.index("1000 click 277 242") + 1      # Load Game
    steps[at:at + 2] = [LOAD_SCREEN_WAIT, "1000 click 445 257", "2000 click 705 573"]
    assert steps[at + 1:at + 4] == ["1000 click 445 257", "2000 click 705 573", "wait first-news"], steps
    return steps

# The fixes of faults of the original that act in the 14 days of a saved test game, by decision (0.6.2 slice 11): its
# run original-faults-keep keeps only the other groups (RECOMP_ORIGINAL_FAULTS=keep:<groups>, original_faults.c).
# yeovil.sav (20.8.02) passes 1 September 2002: the month check opens a vacancy for each national team with no
# manager (Q43), so the save of normal differs from the original's in most parts. This holds for the 14 days of the
# set: a run of other --days may pass another month change (start.sav from 18 days on).
FAULTS_ACT = {"yeovil.sav": "keep:money,age"}


def keep_runs(runs, save_name):
    """The runs with the switch of original-faults-keep for the saved game save_name (FAULTS_ACT)."""
    if save_name not in FAULTS_ACT:
        return runs
    return [(name, dict(o, _env=dict(o["_env"], RECOMP_ORIGINAL_FAULTS=FAULTS_ACT[save_name])))
            if name == "original-faults-keep" else (name, o) for name, o in runs]


def new_game_runs(runs):
    """The runs of a new game: changing_rules = off unless a run sets it, so that the new game is the base
    game (2001/02, 3.9.68, changing rules off) and its save has no season part, as a save of 0.5.0."""
    return [(name, dict({"changing_rules": "off"}, **options)) for name, options in runs]


def same_save(program, game, mode, days, save_name=None, search=False, options=False):
    # No click ends the intro (without_intro_click).
    if save_name is None:
        steps = [s for s in STEPS if "5-holiday" not in s and "quit" not in s]
        return compare_runs(program, game, mode, q48_runs(new_game_runs(SAME_SAVE_RUNS + [RULES_AGAIN_RUN]), True),
                            without_intro_click(steps) + ["400000 quit"], days,
                            allowed={"rules-on": (RULES_PART,) + RULES_ON_PARTS + HOST_ROW_PARTS + (HOST_TABLE_PART,),
                                     "rules-on-q48": HOST_ROW_PARTS + (HOST_TABLE_PART,), "off": NEW_GAME_LAYOUT_PARTS,
                                     "foreign-off": NEW_GAME_LAYOUT_PARTS},
                            required={"rules-on": RULES_PART, "rules-on-again": RULES_PART}, rows={"rules-on": HOST_ROWS, "rules-on-q48": HOST_ROWS},
                            text_050=True)
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
        steps = without_intro_click(load_steps(fixtures)) + SEARCH_STEPS + holiday + ["400000 quit"]
        return compare_runs(program, game, mode, SEARCH_RUNS, steps, days, save)
    if options:
        groups = options_runs(without_intro_click(load_steps(fixtures)), holiday + ["400000 quit"])
        codes = [compare_runs(program, game, mode, runs, runs[0][1]["_steps"], days, save) for runs in groups]
        return max(codes)
    load = load_steps(fixtures)
    if save_name in LOAD_END:
        load[load.index("wait first-news")] = LOAD_END[save_name]
    steps = without_intro_click(load) + holiday + ["400000 quit"]
    q48 = {Q48_REFERENCE: Q48_SAVE_PARTS} if save_name in Q48_SAVE_ROWS else {}
    return compare_runs(program, game, mode, q48_runs(keep_runs(SAME_SAVE_RUNS + [SQUAD_ON_RUN], save_name), False), steps, days, save,
                        allowed=q48, rows={Q48_REFERENCE: Q48_SAVE_ROWS[save_name]} if q48 else None,
                        text_050=True)


# --speed fastest: the Fastest item (#31, commentary.c), one row above Very Fast.
SPEED_CLICKS = {"very-fast": "1000 click 120 255 | 192 344",
                "fastest": "1000 click 120 242 | 192 323"}


def match(program, game, days, speed="very-fast", sleep_danger=False):
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fixtures"))
    import fixtures  # noqa: E402  (fixtures.py imports this file)
    save = os.path.join(fixtures.DEFAULT_DIR, MATCH_SAVE)
    if not os.path.exists(save):
        print(f"FAIL: no saved test game {save} (tests/fixtures/fixtures.py says where it comes from)")
        return 1
    # No click ends the intro (without_intro_click): with the click, the runs on a busy machine played
    # different matches.
    def steps_of(event):
        return without_intro_click(load_steps(fixtures)) + [
            SPEED_CLICKS[speed] if s.startswith("1000 click 120 255") else s
            for s in (MATCH_STEPS if event else MATCH_STEPS_ORIGINAL)]
    ref_steps = steps_of(match_event(program_keep(), REF_BUILD)) if REF_BUILD else None
    if not match_event(program_keep(), program):
        return compare_runs(program, game, "widescreen", MATCH_RUNS, steps_of(False), days, save, timeout=900,
                            ref_steps=ref_steps)
    runs = MATCH_RUNS + ([(SLEEP_DANGER_RUN[0], dict(SLEEP_DANGER_RUN[1], _steps=steps_of(False)))]
                         if sleep_danger else [])
    # A run whose own keep list keeps match_numbers plays the original's match (no MATCH_EVENT).
    runs = [(name, dict(o, _steps=steps_of(False)) if not match_event(program_keep(o.get("_env", {})), program)
             else o) for name, o in runs]
    # MATCH_EVENT opens the tactics screen, whose rows match_columns changes: off moves the memory blocks, so
    # LAYOUT_PARTS may differ (as same_save_views.py match-tactics).
    return compare_runs(program, game, "widescreen", runs, steps_of(True), days, save, timeout=900,
                        allowed={"columns-off": LAYOUT_PARTS}, ref_steps=ref_steps)


def program_keep(env=None):
    """The RECOMP_ORIGINAL_FAULTS value that a run with its own environment env plays with (with_set_keep)."""
    env = with_set_keep(env or {})
    return env.get("RECOMP_ORIGINAL_FAULTS", os.environ.get("RECOMP_ORIGINAL_FAULTS", ""))


_GROUPS_OF = {}


def has_fault_group(program, group):
    """True if the build program knows the original-faults group (its name is in the binary, original_faults.c).
    A reference build from before item 155 has no match_numbers, so its matches have no MATCH_EVENT."""
    key = (os.path.abspath(program), group)
    if key not in _GROUPS_OF:
        with open(program, "rb") as f:
            _GROUPS_OF[key] = b"\0" + group.encode() + b"\0" in f.read()
    return _GROUPS_OF[key]


def match_event(keep_value, program):
    """True if a run plays the match check with MATCH_EVENT: its build has the group match_numbers (item 155) and
    the run does not keep it (keep_value: its RECOMP_ORIGINAL_FAULTS)."""
    return has_fault_group(program, "match_numbers") and not keeps_match_numbers(keep_value)


def keeps_match_numbers(value):
    """True if RECOMP_ORIGINAL_FAULTS=value keeps the group match_numbers (original_faults.c)."""
    return value == "keep" or (value.startswith("keep:") and "match_numbers" in value[5:].split(","))


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
            # The base game (Q152) without --database: the option's default is 3.9.60 since 0.6.1.
            f.write(f"database = {database or '3.9.68'}\n")
        env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
                   RECOMP_SCREEN_LOG="1", RECOMP_SCRIPT="; ".join(scaled_steps(mode, shots)))    # item 202
        env["RECOMP_USER_DIR"] = user
        # The season tables (names, rules) of the build, as the season checks read them
        # (rules_report_check.py). Without them the game reads <game folder>/Seasons: a download has it, the
        # clean files do not, and the news screen of the two showed other names ("Premier Division").
        table = os.path.join(os.path.dirname(program), "seasons", "rules.tsv")
        if os.path.exists(table) and "RECOMP_RULES_TABLE" not in env:
            env["RECOMP_RULES_TABLE"] = table
        r = subprocess.run([program, "--game", game], env=env, capture_output=True, text=True, errors="replace", timeout=300)
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
