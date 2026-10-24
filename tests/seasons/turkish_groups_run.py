#!/usr/bin/env python3
"""Turkish Category B in three groups with Summer 2002 (spec docs/specs/2026-10-24-summer-2002-db-spec.md, slice 5):
the season core case of the groups, promotion and relegation across two season changes, and the other databases.

Usage:
    python3 tests/seasons/turkish_groups_run.py <cm0102_recomp> <game folder> --database "<Summer 2002 folder>"
        [--only summer|other|summer-off] [--ref-build <program>] [--keep <folder>] [--jobs n]

The runs (the season jump of tests/seasons/rules_report_check.py, Select All less the 15 other rows of the Select
League(s) screen, so Turkey plays; the runtime's lines "recomp: turkish groups: ...",
src/recomp_rt/win/season_tur_groups.c):
    summer  a 2002/03 start with the Summer 2002 folder, to the end of 2004/05 (two season changes):
            - 2002/03: Groups A, B and C hold 18, 17 and 17 clubs; each real club of the 2002/03 rows of
              docs/research/rules-0.6.0/rus-tur-clubs.tsv (by its database name) that the game places is in its
              real group, and a group misses no more real clubs than the runtime keeps in another division (the
              club moves of a season game keep two of them in Category A, where real clubs are missing);
            - each season change: each group sends its winner (place 1) to Category A and its last two to the Lower
              Division, Category A sends its relegated clubs to Category B, and nothing stays "not moved";
            - the next season: 18, 17 and 17 clubs again, none of the promoted or relegated clubs, every club that
              Category A relegated, and each club that stays in Category B in its group;
            - the run ends with code 0 and no message box.
    other   a 2002/03 start with the game's own 3.9.68 data to the end of 2003/04 (one season change): no line of the
            groups (the five groups of 10 of the original). With --ref-build the same run of that program gives the
            same rules report, so the change does not touch another database.
    summer-off  (only with --only summer-off) the summer game with RECOMP_TURKISH_GROUPS=off, to tell whether a fault
            of the summer run is the groups' own.
A message box stops a run at once (its code is "box").
--keep keeps each run's folder there (<keep>/<run>). Each run takes about 20 to 40 minutes.
"""
import concurrent.futures
import csv
import filecmp
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import rules_report_check as rrc  # noqa: E402

OTHERS = [n for n in rrc.NATION_ROWS if n != "England"]
GROUP = re.compile(r"recomp: turkish groups: (\d{4})/\d\d Group ([ABC]): (\d+) clubs:(.*)$")
UP = re.compile(r"recomp: turkish groups: (\d{4})/\d\d Group ([ABC]) place (\d+) up: (.*) \((.*)\)$")
DOWN = re.compile(r"recomp: turkish groups: (\d{4})/\d\d Group ([ABC]) place (\d+) down: (.*) \((.*)\)$")
A_DOWN = re.compile(r"recomp: turkish groups: (\d{4})/\d\d Category A down: (.*) \((.*)\)$")
SIZES = {"A": 18, "B": 17, "C": 17}


def real_groups():
    """{letter: set of database club names} of the 2002/03 Category B rows of the club table."""
    out = {k: set() for k in SIZES}
    with open(os.path.join(ROOT, "docs", "research", "rules-0.6.0", "rus-tur-clubs.tsv"), encoding="utf-8") as h:
        for r in csv.DictReader(h, delimiter="\t"):
            m = re.match(r"Turkish 2\. Division Category B \(real: 2\. Lig Category B, Group ([ABC])\)", r["division"])
            if r["season"] == "2002/03" and m:
                out[m.group(1)].add(r["db_club"])
    return out


def read_log(path):
    groups, ups, downs, a_downs, boxes = {}, [], [], [], []
    with open(path, encoding="latin-1") as h:
        for line in h:
            line = line.rstrip("\n")
            if "recomp: message box from" in line:
                boxes.append(line)
            m = GROUP.search(line)
            if m:
                names = [n.strip() for n in m.group(4).split(";") if n.strip()]
                groups[(int(m.group(1)), m.group(2))] = (int(m.group(3)), names)
            for rx, out in ((UP, ups), (DOWN, downs)):
                m = rx.search(line)
                if m:
                    out.append((int(m.group(1)), m.group(2), int(m.group(3)), m.group(4), m.group(5)))
            m = A_DOWN.search(line)
            if m:
                a_downs.append((int(m.group(1)), m.group(2), m.group(3)))
    return groups, ups, downs, a_downs, boxes


def check_summer(log, first, last):
    groups, ups, downs, a_downs, boxes = read_log(log)
    bad = [f"message box: {b}" for b in boxes]
    real = real_groups()
    with open(log, encoding="latin-1") as h:
        text = h.read()
    warn = r"recomp: turkish groups: (?:no dates for|\d+ clubs make groups of|no memory)[^\n]*"
    bad += [f"runtime warning: {w}" for w in re.findall(warn, text)]
    m = re.search(r"turkish groups: three groups of \d+, \d+ and \d+ clubs; (\d+) tagged clubs stay", text)
    if not m:
        return bad + ["no line of the first-season groups"]
    stayed = int(m.group(1))
    missing_all = []
    for k, n in SIZES.items():
        got = groups.get((first, k))
        if not got:
            bad.append(f"{first}/{first + 1 - 2000:02d} Group {k}: no line")
            continue
        if got[0] != n or len(got[1]) != n:
            bad.append(f"{first} Group {k}: {got[0]} clubs, not {n}")
        wrong = [c for c in got[1] if any(c in real[j] for j in SIZES if j != k)]
        if wrong:
            bad.append(f"{first} Group {k}: real clubs of another group: {wrong}")
        missing_all += sorted(real[k] - set(got[1]))
    if len(missing_all) > stayed:
        bad.append(f"{first}: real clubs missing {missing_all}, more than the {stayed} that stay elsewhere")
    for y in range(first, last):
        nxt = {k: set(groups.get((y + 1, k), (0, []))[1]) for k in SIZES}
        everyone = set().union(*nxt.values())
        for k, n in SIZES.items():
            size = len(groups.get((y, k), (0, []))[1])
            u = [r for r in ups if r[0] == y and r[1] == k]
            d = [r for r in downs if r[0] == y and r[1] == k]
            if [(r[2], r[4]) for r in u] != [(1, "Category A")]:
                bad.append(f"{y} Group {k}: up {u}, not the winner to Category A")
            if sorted((r[2], r[4]) for r in d) != [(size - 1, "Lower Division"), (size, "Lower Division")]:
                bad.append(f"{y} Group {k}: down {d}, not the last two to the Lower Division")
            if len(nxt[k]) != n or groups.get((y + 1, k), (0,))[0] != n:
                bad.append(f"{y + 1} Group {k}: {len(nxt[k])} clubs, not {n}")
            for r in u + d:
                if r[3] in everyone:
                    bad.append(f"{y + 1}: {r[3]} moved but is in the groups")
        before = {c: k for k in SIZES for c in groups.get((y, k), (0, []))[1]}
        moved = sorted(c for k in SIZES for c in nxt[k] if c in before and before[c] != k)
        if moved:
            bad.append(f"{y + 1}: clubs that stay in Category B changed group: {moved[:6]}")
        ad = [r for r in a_downs if r[0] == y]
        if not ad or any(r[2] != "Category B" for r in ad):
            bad.append(f"{y}: Category A relegated {ad}")
        bad += [f"{y + 1}: {r[1]}, relegated from Category A, is not in the groups" for r in ad if r[1] not in everyone]
    return bad


def run_one(name, program, game, database, year, last, keep, env=None):
    work = os.path.join(keep, name) if keep else tempfile.mkdtemp(prefix=f"cm0102-turkey-{name}-")
    nations = ["all"] + OTHERS
    code, report = rrc.run(program, game, last, year, database, nations, [], work, 4 * 3600, env_more=env,
                           stop_on_box=True)
    return code, report, os.path.join(work, "stderr.txt")


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    database = os.path.abspath(rrc.arg(argv, "--database")) if "--database" in argv else None
    only = rrc.arg(argv, "--only")
    ref = rrc.arg(argv, "--ref-build")
    keep = os.path.abspath(rrc.arg(argv, "--keep")) if "--keep" in argv else None
    jobs = int(rrc.arg(argv, "--jobs", 3))
    runs = []
    if only in (None, "summer"):
        if not database:
            print("--database <Summer 2002 folder> is needed for the summer run")
            return 2
        runs.append(("summer", program, database, 2002, 2004))
    if only == "summer-off":                # the same game with the groups off: is a fault the groups' own?
        runs.append(("summer-off", program, database, 2002, 2004))
    if only in (None, "other"):
        runs.append(("other", program, None, 2002, 2003))
        if ref:
            runs.append(("other-ref", os.path.abspath(ref), None, 2002, 2003))
        else:
            print("the other run needs --ref-build <program of main>: it proves Q152 against it")
            return 2
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = {r[0]: pool.submit(run_one, r[0], r[1], game, r[2], r[3], r[4], keep,
                                     {"RECOMP_TURKISH_GROUPS": "off"} if r[0] == "summer-off" else None) for r in runs}
        results = {k: f.result() for k, f in futures.items()}
    failed = []
    for name, (code, report, log) in results.items():
        if code != 0:
            failed.append(f"{name}: the run ended with code {code} ({log})")
            continue
        if name == "summer":
            failed += [f"summer: {b}" for b in check_summer(log, 2002, 2004)]
        elif name == "other":
            with open(log, encoding="latin-1") as h:
                lines = [x for x in h if "recomp: turkish groups" in x]
            failed += [f"other: a groups line: {x.strip()}" for x in lines[:5]]
    if "other" in results and "other-ref" in results and results["other"][0] == 0 and results["other-ref"][0] == 0:
        if not filecmp.cmp(results["other"][1], results["other-ref"][1], shallow=False):
            failed.append("other: the rules report differs from that of the reference build")
    for name, (code, report, log) in results.items():
        print(f"{name}: code {code}, log {log}")
    for f in failed:
        print("FAIL  " + f)
    print("PASS" if not failed else f"FAILED: {len(failed)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
