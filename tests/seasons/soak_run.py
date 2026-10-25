#!/usr/bin/env python3
"""Long-game soak (0.6.0 slice 30a, check 2 of docs/research/brief-0.6.0/slice-30a.md): one game from a start
season to the end of a last season, played as the season jump (no match engine, the manager on holiday), with a
save near each season change that a new program loads and plays on.

Usage:
    python3 tests/seasons/soak_run.py <cm0102_recomp> <game folder> --keep <folder> [--start 2001] [--last 2027]
        [--seed 1] [--nations all|England,...] [--database <folder>] [--changing-rules on|off] [--leg-days 365]
        [--extra regional,c2,spain,...] [--timeout 14400]

The legs. Leg 0 makes the game (tests/seasons/rules_report_check.py: the steps of a new game, the season jump to
--last, the rules report) with RECOMP_SAVE_AFTER_DAYS=<leg days>: at the first human turn that many game days after
its first, the runtime saves samesave.sav and ends the program (src/recomp_rt/win/testrun.c). Each later leg copies
that save into a new user folder, loads it (tests/fixtures/fixtures.py LOAD_STEPS, as season_nl_run.py's
load_and_jump), plays on and saves again. The game starts in July, so each save comes in July of the next year,
near the season change. The last leg (the one that starts in the summer of --last) has no save: the season jump
ends the program after the end report of --last (season_report.c).

The checks of each leg:
    - no message box of the game in its log (RECOMP_SCREEN_LOG prints "recomp: message box from", waits.c): the
      script stops the game by its PID at the first one, so a box does not wait for the timeout
    - the program ends with code 0
    - a leg that must save leaves samesave.sav in its user folder
At the end: the last leg's rules report holds the end of --last, and over the reports of all legs every
division's edition moves on from the end of one season to the next (rules_report_check.league_stalls). The run
folder holds one folder per leg (leg00, leg01, ...) with its stderr.txt, report.tsv and user folder, and soak.tsv
with one line per leg (leg, the season it starts in, the result, seconds, the first box).

A leg of Select All takes about 20 to 60 minutes on a loaded machine (a 2001 game to 2027/28: 27 legs). The script
starts one game at a time and waits while /Users/max/Projects/cm0102/run-logs/HOLD exists, while 15 or more games
run on this Mac, or while the load of the last minute is 24 or more.
"""
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import rules_report_check as rrc  # noqa: E402
import smoke_run  # noqa: E402

HOLD = "/Users/max/Projects/cm0102/run-logs/HOLD"
BOX = "recomp: message box from"


def arg(argv, name, default=None):
    return argv[argv.index(name) + 1] if name in argv else default


def wait_for_room():
    """Waits until the HOLD file is gone, fewer than 15 games run and the load is below 24."""
    while True:
        games = subprocess.run(["pgrep", "-x", "cm0102_recomp"], capture_output=True, text=True).stdout.split()
        if len(games) < 15 and os.getloadavg()[0] < 24 and not os.path.exists(HOLD):
            return
        time.sleep(30)


def first_box(log):
    try:
        with open(log, encoding="latin-1") as f:
            for line in f:
                if line.startswith(BOX):
                    return line.strip()
    except OSError:
        pass
    return None


def play(args, env, log, timeout):
    """Runs the program; stops it by its PID at the first error box. Returns (code, seconds, box)."""
    start = time.time()
    with open(log, "w") as out:
        p = subprocess.Popen(args, env=env, stdout=subprocess.DEVNULL, stderr=out)
        box = None
        while p.poll() is None:
            time.sleep(5)
            box = first_box(log)
            if box or time.time() - start > timeout:
                p.kill()
                p.wait()
                break
    code = p.returncode if not box and time.time() - start <= timeout else ("box" if box else "timeout")
    return code, time.time() - start, box or first_box(log)


def leg_env(program, game, user, home, shots, steps, last, report, extra):
    table = rrc.rules_table(program, game)
    return dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
                RECOMP_USER_DIR=user, RECOMP_FIXED_CLOCK="1", RECOMP_SEASON_JUMP=str(last), RECOMP_SCREEN_LOG="1",
                RECOMP_RULES_REPORT=report, RECOMP_RULES_TABLE=table, RECOMP_SEASON_INTL_LOG="1",
                RECOMP_SCRIPT="; ".join(smoke_run.scaled_steps("classic", shots, steps)), **extra)


def folders(work):
    user, home, shots = (os.path.join(work, d) for d in ("user", "home", "shots"))
    for d in (user, home, shots):
        os.makedirs(d, exist_ok=True)
    return user, home, shots


def settings(user, rules, database):
    import custom_database_run as cdr
    lines = ["display = classic", f"changing_rules = {rules}", "faster_days = on"]
    if database:
        cdr.copy_data(database, os.path.join(user, "Databases", os.path.basename(database)))
        lines.append("database = folder:" + os.path.basename(database))
    with open(os.path.join(user, "settings.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")


def new_game_leg(o, work, save_days):
    user, home, shots = folders(work)
    settings(user, o["rules"], o["database"])
    steps = rrc.league_steps(o["nations"], o["extra"])
    extra = {"RECOMP_START_SEASON": str(o["start"])}
    test_extra = [e for e in o["extra"] if e in rrc.ENV_EXTRA]     # Spain and Portugal: a test switch, no click
    if test_extra:
        extra["RECOMP_TEST_EXTRA"] = ",".join(test_extra)
    if save_days:
        extra["RECOMP_SAVE_AFTER_DAYS"] = str(save_days)
    env = leg_env(o["program"], o["game"], user, home, shots, steps, o["last"], os.path.join(work, "report.tsv"), extra)
    return play([o["program"], "--game", o["game"], "-seed", str(o["seed"])], env, os.path.join(work, "stderr.txt"),
                o["timeout"])


def load_leg(o, work, save, save_days):
    import fixtures
    user, home, shots = folders(work)
    settings(user, o["rules"], o["database"])
    shutil.copyfile(save, os.path.join(user, "soak.sav"))
    # The save was made on holiday, so the loaded game goes on by itself and shows no first news to wait for.
    load = smoke_run.without_intro_click(fixtures.LOAD_STEPS)
    steps = [x for x in load if x != "wait first-news"] + [f"{o['timeout'] * 1000} quit"]
    extra = {"RECOMP_SAVE_AFTER_DAYS": str(save_days)} if save_days else {}
    env = leg_env(o["program"], o["game"], user, home, shots, steps, o["last"], os.path.join(work, "report.tsv"), extra)
    return play([o["program"], "--game", o["game"], "-seed", str(o["seed"])], env, os.path.join(work, "stderr.txt"),
                o["timeout"])


def main(argv):
    if len(argv) < 3 or "--keep" not in argv:
        print(__doc__)
        return 2
    nations = arg(argv, "--nations", "all")
    o = {"program": os.path.abspath(argv[1]), "game": os.path.abspath(argv[2]), "keep": os.path.abspath(arg(argv, "--keep")),
         "start": int(arg(argv, "--start", 2001)), "last": int(arg(argv, "--last", 2027)),
         "seed": int(arg(argv, "--seed", 1)), "nations": nations.split(","),
         "database": os.path.abspath(arg(argv, "--database")) if "--database" in argv else None,
         "rules": arg(argv, "--changing-rules", "on"), "days": int(arg(argv, "--leg-days", 365)),
         "timeout": int(arg(argv, "--timeout", 14400)),
         "extra": [e for e in arg(argv, "--extra", "").split(",") if e]}
    if o["last"] < o["start"]:
        print("soak_run.py: --last is before --start")
        return 2
    os.makedirs(o["keep"], exist_ok=True)
    summary = os.path.join(o["keep"], "soak.tsv")
    with open(summary, "w") as f:
        f.write("leg\tseason\tresult\tseconds\tbox\n")
    fails, save = [], None
    for leg in range(o["last"] - o["start"] + 1):
        season = o["start"] + leg
        work = os.path.join(o["keep"], f"leg{leg:02d}")
        if os.path.exists(work):
            print(f"soak_run.py: {work} exists; use a new --keep folder")
            return 2
        save_days = o["days"] if season < o["last"] else 0
        wait_for_room()
        if leg == 0:
            code, took, box = new_game_leg(o, work, save_days)
        else:
            code, took, box = load_leg(o, work, save, save_days)
        saved = os.path.join(work, "user", smoke_run.SAVE_NAME)
        result = "ok"
        if box:
            result = "box"
        elif code != 0:
            result = f"code {code}"
        elif save_days and not os.path.exists(saved):
            result = "no save"
        with open(summary, "a") as f:
            f.write(f"{leg}\t{season}/{(season + 1) % 100:02d}\t{result}\t{took:.0f}\t{box or ''}\n")
        print(f"leg {leg} ({season}/{(season + 1) % 100:02d}): {result} after {took:.0f} s{': ' + box if box else ''}",
              flush=True)
        if result != "ok":
            fails.append(f"leg {leg} ({season}/{(season + 1) % 100:02d}): {result} ({work})")
            break
        save = saved
    if not fails:
        seasons, _ = rrc.read_report(os.path.join(o["keep"], f"leg{o['last'] - o['start']:02d}", "report.tsv"))
        if not seasons or seasons[-1]["season"] != o["last"] or seasons[-1]["phase"] != "end":
            fails.append(f"the last leg's report does not end with the end of {o['last']}/{(o['last'] + 1) % 100:02d}")
    # Every division moves on from one season to the next (rules_report_check.league_stalls, O278), all legs.
    reports = [os.path.join(o["keep"], f"leg{n:02d}", "report.tsv") for n in range(o["last"] - o["start"] + 1)]
    comps = [c for r in reports if os.path.exists(r) for c in rrc.read_report(r)[1]]
    fails += [f"league stall: {line}" for line in rrc.league_stalls(comps)]
    for f in fails:
        print(f"FAIL  {f}")
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
