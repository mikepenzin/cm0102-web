#!/usr/bin/env python3
"""Makes src/recomp_rt/win/season_history_rows_table.c from docs/research/history-rows/ (issues 145 and 165).

Usage:
    python3 -I tools/seasons/make_history_rows_table.py           writes the C table
    python3 -I tools/seasons/make_history_rows_table.py --check   exits 1 when the C table is not the one the
                                                                  research gives, or a row is not valid

A row is not valid when it has no source, no winner or no guard, when a name is longer than 50 bytes in Latin-1 (the
game keeps 51 bytes with the end byte), when a list row holds before the edition was decided (from_start below its
label), or when a list or correction names one edition twice.
"""
import csv
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
DOCS = os.path.join(ROOT, "docs", "research", "history-rows")
OUT = os.path.join(ROOT, "src", "recomp_rt", "win", "season_history_rows_table.c")
NAME_MAX = 50
FIELDS = {"first": 0, "second": 1, "third": 2, "host": 3}


def read(name):
    with open(os.path.join(DOCS, name), newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def c_str(s):
    out = []
    for ch in s.encode("latin-1"):
        if ch in (0x22, 0x5c):
            out.append("\\" + chr(ch))
        elif 0x20 <= ch < 0x7f and ch != 0x3f:      # '?' is escaped: no trigraphs
            out.append(chr(ch))
        else:
            out.append("\\%03o" % ch)
    return '"' + "".join(out) + '"'


def names_bad(where, text):
    bad = []
    for n in text.split("|"):
        try:
            if len(n.encode("latin-1")) > NAME_MAX:
                bad.append(f"{where}: {n!r} is longer than {NAME_MAX} bytes")
        except UnicodeEncodeError:
            bad.append(f"{where}: {n!r} is not Latin-1")
    return bad


def validate(lists, fixes, clears):
    bad, seen = [], set()
    for r in lists:
        where = f"lists.tsv {r['competition']} {r['label']}"
        key = (r["competition"], r["label"])
        if key in seen:
            bad.append(f"{where}: held twice")
        seen.add(key)
        if not r["first"]:
            bad.append(f"{where}: no winner")
        if not r["sources"].strip():
            bad.append(f"{where}: no source")
        if int(r["from_start"]) < int(r["label"]) and int(r["label"]) > 2001:
            bad.append(f"{where}: holds from a start before its edition")
        for f in FIELDS:
            bad += names_bad(where, r[f])
    seen = set()
    for r in fixes:
        where = f"corrections.tsv {r['competition']} {r['label']} {r['field']}"
        key = (r["competition"], r["label"], r["field"])
        if key in seen:
            bad.append(f"{where}: held twice")
        seen.add(key)
        if r["field"] not in FIELDS:
            bad.append(f"{where}: unknown field")
        if not r["guard_first"]:
            bad.append(f"{where}: no guard")
        if not r["sources"].strip():
            bad.append(f"{where}: no source")
        for f in ("guard_first", "accept", "write"):
            bad += names_bad(where, r[f])
    for r in clears:
        if not r["sources"].strip():
            bad.append(f"clears.tsv {r['competition']}: no source")
    return bad


def table():
    lists, fixes, clears = read("lists.tsv"), read("corrections.tsv"), read("clears.tsv")
    bad = validate(lists, fixes, clears)
    if bad:
        raise SystemExit("docs/research/history-rows has rows that are not valid:\n  " + "\n  ".join(bad))
    out = ["/* Made by tools/seasons/make_history_rows_table.py from docs/research/history-rows/. Do not edit. */",
           '#include "season_history.h"', "", "const SeasonHistListRow season_history_list_rows[] = {"]
    for r in lists:
        places = ", ".join(c_str(r[f]) for f in FIELDS)
        out.append(f"    {{{c_str(r['competition'])}, {int(r['label'])}, {int(r['from_start'])}, {{{places}}}}},")
    out += ["};", "const int season_history_list_rows_count = (int)(sizeof season_history_list_rows / "
            "sizeof season_history_list_rows[0]);", "", "const SeasonHistRowFix season_history_row_fixes[] = {"]
    for r in fixes:
        out.append(f"    {{{c_str(r['competition'])}, {int(r['label'])}, {FIELDS[r['field']]}, "
                   f"{c_str(r['guard_first'])}, {c_str(r['accept'])}, {c_str(r['write'])}}},")
    out += ["};", "const int season_history_row_fixes_count = (int)(sizeof season_history_row_fixes / "
            "sizeof season_history_row_fixes[0]);", "", "const SeasonHistClearFrom season_history_clears_from[] = {"]
    for r in clears:
        out.append(f"    {{{c_str(r['competition'])}, {int(r['from_label'])}}},")
    out += ["};", "const int season_history_clears_from_count = (int)(sizeof season_history_clears_from / "
            "sizeof season_history_clears_from[0]);", ""]
    return "\n".join(out)


def main(argv):
    text = table()
    if "--check" in argv:
        have = open(OUT, encoding="latin-1", newline="").read() if os.path.exists(OUT) else ""
        if have.replace("\r\n", "\n") != text:
            print("season_history_rows_table.c is not the table of docs/research/history-rows: run the tool")
            return 1
        print("season_history_rows_table.c ok")
        return 0
    with open(OUT, "w", encoding="latin-1", newline="") as f:
        f.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
