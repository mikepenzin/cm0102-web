#!/usr/bin/env python3
"""No club becomes the club of all Welsh and Irish new players (open item 262; src/recomp_rt/win/youth_club_lists.c).

Usage:
    python3 tests/seasons/youth_list_run.py <cm0102_recomp> <game folder> --keep <folder> [--days <n>] [--keep-fault]
        [--database <folder>]

The run: a new game in 2026 with Italy, changing rules on, the base data (3.9.68), played --days game days (default
120) with RECOMP_YOUTH_LOG. Without the fix the club list of England for new players (build_nation_club_lists
0x7acfc0) holds Guiseley alone, the one English club of reputation 2 or less. pick_club_from_nation_list 0x7ad590
then gives Guiseley every Welsh and Irish new player of that path: 10 in the first 120 days of the fault run (the
first on day 227 of 2026), 53 by day 182 of 2027 in the run of the item.

The checks of the fixed run:
    - the game plays the days and saves, with no message box
    - the log has the line "recomp: youth club lists: Welsh and Irish new players take the English list of"
    - Welsh and Irish new players join 3 clubs or more, and Guiseley gets at most a third of them (youth pool log)
--keep-fault runs with RECOMP_TEST_YOUTH_LIST=original (the original lists). That run passes when it shows the fault:
the days played with no box, 5 or more Welsh and Irish new players, and all of them at Guiseley. This is the proof
that the run finds the fault, on the base data only.
--database <folder> plays a season database in place of the base data. With October 2026 the original English list
holds no club (no English club of reputation 2 or less), so the original gives those players no club at all
(0x7ad590 returns 0 for an empty list, 0x7ad5ff..0x7ad602) and the fault run has no join to count. The fixed run on
October 2026 checks the same three points: 304 clubs in the list, and the players join English clubs.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
import rules_report_check as rrc  # noqa: E402
import soak_run  # noqa: E402

LINE = "recomp: youth club lists: Welsh and Irish new players take the English list of"
NATIONS = ("Wales", "Republic of Ireland", "Northern Ireland")


def main(argv):
    if len(argv) < 4 or "--keep" not in argv:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = os.path.abspath(soak_run.arg(argv, "--keep"))
    days = int(soak_run.arg(argv, "--days", "120"))
    database = os.path.abspath(soak_run.arg(argv, "--database")) if "--database" in argv else None
    fault = "--keep-fault" in argv
    env = {"RECOMP_YOUTH_LOG": "1", "RECOMP_SAVE_AFTER_DAYS": str(days)}
    if fault:
        env["RECOMP_TEST_YOUTH_LIST"] = "original"
    work = os.path.join(keep, "fault" if fault else "fixed")
    soak_run.wait_for_room()
    code, _ = rrc.run(program, game, 2028, 2026, database, ["Italy"], [], work, 7200, {"changing_rules": "on"}, env)
    log = open(os.path.join(work, "stderr.txt"), "rb").read().decode("latin-1")
    failures = 0

    def check(cond, text):
        nonlocal failures
        print(("ok    " if cond else "FAIL  ") + text, flush=True)
        failures += not cond

    saved = f"after {days} game days" in log
    box = [line for line in log.splitlines() if "message box" in line]
    check(saved and not box, f"{days} days played and saved, no message box ({code}, {box[:1]})")
    joins = [line for line in log.splitlines()
             if " joins " in line and any(f"new player of {n} joins" in line for n in NATIONS)]
    clubs = {line.split(" joins ", 1)[1].split(" of ", 1)[0] for line in joins}
    at_guiseley = sum(" joins Guiseley of " in line for line in joins)
    if fault:
        check(len(joins) >= 5 and at_guiseley == len(joins),
              f"the fault: 5 or more Welsh and Irish new players, all at Guiseley ({at_guiseley} of {len(joins)})")
    else:
        check(LINE in log, "the log has the line: " + LINE)
        check(len(clubs) >= 3 and 3 * at_guiseley <= len(joins),
              f"Welsh and Irish new players join {len(clubs)} clubs, {at_guiseley} of {len(joins)} at Guiseley")
    shown = 0
    for line in log.splitlines():
        if ("youth club lists:" in line or line in joins) and shown < 15:
            print("      " + line[:200])
            shown += 1
    for name in ("samesave.sav",):                       # about 50 MB
        p = os.path.join(work, "user", name)
        if os.path.exists(p):
            os.remove(p)
    print(("PASS" if not failures else f"FAIL ({failures})") + (" (the fault run shows the fault)" if fault else ""))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
