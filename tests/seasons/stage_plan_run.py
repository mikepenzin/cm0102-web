#!/usr/bin/env python3
"""A qualifying stage's match days in round order (0.6.2 items 203 and 204, season_stage_plan.c).

Usage:
    python3 tests/seasons/stage_plan_run.py <cm0102_recomp> <game folder> --save <save> --database <folder>
        --keep <folder>
    python3 tests/seasons/stage_plan_run.py --log <stderr.txt>      (check the log of an earlier run only)

The case: run set game a2021s1 (October 2021 database, 2021 start, changing rules on), the save of leg000
(samesave.sav, made in 2031), loaded Tier A and played from 2031 day 202 to 2033 day 188. In 2033 the AFC third
round of the 2034 cycle is made (about day 150), 10 weekly matchdays on days 230 to 293. Before the fix each
qualifier moved on its own to the nearest free FIFA day: matchdays 5 and 6 went back to June, before matchday 1
(item 203), and matchdays 7 and 8 kept days 1 or 2 days from the same sides' matchdays 3 and 4, so add_fixture's
clash move (0x5aaa25) moved them 3 to 5 days (item 204). Real life: the AFC third round of the 2026 cycle played its
10 matchdays in order on the FIFA windows (en.wikipedia.org, "2026 FIFA World Cup qualification – AFC third
round").

The checks, from the game's log (RECOMP_CLUB_LOG=2 with RECOMP_FIXTURE_RECORD names each stored qualifier and its
template day; RECOMP_SCREEN_LOG gives the snap's moves; RECOMP_TEST_FIXTURE_HOLD=log names each clash move):
- the run ends with code 0 and no message box;
- the AFC stage of 2033 whose template days lie from day 200 has 10 matchdays, and each matchday's days all lie
  before every day of the next (in order), with no side playing twice within 2 days;
- no AFC qualifier has a clash move.
Red on ad9be585 (round AF), green on the fix.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))

COMP = "FIFA World Cup qualification (AFC)"
YEAR = 2033
ADD = re.compile(r"club records: add_fixture from 00683(?:a5a|df3): " + re.escape(COMP) +
                 r", clubs \S+ \S+ (.+?) v (.+?) day (\d+)/(\d+)$")
MOVE = re.compile(r"intl dates: " + re.escape(COMP) + r" (.+?) v (.+?) moves from day (\d+) to the .*? (\d+) of "
                  r"(\d+)(?: \(.*\))?$")
CLASH = re.compile(r"fixtures: test: a clash move of " + re.escape(COMP) + r" from day (\d+) of (\d+) to day (\d+)")


def matches(lines):
    """[(template day, final day, home, away)] of the AFC qualifiers of YEAR, in the order they were stored."""
    out = []
    for line in lines:
        m = ADD.search(line)
        if m and int(m.group(4)) == YEAR:
            out.append([int(m.group(3)), int(m.group(3)), m.group(1), m.group(2)])
            continue
        m = MOVE.search(line)
        if m and out and int(m.group(5)) == YEAR:
            last = out[-1]
            if (last[2], last[3], last[0]) == (m.group(1), m.group(2), int(m.group(3))):
                last[1] = int(m.group(4))
    return out


def check(path):
    with open(path, encoding="latin-1") as f:
        lines = [x.rstrip("\n") for x in f]
    fails = []
    stage = [m for m in matches(lines) if m[0] >= 200]
    rounds = {}
    for t, d, h, a in stage:
        rounds.setdefault(t, []).append(d)
    order = sorted(rounds)
    print(f"      {COMP} {YEAR}, template days from 200: {len(stage)} matches, {len(order)} matchdays")
    for t in order:
        print(f"      matchday {order.index(t) + 1}: template day {t}, days {sorted(set(rounds[t]))}")
    if len(order) != 10:
        fails.append(f"{len(order)} matchdays, want 10")
    for a, b in zip(order, order[1:]):
        if max(rounds[a]) >= min(rounds[b]):
            fails.append(f"matchday {order.index(b) + 1} (days {sorted(set(rounds[b]))}) not after matchday "
                         f"{order.index(a) + 1} (days {sorted(set(rounds[a]))})")
    days = {}
    for _, d, h, a in stage:
        for side in (h, a):
            days.setdefault(side, []).append(d)
    for side, ds in sorted(days.items()):
        ds.sort()
        near = [(x, y) for x, y in zip(ds, ds[1:]) if y - x <= 2]
        if near:
            fails.append(f"{side} plays within 2 days: {near}")
    clashes = [line for line in lines if CLASH.search(line)]
    for line in clashes[:10]:
        print("      " + line)
    if clashes:
        fails.append(f"{len(clashes)} clash moves of {COMP}")
    boxes = [line for line in lines if "message box from" in line]
    if boxes:
        fails.append(f"message box: {boxes[0]}")
    return fails


def run(argv):
    import soak_run
    arg = lambda name: argv[argv.index(name) + 1]           # noqa: E731
    work = os.path.abspath(arg("--keep"))
    os.makedirs(work, exist_ok=True)
    for kv in ("RECOMP_CLUB_LOG=2", f"RECOMP_FIXTURE_RECORD={COMP}", "RECOMP_SEASON_INTL_LOG=1",
               "RECOMP_TEST_FIXTURE_HOLD=log"):
        k, v = kv.split("=", 1)
        soak_run.LEG_ENV[k] = v
    os.environ["RECOMP_TIER_A"] = "1"
    o = {"program": os.path.abspath(argv[1]), "game": os.path.abspath(argv[2]), "start": 2021, "last": 2101,
         "seed": 1, "nations": ["all"], "database": os.path.abspath(arg("--database")), "rules": "on", "extra": [],
         "timeout": 21600}
    days = (YEAR * 366 + 188) - (2031 * 366 + 202)
    code, took, box = soak_run.load_leg(o, work, os.path.abspath(arg("--save")), days)
    print(f"      run: code {code}, {round(took)} s, box {box}")
    fails = [] if code == 0 and not box else [f"run ended with code {code}, box {box}"]
    return fails + check(os.path.join(work, "stderr.txt"))


def main(argv):
    if "--log" in argv:
        fails = check(argv[argv.index("--log") + 1])
    elif len(argv) >= 3 and all(x in argv for x in ("--save", "--database", "--keep")):
        fails = run(argv)
    else:
        print(__doc__)
        return 2
    for f in fails:
        print("FAIL  " + f)
    print("PASS" if not fails else f"FAIL  {len(fails)} problems")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
