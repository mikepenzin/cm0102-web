#!/usr/bin/env python3
"""Nations League check (0.6.0 slice 28): the UEFA and CONCACAF Nations Leagues, the CONCACAF Gold Cup and the
Finalissima in a season jump.

Usage:
    python3 tests/seasons/season_nl_run.py <cm0102_recomp> <game folder> [--only <name>,...] [--jobs n]
        [--keep <folder>] [--reuse]

Each run is a season jump of tests/seasons/rules_report_check.py (England, the quick result engine) with the
test log RECOMP_SEASON_NL_LOG=1. The runs:
    2018    October 2018, 2018/19 to 2019/20, changing rules on: the 2018-19 edition from the real leagues,
            the finals in June 2019, the 2020-21 leagues from the results (draw in March 2020)
    2020    November 2020, 2020/21 to 2021/22, changing rules on: the 2020-21 edition, finals in June 2021 (Q277),
            the 2022 World Cup qualifying on its 2021 matchdays and its three paths in March 2022
    2022    October 2022, 2022/23 to 2023/24, changing rules on: the 2024-25 edition (draw February 2024)
            (2018 and 2022: Martinique, with no continent in the data, plays in CONCACAF and its first CONCACAF draw
            puts it in League A, open item 141)
    2024    October 2024, 2024/25, changing rules on: the 2024-25 edition and its finals
    off     October 2018, 2018/19 to 2019/20, changing rules off: the 2018-19 shape again for 2020
    2010    September 2010, 2010/11, changing rules on: the 2011 Gold Cup of 12 teams (no Nations League)
Part b in the runs: the CONCACAF Nations League 2019-20 (run 2018: real leagues, finals in June 2020), 2022-23
(run 2020), 2023-24 and 2024-25 (run 2022), 2024-25 and its finals (run 2024); the Gold Cups of 16 of 2019, 2021,
2023 and 2025; the Finalissima of 1 June 2022 (run 2020).
    save    October 2018, changing rules on: the jump saves after 430 days (September 2019, after the 2018-19
            finals and before the 2020-21 draw) and stops; a second game loads that save in a new user folder and
            jumps to the end of 2019/20. The leagues of its 2020-21 draw must be those of run 2018 (the same
            seed and clicks give the same 2018-19 results), so promotion and relegation hold across a save and a
            load. Run 2018 first (or with it, in --keep).

The checks (each run):
    - the jump ends with code 0 and the report holds the end of the last season
    - the object is made for the first edition of the table from the start season (log "season nl: <edition>")
    - the first draw puts each nation of the real leagues (docs/research/rules-0.6.0/nations-league-tiers.tsv)
      that the log names in its real league and group, and no league holds more nations than the edition
    - each finals line names four nations, all of League A of that edition's draw
    - a nation that moves between editions moves one league, and the next draw puts it there
    repair  0.6.2 slice 8 (open items 4 and 21): loads a save of 0.6.1 (October 2018 start, England, changing
            rules on, the save of run "save" made by the 0.6.1 build: September 2019) whose Nations League record
            keeps the short name "Asian Cup Quals" and continent Asia, plays 400 days (to October 2020: the
            2020-21 draw and its first matchdays) and saves. The log must name the repair at load, and the
            history checker must find the fault in the 0.6.1 save and none in the new save. The save:
            RECOMP_NL_REPAIR_SAVE, else test-fixtures/nl-asia-record-0.6.1.sav; with neither the run fails.
    late    open item 61: a date jump (RECOMP_DATE_JUMP=1) to 2098 with the December 2025 data, changing rules
            on, saved after 400 days (August 2099). The table ends with the 2026-27 editions, so the game takes
            its last editions again (decision A): both Nations Leagues are made, the UEFA one plays its finals in
            June 2099 and the history checker finds the 2099 rows of both and no record fault.
    late-off  the same with changing rules off (Q232: the start season's rules, the last row after the table).
    late-2027 an odd start: 2027 (no date jump), changing rules on, saved after 760 days (August 2029): the first
            edition is the 2028 one, made in 2027, and the 2029 rows of both follow.
    fin-2001  open item 74 (the forum): the Finalissima's past editions. A 2001 game with the 3.9.68 data and
            changing rules on (the forum game), saved after 14 days: the history holds 1985 France / Uruguay and
            1993 Argentina / Denmark (Wikipedia, Finalissima, Results), and no 2022 row yet.
    fin-2025  the same from 2025 with the December 2025 data: also 2022 Argentina / Italy.
    fin-2020  November 2020 to the save after 700 days (June 2022): the game plays the 2022 edition itself, so
            the history holds 1985, 1993 and exactly one 2022 row, the game's.
The history checker (tests/seasons/future/history_check.py) also reads the save of the first game of run "save":
no comp-continent, comp-name, confederation or entrant-confed fault may name the UEFA Nations League or the
"Asian Cup Quals" (open items 4 and 21).
A run takes about 6 to 10 minutes. --jobs (default 1) runs that many at the same time. --reuse checks the
logs already in the --keep folder again without a new run.
"""
import concurrent.futures
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
sys.path.insert(0, os.path.join(ROOT, "tools", "seasons"))
import make_nl  # noqa: E402
import rules_report_check as rrc  # noqa: E402
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import custom_database_run as cdr  # noqa: E402
import fixtures  # noqa: E402
import smoke_run  # noqa: E402

DB = "/Users/max/Projects/cm0102/test-fixtures/databases"
# name: (start year, database, changing rules, last season, first edition, editions drawn in the run)
RUNS = {
    "2018": (2018, "October 2018", "on", 2019, "2018-19", ["2018-19", "2020-21"]),
    "2020": (2020, "November 2020", "on", 2021, "2020-21", ["2020-21", "2022-23"]),
    "2022": (2022, "October 2022", "on", 2023, "2024-25", ["2024-25"]),
    "2024": (2024, "October 2024", "on", 2024, "2024-25", ["2024-25"]),
    "off": (2018, "October 2018", "off", 2019, "2018-19", ["2018-19", "2018-19"]),
    "save": (2018, "October 2018", "on", 2019, "2018-19", ["2018-19", "2020-21"]),
    "2010": (2010, "September 2010", "on", 2010, None, []),
    "repair": (2018, "October 2018", "on", 2020, None, []),
    "late": (2098, "December 2025", "on", 2099, None, []),
    "late-off": (2098, "December 2025", "off", 2099, None, []),
    "late-2027": (2027, "December 2025", "on", 2029, None, []),
    "fin-2001": (2001, None, "on", 2001, None, []),
    "fin-2025": (2025, "December 2025", "on", 2025, None, []),
    "fin-2020": (2020, "November 2020", "on", 2022, None, []),
}
# The Finalissima runs (open item 74; start, database (None: 3.9.68), rules and last season in RUNS): days to the
# save, the rows that must be there (label year, winner, runner-up; None: any teams) and the label years of every row.
FIN = {"fin-2001": (14, [(1985, "France", "Uruguay"), (1993, "Argentina", "Denmark")], [1985, 1993]),
       "fin-2025": (14, [(1985, "France", "Uruguay"), (1993, "Argentina", "Denmark"), (2022, "Argentina", "Italy")],
                    [1985, 1993, 2022]),
       "fin-2020": (700, [(1985, "France", "Uruguay"), (1993, "Argentina", "Denmark"), (2022, None, None)],
                    [1985, 1993, 2022])}
# The late runs: game days to the save, and the year of the history rows that must follow.
LATE = {"late": (400, 2099), "late-off": (400, 2099), "late-2027": (760, 2029)}
REPAIR_SAVE = os.environ.get("RECOMP_NL_REPAIR_SAVE") or "/Users/max/Projects/cm0102/test-fixtures/nl-asia-record-0.6.1.sav"
REPAIR_DAYS = 400
CHECKER = os.path.join(HERE, "future", "history_check.py")
RECORD_CHECKS = ("comp-continent", "comp-name", "confederation", "entrant-confed")
RECORD_NAMES = re.compile(r"UEFA Nations League|Asian Cup Qual")
# The CONCACAF Nations League editions drawn in each run (part b), in the log's names.
CONCACAF = {"2018": ["CONCACAF-2019-20"], "2020": ["CONCACAF-2022-23"],
            "2022": ["CONCACAF-2023-24", "CONCACAF-2024-25"], "2024": ["CONCACAF-2024-25"],
            "off": ["CONCACAF-2019-20"], "2010": []}
SAVE_AFTER = 430
MARTINIQUE_A = {"2018", "2022"}     # runs whose first CONCACAF draw has Martinique in League A (open item 141)
NON_FIFA = ["Martinique", "Guadeloupe", "French Guiana", "Saint-Martin", "Sint Maarten", "Bonaire"]
NON_FIFA_RUNS = {"2018", "2022", "off"}     # with changing rules on and off (the new game gives the continent)
# Log lines each run must hold: the play-off paths (season_nl_euro.c, season_nl_wc.c).
LINES = {"2018": ["euro 2020 qualifying groups from 21 March 2019", "euro 2020 path A (", "euro 2020 path D (",
                  "euro 2020 seeding on day", "season nl: CONCACAF-2019-20 finals:", "season gold cup 2019: 16 teams:",
                  "season gold cup 2019 hosts: United States (stadium"],
         "2020": ["world cup 2022 qualifying groups from 24 March 2021", "world cup 2022 path A:",
                  "world cup 2022 path C:", "season gold cup 2021: 16 teams:", "season finalissima 2022: ",
                  "season gold cup 2021 hosts: United States (stadium"],
         "2022": ["euro 2024 qualifying groups from 23 March 2023", "euro 2024 path C (",
                  "season gold cup 2023: 16 teams:"],
         "2024": ["season nl: CONCACAF-2024-25 finals:", "season gold cup 2025: 16 teams:"],
         "off": ["season gold cup 2019: 16 teams:"],
         "2010": ["season gold cup 2011: 12 teams:", "season gold cup 2011 hosts: United States (stadium"]}
NO_LINES = ["no path play-off", ": no paths (", "teams of 12", "teams of 16", "no winner in the history", " holds 0 nations", " holds 1 nations"]
BASE_ENV = {"RECOMP_STAFF_RESERVE": "23000", "RECOMP_SEASON_NL_LOG": "1"}
DRAW = re.compile(r"recomp: season nl: (\S+) draw league ([A-D]) group (\d+):(.*)$")
FINALS = re.compile(r"recomp: season nl: (\S+) finals:(.*)$")
MADE = re.compile(r"recomp: season nl: (\S+) \((\d+) groups\), draw day")
MOVE = re.compile(r"recomp: season nl: (.+) goes from league ([A-D]) to ([A-D])$")


def database_folder(name):
    hits = sorted(d for d in os.listdir(DB) if d == name or d.startswith(name + " "))
    if not hits:
        raise SystemExit(f"season_nl_run.py: no database folder {name!r} in {DB}")
    return rrc.database_folder(os.path.join(DB, hits[0]))


def real_leagues(edition, confed="UEFA"):
    """nation name (each "/" name of the table) -> (league, group) of the real draw."""
    out = {}
    for t in make_nl.tiers({(confed, edition)}):
        for name in t["nation"].split("/"):
            out[name.encode("latin-1", "replace").decode("latin-1")] = ("ABCD"[t["league"]], t["group"])
    return out


def draws(log, confed="UEFA"):
    """The draws of a confederation in log order: [(edition, {league: [[nations of group 1], ...]})]. The log
    names a CONCACAF edition "CONCACAF-2019-20"."""
    out = []
    for line in log:
        m = DRAW.search(line)
        if not m or m.group(1).startswith("CONCACAF-") != (confed == "CONCACAF"):
            continue
        ed, league, group, names = m.group(1), m.group(2), int(m.group(3)), m.group(4)
        nations = [n.strip() for n in names.split(";") if n.strip()]
        if not out or (out[-1][0] != ed or (league == "A" and group == 1 and out[-1][1])):
            out.append((ed, {}))
        out[-1][1].setdefault(league, []).append(nations)
    return out


def check(name, work):
    start, _, rules, last, first, editions = RUNS[name]
    fails = []
    report = os.path.join(work, "report.tsv")
    if os.path.exists(report):
        seasons, _ = rrc.read_report(report)
        if not seasons or seasons[-1]["season"] != last or seasons[-1]["phase"] != "end":
            fails.append(f"{name}: the report does not end with the end of {last}/{(last + 1) % 100:02d}")
    else:
        fails.append(f"{name}: no report ({work})")
    path = os.path.join(work, "stderr.txt")
    if not os.path.exists(path):
        return fails + [f"{name}: no log"]
    log = open(path, encoding="latin-1").read().split("\n")
    made = [m.groups() for m in (MADE.search(x) for x in log) if m and not m.group(1).startswith("CONCACAF-")]
    if first and (not made or made[0][0] != first):
        fails.append(f"{name}: the object is not made for {first} ({made[:1]})")
    eds = {(e["confed"], e["edition"]): e for e in make_nl.editions()}
    seen = draws(log)
    if [d[0] for d in seen] != editions:
        fails.append(f"{name}: the draws are {[d[0] for d in seen]}, expected {editions}")
    for k, (ed, leagues) in enumerate(seen):
        e = eds.get(("UEFA", ed))
        for li, league in enumerate("ABCD"):
            groups = leagues.get(league, [])
            count = sum(len(g) for g in groups)
            if e and count > e["sizes"][li] + (1 if k else 0):
                fails.append(f"{name}: {ed} league {league} holds {count}, the edition {e['sizes'][li]}")
            if e and len(groups) != e["groups"][li]:
                fails.append(f"{name}: {ed} league {league} has {len(groups)} groups, the edition {e['groups'][li]}")
        if k == 0 and rules == "on" or (k == 0 and name == "off"):
            real = real_leagues(ed)
            for league, groups in leagues.items():
                for gi, g in enumerate(groups):
                    for nation in g:
                        want = real.get(nation)
                        if want and want[0] != league:
                            fails.append(f"{name}: {ed}: {nation} in league {league}, the real league is {want[0]}")
                        elif want and want[1] and want[1] != gi + 1:
                            fails.append(f"{name}: {ed}: {nation} in group {league}{gi + 1}, the real group is "
                                         f"{want[0]}{want[1]}")
    fails += [f"{name}: {x.strip()}" for x in log if "recomp: message box from" in x]
    for text in LINES.get(name, []):
        if not any(text in x for x in log):
            fails.append(f"{name}: no log line {text!r}")
    for text in NO_LINES:
        for x in log:
            if text in x:
                fails.append(f"{name}: {x.strip()}")
    for x in log:
        if "path" in x and "season nl:" in x and " none;" in x:
            fails.append(f"{name}: a path with a missing nation: {x.strip()}")
    seen_c = draws(log, "CONCACAF")
    if [d[0] for d in seen_c] != CONCACAF.get(name, []):
        fails.append(f"{name}: the CONCACAF draws are {[d[0] for d in seen_c]}, expected {CONCACAF.get(name, [])}")
    for k, (ed, leagues) in enumerate(seen_c):
        e = eds.get(("CONCACAF", ed[len("CONCACAF-"):]))
        for li, league in enumerate("ABC"):
            groups = leagues.get(league, [])
            count = sum(len(g) for g in groups)
            if e and count > e["sizes"][li] + 1:
                fails.append(f"{name}: {ed} league {league} holds {count}, the edition {e['sizes'][li]}")
            if e and len(groups) != e["groups"][li]:
                fails.append(f"{name}: {ed} league {league} has {len(groups)} groups, the edition {e['groups'][li]}")
        if k == 0:
            real = real_leagues(ed[len("CONCACAF-"):], "CONCACAF")
            for league, groups in leagues.items():
                for gi, g in enumerate(groups):
                    for nation in g:
                        want = real.get(nation)
                        if want and want[0] != league:
                            fails.append(f"{name}: {ed}: {nation} in league {league}, the real league is {want[0]}")
    league_a = {ed: {n for g in leagues.get("A", []) for n in g} for ed, leagues in seen + seen_c}
    # Open item 141: the CONCACAF members that are not FIFA members play in CONCACAF (season_intl_concacaf_from),
    # though the databases from October 2016 give them no continent. Martinique's first draw puts it in League A, as
    # in 2019-20 and 2023-24.
    if name in NON_FIFA_RUNS:
        for nation in NON_FIFA:
            if any(f"{nation} has no continent" in x for x in log):
                fails.append(f"{name}: {nation} is left out (no continent)")
    if name in MARTINIQUE_A:
        if seen_c and "Martinique" not in league_a.get(seen_c[0][0], set()):
            fails.append(f"{name}: Martinique is not in League A of {seen_c[0][0]}")
    finals = [m.groups() for m in (FINALS.search(x) for x in log) if m]
    for ed, names in finals:
        nations = [n.strip() for n in names.split(";") if n.strip()]
        if len(nations) != 4 or "none" in nations:
            fails.append(f"{name}: {ed} finals: {nations}")
        elif ed in league_a and not set(nations) <= league_a[ed]:
            fails.append(f"{name}: {ed} finals: {nations} not all of League A")
    if len(seen) >= 2:
        before = {n: lg for lg, gs in seen[0][1].items() for g in gs for n in g}
        after = {n: lg for lg, gs in seen[1][1].items() for g in gs for n in g}
        for m in (MOVE.search(x) for x in log):
            if not m:
                continue
            nation, a, b = m.groups()
            if abs("ABCD".index(a) - "ABCD".index(b)) != 1:
                fails.append(f"{name}: {nation} moves from {a} to {b}")
            if before.get(nation) != a or after.get(nation) != b:
                fails.append(f"{name}: {nation} moves from {a} to {b}, the draws give {before.get(nation)} and "
                             f"{after.get(nation)}")
    return fails


def leagues_of(log, edition):
    """nation -> league of the draw of an edition in a log."""
    for ed, leagues in draws(log):
        if ed == edition:
            return {n: lg for lg, gs in leagues.items() for g in gs for n in g}
    return {}


def record_faults(save, work, label):
    """The faults of the history checker on `save` that name the UEFA Nations League or the Asian Cup Quals and
    come from its record (open items 4 and 21), as lines; None when the checker does not run."""
    out = os.path.join(work, "check")
    os.makedirs(out, exist_ok=True)
    faults = os.path.join(out, label + ".faults.tsv")
    if os.path.exists(faults):                           # a file of an earlier run in --keep proves nothing
        os.remove(faults)
    r = subprocess.run([sys.executable, "-I", CHECKER, save, "--out", out, "--label", label],
                       capture_output=True, text=True, encoding="latin-1")
    with open(os.path.join(out, label + ".txt"), "w", encoding="latin-1") as f:
        f.write(r.stdout + r.stderr)
    if r.returncode not in (0, 1) or not os.path.exists(faults):
        return None
    lines = []
    for x in r.stdout.split("\n"):
        parts = x.split()
        if len(parts) >= 3 and parts[0] in ("known", "FAULT") and parts[1] in RECORD_CHECKS and RECORD_NAMES.search(x):
            lines.append(x.strip())
    return lines


def load_and_jump(program, game, database, save, work, last, timeout, extra_env=None):
    """Loads a save in a new user folder and plays the season jump to the end of `last`, as rules_report_check.run
    does for a new game. extra_env: more switches (RECOMP_SAVE_AFTER_DAYS: the game saves and ends then)."""
    user, home, shots = (os.path.join(work, d) for d in ("user", "home", "shots"))
    for d in (user, home, shots):
        os.makedirs(d, exist_ok=True)
    cdr.copy_data(database, os.path.join(user, "Databases", os.path.basename(database)))
    shutil.copyfile(save, os.path.join(user, "nl.sav"))
    with open(os.path.join(user, "settings.txt"), "w") as f:
        f.write(f"display = classic\nchanging_rules = on\nfaster_days = on\ndatabase = folder:{os.path.basename(database)}\n")
    # The save was made on holiday (RECOMP_SAVE_AFTER_DAYS in the jump), so the loaded game goes on by itself and
    # shows no first news to wait for; the jump ends the program at the end of `last`.
    load = smoke_run.without_intro_click(fixtures.LOAD_STEPS)
    steps = [x for x in load if x != "wait first-news"] + ["5400000 quit"]
    table = os.path.join(os.path.dirname(os.path.abspath(program)), "seasons", "rules.tsv")
    env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
               RECOMP_USER_DIR=user, RECOMP_FIXED_CLOCK="1", RECOMP_SEASON_JUMP=str(last), RECOMP_SCREEN_LOG="1",
               RECOMP_RULES_TABLE=table, RECOMP_SCRIPT="; ".join(smoke_run.scaled_steps("classic", shots, steps)),
               **dict(BASE_ENV, **(extra_env or {})))
    start = time.time()
    with open(os.path.join(work, "stderr.txt"), "w") as log:
        try:
            code = subprocess.run([program, "--game", game, "-seed", "1"], env=env, stdout=subprocess.DEVNULL,
                                  stderr=log, timeout=timeout).returncode
        except subprocess.TimeoutExpired:
            code = "timeout"
    rrc.no_fallback(os.path.join(work, "stderr.txt"), database)
    print(f"run: load and jump: code {code} after {time.time() - start:.0f} s, log {os.path.join(work, 'stderr.txt')}")
    return code


def save_run(program, game, keep, reuse):
    start, database, _, last, _, _ = RUNS["save"]
    base = keep or tempfile.mkdtemp(prefix="cm0102-season-nl-save-")
    first, second = os.path.join(base, "save-a"), os.path.join(base, "save-b")
    db = database_folder(database)
    if not reuse:
        save = os.path.join(first, "user", smoke_run.SAVE_NAME)
        if not os.path.exists(save):                     # a save of an earlier run in --keep is used again
            code, _ = rrc.run(program, game, last, start, db, ["England"], [], first, 7200, {"changing_rules": "on"},
                              dict(BASE_ENV, RECOMP_SAVE_AFTER_DAYS=str(SAVE_AFTER)))
            if code != 0 or not os.path.exists(save):
                return [f"save: the first game did not save (code {code}, {first})"]
        if os.path.exists(second):
            second = tempfile.mkdtemp(prefix="save-b-", dir=base)
        code = load_and_jump(program, game, db, save, second, last, 7200)
        if code != 0:
            return [f"save: the loaded game ended with code {code} ({second})"]
    fails = []
    log_b = open(os.path.join(second, "stderr.txt"), encoding="latin-1").read().split("\n")
    fails += [f"save: the loaded game: {x.strip()}" for x in log_b if "recomp: message box from" in x]
    if not any("season jump: done" in x for x in log_b):
        fails.append(f"save: the loaded game did not reach the end of {last}/{(last + 1) % 100:02d}")
    ref = os.path.join(base, "2018", "stderr.txt")
    if not os.path.exists(ref):
        return [f"save: no log of run 2018 in {base} to compare with"]
    log_ref = open(ref, encoding="latin-1").read().split("\n")
    want, got = leagues_of(log_ref, "2020-21"), leagues_of(log_b, "2020-21")
    if not want or not got:
        return [f"save: no 2020-21 draw (run 2018: {len(want)} nations, loaded game: {len(got)})"]
    for n in sorted(set(want) | set(got)):
        if want.get(n) != got.get(n):
            fails.append(f"save: {n} in league {got.get(n)} after the load, {want.get(n)} in run 2018")
    if not any("season nl: 2018-19 finals:" in x for x in open(os.path.join(first, "stderr.txt"), encoding="latin-1")):
        fails.append("save: the first game made no finals before the save")
    found = record_faults(os.path.join(first, "user", smoke_run.SAVE_NAME), first, "save-a")
    if found is None:
        fails.append(f"save: the history checker did not read the save ({first}/check)")
    fails += [f"save: the history checker: {x}" for x in found or []]
    return fails


def repair_run(program, game, keep, reuse):
    """Run "repair" (see the top): a 0.6.1 save with the record of the Asian Cup qualifiers is repaired at load."""
    _, database, _, last, _, _ = RUNS["repair"]
    if not os.path.exists(REPAIR_SAVE):
        return [f"repair: no 0.6.1 save {REPAIR_SAVE} (RECOMP_NL_REPAIR_SAVE)"]
    work = os.path.join(keep, "repair") if keep else tempfile.mkdtemp(prefix="cm0102-season-nl-repair-")
    os.makedirs(work, exist_ok=True)
    save = os.path.join(work, "user", smoke_run.SAVE_NAME)
    if not reuse:
        if os.path.exists(save):
            os.remove(save)
        code = load_and_jump(program, game, database_folder(database), REPAIR_SAVE, work, last, 7200,
                             {"RECOMP_SAVE_AFTER_DAYS": str(REPAIR_DAYS)})
        if code != 0:
            return [f"repair: the loaded game ended with code {code} ({work})"]
    fails = []
    before = record_faults(REPAIR_SAVE, work, "before")
    if not before or not any(x.split()[1] == "comp-continent" for x in before):
        fails.append(f"repair: the 0.6.1 save holds no Nations League record on continent Asia ({work}/check)")
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read().split("\n")
    fails += [f"repair: {x.strip()}" for x in log if "recomp: message box from" in x]
    if not any("season nl: record" in x and "(repaired at load)" in x for x in log):
        fails.append("repair: the log names no repair of the record at load")
    if not any(DRAW.search(x) and DRAW.search(x).group(1) == "2020-21" for x in log):
        fails.append("repair: no 2020-21 draw after the load")
    if not os.path.exists(save):
        return fails + [f"repair: the loaded game did not save ({work})"]
    after = record_faults(save, work, "after")
    if after is None:
        fails.append(f"repair: the history checker did not read the new save ({work}/check)")
    fails += [f"repair: the history checker: {x}" for x in after or []]
    return fails


def late_run(program, game, name, keep, reuse):
    """Runs "late" and "late-off" (see the top): a game that starts after the table has both Nations Leagues."""
    start, database, rules, last, _, _ = RUNS[name]
    days, row_year = LATE[name]
    work = os.path.join(keep, name) if keep else tempfile.mkdtemp(prefix=f"cm0102-season-nl-{name}-")
    save = os.path.join(work, "user", smoke_run.SAVE_NAME)
    if not reuse:
        if os.path.isdir(work):                          # rrc.run makes the folders of a new game
            shutil.rmtree(work)
        code, _ = rrc.run(program, game, last, start, database_folder(database), ["England"], [], work, 7200,
                          {"changing_rules": rules},
                          dict(BASE_ENV, RECOMP_DATE_JUMP="1", RECOMP_SAVE_AFTER_DAYS=str(days)))
        if code != 0:
            return [f"{name}: the run ended with code {code} ({work})"]
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read().split("\n")
    fails = [f"{name}: {x.strip()}" for x in log if "recomp: message box from" in x]
    for d in ("new_uefa_nations_league", "new_concacaf_nations_league"):
        if not any(f"new competition {d} made for" in x for x in log):
            fails.append(f"{name}: no object {d}")
    finals = [m.group(2) for m in map(FINALS.search, log) if m and not m.group(1).startswith("CONCACAF")]
    if not any(len([t for t in f.split(";") if t.strip()]) == 4 for f in finals):
        fails.append(f"{name}: no UEFA finals of four nations")
    if not os.path.exists(save):
        return fails + [f"{name}: the game did not save ({work})"]
    found = record_faults(save, work, name)
    if found is None:
        return fails + [f"{name}: the history checker did not read the save ({work}/check)"]
    fails += [f"{name}: the history checker: {x}" for x in found]
    rows = open(os.path.join(work, "check", name + ".history.tsv"), encoding="latin-1").read().split("\n")
    for comp in ("UEFA Nations League", "CONCACAF Nations League"):
        if not any(f"\t{comp}\t{row_year}\t" in x for x in rows):
            fails.append(f"{name}: no {comp} history row of {row_year} ({work}/check)")
    return fails


def fin_run(program, game, name, keep, reuse):
    """Runs fin-* (see the top): the Finalissima history rows of the editions before the start."""
    start, database, rules, last, _, _ = RUNS[name]
    days, want, years = FIN[name]
    work = os.path.join(keep, name) if keep else tempfile.mkdtemp(prefix=f"cm0102-season-nl-{name}-")
    save = os.path.join(work, "user", smoke_run.SAVE_NAME)
    if not reuse:
        if os.path.isdir(work):                          # rrc.run makes the folders of a new game
            shutil.rmtree(work)
        code, _ = rrc.run(program, game, last, start, database_folder(database) if database else None, ["England"],
                          [], work, 7200, {"changing_rules": rules}, dict(BASE_ENV, RECOMP_SAVE_AFTER_DAYS=str(days)))
        if code != 0:
            return [f"{name}: the run ended with code {code} ({work})"]
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read().split("\n")
    fails = [f"{name}: {x.strip()}" for x in log if "recomp: message box from" in x]
    if not os.path.exists(save):
        return fails + [f"{name}: the game did not save ({work})"]
    found = record_faults(save, work, name)
    if found is None:
        return fails + [f"{name}: the history checker did not read the save ({work}/check)"]
    fails += [f"{name}: the history checker: {x}" for x in found]
    rows = [x.split("\t") for x in open(os.path.join(work, "check", name + ".history.tsv"), encoding="latin-1")
            .read().split("\n")[1:] if x]
    # the module renames the record "2022 Finalissima" (the rules table's name) when it makes the object
    fin = [(int(r[3]), r[4].split(" (")[0], r[5].split(" (")[0]) for r in rows if re.fullmatch(r"(\d{4} )?Finalissima", r[2])]
    if sorted(y for y, _, _ in fin) != years:
        fails.append(f"{name}: Finalissima rows {sorted(fin)}, the label years must be {years}")
    for y, w, r in want:
        if not any(fy == y and (w is None or (fw, fr) == (w, r)) for fy, fw, fr in fin):
            fails.append(f"{name}: no Finalissima row {y} {w or 'any'} / {r or 'any'} ({sorted(fin)})")
    return fails


def one(program, game, name, keep, reuse):
    if name == "save":
        return save_run(program, game, keep, reuse)
    if name == "repair":
        return repair_run(program, game, keep, reuse)
    if name in FIN:
        return fin_run(program, game, name, keep, reuse)
    if name.startswith("late"):
        return late_run(program, game, name, keep, reuse)
    start, database, rules, last, _, _ = RUNS[name]
    work = os.path.join(keep, name) if keep else tempfile.mkdtemp(prefix=f"cm0102-season-nl-{name}-")
    if not reuse:
        code, _ = rrc.run(program, game, last, start, database_folder(database), ["England"], [], work, 7200,
                          {"changing_rules": rules}, BASE_ENV)
        if code != 0:
            return [f"{name}: the run ended with code {code} ({work})"]
    return check(name, work)


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    only = argv[argv.index("--only") + 1].split(",") if "--only" in argv else list(RUNS)
    jobs = int(argv[argv.index("--jobs") + 1]) if "--jobs" in argv else 1
    keep = os.path.abspath(argv[argv.index("--keep") + 1]) if "--keep" in argv else None
    reuse = "--reuse" in argv
    fails = []
    with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
        for r in pool.map(lambda n: one(program, game, n, keep, reuse), only):
            fails += r
    for f in fails:
        print("FAIL  " + f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
