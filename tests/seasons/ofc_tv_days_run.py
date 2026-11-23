#!/usr/bin/env python3
"""The OFC World Cup qualifying groups keep their days (0.6.2 item 205, season_friendly_dates.c, the wrap of
League::move_fixtures_for_tv 0x685640).

Usage:
    python3 tests/seasons/ofc_tv_days_run.py <cm0102_recomp> <game folder> [--keep <folder>]

The OFC template 0x91e2e0 gives the group stage five rounds in April of the year after the draw (league_round_set_date
0x68a160 at 0x91e37a..0x91e434). In a running season the FIFA snap moves rounds 1 and 2 to the March FIFA days and
keeps rounds 3 to 5 on their template days (a side's window holds two matches). League::move_fixtures_for_tv then
moved those three rounds to a TV weekday one day from the next round, and add_fixture 0x5aa680 moved each again for
the clash (0x5ab510 at 0x5aa96e): in a replay of 2033 group A played on 13, 20 and 26 April and group B on 13, 20 and
27 April, where the template has 13, 16 and 18 April (run-logs/0.6.2-s205/rep-jan). Qualifiers are not moved for
television, so after item 205 a league of national sides that the snap places gets no TV move.

One new game in 2025/26 on the December 2025 database, Tier A, England's leagues, seed 1, changing rules on, played
to 31 May 2029 (the 2030 cycle's OFC groups in March and April 2029). The checks: the leg ends with code 0 and no
message box; the OFC section has matches from 1 January to 31 May 2029 in two or more groups (the sets of nations
that meet); every group plays on the same days; and no match of the section is after 20 April (the template's last
day is 18 April). On the code before item 205 the April rounds move to the 20th and later and the groups' days
differ: the test fails there.
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

DATABASES = "/Users/max/Projects/cm0102/test-fixtures/databases"
START = datetime.date(2025, 7, 1)
END = datetime.date(2029, 5, 31)
YEAR = 2029
LAST = datetime.date(YEAR, 4, 20)


def is_section(name):
    return "Oceania" in name and "Qualif" in name or name == "FIFA World Cup qualification (OFC)"


def groups_of(matches):
    """The groups: the sets of nations joined by a match, with each group's match days."""
    group = {}
    for _, h, a in matches:
        g = group.get(h, {h}) | group.get(a, {a})
        for n in g:
            group[n] = g
    out = []
    for g in {frozenset(g) for g in group.values()}:
        days = sorted({d for d, h, a in matches if h in g})
        out.append((sorted(g), days))
    return sorted(out)


def check(save):
    game = history_check.Game(save)
    ids = {cid for cid, c in game.comps.items() if is_section(c["name"])}
    name = lambda t: game.club(t)[1] or game.club(t)[0]     # noqa: E731
    lo, hi = datetime.date(YEAR, 1, 1).toordinal(), END.toordinal()
    matches = sorted((f[0], name(f[1]), name(f[2])) for cid in ids for f in history_check.fixtures(game).get(cid, [])
                     if lo <= f[0] <= hi)
    fails = []
    groups = groups_of(matches)
    for nations, days in groups:
        print(f"      {', '.join(nations)}: {', '.join(datetime.date.fromordinal(d).isoformat() for d in days)}")
    if len(groups) < 2:
        fails.append(f"{len(groups)} OFC groups with matches in {YEAR} (2 or more expected), {len(matches)} matches")
    elif len({tuple(days) for _, days in groups}) != 1:
        fails.append("the OFC groups play on different days")
    late = sorted({datetime.date.fromordinal(d).isoformat() for d, _, _ in matches if d > LAST.toordinal()})
    if late:
        fails.append(f"OFC group matches after {LAST}: {', '.join(late)}")
    if not fails:
        print(f"ok    {len(groups)} OFC groups play {len(matches)} matches on the same days, none after {LAST}")
    return fails


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-ofc-tv-")
    os.environ["RECOMP_TIER_A"] = "1"
    os.environ["RECOMP_SEASON_INTL_LOG"] = "1"
    o = {"program": program, "game": game, "rules": "on", "seed": 1, "start": 2025, "last": 2030,
         "database": os.path.join(DATABASES, "December 2025"), "nations": ["England"], "extra": [], "timeout": 7200}
    w = os.path.join(keep, "leg")
    code, took, box = soak_run.new_game_leg(o, w, (END - START).days)
    print(f"      leg to {END}: {took:.0f} s")
    save = os.path.join(w, "user", "samesave.sav")
    fails = []
    if code != 0 or box:
        fails.append(f"the leg ended with code {code}{', box ' + box if box else ''}")
    elif not os.path.exists(save):
        fails.append("no save")
    else:
        fails += check(save)
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
