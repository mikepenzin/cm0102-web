#!/usr/bin/env python3
"""World Cup qualifiers of 2033 keep their FIFA days (0.6.2 items 210 and 211, season_friendly_dates.c and
src/recomp_rt/win/season_caf_pool.c rt_caf_round_date).

Usage:
    python3 tests/seasons/wcq_2033_days_run.py <cm0102_recomp> <game folder> [--keep <folder>]

The save of run set game a2021s1 (Tier A, the October 2021 database, seed 1, changing rules on) of 21 July 2031
(test-fixtures/wcq-2031.sav, or RECOMP_WCQ_2031_SAVE) plays to 5 April 2034 with RECOMP_SEASON_INTL_LOG=1.

Item 210: the snap held a FIFA day for any continental club match of any confederation. Rounds 3 of the CONCACAF
qualifying of 2034 (El Salvador v Jamaica, Mexico v Costa Rica, United States v Honduras) gave way to Al-Ahli SC v
Khor Fakkan (AFC Champions League Elite): "moves ... (stage plan: its planned day was not free)". Real life: a club
match takes only the players of its clubs. The check (RECOMP_SCREEN_LOG): no CONCACAF qualifier of 2033 leaves its
planned day for a club match whose clubs hold none of the two nations' players, or with no logged reason (the
builds before the item log none).

Item 211: the original's CAF group template (0x90f600) puts rounds 8 to 10 on 30 June, 14 and 17 July of the year
before the World Cup, inside the Africa Cup of Nations of 2033 (the 2027 edition's 19 June repeats every 2 years).
Real life from the 2006 cycle: no deciding group match in July of that year (en.wikipedia.org "<year> FIFA World
Cup qualification (CAF)"; 2026 cycle matchdays 9 and 10 on 8 to 14 October 2025). The checks: the CAF stage plan
of 2033 has no round in July, no CAF qualifier is played from 1 July to 31 July 2033, and the group stage's last
match is from 1 September to 30 November 2033. The Africa Cup of Nations of 2033 keeps its days (52 matches
from mid June, its knockout from 9 to 30 July as on round AG), and the play-off tournament of March 2034 (season_wcq26.c, built from
1 March of the World Cup year) takes the best CAF runner-up: its seeds line comes and no team of it is unknown.

On round AG (b4966397), before the items, both fail.
"""
import datetime
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))
import history_check  # noqa: E402
import soak_run  # noqa: E402

FIXTURES = "/Users/max/Projects/cm0102/test-fixtures"
CAF = ("World Cup African Qualifying Section", "FIFA World Cup qualification (CAF)")
END = datetime.date(2034, 4, 5)
ACN = "Africa Cup of Nations"
PLAN = re.compile(r"stage plan of FIFA World Cup qualification \((\w+)\) ending day (\d+) of (\d+): .*?: (.*)$")


def doy_date(year, doy):
    return datetime.date(year, 1, 1) + datetime.timedelta(days=doy)


def check(log, save):
    fails = []
    lines = open(log, encoding="latin-1").read().splitlines()
    held = [x for x in lines if "FIFA World Cup qualification (CONCACAF)" in x and "of 2033" in x and
            "its planned day was not free" in x]
    # The hold reason (held_why, RECOMP_SCREEN_LOG): a club match holds the day only with players of the nations.
    bad = [x for x in held if "not free)" in x or ", 0 players of the two nations" in x]
    print(f"      CONCACAF 2033: {len(held)} qualifiers leave their planned day, {len(bad)} for no player or no reason")
    for x in held[:6]:
        print("      " + x.split("intl dates: ", 1)[-1][:220])
    if bad:
        fails.append(f"CONCACAF 2033: {len(bad)} qualifiers leave their planned day for a club match with none of "
                     f"their players, or for no logged reason")
    found = [m for m in map(PLAN.search, lines) if m and m.group(1) == "CAF" and m.group(3) == "2033"]
    plans = list({m.group(0): m for m in found}.values())
    if not plans:
        fails.append("no CAF stage plan ending in 2033")
    for m in plans:
        print(f"      CAF stage plan ending day {m.group(2)} of 2033: {m.group(4)}")
        # The rounds lie in order: the year of a round's own day goes up where its day falls; the stage ends in 2033.
        pairs = [tuple(map(int, p.split(">"))) for p in m.group(4).split()]
        year = 2033 - sum(1 for i in range(1, len(pairs)) if pairs[i][0] < pairs[i - 1][0])
        july = []
        for i, (own, to) in enumerate(pairs):
            if i and own < pairs[i - 1][0]:
                year += 1
            if year == 2033 and doy_date(year, to).month == 7:
                july.append(f"{own}>{to}")
        if july:
            fails.append(f"CAF stage plan of 2033 has rounds in July: {july}")
    game = history_check.Game(save)
    ids = {cid for cid, c in game.comps.items() if c["name"] in CAF}
    fx = sorted(f for cid in ids for f in history_check.fixtures(game).get(cid, []))
    in_2033 = [f for f in fx if datetime.date.fromordinal(f[0]).year == 2033]
    july = [f for f in in_2033 if datetime.date.fromordinal(f[0]).month == 7]
    last = datetime.date.fromordinal(in_2033[-1][0]) if in_2033 else None
    print(f"      CAF 2033: {len(in_2033)} matches, {len(july)} in July, the last on {last}")
    if july:
        fails.append(f"CAF 2033: {len(july)} matches in July")
    if not last or not datetime.date(2033, 9, 1) <= last <= datetime.date(2033, 11, 30):
        fails.append(f"CAF 2033: the last match on {last}, not September to November")
    acn = [cid for cid, c in game.comps.items() if c["name"] == ACN]
    days = sorted(datetime.date.fromordinal(f[0]) for cid in acn for f in history_check.fixtures(game).get(cid, [])
                  if datetime.date.fromordinal(f[0]).year == 2033)
    print(f"      {ACN} 2033: {len(days)} matches, {days[0] if days else None} to {days[-1] if days else None}")
    # Its own days (the edition of 2027 from 19 June, shift of season_intl_date_shift): the groups from mid June
    # (13 June in 2029, 18 June in 2031 of this game), the final on 30 July as on round AG. On round AG the CAF
    # qualifiers of 14 June 2033 pushed its first matches to 16 June.
    knockout = sorted({x for x in days if x.month == 7 and x.day >= 9})
    if len(days) != 52 or days[0] < datetime.date(2033, 6, 10) or days[-1] != datetime.date(2033, 7, 30) or \
            knockout != [datetime.date(2033, 7, d) for d in (9, 10, 11, 12, 22, 23, 24, 27, 29, 30)]:
        fails.append(f"{ACN} 2033 does not keep its days (52 matches from mid June, the knockout of round AG)")
    po = [x for x in lines if "wc play-off 2034" in x]
    for x in po:
        if "seeds" in x or "not known" in x:
            print("      " + x.split("season intl: ", 1)[-1][:160])
    if not any("seeds" in x for x in po) or any("not known" in x for x in po):
        fails.append("the play-off tournament of 2034 has no seeds line or a team not known")
    return fails


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-wcq33-")
    save = os.environ.get("RECOMP_WCQ_2031_SAVE") or os.path.join(FIXTURES, "wcq-2031.sav")
    if not os.path.exists(save):
        print(f"FAIL  missing: {save}")
        return 1
    os.environ["RECOMP_TIER_A"] = "1"
    soak_run.LEG_ENV["RECOMP_SEASON_INTL_LOG"] = "1"
    soak_run.LEG_ENV["RECOMP_SCREEN_LOG"] = "1"            # the intl dates lines (log_move)
    o = {"program": program, "game": game, "rules": "on", "seed": 1, "start": 2021, "last": 2101, "nations": ["all"],
         "database": os.path.join(FIXTURES, "databases", "October 2021"), "extra": [], "timeout": 10800}
    y, d = history_check.Game(save).date
    begin = datetime.date(y, 1, 1) + datetime.timedelta(days=d)
    work = os.path.join(keep, "leg")
    code, took, box = soak_run.load_leg(o, work, save, (END - begin).days)
    print(f"      leg {begin} to {END}: {took:.0f} s")
    fails = []
    out = os.path.join(work, "user", "samesave.sav")
    if code != 0 or box:
        fails.append(f"leg ended with code {code}{', box ' + box if box else ''}")
    if not os.path.exists(out):
        fails.append("no save")
    else:
        fails += check(os.path.join(work, "stderr.txt"), out)
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
