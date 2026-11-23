"""The long holiday test of a season database in the original game's start (forum release, 2026-10-22).

usage: python3 tools/seasons/own_db/long_test.py <program> <game folder> <out folder> [england-germany|all]

The game folder is a 3.9.68 game with the season database copied into its Data folder, as a player installs it.
The program is the built cm0102_recomp. The run uses the original start (RECOMP_SEASON_CODE=off, July 2001):
1. a new game with England and Germany (or Select All), Arsenal, then a holiday to 1 July 2002 and a save;
2. to 4. the save is loaded and the holiday goes on to 1 July 2003, 1 July 2004 and 1 September 2004, with a save
   at each date (three season changes, the 2002 World Cup, Euro 2004 and their qualifiers);
5. the last save is loaded and played one more day.
Each save of steps 1 to 4 goes through save_check.py (Real Madrid, Bayern and Arsenal squads). A step starts only
while the 1-minute load is below 24 and fewer than 15 games run (no other limit of the machine is read).

Pass: no error box in any step, every save written and loaded, every squad check passed. The run prints one line
a step and PASS or FAILED, and keeps each step's stderr.txt in the out folder. It takes about 40 minutes with
England and Germany and about 5 hours with every league.
"""
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tests", "smoke"))
sys.path.insert(0, os.path.join(ROOT, "tests", "fixtures"))
import fixtures  # noqa: E402
import smoke_run  # noqa: E402

# The days of each step as the test save counts them (year * 366 + day of the year, testrun.c): from the first
# human turn on 14 July 2001 to 1 July 2002, then to 1 July 2003, 1 July 2004 (a leap year), 1 September 2004.
STEP_DAYS = [353, 366, 367, 62, 1]
STEP_TIMEOUT = 4 * 3600
HOLIDAY = ["2500 click 45 165", "4500 click 150 427", "7000 click 705 572"]   # manager menu, Go on Holiday, Yes
SELECT_ALL = "591 155"                                                      # Select League(s): Select All
GERMANY = "390 385"
ARSENAL = "155 188"                                                         # Select Team: the first club


def wait_for_room():
    while True:
        games = subprocess.run(["pgrep", "-x", "cm0102_recomp"], capture_output=True, text=True).stdout.split()
        if os.getloadavg()[0] < 24 and len(games) < 15:
            return
        time.sleep(30)


def play(program, game, out, steps, days):
    shots, home, user = (os.path.join(out, d) for d in ("shots", "home", "user"))
    for d in (shots, home, user):
        os.makedirs(d, exist_ok=True)
    if not os.path.exists(os.path.join(user, "settings.txt")):
        with open(os.path.join(user, "settings.txt"), "w") as h:
            h.write("display = classic\ndatabase = 3.9.68\n")
    env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
               RECOMP_USER_DIR=user, RECOMP_SCREEN_LOG="1", RECOMP_SEASON_CODE="off",
               RECOMP_SAVE_AFTER_DAYS=str(days),
               RECOMP_SCRIPT="; ".join(smoke_run.scaled_steps("classic", shots, steps)))
    wait_for_room()
    with open(os.path.join(out, "stderr.txt"), "w") as err:
        try:
            r = subprocess.run([program, "--game", game], env=env, stdout=subprocess.DEVNULL, stderr=err,
                               timeout=STEP_TIMEOUT)
            code = r.returncode
        except subprocess.TimeoutExpired:
            code = "timeout"
    log = open(os.path.join(out, "stderr.txt"), encoding="latin-1").read()
    boxes = [ln for ln in log.splitlines() if "message box from" in ln]
    save = os.path.join(user, "samesave.sav")
    return code, boxes, save if os.path.exists(save) else None


def new_game_steps(leagues):
    s = [x.split("#")[0].strip() for x in smoke_run.STEPS]
    k = s.index("wait leagues") + 2
    clicks = [GERMANY] if leagues == "england-germany" else [SELECT_ALL]
    s[k:k + 1] = [f"{2500 + 700 * i} click {c}" for i, c in enumerate(clicks)] + \
                 [f"{3500 + 700 * len(clicks)} click 705 572", f"{7500 + 700 * len(clicks)} click 705 572"]
    if leagues == "all":                   # the load of every league takes minutes
        w = next(i for i, x in enumerate(s) if x.startswith("wait typing"))
        s[w] = "wait typing 1200000"
    j = s.index("wait screen:0x822260")
    s[j + 1] = "1000 click " + ARSENAL
    i = s.index("2000 dump {d}/4-news.bmp")
    return s[:i + 1] + HOLIDAY + ["7000000 quit"]


def main(argv):
    if len(argv) < 3:
        sys.exit(__doc__)
    program, game, out = (os.path.abspath(a) for a in argv[:3])
    leagues = argv[3] if len(argv) > 3 else "england-germany"
    if leagues not in ("england-germany", "all"):
        sys.exit(__doc__)
    fails, prev = [], None
    for n, days in enumerate(STEP_DAYS, 1):
        step = os.path.join(out, f"step{n}")
        if n == 1:
            code, boxes, save = play(program, game, step, new_game_steps(leagues), days)
        else:
            os.makedirs(os.path.join(step, "user"), exist_ok=True)
            shutil.copyfile(prev, os.path.join(step, "user", "long.sav"))
            # The holiday is in the save and goes on after the load: no clicks, no wait for the news.
            steps = list(fixtures.LOAD_STEPS[:-1]) + ["wait loaded 600000", "7000000 quit"]
            code, boxes, save = play(program, game, step, steps, days)
        line = f"step {n}: {days} days, code {code}, {len(boxes)} boxes, save {'written' if save else 'missing'}"
        fails += [f"step {n}: {b}" for b in boxes]
        if not save:
            fails.append(f"step {n}: no save ({step})")
            print(line)
            break
        if n < len(STEP_DAYS):
            r = subprocess.run([sys.executable, "-I", os.path.join(HERE, "save_check.py"), save],
                               capture_output=True, text=True)
            with open(os.path.join(step, "save_check.txt"), "w") as h:
                h.write(r.stdout + r.stderr)
            line += f", squads {'ok' if r.returncode == 0 else 'FAILED'}"
            if r.returncode:
                fails.append(f"step {n}: squad check ({step}/save_check.txt)")
        print(line, flush=True)
        prev = save
    for f in fails:
        print("FAIL  " + f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
