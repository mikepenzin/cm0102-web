#!/usr/bin/env python3
"""The free places of the access lists in a game with fewer UEFA nations (0.6.2 open item 94).

Usage:
    python3 tests/seasons/uefa_free_places_run.py <cm0102_recomp> <game folder> [--keep <folder>]

The run loads a save of June 2026 (the original data 3.9.68 jumped to 2025/26, England and Spain, changing rules
on, Tier A, seed 1: test-fixtures/uefa-free-places-2026.sav, or RECOMP_FREE_PLACES_SAVE). Leg 1 plays 25 days in
Tier A, past the UEFA places of day 171, and saves. Leg 2 loads that save and plays on past the league phase draws
of the three cups (about 1 September). The original data has 52 UEFA nations and the access list of 2026/27 has 55
ranks, so the first qualifying rounds of the Champions League and the Conference League have free places.

Max, 3 November 2026: do what UEFA does (open item 94d). A vacancy moves up its path, and a gap at the bottom stays
empty: the round has fewer clubs, and no club of another association takes it (UEFA Champions League regulations
2026/27, Art. 3.04 to 3.07 and 4.05). The cup code plays only even rounds, so the best-ranked new club of an odd
round enters one round later (a bye, as the rebalanced access lists do). The checks:
- leg 1: no free place of the Champions League or the Conference League goes to a club, and the log shows the
  places that stay empty for both; each nation has no more Champions League clubs than its places in the list
  plus its title holders and European Performance Spots (the seeding's log, and the clubs with a Champions League
  place in the save); the Champions League clubs of each nation, title holders apart, are the first clubs of its
  top division of the last season, with no club left out (item 94b: on main ec04eb89 Spain had 1 to 4 and 8 to 10);
- leg 2: no box, every qualifying layout is made (no odd round, no short group stage), and the league phases of
  the three cups are drawn with 36 clubs each.
On d3deb121 (item 94c) the free places went to the best-ranked nations: Spain took 3 Champions League places and
ranks 1 to 5 one Conference League place each. A run takes about 5 to 10 minutes.
"""
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))
import history_check as hc  # noqa: E402
import rules_report_check as rrc  # noqa: E402
import soak_run  # noqa: E402

FIXTURES = "/Users/max/Projects/cm0102/test-fixtures"
DAYS = 25                                       # leg 1: past the UEFA places of day 171
DAYS_DRAWS = 80                                 # leg 2: past the league phase draws (about 1 September)


def gaps(game):
    """[(nation, [(place, club)], missing places)] for each nation whose Champions League clubs skip a club of
    its top division of the last season."""
    rules = hc.Rules(os.path.join(HERE, "future", "competitions.tsv"),
                     os.path.join(HERE, "future", "history_names.tsv"))
    cl = {cid for cid, c in game.comps.items() if c["kind"] == "club" and rules.key(c["name"]) == "uefa_cl"}
    top_year = max((y for cid, y, *_ in game.rows if cid in cl), default=0)
    holders = {a for cid, y, a, *_ in game.rows
               if rules.key(game.comps[cid]["name"]) in ("uefa_cl", "uefa_el", "uefa_ecl") and y >= top_year}
    top = {}                                  # nation: the highest level of a division that holds clubs
    for div, last, mark, place in game.divisions.values():
        c = game.comps.get(last)
        if c and c["kind"] == "club" and c["nation"] >= 0:
            top[c["nation"]] = max(top.get(c["nation"], -1), c["level"])
    by, held = {}, {}
    for club, (div, last, mark, place) in game.divisions.items():
        c = game.comps.get(last)
        if not (c and c["nation"] in top and c["level"] == top[c["nation"]]):
            continue
        if club in holders:
            held.setdefault(c["nation"], set()).add(place)
        elif mark in cl:
            by.setdefault(c["nation"], []).append((place, game.clubs[club][0]))
    bad = []
    for nation, clubs in sorted(by.items()):
        places = sorted(p for p, _ in clubs)
        missing = sorted(set(range(1, places[-1] + 1)) - set(places) - held.get(nation, set()))
        if missing:
            bad.append((game.nations.get(nation, ("?",))[0], sorted(clubs), missing))
    return bad


FILLED = re.compile(r"recomp: uefa: (Champions League|Conference League) 2026/27: a free place \(entry code .*")
EMPTY = re.compile(r"recomp: uefa: (Champions League|Conference League) 2026/27: (\d+) free places \(entry code "
                   r"\d+\) stay empty")
PLACES = re.compile(r"recomp: uefa: places 2026/27 rank (\d+) (.*): (\d+) Champions League clubs, (\d+) in the list, "
                    r"(\d+) title holders and performance spots")
LAYOUT_FAULTS = re.compile(r"recomp: uefa: .*(not even|gives no layout|CL groups:|clubs, not 36|no draw found|"
                           r"cannot be balanced|no club to balance|has no new clubs)")


def places(game, log):
    """The faults of the rule for the free places in the seeding of 2026/27 (leg 1)."""
    bad = [f"a free place went to a club: {m.group(0)}" for m in FILLED.finditer(log)]
    empty = {}
    for comp, n in EMPTY.findall(log):
        empty[comp] = empty.get(comp, 0) + int(n)
    for comp in ("Champions League", "Conference League"):
        if not empty.get(comp):
            bad.append(f"no log line of a {comp} free place that stays empty")
    allowed = {}
    for rank, name, clubs, listed, extra in PLACES.findall(log):
        allowed[name] = int(listed) + int(extra)
        if int(clubs) > allowed[name]:
            bad.append(f"rank {rank} {name}: {clubs} Champions League clubs, {listed} in the list and {extra} title "
                       f"holders and performance spots")
    if not allowed:
        bad.append("no log line of the places of each nation")
    rules = hc.Rules(os.path.join(HERE, "future", "competitions.tsv"),
                     os.path.join(HERE, "future", "history_names.tsv"))
    cl = {cid for cid, c in game.comps.items() if rules.key(c["name"]) == "uefa_cl"}
    if not cl:
        return bad + ["no Champions League record in the save"]
    by = {}
    for club, (div, last, mark, place) in game.divisions.items():
        c = game.comps.get(last)
        if mark in cl and c:
            name = game.nations.get(c["nation"], ("?",))[0]
            by[name] = by.get(name, 0) + 1
    for name, n in sorted(by.items()):
        if n > allowed.get(name, 0):
            bad.append(f"{name}: {n} clubs with a Champions League place in the save, {allowed.get(name, 0)} allowed")
    return bad


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game_dir = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    work = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="uefa-free-")
    save = os.environ.get("RECOMP_FREE_PLACES_SAVE") or os.path.join(FIXTURES, "uefa-free-places-2026.sav")
    if not os.path.exists(save):
        print(f"FAIL  missing: {save}")
        return 1
    os.environ["RECOMP_TIER_A"] = "1"           # the game of the save was played in Tier A
    o = {"program": program, "game": game_dir, "rules": "on", "database": None, "seed": 1, "last": 2026,
         "timeout": 3600}
    fails = []
    out = os.path.join(work, "leg", "user", "samesave.sav")
    code, took, box = soak_run.load_leg(o, os.path.join(work, "leg"), save, DAYS)
    if code != 0 or box or not os.path.exists(out):
        fails.append(f"leg 1 ended with code {code}{', box ' + box if box else ''} and no save ({work})")
    else:
        game = hc.Game(out)
        if game.date < (2026, 172):
            fails.append(f"the save of day {game.date[1]} of {game.date[0]} is before the UEFA places")
        for nation, clubs, missing in gaps(game):
            fails.append(f"{nation}: Champions League places of {', '.join(f'{p} {n}' for p, n in clubs)}; "
                         f"no place for {', '.join(map(str, missing))} of the same table")
        with open(os.path.join(work, "leg", "stderr.txt"), encoding="latin-1") as f:
            log = f.read()
        for line in re.findall(r"recomp: uefa: .* 2026/27: (?:\d+ free places|a free place|.* enters at).*", log):
            print(line)
        fails += places(game, log)
        out2 = os.path.join(work, "draws", "user", "samesave.sav")
        code, took, box = soak_run.load_leg(o, os.path.join(work, "draws"), out, DAYS_DRAWS)
        log2 = ""
        if os.path.exists(os.path.join(work, "draws", "stderr.txt")):
            with open(os.path.join(work, "draws", "stderr.txt"), encoding="latin-1") as f:
                log2 = f.read()
        if code != 0 or box or not os.path.exists(out2):
            fails.append(f"leg 2 ended with code {code}{', box ' + box if box else ''} and no save ({work})")
        elif hc.Game(out2).date < (2026, 245):
            fails.append(f"leg 2 saved on day {hc.Game(out2).date[1]}, before the league phase draws")
        fails += [f"leg {k}: {m.group(0)}" for k, text in ((1, log), (2, log2)) for m in LAYOUT_FAULTS.finditer(text)]
        drawn = re.findall(r"recomp: uefa: league phase 2026/27: 36 clubs", log2)
        if len(drawn) != 3:
            fails.append(f"leg 2: {len(drawn)} league phases of 2026/27 drawn with 36 clubs, not 3")
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
