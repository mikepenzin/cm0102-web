#!/usr/bin/env python3
"""The Ekstraklasa in its real format of each season (GitHub issue 92, 0.6.2 open item 221).

Usage:
    python3 tests/seasons/pol_first_run.py <cm0102_recomp> <game folder> [--only 2002,2013,2025,long,long2,long3] [--keep <folder>]
        [--reuse] [--timeout <seconds>]

Each run starts a new game in a season with all nations (the rows of the Select League(s) screen differ by
database, so a click on Poland alone is not safe), changing rules on, and plays that one season
(RECOMP_SEASON_JUMP = the start year: the rules report of tests/seasons/rules_report_check.py, the end report of that
season, then the program ends). The format of the rules table (docs/research/rules-0.6.0/gre-pol-cro.tsv, pol_first):
    2002  Summer 2002 (own database): one table of 16 clubs, 30 rounds.
    2013  Autumn 2013: 16 clubs, 30 rounds, then the top 8 and the bottom 8 play 7 more, the points halved.
    2025  December 2025: one table of 18 clubs, 34 rounds.
The checks:
    report  the Polish First Division row of the start report (rules report, file pol_first) has the clubs of the row,
            each pair meeting twice, the places of the season (PLACES); the end report has the same clubs, each with 2 (n - 1)
            matches of the first phase (+0xa7);
    log     RECOMP_POL_FIRST_LOG (src/recomp_rt/win/season_pol_first.c) names one table of n clubs, and in 2013 the
            split (8 and 8 clubs, 7 dates, points halved);
    places  both leagues' places of the season (PLACES, the rules table);
    history the club history records the final table (Club_History::record_comp_season 0x442e80), with the 30 matches
            before the split;
    box     no message box.
    long2   a December 2025 game played to 2026/27: the I liga's own play-off is drawn, both leagues hold 18 clubs.
    long    a 2002 game played to 2005/06: the Ekstraklasa holds 16, 14, 14 and 16 clubs, the I liga 18, and both have
            each season's places.
A run takes about 20 to 40 minutes; they run one after the other. --reuse checks the logs in --keep again.
Exit code 1 on a failure.
"""
import os
import re
import sys
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import rules_report_check as rrc  # noqa: E402

DATABASES = "/Users/max/Projects/cm0102/test-fixtures/databases"
OWN_DBS = "/Users/max/Projects/cm0102/own-dbs"
# start year: (database, own zip or None, clubs, split: (top, bottom, dates, points) or None)
RUNS = {2002: ("Summer 2002", True, 16, None),
        2013: ("Autumn 2013", False, 16, (8, 8, 7, "halved")),
        2025: ("December 2025", False, 18, None)}
# The places of each season (rules table, pol_first and pol_second): the Ekstraklasa (up, play-off up, play-off down,
# down) and the I liga (up, play-off up, play-off down, down). The I liga's play-off places are its play-off against
# the Ekstraklasa (2002/03 2 pairs, 2003/04 to 2008/09 1) or its own play-off of 3rd to 6th (4, from 2019/20); its
# relegation play-offs against the third tier have no opponent in the game (0).
PLACES = {2002: ((0, 0, 2, 3), (1, 2, 0, 6)), 2003: ((0, 0, 1, 2), (2, 1, 0, 4)), 2004: ((0, 0, 1, 1), (3, 1, 0, 2)),
          2005: ((0, 0, 1, 2), (2, 1, 0, 3)), 2013: ((0, 0, 0, 2), (2, 0, 0, 3)), 2025: ((0, 0, 0, 3), (2, 4, 0, 3)),
          2026: ((0, 0, 0, 3), (2, 4, 0, 3))}
# The long run: a 2002 game to the end of 2005/06, the Ekstraklasa's clubs of each season (16, then 3 down and 1 up
# with the play-off pairs swapping: 14; 2 and 2: 14; 1 and 3: 16).
LONG = (2002, 2005, {2002: 16, 2003: 14, 2004: 14, 2005: 16})
# The second long run: a December 2025 game to the end of 2026/27. The I liga's own play-off of 3rd to 6th (2025/26) must
# be drawn and played before the Polish season change (log line of season_playoffs.c), and both leagues keep 18 clubs.
LONG2 = (2025, 2026, {2025: 18, 2026: 18})
# The third long run: a March 2020 game (2019/20) to the end of 2020/21. The I liga's own play-off of 2019/20 has two
# rounds, so its final falls after the Polish update day: the season change of the Polish leagues must wait for it
# (season_pol_playoff.c), and its winner goes up with the 2 direct places.
LONG3 = (2019, 2020, {2019: 16, 2020: 16})
LONG3_DB = "March 2020"


def database(year, work, name=None):
    name, own, _, _ = (name, False, 0, None) if name else RUNS[year]
    if own:
        folder = os.path.join(work, "owndb")
        path = os.path.join(folder, name)
        if not os.path.isdir(path):
            with zipfile.ZipFile(os.path.join(OWN_DBS, name + ".zip")) as z:
                z.extractall(folder)
        return path
    names = sorted(n for n in os.listdir(DATABASES) if n.startswith(name))
    return rrc.data_folder(os.path.join(DATABASES, names[0]))


def report_rows(path, year, file="pol_first"):
    """{phase: row} of the rows of a league file in a season of the rules report."""
    out, cols = {}, None
    with open(path, encoding="latin-1") as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            if p[0] == "columns":
                cols = p[1:]
            elif cols and p[0] == "comp" and len(p) == len(cols) + 1:
                row = dict(zip(cols, p[1:]))
                if row["file"] == file and row["season"] == str(year):
                    out[row["phase"]] = row
    return out


def places(row):
    return tuple(int(row[k]) for k in ("up", "playoff_up", "playoff_down", "down"))


def place_faults(report, year):
    """The places of both leagues in the start report of a season against PLACES."""
    fails = []
    want_first, want_second = PLACES[year]
    first, second = report_rows(report, year).get("start"), report_rows(report, year, "pol_second").get("start")
    if first and places(first) != want_first:
        fails.append(f"{year}: Ekstraklasa places {places(first)}, want {want_first}")
    if not second:
        fails.append(f"{year}: no I liga row in the start report")
    elif places(second) != want_second:
        fails.append(f"{year}: I liga places {places(second)}, want {want_second}")
    return fails


def one(program, game, year, keep, reuse, timeout):
    _, _, clubs, split = RUNS[year]
    work = os.path.join(keep, str(year)) if keep else tempfile.mkdtemp(prefix=f"cm0102-pol-first-{year}-")
    report = os.path.join(work, "report.tsv")
    if not reuse:
        db = database(year, keep or work)
        code, report = rrc.run(program, game, year, year, db, ["all"], [], work, timeout,
                               {"changing_rules": "on"}, {"RECOMP_POL_FIRST_LOG": "1"})
        if code != 0:
            return [f"{year}: the run ended with code {code} ({work})"]
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    fails = [f"{year}: {x.strip()}" for x in log.split("\n") if "recomp: message box from" in x]
    rows = report_rows(report, year)
    want = 2 * (clubs - 1)
    start = rows.get("start")
    if not start:
        fails.append(f"{year}: no Polish First Division row in the start report")
    else:
        got = (int(start["clubs"]), int(start["rounds"]))
        if got != (clubs, 2):
            fails.append(f"{year}: start report clubs and meetings {got}, want {(clubs, 2)}")

    fails += place_faults(report, year)
    end = rows.get("end")
    if not end:
        fails.append(f"{year}: no Polish First Division row in the end report")
    elif (int(end["clubs"]), int(end["matches"])) != (clubs, want):
        fails.append(f"{year}: end report clubs and matches {(int(end['clubs']), int(end['matches']))}, want "
                     f"{(clubs, want)}")
    tag = f"{year}/{(year + 1) % 100:02d}"
    if f"recomp: pol first: {tag}: one table of {clubs} clubs" not in log:
        fails.append(f"{year}: the log names no one table of {clubs} clubs")
    if split:
        top, bottom, dates, points = split
        line = f"recomp: pol first: {tag}: split after 30 rounds, {top} and {bottom} clubs, {dates} dates, points {points}"
        if line not in log:
            got = re.findall(r"recomp: pol first: .*split after.*", log)
            fails.append(f"{year}: no split line '{line}' ({got})")
    elif "split after 30 rounds," in log:
        fails.append(f"{year}: a split in a season of one table")
    hist = f"recomp: pol first: {tag}: club history of the final table, {clubs} clubs, {30 if split else 0} matches before it"
    if hist not in log:
        fails.append(f"{year}: no club history line '{hist}'")
    print(("FAIL  " + "; ".join(fails)) if fails else f"ok    {year}: the Ekstraklasa of {tag} ({work})")
    return fails


def long_run(program, game, keep, reuse, timeout, which="long"):
    """LONG: a 2002 game to 2005/06; the clubs and places of each season. LONG2: a 2025 game to 2026/27."""
    start, last, sizes = {"long": LONG, "long2": LONG2, "long3": LONG3}[which]
    work = os.path.join(keep, which) if keep else tempfile.mkdtemp(prefix="cm0102-pol-first-long-")
    report = os.path.join(work, "report.tsv")
    if not reuse:
        code, report = rrc.run(program, game, last, start, database(start, keep or work, LONG3_DB if which == "long3" else None), ["all"], [], work,
                               timeout * (last - start + 1), {"changing_rules": "on"}, {"RECOMP_POL_FIRST_LOG": "1", "RECOMP_PLAYOFF_LOG": "1"})
        if code != 0:
            return [f"long: the run ended with code {code} ({work})"]
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    fails = [f"long: {x.strip()}" for x in log.split("\n") if "recomp: message box from" in x]
    for year, clubs in sizes.items():
        row = report_rows(report, year).get("start")
        got = int(row["clubs"]) if row else None
        if got != clubs:
            fails.append(f"long: {year}/{(year + 1) % 100:02d} Ekstraklasa clubs {got}, want {clubs}")
        if year in PLACES:
            fails += [f"long: {x}" for x in place_faults(report, year)]
        hist = f"recomp: pol first: {year}/{(year + 1) % 100:02d}: club history of the final table, {clubs} clubs"
        if hist not in log:
            fails.append(f"long: no club history line '{hist}'")
        second = report_rows(report, year, "pol_second").get("start")
        if second and int(second["clubs"]) != 18 and which != "long3":
            fails.append(f"long: {year}/{(year + 1) % 100:02d} I liga clubs {second['clubs']}, want 18")
    if which in ("long2", "long3"):
        own = 2025 if which == "long2" else 2019
        if not re.search(rf"recomp: playoffs: pol_second {own}: play-off of places 3, 4, 5 and 6 drawn on day \d+", log):
            fails.append(f"long: no line for the I liga's own play-off of {own}/{(own + 1) % 100:02d} (drawn before the season change)")
        # 2 direct places and the play-off's winner go up, 3 clubs go down, all between the two leagues.
        def members(season, slot):
            out = set()
            with open(report, encoding="latin-1") as f:
                for line in f:
                    p = line.rstrip("\n").split("\t")
                    if len(p) > 5 and p[0] == "club" and p[1] == str(season) and p[2] == "start" and p[3] == slot:
                        out.add(p[5])
            return out
        e1, e2, i1, i2 = members(own, "133"), members(own + 1, "133"), members(own, "134"), members(own + 1, "134")
        up, down = e2 - e1, e1 - e2
        if not e1 or len(up) != 3 or len(down) != 3 or not up <= i1 or not down <= i2:
            fails.append(f"long: the exchange of {own}/{(own + 1) % 100:02d}: {len(up)} clubs up (want 3, from the I liga), {len(down)} down (want 3)")
        if re.search(r"recomp: season change net: .*pol", log):
            fails.append("long: the season change net acted on the Polish leagues")
    print(("FAIL  " + "; ".join(fails)) if fails else f"ok    {which}: a {start} game to {last}/{(last + 1) % 100:02d} ({work})")
    return fails


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    program, game = argv[0], argv[1]
    only = [y for y in argv[argv.index("--only") + 1].split(",")] if "--only" in argv else [str(y) for y in sorted(RUNS)] + ["long", "long2", "long3"]
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else None
    timeout = int(argv[argv.index("--timeout") + 1]) if "--timeout" in argv else 7200
    fails = []
    for y in only:
        fails += long_run(program, game, keep, "--reuse" in argv, timeout, y) if y in ("long", "long2", "long3") else \
            one(program, game, int(y), keep, "--reuse" in argv, timeout)
    print(("FAIL  " + f"{len(fails)} failures") if fails else f"ok    pol_first_run: {len(only)} seasons")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
