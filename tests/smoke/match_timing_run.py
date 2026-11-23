#!/usr/bin/env python3
"""Plays the match check of smoke_run.py on one build with one set of options, first at full speed and then on a
slow machine, and compares the saves (item 155).

    python3 tests/smoke/match_timing_run.py <cm0102_recomp> <game folder> [--times n] [--stop ms] [--run ms]
        [--env NAME=value]... [--out folder] [--jobs n]

It is a check to run by hand, not a ctest: it takes about 2 minutes per run and depends on timing.

Q152 says that the speed of the machine must not change the game. Under a load of about 22, one run of the match
same-save set (colours-off, 6 November 2026) saved another game than normal, and two later runs on a quiet machine
were the same. Two causes:
- In the original, the background matches of a match day play one engine step (0x6b5ce0) in each pass of the screen
  loop, between clicks and while a commentary line of the human match waits (its delay is in milliseconds of the
  clock 0x947299, 0x6b55c0). They take numbers from the one random table of 0x90cfc0, as the human match does. So the count
  of passes decides which numbers the rest of the game gets. The runtime fixes it (match_numbers.c, item 155).
- In the test: the steps before the first wait ran at fixed times from the window opening, and on a slow machine
  the click "Background Changes Off" came before the settings screen. fixtures.LOAD_STEPS now waits for it.

This test makes the slow machine itself, as tests/smoke/slow_machine_run.py (item 154) does: a thread stops the
game (SIGSTOP) for 0.5 to 1.5 times --stop ms after each --run ms. Run 1 has no throttle. Runs 2 to n have the
throttle, each with its own seed. Each run plays prematch.sav with the steps of the match check up to 21.7.01, where
the runtime saves (RECOMP_SAVE_AFTER_DAYS). Every save must equal the save of run 1. A run that ends with code 3 (a
wait timed out: the script lost a key press) proves nothing, and the test says so.
"""
import concurrent.futures
import os
import random
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


def match_steps(event=True):
    """The steps of the match check, with a wait for Continue Game before each Space that stands for it (on a slow
    machine the Space can come before the button, item 154). event=False takes smoke_run.MATCH_STEPS_ORIGINAL (a run
    that keeps the group match_numbers)."""
    steps, match = [], smoke_run.MATCH_STEPS if event else smoke_run.MATCH_STEPS_ORIGINAL
    for i, s in enumerate(match):
        before = match[i - 1] if i else ""
        if s.endswith("press space") and i > 1 and before.startswith("wait ") and before != "wait continue" \
                and not before.startswith("wait screen:0x7"):
            steps.append("wait continue")
        steps.append(s)
    return smoke_run.without_intro_click(fixtures.LOAD_STEPS) + steps


def throttle(proc, run_ms, stop_ms, seed, done):
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


def play(program, game, index, slow, run_ms, stop_ms, extra_env, out, work):
    run = os.path.join(work, f"run-{index}")
    user, home, shots = (os.path.join(run, d) for d in ("user", "home", "shots"))
    for d in (user, home, shots):
        os.makedirs(d)
    shutil.copyfile(os.path.join(fixtures.DEFAULT_DIR, smoke_run.MATCH_SAVE), os.path.join(user, smoke_run.MATCH_SAVE))
    with open(os.path.join(user, "settings.txt"), "w") as f:
        f.write("display = widescreen\ndatabase = 3.9.68\n")
    env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
               RECOMP_USER_DIR=user, RECOMP_FIXED_CLOCK="1", RECOMP_IDLE_COUNT="1", RECOMP_SCREEN_LOG="1",
               RECOMP_SAVE_AFTER_DAYS=str(smoke_run.MATCH_DAYS),
               RECOMP_SCRIPT="; ".join(smoke_run.scaled_steps("widescreen", shots, match_steps(
                   not smoke_run.keeps_match_numbers(extra_env.get("RECOMP_ORIGINAL_FAULTS", ""))))), **extra_env)
    log = os.path.join(out, f"run-{index}.txt")
    start = time.time()
    with open(log, "w") as err:
        proc = subprocess.Popen([program, "--game", game, "-seed", "1"], env=env, stdout=subprocess.DEVNULL,
                                stderr=err)
        done = threading.Event()
        t = None
        if slow:
            t = threading.Thread(target=throttle, args=(proc, run_ms, stop_ms, index, done), daemon=True)
            t.start()
        try:
            code = proc.wait(timeout=2400)
        except subprocess.TimeoutExpired:
            proc.kill()
            code = proc.wait()
        done.set()
        if t:
            t.join()
    save = os.path.join(user, smoke_run.SAVE_NAME)
    kept = os.path.join(out, f"run-{index}.sav")
    if os.path.exists(save):
        shutil.copyfile(save, kept)
    text = open(log, encoding="latin-1").read()
    why = next((ln for ln in text.splitlines() if "timed out" in ln), f"code {code}")
    return code, os.path.exists(save) and kept, time.time() - start, why


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    opt = lambda name, default: argv[argv.index(name) + 1] if name in argv[3:] else default  # noqa: E731
    times = int(opt("--times", "3"))
    stop_ms, run_ms = int(opt("--stop", "300")), int(opt("--run", "100"))
    extra_env = dict(argv[i + 1].split("=", 1) for i in range(3, len(argv) - 1) if argv[i] == "--env")
    out = os.path.abspath(opt("--out", tempfile.mkdtemp(prefix="cm0102-timing-logs-")))
    os.makedirs(out, exist_ok=True)
    jobs = int(opt("--jobs", "2"))
    with tempfile.TemporaryDirectory(prefix="cm0102-timing-") as work:
        with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
            futures = [pool.submit(play, program, game, i, i > 1, run_ms, stop_ms, extra_env, out, work)
                       for i in range(1, times + 1)]
            results = [f.result() for f in futures]
    first = None
    failed = unproven = 0
    for i, (code, save, took, why) in enumerate(results, 1):
        label = "full speed" if i == 1 else f"slow (seed {i})"
        if code != 0 or not save:
            print(f"NONE  run {i}, {label}: no save after {took:.0f} s: {why}")
            unproven += 1
            continue
        files = smoke_run.save_files(save)
        if first is None:
            first = (i, files)
            print(f"run   run {i}, {label}: saved in {took:.0f} s")
            continue
        differ = sorted(n for n in set(files) | set(first[1]) if files.get(n) != first[1].get(n))
        if differ:
            failed += 1
            print(f"FAIL  run {i}, {label}: saved in {took:.0f} s, differs from run {first[0]} in "
                  f"{len(differ)} files: {', '.join(differ[:8])}")
        else:
            print(f"ok    run {i}, {label}: saved in {took:.0f} s, the same as run {first[0]}")
    print(f"logs and saves in {out}")
    if failed:
        print("FAIL")
        return 1
    if unproven or first is None or len(results) - unproven < 2:
        print("NONE: too few saves to compare")
        return 3
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
