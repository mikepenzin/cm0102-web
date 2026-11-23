#!/usr/bin/env python3
"""The I liga's promotion play-off waits for the end of the Ekstraklasa's spring stage (0.6.2 open item 104,
src/recomp_rt/win/season_pol_playoff.c).

Usage:
    python3 tests/seasons/pol_playoff_run.py <cm0102_recomp> <game folder> [--keep <folder>] [--wait-day <day>]

The run loads a save of day 142 of 2200 (the date jump to 2198 of the run set, jump2198: the December 2025 database,
all nations, changing rules on, seed 1, played on from its save of August 2199) and plays to the start report of
2200/01 (1 August 2200). In this game an Ekstraklasa match of the spring stage comes after the I liga's last round on
day 143. Up to 0.6.2 pass 2 the I liga made its play-off that day from the spring stage's places as they stood, one of
the two relegation play-off places was still open, the play-off's draw never came, and the Polish leagues held
edition 2199 through 2200/01 (league-stall). The save: RECOMP_POL_PLAYOFF_SAVE, else
test-fixtures/pol-playoff-2200.sav; the database: test-fixtures/databases/December 2025*.

--wait-day <day> (test switch RECOMP_TEST_POL_WAIT): the play-off also waits to that day, as if the spring stage ended
then. Day 160: both leg dates of the original (31 May and 4 June) have passed. Day 168: the play-off cannot end before
the Polish update day 170. The run then also checks the play-off's dates after the wait and, for a second leg on or
after day 170, the season change that waits for it.

The checks: no message box; the log has the lines of the play-off that waits and of its making after the wait; at the start report of 2200/01 the
Ekstraklasa (pol_first) and the I liga (pol_second) hold edition 2200.
"""
import os
import re
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
WAIT = "recomp: playoffs: pol_second 2199: the play-off waits for the end of the Ekstraklasa's last stage"
MADE = "recomp: playoffs: pol_second 2199: the play-off is made on day "
HOLD = "recomp: playoffs: entry 25 waits for the play-off of pol_second"
CHANGED = "recomp: playoffs: entry 25 changes its season on day"
UPDATE_DAY = 170
TIMEOUT = 3600


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    keep = os.path.abspath(soak_run.arg(argv, "--keep")) if "--keep" in argv else tempfile.mkdtemp(prefix="pol-playoff-")
    save = os.environ.get("RECOMP_POL_PLAYOFF_SAVE", os.path.join(FIXTURES, "pol-playoff-2200.sav"))
    databases = os.path.join(FIXTURES, "databases")
    names = sorted(n for n in os.listdir(databases) if n.startswith("December 2025"))
    if not os.path.isfile(save) or not names:
        print(f"pol_playoff_run.py: no save {save} or no December 2025 database in {databases}")
        return 2
    work = os.path.join(keep, "run")
    if os.path.exists(work):
        print(f"pol_playoff_run.py: {work} exists; use a new --keep folder")
        return 2
    import fixtures
    user, home, shots = soak_run.folders(work)
    soak_run.settings(user, "on", os.path.join(databases, names[0]))
    shutil.copyfile(save, os.path.join(user, "soak.sav"))
    load = soak_run.smoke_run.without_intro_click(fixtures.LOAD_STEPS)
    steps = [x for x in load if x != "wait first-news"] + [f"{TIMEOUT * 1000} quit"]
    report, log = os.path.join(work, "report.tsv"), os.path.join(work, "stderr.txt")
    extra = {"RECOMP_DATE_JUMP": "1", "RECOMP_SEASON_CHANGE_LOG": "1"}
    if "--wait-day" in argv:
        extra["RECOMP_TEST_POL_WAIT"] = soak_run.arg(argv, "--wait-day")
    env = soak_run.leg_env(os.path.abspath(argv[1]), os.path.abspath(argv[2]), user, home, shots, steps, 2201, report,
                           extra)
    try:
        return play(argv, env, user, report, log)
    finally:
        tidy(user)


def forced_checks(wait, text):
    """--wait-day: the play-off's dates follow from the day the wait ends (season_pol_playoff.c dates_from_today):
    the draw after that day, the first leg after the draw, the second after the first. A second leg on or after the
    Polish update day (170) needs the season change to wait for the play-off (season_playoffs_hold)."""
    m = re.search(r"pol_second 2199: the play-off draws on day (\d+) of 2200, legs on day (\d+) of "
                  r"2200 and day (\d+) of 2200", text)
    if not m:
        return ["no line of the play-off's dates after the wait"]
    draw, leg1, leg2 = (int(x) for x in m.groups())
    fails = []
    if not wait < draw < leg1 < leg2:
        fails.append(f"dates after a wait to day {wait}: draw {draw}, legs {leg1} and {leg2}, not in that order after it")
    if leg2 >= UPDATE_DAY and (HOLD not in text or CHANGED not in text):
        fails.append(f"second leg on day {leg2}, not before the update day {UPDATE_DAY}, and no hold of entry 25")
    return fails


def tidy(user):
    """The loaded copy, the database copy and the *.tmp work files go; the log and the report stay."""
    path = os.path.join(user, "soak.sav")
    if os.path.exists(path):
        os.remove(path)
    shutil.rmtree(os.path.join(user, "Databases"), ignore_errors=True)
    for n in os.listdir(user):
        if n.endswith(".tmp"):
            os.remove(os.path.join(user, n))


def play(argv, env, user, report, log):
    work = os.path.dirname(log)
    start, box, reached = time.time(), None, False
    with open(log, "w") as out:
        p = subprocess.Popen([os.path.abspath(argv[1]), "--game", os.path.abspath(argv[2]), "-seed", "1"], env=env,
                             stdout=subprocess.DEVNULL, stderr=out)
        while p.poll() is None and time.time() - start < TIMEOUT:
            time.sleep(5)
            box = soak_run.first_box(log)
            if os.path.exists(report):
                with open(report, encoding="latin-1") as f:
                    reached = "\nseason\t2200\tstart\t" in f.read()
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
        fails.append(f"no start report of 2200/01 after {time.time() - start:.0f} s")
    with open(log, encoding="latin-1") as f:
        text = f.read()
    if WAIT not in text:
        fails.append("no line of the play-off that waits for the Ekstraklasa")
    elif MADE not in text:
        fails.append("no line of the play-off made after the wait")
    if "--wait-day" in argv:
        fails += forced_checks(int(soak_run.arg(argv, "--wait-day")), text)
    if reached:
        _, comps = rrc.read_report(report)
        for file in ("pol_first", "pol_second"):
            years = {c["year"] for c in comps if c["file"] == file and c["season"] == 2200 and c["phase"] == "start"}
            if years != {2200}:
                fails.append(f"{file} holds edition {sorted(years)} at the start of 2200/01, not 2200")
    for f in fails:
        print(f"FAIL  {f}")
    print("PASS" if not fails else f"FAILED: {len(fails)} ({work})")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
