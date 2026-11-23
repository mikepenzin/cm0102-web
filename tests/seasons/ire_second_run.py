#!/usr/bin/env python3
"""Republic of Ireland: a reserve side does not go up from the First Division (open item 241, 0.7.2;
season_ire_second.c).

Usage:
    python3 tests/seasons/ire_second_run.py <cm0102_recomp> <game folder> [--only on,off,...] [--jobs n]
        [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py with the Republic of Ireland alone on the March 2014
database (changing rules off, so the rule of the start year, League of Ireland 2014, holds in each season), whose
First Division holds Shamrock Rovers B, from the 2013 start to 1 June 2016 (two Irish season changes). In League of
Ireland 2014 Shamrock Rovers B was not eligible for promotion. With the league sizes of 0.7.2 the game's First
Division has 8 clubs, the title (status 5) and the play-off (IreFirst::create_promotion_playoff_stage 0x635dc0, the
First Division row of status 1). The chain (0x689c20, rows in pairs) sends the club of the title up at a change where
the Premier Division has rows of status 3. With the league sizes of lead-072-base c2d8b0db that is already the first
change (2013/14), so the runs of the original's path (off, free) need a reserve side in the First Division before the
first change only. The test switch RECOMP_TEST_IRE_SECOND_TOP=1 gives each reserve side of the First Division 300 more points
before each update, so it takes the title in both seasons. RECOMP_IRE_SECOND_LOG=1 logs the marks, the hand-ons, the
club of the play-off, the places up at the season change, the clubs that change division and a census before and
after each change (IrePrm::start_new_season 0x63a9b0). The runs:
    on      the shipped code: Shamrock Rovers B gets the mark, another club takes the title, no reserve side holds a
            place up at a change, the play-off takes no reserve side and none plays the Premier Division after a change
    off     RECOMP_IRE_SECOND=off, the original: Shamrock Rovers B holds the title and goes up to the Premier Division
            at a change (proves that the test sees the fault)
    free    the shipped code with RECOMP_TEST_IRE_SECOND_YEAR=2014 (a season with no source, the game kept): the
            reserve side goes up as in the original
    nomark  RECOMP_TEST_IRE_SECOND_NO_MARK=1, as a save made after the table was complete: the reserve side holds the
            title, the wrap of 0x635dc0 hands it on, and no reserve side goes up
    old     RECOMP_TEST_IRE_SECOND_TOP=2, RECOMP_TEST_IRE_SECOND_OLD=1 and RECOMP_TEST_IRE_SECOND_PLACE=1: the first
            change plays as the original and the reserve side goes up in place of the last club, as an old save; at
            the second change (2014/15, three places down) the reserve side goes down and a club of status 3 stays up
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
ENV = {"RECOMP_TEST_IRE_SECOND_TOP": "1", "RECOMP_IRE_SECOND_LOG": "1"}
CENSUS = re.compile(r"recomp: ire second: census (before|after) \(season (\d+), rule (bars|keeps the game)\): Premier "
                    r"Division (\d+) clubs, (\d+) reserve sides; First Division (\d+) clubs, (\d+) reserve sides")
PLAYOFF = re.compile(r"recomp: ire second: the play-off takes (.*) of the First Division( \(a reserve side\))?$", re.M)
UP = re.compile(r"recomp: ire second: (.*) goes up to the Premier Division \(a reserve side\)$", re.M)
PLACE = re.compile(r"recomp: ire second: the First Division position (\d+): (.*) status (-?\d+)( \(a reserve side\))?$",
                   re.M)


def jump(name, program, game, work, env, last=2015):
    code, _ = rrc.run(program, game, last, 2013, os.path.join(DB, "March 2014"), ["Republic of Ireland"], [], work,
                      7200, None, dict(ENV, **env))
    fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    fails += [f"{name}: message box {b}" for b in found]
    return fails, log


def changes(log):
    """Each Irish season change: (census before, census after, lines between)."""
    out, cur = [], None
    for line in log.splitlines():
        m = CENSUS.search(line)
        if m and m.group(1) == "before":
            cur = [m.groups()[1:], None, []]
        elif m and cur:
            cur[1] = m.groups()[1:]
            out.append(cur)
            cur = None
        elif cur and "recomp: ire second:" in line:
            cur[2].append(line)
    return out


def check(name, log, want_reserve):
    """Both changes: the places up, the clubs that go up and the club of each play-off. want_reserve: the original's
    path (a reserve side goes up); else the rule's (none holds a place up, none goes up, none plays the play-off)."""
    seen = changes(log)
    playoff = PLAYOFF.findall(log)
    print(f"{name}: play-offs {playoff}")
    fails = []
    if len(seen) < 2:
        return [f"{name}: {len(seen)} Irish season changes seen, want 2"]
    held, up = [], []
    for k, (before, after, lines) in enumerate(seen[:2]):
        text = "\n".join(lines)
        places = PLACE.findall(text)
        print(f"{name}: change {k + 1}: before {before}, after {after}; places up {places}; reserve sides up "
              f"{UP.findall(text)}")
        if int(before[5]) == 0 and (k == 0 or not want_reserve):
            fails.append(f"{name}: no reserve side in the First Division before change {k + 1} (the test sees no case)")
        held += [p[1] for p in places if p[3] and p[2] in ("5", "0", "1")]
        up += UP.findall(text)
        if not want_reserve and int(after[3]):
            fails.append(f"{name}: {after[3]} reserve sides in the Premier Division after change {k + 1}")
    if len(playoff) < 2:
        fails.append(f"{name}: {len(playoff)} play-offs of the First Division seen, want 2")
    reserve = [p[0] for p in playoff if p[1]]
    if want_reserve:
        if not up or not int(seen[1][1][3]):
            fails.append(f"{name}: no reserve side went up to the Premier Division (the test does not see the fault)")
        return fails
    if reserve:
        fails.append(f"{name}: the play-off took a reserve side: {reserve}")
    if held:
        fails.append(f"{name}: reserve sides hold a place up at a change: {held}")
    if up:
        fails.append(f"{name}: reserve sides went up to the Premier Division: {up}")
    return fails


def run_on(program, game, work):
    fails, log = jump("on", program, game, work, {})
    fails += check("on", log, False)
    if "may not go up from the First Division" not in log:
        fails.append("on: Shamrock Rovers B never got the mark")
    if "rule bars" not in log:
        fails.append("on: the census does not read the rule of 2013 (League of Ireland 2014)")
    return fails


def run_off(program, game, work):
    fails, log = jump("off", program, game, work, {"RECOMP_IRE_SECOND": "off"})
    fails += check("off", log, True)
    if "may not go up" in log or "in place of a second team" in log:
        fails.append("off: the rule acted with the switch off")
    return fails


def run_free(program, game, work):
    fails, log = jump("free", program, game, work, {"RECOMP_TEST_IRE_SECOND_YEAR": "2014"})
    fails += check("free", log, True)
    if "may not go up" in log or "rule keeps the game" not in log:
        fails.append("free: the rule acted in a season with no source")
    return fails


def run_nomark(program, game, work):
    fails, log = jump("nomark", program, game, work, {"RECOMP_TEST_IRE_SECOND_NO_MARK": "1"})
    fails += check("nomark", log, False)
    if "in place of a second team before the play-off" not in log:
        fails.append("nomark: no place was handed on before the play-off (the old-save path is not seen)")
    return fails


def run_old(program, game, work):
    fails, log = jump("old", program, game, work, {"RECOMP_TEST_IRE_SECOND_TOP": "2", "RECOMP_TEST_IRE_SECOND_OLD": "1",
                                                   "RECOMP_TEST_IRE_SECOND_PLACE": "1"})
    seen = changes(log)
    if len(seen) < 2:
        return fails + [f"old: {len(seen)} Irish season changes seen, want 2"]
    before, after, lines = seen[1]
    print(f"old: change 1 after {seen[0][1]}; change 2: before {before}, after {after}")
    if "test: Shamrock Rovers B goes up in place of" not in log:
        fails.append("old: the reserve side was not placed in the Premier Division (no old save made)")
    if int(before[3]) == 0:
        fails.append("old: no reserve side in the Premier Division before change 2 (the test sees no case)")
    if int(after[3]):
        fails.append(f"old: {after[3]} reserve sides still in the Premier Division after change 2")
    if before[2] != after[2]:
        fails.append(f"old: the Premier Division size changed: {before[2]} to {after[2]}")
    if not any("goes down from the Premier Division, " in line for line in lines):
        fails.append("old: no reserve side went down in exchange for a club that stays up")
    return fails


RUNS = {"on": run_on, "off": run_off, "free": run_free, "nomark": run_nomark, "old": run_old}


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="ire-second-")
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
