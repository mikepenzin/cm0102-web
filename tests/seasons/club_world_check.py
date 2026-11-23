#!/usr/bin/env python3
"""Seasons of the club competitions outside UEFA (0.6.0 slice 26, part a): each competition of the original plays an
edition only in the seasons that the rules table holds it (docs/research/rules-0.6.0/club-world.tsv,
src/recomp_rt/win/season_kind_club_world.c).

Usage:
    python3 tests/seasons/club_world_check.py <cm0102_recomp> <game folder> [--only <name>,...] [--jobs n]
        [--keep <folder>] [--reuse] [--own-dbs <folder>] [--timeout <seconds>]

Each run is a season jump of tests/seasons/rules_report_check.py with England, Argentina, Australia, Brazil and Japan
selected (a creator makes its objects only when a nation of its continent is selected, 0x6699d0). The runs:
    2001-on   the 3.9.68 data from 2001/02 with changing_rules on, to 2002/03: the Inter-American Cup and the
              Oceania Champions Cup wait from the new game, the Club World Championship waits for 2005, the Copa
              Mercosur, the Merconorte Cup and the Asian Cup Winners' Cup stop after 2001/02
    2004-on   Summer 2004 (our own database: --own-dbs names a folder that holds it unpacked, made from
              Summer 2004.zip of the release input) with changing_rules on, to 2005/06, with England only
              (entry 2): the Intercontinental Cup stops after 2004/05, and the Club World Championship comes
              back in 2005/06. (A 2003/04 start with October 2003 stops on 7 February 2004 at the box
              "comp_stats..CPP 1664", 0x4a25a8, with England alone too: Comp_Stats::set_club_final_position
              0x4a2540 gets no club. The build without this slice stops there too. Which competition calls it is
              unknown.)
    2004      Summer 2004 (--own-dbs) with changing_rules off, to 2005/06, with England only: the start season
              holds no Club World Championship, so it never comes (Q232), and the Intercontinental Cup plays on
    2007      November 2007 with changing_rules off, to 2008/09: the start season rules at a new game, and the
              Club World Championship in two seasons in a row. (A 2005/06 start stops at the World Cup draw of
              December 2005 with the box "world_cup..cpp 1438", 0x92f2b1, before this slice: fewer than 32
              qualified nations. That is not a rule of this slice.)
    2007-load November 2007 with changing_rules off: the game saves 150 days after the start (11 December 2007,
              after the quarter-finals of the 7-club knockout on 10 December), a new program loads that save,
              returns from holiday and plays 10 days (the semi-finals on 13 December, the final and the third
              place match on 16 December).
              The loaded game must end the knockout with places 1 to 3 (the test log of the history thunk) and
              save again. It needs no Select League(s) nations of other continents: entry 2 is made for every game
    2024-on   October 2024 with changing_rules on, to 2025/26: the Club World Championship of 2024/25 is the
              last in the table. (A 2023/24 start with October 2023 stops in February 2024 at the same
              comp_stats box as a 2003/04 start, also without this slice.)

The checks (each run):
    - the jump ends with code 0 and the report holds the start and end of each season (a stopped run fails)
    - for each object of the classes of the kind and each season S of the report: the object holds year S at the
      start or at the end of S exactly when the table holds the competition in S (an edition of year Y plays in
      game season Y, docs/research/2026-10-18-club-world-idle.md section 6)
    - changing_rules on: the news log of the season change (RECOMP_SEASON_CHANGE_LOG=1) has one sentence for
      each competition that the table starts or stops holding in a season of the run
    - part b: each season that the table holds the Club World Championship with 6 or 7 clubs plays it as a
      knockout of that many clubs (the log line of season_kind_club_world.c); with changing rules off, the club
      count of the start season
    - part 26c: a season with 32 clubs in the table plays the groups of Form G (log line "club world groups")
    - open item 125: the edition takes the host of its row ("; host Japan"): its host place comes from that nation,
      or the log says that the nation has fewer than 6 clubs. In 2007 and 2008-dec (December 2008, after Gamba
      Osaka's AFC final) the AFC winner is a club of the host, so the host place is the AFC runner-up (HOST_PLACE)
    - part 26c (Q401): the knockout's first match and final are on the days of the table's format text (December,
      or February for 2020/21 to 2022/23), and when the run reaches the final, places 1 to 3 (Form G: 1 and 2) are
      written on the final's day or at most 7 days later
    - each class whose continent has a selected nation is in the report (NEEDS)
The other runs: 2019-on (October 2019, Oceania into its gap of 2020/21 in the game), 2020-on (November 2020, Oceania
waits from the new game and comes back in 2021/22), 2022-on (October 2022, the Asian Club Championship waits from the
new game and comes back in 2023/24), 2001-all (Select All, so the Merconorte stops after 2001/02), 2002-on, 2008-on
and 2017-on (more start seasons). Runs end before a World Cup draw where they can: a start inside a qualifying
campaign stops at the draw with the box "world_cup..cpp 1438" (0x92f2b1) until slice 27's fix is on main, so 2008-on
and 2017-on end after their start season, and 2020-on crosses the draw of December 2021.
A run takes about 6 to 12 minutes (2001-all longer). --jobs (default 2) runs that many at the same time. --reuse checks the reports
already in the --keep folder again without a new run.
"""
import concurrent.futures
import datetime
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
import rules_report_check as rrc  # noqa: E402

DB = "/Users/max/Projects/cm0102/test-fixtures/databases"
NATIONS = ["England", "Argentina", "Australia", "Brazil", "Japan"]
# name: (start year, database folder or None, last season, changing rules, nations)
RUNS = {
    "2001-on": (2001, None, 2002, "on", NATIONS),
    "2004-on": (2004, "Summer 2004", 2005, "on", ["England"]),
    "2004": (2004, "Summer 2004", 2005, "off", ["England"]),
    "2007": (2007, "November 2007", 2008, "off", NATIONS),
    "2024-on": (2024, "October 2024", 2025, "on", NATIONS),
    "2007-load": (2007, "November 2007", 2007, "off", ["England"]),
    "2008-dec": (2008, "December 2008", 2008, "off", NATIONS),      # host place to the AFC runner-up (item 125)
    # Gaps and their returns, and more starts (the review of 2026-10-18).
    "2019-on": (2019, "October 2019", 2020, "on", NATIONS),         # Oceania into its gap of 2020/21 in the game
    "2020-on": (2020, "November 2020", 2021, "on", NATIONS),        # Oceania waits from the new game, back in 2021
    "2022-on": (2022, "October 2022", 2023, "on", NATIONS),         # Asia waits from the new game, back in 2023
    "2001-all": (2001, None, 2002, "on", ["all"]),                  # Select All: the Merconorte (needs the USA)
    "2002-on": (2002, None, 2003, "on", NATIONS),
    "2008-on": (2008, "September 2008", 2008, "on", NATIONS),
    "2017-on": (2017, "October 2017", 2017, "on", NATIONS),
}
# Known stops that come before this slice (seen in the build without slice 26 too): (run, file, season): reason.
# They print as "known" and do not fail the check.
KNOWN = {
    ("2022-on", "oceania_club_champ", 2023): "the 2022 Oceania edition draws (4 clubs a group) but never completes, so "
                                             "its season change of July 2023 does not run (the build without slice "
                                             "26 too, 2026-10-18)",
}
# The classes that a run must hold: entry 2 is made in every game; a continental entry when a nation of its
# continent is selected (0x6699d0). The Merconorte and the CONCACAF classes need the United States (Select All).
NEEDS = {"world_club_champ": None, "world_club_cup": None, "inter_amer_cup": None, "con_merc_cup": "all",
         "conmebol_merc": "Brazil", "asia_club_champ": "Japan", "asia_cup_winner": "Japan", "asia_super_cup": "Japan",
         "oceania_club_champ": "Australia"}
FILES = ["world_club_champ", "world_club_cup", "inter_amer_cup", "con_merc_cup", "conmebol_merc", "asia_club_champ",
         "asia_cup_winner", "asia_super_cup", "oceania_club_champ"]


def folder(name, own=None):
    """The database folder whose name starts with the neutral name (some folder names add more words), in the
    test databases or in the folder of our own databases."""
    if not name:
        return None
    for top in [DB] + ([own] if own else []):
        for d in sorted(os.listdir(top)):
            if d == name or d.startswith(name + " "):
                return os.path.join(top, d)
    raise SystemExit(f"no database folder for {name} in {DB} or {own}")


def held(rows, file, season):
    """The rule of the file in season (after the last row the rule stays, Q248), or None with no row."""
    found = None
    for r in rows:
        if file in r["comp_file"].split(";") and r["season"] <= season and (found is None or r["season"] > found["season"]):
            found = r
    if found is None:
        return None
    return "not_held" not in (found["clubs"], found["format"])


def check(name, report, log, rules_on):
    failed = []
    seasons, comps = rrc.read_report(report)
    rows = rrc.make_rules.read_rows()
    start, last = RUNS[name][0], RUNS[name][2]
    got = [(s["season"], s["phase"]) for s in seasons]
    want = [(y, p) for y in range(start, last + 1) for p in ("start", "end")]
    if got != want:
        failed.append(f"{name}: the report holds {got[:2]}...{got[-2:]}, not the start and end of {start} to {last}")
    years = {}
    names = {}
    for c in comps:
        if c["file"] in FILES:
            years.setdefault((c["file"], c["slot"]), {}).setdefault(c["season"], set()).add(c["year"])
            names[c["file"]] = c["name"]
    found = {f for f, _ in years}
    for (file, slot), by_season in sorted(years.items()):
        for s in range(start, last + 1):
            table = held(rows, file, s if rules_on else start)   # changing rules off: the start season (Q232)
            if table is None:
                continue
            game = s in by_season.get(s, set())
            line = f"{name}: {file} in {s}/{(s + 1) % 100:02d}: years {sorted(by_season.get(s, set()))}, table " \
                   f"{'holds it' if table else 'not held'}"
            known = KNOWN.get((name, file, s))
            print(("  ok    " if game == table else "  known " if known else "  FAIL  ") + line +
                  (f" ({known})" if known and game != table else ""))
            if game != table and not known:
                failed.append(line)
    missing = [f for f in FILES if f not in found]
    print(f"  {name}: classes not in the report: {', '.join(missing) or 'none'}")
    nations = RUNS[name][4]
    for f in missing:
        need = NEEDS[f]
        # Slice 27: Australia moves to the AFC from 2006 (season_intl.c), so from a 2006 start it no longer makes
        # the Oceania entry.
        if f == "oceania_club_champ" and start >= 2006:
            continue
        if need is None or need in nations:
            failed.append(f"{name}: {f} is not in the report, but a nation of its continent is selected ({need})")
    # Part b: an edition of 6 or 7 clubs in the table is a knockout (season_kind_club_world.c, Form K). The form
    # of the first edition stays with changing rules off (Q232).
    text = open(log, encoding="latin-1").read() if os.path.exists(log) else ""
    made = {int(y): int(n) for n, y in re.findall(r"club world (?:knockout|groups) of (\d+) clubs for (\d+)", text)}
    if "world_club_champ" in found:
        for s in range(start, last + 1):
            if not held(rows, "world_club_champ", s if rules_on else start):
                continue
            row = next((r for r in rows if r["season"] == (s if rules_on else start) and
                        "world_club_champ" in r["comp_file"].split(";")), None)
            want = row["clubs"] if row and row["clubs"] in (6, 7, 32) else None   # 32: Form G, part 26c
            ok = made.get(s) == want
            print(f"  {'ok  ' if ok else 'FAIL'}  {name}: Club World Championship {s}/{(s + 1) % 100:02d}: "
                  f"knockout of {made.get(s)} clubs, table {want}")
            if not ok:
                failed.append(f"{name}: Club World Championship {s}: knockout of {made.get(s)}, want {want}")
            # Part 26c (Q401): the knockout plays on the real dates of the table, the first match to the final.
            if want and row:
                failed += check_dates(name, s, row, text, last)
                failed += check_host(name, s, row, text)
    if rules_on:
        sentences = re.findall(r"The (.+?) (is played again from the \d{4}/\d\d season|is no longer played|is not "
                               r"played in the \d{4}/\d\d season)\.", text)
        for file in sorted(found):
            for s in range(start + 1, last + 1):
                a, b = held(rows, file, s - 1), held(rows, file, s)
                if a is None or b is None or a == b:
                    continue
                hits = [x for x in sentences if x[0] == names.get(file)]
                ok = len(hits) >= 1
                print(f"  {'ok  ' if ok else 'FAIL'}  {name}: news for {file} ({names.get(file)}) in {s}: {hits}")
                if not ok:
                    failed.append(f"{name}: no news sentence for {file} in {s}")
    return failed


MONTHS = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October",
          "November", "December"]


def table_dates(row, year):
    """The first and the last day of the edition of game year `year` from the format text of the row: (first, last)
    as (day, month, year), or None. "11–18 December 2005" plays in the row's season; "4–11 February 2021" one
    year later. With changing rules off the row is that of the start season, so the year moves with `year`."""
    months = "|".join(MONTHS)
    m = re.search(rf"\b(\d{{1,2}})(?: ({months}))?\s*[\u2013-]\s*(\d{{1,2}}) ({months}) (\d{{4}})", row["format_text"] or "")
    if not m:
        return None
    last = MONTHS.index(m.group(4)) + 1
    first = MONTHS.index(m.group(2)) + 1 if m.group(2) else last
    y = year + int(m.group(5)) - row["season"]
    return (int(m.group(1)), first, y), (int(m.group(3)), last, y)


def check_dates(name, s, row, text, last):
    """The log line of the knockout's rounds and the dates of its places (season_kind_club_world.c)."""
    want = table_dates(row, s)
    got = re.search(rf"club world knockout {s}: first match (\d+)\.(\d+)\.(\d+), final (\d+)\.(\d+)\.(\d+)", text)
    got = ((int(got[1]), int(got[2]), int(got[3])), (int(got[4]), int(got[5]), int(got[6]))) if got else None
    ok = want is not None and got == want
    print(f"  {'ok  ' if ok else 'FAIL'}  {name}: Club World Championship {s}: rounds {got}, table {want}")
    fails = [] if ok else [f"{name}: Club World Championship {s}: rounds {got}, table {want}"]
    # The places are written on the day of the final, when the run reaches it.
    places = re.findall(rf"club world knockout {s}: place (\d) .+ on (\d+)\.(\d+)\.(\d+)", text)
    final = want[1] if want else None
    if final and (final[2], final[1]) <= (last + 1, 6):
        days = sorted({(int(d), int(m), int(y)) for _, d, m, y in places})
        want_places = ["1", "2"] if row["clubs"] == 32 else ["1", "2", "3"]      # Form G: no third place match
        # The history is written when the parent completes: on the day of the final (Form K) or a few days later
        # (Form G, 15 July 2025 after the final of 13 July).
        end = datetime.date(final[2], final[1], final[0])
        ok = sorted({p for p, *_ in places}) == want_places and len(days) == 1 and \
            0 <= (datetime.date(days[0][2], days[0][1], days[0][0]) - end).days <= 7
        print(f"  {'ok  ' if ok else 'FAIL'}  {name}: Club World Championship {s}: places {sorted(places)}")
        if not ok:
            fails.append(f"{name}: Club World Championship {s}: places {sorted(places)}, want {want_places} on {final}")
    return fails


# Open item 125: the knockouts whose AFC winner was a club of the host, so the host place went to the AFC
# runner-up (club-world.tsv rows club-world-116 and 135). (run, edition): the runner-up. Not 2007-load: with
# England alone no Asian nation is selected, so the AFC pick is a random club (0x6699d0). Not 2022 (Wydad
# Casablanca, Al Ahly, row club-world-401): the game has no African club competition, so the CAF pick is always a
# random club of the top reputations (pick_entrant 0x92ae60 has no African record, 0x92aeb4..0x92aee5).
HOST_PLACE = {("2007", 2007): "Sepahan", ("2008-dec", 2008): "Adelaide United"}


def check_host(name, s, row, text):
    """Open item 125: the edition takes the host of its row's format text ("; host Saudi Arabia"): the log line of
    the host place, or the line that the host has fewer than 6 clubs (the drawn host stays)."""
    m = re.search(r"; host ([^;(]+)", row["format_text"] or "")
    if not m:
        return []
    host = m.group(1).strip()
    ok = re.search(rf"club world {s}: (host {re.escape(host)}, host place .+|the host {re.escape(host)} has \d+ "
                   rf"clubs, so the drawn host stays)", text)
    print(f"  {'ok  ' if ok else 'FAIL'}  {name}: Club World Championship {s}: host {host}: "
          f"{ok.group(1) if ok else 'no log line'}")
    if not ok:
        return [f"{name}: Club World Championship {s}: no host line for {host}"]
    want = HOST_PLACE.get((name, s))
    if want:
        got = re.search(rf"club world {s}: host {re.escape(host)}, host place {want}\b.* \(runner-up: the winner ",
                        text)
        print(f"  {'ok  ' if got else 'FAIL'}  {name}: Club World Championship {s}: host place {want} (runner-up)")
        if not got:
            return [f"{name}: Club World Championship {s}: the host place is not the runner-up {want}"]
    return []


def load_and_play(program, game, save, out, days, timeout=1200, table=None):
    """Loads save, lets the game go on with its holiday (a loaded game keeps it) and saves `days` game days after
    the load (RECOMP_SAVE_AFTER_DAYS ends the program). timeout: seconds (the quit step comes 300 s before it).
    table: the rules table of the load, else the build's (rules_report_check.rules_table).
    Returns the result and the path of the new save, or None."""
    sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
    import fixtures
    import smoke_run
    user = os.path.join(out, "user")
    os.makedirs(user, exist_ok=True)
    os.replace(save, os.path.join(user, os.path.basename(save)))
    fixtures.write_settings(user, "classic", [])
    steps = fixtures.LOAD_STEPS[:-1] + [f"{(timeout - 300) * 1000} quit"]
    table = table or rrc.rules_table(program, game)
    r, _ = fixtures.run(program, game, "classic", user, smoke_run.scaled_steps("classic", out, steps),
                        {"RECOMP_SAVE_AFTER_DAYS": str(days), "RECOMP_RULES_TABLE": table}, timeout)
    with open(os.path.join(out, "stderr.txt"), "w") as f:
        f.write(r.stderr)
    saved = os.path.join(user, smoke_run.SAVE_NAME)
    return r, saved if os.path.exists(saved) else None


def knockout_load(program, game, keep):
    """The 2007-load run (module docstring)."""
    import shutil
    work = os.path.join(keep, "2007-load") if keep else tempfile.mkdtemp(prefix="cm0102-club-world-load-")
    code, _ = rrc.run(program, game, 2007, 2007, folder("November 2007"), ["England"], [], work, 7200,
                      options_more={"changing_rules": "off"}, env_more={"RECOMP_SAVE_AFTER_DAYS": "150"})
    first = os.path.join(work, "user", "samesave.sav")
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    if code != 0 or not os.path.exists(first):
        return [f"2007-load: the first run made no save (code {code}, {work})"]
    if "club world knockout of 7 clubs for 2007" not in log or "club world knockout 2007: place" in log:
        return [f"2007-load: the save is not inside the knockout ({work}/stderr.txt)"]
    again = os.path.join(work, "again")
    os.makedirs(again, exist_ok=True)
    shutil.copyfile(first, os.path.join(again, "knockout.sav"))
    r, saved = load_and_play(program, game, os.path.join(again, "knockout.sav"), again, 10)
    places = sorted(set(re.findall(r"club world knockout 2007: place (\d) (.+) on ", r.stderr)))
    print(f"  2007-load: places after the load: {places}")
    fails = []
    if r.returncode != 0 or not saved:
        fails.append(f"2007-load: the save did not load and play on (code {r.returncode}, {again})")
    if [p for p, _ in places] != ["1", "2", "3"]:
        fails.append(f"2007-load: the knockout did not end with places 1 to 3 after the load ({again}/stderr.txt)")
    if "message box" in r.stderr:
        fails.append(f"2007-load: a message box after the load ({again}/stderr.txt)")
    return fails


TIMEOUT = 7200                          # seconds a run may take (--timeout: 2001-all with Select All under load)


def one(program, game, name, keep, reuse, own):
    if name == "2007-load":
        return knockout_load(program, game, keep)
    year, db, last, rules, nations = RUNS[name]
    work = os.path.join(keep, name) if keep else tempfile.mkdtemp(prefix=f"cm0102-club-world-{name}-")
    report = os.path.join(work, "report.tsv")
    log = os.path.join(work, "stderr.txt")
    if not reuse:
        code, report = rrc.run(program, game, last, year, folder(db, own), nations, [], work, TIMEOUT,
                               options_more={"changing_rules": rules}, env_more={"RECOMP_SEASON_CHANGE_LOG": "1"})
        if code != 0:
            return [f"{name}: the run ended with code {code} ({log})"]
    if not os.path.exists(report):
        return [f"{name}: no report ({log})"]
    return check(name, report, log, rules == "on")


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    only = rrc.arg(argv, "--only")
    names = only.split(",") if only else list(RUNS)
    jobs = int(rrc.arg(argv, "--jobs", 2))
    keep = os.path.abspath(rrc.arg(argv, "--keep")) if "--keep" in argv else None
    reuse = "--reuse" in argv
    own = os.path.abspath(rrc.arg(argv, "--own-dbs")) if "--own-dbs" in argv else None
    global TIMEOUT
    TIMEOUT = int(rrc.arg(argv, "--timeout", TIMEOUT))
    failed = []
    with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
        for f in [pool.submit(one, program, game, n, keep, reuse, own) for n in names]:
            failed += f.result()
    for text in failed:
        print("FAIL  " + text)
    print("PASS" if not failed else f"FAILED: {len(failed)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
