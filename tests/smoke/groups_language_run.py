#!/usr/bin/env python3
"""Attribute groups in every language (0.6.1 item 8): a goalkeeper gets the Goalkeeping group in each language.

Usage:
    python3 tests/smoke/groups_language_run.py <cm0102_recomp> <game folder> [--dir <fixtures folder>]
        [--only base,season] [--languages 1,4] [--keep <folder>] [--jobs 2]

The groups once found a goalkeeper by the positions text (0x53ee50) starting with "GK". That text is in the
chosen language, so in Italian no player was a goalkeeper: a keeper got the Technical group and no Handling or
Reflexes. The runtime now reads his position ratings (groups_is_keeper, src/recomp_rt/win/groups_layout.c).

Each run sets the language of game.cfg (the int at offset 8, 1 to 10 as database_language_file_exists
0x535930 counts them), turns attribute_groups on, opens the profile of the first player of the squad list (a
goalkeeper) and of a later one (an outfield player), and logs the value labels of both profiles
(RECOMP_COLOUR_LOG=1, columns.c). The check, in every language:
    - the first column of the goalkeeper holds 7 values (the Goalkeeping group) and that of the outfield
      player 10 (the Technical group);
    - base only: the numbers of each profile, in the order of its labels, are the same as in English. The
      values are numbers in every language, so this holds only when each attribute has the cell it has in
      English. The English run must show 26 numbers for the goalkeeper and 29 or more for the outfield player.
The season games pick the default club of the language's nation, so their players differ between languages.
    base     start.sav (tests/fixtures, 3.9.68, Arsenal, 14.7.01): Seaman, then Vieira
    season   a new game on our own 2006/07 database (Summer 2006.zip of --own-dbs, default
             /Users/max/Projects/cm0102/own-dbs), changing rules on, to the first news screen: the first and
             the eighth player of the squad list
The dumps of each profile go to the keep folder (<case>-<language>-gk.png and -outfield.png).
"""
import concurrent.futures
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import fixtures  # noqa: E402
import lng_fit_run  # noqa: E402
import smoke_run  # noqa: E402

LANGUAGES = {1: "English", 2: "French", 3: "German", 4: "Italian", 5: "Spanish", 6: "Danish", 7: "Dutch",
             8: "Norwegian", 9: "Portuguese", 10: "Swedish"}
ENGLISH = 1
VALUE = re.compile(r'recomp: profile value "([^"]*)" colour')
DUMP = "recomp: screen saved to"
ROWS = 13                    # rows of a column of a grouped profile (groups_layout.h)
KEEPER_FIRST, OUTFIELD_FIRST = 7, 10
# Numbers 1 to 20 on a grouped profile: a goalkeeper shows 7 + 12 + 7 attributes, an outfield player
# 10 + 12 + 7 (groups_layout.c). Form and Morale may be numbers too, so these are lower bounds.
KEEPER_NUMBERS, OUTFIELD_NUMBERS = 26, 29
SEASON_DB = "Summer 2006"
OWN_DBS = "/Users/max/Projects/cm0102/own-dbs"

# After the first news screen, 800x600: manager menu, the club's Squad, the first player, dump, Back, a later
# player, dump, quit. The rows of the squad list are 15 pixels apart from y 234 (Seaman), Vieira is row 10.
PROFILES = {
    "base": ["1000 click 45 165", "2500 click 150 156", "6000 click 210 234", "9000 dump {d}/gk.bmp",
             "9500 click 358 573", "12000 click 210 369", "15000 dump {d}/outfield.bmp", "16000 quit"],
    "season": ["1000 click 45 165", "2500 click 150 156", "6000 click 210 234", "9000 dump {d}/gk.bmp",
               "9500 click 358 573", "12000 click 210 339", "15000 dump {d}/outfield.bmp", "16000 quit"],
}


def profiles(log):
    """The value texts of the profile on each screen dump of the log: the labels logged since the last dump."""
    out, values = [], []
    for line in log.splitlines():
        m = VALUE.search(line)
        if m:
            values.append(m.group(1).strip())
        elif line.startswith(DUMP):
            out.append(values)
            values = []
    return out


def first_column(texts):
    """The values in the first column of a grouped profile: rows 2 to 13 (row 1 is the heading). A goalkeeper
    shows 7 (Handling to Technique), an outfield player 10 (Crossing to Technique)."""
    return sum(1 for t in texts[1:ROWS] if t)


def numbers(texts):
    return [int(t) for t in texts if t.isdigit() and 1 <= int(t) <= 20]


def run(program, game, case, language, work, database):
    out = os.path.join(work, f"{case}-{language}")
    user = os.path.join(out, "user")
    os.makedirs(user, exist_ok=True)
    options = [("attribute_groups", "on")]
    if case == "base":
        shutil.copyfile(os.path.join(fixtures.DEFAULT_DIR, "start.sav"), os.path.join(user, "start.sav"))
        options.append(("changing_rules", "off"))
        steps = [s for s in fixtures.LOAD_STEPS if s not in lng_fit_run.SETTINGS + [smoke_run.SETTINGS_WAIT]]
    else:
        shutil.copytree(database, os.path.join(user, "Databases", SEASON_DB))
        options += [("database", "folder:" + SEASON_DB), ("changing_rules", "on")]
        steps = lng_fit_run.NEW_GAME
    fixtures.write_settings(user, "classic", options)
    lng_fit_run.wait_room()
    if not lng_fit_run.language_cfg(program, game, user, language):
        return out, "", ["no game.cfg"]
    lng_fit_run.wait_room()
    r, _ = fixtures.run(program, game, "classic", user,
                        smoke_run.scaled_steps("classic", out, steps + PROFILES[case]),
                        {"RECOMP_COLOUR_LOG": "1", "RECOMP_SCREEN_LOG": "1"}, 1200)
    with open(os.path.join(out, "stderr.txt"), "w") as f:
        f.write(r.stderr)
    problems = [l.split(": ", 1)[1][:120] for l in r.stderr.splitlines() if l.startswith(lng_fit_run.BOX)]
    if r.returncode != 0:
        problems.append(f"code {r.returncode}")
    for name in ("gk", "outfield"):
        bmp = os.path.join(out, name + ".bmp")
        if os.path.exists(bmp) and shutil.which("sips"):
            subprocess.run(["sips", "-s", "format", "png", bmp, "--out", os.path.join(out, name + ".png")],
                           capture_output=True)
    return out, r.stderr, problems


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    opt = lambda name, default: argv[argv.index(name) + 1] if name in argv else default  # noqa: E731
    fixtures.DEFAULT_DIR = opt("--dir", fixtures.DEFAULT_DIR)
    cases = opt("--only", "base,season").split(",")
    languages = [int(x) for x in opt("--languages", ",".join(map(str, LANGUAGES))).split(",")]
    if ENGLISH not in languages:
        languages.insert(0, ENGLISH)
    keep = opt("--keep", None)
    jobs = int(opt("--jobs", "2"))
    work = os.path.abspath(keep) if keep else tempfile.mkdtemp(prefix="cm0102-groups-lang-")
    os.makedirs(work, exist_ok=True)
    database = None
    if "season" in cases:
        database = os.path.join(work, "db")
        if not os.path.isdir(os.path.join(database, SEASON_DB)):
            with zipfile.ZipFile(os.path.join(opt("--own-dbs", OWN_DBS), SEASON_DB + ".zip")) as z:
                z.extractall(database)
        database = os.path.join(database, SEASON_DB)
    failed = 0
    with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
        jobs_ = {(c, l): pool.submit(run, program, game, c, l, work, database) for c in cases for l in languages}
        results = {k: j.result() for k, j in jobs_.items()}
    for case in cases:
        _, english_log, english_problems = results[(case, ENGLISH)]
        english = profiles(english_log)
        if english_problems or len(english) < 2:
            print(f"FAIL  {case} English: {'; '.join(english_problems) or f'{len(english)} profiles'}")
            failed += 1
            continue
        want = [numbers(english[0]), numbers(english[-1])]
        if len(want[0]) != KEEPER_NUMBERS or len(want[1]) < OUTFIELD_NUMBERS:
            print(f"FAIL  {case} English: {len(want[0])} and {len(want[1])} numbers, expected the goalkeeper "
                  f"layout ({KEEPER_NUMBERS}) and the outfield layout ({OUTFIELD_NUMBERS} or more)")
            failed += 1
            continue
        for language in languages:
            out, log, problems = results[(case, language)]
            got = profiles(log)
            if not problems and len(got) < 2:
                problems.append(f"{len(got)} profiles in the log")
            if not problems:
                for who, texts, first in (("goalkeeper", got[0], KEEPER_FIRST),
                                          ("outfield player", got[-1], OUTFIELD_FIRST)):
                    if first_column(texts) != first:
                        problems.append(f"the {who} shows {first_column(texts)} values in the first column, "
                                        f"expected {first}")
                if case == "base":
                    for who, a, b in (("goalkeeper", numbers(got[0]), want[0]),
                                      ("outfield player", numbers(got[-1]), want[1])):
                        if a != b:
                            problems.append(f"the {who} shows the numbers {a}, English {b}")
            name = LANGUAGES[language]
            for p in problems:
                print(f"FAIL  {case} {name}: {p}")
            if problems:
                failed += 1
            else:
                print(f"ok    {case} {name}: the goalkeeper and the outfield player show the English layout")
            for who in ("gk", "outfield"):
                png = os.path.join(out, who + ".png")
                if os.path.exists(png):
                    shutil.copyfile(png, os.path.join(work, f"{case}-{name}-{who}.png"))
    print(f"logs and pictures in {work}")
    print("PASS" if not failed else "FAIL")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
