#!/usr/bin/env python3
"""The OFC Nations Cup of 2008: one group of 4, home and away, no knockout (0.6.2 open item 45,
src/recomp_rt/win/season_intl_ofc.c).

Usage:
    python3 tests/seasons/ofc_2008_run.py <cm0102_recomp> <game folder> [--keep <folder>] [--only 2007|2008|crossed]

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

And one loaded game:
- crossed: a save that crossed builds (the browser's report of 25 November 2026, season_intl_ofc.c one_group_unstaged):
  a 2001/02 game with changing rules on, saved on 19 April 2008, whose 2008 edition an older build set up (two stages,
  one meeting, +0xa7 2) and this build drew (one group of 4, no group B). It stopped on 18 June 2008 with "call to 0".
  Loaded, holiday, 240 days. The save: RECOMP_OFC_CROSSED_SAVE, else test-fixtures/ofc-2008-crossed.sav; with neither
  the run fails. The checks: code 0, no message box, the log names the repair at the load and the edition's end
  with 3 matches each, and the edition has 4 teams, 6 matches (each pair once: the older build's one meeting) and a
  history row with a winner and a runner-up of the 4.

The checks for each new game: the program ends with code 0 and shows no message box; the edition of 2008 has 4 teams with
New Zealand among them, 12 matches, each team at home once against each other team (home and away), no other match
(no knockout), its first and last match in the window of the game's calendar, and a history row with a winner and a
runner-up of the 4.
"""
import collections
import datetime
import os
import shutil
import subprocess
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
CROSSED_SAVE = os.environ.get("RECOMP_OFC_CROSSED_SAVE") or "/Users/max/Projects/cm0102/test-fixtures/ofc-2008-crossed.sav"
CROSSED_DAYS = 240


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


def crossed(program, game_folder, keep):
    """The save that crossed builds: loaded, holiday, CROSSED_DAYS days, then the checks of the docstring."""
    sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
    import big_save_run
    import smoke_run
    if not os.path.isfile(CROSSED_SAVE):
        return [f"crossed: no save {CROSSED_SAVE} (RECOMP_OFC_CROSSED_SAVE)"]
    work = os.path.join(keep, "crossed")
    user, home, shots = (os.path.join(work, d) for d in ("user", "home", "shots"))
    for d in (user, home, shots):
        os.makedirs(d, exist_ok=True)
    shutil.copyfile(CROSSED_SAVE, os.path.join(user, os.path.basename(CROSSED_SAVE)))
    with open(os.path.join(user, "settings.txt"), "w") as f:
        f.write("display = classic\ndatabase = 3.9.68\nchanging_rules = on\n")
    # As big_save_run.play, with the log in a file: the game writes the data's names in Latin-1, not UTF-8.
    steps = big_save_run.load_steps() + ["3000000 quit"]
    env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
               RECOMP_USER_DIR=user, RECOMP_FIXED_CLOCK="1", RECOMP_SAVE_AFTER_DAYS=str(CROSSED_DAYS),
               RECOMP_SEASON_INTL_LOG="1", RECOMP_SCRIPT="; ".join(smoke_run.scaled_steps("classic", shots, steps)))
    log_path = os.path.join(work, "stderr.txt")
    with open(log_path, "w") as log:
        try:
            code = subprocess.run([program, "--game", game_folder, "-seed", "1"], env=env, stdout=subprocess.DEVNULL,
                                  stderr=log, timeout=7200).returncode
        except subprocess.TimeoutExpired:
            code = "timeout"
    with open(log_path, encoding="latin-1") as f:
        err = f.read()
    fails = []
    if code != 0 or "recomp: message box from" in err:
        fails.append(f"crossed: the run ended with code {code} or showed a box ({work})")
    if "oceania_nations 2008: one group with two stages and no group B (a save of an older build): no stage, " \
       "3 matches each (load)" not in err:
        fails.append("crossed: the log names no repair of the 2008 edition at the load")
    if "oceania_nations 2008: group A 4 rows, 1 meetings, 3 matches each" not in err:
        fails.append("crossed: the 2008 edition did not end with its group of 4 rows and 3 matches each")
    save = os.path.join(work, "user", "samesave.sav")
    if not os.path.exists(save):
        return fails + ["crossed: no save after the days"]
    teams, fx, row, names = edition(save, 2008)
    pairs = collections.Counter(frozenset((home, away)) for _, home, away in fx)
    print(f"      crossed: {len(teams)} teams ({', '.join(sorted(names))}), {len(fx)} matches, row {row}")
    if len(teams) != 4 or len(fx) != 6 or sorted(pairs.values()) != [1] * 6:
        fails.append(f"crossed: {len(teams)} teams and {len(fx)} matches, a group of 4 that meets once gives 6")
    if not row or min(row) < 0 or not set(row) <= teams:
        fails.append(f"crossed: no history row with a winner and a runner-up of the 4: {row}")
    if not fails:
        print("ok    crossed: the 2008 edition ends with its group of 4")
    return fails


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-ofc2008-")
    only = argv[argv.index("--only") + 1] if "--only" in argv else None
    years = [] if only == "crossed" else [int(only)] if only else sorted(GAMES)
    fails = []
    if only in (None, "crossed"):
        fails += crossed(program, game, keep)
    os.environ["RECOMP_TIER_A"] = "1"
    for year in years:
        fails += run(program, game, keep, year)
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
