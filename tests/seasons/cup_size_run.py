#!/usr/bin/env python3
"""The cups of western Europe take the real divisions (0.7.2 league sizes, slice 5; spec
docs/specs/2026-11-13-league-sizes-0.7.2-spec.md, D14; src/recomp_rt/win/season_cup_sizes_west.c).

Usage:
    python3 tests/seasons/cup_size_run.py <cm0102_recomp> <game folder> [--only <run>,...] [--keep <folder>]
        [--reuse] [--timeout <seconds>]
    python3 tests/seasons/cup_size_run.py --list

Each run is a season jump of tests/seasons/rules_report_check.py with one nation selected, changing rules on,
RECOMP_SIZE_LOG=1, from the start season through the next season (two season ends). A division of the run's
SIZES plays the count of its row, below the class count (a shrunk division: before slice 5 these were the floors of
tools/recomp/league_sizes.py, and the Danish First Division kept its database count). The grown divisions of these
nations need their league slices (8, 9 and 10, each blocked by this one); test_season_cup_west.c checks the counts
of the cups with grown divisions, and those slices' runs check that no cup stops.
The checks of each run:
    code     the jump ends with code 0 and shows no message box (a cup that is short stops at its box);
    sizes    each division of SIZES holds its count in the start report of the first season;
    fires    each line of FIRES is in the log (the cup's count follows the divisions);
    moves    no league or cup of the nation stays at its edition from one season end to the next
             (rules_report_check.league_stalls): every cup played every round and moved on.
A run takes about 10 to 30 minutes. --reuse checks the logs in --keep again. Exit code 1 on a failure.
"""
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
import rules_report_check as rrc  # noqa: E402

# run: (nation, start year, extra divisions, what it shows)
RUNS = {
    "bel2016": ("Belgium", 2016, [], "First Division 16 (the class holds 18) and a third tier of 16: the Belgian "
                                     "Cup takes more lower clubs"),
    "den2020": ("Denmark", 2020, [], "First Division 12 (the class holds 16; the November 2020 database gives 16): "
                                     "the Danish Cup takes clubs with no division"),
    "fra2024": ("France", 2024, [], "Ligue 1 18, Ligue 2 18, National 17: the French Cup and League Cup"),
    "ita2018": ("Italy", 2018, [], "Serie B 19 (the class holds 20): the Coppa Italia and the Coppa Italia Serie C"),
    "hol2005": ("Holland", 2005, [], "Eerste Divisie row 20 (the class holds 18): the Dutch Cup with no floor"),
}
# run: {file: clubs in the start report}
SIZES = {
    "bel2016": {"bel_first": 16},
    "den2020": {"den_first": 12},
    "fra2024": {"fra_first": 18, "fra_second": 18, "fra_third": 17},
    "ita2018": {"ita_ser_b": 19},
    "hol2005": {"hol_first": 18, "hol_prm": 18},
}
# run: log lines (their start) that must fire
FIRES = {
    "bel2016": ["recomp: cup sizes: Belgian Cup: "],
    "den2020": ["recomp: cup sizes: Danish Cup: "],
    "fra2024": ["recomp: cup sizes: French League Cup: ", "recomp: cup sizes: French Lower Division: "],
    "ita2018": [],
    "hol2005": [],
}
PREFIX = {"Belgium": "bel_", "Denmark": "den_", "France": "fra_", "Italy": "ita_", "Holland": "hol_"}


def faults(name, work):
    nation, year, _extra, _what = RUNS[name]
    log_path, report = os.path.join(work, "stderr.txt"), os.path.join(work, "report.tsv")
    if not os.path.exists(log_path) or not os.path.exists(report):
        return [f"{name}: no log or report in {work}"]
    log = open(log_path, encoding="latin-1").read().split("\n")
    fails = [f"{name}: {x.strip()}" for x in log if "recomp: message box from" in x]
    if not any("recomp: season jump: done" in x for x in log):
        fails.append(f"{name}: the jump did not reach the end of {year + 1}/{(year + 2) % 100:02d}")
    try:
        _seasons, comps = rrc.read_report(report)
    except SystemExit:
        return fails + [f"{name}: no rules report ({report})"]
    start = {}          # a file can name more objects (fra_first: the CFA stub and an empty FraFirst): the largest league
    for c in comps:
        if c["season"] == year and c["phase"] == "start" and c["kind"] == "league":
            start[c["file"]] = max(start.get(c["file"], 0), c["clubs"])
    for file, want in SIZES[name].items():
        if start.get(file) != want:
            fails.append(f"{name}: {file} holds {start.get(file)} clubs at the start of {year}, want {want}")
    for tag in FIRES[name]:
        if not any(x.startswith(tag) for x in log):
            fails.append(f"{name}: no log line '{tag.strip()}'")
    stalls = [s for s in rrc.league_stalls(comps) if f": {PREFIX[nation]}" in s]
    fails += [f"{name}: {s}" for s in stalls]
    return fails


def main(argv):
    if "--list" in argv:
        for k, (nation, year, extra, what) in RUNS.items():
            print(f"{k}\t{nation} {year}{' +' + ','.join(extra) if extra else ''}\t{what}")
        return 0
    if len(argv) < 2:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[0]), os.path.abspath(argv[1])
    only = rrc.arg(argv, "--only", None)
    names = only.split(",") if only else list(RUNS)
    keep = rrc.arg(argv, "--keep", None)
    timeout = int(rrc.arg(argv, "--timeout", "5400"))
    base = os.path.abspath(keep) if keep else tempfile.mkdtemp(prefix="cm0102-cup-size-")
    failed = 0
    for name in names:
        nation, year, extra, _what = RUNS[name]
        work = os.path.join(base, name)
        if "--reuse" not in argv:
            os.makedirs(work, exist_ok=True)
            db = rrc.run_database(argv, year, os.path.join(work, "owndb"))
            code, _report = rrc.run(program, game, year + 1, year, db, [nation], extra, os.path.join(work, "game"),
                                    timeout, {"changing_rules": "on"}, {"RECOMP_SIZE_LOG": "1"}, stop_on_box=True)
            for f in ("stderr.txt", "report.tsv"):
                if os.path.exists(os.path.join(work, "game", f)):
                    os.replace(os.path.join(work, "game", f), os.path.join(work, f))
            if code != 0:
                print(f"FAIL {name}: the run ended with code {code} ({work})")
        fails = faults(name, work)
        for f in fails:
            print("FAIL", f)
        print(("FAIL" if fails else "PASS") + f" {name} ({work})")
        failed += bool(fails)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
