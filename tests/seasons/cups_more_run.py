#!/usr/bin/env python3
"""Domestic cups, part 21c (0.6.0): four-club super cups, group stages and new cups.

Usage:
    python3 tests/seasons/cups_more_run.py <cm0102_recomp> <game folder> [--only <name>,...] [--keep <folder>]
        [--reuse]

Each run is a season jump of tests/seasons/rules_report_check.py with the nations of its cups selected. The checks
read the "cupmore" lines of the rules report (src/recomp_rt/win/season_super_cups.c: file, year, clubs, rounds,
stages, groups, knockout, winners) and the log (docs/research/2026-10-18-cups-more.md).

The runs:
    i2023   October 2023, changing rules on, 2023/24 to 2024/25, Italy: the Supercoppa Italiana has four clubs and
            two rounds (semi-finals and final) at the new game and after the season change, and its final has a
            winner in the history
    s2019   start run, October 2019, changing rules off, Select All without France: the Supercopa de Espana has four
            clubs and two rounds at the new game
    g2001   the test seam RECOMP_CUP_GROUPS_TEST=1, 2001/02, changing rules on, England: the League Cup plays 16
            groups and a knockout of 32 clubs in 5 rounds (17 stages) and has a winner
    e2016   October 2016, changing rules off, England: the EFL Trophy plays 16 groups (56 clubs) and a knockout
            of 32 clubs in 5 rounds, and has a winner
    n2008   September 2008, changing rules on, 2008 to 2010, Norway: the Norwegian Super Cup is made, plays its
            editions of March 2009 and March 2010 with two clubs, and the row of 2010/11 holds no edition of March 2011
    c2007   November 2007, changing rules off, England with the Conference: the Conference League Cup is made and
            has a winner
    c2008-off  2008/09 (the original data moved to 2008), changing rules off, England, Tier A (RECOMP_TIER_A=1), to
            the end of 2024/25 (about 7 minutes at load 15): the rules of the start season stay, so the Conference League Cup
            plays every season in the format of 2008/09, also in 2024/25, whose row is the National League Cup that
            the game does not play (open item 131: before, it took that row's rounds of day 0 and stopped at the box
            "Date..cpp 52" on 15 June 2024); it has 5 rounds for 22 clubs in 2024, a winner at the end of 2024 and
            no message box
    t2010   start run, September 2010 ODB, changing rules off, Select All without France (Turkey, Russia and the
            Republic of Ireland are not in the 16 rows that the steps click): the Turkish Cup has 4 groups after 3
            rounds, the Turkish Super Cup is made with two clubs, the Russian one of March 2011 (the edition of
            the row of 2010/11) with two clubs, the All-Ireland cup with 12 clubs
    a2003   item 145: 2003/04 (the original data moved to 2003), changing rules on, Northern Ireland and the
            Republic of Ireland, Tier A (RECOMP_TIER_A=1), to the end of 2010/11 (about 5 minutes): the season change
            to 2004/05 makes the All-Ireland cup (the Setanta Cup), which plays each real edition from its first.
            2004/05: 6 clubs (2 of each league by place and the two
            national cup winners) in 2 groups of 3, the winners in the final. 2005/06 to 2007/08: 4 clubs of each
            league in 2 groups of 4 with two of each league, semi-finals and final. 2008/09: none. 2009/10: 9 clubs
            (4 of each league and the holders) in 3 groups of 3, the winners and the best runner-up in two-legged
            semi-finals, then the final. 2010/11: the knockout of 12 clubs. Each edition has a winner in the history,
            the news says "for the first time" in 2004/05 and "again" in 2009/10, and no error box
    a2009   open item 148: 2009/10 (the original data moved to 2009), changing rules on, Northern Ireland and the
            Republic of Ireland, Tier A, to the end of 2013/14: the knockout editions of 2010/11 to 2012/13 (12
            clubs, 6 of each league) and 2013/14 (8 clubs, 4 of each league) pick their clubs again on the draw
            day of the first round, so the clubs of each league are its first places of the season just ended, the
            byes go to the first two of each league,
            each first-round tie is one club of each league (the 2011 to 2014 rule); each edition has a winner
    r2002   start run, 2002/03 (the original data), changing rules off, Select All without France: the start hook
            makes the editions that the row of 2002/03 holds, the Russian Premier League Cup of 2003 (first round in
            March 2003) with 16 clubs and the Russian Super Cup of March 2003 with two clubs (log lines)

Read runs (fix of 2026-10-21): RECOMP_CUP_GROUPS_READ=1 reads each cup's round table at each report as the Schedule
tab and scout duty do (Comp::build_match_date_list 0x48d500); the groups and the knockout must stay.
    e2016-read  e2016 with the reads: the EFL Trophy keeps its 16 groups and its knockout of 32 to a winner
    x2016-read  October 2016, changing rules off, Select All without France, the season played to its end (about
                an hour): the Scottish League Cup (8 groups, a knockout of 16), the EFL Trophy and the Turkish Cup
                (8 groups after 2 rounds, a knockout of 16) keep their groups to a winner; no error box

Save and load runs (review 1 of 2026-10-20): the first program plays the season jump and saves the game
RECOMP_SAVE_AFTER_DAYS game days after the start, inside a competition of this part; a second program loads that save
(tests/fixtures/fixtures.py LOAD_STEPS) and plays on with the season jump and the rules report to the end report of
the season. The check reads the loaded program's report and log.
    e2016-load  October 2016, England, changing rules on: saved after 100 days (late October 2016, between the
                second and the third group match day of the EFL Trophy); after the load the knockout of 32 clubs is
                drawn and the Trophy has a winner
    i2023-load  October 2023, Italy, changing rules on: saved after 189 days (20 January 2024, between the
                semi-finals of 18 January and the final of 22 January of the four-club Supercoppa); after the load
                the final is played and the cup has a winner
    n2008-load  September 2008, Norway, changing rules on: saved after 150 days (December 2008, after the module
                made the Norwegian Super Cup of March 2009 on its vtable copy); after the load the cup is played
                and has a winner
    a2004-load-wait  item 145: 2004/05 (the original data moved to 2004), Northern Ireland and the Republic of
                Ireland, changing rules on: saved after 100 days (October 2004, before the draw day of the
                All-Ireland cup, 13 December); after the load the groups and the final are drawn and played
    a2004-load-mid  the same, saved after 262 days (late March 2005, between group match days); after the load the
                groups end, the final is drawn and the cup has a winner
    a2007-load-gap  2007/08 (the original data moved to 2007), the same nations: saved after 400 days (August 2008,
                in 2008/09, which has no edition); after the load the cup comes back in 2009/10 with 9 clubs in 3
                groups and has a winner

A run starts a game only when fewer than 15 games run on this Mac and the load of the last minute is below 24 (Max's
limit), and its game stops when the script stops (SIGTERM, SIGINT, SIGHUP). Run at most three of these scripts at
a time (--only).
"""
import os
import re
import signal
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
import rules_report_check as rrc  # noqa: E402

DB = "/Users/max/Projects/cm0102/test-fixtures/databases"
ALL_BUT_FRANCE = ["all", "France"]


def cupmore(report):
    """The cupmore lines of a report."""
    out = []
    with open(report, encoding="latin-1") as f:
        for line in f:
            c = line.rstrip("\n").split("\t")
            if c[0] != "cupmore" or len(c) < 15:
                continue
            out.append({"season": int(c[1]), "phase": c[2], "file": c[3], "year": int(c[4]), "clubs": int(c[5]),
                        "rounds": int(c[6]), "stages": int(c[7]), "now": int(c[8]), "groups": int(c[9]),
                        "group_clubs": int(c[10]), "ko_clubs": int(c[11]), "ko_rounds": int(c[12]),
                        "winner": c[13], "winner_next": c[14]})
    return out


def lines_of(lines, file, season, phase):
    return [x for x in lines if x["file"] == file and x["season"] == season and x["phase"] == phase]


def need_four(lines, file, season, phase, fails, name):
    got = lines_of(lines, file, season, phase)
    if not got:
        fails.append(f"{name}: no {file} line at the {phase} of {season}")
    for x in got:
        if x["clubs"] != 4 or x["rounds"] != 2:
            fails.append(f"{name}: {file} at the {phase} of {season}: {x['clubs']} clubs, {x['rounds']} rounds")


def need_winner(lines, file, season, fails, name):
    got = lines_of(lines, file, season, "end")
    if not got or all(x["winner"] == "-" and x["winner_next"] == "-" for x in got):
        fails.append(f"{name}: {file} has no winner in the history at the end of {season}")


def check_x2016_read(lines, log, fails, name):
    need_groups(lines, "eng_auto_cup", 2016, (17, 16, 56, 32, 5), fails, name)
    need_winner(lines, "eng_auto_cup", 2016, fails, name)
    need_groups(lines, "sco_lge_cup", 2016, (9, 8, None, 16, 4), fails, name)
    need_winner(lines, "sco_lge_cup", 2016, fails, name)
    need_groups(lines, "tur_cup", 2016, (9, 8, None, 16, 4), fails, name)
    need_winner(lines, "tur_cup", 2016, fails, name)
    if "recomp: message box" in log:
        fails.append(f"{name}: an error box in the log")


def check_i2023(lines, log, fails, name):
    need_four(lines, "ita_super", 2023, "start", fails, name)
    need_winner(lines, "ita_super", 2023, fails, name)
    need_four(lines, "ita_super", 2024, "start", fails, name)
    for s in (2023, 2024):
        if f"super cups: ita_super {s} with four clubs" not in log:
            fails.append(f"{name}: no log line for the four clubs of {s}")


def check_s2019(lines, log, fails, name):
    need_four(lines, "spa_super", 2019, "start", fails, name)


def need_groups(lines, file, season, want, fails, name):
    """want: stages, groups, group clubs (None: any), knockout clubs, knockout rounds at the end of the season."""
    got = lines_of(lines, file, season, "end")
    if not got:
        fails.append(f"{name}: no {file} line at the end of {season}")
    for x in got:
        have = (x["stages"], x["groups"], x["group_clubs"] if want[2] is not None else None, x["ko_clubs"],
                x["ko_rounds"])
        if have != want:
            fails.append(f"{name}: {file} at the end of {season}: stages, groups, group clubs, knockout clubs and "
                         f"rounds {have}, expected {want}")


def need_log(log, text, fails, name):
    if text not in log:
        fails.append(f"{name}: no log line \"{text}\"")


def need_made(lines, file, fails, name, clubs=None):
    got = [x for x in lines if x["file"] == file]
    if not got:
        fails.append(f"{name}: no {file} line in the report")
    for x in got:
        if clubs is not None and x["clubs"] not in (0, clubs):
            fails.append(f"{name}: {file} at the {x['phase']} of {x['season']}: {x['clubs']} clubs, expected {clubs}")


def check_g2001(lines, log, fails, name):
    need_groups(lines, "eng_cc_cup", 2001, (17, 16, None, 32, 5), fails, name)
    need_winner(lines, "eng_cc_cup", 2001, fails, name)
    need_log(log, "cup groups: test_eng_cc_cup 2001 with 16 groups after 0 rounds", fails, name)


def check_e2016(lines, log, fails, name):
    need_groups(lines, "eng_auto_cup", 2016, (17, 16, 56, 32, 5), fails, name)
    need_winner(lines, "eng_auto_cup", 2016, fails, name)


def check_n2008(lines, log, fails, name):
    need_made(lines, "new_nor_super", fails, name, 2)
    for y in (2009, 2010):
        if not any(x["file"] == "new_nor_super" and (x["winner"] != "-" or x["winner_next"] != "-") and
                   y in (x["season"], x["season"] + 1) for x in lines):
            fails.append(f"{name}: new_nor_super has no winner of {y} in the history")
    need_log(log, "new cups: new_nor_super is not held in 2011", fails, name)


def check_c2007(lines, log, fails, name):
    need_made(lines, "new_conf_cup", fails, name)
    need_winner(lines, "new_conf_cup", 2007, fails, name)


def check_c2008_off(lines, log, fails, name):
    if "message box from" in log:
        fails.append(f"{name}: a message box: {log[log.index('message box from'):].splitlines()[0]}")
    need_log(log, "new cups: new_conf_cup 2024: 5 rounds for 22 clubs", fails, name)
    need_winner(lines, "new_conf_cup", 2024, fails, name)


def check_t2010(lines, log, fails, name):
    need_log(log, "cup groups: tur_cup 2010 with 4 groups after 3 rounds", fails, name)
    need_made(lines, "new_tur_super", fails, name, 2)
    # Changing rules off: the edition that the row of 2010/11 holds, the Russian Super Cup of March 2011.
    need_log(log, "new cups: new_rus_super 2011 with 2 clubs", fails, name)
    need_made(lines, "new_all_ireland", fails, name, 12)


# Item 145: the group editions of the All-Ireland cup, by season: clubs, clubs of each league at least (the cup
# winners and the holders may come from either), groups, the knockout clubs and rounds, the mix of each group (2:
# two clubs of each league, None: free).
SETANTA = {2004: (6, (2, 2), 2, 2, 1, None), 2005: (8, (4, 4), 2, 4, 2, 2), 2006: (8, (4, 4), 2, 4, 2, 2),
           2007: (8, (4, 4), 2, 4, 2, 2), 2009: (9, (4, 4), 3, 4, 2, None)}


def setanta_clubs(log, season):
    """The clubs of an edition from the last "club" lines of its pick (the pick of the draw day): a list of (name,
    division 1, 2 or 0, last place) in the order of the list."""
    out = []
    for m in re.finditer(rf"new cups: new_all_ireland {season} club (\d+): (.+) \(division (\d), last place "
                         rf"(-?\d+)\)", log):
        if m.group(1) == "1":
            out = []
        out.append((m.group(2), int(m.group(3)), int(m.group(4))))
    return out


def check_a2003(lines, log, fails, name):
    if "message box from" in log:
        fails.append(f"{name}: a message box: {log[log.index('message box from'):].splitlines()[0]}")
    for season, (clubs, least, groups, ko, ko_rounds, mix) in SETANTA.items():
        need_groups(lines, "new_all_ireland", season, (groups + 1, groups, clubs, ko, ko_rounds), fails, name)
        need_winner(lines, "new_all_ireland", season, fails, name)
        listed = setanta_clubs(log, season)
        picked = {c: d for c, d, _ in listed}
        per = [sum(1 for d in picked.values() if d == k) for k in (1, 2)]
        if len(picked) != clubs or per[0] < least[0] or per[1] < least[1]:
            fails.append(f"{name}: new_all_ireland {season}: {len(picked)} clubs, {per} of the two leagues, expected "
                         f"{clubs} with at least {least}")
        # The clubs taken by place come first in the list, league by league: places 1 to n of each league.
        for k in (1, 2):
            places = [p for _, d, p in listed if d == k][:least[k - 1]]
            if places != list(range(1, least[k - 1] + 1)):
                fails.append(f"{name}: new_all_ireland {season}: the places of league {k} are {places}, expected 1 to "
                             f"{least[k - 1]}")
        drawn = re.findall(rf"cup groups: new_all_ireland {season}/\d\d group (\d+): (.+)", log)
        if len(drawn) != groups:
            fails.append(f"{name}: new_all_ireland {season}: {len(drawn)} groups drawn, expected {groups}")
        for g, members in drawn:
            got = [picked.get(c, -1) for c in members.split(", ")]
            if mix and (got.count(1) != mix or got.count(2) != mix):
                fails.append(f"{name}: new_all_ireland {season} group {g}: {members} is not {mix} of each league")
    need_log(log, "new cups: new_all_ireland is not held in 2008", fails, name)
    need_log(log, "new cups: new_all_ireland 2010 with 12 clubs", fails, name)
    need_winner(lines, "new_all_ireland", 2010, fails, name)
    if not any(x["file"] == "new_all_ireland" and x["season"] == 2010 and x["phase"] == "end" and x["groups"] == 0
               for x in lines):
        fails.append(f"{name}: new_all_ireland 2010/11 is not a knockout at the end of the season")
    need_log(log, "news 2004/05: The Setanta Cup is played for the first time", fails, name)
    need_log(log, "news 2009/10: The Setanta Sports Cup is played again", fails, name)


def check_a2009(lines, log, fails, name):
    if "message box from" in log:
        fails.append(f"{name}: a message box: {log[log.index('message box from'):].splitlines()[0]}")
    for season in (2010, 2011, 2012, 2013):
        each = 6 if season < 2013 else 4            # 12 clubs to 2012/13, 8 clubs (no byes) in 2013/14
        need_log(log, f"new cups: new_all_ireland {season}: clubs picked again on the draw day", fails, name)
        listed = setanta_clubs(log, season)
        for k in (1, 2):
            places = sorted(p for _, d, p in listed if d == k)
            if places != list(range(1, each + 1)):
                fails.append(f"{name}: new_all_ireland {season}: the places of league {k} are {places}, expected 1 to "
                             f"{each}")
        # The byes (the last 4 of the 12 slots): the first two of each league, the champions among them.
        byes = sorted((d, p) for _, d, p in listed[8:])
        if each == 6 and byes != [(1, 1), (1, 2), (2, 1), (2, 2)]:
            fails.append(f"{name}: new_all_ireland {season}: the byes are {byes} (league, place), expected the first two "
                         f"of each league")
        # Open item 148, the 2011 to 2014 rule: each tie of the first round is one club of each league, and the ties
        # hold the 8 clubs of the first round (the first 8 of the list). The game logs the ties after Cup::draw_round
        # 0x51c410 has run, so a later shuffle would show here.
        need_log(log, f"new cups: new_all_ireland {season}: first round paired by league", fails, name)
        league = {c: d for c, d, _ in listed}
        ties = re.findall(rf"new cups: new_all_ireland {season} tie \d+: (.+) v (.+)", log)[-4:]
        if len(ties) != 4:
            fails.append(f"{name}: new_all_ireland {season}: {len(ties)} first-round ties logged, expected 4")
        for h, a in ties:
            if sorted((league.get(h, -1), league.get(a, -1))) != [1, 2]:
                fails.append(f"{name}: new_all_ireland {season}: the tie {h} v {a} is not one club of each league")
        if ties and sorted(c for t in ties for c in t) != sorted(c for c, _, _ in listed[:8]):
            fails.append(f"{name}: new_all_ireland {season}: the ties do not hold the 8 clubs of the first round")
        need_winner(lines, "new_all_ireland", season, fails, name)


def check_r2002(lines, log, fails, name):
    need_log(log, "new cups: new_rus_lge_cup 2003 with 16 clubs", fails, name)
    need_log(log, "new cups: new_rus_super 2003 with 2 clubs", fails, name)


# name: (start year, database folder, changing rules, last season, nations, start run, time limit in seconds, check,
# more: "env" more environment variables, "extra" the lower divisions of rules_report_check.py)
RUNS = {
    "i2023": (2023, os.path.join(DB, "October 2023"), "on", 2024, ["Italy"], False, 3600, check_i2023, {}),
    "s2019": (2019, os.path.join(DB, "October 2019"), "off", 2019, ALL_BUT_FRANCE, True, 1800, check_s2019, {}),
    "g2001": (2001, None, "on", 2001, ["England"], False, 3600, check_g2001, {"env": {"RECOMP_CUP_GROUPS_TEST": "1"}}),
    "e2016": (2016, os.path.join(DB, "October 2016"), "off", 2016, ["England"], False, 3600, check_e2016, {}),
    "e2016-read": (2016, os.path.join(DB, "October 2016"), "off", 2016, ["England"], False, 3600, check_e2016,
                   {"env": {"RECOMP_CUP_GROUPS_READ": "1"}}),
    "x2016-read": (2016, os.path.join(DB, "October 2016"), "off", 2016, ALL_BUT_FRANCE, False, 7200, check_x2016_read,
                   {"env": {"RECOMP_CUP_GROUPS_READ": "1"}}),
    "n2008": (2008, os.path.join(DB, "September 2008"), "on", 2010, ["Norway"], False, 5400, check_n2008, {}),
    "c2007": (2007, os.path.join(DB, "November 2007"), "off", 2007, ["England"], False, 3600, check_c2007,
              {"extra": ["conference"]}),
    "c2008-off": (2008, None, "off", 2024, ["England"], False, 2700, check_c2008_off,
                  {"env": {"RECOMP_TIER_A": "1"}}),
    "t2010": (2010, os.path.join(DB, "September 2010 ODB"), "off", 2010, ALL_BUT_FRANCE, True, 1800, check_t2010,
              {}),
    "r2002": (2002, None, "off", 2002, ALL_BUT_FRANCE, True, 1800, check_r2002, {}),
    "a2009": (2009, None, "on", 2013, ["Northern Ireland", "Republic of Ireland"], False, 3600, check_a2009,
              {"env": {"RECOMP_TIER_A": "1"}}),
    "a2003": (2003, None, "on", 2010, ["Northern Ireland", "Republic of Ireland"], False, 3600, check_a2003,
              {"env": {"RECOMP_TIER_A": "1"}}),
}


# The lead's hold file: while it exists a test game starts only below 8 games and load 16 (the lead's note in it).
HOLD = "/Users/max/Projects/cm0102/run-logs/HOLD"


def wait_for_room():
    """Starts a game only when fewer than 15 games run on this Mac and the load of the last minute is below 24."""
    while True:
        games = subprocess.run(["pgrep", "-x", "cm0102_recomp"], capture_output=True, text=True).stdout.split()
        builds = subprocess.run(["pgrep", "-f", "cmake --build"], capture_output=True, text=True).stdout.split()
        busy = len(builds) >= 2 and len(games) >= 10                # the lead's rule of 2026-10-20 11:35
        load = os.getloadavg()[0]
        if os.path.exists(HOLD):                                    # the lead's note: below 8 games and load 16
            if len(games) < 7 and load < 16:
                return
        elif len(games) < 15 and load < 24 and not busy:
            return
        time.sleep(20)


def check_run(name, program, game, keep, reuse):
    year, database, rules, last, nations, start_run, limit, check, more = RUNS[name]
    work = os.path.join(keep, name) if keep else tempfile.mkdtemp(prefix=f"cm0102-cups-more-{name}-")
    report = os.path.join(work, "report.tsv")
    if reuse and os.path.exists(report):
        code = 0
    elif os.path.exists(work) and keep:
        return [f"{name}: the folder {work} exists; use --reuse to check it again, or another --keep folder"]
    else:
        wait_for_room()
        env = dict({"RECOMP_SEASON_CHANGE_LOG": "1"}, **more.get("env", {}))
        code, report = rrc.run(program, game, last, year, database, nations, more.get("extra", []), work, limit,
                               {"changing_rules": rules}, env)
    if code != 0 and not (start_run and code == "timeout"):
        return [f"{name}: the run ended with code {code} ({work})"]
    if not os.path.exists(report):
        return [f"{name}: no report ({work})"]
    fails = []
    seasons, _ = rrc.read_report(report)
    if start_run and not any(x["season"] == year and x["phase"] == "start" for x in seasons):
        fails.append(f"{name}: the report has no start of {year}/{(year + 1) % 100:02d}")
    if not start_run and not any(x["season"] == last and x["phase"] == "end" for x in seasons):
        fails.append(f"{name}: the report has no end of {last}/{(last + 1) % 100:02d} (the run stopped early)")
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    lines = cupmore(report)
    check(lines, log, fails, name)
    print(f"{name}: {len(lines)} cupmore lines, {len(fails)} problems ({work})")
    return fails


def check_e2016_load(lines, log, fails, name):
    need_log(log, "cup groups: eng_auto_cup 2016/17: knockout of 32 clubs drawn", fails, name)
    need_groups(lines, "eng_auto_cup", 2016, (17, 16, 56, 32, 5), fails, name)
    need_winner(lines, "eng_auto_cup", 2016, fails, name)


def check_i2023_load(lines, log, fails, name):
    need_winner(lines, "ita_super", 2023, fails, name)
    for x in lines_of(lines, "ita_super", 2023, "end"):
        if x["clubs"] != 4 or x["rounds"] != 2:
            fails.append(f"{name}: ita_super after the load: {x['clubs']} clubs, {x['rounds']} rounds")


def check_n2008_load(lines, log, fails, name):
    need_made(lines, "new_nor_super", fails, name, 2)
    if not any(x["file"] == "new_nor_super" and x["phase"] == "end" and x["season"] == 2008 and
               x["winner_next"] != "-" for x in lines):
        fails.append(f"{name}: new_nor_super has no winner of 2009 at the end of 2008/09 after the load")


# name: (start year, database folder, nations, days to the save, season of the end report, a log line of the first
# program that shows the save is inside the competition, check of the loaded program)
def check_a2004_wait(lines, log, fails, name):
    need_log(log, "cup groups: new_all_ireland 2004/05: 6 clubs drawn in 2 groups", fails, name)
    need_log(log, "cup groups: new_all_ireland 2004/05: knockout of 2 clubs drawn", fails, name)
    need_groups(lines, "new_all_ireland", 2004, (3, 2, 6, 2, 1), fails, name)
    need_winner(lines, "new_all_ireland", 2004, fails, name)


def check_a2004_mid(lines, log, fails, name):
    need_log(log, "cup groups: new_all_ireland 2004/05: knockout of 2 clubs drawn", fails, name)
    need_groups(lines, "new_all_ireland", 2004, (3, 2, 6, 2, 1), fails, name)
    need_winner(lines, "new_all_ireland", 2004, fails, name)


def check_a2007_gap(lines, log, fails, name):
    need_log(log, "new cups: new_all_ireland resumes in 2009", fails, name)
    need_log(log, "cup groups: new_all_ireland 2009/10: 9 clubs drawn in 3 groups", fails, name)
    need_groups(lines, "new_all_ireland", 2009, (4, 3, 9, 4, 2), fails, name)
    need_winner(lines, "new_all_ireland", 2009, fails, name)


IRELANDS = ["Northern Ireland", "Republic of Ireland"]
LOAD_RUNS = {
    # Item 145, the All-Ireland cup: saved before its draw day (October 2004, the draw on 13 December 2004), in the
    # middle of its groups (late March 2005) and in the season with no edition (August 2008).
    "a2004-load-wait": (2004, None, IRELANDS, 100, 2004, "new cups: new_all_ireland 2004 with 6 clubs",
                        check_a2004_wait),
    "a2004-load-mid": (2004, None, IRELANDS, 262, 2004,
                       "cup groups: new_all_ireland 2004/05: 6 clubs drawn in 2 groups", check_a2004_mid),
    "a2007-load-gap": (2007, None, IRELANDS, 400, 2009, "new cups: new_all_ireland is not held in 2008",
                       check_a2007_gap),
    "e2016-load": (2016, os.path.join(DB, "October 2016"), ["England"], 100, 2016,
                   "cup groups: eng_auto_cup 2016/17: 56 clubs drawn in 16 groups", check_e2016_load),
    "i2023-load": (2023, os.path.join(DB, "October 2023"), ["Italy"], 189, 2023,
                   "super cups: ita_super 2023 with four clubs", check_i2023_load),
    "n2008-load": (2008, os.path.join(DB, "September 2008"), ["Norway"], 150, 2008,
                   "new cups: new_nor_super 2009 with 2 clubs", check_n2008_load),
}


def load_run(name, program, game, keep):
    """The first program saves inside the competition, the second loads the save and plays to the end report."""
    import shutil
    sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
    import fixtures
    import smoke_run
    year, database, nations, days, last, before, check = LOAD_RUNS[name]
    work = os.path.join(keep, name) if keep else tempfile.mkdtemp(prefix=f"cm0102-cups-more-{name}-")
    if os.path.exists(work) and keep:
        return [f"{name}: the folder {work} exists; use another --keep folder"]
    wait_for_room()
    code, _ = rrc.run(program, game, last, year, database, nations, [], work, 3600, {"changing_rules": "on"},
                      {"RECOMP_SEASON_CHANGE_LOG": "1", "RECOMP_SAVE_AFTER_DAYS": str(days)})
    first = os.path.join(work, "user", smoke_run.SAVE_NAME)
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    if code != 0 or not os.path.exists(first):
        return [f"{name}: the first program made no save (code {code}, {work})"]
    if before not in log:
        return [f"{name}: the save is not inside the competition: no line \"{before}\" before it ({work})"]
    again = os.path.join(work, "again")
    user = os.path.join(again, "user")
    os.makedirs(user)
    shutil.copyfile(first, os.path.join(user, "inside.sav"))
    # The settings of the first program (the database folder too): the load needs the same data.
    shutil.copyfile(os.path.join(work, "user", "settings.txt"), os.path.join(user, "settings.txt"))
    if os.path.isdir(os.path.join(work, "user", "Databases")):
        shutil.copytree(os.path.join(work, "user", "Databases"), os.path.join(user, "Databases"))
    report = os.path.join(again, "report.tsv")
    table = os.path.join(os.path.dirname(os.path.abspath(program)), "seasons", "rules.tsv")
    wait_for_room()
    # A loaded game keeps its holiday, so the last step (wait first-news) is left out, as in club_world_check.py.
    steps = smoke_run.scaled_steps("classic", again, fixtures.LOAD_STEPS[:-1] + ["3300000 quit"])
    try:
        env = {"RECOMP_SEASON_JUMP": str(last), "RECOMP_RULES_REPORT": report, "RECOMP_RULES_TABLE": table,
               "RECOMP_SEASON_CHANGE_LOG": "1", "RECOMP_SCREEN_LOG": "1"}
        r, _ = fixtures.run(program, game, "classic", user, steps, env, 3600)
    except subprocess.TimeoutExpired:
        return [f"{name}: the loaded program did not end in time ({again})"]
    with open(os.path.join(again, "stderr.txt"), "w") as f:
        f.write(r.stderr)
    fails = []
    if r.returncode != 0:
        fails.append(f"{name}: the loaded program ended with code {r.returncode} ({again})")
    if "recomp: message box" in r.stderr:
        fails.append(f"{name}: an error box after the load ({again}/stderr.txt)")
    if not os.path.exists(report):
        return fails + [f"{name}: the loaded program wrote no report ({again})"]
    seasons, _ = rrc.read_report(report)
    if not any(x["season"] == last and x["phase"] == "end" for x in seasons):
        fails.append(f"{name}: the loaded program's report has no end of {last}/{(last + 1) % 100:02d}")
    lines = cupmore(report)
    check(lines, r.stderr, fails, name)
    print(f"{name}: {len(lines)} cupmore lines after the load, {len(fails)} problems ({work})")
    return fails


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    # A stop of this script stops its game too: subprocess.run kills the game when an exception leaves it.
    def stop(*_):
        raise KeyboardInterrupt()
    for sig in (signal.SIGTERM, signal.SIGHUP, signal.SIGINT):
        signal.signal(sig, stop)
    only = rrc.arg(argv, "--only", ",".join(list(RUNS) + list(LOAD_RUNS))).split(",")
    keep = os.path.abspath(rrc.arg(argv, "--keep")) if "--keep" in argv else None
    fails = []
    for name in [n for n in RUNS if n in only]:
        fails += check_run(name, program, game, keep, "--reuse" in argv)
    for name in [n for n in LOAD_RUNS if n in only]:
        fails += load_run(name, program, game, keep)
    for t in fails:
        print("FAIL  " + t)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
