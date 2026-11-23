#!/usr/bin/env python3
"""Norway: a second team does not go up into the First Division (open item 111, 0.7.0; season_nor_second.c).

Usage:
    python3 tests/seasons/nor_second_run.py <cm0102_recomp> <game folder> [--only on,off,old1,old2] [--jobs n]
        [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py with Norway alone on the October 2024 database
(changing rules off), from the 2024 start to 1 June 2025, so the game plays the Norwegian season change of
January 2025 (NorPrm::start_new_season 0x7927a0, which calls 0x792960). The test switch
RECOMP_TEST_NOR_SECOND_REP=1 gives each second team of the eight groups of the Second Division the highest
reputation, so the original would send four of them up. RECOMP_NOR_SECOND_LOG=1 logs the clubs that stay out and
the clubs that go up. The runs:
    on    the shipped code: no second team goes up, four other clubs go up, and at least one second team stays
          (each one back in its group of the Second Division after the call)
    off   RECOMP_NOR_SECOND=off, the original's list: second teams go up (proves that the test sees the fault)
    old1  (with old2: RECOMP_TEST_NOR_SECOND_UP=1 gives the best placed second team of the First Division a
          promotion place at each change of the original's part, and the play-off place before the play-off, so
          the cases come about with any results; from the base of 0.7.2 the results of 2025 gave none)
          the state of an old save, made in memory (no save is written or loaded): RECOMP_TEST_NOR_SECOND_OLD=1
          plays as the original until the season change of January 2025 is done, so second teams go up into the
          First Division, and RECOMP_TEST_NOR_SECOND_NO_MARK=1 lets its 2025 table give them places as the original
          (as a save made after the table was complete). To 1 June 2026: the wrap of the play-off hands a place on,
          at the change of January 2026 every second team goes down, the First Division keeps its size, and no
          second team goes up
    old2  the same (with the marks) and the original until January 2026, to 1 June 2027: the 2026 season plays with the fix, and
          at the change of January 2027 every second team leaves the First Division and those of Eliteserien go down
          (one for each club of Eliteserien that stays up)
    parent the shipped code, with RECOMP_TEST_NOR_SECOND_DOWN naming First Division clubs whose second teams play the
          Second Division (Kongsvinger IL, Raufoss IL): their rows get status 3 before the change of January 2025, so
          they come down into the Second Division. Item 232, § 2-7 (1) (a): a second team plays at most in the
          division below its club's higher team, so after the change no second team plays the Second Division above
          what its club allows (the database also has Grorud IL 2, IL Hødd 2 and Skeid Oslo II there with their first
          teams), each one goes to the Third Division, and every second team of the groups finds its first team
          The season that starts is 2025, so the rule from 2015 holds: no second team of a First Division club goes
    parent-off the same with RECOMP_NOR_SECOND_PAIRS=off (item 232's step alone off): barred second teams stay
          (proves that the test sees the fault)
    parent2014 the same with RECOMP_TEST_NOR_SECOND_YEAR=2014 (the last season of the old rule): the second teams of
          First Division clubs go down too, those of Eliteserien clubs stay
    parent-rules the same as parent with changing rules on: the rule of the season that starts (2025) holds; with
          changing rules off (the other runs) that of the start year (season_rule_year)
    rule2014 September 2005 Helvete, a 2005 start to 1 June 2007 (the changes of January 2006 and January 2007, the
          rule to 2014): the database has Stabæk IF B in the groups with Stabæk IF in the First Division, so the second
          team of a First Division club leaves the Second Division, and those of Eliteserien clubs stay. Until the
          league sizes of 0.7.2 the run used September 2008, the database of the forum report; there the game's
          Eliteserien of 2008 and 2009 has 14 clubs and no place down (the rules table gives 16 clubs), so no club
          comes down into the First Division and no First Division club has a second team in the groups
    rule2014-off the same with RECOMP_NOR_SECOND_PAIRS=off: barred second teams stay
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
ENV = {"RECOMP_TEST_NOR_SECOND_REP": "1", "RECOMP_NOR_SECOND_LOG": "1"}


def jump(name, program, game, work, env, last=2024, year=2024, db="October 2024", options=None):
    code, _ = rrc.run(program, game, last, year, os.path.join(DB, db), ["Norway"], [], work, 7200, options,
                      dict(ENV, **env))
    fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    fails += [f"{name}: message box {b}" for b in found]
    up = re.findall(r"recomp: nor second: (.*) goes up to the First Division( \(a second team\))?", log)
    stays = re.findall(r"recomp: nor second: (.*) stays in (?!Eliteserien one more season)(.*)", log)
    lost = [n for n, d in stays if not d.startswith("Norwegian Second Division Group")]
    if lost:
        fails.append(f"{name}: second teams held back lost their group: {', '.join(lost)}")
    return fails, up, stays, log


CENSUS = re.compile(r"recomp: nor second: census (before|after): Eliteserien (\d+) clubs, (\d+) second teams; "
                    r"First Division (\d+) clubs, (\d+) second teams")


def changes(log):
    """The log of each Norwegian season change: (census before, census after, lines between)."""
    out, cur = [], None
    for line in log.splitlines():
        m = CENSUS.search(line)
        if m and m.group(1) == "before":
            cur = [tuple(map(int, m.groups()[1:])), None, []]
        elif m and cur:
            cur[1] = tuple(map(int, m.groups()[1:]))
            out.append(cur)
            cur = None
        elif cur and "recomp: nor second:" in line:
            cur[2].append(line)
    return out


def run_old(name, program, game, work, n, last, no_mark=False):
    env = {"RECOMP_TEST_NOR_SECOND_OLD": str(n), "RECOMP_TEST_NOR_SECOND_UP": "1",
           **({"RECOMP_TEST_NOR_SECOND_NO_MARK": "1"} if no_mark else {})}
    fails, _, _, log = jump(name, program, game, work, env, last)
    seen = changes(log)
    if len(seen) < n + 1:
        return fails + [f"{name}: {len(seen)} Norwegian season changes seen, want {n + 1}"]
    if not any("goes up to the First Division (a second team)" in l for l in seen[0][2]):
        fails.append(f"{name}: no second team went up as the original in the first change (no old save made)")
    before, after, lines = seen[n]
    print(f"{name}: change {n + 1}: before {before}, after {after} (Eliteserien clubs, second teams; First clubs, "
          f"second teams)")
    if before[3] == 0:
        fails.append(f"{name}: no second team in the First Division before change {n + 1} (the test sees no case)")
    if after[3]:
        fails.append(f"{name}: {after[3]} second teams still in the First Division after change {n + 1}")
    if after[2] != before[2] or after[0] != before[0]:
        fails.append(f"{name}: division sizes changed: {before} to {after}")
    if after[1] > before[1] or (before[1] and after[1] == before[1]):
        fails.append(f"{name}: Eliteserien second teams {before[1]} before, {after[1]} after")
    went = [l for l in lines if "(a second team)" in l]
    if went:
        fails.append(f"{name}: second teams went up: {went}")
    if no_mark and not any("in place of a second team before the play-off" in l for l in log.splitlines()):
        fails.append(f"{name}: no place was handed on before the play-off (the old-save path is not seen)")
    if not any("goes down from the First Division" in l for l in lines):
        fails.append(f"{name}: no second team went down from the First Division")
    if n == 2 and not before[1]:
        fails.append(f"{name}: no second team in Eliteserien before change 3 (the Eliteserien case is not seen)")
    return fails


def run_on(program, game, work):
    fails, up, stays, _ = jump("on", program, game, work, {})
    seconds = [n for n, s in up if s]
    if seconds:
        fails.append(f"on: second teams went up to the First Division: {', '.join(seconds)}")
    if len(up) != 4:
        fails.append(f"on: {len(up)} clubs went up to the First Division, want 4")
    if not stays:
        fails.append("on: no second team stayed in the Second Division (no season change seen)")
    return fails


def run_off(program, game, work):
    fails, up, stays, _ = jump("off", program, game, work, {"RECOMP_NOR_SECOND": "off"})
    if stays:
        fails.append(f"off: {len(stays)} second teams were held back with the switch off")
    if not [n for n, s in up if s]:
        fails.append("off: no second team went up with the original's list (the test does not see the fault)")
    return fails


PAIR = re.compile(r"recomp: nor second: pairs (before|after) \(season (\d+), rule of (\d+)\): (\d+) second teams of "
                  r"the groups, (\d+) with no first team, (\d+) barred there")


def pairs_of(log):
    return {m.group(1): tuple(map(int, m.groups()[1:])) for m in PAIR.finditer(log)}


DOWN = re.compile(r"recomp: nor second: (.*) goes down to the Third Division \(rule of (\d+): (.*) plays (Eliteserien|"
                  r"the First Division|the Second Division|a lower division), (.*?)(, a third team)?\)")
MAY = re.compile(r"recomp: nor second: (.*) may play the Second Division \(rule of (\d+): (.*)\)")


def run_parent(name, program, game, work, env, year=2024, rules="off"):
    """year: the rule that holds (the start year 2024 with changing rules off, 2025 with them on, or the test year)."""
    env = dict(env, RECOMP_TEST_NOR_SECOND_DOWN="Kongsvinger IL,Raufoss IL")
    if year not in (2024, 2025):
        env["RECOMP_TEST_NOR_SECOND_YEAR"] = str(year)
    fails, _, _, log = jump(name, program, game, work, env, options={"changing_rules": rules})
    pairs = pairs_of(log)
    down = DOWN.findall(log)
    may = MAY.findall(log)
    forced = re.findall(r"recomp: nor second: test: (.*) goes down", log)
    print(f"{name}: pairs {pairs}, forced {forced}, to the Third Division: {[(d[0], d[2], d[3]) for d in down]}, "
          f"may stay: {[(m[0], m[2]) for m in may]}")
    if len(forced) != 2:
        fails.append(f"{name}: the test switch sent {len(forced)} First Division clubs down, want 2")
    if "after" not in pairs or "before" not in pairs:
        return fails + [f"{name}: no pair census at the change"]
    if pairs["after"][0] != 2025 or pairs["after"][1] != year:
        fails.append(f"{name}: the change reads season {pairs['after'][0]}, rule of {pairs['after'][1]}, want 2025 and "
                     f"{year}")
    if name == "parent-off":
        if down:
            fails.append(f"{name}: second teams moved with the switch off: {down}")
        if not pairs["after"][4]:
            fails.append(f"{name}: no second team is barred after the change (the test does not see the fault)")
        return fails
    if pairs["after"][4]:
        fails.append(f"{name}: {pairs['after'][4]} second teams still barred from the Second Division")
    if pairs["after"][3] or pairs["before"][3]:
        fails.append(f"{name}: second teams of the groups with no first team: {pairs}")
    names = [d[0] for d in down]
    for want in ("Kongsvinger IL II", "Raufoss IL 2"):
        if want not in names:
            fails.append(f"{name}: {want} did not go to the Third Division")
    allowed = ("Eliteserien",) if year <= 2014 else ("Eliteserien", "the First Division")
    wrong = [d[0] for d in down if d[3] in allowed and not d[5]]
    if wrong:
        fails.append(f"{name}: second teams of clubs that the rule of {year} allows went down: {wrong}")
    stay_wrong = [m[0] for m in may if m[2] not in allowed]
    if stay_wrong:
        fails.append(f"{name}: second teams that the rule of {year} bars stay: {stay_wrong}")
    if not any(m[2] == "Eliteserien" for m in may):
        fails.append(f"{name}: no second team of an Eliteserien club stays (the test sees no allowed case)")
    if year <= 2014 and not any(d[3] == "the First Division" for d in down):
        fails.append(f"{name}: no second team of a First Division club went down (the rule to 2014)")
    if year >= 2015 and not any(m[2] == "the First Division" for m in may):
        fails.append(f"{name}: no second team of a First Division club stays (the rule from 2015)")
    if len(down) < 4:
        fails.append(f"{name}: {len(down)} second teams went down, want the two of the test and those of the data")
    return fails


def run_rule2014(name, program, game, work, env):
    """September 2005 Helvete, a 2005 start to 1 June 2007: the changes of January 2006 and 2007 (the rule to 2014)."""
    fails, _, _, log = jump(name, program, game, work, env, last=2006, year=2005, db="September 2005 Helvete")
    pairs = [(m.group(1),) + tuple(map(int, m.groups()[1:])) for m in PAIR.finditer(log)]
    down = DOWN.findall(log)
    print(f"{name}: pairs {pairs}, to the Third Division: {[(d[0], d[2], d[3]) for d in down]}")
    after = [p for p in pairs if p[0] == "after"]
    if len(after) < 2 or len(pairs) != 2 * len(after):
        return fails + [f"{name}: {len(after)} pair censuses after a change, want 2 changes"]
    if [p[1] for p in after][:2] != [2006, 2007] or any(p[2] != 2005 for p in pairs):
        fails.append(f"{name}: the changes read seasons {[p[1] for p in after]}, rules {[p[2] for p in pairs]}, want "
                     f"2006 and 2007, rule of 2005 (changing rules off: the rule of the start year)")
    if not any(p[5] for p in pairs if p[0] == "before"):
        fails.append(f"{name}: no second team barred before a change (the test sees no case)")
    if name.endswith("-off"):
        if down or not any(p[5] for p in after):
            fails.append(f"{name}: with the switch off second teams moved ({down}) or none is barred")
        return fails
    if any(p[5] or p[4] for p in after):
        fails.append(f"{name}: after a change second teams are barred or have no first team: {after}")
    if not any(d[3] == "the First Division" for d in down):
        fails.append(f"{name}: no second team of a First Division club went down (the rule to 2014)")
    wrong = [d[0] for d in down if d[3] == "Eliteserien" and not d[5]]
    if wrong:
        fails.append(f"{name}: second teams of Eliteserien clubs went down: {wrong}")
    may = MAY.findall(log)
    stay_wrong = [m[0] for m in may if m[2] != "Eliteserien"]
    if stay_wrong:
        fails.append(f"{name}: second teams that the rule to 2014 bars stay: {stay_wrong}")
    if not any(m[2] == "Eliteserien" for m in may):
        fails.append(f"{name}: no second team of an Eliteserien club stays (the test sees no allowed case)")
    return fails


RUNS = {"on": run_on,
        "parent": lambda p, g, w: run_parent("parent", p, g, w, {}),
        "parent-off": lambda p, g, w: run_parent("parent-off", p, g, w, {"RECOMP_NOR_SECOND_PAIRS": "off"}),
        "parent2014": lambda p, g, w: run_parent("parent2014", p, g, w, {}, year=2014),
        "parent-rules": lambda p, g, w: run_parent("parent-rules", p, g, w, {}, year=2025, rules="on"),
        "rule2014": lambda p, g, w: run_rule2014("rule2014", p, g, w, {}),
        "rule2014-off": lambda p, g, w: run_rule2014("rule2014-off", p, g, w, {"RECOMP_NOR_SECOND_PAIRS": "off"}), "off": run_off,
        "old1": lambda p, g, w: run_old("old1", p, g, w, 1, 2025, no_mark=True),
        "old2": lambda p, g, w: run_old("old2", p, g, w, 2, 2026)}


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="nor-second-")
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
