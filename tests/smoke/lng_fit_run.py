#!/usr/bin/env python3
"""The .lng files fitted to the database (guard review row A25, src/recomp_rt/win/season_lng.c).

Usage:
    python3 tests/smoke/lng_fit_run.py <cm0102_recomp> <game folder> [--dir <fixtures folder>] [--only a,b]
        [--keep <folder>]

The game keeps the translated names in CM3_TEMP\\<lang>.lng (the user folder) and reads them by row. The files fit
the 3.9.68 data only, so after the club trim of a new game the runtime writes each file again in the rows of the
database when it does not fit. The save stores them and its load brings them back. The runs play one game at a
time; each one fails on a message box (RECOMP_SCREEN_LOG) or on the box Database..cpp 7784 that the runtime skips:
    base         3.9.68, changing rules off, Italian, to Select League(s): the files fit, so none is written (the
                 base game reads the original files)
    equal        the same with RECOMP_LNG_FIT=always: the written ita.lng is byte for byte the game's Data/ita.lng,
                 except two club rows whose order in the data differs from eng.lng (differing_rows)
    season-ita   the season database April 2022 (<fixtures folder>/databases), Italian, a new game to the first
                 news screen, then Save Game "LngSea": the files are written, and ita.lng's nations hold "Germania"
    season-fra   then a load of LngSea in French: the load finds its key records (no box index.cpp 5809) and its
                 fra.lng, which comes from the save, holds "Allemagne". The load reads the names by row as a
                 language change in Game Options does (Config::apply_and_save 0x5d8480 calls 0x5339a0 both times)
    accent       the season database run to Select League(s) in a user folder whose name has an accent
                 ("season-Jos\u00e9"): the files are found and written through the UTF-8 host paths
    added-clubs  a 2016 start (RECOMP_START_SEASON, October 2016 from <game folder>/Seasons) in Italian, every
                 league selected (the trim of a game of England alone takes the club out), to the first news screen: the club records add AO Kerkyra after the club trim, with a .lng row of its own after
                 the national teams (not row 0): its [0xae23fc] row (RECOMP_LNG_LOG) holds its own name in the written
                 ita.lng. The run is
                 left out when the game folder has no Seasons/October 2016.zip
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import fixtures  # noqa: E402
import smoke_run  # noqa: E402

ITALIAN, FRENCH = 4, 2          # the languages of database_language_file_exists 0x535930
SETTINGS = ["8200 click 522 272", "8500 click 662 524"]
BOX = "recomp: message box from"
SKIPPED = "recomp: skipped the box Database..cpp 7784"
FITTED = "recomp: .lng files fitted to the database"
SEASON_DB = "April 2022"
SAVE = "LngSea"
# The .lng layout (season_lng.c): 8 counts, then clubs 0x4f, continents 0xba, nation comps 0x53, nations 0x6d, ...
SECTIONS = [(0, 0x4f), (1, 0xba), (2, 0x53), (3, 0x6d)]

LEAGUES = ["1000 click 400 300", "wait setup", "2500 click 277 177", "wait leagues", "2000 quit"]
NEW_GAME = [s for s in smoke_run.STEPS if " dump " not in s and s not in SETTINGS + [smoke_run.SETTINGS_WAIT]]
NEW_GAME = NEW_GAME[:NEW_GAME.index("wait first-news") + 1]
SAVE_AS = (["2500 click 45 342", "4500 click 140 334"] + [f"{5000 + 50 * i} key 8" for i in range(40)] +
           [f"7500 type {SAVE}", "9500 click 705 572", "16500 quit"])
LOAD = [s for s in fixtures.LOAD_STEPS
        if "box" not in s and "399 330" not in s and s not in SETTINGS + [smoke_run.SETTINGS_WAIT]] + ["2000 quit"]


def all_leagues_steps():
    """A new game of every league (rules_report_check.league_steps), to the first news screen. The club trim of a
    game of England alone takes the added Greek club out again, so it has no .lng row there."""
    sys.path.insert(0, os.path.join(HERE, "..", "seasons"))
    import rules_report_check as rrc
    steps = [s for s in rrc.league_steps(["all"], []) if " dump " not in s and s not in SETTINGS + [smoke_run.SETTINGS_WAIT]]
    end = next(i for i, s in enumerate(steps) if s.startswith("wait first-news"))
    return steps[:end + 1] + ["2000 quit"]


def wait_room():
    while True:
        games = subprocess.run(["pgrep", "-x", "cm0102_recomp"], capture_output=True, text=True).stdout.split()
        if len(games) < 14 and os.getloadavg()[0] < 24 and not os.path.exists("/Users/max/Projects/cm0102/run-logs/HOLD"):
            return
        time.sleep(30)


def language_cfg(program, game, user, language):
    """game.cfg of a first start with the settings language (the int at offset 8) set to language."""
    r, _ = fixtures.run(program, game, "classic", user, ["1000 click 400 300", *SETTINGS, "wait setup", "2000 quit"],
                        {}, 300)
    path = os.path.join(user, "game.cfg")
    if r.returncode != 0 or not os.path.exists(path):
        return False
    with open(path, "r+b") as f:
        f.seek(8)
        f.write(language.to_bytes(4, "little"))
    return True


def section_names(path, section):
    """The names of a section of a .lng file."""
    with open(path, "rb") as f:
        data = f.read()
    counts = struct.unpack_from("<8i", data, 0)
    at = 32
    for s, size in SECTIONS:
        n = counts[s]
        if s == section:
            return [data[at + i * size:at + i * size + 51].split(b"\0")[0].decode("latin-1") for i in range(n)]
        at += n * size
    return []


def differing_rows(written, data, game):
    """The rows in which a written .lng differs from the game's. The two files must be byte for byte the same,
    except in a club row where the data and eng.lng name other clubs: the 3.9.68 data holds the two clubs Fenix
    (rows 3980 and 3981, with other accents) in the other order than eng.lng, and the fit takes the row of the
    record's own name where the original reads by row number."""
    a, b = open(written, "rb").read(), open(data, "rb").read()
    if a[:32] != b[:32] or len(a) != len(b):
        return ["header"]
    clubs = []
    for name in ("club.dat", "nat_club.dat"):
        with open(os.path.join(game, "Data", name), "rb") as f:
            raw = f.read()
        clubs += [raw[i + 4:i + 55].split(b"\0")[0] for i in range(0, len(raw), 581)]
    eng = open(os.path.join(game, "Data", "eng.lng"), "rb").read()
    club_end = 32 + struct.unpack_from("<i", a, 0)[0] * 0x4f
    out = []
    for i in range(32, len(a)):
        if a[i] == b[i]:
            continue
        row = (i - 32) // 0x4f if i < club_end else None
        if row is not None and row < len(clubs) and \
                clubs[row] != eng[32 + row * 0x4f:32 + row * 0x4f + 51].split(b"\0")[0]:
            continue
        out.append(row if row is not None else i)
        if len(out) > 20:
            break
    return sorted(set(out))


def play(program, game, work, steps, env):
    wait_room()
    r, took = fixtures.run(program, game, "classic", os.path.join(work, "user"),
                           smoke_run.scaled_steps("classic", work, steps),
                           dict(env, RECOMP_SCREEN_LOG="1"), 900)
    with open(os.path.join(work, "stderr.txt"), "w") as f:
        f.write(r.stderr)
    problems = [l.split(": ", 1)[1][:120] for l in r.stderr.splitlines() if l.startswith(BOX)]
    if r.returncode != 0:
        problems.append(f"code {r.returncode}")
    if SKIPPED in r.stderr:
        problems.append("the box Database..cpp 7784 came (the runtime skipped it)")
    return r.stderr, problems


def setup_user(program, game, work, language, options, database=None):
    user = os.path.join(work, "user")
    os.makedirs(user, exist_ok=True)
    if database:
        shutil.copytree(database, os.path.join(user, "Databases", os.path.basename(database)))
    fixtures.write_settings(user, "classic", options)
    wait_room()
    return language_cfg(program, game, user, language)


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    folder = argv[argv.index("--dir") + 1] if "--dir" in argv else fixtures.DEFAULT_DIR
    only = argv[argv.index("--only") + 1].split(",") if "--only" in argv else None
    keep = os.path.abspath(argv[argv.index("--keep") + 1]) if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-lng-")
    os.makedirs(keep, exist_ok=True)
    results = []

    def want(name):
        return not only or name in only

    if want("base"):
        work = os.path.join(keep, "base")
        shutil.rmtree(work, ignore_errors=True)
        ok = setup_user(program, game, work, ITALIAN, [("changing_rules", "off")])
        err, problems = play(program, game, work, LEAGUES, {}) if ok else ("", ["no game.cfg"])
        if FITTED in err:
            problems.append("a fitted file was made, though the 3.9.68 files fit")
        results.append(("base", problems))
    if want("equal"):
        work = os.path.join(keep, "equal")
        shutil.rmtree(work, ignore_errors=True)
        ok = setup_user(program, game, work, ITALIAN, [("changing_rules", "off")])
        err, problems = play(program, game, work, LEAGUES, {"RECOMP_LNG_FIT": "always"}) if ok else ("", ["no game.cfg"])
        dump, data = os.path.join(work, "user", "ita.lng"), os.path.join(game, "Data", "ita.lng")
        if not os.path.exists(dump):
            problems.append("no written ita.lng")
        else:
            rows = differing_rows(dump, data, game)
            if rows:
                problems.append(f"the written ita.lng differs from Data/ita.lng in rows {rows[:5]}")
        results.append(("equal", problems))
    season = os.path.join(keep, "season")
    if want("season-ita") or want("season-fra"):
        shutil.rmtree(season, ignore_errors=True)
        ok = setup_user(program, game, season, ITALIAN, [("database", "folder:" + SEASON_DB)],
                        os.path.join(folder, "databases", SEASON_DB))
        err, problems = play(program, game, season, NEW_GAME + SAVE_AS, {}) if ok else ("", ["no game.cfg"])
        dump = os.path.join(season, "user", "ita.lng")
        if FITTED not in err or not os.path.exists(dump):
            problems.append("no written ita.lng")
        elif "Germania" not in section_names(dump, 3):
            problems.append("the fitted ita.lng has no nation Germania")
        if not os.path.exists(os.path.join(season, "user", SAVE + ".sav")):
            problems.append(f"no {SAVE}.sav")
        results.append(("season-ita", problems))
    if want("accent"):
        work = os.path.join(keep, "season-Jos\u00e9")             # a user folder with an accent (host.h paths)
        shutil.rmtree(work, ignore_errors=True)
        ok = setup_user(program, game, work, ITALIAN, [("database", "folder:" + SEASON_DB)],
                        os.path.join(folder, "databases", SEASON_DB))
        err, problems = play(program, game, work, LEAGUES, {}) if ok else ("", ["no game.cfg"])
        dump = os.path.join(work, "user", "ita.lng")
        if FITTED not in err or not os.path.exists(dump):
            problems.append("no written ita.lng")
        elif "Germania" not in section_names(dump, 3):
            problems.append("the written ita.lng has no nation Germania")
        results.append(("accent", problems))
    if want("season-fra"):
        user = os.path.join(season, "user")
        problems = []
        if os.path.exists(os.path.join(user, SAVE + ".sav")):
            for n in os.listdir(user):
                if n.lower().endswith(".lng"):
                    os.remove(os.path.join(user, n))      # the load must bring them back from the save
            with open(os.path.join(user, "game.cfg"), "r+b") as f:
                f.seek(8)
                f.write(FRENCH.to_bytes(4, "little"))
            err, problems = play(program, game, season, LOAD, {})
            dump = os.path.join(season, "user", "fra.lng")
            if not os.path.exists(dump):
                problems.append("no fra.lng from the save")
            elif "Allemagne" not in section_names(dump, 3):
                problems.append("the fitted fra.lng has no nation Allemagne")
        else:
            problems.append(f"no {SAVE}.sav from season-ita")
        results.append(("season-fra", problems))
    if want("added-clubs"):
        if not os.path.exists(os.path.join(game, "Seasons", "October 2016.zip")):
            print("--    added-clubs: left out (no Seasons/October 2016.zip in the game folder)")
        else:
            work = os.path.join(keep, "added")
            shutil.rmtree(work, ignore_errors=True)
            ok = setup_user(program, game, work, ITALIAN, [("changing_rules", "on")])
            env = {"RECOMP_START_SEASON": "2016", "RECOMP_RULES_TABLE": os.path.join(game, "Seasons", "rules.tsv"),
                   "RECOMP_LNG_LOG": "1"}
            table = os.path.join(os.path.dirname(program), "seasons", "rules.tsv")
            if os.path.exists(table):
                env["RECOMP_RULES_TABLE"] = table
            err, problems = play(program, game, work, all_leagues_steps(), env) if ok else ("", ["no game.cfg"])
            dump = os.path.join(work, "user", "ita.lng")
            if "recomp: club records of 2016: 1 clubs" not in err:
                problems.append("the club records added no club")
            if FITTED not in err or not os.path.exists(dump):
                problems.append("no written ita.lng")
            else:
                clubs = section_names(dump, 0)
                m = re.search(r"recomp: lng: added club AO Kerkyra has \.lng row (\d+)", err)
                if not m:
                    problems.append("no .lng row of the added club AO Kerkyra in the log")
                elif int(m.group(1)) == 0 or int(m.group(1)) >= len(clubs) or clubs[int(m.group(1))] != "AO Kerkyra":
                    problems.append(f"the .lng row {m.group(1)} of AO Kerkyra ([0xae23fc]) does not hold its name")
            results.append(("added-clubs", problems))
    failed = 0
    for name, problems in results:
        print(f"{'FAIL ' if problems else 'ok   '} {name}{': ' + '; '.join(problems) if problems else ''}")
        failed += bool(problems)
    print("FAILED" if failed else "PASS")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
