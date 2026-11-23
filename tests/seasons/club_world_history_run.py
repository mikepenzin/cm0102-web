#!/usr/bin/env python3
"""The FIFA Club World Cup history at a 2025 start (0.6.2 open item 197).

Usage:
    python3 tests/seasons/club_world_history_run.py <cm0102_recomp> <game folder> [--keep <folder>] [--reuse]

The December 2025 data holds the Club World Cup editions to 2023. The 2025 edition (14 June to 13 July 2025 in the
United States, Chelsea 3-0 Paris Saint-Germain; en.wikipedia.org, 2025 FIFA Club World Cup) ended before a 2025
start, and the rules table plays it in 2024/25 (club-world-439), so a 2025 game never plays it. The history fill
(tools/seasons/history_fill.py, GAP_FINALS) adds its row at the new game.

A new game in 2025 with the December 2025 data and changing rules on, saved after 14 days. The history checker
(tests/seasons/future/history_check.py) reads the save: the record holds exactly one row of 2025, Chelsea / Paris
Saint-Germain, no row of 2024 (not held), and the 2023 row of the data (Manchester City / Fluminense).

A 2001 game plays the 2025 edition itself and gets no researched row of it: the fill writes only labels up to the
start year (history_fill.py, kept and gap_rows), so its history holds one 2025 row, the game's (tests/seasons/
history_fill_gap_check.py checks the rule). A run takes about 5 minutes. --reuse checks the save already in the
--keep folder again without a new run. Exit code 1 on a failure.
"""
import os
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import season_nl_run as snr  # noqa: E402

NAME = "cwc-2025"
COMP = ("FIFA Club World Championship", "FIFA Club World Cup")
WANT = {2023: ("Manchester City", "Fluminense Football Club"), 2025: ("Chelsea", "Paris Saint-Germain")}


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    program, game = argv[0], argv[1]
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else None
    reuse = "--reuse" in argv
    work = keep or tempfile.mkdtemp(prefix="cm0102-club-world-")
    save = os.path.join(work, "user", snr.smoke_run.SAVE_NAME)
    fails = []
    if not reuse:
        if os.path.isdir(work):                          # rrc.run makes the folders of a new game
            shutil.rmtree(work)
        code, _ = snr.rrc.run(program, game, 2025, 2025, snr.database_folder("December 2025"), ["England"], [], work,
                              7200, {"changing_rules": "on"}, dict(snr.BASE_ENV, RECOMP_SAVE_AFTER_DAYS="14"))
        if code != 0:
            fails.append(f"the run ended with code {code} ({work})")
    if not fails and not os.path.exists(save):
        fails.append(f"the game did not save ({work})")
    if not fails and snr.record_faults(save, work, NAME) is None:
        fails.append(f"the history checker did not read the save ({work}/check)")
    if not fails:
        rows = [x.split("\t") for x in open(os.path.join(work, "check", NAME + ".history.tsv"), encoding="latin-1")
                .read().split("\n")[1:] if x]
        cwc = [(int(r[3]), r[4].split(" (")[0], r[5].split(" (")[0]) for r in rows if r[2] in COMP]
        labels = [y for y, _, _ in cwc]
        if labels.count(2025) != 1 or 2024 in labels:
            fails.append(f"Club World Cup rows from 2023: {sorted(r for r in cwc if r[0] >= 2023)}; want one of 2025 "
                         f"and none of 2024")
        for y, pair in WANT.items():
            if (y,) + pair not in cwc:
                fails.append(f"no Club World Cup row {y} {pair[0]} / {pair[1]}")
    print(("FAIL  " + "; ".join(fails)) if fails else f"ok    Club World Cup history at a 2025 start ({work})")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
