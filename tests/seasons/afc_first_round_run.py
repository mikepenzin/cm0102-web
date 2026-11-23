#!/usr/bin/env python3
"""The first round of AFC World Cup qualifying (0.6.2 item 196, season_afc_first.c).

Usage:
    python3 tests/seasons/afc_first_round_run.py <cm0102_recomp> <game folder> [--keep <folder>] [--cycles 1|2]

Real life: the AFC played a two-leg first round of its weakest nations before the groups (2006: 14 teams, 2018 and
2022: 12, 2026: 20; docs/research/rules-0.6.0/int-world.tsv, rows wc_asia_league). Brunei played it in 2018, 2022
and 2026. The original's pool builder 0x913f60 reads the Asian nations in nation-table order and stops after 44
less its exempt count, so a nation at the end of the table (Brunei, in the 3.9.68 data) never plays. With changing
rules on, every Asian nation enters, and the weakest 2 x (nations - 40) play two-leg ties from October of the
campaign year; the winners take the last places of the group draw (25 March of the next year).

One new game, Tier A, England's leagues, seed 1, changing rules on, the 3.9.68 database, a 2001/02 start:
- leg 1 to 6 September 2004 (the 2006 cycle's round is made, from 1 August), a save;
- leg 2 loads it and saves one day later: the Asian section's part of the save is the same;
- leg 0 plays the same game unbroken to 31 March 2005: the round's fixtures and results equal those of leg 3 (save
  and load in the middle of the first round give the same round). The runtime keeps no state of the round outside the
  object, so a load in the same run cannot miss a round;
- leg 3 loads the save of leg 1 and plays to 31 March 2005 (after the group draw), a save;
- with --cycles 2, leg 4 loads that and plays to 31 March 2009 (the 2010 cycle);
- leg 5, not Tier A, a 2008/09 start to 20 October 2008: Timor (no player in the data) plays both legs of its tie
  (--no-full leaves it out).

Item 200: Timor (no continent in the 3.9.68 data) joins the AFC in 2005/06, so it plays the first round of the 2010
cycle (the 2006 cycle's round is in October 2004, before it joined).

The checks: every leg ends with code 0 and no message box; in each cycle Brunei (and Timor in 2010) plays two matches of the Asian
section between 1 August of the campaign year and the group draw (25 March); the section's matches of that
time are two-leg ties (each pair of teams meets twice). On main before item 196 Brunei plays no match: the test
fails there.
"""
import datetime
import os
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))
import history_check  # noqa: E402
import soak_run  # noqa: E402

START = datetime.date(2001, 7, 1)          # 3.9.68, 2001/02: the game's first day (soak_run's leg counts from it)
MIDDLE = datetime.date(2004, 9, 6)
NATION = "Brunei"
TIMOR = "Timor"                 # item 200: no continent in the 3.9.68 data, in the AFC from 2005/06
SECTIONS = ("World Cup Asian Qualifying Section", "FIFA World Cup qualification (AFC)")   # base, rules-on name


def first_round(save, campaign):
    """[(day, home name, away name)] of the Asian section from 1 August of campaign to 24 March after it."""
    game = history_check.Game(save)
    ids = {cid for cid, c in game.comps.items() if c["name"] in SECTIONS}
    lo = datetime.date(campaign, 8, 1).toordinal()
    hi = datetime.date(campaign + 1, 3, 24).toordinal()
    name = lambda t: game.club(t)[1] or game.club(t)[0]     # noqa: E731
    return sorted((f[0], name(f[1]), name(f[2])) for cid in ids for f in history_check.fixtures(game).get(cid, [])
                  if lo <= f[0] <= hi)


def check_cycle(save, campaign, nations=(NATION,)):
    fails = []
    fx = first_round(save, campaign)
    pairs = {}
    for _, h, a in fx:
        pairs[frozenset((h, a))] = pairs.get(frozenset((h, a)), 0) + 1
    days = sorted({datetime.date.fromordinal(d).isoformat() for d, _, _ in fx})
    print(f"      {campaign + 2}: {len(fx)} first-round matches, {len(pairs)} ties, days {', '.join(days)}")
    for nation in nations:
        mine = [f for f in fx if any(n.startswith(nation) for n in f[1:])]    # "Brunei Darussalam" in 3.9.68
        for d, h, a in mine:
            print(f"      {datetime.date.fromordinal(d).isoformat()} {h} v {a}")
        if len(mine) != 2:
            fails.append(f"{campaign + 2}: {nation} plays {len(mine)} first-round matches, not 2")
    bad = [sorted(p) for p, n in pairs.items() if n != 2]
    if not pairs or bad:
        fails.append(f"{campaign + 2}: ties that do not meet twice: {bad}")
    near = clashes(save, campaign)
    if near:
        fails.append(f"{campaign + 2}: a team of the round has another match within 2 days of a leg: {near}")
    if not fails:
        print(f"ok    {campaign + 2}: {', '.join(nations)} play a two-leg first-round tie, no other match near a leg")
    return fails


def played(save, nation, campaign):
    """[(day, home, away, home goals, away goals)] of the nation's first-round matches (fixtures_<year>.tmp records
    of 0x4f bytes: clubs +0xc and +0x10, year +0x28, day +0x2a, goals +0x43 and +0x44, -1 when not played;
    tests/seasons/match_day_place_run.py)."""
    game = history_check.Game(save)
    ids = {cid for cid, c in game.comps.items() if c["name"] in SECTIONS}
    name = lambda t: game.club(t)[1] or game.club(t)[0]     # noqa: E731
    lo, hi = datetime.date(campaign, 8, 1), datetime.date(campaign + 1, 3, 24)
    out = []
    for part in game.fixture_parts:
        b = game.sav.read(part)
        count = struct.unpack_from("<i", b, 0)[0] if len(b) >= 4 else 0
        for i in range(count if 4 + count * 0x4f == len(b) else 0):
            o = 4 + i * 0x4f
            if struct.unpack_from("<i", b, o)[0] not in ids:
                continue
            h, a = struct.unpack_from("<ii", b, o + 0xc)
            y, d = struct.unpack_from("<hh", b, o + 0x28)
            day = datetime.date(y, 1, 1) + datetime.timedelta(d)
            if lo <= day <= hi and any(name(t).startswith(nation) for t in (h, a)):
                out.append((day, name(h), name(a)) + struct.unpack_from("<bb", b, o + 0x43))
    return sorted(out)


def clashes(save, campaign):
    """[(leg day, other day, competition, home, away)]: another match of a team of the round within 2 days of a
    leg (the review of item 196 found Bhutan's second leg of 13 October 2004 beside the Asian Cup qualifying of the
    14th)."""
    game = history_check.Game(save)
    ids = {cid for cid, c in game.comps.items() if c["name"] in SECTIONS}
    lo, hi = datetime.date(campaign, 8, 1), datetime.date(campaign + 1, 3, 24)
    rows = []
    for part in game.fixture_parts:
        b = game.sav.read(part)
        count = struct.unpack_from("<i", b, 0)[0] if len(b) >= 4 else 0
        for i in range(count if 4 + count * 0x4f == len(b) else 0):
            o = 4 + i * 0x4f
            cid = struct.unpack_from("<i", b, o)[0]
            h, a = struct.unpack_from("<ii", b, o + 0xc)
            y, d = struct.unpack_from("<hh", b, o + 0x28)
            if cid in game.comps and 1900 < y < 3000 and 0 <= d < 366:
                rows.append((datetime.date(y, 1, 1) + datetime.timedelta(d), cid, h, a))
    legs = [r for r in rows if r[1] in ids and lo <= r[0] <= hi]
    name = lambda t: game.club(t)[1] or game.club(t)[0]     # noqa: E731
    return [(str(r[0]), str(o[0]), game.comps[o[1]]["name"], name(o[2]), name(o[3])) for r in legs for o in rows
            if o[1] not in ids and set(o[2:]) & set(r[2:]) and abs((o[0] - r[0]).days) <= 2]


def section_parts(save):
    sav = history_check.LazySav(save)
    return {n: sav.read(n) for n in sav.names() if n.startswith(SECTIONS)}


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
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-afc1-")
    cycles = int(argv[argv.index("--cycles") + 1]) if "--cycles" in argv else 1
    os.environ["RECOMP_TIER_A"] = "1"
    o = {"program": program, "game": game, "rules": "on", "database": None, "seed": 1, "start": 2001, "last": 2011,
         "nations": ["England"], "extra": [], "timeout": 7200}
    saved = lambda work: os.path.join(work, "user", "samesave.sav")      # noqa: E731
    fails = []

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
            if not a or a != b:
                fails.append(f"the Asian section's part differs after a save and a load ({sorted(a)}, {sorted(b)})")
            else:
                print(f"ok    save and load in the first round: {', '.join(sorted(a))} the same")
        w3 = os.path.join(keep, "leg3")
        end = datetime.date(2005, 3, 31)
        code, took, box = soak_run.load_leg(o, w3, saved(w1), (end - MIDDLE).days)
        print(f"      leg 3 to {end}: {took:.0f} s")
        fails += leg_fails("3", code, box, saved(w3))
        if not fails:
            fails += check_cycle(saved(w3), 2004)
        # The same game unbroken (no save in the round): the round's fixtures and results equal those of the game
        # saved and loaded in the middle of it (Tier A results come from reputation).
        w0 = os.path.join(keep, "leg0-unbroken")
        code, took, box = soak_run.new_game_leg(o, w0, (end - START).days)
        print(f"      leg 0 (unbroken) to {end}: {took:.0f} s")
        fails += leg_fails("0", code, box, saved(w0))
        if not fails:
            a, b = played(saved(w0), "", 2004), played(saved(w3), "", 2004)
            if not a or a != b:
                fails.append(f"the round differs after a save and a load in it: unbroken {a}, loaded {b}")
            else:
                print(f"ok    the round's {len(a)} matches are the same in the unbroken and the loaded game")
        if not fails and cycles > 1:
            w4 = os.path.join(keep, "leg4")
            end4 = datetime.date(2009, 3, 31)
            code, took, box = soak_run.load_leg(o, w4, saved(w3), (end4 - end).days)
            print(f"      leg 4 to {end4}: {took:.0f} s")
            fails += leg_fails("4", code, box, saved(w4))
            if not fails:
                fails += check_cycle(saved(w4), 2008, (NATION, TIMOR))
    if not fails and "--no-full" not in argv:
        # Not Tier A: a 2008/09 start (3.9.68 data) plays the 2010 cycle's round in full. Timor has no player in the
        # data (nation +0x88 0); the original tops up a side of a nation with at most 500 players with virtual
        # players (0x90dae0, test at 0x90dd5d), so it fields a squad and both legs are played with no box.
        os.environ.pop("RECOMP_TIER_A", None)
        w5 = os.path.join(keep, "leg5-full")
        o5 = dict(o, start=2008)
        code, took, box = soak_run.new_game_leg(o5, w5, (datetime.date(2008, 10, 20) - datetime.date(2008, 7, 1)).days)
        print(f"      leg 5 (full, 2008 start) to 2008-10-20: {took:.0f} s")
        fails += leg_fails("5", code, box, saved(w5))
        if not fails:
            games = played(saved(w5), TIMOR, 2008)
            for g in games:
                print(f"      {g[0]} {g[1]} {g[3]}-{g[4]} {g[2]}")
            if len(games) != 2 or any(g[3] < 0 or g[4] < 0 for g in games):
                fails.append(f"full game: {TIMOR} first-round matches not both played: {games}")
            else:
                print(f"ok    full game: {TIMOR} plays both legs with virtual players")
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
