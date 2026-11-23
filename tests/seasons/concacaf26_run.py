#!/usr/bin/env python3
"""The CONCACAF World Cup qualifying of 2026 in its real shape and on its real days (0.6.2 item 214,
season_concacaf26.c, season_concacaf26_days.c).

Usage:
    python3 tests/seasons/concacaf26_run.py <cm0102_recomp> <game folder> [--keep <folder>]
    python3 tests/seasons/concacaf26_run.py --check <folder>     (check the leg of an earlier run only)

Run games from a build folder with its seasons/ folder beside it (the rules table).

Before item 214 the game played the 2026 qualifying in the original's 2002 shape (Caribbean ties, Central American
groups, a play-off, semi-final groups of 4 in the autumn of 2024, a final group of 6 in 2025), so it met the
2024-25 CONCACAF Nations League (real days, September to November 2024): 8 Nations League matches within 3 days of
a qualifier of the same side (tests/seasons/nl_wcq_windows_run.py of item 209). Real life (en.wikipedia.org, "2026
FIFA World Cup qualification (CONCACAF)", read 9 November 2026): a first round of two ties on 22 and 26 March 2024,
a second round of six groups of five in the June 2024 and June 2025 windows, a third round of three groups of four
from 4 September to 18 November 2025; the three group winners qualify, the two best runners-up go to the play-off
tournament.

One new game in 2021/22 on the October 2021 database, Tier A, England's leagues, seed 1, changing rules on, played
to 1 August 2030. The checks, for the 2026 cycle (the real days, three hosts: 3 direct places) and the 2030 cycle
(the window rule of season_concacaf26_days.c, no host: 6 direct places, two a group):
- the leg ends with code 0 and no message box;
- the log names the 2026 shape, the first round of 2 ties, the second round of 6 groups of 5 and the third round of
  3 groups of 4;
- the qualifying's matches of the two years before the finals are on the planned days only, with the planned counts
  (2026: 2 on each of 22 and 26 March 2024, 12 on each of 5, 8 and 11 June 2024 and 6 and 10 June 2025, 6 on each
  of 4 and 8 September, 9 and 13 October, 13 and 18 November 2025);
- the play-off tournament takes the best two after the direct places of the third round (points, then goal
  difference, from the log of the third round's tables);
- no Nations League match from 2024 on is within 3 days of a qualifier of the same side;
- the World Cup holds its direct places to 2 more teams of the third round (the play-off tournament's), at least
  the direct places of each group from each group.
On the code before item 214 the days and the shape differ and the Nations League check fails.
"""
import collections
import datetime
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))
import history_check  # noqa: E402
import soak_run  # noqa: E402

DATABASES = "/Users/max/Projects/cm0102/test-fixtures/databases"
START = datetime.date(2021, 7, 1)
END = datetime.date(2030, 8, 1)
NL = "CONCACAF Nations League"
WC = "FIFA World Cup"
D = datetime.date
# The days of each cycle and their match counts: 2026 the real days, 2030 the window rule (no host: a pool of 35,
# five first-round ties).
CYCLES = {
    2026: ({D(2024, 3, 22): 2, D(2024, 3, 26): 2,
            D(2024, 6, 5): 12, D(2024, 6, 8): 12, D(2024, 6, 11): 12, D(2025, 6, 6): 12, D(2025, 6, 10): 12,
            D(2025, 9, 4): 6, D(2025, 9, 8): 6, D(2025, 10, 9): 6, D(2025, 10, 13): 6, D(2025, 11, 13): 6,
            D(2025, 11, 18): 6}, 3, 2),
    2030: ({D(2028, 3, 23): 5, D(2028, 3, 27): 5,
            D(2028, 6, 1): 12, D(2028, 6, 5): 12, D(2029, 3, 22): 12, D(2029, 6, 7): 12, D(2029, 6, 11): 12,
            D(2029, 9, 27): 6, D(2029, 10, 1): 6, D(2029, 10, 4): 6, D(2029, 10, 8): 6, D(2029, 11, 15): 6,
            D(2029, 11, 19): 6}, 6, 5),
}
RANKED = re.compile(r"wc play-off (\d+): CONCACAF third round: (.*); to the tournament: (.*), (.*)$")


def is_wcq(name):
    return name in ("FIFA World Cup qualification (CONCACAF)", "World Cup CONCACAF Qualifying Section")


def matches(game, test):
    fx = history_check.fixtures(game)
    return [(f[0], f[1], f[2]) for cid, c in game.comps.items() if test(c["name"]) for f in fx.get(cid, [])]


def nation_of(game, club):
    c = game.clubs.get(club)
    return game.nations.get(c[1], ("?",))[0] if c else "?"


def check_cycle(wc, log, game, fails):
    days_want, direct, ties = CYCLES[wc]
    groups_re = re.compile(rf"wc qualifying {wc}: CONCACAF (second|third) round on day \d+ of \d+: (.*)$")
    shape = [x for x in log if f"wc qualifying {wc}: CONCACAF in the 2026 shape" in x]
    first = [x for x in log if f"wc qualifying {wc}: CONCACAF first round of {ties} ties" in x]
    groups = {m.group(1): m.group(2) for m in map(groups_re.search, log) if m}
    for x in shape[:1] + first[:1]:
        print("      " + x.split("recomp: season intl: ")[-1])
    if not shape or not first:
        fails.append(f"{wc}: no log line of the 2026 shape and its first round of {ties} ties")
    third = []
    for kind, want, size in (("second", 6, 5), ("third", 3, 4)):
        g = re.findall(r"([A-F]): ([^:]+?)(?= [A-F]:|$)", groups.get(kind, ""))
        print(f"      {wc} {kind} round: " + "; ".join(f"{k}: {v}" for k, v in g))
        if len(g) != want or any(len(v.split(",")) != size for _, v in g):
            fails.append(f"{wc}: the {kind} round is not {want} groups of {size}")
        if kind == "third":
            third = [set(n.strip() for n in v.split(",")) for _, v in g]
    # the tournament's two: the best two after the direct places (points, then goal difference)
    ranked = [m for m in map(RANKED.search, log) if m and int(m.group(1)) == wc]
    if not ranked:
        fails.append(f"{wc}: no line of the third round's order")
    else:
        m = ranked[-1]
        tables = [[(t.rsplit(" ", 2)[0].strip(), int(t.rsplit(" ", 2)[1]), int(t.rsplit(" ", 2)[2]))
                   for t in grp.split(",")] for grp in re.findall(r"[A-C]: ([^:]+?)(?= [A-C]:|$)", m.group(2))]
        per = direct // 3
        rest = sorted((row for t in tables for row in t[per:per + 1]), key=lambda r: (-r[1], -r[2]))
        rest += sorted((row for t in tables for row in t[per + 1:per + 2]), key=lambda r: (-r[1], -r[2]))
        want = [r[0] for r in rest[direct % 3:direct % 3 + 2]]
        print(f"      {wc} third round: {m.group(2)}; to the tournament: {m.group(3)}, {m.group(4)}")
        if [m.group(3), m.group(4)] != want:
            fails.append(f"{wc}: the tournament takes {m.group(3)} and {m.group(4)}, not {want[0]} and {want[1]}")
    wcq = matches(game, is_wcq)
    lo, hi = D(wc - 2, 1, 1).toordinal(), D(wc, 1, 1).toordinal()
    days = collections.Counter(D.fromordinal(d) for d, _, _ in wcq if lo <= d < hi)
    print(f"      {wc} qualifying days: " + ", ".join(f"{d.isoformat()} {n}" for d, n in sorted(days.items())))
    wrong = {d: n for d, n in days.items() if days_want.get(d) != n}
    missing = [d for d in days_want if d not in days]
    if wrong or missing:
        fails.append(f"{wc}: days not the planned ones: " + ", ".join(f"{d.isoformat()} {n}" for d, n in
                                                               sorted(wrong.items()))
                     + ("; missing " + ", ".join(d.isoformat() for d in missing) if missing else ""))
    wc_teams = {nation_of(game, t) for d, h, a in matches(game, lambda n: n == WC)
                if D(wc, 6, 1).toordinal() <= d < D(wc, 8, 1).toordinal() for t in (h, a)}
    in_wc = [sorted(g & wc_teams) for g in third]
    print(f"      World Cup {wc} teams of the third round: " + "; ".join(", ".join(g) or "none" for g in in_wc))
    count = sum(len(g) for g in in_wc)
    if third and (not direct <= count <= direct + 2 or any(len(g) < direct // 3 for g in in_wc)):
        fails.append(f"{wc}: the World Cup holds {count} teams of the third round")
    if not wc_teams:
        fails.append(f"{wc}: no World Cup matches in June and July")


def check(folder):
    fails = []
    with open(os.path.join(folder, "stderr.txt"), encoding="latin-1") as f:
        log = f.read().splitlines()
    save = os.path.join(folder, "user", "samesave.sav")
    if not os.path.exists(save):
        return fails + ["no save"]
    game = history_check.Game(save)
    for wc in CYCLES:
        check_cycle(wc, log, game, fails)
    wcq = matches(game, is_wcq)
    sides = collections.defaultdict(list)
    for d, h, a in wcq:
        sides[h].append(d), sides[a].append(d)
    near = sorted({D.fromordinal(d).isoformat() for d, h, a in matches(game, lambda n: n == NL)
                   if d >= D(2024, 1, 1).toordinal() and any(abs(d - q) <= 3 for t in (h, a) for q in sides[t])})
    if near:
        fails.append("Nations League matches within 3 days of a qualifier of the same side: " + ", ".join(near))
    boxes = [x for x in log if "message box from" in x]
    if boxes:
        fails.append("message box: " + boxes[0])
    return fails


def main(argv):
    if "--check" in argv:
        fails = check(argv[argv.index("--check") + 1])
    elif len(argv) >= 3:
        program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
        keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-cc26-")
        os.environ["RECOMP_TIER_A"] = "1"
        soak_run.LEG_ENV["RECOMP_SEASON_INTL_LOG"] = "1"
        o = {"program": program, "game": game, "rules": "on", "seed": 1, "start": 2021, "last": 2033,
             "database": os.path.join(DATABASES, "October 2021"), "nations": ["England"], "extra": [], "timeout": 7200}
        w = os.path.join(keep, "leg")
        code, took, box = soak_run.new_game_leg(o, w, (END - START).days)
        print(f"      leg to {END}: {took:.0f} s")
        fails = [] if code == 0 and not box else [f"the leg ended with code {code}{', box ' + box if box else ''}"]
        fails += check(w)
    else:
        print(__doc__)
        return 2
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
