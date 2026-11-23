#!/usr/bin/env python3
"""The cups of eastern Europe and Asia follow the divisions (0.7.2 league sizes, slice 6, D14;
src/recomp_rt/win/season_cup_sizes_east.c).

Usage:
    python3 tests/seasons/cup_sizes_run.py <cm0102_recomp> <game folder> <run> [--keep <folder>] [--reuse]
        [--timeout <seconds>]
    python3 tests/seasons/cup_sizes_run.py --list
    python3 tests/seasons/cup_sizes_run.py --self-test

Each run starts a new game in its year with its nations and changing rules on, and plays two seasons (the season
jump of tests/seasons/rules_report_check.py), with RECOMP_SIZE_LOG=1. A division of a size that the leagues of the
game cannot hold yet (the league slices 11, 12 and 14 grow them) has it in the cup through the test seam
RECOMP_CUP_SIZE_TEST: the cup sees that many clubs in the division while it collects its entrants. The checks:
    box      no message box;
    clubs    each cup of the run logs its places ("recomp: cup sizes: <file> <year>: ..."): at its first collect each
             division of the run holds the clubs of RUNS, and every club of each division is in the cup (the K League
             Cup: 10 clubs, its best clubs out); a list of 44 or 10 pointers (GreCup, KorLeagueCup) is full;
    filled   a list with empty places gets a club for each place before the draw (season_cup_entrants.c);
    room     a division that takes the places left (J2 in the J.League Cup, the Polish second tier) has
             min(its clubs, places less the other division) in the cup;
    winner   each collect line names the last winner in the history: a later collect of the run names a winner of
             a later year, so the edition that the run played ended with a winner.
A run takes about an hour at load 15. Exit code 1 on a failure.
"""
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))

# The divisions of each cup, in its log line.
CUPS = {"gre_cup": ("gre_prm", "gre_second"), "cro_cup": ("cro_a1", "cro_a2a", "cro_a2b"),
        "rus_cup": ("rus_prm", "rus_first"), "tur_cup": ("tur_first", "tur_second"),
        "pol_lge_cup": ("pol_first", "pol_second"), "jap_j_cup": ("jap_j1", "jap_j2"),
        "jap_emp_cup": ("jap_j1", "jap_j2"), "kor_league_cup": ("kor_league",)}
POINTER_CUPS = {"gre_cup": 44, "kor_league_cup": 10}
# The division of a cup that takes the places that the other leaves (JapJCup 0x665aba, PolLgeCup 0x7cae01).
ROOM = {"jap_j_cup": ("jap_j2", "jap_j1"), "pol_lge_cup": ("pol_second", "pol_first")}

# run: (start year, last season, nations, seam, {division: clubs at the first collect}, what it shows).
RUNS = {
    "grown-2010": (2010, 2011, ["Greece", "Russia", "Turkey", "Japan", "South Korea"],
                   "gre_prm:+2,gre_second:+4,rus_first:+2,jap_j1:+2,jap_j2:+8,kor_league:+6",
                   {"gre_prm": 16, "gre_second": 18, "rus_first": 20, "tur_second": 17, "jap_j1": 18, "jap_j2": 20,
                    "kor_league": 16},
                   "grown: the Greek Super League 16 and second tier 18, the Russian First Division 20, J1 18 and J2 "
                   "20, "
                   "the K League 16 (rows of 2010); shrunk: the Turkish second tier 17 (row of 2010)"),
    "shrunk-2012": (2012, 2013, ["Russia", "Croatia", "Japan"], "jap_j2:-2",
                    {"rus_first": 17, "cro_a1": 12, "jap_j2": 10},
                    "shrunk: the Russian First Division 17 and the Croatian First Division 12 (rows of 2012), J2 10"),
    "poland-2007": (2007, 2008, ["Poland"], "pol_first:+2,pol_second:-3", {"pol_first": 18, "pol_second": 15},
                    "the Polish League Cup with an Ekstraklasa of 18 (grown) and a second tier of 15 (shrunk): one "
                    "place empty before the draw"),
    "mixed-2020": (2020, 2021, ["Croatia", "Turkey", "Greece"], "cro_a2a:+2,tur_first:+3,gre_second:-2",
                   {"cro_a1": 10, "cro_a2a": 18, "tur_first": 21, "gre_second": 12},
                   "grown: the Croatian second tier 18 and the Super Lig 21 (rows of 2020); shrunk: the Croatian First "
                   "Division 10 (row of 2020) and the Greek second tier 12 (row of 2020; the database holds 14 until "
                   "the club moves of slice 3)"),
}
NATION_CUPS = {"Greece": ["gre_cup"], "Croatia": ["cro_cup"], "Russia": ["rus_cup"], "Turkey": ["tur_cup"],
               "Poland": ["pol_lge_cup"], "Japan": ["jap_j_cup", "jap_emp_cup"], "South Korea": ["kor_league_cup"]}

LINE = re.compile(r"recomp: cup sizes: (\w+) (-?\d+): (\d+) of (\d+) places \(([^)]*)\)(.*); last winner (.*) "
                  r"\(history (-?\d+)\)$")
FILLED = re.compile(r"recomp: cup entrants: (\w+) \(.*\) (-?\d+): (\d+) of (\d+) places empty .*, (\d+) filled")


def left_out(more):
    """The K League clubs that the cup leaves out ("the 6 best by last place left out"): they are out of the
    division during the call, so the line's count is 10."""
    m = re.search(r"the (\d+) best by last place left out", more)
    return int(m.group(1)) if m else 0


def joined(more, div):
    """The clubs of no cup division that join a division for the call ("2 clubs of no cup division join jap_j2":
    the J.League Cup fills its 28 places)."""
    m = re.search(r"(\d+) clubs of no cup division join (\w+)", more)
    return int(m.group(1)) if m and m.group(2) == div else 0


def parse(log):
    """(collects {file: [(year, filled, places, {division: (in, clubs)}, more, winner, history year)]}, fills
    {file: [(year, empty, filled)]})."""
    collects, fills = {}, {}
    for line in log.split("\n"):
        m = LINE.search(line)
        if m:
            divs = {}
            for part in m.group(5).split(", "):
                name, _, val = part.partition(" ")
                a, _, b = val.partition("/")
                if a.isdigit() and b.isdigit():
                    divs[name] = (int(a), int(b))
            collects.setdefault(m.group(1), []).append((int(m.group(2)), int(m.group(3)), int(m.group(4)), divs,
                                                        m.group(6), m.group(7), int(m.group(8))))
            continue
        m = FILLED.search(line)
        if m:
            fills.setdefault(m.group(1), []).append((int(m.group(2)), int(m.group(3)), int(m.group(5))))
    return collects, fills


def faults(log, run):
    _year, _last, nations, _seam, expect, _what = RUNS[run]
    out = [x.strip() for x in log.split("\n") if "recomp: message box from" in x]
    collects, fills = parse(log)
    for nation in nations:
        for cup in NATION_CUPS[nation]:
            got = collects.get(cup, [])
            if not got:
                out.append(f"{cup}: no collect line")
                continue
            first = got[0][3]
            for div in CUPS[cup]:
                held = first.get(div, (0, 0))[1] + left_out(got[0][4]) - joined(got[0][4], div)
                if div in expect and held != expect[div]:
                    out.append(f"{cup} {got[0][0]}: {div} holds {held} clubs, want {expect[div]}")
            for year, filled, places, divs, more, _w, _h in got:
                for div, (inside, clubs) in divs.items():
                    want = clubs
                    if cup in ROOM and div == ROOM[cup][0]:
                        want = min(clubs, places - divs.get(ROOM[cup][1], (0, 0))[1])
                    if cup == "kor_league_cup" and inside != 10:
                        out.append(f"{cup} {year}: {inside} K League clubs in the cup, want 10")
                    if cup == "cro_cup" and "left out" in more:
                        continue
                    if inside != want:
                        out.append(f"{cup} {year}: {inside} of the {clubs} clubs of {div} in the cup, want {want}")
                if cup in POINTER_CUPS and filled != places:
                    out.append(f"{cup} {year}: {filled} of {places} places")
            for year, empty, filled in fills.get(cup, []):
                if filled != empty:
                    out.append(f"{cup} {year}: {empty} empty places, {filled} filled")
            first_history = got[0][6]
            if not any(h > first_history and w != "none" for *_x, w, h in got[1:]):
                out.append(f"{cup}: no later collect names a winner after history {first_history} "
                           f"({[(g[0], g[5], g[6]) for g in got]})")
    return out


def self_test():
    log = "\n".join([
        "recomp: cup sizes: gre_cup 2010: 44 of 44 places (gre_prm 16/16, gre_second 18/18), 10 lower picks; "
        "last winner AEK (history 2010)",
        "recomp: cup sizes: kor_league_cup 2010: 10 of 10 places (kor_league 10/10), the 6 best by last place left "
        "out; last winner none (history 0)",
        "recomp: cup sizes: rus_cup 2010: 47 of 48 places (rus_prm 16/16, rus_first 20/20); last winner X (history "
        "2010)",
        "recomp: cup entrants: rus_cup (Russian Cup) 2010: 1 of 48 places empty (the first is place 0), 1 filled",
        "recomp: cup sizes: tur_cup 2010: 63 of 64 places (tur_first 18/18, tur_second 16/17); last winner Y "
        "(history 2010)",
        "recomp: cup sizes: jap_j_cup 2010: 28 of 28 places (jap_j1 16/16, jap_j2 12/12), 2 clubs of no cup "
        "division join jap_j2; last winner Z (history 2009)",
        "recomp: cup sizes: gre_cup 2011: 44 of 44 places (gre_prm 16/16, gre_second 18/18), 10 lower picks; "
        "last winner Olympiakos (history 2011)",
        "recomp: cup sizes: rus_cup 2011: 48 of 48 places (rus_prm 16/16, rus_first 20/20); last winner X (history "
        "2010)",
        "recomp: cup sizes: jap_j_cup 2011: 28 of 28 places (jap_j1 18/18, jap_j2 9/20); last winner Q (history "
        "2010)",
    ])
    collects, fills = parse(log)
    bad = []
    if collects["gre_cup"][0][3] != {"gre_prm": (16, 16), "gre_second": (18, 18)} or collects["gre_cup"][1][5:] != (
            "Olympiakos", 2011):
        bad.append(("parse", collects["gre_cup"]))
    if fills.get("rus_cup") != [(2010, 1, 1)]:
        bad.append(("fills", fills))
    RUNS["self"] = (2010, 2011, ["Greece", "Russia", "Turkey", "South Korea", "Japan"], "",
                    {"gre_prm": 16, "rus_first": 22, "kor_league": 16, "jap_j2": 10}, "")
    f = faults(log, "self")
    del RUNS["self"]
    want = ["rus_cup 2010: rus_first holds 20 clubs, want 22", "tur_cup 2010: 16 of the 17 clubs of tur_second",
            "rus_cup: no later collect names a winner", "tur_cup: no later collect", "kor_league_cup: no later",
            "jap_j_cup 2011: 9 of the 20 clubs of jap_j2", "jap_emp_cup: no collect line"]
    for w in want:
        if not any(x.startswith(w) for x in f):
            bad.append(("missing", w, f))
    if any(x.startswith("gre_cup") or x.startswith("kor_league_cup 2010") or x.startswith("jap_j_cup 2010")
           or x.startswith("jap_j_cup:") for x in f):
        bad.append(("extra", f))
    for b in bad:
        print("FAIL self-test", b)
    print(("FAIL" if bad else "PASS") + " cup_sizes_run self-test")
    return 1 if bad else 0


def main(argv):
    if "--list" in argv:
        for k, (y, last, nations, seam, _e, what) in RUNS.items():
            print(f"{k}\t{y} to {last}\t{','.join(nations)}\t{seam}\t{what}")
        return 0
    if "--self-test" in argv:
        return self_test()
    if len(argv) < 3 or argv[2] not in RUNS:
        print(__doc__)
        return 2
    import rules_report_check as rrc
    program, game, run = os.path.abspath(argv[0]), os.path.abspath(argv[1]), argv[2]
    year, last, nations, seam, _e, _w = RUNS[run]
    keep = rrc.arg(argv, "--keep", None)
    timeout = int(rrc.arg(argv, "--timeout", "10800"))
    work = os.path.abspath(keep) if keep else tempfile.mkdtemp(prefix=f"cm0102-cup-sizes-{run}-")
    if "--reuse" not in argv:
        db = rrc.run_database(argv, year, os.path.join(work, "owndb"))
        env = {"RECOMP_SIZE_LOG": "1"}
        if seam:
            env["RECOMP_CUP_SIZE_TEST"] = seam
        code, _report = rrc.run(program, game, last, year, db, nations, [], work, timeout,
                                {"changing_rules": "on"}, env)
        if code != 0:
            print(f"FAIL {run}: the run ended with code {code} ({work})")
            return 1
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    fails = faults(log, run)
    for f in fails:
        print("FAIL", f)
    print(("FAIL" if fails else "PASS") + f" {run} ({work})")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
