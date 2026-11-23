#!/usr/bin/env python3
"""Competition history between 2001 and the start season (0.6.0 slice 11c, Q347).

Usage:
    python3 tests/seasons/season_history_run.py <cm0102_recomp> <game folder> <folder of databases>
        [--only <season or database>,...] [--jobs n] [--extras] [--no-extra-runs] [--duplicates]

For each season database before 2010/11 (and the 3.9.68 data on the 2002/03 tile) a new game starts in its
season and plays to the first news screen. The test log RECOMP_HISTORY_LOG (season_history.c) gives, at the
database load and at the first human turn, every history row from 1999 on of the competitions below. At the
first turn the game holds only the rows of the selected nations (England in these runs). The checks:
    plan       for each competition, every label year from 2002 to the start year that the plan of that
               database fills (docs/research/history-0.6.0/plan.tsv) holds exactly the researched first and
               second clubs, and no other row.
    england    the English Premier Division holds the real champion of every season from 2001/02 to the season
               before the start (an independent list, as in season_dates_run.py).
    duplicates no competition holds the same row (label year, first and second club) twice, at the load and at the
               first turn. Many season databases hold such rows (tests/seasons/history_duplicates_check.py lists
               them; September 2010 ODB holds the English Premier Division 2001 Chelsea / Manchester United twice,
               2010 after the shift). --duplicates adds a run for each season database from 2010/11 on, with this
               check alone (the plan covers only the databases before 2010/11)
Also:
    off        September 2005 in 2005/06 with RECOMP_SEASON_CODE=off: no row is written (the 2002 to 2004 labels
               of the English Premier Division stay empty).
    again      the September 2009 game twice: the saves are byte for byte the same.
    reshift    September 2005 with RECOMP_DATES_RESHIFT=2006 (season_dates.c: the shift is made for 2006 and then
               for 2005 again, as at a creation in another year than the load): the save equals the September
               2005 save, so the undo of the fill and of the shift leave nothing behind.
    load       the September 2005 save loads (tests/fixtures/fixtures.py play), holds the same rows at its first
               turn, runs no fill, and plays 3 days to a save.
At most --jobs (3) games run at the same time.
"""
import collections
import concurrent.futures
import csv
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import season_dates_run as sdr  # noqa: E402

PLAN = os.path.join(ROOT, "docs", "research", "history-0.6.0", "plan.tsv")
COMPS = ["English Premier Division", "English FA Cup", "European Champions Cup", "Scottish Premier Division",
         "Norwegian Premier Division", "Italian Serie A", "FIFA World Cup"]
EPL = "English Premier Division"
FIRST = 2001                    # 2000/01 (Q354) to the start year
# Real winners from public records, independent of the research table: a part of each name (the databases
# spell clubs in different ways). Label years as the 3.9.68 data labels each competition. None: no champion in
# the official record (Serie A 2004/05: the FIGC revoked Juventus's title in July 2006 and gave it to no club; 2005/06
# went to Internazionale on 26 July 2006, item 108).
FACTS = {
    EPL: {2001: "Manchester United", 2002: "Arsenal", 2003: "Manchester United", 2004: "Arsenal", 2005: "Chelsea",
          2006: "Chelsea", 2007: "Manchester United", 2008: "Manchester United", 2009: "Manchester United"},
    "English FA Cup": {2001: "Liverpool", 2002: "Arsenal", 2003: "Arsenal", 2004: "Manchester United",
                       2005: "Arsenal", 2006: "Liverpool", 2007: "Chelsea", 2008: "Portsmouth", 2009: "Chelsea"},
    "European Champions Cup": {2001: "Bayern", 2002: "Real Madrid", 2003: "Milan", 2004: "Porto",
                               2005: "Liverpool", 2006: "Barcelona", 2007: "Milan", 2008: "Manchester United",
                               2009: "Barcelona"},
    "Italian Serie A": {2001: "Roma", 2002: "Juventus", 2003: "Juventus", 2004: "Milan", 2005: None,
                        2006: "Internazionale", 2007: "Internazionale", 2008: "Internazionale", 2009: "Internazionale"},
    "Scottish Premier Division": {2001: "Celtic", 2002: "Celtic", 2003: "Rangers", 2004: "Celtic",
                                  2005: "Rangers", 2006: "Celtic", 2007: "Celtic", 2008: "Celtic", 2009: "Rangers"},
    "Norwegian Premier Division": {2001: "Rosenborg", 2002: "Rosenborg", 2003: "Rosenborg", 2004: "Rosenborg",
                                   2005: "lerenga", 2006: "Rosenborg", 2007: "Brann", 2008: "Stab", 2009: "Rosenborg"},
    "FIFA World Cup": {2002: "Brazil", 2006: "Italy"},
}


def plan_rows():
    """{database: {competition: {label: set of (first, second)}}} and {database: start year} of the plan."""
    want = collections.defaultdict(lambda: collections.defaultdict(lambda: collections.defaultdict(set)))
    starts = {}
    with open(PLAN, newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            starts[r["database"]] = int(r["start_year"])
            c = want[r["database"]][r["competition"]]
            if r["action"] == "clear":
                c.setdefault(int(r["label"]), set())
            else:
                c[int(r["label"])].add((r["first"], r["second"]))
    return want, starts


def parse(log, when):
    """{competition: {label: list of (first, second)}} from the history log of `when` (load or turn)."""
    out = {}
    for m in re.finditer(r"recomp: history log " + when + r": (.+?):((?: \d+ [^;]*;)*)\n", log):
        rows = collections.defaultdict(list)
        for x in re.finditer(r" (\d+) (.*?) / (.*?);", m.group(2)):
            rows[int(x.group(1))].append((x.group(2), x.group(3)))
        out[m.group(1)] = rows
    return out


def check(label, start, got, want, turn=False):
    """At the load every competition of the plan; at the first turn those that the game kept (it keeps only the
    rows of the selected nations, 0x539ae0)."""
    bad, n = [], 0
    for comp in COMPS:
        if comp not in want or (turn and not got.get(comp)):
            continue
        rows = got.get(comp, {})
        for y in range(FIRST, start + 1):
            if y not in want[comp]:
                continue
            w = want[comp][y]
            have = set(rows.get(y, []))
            n += 1
            if have != w or len(rows.get(y, [])) != len(w):
                bad.append(f"{comp} {y}: holds {sorted(have) or 'nothing'}, the plan {sorted(w) or 'nothing'}")
    for comp, facts in FACTS.items():
        rows = got.get(comp, {})
        if comp not in want or (turn and not rows):
            continue
        for y, part in facts.items():
            if y > start or y not in want[comp]:
                continue
            first = [a for a, b in rows.get(y, [])]
            if part is None:
                if first:
                    bad.append(f"{comp} {y}: winner {first}, real none")
            elif len(first) != 1 or part.lower() not in first[0].lower():
                bad.append(f"{comp} {y}: winner {first or 'none'}, real {part}")
    return n, bad


def duplicates(got):
    """The rows that a competition of the log holds more than once: [(competition, label, first, second)]."""
    out = []
    for comp, rows in got.items():
        for y, pairs in rows.items():
            for pair in sorted(set(pairs)):
                if pairs.count(pair) > 1:
                    out.append((comp, y) + pair)
    return out


def run_one(program, game, work, label, year, folder, env=None):
    env = dict(env or {}, RECOMP_HISTORY_LOG=";".join(COMPS))
    save, log, secs = sdr.play(program, game, work, label, year, folder, env)
    return save, log, secs


LOAD_STEPS = ["2500 click 45 165", "4500 click 150 427", "7000 click 705 572", "400000 quit"]   # Go on Holiday


def load_check(program, game, work, save, want_turn):
    """The September 2005 save loads, holds the rows of the first turn, and plays 3 days to a save."""
    folder = os.path.join(work, "load")
    os.makedirs(folder)
    shutil.copy(save, os.path.join(folder, "h.sav"))
    steps = os.path.join(folder, "steps.txt")
    with open(steps, "w") as f:
        f.write("\n".join(LOAD_STEPS) + "\n")
    out = os.path.join(folder, "out")
    # fixtures.py reads the log as UTF-8: only competitions whose clubs have plain letters (England here)
    plain = [c for c in COMPS if c.startswith("English")]
    want_turn = {c: v for c, v in want_turn.items() if c in plain}
    env = dict(os.environ, RECOMP_HISTORY_LOG=";".join(plain))
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tests", "fixtures", "fixtures.py"), "play", program, game,
                        "h.sav", steps, out, "--dir", folder, "--days", "3"], env=env, capture_output=True, timeout=1800)
    log = open(os.path.join(out, "stderr.txt"), encoding="latin-1").read() if os.path.exists(
        os.path.join(out, "stderr.txt")) else ""
    turn = parse(log, "turn")
    saved = os.path.exists(os.path.join(out, "samesave.sav"))
    same = all(turn.get(c) == want_turn.get(c) for c in want_turn)
    ok = saved and same and bool(turn) and "season history:" not in log
    print(f"{'ok  ' if ok else 'FAIL'}  load: the September 2005 save loads ({'the rows of the first turn are there' if same and turn else 'rows differ'}), "
          f"{'plays 3 days to a save' if saved else 'no save after 3 days'}, no fill at the load")
    return 0 if ok else 1


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2
    program, game, dbs_dir = (os.path.abspath(a) for a in argv[1:4])
    jobs_n = int(argv[argv.index("--jobs") + 1]) if "--jobs" in argv else 3
    only = argv[argv.index("--only") + 1].split(",") if "--only" in argv else None
    want, starts = plan_rows()
    dbs = sdr.databases(dbs_dir)
    runs = []
    for r in sdr.season_rows():
        if r["role"] not in ("season database", "extra version") or r["database"] not in starts:
            continue
        if r["role"] == "extra version" and "--extras" not in argv:
            continue
        year = starts[r["database"]]
        if only and str(year) not in only and r["database"] not in only:
            continue
        if r["database"] not in dbs:
            print(f"skip  {r['database']}: not in {dbs_dir}")
            continue
        runs.append((r["database"], year, r["database"], None))
    dup_only = set()
    if "--duplicates" in argv:                     # every season database from 2010/11 on: the duplicates alone
        for r in sdr.season_rows():
            if r["role"] != "season database" or r["database"] in starts or r["database"] not in dbs:
                continue
            year = int(r["data_season"][:4])
            if only and str(year) not in only and r["database"] not in only:
                continue
            runs.append((r["database"], year, r["database"], None))
            dup_only.add(r["database"])
    if not only or "2002" in only or "3.9.68" in only:
        runs.append(("3.9.68", 2002, None, None))
    if "--no-extra-runs" not in argv:
        if "September 2005" in dbs:
            runs.append(("off", 2005, "September 2005", {"RECOMP_SEASON_CODE": "off"}))
            runs.append(("reshift", 2005, "September 2005", {"RECOMP_DATES_RESHIFT": "2006"}))
        if "September 2009" in dbs:
            runs.append(("again", 2009, "September 2009", None))
    work = tempfile.mkdtemp(prefix="season-history-")
    failed = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs_n) as ex:
        futs = {}
        for label, year, name, env in runs:
            folder = sdr.neutral_copy(work, name, dbs[name][1]) if name else None
            futs[ex.submit(run_one, program, game, work, label, year, folder, env)] = (label, year, name)
        results, logs = {}, {}
        for f in concurrent.futures.as_completed(futs):
            label, year, name = futs[f]
            save, log, secs = f.result()
            results[label] = save
            got, turn = parse(log, "load"), parse(log, "turn")
            logs[label] = turn
            if label == "reshift":
                m = re.search(r"season dates: (\d+) history years restored in the moved history table", log)
                ok = bool(m)
                print(f"{'ok  ' if ok else 'FAIL'}  reshift: {m.group(1) + ' history years restored at the new place of the table' if m else 'the table did not move, or the restore missed it'}")
                failed += not ok
            if label == "off":
                got = turn
                eng = got.get(EPL, {})
                filled = [y for y in (2002, 2003, 2004) if eng.get(y)]
                ok = "season history:" not in log and not filled and EPL in got
                print(f"{'ok  ' if ok else 'FAIL'}  off: {'no rows written' if ok else 'rows written ' + str(filled)}"
                      f" ({secs:.0f} s)")
                failed += not ok
                continue
            dups = duplicates(got) + duplicates(turn)
            if label in dup_only:
                ok = bool(turn) and not dups          # no load log: these databases have no fill
                print(f"{'ok  ' if ok else 'FAIL'}  {label} ({year}/{(year + 1) % 100:02d}): "
                      f"{'no duplicate rows' if turn else 'no history log'}; "
                      f"{re.search(r'recomp: season history: .*duplicate.*', log).group(0)[24:] if 'duplicate rows' in log else 'no duplicates cleared'}"
                      f" ({secs:.0f} s)")
                for d in sorted(set(dups)):
                    print(f"      duplicate: {d}")
                failed += not ok
                continue
            db = "September 2009" if label == "again" else (name or "3.9.68")
            if not got or not turn:
                print(f"FAIL  {label}: no history log ({secs:.0f} s)\n{log[-1500:]}")
                failed += 1
                continue
            n, bad = check(label, year, got, want[db])
            n2, bad2 = check(label, year, turn, want[db], turn=True)
            bad += ["at the first turn: " + b for b in bad2]
            bad += [f"duplicate row: {d}" for d in sorted(set(dups))]
            line = re.search(r"recomp: season history: .*", log)
            print(f"{'ok  ' if not bad else 'FAIL'}  {label} ({year}/{(year + 1) % 100:02d}): {n} label years "
                  f"checked at the load, {n2} at the first turn; {line.group(0)[24:] if line else 'no fill line'} ({secs:.0f} s)")
            for b in bad:
                print("      " + b)
            failed += bool(bad)
        if "again" in results:
            first = results.get("September 2009")
            if first and results["again"]:
                same = open(first, "rb").read() == open(results["again"], "rb").read()
                print(f"{'ok  ' if same else 'FAIL'}  again: the two September 2009 saves are "
                      f"{'the same' if same else 'different'}")
                failed += not same
            else:
                print("FAIL  again: a save is missing")
                failed += 1
        for other, base in (("reshift", "September 2005"),):
            if other not in results:
                continue
            a, b = results.get(base), results[other]
            same = bool(a and b) and open(a, "rb").read() == open(b, "rb").read()
            print(f"{'ok  ' if same else 'FAIL'}  {other}: the save after a shift for 2006 and back to 2005 is "
                  f"{'the same as' if same else 'not the same as'} the {base} save")
            failed += not same
    if "--no-extra-runs" not in argv and results.get("September 2005"):
        failed += load_check(program, game, work, results["September 2005"], logs.get("September 2005", {}))
    print(f"{len(runs)} runs, {failed} failed; work folder {work}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
