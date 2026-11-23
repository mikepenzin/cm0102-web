#!/usr/bin/env python3
"""History rows that the 3.9.60 or the 3.9.65 data holds twice, in a 2001 game (0.7.2 open item 227,
season_history.c, the duplicate rows).

Usage:
    python3 tests/seasons/dup_rows_start_run.py <cm0102_recomp> <game folder> [--database 3.9.60|3.9.65]
        [--rules on|off] [--folder <data folder>] [--keep <folder>]

The club_comp_history.dat of 3.9.60 holds four rows twice (the same competition, label year and four places):
African Super Cup 2000 Hearts of Oak / Zamalek, Merconorte Cup 2000 Atletico Nacional / Millonarios, African Cup
Winners' Cup 1983 and the Rio de Janeiro State Championship 1979. 3.9.65 holds the first three and five Portuguese
rows of 2001 (one copy of each stays). The winners tab 0x4991f0 then shows the year twice. A season database with a
shift plan has the second copy cleared at the load (open item 3); a 2001 start has no plan, so before item 227 the
copies stayed. The 3.9.68 data (the base game, Q152) keeps its rows: the same-save set checks that.

One new game: 2001, England, seed 1, changing rules on (default), the option database 3.9.60 or 3.9.65 with
RECOMP_DATA_DIR at the data folder (default /Users/max/Projects/cm0102/data-3960 or data-3965), as run_set.py of the
release tests loads these data sets, 10 days, a save. The checks: the run ends with code 0 and shows
no message box; the save holds no history row twice; the African Super Cup and the Merconorte Cup hold one row of
2000 each; the load line names the data set (open item 230: the signature table knows the shipped folders).
"""
import collections
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))
import history_check  # noqa: E402
import soak_run  # noqa: E402

DAYS = 10
FOLDERS = {"3.9.60": "/Users/max/Projects/cm0102/data-3960", "3.9.65": "/Users/max/Projects/cm0102/data-3965"}
ONE_ROW = ("African Super Cup", "Merconorte Cup")      # the two faults of item 227 (data names), label 2000


def check(save):
    game = history_check.Game(save)
    fails = []
    seen = collections.Counter(game.rows)
    for row, n in sorted(seen.items()):
        if n > 1:
            name = game.comps[row[0]]["name"]
            fails.append(f"{name} {row[1]}: {game.club(row[2])[0]} / {game.club(row[3])[0]} held {n} times")
    for want in ONE_ROW:
        ids = [cid for cid, c in game.comps.items() if c["name"] == want]
        rows = [r for r in game.rows if r[0] in ids and r[1] == 2000]
        print(f"      {want} 2000: {'; '.join(f'{game.club(r[2])[0]} / {game.club(r[3])[0]}' for r in rows) or 'no row'}")
        if len(rows) != 1:
            fails.append(f"{want} holds {len(rows)} rows of 2000, not 1")
    if not fails:
        print(f"ok    no history row twice ({len(game.rows)} rows)")
    return fails


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-duprows-")
    db = argv[argv.index("--database") + 1] if "--database" in argv else "3.9.60"
    rules = argv[argv.index("--rules") + 1] if "--rules" in argv else "on"
    if db not in FOLDERS:
        print(f"--database: one of {', '.join(FOLDERS)}")
        return 2
    folder = argv[argv.index("--folder") + 1] if "--folder" in argv else FOLDERS[db]
    if not os.path.exists(os.path.join(folder, "club_comp_history.dat")):
        print(f"FAIL  no data folder {folder}")
        return 1
    print(f"      database {db} ({folder}), changing rules {rules}")
    os.environ.pop("RECOMP_TIER_A", None)
    os.environ.pop("RECOMP_DATE_JUMP", None)
    os.environ["RECOMP_DATA_DIR"] = folder                  # files.c data_dir_find: the folder of the data set

    def settings(user, rules_, database):
        with open(os.path.join(user, "settings.txt"), "w") as f:
            f.write(f"display = classic\nchanging_rules = {rules_}\nfaster_days = on\ndatabase = {db}\n")
    soak_run.settings = settings
    o = {"program": program, "game": game, "rules": rules, "database": None, "seed": 1, "start": 2001,
         "last": 2300, "nations": ["England"], "extra": [], "timeout": 3600}
    work = os.path.join(keep, "leg")
    code, took, box = soak_run.new_game_leg(o, work, DAYS)
    fails = []
    if code != 0 or box:
        fails.append(f"the run ended with code {code}{', box ' + box if box else ''} ({work})")
    want = f"recomp: season dates: data loaded from a database folder ({db}) in 2001"
    try:
        with open(os.path.join(work, "stderr.txt"), errors="replace") as f:
            named = any(want in line for line in f)
    except OSError:
        named = False
    if named:
        print(f"ok    the game knows the data set: {want}")
    else:
        fails.append(f"no line '{want}': the data set is not known (open item 230)")
    save = os.path.join(work, "user", "samesave.sav")
    if not os.path.exists(save):
        fails.append("no save after the days")
    else:
        print(f"      run {took:.0f} s")
        fails += check(save)
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
