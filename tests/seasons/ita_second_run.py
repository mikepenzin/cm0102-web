#!/usr/bin/env python3
"""Italy: a second team never plays in its first team's division or above (open item 117, 0.7.0;
season_ita_second.c).

Usage:
    python3 tests/seasons/ita_second_run.py <cm0102_recomp> <game folder> [--only on,off,old,old3,d2023,d2025]
        [--jobs n] [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py with Italy (and England) on an Update Team
database, so the game plays the Italian season change in the summer (the chain of the Serie C era,
season_divisions_ita.c). RECOMP_ITA_SECOND_LOG=1 logs a line at each change, a census of every second team and its
first team before the change, after the chain and after the regroup, and every move. Test switches:
RECOMP_TEST_ITA_SECOND_TOP=1 puts the first second team of Serie A and of Serie B at the top and that of each Serie C group in the
place that goes up first; RECOMP_TEST_ITA_SECOND_BOTTOM=1 puts it at the bottom of its group;
RECOMP_TEST_ITA_SECOND_DOWN=<clubs> sends those clubs of Serie A or B down; RECOMP_TEST_ITA_SECOND_OLD=<n> plays the
original's moves for the first n changes. The runs:
    on     October 2024, 2024 start, to the end of 2025/26. TOP, and Juventus FC and Atalanta BC go down at the
           change of 2025: Juventus Next Gen and Atalanta U23 may not go up (their first teams now play Serie B) and
           the next club of each group goes up; Milan Futuro goes up to Serie B (AC Milan stays in Serie A)
    off    the same with RECOMP_ITA_SECOND=off: second teams go up into their first team's division (proves that
           the test sees the fault)
    old    an old save, made in memory (no save is written or loaded): TOP, Atalanta BC goes down at each change,
           and the original's moves at the change of 2025, so the second teams reach Serie B (Atalanta U23 with
           Atalanta). At the change of 2026 (rule on) the check is of the state: before it a second team plays in its
           first team's division, after it none is in Serie A, each is below its first team or in the Serie D or no
           division, and the census lists the same second teams
    old3   TOP and the original's moves at the changes of 2025 and 2026, so a second team reaches Serie A. At the
           change of 2027 (TOP puts it first in Serie A) it goes down, and the best placed relegated club stays up
    d2023  October 2022, 2022 start, changing rules on (the rules of each season): BOTTOM; Juventus Next Gen goes down at the change of 2023 and, before
           2025/26, plays no Serie D (no division)
    d2025  October 2024, 2024 start, changing rules on: BOTTOM; the second teams go down at the change of 2025 and play the Serie D
           (also true with the rule off: this run guards the season boundary). With changing rules off the rules
           of the start season hold, so a 2024 start keeps the rule of 2024/25
In every run after each change: no second team in its first team's division or above (rule on), no "no club takes
its place", no "no club comes back" and no "league sizes: fault" line. All runs end with code 0 and show no message
box of the game.
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
TIERS = ["Serie A", "Serie B", "Serie C", "the Serie D", "no division"]
T = "(Serie A|Serie B|Serie C|the Serie D|no division)"
CENSUS = re.compile(r"recomp: ita second: census (before|after|regroup): (.*) in " + T + r", first team (.*) in " + T
                    + "$")
CHANGE = re.compile(r"recomp: ita second: season change, rule (on|off)")
BAD = ("no club takes its place", "no club comes back", "recomp: league sizes: fault")


def jump(name, program, game, work, env, year=2024, last=2025, database="October 2024", options=None):
    code, _ = rrc.run(program, game, last, year, os.path.join(DB, database), ["Italy"], [], work, 7200, options,
                      dict({"RECOMP_ITA_SECOND_LOG": "1"}, **env))
    fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    fails += [f"{name}: message box {b}" for b in found]
    fails += [f"{name}: {l.strip()}" for l in log.splitlines() if any(b in l for b in BAD)]
    return fails, log


class Change:
    def __init__(self, rule):
        self.rule = rule
        self.census = {"before": {}, "after": {}, "regroup": {}}
        self.lines = []

    def final(self):
        """The census after the regroup, else after the chain."""
        return self.census["regroup"] or self.census["after"]


def changes(log):
    """Each Italian season change, from its header line. A census is {second team: (its tier, first team's
    tier)}."""
    out = []
    for line in log.splitlines():
        m = CHANGE.search(line)
        if m:
            out.append(Change(m.group(1) == "on"))
            continue
        if not out or "recomp: ita second:" not in line:
            continue
        m = CENSUS.search(line)
        if m:
            out[-1].census[m.group(1)][m.group(2)] = (TIERS.index(m.group(3)), TIERS.index(m.group(5)))
        else:
            out[-1].lines.append(line)
    return out


def illegal(census):
    """The second teams in their first team's division or above (not the Serie D or no division)."""
    return [f"{n} ({TIERS[t]}, first team {TIERS[f]})" for n, (t, f) in census.items() if t <= f and t <= 2]


def has(change, text):
    return any(text in l and "test:" not in l for l in change.lines)


def legal_after(name, seen):
    fails = []
    for i, c in enumerate(seen):
        for when in ("after", "regroup"):
            bad = illegal(c.census[when]) if c.rule else []
            if bad:
                fails.append(f"{name}: change {i + 1}, {when}: {', '.join(bad)}")
    return fails


def run_on(program, game, work):
    env = {"RECOMP_TEST_ITA_SECOND_TOP": "1", "RECOMP_TEST_ITA_SECOND_DOWN": "Juventus FC,Atalanta BC"}
    fails, log = jump("on", program, game, work, env)
    seen = changes(log)
    if not seen:
        return fails + ["on: no Italian season change seen"]
    c = seen[0]
    after = c.final()
    print(f"on: change 1: before {c.census['before']}, after {after}")
    for n in ("Juventus Next Gen", "Atalanta U23"):
        if not has(c, f"{n} may not go up from Serie C"):
            fails.append(f"on: {n} was not held back in Serie C")
        if after.get(n, (None, None))[1] != 1:
            fails.append(f"on: the first team of {n} is not in Serie B (the test switch)")
    if sum("takes the place in Serie C" in l for l in c.lines) < 2:
        fails.append("on: fewer than 2 clubs took the place of a second team in Serie C")
    if after.get("Milan Futuro", (None,))[0] != 1:
        fails.append(f"on: Milan Futuro is not in Serie B after change 1 (AC Milan in Serie A, so it may go up): "
                     f"{after.get('Milan Futuro')}")
    if has(c, "Milan Futuro may not go up"):
        fails.append("on: Milan Futuro was held back with its first team in Serie A")
    return fails + legal_after("on", seen)


def run_off(program, game, work):
    env = {"RECOMP_TEST_ITA_SECOND_TOP": "1", "RECOMP_TEST_ITA_SECOND_DOWN": "Juventus FC,Atalanta BC",
           "RECOMP_ITA_SECOND": "off"}
    fails, log = jump("off", program, game, work, env)
    seen = changes(log)
    if not seen:
        return fails + ["off: no Italian season change seen"]
    c = seen[0]
    if c.rule or has(c, "may not go up") or has(c, "goes down from"):
        fails.append("off: a second team was held back or moved with the switch off")
    if not illegal(c.final()):
        fails.append("off: no second team in its first team's division after the change (the test does not see "
                     "the fault)")
    print(f"off: change 1 after: {c.final()}")
    return fails


def run_old(program, game, work):
    env = {"RECOMP_TEST_ITA_SECOND_TOP": "1", "RECOMP_TEST_ITA_SECOND_DOWN": "Atalanta BC",
           "RECOMP_TEST_ITA_SECOND_OLD": "1"}
    fails, log = jump("old", program, game, work, env, last=2026)
    seen = changes(log)
    if len(seen) < 2:
        return fails + [f"old: {len(seen)} Italian season changes seen, want 2"]
    c = seen[1]
    before, after = c.census["before"], c.final()
    print(f"old: change 2: before {before}, after {after}")
    if seen[0].rule or not c.rule:
        fails.append("old: the rule is not off at change 1 and on at change 2")
    if not illegal(before):
        fails.append("old: no second team in its first team's division before change 2 (no old save made)")
    # The state after change 2, not the rule's log lines: on a merged build a second team may go down from Serie B on
    # its own results, so the rule writes no line for it. Every second team is below Serie A and below its first
    # team (legal_after), or in the Serie D or no division.
    if [n for n, (t, f) in after.items() if t == 0]:
        fails.append(f"old: a second team in Serie A after change 2: {after}")
    if len(after) != len(before):
        fails.append(f"old: the census after change 2 lists {len(after)} second teams, before it {len(before)}")
    return fails + legal_after("old", seen)


def run_old3(program, game, work):
    env = {"RECOMP_TEST_ITA_SECOND_TOP": "1", "RECOMP_TEST_ITA_SECOND_OLD": "2"}
    fails, log = jump("old3", program, game, work, env, last=2027)
    seen = changes(log)
    if len(seen) < 3:
        return fails + [f"old3: {len(seen)} Italian season changes seen, want 3"]
    c = seen[2]
    before, after = c.census["before"], c.final()
    print(f"old3: change 3: before {before}, after {after}")
    if not [n for n, (t, f) in before.items() if t == 0]:
        fails.append("old3: no second team in Serie A before change 3 (no old save made)")
    if not has(c, "goes down from Serie A"):
        fails.append("old3: no second team went down from Serie A")
    if [n for n, (t, f) in after.items() if t == 0]:
        fails.append(f"old3: a second team in Serie A after change 3: {after}")
    return fails + legal_after("old3", seen)


def run_d(name, year, database, want):
    def run(program, game, work):
        fails, log = jump(name, program, game, work, {"RECOMP_TEST_ITA_SECOND_BOTTOM": "1"}, year=year,
                          last=year + 1, database=database, options={"changing_rules": "on"})
        seen = changes(log)
        if not seen:
            return fails + [f"{name}: no Italian season change seen"]
        c = seen[0]
        after = c.final()
        print(f"{name}: change 1: before {c.census['before']}, after {after}")
        down = [l.split("test: ")[1].split(" ends last")[0] for l in c.lines if " ends last" in l]
        if not down:
            fails.append(f"{name}: no second team was put last (the test sees no case)")
        for n in down:
            if after.get(n, (None,))[0] != TIERS.index(want):
                fails.append(f"{name}: {n} is in {TIERS[after[n][0]] if n in after else '?'} after the change, "
                             f"want {want}")
        return fails + legal_after(name, seen)
    return run


RUNS = {"on": run_on, "off": run_off, "old": run_old, "old3": run_old3,
        "d2023": run_d("d2023", 2022, "October 2022", "no division"),
        "d2025": run_d("d2025", 2024, "October 2024", "the Serie D")}

def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="ita-second-")
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
