#!/usr/bin/env python3
"""Denmark: no second team in the national tournament (open item 119, 0.7.0; season_den_second.c).

Usage:
    python3 tests/seasons/den_second_run.py <cm0102_recomp> <game folder> [--only on,off,y2003,y2007,old,mark] [--jobs n]
        [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py with Denmark alone on the October 2024 database
(changing rules on: the rule acts only there), from the 2024 start, so the game plays the Danish season change of
summer 2025 (DenPrm::start_new_season 0x553aa0, which calls the chain and then 0x553cb0). The test switch
RECOMP_TEST_DEN_SECOND_REP=1 gives each second team of A Lower Division (Aalborg BK II, Aarhus Fremad II, Hobro IK II,
"Naestved IF (Naestved BK II)") the highest reputation, so the original would send some of them up into the 2nd
Division. RECOMP_DEN_SECOND_LOG=1 logs the clubs held, moved and a census of each change. The runs:
    on     the shipped code (rule of 2025/26, Herre-DM § 7.1): no second team goes up into the 2nd Division, four
           other clubs go up, and all four second teams stay in A Lower Division
    off    RECOMP_DEN_SECOND=off, the original's list: a second team goes up (proves that the test sees the fault)
    y2003  RECOMP_TEST_DEN_SECOND_YEAR=2003, the rule of 2001/02 to 2004/05 (no second team played the 2nd Division,
           DBU Årsberetning 2002 to 2005): as on, all four second teams are held and four other clubs go up
    y2007  RECOMP_TEST_DEN_SECOND_YEAR=2007, the rule of 2005/06 to 2009/10: Aalborg BK II (Aalborg BK plays the
           Superliga) may go up into the 2nd Division and is not held; the three others are held
    mark   as old with the marks of the season on (each second team below the Superliga gets 0xfc in 2025/26, so no
           place is handed on at the change) and RECOMP_TEST_DEN_SECOND_NO_STAY=1, so clubs of A Lower Division take
           the places that second teams leave in the 2nd Division
    old    the state of a long game of an older build, made in memory (no save is written or loaded):
           RECOMP_TEST_DEN_SECOND_OLD=1 plays the change of 2025 as the original (which sends some second teams up
           into the 2nd Division), then RECOMP_TEST_DEN_SECOND_PLACE=1 puts the second teams left in A Lower Division
           into the Superliga, the 1st and the 2nd Division in turn in exchange for the weakest club of each, and RECOMP_TEST_DEN_SECOND_NO_MARK=1 lets the 2025/26 tables give them places as the original
           (as a save made after the table was complete). At the change of 2026 every second team leaves the three
           divisions, no division changes its size, and no second team goes up
All runs end with code 0 and show no message box of the game.
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
ENV = {"RECOMP_TEST_DEN_SECOND_REP": "1", "RECOMP_DEN_SECOND_LOG": "1"}
SECONDS = {"Aalborg BK II", "Aarhus Fremad II", "Hobro IK II", "Naestved IF (Naestved BK II)"}
CENSUS = re.compile(r"recomp: den second: census (before|after): Superliga (\d+) clubs, (\d+) second teams; "
                    r"1st Division (\d+) clubs, (\d+) second teams; 2nd Division (\d+) clubs, (\d+) second teams; "
                    r"change into (\d+)/\d+ \(rule of (\d+)\)")
NEXT = re.compile(r"recomp: den second: next club: (.*), place (\d+) \(above it (\d+) left the division, (\d+) go down, "
                  r"(\d+) are barred\)")


def jump(name, program, game, work, env, last=2025):
    code, _ = rrc.run(program, game, last, 2024, os.path.join(DB, "October 2024"), ["Denmark"], [], work, 7200,
                      {"changing_rules": "on"}, dict(ENV, **env))
    fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    fails += [f"{name}: message box {b}" for b in found]
    return fails, log


def changes(log):
    """The log of each Danish season change: (census before, census after, lines between)."""
    out, cur = [], None
    for line in log.splitlines():
        m = CENSUS.search(line)
        if m and m.group(1) == "before":
            cur = [tuple(map(int, m.groups()[1:7])), None, [], tuple(map(int, m.groups()[7:]))]
        elif m and cur:
            cur[1] = tuple(map(int, m.groups()[1:7]))
            if tuple(map(int, m.groups()[7:])) != cur[3]:
                cur[2].append(f"census years differ: {cur[3]} and {m.groups()[7:]}")
            out.append(cur)
            cur = None
        elif cur and "recomp: den second:" in line:
            cur[2].append(line)
    return out


def ups(lines):
    return re.findall(r"recomp: den second: (.*) goes up to the 2nd Division( \(a second team\))?$", "\n".join(lines),
                      re.M)


def held(lines):
    return {m for m in re.findall(r"recomp: den second: (.*) stays in A Lower Division$", "\n".join(lines), re.M)}


def check_year(name, n, change, rule, fails):
    """The census of change n gives the season that starts (2025 + n - 1) and the rule's season."""
    want = (2024 + n, rule or 2024 + n)
    if change[3] != want:
        fails.append(f"{name}: change {n} is into {change[3][0]} with the rule of {change[3][1]}, want {want}")


def check_next(name, lines, fails):
    """Each place goes to the best placed club still in the division below that may take it."""
    for line in lines:
        m = NEXT.search(line)
        if m:
            print(f"{name}: {line.split('den second: ')[1]}")
            if int(m.group(4)):
                fails.append(f"{name}: a club going down was passed over for {m.group(1)}")
            if int(m.group(2)) != 1 + int(m.group(3)) + int(m.group(4)) + int(m.group(5)):
                fails.append(f"{name}: {m.group(1)} is not the best placed club that may take the place")


def first_change(name, log, fails, rule=None):
    seen = changes(log)
    if not seen:
        fails.append(f"{name}: no Danish season change seen")
        return None
    before, after, lines, _ = seen[0]
    print(f"{name}: change 1: before {before}, after {after}")
    check_year(name, 1, seen[0], rule, fails)
    if after[0::2] != before[0::2]:
        fails.append(f"{name}: division sizes changed: {before} to {after}")
    return lines


def run_on(program, game, work):
    fails, log = jump("on", program, game, work, {})
    lines = first_change("on", log, fails)
    if lines is None:
        return fails
    up = ups(lines)
    if [n for n, s in up if s]:
        fails.append(f"on: second teams went up: {[n for n, s in up if s]}")
    if len(up) != 4:
        fails.append(f"on: {len(up)} clubs went up to the 2nd Division, want 4")
    if held(lines) != SECONDS:
        fails.append(f"on: held {sorted(held(lines))}, want {sorted(SECONDS)}")
    return fails


def run_off(program, game, work):
    fails, log = jump("off", program, game, work, {"RECOMP_DEN_SECOND": "off"})
    lines = first_change("off", log, fails)
    if lines is None:
        return fails
    if held(lines):
        fails.append(f"off: {len(held(lines))} second teams held with the switch off")
    if not [n for n, s in ups(lines) if s]:
        fails.append("off: no second team went up with the original's list (the test does not see the fault)")
    return fails


def run_y2003(program, game, work):
    fails, log = jump("y2003", program, game, work, {"RECOMP_TEST_DEN_SECOND_YEAR": "2003"})
    lines = first_change("y2003", log, fails, rule=2003)
    if lines is None:
        return fails
    up = ups(lines)
    if [n for n, s in up if s]:
        fails.append(f"y2003: second teams went up: {[n for n, s in up if s]}")
    if len(up) != 4:
        fails.append(f"y2003: {len(up)} clubs went up to the 2nd Division, want 4")
    if held(lines) != SECONDS:
        fails.append(f"y2003: held {sorted(held(lines))}, want {sorted(SECONDS)}")
    return fails


def run_y2007(program, game, work):
    fails, log = jump("y2007", program, game, work, {"RECOMP_TEST_DEN_SECOND_YEAR": "2007"})
    lines = first_change("y2007", log, fails, rule=2007)
    if lines is None:
        return fails
    # The first teams the rule reads (exact name, or the pair table for Naestved).
    links = dict(re.findall(r"recomp: den second: first team of (.*?): (.*)$", "\n".join(lines), re.M))
    print(f"y2007: first teams {links}")
    want_links = {"Aalborg BK II": "Aalborg BK", "Aarhus Fremad II": "Aarhus Fremad", "Hobro IK II": "Hobro IK",
                  "Naestved IF (Naestved BK II)": "Naestved Boldklub"}
    for team, first in want_links.items():
        if not links.get(team, "").startswith(first + " ("):
            fails.append(f"y2007: first team of {team} read as {links.get(team)}, want {first}")
    # A second team may play the 2nd Division only while its first team plays the Superliga.
    want = {t for t in SECONDS if not links.get(t, "").endswith("(the Superliga)")}
    if "Aalborg BK II" in want:
        print("y2007: note: Aalborg BK left the Superliga in 2024/25, so its second team is held too")
    if held(lines) != want:
        fails.append(f"y2007: held {sorted(held(lines))}, want {sorted(want)}")
    bad = [n for n, s in ups(lines) if s and n in want]
    if bad:
        fails.append(f"y2007: barred second teams went up: {bad}")
    if len(ups(lines)) != 4:
        fails.append(f"y2007: {len(ups(lines))} clubs went up to the 2nd Division, want 4")
    return fails


def run_old(program, game, work):
    env = {"RECOMP_TEST_DEN_SECOND_OLD": "1", "RECOMP_TEST_DEN_SECOND_PLACE": "1",
           "RECOMP_TEST_DEN_SECOND_NO_MARK": "1"}
    fails, log = jump("old", program, game, work, env, last=2026)
    # The original's change of 2025 sends some second teams up into the 2nd Division by itself, so the switch may
    # find fewer than three in A Lower Division to place. The Superliga and the 1st Division need one each.
    placed = re.findall(r"recomp: den second: test: (.*) placed in (.*) for", log)
    if not {"the Superliga", "the 1st Division"} <= {w for _, w in placed}:
        fails.append(f"old: placed {placed}, want one in the Superliga and one in the 1st Division (no old save made)")
    seen = changes(log)
    if len(seen) < 2:
        return fails + [f"old: {len(seen)} Danish season changes seen, want 2"]
    before, after, lines, _ = seen[1]
    check_year("old", 2, seen[1], None, fails)
    check_next("old", lines, fails)
    if not any(NEXT.search(l) for l in lines):
        fails.append("old: no club took a place of a second team in a division above")
    print(f"old: change 2: before {before}, after {after} (clubs, second teams of the Superliga, 1st, 2nd Division)")
    if before[1] + before[3] + before[5] == 0:
        fails.append("old: no second team in the three divisions before change 2 (the test sees no case)")
    for k, what in ((1, "the Superliga"), (3, "the 1st Division"), (5, "the 2nd Division")):
        if before[k] == 0:
            fails.append(f"old: no second team in {what} before change 2 (that case is not seen)")
        if after[k]:
            fails.append(f"old: {after[k]} second teams still in {what} after change 2")
    if after[0::2] != before[0::2]:
        fails.append(f"old: division sizes changed: {before} to {after}")
    if [n for n, s in ups(lines) if s]:
        fails.append(f"old: second teams went up: {[n for n, s in ups(lines) if s]}")
    return fails


def run_mark(program, game, work):
    """As old, but with the marks of the season on and no club of status 3 staying up."""
    env = {"RECOMP_TEST_DEN_SECOND_OLD": "1", "RECOMP_TEST_DEN_SECOND_PLACE": "1", "RECOMP_TEST_DEN_SECOND_NO_STAY": "1"}
    fails, log = jump("mark", program, game, work, env, last=2026)
    placed = [n for n, _ in re.findall(r"recomp: den second: test: (.*) placed in (.*) for", log)]
    seen = changes(log)
    if len(seen) < 2 or not placed:
        return fails + [f"mark: {len(seen)} Danish season changes seen and {len(placed)} second teams placed"]
    between = log.split("census after", 1)[1].split("census before", 1)[0]
    marked = set(re.findall(r"recomp: den second: (.*) may not go up from the (?:1st|2nd) Division", between))
    print(f"mark: marked during 2025/26: {sorted(marked)}")
    lower = [n for n, w in re.findall(r"recomp: den second: test: (.*) placed in (.*) for", log) if w != "the Superliga"]
    if not lower or not set(lower) <= marked:
        fails.append(f"mark: placed {lower} below the Superliga, marked {sorted(marked)}")
    before, after, lines, _ = seen[1]
    print(f"mark: change 2: before {before}, after {after}")
    check_year("mark", 2, seen[1], None, fails)
    check_next("mark", lines, fails)
    shifted = [l for l in lines if "in place of a second team at the season change" in l]
    if shifted:
        fails.append(f"mark: places handed on at the change although the marks were on: {shifted}")
    if [l for l in lines if "stays up in the 2nd Division" in l]:
        fails.append("mark: a club stayed up with RECOMP_TEST_DEN_SECOND_NO_STAY=1")
    filled = [l for l in lines if "goes up to the 2nd Division in place of a second team" in l]
    if not filled:
        fails.append("mark: no club of A Lower Division took a place of a second team (the fill is not seen)")
    for k in (1, 3, 5):
        if after[k]:
            fails.append(f"mark: {after[k]} second teams left in a division after change 2: {after}")
    if after[0::2] != before[0::2]:
        fails.append(f"mark: division sizes changed: {before} to {after}")
    return fails


RUNS = {"on": run_on, "off": run_off, "y2003": run_y2003, "y2007": run_y2007, "old": run_old, "mark": run_mark}


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="den-second-")
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
