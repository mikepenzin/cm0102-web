#!/usr/bin/env python3
"""The run set of the future play spec (0.6.2 slice 15): every game of run_set.tsv, several at a time within the
machine limits, a save at the end of each leg, the history checker on each save, the Tier B limits, and one pass or
fail per game. The release script checks the results with --verify.

Usage:
    python3 tests/seasons/future/run_set.py <cm0102_recomp> <game folder> --out <folder> [--set release]
        [--only <name,...>] [--jobs 2] [--machine 12] [--floor-gb 100] [--databases <folder>] [--own-dbs <folder>]
        [--data-3960 <folder>] [--data-3965 <folder>] [--seed <n>]
    python3 tests/seasons/future/run_set.py --verify <folder> [--set release] [--seed <n>]
    python3 tests/seasons/future/run_set.py --list [--set release] [--seed <n>]

--set: release (the rows of set release and nightly: the 34 rows of the release tests from 0.7.1), nightly, quick
(the short games that test the script), background (bg2300, the Tier A game to 2300, and bg2100, the full world from
2025 to 2100: they play while the Mac is idle and have no gate of --verify) or all. --only: these rows of the set. A game whose folder holds
result.tsv is done and is not played again; delete
its folder to play it again.

A game: legs of leg_years seasons, each played with tests/seasons/soak_run.py's new_game_leg or load_leg (the season
jump, the manager on holiday, all of RECOMP_FIXED_CLOCK, RECOMP_SCREEN_LOG and a stop at the first box). Every leg
saves on about 20 July after its last season (RECOMP_SAVE_AFTER_DAYS, counted from the save it loaded), the last leg
too (the season jump runs to the season after `last`), so the last save holds the end of the season `last`. A save
before July of the year after its last season fails the leg. Tier A sets RECOMP_TIER_A=1 (slice 3); a jump sets
RECOMP_DATE_JUMP=1 and plays the December 2025 database. A late row (open item 193) plays the full world from the checkpoint save
of its `from` row (a Tier A row of the same --out) in the year `start`: its leg 0 waits for that save, then loads it
with no test switch, and it gets the Tier B checks and limits. The save holds the Tier A seasons' history, so the
checker skips the checks of real results, as in Tier A. The databases of the other start years are those of
tests/seasons/gate_run.py; the runner unpacks the own databases once, before the games start.

After each leg: the leg's own checks (soak_run: no box, no line of the season change net, exit code 0, a save), then
the history checker on the save (history_check.py with its rules, names, nations and access files; with --health for
Tier B; a Tier A game skips same-final, a check of real results). Every fault fails the game, a fault of the known
list too, except an accepted row of history_known.tsv: its note cites a source that shows the game matches the
original or real life (column accepted: design or real). 0.6.2 ships only when the checker finds no fault that we
cannot explain in the run set (Q12); faults.tsv marks each fault new, known or accepted, and the result row counts
the accepted ones apart (column accepted). A known row with a rules condition matches only a game with that setting. The game plays on, so one run lists the faults of every decade. A
leg that is not ok fails the game and ends it. At the end, every division and domestic cup must move on from each
season to the next over the reports of all legs (league-stall: rules_report_check.league_stalls, as soak_run.py),
and with changing rules on the Greek Super League 2 plays one group from 2026/27 (greek-second, open item 68).

The Tier B limits, over the legs of a Tier B game:
    no-manager  a national side with no manager in two saves in a row (more than one season)
    slow        the time per game day of a decade is more than twice that of the first seasons (the median of legs
                1 and 2; leg 0 also makes the game)
    size        a save of 4 GB or more, or, once the game has 30 seasons of saves, a save size that at the growth
                per year of the last 10 seasons would pass 4 GB before the end of 2300 (open item 192: the history
                of the first player generation makes the growth of the first seasons about 2.5 times the later
                rate, so the projection waits for it and reads only the recent rate)
The checker's --health faults (empty-squad, no-youth, overflow) are checker faults of Tier B.

Disk (open item 53): a leg starts only with --floor-gb free on the disk of --out. A game keeps the save of one leg in
each decade (the first save of the decade) and the last save, and of these legs only the save; it deletes the user,
home and shots folders of every other leg once the next leg has loaded its save and passed. Each leg keeps
stderr.txt (gzipped) and report.tsv. A failed leg keeps its folder.

Room: a leg starts only while fewer than --jobs games of this run play, fewer than --machine games run on the Mac,
the load of the last minute is below 24 and /Users/max/Projects/cm0102/run-logs/HOLD does not exist.

The program: when it lies in this checkout, the script first builds it (build-slot.sh cmake --build, target
cm0102_recomp), so it is the build of the checkout's game code. Each result records the commit, "clean" (a program
of this checkout with no change in GAME_CODE), "changes" or "outside" (a program from elsewhere, for tests only), the
program's sha256 and the row of run_set.tsv it was played for.

Files in --out: results.tsv (a head line, then one row per game: name, tier, set, result, reason, reached, legs,
faults, seconds, commit, code, program, row, accepted), and per game <name>/legs.tsv (one row per leg), <name>/faults.tsv,
<name>/result.tsv, run.log and the leg folders. A game whose runner stops with no result gets the result fail.

--verify <folder>: exit 1 unless every row of the set (a row that waits too) has the result pass, played for the same
row of run_set.tsv, with a clean program, all with one program, at a commit whose game code (GAME_CODE: src,
tools/recomp, tools/seasons and docs/research/rules-0.6.0) is that of the checkout; every checkpoint save that
legs.tsv lists still exists; and the checker of this checkout finds no fault on any save the run kept.
"""
import csv
import datetime
import gzip
import hashlib
import os
import shutil
import statistics
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
SEASONS = os.path.join(ROOT, "tests", "seasons")
for p in (HERE, SEASONS, os.path.join(ROOT, "tests", "smoke"), os.path.join(ROOT, "tests", "fixtures")):
    sys.path.insert(0, p)
import gate_run  # noqa: E402
import history_check as hc  # noqa: E402

HOLD = "/Users/max/Projects/cm0102/run-logs/HOLD"
DATABASES = "/Users/max/Projects/cm0102/test-fixtures/databases"
OWN_DBS = "/Users/max/Projects/cm0102/own-dbs"
JUMP_DATABASE = "December 2025"
COLUMNS = ["name", "tier", "start", "changing", "nations", "seed", "last", "leg_years", "set", "status", "note"]
TIERS = {"A", "B", "jump", "normal", "late", "weekly", "world"}
# late (open item 193): the full world (as Tier B, with its health checks and limits) from a checkpoint save of a Tier
# A row. Its optional column `from` names that row (same changing rules, nations and seed); its start is the year of
# the save, so the game plays the seasons start to last in the full world on top of a long Tier A history.
# database (release tests 0.7.1, slice 1): tile (the data that the start screen gives the start year: 3.9.60 for 2001,
# the own and season databases for the other years, the December 2025 database for a jump), 3.9.60, 3.9.65 or 3.9.68.
# extras: none, all (the six extra divisions below) or names joined by commas. last: a season, or tile (the season
# before the end point of the start in ENDS: the game plays to its July save of the end year, after the later of the
# next World Cup and European Championship).
OPTIONAL = ["from", "database", "extras"]
DEFAULTS = {"database": "tile", "extras": "none"}
DATABASES_OF_ROW = ("tile", "3.9.60", "3.9.65", "3.9.68")
# The six extra divisions: the English Conference, German Regionalliga and Serie C2 are clicks of Select League(s)
# (rules_report_check.EXTRA); Spain, Portugal and Sweden come from RECOMP_TEST_EXTRA (rules_report_check.ENV_EXTRA).
ALL_EXTRAS = ["conference", "regional", "c2", "spain", "portugal", "sweden"]
# The nations that those extras need besides a Select All game (a quick row names them in `nations`).
DATA_FOLDERS = {"3.9.60": "/Users/max/Projects/cm0102/data-3960", "3.9.65": "/Users/max/Projects/cm0102/data-3965"}
ENDS = gate_run.ENDS
# weekly (open item 193, Max 8 November 2026): the full world with the test-only market, RECOMP_TEST_MARKET_WEEKLY=1
# (market_weekly.c): the AI market of the day loop runs on Mondays only. Faster, but it is not the shipped game; b2001
# and b2025 play the real market.
TIER_ENV = {"A": {"RECOMP_TIER_A": "1"}, "jump": {"RECOMP_DATE_JUMP": "1"},
            "weekly": {"RECOMP_TEST_MARKET_WEEKLY": "1"}}
SETS = {"release": {"release", "nightly"}, "nightly": {"nightly"}, "quick": {"quick"},
        "background": {"background"}, "all": {"release", "nightly", "quick", "background"}}
# What makes the game: its code, the recompiler, and the tables and their sources that the build and the release turn
# into the game's season data (tools/seasons, docs/research/rules-0.6.0).
GAME_CODE = ["src", "tools/recomp", "tools/seasons", "docs/research/rules-0.6.0"]
FOUR_GB = 4 << 30
# Open item 192: person history (player stats, staff, injury) grows until the first player generation has retired,
# then a new player reuses the history records of the retired one (PlayerStats::reset_player 0x7c6650, FILE_LLIST
# add_node 0x58e340). The Tier A run to 2301 grew 10.3 MB a season in the 2010s and 8.3 MB from the 2060s; b2001
# grew 26 MB a season in 2004 and 20 MB in 2011. So the projection starts after 30 seasons and uses the last 10.
SIZE_FIRST_SEASONS = 30
SIZE_WINDOW = 10
DAYS = 365.25
SAVE_NAME = "samesave.sav"                    # the save of a leg (smoke_run.SAVE_NAME)
SWITCHES = ("RECOMP_TIER_A", "RECOMP_DATE_JUMP", "RECOMP_TEST_EXTRA", "RECOMP_TEST_NATION_JOB",
            "RECOMP_TEST_MARKET_WEEKLY", "RECOMP_DATA_DIR")
# Q15: the rows whose human manager also manages a national team. Leg 0 sets RECOMP_TEST_NATION_JOB (nation_job.c): at
# the first human turn the runtime appoints him through the original's appoint_manager_to_club 0x69ebc0, to this
# nation or, when it has a manager, to the first senior national team with none and a nation value over 150. The
# leg must log the appointment and, after the team's next match, that he still manages the team and that the match
# was played (nation_job_check).
NATION_JOB = {"a2001nation": "England"}
NATION_JOB_LINE = "recomp: nation job test: "
SAVE_ON = 200                                 # every leg saves on day 200 (from 0, about 20 July) after its last season
TAIL_ON = 243                                 # the tail leg of a one-season world row: day 243 (1 September)
SCRIPT_TIMEOUT = "recomp: script: wait"       # a step of the new game script that timed out (testrun.c)


def game_days(year, day):
    """The day count of RECOMP_SAVE_AFTER_DAYS (testrun.c game_day): year * 366 + the day of the year from 0, so a
    year that is not a leap year counts one day more than it has."""
    return year * 366 + day
LEG_HEAD = ["leg", "seasons", "result", "seconds", "save_date", "save_mb", "faults", "note"]


def arg(argv, name, default=None):
    return argv[argv.index(name) + 1] if name in argv else default


def read_set(path=None, seed=None):
    """The rows of run_set.tsv; a row that does not fit the columns stops the script. seed (--seed, R8): the seed of
    every row, in place of the column (the release version gives it: 0.7.2 gives 72)."""
    rows = []
    with open(path or os.path.join(HERE, "run_set.tsv"), encoding="utf-8") as f:
        for r in csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t"):
            bad = [c for c in COLUMNS if r.get(c) in (None, "") and c != "note"]
            if bad or r["tier"] not in TIERS or r["status"] not in ("run", "waits") or \
                    r["set"] not in SETS["all"] or r["changing"] not in ("on", "off"):
                raise SystemExit(f"run_set.tsv: row {r.get('name')} does not fit the columns ({bad or 'a value'})")
            for c in OPTIONAL:
                r[c] = r.get(c) or DEFAULTS.get(c, "")
            if r["database"] not in DATABASES_OF_ROW:
                raise SystemExit(f"run_set.tsv: row {r['name']}: database {r['database']} is not one of "
                                 f"{', '.join(DATABASES_OF_ROW)}")
            for c in ("start", "seed", "leg_years"):
                r[c] = int(r[c])
            if seed is not None:
                r["seed"] = int(seed)
            r["last"] = end_season(r["start"], r["last"], r["name"])
            r["extras"] = extras_of(r["extras"], r["name"])
            if (r["tier"] == "late") != bool(r["from"]):
                raise SystemExit(f"run_set.tsv: row {r['name']}: a late row needs `from`, and only a late row has it")
            rows.append(r)
    names = [r["name"] for r in rows]
    if len(set(names)) != len(names):
        raise SystemExit("run_set.tsv: a name is used twice")
    for r in rows:
        if r["tier"] == "late":
            a = next((x for x in rows if x["name"] == r["from"]), None)
            if not a or a["tier"] != "A" or any(a[c] != r[c] for c in ("changing", "nations", "seed")) or \
                    not a["start"] < r["start"] <= a["last"] + 1 or (r["start"] - a["start"]) % a["leg_years"]:
                raise SystemExit(f"run_set.tsv: row {r['name']}: `from` must name a Tier A row of the same changing "
                                 f"rules, nations and seed, with a checkpoint save in the year {r['start']}")
    return rows


def end_season(start, last, name="row"):
    """The last season of a row: its number, or "tile" for the season before the end point of its start (ENDS)."""
    if last == "tile":
        if start not in ENDS:
            raise SystemExit(f"run_set.tsv: row {name}: no end point for the start {start}")
        return ENDS[start] - 1
    return int(last)


def extras_of(text, name="row"):
    """The list of extra divisions of an `extras` cell: none, all, or names joined by commas."""
    if text == "none":
        return []
    names = ALL_EXTRAS if text == "all" else text.split(",")
    bad = [n for n in names if n not in ALL_EXTRAS]
    if bad:
        raise SystemExit(f"run_set.tsv: row {name}: no extra division {', '.join(bad)}")
    return list(names)


def row_database(row):
    """The database of a row: 3.9.60, 3.9.65 or 3.9.68 for the base data, else the name of the season database
    ("tile" resolves to the data of the start year: 3.9.60 for 2001, DATES or OWN of gate_run for the others)."""
    if row["database"] != "tile":
        return row["database"]
    if row["tier"] == "jump":
        return JUMP_DATABASE
    if row["start"] == 2001:
        return "3.9.60"
    return gate_run.OWN.get(row["start"]) or gate_run.DATES[row["start"]]


def full_world(row):
    """A game of the full world with the health checks (the Tier B limits too, but for a world row)."""
    return row["tier"] in ("B", "late", "weekly", "world")


def checker_tier(row):
    """The tier for the checker's filter: a late game holds the history of its Tier A seasons, so like Tier A it
    skips the checks of real results (hc.tier_a_faults)."""
    return "A" if row["tier"] == "late" else row["tier"]


PROGRAM_FILE = "program.tsv"                  # <out>/<name>/program.tsv: commit, code and sha256 of the game's program


def game_program(out, name):
    """The sha256 of the program that plays (or played) the game `name` in --out: its program.tsv, else its
    result.tsv, else the head line of results.tsv (a game started by an older run_set.py has no program.tsv). None
    when nothing records it."""
    folder = os.path.join(out, name)
    for path, pick in ((os.path.join(folder, PROGRAM_FILE), lambda v: v[2]),
                       (os.path.join(folder, "result.tsv"), lambda v: v[RESULT_HEAD.index("program")])):
        if os.path.exists(path):
            with open(path, encoding="utf-8") as f:
                values = next(csv.reader(f, delimiter="\t"), [])
            try:
                return pick(values) or None
            except IndexError:
                pass
    head = os.path.join(out, "results.tsv")
    if os.path.exists(head):
        with open(head, encoding="utf-8") as f:
            words = f.readline().split()
        if "program" in words[:-1]:
            return words[words.index("program") + 1]
    return None


def checkpoint(row, o, log=print, poll=300):
    """(save, game_days of its date) of the late row's start: the save of the `from` row's leg that saved in the year
    row["start"], in the same --out. Waits while that row plays and has not saved it yet; None when the row has ended
    without it."""
    folder = os.path.join(o["out"], row["from"])
    said = False
    while True:
        legs = os.path.join(folder, "legs.tsv")
        if os.path.exists(legs):
            with open(legs, encoding="utf-8") as f:
                for leg in csv.DictReader(f, delimiter="\t"):
                    if leg["result"] == "ok" and leg["save_date"].startswith(f"{row['start']}-"):
                        save = os.path.join(folder, f"leg{int(leg['leg']):03d}", "user", SAVE_NAME)
                        if os.path.exists(save):
                            return save, game_days(row["start"], int(leg["save_date"].split("-")[1]) - 1)
        if os.path.exists(os.path.join(folder, "result.tsv")):
            return None
        if not said:
            log(f"waiting: the checkpoint save of {row['from']} in {row['start']}")
            said = True
        time.sleep(poll)


def chosen(rows, which, only=None):
    out = [r for r in rows if r["set"] in SETS[which]]
    if only:
        out = [r for r in out if r["name"] in only]
    return out


def legs_of(row):
    """[(first season, last season)] of each leg: leg_years seasons each, to the season `last`."""
    out, s = [], row["start"]
    while s <= row["last"]:
        out.append((s, min(s + row["leg_years"] - 1, row["last"])))
        s += row["leg_years"]
    return out


def free_gb(path):
    return shutil.disk_usage(path).free / 1e9


def machine_games():
    return len(subprocess.run(["pgrep", "-x", "cm0102_recomp"], capture_output=True, text=True).stdout.split())


def wait_room(o, log=print):
    """Waits for the HOLD file, the disk floor, the machine limit and the load (see the docstring)."""
    said = None
    while True:
        why = ("HOLD" if os.path.exists(HOLD) else
               f"{free_gb(o['out']):.0f} GB free, below {o['floor']} GB" if free_gb(o["out"]) < o["floor"] else
               f"{machine_games()} games run on the Mac" if machine_games() >= o["machine"] else
               "load 24 or more" if os.getloadavg()[0] >= 24 else None)
        if not why:
            return
        if why != said:
            log(f"waiting: {why}")
            said = why
        time.sleep(30)


# ---- The checker on one save ----

class Checker:
    """history_check.py with the files next to it (the files of the checkout that runs this script)."""

    def __init__(self):
        self.rules = hc.Rules(os.path.join(HERE, "competitions.tsv"), os.path.join(HERE, "history_names.tsv"))
        self.known = hc.read_known(os.path.join(HERE, "history_known.tsv"))
        self.nations = hc.read_nations(os.path.join(HERE, "nations.tsv"))
        self.access = hc.Access(hc.finals_table(), hc.read_access(os.path.join(HERE, "access.tsv")))

    def is_known(self, fault, rules=None, save=None):
        return hc.is_known(self.known, fault[0], fault[1], fault[2], rules, save)

    def status(self, fault, game):
        """"accepted", "known" or "new" for a fault of this game (the changing-rules setting of the save counts)."""
        return hc.known_status(self.known, fault[0], fault[1], fault[2], hc.rules_of(game), game.date[0]) or "new"

    def run(self, save, health, tier=None):
        """(game, faults [(check, competition, year, detail)], health summary, health rows). Every fault counts, a
        known one too, except an accepted one (Q12). status marks the faults of history_known.tsv in the lists."""
        game = hc.Game(save)
        faults = hc.check(game, self.rules, self.nations) + hc.check_nations(game, self.nations)
        faults += sorted(set(hc.entrants(game, self.rules, self.access, self.nations)[1]))
        summary, rows = {}, []
        if health:
            summary, rows, sick = hc.health(game)
            faults += sick
        faults += [("save-part", n, game.date[0], "the size does not fit its count, not read") for n in game.unread]
        if tier == "A":             # a check of real results counts only in Tier B (spec, Decisions: test switches)
            faults = hc.tier_a_faults(faults)
        return game, faults, summary, rows


# ---- The Tier B limits ----

def tier_b_limits(legs, last_year=2300):
    """Faults [(check, detail)] of the Tier B limits over the legs: [{leg, year (of the save), seconds, days, bytes,
    no_manager (a set of nations, or None when not read)}] in leg order."""
    faults = []
    previous = None
    for g in legs:
        if g.get("no_manager") is not None and previous is not None:
            both = sorted(g["no_manager"] & previous)
            if both:
                faults.append(("no-manager", f"{len(both)} national sides with no manager in the saves of "
                                             f"{g['year'] - 1} and {g['year']}: {', '.join(both[:8])}"))
        previous = g.get("no_manager")
    first = [g["seconds"] / g["days"] for g in legs if g.get("days") and g["leg"] in (1, 2)]
    if first:
        base = statistics.median(first)
        decades = {}
        for g in legs:
            if g.get("days") and g["leg"] > 2:
                decades.setdefault(g["year"] // 10 * 10, []).append(g["seconds"] / g["days"])
        for decade, r in sorted(decades.items()):
            if statistics.median(r) > 2 * base:
                faults.append(("slow", f"{statistics.median(r):.2f} s a game day in the {decade}s, the first seasons "
                                       f"{base:.2f} s"))
    sized = [(g["year"], g["bytes"]) for g in legs if g.get("bytes")]
    for year, size in sized:
        if size >= FOUR_GB:
            faults.append(("size", f"the save of {year} has {size / 1e9:.2f} GB"))
            break
    else:
        old_enough = bool(sized) and sized[-1][0] - sized[0][0] >= SIZE_FIRST_SEASONS
        window = [(y, s) for y, s in sized if y >= sized[-1][0] - SIZE_WINDOW] if old_enough else []
        if len(window) >= 2 and window[-1][0] > window[0][0]:
            per_year = (window[-1][1] - window[0][1]) / (window[-1][0] - window[0][0])
            at_end = window[-1][1] + per_year * (last_year + 1 - window[-1][0])
            if at_end >= FOUR_GB:
                faults.append(("size", f"{window[-1][1] / 1e9:.2f} GB in {window[-1][0]}, {per_year / 1e6:.1f} MB a "
                                       f"year: {at_end / 1e9:.2f} GB at the end of {last_year}"))
    return faults


# ---- The leagues ----

GRE_ONE_GROUP = 2026                          # the first season of one Super League 2 group (open item 68)
GRE_CLUBS = 16                                # rows gre-pol-cro-80 and -81: 16 clubs; GreSecond holds up to 21
                                              # since the 0.7.2 league sizes (tools/recomp/league_sizes.py GROWN)
IDLE = 2998                                   # the year of a division that is not held (season_divisions.c)


def greek_second(comps):
    """[(season, detail)]: a season from 2026/27 in which the Greek Super League 2 does not play as one group of
    GRE_CLUBS clubs, the other group waiting at IDLE (open item 68: the rules table holds one group of 16 from
    2026/27; a forum game kept the North and South groups). comps: the comp lines of the rules reports
    (rules_report_check.read_report), the legs in leg order. Only the start lines of a season count, and of each
    group (slot) only the first: a leg that loads a save writes its first start lines under the game's start season
    with the state at the load (season_report_turn), and the end line of the season before with the next year, as
    league_stalls keeps the first value (open item 104: the date jump to 2198 gave "0 groups play" in 2198/99 from
    the loaded legs of 2199 and 2200). Only a game with changing rules on and Greece loaded has these lines."""
    out = []
    lines = [c for c in comps if c["file"] == "gre_second" and c["phase"] == "start" and c["season"] >= GRE_ONE_GROUP]
    for season in sorted({c["season"] for c in lines}):
        groups = {}
        for c in lines:
            if c["season"] == season:
                groups.setdefault(c.get("slot", len(groups)), c)   # one group a slot, its first line
        playing = [c for c in groups.values() if c["year"] == season]
        waiting = [c for c in groups.values() if c["year"] == IDLE]
        if len(playing) != 1 or len(playing) + len(waiting) != len(groups):
            out.append((season, f"{len(playing)} groups play, {len(waiting)} wait at {IDLE}, not one group"))
        elif playing[0]["clubs"] != GRE_CLUBS:
            out.append((season, f"the group has {playing[0]['clubs']} clubs, not {GRE_CLUBS}"))
    return out


# ---- One game ----

def database_of(row, o):
    """(database folder, settings.txt option, RECOMP_DATA_DIR) of a row. A season database is a folder (as
    tests/seasons/gate_run.py picks it) and the option 3.9.68. The base data of 2001 are the option 3.9.60, 3.9.65 or
    3.9.68: 3.9.60 and 3.9.65 name their data folder (--data-3960, --data-3965, the folders of the clean files'
    Data-3.9.60 and Data-3.9.65 that the clean game of this Mac lacks). A late row loads a save of its `from` row, so
    it needs no database."""
    if row["tier"] == "late":
        return None, "3.9.68", None
    name = row_database(row)
    if name in DATA_FOLDERS:
        return None, name, o["data"][name]
    if name == "3.9.68":
        return None, name, None
    if row["tier"] == "jump":
        names = sorted(n for n in os.listdir(o["databases"]) if n.startswith(JUMP_DATABASE))
        if not names:
            raise SystemExit(f"run_set.py: no {JUMP_DATABASE} folder in {o['databases']}")
        return gate_run.data_folder(os.path.join(o["databases"], names[0])), "3.9.68", None
    args, _ = gate_run.database(row["start"], {"run": o["out"], "own": o["own"], "databases": o["databases"]})
    return (args[1] if args else None), "3.9.68", None


# ---- The new game of a row: its database and extra divisions, read from the save (slice 1) ----

# The nation of each extra division. The flag of a selected extra division is bit 0x04 of the byte +0x11c of the nation
# record (season_divisions.c NATION_SELECTED, EXTRA_DIVISION), and the save holds it. The competitions of the extra
# divisions are in every save, set or not, so only the flag tells.
EXTRA_NATION = {"conference": "England", "regional": "Germany", "c2": "Italy", "spain": "Spain",
                "portugal": "Portugal", "sweden": "Sweden"}
EXTRA_FLAG = (0x11c, 0x04)


def extras_missing(flags, extras):
    """The extra divisions in `extras` that the save does not set. flags: {nation name: the byte +0x11c}."""
    return [e for e in extras if not flags.get(EXTRA_NATION[e], 0) & EXTRA_FLAG[1]]


def extra_flags(sav):
    """{nation name: the byte +0x11c of its record} of a save's nation.dat (history_check.LazySav)."""
    nat = sav.read("nation.dat")
    return {hc.text(nat[i * hc.NATION:(i + 1) * hc.NATION], 4): nat[i * hc.NATION + EXTRA_FLAG[0]]
            for i in range(len(nat) // hc.NATION)}


def club_names(folder):
    """{nation name: the set of its club names} of the club.dat and nation.dat of a data folder."""
    with open(os.path.join(folder, "nation.dat"), "rb") as f:
        nat = f.read()
    nations = {hc.s32(nat, i * hc.NATION): hc.text(nat[i * hc.NATION:(i + 1) * hc.NATION], 4)
               for i in range(len(nat) // hc.NATION)}
    with open(os.path.join(folder, "club.dat"), "rb") as f:
        data = f.read()
    out = {}
    for i in range(len(data) // hc.CLUB):
        r = data[i * hc.CLUB:(i + 1) * hc.CLUB]
        out.setdefault(nations.get(hc.s32(r, 0x53), ""), set()).add(hc.text(r, 4))
    return out


def database_names(folders):
    """{database: {nation name: club names}} for the data folders {database: folder with club.dat and nation.dat}."""
    return {db: club_names(folder) for db, folder in folders.items()}


HITS = 3                # a save that holds this many club names of another database only is not this database


def save_database(save_clubs, names):
    """The database that a save plays. save_clubs: {nation name: club names} of the save (a game with a few nations
    keeps some of their clubs and adds others, for example second teams). A save of database d holds no club name
    that only another database e has. So d is out when the save holds HITS names of e that d lacks, in the nations of
    the save. The answer is the one database that stays, or None when none or more than one stays: the nations of
    the game then do not tell the data apart. 3.9.60, 3.9.65 and 3.9.68 differ most in France, Holland, Russia and
    the United States; England alone tells only 3.9.65 and 3.9.68 apart."""
    here = {n for n, c in save_clubs.items() if n and c}
    stays = [db for db, d in names.items()
             if all(sum(len(save_clubs[n] & (e.get(n, set()) - d.get(n, set()))) for n in here) < HITS
                    for other, e in names.items() if other != db)]
    return stays[0] if len(stays) == 1 else None


def new_game_check(row, game, o):
    """The reason a row's new game is not the game that the row names, or "" when it is. Reads the save of leg 0: the
    database by its club names, and the extra
    divisions by their competitions. A wrong setting would test the wrong game and pass."""
    want = row_database(row)
    if want in DATA_FOLDERS or want == "3.9.68":
        folders = dict(o["data"], **{"3.9.68": os.path.join(o["game"], "Data")})
        by_nation = {}
        for name, nation in game.clubs.values():
            by_nation.setdefault(game.nations.get(nation, ("",))[0], set()).add(name)
        got = save_database(by_nation, database_names(folders))
        if got != want:
            return f"the save plays {got or 'no known'} data, the row says {want}"
    flags = extra_flags(game.sav)
    gone = extras_missing(flags, row["extras"])
    surplus = [e for e in ALL_EXTRAS if e not in row["extras"] and e not in extras_missing(flags, [e])]
    if gone:
        return f"the save does not set the extra divisions {', '.join(gone)}"
    return f"the save sets the extra divisions {', '.join(surplus)}, which the row does not name" if surplus else ""


def program_has(program, text):
    with open(program, "rb") as f:
        return text.encode() in f.read()


def tsv_row(path, values, head=None):
    new = not os.path.exists(path)
    with open(path, "a", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        if new and head:
            w.writerow(head)
        w.writerow(values)


def tidy(work, keep_save):
    """Deletes a passed leg's game folders: all of them, or all but the save when it is kept. The stderr.txt is
    gzipped."""
    err = os.path.join(work, "stderr.txt")
    if os.path.exists(err):
        with open(err, "rb") as f, gzip.open(err + ".gz", "wb") as g:
            shutil.copyfileobj(f, g)
        os.remove(err)
    for d in ("home", "shots") + (() if keep_save else ("user",)):
        shutil.rmtree(os.path.join(work, d), ignore_errors=True)
    user = os.path.join(work, "user")
    if keep_save and os.path.isdir(user):            # the kept save only, not the database copy or the temp files
        for n in os.listdir(user):
            if n != SAVE_NAME:
                p = os.path.join(user, n)
                shutil.rmtree(p) if os.path.isdir(p) and not os.path.islink(p) else os.remove(p)


def nation_job_check(text):
    """The reason a leg 0 of a NATION_JOB row fails, from its stderr text, or "" when it passes: the runtime
    appointed the human, he managed the team at every human turn to the team's next match, and the team played that
    match."""
    lines = [ln[len(NATION_JOB_LINE):] for ln in text.splitlines() if ln.startswith(NATION_JOB_LINE)]
    made = [ln for ln in lines if ln.startswith("the human manages ") and ln.endswith("(appointed)")]
    if not made:
        return "nation job: no appointment" + (f" ({lines[-1]})" if lines else "")
    team = made[0][len("the human manages "):].split(" from day ")[0]
    if not [ln for ln in lines if ln.startswith("on day ") and ln.endswith(f"the human still manages {team}")]:
        return f"nation job: no line that the human still manages {team} after its next match"
    if not [ln for ln in lines if ln.startswith(f"{team} played the match of day ")]:
        return f"nation job: no line that {team} played its next match"
    return ""


STUCK_LEGS = 3          # a club that is full (check full-squad) in this many saves of a game in a row is no design effect


def stuck(streak, faults, kinds):
    """Regrades the accepted full-squad faults of one leg. streak maps a club to the number of saves in a row in which
    it was full (the saves before this one); it is updated in place. A club full in STUCK_LEGS saves in a row is a
    club that never releases players: its fault is "new" again (item 191). Returns the new kinds."""
    now = {comp for c, comp, y, detail in faults if c == "full-squad"}
    for club in list(streak):
        if club not in now:
            del streak[club]
    for club in now:
        streak[club] = streak.get(club, 0) + 1
    return ["new" if kind == "accepted" and c == "full-squad" and streak[comp] >= STUCK_LEGS else kind
            for (c, comp, y, detail), kind in zip(faults, kinds)]


def grade(checker, faults, game, streak=None):
    """([the kind of each fault: "new", "known" or "accepted"], the number of accepted ones). An accepted fault cites
    a source that shows the game matches the original or real life; it is no fault for Q12 and is counted apart.
    streak (a dict, one a game) makes an accepted full-squad fault new again for a club that is full in 3 saves in a
    row (see stuck)."""
    kinds = [checker.status((c, comp, y), game) for c, comp, y, detail in faults]
    if streak is not None:
        kinds = stuck(streak, faults, kinds)
    return kinds, kinds.count("accepted")


def play_game(row, o):
    """Plays one row; writes <out>/<name>/legs.tsv, faults.tsv and result.tsv. Returns the result row."""
    import soak_run
    folder = os.path.join(o["out"], row["name"])
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, PROGRAM_FILE), "w", encoding="utf-8") as f:
        f.write(f"{o['commit']}\t{o['code']}\t{o['sha']}\n")
    log = lambda text: print(f"{row['name']}: {text}", flush=True)  # noqa: E731
    began = time.time()
    if row["status"] == "waits":
        return finish(folder, row, "waits", row["note"], "", 0, 0, began, o)
    if row["tier"] == "A" and not program_has(o["program"], "RECOMP_TIER_A"):
        return finish(folder, row, "waits", "the program has no Tier A switch (slice 3)", "", 0, 0, began, o)
    for name in SWITCHES:                       # a switch of the shell must not reach a game of another tier
        os.environ.pop(name, None)
    if row["tier"] == "weekly" and not program_has(o["program"], "RECOMP_TEST_MARKET_WEEKLY"):
        return finish(folder, row, "waits", "the program has no weekly market switch (item 193)", "", 0, 0, began, o)
    os.environ.update(TIER_ENV.get(row["tier"], {}))
    # one game a process: soak_run's leg_env copies os.environ
    base = next(r for r in read_set(o.get("set_file"), seed=o.get("seed")) if r["name"] == row["from"]) if row.get("from") else row
    so = {"program": o["program"], "game": o["game"], "start": row["start"], "last": row["last"] + 1,
          "seed": row["seed"], "nations": row["nations"].split(","), "rules": row["changing"],
          "extra": row["extras"]}
    so["database"], so["option"], data_dir = database_of(base, o)
    if data_dir:
        os.environ["RECOMP_DATA_DIR"] = data_dir        # every leg: the data of 3.9.60 or 3.9.65 (files.c)
    checker = Checker()
    health = full_world(row)
    legs, kept_decades, save, previous_work, new_faults, accepted = [], set(), None, None, 0, 0
    full_streak = {}                                    # club -> saves in a row in which it was full
    plan = [(s0, s1, SAVE_ON) for s0, s1 in legs_of(row)]
    if row["tier"] == "world" and row["last"] == row["start"]:
        # A one-season row (a tile row, R2): after the July save, one more leg loads it and plays to 1 September, so
        # the save and load of the first season change are tested too.
        plan.append((row["last"], row["last"], TAIL_ON))
    since = game_days(row["start"], 181)              # the first day of a new game: about 1 July of the start year
    if row["tier"] == "late":                         # leg 0 loads the Tier A row's save of the start year
        found = checkpoint(row, o, log)
        if not found:
            return finish(folder, row, "fail", f"{row['from']} ended with no checkpoint save in {row['start']}", "",
                          0, 0, began, o)
        save, since = found
        made_by = game_program(o["out"], row["from"])
        if made_by != o["sha"]:             # the Tier A seasons must be the same game code as the full-world ones
            return finish(folder, row, "fail", f"the checkpoint save of {row['from']} comes from another program "
                                               f"({(made_by or 'not recorded')[:12]}, this one {o['sha'][:12]})", "",
                          0, 0, began, o)
        log(f"leg 0 loads {os.path.relpath(save, o['out'])}")
    for k, (s0, s1, save_on) in enumerate(plan):
        work = os.path.join(folder, f"leg{k:03d}")
        if os.path.exists(work):
            return finish(folder, row, "fail", f"{work} exists: delete the game's folder to play it again", "",
                          k, new_faults, began, o)
        # RECOMP_SAVE_AFTER_DAYS counts in its own days (game_days) from the leg's first human turn: from the save it
        # loads, to day SAVE_ON after the leg's last season. In real days a 10-season leg saved up to 20 days early.
        days = game_days(s1 + 1, save_on) - since
        so["timeout"] = 14400 * (s1 - s0 + 1)
        so["last"] = s1 + (2 if save_on == TAIL_ON else 1)        # the tail leg must not end at the season jump
        wait_room(o, log)
        if k == 0 and row["tier"] != "late":
            if row["name"] in NATION_JOB:
                os.environ["RECOMP_TEST_NATION_JOB"] = NATION_JOB[row["name"]]     # leg 0 only: a loaded game keeps it
            code, took, box = soak_run.new_game_leg(so, work, days)
            err = os.path.join(work, "stderr.txt")
            if code == 3 and not box and os.path.exists(err) and SCRIPT_TIMEOUT in open(err, errors="replace").read():
                # The new game script is timed in ms, not by state: under the load of eight games making their worlds
                # a step went astray (a2013s2 and a2013s3 of pass 1; the same seed passed alone). No game exists
                # yet, so the leg is made again, once.
                os.rename(work, work + "-script-timeout")
                log("leg 0: the new game script timed out; making the game again")
                wait_room(o, log)
                code, took, box = soak_run.new_game_leg(so, work, days)
            os.environ.pop("RECOMP_TEST_NATION_JOB", None)
        else:
            code, took, box = soak_run.load_leg(so, work, save, days)
        saved = os.path.join(work, "user", SAVE_NAME)
        net = soak_run.first_net(os.path.join(work, "stderr.txt"))
        result = ("box" if box else "net" if net else f"code {code}" if code != 0 else
                  "no save" if not os.path.exists(saved) else "ok")
        seasons = f"{s0}/{(s0 + 1) % 100:02d} to {s1}/{(s1 + 1) % 100:02d}"
        if result != "ok":
            tsv_row(os.path.join(folder, "legs.tsv"), [k, seasons, result, f"{took:.0f}", "", "", "", box or net or ""],
                    LEG_HEAD)
            return finish(folder, row, "fail", f"leg {k} ({seasons}): {result} {box or net or ''}".strip(), "", k,
                          new_faults, began, o)
        job = nation_job_check(open(os.path.join(work, "stderr.txt"), errors="replace").read()) \
            if k == 0 and row["name"] in NATION_JOB else ""
        if job:
            tsv_row(os.path.join(folder, "legs.tsv"), [k, seasons, "nation job", f"{took:.0f}", "", "", "", job], LEG_HEAD)
            return finish(folder, row, "fail", f"leg {k} ({seasons}): {job}", "", k, new_faults, began, o)
        loaded = os.path.join(work, "user", "soak.sav")         # load_leg's copy of the save it loaded
        if os.path.exists(loaded):
            os.remove(loaded)
        game, faults, summary, rows = checker.run(saved, health, checker_tier(row))
        year = game.date[0]
        wrong = new_game_check(row, game, o) if k == 0 and row["tier"] != "late" else ""
        if wrong:
            tsv_row(os.path.join(folder, "legs.tsv"), [k, seasons, "wrong game", f"{took:.0f}", "", "", "", wrong],
                    LEG_HEAD)
            return finish(folder, row, "fail", f"leg {k} ({seasons}): {wrong}", "", k, new_faults, began, o)
        if (year, game.date[1]) < (s1 + 1, 181 if save_on == SAVE_ON else TAIL_ON):   # after the season change of s1
            tsv_row(os.path.join(folder, "legs.tsv"), [k, seasons, "short", f"{took:.0f}",
                                                       f"{year}-{game.date[1] + 1:03d}", "", "", ""], LEG_HEAD)
            return finish(folder, row, "fail", f"leg {k} ({seasons}): the save is of day {game.date[1] + 1} of {year}, "
                                               f"before the end of the leg in {s1 + 1}", "", k, new_faults, began, o)
        since = game_days(year, game.date[1])
        kinds, here = grade(checker, faults, game, full_streak)
        for (c, comp, y, detail), kind in zip(faults, kinds):
            tsv_row(os.path.join(folder, "faults.tsv"), [c, comp, y, kind, detail, os.path.relpath(saved, folder)],
                    ["check", "competition", "year", "known", "detail", "save"])
        accepted += here                                # an accepted row is no fault (Q12); it is counted apart
        new_faults += len(faults) - here
        no_manager = None
        if health:
            no_manager = {r[0] for r in rows if r[7] == 0}
            hc.write_health(summary, rows, os.path.join(work, "health.tsv"))
        size = os.path.getsize(saved)
        legs.append({"leg": k, "year": year, "seconds": took, "days": days, "bytes": size, "no_manager": no_manager})
        keep = year // 10 not in kept_decades or k == len(plan) - 1
        if keep:
            kept_decades.add(year // 10)
        tsv_row(os.path.join(folder, "legs.tsv"),
                [k, seasons, "ok", f"{took:.0f}", f"{year}-{game.date[1] + 1:03d}", f"{size / 1e6:.0f}",
                 len(faults) - here, "checkpoint" if keep else ""], LEG_HEAD)
        log(f"leg {k} ({seasons}) ok in {took:.0f} s, save of {year} ({size / 1e6:.0f} MB), {len(faults) - here} faults, "
            f"{here} accepted")
        if previous_work:
            tidy(previous_work[0], previous_work[1])
        previous_work, save = (work, keep), saved
    if previous_work:
        tidy(previous_work[0], previous_work[1])
    limits = tier_b_limits(legs, row["last"]) if health and row["tier"] != "world" else []   # decades of saves
    # every division and domestic cup moves on from one season to the next, over the reports of all legs (O278)
    import rules_report_check as rrc
    reports = [os.path.join(folder, f"leg{k:03d}", "report.tsv") for k in range(len(plan))]
    comps = [c for r in reports if os.path.exists(r) for c in rrc.read_report(r)[1]]
    limits += [("league-stall", line) for line in rrc.league_stalls(comps)]
    if row["changing"] == "on":
        limits += [("greek-second", f"{season}/{(season + 1) % 100:02d}: {detail}")
                   for season, detail in greek_second(comps)]
    for c, detail in limits:
        tsv_row(os.path.join(folder, "faults.tsv"), [c, "Tier B" if c in ("no-manager", "slow", "size") else "leagues",
                                                     legs[-1]["year"], "new", detail, ""],
                ["check", "competition", "year", "known", "detail", "save"])
    reached = f"{legs[-1]['year']}" if legs else ""
    if new_faults or limits:
        return finish(folder, row, "fail", f"{new_faults} checker faults, {len(limits)} other, {accepted} accepted "
                                           f"(faults.tsv)", reached, len(plan), new_faults + len(limits), began, o,
                      accepted)
    return finish(folder, row, "pass", f"{accepted} accepted" if accepted else "", reached, len(plan), 0, began, o,
                  accepted)


def row_key(row):
    """The row as run_set.tsv holds it, but the note: a result counts only for the row it was played for."""
    return "|".join([str(row[c]) for c in COLUMNS if c != "note"] +
                    [f"{c}={','.join(row[c]) if isinstance(row[c], list) else row[c]}" for c in OPTIONAL if row.get(c)])


def finish(folder, row, result, reason, reached, legs, faults, began, o, accepted=0):
    values = [row["name"], row["tier"], row["set"], result, reason, reached, legs, faults, f"{time.time() - began:.0f}",
              o["commit"], o["code"], o["sha"], row_key(row), accepted]
    with open(os.path.join(folder, "result.tsv"), "w", encoding="utf-8", newline="") as f:
        csv.writer(f, delimiter="\t", lineterminator="\n").writerow(values)
    print(f"{row['name']}: {result.upper()} {reason}".rstrip(), flush=True)
    return values


# ---- The run ----

RESULT_HEAD = ["name", "tier", "set", "result", "reason", "reached", "legs", "faults", "seconds", "commit", "code",
               "program", "row", "accepted"]


def git(*args):
    return subprocess.run(["git", "-C", ROOT] + list(args), capture_output=True, text=True).stdout.strip()


def identity(program):
    """(commit, code, program sha256): code is "clean" for a program of this checkout's build folder whose game code
    has no change, else "changes" or "outside" (a program from elsewhere, for tests only)."""
    with open(program, "rb") as f:
        sha = hashlib.sha256(f.read()).hexdigest()
    inside = os.path.realpath(program).startswith(os.path.realpath(ROOT) + os.sep)
    code = "outside" if not inside else "changes" if git("status", "--porcelain", "--", *GAME_CODE) else "clean"
    return git("rev-parse", "HEAD"), code, sha


def write_results(o, rows):
    out = os.path.join(o["out"], "results.tsv")
    with open(out, "w", encoding="utf-8", newline="") as f:
        f.write(f"# commit {o['commit']} {o['code']} program {o['sha']} {datetime.datetime.now():%Y-%m-%d %H:%M}\n")
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(RESULT_HEAD)
        for r in rows:
            done = os.path.join(o["out"], r["name"], "result.tsv")
            if os.path.exists(done):
                with open(done, encoding="utf-8") as g:
                    w.writerow(next(csv.reader(g, delimiter="\t")))
            else:
                w.writerow([r["name"], r["tier"], r["set"], "not run"] + [""] * (len(RESULT_HEAD) - 4))
    return out


def run(argv):
    o = {"program": os.path.abspath(argv[1]), "game": os.path.abspath(argv[2]), "out": os.path.abspath(arg(argv, "--out")),
         "jobs": int(arg(argv, "--jobs", 2)), "machine": int(arg(argv, "--machine", 12)),
         "floor": float(arg(argv, "--floor-gb", 100)), "databases": arg(argv, "--databases", DATABASES),
         "own": arg(argv, "--own-dbs", OWN_DBS),
         "data": {"3.9.60": arg(argv, "--data-3960", DATA_FOLDERS["3.9.60"]),
                  "3.9.65": arg(argv, "--data-3965", DATA_FOLDERS["3.9.65"])}}
    o["seed"] = arg(argv, "--seed")
    only = arg(argv, "--only", "").split(",") if "--only" in argv else None
    rows = chosen(read_set(seed=o["seed"]), arg(argv, "--set", "release"), only)
    if only and set(only) - {r["name"] for r in rows}:
        print(f"run_set.py: not in the set: {', '.join(sorted(set(only) - {r['name'] for r in rows}))}")
        return 2
    os.makedirs(o["out"], exist_ok=True)
    build = os.path.dirname(o["program"])
    if os.path.realpath(build).startswith(os.path.realpath(ROOT) + os.sep):     # the program of this checkout's tree
        if subprocess.run(["/Users/max/Projects/cm0102/build-slot.sh", "cmake", "--build", build, "--target",
                           "cm0102_recomp"], capture_output=True).returncode != 0:
            print(f"run_set.py: the build in {build} failed")
            return 2
    o["commit"], o["code"], o["sha"] = identity(o["program"])
    for r in rows:                              # unpack the own databases once, before the games start
        if r["status"] == "run":
            database_of(r, o)
    todo = [r for r in rows if not os.path.exists(os.path.join(o["out"], r["name"], "result.tsv"))]
    running = {}
    while todo or running:
        for name, p in list(running.items()):
            if p.poll() is not None:
                del running[name]
                done = os.path.join(o["out"], name, "result.tsv")
                if not os.path.exists(done):            # the child stopped with no result: a fail, with its last line
                    with open(os.path.join(o["out"], name, "run.log"), encoding="utf-8", errors="replace") as g:
                        last = ([l.strip() for l in g if l.strip()] or ["no output"])[-1]
                    row = next(r for r in rows if r["name"] == name)
                    finish(os.path.join(o["out"], name), row, "fail", f"the runner stopped (exit {p.returncode}): "
                                                                       f"{last}", "", "", "", time.time(), o)
                write_results(o, rows)
        while todo and len(running) < o["jobs"]:
            r = todo.pop(0)
            os.makedirs(os.path.join(o["out"], r["name"]), exist_ok=True)
            with open(os.path.join(o["out"], r["name"], "run.log"), "a") as logf:
                running[r["name"]] = subprocess.Popen(
                    [sys.executable, "-I", os.path.abspath(__file__), "--one", r["name"], o["program"], o["game"],
                     "--out", o["out"], "--machine", str(o["machine"]), "--floor-gb", str(o["floor"]),
                     "--databases", o["databases"], "--own-dbs", o["own"], "--data-3960", o["data"]["3.9.60"],
                     "--data-3965", o["data"]["3.9.65"], "--commit", o["commit"], "--code", o["code"], "--sha",
                     o["sha"]] + (["--seed", str(o["seed"])] if o["seed"] is not None else []),
                    stdout=logf, stderr=subprocess.STDOUT)
            print(f"{r['name']}: started", flush=True)
            time.sleep(30)                  # the games do not all make their worlds at once
        time.sleep(10)
    out = write_results(o, rows)
    with open(out, encoding="utf-8") as f:
        results = list(csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t"))
    for r in results:
        print(f"{r['result']:8} {r['name']} {r['reason']}")
    return 0 if all(r["result"] == "pass" for r in results) else 1


def one(argv):
    name = argv[argv.index("--one") + 1]
    rest = [a for i, a in enumerate(argv) if i not in (argv.index("--one"), argv.index("--one") + 1)]
    o = {"program": os.path.abspath(rest[1]), "game": os.path.abspath(rest[2]), "out": os.path.abspath(arg(rest, "--out")),
         "machine": int(arg(rest, "--machine", 12)), "floor": float(arg(rest, "--floor-gb", 100)),
         "databases": arg(rest, "--databases", DATABASES), "own": arg(rest, "--own-dbs", OWN_DBS),
         "commit": arg(rest, "--commit"), "code": arg(rest, "--code"), "sha": arg(rest, "--sha"),
         "seed": arg(rest, "--seed"),
         "data": {"3.9.60": arg(rest, "--data-3960", DATA_FOLDERS["3.9.60"]),
                  "3.9.65": arg(rest, "--data-3965", DATA_FOLDERS["3.9.65"])}}
    row = next(r for r in read_set(seed=o["seed"]) if r["name"] == name)
    return 0 if play_game(row, o)[3] in ("pass", "waits") else 1


# ---- The release check ----

def verify(folder, which="release", rows=None):
    """Lines of the faults of a run for the release check; empty when it passes. rows: the run set (run_set.tsv)."""
    rows = chosen(rows if rows is not None else read_set(), which)
    out = []
    path = os.path.join(folder, "results.tsv")
    if not os.path.exists(path):
        return [f"{path} does not exist"]
    with open(path, encoding="utf-8") as f:
        results = {r["name"]: r for r in csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t")}
    programs = set()
    for row in rows:
        r = results.get(row["name"])
        if not r:
            out.append(f"{row['name']}: not in the run")
            continue
        if r["result"] != "pass":
            out.append(f"{row['name']}: {r['result']} {r['reason']}".rstrip())
        if r.get("row") != row_key(row):
            out.append(f"{row['name']}: played for another row of run_set.tsv ({r.get('row')})")
        if r.get("code") != "clean":
            out.append(f"{row['name']}: played with a program that is not a build of the checkout's game code "
                       f"({r.get('code')})")
        commit = r.get("commit") or ""
        if not commit or subprocess.run(["git", "-C", ROOT, "diff", "--quiet", commit, "HEAD", "--"] + GAME_CODE,
                                        capture_output=True).returncode != 0:
            out.append(f"{row['name']}: the game code changed after its commit {commit[:8] or '(none)'} "
                       f"({', '.join(GAME_CODE)})")
        programs.add(r.get("program"))
        if row.get("from"):
            base = results.get(row["from"])
            if not base or base.get("program") != r.get("program"):     # each row's game code is checked above
                out.append(f"{row['name']}: its checkpoint save comes from {row['from']}, which was not played with "
                           f"the same program")
        legs = os.path.join(folder, row["name"], "legs.tsv")
        if r["result"] == "pass" and os.path.exists(legs):
            with open(legs, encoding="utf-8") as f:
                for leg in csv.DictReader(f, delimiter="\t"):
                    save = os.path.join(folder, row["name"], f"leg{int(leg['leg']):03d}", "user", SAVE_NAME)
                    if leg["note"] == "checkpoint" and not os.path.exists(save):
                        out.append(f"{row['name']}: the checkpoint save of leg {leg['leg']} is gone")
        elif r["result"] == "pass":
            out.append(f"{row['name']}: no legs.tsv")
    if len(programs) > 1:
        out.append(f"the games were played with {len(programs)} different programs")
    checker = Checker()
    for row in rows:
        for dirpath, _, files in os.walk(os.path.join(folder, row["name"])):
            if SAVE_NAME in files:
                save = os.path.join(dirpath, SAVE_NAME)
                try:
                    game, faults, _, _ = checker.run(save, full_world(row), checker_tier(row))
                    faults = [f for f in faults if checker.status(f, game) != "accepted"]
                except Exception as e:          # a save the checker cannot read is a fault of the run
                    out.append(f"{row['name']}: {os.path.relpath(save, folder)}: not read ({e})")
                    continue
                for c, comp, y, detail in faults:
                    out.append(f"{row['name']}: {os.path.relpath(save, folder)}: {c} {comp} {y}: {detail}")
    return out


def listing(rows):
    """One line a row for --list: the name, tier, start, database, extras, changing rules, last season, seed and legs."""
    out = [f"{len(rows)} rows"]
    for r in rows:
        out.append(f"  {r['name']:9} {r['tier']:6} start {r['start']} {row_database(r):15} extras "
                   f"{','.join(r['extras']) or 'none':45} changing {r['changing']:3} to {r['last']} seed {r['seed']} "
                   f"legs of {r['leg_years']}")
    return out


def main(argv):
    if "--list" in argv:
        print("\n".join(listing(chosen(read_set(seed=arg(argv, "--seed")), arg(argv, "--set", "release")))))
        return 0
    if "--verify" in argv:
        faults = verify(os.path.abspath(arg(argv, "--verify")), arg(argv, "--set", "release"),
                        read_set(seed=arg(argv, "--seed")))
        for line in faults:
            print("FAIL  " + line)
        print("run set: PASS" if not faults else f"run set: FAILED, {len(faults)} faults")
        return 1 if faults else 0
    if "--one" in argv:
        return one(argv)
    if len(argv) < 3 or "--out" not in argv:
        print(__doc__)
        return 2
    return run(argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
