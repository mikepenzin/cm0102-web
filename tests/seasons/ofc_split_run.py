#!/usr/bin/env python3
"""The direct and qualifying places of the OFC Nations Cup of 8 (0.7.0, open item 45 option b,
src/recomp_rt/win/season_intl_ofc.c split_draw).

Usage:
    python3 tests/seasons/ofc_split_run.py <cm0102_recomp> <game folder> [--keep <folder>] [--only 2002|2012|2028]

The real editions of 8 sent the best-ranked Oceania nations straight to the finals and the rest to a qualifying round
of the lowest-ranked: 6 straight and 2 from the round in 2002 (https://www.rsssf.org/tables/02oc.html), 7 straight
with the host and 1 from the round in 2012 (https://en.wikipedia.org/wiki/2012_OFC_Nations_Cup_qualification). The
game plays no round (Max, 3 November 2026): the qualifying places go to the next nations of the game's FIFA ranking,
which the data's ranking points seed (nation +0xa8, FIFA_RANKINGS::create_default_table 0x58dd50).

Two new games, Tier A, all nations, seed 1, changing rules on:
- 2002: the 3.9.68 database, a 2001/02 start, 510 days (a 2001 game keeps the original's dates of its first
  edition, start_edition_2001 in season_intl_rules.c: the 2002 finals play in November 2002). Its ranking points: Australia 497, New Zealand 414, Tahiti
  287, Solomon Islands 283, Fiji 262, Vanuatu 151 (the real six of 2002), then Cook Islands 147, Samoa 134, Tonga
  100, Papua New Guinea 70, American Samoa 8, New Caledonia 0.
- 2012: the September 2011 ODB database (test-fixtures/databases), a 2011/12 start, 345 days. Australia is in Asia,
  the host is Solomon Islands. Its points: New Zealand 577, Fiji 167, New Caledonia 99, Vanuatu 84, Solomon Islands
  60, Samoa 38, Tonga 23, Tahiti 19, Cook Islands 17, Papua New Guinea 0, American Samoa 0. Papua New Guinea rises
  with its results (186th in August 2012, before Tonga, Tahiti and American Samoa; real life also sent it straight).
- 2028: the October 2026 database, a 2026/27 start, 760 days: the first edition after the table, which takes the
  split of 2024 (7 straight, 1 qualifying place).
The ranking moves with the results of the months before the draw, so the checks name only the clear cases.

The checks for each game: the program ends with code 0 and shows no message box; the edition has 8 teams in two
groups of 4 and a history row (tests/seasons/ofc_eight_run.py edition); the nations of `straight` are entrants; the
nations of `out` (the last of the ranking) are not; the draw's log line (RECOMP_SEASON_INTL_LOG=1) names `places`
qualifying places and 8 - places straight, and these are the 8 entrants.
"""
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))
import ofc_eight_run  # noqa: E402
import soak_run  # noqa: E402

DATABASES = "/Users/max/Projects/cm0102/test-fixtures/databases"
GAMES = {
    2002: {"start": 2001, "days": 510,
           "straight": {"Australia", "New Zealand", "Fiji", "Tahiti", "Solomon Islands", "Vanuatu"},
           "out": {"American Samoa", "New Caledonia"}, "places": 2},
    2012: {"start": 2011, "days": 345, "database": "September 2011 ODB",
           "straight": {"New Zealand", "Fiji", "New Caledonia", "Vanuatu", "Solomon Islands"},
           "out": {"American Samoa"}, "places": 1},
    2028: {"start": 2026, "days": 760, "database": "October 2026/October 2026 Data Update", "straight": set(),
           "out": set(), "places": 1},
}
SPLIT = re.compile(r"oceania_nations (\d+): straight: (.*); qualifying places: (.*)$")


def split_line(stderr, year):
    """(straight names, qualifying names) of the draw's log line of the year in a leg's stderr, or None."""
    found = None
    with open(stderr, errors="replace") as f:
        for line in f:
            m = SPLIT.search(line.rstrip("\n"))
            if m and int(m.group(1)) == year:
                found = ([x.strip() for x in m.group(2).split(",") if x.strip()],
                         [x.strip() for x in m.group(3).split(",") if x.strip()])
    return found


def run(program, game_folder, keep, year):
    g = GAMES[year]
    database = os.path.join(DATABASES, g["database"]) if "database" in g else None
    o = {"program": program, "game": game_folder, "rules": "on", "database": database, "seed": 1, "start": g["start"],
         "last": g["start"] + 10, "nations": ["all"], "extra": [], "timeout": 3600}
    work = os.path.join(keep, f"leg{year}")
    code, took, box = soak_run.new_game_leg(o, work, g["days"])
    fails = []
    if code != 0 or box:
        fails.append(f"{year}: the run ended with code {code}{', box ' + box if box else ''} ({work})")
    save = os.path.join(work, "user", "samesave.sav")
    if not os.path.exists(save):
        return fails + [f"{year}: no save after the days"]
    teams, group, row, names = ofc_eight_run.edition(save, year)
    print(f"      {year}: {len(teams)} teams ({', '.join(sorted(names))}), {group} group matches, row {row} "
          f"(run {took:.0f} s)")
    if len(teams) != 8 or group != 12:
        fails.append(f"{year}: {len(teams)} teams and {group} group matches, two groups of 4 play 12")
    if not row or min(row) < 0:
        fails.append(f"{year}: no history row with a winner and a runner-up: {row}")
    if g["straight"] - names:
        fails.append(f"{year}: {', '.join(sorted(g['straight'] - names))} go straight by the ranking, not entrants")
    if g["out"] & names:
        fails.append(f"{year}: {', '.join(sorted(g['out'] & names))} are last in the ranking, but entrants")
    split = split_line(os.path.join(work, "stderr.txt"), year)
    print(f"      {year}: split {split}")
    if not split:
        fails.append(f"{year}: no log line of the split of direct and qualifying places")
    elif len(split[0]) != 8 - g["places"] or len(split[1]) != g["places"] or set(split[0]) | set(split[1]) != names:
        fails.append(f"{year}: the split {split} is not {8 - g['places']} straight and {g['places']} qualifying "
                     f"places that are the entrants")
    if not fails:
        print(f"ok    {year}: 8 teams, the straight places and the next of the ranking")
    return fails


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-ofcsplit-")
    years = [int(argv[argv.index("--only") + 1])] if "--only" in argv else sorted(GAMES)
    os.environ["RECOMP_TIER_A"] = "1"
    os.environ.setdefault("RECOMP_SEASON_INTL_LOG", "1")
    fails = []
    for year in years:
        fails += run(program, game, keep, year)
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
