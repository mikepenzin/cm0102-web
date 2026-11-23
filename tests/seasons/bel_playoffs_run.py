#!/usr/bin/env python3
"""The Belgian play-offs of issue 150 play in their real seasons (season_bel_playoffs.c, season_bel_plan.c).

Usage:
    python3 tests/seasons/bel_playoffs_run.py <cm0102_recomp> <game folder> [--only off,save-group,save-tie,y2005,...]
        [--jobs n] [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py with Belgium alone, changing rules on, over the first
season and its season change. The runs:
    off     2010/11 with RECOMP_BEL_PLAYOFFS=off: no round is played (proves that the test sees the fault)
    y2005   2005/06: the 17th plays the final round, no Play-off II final
    y2007   2007/08: no First Division club in the final round
    y2008   2008/09: the 15th and 16th play the final round
    y2009   2009/10: the 15th plays the final round, a final and a Europa match of two legs
    y2010   2010/11: the winner of the relegation play-off plays the final round, two legs
    y2014   2014/15: as 2010/11, the last season of the relegation play-off
    y2015   2015/16: a final and a Europa match of two legs, no final round, the champion alone goes up
    y2016   2016/17: groups of six with three First Division B clubs, a final and a Europa match of one match; the
            winner of First Division B's final alone goes up
    y2018   2018/19: as 2016/17, the last season of the form
    save-group  2016/17: a save during the groups of Play-off II, loaded and played out
    save-tie    2010/11: a save during the Play-off II final, loaded and played out
All runs end with code 0 and show no message box of the game.
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

TAG = "recomp: bel playoffs: "
MOST_LATE = 14
#        year: (final legs, Europa legs, First Division clubs in the final round, groups)
WANT = {2005: (0, 0, 1, 0), 2007: (0, 0, 0, 0), 2008: (0, 0, 2, 0), 2009: (2, 2, 1, 0), 2010: (2, 2, 1, 0),
        2014: (2, 2, 1, 0), 2015: (2, 2, 0, 0), 2016: (1, 1, 0, 2), 2018: (1, 1, 0, 2)}
EXCHANGE = re.compile(r"league sizes: bel_first \([^)]*\) down (\d+), bel_second \([^)]*\) up (\d+)")


def faults(year, log):
    """What the log of the season that starts in year lacks. Pure, so that main can test it on made-up logs."""
    legs, europa, top, groups = WANT[year]
    lines = [x.split(TAG, 1)[1] for x in log.splitlines() if TAG in x and x.split(TAG, 1)[1].startswith(f"{year}: ")]
    out = []
    finals = [x for x in lines if "Play-off II final" in x]
    if len(finals) != (1 if legs else 0) or (legs and f"{legs} leg(s)" not in finals[0]):
        out.append(f"{year}: Play-off II final lines {finals}, want {legs} leg(s)")
    matches = [x for x in lines if "Europa League play-off" in x]
    dropped = any("the Play-off II winner has the cup winner's place" in x for x in lines)
    if europa and not dropped and (len(matches) != 1 or f"{europa} leg(s)" not in matches[0]):
        out.append(f"{year}: Europa match lines {matches}, want {europa} leg(s)")
    if not europa and matches:
        out.append(f"{year}: a Europa match in a season with none: {matches}")
    entered = [x for x in lines if "plays the final round" in x]
    if len(entered) != top:
        out.append(f"{year}: {len(entered)} First Division clubs in the final round, want {top}")
    if top and not any("the final round is won by" in x for x in lines):
        out.append(f"{year}: no result of the final round")
    made = [x for x in lines if re.search(r": group [AB]: ", x)]
    if len(made) != groups or any(len(x.split(": ", 2)[2].split(", ")) != 6 for x in made):
        out.append(f"{year}: group lines {made}, want {groups} groups of six")
    out += [f"{year}: {x}" for x in lines if "no more stages" in x and "cup winner's place" not in x]
    if europa and not dropped and not any("European place" in x for x in lines):
        out.append(f"{year}: no line says who has the European place")
    moved = [x for x in lines if "the First Division next season" in x]
    if top and len(moved) < top:
        out.append(f"{year}: {len(moved)} lines of the clubs after the final round, want at least {top}")
    out += [f"{year}: {x}" for x in moved if "(fault)" in x]
    for win, lose in re.findall(r"places for Europe: .* (\d+), .* (\d+)$", "\n".join(lines), re.M):
        if int(win) >= int(lose):
            out.append(f"{year}: the Europa match's winner has place {win}, the loser {lose}")
    if year == 2015 and not any("no final round this season" in x for x in lines):
        out.append("2015: a final round in the season with none")
    # As many clubs go up as come down, but in 2008/09, when the First Division shrinks from 18 clubs to 16.
    moved = EXCHANGE.search(log)
    if not moved:
        out.append(f"{year}: no line of the clubs that the First and Second Divisions exchange")
    elif year != 2008 and moved.group(1) != moved.group(2):
        out.append(f"{year}: {moved.group(1)} clubs go down and {moved.group(2)} go up")
    # The safety net of the season change must never act (season_playoffs.c entry_unfinished).
    out += [x for x in log.splitlines() if "recomp: season change net" in x]
    # The play-offs end near the season change: 2016/17 first waited 34 days, to 23 July.
    for days in re.findall(r"changes its season on day \d+ of \d+, (\d+) days late", log):
        if int(days) > MOST_LATE:
            out.append(f"{year}: the season change came {days} days late, at most {MOST_LATE}")
    return out


def jump(name, program, game, work, year, env):
    code, _ = rrc.run(program, game, year + 1, year, None, ["Belgium"], [], work, 21600, {"changing_rules": "on"},
                      dict({"RECOMP_SIZE_LOG": "1"}, **env))
    fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    return fails + [f"{name}: message box {b}" for b in found], log


def run_year(year):
    def run(program, game, work):
        fails, log = jump(f"y{year}", program, game, work, year, {})
        return fails + faults(year, log)
    return run


def run_off(program, game, work):
    fails, log = jump("off", program, game, work, 2010, {"RECOMP_BEL_PLAYOFFS": "off"})
    return fails + [f"off: a round was played: {x}" for x in log.splitlines() if TAG in x]


def run_save(kind, year):
    """Leg 0 saves while the stage is played (RECOMP_TEST_BEL_SAVE); leg 1 loads the save and plays the season out."""
    def run(program, game, work):
        import smoke_run
        import soak_run
        leg0, leg1 = os.path.join(work, "leg0"), os.path.join(work, "leg1")
        code, _ = rrc.run(program, game, year + 1, year, None, ["Belgium"], [], leg0, 21600, {"changing_rules": "on"},
                          {"RECOMP_SIZE_LOG": "1", "RECOMP_TEST_BEL_SAVE": kind}, stop_on_box=True)
        save = os.path.join(leg0, "user", smoke_run.SAVE_NAME)
        if code != 0 or not os.path.exists(save):
            return [f"save-{kind}: leg 0 ended with code {code}, save {os.path.exists(save)}"]
        soak_run.LEG_ENV["RECOMP_SIZE_LOG"] = "1"
        o = {"program": program, "game": game, "rules": "on", "database": None, "nations": ["Belgium"], "extra": [],
             "start": year, "last": year + 1, "seed": 1, "timeout": 21600}
        code, _took, box = soak_run.load_leg(o, leg1, save, 0)
        if code != 0 or box:
            return [f"save-{kind}: leg 1 ended with code {code}, box {box}"]
        log = open(os.path.join(leg1, "stderr.txt"), encoding="latin-1").read()
        done = [x for x in log.splitlines() if f"{TAG}{year}: " in x and "European place" in x]
        return [] if done else [f"save-{kind}: leg 1 did not play the Europa match of {year} out"]
    return run


RUNS = {"off": run_off, "save-group": run_save("group", 2016), "save-tie": run_save("tie", 2010),
        **{f"y{y}": run_year(y) for y in WANT}}


def self_test():
    good = "\n".join(TAG + x for x in ["2010: Play-off II final, 2 leg(s): A v B",
                                       "2010: Europa League play-off, 2 leg(s): A v C (cup winner's place 0)",
                                       "2010: D of the First Division plays the final round",
                                       "2010: the final round is won by E (a second-division club goes up)",
                                       "2010: C keeps its European place",
                                       "2010: D leaves the First Division next season"]) + \
        "\nrecomp: league sizes: bel_first (A) down 2, bel_second (B) up 2, pairs 1"
    bad = []
    if faults(2010, good):
        bad.append(("good log", faults(2010, good)))
    for broken in (good.replace("2 leg(s): A v B", "1 leg(s): A v B"), good.replace("plays the final round", "x"),
                   good.replace("Europa League play-off", "x"), good.replace("is won by", "x"),
                   good.replace("keeps its European place", "x"), good.replace("next season", "x"),
                   good + "\n" + TAG + "2010: D leaves the First Division next season (fault)",
                   good + "\nrecomp: season change net: entry 10 waits for bel_first",
                   good.replace("down 2, bel_second (B) up 2", "down 1, bel_second (B) up 2"),
                   good + "\n" + TAG + "2010: day 9: places for Europe: A 9, C 4",
                   good + "\nrecomp: playoffs: entry 10 changes its season on day 204 of 2017, 34 days late"):
        if not faults(2010, broken):
            bad.append(("broken log passes", broken))
    return bad


def main(argv):
    bad = self_test()
    if bad:
        print("self-test FAIL", bad)
        return 1
    if len(argv) < 3:
        print(__doc__)
        return 0 if "--self-test" in argv else 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="bel-playoffs-")
    fails = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        futures = {n: ex.submit(RUNS[n], program, game, os.path.join(keep, n)) for n in only}
        for n in only:
            got = futures[n].result()
            print(f"{n}: {'PASS' if not got else 'FAIL'}", flush=True)
            fails += got
    for f in fails:
        print("  " + f)
    print("PASS" if not fails else "FAIL")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
