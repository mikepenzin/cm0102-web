#!/usr/bin/env python3
"""The full check of real league sizes (0.7.2 league sizes spec, slice 18).

Usage:
    python3 tests/seasons/sizes_full_check.py --plan
        prints the runs
    python3 tests/seasons/sizes_full_check.py --run <cm0102_recomp> <game folder> --out <folder>
        [--only <name,...>] [--jobs 2] [--max-load 24] [--floor-gb 100]
        plays the runs of the plan with tests/seasons/rules_report_check.py --run, changing rules on, and writes
        each run's output to <out>/<name>.log and its report and game log to <out>/<name>/
    python3 tests/seasons/sizes_full_check.py --classify <folder> [--files <file,...>]
        reads the logs and the expected files of the runs and fails on each size difference of a grown league

The runs (spec slice 18): the rules report of every nation from 2001 to 2027 with changing rules on, and a start in
each nation group in 2010, 2016 and 2025 that plays to the next season change. The two nation groups are the two
halves of the Select League(s) screen that the expected files of the rules report already use:
    g1  England and the 15 other nations of the first 16 rows (Argentina to Norway), with the English Conference,
        the German regional divisions and Serie C2
    g2  Select All without those 15 nations: England and the 10 nations below Norway, with the extra divisions of
        Spain and Portugal
Together they hold every nation of the game and every extra division.

Each run uses --update-expected, so its differences with the rules table are written to its expected file
(tests/seasons/rules_report_expected/changing-<year>-<nations>.tsv). The classify step then fails on:
    - a run that ended with a failure other than the missing or changed expected file (an error box, a code
      other than 0, a report without every season, a news sentence twice)
    - a clubs, meetings or rounds difference of a grown league: a file of GROWN in tools/recomp/league_sizes.py,
      or a file of --files
It lists every other difference by kind, for the review of the slice: a difference that comes from 0.7.2 is a
fault and goes to the open items. Last it runs tests/seasons/sizes_count_changes.py on the reports of the two 2001
runs (D23: every count change of the rules table shows in its season, and the season before plays its real
places).

The run takes the database of its start year as rules_report_check.py picks it (2001: the base data).
"""
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
CHECK = os.path.join(HERE, "rules_report_check.py")
EXPECTED_DIR = os.path.join(HERE, "rules_report_expected")

FIRST_ROWS = ["argentina", "australia", "belgium", "brazil", "croatia", "denmark", "finland", "france", "germany",
              "greece", "holland", "italy", "japan", "northern_ireland", "norway"]
GROUPS = {
    "g1": (["england"] + FIRST_ROWS, ["conference", "regional", "c2"]),
    "g2": (["all"] + ["-" + n for n in FIRST_ROWS], ["spain", "portugal"]),
}
SIZE_KINDS = ("clubs", "meetings", "rounds")


def plan():
    """(name, group, start year, last year). The 2001 runs play the base data to 2027/28."""
    runs = [(f"r2001-{g}", g, 2001, 2027) for g in GROUPS]
    for year in (2010, 2016, 2025):
        runs += [(f"s{year}-{g}", g, year, year + 1) for g in GROUPS]
    return runs


def expected_name(group, year):
    """The expected file name that rules_report_check.py gives the run (its main(), changing rules on)."""
    nations, extra = GROUPS[group]
    if nations[0] == "all":
        tail = "".join(f"-no{n.lstrip('-')}" for n in nations[1:])
    else:
        tail = "".join(f"-{n}" for n in nations[1:])
    return f"changing-{year}-{nations[0]}{tail}" + "".join(f"-{e}" for e in extra)


def command(program, game, group, year, last, keep, timeout):
    nations, extra = GROUPS[group]
    argv = [sys.executable, CHECK, "--run", program, game, "--last", str(last), "--nations", ",".join(nations),
            "--extra", ",".join(extra), "--changing-rules", "on", "--update-expected", "--keep", keep,
            "--timeout", str(timeout)]
    if year != 2001:
        argv += ["--year", str(year)]
    return argv


def load():
    return os.getloadavg()[0]


def free_gb(path):
    s = os.statvfs(path)
    return s.f_bavail * s.f_frsize / 1e9


def run_all(argv):
    program, game = os.path.abspath(argv[2]), os.path.abspath(argv[3])
    out = os.path.abspath(arg(argv, "--out"))
    jobs = int(arg(argv, "--jobs", 2))
    max_load = float(arg(argv, "--max-load", 24))
    floor = float(arg(argv, "--floor-gb", 100))
    timeout = int(arg(argv, "--timeout", 172800))
    only = set(arg(argv, "--only", "").split(",")) - {""}
    os.makedirs(out, exist_ok=True)
    todo = [r for r in plan() if not only or r[0] in only]
    todo.sort(key=lambda r: r[2] != 2001)               # the long runs first
    running = {}
    while todo or running:
        for name, (p, f) in list(running.items()):
            if p.poll() is not None:
                f.close()
                with open(os.path.join(out, name + ".code"), "w") as c:
                    c.write(f"{p.returncode}\n")
                print(f"{time.strftime('%H:%M')} done {name}: code {p.returncode}", flush=True)
                del running[name]
        if todo and len(running) < jobs:
            if load() >= max_load:
                pass
            elif free_gb(out) < floor:
                print(f"{time.strftime('%H:%M')} less than {floor} GB free: no new run", flush=True)
            else:
                name, group, year, last = todo.pop(0)
                keep = os.path.join(out, name)
                if os.path.exists(keep):
                    raise SystemExit(f"{keep} exists: give another --out or remove it")
                f = open(os.path.join(out, name + ".log"), "w")
                cmd = command(program, game, group, year, last, keep, timeout)
                f.write(" ".join(cmd) + "\n")
                f.flush()
                p = subprocess.Popen(cmd, stdout=f, stderr=subprocess.STDOUT, cwd=ROOT)
                with open(os.path.join(out, name + ".pid"), "w") as pf:
                    pf.write(f"{p.pid}\n")
                running[name] = (p, f)
                print(f"{time.strftime('%H:%M')} start {name} (pid {p.pid}, load {load():.1f})", flush=True)
                continue
        time.sleep(30)
    return 0


def grown_files():
    sys.path.insert(0, os.path.join(ROOT, "tools", "recomp"))
    import league_sizes
    return set(league_sizes.GROWN)


def read_lines(path):
    out = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            s, kind, text = line.rstrip("\n").split("\t", 2)
            out.append((int(s), kind, text))
    return out


# The failures of the run log that only say the expected file was missing or changed: --update-expected wrote it.
EXPECTED_FAILS = re.compile(r"no expected file|differences that the expected file does not list")


def classify(argv):
    out = os.path.abspath(argv[2])
    files = grown_files() | (set(arg(argv, "--files", "").split(",")) - {""})
    print(f"grown leagues: {len(files)} files")
    failed = 0
    for name, group, year, last in plan():
        log = os.path.join(out, name + ".log")
        if not os.path.exists(log):
            print(f"{name}: not run")
            failed += 1
            continue
        with open(log, encoding="utf-8", errors="replace") as f:
            text = f.read()
        fails = [ln[6:] for ln in text.splitlines() if ln.startswith("FAIL  ") and not EXPECTED_FAILS.search(ln)]
        if "PASS" not in text.splitlines()[-1:] and not re.search(r"^FAILED: \d+$", text, re.M):
            fails.append("the check did not finish")
        exp = os.path.join(EXPECTED_DIR, expected_name(group, year) + ".tsv")
        diffs = read_lines(exp) if os.path.exists(exp) else []
        if not os.path.exists(exp):
            fails.append(f"no expected file {exp}")
        size = [d for d in diffs if d[1] in SIZE_KINDS and d[2].split(" ", 1)[0] in files]
        kinds = {}
        for d in diffs:
            kinds[d[1]] = kinds.get(d[1], 0) + 1
        verdict = "FAIL" if fails or size else "pass"
        failed += verdict == "FAIL"
        print(f"{name}: {verdict}; {len(diffs)} differences "
              f"({', '.join(f'{k} {n}' for k, n in sorted(kinds.items()))})")
        for t in fails:
            print(f"  run: {t}")
        for s, k, t in size:
            print(f"  size: {s}/{(s + 1) % 100:02d} {k} {t}")
    # D23: every count change of the rules table from 2001 to 2027 in the two 2001 runs.
    reports = [os.path.join(out, f"r2001-{g}", "report.tsv") for g in GROUPS]
    reports = [r for r in reports if os.path.exists(r)]
    if len(reports) != len(GROUPS):
        print("count changes: a 2001 run has no report")
        failed += 1
    else:
        print("count changes (D23):", flush=True)
        failed += subprocess.call([sys.executable, os.path.join(HERE, "sizes_count_changes.py")] + reports) != 0
    print("PASS" if not failed else f"FAILED: {failed}")
    return 1 if failed else 0


def arg(argv, name, default=None):
    return argv[argv.index(name) + 1] if name in argv else default


def main(argv):
    if len(argv) >= 2 and argv[1] == "--plan":
        for name, group, year, last in plan():
            print(f"{name}\t{year}\t{last}\t{expected_name(group, year)}")
        return 0
    if len(argv) >= 5 and argv[1] == "--run":
        return run_all(argv)
    if len(argv) >= 3 and argv[1] == "--classify":
        return classify(argv)
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
