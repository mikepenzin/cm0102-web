#!/usr/bin/env python3
"""The Scottish play-offs between two divisions (0.7.2 gap row 11; src/recomp_rt/win/season_playoffs.c, the cross
play-off).

Usage:
    python3 tests/seasons/sco_playoffs_run.py <cm0102_recomp> <game folder> [--only <run>,...] [--keep <folder>]
        [--reuse] [--timeout <seconds>]
    python3 tests/seasons/sco_playoffs_run.py --list

Real life (the rules rows, docs/research/rules-0.6.0; SPFL rules C18 to C34): from 2005/06 the 9th club of the First
Division (Championship), and of the Second Division (League One), plays the 2nd to 4th of the division below for one
place, semi-finals 9th v 4th and 3rd v 2nd, a final, all two legs. From 2013/14 the Premiership's 11th plays the
Championship's play-off: 3rd v 4th, the winner v 2nd, that winner v the 11th, all two legs. The League Two 10th plays
the Highland or Lowland League winner from 2014/15: no such league is in the game, so it stays (the nearest form).

Each run starts a new game with Scotland, changing rules on, plays on holiday with no match engine and saves after
365 days, in July of the next year (soak_run.py). The checks of each run:
    places   the direct and play-off places of the four divisions at the start (+0xbe..+0xc1, the report);
    drawn    each lower division of a play-off pair logs its draw (the cross play-off lines);
    moves    in the save (club.dat, after the season change) the final's winner plays in the upper division and the
             upper club in the lower one when a lower club won, else both stay; the divisions keep 12, 10, 10, 10;
    box      no message box of the game, and the program saves and ends with code 0.
A run takes 20 to 60 minutes. --reuse checks the folders in --keep again.
"""
import os
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(HERE)), "tools", "save"))
import rules_report_check as rrc  # noqa: E402
import soak_run  # noqa: E402

NATIONS = ["Scotland"]
COUNTS = {"sco_prm": 12, "sco_first": 10, "sco_second": 10, "sco_third": 10}
# file: (up, playoff_up, playoff_down, down) at the start of the season.
SFL = {"sco_prm": (0, 0, 0, 1), "sco_first": (1, 0, 1, 1), "sco_second": (1, 3, 1, 1), "sco_third": (1, 3, 0, 0)}
SPFL = {"sco_prm": (0, 0, 1, 1), "sco_first": (1, 3, 1, 1), "sco_second": (1, 3, 1, 1), "sco_third": (1, 3, 0, 0)}
# run: (start year, what it shows, places, the lower divisions of a play-off).
RUNS = {
    "s2012": (2012, "2012/13: the two play-offs of the Scottish Football League", SFL, ["sco_second", "sco_third"]),
    "s2013": (2013, "2013/14: the Premiership play-off (3 rounds) and the two below it", SPFL,
              ["sco_first", "sco_second", "sco_third"]),
}
CROSS = "recomp: playoffs: cross\t"
CLUB = 581              # club.dat: +0 id, +4 long name (latin-1, 0 ended), +0x57 the division (club_comp id)


def save_divisions(save):
    """{long name: division id} of the clubs of a save."""
    import savfile
    data = savfile.Sav(save).read_member("club.dat")
    out = {}
    for i in range(len(data) // CLUB):
        rec = data[i * CLUB:(i + 1) * CLUB]
        name = rec[4:rec.index(b"\0", 4)].decode("latin-1")
        out[name] = struct.unpack_from("<i", rec, 0x57)[0]
    return out


def cross_lines(log):
    """[(lower file, season, kind, fields)] of the cross play-off lines of a log."""
    out = []
    with open(log, encoding="latin-1") as f:
        for line in f:
            if line.startswith(CROSS):
                cells = line.rstrip("\n").split("\t")
                out.append((cells[1], int(cells[2]), cells[3], cells[4:]))
    return out


def check(name, work):
    year, _what, places, lowers = RUNS[name]
    failed = []
    log = os.path.join(work, "stderr.txt")
    box = soak_run.first_box(log)
    if box:
        failed.append(f"message box: {box}")
    report = os.path.join(work, "report.tsv")
    if not os.path.isfile(report):
        return failed + ["no report"]
    clubs = []
    _seasons, comps = rrc.read_report(report, clubs)
    start = {c["file"]: c for c in comps if c["season"] == year and c["phase"] == "start"}
    record = {f: c["record"] for f, c in start.items()}
    for file, want in places.items():
        c = start.get(file)
        got = (c["up"], c["playoff_up"], c["playoff_down"], c["down"]) if c else None
        if got != want:
            failed.append(f"places: {file} {got}, want {want} (up, play-off up, play-off down, down)")
    save = os.path.join(work, "user", "samesave.sav")
    if not os.path.isfile(save):
        return failed + ["no save after 365 days"]
    division = save_divisions(save)
    for file, n in COUNTS.items():
        got = sum(1 for d in division.values() if d == record.get(file))
        if got != n:
            failed.append(f"count: {file} in the save {got}, want {n}")
    lines = cross_lines(log)
    lower_won = {}
    for lower in lowers:
        drawn = [x for x in lines if x[0] == lower and x[1] == year and x[2] == "drawn"]
        final = [x for x in lines if x[0] == lower and x[1] == year and x[2] == "final"]
        if not drawn:
            failed.append(f"drawn: no cross play-off of {lower} {year}")
            continue
        if not final:
            failed.append(f"moves: no final of {lower} {year}")
            continue
        winner, winner_file, upper_club, upper_file = final[-1][3][:4]
        upper_goes_to = upper_file if winner_file == upper_file else lower
        lower_won[lower] = winner_file != upper_file
        print(f"  {lower} {year}: {winner} ({winner_file}) won, {upper_club} ({upper_file}) played")
        if division.get(winner) != record.get(upper_file):
            failed.append(f"moves: {winner} in division {division.get(winner)}, want {upper_file}")
        if division.get(upper_club) != record.get(upper_goes_to):
            failed.append(f"moves: {upper_club} in division {division.get(upper_club)}, want {upper_goes_to}")
    # The season changed and each pair swapped its direct places plus the cross play-off's place when a lower club
    # won: clubs of the start (the report's club lines) against the save.
    at_start = {c["name"]: c["division"] for c in clubs if c["season"] == year and c["phase"] == "start"}
    if not at_start:
        failed.append("moves: no club lines at the start in the report")
    pairs = list(zip(COUNTS, list(COUNTS)[1:]))
    for upper, lower in pairs if at_start else []:
        want = places[lower][0] + int(lower_won.get(lower, False))
        up = sum(1 for n, d in at_start.items() if d == record.get(lower) and division.get(n) == record.get(upper))
        down = sum(1 for n, d in at_start.items() if d == record.get(upper) and division.get(n) == record.get(lower))
        if (up, down) != (want, want):
            failed.append(f"moves: {lower} to {upper} {up} up and {down} down, want {want} each")
    return failed


def main(argv):
    if "--list" in argv:
        for name, (year, what, _p, _l) in RUNS.items():
            print(f"{name}\t{year}\t{what}")
        return 0
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    only = soak_run.arg(argv, "--only")
    names = only.split(",") if only else list(RUNS)
    keep = os.path.abspath(soak_run.arg(argv, "--keep")) if "--keep" in argv else tempfile.mkdtemp(prefix="scopo-")
    timeout = int(soak_run.arg(argv, "--timeout", 5400))
    bad = 0
    for name in names:
        year = RUNS[name][0]
        database = None if year in rrc.BASE_DATA_YEARS else rrc.season_database(year)
        work = os.path.join(keep, name)
        if "--reuse" not in argv:
            soak_run.wait_for_room()
            o = {"program": program, "game": game, "start": year, "last": year + 1, "seed": 1, "nations": NATIONS,
                 "database": database, "rules": "on", "extra": [], "timeout": timeout}
            code, secs, _box = soak_run.new_game_leg(o, work, 365)
            print(f"{name}: code {code}, {secs / 60:.0f} minutes")
            if code != 0:
                print(f"{name}: FAIL: exit {code}")
                bad += 1
        failed = check(name, work)
        print(f"{name}: {'ok' if not failed else 'FAIL'}")
        for line in failed:
            print(f"  {line}")
        bad += bool(failed)
    print(f"folders: {keep}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
