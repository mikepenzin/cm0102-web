#!/usr/bin/env python3
"""Game check of the club records history trim (open item 192, src/recomp_rt/win/history_trim.c).

Usage:
    python3 tests/seasons/history_trim_run.py <cm0102_recomp> <game folder> --save <save> --keep <work folder>
        [--seasons 50] [--legs 2] [--tier-a] [--database <folder>] [--changing-rules on|off] [--timeout 14400]

It loads the save (made in July, as the run set's saves are), plays to July of the next year with the season jump
(tests/seasons/soak_run.py load_leg, RECOMP_HISTORY_SEASONS=--seasons), and compares the club records history
lists of the two saves (tools/save/club_history.py). Each later leg (--legs) loads the save of the leg before, so the
second leg writes into the nodes that the first one put after the tail. Checks, for each leg:
- the leg ends with no box, code 0 and a save;
- no list is broken (each reaches its tail, as FILE_LLIST::get_nodes 0x58f120 reads it for the Club History screen);
- a list that gained a season holds min(n + 1, keep) seasons, n its length before, and is the last seasons of the
  old list in order with the new season at the end;
- a list with no new season is as it was;
- the history file grows by one node (0x391 bytes) for each list that gained a season with fewer than keep seasons
  before, and by nothing else: a full list writes its new season into a node it dropped;
- the Club History tabs' list of every club builds after the save (RECOMP_TEST_CLUB_HISTORY=1, history_trim.c: the
  game's own Club_Records::build_history_list 0x450d50), none fails, each is in season order, and the longest holds
  as many seasons as the longest list of the save;
- the fixtures: the save after the leg holds no fixtures_<year>.tmp of a year keep or more before its fixtures year
  (the year after the season it played; the special fixtures_2999.tmp is left out), and every newer year that the loaded save held (history_trim.c part 2). The leg plays every
  match day of the season after the trim with no box.
Prints PASS or FAIL lines.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools", "save"))
sys.path.insert(0, os.path.join(ROOT, "tests", "smoke"))
import club_history  # noqa: E402
import re  # noqa: E402
from savfile import Sav  # noqa: E402
import soak_run  # noqa: E402

NODE = club_history.NODE_DATA + 4


def arg(argv, name, default=None):
    return argv[argv.index(name) + 1] if name in argv else default


def compare(before, after, keep, grow):
    """The FAIL lines of the lists before and after one season change, and a summary line."""
    fails, trimmed, gained, expect = [], 0, 0, 0
    size0, lists0 = before
    size1, lists1 = after
    if len(lists0) != len(lists1):
        return [f"club count {len(lists0)} before and {len(lists1)} after"], ""
    for i, (a, b) in enumerate(zip(lists0, lists1)):
        if a is None or b is None:
            fails.append(f"club {i}: broken list ({'before' if a is None else 'after'})")
            continue
        if b == a:
            continue
        if not b or (a and b[-1] <= a[-1]):
            fails.append(f"club {i}: no new season at the end ({a[-3:]} -> {b[-3:]})")
            continue
        gained += 1
        want = (a + [b[-1]])[-keep:] if keep else a + [b[-1]]
        if b != want:
            fails.append(f"club {i}: {len(a)} seasons {a[:2]}..{a[-2:]} became {len(b)} {b[:2]}..{b[-2:]}, "
                         f"want {len(want)} {want[:2]}..{want[-2:]}")
        if keep and len(a) >= keep:
            trimmed += 1
        else:
            expect += NODE
    if grow and size1 - size0 != expect:
        fails.append(f"history file grew {size1 - size0} bytes, want {expect} ({expect // NODE} new nodes)")
    longest = max((len(b) for b in lists1 if b), default=0)
    if keep and any(b and len(b) > keep for a, b in zip(lists0, lists1) if b != a):
        fails.append("a list that gained a season holds more than keep seasons")
    return fails, (f"{gained} lists gained a season, {trimmed} of them full ({keep} kept), history {size0} -> "
                   f"{size1} bytes, longest list {longest}")


def fixture_years(path):
    """The years of the fixtures_<year>.tmp files of a save."""
    out = set()
    for m in Sav(path).members:
        g = re.fullmatch(r"fixtures_(\d+)\.tmp", m.name)
        if g:
            out.add(int(g.group(1)))
    return out


def fixtures(before, after, keep, newest):
    """The FAIL lines of the fixtures years of the loaded save and the save after the leg (whose fixtures year is
    newest), and a summary line. A year after newest (the game keeps a fixtures_2999.tmp) is left out."""
    before = {y for y in before if y <= newest}
    after = {y for y in after if y <= newest}
    if not after:
        return ["the save after the leg holds no fixtures file"], ""
    out = [f"fixtures_{y}.tmp kept, {newest - y} years before {newest}" for y in sorted(after) if keep and y < newest - keep]
    out += [f"fixtures_{y}.tmp lost" for y in sorted(before) if y not in after and (not keep or y >= newest - keep)]
    return out, (f"fixtures years {min(before)}..{max(before)} ({len(before)}) -> {min(after)}..{newest} "
                 f"({len(after)})")


def screen(stderr, longest):
    """The FAIL lines of the Club History list test line in stderr."""
    lines = [l for l in open(stderr, errors="replace") if "recomp: club history lists:" in l]
    if not lines:
        return ["no club history list line (RECOMP_TEST_CLUB_HISTORY)"]
    print(lines[-1].strip())
    words = lines[-1].replace(",", " ").split()
    value = {w: int(words[i + 1]) for i, w in enumerate(words[:-1]) if words[i + 1].isdigit()}
    out = []
    if value.get("failed", 1):
        out.append(f"{value.get('failed')} club history lists failed to build")
    if value.get("order", 1):
        out.append(f"{value.get('order')} club history lists whose seasons do not run one way")
    if value.get("longest") != longest:
        out.append(f"the longest club history list holds {value.get('longest')} seasons, the save's longest {longest}")
    return out


def main(argv):
    if len(argv) < 3 or "--save" not in argv or "--keep" not in argv:
        print(__doc__)
        return 2
    keep = int(arg(argv, "--seasons", 50))
    work = os.path.abspath(arg(argv, "--keep"))
    save = os.path.abspath(arg(argv, "--save"))
    legs = int(arg(argv, "--legs", 2))
    if os.path.exists(os.path.join(work, "leg1")):
        print(f"history_trim_run.py: {work}/leg1 exists; use a new --keep folder")
        return 2
    os.makedirs(work, exist_ok=True)
    soak_run.LEG_ENV["RECOMP_HISTORY_SEASONS"] = str(keep)
    soak_run.LEG_ENV["RECOMP_TEST_CLUB_HISTORY"] = "1"
    if "--tier-a" in argv:
        soak_run.LEG_ENV["RECOMP_TIER_A"] = "1"
    fails = []
    for leg in range(1, legs + 1):
        before = club_history.lists(save)
        year = max((y[-1] for y in before[1] if y), default=0) + 1     # the save is of July after the last season
        o = {"program": os.path.abspath(argv[1]), "game": os.path.abspath(argv[2]),
             "rules": arg(argv, "--changing-rules", "on"),
             "database": os.path.abspath(arg(argv, "--database")) if "--database" in argv else None,
             "timeout": int(arg(argv, "--timeout", 14400)), "last": year + 1, "seed": 1}
        soak_run.wait_for_room()
        folder = os.path.join(work, f"leg{leg}")
        code, took, box = soak_run.load_leg(o, folder, save, 365)
        saved = os.path.join(folder, "user", soak_run.smoke_run.SAVE_NAME)
        if box:
            fails.append(f"leg {leg}: box: {box}")
        if code != 0:
            fails.append(f"leg {leg}: exit code {code}")
        if not os.path.exists(saved):
            fails.append(f"leg {leg}: no save")
            break
        after = club_history.lists(saved)
        more, line = compare(before, after, keep, True)
        fails += [f"leg {leg}: {m}" for m in more]
        print(f"leg {leg} ({year}/{(year + 1) % 100:02d}) in {took:.0f} s: {line}")
        fails += [f"leg {leg}: {m}" for m in
                  screen(os.path.join(folder, "stderr.txt"), max((len(y) for y in after[1] if y), default=0))]
        more, line = fixtures(fixture_years(save), fixture_years(saved), keep, year + 1)
        fails += [f"leg {leg}: {m}" for m in more]
        print(f"leg {leg}: {line}")
        save = saved
    for f in fails[:40]:
        print(f"FAIL  {f}")
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
