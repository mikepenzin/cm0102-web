#!/usr/bin/env python3
"""The start-year rows of the Brazilian state championships in a 2001 or a 2025 game (0.6.2 open item 189,
season_history_start.c).

Usage:
    python3 tests/seasons/state_rows_start_run.py <cm0102_recomp> <game folder> [--start 2001|2025|2098]
        [--keep <folder>]

Real life: one champion and one runner-up for each edition. In this game the region records of 3.9.68 play the
championships of Para and Ceara (season_bra_states.c), and their data rows are of other states, one a year: Alagoas
under the Paraense (ASA / CSA 2001), Amazonas under the Cearense (Atletico Rio Negro AM / Nacional AM 2001). At the
creation the game writes its own 2001 row of each (League::record_season_honours 0x684640, 0x4afce0 at 0x684771), and
the data row of 2001 goes. Before item 189 the data row stayed (the rule of item 176 for rows of another state), so
2001 held two rows. The database of a 2025 start holds the same kind of row: Maranhao under the Cearense (Maranhao AC /
Imperatriz 2025) and the Federal District under the Mato-Grossense (Gama / Capital 2025). A date jump (RECOMP_DATE_JUMP=1)
takes that database and moves its rows on to the start year, so a 2098 start holds them under 2098.

--start 2001 (the default): one new game as the run-set row a2001to2300, Tier A, every nation, seed 1, changing
rules on, the 3.9.68 database, 10 days, a save. --start 2025: as the run-set row b2025, Tier B, every nation, seed 1,
changing rules on, the database that the run set takes for 2025 (gate_run.database: the own databases in
/Users/max/Projects/cm0102/own-dbs, else test-fixtures/databases), 10 days, a save. --start 2098: as the run-set row
jump2098, a date jump (RECOMP_DATE_JUMP=1, the full world), every nation, seed 1, changing rules on, the December 2025
database (run_set.py database_of), 10 days, a save. The checks: the run ends with code 0 and
shows no message box; each record holds one row of the start year, and it is not the data row of another state.
"""
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))
import gate_run  # noqa: E402
import history_check  # noqa: E402
import soak_run  # noqa: E402

DAYS = 10
OWN_DBS = "/Users/max/Projects/cm0102/own-dbs"                  # as run_set.py
DATABASES = "/Users/max/Projects/cm0102/test-fixtures/databases"
# per start year: the tier, and the clubs of the data rows of another state that the start year's label holds
STARTS = {
    2001: ("A", {"Campeonato Paraense": {"Agremiação Sportiva Arapiraquense", "Centro Sportivo Alagoano"},
                 "Campeonato Cearense": {"Atlético Rio Negro Clube (AM)", "Nacional Futebol Clube (AM)"}}),
    2025: ("B", {"Campeonato Cearense": {"Maranhão AC", "Sociedade Imperatriz de Desportos (MA)"},
                 "Campeonato Mato-Grossense": {"Sociedade Esportiva do Gama (DF)", "Capital CF"}}),
    2098: ("jump", {"Campeonato Cearense": {"Maranhão AC", "Sociedade Imperatriz de Desportos (MA)"},
                    "Campeonato Mato-Grossense": {"Sociedade Esportiva do Gama (DF)", "Capital CF"}}),
}
JUMP_DATABASE = "December 2025"                               # run_set.py JUMP_DATABASE


def rows_of(save, year, names):
    """{record name: [(winner, runner-up) of each row labelled year]} for the records."""
    game = history_check.Game(save)
    out = {name: [] for name in names}
    for cid, c in game.comps.items():
        if c["name"] in out:
            out[c["name"]] += [(game.club(r[2])[0], game.club(r[3])[0]) for r in game.rows
                               if r[0] == cid and r[1] == year]
    return out


def check(rows, year, data):
    fails = []
    for name, rs in rows.items():
        print(f"      {name} {year}: {'; '.join(f'{a} / {b}' for a, b in rs) or 'no row'}")
        if len(rs) != 1:
            fails.append(f"{name} holds {len(rs)} rows of {year}, not 1")
        elif rs[0][0] in data[name] or rs[0][1] in data[name]:
            fails.append(f"{name} {year} is the data row of another state, not the game's row")
        else:
            print(f"ok    {name}: one row of {year}, the game's")
    return fails


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-staterows-")
    start = int(argv[argv.index("--start") + 1]) if "--start" in argv else 2001
    if start not in STARTS:
        print(f"--start: one of {', '.join(map(str, STARTS))}")
        return 2
    tier, data = STARTS[start]
    os.environ.pop("RECOMP_TIER_A", None)
    os.environ.pop("RECOMP_DATE_JUMP", None)
    os.environ.update({"A": {"RECOMP_TIER_A": "1"}, "jump": {"RECOMP_DATE_JUMP": "1"}}.get(tier, {}))
    if tier == "jump":
        names = sorted(n for n in os.listdir(DATABASES) if n.startswith(JUMP_DATABASE))
        database = os.path.join(DATABASES, names[0]) if names else None
        label = names[0] if names else f"no {JUMP_DATABASE} folder in {DATABASES}"
    else:
        args, label = gate_run.database(start, {"run": keep, "own": OWN_DBS, "databases": DATABASES})
        database = args[1] if args else None
    print(f"      database {label}")
    if tier == "jump" and not database:
        print("FAIL  no database")
        return 1
    o = {"program": program, "game": game, "rules": "on", "database": database, "seed": 1, "start": start, "last": 2300,
         "nations": ["all"], "extra": [], "timeout": 5400}
    work = os.path.join(keep, "leg")
    code, took, box = soak_run.new_game_leg(o, work, DAYS)
    fails = []
    if code != 0 or box:
        fails.append(f"the run ended with code {code}{', box ' + box if box else ''} ({work})")
    save = os.path.join(work, "user", "samesave.sav")
    if not os.path.exists(save):
        fails.append("no save after the days")
    else:
        print(f"      run {took:.0f} s")
        fails += check(rows_of(save, start, data), start, data)
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
