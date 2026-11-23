#!/usr/bin/env python3
"""Right ages and dates in every season (0.6.0 slice 11b, Q344).

Usage:
    python3 tests/seasons/season_dates_run.py <cm0102_recomp> <game folder> <folder of databases>
        [--only <season>,<season>] [--jobs n] [--extras] [--no-base]

<folder of databases> holds the Update Team databases, one folder each (test-fixtures/databases). The test
finds each database by its content (databases-signatures.tsv) and copies it into Databases of a new user
folder under its neutral name.

For each season database (and the 3.9.68 data on the 2002/03 tile) a new game starts in its season
(RECOMP_START_SEASON) and plays to the first news screen. The test log (RECOMP_DATES_LOG, season_dates.c)
gives the data in memory at the first human turn. The checks:
    people     at least five well-known people of that database have their real year of birth, and the age
               that the game keeps (+0x18) is the real age on the day of the first turn.
    champions  the last row of the English Premier Division history is the season before the start, with the
               real champions (for example 2019 Manchester City in a 2019/20 game).
    nations    the last rows of the FIFA World Cup and the European Football Championship are the last finals
               before the start, in their real years with their real winners (0.6.2 slice 7, open items 20 and
               39: the nation rows of many databases need another shift than the club rows; October 2023 kept
               France 2018 at 2020, October 2014 Germany 2014 at 2013).
    last rows  the databases that keep their last season one year early in some competitions hold it in its own year
               at the start (open items 59 and 83, LAST_ROWS): Autumn 2013, the Croatian, Greek and Northern Irish competitions of LAST_ROWS
               hold 2011/12 at 2012 and 2012/13 at 2013, once each (the test log of the rows at the creation).
    fields     the stats line of the log: 95 % of the contract ends and 80 % of the national job dates from 2
               years before the start to 6 years after, the latest joined date in the start year or the year
               before and a quarter of them there (not where the joined dates are real), the latest career year
               from 2 years before the start to the start year.
               Each check fails when the shift of its field is left out.
Also:
    second     the Choose Season screen: a start in 2019/20 (October 2019), Back from Select League(s), the
               2010/11 tile, a start again: the game is a 2010/11 game with the September 2010 ODB data and
               passes the checks above.
    base       October 2019 in a game that starts in 2001/02: nothing moves (the database plays as in 0.5.0).
    other      October 2019 in a game that starts in 2020/21, a season after its own (open item 169): the last
               FIFA World Cup and Euro rows are the last finals before the start in their real years (2018
               France, 2016 Portugal). The whole database moves by 1 year, but the nation rows move by whole
               4-year cycles only, so no final lands in a year without one.
    off        October 2019 in 2019/20 with RECOMP_SEASON_CODE=off: nothing moves.
    again      the 2010/11 game twice: the saves are byte for byte the same.
With --extras the extra databases of each season run too (people and champions of their data season).
At most --jobs (3) games run at the same time.
"""
import concurrent.futures
import csv
import datetime
import os
import re
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools", "seasons"))
import measure_date_shifts as mds  # noqa: E402
import season_core_run as scr  # noqa: E402

# Well-known people with their real dates of birth (day, month, year). Names in plain letters only.
PEOPLE = {
    "Alex Ferguson": (31, 12, 1941), "Ryan Giggs": (29, 11, 1973), "Thierry Henry": (17, 8, 1977),
    "Frank Lampard": (20, 6, 1978), "Gianluigi Buffon": (28, 1, 1978), "Steven Gerrard": (30, 5, 1980),
    "John Terry": (7, 12, 1980), "Iker Casillas": (20, 5, 1981), "Wayne Rooney": (24, 10, 1985),
    "Sergio Ramos": (30, 3, 1986), "Lionel Messi": (24, 6, 1987), "Gareth Bale": (16, 7, 1989),
    "Mohamed Salah": (15, 6, 1992), "Harry Kane": (28, 7, 1993), "Erling Haaland": (21, 7, 2000),
    "Bukayo Saka": (5, 9, 2001), "Jude Bellingham": (29, 6, 2003),
}
# English champions by the year the season ends.
CHAMPIONS = {
    2002: "Arsenal", 2003: "Manchester United", 2004: "Arsenal", 2005: "Chelsea", 2006: "Chelsea",
    2007: "Manchester United", 2008: "Manchester United", 2009: "Manchester United", 2010: "Chelsea",
    2011: "Manchester United", 2012: "Manchester City", 2013: "Manchester United", 2014: "Manchester City",
    2015: "Chelsea", 2016: "Leicester City", 2017: "Chelsea", 2018: "Manchester City", 2019: "Manchester City",
    2020: "Liverpool", 2021: "Manchester City", 2022: "Manchester City", 2023: "Manchester City",
    2024: "Manchester City", 2025: "Liverpool",
}
# Referees, logged for the report only: the referees that a database added after 2001 have early years that
# differ per referee (no rule, docs/research/2026-10-17-season-date-shifts.md), so no check.
REFEREES = {"Michael Oliver": (20, 2, 1985), "Anthony Taylor": (20, 10, 1978), "Howard Webb": (14, 7, 1971)}
COMP = "English Premier Division"
# The finals by label year: (winner, (year, month, day) of the final). Euro 2020 was played in 2021; the data
# labels it 2020.
FINALS = {
    "FIFA World Cup": {1998: ("France", (1998, 7, 12)), 2002: ("Brazil", (2002, 6, 30)),
                       2006: ("Italy", (2006, 7, 9)), 2010: ("Spain", (2010, 7, 11)),
                       2014: ("Germany", (2014, 7, 13)), 2018: ("France", (2018, 7, 15)),
                       2022: ("Argentina", (2022, 12, 18))},
    "European Football Championship": {2000: ("France", (2000, 7, 2)), 2004: ("Greece", (2004, 7, 4)),
                                       2008: ("Spain", (2008, 6, 29)), 2012: ("Spain", (2012, 7, 1)),
                                       2016: ("Portugal", (2016, 7, 10)), 2020: ("Italy", (2021, 7, 11)),
                                       2024: ("Spain", (2024, 7, 14))},
}
# A final in July of the start year is history: the game plays no finals in the July of its start
# (tools/seasons/history_fill.py, GAP_FINALS).
START_END = (7, 31)


# Open items 59 and 83: (name pattern, nation) of a competition and its expected (label, winner, runner-up) rows, by
# database in its own season. Sources: the final tables of the 2011/12 and 2012/13 seasons (en.wikipedia.org,
# 2012-13 Prva HNL and 2012-13 Super League Greece; 2011/12 Dinamo Zagreb / Hajduk Split and Olympiacos /
# Panathinaikos; the Northern Irish rows: 2012-13 IFA Premiership, IFA Championship and Irish League Cup).
LAST_ROWS = {
    "Autumn 2013": [
        ("Croatian First Division",
         [(2012, "GNK Dinamo Zagreb", "HNK Hajduk Split"), (2013, "GNK Dinamo Zagreb", "NK Lokomotiva Zagreb")]),
        ("Greek National A Division",
         [(2012, "Olympiacos Piraeus", "Panathinaikos AO"), (2013, "Olympiacos Piraeus", "PAOK Salonika")]),
        ("Northern Irish League Premier Division", [(2012, "Linfield", "Portadown"), (2013, "Cliftonville", "Crusaders")]),
        ("Northern Irish League First Division", [(2012, "Ballinamallard United", "Newry City"), (2013, "Ards", "")]),
        ("Northern Irish League Cup", [(2012, "Crusaders", "Coleraine"), (2013, "Cliftonville", "Crusaders")]),
        # open item 176: the database's own 2012 row stays, 2012/13 moves to 2013; the Trophee des Champions 2011
        # was Marseille against Lille, 2012 Lyon against Montpellier
        ("English Southern League Premier Division", [(2012, "Woking", "Dartford"),
                                                      (2013, "Leamington", "Gosport Borough")]),
        ("French Champions Trophy", [(2010, "Olympique de Marseille", "Paris Saint-Germain"),
                                     (2011, "Olympique de Marseille", "Lille Olympique SC"),
                                     (2012, "Olympique Lyonnais", "Montpellier Hérault SC")]),
    ],
}


def last_rows_env(label):
    """The test log of the rows at the creation (RECOMP_HISTORY_LOG, season_history.c) for the run of LAST_ROWS."""
    return {"RECOMP_HISTORY_LOG": ";".join(c for c, _ in LAST_ROWS[label])} if label in LAST_ROWS else {}


def check_last_rows(label, log):
    """None, or the faults of LAST_ROWS for the run of database label: the rows at the creation, before the game
    keeps only the rows of the selected nations (the runs select England)."""
    want = LAST_ROWS.get(label)
    if not want:
        return None
    bad = []
    for comp, expect in want:
        m = re.search(r"recomp: history log create: " + re.escape(comp) + r":(.*)", log)
        if not m:
            bad.append(f"{comp}: no log of the rows at the creation")
            continue
        rows = [tuple(x.strip().split(" ", 1)) for x in m.group(1).split(";") if x.strip()]
        for year, first, second in expect:
            got = [r[1] for r in rows if r[0] == str(year)]
            if got != [f"{first} / {second}".strip()]:
                bad.append(f"{comp} {year}: {got}, want [{first} / {second}]")
    return bad


def last_final(comp, year):
    """(label, winner) of the last final of comp before a game that starts in year."""
    done = [(y, w) for y, (w, d) in FINALS[comp].items() if d <= (year,) + START_END]
    return max(done) if done else None
MIN_PEOPLE = 5


def databases(folder):
    """{neutral name: (signature row, folder)} of the known databases in the folder."""
    sigs = mds.signatures()
    out = {}
    for sub in sorted(os.listdir(folder)):
        p = os.path.join(folder, sub)
        if os.path.isdir(p):
            r = mds.identify(p, sigs)
            if r and r["data_season"] != "none":
                out[r["database"]] = (r, p)
    return out


def season_rows():
    with open(os.path.join(mds.RULES, "databases-signatures.tsv"), newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def neutral_copy(work, name, src):
    """A folder named by the neutral name, holding the files of src (the name of src may hold a person's)."""
    dest = os.path.join(work, "src", name)
    if not os.path.isdir(dest):
        scr.cdr.copy_data(src, dest)
    return dest


def play(program, game, work, label, year, db_folder, env=None, steps=None, season=None, more_folders=()):
    """As season_core_run.play, with the log read as Latin-1: the game's own log lines may hold the letters of
    code page 1252 (club names). year None: no RECOMP_START_SEASON (the season comes from the settings file,
    season). more_folders: more databases in Databases (for the Choose Season screen)."""
    env = dict(env or {}, RECOMP_DATES_LOG=";".join(list(PEOPLE) + ["ref:" + r for r in REFEREES] + ["comp:" + COMP] + ["comp:" + c for c in FINALS]))
    if year:
        env["RECOMP_START_SEASON"] = str(year)
    steps = [s + " 600000" if s in ("wait typing", "wait first-news") else s for s in (steps or scr.new_game_steps())]
    run = os.path.join(work, label)
    user, home, shots = (os.path.join(run, d) for d in ("user", "home", "shots"))
    for d in (user, home, shots):
        os.makedirs(d)
    settings = "display = classic\n"
    if not db_folder:
        settings += "database = 3.9.68\n"
    for f in more_folders:
        scr.cdr.copy_data(f, os.path.join(user, "Databases", os.path.basename(f)))
    if db_folder:
        scr.cdr.copy_data(db_folder, os.path.join(user, "Databases", os.path.basename(db_folder)))
        settings += f"database = folder:{os.path.basename(db_folder)}\n"
    if season:
        settings += f"season = {season}/{(season + 1) % 100:02d}\n"
    with open(os.path.join(user, "settings.txt"), "w") as f:
        f.write(settings)
    full = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
                RECOMP_USER_DIR=user, RECOMP_FIXED_CLOCK="1", RECOMP_SAVE_AFTER_DAYS="0", RECOMP_SCREEN_LOG="1",
                RECOMP_SCRIPT="; ".join(scr.smoke_run.scaled_steps("classic", shots, steps)), **env)
    start = time.time()
    try:
        r = subprocess.run([program, "--game", game, "-seed", "1"], env=full, capture_output=True, timeout=1500)
    except subprocess.TimeoutExpired as e:
        return None, (e.stderr or b"").decode("latin-1"), time.time() - start
    log = r.stderr.decode("latin-1")
    path = os.path.join(user, scr.smoke_run.SAVE_NAME)
    return (path if r.returncode == 0 and os.path.exists(path) else None), log, time.time() - start


# Choose Season screen positions in classic mode (tests/seasons/season_screen_run.py).
SETUP_SEASON, TILE_2010, CHOOSE, BACK = "click 277 109", "click 132 182", "click 675 573", "click 358 573"


def second_steps():
    """Start New Game in the season of the settings file (2019/20), Back from Select League(s), the 2010/11
    tile of the Choose Season screen, then the new game steps."""
    base = scr.new_game_steps()
    i, start = base.index("wait setup") + 1, base.index("2500 click 277 177")
    return base[:i] + ["2500 click 277 177", "wait leagues", f"3000 {BACK}", "wait setup", f"2000 {SETUP_SEASON}",
                       "wait seasons", f"2000 {TILE_2010}", f"4000 {CHOOSE}", "wait setup 300000"] + base[start:]


def parse(log):
    m = re.search(r"dates log: date (-?\d+)/(\d+), start year (\d+), shifts (.*)", log)
    if not m:
        return None
    day, year = int(m.group(1)), int(m.group(2))
    out = {"date": (day, year), "shifts": m.group(4).strip(), "people": {}, "comp": None}
    careers = {}
    for p in re.finditer(r"dates log: career (.+?) born (\d+)\.(\d+): (\d+) to (\d+)", log):
        careers[(p.group(1), int(p.group(2)), int(p.group(3)))] = (int(p.group(4)), int(p.group(5)))   # the last
    for p in re.finditer(r"dates log: person (.+?): born (\d+)\.(\d+)\.(\d+), age (-?\d+), joined (\d+), "
                         r"contract (\d+), club (.*)", log):
        born = (int(p.group(2)), int(p.group(3)), int(p.group(4)))
        out["people"].setdefault(p.group(1), []).append(
            dict(born=born, age=int(p.group(5)), joined=int(p.group(6)), contract=int(p.group(7)),
                 club=p.group(8).strip(), career=careers.get((p.group(1), born[0], born[1]), (0, 0))))
    out["officials"] = {}
    for p in re.finditer(r"dates log: official (.+?): born (\d+)\.(\d+)\.(\d+)", log):
        out["officials"].setdefault(p.group(1), []).append((int(p.group(2)), int(p.group(3)), int(p.group(4))))
    out["stats"] = None
    for st in re.finditer(r"dates log: stats contracts (\d+) of (\d+) in \d+ to \d+; national dates (\d+) of (\d+); "
                          r"joined max (\d+), (\d+) in \d+ or \d+, (\d+) earlier, (\d+) later; career max (\d+)", log):
        out["stats"] = [int(x) for x in st.groups()]          # the last: after a second shift at the creation
    c = re.search(r"dates log: competition " + re.escape(COMP) + r": (\d+) (.*?); (\d+) ", log)
    if c:
        out["comp"] = (int(c.group(1)), c.group(2))
    out["finals"] = {}
    for comp in FINALS:
        c = re.search(r"dates log: competition " + re.escape(comp) + r": (\d+) (.*?); (\d+) ", log)
        if c:
            out["finals"][comp] = (int(c.group(1)), c.group(2))
    return out


# Databases whose joined dates keep their real years (measured, databases-date-shifts.tsv): no joined check.
JOINED_REAL = ("October 2003", "September 2005", "3.9.68 in 2002/03")


def check_fields(label, res, year):
    """The fields beside the dates of birth: contract ends, national job dates, joined dates and careers. Each
    check fails when the shift of its field is left out. Returns the texts of the failures."""
    bad, st = [], res["stats"]
    if not st:
        return ["no stats line"]
    c_in, c_set, n_in, n_set, j_max, j_now, j_early, j_late, career_max = st
    if c_set and c_in < 0.95 * c_set:
        bad.append(f"contracts: only {c_in} of {c_set} end from {year - 2} to {year + 6}")
    if n_set >= 20 and n_in < 0.8 * n_set:
        bad.append(f"national job dates: only {n_in} of {n_set} from {year - 2} to {year + 6}")
    if label not in JOINED_REAL and (j_max not in (year - 1, year) or j_now < (j_now + j_early + j_late) / 4):
        bad.append(f"joined: latest {j_max}, {j_now} in {year - 1} or {year}, {j_early} earlier, {j_late} later")
    if not year - 2 <= career_max <= year:
        bad.append(f"careers: the latest career year is {career_max}, not {year - 2} to {year}")
    return bad


def game_date(day, year):
    """The game date: the day of the year is 0 on 1 January (include/date.h, Date.cpp)."""
    return datetime.date(year, 1, 1) + datetime.timedelta(days=day)


def age_on(born, when):
    d, m, y = born
    return when.year - y - ((when.month, when.day) < (m, d))


def check_people(res):
    """(texts of the people that are right, texts of those that are wrong)."""
    when = game_date(*res["date"])
    good, bad = [], []
    for name, (d, m, y) in PEOPLE.items():
        rows = [r for r in res["people"].get(name, []) if r["born"][:2] == (d, m)]
        if not rows:
            continue
        r = rows[0]
        want_age = age_on((d, m, y), when)
        text = (f"{name} born {r['born'][2]} age {r['age']} ({r['club']}, joined {r['joined']}, contract "
                f"{r['contract']}, career {r['career'][0]}-{r['career'][1]})")
        if r["born"][2] == y and r["age"] == want_age:
            good.append(text)
        else:
            bad.append(f"{text}: real {y}, age {want_age}")
    return good, bad


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2
    program, game, dbs_dir = (os.path.abspath(a) for a in argv[1:4])
    jobs_n = int(argv[argv.index("--jobs") + 1]) if "--jobs" in argv else 3
    only = argv[argv.index("--only") + 1].split(",") if "--only" in argv else None
    dbs = databases(dbs_dir)
    runs = []          # (label, year, database name or None, env)
    for r in season_rows():
        if r["role"] not in ("season database", "extra version") or (r["role"] == "extra version" and "--extras" not in argv):
            continue
        year = int(r["data_season"][:4])
        if only and str(year) not in only and r["database"] not in only:
            continue
        if r["database"] not in dbs:
            print(f"skip  {r['database']}: not in {dbs_dir}")
            continue
        runs.append((r["database"], year, r["database"], None))
    if not only or "2002" in only:
        runs.append(("3.9.68 in 2002/03", 2002, None, None))
    if "--no-base" not in argv and "October 2019" in dbs:
        runs.append(("base", 2001, "October 2019", None))
        runs.append(("off", 2019, "October 2019", {"RECOMP_SEASON_CODE": "off"}))
    if "October 2019" in dbs and ("--no-base" not in argv or (only and "other" in only)):
        # open item 169: a database in a later season than its own (October 2019 in 2020/21, 1 year on)
        runs.append(("other", 2020, "October 2019", None))
    if "--no-base" not in argv and "September 2010 ODB" in dbs and "October 2019" in dbs:
        # Choose Season: a start in 2019/20, Back, the 2010/11 tile, a start again (Q344, review of slice 11b).
        runs.append(("second", 2010, "October 2019", {"_second": "September 2010 ODB"}))
    if "--no-base" not in argv and "September 2010 ODB" in dbs:
        runs.append(("again", 2010, "September 2010 ODB", None))
        if not any(x[0] == "September 2010 ODB" for x in runs):
            runs.append(("September 2010 ODB", 2010, "September 2010 ODB", None))
    failed = []

    def fail(text):
        failed.append(text)
        print("FAIL  " + text, flush=True)

    with tempfile.TemporaryDirectory(prefix="cm0102-dates-") as work:
        names = {x[2] for x in runs if x[2]} | {x[3]["_second"] for x in runs if x[3] and "_second" in x[3]}
        folders = {n: neutral_copy(work, n, dbs[n][1]) for n in names}
        with concurrent.futures.ThreadPoolExecutor(jobs_n) as pool:
            fut = {}
            for label, year, name, env in runs:
                tag = re.sub(r"[^A-Za-z0-9]", "_", label)
                if env and "_second" in env:
                    fut[label] = pool.submit(play, program, game, work, tag, None, folders[name], None,
                                             second_steps(), 2019, [folders[env["_second"]]])
                else:
                    fut[label] = pool.submit(play, program, game, work, tag, year, folders.get(name),
                                             dict(env or {}, **last_rows_env(label)) or None)
            results = {k: v.result() for k, v in fut.items()}
        for label, year, name, env in runs:
            path, log, took = results[label]
            res = parse(log)
            moved = re.search(r"recomp: season dates: .*", log)
            if moved:
                print(f"info  {label}: {moved.group(0)[len('recomp: season dates: '):]}")
            if not res:
                fail(f"{label}: no dates log (save {'made' if path else 'missing'}, {took:.0f} s); "
                     f"last lines: {log[-400:]!r}")
                continue
            if label in ("base", "off"):
                if res["shifts"] != "none":
                    fail(f"{label}: the shifts {res['shifts']} applied")
                else:
                    print(f"ok    {label}: nothing moves ({res['shifts']}, first turn {res['date']})")
                continue
            if label == "again":
                other = results.get("September 2010 ODB", (None,))[0]
                if path and other and open(path, "rb").read() == open(other, "rb").read():
                    print("ok    again: the 2010/11 game gives the same save twice")
                else:
                    fail("again: the two 2010/11 saves differ or are missing")
                continue
            if label == "other":
                # The finals keep their real years (open item 169): the nation rows move by whole 4-year cycles,
                # so the last rows are the last finals before the start, as in a game of the data's own season.
                for comp in FINALS:
                    want, have = last_final(comp, year), res["finals"].get(comp)
                    if have == want:
                        print(f"ok    other: last {comp} row {have[0]} {have[1]}")
                    else:
                        fail(f"other: last {comp} row {have}, the last final before the start is {want}")
                continue
            if res["date"][1] != year:
                fail(f"{label}: the game starts in {res['date'][1]}, not {year}")
                continue
            wrong = check_fields(label, res, year)
            if wrong:
                fail(f"{label}: " + "; ".join(wrong))
            else:
                print(f"ok    {label}: contracts, national job dates, joined dates and careers fit {year} "
                      f"(stats {res['stats']})")
            good, bad = check_people(res)
            if len(good) >= MIN_PEOPLE and not bad:
                print(f"ok    {label}: {len(good)} people right on {game_date(*res['date'])}: " + "; ".join(good))
            else:
                fail(f"{label}: {len(good)} people right, wrong: {'; '.join(bad) or 'none'} (right: {'; '.join(good)})")
            want = CHAMPIONS.get(year)
            if res["comp"] and res["comp"][0] == year and (res["comp"][1] == want or label.startswith("3.9.68")):
                print(f"ok    {label}: last {COMP} row {res['comp'][0]} {res['comp'][1]}")
            elif res["comp"] and res["comp"][0] == year:
                fail(f"{label}: last {COMP} row {res['comp']} (real champions of {year}: {want})")
            else:
                fail(f"{label}: last {COMP} row {res['comp']}, not the year {year}")
            wrong = check_last_rows(label, log)
            if wrong:
                fail(f"{label}: last rows " + "; ".join(wrong))
            elif wrong is not None:
                print(f"ok    {label}: the last rows of LAST_ROWS are in their own years")
            for comp in FINALS:
                want, have = last_final(comp, year), res["finals"].get(comp)
                # A final of the start year after July (the World Cup 2022 in December, April 2023 in 2022/23) may
                # stand as the data gives it: the game plays that edition, and its row replaces the data's
                # (season_history.c, 0x4afce0).
                later = FINALS[comp].get(year)
                if have == want or (later and have == (year, later[0])):
                    print(f"ok    {label}: last {comp} row {have[0]} {have[1]}")
                else:
                    fail(f"{label}: last {comp} row {have}, the last final before the start is {want}")
    print(f"{len(failed)} failed" if failed else "all passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
