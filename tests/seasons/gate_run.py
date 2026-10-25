#!/usr/bin/env python3
"""Gate (a) runner (0.6.0 slice 30a, docs/research/brief-0.6.0/slice-30a.md): one soak per start season, each played
to about 1 September of its end year (rule O166: after its later World Cup or European Championship), several at a
time within the machine limits.

Usage:
    python3 tests/seasons/gate_run.py <build folder> <run folder> [--starts <list>] [--max 10] [--machine 14]
        [--results <file.md> --run <name>] [--databases <folder>] [--own-dbs <folder>]

<build folder> holds cm0102_recomp and its seasons folder. The soaks run tests/seasons/soak_run.py of this checkout
with --nations all and a save and load each July. <run folder> gets one folder per start (gate-<item>, the soak's
--keep), a log per start and the own databases unpacked once (owndb/).

--starts: start items in order, joined by commas. An item is a start year (2001 to 2025, changing rules on), with
"off" for changing rules off and "c2" for the Serie C2 extra (--extra c2): 2016off, 2014c2. The default is run 8:
2001 to 2025 (oldest first), then 2001off, 2006off, 2011off, 2016off, 2021off (O273) and 2014c2 (O278: the 2014
start crosses the Italian change of 2015/16).

Databases: 2001 plays the base game. 2002, 2004 and 2006 play the own databases (Summer 2002.zip, Summer 2004.zip, Summer 2006.zip
in --own-dbs). Each other year plays the folder of --databases whose name starts with its date (DATES).

Limits: a new soak starts only while fewer than --max soaks of this run folder run, fewer than --machine games run on
the Mac, the load of the last minute is below 24 and /Users/max/Projects/cm0102/run-logs/HOLD does not exist; 30 s
between starts. Each soak waits for room before each leg itself.

The end of a start: when the last leg's log reaches day 244 (1 September) of the end year with no message box, the
runner stops the soak and then its game, by PID. The result:
    FAIL  a leg that is not ok (a box, an exit code, no save), the first one
    FAIL  a division or a domestic cup held each season whose edition does not move on
          (rules_report_check.league_stalls over the leg reports, O278, O324;
          the soak's own check does not run when the runner stops it)
    FAIL  a soak that ends before 1 September of the end year
    PASS  else
With --results and --run, each result is a row of the results table, put above the line "<!-- <run> rows -->" of
the file (or at its end). The runner prints each result too. A start whose folder exists and whose soak no longer
runs is skipped; a start whose soak still runs (an earlier runner) is watched to its end.
"""
import os
import re
import signal
import subprocess
import sys
import threading
import time
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rules_report_check as rrc  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))
GAME = "/Users/max/Projects/cm0102/clean-3968"
HOLD = "/Users/max/Projects/cm0102/run-logs/HOLD"
STOP_DAY = 244                      # 1 September, the day of the year from 0
# O166: the end year of each start, about 1 September after its later tournament.
ENDS = {2001: 2004, 2002: 2006, 2003: 2006, 2004: 2008, 2005: 2008, 2006: 2010, 2007: 2010, 2008: 2012, 2009: 2012,
        2010: 2014, 2011: 2014, 2012: 2016, 2013: 2016, 2014: 2018, 2015: 2018, 2016: 2021, 2017: 2021, 2018: 2023,
        2019: 2023, 2020: 2023, 2021: 2024, 2022: 2024, 2023: 2026, 2024: 2028, 2025: 2028}
# The start of the name of each year's database folder.
DATES = {2003: "October 2003", 2005: "September 2005", 2007: "November 2007", 2008: "September 2008",
         2009: "September 2009", 2010: "September 2010", 2011: "September 2011", 2012: "September 2012",
         2013: "Autumn 2013", 2014: "October 2014", 2015: "October 2015", 2016: "October 2016", 2017: "October 2017",
         2018: "October 2018", 2019: "October 2019", 2020: "November 2020", 2021: "October 2021",
         2022: "October 2022", 2023: "October 2023", 2024: "October 2024", 2025: "December 2025"}
OWN = {2002: "Summer 2002", 2004: "Summer 2004", 2006: "Summer 2006"}
RUN8 = [str(y) for y in range(2001, 2026)] + ["2001off", "2006off", "2011off", "2016off", "2021off", "2014c2"]
LOCK = threading.Lock()


def arg(argv, name, default=None):
    return argv[argv.index(name) + 1] if name in argv else default


def parse(item):
    """(year, rules, extra) of a start item."""
    m = re.fullmatch(r"(20\d\d)(off)?(c2)?", item)
    if not m or int(m.group(1)) not in ENDS:
        raise SystemExit(f"gate_run.py: unknown start item {item}")
    return int(m.group(1)), "off" if m.group(2) else "on", ["c2"] if m.group(3) else []


def database(y, o):
    """(--database arguments, label) of a start year. The own databases are unpacked on first use."""
    if y == 2001:
        return [], "3.9.68"
    if y in OWN:
        folder = os.path.join(o["run"], "owndb", str(y))
        path = os.path.join(folder, OWN[y])
        with LOCK:
            if not os.path.isdir(path):
                with zipfile.ZipFile(os.path.join(o["own"], OWN[y] + ".zip")) as z:
                    z.extractall(folder)
        return ["--database", path], OWN[y] + " (own)"
    names = sorted(n for n in os.listdir(o["databases"]) if n.startswith(DATES[y]))
    if not names:
        raise SystemExit(f"gate_run.py: no database folder for {DATES[y]} in {o['databases']}")
    return ["--database", os.path.join(o["databases"], names[0])], DATES[y]


def lines(cmd):
    return subprocess.run(cmd, capture_output=True, text=True).stdout.splitlines()


def machine_games():
    return len(lines(["pgrep", "-x", "cm0102_recomp"]))


def soak_of(keep):
    """The PID of a running soak with this --keep folder, or None."""
    for line in lines(["ps", "-axo", "pid=,command="]):
        pid, cmd = line.strip().split(" ", 1)
        if "soak_run.py" in cmd and (keep + " ") in (cmd + " ") and not cmd.startswith("/bin/"):
            return int(pid)
    return None


def my_soaks(o):
    return sum(1 for line in lines(["ps", "-axo", "command="])
               if "soak_run.py" in line and o["run"] + "/gate-" in line and not line.startswith("/bin/"))


def stop(pid):
    """Stops a soak, then its games, by PID."""
    kids = [int(k) for k in lines(["pgrep", "-P", str(pid)])]
    for p in [pid] + kids:
        try:
            os.kill(p, signal.SIGTERM)
        except ProcessLookupError:
            pass


def alive(pid):
    """1 while the soak runs. A soak that this runner started is its child, and an ended child stays a zombie until
    it is reaped: os.kill(pid, 0) succeeds for a zombie, so the runner waited for ever after its soaks ended and wrote
    no result row (O369 soaks, run 8). So: reap it (waitpid WNOHANG), then the signal test, then the process state
    of ps (a zombie of another parent, state Z). soak_of and my_soaks read the command lines of ps, which a zombie
    no longer has, so the slot count, the skip of a running start and this test agree."""
    try:
        done, _ = os.waitpid(pid, os.WNOHANG)
        if done == pid:
            return False
    except ChildProcessError:
        pass                                # not a child of this runner: a soak of an earlier runner
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    state = lines(["ps", "-o", "stat=", "-p", str(pid)])
    return bool(state) and not state[0].strip().startswith("Z")


def at_end(err, end):
    """The last leg's log reached 1 September of the end year with no message box."""
    if not os.path.exists(err):
        return False
    text = open(err, errors="replace").read()
    d = re.findall(r"date (\d+)/(\d+)", text)
    return bool(d) and int(d[-1][1]) == end and int(d[-1][0]) >= STOP_DAY and "message box from" not in text


def legs(keep):
    path = os.path.join(keep, "soak.tsv")
    if not os.path.exists(path):
        return []
    return [line.split("\t") for line in open(path, errors="replace").read().splitlines()[1:] if line]


def put_row(o, row):
    print(row.strip(), flush=True)
    if not o["results"]:
        return
    with LOCK:
        text = open(o["results"], encoding="utf-8").read()
        mark = f"<!-- {o['name']} rows -->\n"
        text = text.replace(mark, row + mark, 1) if mark in text else text + row
        open(o["results"], "w", encoding="utf-8").write(text)


def run(item, o):
    y, rules, extra = parse(item)
    end = ENDS[y]
    keep = os.path.join(o["run"], f"gate-{item}")
    db, label = database(y, o)
    pid = soak_of(keep)
    if pid is None:
        log = open(os.path.join(o["run"], f"gate-{item}.log"), "w")
        p = subprocess.Popen(["python3", "tests/seasons/soak_run.py", o["program"], GAME, "--keep", keep,
                              "--start", str(y), "--last", str(end), "--nations", "all", "--changing-rules", rules]
                             + (["--extra", ",".join(extra)] if extra else []) + db,
                             cwd=REPO, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        log.close()
        pid = p.pid
    last_err = os.path.join(keep, f"leg{end - y:02d}", "stderr.txt")
    while alive(pid):
        time.sleep(30)
        if at_end(last_err, end):
            stop(pid)                       # the soak first, so it writes no line for the stopped leg
            break
    while alive(pid):
        time.sleep(2)
    reached = at_end(last_err, end)         # also for a soak that an earlier runner stopped
    ls = legs(keep)
    bad = [x for x in ls if len(x) > 2 and x[2] != "ok" and not (reached and x[1].startswith(str(end)))]
    reports = [os.path.join(keep, f"leg{n:02d}", "report.tsv") for n in range(end - y + 1)]
    stalls = rrc.league_stalls([c for r in reports if os.path.exists(r) for c in rrc.read_report(r)[1]])
    minutes = ", ".join(f"{x[1]} {int(x[3]) // 60} min" for x in ls if len(x) > 3 and x[3].isdigit())
    if bad:
        res, first = "FAIL", " ".join(bad[0])[:250]
    elif stalls:
        res, first = "FAIL", "league stall: " + stalls[0][:230]
    elif reached:
        res, first = "PASS", f"to 1 September {end}"
    else:
        out = open(os.path.join(o["run"], f"gate-{item}.log"), errors="replace").read().strip().splitlines()
        res, first = "FAIL", (out or ["no output"])[-1][:200]
    put_row(o, f"| gate | {y}/{(y + 1) % 100:02d} | {label}{' + Serie C2' if extra else ''} | {rules} | 1 Sep {end} "
               f"| {res} | {first} | {minutes} | {o['name']}/gate-{item}/ |\n")


def room(o):
    return (my_soaks(o) < o["max"] and machine_games() < o["machine"] and os.getloadavg()[0] < 24
            and not os.path.exists(HOLD))


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    build, run_folder = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    o = {"program": os.path.join(build, "cm0102_recomp"), "run": run_folder,
         "max": int(arg(argv, "--max", 10)), "machine": int(arg(argv, "--machine", 14)),
         "results": os.path.abspath(arg(argv, "--results")) if "--results" in argv else None,
         "name": arg(argv, "--run", os.path.basename(run_folder)),
         "databases": arg(argv, "--databases", "/Users/max/Projects/cm0102/test-fixtures/databases"),
         "own": arg(argv, "--own-dbs", "/Users/max/Projects/cm0102/own-dbs")}
    items = arg(argv, "--starts", ",".join(RUN8)).split(",")
    if not os.path.exists(o["program"]):
        print(f"gate_run.py: no {o['program']}")
        return 2
    for item in items:                      # a bad item or a missing database stops the run before any game
        y, _, _ = parse(item)
        if y in OWN and not os.path.exists(os.path.join(o["own"], OWN[y] + ".zip")):
            raise SystemExit(f"gate_run.py: no {OWN[y]}.zip in {o['own']}")
        if y not in OWN:
            database(y, o)
    os.makedirs(run_folder, exist_ok=True)
    threads = []
    for item in items:
        keep = os.path.join(run_folder, f"gate-{item}")
        running = soak_of(keep) is not None
        if os.path.exists(keep) and not running:
            print(f"gate_run.py: {keep} exists; skipped", flush=True)
            continue
        while not running and not room(o):
            time.sleep(60)
        t = threading.Thread(target=run, args=(item, o))
        t.start()
        threads.append(t)
        if not running:
            time.sleep(30)
    for t in threads:
        t.join()
    print("gate queue done", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
