#!/usr/bin/env python3
"""Makes src/recomp_rt/win/season_uefa_dates_table.c: the real dates of the UEFA knockout rounds of each season
(O259, the fixed 2004/05 knockout dates of season_uefa_cup.c and the fixed dates of the Champions League knockout
in season_uefa.c; season_uefa.h, season_uefa_ko_dates).

Usage:
    python3 -I tools/seasons/make_uefa_dates.py            write season_uefa_dates_table.c
    python3 -I tools/seasons/make_uefa_dates.py --check    exit 1 when season_uefa_dates_table.c is not the file that
                                                           the table would give (ctest season_uefa_dates_table)

The table: docs/research/rules-0.6.0/uefa-ko-dates.tsv, one row per competition, season and round (comp cl, uc or
ecl; round po, r16, qf, sf or final; the draw, the first day of the first leg and of the second leg; a uefa.com and a
Wikipedia source). The final has no draw (the draw of the semi-finals sets it: the tool takes the semi-final's
draw) and no second leg. A row with an empty date or with "not found" in its note stops the tool. A two-leg round
with no second leg or with more than MAX_GAP days to it (2019/20: the quarter-finals and semi-finals were single
matches in August 2020, the Europa League round of 16 second legs were in August) is left out with the later rounds of
its season, which keep the dates of the format.
"""
import csv
import datetime
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
TABLE = os.path.join(ROOT, "docs", "research", "rules-0.6.0", "uefa-ko-dates.tsv")
OUT = os.path.join(ROOT, "src", "recomp_rt", "win", "season_uefa_dates_table.c")

COMPS = {"cl": "c", "uc": "u", "ecl": "k"}
ROUNDS = ["po", "r16", "qf", "sf", "final"]
moved = []
MAX_GAP = 35                # days from the first leg to the second; more: the pandemic break of 2019/20


def fail(text):
    raise SystemExit("make_uefa_dates.py: " + text)


def date(text, where):
    try:
        return datetime.date.fromisoformat(text)
    except ValueError:
        fail(f"{where}: date {text!r} is not YYYY-MM-DD")


def read():
    rows, single = {}, set()
    moved.clear()
    with open(TABLE, newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            where = f"{r['comp']} {r['season']} {r['round']}"
            if r["comp"] not in COMPS or r["round"] not in ROUNDS:
                fail(f"{where}: unknown comp or round")
            if "not found" in (r.get("note") or ""):
                fail(f"{where}: a date not found")
            year = int(r["season"][:4])
            if r["season"] != f"{year}/{(year + 1) % 100:02d}":
                fail(f"{where}: season is not Y/YY")
            if not r["source2"] or (not r["source1"] and not (r.get("note") or "").strip()):
                fail(f"{where}: two sources wanted (or one, with the reason in the note)")
            leg1 = date(r["leg1"], where)
            if r["round"] != "final" and not r["leg2"]:
                single.add((r["comp"], year, ROUNDS.index(r["round"])))
                continue
            leg2 = date(r["leg2"], where) if r["round"] != "final" else None
            if leg2 and (leg2 - leg1).days > MAX_GAP:
                single.add((r["comp"], year, ROUNDS.index(r["round"])))
                continue
            draw = date(r["draw"], where) if r["draw"] else None
            if r["round"] != "final" and draw is None:
                fail(f"{where}: no draw")
            if (r["comp"], year, r["round"]) in rows:
                fail(f"{where}: two rows")
            rows[(r["comp"], year, r["round"])] = (draw, leg1, leg2)
    # A two-leg round played as one match (2019/20, the quarter-finals and semi-finals of August 2020) or with its
    # second legs months later (the Europa League round of 16 of 2019/20, March and August 2020): the game plays two
    # legs a week or two apart, so that round and the later rounds of the season keep the format's dates.
    for comp, year, first in single:
        for k in list(rows):
            if k[0] == comp and k[1] == year and ROUNDS.index(k[2]) >= first:
                del rows[k]
    # The final's draw: the semi-finals' draw (UEFA draws the final's home side with the semi-finals).
    for (comp, year, rnd), (draw, leg1, leg2) in list(rows.items()):
        if rnd == "final" and draw is None:
            sf = rows.get((comp, year, "sf"))
            if not sf:
                fail(f"{comp} {year} final: no semi-final row for its draw")
            rows[(comp, year, rnd)] = (sf[0], leg1, leg2)
    # The game draws each round from the winners of the round before (Cup::draw_round 0x51c410 on the round's draw
    # date), and UEFA draws later rounds with an earlier one (the quarter-finals and semi-finals together, from
    # 2024/25 every round with the round of 16): such a draw is the day after the second leg of the round before.
    for (comp, year, rnd) in sorted(rows, key=lambda k: (k[0], k[1], ROUNDS.index(k[2]))):
        i = ROUNDS.index(rnd)
        before = rows.get((comp, year, ROUNDS[i - 1])) if i else None
        if not before:
            continue
        last = before[2] or before[1]
        draw, leg1, leg2 = rows[(comp, year, rnd)]
        if draw <= last:
            moved.append(f"{comp} {year} {rnd}: draw {draw} after the round before, the day after {last}")
            rows[(comp, year, rnd)] = (last + datetime.timedelta(days=1), leg1, leg2)
    for (comp, year, rnd), (draw, leg1, leg2) in rows.items():
        if not draw <= leg1 or (leg2 and not leg1 < leg2):
            fail(f"{comp} {year} {rnd}: dates out of order")
        for d in (draw, leg1, leg2):
            if d and not 0 <= d.year - year <= 1:
                fail(f"{comp} {year} {rnd}: {d} is not in the season")
    return rows


def render(rows):
    lines = ["/* The real dates of the UEFA knockout rounds (season_uefa.h, season_uefa_ko_dates). Made by",
             " * tools/seasons/make_uefa_dates.py from docs/research/rules-0.6.0/uefa-ko-dates.tsv. Do not edit by hand:",
             " * change the table and run the tool. Fields: competition, season, round (0 play-offs or round of 32, 1",
             " * round of 16, 2 quarter-finals, 3 semi-finals, 4 final), draw day, month (0 to 11), year offset, first",
             " * leg day, month, year offset, days to the second leg (0 for the final). */",
             '#include "season_uefa.h"', "", "const SeasonUefaKoDate season_uefa_ko_dates_table[] = {"]
    for (comp, year, rnd) in sorted(rows, key=lambda k: (k[0], k[1], ROUNDS.index(k[2]))):
        draw, leg1, leg2 = rows[(comp, year, rnd)]
        gap = (leg2 - leg1).days if leg2 else 0
        lines.append(f"    {{'{COMPS[comp]}', {year}, {ROUNDS.index(rnd)}, {draw.day}, {draw.month - 1}, "
                     f"{draw.year - year}, {leg1.day}, {leg1.month - 1}, {leg1.year - year}, {gap}}},")
    lines += ["};", "const int season_uefa_ko_dates_count = (int)(sizeof season_uefa_ko_dates_table / "
              "sizeof season_uefa_ko_dates_table[0]);", ""]
    return "\n".join(lines)


def main(argv):
    text = render(read())
    if "--check" not in argv:
        for line in moved:
            print("moved: " + line)
    if "--check" in argv:
        old = open(OUT, encoding="utf-8").read() if os.path.exists(OUT) else ""
        if old != text:
            print("make_uefa_dates.py: season_uefa_dates_table.c is not the file the table gives: run the tool")
            return 1
        print("PASS")
        return 0
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(text)
    print(f"wrote {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
