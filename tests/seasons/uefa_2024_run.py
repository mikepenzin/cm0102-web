#!/usr/bin/env python3
"""UEFA league phase check (0.6.0 slice 25, 2024/25 on): one table of 36 clubs with 8 matches each (6 in the
Conference League), the knockout phase play-offs of places 9 to 24, and the European Performance Spots.

Usage:
    python3 tests/seasons/uefa_2024_run.py <cm0102_recomp> <game folder> [--only <name>,...] [--jobs n]
        [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py with England. The runs:
    2024    October 2024, changing rules off, the season 2024/25
    2025    December 2025, changing rules off, the season 2025/26
    change  October 2023, changing rules on, 2023/24 to the end of 2024/25: the news sentences of the change
    load    October 2024: a save in the qualifying rounds (20 days, late July, before the league phase draw, which
            reads the direct clubs of the list +0x14), one in the league phase (120 days) and one in the knockout
            phase (215 days: 13 February, after the play-off draw of 31 January), each loaded by a new program that
            plays to the final
    repeat  two October 2024 games with the same steps give equal saves 130 days after the start (in the league
            phase)
The checks of a season: the jump ends with code 0 and shows no message box of the game; the log of
src/recomp_rt/win/season_uefa*.c shows the access list places, two European Performance Spots, a league phase
of 36 clubs with 8 matches each for the Champions League and the Europa League (6 for the Conference League
when the game has it), every club of each league phase table with all its matches played when the knockout is
drawn, the play-off ties of places 9 to 24 in the pairs of the bracket, and both knockout phases drawn; the uefa
line of the report at the end of the season: 2 stages, 36 clubs in the league phase, a knockout stage of 24 clubs
(16 in the play-offs and places 1 to 8) in 5 rounds, each with a winner; the label of the play-off round is the
real name of 2024/25. A run takes about 15 to 45 minutes on a loaded machine. --jobs (default 1).
"""
import concurrent.futures
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rules_report_check as rrc  # noqa: E402
import uefa_formats_run as ufr  # noqa: E402

DB = "/Users/max/Projects/cm0102/test-fixtures/databases"
ECL_FILE = "new_conference_league"
LEAGUE = {"stages": 2, "groups": 36, "groups2": 0, "ko_clubs": 24, "ko_rounds": 5}
KO_PO_NAME = "Knockout Phase Play-offs"
NEWS = ["The Champions League group stage is now a league phase: one table of 36 clubs who play 8 matches each."]
# The pairs of places that meet in the play-offs (regulations of 2024/25, Art. 19.02): 9/10 v 23/24, 11/12 v
# 21/22, 13/14 v 19/20, 15/16 v 17/18. Each tie is "unseeded-seeded".
PAIRS = {5: 12, 6: 11, 7: 10, 8: 9}


def ties_ok(text):
    ties = [tuple(int(x) for x in t.split("-")) for t in text.split()]
    places = sorted(p for t in ties for p in t)
    return len(ties) == 8 and places == list(range(9, 25)) and \
        all(PAIRS.get((s + 1) // 2) == (u + 1) // 2 and s < u for u, s in [(t[0], t[1]) for t in ties])


def season_checks(name, log, report, season, ecl):
    key = f"{season}/{(season + 1) % 100:02d}"
    fails = []
    for pattern in (rf"access list {key}: \d+ Champions League and \d+ Europa League places",
                    rf"Champions League {key}: \d+ entrants",
                    rf"league phase {key}: 36 clubs, 8 matches each",
                    r"Champions League knockout phase play-offs drawn",
                    r"Europa League knockout phase play-offs drawn",
                    rf"Europa League {key}: league phase drawn"):
        if not re.search(pattern, log):
            fails.append(f"{name}: no log line {pattern!r}")
    if len(re.findall(rf"European Performance Spot {key}: rank \d+", log)) != 2:
        fails.append(f"{name}: not two European Performance Spots in {key}")
    want = 3 if ecl else 2
    if len(re.findall(rf"league phase {key}: 36 clubs, \d matches each", log)) < want:
        fails.append(f"{name}: fewer than {want} league phases in {key}")
    if ecl and not re.search(rf"league phase {key}: 36 clubs, 6 matches each", log):
        fails.append(f"{name}: no Conference League league phase of 6 matches in {key}")
    tables = re.findall(r"league phase table: (\d+) clubs, played (\d+) to (\d+); play-off ties ([0-9 -]+)", log)
    if len(tables) < want:
        fails.append(f"{name}: {len(tables)} league phase tables at the knockout draws, want {want}")
    for clubs, lo, hi, ties in tables:
        if clubs != "36" or lo != hi or lo not in ("8", "6"):
            fails.append(f"{name}: a league phase table of {clubs} clubs with {lo} to {hi} matches played")
        if not ties_ok(ties):
            fails.append(f"{name}: play-off ties {ties!r} are not the pairs of places 9 to 24 of the bracket")
    # "league phase: n clubs, not 36" is a day that the draw waits for its clubs: the drawn lines above show it came.
    for m in re.finditer(r"league phase order: (\d+) level pairs, (\d+) not by goal difference", log):
        if m.group(2) != "0":
            fails.append(f"{name}: a league phase table with {m.group(2)} level pairs out of the order of Art. 18")
    for bad in ("no draw found", "no format for", "not even", "gives no layout"):
        if bad in log:
            fails.append(f"{name}: log line with {bad!r}")
    if os.path.exists(report):
        lines = ufr.uefa_lines(report)
        fails += ufr.expect(lines, season, "european_cup", LEAGUE, name)
        fails += ufr.expect(lines, season, "uefa_cup", LEAGUE, name)
        labels = ufr.round_labels(report)
        fails += ufr.expect_label(labels, season, "european_cup", 10, KO_PO_NAME, name)
        fails += ufr.expect_label(labels, season, "uefa_cup", 30, KO_PO_NAME, name)
        for file in ("european_cup", "uefa_cup"):
            fails += ufr.expect_label(labels, season, file, 0x41f, "League Phase", name)
        if ecl:
            fails += ufr.expect(lines, season, ECL_FILE, LEAGUE, name)
    else:
        fails.append(f"{name}: no report")
    return fails


def pot_faults(name, log, season, changing):
    """The pots of each league phase (decision L8 of the 0.7.3 spec). Changing rules on: both nation rules hold (draw
    rules 0), and no more clubs of a lower pot have a higher reputation than a club of the pot above than the log says
    changed pots for the nation rules in that league phase. Changing rules off: the entry codes of the original's seed
    order 0x586f60 never fall."""
    key = f"{season}/{(season + 1) % 100:02d}"
    fails, phases, moved = [], [], 0
    line = re.compile(rf"league phase {key}: (?:(\d+) clubs change pots|\d matches, pot (\d):((?: \d+:-?\d+:-?\d+)+)"
                      r"|36 clubs, \d matches each, draw rules (\d))")
    for m in line.finditer(log):
        if m.group(1):
            moved = int(m.group(1))
        elif m.group(2):
            if m.group(2) == "1":
                phases.append({"pots": [], "moved": moved})
                moved = 0
            phases[-1]["pots"].append([tuple(int(x) for x in c.split(":")) for c in m.group(3).split()])
        elif changing and m.group(4) != "0":
            fails.append(f"{name}: a league phase of {key} has draw rules {m.group(4)}")
    if len(phases) < 2:
        fails.append(f"{name}: {len(phases)} league phases with pot lines in {key}")
    for phase in phases:
        pots = phase["pots"]
        size = len(pots[0]) if pots else 0
        if len(pots) * size != 36 or any(len(pot) != size for pot in pots):
            fails.append(f"{name}: a league phase of {key} logs pots of {[len(pot) for pot in pots]} clubs")
            continue
        clubs = [c for pot in pots for c in pot]
        if not changing and [c[0] for c in clubs] != sorted(c[0] for c in clubs):
            fails.append(f"{name}: changing rules off, the pots of {key} left the seed order")
        if changing:
            high = sum(c[1] > min(u[1] for u in upper) for i, upper in enumerate(pots) for lower in pots[i + 1:]
                       for c in lower)
            if high > phase["moved"] * len(pots):
                fails.append(f"{name}: {high} clubs of lower pots of {key} hold a higher reputation, "
                             f"{phase['moved']} changed pots")
    return fails


def has_ecl(log, season):
    return re.search(rf"new competition {ECL_FILE} made for {season}", log) is not None


def run_season(name, season, database):
    def run(program, game, work):
        code, report = rrc.run(program, game, season, season, os.path.join(DB, database), ["England"], [], work, 7200)
        fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
        found, log = ufr.boxes(work)
        fails += [f"{name}: message box {b}" for b in found]
        fails += season_checks(name, log, report, season, has_ecl(log, season))
        fails += pot_faults(name, log, season, False)
        m = re.search(r"suspended Russia: (\d+) clubs with a place", log)
        if not m or m.group(1) != "0":
            fails.append(f"{name}: Russia: {m.group(0) if m else 'no log line of the suspension'}")
        return fails
    return run


def run_change(program, game, work):
    code, report = rrc.run(program, game, 2024, 2023, os.path.join(DB, "October 2023"), ["England"], [], work, 10800,
                           {"changing_rules": "on"}, {"RECOMP_SEASON_CHANGE_LOG": "1"})
    fails = [] if code == 0 else [f"change: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    fails += [f"change: message box {b}" for b in found]
    if os.path.exists(report):
        lines = ufr.uefa_lines(report)
        fails += ufr.expect(lines, 2023, "european_cup", {"stages": 9, "groups": 32}, "change")
    if re.search(r"league phase 2023/24", log):
        fails.append("change: a league phase in 2023/24")
    fails += season_checks("change", log, report, 2024, has_ecl(log, 2024))
    fails += pot_faults("change", log, 2024, True)
    for sentence in NEWS:
        if sentence not in log:
            fails.append(f"change: no news sentence {sentence!r}")
    return fails


def run_load(program, game, work):
    """Saves in the league phase and in the knockout phase, each loaded by a new program that plays to the end
    of the season (as tests/seasons/uefa_2021_run.py run_load)."""
    import shutil
    import fixtures
    import smoke_run
    db = os.path.join(DB, "October 2024")
    fails = []
    for days in (20, 120, 215):            # 20: late July, before the league phase draw (slice 23 review 2)
        w = os.path.join(work, f"d{days}")
        code, _ = rrc.run(program, game, 2024, 2024, db, ["England"], [], w, 7200, None,
                          {"RECOMP_SAVE_AFTER_DAYS": str(days)})
        first = os.path.join(w, "user", smoke_run.SAVE_NAME)
        if code != 0 or not os.path.exists(first):
            fails.append(f"load {days}: the first run made no save (code {code}, {w})")
            continue
        # The save of 215 days is inside the knockout phase: the first run drew both knockouts before it saved,
        # and the save of 120 days is inside the league phase: no knockout yet.
        drawn = "Champions League knockout phase play-offs drawn" in ufr.boxes(w)[1]
        if drawn != (days > 200):
            fails.append(f"load {days}: the save is not in the {'knockout' if days > 200 else 'league'} phase")
        again = os.path.join(w, "again")
        user = os.path.join(again, "user")
        os.makedirs(user, exist_ok=True)
        shutil.copyfile(first, os.path.join(user, "uefa.sav"))
        shutil.copytree(os.path.join(w, "user", "Databases"), os.path.join(user, "Databases"))
        fixtures.write_settings(user, "classic", [("database", f"folder:{os.path.basename(db)}"), ("faster_days", "on")])
        report = os.path.join(again, "report.tsv")
        steps = fixtures.LOAD_STEPS[:-1] + ["3600000 quit"]     # no first-news wait (its 120 s limit under load)
        r, _ = fixtures.run(program, game, "classic", user, smoke_run.scaled_steps("classic", again, steps),
                            {"RECOMP_SEASON_JUMP": "2024", "RECOMP_RULES_REPORT": report}, 7200)
        with open(os.path.join(again, "stderr.txt"), "w") as f:
            f.write(r.stderr)
        if r.returncode != 0:
            fails.append(f"load {days}: the loaded game ended with code {r.returncode} ({again})")
        fails += [f"load {days}: message box {b}" for b in ufr.BOX.findall(r.stderr)]
        if os.path.exists(report):
            lines = ufr.uefa_lines(report)
            fails += ufr.expect(lines, 2024, "european_cup", LEAGUE, f"load {days}")
            fails += ufr.expect(lines, 2024, "uefa_cup", LEAGUE, f"load {days}")
        else:
            fails.append(f"load {days}: no report ({again})")
    return fails


def run_repeat(program, game, work):
    """Two 2024/25 games with the same steps give equal saves 130 days after the start (as
    tests/seasons/uefa_access_run.py run_repeat), both at the same time."""
    import smoke_run
    db = os.path.join(DB, "October 2024")

    def one(k):
        w = os.path.join(work, k)
        rrc.run(program, game, 2024, 2024, db, ["England"], [], w, 3600, None, {"RECOMP_SAVE_AFTER_DAYS": "130"})
        path = os.path.join(w, "user", smoke_run.SAVE_NAME)
        return open(path, "rb").read() if os.path.exists(path) else None
    with concurrent.futures.ThreadPoolExecutor(2) as ex:
        saves = list(ex.map(one, ("a", "b")))
    if None in saves:
        return [f"repeat: a run made no save ({work})"]
    return [] if saves[0] == saves[1] else ["repeat: the two 2024/25 saves differ"]


RUNS = {
    "2024": run_season("2024", 2024, "October 2024"),
    "2025": run_season("2025", 2025, "December 2025"),
    "change": run_change,
    "load": run_load,
    "repeat": run_repeat,
}


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="uefa-2024-")
    fails = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        futures = {name: ex.submit(RUNS[name], program, game, os.path.join(keep, name)) for name in only}
        for name, fut in futures.items():
            got = fut.result()
            print(f"{name}: {'ok' if not got else 'FAIL'} ({os.path.join(keep, name)})")
            fails += got
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
