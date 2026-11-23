#!/usr/bin/env python3
"""New-game check on every Update Team database (0.4.0 slice 12).

Usage:
    python3 tests/smoke/update_team_sweep.py <cm0102_recomp> <game folder> <folder of databases> [--jobs n]
        [--timeout s] [--out table.md] [--seasons <Seasons folder> --seasons-tool <seasons_db_test>]
        [--select-all [--start <year>]]

<folder of databases> holds one folder per database (Max keeps them in test-fixtures/databases). Each run
copies one whole folder into Databases of a new user folder, sets database = folder:<name> and plays the
steps of smoke_run.py up to the first news screen and holiday. A run passes when the date box moves on
after "Go on Holiday". A run that fails or hangs names the last step it reached. At most --jobs (6) games
run at the same time. The table is a Markdown table (folder, result, seconds, failure step).

With --seasons the databases are those of a download (0.6.0 slice 14): <Seasons folder> holds the season
database zips (tools/release/make_season_zips.py). The run unpacks each one with the first pick of the game
(seasons_db_prepare_in, through the test program seasons_db_test that ctest builds) into a new save folder, and
sweeps those folders. <folder of databases> is then not used: give any existing folder.

With --select-all (0.6.0) each run clicks Select All on Select League(s) and starts the game in the season of
its database (RECOMP_START_SEASON, the year from docs/research/2026-10-16-season-database-table.md). Only the 21
season databases of that table run. A message box of the game ends the run and names its text in the failure
step. In a season that is not 2001 the box "fra_first..cpp 108" is an expected stop and counts as a pass
(EXPECTED_STOP); "Database.cpp 1844" and any other box fail. The table gives the reserve rows of blank staff
that the new game used (staff_reserve.c). --start <year> starts every game in that year instead.
"""
import concurrent.futures
import os
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import custom_database_run as cdr  # noqa: E402
import smoke_run  # noqa: E402

SHOTS = ["1-settings", "2-setup", "3-leagues", "4-news", "5-holiday"]
AFTER = {"1-settings": "the setup screen", "2-setup": "Select League(s) or the name entry",
         "3-leagues": "the new game build (after Next on Select League(s))", "4-news": "Go on Holiday",
         "5-holiday": "the end"}


def check(program, game, base, name, timeout, extra=None, steps=None, env_extra=None):
    """Returns (passed, seconds, failure text)."""
    start = time.time()
    with tempfile.TemporaryDirectory(prefix="cm0102-sweep-") as work:
        shots, home, user = (os.path.join(work, d) for d in ("shots", "home", "user"))
        for d in (shots, home, user):
            os.mkdir(d)
        folder = os.path.join(user, "Databases", name)
        cdr.copy_data(os.path.join(base, name), folder)
        if extra:
            extra(folder)
        with open(os.path.join(user, "settings.txt"), "w") as f:
            f.write(f"display = classic\ndatabase = folder:{name}\n")
        env = dict(os.environ, HOME=home, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
                   RECOMP_USER_DIR=user,
                   RECOMP_SCRIPT="; ".join(smoke_run.scaled_steps("classic", shots, *([steps] if steps else []))),
                   **(env_extra or {}))
        try:
            if env_extra and env_extra.get("RECOMP_SCREEN_LOG"):     # --select-all: stop at a box of the game
                r = run_to_box(program, game, env, timeout)
                RESERVE[name] = reserve_text(r.stderr)
            else:
                r = subprocess.run([program, "--game", game], env=env, capture_output=True, text=True,
                                   timeout=timeout)
        except subprocess.TimeoutExpired as e:
            done = [s for s in SHOTS if os.path.exists(os.path.join(shots, s + ".bmp"))]
            last = done[-1] if done else "none"
            return False, time.time() - start, f"hang (no end after {timeout} s) after {last}: {AFTER.get(last, '?')}" \
                + box_text(e.stderr)
        took = time.time() - start
        done = [s for s in SHOTS if os.path.exists(os.path.join(shots, s + ".bmp"))]
        last = done[-1] if done else "none"
        if r.returncode != 0:
            line = next((ln for ln in r.stderr.splitlines() if "timed out" in ln), r.stderr[-120:].strip())
            if r.returncode == 99:      # --select-all: a message box of the game ended the run
                if EXPECTED_STOP in box_text(r.stderr) and env_extra.get("RECOMP_START_SEASON") != "2001" \
                        and reserve_text(r.stderr).endswith("result 1"):     # the staff step ran and passed
                    return True, took, f"expected stop after {last}: {EXPECTED_STOP} (Ligue 1 club count, slice 15)"
                return False, took, f"box after {last}" + box_text(r.stderr)
            return False, took, f"code {r.returncode} after {last}: {line[:110]}"
        if "5-holiday" not in done:
            return False, took, f"no holiday picture, last {last}"
        moved = smoke_run.differing_share(os.path.join(shots, "5-holiday.bmp"), os.path.join(shots, "4-news.bmp"),
                                          smoke_run.scaled_box("classic"))
        if moved <= 0.02:
            return False, took, "no days passed on holiday"
        return True, took, ""


# --select-all (0.6.0): every league selected, the game in the season of its database.
SELECT_ALL = "2000 click 592 155"           # Select League(s): Select All
SEASON_TABLE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "docs", "research",
                            "2026-10-16-season-database-table.md")


# --select-all in a season that is not 2001: every season database holds 18 Ligue 1 clubs, and FraFirst wants 20
# (docs/research/2026-10-17-select-all-databases.md). Until slice 15 moves 2 clubs up, this box is the expected
# end of the run. It counts as a pass: the run has passed the staff step, where "Database.cpp 1844" is the failure.
EXPECTED_STOP = "fra_first..cpp 108"
RESERVE = {}                                # --select-all: database folder: reserve rows used (staff_reserve.c)


def reserve_text(err):
    """The reserve line of staff_reserve.c in the stderr of a run, shortened, or ''."""
    line = next((ln for ln in (err or "").splitlines() if "extra non-playing staff:" in ln), "")
    return line.split("staff: ", 1)[-1].replace(" reserve rows used", "") if line else ""


def box_text(err):
    """The text of the last message box of the game in the stderr of a run (RECOMP_SCREEN_LOG), or ''."""
    if isinstance(err, bytes):
        err = err.decode(errors="replace")
    boxes = [ln.split(": ", 2)[-1] for ln in (err or "").splitlines() if "recomp: message box from" in ln]
    return f"; box: {boxes[-1][:80]}" if boxes else ""


def run_to_box(program, game, env, timeout):
    """subprocess.run, but a message box of the game (it waits for a click for ever) ends the run at once with
    code 99. The box text is in the stderr of the result."""
    import threading
    p = subprocess.Popen([program, "--game", game], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                         text=True, errors="replace")
    lines, boxed = [], threading.Event()

    def read():
        for ln in p.stderr:
            lines.append(ln)
            if "recomp: message box from" in ln:
                boxed.set()
                p.kill()
    t = threading.Thread(target=read, daemon=True)
    t.start()
    try:
        p.wait(timeout)
    except subprocess.TimeoutExpired:
        p.kill()
        t.join(5)
        raise subprocess.TimeoutExpired(program, timeout, stderr="".join(lines))
    t.join(5)
    return subprocess.CompletedProcess(program, 99 if boxed.is_set() else p.returncode, "", "".join(lines))


def season_databases():
    """{database folder: start year} of the 21 season databases of the season database table."""
    out = {}
    for line in open(SEASON_TABLE):
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) >= 2 and cells[0][:4].isdigit() and "/" in cells[0] and cells[1] not in ("none", "") \
                and not cells[1].startswith("3.9."):
            out[cells[1]] = int(cells[0][:4])
    return out


def season_folders(names):
    """{folder: (neutral name, start year)} of the folders that hold a season database. A source folder can
    have more words after the neutral name of the table (for example "October 2003 ..."): the tables and the
    log print the neutral name only."""
    out = {}
    for key, year in season_databases().items():
        folder = next((n for n in names if n == key), None) or next((n for n in names if n.startswith(key + " ")), None)
        if folder:
            out[folder] = (key, year)
    return out


def select_all_steps():
    """The steps of smoke_run.py with Select All. With more than one league the game asks for the start season
    (screen 0x81e8d0: Next keeps the first nation) and the Select Team screen has a Nation box above the
    list, so the team is one row lower. A game with every league takes many minutes to make."""
    out = []
    for s in smoke_run.STEPS:
        if s.startswith("2500 click 705 572") and not any(x == SELECT_ALL for x in out):
            out += [SELECT_ALL, "4000 click 705 572", "wait screen:0x81e8d0 600000", "2500 click 705 572"]
            continue
        if s in ("wait typing", "wait first-news"):
            s += " 1800000"
        if s == "wait screen:0x454900":
            s += " 600000"
        if s.startswith("1000 click 155 163"):
            s = "2000 click 155 188"
        if s.endswith("quit"):
            s = "60000 quit"
        out.append(s)
    return out


def clean_euro(game):
    """A folder change that puts the clean euro.cfg in the folder (the mismatch case of the guard)."""
    def put(folder):
        src = os.path.join(game, "Data", "euro.cfg")
        with open(src, "rb") as a, open(os.path.join(folder, "euro.cfg"), "wb") as b:
            b.write(a.read())
    return put


def unpack_seasons(seasons, tool, work):
    """Unpacks every season zip with the first pick of the game. Returns the Databases folder."""
    game = os.path.join(work, "game")
    save = os.path.join(work, "save")
    os.makedirs(game)
    os.makedirs(save)
    os.symlink(seasons, os.path.join(game, "Seasons"))
    for year in range(2001, 2027):
        r = subprocess.run([tool, "prepare", game, save, str(year)], capture_output=True, text=True)
        if "status=0 " in r.stdout:
            print(f"unpacked {r.stdout.strip()}", flush=True)
        elif "status=4 " in r.stdout or r.returncode:
            raise SystemExit(f"first pick of {year} failed: {r.stdout} {r.stderr}")
    return os.path.join(save, "Databases")


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2
    program, game, base = (os.path.abspath(a) for a in argv[1:4])
    if "--seasons" in argv:
        import shutil
        seasons = os.path.abspath(argv[argv.index("--seasons") + 1])
        tool = os.path.abspath(argv[argv.index("--seasons-tool") + 1])
        work = tempfile.mkdtemp(prefix="cm0102-seasons-sweep-")
        try:
            base = unpack_seasons(seasons, tool, work)
            return sweep(argv, program, game, base)
        finally:
            shutil.rmtree(work, ignore_errors=True)
    return sweep(argv, program, game, base)


def sweep(argv, program, game, base):
    jobs = int(argv[argv.index("--jobs") + 1]) if "--jobs" in argv else 6
    timeout = int(argv[argv.index("--timeout") + 1]) if "--timeout" in argv else 420
    out = argv[argv.index("--out") + 1] if "--out" in argv else None
    names = sorted(n for n in os.listdir(base) if os.path.isdir(os.path.join(base, n)) and n != "zips")
    years = season_folders(names) if "--select-all" in argv else None
    if years:
        names = sorted(years, key=lambda n: years[n][1])
        if "--start" in argv:      # every game in this start year in place of the season of its database
            start = int(argv[argv.index("--start") + 1])
            years = {n: (key, start) for n, (key, _) in years.items()}
    work = [(n, None) for n in names]
    if "May 2026" in names and not years:
        work.append(("May 2026", clean_euro(game)))      # the guard case: euro.cfg that does not match
    rows, select_rows = [], []
    with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
        if years:      # --select-all
            futs = [(n, ex, pool.submit(check, program, game, base, n, timeout, ex, select_all_steps(),
                                        {"RECOMP_START_SEASON": str(years[n][1]), "RECOMP_SCREEN_LOG": "1"}))
                    for n, ex in work]
        else:
            futs = [(n, ex, pool.submit(check, program, game, base, n, timeout, ex)) for n, ex in work]
        for n, ex, fut in futs:
            ok, took, why = fut.result()
            label = (years[n][0] if years else n) + (" + clean euro.cfg" if ex else "")
            rows.append((label, ok, took, why))
            if years:
                select_rows.append((years[n][0], years[n][1], ok, took, RESERVE.get(n, ""), why))
            print(f"{'ok  ' if ok else 'FAIL'} {label}: {took:.0f} s {why}", flush=True)
    table = ["| Folder | Result | Seconds | Failure step |", "|---|---|---|---|"]
    table += [f"| {n} | {'pass' if ok else 'FAIL'} | {took:.0f} | {why} |" for n, ok, took, why in rows]
    if years:      # --select-all: the start year and the reserve rows that the new game used
        table = ["| Database | Start | Result | Seconds | Reserve rows used | Failure step |", "|---|---|---|---|---|---|"]
        table += [f"| {n} | {y} | {'pass' if ok else 'FAIL'} | {took:.0f} | {res} | {why} |"
                  for n, y, ok, took, res, why in select_rows]
    if out:
        with open(out, "w") as f:
            f.write("\n".join(table) + "\n")
    print("\n".join(table))
    failed = sum(not ok for _, ok, _, _ in rows)
    print(f"{len(rows) - failed} of {len(rows)} pass")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
