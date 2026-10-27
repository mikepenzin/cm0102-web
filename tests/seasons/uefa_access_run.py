#!/usr/bin/env python3
"""UEFA 2009/10 to 2020/21 check (0.6.0 slice 23): the access list, the Champions League paths, the Europa
League and the end of the Intertoto Cup, in a game that starts in a season of the access list.

Usage:
    python3 tests/seasons/uefa_access_run.py <cm0102_recomp> <game folder> [--only <name>,...] [--jobs n]
        [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py with England (changing rules off: the start
season's rules). The runs:
    2010    September 2010 ODB, the season 2010/11 (the format of 2009/10 to 2017/18)
    2018    October 2018, the season 2018/19 (a preliminary round, paths from Q2, four group places for the top
            four associations: England's four clubs in the group stage)
    load2018  October 2018: the game saves 10 days after the start (23 July 2018, before the Champions League's
            second qualifying round: from row A18 a start season after 2001 plays the real qualifying days, Q2 on 25
            July and 1 August), a new program loads the save and plays to the end of the season (slice
            23 review 2: the draws after a load read the direct clubs of the list +0x14)
    load    September 2010 ODB: the game saves 30 days after the start (August 2010, inside the qualifying
            rounds), a new program loads the save and plays to 1 June 2011; both cups play their group stages and
            knockouts to a winner (the hand-over of Champions League losers after a load)
    2013    Autumn 2013: the Super Cup of 2013 at the row's stadium in Prague (the log line of
            season_uefa_super_cup.c, written when the game is made); the run ends after 600 s (the World Cup box
            of a moved season, world_cup.cpp 1438, slice 27, stops a 2013 start in December)
    repeat  September 2009, twice: the saves 60 days after the start (September 2009, after the qualifying
            rounds) are equal byte for byte; each run ends after 600 s (the same World Cup box in December 2009)
The checks of the season runs: the jump ends with code 0 and shows no message box of the game; the log of
src/recomp_rt/win/season_uefa_seeding.c and season_uefa_qual.c shows the places of the access list, the
entrants of both cups (Champions League 76 clubs, 79 from 2018/19), the Champions League ties in two paths
and the round codes (240, the preliminary round, from 2018/19); the uefa lines of the report at the end of the
season: Champions League 9 stages, 32 clubs in groups, a knockout of 16 and 4 rounds; Europa League 13 stages, 48
clubs in 12 groups, a knockout of 32 and 5 rounds, each with a winner; the round labels of Q370 (Play-off
Round, Third Qualifying Round of the Europa League, Preliminary Round and Preliminary Round Final from 2018/19);
and the Intertoto Cup plays no round.
A run takes about 4 to 40 minutes on a loaded machine. --jobs (default 1). Ctrl-C stops the runner and its
games.
"""
import concurrent.futures
import os
import re
import shutil
import signal
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rules_report_check as rrc  # noqa: E402
import uefa_formats_run as ufr  # noqa: E402

DB = "/Users/max/Projects/cm0102/test-fixtures/databases"
CL = {"stages": 9, "groups": 32, "groups2": 0, "ko_clubs": 16, "ko_rounds": 4}
EL = {"stages": 13, "groups": 48, "groups2": 0, "ko_clubs": 32, "ko_rounds": 5}


def run_season(name, season, database, cl_clubs, labels_want, england_groups=None, codes=None):
    def run(program, game, work):
        code, report = rrc.run(program, game, season, season, os.path.join(DB, database), ["England"], [], work, 7200)
        fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
        found, log = ufr.boxes(work)
        fails += [f"{name}: message box {b}" for b in found]
        key = f"{season}/{(season + 1) % 100:02d}"
        for pattern in (rf"access list {key}: {cl_clubs} Champions League and \d+ Europa League places",
                        rf"Champions League {key}: {cl_clubs} entrants",
                        rf"Europa League {key}: \d+ qualifying rounds, 12 groups of 4",
                        rf"UEFA Cup {key}: 12 groups of 4 drawn", r"UEFA Cup round of 32 drawn",
                        r"Champions League round of 16 drawn", r"Champions League ties of \d rounds in two paths"):
            if not re.search(pattern, log):
                fails.append(f"{name}: no log line {pattern!r}")
        if england_groups is not None and not re.search(
                rf"England {key}: \d+ Champions League places, {england_groups} in the group stage", log):
            fails.append(f"{name}: England has not {england_groups} clubs in the Champions League group stage")
        m = re.search(rf"Champions League {key}: \d+ qualifying rounds \(codes([ \d]+)\)", log)
        if codes and (not m or m.group(1).split() != codes.split()):
            fails.append(f"{name}: Champions League round codes {m.group(1) if m else None!r}, not {codes!r}")
        if re.search(r"Intertoto Cup \d+ with \d rounds", log):
            fails.append(f"{name}: the Intertoto Cup played rounds")
        for bad in ("not even", "gives no layout", "not 32", "no Champions League third"):
            if bad in log:
                fails.append(f"{name}: log line with {bad!r}")
        fails += group_draw_fails(name, log)
        if os.path.exists(report):
            lines = ufr.uefa_lines(report)
            fails += ufr.expect(lines, season, "european_cup", CL, name)
            fails += ufr.expect(lines, season, "uefa_cup", EL, name)
            labels = ufr.round_labels(report)
            for file, code, want in labels_want:
                fails += ufr.expect_label(labels, season, file, code, want, name)
        return fails
    return run


def load_run(season, database, days, name):
    """A save inside the qualifying rounds, loaded by a new program that plays to the end of the season. The
    group draws take the 16 direct clubs from the list +0x14 (draw_phase1_groups 0x585365), and the constructors
    skip build_entrants after a load (0x902366, 0x583870): the draws after the load show the list came back."""
    return lambda program, game, work: run_load(program, game, work, season, database, days, name)


def run_load(program, game, work, season=2010, database="September 2010 ODB", days=30, name="load"):
    """A save inside the qualifying rounds, loaded by a new program that plays to the end of the season."""
    import fixtures
    import smoke_run
    db = os.path.join(DB, database)
    code, _ = rrc.run(program, game, season, season, db, ["England"], [], work, 7200, None,
                      {"RECOMP_SAVE_AFTER_DAYS": str(days)})
    first = os.path.join(work, "user", smoke_run.SAVE_NAME)
    if code != 0 or not os.path.exists(first):
        return [f"{name}: the first run made no save (code {code}, {work})"]
    # Inside the qualifying rounds: the first run saved before the Champions League's second qualifying round
    # ended (its losers go to the Europa League with a log line, season_uefa_qual.c put_losers).
    if "losers of Champions League round 260" in ufr.boxes(work)[1]:
        return [f"{name}: the save after {days} days is past the second qualifying round ({work})"]
    again = os.path.join(work, "again")
    user = os.path.join(again, "user")
    os.makedirs(user, exist_ok=True)
    shutil.copyfile(first, os.path.join(user, "uefa.sav"))
    shutil.copytree(os.path.join(work, "user", "Databases"), os.path.join(user, "Databases"))
    fixtures.write_settings(user, "classic", [])
    with open(os.path.join(user, "settings.txt"), "a") as f:
        f.write(f"database = folder:{os.path.basename(db)}\nfaster_days = on\n")
    report = os.path.join(again, "report.tsv")
    # The loaded game goes on with its holiday at once and ends when the season jump ends: no wait for the first
    # news (its 120 s limit is shorter than the jump on a loaded machine).
    steps = fixtures.LOAD_STEPS[:-1] + ["3600000 quit"]
    r, _ = fixtures.run(program, game, "classic", user, smoke_run.scaled_steps("classic", again, steps),
                        {"RECOMP_SEASON_JUMP": str(season), "RECOMP_RULES_REPORT": report}, 7200)
    with open(os.path.join(again, "stderr.txt"), "w") as f:
        f.write(r.stderr)
    fails = [] if r.returncode == 0 else [f"{name}: the loaded game ended with code {r.returncode} ({again})"]
    fails += [f"{name}: message box {b}" for b in ufr.BOX.findall(r.stderr)]
    fails += group_draw_fails(name, r.stderr)
    if os.path.exists(report):
        lines = ufr.uefa_lines(report)
        fails += ufr.expect(lines, season, "european_cup", CL, name)
        fails += ufr.expect(lines, season, "uefa_cup", EL, name)
    else:
        fails.append(f"{name}: no report ({again})")
    return fails


def run_2013(program, game, work):
    """The Super Cup venue of 2013 (Prague): the row's stadium, else the national stadium of the Czech Republic.
    The line comes when the game is made."""
    rrc.run(program, game, 2013, 2013, os.path.join(DB, "Autumn 2013"), ["England"], [], work, 600)
    _, log = ufr.boxes(work)
    m = re.search(r"Super Cup 2013 at .*Prague.*", log)
    if m:
        print("2013 venue:", m.group(0))     # names the stadium used, also when it is the fallback
        return []
    m = re.search(r"Super Cup 2013: .*", log)
    return [f"2013: no Super Cup at the 2013 stadium ({m.group(0) if m else 'no Super Cup line'})"]


def run_repeat(program, game, work):
    """Two 2009/10 games with the same steps give equal saves 60 days after the start."""
    import smoke_run
    saves = []
    for k in ("a", "b"):
        w = os.path.join(work, k)
        rrc.run(program, game, 2009, 2009, os.path.join(DB, "September 2009"), ["England"], [], w, 600, None,
                {"RECOMP_SAVE_AFTER_DAYS": "60"})
        path = os.path.join(w, "user", smoke_run.SAVE_NAME)
        if not os.path.exists(path):
            return [f"repeat: run {k} made no save ({w})"]
        saves.append(open(path, "rb").read())
    return [] if saves[0] == saves[1] else ["repeat: the two 2009/10 saves differ"]


def group_draw_fails(name, log):
    """The Europa League group draw waits, a day at a time, while a qualifying round still plays ("42 clubs, not
    48"). It must end with the draw of 12 groups, and its waiting is told."""
    waits = log.count("clubs, not 48")
    if waits and not re.search(r"clubs, not 48.*\n(?:.*\n)*?.*UEFA Cup \d+/\d+: 12 groups of 4 drawn", log):
        return [f"{name}: the Europa League group draw did not get 48 clubs"]
    if waits:
        print(f"{name}: the Europa League group draw waited {waits} times for the last qualifying round")
    return []


LABELS_2009 = [("european_cup", 220, "Play-off Round"), ("uefa_cup", 220, "Play-off Round"),
               ("uefa_cup", 270, "Third Qualifying Round")]
LABELS_2018 = LABELS_2009 + [("european_cup", 240, "Preliminary Round"), ("european_cup", 230, "Preliminary Round Final"),
                             ("uefa_cup", 240, "Preliminary Round")]
RUNS = {
    "2010": run_season("2010", 2010, "September 2010 ODB", 76, LABELS_2009, None, "250 260 270 220"),
    "2018": run_season("2018", 2018, "October 2018", 79, LABELS_2018, 4, "240 230 250 260 270 220"),
    "load": run_load,
    "load2018": load_run(2018, "October 2018", 10, "load2018"),
    "2013": run_2013,
    "repeat": run_repeat,
}


def stop_all(signum, frame):
    """Ctrl-C: stop the runner and every game it started (they are in its process group)."""
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    signal.signal(signal.SIGTERM, signal.SIG_DFL)
    os.killpg(os.getpgid(0), signal.SIGTERM)


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    signal.signal(signal.SIGINT, stop_all)
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or __import__("tempfile").mkdtemp(prefix="uefa-access-")
    fails = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        futures = {name: ex.submit(RUNS[name], program, game, os.path.join(keep, name)) for name in only}
        for name, fut in futures.items():
            got = fut.result()
            print(f"{name}: {'ok' if not got else 'FAIL'} ({os.path.join(keep, name)})")
            fails += got
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
