#!/usr/bin/env python3
"""Rules report check (0.6.0 slice 12, Q265): the competitions that the game holds in each season, compared
with the rules table.

Usage:
    python3 tests/seasons/rules_report_check.py <report> [--expected <name>] [--update-expected]
        [--data <data folder>] [--start-only]
        compares a report with the rules table and lists each difference (--data: the club checks against the
        database of the run)
    python3 tests/seasons/rules_report_check.py --run <cm0102_recomp> <game folder> [--year y] [--last y]
        [--database <folder>] [--nations england|all|all,-<name>,...|<name,name,...>]
        [--extra conference,regional,c2,spain,portugal]
        [--keep <folder>] [--timeout s] [--start-only] [--update-expected] [--changing-rules on|off]
        plays the season jump, then compares its report

    python3 tests/seasons/rules_report_check.py --league-advance <report> [<report> ...]
        the gate check of the divisions (league_stalls): at the end of each season every division's edition must
        have moved on since the end of the season before; the reports of the legs of one soak read in leg order

Runs:
    per slice   --year X --last X+1: two seasons from the start in X (about 6 minutes with England alone)
    release     the base game from 2001 with --last 2027: 27 seasons (about 65 minutes with England alone)
    start only  --year X --start-only: the report of the first day of a season game, then the program ends
                (RECOMP_RULES_REPORT_STOP=start): the club moves and names of that start (slice 15). Its
                expected file has the name <year>-<nations>-start

The run: a new game (Arsenal, as in tests/smoke/smoke_run.py) with changing_rules = off, then Go on Holiday.
RECOMP_SEASON_JUMP=<last> turns the match engine off, so the original's quick result engine plays every match
in the real day loop. RECOMP_RULES_REPORT writes the competitions at the start of each season (the first day,
then 1 August) and at its end (1 June of the next year) (src/recomp_rt/win/season_report.c). The run ends after
the end report of the last season. --year starts the game in that season (RECOMP_START_SEASON) with the
database folder. --nations picks the leagues on the Select League(s) screen: england (the default
selection), all (Select All), all,-france (Select All, then France out), or names of the first 16 rows
(Argentina to Norway). --extra adds the lower
divisions of the screen: the English Conference, the German regional divisions and Serie C2. --changing-rules on
plays the jump with changing rules on (slice 22; the default is off, the start season's rules). --keep keeps the
report and the log there.

The checks that fail the test (exit code 1):
    - the run ends with code 0 and its report holds the start and end of every season from the start year to
      the last year
    - every competition of the report has a known class (its vtable is in the class table of the build)
    - base game (start 2001): the name of each competition record is the name of that record in
      club_comp.dat or nation_comp.dat of the game folder, and each English division holds as many clubs as
      club.dat puts in it
    - a difference with the rules table that the expected file does not list. The expected files are
      tests/seasons/rules_report_expected/<start year>-<nations>.tsv: one line per difference (season, kind,
      text). A slice that brings a rule removes its lines with --update-expected; a difference that has gone
      is listed, and does not fail the check.

The differences, by kind:
    clubs, subs_used, bench     the game's value at the start and at the end differ from the count of the row
    up, down                    a division's promotion places (+0xbe) or relegation places (+0xc1) differ from
                                the row (a row with a numbered tier); the row's count can also hold the
                                play-off winner (+0xbf) or the play-out losers (+0xc0)
    playoffs                    a division's promotion play-off places (+0xbf) or relegation play-off places
                                (+0xc0) differ from the places of the row's play-off text, as
                                tools/seasons/encode_leagues.py reads it (slice 19). A play-off that moves no
                                club between tiers (a finals series, a European play-off, a tie-break) has no
                                places. For a text that the tool reads only in part, only whether the division
                                has play-off places at all is compared
    name                        the game's name differs from the table's name of that season and from its shown
                                form (tools/seasons/make_names.py shown_name)
    no_row                      the game holds a competition whose file has no row in that season
    not_held                    the game holds a competition (its year is the season, one year before or one
                                after) that the table marks not held in that season
    idle                        the game's object waits for a later edition (its year is later), but the
                                table holds the competition in that season (a row that is not_played counts as not
                                held here and in not_held)
    club_list                   (a season game, its start season) a division with a club list of the club moves
                                table (tools/seasons/make_clubs.py) whose listed clubs that the database holds
                                are not all in it, or that holds other clubs too: the division keeps the count of
                                its class (slice 18 brings the real count), or the list lacks clubs of the
                                database. The line names each club, so another fill is a new difference
    division_clubs              (slice 18) a league of the size table whose division holds another number of clubs
                                at a season start than its table has rows
    format                      the stages of the Champions League at the end of a season (the uefa line of the
                                report, slice 22): the second group stage and the first knockout round differ
                                from the "Rounds (clubs):" list of the row ("Groups 2 16, KO 8" or "KO 16")
    not_in_game                 a row with no competition in the game, of a nation (or body, such as UEFA)
                                that has a numbered tier in the report, or of a file of the report; not for a row whose
                                notes say "not played in the game" (not_played, Q336)

The row of a competition: the rows of its file in that season; of those, a row with this file alone before a
row of two files; a row that is not a qualifying path ("qualifying" in comp); then the k-th row for the k-th
object of the same file (the Regionalliga groups).
"""
import collections
import os
import struct
import subprocess
import sys
import tempfile
import time


# Friendly competitions: made by chance, not in the rules table.
FRIENDLY_FILES = {"mini_cup", "mini_league"}

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
EXPECTED_DIR = os.path.join(HERE, "rules_report_expected")
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
sys.path.insert(0, os.path.join(ROOT, "tools", "seasons"))
import make_rules  # noqa: E402
import make_names  # noqa: E402
import make_clubs  # noqa: E402
import encode_leagues  # noqa: E402   slice 19: the play-off places of each league row

COLS = ["season", "phase", "slot", "record", "file", "class", "kind", "vtable", "year", "clubs", "rounds",
        "subs_used", "bench", "matches", "up", "playoff_up", "playoff_down", "down", "flags", "above", "below",
        "name"]
INTS = ["season", "record", "year", "clubs", "rounds", "subs_used", "bench"]
LEAGUE_INTS = ["matches", "up", "playoff_up", "playoff_down", "down", "above", "below"]
RECORD = 0x6b
NATION_ROWS = ["Argentina", "Australia", "Belgium", "Brazil", "Croatia", "Denmark", "England", "Finland", "France",
               "Germany", "Greece", "Holland", "Italy", "Japan", "Northern Ireland", "Norway"]
EXTRA = {"conference": 320, "regional": 385, "c2": 451}     # y of the row; the extra division is at x 675
ENV_EXTRA = {"spain", "portugal"}                           # slice 20: set by RECOMP_TEST_EXTRA, not by a click


UEFA_LINES = []          # the uefa lines of the last report that read_report read (slice 22)


def read_report(path, clubs=None):
    """(season lines, comp lines); club lines go into clubs when a list is given (slice 15)."""
    seasons, comps = [], []
    UEFA_LINES.clear()
    with open(path, encoding="latin-1") as f:
        lines = f.read().split("\n")
    if not lines or not lines[0].startswith("#bosman-rules-report\t2"):
        raise SystemExit(f"{path}: not a rules report of format 2")
    for line in lines[1:]:
        if not line:
            continue
        cells = line.split("\t")
        if cells[0] == "season":
            seasons.append({"season": int(cells[1]), "phase": cells[2], "date": cells[3], "start": int(cells[4]),
                            "rules": cells[5]})
        elif cells[0] == "club":
            if clubs is not None and len(cells) == 6:
                clubs.append({"season": int(cells[1]), "phase": cells[2], "division": int(cells[3]),
                              "club": int(cells[4]), "name": cells[5]})
        elif cells[0] == "uefa":                         # slice 22: the stages of the Champions League
            UEFA_LINES.append({"season": int(cells[1]), "phase": cells[2], "file": cells[3], "stages": int(cells[4]),
                               "current": int(cells[5]), "groups": int(cells[6]), "groups2": int(cells[7]),
                               "ko_clubs": int(cells[8]), "ko_rounds": int(cells[9]),
                               "winner": cells[10] if len(cells) > 10 else "-"})
        elif cells[0] == "comp":
            if len(cells) != len(COLS) + 1:
                raise SystemExit(f"{path}: a comp line with {len(cells)} cells")
            c = dict(zip(COLS, cells[1:]))
            for k in INTS:
                c[k] = int(c[k])
            for k in LEAGUE_INTS:
                c[k] = int(c[k]) if c[k] != "-" else None
            comps.append(c)
    return seasons, comps


def record_names(game):
    """{(file, id): name} of club_comp.dat and nation_comp.dat (0x6b bytes a record: id, long name at +4)."""
    out = {}
    for name in ("club_comp.dat", "nation_comp.dat"):
        path = os.path.join(game, "Data", name)
        if not os.path.exists(path):
            continue
        data = open(path, "rb").read()
        for i in range(len(data) // RECORD):
            r = data[i * RECORD:(i + 1) * RECORD]
            out[(name, struct.unpack_from("<i", r, 0)[0])] = r[4:56].split(b"\0")[0].decode("latin-1")[:51]
    return out


def division_counts(game):
    """{division id: club count} of club.dat (581 bytes a record). Inference from the data: the int at +0x57
    is the club's division (the club_comp id): 20 clubs hold 7 (English Premier Division) and 24 each hold
    8, 9 and 10 in the 3.9.68 data, as the English divisions of 2001/02."""
    path = os.path.join(game, "Data", "club.dat")
    out = collections.Counter()
    if os.path.exists(path):
        data = open(path, "rb").read()
        for i in range(len(data) // 581):
            out[struct.unpack_from("<i", data, i * 581 + 0x57)[0]] += 1
    return out


def pick_row(found, file, k):
    """The row of the k-th object of a file in a season (module docstring)."""
    alone = [r for r in found if file in row_files(r)] or found
    main = [r for r in alone if "qualifying" not in r["comp"].lower()] or alone
    return main[min(k, len(main) - 1)]


# Slice 21c: the new cups of season_new_cups.c report the file of their def; their rows have comp_file "new" and are
# found by nation and comp (the same pairs as new_keys in src/recomp_rt/win/season_cups_more.c).
NEW_FILES = {("Turkey", "super cup"): "new_tur_super", ("Russia", "super cup"): "new_rus_super",
             ("Norway", "Norwegian Super Cup"): "new_nor_super", ("Russia", "league cup"): "new_rus_lge_cup",
             ("England", "Conference League Cup"): "new_conf_cup",
             ("Northern Ireland and Republic of Ireland", "All-Ireland cup"): "new_all_ireland"}


def new_matches(name, row):
    """A new competition of the shared module (season_comps.h, slice 28) and a row of comp_file "new": the module
    gives the object the name of each season's row, and keeps the name of the season before until its next
    change ("2018-19 UEFA Nations League" in 2019/20 holds the comp "UEFA Nations League")."""
    return name in (row["name"], row["comp"]) or (row["comp"] and row["comp"] in name)


def row_files(r):
    """The files of a row. A row of a new competition (comp_file "new") has the file of its def in
    src/recomp_rt/win/season_comps.c: "new_" and its comp in lower case with "_" for spaces ("Conference League"
    gives new_conference_league). The new cups of slice 21c have files of their own (NEW_FILES)."""
    if r["comp_file"] == "new" and (r["nation"], r["comp"]) in NEW_FILES:
        return [NEW_FILES[(r["nation"], r["comp"])]]
    return ["new_" + r["comp"].lower().replace(" ", "_") if f == "new" else f for f in r["comp_file"].split(";")]


_LEAGUE_PLAYOFFS = {}


def league_playoffs():
    """{row id: encoded league row} of tools/seasons/encode_leagues.py (slice 19)."""
    if not _LEAGUE_PLAYOFFS:
        rows, _, problems = encode_leagues.encode()
        if problems:
            raise SystemExit("encode_leagues.py: " + problems[0])
        _LEAGUE_PLAYOFFS.update((e["id"], e) for e in rows)
    return _LEAGUE_PLAYOFFS



NOT_PLAYED = "not played in the game"


def not_played(r):
    """A row of a competition that the game does not play by a decision, marked in its notes column (the National
    League Cup from 2024/25, Q336: the game holds no under-21 sides). The one source for the not_in_game check and
    league_stalls: such a row counts as not held."""
    return NOT_PLAYED in (r.get("notes") or "")


# The calendar-year leagues whose start report (1 August) is not read by league_stalls: they hold the year before at
# 1 August and move on by the end of the season (run 8b, 2019 start: arg_prm 2020 at the 2021/22 start, 2021 at its end).
CALENDAR_START_FILES = {"arg_prm", "arg_second"}


def league_stalls(comps, rows=None):
    """Gate check (O278, O282): the divisions whose edition does not move on from one season to the next. comps: the
    comp lines of one or more reports, the legs of a soak in leg order. A division is a file of a table row with a
    numbered tier. Each object (its slot) holds a year at the end of each season (1 June). It must be later than at
    the end of the season before, unless the object waits for a later edition (its year is after the season). The
    first value of a season is kept, because a loaded leg writes the state at the load under the season of the save.
    A season with a start report (1 August) and no end report (the last season of a run that ends before June) is
    checked at its start, but only for an object that holds the season's own year at two or more other season starts
    of the run, and at all of them. Argentina's leagues move on around 1 August (some starts hold the year before),
    so their start report is not read. A league
    that stops (Serie A at its 2015 edition from 2016/17 on) keeps its year. Objects with no year (1900 to 2200)
    give no line, as in differences().
    O324: the domestic cups too, the files of the rows with tier "cup" (each object of the file, league or cup kind).
    A cup is checked in a season only when the table holds it in that season and the season before: a season where
    its row says not held or not played in the game (not_played), or where it has no row (before its first or after
    its last edition), gives no line.
    Returns lines "<season>: <file> (<name>) ..."."""
    if rows is None:
        rows = make_rules.read_rows()
    divisions = {f for r in rows if isinstance(r["tier"], int) for f in row_files(r)}
    cup_held = collections.defaultdict(set)         # cup file: the seasons the table holds it
    for r in rows:
        if r["tier"] == "cup" and "not_held" not in (r["clubs"], r["format"]) and not not_played(r):
            for f in row_files(r):
                cup_held[f].add(r["season"])
    years = {"start": collections.defaultdict(dict), "end": collections.defaultdict(dict)}
    names = {}
    for c in comps:
        file = c["file"]
        if c["phase"] not in years or not 1900 <= c["year"] <= 2200:
            continue
        if (c["kind"] == "league" and file in divisions) or (file in cup_held and file not in divisions):
            years[c["phase"]][(c["slot"], file)].setdefault(c["season"], c["year"])
            names[(c["slot"], file)] = c["name"]

    def held(file, season):
        return file not in cup_held or (season in cup_held[file] and season - 1 in cup_held[file])

    out = []
    for key in sorted(names):
        ends, starts = years["end"].get(key, {}), years["start"].get(key, {})
        for season in sorted(ends):
            before, now = ends.get(season - 1), ends[season]
            if before is not None and before <= season - 1 and now <= before and held(key[1], season):
                out.append(f"{season}: {key[1]} ({names[key]}) holds edition {now} at the end of "
                           f"{season}/{(season + 1) % 100:02d}, as at the end of the season before: it did not move on")
        for season in sorted(starts):
            before = ends.get(season - 1)
            if season in ends or before is None or before > season - 1 or not held(key[1], season):
                continue
            if key[1] in CALENDAR_START_FILES:
                continue
            # The object moved on by 1 August in this run: at two or more other season starts it holds the
            # season's own year, and at no other start an earlier one.
            others = [starts[x] - x for x in starts if x != season]
            if len(others) < 2 or any(d < 0 for d in others) or not all(d == 0 for d in others):
                continue
            if starts[season] <= before:
                out.append(f"{season}: {key[1]} ({names[key]}) holds edition {starts[season]} at the start of "
                           f"{season}/{(season + 1) % 100:02d}, as at the end of the season before, though it moved "
                           f"on by 1 August in the other seasons of the run: it did not move on")
    return out


def differences(seasons, comps, rows):
    """A sorted list of (season, kind, text)."""
    by_season_file = collections.defaultdict(list)
    for r in rows:
        for f in row_files(r):
            by_season_file[(r["season"], f)].append(r)
    # One entry per object and season: the start and end lines together.
    objects = collections.OrderedDict()
    for c in comps:
        objects.setdefault((c["season"], c["slot"]), {})[c["phase"]] = c
    out = set()
    files_in_report = {c["file"] for c in comps}
    nations = set()
    held = collections.defaultdict(set)
    held_new = collections.defaultdict(set)         # the names of the new competitions of each season (slice 28)
    nth = collections.Counter()
    for (season, _), phases in objects.items():
        c = phases.get("start") or phases["end"]
        both = [x for x in (phases.get("start"), phases.get("end")) if x]
        held[season].add(c["file"])
        if c["file"].startswith("new_"):
            held_new[season].add(c["name"])
        found = by_season_file.get((season, c["file"]), [])
        if not found and c["file"].startswith("new_"):
            # A new competition of the shared module (season_comps.h, slice 28): the rows of comp_file "new"
            # with the object's name (the module gives it the name of each season's row).
            found = [r for r in by_season_file.get((season, "new"), []) if new_matches(c["name"], r)]
        k = nth[(season, c["file"])]
        nth[(season, c["file"])] += 1
        if not found:
            # The game makes the friendly competitions by chance at each start,
            # and the rules table has no rows for them. So they give no line.
            if c["file"] not in FRIENDLY_FILES:
                out.add((season, "no_row", f"{c['file']} ({c['name']}): no table row"))
            continue
        # Slice 20: a second object of a file that waits for a later season (the second Greek group between its
        # editions) and has no row of its own this season gives no line.
        own = [x for x in found if c["file"] in row_files(x)]
        if k >= len(own) and own and not any(season - 1 <= x["year"] <= season + 1 for x in both):
            continue
        r = pick_row(found, c["file"], k)
        nations |= {x["nation"] for x in found if isinstance(x["tier"], int)}
        label = f"{c['file']} ({c['name']}), row {r['id']}"
        table_held = "not_held" not in (r["clubs"], r["format"]) and not not_played(r)
        # Some objects of a record hold no year at +0x40 (French CFA and French Lower Division of fra_first): the
        # value is whatever the memory holds and changes from build to build, so such an object gives no line.
        if not any(1900 <= x["year"] <= 2200 for x in both):
            continue
        # The year of an object is the season (club competitions), the year of the final (the World Cup holds
        # 2002 in 2001/02) or the first year of a qualifying campaign (2000 for the 2002 World Cup). An object
        # with a later year waits for a later edition (the World Cup holds 2006 in 2002/03).
        if not any(season - 1 <= x["year"] <= season + 1 for x in both):
            if table_held:
                out.add((season, "idle", f"{label}: the game waits for {c['year']}, the table holds it"))
            continue
        if not table_held:
            out.add((season, "not_held", f"{label}: the game holds {c['year']}, the table says not held"))
        for key in ("clubs", "subs_used", "bench"):
            values = sorted({x[key] for x in both})
            if isinstance(r[key], int) and r[key] not in values:
                game = "/".join(str(v) for v in values)
                out.add((season, key, f"{label}: game {game}, table {r[key]} ({r[key + '_text'][:80]})"))
        if c["kind"] == "league" and isinstance(r["tier"], int):     # a division, not a group tournament
            for key, extra in (("up", "playoff_up"), ("down", "playoff_down")):
                want = r[key]
                if want == "na" and c[key] == 0:
                    continue
                accept = {c[key], c[key] + (1 if c[extra] else 0), c[key] + c[extra]}
                if not isinstance(want, int) or want not in accept:
                    out.add((season, key, f"{label}: game {c[key]} (+{c[extra]} play-off places), "
                                          f"table {want} ({r[key + '_text'][:80]})"))
            # Slice 19: the places of the encoded play-off text.
            e = league_playoffs().get(r["id"])
            if e is not None and r["playoffs"] != "unknown":
                want = (encode_leagues.count(e["po_up"]), encode_leagues.count(e["po_down"]))
                game = (c["playoff_up"], c["playoff_down"])
                same = game == want if e["po_status"] == "ok" else bool(sum(game)) == bool(sum(want))
                if not same:
                    out.add((season, "playoffs", f"{label}: game {game[0]} up and {game[1]} down play-off places, "
                                                 f"table {want[0]} and {want[1]} ({r['playoffs_text'][:80]!r})"))
        shown = make_names.shown_name(r["name"])
        if shown and c["name"] not in (r["name"], shown):
            out.add((season, "name", f"{label}: game {c['name']!r}, table {r['name']!r}"))
    out |= uefa_format_differences(rows)
    for s in sorted({x["season"] for x in seasons}):
        for r in rows:
            if r["season"] != s or r["comp_file"] == "na" or "not_held" in (r["clubs"], r["format"]) or not_played(r):
                continue
            files = row_files(r)
            if any(f in held[s] for f in files):
                continue
            if r["comp_file"] == "new" and any(new_matches(n, r) for n in held_new[s]):
                continue
            if r["nation"] in nations or any(f in files_in_report for f in files):
                out.add((s, "not_in_game", f"{r['id']} {r['comp_file']} {r['name']!r} ({r['nation']})"))
    return sorted(out)


def club_records(folder):
    """{id: long name} of club.dat in a data folder (581 bytes a record: id +0, long name +4)."""
    path = os.path.join(folder, "club.dat")
    out = {}
    if os.path.exists(path):
        data = open(path, "rb").read()
        for i in range(len(data) // 581):
            r = data[i * 581:(i + 1) * 581]
            out[struct.unpack_from("<i", r, 0)[0]] = r[4:56].split(b"\0")[0].decode("latin-1")
    return out


def division_records(folder):
    """{long name: id} of club_comp.dat in a data folder."""
    path = os.path.join(folder, "club_comp.dat")
    out = {}
    if os.path.exists(path):
        data = open(path, "rb").read()
        for i in range(len(data) // RECORD):
            r = data[i * RECORD:(i + 1) * RECORD]
            out.setdefault(r[4:56].split(b"\0")[0].decode("latin-1"), struct.unpack_from("<i", r, 0)[0])
    return out


DATA_COUNTING = {"IrePrm", "IreFirst", "PolFirst", "NorFirst"}     # the classes that count the clubs of the data


def new_division_alias(division, season, rows):
    """Slice 20: the record whose clubs a list names. A new division that lives in the object of a group division
    holds its clubs in the object's record: the club moves put them in a group, the first-season merge of the class
    makes the parent record their division and the group their tag (GerFirst::create_table 0x5db23e,
    SpaSecondB::load_group_clubs 0x856cdc, PorSecondB::create_table 0x7d240c). So the lists of the groups of one
    season are one list of the parent: the 3. Liga (one ger_regional row), Primera Federación (spa_second_b of 40)
    and Liga 3 (por_second_b of 24 or fewer)."""
    def row(file):
        found = [r for r in rows if r["season"] == season and r["comp_file"] == file]
        return found
    if division == "German Regional Division North" and len(row("ger_regional")) == 1:
        return "German Regional"
    if division in ("Spanish Second Division B1", "Spanish Second Division B2"):
        r = row("spa_second_b")
        if r and isinstance(r[0]["clubs"], int) and r[0]["clubs"] <= 48:
            return "Spanish Second Division B"
    if division in ("Portuguese Second Division B North", "Portuguese Second Division B Central"):
        r = row("por_second_b")
        if r and isinstance(r[0]["clubs"], int) and r[0]["clubs"] <= 30:
            return "Portuguese Second Division B"
    return division


def club_record_names(season):
    """Slice 15b: {(division, real name of the club moves table): the name of the new club record} of a season
    (tools/seasons/make_club_records.py). RECOMP_CLUB_RECORDS=off: none."""
    if os.environ.get("RECOMP_CLUB_RECORDS") == "off":
        return {}
    import make_club_records
    out, _errors = make_club_records.build()
    return {(r[1], r[2]): r[3] for r in out if r[0] == season}


def club_differences(seasons, comps, clubs, data_dir, rows=()):
    """The club moves of the start season (slice 15): for each division with a club list of the table
    (tools/seasons/make_clubs.py) whose league is in the report, the listed clubs that the database holds
    must be the clubs of that division at the start. One difference line per division that differs."""
    start = seasons[0]
    if start["start"] == 2001 and start["rules"] == "off":
        return set()                                   # the base game moves no club
    season = start["season"]
    records = division_records(data_dir)
    names = club_records(data_dir)
    # A league with no club count (+0x3e is 0) is a table entry that the game does not play: the French Lower
    # Division (FraFirst class, record 15) in every start, the base game too. The game keeps no club in such a
    # division: the clubs of the divisions it does not play go to "A Lower Division" before the start (a base
    # game run of 2001/02 with France shows French CFA and Lower Division with 0 clubs and 558 in A Lower
    # Division), so its list is not compared.
    in_report = {c["record"] for c in comps if c["season"] == season and c["kind"] == "league" and c["clubs"] > 0}
    # A class that counts the clubs of the data (season_clubs.c) has no club count when the moves leave its
    # division empty. That must fail, so such a league with no clubs is a difference.
    emptied = {c["record"]: c["class"] for c in comps if c["season"] == season and c["kind"] == "league"
               and c["clubs"] == 0 and c["class"] in DATA_COUNTING}
    # By name: the trim of the club table (database_trim_club_table 0x5267c0) gives the clubs new ids, so the
    # ids of the report are not those of club.dat. The club table holds each name once per nation; a name in
    # two nations counts once here, which the check accepts.
    game = collections.defaultdict(set)
    for c in clubs:
        if c["season"] == season and c["phase"] == "start":
            game[c["division"]].add(c["name"])
    held = set(names.values())
    plain_to = collections.defaultdict(set)
    for n in held:
        plain_to[make_clubs.plain_name(n)].add(n)
    lists = collections.OrderedDict()
    records = club_record_names(season)             # slice 15b: the new club records of the start season
    for s, division, club_id, club, _ in make_clubs.build()[0]:
        if s != season:
            continue
        if names.get(club_id) == club or club in held:
            name = club
        elif club_id == -1 and (division, club) in records:
            name = records[(division, club)]
        else:                               # the plain form, when one club of the database has it (season_clubs.c)
            name = make_clubs.plain_match(club, plain_to.get(make_clubs.plain_name(club), set()))
        lists.setdefault(new_division_alias(division, season, rows), []).append(name)
    out = set()
    for division, listed in lists.items():
        rec = records.get(division)
        if rec in emptied:
            out.add((season, "club_list", f"{division}: no clubs at the start ({emptied[rec]} counts the clubs of "
                                          f"the data)"))
        if rec is None or rec not in in_report:
            continue
        found = [n for n in listed if n]
        have = game[rec]
        absent = sorted(n for n in found if n not in have)
        others = sorted(have - set(found))
        if absent or others:
            out.add((season, "club_list", f"{division}: {len(found) - len(absent)} of {len(found)} listed clubs that "
                                          f"the database holds ({len(listed)} listed), {len(others)} other clubs, "
                                          f"{len(have)} in all; not in it: {', '.join(absent) or '-'}; others: "
                                          f"{', '.join(others) or '-'}"))
    return out


def division_differences(comps, clubs):
    """Slice 18: at each season start, a league of the size table (tools/recomp/league_sizes.py) holds as many clubs
    in its division (the club lines, club +0x57) as its table has rows (+0x3e). More clubs than rows play no match;
    fewer leave the table short."""
    sys.path.insert(0, os.path.join(ROOT, "tools", "recomp"))
    import league_sizes
    files = {f for _c, f, _r, stores in league_sizes.CLASSES if stores}
    held = collections.Counter((c["season"], c["division"]) for c in clubs if c["phase"] == "start")
    out = set()
    for c in comps:
        if c["phase"] != "start" or c["kind"] != "league" or c["file"] not in files or c["clubs"] <= 0:
            continue
        if c["year"] > c["season"] + 1:                 # slice 20: a league that waits for a later season
            continue
        n = held.get((c["season"], c["record"]), 0)
        if n != c["clubs"]:
            out.add((c["season"], "division_clubs", f"{c['file']} ({c['name']}): {n} clubs in the division, "
                                                    f"{c['clubs']} in its table"))
    return out


def uefa_format_differences(rows):
    """Slice 22: the stages that the Champions League and the UEFA Cup played in a season (their end lines)
    against the "Rounds (clubs):" list of the row. Champions League: the clubs of a second group stage and of the
    first knockout round. UEFA Cup: the clubs of the group stage and of the knockout round after it (0 and 0 for
    a row with no groups)."""
    import re
    out = set()
    for u in UEFA_LINES:
        if u["phase"] != "end":
            continue
        found = [r for r in rows if r["season"] == u["season"] and r["comp_file"] == u["file"]
                 and "qualifying" not in r["comp"].lower()]
        if not found:
            continue
        r = found[0]
        text = r["format_text"] or ""
        groups = re.search(r"Groups (\d+)", text)
        groups2 = re.search(r"Groups 2 (\d+)", text)
        ko = re.search(r"\b(?:KO|R3) (\d+)\.", text) or re.search(r"\bKO (\d+)", text)
        # Slice 25 (2024/25 on): the league phase of 36 is the group stage, and its knockout stage is the
        # play-offs and the round of 16 ("League phase 36, KO play-offs 16, R16 16"): 16 + 16 - 8 = 24 clubs.
        league = re.search(r"League phase (\d+)", text)
        r16 = re.search(r"\bR16 (\d+)", text)
        league_po = re.search(r"\bKO play-offs (\d+)", text)
        if league and r16 and league_po:
            want = (0 if u["file"] == "european_cup" else int(league.group(1)),
                    int(league_po.group(1)) + int(r16.group(1)) - int(league_po.group(1)) // 2)
            game = (u["groups2"] if u["file"] == "european_cup" else u["groups"], u["ko_clubs"])
            if game != want:
                out.add((u["season"], "format", f"{u['file']}, row {r['id']}: game {game[0]} and {game[1]} clubs "
                                                f"in the league phase and its knockout stage, table {want[0]} and "
                                                f"{want[1]}"))
            continue
        if u["file"] == "european_cup":
            if not ko:
                continue
            want = (int(groups2.group(1)) if groups2 else 0, int(ko.group(1)))
            game = (u["groups2"], u["ko_clubs"])
            words = ("in a second group stage", "in the first knockout round")
        else:
            want = (int(groups.group(1)), int(ko.group(1)) if ko else 0) if groups else (0, 0)
            # Slice 24: knockout round play-offs (2021/22 on) are one knockout stage with the round of 16: its
            # 16 clubs and the 8 group winners who enter the round of 16.
            ko_po = re.search(r"\bKO play-offs (\d+)", text)
            if groups and ko and ko_po:
                want = (want[0], int(ko_po.group(1)) + int(ko.group(1)) - int(ko_po.group(1)) // 2)
            game = (u["groups"], u["ko_clubs"])
            words = ("in the group stage", "in the knockout round after it")
        if game != want:
            out.add((u["season"], "format", f"{u['file']}, row {r['id']}: game {game[0]} clubs {words[0]} and "
                                            f"{game[1]} {words[1]}, table {want[0]} and {want[1]}"))
    return out


def expected_path(name):
    return os.path.join(EXPECTED_DIR, name + ".tsv")


def read_expected(name):
    path = expected_path(name)
    if not os.path.exists(path):
        return None
    out = set()
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            s, kind, text = line.rstrip("\n").split("\t", 2)
            out.add((int(s), kind, text))
    return out


def write_expected(name, diffs, seasons):
    os.makedirs(EXPECTED_DIR, exist_ok=True)
    old = read_expected(name) or set()
    keep = {d for d in old if d[0] not in seasons}           # seasons that this report does not hold
    with open(expected_path(name), "w", encoding="utf-8") as f:
        f.write("# Differences between the rules report and the rules table that the check accepts. Made by\n"
                "# tests/seasons/rules_report_check.py --update-expected. Columns: season, kind, text.\n")
        for s, kind, text in sorted(keep | set(diffs)):
            f.write(f"{s}\t{kind}\t{text}\n")
    print(f"expected differences written: {expected_path(name)}")


def check(report, game=None, last=None, expected=None, update=False, data_dir=None, start_only=False):
    """Prints the checks and the differences. Returns the failure texts."""
    clubs = []
    seasons, comps = read_report(report, clubs)
    rows = make_rules.read_rows()
    failed = []
    if not seasons:
        return ["the report holds no season"]
    start = seasons[0]["season"]
    got = [(s["season"], s["phase"]) for s in seasons]
    if last is not None:
        want = [(start, "start")] if start_only else [(y, p) for y in range(start, last + 1) for p in ("start", "end")]
        if got != want:
            failed.append(f"the report holds {got[:2]}...{got[-2:]} ({len(got)} parts), not the start and end of "
                          f"{start} to {last}")
    unknown = sorted({c["vtable"] for c in comps if c["file"] == "?"})
    if unknown:
        failed.append(f"competitions with an unknown class: {', '.join(unknown)}")
    if game and start == 2001:
        names = record_names(game)
        for c in comps:
            if c["season"] != 2001:
                continue
            want = names.get(("club_comp.dat", c["record"])), names.get(("nation_comp.dat", c["record"]))
            if c["name"] not in want:
                failed.append(f"base game: {c['slot']} {c['file']} record {c['record']} is {c['name']!r}, "
                              f"the data has {want[0]!r} or {want[1]!r}")
        divisions = division_counts(game)
        for c in comps:
            # Only the English divisions: the inference about +0x57 comes from them. Other nations hold
            # several objects of one class (stages) and groups, so a division count does not fit them.
            if (c["season"] == 2001 and c["phase"] == "start" and c["kind"] == "league" and c["file"].startswith("eng_")
                    and divisions.get(c["record"])):
                if c["clubs"] != divisions[c["record"]]:
                    failed.append(f"base game: {c['file']} holds {c['clubs']} clubs, club.dat has "
                                  f"{divisions[c['record']]} in division {c['record']}")
    per = collections.Counter((c["season"], c["phase"]) for c in comps)
    print(f"report: {len(got)} parts, {got[0][0]}/{(got[0][0] + 1) % 100:02d} to {got[-1][0]}/"
          f"{(got[-1][0] + 1) % 100:02d}")
    for s in seasons:
        print(f"  season {s['season']}/{(s['season'] + 1) % 100:02d} {s['phase']:5} on {s['date']}: "
              f"{per[(s['season'], s['phase'])]} competitions")
    diffs = differences(seasons, comps, rows)
    if data_dir and clubs:
        diffs = sorted(set(diffs) | club_differences(seasons, comps, clubs, data_dir, rows))
    if clubs:
        diffs = sorted(set(diffs) | division_differences(comps, clubs))
    kinds = collections.Counter(k for _, k, _ in diffs)
    print(f"differences with the rules table: {len(diffs)} ({', '.join(f'{k} {n}' for k, n in sorted(kinds.items()))})")
    if expected:
        held_seasons = {s["season"] for s in seasons}
        if update:
            write_expected(expected, diffs, held_seasons)
        known = read_expected(expected)
        if known is None:
            failed.append(f"no expected file {expected_path(expected)}: run with --update-expected")
        else:
            new = [d for d in diffs if d not in known]
            gone = sorted(d for d in known if d[0] in held_seasons and d not in set(diffs))
            for s, k, text in new:
                print(f"  NEW   {s}/{(s + 1) % 100:02d}  {k:12} {text}")
            for s, k, text in gone:
                print(f"  GONE  {s}/{(s + 1) % 100:02d}  {k:12} {text}")
            print(f"against {expected_path(expected)}: {len(new)} new, {len(gone)} gone")
            if new:
                failed.append(f"{len(new)} differences that the expected file does not list")
    else:
        for s, k, text in diffs:
            print(f"  {s}/{(s + 1) % 100:02d}  {k:12} {text}")
    return failed


def league_steps(nations, extra):
    """The new game steps of tests/smoke/smoke_run.py with other leagues selected."""
    import smoke_run
    steps = smoke_run.without_intro_click([s for s in smoke_run.STEPS if "dump" not in s and "quit" not in s])
    if nations == ["England"] and not [e for e in extra if e in EXTRA]:
        return steps
    points = []                                                     # a step's time counts from the last wait
    if nations[0] == "all":
        points.append("592 155")                                    # Select All
        for n in nations[1:]:                                       # then a click takes a nation out again
            points.append(f"390 {188 + 22 * NATION_ROWS.index(n)}")
    else:
        for n in nations:
            if n != "England":
                points.append(f"390 {188 + 22 * NATION_ROWS.index(n)}")
    for e in extra:
        if e in EXTRA:
            points.append(f"675 {EXTRA[e]}")
    picks = [f"{2000 + 1500 * i} click {p}" for i, p in enumerate(points)]
    many = nations != ["England"]
    out = []
    for s in steps:
        if s == "2500 click 705 572" and not picks[0] in out:      # Select League(s): Next
            out += picks + [f"{2000 + 1500 * len(picks) + 1000} click 705 572"]
            if many:
                out += ["wait screen:0x81e8d0", "2500 click 705 572"]   # Select Start Season: Next (England)
            continue
        if many and s in ("wait typing", "wait first-news"):
            # The game is made after the run to the start date. A 2025 start with all nations took 1226 to 1676 s on
            # the gates and passed 1800 s under the load of gate (a) and gate (b) together (ec579543, a b6a4c3ad
            # build the same to 0.1 s), so the wait is an hour. A hang still fails.
            s += " 3600000"
        if many and s == "1000 click 155 163":
            s = "2000 click 155 188"                                # Select Team: Arsenal, below the Nation box
        out.append(s)
    return out


def rules_table(program, game):
    """The rules table for RECOMP_RULES_TABLE (the names and club tables lie next to it): the build's
    seasons/rules.tsv next to the program, else the game folder's Seasons/rules.tsv. A download, the AppImage
    and a wrapper script (the Windows build under wine) have no seasons folder next to the program; without
    the fall-back their runs had no table at all."""
    table = os.path.join(os.path.dirname(os.path.abspath(program)), "seasons", "rules.tsv")
    return table if os.path.exists(table) else os.path.join(os.path.abspath(game), "Seasons", "rules.tsv")


def run(program, game, last, year, database, nations, extra, keep, timeout, options_more=None, env_more=None,
        start_only=False, stop_on_box=False):
    """Plays the season jump. options_more: lines of settings.txt over the defaults (for example
    changing_rules = on, tests/seasons/season_change_run.py); env_more: more environment variables; start_only:
    the run ends after the first start report (slice 15)."""
    import smoke_run
    import custom_database_run as cdr
    work = keep or tempfile.mkdtemp(prefix="cm0102-rules-report-")
    os.makedirs(work, exist_ok=True)
    user, home, shots = (os.path.join(work, d) for d in ("user", "home", "shots"))
    for d in (user, home, shots):
        os.makedirs(d)
    options = {"display": "classic", "changing_rules": "off", "faster_days": "on"}
    options.update(options_more or {})
    env_extra = dict(env_more or {})
    # Slice 20: Spain and Portugal sit below the 16 rows of the Select League(s) screen, so their extra division
    # comes from the test switch RECOMP_TEST_EXTRA (season_divisions.h).
    test_extra = [e for e in extra if e in ENV_EXTRA]
    if test_extra:
        env_extra["RECOMP_TEST_EXTRA"] = ",".join(test_extra)
    if year:
        env_extra["RECOMP_START_SEASON"] = str(year)
    if database:
        cdr.copy_data(database, os.path.join(user, "Databases", os.path.basename(database)))
        options["database"] = "folder:" + os.path.basename(database)
    with open(os.path.join(user, "settings.txt"), "w") as f:
        for k, v in options.items():
            f.write(f"{k} = {v}\n")
    steps = league_steps(nations, extra)
    report = os.path.join(work, "report.tsv")
    table = rules_table(program, game)
    env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
               RECOMP_USER_DIR=user, RECOMP_FIXED_CLOCK="1", RECOMP_SEASON_JUMP=str(last), RECOMP_SCREEN_LOG="1",
               RECOMP_RULES_REPORT=report, RECOMP_RULES_TABLE=table,
               **({"RECOMP_RULES_REPORT_STOP": "start"} if start_only else {}),
               RECOMP_SCRIPT="; ".join(smoke_run.scaled_steps("classic", shots, steps)), **env_extra)
    start = time.time()
    log_path = os.path.join(work, "stderr.txt")
    with open(log_path, "w") as log:
        if stop_on_box:                         # slice 30a: a box makes the game wait; stop it by its PID at once
            p = subprocess.Popen([program, "--game", game, "-seed", "1"], env=env, stdout=subprocess.DEVNULL,
                                 stderr=log)
            code = None
            while code is None:
                time.sleep(5)
                code = p.poll()
                with open(log_path, encoding="latin-1") as f:
                    boxed = "recomp: message box from" in f.read()
                if code is None and (boxed or time.time() - start > timeout):
                    p.kill()
                    p.wait()
                    code = "box" if boxed else "timeout"
        else:
            try:
                r = subprocess.run([program, "--game", game, "-seed", "1"], env=env, stdout=subprocess.DEVNULL,
                                   stderr=log, timeout=timeout)
                code = r.returncode
            except subprocess.TimeoutExpired:
                code = "timeout"
    print(f"run: code {code} after {time.time() - start:.0f} s, log {os.path.join(work, 'stderr.txt')}")
    return code, report


def arg(argv, name, default=None):
    return argv[argv.index(name) + 1] if name in argv else default


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    update = "--update-expected" in argv
    if argv[1] == "--league-advance":
        comps = [c for path in argv[2:] for c in read_report(path)[1]]
        stalls = league_stalls(comps)
        for line in stalls:
            print(f"FAIL  {line}")
        print("PASS" if not stalls else f"FAILED: {len(stalls)}")
        return 1 if stalls else 0
    if argv[1] != "--run":
        failed = check(argv[1], expected=arg(argv, "--expected"), update=update, data_dir=arg(argv, "--data"),
                       start_only="--start-only" in argv)
    else:
        program, game = os.path.abspath(argv[2]), os.path.abspath(argv[3])
        year = int(arg(argv, "--year", 0)) or None
        last = int(arg(argv, "--last", 2027))
        database = os.path.abspath(arg(argv, "--database")) if "--database" in argv else None
        nations = arg(argv, "--nations", "england")
        words = [x.strip().lower().replace("_", " ") for x in nations.split(",")]
        names = [next(n for n in NATION_ROWS if n.lower() == w.lstrip("-")) for w in words if w != "all"]
        nations = (["all"] + names) if words[0] == "all" else names
        if nations[0] != "all" and "England" not in nations:
            nations = ["England"] + nations                      # Arsenal: England stays selected
        extra = [x for x in arg(argv, "--extra", "").split(",") if x]
        keep = os.path.abspath(arg(argv, "--keep")) if "--keep" in argv else None
        timeout = int(arg(argv, "--timeout", 7200))
        start_only = "--start-only" in argv
        if start_only:
            last = year or 2001
        rules = arg(argv, "--changing-rules", "off")                    # slices 18 and 22
        if rules not in ("on", "off"):
            print("--changing-rules takes on or off")
            return 2
        code, report = run(program, game, last, year, database, nations, extra, keep, timeout,
                           options_more={"changing_rules": rules}, start_only=start_only)
        failed = [] if code == 0 else [f"the run ended with code {code}"]
        name = ("changing-" if rules == "on" else "") + f"{year or 2001}-{nations[0].lower()}" + "".join(
            f"{'-no' if nations[0] == 'all' else '-'}{n.lower().replace(' ', '_')}" for n in nations[1:]) + \
            "".join(f"-{e}" for e in extra) + ("-start" if start_only else "")
        if os.path.exists(report):
            failed += check(report, game if not database else None, last, name, update,
                            data_dir=database or os.path.join(game, "Data"), start_only=start_only)
        else:
            failed.append("the run wrote no report")
    for text in failed:
        print("FAIL  " + text)
    print("PASS" if not failed else f"FAILED: {len(failed)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
