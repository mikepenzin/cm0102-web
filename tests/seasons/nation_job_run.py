#!/usr/bin/env python3
"""The national team job of the run set row a2001nation (Q15): a new game whose human manager also manages a national
team (RECOMP_TEST_NATION_JOB, src/recomp_rt/win/nation_job.c), played on holiday past the team's next match after
the appointment, and the same new game without the switch.

Usage:
    python3 tests/seasons/nation_job_run.py <cm0102_recomp> <game folder> [--nations England|all] [--days 75]
        [--keep <folder>]

Run "job" is leg 0 of a run set game (soak_run.new_game_leg, the 2001 start, changing rules on) with
RECOMP_TEST_NATION_JOB=England. It saves after --days game days (75: mid September 2001, after the FIFA days of
August and September). Its log must pass run_set.nation_job_check: the runtime appointed the human through
appoint_manager_to_club 0x69ebc0, he managed the team at every human turn to the team's next match, and the team
played that match. Run "plain" is the same game without the switch: its log must have
no line of the switch. Both must end with code 0 and no box.
With --nations England a new game takes a few minutes; all takes about 25 minutes. One game at a time.
"""
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [HERE, os.path.join(HERE, "future"), os.path.join(HERE, "..", "smoke"),
                os.path.join(HERE, "..", "fixtures")]
import run_set  # noqa: E402
import soak_run  # noqa: E402


def play(o, work, days, job):
    """Leg 0 with RECOMP_TEST_NATION_JOB=job, or without the switch when job is None."""
    soak_run.LEG_ENV.pop("RECOMP_TEST_NATION_JOB", None)
    os.environ.pop("RECOMP_TEST_NATION_JOB", None)              # leg_env copies os.environ
    if job:
        soak_run.LEG_ENV["RECOMP_TEST_NATION_JOB"] = job
    try:
        return soak_run.new_game_leg(dict(o, extra=[]), work, days)
    finally:
        soak_run.LEG_ENV.pop("RECOMP_TEST_NATION_JOB", None)


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    opt = lambda name, default: argv[argv.index(name) + 1] if name in argv[3:] else default  # noqa: E731
    keep = os.path.abspath(opt("--keep", tempfile.mkdtemp(prefix="cm0102-nation-job-")))
    days = int(opt("--days", "75"))
    o = {"program": os.path.abspath(argv[1]), "game": os.path.abspath(argv[2]), "start": 2001, "last": 2002,
         "seed": 1, "nations": opt("--nations", "England").split(","), "database": None, "rules": "on",
         "timeout": 7200}
    failed = []
    # One game at a time: soak_run.LEG_ENV is global, so the switch of one run must not reach the other.
    results = {name: play(o, os.path.join(keep, name), days, job) for name, job in (("plain", None), ("job", "England"))}
    for name, (code, took, box) in results.items():
        log = open(os.path.join(keep, name, "stderr.txt"), errors="replace").read()
        if code != 0 or box:
            failed.append(f"{name}: code {code}, box {box}")
        lines = [ln for ln in log.splitlines() if ln.startswith(run_set.NATION_JOB_LINE)]
        print("\n".join(f"{name}: {ln}" for ln in lines) or f"{name}: no line of the switch")
        if name == "plain" and lines:
            failed.append("plain: the switch acted without RECOMP_TEST_NATION_JOB")
        if name == "job":
            why = run_set.nation_job_check(log)
            if why:
                failed.append(f"job: {why}")
        print(f"{name}: {took:.0f} s, code {code}")
    for f in failed:
        print("FAIL  " + f)
    print(f"logs in {keep}")
    print("FAIL" if failed else "PASS")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
