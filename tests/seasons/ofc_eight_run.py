#!/usr/bin/env python3
"""The OFC Nations Cup of 8 teams (0.6.2 open item 45, src/recomp_rt/win/season_intl_ofc.c).

Usage:
    python3 tests/seasons/ofc_eight_run.py <cm0102_recomp> <game folder> [--keep <folder>] [--only 2002|2012]

Two new games, Tier A, all nations, seed 1, changing rules on:
- 2002: the own database Summer 2002 from its start date (13 July 2002), 40 days. The 2002 edition plays from 5 July
  2002 (tests/seasons/intl_late_draw_run.py). Australia is in Oceania.
- 2012: the September 2011 ODB database (test-fixtures/databases) from its start date in 2011, 345 days. The 2012
  edition plays from 1 June 2012, with Australia in Asia and the host Solomon Islands.
The finals table gives both editions 8 teams (season_intl_table.c, int-continental-21 and -120); the original's
class plays 6 (OceaniaNations::select_and_draw_teams 0x799e30 writes 6 at 0x799e42).

The checks for each game: the program ends with code 0 and shows no message box; the edition of the year has 8 teams
in its fixtures, two groups of 4 and a history row with a winner and a runner-up; the edition's host of the table is
still an entrant after the two added nations (New Zealand and Solomon Islands are in the original's six too, so this
check guards the order of the steps only). The group check counts each team's first 3 matches of the edition: 12 for
two groups of 4. With 6 teams it counts the 6 group matches and 2 of the knockout.
"""
import collections
import os
import sys
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))
import history_check  # noqa: E402
import soak_run  # noqa: E402

OWN_ZIP = "/Users/max/Projects/cm0102/own-dbs/Summer 2002.zip"
DATABASES = "/Users/max/Projects/cm0102/test-fixtures/databases"
GAMES = {
    2002: {"start": 2002, "days": 40, "host": "New Zealand"},
    2012: {"start": 2011, "days": 345, "host": "Solomon Islands", "database": "September 2011 ODB"},
}


def edition(save, year):
    """(teams, group matches, history row or None, nation names of the teams) of the OFC Nations Cup edition of the
    year in a save."""
    game = history_check.Game(save)
    ids = {cid for cid, c in game.comps.items() if c["name"].startswith(("OFC", "Oceania Nations"))}
    fx = [f for cid in ids for f in history_check.fixtures(game).get(cid, [])]
    teams, group = set(), 0
    for y, first, last, t in history_check.editions_of(fx):
        if y != year:
            continue
        teams = t
        played = collections.Counter()
        for day, home, away in sorted(fx):
            if first <= day <= last and home in t and away in t and played[home] < 3 and played[away] < 3:
                played[home] += 1
                played[away] += 1
                group += 1
    row = next(((a, b) for cid, y, a, b, c, h in game.rows if cid in ids and y == year), None)
    names = {game.nations.get(game.clubs.get(t, ("", -1))[1], ("", ""))[0] for t in teams}
    return teams, group, row, names


def run(program, game_folder, keep, year):
    g = GAMES[year]
    if "database" in g:
        database = os.path.join(DATABASES, g["database"])
    else:
        if not os.path.exists(OWN_ZIP):
            return [f"{year}: missing {OWN_ZIP}"]
        database = os.path.join(keep, "owndb", "Summer 2002")
        if not os.path.isdir(database):
            with zipfile.ZipFile(OWN_ZIP) as z:
                z.extractall(os.path.join(keep, "owndb"))
    o = {"program": program, "game": game_folder, "rules": "on", "database": database, "seed": 1, "start": g["start"],
         "last": year + 10, "nations": ["all"], "extra": [], "timeout": 3600}
    work = os.path.join(keep, f"leg{year}")
    code, took, box = soak_run.new_game_leg(o, work, g["days"])
    fails = []
    if code != 0 or box:
        fails.append(f"{year}: the run ended with code {code}{', box ' + box if box else ''} ({work})")
    save = os.path.join(work, "user", "samesave.sav")
    if not os.path.exists(save):
        return fails + [f"{year}: no save after the days"]
    teams, group, row, names = edition(save, year)
    print(f"      {year}: {len(teams)} teams ({', '.join(sorted(names))}), {group} group matches, row {row} "
          f"(run {took:.0f} s)")
    if len(teams) != 8:
        fails.append(f"{year}: {len(teams)} teams, the table has 8")
    if group != 12:
        fails.append(f"{year}: {group} group matches, two groups of 4 play 12")
    if not row or min(row) < 0:
        fails.append(f"{year}: no history row with a winner and a runner-up: {row}")
    if g["host"] not in names:
        fails.append(f"{year}: the host {g['host']} is not an entrant")
    if not fails:
        print(f"ok    {year}: 8 teams, two groups of 4, final played")
    return fails


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-ofc8-")
    years = [int(argv[argv.index("--only") + 1])] if "--only" in argv else sorted(GAMES)
    os.environ["RECOMP_TIER_A"] = "1"
    fails = []
    for year in years:
        fails += run(program, game, keep, year)
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
