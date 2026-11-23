#!/usr/bin/env python3
"""A weather postponement keeps the match day's place in its fixture list (0.6.2 item 84;
src/recomp_rt/win/match_day_place.c).

Usage:
    python3 tests/seasons/match_day_place_run.py <cm0102_recomp> <game folder> --keep <folder> [--save <in.sav>]
        [--days 6] [--keep-fault] [--normal]

The save: --save, else RECOMP_MATCH_DAY_SAVE, else test-fixtures/fin-2046-postpone.sav. It is a save of the Tier A game
a2021s1 of run set pass 1 (October 2021 data, all nations, changing rules on, seed 1), made on holiday in October
2046, before 18 October (day 291). On that day the weather postpones SC Kufu-98 v SJK Seinäjoki of the Veikkausliiga,
the next match of that day's list is HJK Helsinki v JIPPO Joensuu, and without the fix it is never played.

The weather's rolls do not come back the same after a load, so the run names the match to postpone with the test
switch RECOMP_TEST_POSTPONE=7163,7286 (SC Kufu-98 v SJK Seinäjoki). It loads the save in Tier A (RECOMP_TIER_A=1, as
the save was made), plays --days game days on holiday and saves (tests/seasons/soak_run.py load_leg). The checks:
    - HJK Helsinki v JIPPO Joensuu of round 28 of 2046 has a result in the end save
    - no fixture of any competition dated from the first day after the save's day to two days before the end save's
      day is left without a result; a postponed one (result byte -2) is moved, not lost
    - the log has the line "recomp: match day: a weather postponement on day 291 of 2046 moved the place in its fixture
      list, set back"
    - no message box, and the program ends with code 0
--normal plays the normal game (no Tier A switch): the fault is in the original's code, which Tier A does not change.
--keep-fault runs with RECOMP_MATCH_DAY_PLACE=off (the original's place). The first three checks must then fail: this
is the proof that the run finds the fault. (The full replay of the Tier A leg from its 2041 checkpoint, about an hour,
lost the match on a build without the fix too.)

The fixture records of fixtures_<year>.tmp (the save's form of the fixture store), 0x4f bytes each, with offsets
counted from a point 0x10 bytes before a record (so the game's date at record +0x28 is +0x38 here): the clubs' ids at
+0x1c and +0x20, the competition word at +0x24, the year at +0x38 and the day at +0x3a, the round at +0x44, the home
goals at +0x53 (-1 not played, -2 postponed). Only records whose three dates (+0x34, +0x38, +0x3c) all fall in 2046
are read. The run deletes the loaded copy of the save and the end save when it has read them (about 700 MB).
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "save"))
import savfile  # noqa: E402
import soak_run  # noqa: E402

FIXTURES = "/Users/max/Projects/cm0102/test-fixtures"
DATABASE = os.path.join(FIXTURES, "databases", "October 2021")
HJK, JIPPO = 3735, 4341
LINE = "recomp: match day: a weather postponement on day 291 of 2046 moved the place in its fixture list, set back"


def records(sav, year):
    """(home, away, day, round, home goals) of every fixture of the year's store in the save."""
    name = f"fixtures_{year}.tmp"
    if not any(m.name == name for m in sav.members):
        return []
    d, y, out = sav.read_member(name), struct.pack("<h", year), []
    # A record is found by the year of its three dates (+0x34, +0x38, +0x3c) and a day of the year at +0x3a.
    for i in range(0, len(d) - 0x58):
        if d[i + 0x34:i + 0x36] == y and d[i + 0x38:i + 0x3a] == y and d[i + 0x3c:i + 0x3e] == y and \
                0 <= struct.unpack_from("<h", d, i + 0x3a)[0] < 366:
            h, a = struct.unpack_from("<ii", d, i + 0x1c)
            day, goals = struct.unpack_from("<h", d, i + 0x3a)[0], struct.unpack_from("<b", d, i + 0x53)[0]
            out.append((h, a, day, d[i + 0x44], goals))
    return out


def main(argv):
    if len(argv) < 4 or "--keep" not in argv:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = os.path.abspath(soak_run.arg(argv, "--keep"))
    save = os.path.abspath(soak_run.arg(argv, "--save", os.environ.get("RECOMP_MATCH_DAY_SAVE")
                                        or os.path.join(FIXTURES, "fin-2046-postpone.sav")))
    days = int(soak_run.arg(argv, "--days", 6))
    fault = "--keep-fault" in argv
    normal = "--normal" in argv
    os.environ.pop("RECOMP_TIER_A", None)
    if not normal:
        os.environ["RECOMP_TIER_A"] = "1"
    os.environ["RECOMP_TEST_POSTPONE"] = "7163,7286"
    os.environ.pop("RECOMP_MATCH_DAY_PLACE", None)
    if fault:
        os.environ["RECOMP_MATCH_DAY_PLACE"] = "off"
    o = {"program": program, "game": game, "rules": "on", "database": DATABASE, "last": 2100, "seed": 1,
         "timeout": 3600}
    work = os.path.join(keep, ("normal-" if normal else "") + ("fault-kept" if fault else "fixed"))
    code, took, box = soak_run.load_leg(o, work, save, days)
    log = os.path.join(work, "stderr.txt")
    print(f"run: code {code} after {took:.0f} s, log {log}")
    fails = []
    if code != 0:
        fails.append(f"the program ended with {code}" + (f" ({box})" if box else ""))
    end_path = os.path.join(work, "user", "samesave.sav")
    if not os.path.exists(end_path):
        return fails + ["no save at the end"]
    start, end = records(savfile.Sav(save), 2046), records(savfile.Sav(end_path), 2046)
    played0 = {(h, a, day) for h, a, day, r, g in start if g >= 0}
    first = max((day for h, a, day, r, g in start if g >= 0), default=0)
    last = max((day for h, a, day, r, g in end if g >= 0), default=0)
    lost = sorted((day, h, a, r) for h, a, day, r, g in end if first < day <= last - 2 and g == -1)
    match = [g for h, a, day, r, g in end if (h, a, r) == (HJK, JIPPO, 28)]
    print(f"the save played to day {first} of 2046, the end save to day {last}; {len(played0)} results before; "
          f"HJK v JIPPO round 28: {match}; fixtures left without a result: {len(lost)}")
    if not match or max(match) < 0:
        fails.append("HJK Helsinki v JIPPO Joensuu (round 28 of 2046) has no result")
    if lost:
        fails.append(f"{len(lost)} fixtures dated days {first + 1} to {last - 2} have no result, for example "
                     f"day {lost[0][0]} clubs {lost[0][1]} v {lost[0][2]}")
    for big in (os.path.join(work, "user", "soak.sav"), end_path):
        os.remove(big)
    text = open(log, encoding="latin-1").read()
    if LINE not in text:
        fails.append("no log line of the place set back after the postponement of day 291")
    if "message box" in text:
        fails.append("a message box")
    return fails


if __name__ == "__main__":
    fails = main(sys.argv)
    if isinstance(fails, int):
        sys.exit(fails)
    for f in fails:
        print(f"FAIL: {f}")
    print("FAIL" if fails else "PASS")
    sys.exit(1 if fails else 0)
