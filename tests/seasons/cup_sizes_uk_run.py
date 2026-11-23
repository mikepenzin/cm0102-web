#!/usr/bin/env python3
"""The cups of Britain and Ireland follow the divisions (0.7.2 league sizes, slice 4; spec
docs/specs/2026-11-13-league-sizes-0.7.2-spec.md, D14; src/recomp_rt/win/season_cup_sizes_uk.c).

Usage:
    python3 tests/seasons/cup_sizes_uk_run.py <cm0102_recomp> <game folder> [--only <run>,...] [--keep <folder>]
        [--reuse] [--timeout <seconds>]
    python3 tests/seasons/cup_sizes_uk_run.py --list

Each run starts a new game with England, Northern Ireland, Scotland and Wales, changing rules on, plays on holiday
with no match engine (the season jump of tests/seasons/rules_report_check.py) and saves after 365 days, in July of
the next year (RECOMP_SAVE_AFTER_DAYS, soak_run.py). The checks of each run:
    count    the club count (+0x3e) of each cup and division at the start, from the start report (RUNS), and the
             League Cup and the Scottish cups hold the sum of their divisions (SUMS);
    played   the save's club_comp_history.dat holds a row with a winner for each of the seven cups that the
             database's own file does not hold: the cup played every round of its edition;
    box      no message box of the game, and the program saves and ends with code 0.
The divisions hold at most the cap of their class until the slices of their leagues grow them (the Conference 22,
the Northern Irish divisions 10 and 10), so the larger divisions of the spec (a Conference of 24, 24 to 29 Northern
Irish clubs) are the host test tests/seasons/test_season_cup_sizes_uk.c. A run takes 20 to 60 minutes. --reuse checks
the folders in --keep again.
"""
import os
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
sys.path.insert(0, os.path.join(ROOT, "tools", "save"))
import rules_report_check as rrc  # noqa: E402
import soak_run  # noqa: E402

NATIONS = ["England", "Northern Ireland", "Scotland", "Wales"]
CUPS = ["eng_fa_cup", "eng_cc_cup", "wel_cup", "wel_lge_cup", "nir_cup", "sco_lge_cup", "sco_chal_cup"]
# The cups that take every club of their divisions: their count is the sum of the divisions at the start.
SUMS = {"eng_cc_cup": ["eng_prm", "eng_first", "eng_second", "eng_third"],
        "sco_lge_cup": ["sco_prm", "sco_first", "sco_second", "sco_third"],
        "sco_chal_cup": ["sco_first", "sco_second", "sco_third"]}
HIST = 26       # club_comp_history.dat: +0 id, +4 competition, +8 u16 year, +0xa first, +0xe second, +0x12 third

# run: (start year, what it shows, {file: club count at the start}). The Welsh League Cup is a League of groups:
# its +0x3e is the clubs of one group (3, wel_lge_cup_init_teams 0x926712), and its 18 entrants are the groups.
RUNS = {
    "w2012": (2012, "Welsh Premier of 12: the Welsh League Cup lends 6 lower clubs, the Welsh Cup takes 20 lower",
              {"wel_first": 12, "wel_lge_cup": 3, "wel_cup": 32, "eng_cc_cup": 92, "eng_fa_cup": 152,
               "nir_cup": 32, "sco_lge_cup": 42, "sco_chal_cup": 30}),
    # The Conference is no competition of the game in these runs (no line in the report): its clubs enter the FA Cup.
    "c2015": (2015, "2015/16: the FA Cup with the Conference clubs of the database (22 until slice 8)",
              {"eng_fa_cup": 152, "eng_cc_cup": 92, "wel_first": 12, "sco_lge_cup": 42}),
    "e2019": (2019, "2019/20 (Bury left League One): the League Cup of the four League divisions",
              {"eng_fa_cup": 152}),
    "n2002": (2002, "2002/03 on the base data: a Northern Irish First Division of 8, an Irish Cup of 18 top clubs",
              {"nir_first": 8, "nir_prm": 10, "nir_cup": 32}),
}


def history(path):
    """{(competition, year): first} of a club_comp_history.dat image."""
    out = {}
    for i in range(len(path) // HIST):
        _id, comp, year, first, _second, _third, _host = struct.unpack_from("<iiHiiii", path, i * HIST)
        out[(comp, year)] = first
    return out


def database_history(database, game):
    folder = database or os.path.join(game, "Data")       # the base data (3.9.68) of the game folder
    with open(os.path.join(folder, "club_comp_history.dat"), "rb") as f:
        return history(f.read())


def check(name, work, database, game):
    import savfile
    year, _what, counts = RUNS[name]
    failed = []
    log = os.path.join(work, "stderr.txt")
    box = soak_run.first_box(log)
    if box:
        failed.append(f"message box: {box}")
    save = os.path.join(work, "user", "samesave.sav")
    report = os.path.join(work, "report.tsv")
    if not os.path.isfile(report):
        return failed + ["no report"]
    _seasons, comps = rrc.read_report(report)
    start = {c["file"]: c for c in comps if c["season"] == year and c["phase"] == "start"}
    for file, want in counts.items():
        got = start.get(file, {}).get("clubs")
        if got != want:
            failed.append(f"count: {file} {got}, want {want}")
    for cup, divisions in SUMS.items():
        if cup in start and all(d in start for d in divisions):
            want = sum(start[d]["clubs"] for d in divisions)
            if start[cup]["clubs"] != want:
                failed.append(f"count: {cup} {start[cup]['clubs']}, want {want} (its divisions)")
        else:
            failed.append(f"count: {cup} or a division of it is not in the start report")
    if not os.path.isfile(save):
        return failed + ["no save after 365 days"]
    known = database_history(database, game)
    rows = history(savfile.Sav(save).read_member("club_comp_history.dat"))
    for file in CUPS:
        c = start.get(file)
        if not c:
            failed.append(f"played: {file} is not in the start report")
            continue
        new = [(y, first) for (comp, y), first in rows.items() if comp == c["record"] and (comp, y) not in known]
        if not any(first not in (0, -1) for _y, first in new):
            failed.append(f"played: {file} (record {c['record']}) has no new history row with a winner ({new})")
    with open(log, encoding="latin-1") as f:
        notes = sorted({line.strip() for line in f if line.startswith("recomp: league sizes: ") and "cup" in line.lower()})
    for line in notes:
        print(f"  {line}")
    return failed


def main(argv):
    if "--list" in argv:
        for name, (year, what, _c) in RUNS.items():
            print(f"{name}\t{year}\t{what}")
        return 0
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    only = soak_run.arg(argv, "--only")
    names = only.split(",") if only else list(RUNS)
    keep = os.path.abspath(soak_run.arg(argv, "--keep")) if "--keep" in argv else tempfile.mkdtemp(prefix="cupuk-")
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
        failed = check(name, work, database, game)
        print(f"{name}: {'ok' if not failed else 'FAIL'}")
        for line in failed:
            print(f"  {line}")
        bad += bool(failed)
    print(f"folders: {keep}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
