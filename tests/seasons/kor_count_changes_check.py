"""Every change of the K League club count in the rules table, and how the game plays it (0.7.2 league sizes,
gap 47).

The K League count changes only by clubs that join from outside the league (new clubs, before the K League 2 began
in 2013) or by moves to and from the K League 2. The game has one Korean league class, KorLeague (kor_league), and
no class for a K League 2. So no division stands in for the K League 2 (lead decision of 15 November): at each season
change the row's relegation places go to no division (season_sizes.c lone_exchange), and the size step fills the
league to its next row from the Korean clubs with no division (note_alone, LD6 of slice 14). It sends down only as
many clubs as those clubs can replace (season_size_lone_down). Clubs founded after the start of a game are not in its
club table (season_records.c makes clubs only at the data load), so a new club of REAL is played by a club with no
division: a later release (gap 46).

This check reads the encoded rules (docs/research/2026-10-18-league-rules-encoded.tsv) and the competition classes
of the build (seasons/comp_classes.c) and fails when:
- a count change of kor_league is not in REAL, or REAL holds a change the rows do not make;
- the build has a Korean league class other than kor_league and kor_league_cup (then a K League 2 class exists and
  should take the place of no division).
The run rows kor_league, kor_league@2024 and kor_league@2026 of tests/seasons/league_size_run.py play the moves.

Usage:
    python3 -I tests/seasons/kor_count_changes_check.py <build folder>
"""
import csv
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ENCODED = os.path.join(ROOT, "docs", "research", "2026-10-18-league-rules-encoded.tsv")

NEW = "joined from outside the league (no K League 2 before 2013)"
K2 = "moved to or from the K League 2 (no division in the game)"
W = "https://en.wikipedia.org/wiki/"

# Row season (the real season is the next calendar year): (change of the count, how, the clubs, source).
REAL = {
    2002: (+2, NEW, "Daegu FC (new) and Gwangju Sangmu", W + "2003_K_League"),
    2003: (+1, NEW, "Incheon United (new)", W + "2004_K_League"),
    2005: (+1, NEW, "Gyeongnam FC (new)", W + "2006_K_League"),
    2008: (+1, NEW, "Gangwon FC (new)", W + "2009_K_League"),
    2010: (+1, NEW, "Gwangju FC (new)", W + "2011_K_League"),
    2012: (-2, K2, "Gwangju and Sangju Sangmu to the K League Challenge", W + "2012_K_League"),
    2013: (-2, K2, "Daegu and Daejeon down, Gangwon down and Sangju up by the play-off",
           W + "2013_K_League_Classic"),
    2026: (+2, K2, "two more clubs from the K League 2", "https://www.koreatimes.co.kr/amp/sports/20251201/"
           "top-football-league-to-add-2-clubs-in-2027"),
}


def counts():
    out = {}
    with open(ENCODED, encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            if r["comp_file"] == "kor_league":
                out[int(r["season"])] = int(r["clubs"])
    return out


def changes(rows):
    out, last = {}, None
    for season in sorted(rows):
        n = rows[season]
        if n < 0:
            continue
        if last is not None and n != last:
            out[season] = n - last
        last = n
    return out


def korean_leagues(build):
    text = open(os.path.join(build, "seasons", "comp_classes.c"), encoding="utf-8").read()
    return sorted(re.findall(r'"(kor_\w+)", "\w+", SEASON_KIND_LEAGUE', text))


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    fails = []
    got = changes(counts())
    for season in sorted(set(got) | set(REAL)):
        want = REAL.get(season)
        if season not in got or want is None or want[0] != got[season]:
            fails.append(f"row {season}: the rows change the count by {got.get(season)}, REAL says "
                         f"{want[0] if want else None}")
        else:
            print(f"ok    row {season} ({season + 1}): {got[season]:+d}, {want[1]}: {want[2]} ({want[3]})")
    leagues = korean_leagues(argv[1])
    if leagues != ["kor_league", "kor_league_cup"]:
        fails.append(f"Korean league classes {leagues}: a class other than kor_league and kor_league_cup")
    else:
        print("ok    the game's Korean league classes are kor_league and kor_league_cup: no division stands in for the K League 2")
    for f in fails:
        print("FAIL  " + f)
    print("FAIL" if fails else "PASS")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
