#!/usr/bin/env python3
"""Writes the script steps that the server gives the game (RECOMP_SCRIPT), from the steps of the test runs, so that
both click the same places.

Usage: python3 server/steps/export_steps.py [--check]

- new-game.txt: the new game of the smoke run (tests/smoke/smoke_run.py STEPS) up to the first news screen, without
  the screen dumps: Test Smoke, England, Arsenal. The game reaches its first human turn there, where the engine
  worker stops (src/recomp_rt/win/worker.c).
- load.txt: the load of the first save of the user folder (tests/fixtures/fixtures.py LOAD_STEPS).

Both end at their last click, without the "wait first-news" of the test runs. The worker stops the game at the
human turn before the news screen is drawn, and the wait counts real time while the server thinks: a wait that is
still open when the game goes on ends the program with code 3 after 120 s (wait_done, WAIT_DEFAULT_MS in
src/recomp_rt/win/user32.c). With no step left, the script does nothing more.

Both are for classic mode (800x600); the server writes "display = classic" into the settings file of each run.
With --check the tool changes nothing and fails when a file differs from what it would write (the server test runs
it). """
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tests", "fixtures"))
sys.path.insert(0, os.path.join(ROOT, "tests", "smoke"))
import fixtures  # noqa: E402
import smoke_run  # noqa: E402

HEAD = "# Made by server/steps/export_steps.py from {}. Do not edit: run the tool again.\n"


def files():
    new = [s for s in smoke_run.STEPS if " dump " not in s]
    new = new[:new.index("wait first-news")]
    load = fixtures.LOAD_STEPS[:fixtures.LOAD_STEPS.index("wait first-news")]
    return {
        "new-game.txt": HEAD.format("tests/smoke/smoke_run.py STEPS")
                        + "\n".join(smoke_run.scaled_steps("classic", "", new)) + "\n",
        "load.txt": HEAD.format("tests/fixtures/fixtures.py LOAD_STEPS")
                    + "\n".join(smoke_run.scaled_steps("classic", "", load)) + "\n",
    }


def main():
    check = "--check" in sys.argv[1:]
    bad = 0
    for name, text in files().items():
        path = os.path.join(HERE, name)
        old = open(path, encoding="utf-8").read() if os.path.exists(path) else None
        if old == text:
            continue
        if check:
            print(f"differs: {path}")
            bad += 1
        else:
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)
            print(f"wrote {path}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
