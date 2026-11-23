#!/usr/bin/env python3
"""The Allsvenskan with 16 clubs from 2008 (GitHub issue 106).

Usage:
    python3 tests/seasons/swe_prm_run.py <cm0102_recomp> <game folder> [--year 2025] [--keep <folder>] [--reuse]
        [--last <season>] [--timeout <seconds>]

The run starts a new game in a season (default 2025, December 2025) with all nations (Sweden is not one of the
first 16 rows of the Select League(s) screen), changing rules on, and plays that one season (RECOMP_SEASON_JUMP =
the start year: the rules report of tests/seasons/rules_report_check.py, the end report of that season, then the
program ends). --last plays to that season and checks it: a 2006 start (Summer 2006, own database) with --last
2007 plays the change from 14 to 16 at a season start. A calendar-year league holds edition Y in the start report
of season Y (real 2007: 14 clubs) and edition Y + 1 in its end report (real 2008: 16), so a later season checks the
end report and the log lines only. SwePrm held 14 clubs: create_table 0x890430 allocates 14 rows (push 0x33a at 0x89043b) and
create_schedule 0x88feb0 makes 26 dates (0x88feea). The rules table (swe_prm) has 16 from 2008.
The checks:
    report  the Allsvenskan row (file swe_prm) of the start report holds 16 clubs, each pair meeting twice; the end
            report holds 16 clubs with 30 matches each (+0xa7, League::check_complete 0x689ad0);
    log     RECOMP_SIZE_LOG (src/recomp_rt/win/season_sizes.c) names the table of 16 rows and the template of 30
            dates of swe_prm;
    expect  the rules report check of the run lists no clubs difference of swe_prm;
    box     no message box.
A run takes about 20 to 40 minutes. --reuse checks the logs in --keep again. Exit code 1 on a failure.
"""
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import rules_report_check as rrc  # noqa: E402

CLUBS = 16


def report_rows(path, year, file="swe_prm"):
    """{phase: row} of the rows of a league file in a season of the rules report."""
    out, cols = {}, None
    with open(path, encoding="latin-1") as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            if p[0] == "columns":
                cols = p[1:]
            elif cols and p[0] == "comp" and len(p) == len(cols) + 1:
                row = dict(zip(cols, p[1:]))
                if row["file"] == file and row["season"] == str(year):
                    out[row["phase"]] = row
    return out


def faults(work, year, first):
    report = os.path.join(work, "report.tsv")
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    fails = [f"{year}: {x.strip()}" for x in log.split("\n") if "recomp: message box from" in x]
    rows = report_rows(report, year)
    start, end = rows.get("start"), rows.get("end")
    if year != first:
        pass                            # the start report holds the edition of that calendar year
    elif not start:
        fails.append(f"{year}: no Allsvenskan row in the start report")
    elif (int(start["clubs"]), int(start["rounds"])) != (CLUBS, 2):
        fails.append(f"{year}: start report clubs and meetings {(int(start['clubs']), int(start['rounds']))}, "
                     f"want {(CLUBS, 2)}")
    want = 2 * (CLUBS - 1)
    if not end:
        fails.append(f"{year}: no Allsvenskan row in the end report")
    elif (int(end["clubs"]), int(end["matches"])) != (CLUBS, want):
        fails.append(f"{year}: end report clubs and matches {(int(end['clubs']), int(end['matches']))}, want "
                     f"{(CLUBS, want)}")
    table = [x for x in log.split("\n") if "recomp: league sizes: swe_prm table" in x]
    if not any(f"{CLUBS} rows" in x for x in table):
        fails.append(f"{year}: no log line of a swe_prm table of {CLUBS} rows")
    dates = [x for x in log.split("\n") if "recomp: league sizes: swe_prm dates" in x]
    if not any(f"{want} dates for {CLUBS} clubs" in x for x in dates):
        fails.append(f"{year}: no log line of a swe_prm template of {want} dates")
    if year != first:
        return fails
    clubs = []
    seasons, comps = rrc.read_report(report, clubs)
    diffs = set(rrc.differences(seasons, comps, rrc.make_rules.read_rows())) | rrc.division_differences(comps, clubs)
    diffs = sorted(d for d in diffs if d[1] in ("clubs", "division_clubs") and "swe_prm" in d[2])
    fails += [f"{year}: rules report difference: {d[2]}" for d in diffs]
    return fails


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[0]), os.path.abspath(argv[1])
    year = int(rrc.arg(argv, "--year", "2025"))
    last = int(rrc.arg(argv, "--last", str(year)))
    keep = rrc.arg(argv, "--keep", None)
    timeout = int(rrc.arg(argv, "--timeout", "5400"))
    work = os.path.abspath(keep) if keep else tempfile.mkdtemp(prefix=f"cm0102-swe-prm-{year}-")
    if "--reuse" not in argv:
        db = rrc.run_database(argv, year, os.path.join(work, "owndb"))
        code, _report = rrc.run(program, game, last, year, db, ["all"], [], work, timeout,
                                {"changing_rules": "on"}, {"RECOMP_SIZE_LOG": "1"})
        if code != 0:
            print(f"FAIL {year}: the run ended with code {code} ({work})")
            return 1
    fails = faults(work, last, year)
    for f in fails:
        print("FAIL", f)
    print(("FAIL" if fails else "PASS") + f" swe_prm {year} to {last} ({work})")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
