#!/usr/bin/env python3
"""ctest of tests/seasons/future/run_set.py without a game: the run set file, the legs of a row, the Tier B limits
and the release check (--verify) on made-up result folders.

Usage:
    python3 -I tests/seasons/future/run_set_test.py
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import history_check_test as hct  # noqa: E402
import run_set  # noqa: E402

GB = 1 << 30


def results(folder, head, rows):
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, "results.tsv"), "w", encoding="utf-8") as f:
        f.write(head + "\n" + "\t".join(run_set.RESULT_HEAD) + "\n")
        for r in rows:
            f.write("\t".join(r) + "\n")


LEGACY = os.path.join(HERE, "run_set_legacy.tsv")      # the 0.7.0 rows: the test plays the script's features on them


def main():
    bad = []
    rows = run_set.read_set(LEGACY)
    release = run_set.chosen(rows, "release")
    names = {r["name"] for r in release}
    for want in ("a2001s1", "a2001offs3", "a2025s3", "a2026s1", "a2026s2", "a2026s3", "a2001esb", "a2001nation", "a2001to2300", "jump2298", "b2001"):
        if want not in names:
            bad.append(f"run set: {want} is not in the release set")
    # The Tier B gate (Max, 9 November 2026): the release rows end after the World Cup and the Euro; the full games to
    # 2300 are background rows that no release check asks for.
    gate = {r["name"]: r["last"] for r in release if r["tier"] == "B"}
    if gate != {"b2001": 2010, "b2025": 2028}:
        bad.append(f"run set: the Tier B release rows and their last seasons are {gate}, not b2001 2010 and b2025 2028")
    background = {r["name"]: (r["tier"], r["start"], r["last"]) for r in run_set.chosen(rows, "background")
                  if r["tier"] == "B"}    # the weekly rows of item 193 are checked under weekly below
    if background != {"b2001long": ("B", 2001, 2300), "b2025long": ("B", 2025, 2300)}:
        bad.append(f"run set: the Tier B background rows are {background}")
    if {r["name"] for r in run_set.chosen(rows, "all")} - {r["name"] for r in rows} or \
            not {"b2001long", "b2025long"} <= {r["name"] for r in run_set.chosen(rows, "all")}:
        bad.append("run set: set all does not hold the background rows")
    if names & {"b2001long", "b2025long"}:
        bad.append("run set: a background row is in the release set")
    if any(r["set"] == "quick" for r in release):
        bad.append("run set: a quick game is in the release set")
    if len([r for r in release if r["tier"] == "A" and r["last"] == 2100 and r["nations"] == "all"]) != 25:
        bad.append("run set: not 25 Tier A games of all nations to 2100 (8 starts with Q56's 2026, three seeds, and Q15)")
    for r in (r for r in release if r["start"] == 2026):
        db = run_set.database_of(r, {"databases": run_set.DATABASES, "out": "", "own": run_set.OWN_DBS,
                                     "data": run_set.DATA_FOLDERS})[0]
        if not (os.path.relpath(db, run_set.DATABASES).startswith("October 2026")
                and os.path.isfile(os.path.join(db, "index.dat"))):
            bad.append(f"run set: {r['name']} does not play the October 2026 data (the folder of its index.dat): {db}")

    # The new game steps of every row (soak_run's leg 0 uses rules_report_check.league_steps). a2001esb picks Spain,
    # below the 16 rows of Select League(s): pass 2 stopped in the runner with a ValueError. Spain is the 13th row
    # after 10 clicks on the list's down arrow (screenshots in run-logs/runset-pass2/esb).
    import rules_report_check as rrc
    for r in rows:
        try:
            rrc.league_steps(r["nations"].split(","), [])
        except ValueError as e:
            bad.append(f"league steps of {r['name']} ({r['nations']}): {e}")
    esb = [s.split(" ", 1)[1] for s in rrc.league_steps(["England", "Spain", "Brazil"], []) if " click " in s]
    esb = esb[esb.index("click 390 254"):] if "click 390 254" in esb else esb      # from the Brazil row
    if esb[:rrc.SCROLL + 3] != ["click 390 254"] + ["click 768 517"] * rrc.SCROLL + ["click 390 430", "click 705 572"]:
        bad.append(f"league steps of England, Spain and Brazil: {esb[:rrc.SCROLL + 3]}")
    # Open item 231: the list holds 27 rows (Wales is the last), so the scroll ends at row 11 and Wales is the bottom row.
    if rrc.below_y("Wales") != 188 + 22 * 15 or rrc.below_y("Portugal") != 188 + 22 * 6 or \
            len(rrc.NATION_ROWS + rrc.NATION_ROWS_BELOW) != 27:
        bad.append("click rows: Wales is not the bottom row, or Portugal is not row 6 of the scrolled list")

    legs = run_set.legs_of({"start": 2001, "last": 2100, "leg_years": 10})
    if len(legs) != 10 or legs[0] != (2001, 2010) or legs[-1] != (2091, 2100):
        bad.append(f"legs of 2001 to 2100 by 10: {legs[:2]} ... {legs[-1:]}")
    if run_set.legs_of({"start": 2098, "last": 2100, "leg_years": 1}) != [(2098, 2098), (2099, 2099), (2100, 2100)]:
        bad.append("legs of a date jump: not one season each")

    # The day count of RECOMP_SAVE_AFTER_DAYS (testrun.c): 366 a year. From day 196 of 2056 to day 200 of 2066 it
    # gives 3664; the calendar gives 3657, which saved the leg of pass 1 on day 181 of 2066, before July.
    if run_set.game_days(2066, run_set.SAVE_ON) - run_set.game_days(2056, 196) != 3664:
        bad.append("game_days: not the count of testrun.c (year * 366 + day)")

    def leg(k, year, seconds, size, no_manager=frozenset()):
        return {"leg": k, "year": year, "seconds": seconds, "days": 365, "bytes": size, "no_manager": set(no_manager)}

    good = [leg(k, 2002 + k, 1000 + 10 * k, 200e6 + 1e6 * k) for k in range(30)]
    if run_set.tier_b_limits(good):
        bad.append(f"Tier B limits: faults in a good game: {run_set.tier_b_limits(good)}")
    checks = {
        "no-manager": good[:5] + [leg(5, 2007, 1050, 205e6, {"Wales"}), leg(6, 2008, 1060, 206e6, {"Wales"})],
        "slow": good[:20] + [leg(k, 2002 + k, 2500, 220e6) for k in range(20, 30)],
        # leg 0 also makes the game and is slow: it must not set the speed of the first seasons
        "slow ": [leg(0, 2002, 9000, 200e6)] + good[1:20] + [leg(k, 2002 + k, 2500, 220e6) for k in range(20, 30)],
        "size": [leg(k, 2002 + k, 1000, 200e6 + 30e6 * k) for k in range(31)],
    }
    for check, game in checks.items():
        got = {c for c, _ in run_set.tier_b_limits(game)}
        if got != {check.strip()}:
            bad.append(f"Tier B limits: {check} not found alone, got {got}")
    one_season = good[:5] + [leg(5, 2007, 1050, 205e6, {"Wales"}), leg(6, 2008, 1060, 206e6, {"Chile"})]
    if run_set.tier_b_limits(one_season):
        bad.append("Tier B limits: a side with no manager in one save only fails")
    if [c for c, _ in run_set.tier_b_limits(good[:3] + [leg(3, 2005, 1000, 4 * GB)])] != ["size"]:
        bad.append("Tier B limits: a save of 4 GB is not a fault")

    # Open item 192: b2001 of pass 4 (run-logs/runset-pass4/run/b2001/legs.tsv, MB). The first seasons grow 20 to 26 MB
    # a season while the first player generation builds its history; the projection waits 30 seasons.
    b2001 = [121, 145, 170, 196, 220, 242, 264, 286, 307, 327]
    young = [leg(k, 2002 + k, 1000, mb * 1e6) for k, mb in enumerate(b2001)]
    if run_set.tier_b_limits(young):
        bad.append(f"Tier B limits: a young Tier B game gives a size fault: {run_set.tier_b_limits(young)}")
    # The same start, then the rate falls to the Tier A rate of 8.4 MB by 2030: 2.9 GB at the end of 2300, no fault.
    level = list(b2001)
    for k in range(10, 40):
        level.append(level[-1] + max(8.4, 20 - 0.6 * (k - 9)))
    levelled = [leg(k, 2002 + k, 1000, mb * 1e6) for k, mb in enumerate(level)]
    if run_set.tier_b_limits(levelled):
        bad.append(f"Tier B limits: a game whose rate levels at 8.4 MB fails: {run_set.tier_b_limits(levelled)}")
    # The same start, then a rate that stays at 20 MB a season: 6.6 GB at the end of 2300, a fault.
    steady = [leg(k, 2002 + k, 1000, (327 + 20 * (k - 9)) * 1e6 if k > 9 else b2001[k] * 1e6) for k in range(40)]
    if [c for c, _ in run_set.tier_b_limits(steady)] != ["size"]:
        bad.append("Tier B limits: a steady rate that passes 4 GB is not a fault after 30 seasons")
    # Only the last 10 seasons count: an early fast start does not fail a game that grows slowly now.
    if [c for c, _ in run_set.tier_b_limits(steady[:30] + [leg(k, 2002 + k, 1000, steady[29]["bytes"] + 5e6 *
                                                             (k - 29)) for k in range(30, 41)])]:
        bad.append("Tier B limits: the rate of the first seasons still counts after 30 seasons")

    # Open item 68: one group of the Greek Super League 2 plays from 2026/27, the other waits at 2998.
    def gre(season, year1, year2, clubs=run_set.GRE_CLUBS):
        return [{"season": season, "phase": "start", "file": "gre_second", "year": year1, "clubs": clubs,
                 "name": "Super League 2"},
                {"season": season, "phase": "start", "file": "gre_second", "year": year2, "clubs": 10,
                 "name": "Super League 2"}]
    # a loaded leg's end line of the season before already shows the next year: not a group that plays
    late = [dict(c, phase="end", year=2027) if c["year"] == 2026 else dict(c, phase="end") for c in gre(2026, 2026, 2998)]
    one_group = gre(2025, 2025, 2025) + gre(2026, 2026, 2998) + late + gre(2027, 2027, 2998)
    if run_set.greek_second(one_group):
        bad.append(f"Greek Super League 2: faults in a game with one group from 2026: {run_set.greek_second(one_group)}")
    for name, comps, want in (("two groups", gre(2026, 2026, 2998) + gre(2027, 2027, 2027), 2027),
                              ("small group", gre(2026, 2026, 2998, clubs=10), 2026),
                              ("no group", gre(2026, 2998, 2998), 2026)):
        got = run_set.greek_second(comps)
        if [s_ for s_, _ in got] != [want]:
            bad.append(f"Greek Super League 2 ({name}): {got}, not one fault in {want}")
    if run_set.greek_second(gre(2025, 2025, 2025)):
        bad.append("Greek Super League 2: two groups in 2025/26 are a fault")
    # Open item 104: a leg that loads a save writes its first start lines under the game's start season
    # (season_report_turn), with the state at the load. jump2198 has one group, slot t144: leg 0 writes 2198 at the
    # start of 2198/99, legs 1 and 2 write 2199 and 2200 under 2198 again. Only the first line of a slot counts.
    def slot(season, year, clubs=run_set.GRE_CLUBS, name="t144"):
        return {"season": season, "phase": "start", "file": "gre_second", "year": year, "clubs": clubs,
                "slot": name, "name": "Super League 2"}
    jump = [slot(2198, 2198), slot(2198, 2199), slot(2199, 2199), slot(2198, 2200), slot(2200, 2200)]
    if run_set.greek_second(jump):
        bad.append(f"Greek Super League 2: the loaded legs of a date jump give faults: {run_set.greek_second(jump)}")
    loaded = [slot(2026, 2026), slot(2026, 2998, 10, "t145"), slot(2027, 2027), slot(2027, 2998, 10, "t145"),
              slot(2026, 2027), slot(2026, 2998, 10, "t145")]
    if run_set.greek_second(loaded):
        bad.append(f"Greek Super League 2: a leg loaded in 2027 gives faults in 2026: {run_set.greek_second(loaded)}")
    if [s_ for s_, _ in run_set.greek_second([slot(2026, 2026), slot(2026, 2026, 10, "t145"),
                                             slot(2026, 2027), slot(2026, 2998, 10, "t145")])] != [2026]:
        bad.append("Greek Super League 2: a later loaded leg hides two groups at the start of 2026/27")

    # A passed leg: a kept leg keeps its save and the gzipped log, another leg the log only.
    work = tempfile.mkdtemp(prefix="run-set-")
    for keep in (True, False):
        leg_dir = os.path.join(work, f"leg-{keep}")
        for d in ("user/Databases/December 2025", "home", "shots"):
            os.makedirs(os.path.join(leg_dir, d))
        for n in ("user/" + run_set.SAVE_NAME, "user/match_stats_matches.tmp", "stderr.txt", "report.tsv"):
            open(os.path.join(leg_dir, n), "w").write("x")
        run_set.tidy(leg_dir, keep)
        left = sorted(os.path.relpath(os.path.join(d, n), leg_dir) for d, _, fs in os.walk(leg_dir) for n in fs)
        want = sorted(["report.tsv", "stderr.txt.gz"] + (["user/" + run_set.SAVE_NAME] if keep else []))
        if left != want:
            bad.append(f"tidy (keep {keep}): {left}, not {want}")

    # The release check on made-up runs. Each game passes with one leg and no kept save, unless a case says else.
    head = run_set.git("rev-parse", "HEAD")
    runs = [r for r in release if r["status"] == "run"]      # the rows a run can pass

    def result(r, **change):
        values = {"name": r["name"], "tier": r["tier"], "set": r["set"], "result": "pass", "reason": "",
                  "reached": "2101", "legs": "1", "faults": "0", "seconds": "1", "commit": head, "code": "clean",
                  "program": "sha-x", "row": run_set.row_key(r), "accepted": "0"}
        values.update(change)
        return [str(values[c]) for c in run_set.RESULT_HEAD]

    def make(name, rows, checkpoint=None, save=None):
        folder = os.path.join(work, name)
        results(folder, "# a head line", rows)
        for r in rows:
            os.makedirs(os.path.join(folder, r[0]), exist_ok=True)
            with open(os.path.join(folder, r[0], "legs.tsv"), "w", encoding="utf-8") as f:
                f.write("\t".join(run_set.LEG_HEAD) + "\n")
                f.write(f"0\t2001/02 to 2001/02\tok\t1\t2002-201\t1\t0\t{'checkpoint' if r[0] == checkpoint else ''}\n")
        if save:
            os.makedirs(os.path.join(folder, checkpoint, "leg000", "user"))
            hct.write_sav(os.path.join(folder, checkpoint, "leg000", "user", run_set.SAVE_NAME), save)
        return run_set.verify(folder, rows=runs)

    def finds(lines, text):
        return any(text in line for line in lines)

    ok = make("ok", [result(r) for r in runs])
    if ok:
        bad.append(f"verify: faults in a run where every game passed: {ok[:3]}")
    cases = [
        ("fail", [result(r, result="fail", reason="3 checker faults") if r["name"] == "b2001" else result(r)
                  for r in runs], "b2001: fail 3 checker faults"),
        ("short", [result(r) for r in runs[1:]], "not in the run"),
        ("old", [result(r, commit=run_set.git("rev-list", "-1", "HEAD", "--", "src") + "~1") for r in runs],
         "game code changed"),
        ("dirty", [result(r, code="changes") for r in runs], "not a build of the checkout"),
        ("outside", [result(r, code="outside") for r in runs], "not a build of the checkout"),
        ("two", [result(r, program="sha-y" if r["name"] == "b2025" else "sha-x") for r in runs], "different programs"),
        ("row", [result(r, row=run_set.row_key(dict(r, last=2099))) if r["name"] == "a2013s2" else result(r)
                 for r in runs], "another row"),
    ]
    for name, rows_, text in cases:
        if not finds(make(name, rows_), text):
            bad.append(f"verify ({name}): no line with {text!r}")
    if not finds(make("gone", [result(r) for r in runs], checkpoint="b2001"), "checkpoint save of leg 0 is gone"):
        bad.append("verify: a lost checkpoint save passes")
    members, _ = hct.records()
    lines = make("faults", [result(r) for r in runs], checkpoint="b2001", save=members)
    for text in ("samesave.sav: duplicate Euro Finals 2028", "samesave.sav: no-host World Finals 2034",
                 "samesave.sav: same-final Euro Super Cup 2028"):      # history faults that the fixture plants
        if not finds(lines, text):
            bad.append(f"verify: the checker does not find {text!r} on a kept save")
    lines = make("faults-a", [result(r) for r in runs], checkpoint="a2006s1", save=members)
    if not finds(lines, "samesave.sav: duplicate Euro Finals 2028") or finds(lines, "same-final"):
        bad.append("verify: a Tier A save does not skip same-final (a check of real results) or skips more")
    # A row that waits has no result, and it blocks a release (a2001nation waited until Q15 was built).
    make("waits", [result(r) for r in runs if r["name"] != "a2001nation"])
    waiting = run_set.verify(os.path.join(work, "waits"), rows=[dict(r, status="waits") if r["name"] == "a2001nation"
                                                               else r for r in rows])
    if not finds(waiting, "a2001nation: not in the run"):
        bad.append("verify: a waiting row does not block a release")

    # Q15: leg 0 of a NATION_JOB row passes only with the appointment and the lines after the team's next match.
    if run_set.NATION_JOB.get("a2001nation") != "England" or \
            [r for r in run_set.read_set(LEGACY) if r["name"] == "a2001nation"][0]["status"] != "run":
        bad.append("nation job: a2001nation is not a NATION_JOB row with status run")
    made = "recomp: nation job test: the human manages Wales from day 182 of 2001 (appointed)\n"
    still = "recomp: nation job test: on day 245 of 2001 (after the match of day 244) the human still manages Wales\n"
    lost = still.replace("still manages", "no longer manages")
    match = "recomp: nation job test: Wales played the match of day 244 of 2001 against Spain\n"
    none = "recomp: nation job test: Wales did not play the match of day 244 of 2001\n"
    for text, want in ((made + still + match, ""), (made + lost + match, "no line that the human still manages Wales"),
                       (made + still, "no line that Wales played its next match"),
                       (made + still + none, "no line that Wales played its next match"),
                       (made + still + match.replace("Wales", "Scotland"), "no line that Wales played"),
                       (made, "no line that the human still manages Wales"), ("", "no appointment"),
                       (made + still.replace("Wales", "Scotland") + match, "no line that the human still manages Wales"),
                       ("recomp: nation job test: no national team without a manager and a value over 150\n",
                        "no appointment (no national team"),
                       (made.replace("(appointed)", "(the call did not appoint him)") + still + match, "no appointment")):
        got = run_set.nation_job_check(text)
        if (want == "" and got) or (want and want not in got):
            bad.append(f"nation_job_check: {got!r} for {text!r}, wanted {want!r}")
    code = subprocess.run([sys.executable, "-I", os.path.join(HERE, "run_set.py"), "--verify",
                           os.path.join(work, "fail")], capture_output=True).returncode
    if code != 1:
        bad.append(f"verify: exit code {code} for a failed run, not 1")

    # Accepted rows (Q12): a known row that cites a source is no fault and is counted apart; a rules condition holds.
    class FakeGame:
        def __init__(self, changing):
            self.changing = changing
            self.date = (2022, 7, 1)
    checker = run_set.Checker.__new__(run_set.Checker)
    checker.known = [{"check": "gap", "competition": "A Cup", "year": "2022", "note": "source", "rules": "",
                      "accepted": "real"},
                     {"check": "gap", "competition": "B Cup", "year": "*", "note": "no source", "rules": "",
                      "accepted": ""},
                     {"check": "entrant-host", "competition": "C Cup", "year": "2002", "note": "design 0x5f9970",
                      "rules": "off", "accepted": "design"}]
    faults = [("gap", "A Cup", 2022, "d"), ("gap", "B Cup", 2001, "d"), ("gap", "D Cup", 2001, "d"),
              ("entrant-host", "C Cup", 2002, "d")]
    for changing, want_kinds in ((0, ["accepted", "known", "new", "accepted"]), (1, ["accepted", "known", "new", "new"])):
        kinds, n = run_set.grade(checker, faults, FakeGame(changing))
        if kinds != want_kinds or n != want_kinds.count("accepted"):
            bad.append(f"grade (changing rules {changing}): {kinds} and {n}, not {want_kinds}")
    # full-squad (item 191): accepted by the design row, new again for a club full in 3 saves in a row
    checker.known = [{"check": "full-squad", "competition": "*", "year": "*", "note": "design 0x83cb40", "rules": "",
                      "accepted": "design"}]
    streak, seen = {}, []
    for leg, clubs in enumerate((["Alpha", "Beta"], ["Alpha"], ["Alpha", "Beta"], ["Beta"], ["Alpha"])):
        fs = [("full-squad", c, 2010 + leg, "d") for c in clubs]
        kinds, n = run_set.grade(checker, fs, FakeGame(0), streak)
        seen.append((kinds, n))
    # Alpha: legs 0,1,2 in a row -> new on leg 2; Beta: 0, then 2, 3 -> never 3 in a row; Alpha: leg 4 after a break
    want = [(["accepted", "accepted"], 2), (["accepted"], 1), (["new", "accepted"], 1), (["accepted"], 1),
            (["accepted"], 1)]
    if seen != want:
        bad.append(f"grade full-squad streak: {seen}, not {want}")
    if run_set.grade(checker, [("full-squad", "Alpha", 2010, "d")] * 1, FakeGame(0))[0] != ["accepted"]:
        bad.append("grade without a streak does not accept a full-squad fault")
    # a late row (item 193) starts from a save: no database, whatever its start year
    if run_set.database_of({"tier": "late", "start": 2101, "name": "late2101"}, {"databases": "/nonexistent"})[0] is not None:
        bad.append("database_of: a late row has a database")
    old_read = run_set.hc.read_known
    run_set.hc.read_known = lambda path: [{"check": "duplicate", "competition": "Euro Finals", "year": "2028",
                                           "note": "source", "rules": "", "accepted": "real"}]
    try:
        lines = make("accepted", [result(r) for r in runs], checkpoint="b2001", save=members)
    finally:
        run_set.hc.read_known = old_read
    if finds(lines, "duplicate Euro Finals 2028") or not finds(lines, "no-host World Finals 2034"):
        bad.append("verify: an accepted fault still fails the release check, or an unknown one passes")
    os.makedirs(os.path.join(work, "fin"), exist_ok=True)
    row_ = run_set.finish(os.path.join(work, "fin"), runs[0], "pass", "3 accepted", "2101", 1, 0, 0.0,
                          {"commit": "c", "code": "clean", "sha": "s"}, 3)
    if row_[-1] != 3 or len(row_) != len(run_set.RESULT_HEAD):
        bad.append(f"finish: the result row {row_} has no accepted count at the end")

    # Open item 193: a late row (the full world from a checkpoint save of a Tier A row).
    head_line = "\t".join(run_set.COLUMNS + ["from"]) + "\n"
    tier_a = "a\tA\t2001\ton\tall\t1\t2300\t10\trelease\trun\t\t\n"
    for line, ok_ in (("l\tlate\t2291\ton\tall\t1\t2300\t1\trelease\trun\tn\ta\n", True),
                      ("l\tlate\t2290\ton\tall\t1\t2300\t1\trelease\trun\tn\ta\n", False),   # no save of 2290
                      ("l\tlate\t2291\toff\tall\t1\t2300\t1\trelease\trun\tn\ta\n", False),  # other rules
                      ("l\tlate\t2291\ton\tall\t1\t2300\t1\trelease\trun\tn\t\n", False),    # no from
                      ("l\tB\t2001\ton\tall\t1\t2300\t1\trelease\trun\tn\ta\n", False)):     # from on B
        path = os.path.join(work, "late_set.tsv")
        with open(path, "w", encoding="utf-8") as f:
            f.write(head_line + tier_a + line)
        try:
            got = run_set.read_set(path)
            if not ok_:
                bad.append(f"late: read_set takes {line.strip()!r}")
            elif got[1]["from"] != "a" or got[0]["from"] != "":
                bad.append("late: the column from is not read")
        except SystemExit:
            if ok_:
                bad.append(f"late: read_set refuses {line.strip()!r}")
    if any("from=" in run_set.row_key(r) for r in rows if r["tier"] != "late"):
        bad.append("late: the row key of a row with no `from` changed (old results would not verify)")
    with open(path, "w", encoding="utf-8") as f:
        f.write(head_line + tier_a + "l\tlate\t2291\ton\tall\t1\t2300\t1\trelease\trun\tn\ta\n")
    late = dict(run_set.read_set(path)[1])
    if "|from=a" not in run_set.row_key(late) or run_set.checker_tier(late) != "A" or \
            not run_set.full_world(late) or run_set.full_world(dict(late, tier="A")):
        bad.append("late: row key, checker tier or full world")
    out = os.path.join(work, "late_out")
    os.makedirs(os.path.join(out, "a", "leg028", "user"))
    with open(os.path.join(out, "a", "legs.tsv"), "w", encoding="utf-8") as f:
        f.write("\t".join(run_set.LEG_HEAD) + "\n")
        f.write("27\t2271/72 to 2280/81\tok\t1\t2281-202\t1\t0\tcheckpoint\n")
        f.write("28\t2281/82 to 2290/91\tok\t1\t2291-201\t1\t0\tcheckpoint\n")
    with open(os.path.join(out, "a", "result.tsv"), "w") as f:
        f.write("a\n")
    if run_set.checkpoint(late, {"out": out}, log=lambda t: None, poll=0) is not None:
        bad.append("late: a checkpoint without its save file")
    save = os.path.join(out, "a", "leg028", "user", run_set.SAVE_NAME)
    open(save, "wb").close()
    if run_set.checkpoint(late, {"out": out}, log=lambda t: None, poll=0) != (save, run_set.game_days(2291, 200)):
        bad.append("late: the checkpoint of 2291 is not leg 28's save of day 201")
    if run_set.checkpoint(dict(late, start=2301), {"out": out}, log=lambda t: None, poll=0) is not None:
        bad.append("late: an ended Tier A row gives a checkpoint it did not save")
    # The checkpoint save must come from a game of the same program (lead, 8 November 2026).
    if run_set.game_program(out, "a") is not None:
        bad.append("late: game_program finds a program that nothing records")
    with open(os.path.join(out, "results.tsv"), "w", encoding="utf-8") as f:
        f.write("# commit c1 clean program sha-head 2026-11-08 18:00\n")
    if run_set.game_program(out, "a") != "sha-head":
        bad.append("late: game_program does not fall back to the head line of results.tsv")
    with open(os.path.join(out, "a", "result.tsv"), "w", encoding="utf-8") as f:
        f.write("\t".join(result(dict(late, name="a", tier="A"), program="sha-result")) + "\n")
    if run_set.game_program(out, "a") != "sha-result":
        bad.append("late: game_program does not read result.tsv")
    with open(os.path.join(out, "a", run_set.PROGRAM_FILE), "w", encoding="utf-8") as f:
        f.write("c1\tclean\tsha-a\n")
    if run_set.game_program(out, "a") != "sha-a":
        bad.append("late: game_program does not read program.tsv first")
    real = [r for r in rows if r["name"] == "late2291"]
    if not real or real[0]["from"] != "a2001to2300" or real[0]["last"] != 2300:
        bad.append("late: no row late2291 from a2001to2300 to 2300")
    else:
        lo = {"program": "/no/program", "game": "/no/game", "out": out, "databases": run_set.DATABASES,
              "own": run_set.OWN_DBS, "commit": "c1", "code": "clean", "sha": "sha-b", "machine": 12, "floor": 0,
              "data": run_set.DATA_FOLDERS, "seed": None, "set_file": LEGACY}
        os.rename(os.path.join(out, "a"), os.path.join(out, "a2001to2300"))
        got = run_set.play_game(real[0], lo)
        if got[3] != "fail" or "comes from another program" not in got[4]:
            bad.append(f"late: a checkpoint save of another program does not fail the game ({got[3]} {got[4]})")
        if open(os.path.join(out, "late2291", run_set.PROGRAM_FILE)).read() != "c1\tclean\tsha-b\n":
            bad.append("late: play_game does not record its program in program.tsv")
        gone = dict(real[0], start=2281, last=2290)          # the Tier A row ended with no save of 2281
        got = run_set.play_game(dict(gone, name="late2281"), lo)
        if got[3] != "fail" or "no checkpoint save in 2281" not in got[4]:
            bad.append(f"late: a missing checkpoint save does not fail the game ({got[3]} {got[4]})")
    commit = make("late-commit", [result(r, commit=run_set.git("rev-parse", "HEAD~1")) if r["name"] == "a2001to2300"
                                  else result(r) for r in runs])
    if finds(commit, "late2291"):
        bad.append("verify: a late row fails when its Tier A row ran the same program from an older commit of the "
                   "same game code")
    other = make("late-other", [result(r, program="sha-y") if r["name"] == "a2001to2300" else result(r) for r in runs])
    if not finds(other, "late2291: its checkpoint save comes from a2001to2300, which was not played with the same"):
        bad.append("verify: a late row whose Tier A row used another program passes")

    late_rows = [dict(late, name="late1")]
    late_res = [result(late_rows[0])]
    folder = os.path.join(work, "late_verify")
    results(folder, "# a head line", late_res)
    os.makedirs(os.path.join(folder, "late1", "leg000", "user"))
    hct.write_sav(os.path.join(folder, "late1", "leg000", "user", run_set.SAVE_NAME), members)
    with open(os.path.join(folder, "late1", "legs.tsv"), "w", encoding="utf-8") as f:
        f.write("\t".join(run_set.LEG_HEAD) + "\n0\t2291/92 to 2291/92\tok\t1\t2292-201\t1\t0\tcheckpoint\n")
    lines = run_set.verify(folder, rows=late_rows)
    if not finds(lines, "duplicate Euro Finals 2028") or finds(lines, "same-final"):
        bad.append(f"late: verify does not check a late save like a Tier A history ({lines[:3]})")

    # Open item 193: the weekly rows play the full world with the test-only market switch, and no other row has it.
    if [r for r in release if r["tier"] == "weekly"] or len(release) != 35:
        bad.append(f"weekly: the release set has {len(release)} games or a weekly row (the lead: 35, weekly rows in "
                   f"background so --verify does not wait for them)")
    weekly = {r["name"]: r for r in run_set.chosen(rows, "background") if r["tier"] == "weekly"}
    if {r["name"] for r in run_set.chosen(rows, "all") if r["tier"] == "weekly"} != set(weekly):
        bad.append("weekly: --set all does not hold the background rows")
    if set(weekly) != {"bw2001", "bw2025"} or any(r["last"] != 2300 or r["leg_years"] != 1 or
                                                   "TEST-ONLY MARKET" not in r["note"] for r in weekly.values()):
        bad.append(f"weekly: rows {sorted(weekly)}, not bw2001 and bw2025 to 2300 in legs of one season marked TEST-ONLY")
    if run_set.TIER_ENV.get("weekly") != {"RECOMP_TEST_MARKET_WEEKLY": "1"} or \
            any("RECOMP_TEST_MARKET_WEEKLY" in e for t, e in run_set.TIER_ENV.items() if t != "weekly") or \
            "RECOMP_TEST_MARKET_WEEKLY" not in run_set.SWITCHES:
        bad.append("weekly: the switch is not set for the weekly tier only, or not cleared between games")
    if not all(run_set.full_world(r) and run_set.checker_tier(r) == "weekly" for r in weekly.values()):
        bad.append("weekly: the rows do not get the Tier B checks and all checks of real results")
    if not all(r["tier"] == "B" for r in release if r["name"] in ("b2001", "b2025")):
        bad.append("weekly: b2001 and b2025 no longer play the real market")


    # Release tests 0.7.1, slice 1: the database and extras columns, the end point table and --seed.
    spec_ends = {2001: 2004, 2002: 2006, 2003: 2006, 2004: 2008, 2005: 2008, 2006: 2010, 2007: 2010, 2008: 2012,
                 2009: 2012, 2010: 2014, 2011: 2014, 2012: 2016, 2013: 2016, 2014: 2018, 2015: 2018, 2016: 2021,
                 2017: 2021, 2018: 2023, 2019: 2023, 2020: 2023, 2021: 2024, 2022: 2024, 2023: 2026, 2024: 2028,
                 2025: 2028, 2026: 2030}
    if run_set.ENDS != spec_ends:
        bad.append("end points: the table is not the one of the spec (Decisions, End point)")
    if run_set.end_season(2001, "tile") != 2003 or run_set.end_season(2016, "tile") != 2020 or \
            run_set.end_season(2001, "2001") != 2001:
        bad.append("end points: tile is not the season before the end year")
    if run_set.extras_of("none") != [] or run_set.extras_of("all") != ["conference", "regional", "c2", "spain",
                                                                       "portugal", "sweden"] or \
            run_set.extras_of("c2,sweden") != ["c2", "sweden"]:
        bad.append("extras: none, all or a list is not read")
    for text in ("c3", "all,c2x"):
        try:
            run_set.extras_of(text)
            bad.append(f"extras: {text} is taken")
        except SystemExit:
            pass
    head2 = "\t".join(run_set.COLUMNS + ["from", "database", "extras"]) + "\n"
    cases = (("x\tnormal\t2001\ton\tall\t1\ttile\t1\trelease\trun\tn\t\t3.9.65\tall\n", True),
             ("x\tnormal\t2001\ton\tall\t1\ttile\t1\trelease\trun\tn\t\t\t\n", True),
             ("x\tnormal\t2001\ton\tall\t1\t2003\t1\trelease\trun\tn\n", True),     # no optional cell at all
             ("x\tnormal\t2001\ton\tall\t1\ttile\t1\trelease\trun\tn\t\t3.9.99\t\n", False),
             ("x\tnormal\t2001\ton\tall\t1\ttile\t1\trelease\trun\tn\t\t\tc3\n", False),
             ("x\tnormal\t2030\ton\tall\t1\ttile\t1\trelease\trun\tn\n", False))      # no end point for 2030
    for line, ok_ in cases:
        path = os.path.join(work, "cols_set.tsv")
        with open(path, "w", encoding="utf-8") as f:
            f.write(head2 + line)
        try:
            got = run_set.read_set(path)[0]
            if not ok_:
                bad.append(f"columns: read_set takes {line.strip()!r}")
            elif "3.9.65" in line and (got["database"], got["extras"], got["last"]) != (
                    "3.9.65", run_set.ALL_EXTRAS, 2003):
                bad.append(f"columns: a row with database and extras reads as {got}")
            elif "\t\t\t\n" in line and (got["database"], got["extras"]) != ("tile", []):
                bad.append("columns: empty database and extras are not tile and none")
        except SystemExit:
            if ok_:
                bad.append(f"columns: read_set refuses {line.strip()!r}")
    seeded = run_set.read_set(LEGACY, seed=72)
    if {r["seed"] for r in seeded} != {72} or run_set.row_key(seeded[0]) == run_set.row_key(rows[0]):
        bad.append("seed: --seed does not set the seed of every row, or the row key does not hold it")
    if {r["seed"] for r in rows} == {72}:
        bad.append("seed: the column is lost")
    dbo = {"databases": run_set.DATABASES, "out": "", "own": run_set.OWN_DBS, "data": {"3.9.60": "/d60", "3.9.65": "/d65"}}
    base = {"tier": "normal", "start": 2001, "database": "tile"}
    for row, want in ((base, (None, "3.9.60", "/d60")), (dict(base, database="3.9.65"), (None, "3.9.65", "/d65")),
                      (dict(base, database="3.9.68"), (None, "3.9.68", None))):
        if run_set.database_of(row, dbo) != want:
            bad.append(f"database: {row['database']} gives {run_set.database_of(row, dbo)}, want {want}")
    if run_set.row_database(dict(base, start=2006)) != "Summer 2006" or \
            run_set.row_database(dict(base, start=2013)) != "Autumn 2013" or \
            run_set.row_database(dict(base, tier="jump", start=2098)) != "December 2025":
        bad.append("database: the tile data of a start year")
    import soak_run
    with tempfile.TemporaryDirectory() as user:
        soak_run.settings(user, "on", None, "3.9.65")
        if "database = 3.9.65" not in open(os.path.join(user, "settings.txt")).read():
            bad.append("database: soak_run does not write the option 3.9.65")
    if not {"conference", "regional", "c2"} <= set(rrc.EXTRA) or not {"spain", "portugal", "sweden"} <= rrc.ENV_EXTRA:
        bad.append("extras: the three clicks and the three test switch names")
    if len([s for s in rrc.league_steps(["all"], ["conference", "regional", "c2"]) if " click 675 " in s]) != 3:
        bad.append("extras: Select All with three extra divisions does not click three boxes")
    # The save checks: the database by club names, the extras by competition names.
    n60 = {"Fra": {f"f{i}" for i in range(0, 60)}, "Swe": {f"s{i}" for i in range(0, 50)}}
    n65 = {"Fra": {f"f{i}" for i in range(10, 70)}, "Swe": {f"s{i}" for i in range(0, 50)}}
    n68 = {"Fra": {f"f{i}" for i in range(70, 400)}, "Swe": {f"s{i}" for i in range(0, 80)}}
    names = {"3.9.60": n60, "3.9.65": n65, "3.9.68": n68}
    for db, own in names.items():           # a game with half the clubs of France and Sweden, two second teams added
        save = {nation: set(sorted(own[nation])[::2]) | {"second 1", "second 2"} for nation in ("Fra", "Swe")}
        if run_set.save_database(save, names) != db:
            bad.append(f"save database: the save of {db} is read as {run_set.save_database(save, names)}")
    if run_set.save_database({"Swe": {"s1", "s2"}}, names) is not None:
        bad.append("save database: a nation that does not tell the databases apart gives an answer")
    if run_set.save_database({}, names) is not None:
        bad.append("save database: an empty save gives a database")
    flags = {"England": 2, "Sweden": 4, "Spain": 4, "Germany": 0}
    if run_set.extras_missing(flags, ["sweden", "spain"]) != [] or \
            run_set.extras_missing(flags, ["sweden", "c2", "regional", "conference"]) != ["c2", "regional", "conference"]:
        bad.append("extras: a save that does not set an extra division is not found (the competitions are in every save)")
    for b in bad:
        print("FAIL  " + b)
    print(f"{'ok  ' if not bad else 'FAIL'}  run set: {len(release)} games in the release set, legs, Tier B limits and "
          f"the release check (work folder {work})")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
