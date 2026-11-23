#!/usr/bin/env python3
"""Hosts of future tournaments (0.7.3 slice 16, issue 146).

Usage:
    python3 tests/seasons/hosts_news_run.py <cm0102_recomp> <game folder> [--only 2026,2018,2022] [--jobs n] [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py (England, changing rules on) with the test log
RECOMP_SEASON_INTL_LOG=1. The runs:
    2026    October 2026, 2026/27 to 2028/29: the qualifiers of Euro 2028 and the host rows of 2028, 2030 and 2032
    2018    October 2018, 2018/19 to 2019/20: the qualifiers of Euro 2020 (a start in 2019 takes the real draw, with no
            qualifiers and so no news)
    2022    October 2022, 2022/23 to 2023/24: the qualifiers of Euro 2024, with older host rows in the table

The checks:
    - the jump ends with code 0
    - the news of the qualified nations names the hosts of a Euro whose host row is empty
    - that host row stays empty, so no host takes a place at the finals
    - 2026: the host rows of World Cup 2030 and Euro 2032 hold their real hosts
"""
import concurrent.futures
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
import rules_report_check as rrc  # noqa: E402

DB = "/Users/max/Projects/cm0102/test-fixtures/databases"
RUNS = {
    "2026": (2026, 2028, "October 2026", [
        r"qualified news of [^\n]* 2028 names the hosts England;Republic of Ireland\n",
        r"host euro_champ 2028: -",
        r"host world_cup 2030: Morocco;Portugal;Spain;Argentina;Paraguay;Uruguay",
        r"host euro_champ 2032: Italy;T",
    ]),
    "2018": (2018, 2019, "October 2018", [
        r"qualified news of [^\n]* 2020 names the hosts England\n",
        r"host euro_champ 2020: -",
    ]),
    "2022": (2022, 2023, "October 2022", [
        r"qualified news of [^\n]* 2024 names the hosts Germany\n",
    ]),
}


def one(program, game, name, keep):
    start, last, database, wanted = RUNS[name]
    work = os.path.join(keep, name)
    code, _ = rrc.run(program, game, last, start, rrc.database_folder(os.path.join(DB, database)), ["England"], [], work,
                      7200, {"changing_rules": "on"}, {"RECOMP_SEASON_INTL_LOG": "1"})
    with open(os.path.join(work, "stderr.txt"), encoding="latin-1") as f:
        log = f.read()
    fails = [] if code == 0 else [f"the jump ended with code {code}"]
    fails += [f"no log line: {w}" for w in wanted if not re.search(w, log)]
    return name, fails


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = (os.path.abspath(a) for a in argv[1:3])
    names = argv[argv.index("--only") + 1].split(",") if "--only" in argv else list(RUNS)
    jobs = int(argv[argv.index("--jobs") + 1]) if "--jobs" in argv else 2
    keep = os.path.abspath(argv[argv.index("--keep") + 1]) if "--keep" in argv else tempfile.mkdtemp(prefix="hosts-news-")
    failed = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        for name, fails in ex.map(lambda n: one(program, game, n, keep), names):
            print(f"{'ok  ' if not fails else 'FAIL'}  {name}" + "".join(f"\n      {x}" for x in fails))
            failed += bool(fails)
    print(f"logs: {keep}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
