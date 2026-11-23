#!/usr/bin/env python3
"""Belgium: U23 sides stay below their first team and out of the First Division, with the relegation shields (open
item 115), and from 2015/16 the Second Division's 16th goes down directly (open item 124); season_bel_second.c.

Usage:
    python3 tests/seasons/bel_second_run.py <cm0102_recomp> <game folder> [--only off,on,parent,old1] [--jobs n]
        [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py with Belgium alone on the October 2024 database
(changing rules off, a season game of 2024/25), from the 2024 start to 1 June 2026, so the game plays the Belgian
season change of summer 2025 (BelFirst::process_promotion_relegation_chain 0x41ee80) and the third tier's
promotion play-offs of May 2025 and May 2026 (BelThird::create_promotion_playoff_stage 0x423880). The test switch
RECOMP_TEST_BEL_POINTS puts the named clubs on top of their table (=+) or at the bottom (=-) by points, so the
original would send the U23 sides up. RECOMP_BEL_SECOND_LOG=1 logs the marks, the moves, the play-off and a
census of the U23 sides at each season change. The runs:
    off     RECOMP_BEL_SECOND=off, the original: a U23 side goes up to the First Division, and the 16th of the
            Second Division plays the play-off (proves that the test sees both faults)
    on      the shipped code: no U23 side goes up to the First Division, KAA Gent B (its first team in the First
            Division) may go up from the Third Division, the 16th goes down directly in both play-offs and a
            third-tier club takes its place, the shield of 2024/25 keeps KRC Genk B (put at the bottom) up with no
            club down in its place and one third-tier promotion dropped, and with no shield in 2025/26 (abolished
            in May 2026) KRC Genk B goes down. To 1 June 2027
    parent  as on, and RECOMP_TEST_BEL_DOWN relegates Club Brugge KV and KAA Gent from the First Division in summer
            2025: Club Brugge KV II goes down from the Second Division and KAA Gent B (on top of the Third
            Division) does not go up to its first team's division
    old1    the state of an old save, made in memory (no save is written or loaded): RECOMP_TEST_BEL_SECOND_OLD=1
            plays as the original until the season change of summer 2025 is done, so U23 sides go up into the
            First Division. To 1 June 2027: at the change of summer 2026 every U23 side leaves the First Division
            and the division sizes stay
All runs end with code 0 and show no message box of the game. Division sizes are the same before and after each
season change.
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
TOP = ["Club Brugge KV II", "RSC Anderlecht B", "KAA Gent B"]
ENV = {"RECOMP_BEL_SECOND_LOG": "1", "RECOMP_TEST_BEL_POINTS": ";".join(f"{n}=+" for n in TOP)}
CENSUS = re.compile(r"recomp: bel second: census (before|after): First (\d+) clubs, (\d+) U23; Second (\d+) clubs, "
                    r"(\d+) U23; Third (\d+) clubs, (\d+) U23; Fourth (\d+) clubs, (\d+) U23")
SIDE = re.compile(r"recomp: bel second: (before|after): (.*) at level (\d), first team (.*) at level (\d)")


def jump(name, program, game, work, env, last=2025):
    code, _ = rrc.run(program, game, last, 2024, os.path.join(DB, "October 2024"), ["Belgium"], [], work, 7200, None,
                      dict(ENV, **env))
    fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    fails += [f"{name}: message box {b}" for b in found]
    return fails, log


def changes(log):
    """Each Belgian season change: (census before, census after, {club: level before}, {club: level after}, lines,
    {club: its first team's level after})."""
    out, cur = [], None
    for line in log.splitlines():
        m = CENSUS.search(line)
        s = SIDE.search(line)
        if s and cur is not None:
            cur[2 if s.group(1) == "before" else 3][s.group(2)] = int(s.group(3))
            if s.group(1) == "after":
                cur[5][s.group(2)] = int(s.group(5))
        elif s and s.group(1) == "before":
            cur = [None, None, {s.group(2): int(s.group(3))}, {}, [], {}]
        elif m and m.group(1) == "before":
            cur = cur or [None, None, {}, {}, [], {}]
            cur[0] = tuple(map(int, m.groups()[1:]))
        elif m and cur:
            cur[1] = tuple(map(int, m.groups()[1:]))
            out.append(cur)
            cur = None
        elif cur and "recomp: bel second:" in line:
            cur[4].append(line)
    return out


def sizes(name, seen):
    fails = []
    for k, (before, after, _b, _a, _l, _f) in enumerate(seen):
        if after is None or [before[i] for i in (0, 2, 4)] != [after[i] for i in (0, 2, 4)]:
            fails.append(f"{name}: change {k + 1}: division sizes changed: {before} to {after}")
    return fails


def playoffs(log):
    direct = re.findall(r"recomp: bel second: (.*) goes down directly \(status (\d+)\), (.*) \((-?\d+) points\) takes",
                        log)
    plays = re.findall(r"recomp: bel second: (.*) \(status (\d+)\) plays the third tier's promotion play-off", log)
    return direct, plays


def run_off(program, game, work):
    fails, log = jump("off", program, game, work, {"RECOMP_BEL_SECOND": "off"})
    seen = changes(log)
    if not seen:
        return fails + ["off: no Belgian season change seen"]
    if seen[0][1] is None or not seen[0][1][1]:
        fails.append(f"off: no U23 side went up to the First Division ({seen[0][1]}; the test does not see the fault)")
    direct, plays = playoffs(log)
    if direct or len(plays) < 2:
        fails.append(f"off: the 16th went down directly {len(direct)} times and played {len(plays)} play-offs")
    return fails


def check_on(name, log, seen):
    fails = sizes(name, seen)
    if not seen:
        return fails + [f"{name}: no Belgian season change seen"]
    before, after, _b, lv, lines, _f = seen[0]
    if after is None:
        return fails + [f"{name}: the first change has no census after it"]
    if after[1]:
        fails.append(f"{name}: {after[1]} U23 sides in the First Division after the change")
    if not any("may not go up from level 2" in l for l in log.splitlines()):
        fails.append(f"{name}: no U23 side of the Second Division was held back (the test sees no case)")
    direct, plays = playoffs(log)
    if len(direct) < 2 or plays:
        fails.append(f"{name}: the 16th went down directly {len(direct)} times (want 2) and played {len(plays)} "
                     f"play-offs (want 0)")
    if any(int(s) != 3 for _c, s, _n, _p in direct):
        fails.append(f"{name}: a 16th did not get status 3: {direct}")
    return fails, lv, lines


def run_on(program, game, work):
    fails, log = jump("on", program, game, work, {"RECOMP_TEST_BEL_POINTS": ENV["RECOMP_TEST_BEL_POINTS"] +
                                                  ";KRC Genk B=-"}, last=2026)
    seen = changes(log)
    got = check_on("on", log, seen)
    if isinstance(got, list):
        return fails + got
    more, lv, lines = got
    fails += more
    if lv.get("KAA Gent B") != 2:
        fails.append(f"on: KAA Gent B (first team in the First Division) is at level {lv.get('KAA Gent B')} after "
                     f"the change, want 2")
    if not re.search(r"after: Beerschot V\.A U23 at level \d, first team Beerschot VA at level 1", log):
        fails.append("on: Beerschot V.A U23 is not linked to Beerschot VA in the First Division (the case is not seen)")
    if any("may not go up into its first team's division" in l and "Beerschot" in l for l in log.splitlines()):
        fails.append("on: Beerschot V.A U23 (first team in the First Division) was held in the Fourth Division")
    # 2024/25 as in real life: the U23 side stays up, no club goes down in its place, one fewer third-tier club
    # goes up (the sizes check above holds the divisions at their counts).
    shield = [l for l in lines if "the shield of 2024/25 keeps KRC Genk B up, no club goes down in its place" in l]
    if not shield or lv.get("KRC Genk B") != 2:
        fails.append(f"on: the shield did not keep KRC Genk B up (level {lv.get('KRC Genk B')})")
    else:
        if not re.search(r"place, .* \(status [05]\) stays in the Third Division", shield[0]):
            fails.append(f"on: no promotion of the Third Division dropped for the shield: {shield[0]}")
    if any("goes down in its place" in l and "no club goes down in its place" not in l for l in lines):
        fails.append("on: a club went down in the U23 side's place in 2024/25")
    if "plays the relegation play-off of level 3" not in log:
        fails.append("on: no barred U23 side played the Third Division's relegation play-off (the test sees no case)")
    # 2025/26: the shield was abolished (May 2026), so KRC Genk B at the bottom goes down.
    if len(seen) < 2 or seen[1][1] is None:
        return fails + [f"on: {len(seen)} Belgian season changes seen, want 2"]
    lv2, lines2 = seen[1][3], seen[1][4]
    if any("the shield of" in l for l in lines2) or lv2.get("KRC Genk B") != 3:
        fails.append(f"on: KRC Genk B (bottom, 2025/26, no shield) is at level {lv2.get('KRC Genk B')}, want 3")
    return fails


def run_parent(program, game, work):
    fails, log = jump("parent", program, game, work, {"RECOMP_TEST_BEL_DOWN": "Club Brugge KV;KAA Gent"})
    seen = changes(log)
    got = check_on("parent", log, seen)
    if isinstance(got, list):
        return fails + got
    more, lv, lines = got
    fails += more
    if lv.get("Club Brugge KV II") != 3 or not any("Club Brugge KV II goes down from the Second Division" in l
                                                  for l in lines):
        fails.append(f"parent: Club Brugge KV II is at level {lv.get('Club Brugge KV II')}, want 3 (down with its "
                     f"first team)")
    if lv.get("KAA Gent B") != 3 or not any(re.search(r"KAA Gent B may not (go up to its first team's division|take "
                                                     r"status [05] at the season change)", l) for l in lines):
        fails.append(f"parent: KAA Gent B is at level {lv.get('KAA Gent B')}, want 3 (held in the Third Division)")
    return fails


def run_old1(program, game, work):
    fails, log = jump("old1", program, game, work, {"RECOMP_TEST_BEL_SECOND_OLD": "1"}, last=2026)
    seen = changes(log)
    fails += sizes("old1", seen)
    if len(seen) < 2:
        return fails + [f"old1: {len(seen)} Belgian season changes seen, want 2"]
    if seen[0][1] is None or not seen[0][1][1]:
        fails.append("old1: no U23 side went up as the original in the first change (no old save made)")
    before, after, lv_b, lv_a, _l, first_a = seen[1]
    if after is None:
        return fails + ["old1: change 2 has no census after it"]
    print(f"old1: change 2: before {before}, after {after}")
    if not before[1]:
        fails.append("old1: no U23 side in the First Division before change 2 (the test sees no case)")
    # The state after the change, not one log line: a U23 side may go down on its own results or by the rule.
    if after[1]:
        fails.append(f"old1: {after[1]} U23 sides still in the First Division after change 2")
    if not lv_a:
        fails.append("old1: change 2 lists no U23 side after it")
    for club, level in lv_a.items():
        first = first_a.get(club)
        if level == 1:
            fails.append(f"old1: {club} is in the First Division after change 2")
        elif first is not None and level <= first and not (lv_b.get(club) == 1 and level == first == 2):
            # Accepted effect: a side sent down from the First Division shares the Second with its first team.
            fails.append(f"old1: {club} at level {level} is not below its first team (level {first}) after change 2")
    return fails


RUNS = {"off": run_off, "on": run_on, "parent": run_parent, "old1": run_old1}


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="bel-second-")
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
