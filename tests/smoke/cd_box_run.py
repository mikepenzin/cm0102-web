#!/usr/bin/env python3
"""No "CD Not Required" box: a load and a new game go on without a click on Ok.

Usage:
    python3 tests/smoke/cd_box_run.py <cm0102_recomp> <game folder> [--dir <fixtures folder>]
        [--data-3965 <3.9.65 data set folder>]

The original shows the message box "CD Not Required" ("You may now remove the CM01/02 CD at any time")
at the end of a new game (call 0x82a98b in 0x828110) and of a load (call 0x82e400 in 0x82a9c0). The
build uses no CD, so the runtime skips the box at these two calls (cd_box in waits.c).

The runs play at the same time, with steps that never click Ok:
    load-classic     Restore Saved Game with start.sav (tests/fixtures/fixtures.py), to the first news screen
    load-widescreen  the same in widescreen
    new-game         Start New Game, England, Next, to the Enter Name screen
A run fails when a wait does not end (code 3; the box loop has no input poll, so the game stops there)
or when the log of the runtime does not name the skipped box.

The Italian runs play in the language Italian (game.cfg, see italian_cfg). There the title of the box is
"CD Non Richiesto". A player with any database other than 3.9.68 also saw the box "Database..cpp 7784" of
database_load_language_names 0x5339a0 at each load and new game (call 0x5358b7), because the .lng files fit the
3.9.68 data only. The runtime now writes the copied .lng files in the rows of a database that they do not fit, at
the data load of a new game (season_lng.c; tests/smoke/lng_fit_run.py tests it), so the box does not come. The runs
log each box that shows (RECOMP_SCREEN_LOG); any box and a skipped box 7784 fail the run, and so does a written
file in a run whose files fit, or no written file in the season run:
    load-italian         start.sav (3.9.68) in Italian, to the first news screen
    new-italian-3965     a new game on 3.9.65 in Italian, to the first news screen, then Save Game "Ita65"
    load-italian-3965    then a load of Ita65 in Italian, to the first news screen
    new-italian-season   a new game on the season database April 2022 (no .lng files of its own, other
                         counts) in Italian, to the Select Leagues screen
The season run copies <fixtures folder>/databases/April 2022 to Databases.
The 3.9.65 runs use a game folder of links to the given one, with Data-3.9.65 (--data-3965, by default
/Users/max/Projects/cm0102/data-3965). Their game.cfg comes from a first start in English.
"""
import concurrent.futures
import os
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import fixtures  # noqa: E402
import smoke_run  # noqa: E402

SKIPPED = "recomp: skipped the box CD Not Required"
SHOWN = "recomp: message box from"
SKIPPED_LNG = "recomp: skipped the box Database..cpp 7784"
FITTED_LNG = "recomp: .lng files fitted to the database"
SEASON_DB = "April 2022"
DATA_3965 = "/Users/max/Projects/cm0102/data-3965"
ITALIAN = 4                 # the language Italian: database_language_file_exists 0x535930 gives it ita.lng
SETTINGS = ["8200 click 522 272", "8500 click 662 524"]  # the settings screen of a first start (no game.cfg)

LOAD = [s for s in fixtures.LOAD_STEPS if "box" not in s and "399 330" not in s] + ["2000 quit"]
NEW_GAME = smoke_run.STEPS[:smoke_run.STEPS.index("wait typing") + 1]
NEW_GAME = [s for s in NEW_GAME if "dump" not in s and "box" not in s and "399 330" not in s] + ["2000 quit"]


# Save Game after the first news screen (tests/smoke/save_folder_run.py). The suggested name is the club
# and the date, longer than "Arsenal 2001-07-14" with the clubs of 3.9.65 in Italian: 40 Backspaces.
SAVE_AS = (["2500 click 45 342", "4500 click 140 334"] + [f"{5000 + 50 * i} key 8" for i in range(40)] +
           ["7500 type Ita65", "9500 click 705 572", "16500 quit"])
NEW_ITALIAN = [s for s in smoke_run.STEPS if " dump " not in s and s not in SETTINGS + [smoke_run.SETTINGS_WAIT]]
NEW_ITALIAN = NEW_ITALIAN[:NEW_ITALIAN.index("wait first-news") + 1] + SAVE_AS
LOAD_ITALIAN = [s for s in LOAD if s not in SETTINGS + [smoke_run.SETTINGS_WAIT]]
LEAGUES_ITALIAN = ["1000 click 400 300", "wait setup", "2500 click 277 177", "wait leagues", "2000 quit"]


def italian_cfg(program, game, user):
    """game.cfg of a first start, with the settings language (the int at offset 8, Config +0x08, that
    Config::apply_and_save 0x5d8480 compares with langlib_get_language 0x66f4d0) set to Italian."""
    steps = ["1000 click 400 300", *SETTINGS, "wait setup", "2000 quit"]
    r, _ = fixtures.run(program, game, "classic", user, steps, {}, 300)
    path = os.path.join(user, "game.cfg")
    if r.returncode != 0 or not os.path.exists(path):
        return False
    with open(path, "r+b") as f:
        f.seek(8)
        f.write(ITALIAN.to_bytes(4, "little"))
    return True


def play_italian(program, game, folder, data_3965):
    """The three Italian runs. Returns a list of (name, seconds, problems)."""
    out = []
    with tempfile.TemporaryDirectory(prefix="cm0102-cd-box-ita-") as work:
        game65 = os.path.join(work, "game")
        os.makedirs(game65)
        for n in os.listdir(game):
            if n.lower() != "data-3.9.65":
                os.symlink(os.path.join(game, n), os.path.join(game65, n))
        os.symlink(data_3965, os.path.join(game65, "Data-3.9.65"))
        runs = [("load-italian", game, [], LOAD_ITALIAN, "start.sav"),
                ("new-italian-3965", game65, [("database", "3.9.65")], NEW_ITALIAN, None),
                ("load-italian-3965", game65, [("database", "3.9.65")], LOAD_ITALIAN, "Ita65.sav"),
                ("new-italian-season", game, [("database", "folder:" + SEASON_DB)], LEAGUES_ITALIAN, None)]
        user65 = os.path.join(work, "user65")
        for name, g, options, steps, save in runs:
            user = user65 if g is game65 else os.path.join(work, name)
            if not os.path.exists(os.path.join(user, "game.cfg")):
                os.makedirs(user, exist_ok=True)
                if name == "new-italian-season":
                    shutil.copytree(os.path.join(folder, "databases", SEASON_DB),
                                    os.path.join(user, "Databases", SEASON_DB))
                fixtures.write_settings(user, "classic", options)
                if not italian_cfg(program, g, user):
                    out.append((name, 0, ["no game.cfg from the first start"]))
                    continue
            if save == "start.sav":
                shutil.copyfile(os.path.join(folder, save), os.path.join(user, save))
            elif save and not os.path.exists(os.path.join(user, save)):
                out.append((name, 0, [f"no {save} from the run before"]))
                continue
            if save:     # one save in the user folder: the load list shows it first
                for n in os.listdir(user):
                    if n.lower().endswith(".sav") and n != save:
                        os.rename(os.path.join(user, n), os.path.join(user, n + ".off"))
            r, took = fixtures.run(program, g, "classic", user, smoke_run.scaled_steps("classic", work, steps),
                                   {"RECOMP_SCREEN_LOG": "1"}, 900)
            problems = [l.split(": ", 1)[1][:120] for l in r.stderr.splitlines() if l.startswith(SHOWN)]
            if r.returncode != 0:
                problems.append(f"code {r.returncode}")
            if save and SKIPPED not in r.stderr:
                problems.append("the log does not name the skipped box")
            if SKIPPED_LNG in r.stderr:
                problems.append("the box Database..cpp 7784 came (the runtime skipped it)")
            # Only the season database needs the .lng files written in its rows: the 3.9.68 save and the 3.9.65
            # data set have files that fit.
            if (name == "new-italian-season") != (FITTED_LNG in r.stderr):
                problems.append("the .lng files were written" if FITTED_LNG in r.stderr else
                                "the log does not name the written .lng files")
            out.append((name, took, problems))
    return out


def play(program, game, folder, name, mode, steps):
    with tempfile.TemporaryDirectory(prefix="cm0102-cd-box-") as work:
        user = os.path.join(work, "user")
        os.makedirs(user)
        if name.startswith("load"):
            shutil.copyfile(os.path.join(folder, "start.sav"), os.path.join(user, "start.sav"))
        fixtures.write_settings(user, mode, [])
        r, took = fixtures.run(program, game, mode, user, smoke_run.scaled_steps(mode, work, steps), {}, 900)
    problems = []
    if r.returncode != 0:
        tail = [l for l in r.stderr.splitlines() if "wait" in l or "box" in l][-3:]
        problems.append(f"code {r.returncode}: {' / '.join(tail)}")
    if SKIPPED not in r.stderr:
        problems.append("the log does not name the skipped box")
    return name, took, problems


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    folder = argv[argv.index("--dir") + 1] if "--dir" in argv else fixtures.DEFAULT_DIR
    if not os.path.exists(os.path.join(folder, "start.sav")):
        print(f"no saved game {os.path.join(folder, 'start.sav')}: see tests/fixtures/fixtures.py")
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    data_3965 = os.path.abspath(argv[argv.index("--data-3965") + 1] if "--data-3965" in argv else DATA_3965)
    runs = [("load-classic", "classic", LOAD), ("load-widescreen", "widescreen", LOAD),
            ("new-game", "classic", NEW_GAME)]
    failed = 0
    with concurrent.futures.ThreadPoolExecutor(len(runs) + 1) as pool:
        jobs = [pool.submit(play, program, game, folder, n, m, s) for n, m, s in runs]
        italian = pool.submit(play_italian, program, game, folder, data_3965)
        results = [job.result() for job in jobs] + italian.result()
        for name, took, problems in results:
            print(f"{'FAIL ' if problems else 'ok   '} {name} ({took:.0f} s){': ' + '; '.join(problems) if problems else ''}")
            failed += bool(problems)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
