#!/usr/bin/env python3
"""Plays the script of the new game and the start of the match check on a slow machine (item 154).

    python3 tests/smoke/slow_machine_run.py <cm0102_recomp> <game folder> [--part newgame|match|both]
        [--times n] [--stop ms] [--run ms] [--out folder]

It is a check to run by hand, not a ctest: it depends on timing. Before the fix (main 0d32841c) the new game
failed 3 of 3 runs and the match 2 of 3; with the fix both passed 3 of 3 (run-logs/0.6.2-s154, t-base,
t-base-m2, t-fix, t-fix-m2).

Under a load of 15 to 18, three same-save runs of 6 November 2026 ended in a wait that timed out (code 3):
the script sent a key press or the up of a click before the game could take it. This test makes the slow
machine itself: a thread stops the game (SIGSTOP) for --stop ms after each --run ms, so some frames of the
game take longer than the times of the script (100 ms from the down of a click to its up, 100 ms between
typed letters, 500 ms from a reached day to the next Space). Each run plays the steps of smoke_run.py,
only slower. The defaults: --stop 600 --run 150 for the new game, --stop 1000 --run 100 for the match (on
a new day Continue Game comes 0.1 to 0.6 s after the date at the milder throttle, too close to the 500 ms).

Each run must reach the end of its script:
    newgame  smoke_run.STEPS up to the holiday, with the manager named Test Smoke (no typed letter lost;
             the left bar of the news screen names the manager) and a save one game day later: the
             holiday clicks took (item 185)
    match    smoke_run.MATCH_STEPS after the load of prematch.sav up to the kick-off: the Pause button
             shows. The five Spaces of the days 16.7.01 to 20.7.01 and the Commentary Speed popup come
             before it. The test stops there: under this throttle the match itself can play another way
             (one run of three on 6 November met the tactics screen 0x899ea0 in the second half), and that
             is not a lost key press.
A run that ends with code 3 fails, and the test prints the line of the wait that timed out. The stderr of
each run is in --out (RECOMP_SCREEN_LOG=1 prints each change of the screen state).
"""
import os
import random
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fixtures"))
import smoke_run  # noqa: E402
import fixtures  # noqa: E402


def new_game_steps():
    """smoke_run.STEPS to the holiday: the run saves one game day after it (RECOMP_SAVE_AFTER_DAYS=1) and ends."""
    return [s for s in smoke_run.STEPS if " dump " not in s and not s.endswith(" quit")] + ["300000 quit"]


def match_steps():
    """The load of the match check and its steps to the kick-off, then a wait for the Pause button: the kick-off
    took."""
    end = next(i for i, s in enumerate(smoke_run.MATCH_STEPS) if s.endswith("click 45 120"))
    return smoke_run.without_intro_click(smoke_run.load_steps(fixtures)) + smoke_run.MATCH_STEPS[:end + 1] + \
        ["wait label:Pause", "1000 quit"]


def throttle(proc, run_ms, stop_ms, seed, done):
    """Stops and continues the game: each cycle runs it for run_ms and stops it for a time of 0.5 to 1.5
    times stop_ms (seeded, so a run is the same cycle each time)."""
    rnd = random.Random(seed)
    while not done.is_set() and proc.poll() is None:
        time.sleep(run_ms / 1000)
        if done.is_set() or proc.poll() is not None:
            break
        try:
            os.kill(proc.pid, signal.SIGSTOP)
            time.sleep(stop_ms * rnd.uniform(0.5, 1.5) / 1000)
        finally:
            try:
                os.kill(proc.pid, signal.SIGCONT)
            except ProcessLookupError:
                pass


def play(program, game, part, index, run_ms, stop_ms, out):
    with tempfile.TemporaryDirectory(prefix="cm0102-slow-") as work:
        user, home, shots = (os.path.join(work, d) for d in ("user", "home", "shots"))
        for d in (user, home, shots):
            os.makedirs(d)
        mode = "widescreen" if part == "match" else "classic"
        steps = match_steps() if part == "match" else new_game_steps()
        if part == "match":
            shutil.copyfile(os.path.join(fixtures.DEFAULT_DIR, smoke_run.MATCH_SAVE),
                            os.path.join(user, smoke_run.MATCH_SAVE))
        with open(os.path.join(user, "settings.txt"), "w") as f:
            f.write(f"display = {mode}\ndatabase = 3.9.68\n")
        env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
                   RECOMP_USER_DIR=user, RECOMP_FIXED_CLOCK="1", RECOMP_IDLE_COUNT="1", RECOMP_SCREEN_LOG="1",
                   RECOMP_SCRIPT="; ".join(smoke_run.scaled_steps(mode, shots, steps)))
        if part == "newgame":
            env["RECOMP_SAVE_AFTER_DAYS"] = "1"
        log = os.path.join(out, f"{part}-{index}.txt")
        start = time.time()
        with open(log, "w") as err:
            proc = subprocess.Popen([program, "--game", game, "-seed", "1"], env=env, stdout=subprocess.DEVNULL,
                                    stderr=err)
            done = threading.Event()
            t = threading.Thread(target=throttle, args=(proc, run_ms, stop_ms, index, done), daemon=True)
            t.start()
            try:
                code = proc.wait(timeout=1800)
            except subprocess.TimeoutExpired:
                proc.kill()
                code = proc.wait()
            done.set()
            t.join()
        took = time.time() - start
        text = open(log, encoding="latin-1").read()
        named = "|Test Smoke|" in text
        saved = os.path.exists(os.path.join(user, smoke_run.SAVE_NAME))
        ok = code == 0 and ("reached label:Pause" in text if part == "match" else saved and named)
        why = next((ln for ln in text.splitlines() if "timed out" in ln), f"code {code}")
        if code == 0 and part == "newgame" and not saved:
            why = "no save: the holiday did not start"
        if code == 0 and part == "newgame" and not named:
            names = re.findall(r"\|Continue Game\|([^|]*)\|", text)
            why = f"the manager is {names[-1] if names else 'not named'}, not Test Smoke"
        return ok, took, why, log


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    opt = lambda name, default: argv[argv.index(name) + 1] if name in argv[3:] else default  # noqa: E731
    part = opt("--part", "both")
    times = int(opt("--times", "3"))
    out = os.path.abspath(opt("--out", tempfile.mkdtemp(prefix="cm0102-slow-logs-")))
    os.makedirs(out, exist_ok=True)
    parts = ["newgame", "match"] if part == "both" else [part]
    failed = 0
    for p in parts:
        for i in range(1, times + 1):
            stop_ms = int(opt("--stop", "1000" if p == "match" else "600"))
            run_ms = int(opt("--run", "100" if p == "match" else "150"))
            ok, took, why, log = play(program, game, p, i, run_ms, stop_ms, out)
            print(f"{'ok  ' if ok else 'FAIL'}  {p} {i}: {took:.0f} s" + ("" if ok else f": {why} ({log})"), flush=True)
            failed += not ok
    print(f"logs in {out}")
    print("FAIL" if failed else "PASS")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
