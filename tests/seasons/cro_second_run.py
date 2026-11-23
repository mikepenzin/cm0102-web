#!/usr/bin/env python3
"""Croatia: reserve sides follow the real rule (item 240, 0.7.2; season_cro_second.c).

Usage:
    python3 tests/seasons/cro_second_run.py <cm0102_recomp> <game folder> [--only on,off,...] [--jobs n]
        [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py with Croatia alone (changing rules off, so the rule
of the start year holds, season_rule_year). The Croatian season change of July (CroA1::start_new_season 0x5149a0)
calls the chain 0x5150f0, then 0x514ed0, which takes the top 4 Croatian clubs by reputation of A Lower Division and
the Third Divisions into the Second Division. RECOMP_CRO_SECOND_LOG=1 logs a census before and after each change,
the clubs held and moved. The runs:
    on      October 2023, 2023 to 1 June 2025 (the change of July 2024, rule of 2023: no reserve side above the third
            tier). RECOMP_TEST_CRO_SECOND_REP=1 gives each reserve side of the list the highest reputation. No reserve
            side goes up, each stays in its division, and the list still sends 4 clubs up (sizes_kept)
    off     the same with RECOMP_CRO_SECOND=off: a reserve side goes up (the test sees the fault)
    allowed March 2019, 2019 to 1 June 2021 (the change of July 2020, rule of 2019). The club moves of the new game
            put Dinamo Zagreb II, Hajduk Split II and Osijek II into the Second Division (as 2019/20): they get the mark
            0xfc during the season, none goes up into the First Division, and the rule keeps them in the Second
            Division (their first teams play the First Division)
    a1      November 2020, 2020 to 1 June 2022 (rule of 2020; the club moves of the new game put the reserve sides into
            the Second Division, as 2020/21) and RECOMP_TEST_CRO_SECOND_UP=1 (before the chain of July 2021 the best
            placed reserve side of each group takes the title or promotion status): the wrap of the chain hands the
            place on, no reserve side goes up into the First Division
    a1-off  the same with RECOMP_CRO_SECOND=off: a reserve side goes up into the First Division
    rule2022 November 2020, 2020 to 1 June 2023, changing rules on: at the change of July 2022 the rule of 2022 holds
            (no reserve side above the third tier), so every reserve side leaves the Second Division (as Dinamo II and
            Osijek II did) and a club stays up in each place
    rule2022-off the same with RECOMP_CRO_SECOND=off: the reserve sides stay in the Second Division
    start2022 October 2022, the new game of 2022 only (the run ends after the start report): the club moves of the
            new game bar every reserve side from the Second Division (barred_at_start, "new game" log lines)
    old     November 2020, 2020 to 1 June 2023, RECOMP_TEST_CRO_SECOND_OLD=1 with _UP=1 and _HOLD=1 (the reserve
            side takes the middle club's points in the First Division, so the table does not send it down): the
            original until the
            change of July 2021 sends a reserve side up into the First Division (an old save, in memory); at the
            change of July 2022 (rule of 2020) it goes to A Lower Division, the next club of the groups takes its
            place, and the fix's moves keep the sizes
sizes_kept counts the fix's own moves: the league sizes of 0.7.2 change the size of the groups at a change by the
original's own path (the switch off shows the same change), so the test does not compare the sizes before and after.
All runs end with code 0 and show no message box of the game, other than the known box of open item 79.
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
ENV = {"RECOMP_CRO_SECOND_LOG": "1"}
# Open item 79 (lead-notes/open-items-0.6.2.md): a side with fewer than 11 players stops init_match 0x6b8973 or
# 0x6b8aca with this box, and the game then waits. October 2021 and October 2022 give it in their first autumn,
# before any Croatian season change and with the switch off as well, so it is not this rule's: the runs that play a
# season use other databases, and a box ends a run at once (stop_on_box).
KNOWN_BOX = re.compile(r"match_eng\.\.cpp 6(12|52)")

CENSUS = re.compile(r"recomp: cro second: census (before|after): First Division (\d+) clubs, (\d+) reserve sides; "
                    r"North (\d+) clubs, (\d+) reserve sides; South (\d+) clubs, (\d+) reserve sides; list (\d+) "
                    r"reserve sides; change into (\d+)/\d+ \(rule of (\d+)\)")
UP2 = re.compile(r"recomp: cro second: (.*) goes up to the Second Division( \(a reserve side\))?$", re.M)
UP1 = re.compile(r"recomp: cro second: (.*) goes up to the First Division( \(a reserve side\))?$", re.M)
STAYS = re.compile(r"recomp: cro second: (.*) stays in (.*)$", re.M)


def jump(name, program, game, work, env, db, year, last, options=None, start_only=False):
    code, _ = rrc.run(program, game, last, year, os.path.join(DB, db), ["Croatia"], [], work, 7200, options,
                      dict(ENV, **env), start_only=start_only,
                      stop_on_box=True)
    fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    known = [b for b in found if KNOWN_BOX.search(b)]
    if known:
        print(f"{name}: known boxes of open item 79 (a side with too few players), not counted: {known}")
    fails += [f"{name}: message box {b}" for b in found if b not in known]
    return fails, log


def changes(log):
    """Each Croatian season change: (census before, census after, lines between). A census: (a1 clubs, a1 reserve,
    north clubs, north reserve, south clubs, south reserve, list reserve, season, rule)."""
    out, cur = [], None
    for line in log.splitlines():
        m = CENSUS.search(line)
        if m and m.group(1) == "before":
            cur = [tuple(map(int, m.groups()[1:])), None, []]
        elif m and cur:
            cur[1] = tuple(map(int, m.groups()[1:]))
            out.append(cur)
            cur = None
        elif cur and "recomp: cro second:" in line:
            cur[2].append(line)
    return out


def one_change(name, log, k=0, season=None, rule=None):
    seen = changes(log)
    if len(seen) <= k:
        return None, [f"{name}: {len(seen)} Croatian season changes seen, want {k + 1}"]
    before, after, lines = seen[k]
    print(f"{name}: change {k + 1}: before {before}, after {after}")
    fails = []
    if season and after[7] != season:
        fails.append(f"{name}: the change is into {after[7]}, want {season}")
    if rule and after[8] != rule:
        fails.append(f"{name}: the rule of {after[8]} holds, want {rule}")
    return (before, after, "\n".join(lines)), fails


def sizes_kept(name, before, after, lines):
    """The fix's own moves keep the sizes. Since the league sizes of 0.7.2 the original's own path changes the size
    of the groups at a change (as the switch off shows), so the test counts the moves of the fix in the change: each
    club the fix takes out of a division (a reserve side sent down, a club of the groups that goes up into the First
    Division in its place) has a club put back (a club of status 3 that stays up, a club of the list that goes up in
    place of a reserve side), and the list of 0x514ed0 still sends its 4 clubs up. The original's call does not
    touch the First Division, so its size stays."""
    count = lambda text: sum(1 for line in lines.splitlines() if text in line)
    down1, in1 = count("goes down from the First Division"), count("goes up to the First Division in place of")
    down2, stay = count("goes down from the Second Division (rule of"), count("stays up in the Second Division")
    in2 = count("goes up to the Second Division in place of a reserve side")
    listed = len(UP2.findall(lines)) - in2
    print(f"{name}: moves of the fix: First out {down1} in {in1}; Second out {down2 + in1} in {stay + in2}; "
          f"the list sends up {listed}")
    fails = []
    if before[0] != after[0]:
        fails.append(f"{name}: the First Division changed size in the change: {before[0]} to {after[0]}")
    if down1 != in1:
        fails.append(f"{name}: {down1} reserve sides left the First Division, {in1} clubs took their places")
    if down2 + in1 != stay + in2:
        fails.append(f"{name}: the fix took {down2 + in1} clubs out of the Second Division and put {stay + in2} back")
    if listed != 4:
        fails.append(f"{name}: the list sent {listed} clubs up to the Second Division, the original sends 4")
    return fails


def run_on(program, game, work, name="on", env=None):
    fails, log = jump(name, program, game, work, dict({"RECOMP_TEST_CRO_SECOND_REP": "1"}, **(env or {})),
                      "October 2023", 2023, 2024)
    got, more = one_change(name, log, 0, 2024, 2023)
    fails += more
    if not got:
        return fails
    before, after, lines = got
    reserves = [n for n, r in UP2.findall(lines) if r]
    ups = UP2.findall(lines)
    if name == "off":
        if not reserves:
            fails.append("off: no reserve side went up with the original's list (the test does not see the fault)")
        return fails
    if reserves:
        fails.append(f"{name}: reserve sides went up to the Second Division: {', '.join(reserves)}")
    if len(ups) < 4:
        fails.append(f"{name}: {len(ups)} clubs went up to the Second Division, want at least 4")
    stays = STAYS.findall(lines)
    if not stays:
        fails.append(f"{name}: no reserve side was held in its division (the test sees no case)")
    if after[1] or after[3] or after[5]:
        fails.append(f"{name}: reserve sides in the First or Second Division after the change: {after}")
    return fails + sizes_kept(name, before, after, lines)


def run_allowed(program, game, work):
    name = "allowed"
    fails, log = jump(name, program, game, work, {"RECOMP_TEST_CRO_SECOND_REP": "1"}, "March 2019", 2019, 2020)
    got, more = one_change(name, log, 0, 2020, 2019)
    fails += more
    if not got:
        return fails
    before, after, lines = got
    marks = sorted(set(re.findall(r"recomp: cro second: (.*) may not go up from Second Division", log)))
    down = re.findall(r"recomp: cro second: (.*) goes down from the Second Division", lines)
    print(f"{name}: marked {marks}, sent down by the rule {down}")
    if not before[3] + before[5]:
        fails.append(f"{name}: no reserve side in the Second Division at the change (the test sees no case)")
    if not marks:
        fails.append(f"{name}: no reserve side got the mark 0xfc in the Second Division")
    if after[1] or [n for n, r in UP1.findall(log) if r]:
        fails.append(f"{name}: a reserve side went up into the First Division")
    if down or STAYS.findall(lines):
        fails.append(f"{name}: the rule of 2019 moved or held reserve sides it allows: {down} {STAYS.findall(lines)}")
    if not after[3] + after[5]:
        fails.append(f"{name}: no reserve side left in the Second Division after the change")
    return fails + sizes_kept(name, before, after, lines)


def run_a1(program, game, work, name="a1"):
    env = {"RECOMP_TEST_CRO_SECOND_UP": "1"}
    if name.endswith("-off"):
        env["RECOMP_CRO_SECOND"] = "off"
    fails, log = jump(name, program, game, work, env, "November 2020", 2020, 2021)
    test = re.findall(r"recomp: cro second: test: (.*) takes status", log)
    up = [n for n, r in UP1.findall(log) if r]
    print(f"{name}: test statuses {test}, reserve sides up to the First Division {up}")
    got, more = one_change(name, log, 0, 2021, 2020)
    fails += more
    if not test:
        fails.append(f"{name}: no reserve side took a promotion status (the test sees no case)")
    if name.endswith("-off"):
        if not up:
            fails.append(f"{name}: no reserve side went up into the First Division (the test does not see the fault)")
        return fails
    if up:
        fails.append(f"{name}: reserve sides went up into the First Division: {up}")
    if "in place of a second team at the season change" not in log:
        fails.append(f"{name}: no place was handed on at the season change")
    if got:
        before, after, lines = got
        if after[1]:
            fails.append(f"{name}: {after[1]} reserve sides in the First Division after the change")
        fails += sizes_kept(name, before, after, lines)
    return fails


def run_rule2022(program, game, work, name="rule2022"):
    env = {"RECOMP_CRO_SECOND": "off"} if name.endswith("-off") else {}
    fails, log = jump(name, program, game, work, env, "November 2020", 2020, 2022, {"changing_rules": "on"})
    got, more = one_change(name, log, 1, 2022, 2022)
    fails += more
    if not got:
        return fails
    before, after, lines = got
    if not before[3] + before[5]:
        fails.append(f"{name}: no reserve side in the Second Division at the change (the test sees no case)")
    if name.endswith("-off"):
        if not after[3] + after[5]:
            fails.append(f"{name}: no reserve side stays in the Second Division (the test does not see the fault)")
        return fails
    if after[3] + after[5] or after[1]:
        fails.append(f"{name}: reserve sides in the First or Second Division after the change: {after}")
    if "goes down from the Second Division (rule of 2022" not in lines:
        fails.append(f"{name}: no reserve side went down by the rule of 2022")
    return fails + sizes_kept(name, before, after, lines)


def run_start2022(program, game, work, name="start2022"):
    env = {"RECOMP_CRO_SECOND": "off"} if name.endswith("-off") else {}
    fails, log = jump(name, program, game, work, env, "October 2022", 2022, 2022, start_only=True)
    barred = re.findall(r"recomp: cro second: new game: (.*) may not play the (First|Second) Division \(rule of "
                        r"(\d+)\)", log)
    print(f"{name}: barred at the new game {barred}")
    if name.endswith("-off"):
        return fails + ([f"{name}: clubs barred with the switch off: {barred}"] if barred else [])
    if not barred:
        fails.append(f"{name}: no reserve side barred at the new game (the test sees no case)")
    if [b for b in barred if b[2] != "2022"]:
        fails.append(f"{name}: a rule other than 2022 held: {barred}")
    return fails


def run_old(program, game, work, name="old"):
    env = {"RECOMP_TEST_CRO_SECOND_OLD": "1", "RECOMP_TEST_CRO_SECOND_UP": "1", "RECOMP_TEST_CRO_SECOND_HOLD": "1"}
    fails, log = jump(name, program, game, work, env, "November 2020", 2020, 2022)
    first, more = one_change(name, log, 0)
    fails += more
    got, more = one_change(name, log, 1, 2022, 2020)
    fails += more
    if not first or not got:
        return fails
    before, after, lines = got
    if not before[1]:
        fails.append(f"{name}: no reserve side in the First Division before change 2 (no old save made)")
    if after[1]:
        fails.append(f"{name}: {after[1]} reserve sides still in the First Division after change 2")
    if "goes down from the First Division" not in lines or "in place of a reserve side" not in lines:
        fails.append(f"{name}: no reserve side went down from the First Division with a club in its place")
    return fails + sizes_kept(name, before, after, lines)


RUNS = {"on": run_on,
        "off": lambda p, g, w: run_on(p, g, w, "off", {"RECOMP_CRO_SECOND": "off"}),
        "allowed": run_allowed,
        "a1": run_a1,
        "a1-off": lambda p, g, w: run_a1(p, g, w, "a1-off"),
        "rule2022": run_rule2022,
        "rule2022-off": lambda p, g, w: run_rule2022(p, g, w, "rule2022-off"),
        "start2022": run_start2022,
        "old": run_old}


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="cro-second-")
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
