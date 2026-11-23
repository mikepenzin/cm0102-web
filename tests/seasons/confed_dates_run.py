#!/usr/bin/env python3
"""The Confederations Cup in June (0.6.2 open items 157 to 159, original-faults group confed,
src/recomp_rt/win/season_intl_rules.c confed_june_shift).

Usage:
    python3 tests/seasons/confed_dates_run.py <cm0102_recomp> <game folder> [--keep <folder>] [--keep-faults]

The original plays the Confederations Cup of every odd year from 29 July to 9 August (0x58b601, 0x58b984), in the days
of its own Club World Championship and the European club qualifiers. In the changing-rules-off games of run set pass 3
clubs of those cups played the day before, of or after a match of a side that held 3 or more of their players
(the duty-clash check of tests/seasons/future/history_check.py). The fix moves each such edition to the Saturday on
or before 18 June, so the whole edition plays in June, as the real editions mostly did.

One new game, Tier A, every nation, seed 1, changing rules off (the base game: the 3.9.68 database, 2001/02). Leg 1
saves on 13 June 2003, after the squad picks of the June edition (from 14 June 2003; a finals squad is picked 10 days
before its first match, NationalTeams::select_squad 0x76c120). Leg 2 loads that save and saves on 25 July 2003, after
the squad picks of the original's July edition.

The checks: each leg ends with code 0 and no message box; every Confederations Cup match of 2003 lies in June (the
fixtures of the second save); the duty-clash check finds, in either save, no club match within a day of a match of a
side that holds 3 or more of its players, on a day within a day of a Confederations Cup match (other rows belong to
other items). With RECOMP_DUTY_LOG=1 (src/recomp_rt/win/duty_log.c) the script also prints
how many club matches the original's call-up move (0x5abb10) moved and how many stayed.
--keep-faults runs with RECOMP_ORIGINAL_FAULTS=keep:confed (the original's dates). It passes only when the June check
fails and the legs end well: the proof that the check finds the fault.
"""
import datetime
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))
import history_check as hc  # noqa: E402
import soak_run  # noqa: E402

SAVE = "samesave.sav"
FIRST_DAY = 2001 * 366 + 181             # the first human turn of a new game (game_days, run_set.py)
# The save comes 12 days after the target here: a new game's first human turn is before day 181, and game_days counts
# 366 days a year (red run on main: targets 163 and 205 saved on days 175 and 217 of 2003).
SAVE_1 = 2003 * 366 + 151                 # saves on about 13 June 2003 (day 163 from 0)
SAVE_2 = 2003 * 366 + 193                 # saves on about 25 July 2003


def confed_days(game):
    """The days (ordinals) of the Confederations Cup matches of 2003."""
    ids = [cid for cid, c in game.comps.items() if c["name"] == "Confederations Cup"]
    return sorted({d for cid in ids for d, h, a in hc.fixtures(game).get(cid, ())
                   if datetime.date.fromordinal(d).year == 2003})


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game_dir = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-confed-")
    os.environ["RECOMP_TIER_A"] = "1"
    os.environ["RECOMP_DUTY_LOG"] = "1"
    keep_faults = "--keep-faults" in argv
    if keep_faults:
        os.environ["RECOMP_ORIGINAL_FAULTS"] = "keep:confed"
    o = {"program": program, "game": game_dir, "rules": "off", "database": None, "seed": 1, "start": 2001,
         "last": 2005, "nations": ["all"], "extra": [], "timeout": 7200}
    fails, june, saves = [], [], []
    legs = ((soak_run.new_game_leg, SAVE_1 - FIRST_DAY), (soak_run.load_leg, SAVE_2 - SAVE_1))
    for k, (run, days) in enumerate(legs):
        work = os.path.join(keep, f"leg{k + 1}")
        code, took, box = run(o, work, days) if k == 0 else run(o, work, saves[-1], days)
        save = os.path.join(work, "user", SAVE)
        print(f"      leg {k + 1}: code {code}, {took:.0f} s")
        if code != 0 or box or not os.path.exists(save):
            fails.append(f"leg {k + 1} ended with code {code}{', box ' + box if box else ''}, save "
                         f"{'there' if os.path.exists(save) else 'missing'} ({work})")
            break
        saves.append(save)
        with open(os.path.join(work, "stderr.txt"), errors="replace") as f:
            log = f.read()
        print(f"      leg {k + 1}: the call-up move moved {log.count('recomp: duty: moved ')} club matches, "
              f"{log.count('not removed, stays')} stayed")
    games = [hc.Game(save) for save in saves]
    days = confed_days(games[-1]) if len(games) == len(legs) else []
    near = {d + k for d in days for k in (-1, 0, 1)}
    for k, game in enumerate(games):
        print(f"      save {k + 1}: {game.date}")
        for check, comp, year, detail in hc.duty_clashes(game):
            m = re.search(r" on (\d{4}-\d\d-\d\d) ", detail)
            if m and datetime.date.fromisoformat(m.group(1)).toordinal() in near:
                fails.append(f"save {k + 1}: {check} {comp} {year}: {detail}")
            else:
                print(f"      not a Confederations Cup day: {check} {comp} {year}: {detail}")
        if k == 1:
            print("      Confederations Cup 2003: " + ", ".join(str(datetime.date.fromordinal(d)) for d in days))
            if not days:
                fails.append("no Confederations Cup match in 2003")
            out = [d for d in days if datetime.date.fromordinal(d).month != 6]
            if out:
                june.append(f"{len(out)} Confederations Cup days of 2003 outside June, the first "
                            f"{datetime.date.fromordinal(out[0])}")
    for f in june + fails:
        print("FAIL ", f)
    if keep_faults:
        legs_ok = len(saves) == len(legs)
        ok = bool(june) and legs_ok
        print("PASS: the June check finds the fault" if ok else "FAIL: the June check does not find the fault")
        return 0 if ok else 1
    fails += june
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
