#!/usr/bin/env python3
"""Every African nation plays CAF World Cup qualifying (0.6.2 item 199, src/recomp_rt/win/season_caf_pool.c).

Usage:
    python3 tests/seasons/caf_all_nations_run.py <cm0102_recomp> <game folder> [--keep <folder>] [--part a|b|c]

Real life: every CAF member that enters plays, the weakest in a two-leg first round (2006: 51 entered, 42 in the
first round; 2026: 54; en.wikipedia.org "<year> FIFA World Cup qualification (CAF)"). The original's pool builder
0x9106a0 reads at most 52 African nations and plays the 50 best, so in the 3.9.68 data (52 African nations)
Somalia and Equatorial Guinea are out in nearly every cycle. With changing rules on every African nation enters
from the 2006 cycle: 5 groups of 6 after a first round of 44 (2006 to 2022), 9 groups of 5 after a first round
of 14 (2026 on).

Part a: a new game, Tier A, England's leagues, seed 1, changing rules on, the 3.9.68 database, a 2001/02 start.
Leg 1 plays to 31 July 2004 (the 2006 cycle's first round in April, two group matchdays from June), a save; leg 2
loads it and plays 7 days (a save of groups of 6 loads). Part b: the save of 17 July 2021 of run set game
a2001to2300 (test-fixtures/caf-all-2021.sav, or RECOMP_CAF_ALL_SAVE) plays to 31 July 2024 (the 2026 cycle).
Part c: a new game of the December 2025 database (54 African nations, a 2025/26 start, changing rules on, Tier A,
seed 1) saves after 7 days, and a leg loads that save and plays 7 days: a pool of more than the original's 52
slots saves and loads (load_extra_data 0x910f00).

The checks: each leg ends with code 0 and no message box; in each cycle every African nation of the save plays a
match of the section from 1 January to 31 July of the campaign year, Somalia plays one two-leg first-round tie, the
first round has the plan's ties (22 in 2006, 7 in 2026), and the groups (the teams that meet from 1 June) are 5 of
6 (2006) or 9 of 5 (2026); part c's log names a pool of more than 52. On main before item 199, 50 nations play and
Somalia none: the test fails there.
"""
import datetime
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))
import history_check  # noqa: E402
import soak_run  # noqa: E402

START = datetime.date(2001, 7, 1)          # 3.9.68, 2001/02: the game's first day (soak_run's leg counts from it)
FIXTURES = "/Users/max/Projects/cm0102/test-fixtures"
SECTIONS = ("World Cup African Qualifying Section", "FIFA World Cup qualification (CAF)")   # base, rules-on name
NATION = "Somalia"
TIES = {2004: 22, 2024: 7}                 # 52 nations: 2 x (52 - 30) / 2 and 2 x (52 - 45) / 2
GROUPS = {2004: [6] * 5, 2024: [5] * 9}


def groups(save, campaign):
    """The sizes of the sets of teams that meet in the section from 1 June of campaign (the groups)."""
    game = history_check.Game(save)
    ids = {cid for cid, c in game.comps.items() if c["name"] in SECTIONS}
    lo = datetime.date(campaign, 6, 1).toordinal()
    parent = {}

    def root(t):
        while parent.setdefault(t, t) != t:
            t = parent[t]
        return t
    for cid in ids:
        for d, h, a in history_check.fixtures(game).get(cid, []):
            if d >= lo:
                parent[root(h)] = root(a)
    sizes = {}
    for t in parent:
        sizes[root(t)] = sizes.get(root(t), 0) + 1
    return sorted(sizes.values(), reverse=True)


def section(save, campaign):
    """(game, [(day, home id, away id)]) of the African section from 1 January to 31 July of campaign."""
    game = history_check.Game(save)
    ids = {cid for cid, c in game.comps.items() if c["name"] in SECTIONS}
    lo, hi = datetime.date(campaign, 1, 1).toordinal(), datetime.date(campaign, 7, 31).toordinal()
    return game, sorted(f for cid in ids for f in history_check.fixtures(game).get(cid, []) if lo <= f[0] <= hi)


def check_cycle(save, campaign):
    fails = []
    game, fx = section(save, campaign)
    nation = lambda t: game.club(t)[1]       # noqa: E731
    african = {nm for nm, conf in game.nations.values() if conf == "CAF"}
    played = {nation(t) for f in fx for t in f[1:]}
    first = [f for f in fx if f[0] < datetime.date(campaign, 6, 1).toordinal()]
    pairs = {}
    for _, h, a in first:
        pairs[frozenset((h, a))] = pairs.get(frozenset((h, a)), 0) + 1
    mine = [f for f in first if NATION in (nation(f[1]), nation(f[2]))]
    print(f"      {campaign + 2}: {len(african)} African nations, {len(played & african)} play, "
          f"{len(pairs)} first-round ties, {len(fx)} matches")
    for d, h, a in mine:
        print(f"      {datetime.date.fromordinal(d).isoformat()} {nation(h)} v {nation(a)}")
    missing = sorted(african - played)
    if missing:
        fails.append(f"{campaign + 2}: {len(missing)} African nations play no match: {', '.join(missing)}")
    if len(mine) != 2 or len({frozenset(f[1:]) for f in mine}) != 1:
        fails.append(f"{campaign + 2}: {NATION} plays {len(mine)} first-round matches, not one two-leg tie")
    bad = [sorted(nation(t) for t in p) for p, n in pairs.items() if n != 2]
    if bad or len(pairs) != TIES[campaign]:
        fails.append(f"{campaign + 2}: {len(pairs)} first-round ties, not {TIES[campaign]}; not two legs: {bad}")
    sizes = groups(save, campaign)
    print(f"      {campaign + 2}: groups of {sizes}")
    if sizes != GROUPS[campaign]:
        fails.append(f"{campaign + 2}: groups of {sizes}, not {GROUPS[campaign]}")
    if not fails:
        print(f"ok    {campaign + 2}: every African nation plays, {NATION} in a two-leg first-round tie, groups of "
              f"{GROUPS[campaign][0]}")
    return fails


def leg_fails(name, code, box, save):
    fails = []
    if code != 0 or box:
        fails.append(f"leg {name} ended with code {code}{', box ' + box if box else ''}")
    if not os.path.exists(save):
        fails.append(f"leg {name}: no save")
    return fails


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-caf-")
    parts = argv[argv.index("--part") + 1] if "--part" in argv else "abc"
    os.environ["RECOMP_TIER_A"] = "1"
    os.environ["RECOMP_SEASON_INTL_LOG"] = "1"
    o = {"program": program, "game": game, "rules": "on", "database": None, "seed": 1, "start": 2001, "last": 2030,
         "nations": ["England"], "extra": [], "timeout": 7200}
    saved = lambda work: os.path.join(work, "user", "samesave.sav")      # noqa: E731
    fails = []
    if "a" in parts:
        w1, end = os.path.join(keep, "a1"), datetime.date(2004, 7, 31)
        code, took, box = soak_run.new_game_leg(o, w1, (end - START).days)
        print(f"      leg a1 to {end}: {took:.0f} s")
        fails += leg_fails("a1", code, box, saved(w1))
        if not fails:
            fails += check_cycle(saved(w1), 2004)
            w2 = os.path.join(keep, "a2")
            code, took, box = soak_run.load_leg(o, w2, saved(w1), 7)
            print(f"      leg a2, 7 days: {took:.0f} s")
            fails += leg_fails("a2", code, box, saved(w2))
    if "b" in parts:
        save = os.environ.get("RECOMP_CAF_ALL_SAVE") or os.path.join(FIXTURES, "caf-all-2021.sav")
        if not os.path.exists(save):
            fails.append(f"missing: {save}")
        else:
            y, d = history_check.Game(save).date
            begin, end = datetime.date(y, 1, 1) + datetime.timedelta(days=d), datetime.date(2024, 7, 31)
            wb = os.path.join(keep, "b1")
            code, took, box = soak_run.load_leg(o, wb, save, (end - begin).days)
            print(f"      leg b1 {begin} to {end}: {took:.0f} s")
            fails += leg_fails("b1", code, box, saved(wb))
            if not fails:
                fails += check_cycle(saved(wb), 2024)
    if "c" in parts:
        import gate_run
        args, _ = gate_run.database(2025, {"run": keep, "own": None, "databases": os.path.join(FIXTURES, "databases")})
        oc = dict(o, database=args[1], start=2025, last=2027)
        w1 = os.path.join(keep, "c1")
        code, took, box = soak_run.new_game_leg(oc, w1, 7)
        print(f"      leg c1, a 2025 game, 7 days: {took:.0f} s")
        fails += leg_fails("c1", code, box, saved(w1))
        log = open(os.path.join(w1, "stderr.txt"), encoding="latin-1").read()
        pools = [int(x.split(" of ")[1].split()[0]) for x in log.splitlines() if "Africa takes" in x]
        print(f"      pools: {pools}")
        if not pools or max(pools) <= 52:
            fails.append(f"leg c1: no pool of more than 52 ({pools})")
        if not fails:
            w2 = os.path.join(keep, "c2")
            code, took, box = soak_run.load_leg(oc, w2, saved(w1), 7)
            print(f"      leg c2 loads it, 7 days: {took:.0f} s")
            fails += leg_fails("c2", code, box, saved(w2))
            if not fails:
                print(f"ok    a pool of {max(pools)} saves and loads")
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
