#!/usr/bin/env python3
"""The Australian NSL plays its finals series and Grand Final in every season (open item 128,
tools/recomp/original_faults.py "Australian NSL stages").

Usage:
    python3 -I tests/seasons/aus_nsl_stages_run.py <cm0102_recomp> <game folder> --keep <folder> [--start 2001]
        [--seasons 3] [--changing-rules on|off] [--keep-faults]
    python3 -I tests/seasons/aus_nsl_stages_run.py --check <save.sav> [<save.sav> ...]

The run plays a Tier A game with Australia only (tests/seasons/soak_run.py, --env RECOMP_TIER_A=1) from --start for
--seasons seasons, with a save each 380 game days (soak_run.py --leg-days 380; it plays one leg more, which makes no
save). The Grand Final can come as late as 10 July (2004, Perth Glory v South Melbourne in a 2001 game), after a save
of 365 days (3 July), so the saves come 15 days later each season: from about 18 July. Each save checks the season
that ended before it: the save of year Y checks Y-1/Y. --check reads the saves of any run.

The NSL is the club competition of Australia named "Australian National Soccer League" (changing rules on:
"National Soccer League", later "A-League Men"; the American A-League has another nation). Its
season has a regular season, a finals series of the top six (stage 0: a League_Stage of 6 clubs, `AusNsl::
create_finals_series_stage` 0x412010; in a 2001 game it plays 5 match days of 3 matches) and a Grand Final of the
top two (stage 1, one match, 0x4121c0). The checks of a season (its fixtures from 1 August to 31 July in the
fixtures parts, history_check.fixtures):
    - finals-series  the 15 matches before the last are among exactly 6 clubs, and each pair of them meets once
    - grand-final    the last match is alone on its day, and both clubs played the finals series
    - history        the competition history row of the season's year (the year of its last match) names the two
                     Grand Final clubs as winner and runner-up (`AusNsl::record_season_honours` 0x411d90)
With the fault (RECOMP_ORIGINAL_FAULTS=keep:nsl, --keep-faults) every season after the first fails all three: the
season ends after the regular season, and the history row repeats the first season's Grand Final pair. With
--keep-faults the run proves that the checks find the fault: it passes when every save was read, the first season
passes and each later season fails.
Exit code 0 when every season passes (with --keep-faults: as above). A save must be from July to December.
"""
import datetime
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(HERE, "future"))
import history_check as hc  # noqa: E402

NSL = re.compile(r"National Soccer League|A-League", re.I)
FINALS_MATCHES = 15     # each pair of the 6 clubs once
FINALS_CLUBS = 6


def arg(argv, name, default=None):
    return argv[argv.index(name) + 1] if name in argv and argv.index(name) + 1 < len(argv) else default


def season_faults(game, cid, season):
    """[(check, detail)] of the NSL season season/season+1 in game."""
    first = datetime.date(season, 8, 1).toordinal()
    last = datetime.date(season + 1, 7, 31).toordinal()
    fx = sorted(f for f in hc.fixtures(game).get(cid, []) if first <= f[0] <= last)
    if len(fx) < FINALS_MATCHES + 1:
        return [("fixtures", f"only {len(fx)} matches from 1 August {season} to 31 July {season + 1}")]
    out = []
    finals, final = fx[-FINALS_MATCHES - 1:-1], fx[-1]
    counts = {}
    for _, home, away in finals:
        for t in (home, away):
            counts[t] = counts.get(t, 0) + 1
    pairs = {frozenset((home, away)) for _, home, away in finals}
    if len(counts) != FINALS_CLUBS or len(pairs) != FINALS_MATCHES:
        day = datetime.date.fromordinal(finals[0][0])
        out.append(("finals-series", f"the {FINALS_MATCHES} matches before the last (from {day}) are among "
                                     f"{len(counts)} clubs, {len(pairs)} pairs"))
    alone = sum(1 for f in fx if f[0] == final[0]) == 1
    if not alone or final[1] not in counts or final[2] not in counts:
        day = datetime.date.fromordinal(final[0])
        out.append(("grand-final", f"the last match ({day}, {game.club(final[1])[0]} v {game.club(final[2])[0]}) "
                                   f"{'is not alone on its day' if not alone else 'has a club from outside the finals series'}"))
    year = datetime.date.fromordinal(final[0]).year
    rows = [r for r in game.rows if r[0] == cid and r[1] == year]
    pair = {final[1], final[2]}
    if not rows:
        out.append(("history", f"no history row for {year}"))
    elif not any({r[2], r[3]} == pair for r in rows):
        r = rows[-1]
        out.append(("history", f"the {year} row names {game.club(r[2])[0]} and {game.club(r[3])[0]}, the last match "
                               f"{game.club(final[1])[0]} v {game.club(final[2])[0]}"))
    return out


def check(saves):
    """(seasons checked, [fault lines], [1 for each season with a fault, in the order of the saves])."""
    lines, seasons, failed = [], 0, []
    for path in saves:
        game = hc.Game(path)
        cids = [c for c, v in game.comps.items() if v["kind"] == "club" and NSL.search(v["name"])
                and game.nations.get(v["nation"], ("",))[0] == "Australia"]
        if len(cids) != 1:
            print(f"FAIL  {path}: {len(cids)} NSL competitions")
            lines.append(f"{path}: {len(cids)} NSL competitions")
            continue
        if game.date[1] < 181:      # day 181: 1 July (30 June in a leap year)
            print(f"FAIL  {path}: a save of day {game.date[1]} of {game.date[0]}, not July to December")
            lines.append(f"{path}: not July to December")
            continue
        season = game.date[0] - 1
        faults = season_faults(game, cids[0], season)
        seasons += 1
        failed.append(1 if faults else 0)
        state = "FAIL" if faults else "ok"
        print(f"{state}  {season}/{(season + 1) % 100:02d} ({os.path.basename(os.path.dirname(os.path.dirname(path)))})")
        for name, detail in faults:
            print(f"      {name}: {detail}")
            lines.append(f"{season}/{(season + 1) % 100:02d} {name}: {detail}")
    return seasons, lines, failed


def main(argv):
    if "--check" in argv:
        saves = argv[argv.index("--check") + 1:]
        seasons, faults, _ = check(saves)
        print("PASS" if seasons == len(saves) and not faults else f"FAILED: {len(faults)}")
        return 0 if seasons == len(saves) and not faults else 1
    if len(argv) < 3 or "--keep" not in argv:
        print(__doc__)
        return 2
    program, game_folder, keep = argv[1], argv[2], os.path.abspath(arg(argv, "--keep"))
    start, count = int(arg(argv, "--start", 2001)), int(arg(argv, "--seasons", 3))
    faults_kept = "--keep-faults" in argv
    cmd = [sys.executable, "-I", os.path.join(HERE, "soak_run.py"), program, game_folder, "--keep", keep,
           "--start", str(start), "--last", str(start + count), "--nations", "Australia",
           "--changing-rules", arg(argv, "--changing-rules", "on"), "--leg-days", "380", "--env", "RECOMP_TIER_A=1"]
    if faults_kept:
        cmd += ["--env", "RECOMP_ORIGINAL_FAULTS=keep:nsl"]
    code = subprocess.call(cmd)
    saves = [os.path.join(keep, f"leg{n:02d}", "user", "samesave.sav") for n in range(count)]
    missing = [s for s in saves if not os.path.exists(s)]
    if code or missing:
        print(f"FAIL  the game run (code {code}, {len(missing)} saves missing)")
        return 1
    seasons, faults, failed = check(saves)
    if faults_kept:
        found = seasons == count and count > 1 and failed == [0] + [1] * (count - 1)
        print("PASS (season 1 passes and each later season fails with the fault kept)" if found
              else f"FAILED: with the fault kept the seasons gave {failed} of {count} saves, not one pass and then fails")
        return 0 if found else 1
    print("PASS" if seasons == count and not faults else f"FAILED: {len(faults)}")
    return 0 if seasons == count and not faults else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
