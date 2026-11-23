#!/usr/bin/env python3
"""The count changes of the rules table in a game (0.7.2 league sizes spec, D23 and slice 18).

Usage:
    python3 tests/seasons/sizes_count_changes.py <report> [<report> ...] [--first 2002] [--last 2027] [--list]

Each report is a rules report of a game with changing rules on (tests/seasons/rules_report_check.py --run
--changing-rules on --keep <folder>: <folder>/report.tsv). The reports of the two nation groups of
tests/seasons/sizes_full_check.py (r2001-g1, r2001-g2) together hold every league of the game.

A count change is a division file whose club counts in the rules table differ between season S - 1 and season S
(the counts of all rows of the file in the season, so a division of groups counts as one change). For each change
from --first to --last the check needs:
    shown    the game's divisions of that file in season S hold the counts of the table's rows in S (the start or
             the end line of the season, as the clubs check of rules_report_check.py reads them)
    places   the game's divisions of that file in season S - 1 play the promotion, relegation and play-off places
             of the table's rows in S - 1 (no up, down or playoffs difference of rules_report_check.py in S - 1)
A change whose file no report holds in S is listed as not covered and fails: no nation group loaded it. A change of
a division that the spec keeps as it is (KEPT: D16 and D17) is listed as kept and does not fail.
--list prints every change, not only the failures.
"""
import collections
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rules_report_check as rrc  # noqa: E402

PLACE_KINDS = ("up", "down", "playoffs")
# Divisions that the spec keeps as they are (D16: groups of one division, the Ykkonen, the Danish 2nd Divisions;
# D17: the MLS). Their failing changes are listed as "kept", and do not fail the check. The lead decides on them.
KEPT = {"usa_mls": "D17", "fin_first": "D16", "den_second": "D16", "spa_second_b": "D16 groups",
        "por_second_b": "D16 groups", "tur_second_b": "D16 groups", "bra_nat_third": "D16 groups",
        "ita_ser_c2a": "D16 groups", "ita_ser_c2b": "D16 groups", "ita_ser_c2c": "D16 groups",
        "ger_regional": "D16 groups", "fra_cfa": "D16 groups", "fra_lower": "D16 groups", "pol_first": "D16 groups"}
CONFEDERATIONS = {"UEFA", "AFC", "CAF", "CONMEBOL", "CONCACAF", "OFC", "FIFA"}


def table_counts(rows):
    """{(file, season): sorted counts} of the division rows (a numbered tier and an integer count)."""
    out = collections.defaultdict(list)
    for r in rows:
        if not isinstance(r["tier"], int) or r["tier"] < 1 or not isinstance(r["clubs"], int):
            continue
        if r["nation"] in CONFEDERATIONS:               # the club cups of UEFA, AFC and the others hold a tier too
            continue
        if r["comp_file"] in ("na", "new") or "not_held" in (r["clubs"], r["format"]) or rrc.not_played(r):
            continue
        for f in rrc.row_files(r):
            out[(f, r["season"])].append(r["clubs"])
    return {k: sorted(v) for k, v in out.items()}


def changes(counts, first, last):
    """[(file, season, counts before, counts after)]."""
    out = []
    for (f, s), now in sorted(counts.items()):
        before = counts.get((f, s - 1))
        if first <= s <= last and before is not None and before != now:
            out.append((f, s, before, now))
    return out


def game_counts(comps):
    """{(file, season): [sorted counts of the start line, sorted counts of the end line]} of the leagues."""
    per = collections.defaultdict(lambda: {"start": [], "end": []})
    leagues = [c for c in comps if c["kind"] == "league"]
    first = {}
    for c in leagues:
        k = (c["file"], c["season"], c["phase"])
        first[k] = min(first.get(k, c["year"]), c["year"])
    for c in leagues:
        # An idle group waits for a later season (season_divisions.c gre_swap: the Greek group of record 459 holds
        # edition 2020 from 2015/16 to 2020/21 and is never played). Its edition is past season + 1, or later than
        # another edition of the same file in the season (2019/20: 2019 and 2020). league_size_run.py skips it too.
        if c["year"] <= c["season"] + 1 and c["year"] == first[(c["file"], c["season"], c["phase"])]:
            per[(c["file"], c["season"])][c["phase"]].append(c["clubs"])
    return {k: [sorted(v["start"]), sorted(v["end"])] for k, v in per.items()}


def main(argv):
    reports = [a for i, a in enumerate(argv[1:], 1) if not a.startswith("--") and argv[i - 1] not in ("--first",
                                                                                                      "--last")]
    if not reports:
        print(__doc__)
        return 2
    first = int(rrc.arg(argv, "--first", 2002))
    last = int(rrc.arg(argv, "--last", 2027))
    rows = rrc.make_rules.read_rows()
    todo = changes(table_counts(rows), first, last)
    games, place_diffs = {}, set()
    for path in reports:
        seasons, comps = rrc.read_report(path)
        games.update(game_counts(comps))
        for s, kind, text in rrc.differences(seasons, comps, rows):
            if kind in PLACE_KINDS:
                place_diffs.add((s, text.split(" ", 1)[0], f"{kind}: {text}"))
    failed = n_kept = 0
    for f, s, before, now in todo:
        got = games.get((f, s))
        fails = []
        if got is None:
            fails.append("not covered: no report holds the file in this season")
        elif now not in got:
            fails.append(f"shown: the game holds {'/'.join(str(g) for g in got)}, the table {now}")
        fails += [t for (ps, pf, t) in sorted(place_diffs) if ps == s - 1 and pf == f]
        kept = KEPT.get(f) if fails else None
        failed += bool(fails) and not kept
        n_kept += bool(kept)
        if fails or "--list" in argv:
            word = f"kept {kept}" if kept else "FAIL" if fails else "ok  "
            print(f"{word}  {f} {s - 1}/{s % 100:02d} -> {s}/{(s + 1) % 100:02d}: "
                  f"{before} -> {now}")
            for t in fails:
                print(f"        {t}")
    print(f"{len(todo)} count changes from {first} to {last}, {failed} fail, {n_kept} differ in divisions the spec "
          f"keeps (D16, D17)")
    print("PASS" if not failed else f"FAILED: {failed}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
