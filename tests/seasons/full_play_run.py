#!/usr/bin/env python3
"""Full play (0.6.0 slice 30a, gate (b) of docs/research/brief-0.6.0/slice-30a.md): one new game from a start
season, played with the match engine on and the manager on holiday, with a save near each 1 July that a new program
loads and plays on (as tests/seasons/soak_run.py does for the season jump).

Usage:
    python3 tests/seasons/full_play_run.py <cm0102_recomp> <game folder> --keep <folder> [--start 2001]
        [--seasons 2 | --end 2028 [--end-days 62]] [--nations all|England,...] [--database <folder>]
        [--changing-rules on|off] [--seed 1] [--stall-minutes 20] [--timeout 86400]

The legs. Leg 0 makes the game with the new-game and holiday steps of tests/seasons/rules_report_check.py, but
without RECOMP_SEASON_JUMP, so the match engine stays on (the jump turns it off, src/recomp_rt/win/season_report.c).
With RECOMP_SAVE_AFTER_DAYS=365, at the first human turn 365 game days after its first, the runtime saves
samesave.sav and ends the program (src/recomp_rt/win/testrun.c): a save in July of the next year. Each later leg
copies that save into a new user folder, loads it (tests/fixtures/fixtures.py LOAD_STEPS), plays on and saves
again. --seasons N plays N legs of 365 days. --end E plays to about 1 September of E (rule O166): the legs of 365
days to July of E, then a last leg of --end-days days (62 from 1 July). The rules report (RECOMP_RULES_REPORT) is
written as in the jump.

The checks of each leg:
    - no message box of the game in its log (RECOMP_SCREEN_LOG prints "recomp: message box from", waits.c): the
      script stops the game by its PID at the first one, so a box does not wait for the timeout
    - no stall: once the game is made, the game date (the "date d/y" of the screen log) moves at least once in
      --stall-minutes
    - the program ends with code 0 and leaves samesave.sav in its user folder
At the end, over the reports of all legs: every division's edition moves on from the end of one season to the
next (rules_report_check.league_stalls).
The run folder holds one folder per leg (leg00, leg01, ...) with its stderr.txt, report.tsv and user folder, and
full.tsv with one line per leg (leg, the season it starts in, the result, minutes, the first fault). The script
prints the wall time at each 1 July that the game reaches and waits before each leg while
/Users/max/Projects/cm0102/run-logs/HOLD exists, while 15 or more games run on this Mac, or while the load of the
last minute is 24 or more.
"""
import os
import re
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
import custom_database_run as cdr  # noqa: E402

HOLD = "/Users/max/Projects/cm0102/run-logs/HOLD"
BOX = re.compile(r"recomp: message box from (\S+): (.*)")
DATE = re.compile(r"date (\d+)/(\d+)")


def arg(argv, name, default=None):
    return argv[argv.index(name) + 1] if name in argv else default


def wait_for_room():
    """Waits until the HOLD file is gone, fewer than 15 games run and the load is below 24."""
    while True:
        games = subprocess.run(["pgrep", "-x", "cm0102_recomp"], capture_output=True, text=True).stdout.split()
        if len(games) < 15 and os.getloadavg()[0] < 24 and not os.path.exists(HOLD):
            return
        time.sleep(30)


def nation_list(text):
    words = [x.strip().lower().replace("_", " ") for x in text.split(",")]
    names = [next(n for n in rrc.NATION_ROWS if n.lower() == w.lstrip("-")) for w in words if w != "all"]
    nations = (["all"] + names) if words[0] == "all" else names
    if nations[0] != "all" and "England" not in nations:
        nations = ["England"] + nations                      # Arsenal: England stays selected
    return nations


def folders(work):
    user, home, shots = (os.path.join(work, d) for d in ("user", "home", "shots"))
    for d in (user, home, shots):
        os.makedirs(d)
    return user, home, shots


def write_settings(user, argv, database):
    options = {"display": "classic", "changing_rules": arg(argv, "--changing-rules", "on"), "faster_days": "on"}
    if database:
        cdr.copy_data(database, os.path.join(user, "Databases", os.path.basename(database)))
        options["database"] = "folder:" + os.path.basename(database)
    with open(os.path.join(user, "settings.txt"), "w") as f:
        for k, v in options.items():
            f.write(f"{k} = {v}\n")


def run_leg(o, work, save, days):
    """Runs one leg: the new game (save None) or the load of save, to a save after days game days. Returns
    (faults, minutes)."""
    user, home, shots = folders(work)
    write_settings(user, o["argv"], o["database"])
    if save is None:
        steps = rrc.league_steps(nation_list(arg(o["argv"], "--nations", "all")), [])
    else:
        import fixtures
        shutil.copyfile(save, os.path.join(user, "full.sav"))
        # The save was made on holiday, so the loaded game goes on by itself and shows no first news to wait for.
        load = smoke_run.without_intro_click(fixtures.LOAD_STEPS)
        steps = [x for x in load if x != "wait first-news"] + [f"{o['timeout'] * 1000} quit"]
    table = os.path.join(os.path.dirname(o["program"]), "seasons", "rules.tsv")
    env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
               RECOMP_USER_DIR=user, RECOMP_FIXED_CLOCK="1", RECOMP_SCREEN_LOG="1",
               RECOMP_SAVE_AFTER_DAYS=str(days), RECOMP_START_SEASON=str(o["start"]),
               RECOMP_RULES_REPORT=os.path.join(work, "report.tsv"), RECOMP_RULES_TABLE=table,
               RECOMP_SEASON_INTL_LOG="1",
               RECOMP_SCRIPT="; ".join(smoke_run.scaled_steps("classic", shots, steps)))
    wait_for_room()
    t0 = time.time()
    err = os.path.join(work, "stderr.txt")
    fails, box = [], None
    with open(err, "w") as log:
        p = subprocess.Popen([o["program"], "--game", o["game"], "-seed", o["seed"]], env=env,
                             stdout=subprocess.DEVNULL, stderr=log)
        last_date, moved = None, time.time()
        while p.poll() is None:
            time.sleep(30)
            text = open(err, errors="replace").read()
            m = BOX.search(text)
            if m:
                time.sleep(3)
                dates = DATE.findall(open(err, errors="replace").read())
                box = f"{m.group(2).strip()} (from {m.group(1)}, day {'/'.join(dates[-1]) if dates else '?'})"
                p.terminate()
                break
            dates = DATE.findall(text)
            if dates:
                day, year = int(dates[-1][0]), int(dates[-1][1])
                if (day, year) != last_date:
                    last_date, moved = (day, year), time.time()
                if year > o["start"] and day >= 182 and year not in o["reached"]:
                    o["reached"].add(year)
                    print(f"1 July {year}: {(time.time() - o['t0']) / 60:.0f} min", flush=True)
            # The making or the load of a game (date day 0, or no date yet) is not a stall: with Select All on a
            # loaded Mac it takes about 30 minutes. The stall clock runs from the first date past day 0.
            if last_date and last_date[0] > 0 and time.time() - moved > o["stall"]:
                fails.append(f"stall: the date stays at {last_date} for {o['stall'] // 60} minutes")
                p.terminate()
                break
            if time.time() - t0 > o["timeout"]:
                fails.append(f"timeout after {o['timeout']} s")
                p.terminate()
                break
        p.wait()
    if box:
        fails.append(f"message box: {box}")
    elif not fails:
        if p.returncode != 0:
            fails.append(f"the program ended with code {p.returncode}")
        if not os.path.exists(os.path.join(user, smoke_run.SAVE_NAME)):
            fails.append(f"no save after {days} game days")
    return fails, (time.time() - t0) / 60


def main(argv):
    if len(argv) < 3 or "--keep" not in argv:
        print(__doc__)
        return 2
    start = int(arg(argv, "--start", 2001))
    o = {"argv": argv, "program": os.path.abspath(argv[1]), "game": os.path.abspath(argv[2]), "start": start,
         "database": os.path.abspath(arg(argv, "--database")) if "--database" in argv else None,
         "seed": arg(argv, "--seed", "1"), "stall": 60 * int(arg(argv, "--stall-minutes", 20)),
         "timeout": int(arg(argv, "--timeout", 86400)), "reached": set(), "t0": time.time()}
    keep = os.path.abspath(arg(argv, "--keep"))
    if "--end" in argv:
        end = int(arg(argv, "--end"))
        legs = [365] * (end - start) + [int(arg(argv, "--end-days", 62))]
    else:
        legs = [365] * int(arg(argv, "--seasons", 2))
    if not legs or min(legs) <= 0:
        print("full_play_run.py: no leg to play (check --end or --seasons)")
        return 2
    if os.path.exists(keep):
        print(f"full_play_run.py: {keep} exists; give a new --keep folder")
        return 2
    os.makedirs(keep)
    summary = os.path.join(keep, "full.tsv")
    with open(summary, "w") as f:
        f.write("leg\tseason\tresult\tminutes\tfault\n")
    fails, save = [], None
    for leg, days in enumerate(legs):
        season = start + leg
        work = os.path.join(keep, f"leg{leg:02d}")
        faults, minutes = run_leg(o, work, save, days)
        result = "ok" if not faults else "FAIL"
        with open(summary, "a") as f:
            f.write(f"{leg}\t{season}/{(season + 1) % 100:02d}\t{result}\t{minutes:.0f}\t{'; '.join(faults)}\n")
        print(f"leg {leg} ({season}/{(season + 1) % 100:02d}, {days} days): {result} after {minutes:.0f} min"
              + (": " + "; ".join(faults) if faults else ""), flush=True)
        if faults:
            fails += [f"leg {leg} ({season}/{(season + 1) % 100:02d}): {x} ({work})" for x in faults]
            break
        save = os.path.join(work, "user", smoke_run.SAVE_NAME)
    # Every division moves on from one season to the next (rules_report_check.league_stalls, O278), all legs.
    reports = [os.path.join(keep, f"leg{n:02d}", "report.tsv") for n in range(len(legs))]
    comps = [c for r in reports if os.path.exists(r) for c in rrc.read_report(r)[1]]
    fails += [f"league stall: {line}" for line in rrc.league_stalls(comps)]
    print(f"run: {(time.time() - o['t0']) / 60:.0f} min, folder {keep}")
    for f in fails:
        print(f"FAIL  {f}")
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
