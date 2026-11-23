#!/usr/bin/env python3
"""Finished level clubs under head-to-head stay open while a club can still reach their points (0.6.2 open item 167,
src/recomp_rt/win/season_level_ties_game.c).

Usage:
    python3 tests/seasons/pol_level_ties_run.py <cm0102_recomp> <game folder> [--keep <folder>]

The run loads a save of day 141 of 2075 (run set pass 3, a2026s3: the October 2026 database, all nations, changing
rules on, seed 3, Tier A, played on from its save of July 2066 by the build of the run set) and plays to the start
report of 2075/76 (1 August 2075). In the continuous game the I liga 2074/75 ended with places 2 and 3 level on 52
points while the champion, on 52 with a match left on day 144, was level with them; both got promotion (status 0), the
play-off had one I liga club, and both Polish leagues stalled. A load changes the later match results, so this save
does not replay that tie (with the run set's build the leagues change their season here). The case itself is in
test_season_level_ties.c. This run checks the game part on a real save: the window of level clubs opens for the
I liga 2074 and both leagues reach edition 2075. The save: RECOMP_POL_LEVEL_TIES_SAVE, else
test-fixtures/pol-level-ties-2075.sav.

The checks: no message box; the log has the line of the level clubs that stay open for the I liga 2074; at the start
report of 2075/76 the Ekstraklasa (pol_first) and the I liga (pol_second) hold edition 2075.
"""
import os
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import soak_run  # noqa: E402
import rules_report_check as rrc  # noqa: E402

FIXTURES = "/Users/max/Projects/cm0102/test-fixtures"
DATABASE = os.path.join(FIXTURES, "databases", "October 2026", "October 2026 Data Update")
OPEN = "recomp: level ties: I liga 2074: finished level clubs stay open"
SEASON = 2075
TIMEOUT = 3600


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    keep = os.path.abspath(soak_run.arg(argv, "--keep")) if "--keep" in argv else tempfile.mkdtemp(prefix="pol-ties-")
    save = os.environ.get("RECOMP_POL_LEVEL_TIES_SAVE", os.path.join(FIXTURES, "pol-level-ties-2075.sav"))
    if not os.path.isfile(save) or not os.path.isdir(DATABASE):
        print(f"pol_level_ties_run.py: no save {save} or no database {DATABASE}")
        return 2
    work = os.path.join(keep, "run")
    if os.path.exists(work):
        print(f"pol_level_ties_run.py: {work} exists; use a new --keep folder")
        return 2
    import fixtures
    user, home, shots = soak_run.folders(work)
    soak_run.settings(user, "on", DATABASE)
    shutil.copyfile(save, os.path.join(user, "soak.sav"))
    load = soak_run.smoke_run.without_intro_click(fixtures.LOAD_STEPS)
    steps = [x for x in load if x != "wait first-news"] + [f"{TIMEOUT * 1000} quit"]
    report, log = os.path.join(work, "report.tsv"), os.path.join(work, "stderr.txt")
    env = soak_run.leg_env(os.path.abspath(argv[1]), os.path.abspath(argv[2]), user, home, shots, steps, SEASON + 1,
                           report, {"RECOMP_TIER_A": "1"})
    try:
        return play(argv, env, report, log)
    finally:
        tidy(user)


def tidy(user):
    """The loaded copy, the database copy and the *.tmp work files go; the log and the report stay."""
    path = os.path.join(user, "soak.sav")
    if os.path.exists(path):
        os.remove(path)
    shutil.rmtree(os.path.join(user, "Databases"), ignore_errors=True)
    for n in os.listdir(user):
        if n.endswith(".tmp"):
            os.remove(os.path.join(user, n))


def play(argv, env, report, log):
    work = os.path.dirname(log)
    start, box, reached = time.time(), None, False
    with open(log, "w") as out:
        p = subprocess.Popen([os.path.abspath(argv[1]), "--game", os.path.abspath(argv[2]), "-seed", "3"], env=env,
                             stdout=subprocess.DEVNULL, stderr=out)
        while p.poll() is None and time.time() - start < TIMEOUT:
            time.sleep(5)
            box = soak_run.first_box(log)
            if os.path.exists(report):
                with open(report, encoding="latin-1") as f:
                    reached = f"\nseason\t{SEASON}\tstart\t" in f.read()
            if box or reached:
                time.sleep(2)               # the report of the start is flushed whole (season_report.c write_season)
                break
        if p.poll() is None:
            p.kill()
            p.wait()
    fails = []
    if box:
        fails.append(f"message box: {box}")
    if not reached:
        fails.append(f"no start report of {SEASON}/{(SEASON + 1) % 100:02d} after {time.time() - start:.0f} s")
    with open(log, encoding="latin-1") as f:
        text = f.read()
    if OPEN not in text:
        fails.append("no line of the I liga's level clubs that stay open")
    if reached:
        _, comps = rrc.read_report(report)
        for file in ("pol_first", "pol_second"):
            years = {c["year"] for c in comps if c["file"] == file and c["season"] == SEASON and c["phase"] == "start"}
            if years != {SEASON}:
                fails.append(f"{file} holds edition {sorted(years)} at the start of {SEASON}/76, not {SEASON}")
    for f in fails:
        print(f"FAIL  {f}")
    print("PASS" if not fails else f"FAILED: {len(fails)} ({work})")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
