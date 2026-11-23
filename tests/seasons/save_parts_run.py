#!/usr/bin/env python3
"""The parts of a save merged across seasons (0.6.2 slice 13, fault 29, Q26; src/recomp_rt/win/save_parts.c).

Usage:
    python3 tests/seasons/save_parts_run.py fill <cm0102_recomp> <game folder> --keep <folder> [--parts 990,999]
        [--save <in.sav>] [--rules on|off] [--keep-parts]
    python3 tests/seasons/save_parts_run.py old <cm0102_recomp> <game folder> --keep <folder> --save <in.sav>
        [--holiday]

fill: for each count of --parts, the script adds temporary files to the save (default start.sav of the test fixtures) until
it holds that many parts: "slice13 filler <n>.tmp", 100 bytes each, all of one type. It loads that save, goes on
holiday, saves 3 game days later, then loads the new save and saves again 3 days later (tests/fixtures/fixtures.py
play). The checks, for each of the two saves:
    - the program ends with code 0 and writes the save (the original stops at 999 parts: "save_game_data(): unable
      to pack the temporary files", 0x82fea5)
    - the save has a merged part for each type of two temporary files or more, and every filler file with its bytes
      (tools/save/savfile.py reads the files of a merged part)
    - every temporary file of the loaded save is still there, and no message box showed
    - the second load wrote the files of the merged parts back ("save parts: ... gave" in the log)
--keep-parts runs with RECOMP_SAVE_PARTS=keep (the original's parts) and leaves out the check of the merged parts.
With --save allleagues.sav of the test fixtures, --rules on and --parts 990 the game writes about 130 new temporary
files (the competitions of changing rules), so with the original's parts the second save fails with "save_game_data():
unable to pack the temporary files": this is the proof that the test finds the fault. With start.sav no new file
comes, so the original's parts pass too.

old: loads a save of an older build (0.6.0, 0.6.1, or main before slice 13: season part versions 1 to 4), saves 3
game days later, loads that save and saves again 3 days later. --holiday for a save made on holiday (a soak leg, which
goes on by itself: tests/seasons/soak_run.py load_leg). The checks, for each of the two saves:
    - the program ends with code 0, shows no message box and writes the save
    - every temporary file of the old save is in the new save (in a part of its own or in a merged part)
    - the new save has a merged part for each type that has two files or more, and its season part when the old
      save has one
"""
import os
import re
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "save"))
import savfile  # noqa: E402
import soak_run  # noqa: E402

sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
import fixtures  # noqa: E402

FIXTURES = "/Users/max/Projects/cm0102/test-fixtures"
HOLIDAY_STEPS = ["2500 click 45 165", "4500 click 150 427 | 199 474", "7000 click 705 572"]


def filler(n):
    return f"slice13 filler {n:04d}.tmp", (f"filler {n:04d} " * 8).encode()[:100]


def make_save(source, parts, path):
    sav = savfile.Sav(source)
    real = len(sav.real_members())
    added = {}
    for n in range(parts - real):
        name, data = filler(n)
        sav.add_member(name, data)
        added[name] = data
    if os.path.exists(path):
        os.remove(path)
    sav.save(path)
    return added


def expected_merged(sav):
    """The merged parts that a save of these files must have: one for each type of two temporary files or more."""
    types = {}
    for n in tmp_names(sav):
        types.setdefault(re.sub(r"\d+", "#", n), []).append(n)
    return sorted(t + savfile.MERGED for t, files in types.items() if len(files) >= 2)


def check(save, log, added, step, before):
    if not os.path.exists(save):
        return [f"{step}: no save ({soak_run.first_box(log) or last_line(log)})"]
    fails = []
    sav = savfile.Sav(save)
    real = sav.real_members()
    merged = sorted(m.name for m in real if m.name.lower().endswith(savfile.MERGED))
    files = {m.name: m for m in sav.members}
    print(f"{step}: {len(real)} parts, {len(merged)} merged: {', '.join(merged)}")
    if os.environ.get("RECOMP_SAVE_PARTS") != "keep" and merged != expected_merged(sav):
        fails.append(f"{step}: merged parts {merged}, expected {expected_merged(sav)}")
    lost = [n for n, data in added.items() if n not in files or sav.read_member(n) != data]
    if lost:
        fails.append(f"{step}: {len(lost)} filler files are missing or changed, for example {lost[0]}")
    gone = sorted(tmp_names(before) - tmp_names(sav))
    if gone:
        fails.append(f"{step}: {len(gone)} temporary files of the loaded save are missing: {', '.join(gone[:5])}")
    box = soak_run.first_box(log)
    if box:
        fails.append(f"{step}: {box}")
    return fails


def last_line(log):
    if not os.path.exists(log):
        return "none"
    lines = [x for x in open(log, encoding="latin-1").read().split("\n") if x.strip()]
    pack = [x for x in lines if "unable to pack" in x]
    return pack[0] if pack else lines[-1] if lines else "empty"


RULES = []      # the options of the fill runs: [("changing_rules", "on")] with --rules on


def run(program, game, work, name, out):
    steps = os.path.join(work, "steps.txt")
    with open(steps, "w") as f:
        f.write("\n".join(HOLIDAY_STEPS) + "\n")
    code = fixtures.play(program, game, work, name, steps, out, "classic", RULES, 3)
    return code, os.path.join(out, "samesave.sav"), os.path.join(out, "stderr.txt")


def tmp_names(sav):
    return {m.name for m in sav.members if m.name.lower().endswith(".tmp")}


def old_check(save, before, step):
    if not os.path.exists(save):
        return [f"{step}: no save"]
    sav = savfile.Sav(save)
    real = {m.name for m in sav.real_members()}
    merged = sorted(n for n in real if n.lower().endswith(savfile.MERGED))
    names = tmp_names(sav)
    print(f"{step}: {len(real)} parts, {len(names)} temporary files, merged: {', '.join(merged)}")
    fails = []
    lost = sorted(tmp_names(before) - names)
    if lost:
        fails.append(f"{step}: {len(lost)} temporary files of the old save are missing: {', '.join(lost[:5])}")
    want = expected_merged(sav)
    if want != merged:
        fails.append(f"{step}: merged parts {merged}, expected {want}")
    if "bosman_season.dat" in {m.name for m in before.real_members()} and "bosman_season.dat" not in real:
        fails.append(f"{step}: no season part")
    return fails


def old(argv):
    program, game = os.path.abspath(argv[0]), os.path.abspath(argv[1])
    keep = os.path.abspath(soak_run.arg(argv, "--keep"))
    source = os.path.abspath(soak_run.arg(argv, "--save"))
    os.environ["RECOMP_SAVE_PARTS_LOG"] = "1"
    fails, save = [], source
    before = savfile.Sav(source)
    print(f"the old save: {len(before.real_members())} parts, {len(tmp_names(before))} temporary files")
    for step in ("first", "second"):
        work = os.path.join(keep, step)
        os.makedirs(work, exist_ok=True)
        if "--holiday" in argv or step == "second":      # the first save was made on holiday
            o = {"program": program, "game": game, "rules": "on", "database": None, "last": 2099, "seed": 1,
                 "timeout": 1800}
            code, took, box = soak_run.load_leg(o, work, save, 3)
            result, log = os.path.join(work, "user", "samesave.sav"), os.path.join(work, "stderr.txt")
        else:
            shutil.copyfile(save, os.path.join(work, "in.sav"))
            code, result, log = run(program, game, work, "in.sav", os.path.join(work, "out"))
            box = None
        box = box or soak_run.first_box(log)
        found = old_check(result, before, f"{step} save (code {code})")
        if code != 0 or box:
            found.append(f"{step}: code {code}, box {box}")
        fails += found
        if found:
            break
        save = os.path.join(keep, f"{step}.sav")
        shutil.copyfile(result, save)
    return fails


def main(argv):
    if len(argv) < 5 or argv[1] not in ("fill", "old") or "--keep" not in argv \
            or (argv[1] == "old" and "--save" not in argv):
        print(__doc__)
        return 2
    if argv[1] == "old":
        fails = old(argv[2:])
        for f in fails:
            print("FAIL", f)
        print("PASS" if not fails else f"FAIL: {len(fails)}")
        return 1 if fails else 0
    argv = argv[1:]
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = os.path.abspath(soak_run.arg(argv, "--keep"))
    source = soak_run.arg(argv, "--save", os.path.join(FIXTURES, "start.sav"))
    os.environ["RECOMP_SAVE_PARTS_LOG"] = "1"
    if "--keep-parts" in argv:
        os.environ["RECOMP_SAVE_PARTS"] = "keep"
    RULES[:] = [("changing_rules", soak_run.arg(argv, "--rules", "off"))]
    fails = []
    for parts in (int(x) for x in soak_run.arg(argv, "--parts", "990,999").split(",")):
        work = os.path.join(keep, f"parts-{parts}")
        os.makedirs(work, exist_ok=True)
        added = make_save(source, parts, os.path.join(work, "start.sav"))
        before = savfile.Sav(os.path.join(work, "start.sav"))
        print(f"{parts} parts: {len(added)} filler files added")
        code, first, log = run(program, game, work, "start.sav", os.path.join(work, "first"))
        found = check(first, log, added, f"{parts} parts, first save (code {code})", before)
        if code != 0:
            found.append(f"{parts} parts: the first run failed (code {code})")
        fails += found
        if found:
            continue
        again = os.path.join(work, "again")
        os.makedirs(again, exist_ok=True)
        os.replace(first, os.path.join(again, "first.sav"))
        code, second, log = run(program, game, again, "first.sav", os.path.join(again, "out"))
        found = check(second, log, added, f"{parts} parts, second save (code {code})", before)
        if code != 0:
            found.append(f"{parts} parts: the second run failed (code {code})")
        gave = [x for x in open(log, encoding="latin-1").read().split("\n") if "save parts:" in x and "gave" in x]
        print(f"{parts} parts, second load: {gave[0] if gave else 'no merged part written back'}")
        if not gave:
            found.append(f"{parts} parts: the second load wrote no file of a merged part back")
        fails += found
    for f in fails:
        print("FAIL", f)
    print("PASS" if not fails else f"FAIL: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
