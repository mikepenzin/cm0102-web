#!/usr/bin/env python3
"""The OFC Nations Cup of 2008: one group of 4, home and away, no knockout (0.6.2 open item 45,
src/recomp_rt/win/season_intl_ofc.c).

Usage:
    python3 tests/seasons/ofc_2008_run.py <cm0102_recomp> <game folder> [--keep <folder>] [--only 2007|2008]

The real edition (https://www.rsssf.org/tables/08oc.html, int-continental-87): New Zealand, New Caledonia, Fiji and
Vanuatu in one group, home and away, 17 October 2007 to 19 November 2008, also the second stage of the OFC World Cup
qualifying. The finals table gives 4 teams (season_intl_table.c); the original's class plays 6 in two groups of 3 and
a knockout.

Two new games, Tier A, the 3.9.68 database, seed 1, changing rules on:
- 2007: all nations, a 2007/08 start, 300 days, a save in the pause of the edition (May 2008), then the save loaded
  and 200 days more. The draw comes before the first round (17 November 2007), so the edition plays on the real days.
- 2008: England's leagues (the national sides of every nation play), a 2008/09 start, 170 days. The game is made on 1
  January 2008, after the draw day and the two rounds of November 2007, so the edition plays on the days still to
  come (from 14 June to 19 November 2008). A 2008/09 start with all nations stops at "ire_prm..cpp 82" with or without
  this change, with the 3.9.68 and the February 2008 database (0.6.2 open item 123).

The checks for each game: the program ends with code 0 and shows no message box; the edition of 2008 has 4 teams with
New Zealand among them, 12 matches, each team at home once against each other team (home and away), no other match
(no knockout), its first and last match in the window of the game's calendar, and a history row with a winner and a
runner-up of the 4.
"""
import collections
import datetime
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))
import history_check  # noqa: E402
import soak_run  # noqa: E402

D = datetime.date
# first: the first match day of the edition; last: its last match day, and LATE days more for a match that a clash
# moves (add_fixture 0x5aa680 moves a clashing fixture by days).
GAMES = {
    2007: {"start": 2007, "days": 300, "load_days": 200, "first": D(2007, 11, 17), "last": D(2008, 9, 10),
           "nations": ["all"]},
    2008: {"start": 2008, "days": 170, "first": D(2008, 6, 14), "last": D(2008, 11, 19), "nations": ["England"]},
}
LATE = 21


def edition(save, year):
    """(teams, [(day, home, away)] of the edition, history row or None, nation names of the teams) of the OFC Nations
    Cup edition of the year as the history checker splits the fixtures into editions, in a save."""
    game = history_check.Game(save)
    ids = {cid for cid, c in game.comps.items() if c["name"].startswith(("OFC", "Oceania Nations"))}
    fx = sorted(f for cid in ids for f in history_check.fixtures(game).get(cid, []))
    eds = [(first, last, t) for y, first, last, t in history_check.editions_of(fx) if y == year]
    fx = [f for f in fx if eds and eds[0][0] <= f[0] <= eds[0][1]]
    teams = eds[0][2] if eds else set()
    row = next(((a, b) for cid, y, a, b, c, h in game.rows if cid in ids and y == year), None)
    names = {game.nations.get(game.clubs.get(t, ("", -1))[1], ("", ""))[0] for t in teams}
    return teams, fx, row, names


def run(program, game_folder, keep, year):
    g = GAMES[year]
    o = {"program": program, "game": game_folder, "rules": "on", "database": None, "seed": 1, "start": g["start"],
         "last": g["start"] + 10, "nations": g["nations"], "extra": [], "timeout": 3600}
    work = os.path.join(keep, f"leg{year}")
    code, took, box = soak_run.new_game_leg(o, work, g["days"])
    fails = []
    if code != 0 or box:
        fails.append(f"{year}: the run ended with code {code}{', box ' + box if box else ''} ({work})")
    save = os.path.join(work, "user", "samesave.sav")
    if not os.path.exists(save):
        return fails + [f"{year}: no save after the days"]
    if "load_days" in g:
        work = os.path.join(keep, f"leg{year}-load")
        code, more, box = soak_run.load_leg(o, work, save, g["load_days"])
        took += more
        if code != 0 or box:
            fails.append(f"{year}: the loaded run ended with code {code}{', box ' + box if box else ''} ({work})")
        save = os.path.join(work, "user", "samesave.sav")
        if not os.path.exists(save):
            return fails + [f"{year}: no save after the days of the loaded game"]
    teams, fx, row, names = edition(save, 2008)
    pairs = collections.Counter((home, away) for _, home, away in fx)
    first = D.fromordinal(fx[0][0]) if fx else None
    last = D.fromordinal(fx[-1][0]) if fx else None
    print(f"      {year}: {len(teams)} teams ({', '.join(sorted(names))}), {len(fx)} matches from {first} to {last}, "
          f"row {row} (run {took:.0f} s)")
    if len(teams) != 4:
        fails.append(f"{year}: {len(teams)} teams, the table has 4")
    if "New Zealand" not in names:
        fails.append(f"{year}: New Zealand is not an entrant")
    if len(fx) != 12 or sorted(pairs.values()) != [1] * 12 or any(h == a for h, a in pairs):
        fails.append(f"{year}: {len(fx)} matches, home and away in a group of 4 gives 12, each pair once at each home "
                     f"({dict(pairs)})")
    if fx and not (g["first"] <= first <= g["first"] + datetime.timedelta(LATE)):
        fails.append(f"{year}: the first match is on {first}, the calendar's first day is {g['first']}")
    if fx and not (g["last"] <= last <= g["last"] + datetime.timedelta(LATE)):
        fails.append(f"{year}: the last match is on {last}, the calendar's last day is {g['last']}")
    if not row or min(row) < 0 or not set(row) <= teams:
        fails.append(f"{year}: no history row with a winner and a runner-up of the 4: {row}")
    if not fails:
        print(f"ok    {year}: 4 teams, home and away, no knockout, from {first} to {last}")
    return fails


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-ofc2008-")
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
