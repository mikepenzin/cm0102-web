#!/usr/bin/env python3
"""New divisions (0.6.0 slice 20, docs/research/2026-10-20-new-divisions.md): the season end of the 3. Liga, and a
save and a load across a season change for each new tier.

Usage:
    python3 tests/seasons/new_divisions_check.py <cm0102_recomp> <game folder> [--only <name>,...] [--keep <folder>]

The runs (changing_rules off, the database of the start season, test-fixtures/databases):
    liga3        2008/09 to 2009/10 with Germany and the regional divisions: at the season change of 2009 the 3. Liga
                 of 2008/09 is complete (+0x4c, set by League::update_for_date 0x684a9e when the season ends with its
                 history and honours) and has no stage (+0x2c 0, +0x30 -1); the log line of the exchange says so
    liga3-load   the same game saved 330 days after the start (June 2009, before the season change); a new program
                 loads the save and plays 30 days, across the change: the 3. Liga ends complete, 20 clubs, no box
    iberia-load  2021/22 with Spain and Portugal (their extra division by RECOMP_TEST_EXTRA), saved after 330 days,
                 loaded and played 30 days: Primera Federación and Liga 3 take their new clubs at the change, no box
    greece-load  2021/22 with Greece, saved after 330 days, loaded and played 30 days: the module makes the second
                 group again from its save file, the group's exchange runs at the change, no box
    por-cup      a 2021/22 start with Portugal's Liga 3 (and every nation below the 16 rows) to its first start report,
                 with the Portuguese Cup's safety net off (RECOMP_BOX_FIXES=off, rt_por_cup_none): the club moves send
                 the clubs that leave Liga 3's group divisions to the Third Division, so the cup finds its 164
                 entrants with no club of no division (O260 A24; before, por_cup.cpp 74)
    subset-extra a 2001/02 start (base data) with England and Italy and RECOMP_TEST_EXTRA=spain,portugal to its first
                 start report: the switch selects Spain and Portugal as the box of their extra division does
                 (id 0x38), so no box (item 226: before, por_super.cpp 211 from PorSuper::select_teams 0x7d3260)
A run takes about 5 to 25 minutes (iberia-load selects every nation below the 16 rows).
"""
import os
import re
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
import rules_report_check as rrc  # noqa: E402
import club_world_check as cwc  # noqa: E402

DATABASES = "/Users/max/Projects/cm0102/test-fixtures/databases"
SAVE_DAYS = 330                 # from mid July: early June, before the season changes of 20 June
PLAY_DAYS = 30
OTHERS = [n for n in rrc.NATION_ROWS if n not in ("England", "Greece")]
RUNS = {                        # name: (year, database, nations, extra, the log lines that must come after the change)
    "liga3-load": (2008, "September 2008", ["England", "Germany"], ["regional"],
                   [r"new divisions: 3\. Liga 2008: .* 20 clubs; season 2008 complete 1, stages 0 and -1"]),
    "iberia-load": (2021, "October 2021", ["all"] + OTHERS, ["spain", "portugal"],
                    [r"new divisions: spa_second_b 2021: .* 40 clubs", r"new divisions: por_second_b 2021: .* 24 clubs"]),
    "greece-load": (2021, "October 2021", ["England", "Greece"], [],
                    [r"new divisions: Greek second group 2021: .* 14 clubs"]),
}


def check_liga3(program, game, work):
    code, _ = rrc.run(program, game, 2009, 2008, os.path.join(DATABASES, "September 2008"), ["England", "Germany"],
                      ["regional"], work, 7200, options_more={"changing_rules": "off"},
                      env_more={"RECOMP_DIVISION_LOG": "1"})
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    fails = [] if code == 0 else [f"liga3: code {code} ({work})"]
    lines = re.findall(r"new divisions: 3\. Liga \d+: .*", log)
    print("  liga3: " + " | ".join(lines))
    if not re.search(r"3\. Liga 2008: .*season 2008 complete 1, stages 0 and -1", log):
        fails.append(f"liga3: the 3. Liga of 2008/09 did not end complete with no stage ({work}/stderr.txt)")
    if "message box" in log:
        fails.append(f"liga3: a message box ({work}/stderr.txt)")
    return fails


def check_por_cup(program, game, work):
    code, _ = rrc.run(program, game, 2021, 2021, os.path.join(DATABASES, "October 2021"), ["all"] + OTHERS,
                      ["portugal"], work, 7200, options_more={"changing_rules": "off"},
                      env_more={"RECOMP_BOX_FIXES": "off"}, start_only=True)
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    fails = [] if code == 0 else [f"por-cup: code {code} ({work})"]
    if "message box" in log:
        fails.append(f"por-cup: a message box ({work}/stderr.txt)")
    moved = re.search(r"club moves: (\d+) clubs that leave a group division of a new form go to its lower division", log)
    print("  por-cup: " + (moved.group(0) if moved else "no clubs to the lower division"))
    if not moved:
        fails.append(f"por-cup: the club moves sent no club of a Liga 3 group to the Third Division ({work}/stderr.txt)")
    if "with no division (safety net)" in log:
        fails.append(f"por-cup: the Portuguese Cup took a club with no division ({work}/stderr.txt)")
    return fails


def check_subset_extra(program, game, work):
    code, _ = rrc.run(program, game, 2001, 2001, None, ["England", "Italy"], ["spain", "portugal"], work, 3600,
                      start_only=True, stop_on_box=True)
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    fails = [] if code == 0 else [f"subset-extra: code {code} ({work})"]
    box = re.search(r"message box from .*", log)
    if box:
        fails.append(f"subset-extra: {box.group(0)} ({work}/stderr.txt)")
    for n in ("spain", "portugal"):
        if not re.search(rf"test: extra division of {n} on \(flags 0 before, the nation is selected with it\)", log):
            fails.append(f"subset-extra: the switch did not select {n} with its extra division ({work}/stderr.txt)")
    print("  subset-extra: " + " | ".join(re.findall(r"test: extra division of .*", log)))
    return fails


def check_load(name, program, game, work):
    year, db, nations, extra, want = RUNS[name]
    # The jump goes on to the next season, so it does not end at the last report (1 June) before the save.
    code, _ = rrc.run(program, game, year + 1, year, os.path.join(DATABASES, db), nations, extra, work, 7200,
                      options_more={"changing_rules": "off"},
                      env_more={"RECOMP_SAVE_AFTER_DAYS": str(SAVE_DAYS), "RECOMP_DIVISION_LOG": "1"})
    first = os.path.join(work, "user", "samesave.sav")
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    if code != 0 or not os.path.exists(first):
        return [f"{name}: the first run made no save (code {code}, {work})"]
    if any(re.search(w, log) for w in want):
        return [f"{name}: the season changed before the save ({work}/stderr.txt)"]
    again = os.path.join(work, "again")
    os.makedirs(again, exist_ok=True)
    shutil.copyfile(first, os.path.join(again, "divisions.sav"))
    os.environ["RECOMP_DIVISION_LOG"] = "1"                 # fixtures.run takes the environment of this process
    try:
        r, saved = cwc.load_and_play(program, game, os.path.join(again, "divisions.sav"), again, PLAY_DAYS)
    finally:
        os.environ.pop("RECOMP_DIVISION_LOG", None)
    fails = []
    print(f"  {name}: " + " | ".join(re.findall(r"new divisions: .*|new competition .* from the save.*", r.stderr)))
    if r.returncode != 0 or not saved:
        fails.append(f"{name}: the save did not load and play on (code {r.returncode}, {again})")
    for w in want:
        if not re.search(w, r.stderr):
            fails.append(f"{name}: no line /{w}/ after the load ({again}/stderr.txt)")
    if name == "greece-load" and not re.search(r"new competition gre_second made for \d+ .*from the save", r.stderr):
        fails.append(f"{name}: the module did not make the group again from the save ({again}/stderr.txt)")
    if "message box" in r.stderr or "not a heap block" in r.stderr:
        fails.append(f"{name}: a message box or a fatal error after the load ({again}/stderr.txt)")
    return fails


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    only = rrc.arg(argv, "--only")
    keep = os.path.abspath(rrc.arg(argv, "--keep")) if "--keep" in argv else tempfile.mkdtemp(prefix="cm0102-div-")
    names = only.split(",") if only else ["liga3", "por-cup", "subset-extra"] + list(RUNS)
    failed = []
    for name in names:
        work = os.path.join(keep, name)
        check = {"liga3": check_liga3, "por-cup": check_por_cup, "subset-extra": check_subset_extra}.get(name)
        failed += check(program, game, work) if check else check_load(name, program, game, work)
    for text in failed:
        print("FAIL  " + text)
    print("PASS" if not failed else f"FAILED: {len(failed)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
