#!/usr/bin/env python3
"""Release test 4 of the online play spec (Q29, slice 9): a four-player network game runs through a World Cup, a
Euros and every season change between them, with a drop and rejoin, a host quit and a version mismatch.

Usage:
    python3 tests/online/season_net_run.py <cm0102_recomp> [<game folder>] --keep <folder> [--start 2001]
        [--end 2004 [--end-days 62] | --seasons n | --days d --legs n] [--nations England|all]
        [--leg-days d] [--leg n] [--lobby-url <address>] [--via lan|relay] [--seed 1] [--stall-minutes 20]
        [--timeout 86400] [--slow k]

--leg-days d cuts the same days into legs of d days (more restores and rejoins, and shorter turns at the lock).
--leg n plays only leg n, in the --keep folder of the earlier legs, from the save of leg n-1 (n = the number of
play legs is the quit leg). Run each leg as its own turn at lead-notes/tools/net-lock.sh, so that other runs get
turns between them; the run's checks (6, 7 and the lobby's seasons) come with the last play leg.

It is the full play of tests/seasons/full_play_run.py (gate (b) of 0.6.0: the match engine on, the managers on
holiday, a save near each 1 July that a new program loads) with the host's game as a network game of four human
managers. The default, 2001 to about 1 September 2004 (rule O166, as gate_run.py's end year of 2001), crosses the
World Cup 2002, the Euro 2004 and the season changes of 2002, 2003 and 2004. --days d --legs n plays n legs of d
days, for a quick check of the clicks. The lobby is a local copy of server/ (`npx wrangler dev`, as
tests/smoke/network_run.py --lobby), or --lobby-url.

The legs (each in <keep>/legNN, with a user folder and a stderr file for each game):
    leg 0  The host makes the network game "00Season" (Start Network Game) with its manager Host One at Arsenal, no
           password. Three joiners join one at a time and each makes a manager with the password "pw" at the
           2nd, 3rd and 4th club of Select Team. Then:
             drop and rejoin  the script kills join3 at its Manager Status (its socket closes with no quit
                              packet, as when a line drops; its club goes on holiday, research note section 7). A new join3 joins, clicks its old manager's row
                              on Manager Status, types "pw" and takes the club back.
             version          a fifth game of another version (RECOMP_TEST_VERSION=0.0.1) clicks the host's
                              lobby row and gets the box that names the host's version (slice 4b).
           Each joiner clicks Finish, the host clicks Finish, the game starts, and each joiner quits: its club is
           on holiday (Away). The host goes on holiday. At the first human turn --days game days after its first
           (RECOMP_SAVE_AFTER_DAYS, 365 by default), the runtime saves samesave.sav and ends the program.
    leg n  The host restores that save (Restore Network Game, Select File, Ok; setup_assign_restored_network_managers
           0x8387e0 puts its manager, who has no password, on its channel) and takes its manager back on Manager
           Status. The three joiners join again; each clicks its manager's row, types "pw" and takes its club back
           (the password box 0x8230b0). All click Finish; then as leg 0 from "each joiner quits".
    quit   Only with slice 6 in the build ("The host left the game"). The host restores the last save, the
           joiners take their clubs back, all click Finish, and the host quits (a window close) while the joiners
           play. The host's game saves, and each joiner shows "The host left the game" and goes back to the start
           screen.

The joiners play only at the start of each leg: in the original a joiner who goes on holiday leaves the network
game ("Exit the network game as you go on holiday ?", 0x7623da), and a joiner who stays must click Continue Game
for each day. So the host crosses every match day, tournament and season change as a network game host with three
human managers Away, and the joiners connect again in July of each season.

Checks:
    1  each leg: the host ends with code 0 and leaves samesave.sav; no message box of the game in its log (the
       script stops the game at the first one); no stall (the date moves at least once in --stall-minutes)
    2  each leg: every joiner reaches Manager Status with its club (leg 0: makes its manager; leg n: takes its club
       back with its password; its dump is in its log and no wait timed out), and ends with code 0
    3  each leg: the lobby lists the game with the leg's season, and with 4 managers online once all joined
    4  leg 0: after join3's drop the lobby shows one manager online less, and after its rejoin 4 again
    5  leg 0: the fifth game's box names the host's version
    6  all legs: every division moves on from one season to the next (rules_report_check.league_stalls)
    7  the run reaches 1 September of --end (or the last leg of --days)
    8  quit (needs slice 6, "The host left the game" in the build; else it is skipped and the result is STAGE): each
       joiner shows "The host left the game" and then the start screen 0x81bab0; the host saved at its quit
The output ends with PASS (exit code 0: every check, with the relay, slice 6 and the full run to --end), STAGE
(exit code 3: every check that ran passes, but the joins went over the LAN, the quit leg waits for slice 6 or the
run was short) or FAIL (exit code 1). The run waits for room before each game (online_common.wait_for_room).
"""
import os
import re
import shutil
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import online_common as oc  # noqa: E402

nr = oc.nr
sys.path.insert(0, os.path.join(oc.REPO, "tests", "seasons"))
import rules_report_check as rrc  # noqa: E402
import smoke_run  # noqa: E402

BOX = re.compile(r"recomp: message box from (\S+): (.*)")
DATE = re.compile(r"date (\d+)/(\d+)")
STATUS = "0x81ff10"                 # Manager Status (setup_build_manager_status_screen)
START_SCREEN = "0x81bab0"           # the start screen (setup_build_setup_game_screen)
GAME_NAME = "00Season"           # one word (the type step reads one); 00 sorts it first in a list
OTHER_VERSION = "0.0.1"
JOINERS = 3
STATUS_ROW_Y, STATUS_ROW_H = 188, 23  # Manager Status rows: the first at y 188 (dropin_run.py: the second at 211)
TEAM_ROW_Y, TEAM_ROW_H = 163, 24      # Select Team rows, England only (smoke_run: Arsenal at 163, the 2nd at 187)
TEAM_ROW_Y_ALL = 188                  # Select Team with a Nation box (rules_report_check.league_steps)
# Go on Holiday. In a network game the first click on the manager's name opened no menu in a probe (2026-10-27,
# run-logs/s9-tests/probe/p3: the left bar had just changed from its countdown to Continue Game), and a click after
# Escape did. So: click, Escape (it closes a menu that did open), click, and wait for the menu.
HOLIDAY_PRE = [
    "2500 click 45 165",            # the manager menu (the manager's name in the left bar)
    "3500 press Escape",
    "4500 click 45 165",
]
HOLIDAY_POST = [
    "wait popup 10000",
    "1000 dump {d}/host-4a-menu.bmp",
    "1500 click 150 {holiday_y}",   # Go on Holiday
    "3500 dump {d}/host-4b-confirm.bmp",
    "4000 click 705 572",           # Please Confirm: Yes
    "40000 dump {d}/host-4-holiday.bmp",
]
NEW_NAME = [                        # Enter Name: names and the manager password
    "1000 click 560 246", "1500 type {first}", "3000 click 560 310", "3500 type {second}",
]
PASSWORD = ["5000 click 560 374", "5500 type pw", "7000 click 560 438", "7500 type pw"]
SECOND = ["Two", "Three", "Four"]


def arg(argv, name, default=None):
    return argv[argv.index(name) + 1] if name in argv else default


def at(t, steps):
    """The steps with t ms added to each step time up to the first wait (later times count from that wait)."""
    out, waited = [], False
    for s in steps:
        p = s.split(" ", 1)
        waited = waited or p[0] == "wait"
        out.append(s if waited else f"{int(p[0]) + t} {p[1]}")
    return out


def scaled(steps, slow):
    """dropin_run.scaled: every step time times slow; a wait without its own limit gets 120 s times slow."""
    out = []
    for s in steps:
        p = s.split()
        if p[0] == "wait":
            out.append(s if len(p) > 2 else f"{s} {int(120000 * slow)}")
        else:
            out.append(" ".join([str(int(int(p[0]) * slow))] + p[1:]))
    return out


# ---- the host's steps ----

def host_new(o):
    """Leg 0: the network game, the leagues, the host's manager, Manager Status."""
    many = o["nations"] != "England"
    leagues = ["2000 click 592 155", "4000 click 705 572", "wait screen:0x81e8d0",
               "2500 click 705 572"] if many else ["2000 click 705 572"]
    team_y = TEAM_ROW_Y_ALL if many else TEAM_ROW_Y
    return nr.START + [
        "3500 click 277 177",               # Start Network Game
        f"6500 type {GAME_NAME}",
        "8500 click 705 572",               # Next (ends the text entry)
        "10000 click 705 572",              # Next
        "wait leagues",
    ] + leagues + [                         # Select League(s) (and Select Start Season): the game is made
        f"wait screen:{nr.NAME_BUILD} 3600000",
    ] + [s.format(first="Host", second="One") for s in NEW_NAME] + [
        "5000 click 705 572",               # Enter Name: Next
        "wait screen:0x821900",
        "1000 click 705 572",               # Select Nationality: Next
        "wait screen:0x822260",
        f"1000 click 155 {team_y}",         # Select Team: Arsenal
        "wait screen:0x454900",
        "1000 click 722 14",                # Take Control
        "wait screen:0x822aa0",
        "1000 click 705 572",               # Please Confirm: Yes
        f"wait screen:{STATUS}",
        "2000 dump {d}/host-1-status.bmp",
    ]


def host_restore():
    """Leg n: Restore Network Game, the first save of the list, Manager Status, the host's manager back."""
    return nr.START + [
        "4000 click 277 242",               # Restore Network Game
        f"6500 type {GAME_NAME}",
        "8500 click 705 572",               # Next (ends the text entry)
        "10000 click 705 572",              # Next: Select File
        "13000 click 445 257",              # the first save (the folder holds only samesave.sav)
        "15000 click 705 573",              # Ok: the game loads
        f"wait screen:{STATUS} 3600000",
        "2000 dump {d}/host-0-restored.bmp",
        f"3000 click 150 {STATUS_ROW_Y}",   # the host's manager, no password: the resume confirm (0x823420)
        "6000 dump {d}/host-0-resume.bmp",
        "6500 click 705 572",               # Yes
        "9500 dump {d}/host-1-status.bmp",
    ]


# Go on Holiday in the manager's menu: with a club the menu starts with five club items, without one (the board
# sacked the host's manager, as in leg 8 of run season-final, 2004) it starts at News, and the item is higher.
HOLIDAY_Y_CLUB, HOLIDAY_Y_NO_CLUB = 427, 298


def host_after_status(finish_ms, holiday, quit_ms=None, holiday_y=HOLIDAY_Y_CLUB):
    """From the last dump on Manager Status: Finish at finish_ms, then the holiday (or the quit of the quit leg)."""
    steps = [
        f"{finish_ms - 1000} dump {{d}}/host-2-status.bmp",
        f"{finish_ms} click 705 572",       # Manager Status: Finish
        "wait news 600000",
        "3000 dump {d}/host-3-news.bmp",
    ]
    if quit_ms:
        return steps + [f"{quit_ms - 1000} dump {{d}}/host-4-playing.bmp", f"{quit_ms} quit"]
    return steps + at(holiday, HOLIDAY_PRE) + [x.replace("{holiday_y}", str(holiday_y)) for x in HOLIDAY_POST] + \
        ["86400000 quit"]


# ---- the joiners' steps ----

def join_start(o, n):
    steps = nr.START + [
        "3500 click 612 243",               # Join Network Game
        "wait screen:0x81f8d0",
        f"3000 dump {{d}}/join{n}-0-list.bmp",
    ]
    if o["via"] == "relay":                 # lobby rows only: the host's row is the first row
        return steps + [f"3500 {nr.FIRST_ROW}", "7500 click 705 572"]
    return steps + [f"3500 type {o['address']}", "6000 click 705 572", "7500 click 705 572"]


def join_new(o, n, team_row, finish_ms, quit_ms, drop=False, add=False):
    """A joiner that makes its manager: names, password, nationality, a club, Manager Status, Finish (or the drop
    at Manager Status), quit. add: the game already runs, so the join shows Manager Status, and Add Manager in the
    left bar opens Enter Name (match_day_run.py, 2026-10-27)."""
    many = o["nations"] != "England"
    team_y = (TEAM_ROW_Y_ALL if many else TEAM_ROW_Y) + TEAM_ROW_H * team_row
    t = 11000
    steps = join_start(o, n)
    if add:
        steps += [f"{t} dump {{d}}/join{n}-0-status.bmp", f"{t + 500} click 45 127"]   # Add Manager
        t += 4000
    steps += [f"{t} dump {{d}}/join{n}-1-name.bmp"]
    steps += at(t, [s.format(first="Player", second=SECOND[n - 1]) for s in NEW_NAME] + PASSWORD)
    t += 9000
    steps += [
        f"{t} click 705 572",               # Enter Name: Next
        f"{t + 4500} click 705 572",        # Select Nationality: Next
        f"{t + 9000} click 155 {team_y}",   # Select Team
        f"{t + 13500} click 722 14",        # Take Control
        f"{t + 18000} click 705 572",       # Please Confirm: Yes
        f"{t + 22000} dump {{d}}/join{n}-2-status.bmp",
    ]
    if drop:                                # the script kills the game here: a line drop, no quit
        return steps + [f"{t + 600000} quit"]
    return steps + [f"{max(finish_ms, t + 23000)} click 705 572",   # Manager Status: Finish
                    f"{quit_ms - 1000} dump {{d}}/join{n}-4-game.bmp", f"{quit_ms} quit"]


def join_back(o, n, row, finish_ms, quit_ms=None, host_quits=False):
    """A joiner that takes its club back: its manager's row on Manager Status, the password, Finish."""
    t = 11000
    steps = join_start(o, n) + [
        f"{t} dump {{d}}/join{n}-1-status.bmp",
        f"{t + 500} click 150 {STATUS_ROW_Y + STATUS_ROW_H * row}",   # the row of its manager
        f"{t + 4500} dump {{d}}/join{n}-2-password.bmp",
        f"{t + 5000} click 445 340",        # the password field
        f"{t + 5500} type pw",
        f"{t + 7000} click 705 572",        # Ok (ends the text entry)
        f"{t + 8500} click 705 572",        # Ok
        f"{t + 12500} dump {{d}}/join{n}-3-back.bmp",
        f"{max(finish_ms, t + 13000)} click 705 572",                 # Manager Status: Finish
    ]
    if host_quits:
        return steps + [
            "wait box 1800000",             # the host left (slice 6)
            "2000 dump {d}/join%d-5-box.bmp" % n,
            "2500 click 398 330",           # Ok
            "wait setup",
            "2000 dump {d}/join%d-6-start.bmp" % n,
            "3000 quit",
        ]
    return steps + [f"{quit_ms - 1000} dump {{d}}/join{n}-4-game.bmp", f"{quit_ms} quit"]


def version_steps(n):
    return nr.refused_steps(n)


# ---- running the games ----

def start(o, work, name, steps, shots, env_extra, settings):
    user = os.path.join(work, name)
    os.makedirs(user, exist_ok=True)
    with open(os.path.join(user, "settings.txt"), "w") as f:
        for k, v in settings.items():
            f.write(f"{k} = {v}\n")
    script = smoke_run.scaled_steps("classic", shots, scaled(steps, o["slow"]))
    env = dict(os.environ, HOME=user, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
               RECOMP_USER_DIR=user, RECOMP_SCREEN_LOG="1", RECOMP_NET_LOG=os.path.join(shots, f"{name}-net.log"),
               RECOMP_SCRIPT="; ".join(script), RECOMP_LOBBY_URL=o["lobby"])
    env.update(env_extra)
    err = open(os.path.join(shots, f"{name}-stderr.txt"), "w", errors="replace")
    p = subprocess.Popen([o["program"], "--game", o["game"]] + (["-seed", o["seed"]] if name == "host" else []),
                         env=env, stdout=subprocess.DEVNULL, stderr=err)
    return p, err


def text(path):
    return open(path, errors="replace").read() if os.path.exists(path) else ""


def wait_text(path, needle, proc, seconds, count=1):
    end = time.time() + seconds
    while time.time() < end and proc.poll() is None:
        if text(path).count(needle) >= count:
            return True
        time.sleep(1)
    return text(path).count(needle) >= count


def lobby_game(o):
    games = nr.lobby_games(o["lobby"]) or []
    g = [x for x in games if x.get("name") == GAME_NAME]
    return g[0] if g else None


def watch_lobby(o, store, stop):
    """Notes each new (season, day, managersOnline) of the game in the lobby."""
    while not stop.wait(3):
        g = lobby_game(o)
        if g:
            v = (g.get("season"), g.get("day"), g.get("managersOnline"), time.time())
            if not store or store[-1][:3] != v[:3]:
                store.append(v)
            o["clubs_seen"] = g.get("humanClubs") or []


def play_host(o, p, err_path, days, check, leg):
    """Watches the host to its end: a box, a stall or the timeout stop it. Returns its faults."""
    faults, last, moved, t0 = [], None, time.time(), time.time()
    while p.poll() is None:
        time.sleep(10)
        t = text(err_path)
        m = BOX.search(t)
        if m:
            time.sleep(3)
            d = DATE.findall(text(err_path))
            faults.append(f"message box: {m.group(2).strip()} (from {m.group(1)}, day "
                          f"{'/'.join(d[-1]) if d else '?'})")
            p.terminate()
            break
        d = DATE.findall(t)
        if d:
            cur = (int(d[-1][0]), int(d[-1][1]))
            if cur != last:
                last, moved = cur, time.time()
                if cur[1] > o["start"] and cur[0] >= 182 and cur[1] not in o["reached"]:
                    o["reached"].add(cur[1])
                    print(f"1 July {cur[1]}: {(time.time() - o['t0']) / 60:.0f} min", flush=True)
        if last and last[0] > 0 and time.time() - moved > o["stall"]:
            faults.append(f"stall: the date stays at {last} for {o['stall'] // 60} minutes")
            p.terminate()
            break
        if time.time() - t0 > o["timeout"]:
            faults.append(f"timeout after {o['timeout']} s")
            p.terminate()
            break
    p.wait()
    return faults, last


def run_leg(o, leg, kind, save, days, check):
    """kind: new, restore or quit. Returns (host user folder, last date)."""
    work = os.path.join(o["keep"], f"leg{leg:02d}" if kind != "quit" else "quit")
    shots = os.path.join(work, "shots")
    os.makedirs(shots)
    settings = {"display": "classic", "changing_rules": "on", "faster_days": "on"}
    host_env = {"RECOMP_FIXED_CLOCK": "1", "RECOMP_START_SEASON": str(o["start"]),
                "RECOMP_RULES_REPORT": os.path.join(work, "report.tsv"),
                "RECOMP_RULES_TABLE": rrc.rules_table(o["program"], o["game"]), "RECOMP_SEASON_INTL_LOG": "1"}
    if kind != "quit":
        host_env["RECOMP_SAVE_AFTER_DAYS"] = str(days)
    if save:
        os.makedirs(os.path.join(work, "host"))
        shutil.copyfile(save, os.path.join(work, "host", "samesave.sav"))
    if kind == "new":
        # Seconds after the host's Manager Status: join1 at 10, join2 at 55, join3 at 100 (it drops at its Manager
        # Status, about 150), the new join3 at 195, the fifth game at 210. The joiners click Finish from 240, the
        # host at 300; the joiners quit at 390, the host goes on holiday 120 s after its news screen.
        starts, finish, quit_at = [10, 55, 100], 300, 390
    else:
        # The host takes its manager back by 9.5 s. join1 at 15, join2 at 50, join3 at 85; Finish of the joiners at
        # 140, of the host at 160; the joiners quit at 250 (the quit leg: the host quits 120 s after its news).
        starts, finish, quit_at = [15, 50, 85], 160, 250
    # The host's club, from the lobby's human clubs at the end of the leg before (lobby-clubs.txt): without it the
    # host's manager has no club, and Go on Holiday sits higher in the menu.
    clubs = text(os.path.join(o["keep"], f"leg{leg - 1:02d}", "lobby-clubs.txt")) if leg else ""
    holiday_y = HOLIDAY_Y_NO_CLUB if clubs and "Arsenal" not in clubs.split("\t") else HOLIDAY_Y_CLUB
    host = (host_new(o) if kind == "new" else host_restore()) + host_after_status(
        finish * 1000, 120000, quit_ms=120000 if kind == "quit" else None, holiday_y=holiday_y)
    # The host's steps after the wait for Manager Status count from that wait, as the orchestra below does.
    oc.wait_for_room()
    hp, herr = start(o, work, "host", host, shots, host_env, settings)
    herr_path = os.path.join(shots, "host-stderr.txt")
    procs, lobby_seen, stop = [], [], threading.Event()
    threading.Thread(target=watch_lobby, args=(o, lobby_seen, stop), daemon=True).start()
    try:
        ready = wait_text(herr_path, f"screen {STATUS}", hp, 7200 * o["slow"])
        check(ready, f"{kind} leg {leg}: the host reaches Manager Status")
        if not ready:
            return None, None
        t_status = time.time()

        def at_time(sec):
            delay = t_status + sec * o["slow"] - time.time()
            if delay > 0:
                time.sleep(delay)

        def rel(sec, start_sec):
            """A joiner's step time (ms) for an event at sec after the host's Manager Status. Its steps count from
            its wait for the game list, about 15 s after its start."""
            return int((sec - start_sec - 15) * 1000)

        env = {"RECOMP_TEST_NO_LAN_ROWS": "1"} if o["via"] == "relay" else {}
        jset = {"display": "classic"}
        for n, st in enumerate(starts, 1):
            at_time(st)
            if kind == "new":
                steps = join_new(o, n, n, rel(finish - 70 + 10 * n, st), rel(quit_at, st), drop=(n == 3))
            else:
                steps = join_back(o, n, n, rel(finish - 20, st), rel(quit_at, st), host_quits=(kind == "quit"))
            procs.append((f"join{n}",) + start(o, work, f"join{n}", steps, shots, env, jset))
        if kind == "new":
            # join3 drops at its Manager Status: the script kills it (SIGKILL), so its socket closes with no quit
            # packet, as when a line drops. The original's leave path 0x838c00 puts its club on holiday.
            wait_text(os.path.join(shots, "join3-stderr.txt"), "join3-2-status.bmp", procs[2][1], 150 * o["slow"])
            procs[2][1].kill()
            # The heartbeat (every 30 s) tells the lobby of the drop.
            while time.time() < t_status + 190 * o["slow"]:
                o["drop_seen"] = lobby_game(o)
                if (o["drop_seen"] or {}).get("managersOnline") == JOINERS:
                    break
                time.sleep(2)
            at_time(195)
            procs.append(("join3b",) + start(o, work, "join3b", join_back(o, 3, 3, rel(finish - 20, 195),
                                                                           rel(quit_at, 195)), shots, env, jset))
            at_time(210)
            procs.append(("join5",) + start(o, work, "join5", version_steps(5), shots,
                                            {"RECOMP_TEST_NO_LAN_ROWS": "1", "RECOMP_TEST_VERSION": OTHER_VERSION},
                                            jset))
            at_time(finish - 10)
            o["rejoin_seen"] = lobby_game(o)
        for name, p, err in procs:
            try:
                p.wait(timeout=(quit_at + 1900) * o["slow"])
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait()
            err.close()
        faults, last = play_host(o, hp, herr_path, days, check, leg)
    finally:
        stop.set()
        for _, p, err in procs:
            if p.poll() is None:
                p.kill()
                p.wait()
            err.close()
        if hp.poll() is None:
            hp.kill()
            hp.wait()
        herr.close()

    user = os.path.join(work, "host")
    h = text(herr_path)
    for f in faults:
        check(False, f"1 {kind} leg {leg}: {f}")
    if kind == "quit":
        check(not faults and hp.returncode == 0, f"quit leg: the host ended with code {hp.returncode}")
    else:
        if not faults:
            check(hp.returncode == 0, f"1 leg {leg}: the host ended with code {hp.returncode}")
            check(os.path.exists(os.path.join(user, "samesave.sav")), f"1 leg {leg}: the host saved after "
                                                                       f"{days} game days")
    for name, p, _ in procs:
        j = text(os.path.join(shots, f"{name}-stderr.txt"))
        if name == "join5":
            check(f"join box: This network game runs Bosman Edition {nr.project_version()}, and you have "
                  f"{OTHER_VERSION}" in j, "5 leg 0: the game of another version gets the box with the host's "
                                           "version")
            continue
        if name == "join3" and kind == "new":
            check("join3-2-status.bmp" in j and p.returncode == -9,
                  f"2 leg 0: join3 reached its Manager Status and dropped (code {p.returncode})")
            continue
        dump = f"{name[:5]}-3-back.bmp" if kind != "new" or name == "join3b" else f"{name}-2-status.bmp"
        check(dump in j and "timed out" not in j, f"2 {kind} leg {leg}: {name} reached Manager Status with its club "
                                                  f"({dump})")
        if kind == "quit" and o["host_left"]:
            at_box = j.find("The host left the game")
            check(at_box >= 0 and f"screen {START_SCREEN}" in j[at_box:],
                  f"8 quit leg: {name} shows \"The host left the game\", then the start screen")
        check(p.returncode == 0, f"2 {kind} leg {leg}: {name} ended with code {p.returncode}")
    full = [g for g in lobby_seen if g[2] is not None and g[2] >= 1 + JOINERS]
    check(bool(full), f"3 {kind} leg {leg}: the lobby showed {1 + JOINERS} managers online "
                      f"(seen: {sorted(set(g[2] for g in lobby_seen))})")
    seasons = sorted(set(g[0] for g in lobby_seen if g[0]))
    print(f"     leg {leg}: lobby seasons {seasons}, days {lobby_seen[0][1] if lobby_seen else '-'} to "
          f"{lobby_seen[-1][1] if lobby_seen else '-'}", flush=True)
    o["seasons_seen"].append(seasons)
    if kind == "new":
        d, r = o.get("drop_seen") or {}, o.get("rejoin_seen") or {}
        check(d.get("managersOnline") == JOINERS and r.get("managersOnline") == 1 + JOINERS,
              f"4 leg 0: managers online {d.get('managersOnline')} after join3's drop, "
              f"{r.get('managersOnline')} after its rejoin ({JOINERS} and {1 + JOINERS} expected)")
    if kind == "quit" and o["host_left"]:
        check("the host's save ended" in h, "8 quit leg: the host saved at its quit")
    return user, last


def main(argv):
    if len(argv) < 2 or "--keep" not in argv:
        print(__doc__)
        return 2
    program = os.path.abspath(argv[1])
    game = os.path.abspath(argv[2]) if len(argv) > 2 and not argv[2].startswith("--") else oc.GAME
    start_year = int(arg(argv, "--start", 2001))
    o = {"program": program, "game": game, "start": start_year, "keep": os.path.abspath(arg(argv, "--keep")),
         "nations": arg(argv, "--nations", "England"), "seed": arg(argv, "--seed", "1"),
         "stall": 60 * int(arg(argv, "--stall-minutes", 20)), "timeout": int(arg(argv, "--timeout", 86400)),
         "slow": float(arg(argv, "--slow", 1)), "reached": set(), "t0": time.time(), "seasons_seen": [],
         "address": nr.lan_address()}
    if o["nations"] not in ("England", "all"):
        print("season_net_run.py: --nations England or all")
        return 2
    relay = oc.relay_in_build(program)
    o["via"] = arg(argv, "--via", "relay" if relay else "lan")
    if o["via"] == "relay" and not relay:
        print("season_net_run.py: the build has no relay (slice 5); use --via lan")
        return 2
    with open(program, "rb") as f:
        o["host_left"] = b"The host left the game" in f.read()
    if "--days" in argv:
        legs = [int(arg(argv, "--days"))] * int(arg(argv, "--legs", 2))
        end = None
    elif "--seasons" in argv:
        legs, end = [365] * int(arg(argv, "--seasons")), None
    else:
        end = int(arg(argv, "--end", 2004))
        legs = [365] * (end - start_year) + [int(arg(argv, "--end-days", 62))]
    if "--leg-days" in argv:                # shorter legs: the same days, cut into pieces of --leg-days
        total, d = sum(legs), int(arg(argv, "--leg-days"))
        legs = [d] * (total // d) + ([total % d] if total % d else [])
    if not legs or min(legs) <= 0:
        print("season_net_run.py: no leg to play")
        return 2
    only = int(arg(argv, "--leg")) if "--leg" in argv else None
    if only is not None and not 0 <= only <= len(legs):
        print(f"season_net_run.py: --leg from 0 to {len(legs)} ({len(legs)} is the quit leg)")
        return 2
    if only:                                # a later leg goes on in the folder of the earlier ones
        prev = os.path.join(o["keep"], f"leg{only - 1:02d}", "host", "samesave.sav")
        if not os.path.exists(prev):
            print(f"season_net_run.py: no save of leg {only - 1} ({prev})")
            return 2
    elif os.path.exists(o["keep"]):
        print(f"season_net_run.py: {o['keep']} exists; give a new --keep folder")
        return 2
    os.makedirs(o["keep"], exist_ok=True)
    failed = []

    def check(ok, what):
        print(("ok   " if ok else "FAIL ") + what, flush=True)
        if not ok:
            failed.append(what)

    watch = nr.AddressWatch(o["address"])   # item 216: the network must keep the run's address
    lobby_proc, stop = None, threading.Event()
    o["lobby"] = arg(argv, "--lobby-url")
    if not o["lobby"]:
        port = oc.free_port()
        lobby_proc = nr.start_lobby(port)
        if not lobby_proc:
            print("FAIL the local lobby (npx wrangler dev in server/) does not answer")
            return 1
        o["lobby"] = f"http://127.0.0.1:{port}"
        threading.Thread(target=nr.keep_awake, args=(o["lobby"], stop), daemon=True).start()
    print(f"season run: {len(legs)} legs {legs} from {start_year}, nations {o['nations']}, via {o['via']}, lobby "
          f"{o['lobby']}, slice 6 in the build: {o['host_left']}", flush=True)
    summary = os.path.join(o["keep"], "season.tsv")
    if not only:
        with open(summary, "w") as f:
            f.write("leg\tseason\tresult\tminutes\tlast date\n")
    save, last = None, None
    if only:
        save = os.path.join(o["keep"], f"leg{only - 1:02d}", "host", "samesave.sav")
    try:
        for leg, days in enumerate(legs):
            if only is not None and leg != only:
                continue
            t0 = time.time()
            before = len(failed)
            user, last = run_leg(o, leg, "new" if leg == 0 else "restore", save, days, check)
            ok = len(failed) == before
            with open(os.path.join(o["keep"], f"leg{leg:02d}", "lobby-clubs.txt"), "w") as f:
                f.write("\t".join(o.get("clubs_seen", [])) + "\n")
            with open(os.path.join(o["keep"], f"leg{leg:02d}", "lobby-seasons.txt"), "w") as f:
                f.write(" ".join(str(x) for x in (o["seasons_seen"][-1] if o["seasons_seen"] else [])) + "\n")
            with open(summary, "a") as f:
                f.write(f"{leg}\t{start_year + leg}/{(start_year + leg + 1) % 100:02d}\t{'ok' if ok else 'FAIL'}\t"
                        f"{(time.time() - t0) / 60:.0f}\t{last}\n")
            print(f"leg {leg}: {'ok' if ok else 'FAIL'} after {(time.time() - t0) / 60:.0f} min, last date {last}",
                  flush=True)
            if not user or not os.path.exists(os.path.join(user, "samesave.sav")):
                break
            save = os.path.join(user, "samesave.sav")
        else:
            if only is not None and only < len(legs):
                pass                            # one play leg only: the quit leg is --leg len(legs)
            elif not o["host_left"]:
                print("quit leg skipped: the build has no slice 6 (\"The host left the game\")", flush=True)
            else:
                if only == len(legs):
                    save = os.path.join(o["keep"], f"leg{len(legs) - 1:02d}", "host", "samesave.sav")
                run_leg(o, len(legs), "quit", save, 0, check)
    finally:
        stop.set()
        if lobby_proc:
            os.killpg(lobby_proc.pid, 9)
    if only is not None and only < len(legs) - 1:
        # One leg of a run in pieces: its own checks only; the run's checks come with the last play leg.
        verdict = "FAIL" if failed else "LEG OK"
        print(f"season run, leg {only} of {len(legs)}: {verdict}")
        oc.put_result("release4", f"{os.path.basename(o['keep'])} leg {only}", verdict, "; ".join(failed[:2]),
                      o["keep"])
        return 1 if failed else 0
    if only is not None:                        # the run's checks over the legs of the folder
        o["seasons_seen"] = [[int(x) for x in text(os.path.join(o["keep"], f"leg{n:02d}", "lobby-seasons.txt"))
                              .split()] for n in range(len(legs))]
        dates = DATE.findall(text(os.path.join(o["keep"], f"leg{len(legs) - 1:02d}", "shots", "host-stderr.txt")))
        last = (int(dates[-1][0]), int(dates[-1][1])) if dates else last
    reports = [os.path.join(o["keep"], f"leg{n:02d}", "report.tsv") for n in range(len(legs))]
    comps = [c for r in reports if os.path.exists(r) for c in rrc.read_report(r)[1]]
    stalls = rrc.league_stalls(comps)
    check(not stalls, f"6 every division moves on each season{': ' + stalls[0] if stalls else ''}")
    if end:
        check(bool(last) and last[1] == end and last[0] >= 244, f"7 the run reaches 1 September {end}: last date "
                                                                 f"{last}")
    flat = [s for x in o["seasons_seen"] for s in x]
    check(flat == sorted(flat), f"3 the lobby's season moves forward: {o['seasons_seen']}")
    intl = [line for leg in range(len(legs)) for line in
            text(os.path.join(o["keep"], f"leg{leg:02d}", "shots", "host-stderr.txt")).splitlines()
            if "recomp: season intl:" in line and ("world_cup" in line.lower() or "euro" in line.lower())]
    print(f"     international log lines on the World Cup and the Euros: {len(intl)}")
    for line in intl[:6]:
        print("     " + line[:160])
    nr.check_address(check, watch)
    verdict = "FAIL" if failed else "PASS" if o["host_left"] and end and o["via"] == "relay" else "STAGE"
    print(f"run: {(time.time() - o['t0']) / 60:.0f} min, folder {o['keep']}")
    print(f"season run: {verdict}" + (f" ({len(failed)})" if failed else ""))
    oc.put_result("release4", os.path.basename(o["keep"]), verdict, "; ".join(failed[:2]) or
                  ("the quit leg waits for slice 6" if not o["host_left"] else "") or
                  ("the joins went over the LAN" if o["via"] != "relay" else "") or
                  ("a short run (--days or --seasons)" if not end else ""), o["keep"])
    return 1 if failed else 0 if verdict == "PASS" else 3


if __name__ == "__main__":
    sys.exit(main(sys.argv))
