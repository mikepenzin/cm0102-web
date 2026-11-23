#!/usr/bin/env python3
"""Senior call-ups of players at clubs on another continent (open item 143; tools/recomp/original_faults.py, group
callups).

Usage:
    python3 tests/seasons/callups_run.py <cm0102_recomp> <game folder> --keep <folder> [--days 66] [--keep-faults]

The run loads allleagues.sav of the test fixtures (every league, Arsenal, 14.7.01; tests/fixtures/fixtures.py make
--all), goes on holiday with no setting (the clicks of tests/seasons/holiday_offers_run.py) and saves --days game
days later. RECOMP_SQUAD_LOG=1 logs each senior pick of NationalTeams::select_squad 0x76c120: the nation, the record
(friendly or other), the squad slots set after the pick, the nation's players with a club, those at a club on another
continent and those of them that the original's release rule refuses (src/recomp_rt/win/squad_log.c). The picks for
the friendlies of 30.9.01 come on 15.9.01 (day 258), so 66 days hold them. The checks:
    - the senior picks of Mali, Senegal, Guinea, Cape Verde Islands and Guinea-Bissau came. The original's rule
      refuses most of their players (Mali 35 of 38, Senegal 85 of 103), and their squads held 1, 7, 4, 1 and 1
      players (on the fix 12, 26, 24, 17 and 14)
    - each of them picked at least MIN_SQUAD (11, a side that can play with no virtual player). The other senior
      picks below MIN_SQUAD of a nation with 26 or more players with a club are listed, not failed: other gates of
      the pick can cause them
The pick keeps its other gates. With the fix, Mali's 26 players with a club that it does not pick are 12 regular
players of clubs with a competitive match on the friendly's date (the club table of
NationalTeams::mark_clubs_with_fixtures 0x7775c0 and the playing-time mood, contract +0x3d, over -10: 0x76cd44..
0x76cd73), 7 in its under-21 squad, 6 injured and 1 more (RECOMP_SQUAD_LOG_NATION=Mali logs the gate of each).
    - no message box (RECOMP_SCREEN_LOG=1: "recomp: message box from"), and the run ends with code 0
--keep-faults runs with RECOMP_ORIGINAL_FAULTS=keep:callups (the original's rule) and passes only when the squad
checks fail: the proof that they find the fault. The run takes about 20 minutes.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FIXTURES = os.path.join(HERE, "..", "fixtures", "fixtures.py")

STEPS = ["2500 click 45 165", "4500 click 150 427", "10000 click 705 572"]   # Go on Holiday, Yes, no setting
NATIONS = ["Mali", "Senegal", "Guinea", "Cape Verde Islands", "Guinea-Bissau"]
MIN_SQUAD = 11
PICK = re.compile(r"recomp: squad log: day (\d+)/(\d+) (.*?): (\w+), competition .*? rep (-?\d+), opponent (.*?) rep "
                  r"(-?\d+): (\d+) picked; (\d+) players with a club, (\d+) on another continent, "
                  r"(\d+) of them refused")


def play(program, game, keep, days, keep_faults):
    os.makedirs(keep, exist_ok=True)
    steps = os.path.join(keep, "steps.txt")
    with open(steps, "w") as f:
        f.write("\n".join(STEPS) + "\n")
    env = dict(os.environ, RECOMP_SQUAD_LOG="1", RECOMP_SCREEN_LOG="1")
    if keep_faults:
        env["RECOMP_ORIGINAL_FAULTS"] = "keep:callups"
    r = subprocess.run([sys.executable, FIXTURES, "play", program, game, "allleagues.sav", steps, keep, "--days",
                        str(days), "--set", "faster_days=on"], env=env, capture_output=True, text=True)
    path = os.path.join(keep, "stderr.txt")
    return r.returncode, open(path, errors="replace").read() if os.path.exists(path) else ""


def check(log):
    """(failures of the squad checks, other failures)."""
    picks = [m.groups() for m in PICK.finditer(log)]
    squad, other = [], []
    if not picks:
        other.append("no senior pick in the log (is RECOMP_SQUAD_LOG in this build?)")
    seen = set()
    for day, year, nation, kind, comp_rep, opp, opp_rep, picked, players, away, refused in picks:
        picked, players = int(picked), int(players)
        if nation in NATIONS:
            seen.add(nation)
            print(f"{nation} {day}/{year} {kind} v {opp}: {picked} picked of {players} with a club, {away} abroad, "
                  f"{refused} that the original's rule refuses")
        if players >= 26 and picked < MIN_SQUAD and nation not in NATIONS:
            print(f"note: {nation} on day {day}/{year} ({kind} v {opp}) picked {picked} of {players} players with a "
                  f"club ({away} abroad, {refused} refused by the original's rule)")
        if nation in NATIONS and picked < MIN_SQUAD:
            squad.append(f"{nation} on day {day}/{year} ({kind} v {opp}) picked {picked} of {players} players with a "
                         f"club ({away} abroad, {refused} refused by the original's rule)")
    for nation in NATIONS:
        if nation not in seen:
            other.append(f"no senior pick of {nation}")
    return squad, other


def main(argv):
    if len(argv) < 3 or "--keep" not in argv:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = os.path.abspath(argv[argv.index("--keep") + 1])
    days = int(argv[argv.index("--days") + 1]) if "--days" in argv else 66
    keep_faults = "--keep-faults" in argv
    code, log = play(program, game, keep, days, keep_faults)
    squad, other = check(log)
    if code != 0:
        other.append(f"the run ended with code {code}")
    if "recomp: message box from" in log:
        other.append("a message box")
    for text in squad + other:
        print("FAIL: " + text)
    if keep_faults:
        ok = bool(squad) and not other
        print("PASS: the checks find the fault" if ok else "FAIL: the checks do not find the fault")
        return 0 if ok else 1
    print("PASS" if not squad and not other else f"{len(squad) + len(other)} failures")
    return 0 if not squad and not other else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
