#!/usr/bin/env python3
"""Both legs of a qualifying tie in one FIFA window, in order (0.6.2 item 207, season_tie_legs.c).

Usage:
    python3 tests/seasons/tie_legs_run.py <cm0102_recomp> <game folder> --save <save> --database <folder>
        --keep <folder>
    python3 tests/seasons/tie_legs_run.py --log <stderr.txt>      (check the log of an earlier run only)

The case: run set game a2021s1 (October 2021 database, 2021 start, changing rules on), the save of leg000
(samesave.sav, made in 2031), loaded Tier A and played from 2031 day 202 to 2032 day 230. In June 2032 a CONCACAF
qualifying round of two-leg ties plays its first legs on day 156 (in the FIFA window of days 154 to 159) and its
second legs the round's gap of 14 days later, on day 170. Cup::create_tie_fixtures 0x51cb90 stores both legs, and
the snap of single qualifiers leaves them alone, so add_fixture's clash move (0x5aaa25) moved Haiti v Canada's
second leg from day 170 to 167. Real life: both legs of a qualifying tie are played in one FIFA window, in order
(Anguilla v Turks and Caicos Islands, CONCACAF first round of the 2026 cycle, 22 and 26 March 2024).

The checks, from the game's log (RECOMP_CLUB_LOG=2 with RECOMP_FIXTURE_RECORD names each stored leg and its caller;
RECOMP_SCREEN_LOG gives the tie lines; RECOMP_TEST_FIXTURE_HOLD=log names each clash move):
- the run ends with code 0 and no message box;
- the CONCACAF section stores two-leg ties in 2032 (first legs from 0x51cf5d), each has a tie line, and a tie that
  moves has its legs 3 to 6 days apart, in order; at least one tie moves to a FIFA window (four tie rounds share the
  March and June windows of 2032, so the April and May rounds keep their days);
- no leg of a qualifying tie (the line after the clash names round code digit 3 or 4) has a clash move.
Red on 0c41f954 (items 203 and 204), green on the fix.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))

COMP = "FIFA World Cup qualification (CONCACAF)"
YEAR = 2032
FIRST = re.compile(r"club records: add_fixture from 0051cf5d: " + re.escape(COMP) + r", clubs \S+ \S+ (.+?) v (.+?) "
                   r"day (\d+)/(\d+)$")
TIE = re.compile(r"intl dates: " + re.escape(COMP) + r" (.+?) v (.+?), a two-leg tie of days (\d+) and (\d+) of (\d+), "
                 r"(?:plays on the FIFA days (-?\d+) and (-?\d+)|keeps its days)")
CLASH = re.compile(r"fixtures: test: a clash move of (.*qualification.*) from day (\d+) of (\d+) to day (\d+)")
LEG = re.compile(r"fixtures: test: other leg .* code (\d+) ")


def check(path):
    with open(path, encoding="latin-1") as f:
        lines = [x.rstrip("\n") for x in f]
    fails = []
    firsts = [(m.group(1), m.group(2), int(m.group(3))) for m in map(FIRST.search, lines)
              if m and int(m.group(4)) == YEAR]
    ties = []                                   # (home, away, own days, planned days or None)
    for m in map(TIE.search, lines):
        if m and int(m.group(5)) == YEAR:
            ties.append((m.group(1), m.group(2), (int(m.group(3)), int(m.group(4))),
                         (int(m.group(6)), int(m.group(7))) if m.group(6) else None))
    moved = [t for t in ties if t[3]]
    print(f"      {COMP} {YEAR}: {len(firsts)} first legs stored, {len(ties)} tie lines, {len(moved)} moved")
    rounds = {}
    for _, _, own, plan in ties:
        rounds.setdefault(own, set()).add(plan)
    for own in sorted(rounds):
        print(f"      tie round of days {own[0]} and {own[1]}: " +
              ", ".join(f"FIFA days {p[0]} and {p[1]}" if p else "keeps its days" for p in sorted(rounds[own], key=str)))
    if not firsts:
        fails.append(f"no two-leg tie of {COMP} in {YEAR}")
    if not moved:
        fails.append(f"no tie of {COMP} in {YEAR} moved to a FIFA window")
    for h, a, _ in firsts:                      # the records log cuts names: match by the start
        t = [x for x in ties if x[0].startswith(h) and x[1].startswith(a)]
        if not t:
            fails.append(f"{h} v {a}: no tie line")
            continue
        own, plan = t[0][2], t[0][3]
        if plan and not (3 <= plan[1] - plan[0] <= 6):
            fails.append(f"{h} v {a}: legs {plan[0]} and {plan[1]} (own days {own[0]} and {own[1]})")
    for i, line in enumerate(lines):
        m = CLASH.search(line)
        if not m or i + 1 >= len(lines):
            continue
        leg = LEG.search(lines[i + 1])
        if leg and int(leg.group(1)) % 10 in (3, 4):
            print("      " + line)
            fails.append(f"clash move of a tie leg of {m.group(1)}: day {m.group(2)} of {m.group(3)} to {m.group(4)}")
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
               "RECOMP_TEST_FIXTURE_HOLD=log", "RECOMP_SCREEN_LOG=1"):
        k, v = kv.split("=", 1)
        soak_run.LEG_ENV[k] = v
    os.environ["RECOMP_TIER_A"] = "1"
    o = {"program": os.path.abspath(argv[1]), "game": os.path.abspath(argv[2]), "start": 2021, "last": 2101,
         "seed": 1, "nations": ["all"], "database": os.path.abspath(arg("--database")), "rules": "on", "extra": [],
         "timeout": 21600}
    days = (YEAR * 366 + 230) - (2031 * 366 + 202)
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
