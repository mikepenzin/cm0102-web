#!/usr/bin/env python3
"""The preliminary round of OFC World Cup qualifying (0.6.2 item 198, season_ofc_first.c).

Usage:
    python3 tests/seasons/ofc_first_round_run.py <cm0102_recomp> <game folder> [--keep <folder>] [--cycles 1|2]
        [--late]

Real life: every OFC member enters World Cup qualifying, and the lowest-ranked play a first round before the main
stage (2014, 2018 and 2026 cycles: 4 teams at one host; docs/research/rules-0.6.0/int-world.tsv, rows
wc_oceania_league). Papua New Guinea played the qualifying of 2006 and of every cycle from 2014 to 2026. The
original's pool builder 0x91fa80 takes 10 nations and leaves out Papua New Guinea (key nation 0x9cf41c, 0x91fc93),
so it never plays. With changing rules on every Oceania nation enters: the best 10 take the group places, and the
weakest W = nations - 10 play two-leg ties against the W weakest of the 10 on FIFA days in the autumn before the
group draw (January of the campaign year); each winner takes its seed's place.

One new game, Tier A, England's leagues, seed 1, changing rules on, the 3.9.68 database, a 2001/02 start:
- leg 1 to MIDDLE (8 September 2003, between the two legs of the 2006 cycle's round), a save;
- leg 2 loads it and saves one day later: the Oceania section's part of the save is the same (save and load in
  the middle of the round give the same game), apart from the heap address words of group A's rows (masked);
- leg 3 loads the save of leg 1 and plays to 30 April 2005 (the groups under way), a save;
- with --cycles 2, leg 4 loads that and plays to 30 April 2009 (the 2010 cycle, Australia in Asia).

--late: one new game in 2025/26 on the December 2025 database, Tier A, played about ten years (to April 2035); the
same checks for the 2030 and 2034 cycles (the 2026 qualifying code of season_wcq26.c acts there), except that the
tie may be another nation's: by then Papua New Guinea has a ranking from its matches.

The checks: every leg ends with code 0 and no message box; in each cycle Papua New Guinea plays two matches of the
Oceania section between 1 July of the year before the campaign year and 16 January of the campaign year, each pair
of teams of that time meets twice, and every Oceania nation plays a match of the section in the cycle (to 30 April
of the year after the campaign year). On main before item 198 Papua New Guinea plays no match: the test fails there.
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
MIDDLE = datetime.date(2003, 9, 8)
NATION = "Papua New Guinea"
DATABASES = "/Users/max/Projects/cm0102/test-fixtures/databases"
LATE_DAYS = 3590             # about 1 July 2025 to April 2035


def is_section(name):
    return "Oceania" in name and "Qualif" in name or name == "FIFA World Cup qualification (OFC)"


def section_matches(save, lo, hi):
    """[(day, home name, away name)] of the Oceania section from lo to hi (dates), and the Oceania nation names."""
    game = history_check.Game(save)
    ids = {cid for cid, c in game.comps.items() if is_section(c["name"])}
    name = lambda t: game.club(t)[1] or game.club(t)[0]     # noqa: E731
    out = sorted((f[0], name(f[1]), name(f[2])) for cid in ids for f in history_check.fixtures(game).get(cid, [])
                 if lo.toordinal() <= f[0] <= hi.toordinal())
    oceania = {n for n, c in game.nations.values() if c == "OFC"}
    return out, oceania


def check_cycle(save, campaign, in_tie=True):
    fails = []
    pre, oceania = section_matches(save, datetime.date(campaign - 1, 7, 1), datetime.date(campaign, 1, 16))
    pairs = {}
    for _, h, a in pre:
        pairs[frozenset((h, a))] = pairs.get(frozenset((h, a)), 0) + 1
    days = sorted({datetime.date.fromordinal(d).isoformat() for d, _, _ in pre})
    print(f"      {campaign + 2}: {len(pre)} preliminary matches, {len(pairs)} ties, days {', '.join(days)}")
    mine = [f for f in pre if NATION in f[1:]]
    for d, h, a in mine:
        print(f"      {datetime.date.fromordinal(d).isoformat()} {h} v {a}")
    if in_tie and len(mine) != 2:
        fails.append(f"{campaign + 2}: {NATION} plays {len(mine)} preliminary matches, not 2")
    bad = [sorted(p) for p, n in pairs.items() if n != 2]
    if not pairs or bad:
        fails.append(f"{campaign + 2}: ties that do not meet twice: {bad}")
    cycle, _ = section_matches(save, datetime.date(campaign - 1, 7, 1), datetime.date(campaign + 1, 4, 30))
    played = {n for _, h, a in cycle for n in (h, a)}
    missing = sorted(n for n in oceania if n not in played)
    print(f"      {campaign + 2}: {len(played & oceania)} of {len(oceania)} Oceania nations play")
    if missing:
        fails.append(f"{campaign + 2}: Oceania nations with no match of the section: {missing}")
    if not fails:
        print(f"ok    {campaign + 2}: {'%s plays a two-leg preliminary tie, ' % NATION if in_tie else 'a two-leg '
                                        'preliminary round, '}every Oceania nation plays")
    return fails


def masked(a, b):
    """a and b with each heap address word that differs set to 0 in both: each table row of group A keeps at +9 the
    pointer of its malloc'd array (init_table_row 0x687570..0x687582), which the League save writes as it is (5
    rows of 0x3b bytes), so two processes differ there whatever the game. A run of 1 to 4 differing bytes is such
    a word when it ends at the word's top byte and that byte differs and is 0x10 or more in both (addresses of
    0x10000000 and up; a score, count or day word has a top byte of 0). Returns (a, b)."""
    a, b = bytearray(a), bytearray(b)
    n, i = min(len(a), len(b)), 0
    while i < n:
        if a[i] == b[i]:
            i += 1
            continue
        e = i
        while e + 1 < n and a[e + 1] != b[e + 1]:
            e += 1
        w = e - 3
        if e - i < 4 and w >= 0 and a[e] != b[e] and a[e] >= 0x10 and b[e] >= 0x10:
            a[w:e + 1] = b[w:e + 1] = bytes(4)
        i = e + 1
    return bytes(a), bytes(b)


def section_parts(save):
    sav = history_check.LazySav(save)
    return {n: sav.read(n) for n in sav.names() if n.startswith("FIFA World Cup qualification (OFC)") or n.startswith("World Cup Oceania")}


def leg_fails(name, code, box, save):
    fails = []
    if code != 0 or box:
        fails.append(f"leg {name} ended with code {code}{', box ' + box if box else ''}")
    if not os.path.exists(save):
        fails.append(f"leg {name}: no save")
    return fails


def late(o, keep, saved):
    """--late: a new game in 2025/26 on the December 2025 database (season_wcq26.c acts: World Cups from 2026), Tier
    A, played about ten years: in the 2030 and 2034 cycles a preliminary round of two-leg ties is played and every
    Oceania nation, Papua New Guinea with them, plays a match of the section (the weakest by then need not be Papua
    New Guinea, which has a ranking from its matches)."""
    o = dict(o, start=2025, last=2036, database=os.path.join(DATABASES, "December 2025"))
    w = os.path.join(keep, "late")
    code, took, box = soak_run.new_game_leg(o, w, LATE_DAYS)
    print(f"      late leg ({LATE_DAYS} days from a 2025/26 start): {took:.0f} s")
    fails = leg_fails("late", code, box, saved(w))
    if not fails:
        fails += check_cycle(saved(w), 2028, in_tie=False)
        fails += check_cycle(saved(w), 2032, in_tie=False)
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-ofc1-")
    cycles = int(argv[argv.index("--cycles") + 1]) if "--cycles" in argv else 1
    os.environ["RECOMP_TIER_A"] = "1"
    o = {"program": program, "game": game, "rules": "on", "database": None, "seed": 1, "start": 2001, "last": 2011,
         "nations": ["England"], "extra": [], "timeout": 7200}
    saved = lambda work: os.path.join(work, "user", "samesave.sav")      # noqa: E731
    fails = []
    if "--late" in argv:
        return late(o, keep, saved)

    w1 = os.path.join(keep, "leg1")
    code, took, box = soak_run.new_game_leg(o, w1, (MIDDLE - START).days)
    print(f"      leg 1 to {MIDDLE}: {took:.0f} s")
    fails += leg_fails("1", code, box, saved(w1))
    if not fails:
        w2 = os.path.join(keep, "leg2")
        code, took, box = soak_run.load_leg(o, w2, saved(w1), 1)
        fails += leg_fails("2", code, box, saved(w2))
        if not fails:
            a, b = section_parts(saved(w1)), section_parts(saved(w2))
            if a and set(a) == set(b):
                a, b = {n: masked(a[n], b[n])[0] for n in a}, {n: masked(a[n], b[n])[1] for n in a}
            if not a or a != b:
                fails.append(f"the Oceania section's part differs after a save and a load ({sorted(a)}, {sorted(b)})")
            else:
                print(f"ok    save and load in the preliminary round: {', '.join(sorted(a))} the same")
        w3 = os.path.join(keep, "leg3")
        end = datetime.date(2005, 4, 30)
        code, took, box = soak_run.load_leg(o, w3, saved(w1), (end - MIDDLE).days)
        print(f"      leg 3 to {end}: {took:.0f} s")
        fails += leg_fails("3", code, box, saved(w3))
        if not fails:
            fails += check_cycle(saved(w3), 2004)
        if not fails and cycles > 1:
            w4 = os.path.join(keep, "leg4")
            end4 = datetime.date(2009, 4, 30)
            code, took, box = soak_run.load_leg(o, w4, saved(w3), (end4 - end).days)
            print(f"      leg 4 to {end4}: {took:.0f} s")
            fails += leg_fails("4", code, box, saved(w4))
            if not fails:
                fails += check_cycle(saved(w4), 2008)
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
