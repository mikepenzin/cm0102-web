#!/usr/bin/env python3
"""Choose Season screen check (0.6.0 slice 13): each tile starts its season database.

Usage:
    python3 -I tests/seasons/season_screen_run.py <cm0102_recomp> <game folder> <folder of source zips>
        [--ref-build <program>] [--only <run>] [--shots <folder>] [--own-zips <folder>]
        [--data-3960 <folder> --data-3965 <folder>]

<folder of source zips> is test-fixtures/databases/zips. The run makes the season zips of four seasons with
tools/release/make_season_zips.py and a new game folder: links to the files of <game folder> and Seasons/ with
those zips, as in a download. Each run has a new user folder, -seed 1, the test clock and classic mode.

Runs, played at the same time (one per era, Q240):
    2003    2000s: the 2003/04 tile (October 2003)
    2010    2010s: the 2010/11 tile (September 2010 ODB)
    2021    early 2020s: the 2021/22 tile (October 2021)
    2025    league phase: the 2025/26 tile (December 2025)
    2002    with --own-zips: the 2002/03 tile (Summer 2002, our own database, Summer 2002 spec slice 4)
    2004    with --own-zips: the 2004/05 tile (Summer 2004, our own database, Q351)
    2006    with --own-zips: the 2006/07 tile (Summer 2006, our own database, Q351). <folder> holds the source
            zips of the converted folders; it is a second source of make_season_zips.py.
Each run opens the screen from Setup Game, clicks the tile and Choose, waits for Setup Game, starts a new game
with the steps of tests/smoke/smoke_run.py and saves 1 day after the first news screen. Checks: the first news
screen is in July of the season, settings.txt has database and season of the tile, a zipped database is
unpacked into Databases (the log of seasons_db_prepare), and the save has the season part with the year.

    custom      two Custom database folders in Databases (copies of the clean Data and of the April 2022 database of
                <folder of source zips>/.., each with one byte added to staff.dat, so neither is a known
                database): the Custom tile, Next folder (the second folder), the season arrow three times (2022/23),
                Choose and a new game. Checks: settings.txt has database = folder:B Custom and season = 2022/23,
                and the first news screen is in July 2022.
    base        the 2001/02 tile through the screen with changing_rules = off, saved 14 days after the first
                news screen: the save has no season part and the first news screen is in 2001. The run "ref" plays
                the plain new game (this build, or --ref-build) and the run prints a note whether the saves are the
                same: opening a screen moves the original's memory blocks (Q152), so a difference is expected.
    data60      the data sets of the 2001/02 tile (item 11 of 0.6.1), with no database line (the default): the
                screen opens with the 2001/02 tile chosen, three clicks on it go to 3.9.65, 3.9.68 and back to
                3.9.60, then Start. Checks: settings.txt has database = 3.9.60 and Rio Ferdinand is at Leeds in
                the save.
    data65      as data60 with one click: database = 3.9.65, Ferdinand at Leeds.
    data68      database = 3.9.65 in settings.txt: a click on the 2003/04 tile, a click on the 2001/02 tile (it
                chooses the tile and keeps 3.9.65, the value of the setting), a click more (3.9.68), then Start.
                Checks: database = 3.9.68, Ferdinand at Manchester United.
                The data runs need Data-3.9.60 and Data-3.9.65 in <game folder>, or --data-3960 and --data-3965.
                "--only data" plays the three of them and makes no season zips.
--shots keeps a picture of the screen after the tile click of each run in that folder.
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

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tests", "smoke"))
sys.path.insert(0, HERE)
import season_core_run as core  # noqa: E402
import smoke_run  # noqa: E402
sys.path.insert(0, os.path.join(ROOT, "tools", "save"))
sys.path.insert(0, os.path.join(ROOT, "tools", "seasons", "own_db"))
import cmdb  # noqa: E402
import savfile  # noqa: E402

MAKE = os.path.join(ROOT, "tools", "release", "make_season_zips.py")

# Classic positions (800 x 600). Setup Game: the season button, left in the top row (database.c). The screen:
# the tiles (season_screen.c layout) and the Choose button of the strip.
SETUP_SEASON = "click 277 109"
# Layout C (Q346): a decade per row, a column per last digit of the start year: x = 132 + 69 * digit,
# y = 89 + 93 * row.
TILE = {2001: (201, 89), 2002: (270, 89), 2003: (339, 89), 2004: (408, 89), 2006: (546, 89), 2010: (132, 182), 2021: (201, 275),
        2025: (477, 275)}
CHOOSE = "click 675 573"                    # Start <season>, the yellow button
CUSTOM_TILE = "click 649 275"
NEXT_FOLDER = "click 470 415"
EARLIER = "click 327 415"
CUSTOM_FOLDERS = ("A Custom", "B Custom")
# year: (database value, zip to make or None)
RUNS = {2003: ("folder:October 2003", "October 2003"),
        2010: ("folder:September 2010 ODB", "September 2010 ODB"), 2021: ("folder:October 2021", "October 2021"),
        2025: ("folder:December 2025", "December 2025")}


def screen_steps(year, shots):
    x, y = TILE[year]
    steps = [f"2000 {SETUP_SEASON}", "wait seasons", f"2000 click {x} {y}"]
    if shots:
        steps.append(f"3500 dump {shots}/tile-{year}.bmp")
    return steps + [f"4000 {CHOOSE}", "wait setup 300000"]


def game_steps(year, shots):
    """The new game steps of smoke_run.py with the screen steps after the first wait for Setup Game."""
    base = core.new_game_steps()
    i = base.index("wait setup") + 1
    return base[:i] + screen_steps(year, shots) + base[i:]


# The data runs (item 11 of 0.6.1): settings.txt, the clicks on the 2001/02 tile, the database and the club of
# Rio Ferdinand that the save must have.
DATA_RUNS = {"data60": ({}, [2001, 2001, 2001], "3.9.60", "Leeds United"),
             "data65": ({}, [2001], "3.9.65", "Leeds United"),
             "data68": ({"database": "3.9.65"}, [2003, 2001, 2001], "3.9.68", "Manchester United")}


def data_steps(clicks, shots, name):
    base = core.new_game_steps()
    i = base.index("wait setup") + 1
    screen = [f"2000 {SETUP_SEASON}", "wait seasons"]
    for k, year in enumerate(clicks):
        screen.append(f"{2000 + 1500 * k} click {TILE[year][0]} {TILE[year][1]}")
    t = 2000 + 1500 * len(clicks)
    if shots:
        screen.append(f"{t} dump {shots}/tile-{name}.bmp")
    screen += [f"{t + 1000} {CHOOSE}", "wait setup 300000"]
    return base[:i] + screen + base[i:]


def clubs_in_save(path, first, second):
    """The clubs of the players with these names in the data tables of a saved game (cmdb.py reads them: the
    save's index.dat has the same rows as a data folder's, with more tables)."""
    sav = savfile.Sav(path)
    f = cmdb.Folder.__new__(cmdb.Folder)
    f.path, f.raw, f.idx = path, {}, {}
    for n in ("index.dat", "staff.dat", "first_names.dat", "second_names.dat", "common_names.dat", "club.dat",
              "nat_club.dat"):
        f.raw[n] = bytearray(sav.read_member(n))
    ib = f.raw["index.dat"]
    for i in range(8, len(ib) - 66, 67):
        name = ib[i:i + 51].split(b"\0")[0].decode("latin1")
        if name in f.raw:
            tid, count, off, ver = struct.unpack_from("<iiii", ib, i + 51)
            f.idx.setdefault(tid, dict(file=name, count=count, off=off, ver=ver, pos=i))
    staff, clubs = cmdb.staff_view(f)
    return [clubs.get(p["club"]) for p in staff if p["first"] == first and p["second"] == second]


def custom_steps():
    base = core.new_game_steps()
    i = base.index("wait setup") + 1
    screen = [f"2000 {SETUP_SEASON}", "wait seasons", f"2000 {CUSTOM_TILE}", f"3500 {NEXT_FOLDER}",
              f"5000 {EARLIER}", f"6500 {EARLIER}", f"8000 {EARLIER}", f"9500 {CHOOSE}", "wait setup"]
    return base[:i] + screen + base[i:]


def play_custom(program, game, work, zips):
    """The custom run: its user folder gets the two Custom folders first, then the settings of core.play."""
    run = os.path.join(work, "custom")
    user, home, shots = (os.path.join(run, d) for d in ("user", "home", "shots"))
    sources = (os.path.join(game, "Data"), os.path.join(os.path.dirname(zips), "April 2022"))
    for name, src in zip(CUSTOM_FOLDERS, sources):
        dest = os.path.join(user, "Databases", name)
        os.makedirs(dest)
        for n in os.listdir(src):
            if n.lower().endswith(".dat"):
                shutil.copyfile(os.path.join(src, n), os.path.join(dest, n.lower()))
        with open(os.path.join(dest, "staff.dat"), "ab") as f:
            f.write(b"\0")
    for d in (home, shots):
        os.makedirs(d)
    with open(os.path.join(user, "settings.txt"), "w") as f:
        f.write("display = classic\n")
    env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
               RECOMP_USER_DIR=user, RECOMP_FIXED_CLOCK="1", RECOMP_SAVE_AFTER_DAYS="1", RECOMP_SCREEN_LOG="1",
               RECOMP_SCRIPT="; ".join(smoke_run.scaled_steps("classic", shots, custom_steps())))
    start = time.time()
    r = subprocess.run([program, "--game", game, "-seed", "1"], env=env, capture_output=True, text=True, timeout=900)
    path = os.path.join(user, smoke_run.SAVE_NAME)
    return (path if r.returncode == 0 and os.path.exists(path) else None), r.stderr, time.time() - start


def make_game(game, zips, work, own=None, data_sets=None):
    """A game folder as in a download: links to the files of game, and Seasons/ with the season zips (none when
    zips is None). data_sets: {"Data-3.9.60": folder, ...} links the data sets that game lacks."""
    out = os.path.join(work, "zips")
    if zips:
        only = [a for _, (_, z) in RUNS.items() if z for a in ("--only", z)]
        sources = ["--season-dbs", zips] + (["--season-dbs", own] if own else [])
        r = subprocess.run([sys.executable, "-I", MAKE, *sources, "--out", out, *only], capture_output=True,
                           text=True)
        if r.returncode:
            raise SystemExit("make_season_zips.py failed: " + r.stderr[-400:])
    folder = os.path.join(work, "game")
    os.makedirs(folder)
    for n in os.listdir(game):
        if n != "Seasons":
            os.symlink(os.path.join(game, n), os.path.join(folder, n))
    for n, src in (data_sets or {}).items():
        if src and not os.path.exists(os.path.join(folder, n)):
            os.symlink(src, os.path.join(folder, n))
    if zips:
        os.symlink(out, os.path.join(folder, "Seasons"))
    return folder


def settings_of(path):
    out = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or "=" not in line:
                continue
            k, _, v = line.partition("=")
            out[k.strip()] = v.strip()
    return out


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2
    program, game, zips = (os.path.abspath(a) for a in argv[1:4])
    ref = os.path.abspath(argv[argv.index("--ref-build") + 1]) if "--ref-build" in argv else None
    only = argv[argv.index("--only") + 1] if "--only" in argv else None
    shots = os.path.abspath(argv[argv.index("--shots") + 1]) if "--shots" in argv else None
    own = os.path.abspath(argv[argv.index("--own-zips") + 1]) if "--own-zips" in argv else None
    data_sets = {f"Data-{v}": os.path.abspath(argv[argv.index(f"--data-{v.replace('.', '')}") + 1])
                 for v in ("3.9.60", "3.9.65") if f"--data-{v.replace('.', '')}" in argv}
    if only in ("2002", "2004", "2006") and not own:
        print(f"FAIL  run {only} needs --own-zips (our own databases)")
        return 1
    if own:
        RUNS[2002] = ("folder:Summer 2002", "Summer 2002")
        RUNS[2004] = ("folder:Summer 2004", "Summer 2004")
        RUNS[2006] = ("folder:Summer 2006", "Summer 2006")
    if shots:
        os.makedirs(shots, exist_ok=True)
    failed = []

    def check(cond, text):
        print(("ok    " if cond else "FAIL  ") + text, flush=True)
        if not cond:
            failed.append(text)

    with tempfile.TemporaryDirectory(prefix="cm0102-season-screen-") as work:
        folder = make_game(game, None if only == "data" else zips, work, own, data_sets)
        jobs = {}
        with concurrent.futures.ThreadPoolExecutor(7) as pool:
            for year in RUNS:
                if only in (None, str(year)):
                    jobs[str(year)] = pool.submit(core.play, program, folder, work, str(year), {},
                                                  game_steps(year, shots), 1)
            for name, (options, clicks, _, _) in DATA_RUNS.items():
                if only in (None, "data", name):
                    jobs[name] = pool.submit(core.play, program, folder, work, name, options,
                                             data_steps(clicks, shots, name), 1)
            if only in (None, "custom"):
                jobs["custom"] = pool.submit(play_custom, program, folder, work, zips)
            if only in (None, "base"):
                jobs["base"] = pool.submit(core.play, program, folder, work, "base",
                                           {"changing_rules": "off", "database": "3.9.68"},
                                           game_steps(2001, shots), 14)
                # Q342: the reference is the plain new game of the same build (or of --ref-build, the main build
                # from slice 13 on): opening the screen and choosing 2001/02 must not change the game.
                jobs["ref"] = pool.submit(core.play, ref or program, folder, work, "ref",
                                          {"changing_rules": "off", "database": "3.9.68"},
                                          core.new_game_steps(), 14, None, None, None, 900)
            results = {k: v.result() for k, v in jobs.items()}
        for name, (path, log, took) in results.items():
            print(f"run   {name}: {'saved' if path else 'NO SAVE'} in {took:.0f} s", flush=True)
            if not path:
                print(log[-1500:])
        for year, (value, zipname) in RUNS.items():
            if str(year) not in results:
                continue
            path, log, _ = results[str(year)]
            if not path:
                check(False, f"{year}: no save")
                continue
            news = core.first_news(log)
            check(news is not None and news[1] == year and core.month_of(news[0], year) == 7,
                  f"{year}: the first news screen is in July {year} (date {news})")
            s = settings_of(os.path.join(work, str(year), "user", "settings.txt"))
            label = f"{year}/{(year + 1) % 100:02d}"
            check(s.get("database") == value and s.get("season") == label,
                  f"{year}: settings.txt has database = {s.get('database')}, season = {s.get('season')}")
            if zipname:
                check(re.search(rf"choose season {year}: prepare status 0, folder {re.escape(zipname)}, unpacked 1",
                                log) is not None, f"{year}: the tile unpacked {zipname} into Databases")
                check(os.path.isdir(os.path.join(work, str(year), "user", "Databases", zipname)),
                      f"{year}: Databases/{zipname} is there")
            part = core.read_part(path)
            check(part is not None and part[2] == year, f"{year}: the save has the season part with {year} ({part})")
        for name, (_, _, value, club) in DATA_RUNS.items():
            if name not in results:
                continue
            path, log, _ = results[name]
            s = settings_of(os.path.join(work, name, "user", "settings.txt"))
            check(s.get("database") == value and s.get("season") == "2001/02",
                  f"{name}: settings.txt has database = {s.get('database')}, season = {s.get('season')}")
            clubs = clubs_in_save(path, "Rio", "Ferdinand") if path else None
            check(clubs == [club], f"{name}: Rio Ferdinand is at {clubs} in the save (want {club})")
        if "custom" in results:
            path, log, _ = results["custom"]
            s = settings_of(os.path.join(work, "custom", "user", "settings.txt"))
            check(s.get("database") == "folder:B Custom" and s.get("season") == "2022/23",
                  f"custom: settings.txt has database = {s.get('database')}, season = {s.get('season')}")
            news = core.first_news(log) if path else None
            check(news is not None and news[1] == 2022 and core.month_of(news[0], 2022) == 7,
                  f"custom: the first news screen is in July 2022 (date {news})")
            part = core.read_part(path) if path else None
            check(part is not None and part[2] == 2022, f"custom: the save has the season part with 2022 ({part})")
        if "base" in results:
            path, log, _ = results["base"]
            check(path is not None and core.read_part(path) is None, "base: the save has no season part")
            news = core.first_news(log) if path else None
            check(news is not None and news[1] == 2001, f"base: the first news screen is in 2001 ({news})")
            if "ref" in results and results["ref"][0] and path:
                same = open(path, "rb").read() == open(results["ref"][0], "rb").read()
                if not same:
                    a, b = smoke_run.save_parts(path), smoke_run.save_parts(results["ref"][0])
                    differ = sorted(k for k in set(a) | set(b) if a.get(k) != b.get(k))
                    print("      parts that differ: " + ", ".join(differ))
                # Opening a screen moves the original's memory blocks, so the later days differ (Q152): this is a
                # note, not a check. The proof that the season code changes nothing is the run season-code-off of
                # smoke_run.py --same-save (Q342).
                print(f"note  base: the save through the screen {'equals' if same else 'differs from'} the plain new "
                      "game (opening a screen moves memory, Q152)")
        if shots:
            for n in os.listdir(shots):
                if n.endswith(".bmp"):
                    subprocess.run(["sips", "-s", "format", "png", os.path.join(shots, n), "--out",
                                    os.path.join(shots, n[:-4] + ".png")], capture_output=True)
    print("PASS" if not failed else f"FAIL ({len(failed)})")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
