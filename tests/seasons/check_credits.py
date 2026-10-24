#!/usr/bin/env python3
"""Checks the credit table of the Choose Season screen against the signatures table (0.6.0, Q147, Q269, Q340, Q351).

Usage: python3 -I tests/seasons/check_credits.py

The credits of src/recomp_rt/win/season_screen.c (season_screen_credit) name databases of
docs/research/rules-0.6.0/databases-signatures.tsv. A database that the table does not name gets the Update
Team credit, so a misspelt name would silently give the wrong credit. The checks:
- every name of the credit table is a database of the signatures table;
- the official data sets (roles official version and official base) credit Sports Interactive;
- our own season databases (OWN) credit the Bosman Edition and the Update Team's ratings, and Summer 2002
  (OWN_SI: Xbox facts and 3.9.68 ratings) the Bosman Edition and Sports Interactive;
- no other database is in the credit table (they all take the Update Team credit).
Exit code 1 on a failed check.
"""
import csv
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SCREEN = os.path.join(ROOT, "src", "recomp_rt", "win", "season_screen.c")
SIGNATURES = os.path.join(ROOT, "docs", "research", "rules-0.6.0", "databases-signatures.tsv")
OWN = {"Summer 2004", "Summer 2006"}
OWN_SI = {"Summer 2002"}          # facts and ratings of Sports Interactive (spec 2026-10-24, Q13)
CREDIT_SI = "Data by Sports Interactive"
CREDIT_OWN = "Bosman Edition database, ratings by the CM 01/02 Update Team"
CREDIT_OWN_SI = "Bosman Edition database, data by Sports Interactive"


def main():
    src = open(SCREEN, encoding="utf-8").read()
    block = re.search(r"static const Credit credits\[\] = \{(.*?)\n\};", src, re.S)
    if not block:
        print("FAIL  no credit table in season_screen.c")
        return 1
    credits = dict(re.findall(r'\{"([^"]+)", "([^"]+)"\}', block.group(1)))
    with open(SIGNATURES, newline="", encoding="utf-8") as f:
        roles = {r["database"]: r["role"] for r in csv.DictReader(f, delimiter="\t")}
    fails = []
    for name in credits:
        if name not in roles:
            fails.append(f"credit for {name!r}, which the signatures table does not know")
    for name, role in roles.items():
        want = (CREDIT_SI if role.startswith("official") else CREDIT_OWN if name in OWN else
                CREDIT_OWN_SI if name in OWN_SI else None)
        if credits.get(name) != want:
            fails.append(f"{name} ({role}): credit {credits.get(name)!r}, expected {want!r}")
    for name in (OWN | OWN_SI) - set(roles):
        fails.append(f"{name} is not in the signatures table")
    for line in fails:
        print("FAIL ", line)
    print(f"{len(credits)} credits, {len(roles)} known databases: " + ("FAIL" if fails else "all checks passed"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
