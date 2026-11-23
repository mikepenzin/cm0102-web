#!/usr/bin/env python3
"""The OFC Nations Cup and Africa Cup of Nations history at late starts (0.7.3, release set full2).

Usage:
    python3 tests/seasons/ofc_history_run.py <cm0102_recomp> <game folder> <out folder> [--reuse]

Three new games with changing rules on, each saved after 14 days. The history checker's reader
(tests/seasons/future/history_check.py) reads each save.

- September 2010 ODB in 2010: the data holds no OFC Nations Cup row of 2008. The history fill (GAP_FINALS) adds
  New Zealand / New Caledonia with no host (home and away, October 2007 to November 2008).
- October 2016 in 2016: the data holds no row of 2016. The fill adds New Zealand / Papua New Guinea, host Papua
  New Guinea (Port Moresby, 28 May to 11 June 2016).
- September 2012 in 2012: the data's hosts point at clubs, and the club host clear empties them. The corrections
  (docs/research/history-rows/corrections.tsv) give the Africa Cup of Nations 2012 the host Equatorial Guinea and
  the OFC Nations Cup 2012 the host Solomon Islands and the third New Zealand (Honiara, 1 to 10 June 2012; third
  place New Zealand 4-3 Solomon Islands).

At most 3 games at once. --reuse checks the saves already in the out folder. Exit code 1 on a failure.
"""
import os
import shutil
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))
import history_check as hc  # noqa: E402
import season_nl_run as snr  # noqa: E402

OFC, ACN = "OFC Nations Cup", "Africa Cup of Nations"
# (database, start year, {(competition, label): (first, second, third, host)})
GAMES = [
    ("September 2010 ODB", 2010, {(OFC, 2008): ("New Zealand", "New Caledonia", "", "")}),
    ("October 2016", 2016, {(OFC, 2016): ("New Zealand", "Papua New Guinea", "", "Papua New Guinea")}),
    ("September 2012", 2012, {(OFC, 2012): ("Tahiti", "New Caledonia", "New Zealand", "Solomon Islands"),
                              (ACN, 2012): ("Zambia", "Ivory Coast", "Mali", "Equatorial Guinea")}),
]


def play(program, game, out, db, start):
    work = os.path.join(out, db.replace(" ", "-"))
    if os.path.isdir(work):                              # rrc.run makes the folders of a new game
        shutil.rmtree(work)
    code, _ = snr.rrc.run(program, game, start, start, snr.database_folder(db), ["England"], [], work, 7200,
                          {"changing_rules": "on"}, dict(snr.BASE_ENV, RECOMP_SAVE_AFTER_DAYS="14"))
    return code


def rows_of(save, comps):
    g = hc.Game(save)
    name = lambda i: g.clubs.get(i, ("",))[0] if i >= 0 else ""
    out = {}
    for cid, year, a, b, c, h in g.rows:
        n = g.comps[cid]["name"]
        if n in comps:
            out.setdefault((n, year), []).append((name(a), name(b), name(c), name(h)))
    return out


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game, out = (os.path.abspath(a) for a in argv[:3])
    fails = []
    if "--reuse" not in argv:
        with ThreadPoolExecutor(3) as pool:
            codes = list(pool.map(lambda x: play(program, game, out, x[0], x[1]), GAMES))
        fails += [f"{db}: the run ended with code {c}" for (db, _, _), c in zip(GAMES, codes) if c != 0]
    for db, start, want in GAMES:
        save = os.path.join(out, db.replace(" ", "-"), "user", snr.smoke_run.SAVE_NAME)
        if not os.path.exists(save):
            fails.append(f"{db}: the game did not save")
            continue
        got = rows_of(save, {OFC, ACN})
        for key, row in want.items():
            if got.get(key) != [row]:
                fails.append(f"{db}: {key[0]} {key[1]} is {got.get(key)}, want {row}")
        print(f"{db}: " + "; ".join(f"{k[0]} {k[1]} {v}" for k, v in sorted(got.items()) if k[1] >= start - 8))
    print(("FAIL  " + "; ".join(fails)) if fails else "ok    OFC Nations Cup and Africa Cup of Nations rows at late starts")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
