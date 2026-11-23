#!/usr/bin/env python3
"""Finland: second teams stay out of the Veikkausliiga and play below their first team (open item 118, 0.7.0;
season_fin_second.c).

Usage:
    python3 tests/seasons/fin_second_run.py <cm0102_recomp> <game folder> [--only on,off,drop,old,late,hold,names] [--jobs n]
        [--keep <folder>] [--fix-off]

Each run is a season jump of tests/seasons/rules_report_check.py with Finland on the May 2026 database (changing
rules off), from the 2026 start. That database has two second teams in the First Division (HJK Klubi 04 Helsinki and
SJK Seinajoki II), and their first teams play in the Premier Division. The test switch RECOMP_TEST_FIN_SECOND_TOP=1
puts each second team at the top of its group when the group stage ends (FinFirst::create_play_off_stages
0x594080), as if it won the group. RECOMP_FIN_SECOND_LOG=1 logs the play-offs, the moves and a census at each
season change. Every run but off checks the rule's state, so any correct result of the season passes: after each
season change no second team plays in the Premier Division, none plays in the First Division unless its first team
plays in the Premier Division, the division sizes stay, and no second team plays a promotion play-off or goes up to
the Premier Division. A second team of a Premier Division club may go up to the First Division (Ilves Tampere II).
The runs, each with what it must also show:
    on    the shipped code, to 1 June 2027: a club of the table plays the promotion play-off in a second team's
          place
    off   RECOMP_FIN_SECOND=off, the original: a second team plays the promotion play-off (proves that the test
          sees the fault)
    drop  RECOMP_TEST_FIN_SECOND_DROP=1, to 1 June 2027: the first teams of both second teams go down at the season
          change, so both second teams go down to the Second Division and a club of the table stays up
    old   an older save, made in memory (no save is written or loaded): RECOMP_TEST_FIN_SECOND_OLD=1 plays as the
          original until the season change of 2026/27 is done and sends a second team up into the Premier Division.
          To 1 June 2028: at the next change it goes down and a club of status 3 stays up (the state checks start
          at change 2)
    late  RECOMP_TEST_FIN_SECOND_LATE=1, to 1 June 2027, an older save made after the play-off: a second team is the
          stage-1 finalist when stage 3 is built, and holds stage 1's place (status 5) at the season change. The
          next club of its group plays stage 3 in its place and the next club of the table takes status 5
    hold  the October 2018 database, 2018 start, to 1 June 2020, with RECOMP_TEST_FIN_SECOND_REP=1 (the second teams
          of the Second Division groups get the highest reputation) and RECOMP_TEST_FIN_SECOND_DROP=1: a second team
          whose first team was sent down is not picked to go up (Klubi 04 Helsinki in 2019). It may still go up at
          that change only when its first team goes up to the Premier Division at the same change. Second teams of
          Premier Division clubs are picked, and one whose first team then goes down leaves the First Division
          again, its place taken by a club of the groups
    names the September 2005 database, 2005 start, to 1 June 2006: HJK Reserves is read as HJK Helsinki's second
          team, Atlantis Akatemia and KPV juniorit as first teams
    vps   the April 2016 database, 2016 start, to 1 June 2017: Vaasan Palloseura-Juniorit is read as the farm side
          (second team) of Vaasan Palloseura, FC Jazz Pori as a first team, and the rule's state holds
--fix-off adds RECOMP_FIN_SECOND=off to every run, as a build without the fix: on, drop, late and hold must then FAIL
(the test first, item 118). All runs end with code 0 and show no message box of the game.
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
ENV = {"RECOMP_TEST_FIN_SECOND_TOP": "1", "RECOMP_FIN_SECOND_LOG": "1"}
CENSUS = re.compile(r"recomp: fin second: census (before|after): Premier (\d+) clubs, (\d+) second teams; "
                    r"First Division (\d+) clubs, (\d+) second teams, (\d+) below")


FIX_OFF = {}


def jump(name, program, game, work, env, last=2026, start=2026, db="May 2026"):
    code, _ = rrc.run(program, game, last, start, os.path.join(DB, db), ["Finland"], [], work, 7200, None,
                      dict(ENV, **env, **FIX_OFF))
    fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    fails += [f"{name}: message box {b}" for b in found]
    return fails, log


def changes(log):
    """Each Finnish season change: (census before, census after, the lines between)."""
    out, cur = [], None
    for line in log.splitlines():
        m = CENSUS.search(line)
        if m and m.group(1) == "before":
            cur = [tuple(map(int, m.groups()[1:])), None, []]
        elif m and cur:
            cur[1] = tuple(map(int, m.groups()[1:]))
            out.append(cur)
            cur = None
        elif cur and "recomp: fin second:" in line:
            cur[2].append(line)
    return out


def playoffs(log):
    return [l for l in log.splitlines() if "recomp: fin second: promotion play-off:" in l]


def common(name, log, n, skip=0):
    """The rule's state, whatever the results of the season: after each Finnish season change (from change skip + 1)
    no second team plays in the Premier Division and none plays in the First Division unless its first team plays
    in the Premier Division (so every second team plays below its first team), the division sizes stay, no second
    team goes up to the Premier Division, and no second team plays a promotion play-off."""
    fails = []
    seen = changes(log)
    if len(seen) < n:
        return fails + [f"{name}: {len(seen)} Finnish season changes seen, want {n}"], seen
    for k, (before, after, lines) in enumerate(seen):
        if k < skip:
            continue
        if before[0] != after[0] or before[2] != after[2]:
            fails.append(f"{name}: change {k + 1}: division sizes changed: {before} to {after}")
        if after[1]:
            fails.append(f"{name}: change {k + 1}: {after[1]} second teams in the Premier Division after the change")
        if after[4]:
            fails.append(f"{name}: change {k + 1}: {after[4]} second teams in the First Division whose first team does "
                         f"not play in the Premier Division")
        up = [l for l in lines if "goes up to the Premier Division (a second team)" in l]
        if up:
            fails.append(f"{name}: change {k + 1}: a second team went up to the Premier Division: {up}")
    po, done = [], 0
    for line in log.splitlines():
        if "recomp: fin second: census after:" in line:
            done += 1
        elif done >= skip and "recomp: fin second: promotion play-off:" in line and "(a second team)" in line:
            po.append(line)
    if po:
        fails.append(f"{name}: a second team plays the promotion play-off: {po}")
    return fails, seen


def run_on(program, game, work):
    fails, log = jump("on", program, game, work, {})
    more, seen = common("on", log, 1)
    fails += more
    if not playoffs(log):
        fails.append("on: no promotion play-off seen")
    if "plays the promotion play-off in place of a second team" not in log:
        fails.append("on: no club took a second team's place in the play-off (the test sees no case)")
    if seen:
        before, after, _ = seen[0]
        print(f"on: change 1: before {before}, after {after}")
    return fails


def run_off(program, game, work):
    fails, log = jump("off", program, game, work, {"RECOMP_FIN_SECOND": "off"})
    if not any("(a second team)" in l for l in playoffs(log)):
        fails.append("off: no second team plays the promotion play-off with the original (the test does not see "
                     "the fault)")
    return fails


def run_drop(program, game, work):
    fails, log = jump("drop", program, game, work, {"RECOMP_TEST_FIN_SECOND_DROP": "1"})
    more, seen = common("drop", log, 1)
    fails += more
    if seen:
        before, after, lines = seen[0]
        print(f"drop: change 1: before {before}, after {after}")
        if before[3] < 2:
            fails.append(f"drop: {before[3]} second teams in the First Division before the change, want 2")
        if sum("goes down from the First Division" in l for l in lines) < 2:
            fails.append("drop: fewer than 2 second teams went down from the First Division")
        if not any("stays up in First Division" in l for l in lines):
            fails.append("drop: no club of the table stayed up in a second team's place")
    return fails


def run_old(program, game, work):
    fails, log = jump("old", program, game, work, {"RECOMP_TEST_FIN_SECOND_OLD": "1"}, 2027)
    more, seen = common("old", log, 2, skip=1)
    fails += more
    if len(seen) >= 2:
        if not re.search(r"goes up to the Premier Division \(a second team\)", "\n".join(seen[0][2])):
            fails.append("old: no second team went up as the original in the first change (no old save made)")
        before, after, lines = seen[1]
        print(f"old: change 2: before {before}, after {after}")
        if not before[1]:
            fails.append("old: no second team in the Premier Division before change 2")
        if not any("goes down from the Premier Division" in l for l in lines):
            fails.append("old: no second team went down from the Premier Division")
        if not any("stays up in the Premier Division" in l for l in lines):
            fails.append("old: no club stayed up in the Premier Division in its place")
    return fails


def run_late(program, game, work):
    fails, log = jump("late", program, game, work, {"RECOMP_TEST_FIN_SECOND_LATE": "1"})
    more, seen = common("late", log, 1)
    fails += more
    if "is the stage-1 finalist" not in log:
        fails.append("late: no second team was made the stage-1 finalist (the test sees no case)")
    if "plays the Premier play-off in place of a second team" not in log:
        fails.append("late: no club played stage 3 in a second team's place")
    if not re.search(r"takes status 5 in place of a second team", log):
        fails.append("late: no club took stage 1's place from a second team")
    return fails


def run_hold(program, game, work):
    fails, log = jump("hold", program, game, work, {"RECOMP_TEST_FIN_SECOND_REP": "1", "RECOMP_TEST_FIN_SECOND_DROP": "1"},
                      2019, 2018, "October 2018")
    more, seen = common("hold", log, 1)
    fails += more
    for club, first in (("Helsinki IFK II", "Helsinki IFK"), ("Klubi 04 Helsinki", "HJK Helsinki"),
                        ("Lahti Akatemia", "FC Lahti"), ("SJK Seinäjoki II", "SJK Seinäjoki")):
        if f"recomp: fin second: name: {club}: second team of {first}" not in log:
            fails.append(f"hold: {club} is not read as the second team of {first}")
    firsts = dict(re.findall(r"recomp: fin second: name: (.*): second team of (.*)", log))
    # A club held at the play-off build may still go up at the change when its first team goes up to the Premier
    # Division in that change (the first team then plays above it). The census check of common covers the end state.
    held, n_held, k = set(), 0, 0
    for line in log.splitlines():
        m = re.search(r"recomp: fin second: (.*) may not go up from Finnish Second Division", line)
        if m:
            held.add(m.group(1))
            n_held += 1
        elif CENSUS.search(line) and "census after" in line:
            lines = seen[k][2] if k < len(seen) else []
            for club in held:
                if any(f"{club} goes up to the First Division" in l for l in lines) and not any(
                        f"{firsts.get(club)} goes up to the Premier Division" in l for l in lines):
                    fails.append(f"hold: {club} was held and went up to the First Division, but its first team "
                                 f"{firsts.get(club)} did not go up to the Premier Division")
            held, k = set(), k + 1
    print(f"hold: {n_held} second teams held in the Second Division")
    if not n_held:
        fails.append("hold: no second team was held in the Second Division (its first team below the Premier)")
    if not re.search(r"will go up from the Second Division \(a second team\)", log):
        fails.append("hold: no second team of a Premier Division club was picked (the allowed path is not seen)")
    if "does not play in the Premier Division)" not in log or "takes a place of the First Division" not in log:
        fails.append("hold: no picked second team went down after its first team went down, or no club took the place")
    return fails


def run_names(program, game, work):
    """September 2005: HJK Reserves is a second team; Atlantis Akatemia and KPV juniorit are first teams."""
    fails, log = jump("names", program, game, work, {}, 2005, 2005, "September 2005 Helvete")
    want = {"HJK Reserves": "second team of HJK Helsinki", "Atlantis Akatemia": "first team",
            "KPV juniorit": "first team"}
    for club, what in want.items():
        if f"recomp: fin second: name: {club}: {what}" not in log:
            fails.append(f"names: {club} is not read as: {what}")
    return fails


def run_vps(program, game, work):
    """April 2016: Vaasan Palloseura-Juniorit (Second Division West) is VPS's farm side."""
    fails, log = jump("vps", program, game, work, {}, 2016, 2016, "April 2016")
    more, _ = common("vps", log, 1)
    fails += more
    want = {"Vaasan Palloseura-Juniorit": "second team of Vaasan Palloseura", "FC Jazz Pori": "first team"}
    for club, what in want.items():
        if f"recomp: fin second: name: {club}: {what}" not in log:
            fails.append(f"vps: {club} is not read as: {what}")
    return fails


RUNS = {"on": run_on, "off": run_off, "drop": run_drop, "old": run_old, "late": run_late, "hold": run_hold,
        "names": run_names, "vps": run_vps}


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="fin-second-")
    if "--fix-off" in argv:
        FIX_OFF["RECOMP_FIN_SECOND"] = "off"
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        futures = {n: ex.submit(RUNS[n], program, game, os.path.join(keep, n)) for n in only}
        fails = []
        for n in only:
            got = futures[n].result()
            print(f"{n}: {'PASS' if not got else 'FAIL'}")
            fails += got
    for f in fails:
        print("  " + f)
    print("PASS" if not fails else "FAIL")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
