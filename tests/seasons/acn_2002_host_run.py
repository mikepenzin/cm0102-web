#!/usr/bin/env python3
"""The host of the Africa Cup of Nations of 2002 in a 2001 game (0.6.2 open item 181, season_intl.c
season_intl_draw_hosts).

Usage:
    python3 tests/seasons/acn_2002_host_run.py <cm0102_recomp> <game folder> [--keep <folder>]

Real life: Mali hosted the 2002 finals, 16 teams, 19 January to 10 February 2002
(https://en.wikipedia.org/wiki/2002_African_Cup_of_Nations). Egypt played there as a qualifier. The original's host
row of 2002 names Egypt (HostCountry::init_default_table 0x5f9970, fixed choice at 0x5f9f2f), and the ACN has no
qualifying stage: its entrants are the hosts and the best African nations (draw_finalists 0x4022c0, 0x40242f). So
in the base game Egypt hosts and Mali plays only on rank. With changing rules on, the draw of a 2001 game takes the
table's host, Mali.

One new game, Tier A, England's leagues (the national sides of every nation play), seed 1, changing rules on: the
3.9.68 database, a 2001/02 start, to 31 March 2002 (after the finals: the game plays them from 9 February to
2 March), a save.

The checks: the run ends with code 0 and shows no message box; the 2002 edition plays 16 teams, Mali and Egypt among
them; its history row of 2002 names Mali as host. On the original's host row (main before item 181) the row names
Egypt: the test fails there.
"""
import datetime
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))
import history_check  # noqa: E402
import soak_run  # noqa: E402

START = datetime.date(2001, 7, 1)          # 3.9.68, 2001/02: the game's first day (soak_run's leg counts from it)
END = datetime.date(2002, 3, 31)
TEAMS = 16
HOST = "Mali"
ON_RANK = "Egypt"


def edition_2002(save):
    """(names of the teams of the 2002 finals, host nation of the 2002 history row or None) in a save."""
    game = history_check.Game(save)
    ids = {cid for cid, c in game.comps.items() if c["name"].startswith(("Africa Cup of Nations",
                                                                          "African Cup of Nations"))}
    lo, hi = datetime.date(2001, 12, 1).toordinal(), datetime.date(2002, 3, 31).toordinal()
    fx = [f for cid in ids for f in history_check.fixtures(game).get(cid, []) if lo <= f[0] <= hi]
    teams = {game.club(t)[1] or game.club(t)[0] for _, h, a in fx for t in (h, a)}
    row = next((r for r in game.rows if r[0] in ids and r[1] == 2002), None)
    host = row and (game.club(row[5])[1] or game.club(row[5])[0])
    return teams, host, len(fx)


def check(teams, host, matches):
    fails = []
    print(f"      2002: {matches} matches, {len(teams)} teams ({', '.join(sorted(teams))}); host of the row {host}")
    if len(teams) != TEAMS:
        fails.append(f"the 2002 finals have {len(teams)} teams, not {TEAMS}")
    for nation in (HOST, ON_RANK):
        if nation not in teams:
            fails.append(f"{nation} does not play the 2002 finals")
    if host != HOST:
        fails.append(f"the 2002 history row names {host} as host, not {HOST}")
    if not fails:
        print(f"ok    2002: {HOST} hosts and plays, {ON_RANK} plays on rank")
    return fails


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-acn2002-")
    os.environ["RECOMP_TIER_A"] = "1"
    o = {"program": program, "game": game, "rules": "on", "database": None, "seed": 1, "start": 2001, "last": 2011,
         "nations": ["England"], "extra": [], "timeout": 5400}
    work = os.path.join(keep, "leg")
    code, took, box = soak_run.new_game_leg(o, work, (END - START).days)
    fails = []
    if code != 0 or box:
        fails.append(f"the run ended with code {code}{', box ' + box if box else ''} ({work})")
    save = os.path.join(work, "user", "samesave.sav")
    if not os.path.exists(save):
        fails.append("no save after the days")
    else:
        print(f"      run {took:.0f} s")
        fails += check(*edition_2002(save))
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
