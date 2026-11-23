"""Item 257 (0.7.2): the new-game pair rules-on and history-q48-off of the same-save check is defined with no rules
table (RULES_ON_PARTS in smoke_run.py lists the parts that a new game with changing rules on and no Seasons/rules.tsv
changes). With a table, changing rules on applies the 2001/02 rows of the table at the new game (club moves, new
competitions, names, the .lng fit), so the pair differs in most parts by design. A set run with RECOMP_RULES_TABLE in
its environment (the build's own seasons/) therefore failed in about 118 parts.

The check: rules-on of a new game always runs with no table (RECOMP_RULES_TABLE set to NO_TABLE, a path that does
not exist, which the game reads as no table: season_rules_init and season_data_path; compare_runs fails the run when
its log does not say "rules table: none"), and history-q48-off keeps the environment of the set. So the pair stays
the comparison it was made for, and with a table set the pair also shows that the base game (changing rules off)
with the table equals the base game with none. A loaded save is not changed.

Run: python3 -I tests/smoke/rules_pair_table_check.py (ctest same_save_rules_pair_table)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smoke_run  # noqa: E402


def main():
    failed = 0

    def expect(label, ok):
        nonlocal failed
        print(f"{'ok  ' if ok else 'FAIL'}  {label}")
        failed += not ok

    new = dict(smoke_run.q48_runs(smoke_run.new_game_runs(smoke_run.SAME_SAVE_RUNS), True))
    expect("new game: rules-on runs with no rules table",
           new["rules-on"].get("_env", {}).get("RECOMP_RULES_TABLE") == smoke_run.NO_TABLE)
    expect("new game: rules-on is compared with history-q48-off", new["rules-on"].get("_against") == "history-q48-off")
    expect("new game: history-q48-off keeps the table of the set",
           "RECOMP_RULES_TABLE" not in new["history-q48-off"].get("_env", {}))
    expect("new game: rules-on-q48 keeps the table of the set",
           "RECOMP_RULES_TABLE" not in new["rules-on-q48"].get("_env", {}))
    expect("NO_TABLE names no folder that exists", not os.path.exists(os.path.dirname(smoke_run.NO_TABLE)))
    loaded = dict(smoke_run.q48_runs(smoke_run.SAME_SAVE_RUNS, False))
    expect("loaded save: rules-on keeps the table of the set",
           "RECOMP_RULES_TABLE" not in loaded["rules-on"].get("_env", {}))
    print("FAIL" if failed else "PASS")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
