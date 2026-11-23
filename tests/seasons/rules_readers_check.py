#!/usr/bin/env python3
"""Every reader of the rules table that a file of the Irish calendar move can reach maps an edition to its row in one
way (gap row 2 of 0.7.2, season_rules.h season_rules_row_of).

Usage: python3 -I tests/seasons/rules_readers_check.py

A file of src/recomp_rt/win that calls season_rules_find or season_rules_last_season must be listed below with why its
reads are right for the Irish files (ire_*): it calls season_rules_row_of for them, or it reads only files that are not
Irish. A new reader, or a listed file whose stated call is gone, fails the check. Exit code 1 on a failure.
"""
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
WIN = os.path.join(ROOT, "src", "recomp_rt", "win")

# file: (a call the file must hold, why).
READERS = {
    "season_rules.c": ("season_rules_row_of(", "the table and the mapping itself"),
    "season_league.c": ("season_league_edition_row(", "the league rows of an edition: row Y, else a row of Y - 1 "
                        "whose real season begins with Y (the same rule; season_sizes.c reads through it)"),
    "season_cup_breaks.c": ("season_rules_row_of(", "the Irish cups while the move is on"),
    "season_kind_subs.c": ("season_rules_row_of(", "the substitutes: the season of the edition's row"),
    "season_kind_nation.c": ("season_rules_row_of(", "the nation of a file and its tier"),
    "season_playoffs.c": ("season_rules_row_of(", "the play-off places of a league's edition"),
    "season_subs_rule.c": ("season_rules_find(", "the rows of the season that season_kind_subs.c gives it"),
    "season_bra_states.c": ("season_rules_find(", "the Brazilian state championships only"),
    "season_club_editions.c": ("season_rules_find(", "the continental club cups only"),
    "season_divisions.c": ("season_rules_find(", "the new divisions (Greek, Spanish, Portuguese, German) only"),
    "season_kind_club_world.c": ("season_rules_find(", "the club competitions outside UEFA only"),
    "season_playoff_winners.c": ("season_rules_find(", "the Brazilian Série B only"),
    "season_pol_first.c": ("season_rules_find(", "the Ekstraklasa only"),
    "season_mls.c": ("season_rules_find(", "the MLS only (gap row 7)"),
    "season_closed_step.c": ("season_rules_find(", "the A-League (aus_nsl) only"),
    "season_sizes_arg_second.c": ("season_rules_find(", "the Argentine second division (Primera B Nacional) only"),
    "season_sizes_places.c": ("season_rules_find(", "J1 and J2 only"),
    "season_uefa.c": ("season_rules_find(", "the UEFA Cup only"),
    "season_report.c": ("season_rules_find(", "the rules report of the tests, the row of each season, which "
                        "tests/seasons/rules_report_check.py compares with the table by its own rule"),
}
CALL = re.compile(r"\bseason_rules_(find|last_season)\s*\(")


def main():
    fails = []
    for name in sorted(os.listdir(WIN)):
        if not name.endswith(".c"):
            continue
        text = open(os.path.join(WIN, name), encoding="utf-8", errors="replace").read()
        reads = bool(CALL.search(text))
        if reads and name not in READERS:
            fails.append(f"{name} reads the rules table and is not listed")
        if name in READERS and READERS[name][0] not in text:
            fails.append(f"{name}: no {READERS[name][0]} ({READERS[name][1]})")
    for f in fails:
        print("FAIL", f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
