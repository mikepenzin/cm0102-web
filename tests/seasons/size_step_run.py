#!/usr/bin/env python3
"""The size step at a season change (0.7.2 league sizes, slice 3, D13; spec
docs/specs/2026-11-13-league-sizes-0.7.2-spec.md).

Usage:
    python3 tests/seasons/size_step_run.py <cm0102_recomp> <game folder> <case> [--keep <folder>] [--reuse]
        [--timeout <seconds>]
    python3 tests/seasons/size_step_run.py save <cm0102_recomp> <game folder> --old <0.7.1 cm0102_recomp>
        --keep <folder>
    python3 tests/seasons/size_step_run.py --list
    python3 tests/seasons/size_step_run.py --self-test

A case starts a season game with changing rules on in a season before a size change of the rules table and plays
through the change (tests/seasons/rules_report_check.py, the season jump), with RECOMP_SIZE_LOG=1. The checks:
    count    at the start of the season of the change each league of the case holds the count of its new row (the
             comp line of the start report);
    division at every season start each league of the size table holds as many clubs in its division as its table
             has rows (rules_report_check.division_differences);
    step     the log has the step line of each league of the case in its new edition, with no shortfall;
    box      no message box.

save (D18, slice 3 check 2): the 0.7.1 build (--old, driven by this tree's scripts: the 0.7.1 scripts click the row
below Portugal on a 2015 database, open item 231) makes a 2015 game of
England and Portugal with changing rules on and saves it 200 game days after its start (RECOMP_SAVE_AFTER_DAYS,
tests/seasons/soak_run.py new_game_leg), in the 2015/16 season. 0.7.1 holds the Segunda Liga at 18 (its class
count). This build loads the save and plays to the end of 2016/17 (soak_run.load_leg). The checks: the Segunda Liga
holds 22 clubs (its 2016/17 row) at the start of 2016/17 with its step line, the division check of every season start,
and no message box in either leg.
"""
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))

# case: (start year, last season, nations, {file: (season of the change, count)}, what it shows)
CASES = {
    "portugal": (2013, 2014, ["England", "Portugal"], {"por_prm": (2014, 18), "por_second": (2014, 24)},
                 "Primeira Liga 16 to 18 (it takes the best two of the Segunda Liga that stay), Segunda Liga 22 to 24 "
                 "(the lowest division of the chain takes clubs of Portugal with no division)"),
    "norway": (2007, 2009, ["England", "Norway"], {"nor_prm": (2009, 16)},
               "Eliteserien 14 to 16 in 2009 (the First Division gives two and takes two clubs with no division)"),
    "grow": (2005, 2008, ["England", "Sweden"], {"swe_prm": (2008, 16)},
             "D23: the Allsvenskan grows from 14 to 16 in 2008 through the real places of 2007 (1 down, 3 up)"),
    "shrink": (2021, 2023, ["England", "France"], {"fra_first": (2023, 18)},
               "D23: Ligue 1 shrinks from 20 to 18 in 2023/24 through the real places of 2022/23 (4 down, 2 up)"),
    "italy": (2003, 2004, ["England", "Italy"], {"ita_ser_a": (2004, 20)},
              "Serie A 18 to 20 in 2004/05 (slice 9 grows the class to 20)"),
    # Slice 9: a 2001 game with changing rules on to 2005/06: Serie B 20, 24 in 2003/04, 22 from 2004/05; Serie A
    # 20 from 2004/05; C1/A 19 in 2004/05 and 18 again in 2005/06.
    "italy2001": (2001, 2005, ["England", "Italy"],
                  {"ita_ser_a": (2004, 20), "ita_ser_b": (2004, 22), "ita_ser_c1a": (2005, 18)},
                  "Italy from 2001 to 2005/06: Serie B 24 then 22, Serie A 20, Serie C1/A 19 then 18"),
    "portugal2015": (2014, 2016, ["England", "Portugal"], {"por_second": (2016, 22), "por_prm": (2016, 18)},
                     "D23 towards a division outside the size table: LigaPro 24 to 22 in 2016/17 through the real "
                     "places of 2015/16 (5 down, open item 248)"),
    "portugal2013": (2013, 2014, ["England", "Portugal"], {"por_prm": (2014, 18), "por_second": (2014, 24)},
                     "Issue 154: the Primeira Liga's 15th and the Segunda Liga's 3rd club that is no reserve side play "
                     "one tie over two legs in 2013/14 (rows por-109, por-110, open item 248)"),
    "belgium2009": (2009, 2010, ["England", "Belgium"], {"bel_second": (2010, 18)},
                    "Item 279: the Belgian second division 19 to 18 in 2010/11: a club that the step moves into the "
                    "third division takes a group (+0x60), so both groups fill (bel_third.cpp 758)"),
    "brazil": (2004, 2006, ["England", "Brazil"], {"bra_nat_second": (2006, 20)},
               "D23 towards a division outside the size table: Serie B 22 to 20 in 2006 through the real places of "
               "2005 (6 down, open item 249)"),
    "conference": (2005, 2007, ["England"], {"eng_conf": (2006, 24)},
                   "D15 (slice 8): League Two sends 2 down and the Conference 2 up (the champion and the play-off "
                   "winner) at each change; the Conference grows from 22 to 24 in 2006/07"),
    # Gap rows of slice 8 (lead-notes/sizes-gaps.md rows 25 to 29, 15 November 2026).
    "refuse": (2005, 2006, ["England"], {},
               "Gap row 26: a Conference champion that may not go up (RECOMP_TEST_REFUSE_CHAMPION=1) gives its place "
               "to the next club that may"),
    "unplayed": (2005, 2006, ["England"], {},
                 "Gap row 27: the unplayed Conference sends up its second club by the original's pick (0x4afbd0)"),
    "ligue3": (2026, 2027, ["England", "France"], {},
               "Gap row 29: the Ligue 3 play-off of the 3rd to 6th plays, and Ligue 2 sends down its 16th"),
    # Item 273 (open-items-0.6.2.md): the National League of 2017/18 on.
    "nl2019": (2019, 2020, ["England"], {},
               "Item 273: the National League 2019/20 sends 4 down (row eng-212, Q278) and plays the play-off of "
               "the 2nd to 7th"),
    "nl2021": (2021, 2022, ["England"], {},
               "Item 273: the National League 2021/22 sends 3 down (row eng-232) and plays the play-off of the 2nd "
               "to 7th"),
}

# The extra divisions of the new game screen of a case (rules_report_check.EXTRA, ENV_EXTRA). portugal2015 needs the
# Segunda Divisao B: without it PorSecond::set_up_rules sets no division below (0x7d1714..0x7d1728) and the PorPrm
# chain returns before its Segunda Liga call (0x7cfc55), so no club of the Segunda Liga goes down there.
EXTRA = {"conference": ["conference"], "refuse": ["conference"], "nl2019": ["conference"], "nl2021": ["conference"],
         "portugal2015": ["portugal"], "portugal2013": ["portugal"]}

# The environment of a case beyond RECOMP_SIZE_LOG=1.
ENV = {"refuse": {"RECOMP_TEST_REFUSE_CHAMPION": "1"}}

# Log lines a case must have: (pattern, the least count, groups that must be equal).
LINES = {
    "conference": [(r"recomp: league sizes: eng: the feeder exchange sent (\d+) down and (\d+) up \((\d+) rows", 1,
                    True)],
    "refuse": [(r"recomp: league sizes: eng: .* may not go up: .* \(place \d+\) goes up in its place", 1, False)],
    "unplayed": [(r"recomp: league sizes: eng: .* goes up to League Two \(the original's pick\)", 1, False),
                 (r"recomp: league sizes: eng: the unplayed Conference exchange moved 2 down", 1, False)],
    "ligue3": [(r"recomp: playoffs: fra_third 2026: places 3 to 6", 1, False)],
    "nl2019": [(r"recomp: playoffs: eng_conf 2019: play-off of places 2 to 7 drawn .*quarter-finals 4 v 7 and 5 v 6",
                1, False),
               (r"recomp: league sizes: eng: the feeder exchange sent (\d+) down and (\d+) up \((\d+) rows", 1,
                True)],
    "nl2021": [(r"recomp: playoffs: eng_conf 2021: play-off of places 2 to 7 drawn .*quarter-finals 4 v 7 and 5 v 6",
                1, False)],
}

# The exchanges of league_promote_and_relegate 0x689c80 between two leagues of a case (the log line of the wrap in
# season_sizes.c): (upper, lower, clubs each way, the least count of such lines). The chain body 0x5752f0 makes no
# such call in a season whose Conference champion may not go up (0x57541a, return at 0x575468), so the least is 1 of
# the 2 season changes of the case.
EXCHANGES = {"conference": ("eng_third", "eng_conf", 2, 1), "refuse": ("eng_third", "eng_conf", 2, 1),
             "nl2019": ("eng_third", "eng_conf", 2, 1), "nl2021": ("eng_third", "eng_conf", 2, 1)}
EXCHANGE_LINE = re.compile(r"recomp: league sizes: (\S+) \(.*?\) down (-?\d+), (\S+) \(.*?\) up (-?\d+), pairs")

# D23: the places of the season before a change (report season, up, down; None: not checked) and the files whose
# count the real places reach, so the size step moves no club into or out of them.
PLACES = {
    "grow": {"swe_prm": (2007, None, 1), "swe_first": (2007, 3, None)},
    "shrink": {"fra_first": (2022, None, 4), "fra_second": (2022, 2, None)},
    "conference": {"eng_third": [(2005, None, 2), (2006, None, 2)], "eng_conf": [(2005, 2, 2), (2006, 2, 4)]},
    "unplayed": {"eng_third": (2005, None, 2)},
    "ligue3": {"fra_second": (2026, None, 3), "fra_third": (2026, 3, None)},
    "nl2019": {"eng_conf": (2019, 2, 4)},
    "nl2021": {"eng_conf": (2021, 2, 3)},
    "portugal2015": {"por_second": (2015, None, 5)},
    "brazil": {"bra_nat_second": (2005, None, 6)},
}
NO_STEP = {"grow": ["swe_prm"], "shrink": ["fra_first"]}
# Gap row 8 (lead-notes/sizes-gaps.md): the lowest division of the size table trades with the real tier below it, so
# the step sends none of its clubs to no division.
NO_EXIT = {"portugal2015": ["por_second"], "brazil": ["bra_nat_second"]}
# Gap row 9: the rows of the Allsvenskan and the Superettan for 2007 say no play-off, so neither plays the class's
# kvalspel (report season, the line of the edition).
# Issue 154: the tie between two divisions, with its log lines (season_playoffs.c build_tie, cross_result).
TIE = {"portugal2013": {"por_second": 2013}}
NO_PLAYOFF = {"grow": {"swe_prm": 2007, "swe_first": 2007}}

MOVE_LINE = re.compile(r"recomp: league sizes: step: (.*) from (.*) to (.*)$")

STEP_LINE = re.compile(r"recomp: league sizes: step: (\S+) \((.*?)\) (\d+): (\d+) clubs, the row (\d+)(.*)")


def step_lines(log):
    """[(file, edition, clubs, row, rest)] of the step lines of a log."""
    out = []
    for line in log.split("\n"):
        m = STEP_LINE.match(line.strip())
        if m:
            out.append((m.group(1), int(m.group(3)), int(m.group(4)), int(m.group(5)), m.group(6)))
    return out


def start_counts(comps):
    """{(file, season): clubs} of the start lines of the leagues."""
    # A file can give two lines: the French Lower Division holds fra_first with no clubs and year 0.
    return {(c["file"], c["season"]): c["clubs"] for c in comps
            if c["phase"] == "start" and c["kind"] == "league" and c["year"]}


def faults(work, case):
    import rules_report_check as rrc
    _year, _last, _nations, wants, _what = CASES[case]
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    fails = [x.strip() for x in log.split("\n") if "recomp: message box from" in x]
    report = os.path.join(work, "report.tsv")
    if not os.path.exists(report):
        return fails + ["no report"]
    clubs = []
    _seasons, comps = rrc.read_report(report, clubs)
    counts = start_counts(comps)
    steps = step_lines(log)
    for file, (season, want) in wants.items():
        have = counts.get((file, season))
        if have != want:
            fails.append(f"{file} {season}: {have} clubs at the start, want {want}")
        mine = [s for s in steps if s[0] == file]
        if not any(s[2] == s[3] == want for s in mine):
            fails.append(f"{file}: no step line with {want} clubs ({mine})")
        fails += [f"{file} {e}: step: {n} clubs, the row {r}{rest}" for f, e, n, r, rest in mine if n != r]
    places = [(file, p) for file, ps in PLACES.get(case, {}).items() for p in (ps if isinstance(ps, list) else [ps])]
    for file, (season, up, down) in places:
        line = [c for c in comps if c["file"] == file and c["season"] == season and c["phase"] == "start"
                and c["kind"] == "league" and c["year"]]
        if not line:
            fails.append(f"{file} {season}: no start line")
            continue
        for key, want in (("up", up), ("down", down)):
            # The reading of rules_report_check.py: the row's count is the direct places, or the direct places and
            # the winners of the class's own play-off (the Allsvenskan 2007: 0 down and its play-off, the row 1).
            c, extra = line[0][key], line[0]["playoff_" + key]
            if want is not None and want not in (c, c + (1 if extra else 0), c + extra):
                fails.append(f"{file} {season}: {key} {c} (+{extra} play-off places), its row {want}")
    fails += exchange_faults(log, case)
    fails += line_faults(log, case)
    for file in NO_STEP.get(case, []):
        names = {c["name"] for c in comps if c["file"] == file}
        moves = [m.group(0) for m in map(MOVE_LINE.match, log.split("\n")) if m and
                 (m.group(2) in names or m.group(3) in names)]
        fails += [f"{file}: the step moved a club: {m}" for m in moves]
    for file, season in NO_PLAYOFF.get(case, {}).items():
        for c in comps:
            if c["file"] == file and c["season"] == season and c["phase"] == "start" and c["kind"] == "league" \
                    and c["year"] and (c["playoff_up"] or c["playoff_down"]):
                fails.append(f"{file} {season}: {c['playoff_up']} up and {c['playoff_down']} down play-off places, "
                             f"its row none")
    for file, season in TIE.get(case, {}).items():
        for step in ("drawn", "final"):
            if not re.search(rf"recomp: playoffs: cross\t{file}\t{season}\t{step}\t", log):
                fails.append(f"{file} {season}: no line of the tie's {step}")
        # The upper club won: the lower club that lost takes no extra place of the size change (review, issue 154).
        if re.search(rf"recomp: playoffs: cross\t{file}\t{season}\tfinal\t[^\t]*\tpor_prm\t", log) and \
                not re.search(rf"recomp: league sizes: .* lost the tie and stays in {file}\n", log):
            fails.append(f"{file} {season}: the club that lost the tie has no line that it stays")
    for file in NO_EXIT.get(case, []):
        names = {c["name"] for c in comps if c["file"] == file}
        fails += [f"{file}: the step sent a club to no division: {m.group(0)}"
                  for m in map(MOVE_LINE.match, log.split("\n")) if m and m.group(2) in names and m.group(3) == "no division"]
    fails += [f"{d[0]} {d[2]}" for d in sorted(rrc.division_differences(comps, clubs))]
    return fails


def exchange_faults(log, case):
    """The exchanges of EXCHANGES: each line of the two leagues moves the clubs of the case each way."""
    if case not in EXCHANGES:
        return []
    upper, lower, clubs, least = EXCHANGES[case]
    lines = [m for m in map(EXCHANGE_LINE.search, log.split("\n")) if m and m.group(1) == upper and m.group(3) == lower]
    fails = [f"{upper} {m.group(2)} down and {lower} {m.group(4)} up, want {clubs} each way" for m in lines
             if int(m.group(2)) != clubs or int(m.group(4)) != clubs]
    if len(lines) < least:
        fails.append(f"{len(lines)} exchanges of {upper} and {lower}, want at least {least}")
    return fails


def line_faults(log, case):
    """The lines of LINES: each pattern at least its count of times; with same, each line's groups are equal."""
    fails = []
    for pattern, least, same in LINES.get(case, []):
        found = [m for m in map(re.compile(pattern).search, log.split("\n")) if m]
        if len(found) < least:
            fails.append(f"{len(found)} lines of {pattern!r}, want at least {least}")
        fails += [f"unequal counts: {m.group(0)}" for m in found if same and len(set(m.groups())) != 1]
    return fails


SAVE_CASE = {"start": 2015, "last": 2016, "nations": ["England", "Portugal"], "days": 200,
             "wants": {"por_second": (2016, 22), "por_prm": (2016, 18)}}


def save_case(argv):
    import rules_report_check as rrc
    import soak_run
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    old, keep = (os.path.abspath(rrc.arg(argv, n)) for n in ("--old", "--keep"))
    os.makedirs(keep, exist_ok=True)
    db = rrc.run_database([], SAVE_CASE["start"], os.path.join(keep, "owndb"))
    leg0, leg1 = os.path.join(keep, "leg00"), os.path.join(keep, "leg01")
    o = {"program": old, "game": game, "rules": "on", "database": db, "nations": SAVE_CASE["nations"], "extra": [],
         "start": SAVE_CASE["start"], "last": SAVE_CASE["last"], "seed": 1, "timeout": 14400}
    if "--reuse" not in argv:
        print("leg 0:", soak_run.new_game_leg(o, leg0, SAVE_CASE["days"]))
        save = os.path.join(leg0, "user", "samesave.sav")
        if not os.path.exists(save):
            print(f"FAIL save: the 0.7.1 leg made no save ({leg0})")
            return 1
        soak_run.LEG_ENV["RECOMP_SIZE_LOG"] = "1"
        o["program"] = program
        result = soak_run.load_leg(o, leg1, save, 0)
        print("leg 1:", result)
    fails = []
    for leg in (leg0, leg1):
        log = os.path.join(leg, "stderr.txt")
        fails += [f"{os.path.basename(leg)}: {x.strip()}" for x in open(log, encoding="latin-1")
                  if "recomp: message box from" in x] if os.path.exists(log) else [f"no {log}"]
    CASES["save"] = (SAVE_CASE["start"], SAVE_CASE["last"], SAVE_CASE["nations"], SAVE_CASE["wants"], "")
    fails += faults(leg1, "save")
    for f in fails:
        print("FAIL", f)
    print(("FAIL" if fails else "PASS") + f" size step save of 0.7.1 ({keep})")
    return 1 if fails else 0


def self_test():
    bad = []
    log = ("recomp: league sizes: step: por_prm (Portuguese Premier League) 2014: 18 clubs, the row 18\n"
           "recomp: league sizes: step: por_second (Portuguese Second League) 2014: 22 clubs, the row 24: no club of "
           "its nation with no division\n")
    s = step_lines(log)
    if s != [("por_prm", 2014, 18, 18, ""),
             ("por_second", 2014, 22, 24, ": no club of its nation with no division")]:
        bad.append(("step_lines", s))
    c = start_counts([{"file": "por_prm", "season": 2014, "phase": "start", "kind": "league", "clubs": 18, "year": 2014},
                      {"file": "por_prm", "season": 2014, "phase": "start", "kind": "league", "clubs": 0, "year": 0},
                      {"file": "por_prm", "season": 2014, "phase": "end", "kind": "league", "clubs": 16, "year": 2015}])
    if c != {("por_prm", 2014): 18}:
        bad.append(("start_counts", c))
    x = ("recomp: league sizes: eng_third (Football League Two) down 2, eng_conf (Football Conference) up 2, pairs 1, "
         "expected -1 up -1 down\n")
    if exchange_faults(x * 2, "conference") or not exchange_faults("", "conference") or \
            not exchange_faults(x.replace("down 2", "down 1") * 2, "conference"):
        bad.append(("exchange_faults", exchange_faults(x, "conference")))
    f = ("recomp: league sizes: eng: the feeder exchange sent 4 down and 4 up (4 rows of status 3, 1 beyond the "
         "original's 3)")
    if line_faults(f, "conference") or not line_faults(f.replace("4 up", "3 up"), "conference") or \
            not line_faults("", "conference"):
        bad.append(("line_faults", line_faults(f, "conference")))
    for b in bad:
        print("FAIL self-test", b)
    print(("FAIL" if bad else "PASS") + " size_step_run self-test")
    return 1 if bad else 0


def main(argv):
    if "--list" in argv:
        for k, (y, last, nations, wants, what) in CASES.items():
            print(f"{k}\t{y} to {last}\t{','.join(nations)}\t{what}")
        return 0
    if "--self-test" in argv:
        return self_test()
    if argv and argv[0] == "save":
        return save_case(argv)
    if len(argv) < 3 or argv[2] not in CASES:
        print(__doc__)
        return 2
    import rules_report_check as rrc
    program, game, case = os.path.abspath(argv[0]), os.path.abspath(argv[1]), argv[2]
    year, last, nations, _w, _what = CASES[case]
    keep = rrc.arg(argv, "--keep", None)
    timeout = int(rrc.arg(argv, "--timeout", "7200"))
    work = os.path.abspath(keep) if keep else tempfile.mkdtemp(prefix=f"cm0102-size-step-{case}-")
    if "--reuse" not in argv:
        db = rrc.run_database(argv, year, os.path.join(work, "owndb"))
        code, _report = rrc.run(program, game, last, year, db, nations, EXTRA.get(case, []), work, timeout,
                                {"changing_rules": "on"}, {"RECOMP_SIZE_LOG": "1", **ENV.get(case, {})})
        if code != 0:
            print(f"FAIL {case}: the run ended with code {code} ({work})")
            return 1
    fails = faults(work, case)
    for f in fails:
        print("FAIL", f)
    print(("FAIL" if fails else "PASS") + f" size step {case} ({work})")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
