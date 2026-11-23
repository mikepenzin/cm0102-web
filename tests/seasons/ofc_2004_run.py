#!/usr/bin/env python3
"""The OFC Nations Cup of 2004 (0.6.2 open item 136, src/recomp_rt/win/season_intl_ofc.c).

Usage:
    python3 tests/seasons/ofc_2004_run.py <cm0102_recomp> <game folder> [--keep <folder>]

The real final stage of 2004 (rules table rows int-continental-32 and -43; https://www.rsssf.org/tables/04oc.html and
https://en.wikipedia.org/wiki/2004_OFC_Nations_Cup): one group of 6 in Adelaide from 29 May to 6 June 2004, each pair
once, then the top two in a two-legged final, 9 October 2004 in Honiara and 12 October 2004 in Sydney. The original's
class plays two groups of 3 and semi-finals (OceaniaNations, create_group_table 0x7995a0, create_group_stage
0x799a60, create_knockout_stage 0x799c50).

One new game, Tier A, England's leagues (the national sides of every nation play), seed 1, changing rules on: the
3.9.68 database, a 2001/02 start, to 15 August 2004 (in the pause between the group and the final), a save, then the
save loaded and on to 31 December 2008, so the final is made before the save and played after the load, and the class
moves on to the next edition of the table (2008: one group of 4, home and away, ofc_2008_run.py).

The checks: each leg ends with code 0 and shows no message box; the 2004 edition's matches from May to June are one
round robin of 6 teams (15 matches, each pair once) from 29 May to 6 June 2004; its later matches are two, from 9 and
12 October 2004, between two teams of the group, home and away, the group's second at home in the first (the log
line of the final, RECOMP_SEASON_INTL_LOG, names the second and the first); the history row of 2004 has those two as winner and
runner-up; no match of the class is played from November 2004 to September 2007 (the table holds no edition of
2006); the 2008 edition has 4 teams and 12 matches and a history row. A day may be up to LATE days later than the real one, for a match that a clash moves (add_fixture
0x5aa680), as tests/seasons/ofc_2008_run.py allows.
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

START = datetime.date(2001, 7, 1)          # 3.9.68, 2001/02: the game's first day (soak_run's leg counts from it)
PAUSE = datetime.date(2004, 8, 15)
END = datetime.date(2008, 12, 31)
GROUP = (datetime.date(2004, 5, 29), datetime.date(2004, 6, 6))
LEGS = (datetime.date(2004, 10, 9), datetime.date(2004, 10, 12))
LATE = 21
FINAL = re.compile(r"oceania_nations 2004: final of two legs .*club (-?\d+) \(second, home first\) v club (-?\d+)")


def late(day, real):
    return real <= day <= real + datetime.timedelta(LATE)


def edition_2004(save):
    """(group fixtures, October fixtures, history row or None, names) of the OFC Nations Cup of 2004 in a save."""
    game = history_check.Game(save)
    ids = {cid for cid, c in game.comps.items() if c["name"].startswith(("OFC", "Oceania Nations"))}
    fx = sorted(f for cid in ids for f in history_check.fixtures(game).get(cid, []))
    group = [f for f in fx if datetime.date(2004, 5, 1).toordinal() <= f[0] <= datetime.date(2004, 6, 30).toordinal()]
    final = [f for f in fx if datetime.date(2004, 7, 1).toordinal() <= f[0] <= datetime.date(2004, 12, 31).toordinal()]
    between = [f for f in fx if datetime.date(2005, 1, 1).toordinal() <= f[0] < datetime.date(2007, 10, 1).toordinal()]
    later = [f for f in fx if f[0] >= datetime.date(2007, 10, 1).toordinal()]
    row = next(((a, b) for cid, y, a, b, c, h in game.rows if cid in ids and y == 2004), None)
    row08 = next(((a, b) for cid, y, a, b, c, h in game.rows if cid in ids and y == 2008), None)

    def name(t):
        return game.nations.get(game.clubs.get(t, ("", -1))[1], ("", ""))[0] or str(t)

    return group, final, row, name, between, later, row08


def check(group, final, row, name, between, later, row08, finalists):
    fails = []
    teams = {t for _, h, a in group for t in (h, a)}
    pairs = collections.Counter(frozenset((h, a)) for _, h, a in group)
    first, last = (datetime.date.fromordinal(group[0][0]), datetime.date.fromordinal(group[-1][0])) if group else (
        None, None)
    print(f"      group: {len(group)} matches, {len(teams)} teams ({', '.join(sorted(name(t) for t in teams))}), "
          f"{first} to {last}")
    if len(teams) != 6 or len(group) != 15 or any(n != 1 for n in pairs.values()):
        fails.append(f"the group of May and June is not one round robin of 6: {len(teams)} teams, {len(group)} "
                     f"matches, pairs {sorted(pairs.values())}")
    if not (first and late(first, GROUP[0]) and late(last, GROUP[1])):
        fails.append(f"the group plays from {first} to {last}, the real days are {GROUP[0]} to {GROUP[1]}")
    legs = [(datetime.date.fromordinal(d), name(h), name(a)) for d, h, a in final]
    print(f"      final: {legs}; history row {row and (name(row[0]), name(row[1]))}")
    if len(final) != 2 or not (late(legs[0][0], LEGS[0]) and late(legs[1][0], LEGS[1]) and legs[0][0] < legs[1][0]):
        fails.append(f"the final is not two legs from {LEGS[0]} and {LEGS[1]}: {legs}")
    else:
        (_, h1, a1), (_, h2, a2) = final
        if (h1, a1) != (a2, h2) or h1 not in teams or a1 not in teams:
            fails.append(f"the two legs are not home and away between two teams of the group: {legs}")
        elif finalists != (h1, a1):
            fails.append(f"the first leg is not at the group's second: log {finalists}, legs {legs}")
        elif not row or {row[0], row[1]} != {h1, a1}:
            fails.append(f"the history row {row} does not name the finalists as winner and runner-up")
    teams08 = {t for _, h, a in later for t in (h, a)}
    print(f"      after: {len(between)} matches from 2005 to September 2007; 2008: {len(later)} matches, "
          f"{len(teams08)} teams, history row {row08 and (name(row08[0]), name(row08[1]))}")
    if between:
        fails.append(f"{len(between)} matches of the class from 2005 to September 2007, the table holds no edition")
    if len(later) != 12 or len(teams08) != 4 or not row08 or not set(row08) <= teams08:
        fails.append(f"the 2008 edition after it is not one group of 4 home and away with a history row: "
                     f"{len(later)} matches, {len(teams08)} teams, row {row08}")
    if not fails:
        print(f"ok    2004: one group of 6 from {first} to {last}, final on {legs[0][0]} and {legs[1][0]}")
    return fails


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-ofc2004-")
    os.environ["RECOMP_TIER_A"] = "1"
    o = {"program": program, "game": game, "rules": "on", "database": None, "seed": 1, "start": 2001, "last": 2011,
         "nations": ["England"], "extra": [], "timeout": 5400}
    work = os.path.join(keep, "leg")
    code, took, box = soak_run.new_game_leg(o, work, (PAUSE - START).days)
    fails = []
    finalists = None
    try:
        with open(os.path.join(work, "stderr.txt"), errors="replace") as f:
            m = next((m for m in map(FINAL.search, f) if m), None)
        finalists = m and (int(m.group(1)), int(m.group(2)))
    except OSError:
        pass
    print(f"      log: final {finalists} (second, first)")
    if code != 0 or box:
        fails.append(f"the run ended with code {code}{', box ' + box if box else ''} ({work})")
    save = os.path.join(work, "user", "samesave.sav")
    if os.path.exists(save):
        work = os.path.join(keep, "leg-load")
        code, more, box = soak_run.load_leg(o, work, save, (END - PAUSE).days)
        took += more
        if code != 0 or box:
            fails.append(f"the loaded run ended with code {code}{', box ' + box if box else ''} ({work})")
        save = os.path.join(work, "user", "samesave.sav")
    if not os.path.exists(save):
        fails.append("no save after the days")
    else:
        print(f"      run {took:.0f} s")
        fails += check(*edition_2004(save), finalists)
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
