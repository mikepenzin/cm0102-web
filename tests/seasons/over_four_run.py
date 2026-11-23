#!/usr/bin/env python3
"""Relegation play-offs over a play-off of four (issue 160; src/recomp_rt/win/season_playoffs.c, build_over; the list of
leagues and seasons is docs/research/2026-11-18-relegation-playoffs-over-four.md).

Usage:
    python3 tests/seasons/over_four_run.py <cm0102_recomp> <game folder> [--only <run>,...] [--keep <folder>]
        [--reuse] [--timeout <seconds>]
    python3 tests/seasons/over_four_run.py --list
    python3 tests/seasons/over_four_run.py --self-test

Each run starts a new game of one nation with changing rules on, plays on holiday with no match engine and saves after
the days of the run, past the season change after the play-off. The checks of each run:
    drawn    the lower division logs one draw of five clubs (the cross play-off line with "over four");
    final    the last tie logs its winner;
    moves    at the start of the next season (the club lines of the report) the winner plays in the upper division, and
             the upper club plays in the lower division when a lower club won, else it stays;
    count    both divisions hold their clubs of the start at that season;
    home     J2 only: the J1 club is at home in the last tie;
    box      no message box of the game, and the program saves and ends with code 0.
A run fails on a build without the play-off, because the draw line is missing.
"""
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(HERE)), "tools", "save"))
import rules_report_check as rrc  # noqa: E402
import soak_run  # noqa: E402
from sco_playoffs_run import cross_lines  # noqa: E402

# run: (start year, nation, lower file, upper file, days to play, what it shows)
RUNS = {
    "nor2012": (2012, "Norway", "nor_first", "nor_prm", 640, "1. divisjon 3rd to 6th, the winner v the Tippeligaen 14th"),
    "ire2019": (2019, "Republic of Ireland", "ire_first", "ire_prm", 640, "First Division 2nd to 5th, the winner v the 9th"),
    "jap2018": (2018, "Japan", "jap_j2", "jap_j1", 640, "J2 3rd to 6th, the winner v the J1 16th"),
    "fra2026": (2026, "France", "fra_third", "fra_second", 365, "Ligue 3 3rd to 6th, the winner v the Ligue 2 16th"),
}


def judge(lines, lower, upper, record, division, start_counts):
    """The failures of one run, from the cross lines, the division ids and the club counts of the start."""
    failed = []
    drawn = [x for x in lines if x[0] == lower and x[2] == "drawn" and "over four" in x[3]]
    final = [x for x in lines if x[0] == lower and x[2] == "final"]
    if not drawn:
        return [f"drawn: no play-off over four of {lower}"]
    if len(drawn[0][3]) < 7:
        failed.append(f"drawn: {lower} line has {len(drawn[0][3])} cells, want five clubs")
    if not final:
        return failed + [f"final: no last tie of {lower}"]
    winner, winner_file, upper_club, upper_file = final[0][3][:4]
    print(f"  {lower} {final[0][1]}: {winner} ({winner_file}) won, {upper_club} ({upper_file}) played")
    if upper_file != upper:
        failed.append(f"final: upper file {upper_file}, want {upper}")
    goes_to = upper if winner_file == upper else lower
    if division.get(winner) != record.get(upper):
        failed.append(f"moves: {winner} in division {division.get(winner)}, want {upper}")
    if division.get(upper_club) != record.get(goes_to):
        failed.append(f"moves: {upper_club} in division {division.get(upper_club)}, want {goes_to}")
    for file, want in start_counts.items():
        got = sum(1 for d in division.values() if d == record.get(file))
        if got != want:
            failed.append(f"count: {file} in the save {got}, want {want}")
    return failed


def check(name, work):
    year, _nation, lower, upper, _days, _what = RUNS[name]
    log = os.path.join(work, "stderr.txt")
    box = soak_run.first_box(log)
    failed = [f"message box: {box}"] if box else []
    report = os.path.join(work, "report.tsv")
    save = os.path.join(work, "user", "samesave.sav")
    if not os.path.isfile(report) or not os.path.isfile(save):
        return failed + ["no report or no save"]
    clubs = []
    _seasons, comps = rrc.read_report(report, clubs)
    start = {c["file"]: c for c in comps if c["phase"] == "start" and c["season"] == year and c["file"] in (lower, upper)}
    if set(start) != {lower, upper}:
        return failed + [f"no start lines of {lower} and {upper}"]
    record = {f: c["record"] for f, c in start.items()}
    counts = {f: c["clubs"] for f, c in start.items()}
    lines = cross_lines(log)
    # The divisions at the start of the season after the first final: the save can lie a season change later.
    finals = [x for x in lines if x[0] == lower and x[2] == "final"]
    after = finals[0][1] + 1 if finals else year + 1
    division = {c["name"]: c["division"] for c in clubs if c["season"] == after and c["phase"] == "start"}
    if finals and not division:
        return failed + [f"moves: no club lines at the start of {after}"]
    failed += judge(lines, lower, upper, record, division, counts)
    if lower == "jap_j2":
        failed += home_failures(log, lower, [x for x in lines if x[0] == lower and x[2] == "final"])
    return failed


def home_failures(log, lower, finals):
    """The J1 club is at home in the last tie: the first club of the "round 3 tie 1" line (RECOMP_PLAYOFF_LOG)."""
    mark = f"recomp: playoffs: {lower}: round 3 tie 1: "
    with open(log, encoding="latin-1") as f:
        ties = [line[len(mark):] for line in f if line.startswith(mark)]
    if not finals or not ties:
        return ["home: no last tie in the log"]
    upper_club = finals[0][3][2]
    return [] if ties[0].startswith(f"{upper_club} (place") else [f"home: {upper_club} is not at home: {ties[0].strip()}"]


def self_test():
    five = ["a", "b", "c", "d", "e", "over four", "day 1"]
    lines = [("low", 2012, "drawn", five), ("low", 2012, "final", ["a", "low", "u", "up"])]
    record = {"low": 1, "up": 2}
    good = {"a": 2, "u": 1, "x": 1, "y": 2}
    assert not judge(lines, "low", "up", record, good, {"low": 2, "up": 2})
    assert judge([], "low", "up", record, good, {})[0].startswith("drawn")
    assert judge(lines[:1], "low", "up", record, good, {})[0].startswith("final")
    assert any(f.startswith("moves") for f in judge(lines, "low", "up", record, {"a": 1, "u": 1}, {}))
    assert any(f.startswith("count") for f in judge(lines, "low", "up", record, good, {"low": 9}))
    kept = [("low", 2012, "drawn", five), ("low", 2012, "final", ["u", "up", "u", "up"])]
    assert not judge(kept, "low", "up", record, {"u": 2}, {})
    print("over_four_run self-test: ok")
    return 0


def main(argv):
    if "--self-test" in argv:
        return self_test()
    if "--list" in argv:
        for name, run in RUNS.items():
            print(f"{name}\t{run[0]}\t{run[5]}")
        return 0
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    os.environ["RECOMP_PLAYOFF_LOG"] = "1"      # the ties of each round, for the home check
    only = soak_run.arg(argv, "--only")
    names = only.split(",") if only else list(RUNS)
    keep = os.path.abspath(soak_run.arg(argv, "--keep")) if "--keep" in argv else tempfile.mkdtemp(prefix="overfour-")
    timeout = int(soak_run.arg(argv, "--timeout", 5400))
    bad = 0
    for name in names:
        year, nation, _lower, _upper, days, _what = RUNS[name]
        database = None if year in rrc.BASE_DATA_YEARS else rrc.season_database(year)
        work = os.path.join(keep, name)
        if "--reuse" not in argv:
            soak_run.wait_for_room()
            o = {"program": program, "game": game, "start": year, "last": year + 2, "seed": 1, "nations": [nation],
                 "database": database, "rules": "on", "extra": [], "timeout": timeout}
            code, secs, _box = soak_run.new_game_leg(o, work, days)
            print(f"{name}: code {code}, {secs / 60:.0f} minutes")
            if code != 0:
                print(f"{name}: FAIL: exit {code}")
                bad += 1
        failed = check(name, work)
        print(f"{name}: {'ok' if not failed else 'FAIL'}")
        for line in failed:
            print(f"  {line}")
        bad += bool(failed)
    print(f"folders: {keep}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
