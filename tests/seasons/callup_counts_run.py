#!/usr/bin/env python3
"""The club that sorts last in a national squad gets its call-up count (0.6.2 open item 300, original-faults group
callup_counts, tools/recomp/original_faults.py).

Usage:
    python3 tests/seasons/callup_counts_run.py <cm0102_recomp> <game folder> --save <samesave.sav> [--keep <folder>]
        [--keep-faults]

NationalTeams::record_club_call_ups 0x7771c0 counts a squad's players club by club, in the order of the club ids
(0x778ef0), and writes a club's count only when the next club starts (0x7772f7), so the club that sorts last got no
count. NationalTeams::can_schedule_club_fixture 0x776fc0 reads the counts, so a club fixture made or moved after the
squad pick could land next to the national match. In release set row t2006 (seed 73) the AFC Champions League first
leg of Urawa Red Diamonds (the highest club id in Japan's squad), made on 29 August 2007 for 30 August, moved to
12 September, the day after Japan's friendly, with 5 Urawa players in Japan's squad.

The save: leg 0 of release set row t2006, seed 73 (run-logs/0.7.3-lead/full2/release-set/t2006/leg000/user/samesave.sav,
3 August 2007; the tile database with the six extra divisions, all nations, changing rules on). The run loads it with
the row's settings (run_set.py) and plays to 1 September 2007, as leg 1 of the row does.

The checks: the leg ends with code 0 and no message box, and no fixture of Urawa Red Diamonds still to play in the new
save is within a day of a match of a side of Japan that holds DUTY_MIN or more Urawa players in its squad (the test of
the duty-clash check of tests/seasons/future/history_check.py, done here for this club alone: that check keeps only the
first case of each competition and year). The duty-clash rows of other clubs are printed and do not fail the test.
--keep-faults runs with RECOMP_ORIGINAL_FAULTS=keep:callup_counts: it passes only when the checks find the fault.
"""
import collections
import datetime
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [HERE, os.path.join(HERE, "future"), os.path.join(os.path.dirname(HERE), "smoke")]
import history_check as hc  # noqa: E402
import run_set as rs  # noqa: E402
import soak_run  # noqa: E402

CLUB = "Urawa Red Diamonds"
ROW = "t2006"
SEED = 73
FROM = 214                                # the save's day of 2007, from 0
DAYS = rs.game_days(2007, rs.TAIL_ON) - rs.game_days(2007, FROM)


def club_clashes(game, club_name, nation_name):
    """[text]: each fixture of the club still to play within a day of a match of a side named nation_name whose squad
    holds hc.DUTY_MIN or more players of the club (staff.dat +0x24 the side, +0x39 the club)."""
    club = next(c for c, v in game.clubs.items() if v[0] == club_name and c not in game.sides)
    sides = {s for s in game.sides if game.clubs[s][0] == nation_name}
    staff = game.sav.read("staff.dat")
    held = collections.Counter(hc.s32(staff, o + 0x24) for o in range(0, len(staff) - hc.STAFF + 1, hc.STAFF)
                               if hc.s32(staff, o + 0x39) == club and hc.s32(staff, o + 0x24) in sides)
    today = datetime.date(game.date[0], 1, 1).toordinal() + game.date[1]
    own, national = [], collections.defaultdict(set)
    for cid, fx in hc.fixtures(game).items():
        for day, home, away in fx:
            if day < today:
                continue
            if club in (home, away):
                own.append((day, game.comps[cid]["name"]))
            for t in (home, away):
                if t in sides:
                    national[day].add(t)
    out = []
    for day, name in sorted(own):
        for d in (day - 1, day, day + 1):
            for side in sorted(national.get(d, ())):
                if held[side] >= hc.DUTY_MIN:
                    out.append(f"{club_name} plays the {name} on {datetime.date.fromordinal(day)} with "
                               f"{held[side]} players in the squad of {nation_name}, which plays on "
                               f"{datetime.date.fromordinal(d)}")
    return out


def main(argv):
    if len(argv) < 3 or "--save" not in argv:
        print(__doc__)
        return 2
    program, game_dir = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    save = os.path.abspath(argv[argv.index("--save") + 1])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-callups-")
    keep_faults = "--keep-faults" in argv
    if keep_faults:
        os.environ["RECOMP_ORIGINAL_FAULTS"] = "keep:callup_counts"
    root = os.path.dirname(os.path.abspath(game_dir))
    o = {"databases": os.path.join(root, "test-fixtures", "databases"), "own": os.path.join(root, "own-dbs"),
         "out": keep, "data": {"3.9.60": os.path.join(root, "data-3960"), "3.9.65": os.path.join(root, "data-3965")},
         "seed": SEED}
    row = next(r for r in rs.read_set(None, seed=SEED) if r["name"] == ROW)
    so = {"program": program, "game": game_dir, "start": row["start"], "last": row["last"] + 2, "seed": row["seed"],
          "nations": row["nations"].split(","), "rules": row["changing"], "extra": row["extras"], "timeout": 14400}
    so["database"], so["option"], data_dir = rs.database_of(row, o)
    if data_dir:
        os.environ["RECOMP_DATA_DIR"] = data_dir
    os.environ.update(rs.TIER_ENV.get(row["tier"], {}))
    work = os.path.join(keep, "leg")
    code, took, box = soak_run.load_leg(so, work, save, DAYS)
    out = os.path.join(work, "user", "samesave.sav")
    other, found = [], []
    if code != 0 or box or not os.path.exists(out):
        other.append(f"the leg ended with code {code}{', box ' + box if box else ''} ({work})")
    print(f"      leg of {DAYS} days in {took:.0f} s")
    if os.path.exists(out):
        game = hc.Game(out)
        for check, comp, year, detail in hc.duty_clashes(game):
            print(f"      {check} {comp} {year}: {detail}")
        found += club_clashes(game, CLUB, "Japan")
    for text in found + other:
        print("FAIL ", text)
    if keep_faults:
        ok = bool(found) and not other
        print("PASS: the checks find the fault" if ok else "FAIL: the checks do not find the fault")
        return 0 if ok else 1
    print("PASS" if not found and not other else f"FAILED: {len(found) + len(other)}")
    return 0 if not found and not other else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
