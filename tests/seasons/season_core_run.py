#!/usr/bin/env python3
"""Season core check (0.6.0 slice 11): a game that starts in another season, and the season part of a save.

Usage:
    python3 tests/seasons/season_core_run.py <cm0102_recomp> <game folder> [--database <folder>] [--year y]
        [--days n] [--only base|season|rename] [--keep <folder>] [--jobs n]

The checks, played at the same time:
    base-off    a new base game (2001/02, 3.9.68, changing_rules = off) saved after n days (default 14): its save
                has no season part, as a save of 0.5.0.
    base-on     the same with changing_rules = on: the save has the part with 2001 and changing rules on.
    season      a new game in the year (default 2010) with the database folder (default the September 2010 ODB
                of test-fixtures/databases): the first news screen is in July of the year, the fixture store
                holds no calendar year before it (RECOMP_SEASON_LOG), the game goes on holiday for n days without
                a crash and the save has the part with the year and changing rules on.
    season-again  the same steps again: the save is byte for byte the same (the same clicks, the same game).
    season-italy  the season game with Italy selected too (the Italian leagues hold fixed 2001/02 fixture lists):
                the first news screen is in July of the year and the fixture store holds no year before it.
    season-off  the season game with changing_rules = off: the part holds the year and changing rules off.
    reload      loads the save of "season" in a new user folder with no RECOMP_START_SEASON, plays 3 days and
                saves: the runtime reads the year from the part, and the new save has the part with the year.
    reload-off  the same with the save of season-off: the new save keeps changing rules off.
    later       the save of "season" with a part of a later version (version 3: the fields of version 2, then 8
                bytes): it loads with the fields that this version knows, and the new save has a version 1 or 2
                part (2 when the game has names, slice 15).
    damaged     the save of "season" with a damaged part (year 1999): the load is refused with a message box.
Names and club moves (slice 15): the runs read the season tables of the build (RECOMP_RULES_TABLE, build
folder seasons/), when the build has them. Then also:
    base-off    no competition gets a name and no club moves (the base game);
    season      the log shows the club moves and the names of the start season; the save holds the season
                names (club_comp.dat) and its season part the database names, which the load matches by name
                (0x60efd0, season_change_rename of slice 16);
    reload      the load gives the names again.
Abbreviations (0.6.1 item 2): the base-off save keeps the data's PRM, D1, D2 and D3 for the English divisions;
the season save (a year of 2004 or later) and the reload save have PL, CH, L1 and L2, as Select Team shows them.
    rename      a new base game (2001/02, changing_rules = off) with the test variable
                RECOMP_SEASON_CHANGE_TEST=rename (the record "UEFA Cup" becomes "Test Cup" at the new game, as a
                name of slice 15 would): its save has the season part, version 2, with the name in the data.
    rename-reload  loads that save, plays 3 days and saves: the load finds its key competitions (no box
                "index.cpp 5809"), and the new save keeps the name in the data.
--keep <folder> keeps the save of "season" there (for a check with another build).
--jobs n plays at most n games at the same time (default 6).
Each run uses -seed 1, the test clock and a new user folder. The steps are those of tests/smoke/smoke_run.py
(Arsenal, or the first club of the list) in classic mode.
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
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "save"))
import custom_database_run as cdr  # noqa: E402
import rules_report_check as rrc  # noqa: E402
import fixtures  # noqa: E402
import savfile  # noqa: E402
import smoke_run  # noqa: E402

PART = "bosman_season.dat"
# 0.6.1 item 2: the abbreviation of the English divisions in a season game (record +0x53), which Select Team
# shows; the 3.9.68 data gives PRM, D1, D2 and D3.
ENGLISH = {"Premier League": "PL", "Football League Championship": "CH", "Football League One": "L1",
           "Football League Two": "L2"}


def abbreviations(comps):
    """{long name: abbreviation} of the records of a club_comp.dat member (0x6b bytes a record)."""
    out = {}
    for i in range(0, len(comps) - 0x6a, 0x6b):
        name = comps[i + 4:i + 56].split(b"\0")[0].decode("cp1252", "replace")
        out[name] = comps[i + 0x53:i + 0x57].split(b"\0")[0].decode("cp1252", "replace")
    return out


def english_abbreviations(path):
    """The English divisions of a season save whose abbreviation is not the season's: [(name, abbreviation)]."""
    got = abbreviations(savfile.Sav(path).read_member("club_comp.dat"))
    return [(n, got.get(n)) for n, a in ENGLISH.items() if got.get(n) != a]
NEWS = "0x7881e0"
DEFAULT_DB = os.path.join(fixtures.DEFAULT_DIR, "databases", "September 2010 ODB")
MONTH_DAYS = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]


def leap(y):
    return (y % 4 == 0 and y % 100 != 0) or y % 400 == 0


def month_of(day, year):
    """The month (1 to 12) of a day of the year of the game date (from 1 on 1 January, Date::set 0x54a130)."""
    days = list(MONTH_DAYS)
    if leap(year):
        days[1] = 29
    n = day
    for m, d in enumerate(days):
        if n <= d:
            return m + 1
        n -= d
    return 13


def part_is(part, year, rules):
    """True for a season part of version 1 (16 bytes) or 2 (with the news and the names of slice 16, which a
    season game with renamed competitions writes, slice 15) with that start year and changing rules."""
    return part is not None and (part[:2] == (1, 16) or (part[0] == 2 and part[1] > 16)) and part[2:] == (year, rules)


def read_part(path):
    """(version, length, start year, changing rules) of the season part of a save, or None."""
    sav = savfile.Sav(path)
    try:
        data = sav.read_member(PART)
    except KeyError:
        return None
    return struct.unpack_from("<4I", data, 0)


def replace_part(src, dst, data):
    """Writes dst: the save src with the bytes of its season part replaced by data (any length). The runtime
    adds the part at the end of the file, so the new bytes go there and its row gets the new size."""
    blob = bytearray(open(src, "rb").read())
    sav = savfile.Sav(src)
    m = sav.member(PART)
    if m.offset != max(x.offset for x in sav.real_members()):
        raise ValueError("the season part is not the last part")
    raw = savfile.encode(data) if sav.compressed else data
    struct.pack_into("<i", blob, 12 + m.index * savfile.ENTRY_SIZE + 4, len(data))
    with open(dst, "wb") as f:
        f.write(bytes(blob[:m.offset]) + raw)


def new_game_steps():
    steps = [s for s in smoke_run.STEPS if "5-holiday" not in s and "quit" not in s]
    return smoke_run.without_intro_click(steps) + ["400000 quit"]


def italy_steps():
    """The new game steps with Italy selected too: the Italian leagues hold the fixed 2001/02 fixture lists.
    With two leagues the game asks for the start season (screen 0x81e8d0, England is chosen), and the Select
    Team screen has a Nation box above the list, so Arsenal is one row lower."""
    out = []
    for s in new_game_steps():
        if s == "2500 click 705 572" and not any("390 451" in x for x in out):
            out += ["2000 click 390 451",               # Select League(s): Italy
                    "4000 click 705 572",               # Next
                    "wait screen:0x81e8d0",
                    "2500 click 705 572"]               # Select Start Season: Next (England)
            continue
        if s in ("wait typing", "wait first-news"):
            s += " 600000"                              # the game is made after the start season
        if s == "1000 click 155 163":
            s = "2000 click 155 188"                    # Select Team: Arsenal
        out.append(s)
    return out


def play(program, game, work, name, options, steps, days, env_extra=None, database=None, save=None, timeout=900):
    run = os.path.join(work, name)
    user, home, shots = (os.path.join(run, d) for d in ("user", "home", "shots"))
    for d in (user, home, shots):
        os.makedirs(d)
    if database:                               # never the base data in place of the database
        database = rrc.database_folder(database)
        cdr.copy_data(database, os.path.join(user, "Databases", os.path.basename(database)))
        options = dict(options, database="folder:" + os.path.basename(database))
    if save:
        shutil.copyfile(save, os.path.join(user, os.path.basename(save)))
    with open(os.path.join(user, "settings.txt"), "w") as f:
        f.write("display = classic\n")
        for k, v in options.items():
            f.write(f"{k} = {v}\n")
    env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
               RECOMP_USER_DIR=user, RECOMP_FIXED_CLOCK="1", RECOMP_SAVE_AFTER_DAYS=str(days),
               RECOMP_SCREEN_LOG="1", RECOMP_SEASON_LOG="1",
               RECOMP_SCRIPT="; ".join(smoke_run.scaled_steps("classic", shots, steps)), **(env_extra or {}))
    table = os.path.join(os.path.dirname(program), "seasons", "rules.tsv")
    if os.path.exists(table):
        env["RECOMP_RULES_TABLE"] = table
    start = time.time()
    try:
        r = subprocess.run([program, "--game", game, "-seed", "1"], env=env, capture_output=True, text=True, errors="replace",
                           timeout=timeout)
    except subprocess.TimeoutExpired as e:
        return None, (e.stderr or b"").decode("latin-1") if isinstance(e.stderr, bytes) else (e.stderr or ""), \
            time.time() - start
    path = os.path.join(user, smoke_run.SAVE_NAME)
    with open(os.path.join(run, "stderr.txt"), "w") as f:
        f.write(r.stderr)
    if database:
        rrc.no_fallback(os.path.join(run, "stderr.txt"), database)
    return (path if r.returncode == 0 and os.path.exists(path) else None), r.stderr, time.time() - start


def first_news(log):
    m = re.search(r"date (\d+)/(\d+): screen " + NEWS, log)
    return (int(m.group(1)), int(m.group(2))) if m else None


def fixture_years(log):
    years = set()
    for m in re.finditer(r"season log: date [-\d]+/[-\d]+, start year \d+, changing rules \w+, fixture years([ \d-]*)",
                         log):
        years |= {int(y) for y in m.group(1).split()}
    return years


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    database = os.path.abspath(argv[argv.index("--database") + 1]) if "--database" in argv else DEFAULT_DB
    year = int(argv[argv.index("--year") + 1]) if "--year" in argv else 2010
    days = int(argv[argv.index("--days") + 1]) if "--days" in argv else 14
    only = argv[argv.index("--only") + 1] if "--only" in argv else None
    keep = os.path.abspath(argv[argv.index("--keep") + 1]) if "--keep" in argv else None
    jobs_max = int(argv[argv.index("--jobs") + 1]) if "--jobs" in argv else 6
    tables = os.path.exists(os.path.join(os.path.dirname(program), "seasons", "names.tsv"))
    failed = []

    def fail(text):
        failed.append(text)
        print("FAIL  " + text, flush=True)

    with tempfile.TemporaryDirectory(prefix="cm0102-season-") as work:
        steps = new_game_steps()
        season_env = {"RECOMP_START_SEASON": str(year)}
        jobs = {}
        with concurrent.futures.ThreadPoolExecutor(jobs_max) as pool:
            if only in (None, "base"):
                jobs["base-off"] = pool.submit(play, program, game, work, "base-off", {"changing_rules": "off"},
                                               steps, days)
                jobs["base-on"] = pool.submit(play, program, game, work, "base-on", {"changing_rules": "on"},
                                              steps, days)
            if only in (None, "season"):
                for name in ("season", "season-again"):
                    jobs[name] = pool.submit(play, program, game, work, name, {}, steps, days, season_env, database)
                jobs["season-off"] = pool.submit(play, program, game, work, "season-off", {"changing_rules": "off"},
                                                 steps, days, season_env, database)
                jobs["season-italy"] = pool.submit(play, program, game, work, "season-italy", {}, italy_steps(), days,
                                                   season_env, database)
            results = {k: v.result() for k, v in jobs.items()}
        for name, (path, log, took) in results.items():
            print(f"run   {name}: {'saved' if path else 'NO SAVE'} in {took:.0f} s", flush=True)
            if not path:
                fail(f"{name}: no saved game; last lines: {log[-600:]!r}")
        if "base-off" in results and results["base-off"][0]:
            part = read_part(results["base-off"][0])
            if part is not None:
                fail(f"base-off: the save has the season part {part}")
            else:
                print("ok    base-off: the save has no season part")
            got = abbreviations(savfile.Sav(results["base-off"][0]).read_member("club_comp.dat"))
            base = [got.get(n) for n in ("English Premier Division", "English First Division",
                                         "English Second Division", "English Third Division")]
            if base != ["PRM", "D1", "D2", "D3"]:
                fail(f"base-off: the English divisions have the abbreviations {base}, not the data's")
            else:
                print("ok    base-off: the English divisions keep PRM, D1, D2 and D3")
            blog = results["base-off"][1]
            if tables and ("recomp: names of" in blog or "recomp: club moves of" in blog):
                fail("base-off: the base game got names or club moves")
            elif tables:
                print("ok    base-off: no names and no club moves")
        if "base-on" in results and results["base-on"][0]:
            part = read_part(results["base-on"][0])
            if not part_is(part, 2001, 1):
                fail(f"base-on: the season part is {part}, not one with 2001 and changing rules on")
            else:
                print("ok    base-on: the save has the part with 2001 and changing rules on")
        if "season" in results and results["season"][0]:
            path, log, _ = results["season"]
            news = first_news(log)
            if not news or news[1] != year or month_of(*news) != 7:
                fail(f"season: the first news screen is on day {news} (day of the year/year), not in July {year}")
            else:
                print(f"ok    season: the first news screen is on day {news[0]} of {year} (July)")
            years = fixture_years(log)
            if not years or min(years) < year:
                fail(f"season: the fixture store holds the years {sorted(years)}")
            else:
                print(f"ok    season: the fixture store holds the years {sorted(years)}")
            part = read_part(path)
            if not part_is(part, year, 1):
                fail(f"season: the season part is {part}, not one with {year} and changing rules on")
            else:
                print(f"ok    season: the save has the part with {year} and changing rules on")
            if tables:
                m = re.search(r"recomp: names of %d/\d\d: (\d+) competition records renamed" % year, log)
                comps = savfile.Sav(path).read_member("club_comp.dat")
                data = savfile.Sav(path).read_member(PART)
                if not m or int(m.group(1)) == 0 or f"recomp: club moves of {year}/" not in log:
                    fail(f"season: the log shows no names or no club moves of {year}")
                elif b"English First Division\x00" in comps or b"English First Division\x00" not in data:
                    fail("season: the save does not hold the season names with the database names in its part "
                         "(season_change_rename)")
                else:
                    print(f"ok    season: {m.group(1)} names and the club moves of {year}; the save holds the "
                          "season names and its part the database names")
                if year >= 2004:
                    wrong = english_abbreviations(path)
                    if wrong:
                        fail(f"season: the English divisions keep other abbreviations (Select Team): {wrong}")
                    else:
                        print("ok    season: the English divisions have the abbreviations PL, CH, L1 and L2")
            opath = results.get("season-off", (None,))[0]
            if opath and not part_is(read_part(opath), year, 0):
                fail(f"season-off: the season part is {read_part(opath)}, not one with {year} and changing rules off")
            elif opath:
                print(f"ok    season-off: the save has the part with {year} and changing rules off")
            again = results["season-again"][0]
            if again and open(again, "rb").read() != open(path, "rb").read():
                a, b = smoke_run.save_parts(path), smoke_run.save_parts(again)
                differ = sorted(k for k in set(a) | set(b) if a.get(k) != b.get(k))
                fail(f"season-again: the save differs from season in: {', '.join(differ) or 'the file header'}")
            elif again:
                print("ok    season-again: the save is the same as season")
            ipath, ilog, _ = results["season-italy"]
            if ipath:
                years = fixture_years(ilog)
                news = first_news(ilog)
                if not years or min(years) < year or not news or news[1] != year or month_of(*news) != 7:
                    fail(f"season-italy: first news {news}, fixture years {sorted(years)}")
                else:
                    print(f"ok    season-italy: with Italy, first news in July {year}, fixture years {sorted(years)}")
            # Second round, from the saves of the first: reloads, a part of a later version and a damaged part.
            load = smoke_run.without_intro_click(fixtures.LOAD_STEPS) + ["400000 quit"]
            holiday = smoke_run.STEPS[smoke_run.STEPS.index("wait first-news") + 1:]
            holiday = [s for s in holiday if "dump" not in s and "quit" not in s]
            reload_steps = load[:-1] + holiday + load[-1:]
            refuse_steps = [s for s in load if s != "wait first-news"][:-1] + ["30000 quit"]
            saves = {"reload": path}
            if results.get("season-off", (None,))[0]:
                saves["reload-off"] = results["season-off"][0]
            later = os.path.join(work, "later.sav")
            # Version 3: the fields of version 2 (slice 16), then 8 unknown bytes. A season game with names
            # (slice 15) has the names in the data in those fields, which the load needs (0x60efd0); without
            # them it is the version 2 fields with no news, no names and season point 0.
            data = savfile.Sav(path).read_member(PART)
            if struct.unpack_from("<I", data, 0)[0] == 2:
                replace_part(path, later, struct.pack("<2I", 3, len(data) + 8) + data[8:] + struct.pack("<2I", 7, 7))
            else:
                replace_part(path, later, struct.pack("<9I", 3, 36, year, 1, 0, 0, 0, 7, 7))
            saves["later"] = later
            damaged = os.path.join(work, "damaged.sav")
            replace_part(path, damaged, struct.pack("<4I", 1, 16, 1999, 1))
            if keep:
                shutil.copyfile(path, os.path.join(keep, f"season-{year}.sav"))
            jobs = {}
            with concurrent.futures.ThreadPoolExecutor(min(4, jobs_max)) as pool:
                for name, src in saves.items():
                    saved = os.path.join(work, name + ".sav")
                    if src != saved:
                        shutil.copyfile(src, saved)
                    jobs[name] = pool.submit(play, program, game, work, name, {}, reload_steps, 3, database=database,
                                             save=saved)
                dsave = os.path.join(work, "damaged-load.sav")
                shutil.copyfile(damaged, dsave)
                jobs["damaged"] = pool.submit(play, program, game, work, "damaged", {}, refuse_steps, 3,
                                              database=database, save=dsave)
                second = {k: v.result() for k, v in jobs.items()}
            want = {"reload": (year, 1), "reload-off": (year, 0), "later": (year, 1)}
            for name, part_want in want.items():
                if name not in second:
                    continue
                rpath, rlog, took = second[name]
                print(f"run   {name}: {'saved' if rpath else 'NO SAVE'} in {took:.0f} s", flush=True)
                if not rpath:
                    fail(f"{name}: no saved game; last lines: {rlog[-600:]!r}")
                    continue
                if f"the save is a game of {year}/{(year + 1) % 100:02d}" not in rlog:
                    fail(f"{name}: the runtime did not read the year from the part")
                if tables and name == "reload" and "recomp: names of" not in rlog:
                    fail("reload: the load did not give the names again")
                if tables and name == "reload" and year >= 2004 and english_abbreviations(rpath):
                    fail(f"reload: the new save has the abbreviations {english_abbreviations(rpath)}")
                part = read_part(rpath)
                if not part_is(part, *part_want):
                    fail(f"{name}: the season part of the new save is {part}, not one with {part_want}")
                else:
                    print(f"ok    {name}: the load read the part, and the new save has {part_want}")
            dpath, dlog, took = second["damaged"]
            print(f"run   damaged: {'saved' if dpath else 'no save'} in {took:.0f} s", flush=True)
            if "season box: Load Game" not in dlog or "season log:" in dlog or dpath:
                fail(f"damaged: the load was not refused with the box; last lines: {dlog[-600:]!r}")
            else:
                print("ok    damaged: the load is refused with a message box, and no game runs")
        if only in (None, "rename"):
            rpath, rlog, took = play(program, game, work, "rename", {"changing_rules": "off"}, steps, 7,
                                     {"RECOMP_SEASON_CHANGE_TEST": "rename"})
            print(f"run   rename: {'saved' if rpath else 'NO SAVE'} in {took:.0f} s", flush=True)
            data = savfile.Sav(rpath).read_member(PART) if rpath and read_part(rpath) else b""
            if "renamed to Test Cup" not in rlog:
                fail("rename: the test kind renamed no record")
            elif not rpath or data[:4] != struct.pack("<I", 2) or b"UEFA Cup\x00" not in data:
                fail(f"rename: the save has no version 2 part with the name in the data: {data[:80]!r}")
            else:
                print("ok    rename: the save has the season part, version 2, with the name in the data")
                load = smoke_run.without_intro_click(fixtures.LOAD_STEPS) + ["400000 quit"]
                holiday = smoke_run.STEPS[smoke_run.STEPS.index("wait first-news") + 1:]
                holiday = [s for s in holiday if "dump" not in s and "quit" not in s]
                saved = os.path.join(work, "rename-load.sav")
                shutil.copyfile(rpath, saved)
                lpath, llog, took = play(program, game, work, "rename-reload", {}, load[:-1] + holiday + load[-1:], 3,
                                         save=saved)
                print(f"run   rename-reload: {'saved' if lpath else 'NO SAVE'} in {took:.0f} s", flush=True)
                ldata = savfile.Sav(lpath).read_member(PART) if lpath and read_part(lpath) else b""
                if not lpath or "index..cpp" in llog or b"UEFA Cup\x00" not in ldata:
                    fail(f"rename-reload: the save did not load and keep the name; last lines: {llog[-600:]!r}")
                else:
                    print("ok    rename-reload: the save loads, and the new save keeps the name in the data")
    print("PASS" if not failed else "FAIL")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
