#!/usr/bin/env python3
"""Clubs of a nation with no league in the game keep their regulars for a continental match (0.6.2 open item 160,
original-faults group club_regulars, tools/recomp/original_faults.py).

Usage:
    python3 tests/seasons/club_regulars_run.py <cm0102_recomp> <game folder> --save <samesave.sav> [--keep <folder>]
        [--keep-faults]

NationalTeams::mark_clubs_with_fixtures 0x7775c0 marks each club with a match within a day of a national match: 1 its
own league, 2 another competition of its nation, 3 a competition with no nation (a continental cup). The squad pick
NationalTeams::select_squad 0x76c120 then leaves out a regular of a marked club (contract +0x3d over -10,
0x76cd44..0x76cd73). The original marks a club of a continental match only when the club's nation has a league in the
game (nation +0x11c, 0x7776fc and 0x777721). So in run set pass 3 (a2001offs1, changing rules off) Bolivia picked 3
players of Club Jorge Wilstermann for a friendly on 5 October 2011, the day of Wilstermann's Merconorte Cup match.

The suggested save: run-logs/runset-pass3/run/a2001offs1/leg000/user/samesave.sav (2 August 2011, Tier A, every
nation, changing rules off). The run loads it with Tier A and plays to about 3 October 2011 with RECOMP_DUTY_LOG=1 and
RECOMP_DUTY_LOG_CLUB=Club Jorge Wilstermann (src/recomp_rt/win/duty_log.c: the club's mark after each call-up check).

The checks: the leg ends with code 0 and no message box; the call-up check for 5 October 2011 (day 277) marks
Wilstermann; the duty-clash check of tests/seasons/future/history_check.py finds no Wilstermann row in the new save.
--keep-faults runs with RECOMP_ORIGINAL_FAULTS=keep:club_regulars: it passes only when the checks find the fault.
"""
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))
import history_check as hc  # noqa: E402
import soak_run  # noqa: E402

CLUB = "Club Jorge Wilstermann"
FROM = 2011 * 366 + 214                   # the save's date (game_days, run_set.py)
TO = 2011 * 366 + 275                     # about 3 October 2011
MATCH_DAY = 277                           # 5 October 2011, from 0
CHECK = re.compile(r"recomp: duty: call-up check for day (\d+) of (\d+)")
MARK = re.compile(r"recomp: duty: " + re.escape(CLUB) + r" has mark (\d+)")


def main(argv):
    if len(argv) < 3 or "--save" not in argv:
        print(__doc__)
        return 2
    program, game_dir = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    save = os.path.abspath(argv[argv.index("--save") + 1])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-marks-")
    keep_faults = "--keep-faults" in argv
    os.environ.update({"RECOMP_TIER_A": "1", "RECOMP_DUTY_LOG": "1", "RECOMP_DUTY_LOG_CLUB": CLUB})
    if keep_faults:
        os.environ["RECOMP_ORIGINAL_FAULTS"] = "keep:club_regulars"
    o = {"program": program, "game": game_dir, "rules": "off", "database": None, "seed": 1, "start": 2001,
         "last": 2101, "nations": ["all"], "extra": [], "timeout": 7200}
    work = os.path.join(keep, "leg")
    code, took, box = soak_run.load_leg(o, work, save, TO - FROM)
    out = os.path.join(work, "user", "samesave.sav")
    other, found = [], []
    if code != 0 or box or not os.path.exists(out):
        other.append(f"the leg ended with code {code}{', box ' + box if box else ''} ({work})")
    marks, day = {}, None
    with open(os.path.join(work, "stderr.txt"), errors="replace") as f:
        for line in f:
            m = CHECK.search(line)
            if m:
                day = int(m.group(1)) if m.group(2) == "2011" else None
            m = MARK.search(line)
            if m and day is not None:
                marks[day] = int(m.group(1))
    print(f"      {CLUB}: marks by day {marks} (run {took:.0f} s)")
    if MATCH_DAY not in marks:
        other.append(f"no call-up check for day {MATCH_DAY} of 2011 in the log")
    elif marks[MATCH_DAY] == 0:
        found.append(f"day {MATCH_DAY} of 2011: {CLUB} is not marked, so its regulars can be picked")
    if os.path.exists(out):
        for check, comp, year, detail in hc.duty_clashes(hc.Game(out)):
            print(f"      {check} {comp} {year}: {detail}")
            if detail.startswith(CLUB):
                found.append(f"{check} {comp} {year}: {detail}")
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
