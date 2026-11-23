#!/usr/bin/env python3
"""Speed benchmark (0.8.0 slice 1, issue 76): two numbers for one build on one machine.

    new      an all-nations game of 2001/02 from "Start New Game" to the first human turn. The number is the sum of
             the progress steps of the game (RECOMP_PROGRESS_LOG, the way docs/research/2026-10-29-new-game-speed.md
             measures), in seconds. The clicks of the script and the start of the program are not in it.
    holiday  four weeks of holiday on the fixed full-world save, with the manager on holiday and the match engine on.
             The number is the time between the first and the last game date line of the screen log (a line comes
             at a human turn 7 or more game days after the last one; the span is 28 days), so the load of the save and the write of the
             next save are not in it.

Usage:
    python3 tests/seasons/speed_bench.py <cm0102_recomp> <game folder> --keep <folder> [--part new|holiday|all]
        [--runs 3] [--save <fixed save>] [--make-save] [--limit 5] [--seed 1] [--timeout 7200] [--label <text>]

--make-save plays a new game with all nations, goes on holiday and saves after 60 game days (early September 2001),
then copies the save to <keep>/fixed-2001-09.sav and writes its SHA-256 next to it. The same run records a "new"
number. Make the fixed save once, with the build that makes the "before" numbers, and keep it: a faster build may play
the same days differently (decision of the 0.8.0 spec), so a save made by a later build is another save.

--part new and --part holiday play --runs games one after the other (the holiday part needs --save). Every run adds a
line to <keep>/bench.tsv: the time, the platform, the build label, the 1-minute load at the start and at the end of
the run, and the result. A part passes when every run ended with code 0 and no error box, and the longest run is less
than --limit percent above the shortest (the spec asks for 5 percent). A part that fails the spread is not a number:
repeat it when the machine is quiet. The script starts no game while the 1-minute load is 20 or more (the 0.8.0 brief),
while /Users/max/Projects/cm0102/run-logs/HOLD exists, or below 100 GB of free disk. --no-wait turns the wait off.

The program prints its own times, so the script reads stderr. Steps for Windows and Linux: docs/TESTING.md, "Speed
benchmark". The ctest speed_bench plays no game: it runs this file's functions on made-up logs and one fake program.
"""
import hashlib
import os
import platform
import re
import shutil
import statistics
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
HOLD = "/Users/max/Projects/cm0102/run-logs/HOLD"
LOAD_LIMIT = 20.0               # the brief: start a game only while the 1-minute load is under 20
DISK_FLOOR_GB = 100
BOX = "recomp: message box from"
DONE = re.compile(r'^recomp: progress done "(.*)" (\d+) ms, \d+ calls')
DATE = "recomp: game date "
DATE_LINE = re.compile(r"^recomp: game date (\d+)/(\d+) at a human turn")
SAVED = re.compile(r"^recomp: saved .* after (\d+) game days")
HOLIDAY_DAYS = 28               # the span of the holiday number: date lines at days 0, 7, 14, 21 and 28
MAKE_DAYS = 60                  # the fixed save: a new game of 1 July 2001 plus 60 game days
SAVE_DAYS = 35                  # the holiday run ends after this many days, so a line at day 28 exists
HEAD = ["when", "platform", "label", "part", "run", "seconds", "load_start", "load_end", "result", "detail"]


# ---- functions that read a log (tested without a game) ----

def progress_steps(lines):
    """The progress steps of a new game: [(text, ms)] in order, from the 'progress done' lines."""
    out = []
    for line in lines:
        m = DONE.match(line)
        if m:
            out.append((m.group(1), int(m.group(2))))
    return out


def new_game_seconds(lines):
    """The new game number: the sum of the progress steps, in seconds. None when the log has no step."""
    steps = progress_steps(lines)
    return sum(ms for _, ms in steps) / 1000.0 if steps else None


def holiday_seconds(timed):
    """The holiday number from [(time, line)] of a run: the seconds between the first game date line and the first
    line that is HOLIDAY_DAYS or more game days later. A line reads "recomp: game date <day of the year>/<year>".
    Returns (seconds, game days between the two lines), or (None, lines found) when no such line exists."""
    marks = []
    for t, line in timed:
        m = DATE_LINE.match(line)
        if m:
            marks.append((t, int(m.group(1)) + 365 * int(m.group(2))))
    for t, day in marks[1:]:
        if day - marks[0][1] >= HOLIDAY_DAYS:
            return t - marks[0][0], day - marks[0][1]
    return None, len(marks)


def spread_percent(values):
    """(longest - shortest) / shortest in percent. 0 for one value."""
    if len(values) < 2:
        return 0.0
    return 100.0 * (max(values) - min(values)) / min(values)


def judge(runs, limit):
    """runs: [(result, seconds)] of one part. Returns (ok, text). A run counts when its result is 'ok'."""
    bad = [f"run {i + 1}: {r}" for i, (r, _) in enumerate(runs) if r != "ok"]
    if bad:
        return False, "; ".join(bad)
    secs = [s for _, s in runs]
    pct = spread_percent(secs)
    text = f"{len(secs)} runs, median {statistics.median(secs):.1f} s, spread {pct:.1f} %"
    return pct < limit, text + ("" if pct < limit else f" (over {limit} %)")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def load_now():
    try:
        return os.getloadavg()[0]
    except (AttributeError, OSError):       # Windows has no load average
        return -1.0


def append_row(keep, row):
    path = os.path.join(keep, "bench.tsv")
    new = not os.path.exists(path)
    with open(path, "a", encoding="utf-8") as f:
        if new:
            f.write("\t".join(HEAD) + "\n")
        f.write("\t".join(str(row.get(k, "")) for k in HEAD) + "\n")


# ---- running a program ----

def run_timed(args, env, log, timeout):
    """Runs the program, reads its stderr line by line with the time of each line, and stops it by its PID at the
    first error box or at the timeout. Returns (code, timed lines, box). code is 'box' or 'timeout' when stopped."""
    start = time.time()
    timed, box = [], []
    p = subprocess.Popen(args, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

    def reader():
        with open(log, "w", encoding="latin-1") as out:
            for raw in p.stderr:
                line = raw.decode("latin-1").rstrip("\n")
                timed.append((time.time() - start, line))
                out.write(line + "\n")
                out.flush()
                if line.startswith(BOX) and not box:
                    box.append(line)

    t = threading.Thread(target=reader, daemon=True)
    t.start()
    stopped = None
    while p.poll() is None:
        time.sleep(1)
        if box:
            stopped = "box"
        elif time.time() - start > timeout:
            stopped = "timeout"
        if stopped:
            p.kill()
            p.wait()
            break
    t.join(10)
    return (stopped or p.returncode), timed, (box[0] if box else None)


def wait_for_room(keep, wait=True):
    """Waits until the load is under LOAD_LIMIT, HOLD is gone and the disk has room. Returns the load at the start."""
    while True:
        free = shutil.disk_usage(keep).free / 1e9
        load = load_now()
        if free < DISK_FLOOR_GB:
            sys.exit(f"speed_bench.py: only {free:.0f} GB free in {keep}; start nothing below {DISK_FLOOR_GB} GB")
        if not wait or (not os.path.exists(HOLD) and (load < 0 or load < LOAD_LIMIT)):
            return load
        time.sleep(30)


# ---- the game runs ----

def game_env(program, game, user, home, steps, extra):
    import rules_report_check as rrc
    env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
               RECOMP_USER_DIR=user, RECOMP_FIXED_CLOCK="1", RECOMP_SCREEN_LOG="1", RECOMP_PROGRESS_LOG="1",
               RECOMP_RULES_TABLE=rrc.rules_table(program, game), RECOMP_SCRIPT="; ".join(steps))
    env.update(extra)
    return env


def write_settings(user):
    with open(os.path.join(user, "settings.txt"), "w") as f:
        f.write("display = classic\nchanging_rules = on\nfaster_days = on\ndatabase = 3.9.68\n")


def new_game_steps(holiday):
    """The new game steps of tests/seasons/rules_report_check.py with all nations. The steps after Finish (the
    holiday click) stay only when holiday is true."""
    import rules_report_check as rrc
    steps = rrc.league_steps(["all"], [])
    if holiday:
        return steps
    end = next(i for i, s in enumerate(steps) if s.startswith("wait first-news"))
    return steps[:end + 1]


def game_run(o, work, steps, days, name):
    user, home = (os.path.join(work, d) for d in ("user", "home"))
    for d in (user, home):
        os.makedirs(d, exist_ok=True)
    write_settings(user)
    if o.get("save") and name == "holiday":
        shutil.copyfile(o["save"], os.path.join(user, "soak.sav"))
    env = game_env(o["program"], o["game"], user, home, steps, {"RECOMP_SAVE_AFTER_DAYS": str(days)})
    code, timed, box = run_timed([o["program"], "--game", o["game"], "-seed", str(o["seed"])], env,
                                 os.path.join(work, "stderr.txt"), o["timeout"])
    return code, timed, box, user


def one_run(o, part, n):
    """One timed run of part 'new', 'holiday' or 'make'. Returns the row for bench.tsv and the user folder."""
    import fixtures
    import smoke_run
    work = os.path.join(o["keep"], f"{part}-{n:02d}")
    if os.path.exists(work):
        sys.exit(f"speed_bench.py: {work} exists; use a new --keep folder")
    load0 = wait_for_room(o["keep"], o["wait"])
    if part == "new":
        code, timed, box, user = game_run(o, work, new_game_steps(False), 0, "new")
    elif part == "make":
        code, timed, box, user = game_run(o, work, new_game_steps(True), MAKE_DAYS, "make")
    else:
        load = smoke_run.without_intro_click(fixtures.LOAD_STEPS)
        steps = [s for s in load if s != "wait first-news"] + [f"{o['timeout'] * 1000} quit"]
        code, timed, box, user = game_run(o, work, steps, SAVE_DAYS, "holiday")
    lines = [line for _, line in timed]
    row = {"when": time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime()), "platform": platform_name(),
           "label": o["label"], "part": part, "run": n, "load_start": f"{load0:.1f}", "load_end": f"{load_now():.1f}"}
    result, secs, detail = "ok", None, ""
    if box:
        result, detail = "box", box
    elif code != 0:
        result = f"code {code}"
    elif part == "holiday":
        secs, got = holiday_seconds(timed)
        if secs is None:
            result, detail = "no dates", f"{got} game date lines"
        elif not any(SAVED.match(x) for x in lines):
            result = "no save"
        else:
            detail = f"{got} game days"
    else:
        secs = new_game_seconds(lines)
        if secs is None:
            result = "no steps"
    row.update(result=result, seconds="" if secs is None else f"{secs:.1f}", detail=detail)
    append_row(o["keep"], row)
    print(f"{part} {n}: {result} {row['seconds']} s (load {row['load_start']} to {row['load_end']})", flush=True)
    return row, user


def platform_name():
    return f"{platform.system()} {platform.machine()}"


def make_save(o):
    row, user = one_run(o, "make", 1)
    saved = os.path.join(user, "samesave.sav")
    if row["result"] != "ok" or not os.path.exists(saved):
        print(f"FAIL  make-save: {row['result']} {row['detail']}")
        return 1
    dest = os.path.join(o["keep"], "fixed-2001-09.sav")
    shutil.copyfile(saved, dest)
    with open(dest + ".sha256", "w") as f:
        f.write(f"{sha256(dest)}  fixed-2001-09.sav\n{o['label']}\n")
    print(f"fixed save {dest} ({os.path.getsize(dest) >> 20} MB), SHA-256 {sha256(dest)}")
    return 0


def part_runs(o, part):
    runs = []
    for n in range(1, o["runs"] + 1):
        row, _ = one_run(o, part, n)
        runs.append((row["result"], float(row["seconds"]) if row["seconds"] else 0.0))
    ok, text = judge(runs, o["limit"])
    print(f"{'PASS' if ok else 'FAIL'}  {part}: {text}")
    return ok


def arg(argv, name, default=None):
    return argv[argv.index(name) + 1] if name in argv else default


def main(argv):
    if len(argv) < 3 or "--keep" not in argv:
        print(__doc__)
        return 2
    sys.path[:0] = [HERE, os.path.join(HERE, "..", "smoke"), os.path.join(HERE, "..", "fixtures")]
    o = {"program": os.path.abspath(argv[1]), "game": os.path.abspath(argv[2]), "keep": os.path.abspath(arg(argv, "--keep")),
         "runs": int(arg(argv, "--runs", 3)), "limit": float(arg(argv, "--limit", 5)), "seed": int(arg(argv, "--seed", 1)),
         "timeout": int(arg(argv, "--timeout", 7200)), "label": arg(argv, "--label", "unlabelled"),
         "save": os.path.abspath(arg(argv, "--save")) if "--save" in argv else None, "wait": "--no-wait" not in argv}
    os.makedirs(o["keep"], exist_ok=True)
    if "--make-save" in argv:
        return make_save(o)
    part = arg(argv, "--part", "all")
    if part not in ("new", "holiday", "all"):
        print("speed_bench.py: --part is new, holiday or all")
        return 2
    if part != "new" and not o["save"]:
        print("speed_bench.py: the holiday part needs --save <fixed save> (made with --make-save)")
        return 2
    oks = []
    if part in ("new", "all"):
        oks.append(part_runs(o, "new"))
    if part in ("holiday", "all"):
        oks.append(part_runs(o, "holiday"))
    return 0 if all(oks) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
