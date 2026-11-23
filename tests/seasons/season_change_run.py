#!/usr/bin/env python3
"""Season change check (0.6.0 slice 16, Q232, Q258): the July step and its news item.

Usage:
    python3 tests/seasons/season_change_run.py <cm0102_recomp> <game folder> [--only <name>,...] [--jobs n]
        [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py over one season change (start in X, last
season X+1), with changing_rules on and the test log RECOMP_SEASON_CHANGE_LOG=1. The names are those of the
names table of slice 15 (Seasons/names.tsv, tools/seasons/make_names.py). The runs (the era boundaries of the
build brief):
    2002    3.9.68 data from 2002/03, across 2002/03 to 2003/04
    2008    September 2008, across 2008/09 to 2009/10 (the UEFA Cup becomes the Europa League)
    2020    November 2020, across 2020/21 to 2021/22
    2023    October 2023, across 2023/24 to 2024/25
    off     September 2008 with changing_rules off: no change and no news
    load    September 2008: the game saves 352 days after the start (mid July 2009, after the change of 20 June),
            then a new program loads that save, returns from holiday, goes through the unread items of the
            Competitions filter with Next Unread (the runtime logs the text that the season item's write_text
            gives the screen, and the item's date), plays on with Space (each news item, then Continue Game)
            and saves again 8 days after the load (RECOMP_SAVE_AFTER_DAYS). The save of a game with a
            changed name must load: the game finds its key competitions by name (0x60efd0), and without the names in the
            data the load stops at the box "index.cpp 5809". Both saves must hold the news text and the
            changed name in their season part (bosman_season.dat, version 2)
    december September 2010 ODB with Japan and Norway, across 2010/11 to 2011/12, with the test kind
            RECOMP_SEASON_CHANGE_TEST=1 (a sentence for each competition that starts from 1 October). The
            calendar-year leagues change on 27 November and 27 December 2010 for season 2011: their sentences
            must wait, the season hook of 2011 and the news item of 2011/12 come on the season point in June
            2011, and no item is sent in 2010

The checks (each run):
    - the jump ends with code 0 and the report holds the end of X and of X+1 (a stopped run fails)
    - on: the competitions whose names table name of X+1 (the row of the object's number) differs from their
      name at the end of X have the new name at the end of X+1 in the rules report (1 June: the international entries change as late as 15 and
      28 August, after the start report of 1 August), and every other competition keeps its name
    - on: the news log lists exactly one sentence "<old name> is now called <new name>." for each of them,
      for season X+1, and the news item of X+1 is sent once, in parts 1, 2, ... when its text is long (no news
      item when no name changes)
    - off: the report names at the end of X+1 are the names at the end of X, and the log has no change after
      the start season (the hooks of the start season run with any changing rules, "(start season)" in the log)
A run takes about 6 to 8 minutes. --jobs (default 3) runs that many at the same time. --reuse checks the
reports already in the --keep folder again without a new run.
"""
import collections
import concurrent.futures
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
import rules_report_check as rrc  # noqa: E402

DB = "/Users/max/Projects/cm0102/test-fixtures/databases"
RUNS = {
    "2002": (2002, None, "on"),
    "2008": (2008, os.path.join(DB, "September 2008"), "on"),
    "2020": (2020, os.path.join(DB, "November 2020"), "on"),
    "2023": (2023, os.path.join(DB, "October 2023"), "on"),
    "off": (2008, os.path.join(DB, "September 2008"), "off"),
    "load": (2008, os.path.join(DB, "September 2008"), "on"),
    "december": (2010, os.path.join(DB, "September 2010 ODB"), "on"),
}
LOAD_NEWS = b"UEFA Cup is now called UEFA Europa League."
# The staff reserve of the custom data sets (staff_reserve.c, on main since 2026-10-17) moves the game's memory, so
# the games of these databases go another way. With 46,000 rows the September 2008 jump stops on 23 August 2008 at
# the original's box "news..cpp 7797" (NewsItem::get_slot 0x785830 with a slot outside 0 to 49), in the main build
# too, before any season change. With the 23,000 rows of the original the jump plays on. The runs of this check
# take 23,000 rows, so they test the season change and not that box (a finding for main, in the slice 16 report).
BASE_ENV = {"RECOMP_STAFF_RESERVE": "23000"}


def season_part(path):
    sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "save"))
    import savfile
    return savfile.Sav(path).read_member("bosman_season.dat")


LOAD_PLAY = [                               # the time of a step counts from the last wait
    "6500 click 45 165",                    # the manager's name: "Return from holiday ?" (the loaded game goes on
                                            # with its holiday, about 2.5 days a second, until the box is open)
    "wait screen:0x591b70 60000",           # the confirm box
    "1000 click 705 572",                   # Yes
    "wait news 60000",                      # the news screen of the day
    "2000 click 531 97",                    # the Competitions filter
] + [f"{3000 + 800 * k} click 717 230" for k in range(60)] + [   # Next Unread, item by item: the runtime logs
    "52000 dump {d}/item.bmp",                                    # the text of the season item when it shows
] + [f"{54000 + 400 * k} press space" for k in range(400)] + [   # Space: the next news item, then Continue
#                     Game (space_to_continue); the save comes 8 days after the load (check_load)
    # The original's news item "<player> seeks assurances" (a player of the save, on 29 June or 6 July 2009 as the
    # holiday runs) wants a reply and greys out Continue Game: Respond, "a backup for the first team", Set, then
    # Space again. Where no reply is due, the clicks hit an empty part, the item text and Next of the news screen.
    "216000 click 172 230",                 # Respond
    "219000 click 360 343",                 # This player is a backup for the first team
    "221000 click 705 572",                 # Set
] + [f"{224000 + 400 * k} press space" for k in range(200)]


def load_and_save(program, game, save, out, days, env):
    """Loads save (the first in the list), plays the LOAD_PLAY steps and saves days game days after the load.
    Returns the result and the path of the new save, or None."""
    sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))
    import fixtures
    import smoke_run
    user = os.path.join(out, "user")
    os.makedirs(user, exist_ok=True)
    os.replace(save, os.path.join(user, os.path.basename(save)))
    fixtures.write_settings(user, "classic", [])
    steps = fixtures.LOAD_STEPS[:-1] + LOAD_PLAY + ["900000 quit"]
    table = os.path.join(os.path.dirname(os.path.abspath(program)), "seasons", "rules.tsv")   # as rules_report_check
    r, _ = fixtures.run(program, game, "classic", user, smoke_run.scaled_steps("classic", out, steps),
                        dict(env, RECOMP_SAVE_AFTER_DAYS=str(days), RECOMP_RULES_TABLE=table), 1200)
    with open(os.path.join(out, "stderr.txt"), "w") as f:
        f.write(r.stderr)
    for f in sorted(os.listdir(out)):
        if f.endswith(".bmp"):
            subprocess.run(["sips", "-s", "format", "png", os.path.join(out, f), "--out",
                            os.path.join(out, f[:-4] + ".png")], capture_output=True)
    saved = os.path.join(user, smoke_run.SAVE_NAME)
    return r, saved if os.path.exists(saved) else None


def check_load(program, game, keep):
    """The load run (module docstring)."""
    import shutil
    work = os.path.join(keep, "load") if keep else tempfile.mkdtemp(prefix="cm0102-season-change-load-")
    year, database, _ = RUNS["load"]
    code, _ = rrc.run(program, game, year + 2, year, database, ["England"], [], work, 7200, {"changing_rules": "on"},
                      dict(BASE_ENV, RECOMP_SEASON_CHANGE_LOG="1", RECOMP_SAVE_AFTER_DAYS="352"))
    first = os.path.join(work, "user", "samesave.sav")
    if code != 0 or not os.path.exists(first):
        return [f"load: the first run made no save (code {code}, {work})"]
    fails = []
    part = season_part(first)
    if part[:4] != b"\x02\x00\x00\x00" or LOAD_NEWS not in part or b"UEFA Cup\x00" not in part:
        fails.append(f"load: the first save's season part lacks the news text or the name: {part!r}")
    again = os.path.join(work, "again")
    os.makedirs(again, exist_ok=True)
    shutil.copyfile(first, os.path.join(again, "change.sav"))
    # 8 days: the load is on day 179 of 2009, the item is read on day 186, and on the evening of day 187 the
    # original's own news item "seeks assurances" (a player of the save) wants a reply and greys out Continue Game,
    # so the Space steps cannot go past it. The save comes at the first human turn of day 187.
    r, saved = load_and_save(program, game, os.path.join(again, "change.sav"), again, 8,
                             dict(BASE_ENV, RECOMP_SEASON_CHANGE_LOG="1"))
    if r.returncode != 0 or not saved:
        fails.append(f"load: the save did not load and play on (code {r.returncode}, {again})")
    shown = shown_items(r.stderr)
    if not shown:
        fails.append(f"load: the news item was not shown ({again}/item.png)")
    for season, day, y, text in shown:
        if (season, y) != ("2009", "2009") or not 151 <= int(day) <= 273 or LOAD_NEWS.decode() not in text:
            fails.append(f"load: the item shows {season}, day {day} of {y}: {text!r}")
    if saved:
        part = season_part(saved)
        if LOAD_NEWS not in part or b"UEFA Cup\x00" not in part:
            fails.append(f"load: the second save's season part lacks the news text or the name: {part!r}")
    print(f"load: item shown {shown}, {len(fails)} problems ({work})")
    return fails


def check_december(program, game, keep):
    """The december run (module docstring)."""
    work = os.path.join(keep, "december") if keep else tempfile.mkdtemp(prefix="cm0102-season-change-december-")
    year, database, _ = RUNS["december"]
    code, _ = rrc.run(program, game, year + 1, year, database, ["England", "Japan", "Norway"], [], work, 10800,
                      {"changing_rules": "on"},
                      dict(BASE_ENV, RECOMP_SEASON_CHANGE_LOG="1", RECOMP_SEASON_CHANGE_TEST="1"))
    if code != 0:
        return [f"december: the run ended with code {code} ({work})"]
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read().split("\n")
    fails = []
    tests = [m.group(2) for m in (NEWS.search(x) for x in log) if m and m.group(2).startswith("Test: ")]
    late = [t for t in tests if "starts its 2011 season" in t and t.endswith("of 2010.")]
    if not late:
        fails.append("december: no competition started its 2011 season in November or December 2010")
    sent = [(int(m.group(1)), int(m.group(3)), int(m.group(4))) for m in (SENT.search(x) for x in log) if m
            and m.group(2) == "1"]
    points = [tuple(int(g) for g in m.groups()) for m in (POINT.search(x) for x in log) if m]
    hooks = [tuple(int(g) for g in m.groups()) for m in (HOOK.search(x) for x in log) if m]
    if [s for s in sent if s[2] == 2010]:
        fails.append(f"december: a news item was sent in 2010: {sent}")
    if not [s for s in sent if s[0] == 2011 and s[2] == 2011 and 151 <= s[1] <= 273]:
        fails.append(f"december: no item of 2011/12 at its season point in June to September 2011: {sent}")
    if [h for h in hooks if h[0] == 2011 and h[2] != 2011] or not [h for h in hooks if h[0] == 2011]:
        fails.append(f"december: the season hook of 2011 ran {hooks}, expected once in 2011")
    if [p for p in points if p[0] == 2011 and p[2] != 2011]:
        fails.append(f"december: season point of 2011 in another year: {points}")
    print(f"december: {len(late)} competitions start 2011 late in 2010, points {points}, items {sent}, "
          f"{len(fails)} problems ({work})")
    return fails
NEWS = re.compile(r"recomp: season change: news (\d{4})/\d\d: (.*)$")
SENT = re.compile(r"recomp: season change: news item for (\d{4})/\d\d part (\d+) sent on day (\d+) of (\d+)$")
SHOWN = re.compile(r"recomp: season change: news shown for (\d{4})/\d\d, dated day (\d+) of (\d+): (.*)$")
POINT = re.compile(r"recomp: season change: season point of (\d{4})/\d\d on day (\d+) of (\d+)(?: \(start season\))?$")
HOOK = re.compile(r"recomp: season change: test: season hook (\d{4}) on day (\d+) of (\d+)$")


def shown_items(log):
    """[(season, day, year, [sentences])] of the season items that the log shows. The runtime logs the first
    sentence on the line of SHOWN and each further sentence on a line of its own (the UEFA slices put their
    sentences before the names, so "UEFA Cup is now called ..." is the last of four in 2009/10)."""
    out, lines = [], log.split("\n")
    for i, x in enumerate(lines):
        m = SHOWN.search(x)
        if not m:
            continue
        text = [m.group(4)]
        for y in lines[i + 1:]:
            if not y or y.startswith("recomp:"):
                break
            text.append(y)
        out.append((m.group(1), m.group(2), m.group(3), text))
    return out


def season_names():
    """{(season, file): {n: long name}} of the names table of slice 15 (tools/seasons/make_names.py)."""
    sys.path.insert(0, os.path.join(HERE, "..", "..", "tools", "seasons"))
    import make_names
    import make_rules
    return make_names.names_by_key(make_rules.read_rows())


def name_of(names, season, file, n):
    """The name that the game gives the n-th object of a file (season_names.c season_names_get_nth): the row n,
    no name for a gap, the last row for an object after it."""
    rows = names.get((season, file), {})
    if n in rows:
        return rows[n]
    return rows[max(rows)] if rows and n > max(rows) else None


def check_run(name, program, game, keep, reuse=False):
    year, database, rules = RUNS[name]
    work = os.path.join(keep, name) if keep else tempfile.mkdtemp(prefix=f"cm0102-season-change-{name}-")
    report = os.path.join(work, "report.tsv")
    if reuse and os.path.exists(report):
        code = 0
    else:
        code, report = rrc.run(program, game, year + 1, year, database, ["England"], [], work, 7200,
                               {"changing_rules": rules},
                               dict(BASE_ENV, RECOMP_SEASON_CHANGE_LOG="1"))
    fails = []
    if code != 0:
        return [f"{name}: the run ended with code {code} ({work})"]
    seasons, comps = rrc.read_report(report)
    ends = {x["season"] for x in seasons if x["phase"] == "end"}
    if year not in ends or year + 1 not in ends:
        return [f"{name}: the report has no end of {year} and {year + 1} (ends {sorted(ends)}): the run stopped ({work})"]
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read().split("\n")
    news = [(int(m.group(1)), m.group(2)) for m in (NEWS.search(x) for x in log) if m]
    parts = [(int(m.group(1)), int(m.group(2))) for m in (SENT.search(x) for x in log) if m]
    sent_at = [(int(m.group(1)), int(m.group(3)), int(m.group(4))) for m in (SENT.search(x) for x in log) if m
               and m.group(2) == "1"]
    sent = [x[0] for x in sent_at]
    for season in set(sent):
        got_parts = [p for s, p in parts if s == season]
        if got_parts != list(range(1, len(got_parts) + 1)):
            fails.append(f"{name}: the parts of the item of {season} were sent as {got_parts}")
    for season, day, y in sent_at:
        if y != season or not 151 <= day <= 273:
            fails.append(f"{name}: the item of {season} was sent on day {day} of {y}, not at its season point")
    end = [c for c in comps if c["season"] == year and c["phase"] == "end"]
    start = {c["slot"]: c for c in comps if c["season"] == year + 1 and c["phase"] == "end"}
    names = season_names()
    expected = []
    count = collections.Counter()
    for c in end:                       # the report order is that of season_comp_objects (season_names.c)
        k = count[c["file"]]
        count[c["file"]] += 1
        n = start.get(c["slot"])
        if not n:
            continue
        after = name_of(names, year + 1, c["file"], k)
        changes = rules == "on" and after and c["name"] != after[:50]
        want = after[:50] if changes else c["name"]
        if n["name"] != want:
            fails.append(f"{name}: {c['file']} ends {year + 1} as {n['name']!r}, expected {want!r}")
        if changes:
            expected.append(f"{c['name']} is now called {after[:50]}.")
    # The names kind gives "<old> is now called <new>." Other kinds (slices 17, 21a, 22) add their own sentences
    # to the same item; this check reads the names only.
    others = [t for s, t in news if s == year + 1 and " is now called " not in t]
    got = [t for s, t in news if s == year + 1 and " is now called " in t]
    if collections.Counter(got) != collections.Counter(expected):
        for t in sorted(set(expected) - set(got)):
            fails.append(f"{name}: the news lacks: {t}")
        for t in sorted(set(got) - set(expected)):
            fails.append(f"{name}: the news has an extra line: {t}")
    if [s for s, _ in news if s != year + 1]:
        fails.append(f"{name}: news for another season: {[s for s, _ in news if s != year + 1]}")
    want_sent = [year + 1] if expected or others else []
    if sent != want_sent:
        fails.append(f"{name}: news items sent {sent}, expected {want_sent}")
    if rules == "off" and any("season change" in x and "(start season)" not in x for x in log):
        fails.append(f"{name}: changing rules off, but the log shows a season change")
    print(f"{name}: {len(expected)} changes of name, news items {sent}, {len(fails)} problems ({work})")
    for t in got:
        print(f"    news {year + 1}: {t}")
    return fails


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", 3))
    keep = os.path.abspath(rrc.arg(argv, "--keep")) if "--keep" in argv else None
    fails = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        special = {"load": check_load, "december": check_december}
        for f in [pool.submit(special[n], program, game, keep) if n in special else
                  pool.submit(check_run, n, program, game, keep, "--reuse" in argv) for n in only]:
            fails += f.result()
    for t in fails:
        print("FAIL  " + t)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
