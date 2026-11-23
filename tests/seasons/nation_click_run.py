#!/usr/bin/env python3
"""The click on a nation below the 16 rows of Select League(s) selects that nation (open item 231).

Usage:
    python3 tests/seasons/nation_click_run.py <cm0102_recomp> <game folder> [--only 3.9.60,3.9.65,3.9.68] [--nations Poland,...]
        [--jobs 2] [--keep <folder>] [--data-3960 <folder>] [--data-3965 <folder>]

For each data set and each nation of rules_report_check.NATION_ROWS_BELOW (Poland to Wales) a new 2001/02 game selects
England and that nation with the clicks of rules_report_check.league_steps, and stops at the first start report. The
report must hold the domestic competitions of that nation (the file prefix of NATION_CODE) and of no other nation
below Norway. The first version clicked one row too low: Portugal gave the Republic of Ireland on 3.9.68. The test
fails when a click selects another nation, or none, or when a game ends with a message box.
"""
import concurrent.futures
import os
import shutil
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rules_report_check as rrc  # noqa: E402

NATION_CODE = {"Poland": "pol", "Portugal": "por", "Republic of Ireland": "ire", "Russia": "rus", "Scotland": "sco",
               "South Korea": "kor", "Spain": "spa", "Sweden": "swe", "Turkey": "tur", "United States": "usa",
               "Wales": "wel"}
DATA = {"3.9.60": "/Users/max/Projects/cm0102/data-3960", "3.9.65": "/Users/max/Projects/cm0102/data-3965",
        "3.9.68": None}


def selected(report):
    """{nation of NATION_CODE: 1} whose competitions the start report holds (a file prefix before the first _)."""
    prefixes = {c["file"].split("_")[0] for c in rrc.read_report(report)[1]}
    return {n for n, code in NATION_CODE.items() if code in prefixes}


def wait_for_load():
    """No game starts while the load of the last minute is 20 or more (the machine limit is 24)."""
    while os.getloadavg()[0] >= 20 or os.path.exists("/Users/max/Projects/cm0102/run-logs/HOLD"):
        time.sleep(20)


def one(program, game, db, nation, work, data):
    wait_for_load()
    shutil.rmtree(work, ignore_errors=True)       # a folder of an earlier run
    env = {"RECOMP_DATA_DIR": data} if data else {}
    code, report = rrc.run(program, game, 2001, None, None, ["England", nation], [], work, 1800,
                           {"changing_rules": "off", "database": db}, env, start_only=True)
    if not os.path.exists(report):
        return [f"{db} {nation}: no start report (code {code})"]
    got = selected(report)
    if got != {nation}:
        return [f"{db} {nation}: the click selected {sorted(got) or 'no nation below Norway'}"]
    return []


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])

    def opt(name, default):
        return argv[argv.index(name) + 1] if name in argv else default
    dbs = opt("--only", ",".join(DATA)).split(",")
    nations = opt("--nations", ",".join(rrc.NATION_ROWS_BELOW)).split(",")
    data = dict(DATA, **{"3.9.60": opt("--data-3960", DATA["3.9.60"]), "3.9.65": opt("--data-3965", DATA["3.9.65"])})
    keep = opt("--keep", None) or tempfile.mkdtemp(prefix="cm0102-nation-click-")
    fails = []
    with concurrent.futures.ThreadPoolExecutor(int(opt("--jobs", 2))) as pool:
        jobs = [pool.submit(one, program, game, db, n, os.path.join(keep, f"{db}-{n.replace(' ', '_')}"), data[db])
                for db in dbs for n in nations]
        for j in jobs:
            fails += j.result()
    for f in fails:
        print("FAIL  " + f)
    print(f"{'ok  ' if not fails else 'FAIL'}  nation click: {len(dbs) * len(nations)} starts, {len(fails)} wrong")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
