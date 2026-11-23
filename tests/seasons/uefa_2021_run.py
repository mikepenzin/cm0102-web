#!/usr/bin/env python3
"""UEFA 2021/22 to 2023/24 check (0.6.0 slice 24): the Europa League of 32 clubs with knockout round play-offs,
no away goals rule, and no UEFA places for a suspended nation (Russia from 2022/23).

Usage:
    python3 tests/seasons/uefa_2021_run.py <cm0102_recomp> <game folder> [--only <name>,...] [--jobs n]
        [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py with England. The runs:
    2021    October 2021, changing rules off, the season 2021/22
    2022    October 2022, changing rules off, the season 2022/23: Russia is suspended
    2023    October 2023, the season 2023/24: Russia is suspended, the Super Cup goes to penalties after 90
            minutes
    change  November 2020, changing rules on, 2020/21 to the end of 2022/23: the news sentences of the change; the
            2021/22 Conference League winner gets its Europa League group place in 2022/23 (rule Y), or the log
            says it has a place already
The checks of each run: the jump ends with code 0 and shows no message box of the game; the log of
src/recomp_rt/win/season_uefa*.c shows the access list places, 8 Europa League groups of 4 drawn, the knockout
round play-offs drawn, and the no away goals rule in the two-legged rounds of the Champions League and the
Europa League (qualifying rounds and knockout stages, 4 round tables); the uefa line of the report at the end of
each season of the rules: Champions League 9 stages, 32 clubs in groups, a knockout of 16 in 4 rounds; Europa
League 9 stages, 32 clubs in 8 groups, a knockout stage of 24 clubs (16 in the play-offs and the 8 group winners)
in 5 rounds, each with a winner; the label of round code 30 of the Europa League is the real name of 2021/22.
2022 and 2023: the log line of the seeding counts 0 clubs of Russia with a UEFA place. No Champions League or
Europa League tie of a season from 2021/22 is decided on away goals (the log line of the wrap of 0x51f2f0), and
in the change run at least one of 2020/21 is.
Part 24b, the Conference League (a new competition record and a second object of the Uefa_Cup class): the object
is made for the first season of the rules, it has entrants and qualifying rounds, takes the losers of the
Champions League and the Europa League, draws 8 groups of 4 with 32 clubs and its knockout round play-offs with the
8 Europa League thirds, and its uefa line at the end of each season is as the Europa League's (9 stages, 32 in
groups, 24 in the knockout, 5 rounds, a winner). Its round code 30 has the label of the Europa League's. No
Conference League tie of the season is decided on away goals either. The change run gives the news sentence of
its first season.
    load    October 2021: the game saves 30 days after the start (inside the qualifying rounds), 120 days after
            the start (in the group stage) and 225 days after it (late February, between the legs of the knockout
            round play-offs); a new program loads each save and plays to the end of the season;
            the three cups play to a winner
    repeat  October 2021, twice: the saves 120 days after the start (in the group stages, with the Conference League)
            are equal (a moved season repeats)
A run takes about 10 to 40 minutes on a loaded machine. --jobs (default 1).
"""
import concurrent.futures
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rules_report_check as rrc  # noqa: E402
import uefa_formats_run as ufr  # noqa: E402

DB = "/Users/max/Projects/cm0102/test-fixtures/databases"
CL = {"stages": 9, "groups": 32, "groups2": 0, "ko_clubs": 16, "ko_rounds": 4}
EL = {"stages": 9, "groups": 32, "groups2": 0, "ko_clubs": 24, "ko_rounds": 5}
EL_2020 = {"stages": 13, "groups": 48, "groups2": 0, "ko_clubs": 32, "ko_rounds": 5}
ECL = {"stages": 9, "groups": 32, "groups2": 0, "ko_clubs": 24, "ko_rounds": 5}
ECL_FILE = "new_conference_league"
KO_PO_NAME = "Knockout Round Play-offs"
NEWS = ["The Europa League group winners now go to the round of 16, and the runners-up meet the Champions League "
        "group thirds in knockout round play-offs.",
        "Away goals no longer decide UEFA ties: a tie level on aggregate goes to extra time and penalties.",
        "The UEFA Europa Conference League is played for the first time."]


def ecl_checks(name, log, lines, season):
    """Part 24b: the Conference League of the season in the log and in the uefa lines of the report."""
    key = f"{season}/{(season + 1) % 100:02d}"
    fails = []
    for pattern in (r"new competition new_conference_league made for \d+",
                    rf"Conference League {key}: \d+ entrants, 0 direct, \d+ qualifying rounds",
                    rf"Conference League {key}: \d+ qualifying rounds, 8 groups of 4",
                    rf"Conference League {key}: 8 groups of 4 drawn",
                    r"Conference League knockout round play-offs drawn",
                    r"losers of Champions League round \d+ \(path \d\) into Conference League slots",
                    r"losers of Europa League round \d+ into Conference League slots"):
        if not re.search(pattern, log):
            fails.append(f"{name}: no log line {pattern!r}")
    if lines is not None:
        fails += ufr.expect(lines, season, ECL_FILE, ECL, name)
    return fails


def season_checks(name, log, report, season):
    key = f"{season}/{(season + 1) % 100:02d}"
    fails = []
    for pattern in (rf"access list {key}: \d+ Champions League and \d+ Europa League places",
                    rf"Champions League {key}: \d+ entrants",
                    rf"Europa League {key}: \d+ qualifying rounds, 8 groups of 4",
                    rf"UEFA Cup {key}: 8 groups of 4 drawn", r"Europa League knockout round play-offs drawn",
                    r"Champions League round of 16 drawn"):
        if not re.search(pattern, log):
            fails.append(f"{name}: no log line {pattern!r}")
    # Each round table by its first code (a knockout can log twice: cl_rank_teams asks for its table too): the
    # Champions League qualifying rounds (250), its knockout (20), the Europa League qualifying rounds (270) and
    # its knockout (30).
    tables = {int(c): int(n) for n, c in
              re.findall(rf"no away goals rule in (\d+) of \d+ rounds, first code (\d+) \({key}\)", log)}
    for code in (250, 20, 270, 30):
        if tables.get(code, 0) <= 0:
            fails.append(f"{name}: no round table of first code {code} with the no away goals rule in {key}")
    # Real ties: no Champions League or Europa League tie of the season is decided on away goals (the log line of
    # the wrap of 0x51f2f0, season_uefa.c). The log of the season starts at its access list line.
    at = log.find(f"access list {key}:")
    if at >= 0 and "decided on away goals" in log[at:]:
        fails.append(f"{name}: a UEFA tie decided on away goals in {key}")
    # "UEFA Cup groups: n clubs, not 32" is a day that the group draw waits for its clubs (slice 23 on main): the
    # drawn lines above show that it came.
    for bad in ("not even", "gives no layout", "winners, not 32", "no Champions League third", "runners-up\n",
                "no Europa League third", "no format for", "the Europa League has no 8 groups"):
        if bad in log:
            fails.append(f"{name}: log line with {bad!r}")
    if os.path.exists(report):
        lines = ufr.uefa_lines(report)
        fails += ufr.expect(lines, season, "european_cup", CL, name)
        fails += ufr.expect(lines, season, "uefa_cup", EL, name)
        fails += ufr.expect_label(ufr.round_labels(report), season, "uefa_cup", 30, KO_PO_NAME, name)
        fails += ufr.expect_label(ufr.round_labels(report), season, ECL_FILE, 30, KO_PO_NAME, name)
    else:
        fails.append(f"{name}: no report")
    return fails


def run_season(name, season, database, suspended_check, super_cup_penalties=False):
    def run(program, game, work):
        code, report = rrc.run(program, game, season, season, os.path.join(DB, database), ["England"], [], work, 7200)
        fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
        found, log = ufr.boxes(work)
        fails += [f"{name}: message box {b}" for b in found]
        fails += season_checks(name, log, report, season)
        fails += ecl_checks(name, log, ufr.uefa_lines(report) if os.path.exists(report) else None, season)
        m = re.search(r"suspended Russia: (\d+) clubs with a place", log)
        if suspended_check and (not m or m.group(1) != "0"):
            fails.append(f"{name}: Russia: {m.group(0) if m else 'no log line of the suspension'}")
        if not suspended_check and m:
            fails.append(f"{name}: Russia is suspended in {season}")
        sc = f"Super Cup {season}: penalties after 90 minutes" in log
        if sc != super_cup_penalties:
            fails.append(f"{name}: the Super Cup of {season} {'has' if super_cup_penalties else 'has no'} extra time")
        return fails
    return run


def run_change(program, game, work):
    code, report = rrc.run(program, game, 2022, 2020, os.path.join(DB, "November 2020"), ["England"], [], work, 14400,
                           {"changing_rules": "on"}, {"RECOMP_SEASON_CHANGE_LOG": "1"})
    fails = [] if code == 0 else [f"change: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    fails += [f"change: message box {b}" for b in found]
    if os.path.exists(report):
        lines = ufr.uefa_lines(report)
        fails += ufr.expect(lines, 2020, "uefa_cup", EL_2020, "change")
        fails += ufr.expect_label(ufr.round_labels(report), 2020, "uefa_cup", 30, KO_PO_NAME, "change", absent=True)
    if re.search(r"no away goals rule in \d+ of \d+ rounds, first code \d+ \(2020/21\)", log):
        fails.append("change: the no away goals rule in 2020/21")
    before = log[:log.find("access list 2021/22:")] if "access list 2021/22:" in log else log
    if "decided on away goals" not in before:
        fails.append("change: no UEFA tie of 2020/21 decided on away goals (the rule still holds in 2020/21)")
    fails += season_checks("change", log, report, 2021)
    fails += ecl_checks("change", log, ufr.uefa_lines(report) if os.path.exists(report) else None, 2021)
    if re.search(r"new competition new_conference_league made for 2020", log):
        fails.append("change: the Conference League made for 2020/21")
    # The Conference League record keeps its own name at each season change (the names table of slice 15 counts
    # it as its own file, not as a second uefa_cup object).
    if os.path.exists(report):
        _, comps = rrc.read_report(report)
        names = sorted({(c["season"], c["name"]) for c in comps if c["file"] == ECL_FILE})
        if not names or any(n != "UEFA Europa Conference League" for _, n in names):
            fails.append(f"change: the Conference League record's names {names}")
    # Rule Y: the 2021/22 winner (the uefa line of the report) is the holder that the 2022/23 seeding places.
    lines = ufr.uefa_lines(report) if os.path.exists(report) else {}
    winner = (lines.get((2021, "end", ECL_FILE)) or {}).get("winner", "-")
    m = re.search(r"Conference League holder (.+): (a Europa League place|has a place already)", log)
    if winner in ("", "-") or not m or m.group(1) != winner[:50]:
        fails.append(f"change: the 2022/23 seeding does not place the 2021/22 Conference League winner {winner!r} "
                     f"({m.group(0) if m else 'no holder line'})")
    else:
        print(f"change: Conference League holder {winner}: {m.group(2)}")
    for sentence in NEWS:
        if sentence not in log:
            fails.append(f"change: no news sentence {sentence!r}")
    return fails


def run_load(program, game, work):
    """Saves inside the qualifying rounds and in the group stage, each loaded by a new program that plays to the
    end of the season (tests/seasons/uefa_access_run.py run_load)."""
    import shutil
    import fixtures
    import smoke_run
    db = os.path.join(DB, "October 2021")
    fails = []
    for days in (30, 120, 225):               # qualifying rounds, groups, between the knockout play-off legs
        w = os.path.join(work, f"d{days}")
        code, _ = rrc.run(program, game, 2021, 2021, db, ["England"], [], w, 7200, None,
                          {"RECOMP_SAVE_AFTER_DAYS": str(days)})
        first = os.path.join(w, "user", smoke_run.SAVE_NAME)
        if code != 0 or not os.path.exists(first):
            fails.append(f"load {days}: the first run made no save (code {code}, {w})")
            continue
        if not re.search(r"new competition new_conference_league made for 2021", ufr.boxes(w)[1]):
            fails.append(f"load {days}: no Conference League in the first run")
        again = os.path.join(w, "again")
        user = os.path.join(again, "user")
        os.makedirs(user, exist_ok=True)
        shutil.copyfile(first, os.path.join(user, "uefa.sav"))
        shutil.copytree(os.path.join(w, "user", "Databases"), os.path.join(user, "Databases"))
        fixtures.write_settings(user, "classic", [("database", f"folder:{os.path.basename(db)}"), ("faster_days", "on")])
        report = os.path.join(again, "report.tsv")
        steps = fixtures.LOAD_STEPS[:-1] + ["3600000 quit"]    # no first-news wait, as uefa_access_run.py
        r, _ = fixtures.run(program, game, "classic", user, smoke_run.scaled_steps("classic", again, steps),
                            {"RECOMP_SEASON_JUMP": "2021", "RECOMP_RULES_REPORT": report}, 7200)
        with open(os.path.join(again, "stderr.txt"), "w") as f:
            f.write(r.stderr)
        if r.returncode != 0:
            fails.append(f"load {days}: the loaded game ended with code {r.returncode} ({again})")
        fails += [f"load {days}: message box {b}" for b in ufr.BOX.findall(r.stderr)]
        # The module logs the game year of the load (2022 for a save of February 2022); the object reads its own
        # year from its saved file.
        if not re.search(r"new competition new_conference_league made for 20(21|22) \(entry 3, record \d+\) from the save",
                         r.stderr):
            fails.append(f"load {days}: the Conference League was not made again from the save")
        if os.path.exists(report):
            lines = ufr.uefa_lines(report)
            fails += ufr.expect(lines, 2021, "european_cup", CL, f"load {days}")
            fails += ufr.expect(lines, 2021, "uefa_cup", EL, f"load {days}")
            fails += ufr.expect(lines, 2021, ECL_FILE, ECL, f"load {days}")
        else:
            fails.append(f"load {days}: no report ({again})")
    return fails


def run_repeat(program, game, work):
    """Two 2021/22 games with the same steps give equal saves 120 days after the start
    (tests/seasons/uefa_access_run.py run_repeat)."""
    import smoke_run
    saves = []
    for k in ("a", "b"):
        w = os.path.join(work, k)
        rrc.run(program, game, 2021, 2021, os.path.join(DB, "October 2021"), ["England"], [], w, 1800, None,
                {"RECOMP_SAVE_AFTER_DAYS": "120"})
        path = os.path.join(w, "user", smoke_run.SAVE_NAME)
        if not os.path.exists(path):
            return [f"repeat: run {k} made no save ({w})"]
        saves.append(open(path, "rb").read())
    return [] if saves[0] == saves[1] else ["repeat: the two 2021/22 saves differ"]


RUNS = {
    "2021": run_season("2021", 2021, "October 2021", False),
    "2022": run_season("2022", 2022, "October 2022", True),
    "2023": run_season("2023", 2023, "October 2023", True, True),
    "change": run_change,
    "load": run_load,
    "repeat": run_repeat,
}


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="uefa-2021-")
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
