#!/usr/bin/env python3
"""The abbreviation column of the names table (0.6.1 item 2).

Usage:
    python3 -I tests/seasons/names_abbr_check.py

A competition record holds a 3-character abbreviation at +0x53 (4 bytes with the 0), next to the long and short
name. Select Team shows it for the division of each club. The 3.9.68 data gives the 2001 names: "PRM", "D1",
"D2" and "D3" for the four English divisions. The names table (tools/seasons/make_names.py) gives each league
row of a later season an abbreviation of its shown name, and season_names.c writes it with the names.

Checks:
    - every league row (a number in the tier column of the rules table) has an abbreviation of 1 to 3
      characters that Windows-1252 holds, with no space or tab; a cup row has none;
    - the English divisions of 2009/10 and 2004/05 give PL, CH, L1 and L2;
    - some other nations of the rename: Serie A and B, the German 3. Liga, Ligue 1, Primera División;
    - the table that --out writes has format 2 and six columns.
"""
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "seasons"))
import make_names  # noqa: E402
import make_rules  # noqa: E402

WANT = {
    (2009, "eng_prm"): "PL", (2009, "eng_first"): "CH", (2009, "eng_second"): "L1", (2009, "eng_third"): "L2",
    (2004, "eng_prm"): "PL", (2004, "eng_first"): "CH", (2004, "eng_second"): "L1", (2004, "eng_third"): "L2",
    (2009, "ita_ser_a"): "A", (2009, "ita_ser_b"): "B", (2009, "ger_first"): "BL", (2009, "ger_second"): "2BL",
    (2009, "ger_regional"): "3L", (2009, "fra_first"): "L1", (2009, "fra_second"): "L2",
    (2009, "spa_first"): "D1", (2009, "spa_second"): "D2", (2009, "spa_second_b"): "D2B",
}


def main():
    errors = []
    rows = make_rules.read_rows()
    out = make_names.table(rows)
    if not out or len(out[0]) != 6:
        print("names_abbr_check: make_names.table gives no abbreviation column")
        return 1
    leagues = 0
    for season, file, n, long, short, abbr in out:
        if abbr is None:
            continue
        leagues += 1
        if not 1 <= len(abbr) <= 3 or " " in abbr or "\t" in abbr:
            errors.append(f"{season} {file} {n}: abbreviation {abbr!r} of {short!r} is not 1 to 3 characters")
        try:
            abbr.encode("cp1252")
        except UnicodeEncodeError:
            errors.append(f"{season} {file} {n}: abbreviation {abbr!r} is not Windows-1252")
    got = {(int(s), f): a for s, f, n, lg, sh, a in out if n == 0}
    for key, want in WANT.items():
        if got.get(key) != want:
            errors.append(f"{key[0]} {key[1]}: abbreviation {got.get(key)!r}, want {want!r}")
    cups = [(s, f) for s, f, n, lg, sh, a in out if f in ("eng_fa_cup", "eng_cc_cup", "ita_cup") and a is not None]
    if cups:
        errors.append(f"cup rows with an abbreviation: {cups[:5]}")
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "names.tsv")
        if make_names.main(["--out", path]):
            errors.append("make_names.py --out failed")
        else:
            lines = open(path, encoding="cp1252").read().split("\n")
            if not lines[0].startswith("#bosman-names\t2\t") or lines[1] != "season\tfile\tn\tlong\tshort\tabbr":
                errors.append(f"the table header is {lines[:2]}")
            if any(line and line.count("\t") != 5 for line in lines[2:]):
                errors.append("a row of the table has not six columns")
    for e in errors:
        print("FAIL  " + e)
    if errors:
        return 1
    print(f"names_abbr_check: {leagues} league rows of {len(out)} have an abbreviation")
    return 0


if __name__ == "__main__":
    sys.exit(main())
