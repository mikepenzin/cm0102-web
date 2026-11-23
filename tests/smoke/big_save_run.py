#!/usr/bin/env python3
"""Big save test: saves from 2 to 4 GB save and load, and a save above 4 GB fails without harm (0.6.0, O156).

Usage:
    python3 tests/smoke/big_save_run.py <cm0102_recomp> <game folder> [--case load|season|over4 ...]
        [--dir <fixtures folder>] [--work <folder>]

The save container keeps the offset of each part in an unsigned 32-bit word (tools/save/savfile.py). The original
wrote 0xffffffff for every part from 2 GB on (tools/recomp/save_size.py, src/recomp_rt/win/files.c). A save takes
every .tmp file of the user folder along, so the test makes the save large with filler .tmp files of random bytes
(1.1 GB each; the byte run code stores random bytes at about 1.5 times their size). The fillers are of one type,
so from 0.6.2 they go into one merged part (src/recomp_rt/win/save_parts.c). A load deletes the .tmp files of
the user folder, so the fillers are hard links that go in after the load (or, for a new game, again until the save
starts). Cases:
    load    start.sav, two fillers, save after 7 days: a save of about 3.3 GB with no offset 0xffffffff and parts
            from 2 GB on. That save then loads and saves again.
    season  a new game with changing rules on and two fillers: the season part bosman_season.dat lies above 2 GB,
            and the load reads changing rules on.
    over4   three fillers, saved over samesave.sav (a copy of start.sav): the game shows its own box "Unable to
            Save Game", then the runtime's box "larger than 4 GB". The save result is 0, temp_save_file.sav is
            gone and samesave.sav is byte-identical to the file before.
Each case plays one game at a time and takes 2 to 5 minutes (one run on 2026-10-22: 159, 182 and 92 s). The work folder (a temporary folder by default) holds
up to about 12 GB and is deleted at the end. Exit code 0 when every case passes.
"""
import argparse
import filecmp
import glob
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import threading
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fixtures"))
import fixtures  # noqa: E402
import smoke_run  # noqa: E402

NAME = smoke_run.SAVE_NAME          # samesave.sav, the name that RECOMP_SAVE_AFTER_DAYS writes
FILLER = 1181116006                 # bytes of one filler: two give a save of about 3.3 GB, three pass 4 GB
TWO_GB = 1 << 31
OK_BUTTON = "399 330"               # the Ok button of both boxes in classic mode
TIMEOUT = 3600


def holiday_steps():
    holiday = smoke_run.STEPS[smoke_run.STEPS.index("wait first-news") + 1:]
    return [s for s in holiday if "dump" not in s and "quit" not in s]


def load_steps():
    return smoke_run.without_intro_click(list(fixtures.LOAD_STEPS)) + holiday_steps()


def new_game_steps():
    return smoke_run.without_intro_click([s for s in smoke_run.STEPS if "5-holiday" not in s and "quit" not in s])


def make_fillers(folder, count):
    os.makedirs(folder, exist_ok=True)
    for i in range(1, count + 1):
        path = os.path.join(folder, f"aaa filler {i}.tmp")
        if not os.path.exists(path):
            with open(path, "wb") as f:
                left = FILLER
                while left:
                    n = min(left, 1 << 24)
                    f.write(os.urandom(n))
                    left -= n
    return sorted(glob.glob(os.path.join(folder, "*.tmp")))[:count]


def link_fillers(user, fillers, again):
    """Links the fillers into the user folder: after a load (once 20 .tmp files are there, 3 s later), or for a
    new game again and again until the save starts (the game creation deletes .tmp files)."""
    if again:
        while not os.path.exists(os.path.join(user, "temp_save_file.sav")):
            for f in fillers:
                t = os.path.join(user, os.path.basename(f))
                if not os.path.exists(t):
                    try:
                        os.link(f, t)
                    except OSError:
                        pass
            time.sleep(0.2)
        return
    while len(glob.glob(os.path.join(user, "*.tmp"))) < 20:
        time.sleep(0.2)
    time.sleep(3)
    for f in fillers:
        os.link(f, os.path.join(user, os.path.basename(f)))


def play(program, game, run, steps, days, save=None, fillers=(), again=False, settings=""):
    """One game in run/user. Returns (exit code, stderr). The quit step comes late: a save of 3 GB or more takes
    minutes."""
    user, home, shots = (os.path.join(run, d) for d in ("user", "home", "shots"))
    for d in (user, home, shots):
        os.makedirs(d, exist_ok=True)
    if save:
        shutil.copyfile(save, os.path.join(user, os.path.basename(save)))
    with open(os.path.join(user, "settings.txt"), "w") as f:
        f.write("display = classic\ndatabase = 3.9.68\n" + settings)
    env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
               RECOMP_USER_DIR=user, RECOMP_FIXED_CLOCK="1", RECOMP_SAVE_AFTER_DAYS=str(days),
               RECOMP_SCRIPT="; ".join(smoke_run.scaled_steps("classic", shots, steps + ["3000000 quit"])))
    if fillers:
        threading.Thread(target=link_fillers, args=(user, fillers, again), daemon=True).start()
    r = subprocess.run([program, "--game", game, "-seed", "1"], env=env, capture_output=True, text=True,
                       timeout=TIMEOUT)
    for f in fillers:                       # free the space at once
        t = os.path.join(user, os.path.basename(f))
        if os.path.exists(t):
            os.unlink(t)
    return r.returncode, r.stderr


def rows(path):
    """The rows of a save: [(offset, size, name)], read as unsigned words."""
    with open(path, "rb") as f:
        head = f.read(0x416ec)
    count = struct.unpack_from("<I", head, 8)[0]
    out = []
    for k in range(count):
        o = 12 + k * 0x10c
        off, size = struct.unpack_from("<2I", head, o)
        out.append((off, size, head[o + 8:o + 268].split(b"\0")[0].decode("latin-1")))
    return out


def check_rows(path, fails, label):
    r = rows(path)
    bad = [n for off, _, n in r if off == 0xffffffff]
    high = [n for off, _, n in r if TWO_GB <= off < 0xffffffff]
    size = os.path.getsize(path)
    print(f"      {label}: {size / 2**30:.2f} GB, {len(r)} parts, {len(high)} from 2 GB on, {len(bad)} at 0xffffffff")
    if bad:
        fails.append(f"{label}: parts at offset 0xffffffff: {', '.join(bad[:5])}")
    if size < TWO_GB or not high:
        fails.append(f"{label}: the save does not pass 2 GB, so the test proves nothing")
    return r


def case_load(program, game, work, fixtures_dir, fails):
    fillers = make_fillers(os.path.join(work, "fillers"), 2)
    run = os.path.join(work, "load-save")
    code, err = play(program, game, run, load_steps(), 7, os.path.join(fixtures_dir, "start.sav"), fillers)
    saved = os.path.join(run, "user", NAME)
    if code or not os.path.exists(saved):
        fails.append(f"load: no save with the fillers (code {code}): {err[-500:]}")
        return
    check_rows(saved, fails, "load: save")
    big = os.path.join(work, "big.sav")
    os.replace(saved, big)
    shutil.rmtree(run)
    run = os.path.join(work, "load-load")
    code, err = play(program, game, run, load_steps(), 1, big)
    if code or not os.path.exists(os.path.join(run, "user", NAME)):
        fails.append(f"load: the big save does not load and save again (code {code}): {err[-500:]}")
    else:
        print("      load: the big save loads and saves again")
    os.unlink(big)
    shutil.rmtree(run)


def case_season(program, game, work, fails):
    fillers = make_fillers(os.path.join(work, "fillers"), 2)
    run = os.path.join(work, "season-save")
    code, err = play(program, game, run, new_game_steps(), 14, fillers=fillers, again=True,
                     settings="changing_rules = on\n")
    saved = os.path.join(run, "user", NAME)
    if code or not os.path.exists(saved):
        fails.append(f"season: no save with the fillers (code {code}): {err[-500:]}")
        return
    r = check_rows(saved, fails, "season: save")
    part = [off for off, _, n in r if n == "bosman_season.dat"]
    if not part or part[0] < TWO_GB:
        fails.append(f"season: the season part is not above 2 GB ({part})")
    big = os.path.join(work, "season.sav")
    os.replace(saved, big)
    shutil.rmtree(run)
    run = os.path.join(work, "season-load")
    code, err = play(program, game, run, load_steps(), 1, big)
    if code or "with changing rules on" not in err:
        fails.append(f"season: the load does not read changing rules on (code {code}): {err[-500:]}")
    else:
        print("      season: the load reads changing rules on")
    os.unlink(big)
    shutil.rmtree(run)


def case_over4(program, game, work, fixtures_dir, fails):
    fillers = make_fillers(os.path.join(work, "fillers"), 3)
    run = os.path.join(work, "over4")
    earlier = os.path.join(work, NAME)
    shutil.copyfile(os.path.join(fixtures_dir, "start.sav"), earlier)
    # The game's own box comes first (its delete of the old save fails), then the runtime's box.
    boxes = [f"wait box 2400000", f"1500 move {OK_BUTTON}", f"1500 click {OK_BUTTON}",
             f"wait box 600000", f"1500 move {OK_BUTTON}", f"1500 click {OK_BUTTON}"]
    code, err = play(program, game, run, load_steps() + boxes, 7, earlier, fillers)
    user = os.path.join(run, "user")
    if "larger than 4 GB" not in err:
        fails.append(f"over4: no box about 4 GB (code {code}): {err[-500:]}")
    if "(result 0)" not in err:
        fails.append("over4: the save result is not 0")
    if not filecmp.cmp(os.path.join(user, NAME), os.path.join(fixtures_dir, "start.sav"), shallow=False):
        fails.append("over4: the earlier save changed")
    if os.path.exists(os.path.join(user, "temp_save_file.sav")):
        fails.append("over4: temp_save_file.sav is left")
    if not [f for f in fails if f.startswith("over4")]:
        print("      over4: the save fails, the earlier save is byte-identical, no temp file is left")
    shutil.rmtree(run)
    os.unlink(earlier)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("program")
    ap.add_argument("game")
    ap.add_argument("--case", action="append", choices=["load", "season", "over4"])
    ap.add_argument("--dir", default=fixtures.DEFAULT_DIR)
    ap.add_argument("--work")
    a = ap.parse_args()
    program = os.path.abspath(a.program)
    work = a.work or tempfile.mkdtemp(prefix="cm0102-big-save-")
    os.makedirs(work, exist_ok=True)
    failed = 0
    try:
        for case in a.case or ["load", "season", "over4"]:
            fails = []
            start = time.time()
            if case == "load":
                case_load(program, a.game, work, a.dir, fails)
            elif case == "season":
                case_season(program, a.game, work, fails)
            else:
                case_over4(program, a.game, work, a.dir, fails)
            for f in fails:
                print(f"FAIL  {f}")
            print(f"{'FAIL' if fails else 'ok  '}  {case} ({time.time() - start:.0f} s)")
            failed += bool(fails)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print("PASS" if not failed else "FAIL")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
