#!/usr/bin/env python3
"""The group matches of the international finals keep their group days (0.6.2 item 217,
src/recomp_rt/win/season_intl_cups.c season_intl_twin_venues).

Usage:
    python3 tests/seasons/finals_group_days_run.py <cm0102_recomp> <game folder> [--keep <folder>]
    python3 tests/seasons/finals_group_days_run.py --check <stderr.txt> <save>     (the checks on a finished leg)

The save of run set game a2021s1 (Tier A, the October 2021 database, seed 1, changing rules on) of 21 July 2031
(test-fixtures/wcq-2031.sav, or RECOMP_WCQ_2031_SAVE) plays to 25 February 2035 with RECOMP_TEST_FIXTURE_HOLD=log
(fixture_season_end.c: each move of add_fixture 0x5aa680 for a clash is logged, "test: a clash move of <name>")
and RECOMP_SEASON_INTL_LOG=1. The leg holds Euro 2032 (24 teams), the Africa Cup of Nations 2033 (24), the Gold Cup
2033 (16), the World Cup 2034 (48) and the Asian Cup 2035 (24).

Cause: a finals group that takes another group's template (the ACN and Asian Cup of 24, the Euro of 24, the World
Cup of 48, the Copa America and Gold Cup of 16) took that group's venue flags too. Flag 4 is the host's national
stadium, given with no check that it is free (Stadium::set_fixture_venue 0x85c260), so two group matches of one day
met there, find_fixture_clashes 0x5ad780 gave severity 20 and add_fixture moved one by a week or more. On round AG
(a6babf87) the ACN 2033 played Ivory Coast v Tunisia on 2 July and Ghana v South Africa on 6 July, after the other
group matches, and its knockout started 14 days late (run-logs/merge-062ag/lane44/iwcq-2033).

Checks, for each of those finals:
- no match is moved a week or more for a clash of severity 20 (the same venue on the same day, 0x5adba0; the log
  line "test: clash severity" before each move);
- the knockout is made no more than 7 days late ("knockout made ... delay N" of season_intl_rest_delay: 7 is the
  week that the groups of 24 and the Copa America and Gold Cup of 16 always take; 14 on round AH for the ACN 2033
  and the Asian Cup 2035);
- each edition's last group match is before its first knockout match (fixtures: +0x32 the template id of a group,
  0 for the knockout).
The other clash moves stay: the competitions' own rule for two strong teams in one kick-off slot (severity 10,
the class callback at vtable +0x60 in 0x5adba0) and the moves with no clash at the old date (severity 0) are on
round AH too and keep the matches in their group days.
Real life: each edition plays in its own stadiums, several a day (2019, 2023 and 2025 Africa Cup of Nations, 2019
and 2023 Asian Cup, Euro 2016, 2020 and 2024; en.wikipedia.org, each edition's article, "Venues").
"""
import collections
import datetime
import os
import re
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))
import history_check  # noqa: E402
import soak_run  # noqa: E402

FIXTURES = "/Users/max/Projects/cm0102/test-fixtures"
END = datetime.date(2035, 2, 25)
FINALS = ("Africa Cup of Nations", "AFC Asian Cup", "UEFA European Championship", "FIFA World Cup",
          "Copa America", "Copa América", "CONCACAF Gold Cup")
MOVE = re.compile(r"recomp: fixtures: test: a (?:same-day )?clash move of (.*?) from day (\d+) of (\d+) to day (\d+) of "
                  r"(\d+)")
KNOCKOUT = re.compile(r"(\w+) (\d+) knockout made on day \d+ of \d+, first day \d+ of \d+, delay (\d+)")
FIRST = datetime.date(2031, 7, 21)
EXPECTED = ("Africa Cup of Nations", "AFC Asian Cup", "UEFA European Championship", "FIFA World Cup", "Copa América",
            "CONCACAF Gold Cup")


def finals_fixtures(game):
    """{(name, edition start ordinal): {"group": [ordinals], "ko": [ordinals]}} of the finals fixtures after FIRST.
    Fixtures part records of 0x4f bytes: +0 competition id, +0x28 s16 year, +0x2a s16 day of the year from 0,
    +0x32 s16 the group template id (0 in the knockout). An edition: the fixtures of one name within 120 days."""
    comps = {cid: c["name"] for cid, c in game.comps.items() if c["name"] in FINALS}
    rows = []
    for n in game.fixture_parts:
        b = game.sav.read(n)
        count = struct.unpack_from("<i", b, 0)[0] if len(b) >= 4 else 0
        if count < 0 or 4 + count * 0x4f != len(b):
            continue
        for i in range(count):
            o = 4 + i * 0x4f
            cid = struct.unpack_from("<i", b, o)[0]
            if cid not in comps:
                continue
            y, d = struct.unpack_from("<hh", b, o + 0x28)
            gid = struct.unpack_from("<h", b, o + 0x32)[0]
            day = datetime.date(y, 1, 1).toordinal() + d
            if day >= FIRST.toordinal():
                rows.append((comps[cid], day, gid))
    out = collections.OrderedDict()
    last = {}
    for name, day, gid in sorted(rows):
        start = last.get(name)
        if start is None or day - start[1] > 120:
            start = (day, day)
        last[name] = (start[0], day)
        e = out.setdefault((name, start[0]), {"group": [], "ko": []})
        e["ko" if gid == 0 else "group"].append(day)
    return out


def check(log, save):
    fails = []
    lines = open(log, encoding="latin-1").read().splitlines()
    venue = []
    for i, line in enumerate(lines):
        m = MOVE.search(line)
        if not m or m.group(1) not in FINALS:
            continue
        sev = re.search(r"clash severity (-?\d+)", lines[i - 1]) if i else None
        old = datetime.date(int(m.group(3)), 1, 1).toordinal() + int(m.group(2))
        new = datetime.date(int(m.group(5)), 1, 1).toordinal() + int(m.group(4))
        if not sev:                                     # the test log gives a severity before each move
            venue.append(m.group(0).split("test: ", 1)[-1] + " (no severity line)")
        elif sev.group(1) == "20" and abs(new - old) >= 7:
            venue.append(m.group(0).split("test: ", 1)[-1] + " (" + lines[i - 1].split("priority ")[-1] + ")")
    print(f"      finals matches moved a week or more for a venue clash: {len(venue)}")
    for v in venue[:8]:
        print("        " + v)
    if venue:
        fails.append(f"{len(venue)} finals matches moved a week or more for a venue clash")
    for line in lines:
        m = KNOCKOUT.search(line)
        if m:
            print(f"      {m.group(0)}")
            if int(m.group(3)) > 7:
                fails.append(f"{m.group(1)} {m.group(2)}: the knockout is made {m.group(3)} days late")
    eds = finals_fixtures(history_check.Game(save))
    # Each finals of the leg must be found by its name (the October 2021 database names them otherwise until the
    # rules rename them), so that no check passes for want of matches.
    for name in EXPECTED:
        if not any(n == name and e["group"] and e["ko"] for (n, _), e in eds.items()):
            fails.append(f"{name}: no complete edition in the leg")
    for (name, _), e in eds.items():
        g, k = sorted(e["group"]), sorted(e["ko"])
        if not g or not k:
            print(f"      {name}: {len(g)} group and {len(k)} knockout matches (edition not complete in the leg)")
            continue
        gl, kf = datetime.date.fromordinal(g[-1]), datetime.date.fromordinal(k[0])
        print(f"      {name} {datetime.date.fromordinal(g[0])}: {len(g)} group matches to {gl}, "
              f"{len(k)} knockout matches from {kf}")
        if g[-1] >= k[0]:
            fails.append(f"{name} {gl.year}: the last group match ({gl}) is not before the knockout ({kf})")
    return fails


def main(argv):
    if len(argv) >= 4 and argv[1] == "--check":
        fails = check(argv[2], argv[3])
    elif len(argv) >= 3:
        program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
        keep = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-fgd-")
        save = os.environ.get("RECOMP_WCQ_2031_SAVE") or os.path.join(FIXTURES, "wcq-2031.sav")
        if not os.path.exists(save):
            print(f"FAIL  missing: {save}")
            return 1
        os.environ["RECOMP_TIER_A"] = "1"
        soak_run.LEG_ENV["RECOMP_SEASON_INTL_LOG"] = "1"
        soak_run.LEG_ENV["RECOMP_TEST_FIXTURE_HOLD"] = "log"   # log only: the moves stay those of the game
        o = {"program": program, "game": game, "rules": "on", "seed": 1, "start": 2021, "last": 2101,
             "nations": ["all"], "database": os.path.join(FIXTURES, "databases", "October 2021"), "extra": [],
             "timeout": 10800}
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
    else:
        print(__doc__)
        return 2
    for f in fails:
        print("FAIL ", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
