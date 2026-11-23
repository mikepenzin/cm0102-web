#!/usr/bin/env python3
"""The League of Ireland moves to calendar years in 2003 (gap row 2 of the 0.7.2 league sizes; the rules and dates in
src/recomp_rt/win/season_calendar.h).

Usage:
    python3 tests/seasons/ire_calendar_run.py <cm0102_recomp> <game folder> --keep <folder> [--leg-days 450]
        [--start 2001] [--last 2004] [--database <folder>] [--saves <days,days,...>] [--timeout 14400] [--reuse]

--start 2002 or 2003 starts there (a 2003 start needs --database, the October 2003 folder); the checks follow the
start. --main-ireland makes the Republic of Ireland the main nation (England off; a test-only switch of the steps)
and checks that the game starts on 1 March of the start year, the start of Ireland's entry. --saves 572,15 plays legs that save after those game days (from 12 July 2001: 4 February 2003, in the
short season's play-off, then 20 February 2003, the change day), each loaded by the next leg.

A season game from 2001 with changing rules on, England and the Republic of Ireland, as the soak (soak_run.py: the
season jump, Tier A) to the end of 2004/05, in 4 legs of --leg-days game days: a save in October 2002 (the short
season), in January 2004 and in April 2005 (calendar seasons), each loaded by a new program. RECOMP_CALENDAR_LOG,
RECOMP_SEASON_CHANGE_LOG and RECOMP_SIZE_LOG write the lines that the checks read:
    changes   entry 20 changes on 20 June 2002 (day 170) and on 20 February (day 50) of 2003, 2004 and 2005, once
              each, and never on 20 June from 2003;
    short     the 2002-03 template of ire_prm holds 27 dates and that of ire_first 22, all before 1 February 2003;
    calendar  the 2003 and 2004 templates of ire_prm hold 36 dates of 10 clubs from March or April to November;
    played    each finished edition of ire_prm played its row's matches (27 in 2002, 36 in 2003);
    news      one news item a season;
    labels    the history rows of ire_prm and ire_first: 2002 (2001/02), 2002 (2002-03, shown "2002/03"), 2003 (2003),
              2004 (2004), and no second-edition label (RECOMP_HISTORY_LOG names the Irish competitions, so the
              history wrap logs each second-edition label);
    save      each load gives the entry fields of its save (edition, update day, two years, start, end, years);
    windows   Ireland keeps the game's window in 2003 and 2004 and takes the 2005 windows at the change of 2005
              (open item 258; RECOMP_SEASON_NATION_LOG);
    soak      no box, no season change net, every division moves on (soak_run.py).
--reuse checks the logs in --keep again. Exit code 1 on a failure.
"""
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import soak_run  # noqa: E402

WAIT_DIR = "/Users/max/Projects/cm0102/run-logs/samesave-wait"
HISTORY = ";".join(["Irish Premier Division", "Irish First Division", "Irish Senior Challenge Cup", "Irish League Cup"])


def wait_for_room():
    """The lead's rule: a test game starts while the 1-minute load is under 16 when a same-save check waits, else
    under 20, and while fewer than 15 games run."""
    while True:
        waiting = os.path.isdir(WAIT_DIR) and any(os.scandir(WAIT_DIR))
        games = subprocess.run(["pgrep", "-x", "cm0102_recomp"], capture_output=True, text=True).stdout.split()
        if os.getloadavg()[0] < (16 if waiting else 20) and len(games) < 15:
            return
        time.sleep(30)


def logs(keep):
    out = []
    for n in sorted(os.listdir(keep)):
        p = os.path.join(keep, n, "stderr.txt")
        if n.startswith("leg") and os.path.exists(p):
            with open(p, encoding="latin-1") as f:
                out.append((n, f.read()))
    return out


def faults(keep, start=2001, last=2004):
    legs = logs(keep)
    text = "\n".join(t for _n, t in legs)
    out = []
    changes = [(int(d), int(y)) for d, y in
               re.findall(r"recomp: season change: entry 20 starts \d+/\d+ on day (\d+) of (\d+)", text)]
    want = ([(170, 2002)] if start == 2001 else []) + [(50, y) for y in range(max(start, 2002) + 1, last + 2)]
    for w in want:
        if changes.count(w) != 1:
            out.append(f"changes: entry 20 on day {w[0]} of {w[1]}: {changes.count(w)} changes, want 1")
    late = [c for c in changes if c[1] >= 2003 and c[0] > 60]
    if late:
        out.append(f"changes: entry 20 changes after February from 2003: {late}")
    tmpl = {}
    for f, base, kind, n, dates in re.findall(r"recomp: league sizes: (ire_prm|ire_first) template (\d+): (class|new) "
                                              r"(\d+)((?: \d{4}-\d\d-\d\d\+?)*)", text):
        tmpl.setdefault((f, int(base), kind), (int(n), dates.split()))
    for f, n in ((("ire_prm", 27), ("ire_first", 22)) if start <= 2002 else ()):
        t = tmpl.get((f, 2002, "new"))
        if not t or t[0] != n or any(d.rstrip("+") >= "2003-02-01" for d in t[1]):
            out.append(f"short: {f} 2002: {t[0] if t else 'no'} dates, want {n} before 1 February 2003")
    for base in range(2003, last + 1):
        t = tmpl.get(("ire_prm", base, "new"))
        if not t or t[0] != 36 or not (f"{base}-03-01" <= t[1][0].rstrip("+") <= f"{base}-04-30") or \
                not (f"{base}-11-15" <= t[1][-1].rstrip("+") <= f"{base}-12-07"):
            out.append(f"calendar: ire_prm {base}: {t[0] if t else 'no'} dates {t[1][:1] + t[1][-1:] if t else ''}, "
                       "want 36 from March or April to late November")
    played = {int(e): (int(c), int(m)) for e, c, m in
              re.findall(r"recomp: league sizes: ire_prm played (\d+): (\d+) clubs, (\d+) matches", text)}
    for ed, want_m in ((2002, 27), (2003, 36), (2004, 36)):
        if ed < start or ed >= last + 1:
            continue
        if played.get(ed, (0, 0))[1] != want_m or played.get(ed, (0, 0))[0] != 10:
            out.append(f"played: ire_prm {ed}: {played.get(ed)}, want 10 clubs and {want_m} matches")
    items = re.findall(r"recomp: season change: news item for (\d+)/\d+ part 1 sent", text)
    for s in set(items):
        if items.count(s) != 1:
            out.append(f"news: {items.count(s)} items for {s}")
    rows = re.findall(r"recomp: calendar: history row of (ire_prm|ire_first) edition (\d+): label (\d+)", text)
    for f in ("ire_prm", "ire_first"):
        got = [(int(e), int(lab)) for ff, e, lab in rows if ff == f]
        for ed, lab in ((2001, 2002), (2002, 2002), (2003, 2003), (2004, 2004)):
            if (ed, lab) not in got and any(e == ed for e, _l in got):
                out.append(f"labels: {f} edition {ed}: labels {[l for e, l in got if e == ed]}, want {lab}")
        if not any(e == 2003 for e, _l in got) and start <= 2003:
            out.append(f"labels: no history row of {f} 2003")
    bumps = re.findall(r"recomp: season history: a second edition in (\d+) gets the label (\d+)", text)
    if bumps:
        out.append(f"labels: second-edition labels {bumps} (check that none is Irish)")
    # Open item 258: Ireland keeps the game's own window in 2003 and 2004 (not stated), and takes the real 2005
    # windows (7 January to 4 March, 1 to 30 July) at its change of 20 February 2005.
    if start <= 2004 <= last:
        ch = text.find("recomp: season change: entry 20 starts 2005/06 on day 50 of 2005")
        wins = [m.start() for m in re.finditer(r"recomp: season nation: Republic of Ireland windows \d+ -> 2: "
                                                r"7\.1-4\.3\(#0\) 1\.7-30\.7\(#1\)", text)]
        early = [m.group(0) for m in re.finditer(r"recomp: season nation: Republic of Ireland windows [^\n]*", text)
                 if ch < 0 or m.start() < ch]
        if ch < 0 or not [w for w in wins if w > ch]:
            out.append("windows: no 2005 Irish windows (7 January to 4 March, 1 to 30 July) after the change of 2005")
        if early:
            out.append(f"windows: Irish windows changed before 2005: {early[:2]}")
    entry = re.compile(r"recomp: calendar: entry at (save|load) on day \d+ of \d+: (.*)")
    for i in range(len(legs) - 1):
        saves = [m.group(2) for m in entry.finditer(legs[i][1]) if m.group(1) == "save"]
        loads = [m.group(2) for m in entry.finditer(legs[i + 1][1]) if m.group(1) == "load"]
        if not saves or not loads or saves[-1] != loads[0]:
            out.append(f"save: {legs[i][0]} saved {saves[-1:] or 'nothing'}, {legs[i + 1][0]} loaded "
                       f"{loads[:1] or 'nothing'}")
    return out


ORIGINAL_STEPS = []          # rules_report_check.league_steps before --main-ireland replaces it


def main_ireland_steps(nations, extra):
    """The new game steps of rules_report_check.league_steps with the Republic of Ireland as the main nation (the
    test-only switch --main-ireland): England's row (selected at first) is clicked off before Ireland's row is
    clicked on, so Ireland is the first selected nation [0xae2d08] (setup_handle_select_leagues_input 0x81dd54..0x81e487)
    whose league table entry gives the start date (setup_set_game_start_date 0x830520, 0x830537..0x830565)."""
    import rules_report_check as rrc
    steps = ORIGINAL_STEPS[0](["England", "Republic of Ireland"], [])
    points = [f"390 {188 + 22 * rrc.NATION_ROWS.index('England')}"] + [rrc.DOWN_ARROW] * rrc.SCROLL + \
        [f"390 {rrc.below_y('Republic of Ireland')}"]
    picks = [f"{2000 + 1500 * i} click {p}" for i, p in enumerate(points)]
    out, skipping, dropping = [], False, False
    for s in steps:
        if s.endswith(f"click {rrc.DOWN_ARROW}") and s.startswith("2000 "):
            out += picks
            skipping = True
            continue
        if skipping and (" click 390 " in s or s.endswith(f"click {rrc.DOWN_ARROW}")):
            continue
        if skipping and s.endswith("click 705 572"):
            out.append(f"{2000 + 1500 * len(picks) + 1000} click 705 572")
            skipping = False
            dropping = True
            continue
        # With England off the game shows no Select Start Season screen after Select League(s) (a run of 16
        # November 2026 waited for screen 0x81e8d0 in vain and was at 0x820f50, the game made): its wait and Next go.
        if dropping and (s == "wait screen:0x81e8d0" or s == "2500 click 705 572"):
            if s == "2500 click 705 572":
                dropping = False
            continue
        out.append(s)
    return out


def main_start_faults(keep, start):
    """--main-ireland: the start date is 1 March of the start year, from Ireland's entry (20)."""
    text = "\n".join(t for _n, t in logs(keep))
    m = re.search(r"recomp: calendar: game start date day (\d+) of (\d+), main nation entry (-?\d+)", text)
    want = (59 if start % 4 else 60, start, 20)
    got = tuple(int(x) for x in m.groups()) if m else None
    return [] if got == want else [f"start: {got}, want day {want[0]} of {start} from entry 20"]


def run_legs(o, keep, saves):
    """Legs with the given save days (--saves): leg 0 makes the game and saves after saves[0] game days, each next leg
    loads the save before and saves after its own days; the last leg plays to the end of --last."""
    os.makedirs(keep, exist_ok=True)
    save = None
    for leg in range(len(saves) + 1):
        work = os.path.join(keep, f"leg{leg:02d}")
        days = saves[leg] if leg < len(saves) else 0
        wait_for_room()
        if leg == 0:
            code, took, box = soak_run.new_game_leg(o, work, days)
        else:
            code, took, box = soak_run.load_leg(o, work, save, days)
        net = soak_run.first_net(os.path.join(work, "stderr.txt"))
        save = os.path.join(work, "user", soak_run.smoke_run.SAVE_NAME)
        print(f"leg {leg}: code {code} after {took:.0f} s{': ' + (box or net) if box or net else ''}", flush=True)
        if box or net or code != 0 or (days and not os.path.exists(save)):
            return 1
    return 0


def main(argv):
    if len(argv) < 3 or "--keep" not in argv:
        print(__doc__)
        return 2
    keep = os.path.abspath(soak_run.arg(argv, "--keep"))
    start, last = int(soak_run.arg(argv, "--start", "2001")), int(soak_run.arg(argv, "--last", "2004"))
    env = ["--env", "RECOMP_TIER_A=1", "--env", "RECOMP_CALENDAR_LOG=1", "--env", "RECOMP_SEASON_CHANGE_LOG=1",
           "--env", "RECOMP_SIZE_LOG=1", "--env", "RECOMP_HISTORY_LOG=" + HISTORY, "--env", "RECOMP_SEASON_NATION_LOG=1"]
    code = 0
    main_ireland = "--main-ireland" in argv
    if main_ireland:
        if start < 2003:
            print("ire_calendar_run.py: --main-ireland needs --start 2003 or later (a calendar start)")
            return 2
        import rules_report_check as rrc
        ORIGINAL_STEPS[:] = [rrc.league_steps]
        rrc.league_steps = main_ireland_steps
    if "--reuse" not in argv and ("--saves" in argv or main_ireland):
        for i in range(0, len(env), 2):
            name, value = env[i + 1].split("=", 1)
            soak_run.LEG_ENV[name] = value
        db = soak_run.arg(argv, "--database")
        o = {"program": os.path.abspath(argv[0]), "game": os.path.abspath(argv[1]), "start": start, "last": last,
             "seed": 1, "nations": ["England", "Republic of Ireland"], "database": os.path.abspath(db) if db else None,
             "rules": "on", "timeout": int(soak_run.arg(argv, "--timeout", "14400")), "extra": []}
        if main_ireland:
            o["nations"] = ["Republic of Ireland"]
        code = run_legs(o, keep, [int(x) for x in soak_run.arg(argv, "--saves", "").split(",") if x])
    elif "--reuse" not in argv:
        soak_run.wait_for_room = wait_for_room
        code = soak_run.main(["soak_run.py", argv[0], argv[1], "--keep", keep, "--start", str(start), "--last",
                              str(last), "--nations", "England,Republic of Ireland", "--changing-rules", "on",
                              "--leg-days", soak_run.arg(argv, "--leg-days", "450"),
                              "--timeout", soak_run.arg(argv, "--timeout", "14400")] +
                             (["--database", soak_run.arg(argv, "--database")] if "--database" in argv else []) + env)
    fails = faults(keep, start, last) + (main_start_faults(keep, start) if main_ireland else [])
    for f in fails:
        print("FAIL", f)
    ok = not fails and code == 0
    print(("PASS" if ok else "FAIL") + f" ire_calendar ({keep})")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
