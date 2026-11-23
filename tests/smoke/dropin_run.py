#!/usr/bin/env python3
"""Drop in, drop out and host quit (0.7.0 slice 6, #45): a two-game run with a drop, a rejoin and a host quit.

Usage:
    python3 tests/smoke/dropin_run.py <cm0102_recomp> <game folder> [--shots <folder>] [--slow k] [--kick]
        [--lobby-url URL] [--relay] [--display H,J]

--display H,J (0.7.0 item 212) puts the host in mode H and the joiners in mode J (classic or widescreen; default
classic,classic), as tests/smoke/network_run.py --display does.

It uses the timed clicks of tests/smoke/network_run.py and a local copy of the lobby (server/, `npx wrangler dev`),
or --lobby-url.

The host makes the network game "Bosman" and its own manager with Arsenal, and waits on Manager Status. Then:
    join1  joins, types the names "Join Two" and the manager password "pw", takes the second club of Select Team,
           reaches Manager Status and quits: a drop. The host's runtime sees the closed connection, and the
           original's leave path (0x838c00) puts the club on holiday: Away.
    join2  joins again with the same names. The host opens Manager Status for it (0x838a80), it clicks the row of
           its old manager, types "pw" in the password box (0x822ec0) and takes the club back. It clicks Finish.
Then the host clicks Finish, the game starts, and the host quits (the script's quit, as a window close). The host's
game saves at its screen loop (net_session.c), and join2 sees "The host left the game" and goes back.

--kick (in place of the rejoin): join1 clicks Finish, the game starts, and the host opens the Network tab of the
Options box. It clicks Kick for join1, then No (join1 stays), then Kick and Yes: join1's club goes on holiday and
join1 sees "The host removed you from the game" (0.7.0 slice 7). The Network tab shows join1's ping.
--drop-at-finish (in place of the rejoin): join1 clicks Finish, the host clicks Finish, and after the first news the
script kills join1 when the host's left bar counts down to 0, so the host works out the day with join1 gone (slice
9's four-player run, 2026-10-29). The host must show no box of
the game ("scrman.cpp 2480" from ScreenManager::send_clear_click 0x800000) and go on to its first news.
--leave (in place of the rejoin): join1 clicks Exit Game and Yes. The host sends it the quit packet (result -12,
0x7fcd79), its club goes on holiday, and join1 shows no "The host left the game".

Checks:
    1 each game ends with code 0
    2 the host's lobby entry lists join1's club as Away after the drop (or the kick), and no club as Away after the
      rejoin
    3 the host's log shows join1's club on holiday after the drop and back in play after join2's password
    4 the host saved at its quit: the log line and a save file in the host's save folder
    5 join2 shows "The host left the game" and then the start screen (0x81bab0)
    6 kick: No keeps join1, Yes kicks it (host log), join1's game shows "The host removed you from the game", and the Network
      tab showed join1's ping
    7 leave: join1 shows no "The host left the game"
    8 drop at Finish: the host shows no box of the game and reaches its first news
"""
import glob
import os
import subprocess
import sys
import tempfile
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import network_run as nr  # noqa: E402

STATUS_BUILD = "0x81ff10"          # Manager Status (setup_build_manager_status_screen)
START_BUILD = "0x81bab0"           # the start screen (setup_build_setup_game_screen)

NAMES = [
    "1000 click 560 246",          # First Name
    "1500 type {first}",
    "3000 click 560 310",          # Second Name
    "3500 type {second}",
]
PASSWORD = [
    "5000 click 560 374",          # Password (Optional)
    "5500 type pw",
    "7000 click 560 438",          # Re-Type
    "7500 type pw",
]


KICK_ROW = "click 688 321"          # the Kick button of the first human manager on the Network tab (0.7.0 slice 7:
                                    # below the Connection test row)
NETWORK_TAB = "click 680 155"
KICK_STEPS = [                      # the host, in the game: Kick with No, then with Yes (Q39)
    # In a network game the left bar counts down (5 to 0) and the day moves on, once or twice, until it rests on
    # Continue Game or Waiting For Play (about 12 s after the news screen, logged 2026-10-28). A click while the bar
    # changes opens nothing.
    "20000 click 45 344",           # Game Options
    "wait popup",
    "500 dump {d}/host-3-popup.bmp",
    "1000 click 175 439",           # Bosman Edition Options: one row lower than in a single-player game (0x823950 above)
    "wait options",
    f"1500 {NETWORK_TAB}",
    "3500 dump {d}/host-4-network.bmp",
    f"4000 {KICK_ROW}",
    "6500 dump {d}/host-5-confirm.bmp",
    "7000 click 300 572",           # No
    "9500 dump {d}/host-6-no.bmp",
    f"10000 {KICK_ROW}",
    "12500 click 705 572",          # Yes
    "15500 dump {d}/host-7-yes.bmp",
    "16000 click 444 524",          # Close
    "18000 quit",
]


def host_steps(finish_ms, finish=True, kick=False):
    return nr.START + [
        "3500 click 277 177",               # Start Network Game
        "6500 type Bosman",
        "8500 click 705 572",
        "10000 click 705 572",
        "wait leagues",
        "2000 click 705 572",               # Select League(s): Next makes the network game
        f"wait screen:{nr.NAME_BUILD}",
    ] + [s.format(first="Host", second="One") for s in NAMES] + [
        "5000 click 705 572",               # Enter Name: Next
        "wait screen:0x821900",
        "1000 click 705 572",               # Select Nationality: Next
        "wait screen:0x822260",
        "1000 click 155 163",               # Select Team: Arsenal
        "wait screen:0x454900",
        "1000 click 722 14",                # Take Control
        "wait screen:0x822aa0",
        "1000 click 705 572",               # Please Confirm: Yes
        f"wait screen:{STATUS_BUILD}",
        "2000 dump {d}/host-1-status.bmp",
        f"{finish_ms} dump {{d}}/host-2-status.bmp",
    ] + ([] if finish else [f"{finish_ms + 500} quit"]) + [
        f"{finish_ms + 500} click 705 572",  # Manager Status: Finish
        "wait first-news",
        "3000 dump {d}/host-3-news.bmp",
    ] + (KICK_STEPS if kick else [
        "8000 quit",                        # as a window close: the save waits for the screen loop
    ])


RELAY = False                               # --relay: the joiners join the host's lobby row through the relay


def join_steps(n, address, rejoin=False, quit_ms=None, leave=False):
    steps = nr.START + [
        "3500 click 612 243",               # Join Network Game
        "wait screen:0x81f8d0",
    ] + ([f"3500 {nr.FIRST_ROW}",           # the host's lobby row: its click copies the name to Join
          "7500 click 705 572"] if RELAY else [
        f"3500 type {address}",
        "6000 click 705 572",               # Next ends the text entry
        "7500 click 705 572",               # Next connects
    ])
    t = 11000
    if not rejoin:
        steps += [f"{t} dump {{d}}/join{n}-1-name.bmp"]
        steps += [f"{t + int(s.split()[0])} " + " ".join(s.split()[1:]).format(first="Join", second="Two")
                  for s in NAMES + PASSWORD]
        t += 9000
        steps += [
            f"{t} click 705 572",           # Enter Name: Next
            f"{t + 4000} dump {{d}}/join{n}-2-nation.bmp",
            f"{t + 4500} click 705 572",    # Select Nationality: Next
            f"{t + 8500} dump {{d}}/join{n}-3-team.bmp",
            f"{t + 9000} click 155 187",    # Select Team: the second club
            f"{t + 13000} dump {{d}}/join{n}-4-club.bmp",
            f"{t + 13500} click 722 14",    # Take Control
            f"{t + 17500} dump {{d}}/join{n}-5-confirm.bmp",
            f"{t + 18000} click 705 572",   # Please Confirm: Yes
            f"{t + 22000} dump {{d}}/join{n}-6-status.bmp",
        ]
        if quit_ms:  # --kick: Finish, then wait for the kick
            steps += [f"{t + 23000} click 705 572", f"{quit_ms - 1000} dump {{d}}/join{n}-7-kicked.bmp",
                      f"{quit_ms} quit"]
        elif leave:   # the joiner's own Exit Game: the host sends the quit packet (0x7fcd79), not a host quit
            steps += [
                f"{t + 23000} click 45 210",    # Exit Game
                f"{t + 27000} dump {{d}}/join{n}-7-exit.bmp",
                f"{t + 27500} click 705 572",   # Exit the network game ? Yes
                f"{t + 33000} dump {{d}}/join{n}-8-left.bmp",
                f"{t + 34000} quit",
            ]
        else:
            steps += [f"{t + 24000} quit"]   # the drop
    else:
        steps += [
            f"{t} dump {{d}}/join{n}-1-status.bmp",
            f"{t + 500} click 150 211",     # Manager Status: the second row, the manager on holiday
            f"{t + 4500} dump {{d}}/join{n}-2-password.bmp",
            f"{t + 5000} click 445 340",    # the password field
            f"{t + 5500} type pw",
            f"{t + 7000} click 705 572",    # Ok (ends the text entry)
            f"{t + 8500} click 705 572",    # Ok
            f"{t + 12500} dump {{d}}/join{n}-3-back.bmp",
            f"{t + 13000} click 705 572",   # Manager Status: Finish
            f"{t + 17000} dump {{d}}/join{n}-4-finish.bmp",
            "wait box 900000",              # the game's message box: the host left
            "2000 dump {d}/join%d-5-box.bmp" % n,
            "2500 click 398 330",           # Ok
            "wait setup",
            "2000 dump {d}/join%d-6-start.bmp" % n,
            "3000 quit",
        ]
    return steps


def scaled(steps, slow):
    """Every step time times slow. A wait gets a limit of 120 s times slow, unless it names its own."""
    out = []
    for s in steps:
        p = s.split()
        if p[0] == "wait":
            out.append(s if len(p) > 2 else f"{s} {int(120000 * slow)}")
        else:
            out.append(" ".join([str(int(int(p[0]) * slow))] + p[1:]))
    return out


def start(program, game, work, name, steps, shots, slow, env):
    return nr.start(program, game, work, name, scaled(steps, slow), shots, 1, env)


def text(path):
    return open(path, errors="replace").read() if os.path.exists(path) else ""


def lobby_away(url):
    games = nr.lobby_games(url) or []
    g = [x for x in games if x.get("name") == "Bosman"]
    return g[0].get("awayClubs") if g else None


def main(argv):
    args = argv[1:]
    if len(args) < 2:
        print(__doc__)
        return 2
    program, game = os.path.abspath(args[0]), os.path.abspath(args[1])
    shots, slow, kick, leave, lobby_url, dayquit = None, 1.0, False, False, None, False
    rest = args[2:]
    while rest:
        k = rest.pop(0)
        if k == "--drop-at-finish":
            dayquit = True
            continue
        if k == "--relay":
            global RELAY
            RELAY = True
            continue
        if k in ("--kick", "--leave"):
            kick, leave = kick or k == "--kick", leave or k == "--leave"
            continue
        v = rest.pop(0) if rest else None
        if v is None:
            print(__doc__)
            return 2
        if k == "--shots":
            shots = os.path.abspath(v)
        elif k == "--slow":
            slow = float(v)
        elif k == "--lobby-url":
            lobby_url = v
        elif k == "--display" and v in ("classic,classic", "classic,widescreen", "widescreen,classic",
                                         "widescreen,widescreen"):
            nr.DISPLAYS["host"], nr.DISPLAYS["join"] = v.split(",")      # item 212: the mode of the host, of joiners
        else:
            print(__doc__)
            return 2
    failed = []

    def check(ok, what):
        print(("ok   " if ok else "FAIL ") + what)
        if not ok:
            failed.append(what)

    address = nr.lan_address()
    watch = nr.AddressWatch(address)        # item 216: the network must keep the run's address
    lobby_proc = None
    if not lobby_url:
        port = 8890 + os.getpid() % 100
        lobby_proc = nr.start_lobby(port)
        if not lobby_proc:
            print("FAIL the local lobby (npx wrangler dev in server/) does not answer")
            return 1
        lobby_url = f"http://127.0.0.1:{port}"
    env = {"RECOMP_LOBBY_URL": lobby_url}
    if RELAY:                                  # lobby rows only, as on a computer that is not on the host's LAN
        env = dict(env, RECOMP_TEST_NO_LAN_ROWS="1")
    finish_ms = 200000 if not leave else 150000   # step times; start() scales them
    procs = []
    away_after_drop, away_after_rejoin = None, None
    with tempfile.TemporaryDirectory(prefix="cm0102-dropin-") as work:
        shots = shots or work
        os.makedirs(shots, exist_ok=True)
        stop = threading.Event()
        threading.Thread(target=nr.keep_awake, args=(lobby_url, stop), daemon=True).start()
        host_env = env
        host, host_err = start(program, game, work, "host", host_steps(finish_ms, not leave, kick), shots, slow,
                                host_env)
        host_log = os.path.join(shots, "host-stderr.txt")
        try:
            ready = nr.wait_for(host_log, f"screen {STATUS_BUILD}", host, 300 * slow)
            check(ready, "the host reaches Manager Status with its own manager")
            if ready:
                # join1 joins and makes its manager. Without --kick it quits at the end of its steps.
                quit_ms = 330000 if kick or dayquit else None
                p1, e1 = start(program, game, work, "join1", join_steps(1, address, quit_ms=quit_ms, leave=leave), shots, slow,
                                  env)
                procs.append(("join1", p1, e1))
                if dayquit:
                    # join1 quits while the host works out a day of the game: the left bar counts down to 0, then the
                    # host works out the day and opens the next screens for each channel (slice 9's run: the box came
                    # from the news screen 0x788eb3, through the menu bar build, to send_clear_click 0x800000).
                    end = time.time() + 900 * slow
                    while time.time() < end and host.poll() is None:
                        h0 = text(host_log)
                        at = h0.find("reached first-news")
                        if at >= 0 and "left bar (|)|0|" in h0[at:]:
                            break
                        time.sleep(0.05)
                    p1.kill()
                    print("join1 killed when the host's left bar reached 0, after the first news")
                p1.wait(timeout=(600 if kick else 240) * slow)
                for _ in range(int(60 * slow)):    # the heartbeat comes every 30 s
                    away_after_drop = lobby_away(lobby_url)
                    if away_after_drop:
                        break
                    time.sleep(1)
                if not kick and not leave and not dayquit:
                    time.sleep(4 * slow)
                    p2, e2 = start(program, game, work, "join2",
                                      join_steps(2, address, rejoin=True), shots, slow,
                                      env)
                    procs.append(("join2", p2, e2))
                    for _ in range(int(60 * slow)):
                        away_after_rejoin = lobby_away(lobby_url)
                        if away_after_rejoin == []:
                            break
                        time.sleep(1)
            for _, p, _ in procs:
                try:
                    p.wait(timeout=400 * slow)
                except subprocess.TimeoutExpired:
                    pass
            try:
                host.wait(timeout=400 * slow)
            except subprocess.TimeoutExpired:
                pass
        finally:
            for _, p, err in procs + [("host", host, host_err)]:
                if p.poll() is None:
                    p.kill()
                    p.wait()
                err.close()
            stop.set()
            if lobby_proc:
                os.killpg(lobby_proc.pid, 9)

        check(host.returncode == 0, f"1 host ended with code {host.returncode}")
        for name, p, _ in procs:
            if not dayquit:
                check(p.returncode == 0, f"1 {name} ended with code {p.returncode}")
        h = text(host_log)
        if kick or dayquit:   # the host quits before the heartbeat after the drop: its own log shows Away
            away_after_drop = [x.split("lobby: away 1 ", 1)[1] for x in h.splitlines() if "lobby: away 1 " in x] or None
        check(bool(away_after_drop) and len(away_after_drop) == 1 and away_after_drop[0] != "Arsenal",
              f"2 the lobby lists join1's club as Away after the {'kick' if kick else 'exit' if leave else 'drop'}: "
              f"{away_after_drop}")
        if dayquit:
            check("message box from" not in h, "8 the host shows no box of the game after join1's drop (scrman.cpp 2480)")
            check(os.path.exists(os.path.join(shots, "host-3-news.bmp")), "8 the host goes on to its first news")
        elif leave:
            j1 = text(os.path.join(shots, "join1-stderr.txt"))
            check("The host left the game" not in j1, "7 join1 left by its own Exit Game and shows no host-left box")
        elif kick:
            j1 = text(os.path.join(shots, "join1-stderr.txt"))
            no_at, yes_at = h.find(": no\n", h.find("options box: Kick")), h.find("the host kicks the human manager")
            check(0 <= no_at < yes_at and ": yes" in h, "6 Kick: No keeps join1, then Yes kicks it (host log)")
            check("net box: Network Game: The host removed you from the game." in j1,
                  "6 join1 shows the kicked box after the kick")
            pings = [l.split(" ping ")[1] for l in h.splitlines() if "options box: " in l and " ping " in l]
            check(any(p.endswith(" ms") for p in pings), f"6 the Network tab shows join1's ping: {pings[:3]}")
        else:
            check(away_after_rejoin == [], f"2 no club is Away after the rejoin: {away_after_rejoin}")
            away1 = h.find("lobby: away 1 ")
            check(away1 >= 0 and "lobby: away 0" in h[away1:], "3 on the host, join1's club is on holiday after the "
                  "drop and back in play after join2's password (host log)")
            # The game's save folder is the game folder (options + 0xf), which the runtime moves to the user folder.
            saves = glob.glob(os.path.join(work, "host", "Network Game.sav"))
            check("the host's save ended" in h and bool(saves), f"4 the host saved at its quit: {saves}")
            j2 = text(os.path.join(shots, "join2-stderr.txt"))
            at = j2.find("net box: Network Game: The host left the game.")
            check(at >= 0, "5 join2 shows \"The host left the game\"")
            check(at >= 0 and f"screen {START_BUILD}" in j2[at:], "5 join2 goes back to the start screen after Ok")
        for f in sorted(os.listdir(shots)):
            if f.endswith(".bmp"):
                subprocess.run(["sips", "-s", "format", "png", os.path.join(shots, f), "--out",
                                os.path.join(shots, f[:-4] + ".png")], capture_output=True)
    nr.check_address(check, watch)
    print("dropin run: " + ("PASS" if not failed else f"FAIL ({len(failed)})"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
