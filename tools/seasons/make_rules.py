#!/usr/bin/env python3
"""Makes the rules table of the game from the research parts (0.6.0 slice 12).

Usage:
    python3 -I tools/seasons/make_rules.py --out <rules.tsv>     write the table for the game
    python3 -I tools/seasons/make_rules.py --check               read every part, write nothing

Input: the 20 parts docs/research/rules-0.6.0/<part>.tsv (one row per league or competition per season, the
columns in the first line). The club moves, academy sides, signatures and the research evidence (.check,
.reconcile, .md) are not parts.

Output: one UTF-8 file with tabs, for Seasons/rules.tsv of the download. The game reads it at the start
(src/recomp_rt/win/season_rules.c). Line 1 is "#bosman-rules<TAB>1<TAB><row count>", line 2 holds the column
names, then one line per row in the order of the parts (sorted by name) and of their lines.

The structured columns become values:
    id          as it is, unique in the table
    season      the start year: 2001 for 2001/02
    nation      as it is
    comp_file   the source file of the original's class without ".cpp" (eng_prm), "new" (no class in the
                original), "na" (no competition: nation rules, the access list), or two files with ";"
    comp, name  as they are
    tier        a number (1 is the top division), or cup, super_cup, state_1, international,
                national_teams, world, nation, na
    clubs, up, down, subs_used, bench
                a count, or a code: na, unknown, not_held, see_each (see each competition), as_cl (as the
                Champions League), text (a description with no single count). A count is the number that the
                text starts with, the sum for "N + M" and "N direct + M via ...", the number in brackets for a
                place range such as "15th-16th (2)", and N for "up to N", "max N", "all N ..." and "bottom N".
                "none" and "0 ..." give 0.
    playoffs, split, format
                a code: na, unknown, none, not_held, see_format, see_split, next_season, or text
Each of the eight count and code columns also keeps its text in <name>_text. The other columns (real_season,
foreign, work_permit, registration, windows, notes, sources) are kept as text for slice 17. A tab or line
break in a value is not possible in the parts (the TSV reader would split it).

The stop rule: a row that the tool cannot read stops it with exit code 1 and names the part, the line and the
column: a wrong column count, an empty or repeated id, a season that is not Y/(Y+1) or not from 2001 to
2099, a comp_file that is not a file name, or a count that fits none of the forms above or is over 100000.
The game's reader (src/recomp_rt/win/season_rules.c) has the same limits.
"""
import argparse
import csv
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
PARTS = os.path.join(ROOT, "docs", "research", "rules-0.6.0")
NOT_PARTS = ("-clubs.tsv", "-academy.tsv", ".check.tsv", ".check2.tsv", ".reconcile.tsv", "databases-signatures.tsv",
             "databases-date-shifts.tsv", "databases-date-shifts-last-rows.tsv", "databases-date-shifts-careers.tsv",  # slice 11b
             "databases-date-shifts-nation-comps.tsv",  # 0.6.2 slice 7
             "uefa-access.tsv",  # slice 23: the UEFA access list by rank (tools/seasons/make_uefa_table.py)
             "nations-league-tiers.tsv", "nations-league-dates.tsv",  # slice 28
             "uefa-ko-dates.tsv")  # O259: the UEFA knockout dates (tools/seasons/make_uefa_dates.py)
COLUMNS = ["id", "season", "real_season", "nation", "comp_file", "comp", "tier", "name", "clubs", "up", "down",
           "playoffs", "split", "format", "subs_used", "bench", "foreign", "work_permit", "registration",
           "windows", "notes", "sources"]
COUNTS = ["clubs", "up", "down", "subs_used", "bench"]
CODES = ["playoffs", "split", "format"]
TEXTS = ["real_season", "foreign", "work_permit", "registration", "windows", "notes", "sources"]
OUT_COLUMNS = (["id", "season", "nation", "comp_file", "comp", "tier", "name"] + COUNTS + CODES
               + [c + "_text" for c in COUNTS + CODES] + TEXTS)
FORMAT = 1
# The limits of src/recomp_rt/win/season_rules.h: a row outside them stops the tool, so it never reaches a game.
FIRST_SEASON, LAST_SEASON = 2001, 2099
MAX_COUNT = 100000

TIER_CODES = {"cup": "cup", "super cup": "super_cup", "state 1": "state_1", "international": "international",
              "national teams": "national_teams", "world": "world", "nation": "nation", "n/a": "na"}
# Texts that describe places or a field with no single count. They are kept with the code "text".
DESCRIBED = re.compile(
    r"^(path: |champions only rounds: |veikkausliiga clubs|all veikkausliiga clubs and |group winners |"
    r"vv |top 2 of each zone |varies |bottom clubs |last of each group |last-placed teams |last direct |"
    r"5th and 6th |two worst |zone relegation groups|\d+(?:st|nd|rd|th) v )")
ORDINAL = r"\d+(?:st|nd|rd|th)"


class Stop(Exception):
    pass


def count(text):
    """The value of a count column: an int from 0 to MAX_COUNT or a code (module docstring)."""
    v = count_value(text)
    if isinstance(v, int) and v > MAX_COUNT:
        raise Stop(f"the count {v} is larger than {MAX_COUNT}")
    return v


def count_value(text):
    t = text.strip()
    low = t.lower()
    if not t:
        raise Stop("empty")
    if low.startswith("n/a"):
        return "na"
    if low.startswith("unknown"):
        return "unknown"
    if low.startswith("not held"):
        return "not_held"
    if low.startswith("none"):
        return 0
    if low.startswith("see each competition"):
        return "see_each"
    if low.startswith("as champions league"):
        return "as_cl"
    m = re.match(r"^(\d+) direct \+ (\d+) via\b", low) or re.match(r"^(\d+) \+ (\d+)\b", low)
    if m:
        return int(m.group(1)) + int(m.group(2))
    m = re.match(r"^(\d+)\b", low)
    if m:
        return int(m.group(1))
    m = re.match(rf"^{ORDINAL}(?:-{ORDINAL})? \((\d+)\)", low)
    if m:
        return int(m.group(1))
    m = (re.match(r"^(?:up to|max) (\d+)\b", low) or re.match(r"^all (\d+) ", low)
         or re.match(r"^bottom (\d+)\b", low))
    if m:
        return int(m.group(1))
    if DESCRIBED.match(low):
        return "text"
    raise Stop(f"no count in {t[:60]!r}")


def code(text):
    t = text.strip().lower()
    for prefix, c in (("n/a", "na"), ("unknown", "unknown"), ("none", "none"), ("not held", "not_held"),
                      ("see format", "see_format"), ("see split", "see_split"), ("next season", "next_season")):
        if t.startswith(prefix):
            return c
    return "text" if t else "na"


def season(text):
    m = re.match(r"^(\d{4})/(\d{2})$", text)
    if not m or (int(m.group(1)) + 1) % 100 != int(m.group(2)):
        raise Stop(f"season {text!r} is not Y/(Y+1)")
    if not FIRST_SEASON <= int(m.group(1)) <= LAST_SEASON:
        raise Stop(f"season {text!r} is not from {FIRST_SEASON} to {LAST_SEASON}")
    return int(m.group(1))


def comp_file(text):
    t = text.strip()
    if t.startswith("n/a"):
        return "na"
    if t == "new":
        return "new"
    files = []
    for piece in t.split(";"):
        name = piece.strip().split(" ")[0]
        if name.endswith(".cpp"):
            name = name[:-4]
        if not re.match(r"^[a-z][a-z0-9_]*$", name):
            raise Stop(f"comp_file {t[:60]!r} is not a file name")
        files.append(name)
    return ";".join(files)


def tier(text):
    t = text.strip()
    if re.match(r"^\d+$", t):
        return int(t)
    if t in TIER_CODES:
        return TIER_CODES[t]
    raise Stop(f"tier {t!r}")


def parts():
    out = []
    for path in sorted(glob.glob(os.path.join(PARTS, "*.tsv"))):
        if not path.endswith(NOT_PARTS):
            out.append(path)
    return out


def read_rows():
    rows, seen = [], set()
    found = parts()
    if len(found) != 20:
        raise SystemExit(f"make_rules.py: {len(found)} parts in {PARTS}, 20 expected")
    for path in found:
        name = os.path.basename(path)
        with open(path, newline="", encoding="utf-8") as f:
            lines = list(csv.reader(f, delimiter="\t", quoting=csv.QUOTE_NONE))
        if lines[0] != COLUMNS:
            raise SystemExit(f"make_rules.py: {name}: the columns are not those of the table")
        for n, cells in enumerate(lines[1:], start=2):
            col = "row"
            try:
                if len(cells) != len(COLUMNS):
                    raise Stop(f"{len(cells)} columns, {len(COLUMNS)} expected")
                r = dict(zip(COLUMNS, cells))
                col = "id"
                if not r["id"] or r["id"] in seen:
                    raise Stop(f"id {r['id']!r} is empty or repeated")
                seen.add(r["id"])
                v = {"id": r["id"], "nation": r["nation"], "comp": r["comp"], "name": r["name"]}
                col = "season"
                v["season"] = season(r["season"])
                col = "comp_file"
                v["comp_file"] = comp_file(r["comp_file"])
                col = "tier"
                v["tier"] = tier(r["tier"])
                for c in COUNTS:
                    col = c
                    v[c] = count(r[c])
                for c in CODES:
                    v[c] = code(r[c])
                for c in COUNTS + CODES:
                    v[c + "_text"] = r[c]
                for c in TEXTS:
                    v[c] = r[c]
            except Stop as e:
                raise SystemExit(f"make_rules.py: {name} line {n}, column {col}: {e}")
            rows.append(v)
    return rows


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--out")
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args(argv)
    if not a.out and not a.check:
        ap.error("give --out or --check")
    rows = read_rows()
    if a.check:
        print(f"make_rules.py: {len(rows)} rows read")
        return 0
    lines = [f"#bosman-rules\t{FORMAT}\t{len(rows)}", "\t".join(OUT_COLUMNS)]
    for v in rows:
        lines.append("\t".join(str(v[c]) for c in OUT_COLUMNS))
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    tmp = a.out + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    os.replace(tmp, a.out)
    print(f"make_rules.py: {len(rows)} rows written to {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
