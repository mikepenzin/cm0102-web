#!/usr/bin/env python3
"""Tests the choice of match steps in smoke_run.py (round AA, items 155 and 185): a run plays MATCH_EVENT only when
its build has the original-faults group match_numbers and the run does not keep it. A reference build from before
item 155 (no match_numbers) and a run with keep:match_numbers play MATCH_STEPS_ORIGINAL.

    python3 -I tests/smoke/match_steps_test.py
"""
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smoke_run  # noqa: E402


def fake_build(folder, name, groups):
    path = os.path.join(folder, name)
    with open(path, "wb") as f:
        f.write(b"\x7fELF...\0managers\0money\0" + b"".join(g.encode() + b"\0" for g in groups) + b"tail")
    return path


def main():
    bad = 0

    def check(ok, text):
        nonlocal bad
        print(("ok    " if ok else "FAIL  ") + text)
        bad += not ok

    with tempfile.TemporaryDirectory() as d:
        new = fake_build(d, "new", ["callups", "match_numbers"])
        old = fake_build(d, "old", ["callups", "club_regulars"])
        near = fake_build(d, "near", ["match_numbers_x"])      # a longer name is not the group
        os.environ.pop("RECOMP_ORIGINAL_FAULTS", None)
        check(smoke_run.match_event(smoke_run.program_keep(), new), "a build with match_numbers plays MATCH_EVENT")
        check(not smoke_run.match_event(smoke_run.program_keep(), old),
              "a reference build without match_numbers plays the original's match")
        check(not smoke_run.match_event("", near), "only the whole name counts")
        check(not smoke_run.match_event("keep", new), "keep keeps match_numbers")
        check(not smoke_run.match_event("keep:confed,match_numbers", new), "keep:<list> with match_numbers")
        check(smoke_run.match_event("keep:confed,club_regulars", new), "keep:<list> without match_numbers")
        os.environ["RECOMP_ORIGINAL_FAULTS"] = "keep:match_numbers"
        check(not smoke_run.match_event(smoke_run.program_keep(), new), "a set-wide keep:match_numbers")
        check(not smoke_run.match_event(smoke_run.program_keep({"RECOMP_ORIGINAL_FAULTS": "keep:money"}), new),
              "a run's own keep list merges the set-wide one")
        os.environ["RECOMP_ORIGINAL_FAULTS"] = "keep:confed"
        check(smoke_run.match_event(smoke_run.program_keep({"RECOMP_ORIGINAL_FAULTS": smoke_run.KEEP_IN_RUNS}), new),
              "original-faults-keep (KEEP_IN_RUNS) leaves match_numbers on")
        os.environ.pop("RECOMP_ORIGINAL_FAULTS", None)
    print("FAIL" if bad else "PASS")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
