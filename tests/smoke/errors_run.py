#!/usr/bin/env python3
"""Clear errors, connection test and ping (0.7.0 slice 7, #45): one forced failure for each message.

Usage:
    python3 tests/smoke/errors_run.py <cm0102_recomp> <game folder> [--shots <folder>] [--slow k] [--only a,b]
        [--lobby-url URL]

It uses the timed clicks of tests/smoke/network_run.py. Each case runs one game, or a host and a joiner, one case
after the other: two network runs on one computer collide on port 6667, so start it through
/Users/max/Projects/cm0102/lead-notes/tools/net-lock.sh. The local lobby is a copy of the server
(server/, `npx wrangler dev`), or --lobby-url. A closed port stands in for a lobby that is down.

Cases (each checks the text in the game's log: "join box:" and "create box:" come from the wrap of confirm_push
0x591840 as the original's box gets the text, "net box:" from the game's message box 0x5e8290):
    lobby-down      Online play on, the lobby does not answer, LAN rows off: the list shows the "lobby does not
                    answer" row, and Join on it gives that reason
    full-today      a lobby that answers full_today: the "full today" row and its box
    game-gone       the joiner types an address of a lobby row that is not in the list (0.66.1.2)
    host-refused    the joiner types this computer's LAN address, where no game runs
    timeout         the joiner types 192.0.2.1 (TEST-NET-1), where no host answers
    create-port     another program holds port 6667 on each adapter: Start Network Game gives the "Unable To
                    Create" box with that reason
    version         a host, and a joiner of another version (RECOMP_TEST_VERSION) that types its address
    lost            a host in the lobby and a joiner that joins and waits: the lobby lists the joiner's ping
                    (pingMs), then the host is killed, and the joiner's box says that the connection was lost
    test-up         the connection test on the Network tab of the Options box, with the local lobby: it works
    test-down       the connection test with the lobby down: it says so
The other messages have their own runs: wrong password (password_run.py), kicked and host left (dropin_run.py
--kick and dropin_run.py, which also shows the ping on the Network tab).
"""
import os
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import network_run as nr  # noqa: E402

START = nr.START[:-1]                       # to the start screen, before Network Play
JOIN_LIST = START + [
    "1000 click 277 346",                   # Network Play
    "3500 click 612 243",                   # Join Network Game
    "wait screen:0x81f8d0",                 # the game list
    "3000 dump {d}/%s-0-list.bmp",
]


def by_row(name):
    """Join on the first row of the list, then the box."""
    return [s.replace("%s", name) for s in JOIN_LIST] + [
        f"3500 {nr.FIRST_ROW}",
        "7500 click 705 572",               # Next connects
        "16000 dump {d}/%s-1-box.bmp" % name,
        "17000 quit",
    ]


def typed(name, address):
    """Join by a typed address, then the box. A connect waits at most 4 s (NC_CONNECT_MS)."""
    return [s.replace("%s", name) for s in JOIN_LIST] + [
        f"3500 type {address}",
        "6000 click 705 572",               # Next ends the text entry
        "7500 click 705 572",               # Next connects
        "16000 dump {d}/%s-1-box.bmp" % name,
        "17000 quit",
    ]


def host_to_leagues(name, then):
    return START + [
        "1000 click 277 346",               # Network Play
        "3500 click 277 177",               # Start Network Game
        "6500 type Bosman",
        "8500 click 705 572",
        "10000 click 705 572",
        "wait leagues",
        "2000 click 705 572",               # Select League(s): Next makes the network game
    ] + then


def joined_waits(address):
    """Join, then wait on Enter Name until the host goes, then the box and Ok."""
    return START + [
        "1000 click 277 346",
        "3500 click 612 243",
        "wait screen:0x81f8d0",
        f"3500 type {address}",
        "6000 click 705 572",
        "7500 click 705 572",
        "11000 dump {d}/lost-1-name.bmp",
        "wait box 400000",                  # the game's message box
        "2000 dump {d}/lost-2-box.bmp",
        "2500 click 398 330",               # Ok
        "wait setup",
        "2000 dump {d}/lost-3-start.bmp",
        "3000 quit",
    ]


TEST_BUTTON = "click 380 227"              # the Test button of the Connection test row (options_screen.c)
NETWORK_TAB = "click 680 155"


def connection_test(name):
    return START + [
        "1500 click 277 425",               # Bosman Edition Options (the start screen's menu)
        f"3500 {NETWORK_TAB}",
        "5000 dump {d}/%s-0-tab.bmp" % name,
        f"5500 {TEST_BUTTON}",
        "6500 dump {d}/%s-1-testing.bmp" % name,
        "19000 dump {d}/%s-2-result.bmp" % name,
        "19500 click 444 524",              # Close
        "21000 quit",
    ]


def adapters():
    """The IPv4 addresses of this computer's adapters that are up, not loopback."""
    out = subprocess.run(["ifconfig"], capture_output=True, text=True).stdout
    return [line.split()[1] for line in out.splitlines()
            if line.strip().startswith("inet ") and not line.split()[1].startswith("127.")]


def closed_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return f"http://127.0.0.1:{port}"


def main(argv):
    args = argv[1:]
    if len(args) < 2:
        print(__doc__)
        return 2
    program, game = os.path.abspath(args[0]), os.path.abspath(args[1])
    shots, slow, only, lobby_url = None, 1.0, None, None
    rest = args[2:]
    while rest:
        k, v = rest.pop(0), rest.pop(0) if rest else None
        if v is None:
            print(__doc__)
            return 2
        if k == "--shots":
            shots = os.path.abspath(v)
        elif k == "--slow":
            slow = float(v)
        elif k == "--only":
            only = set(v.split(","))
        elif k == "--lobby-url":
            lobby_url = v
        else:
            print(__doc__)
            return 2
    failed = []

    def check(ok, what):
        print(("ok   " if ok else "FAIL ") + what, flush=True)
        if not ok:
            failed.append(what)

    def want(case):
        return only is None or case in only

    address = nr.lan_address()
    version = nr.project_version()
    lobby_proc = None
    needs_lobby = any(want(c) for c in ("lost", "test-up"))
    if needs_lobby and not lobby_url:
        port = 8990 + os.getpid() % 100
        lobby_proc = nr.start_lobby(port)
        if not lobby_proc:
            print("FAIL the local lobby (npx wrangler dev in server/) does not answer")
            return 1
        lobby_url = f"http://127.0.0.1:{port}"
    stop = threading.Event()
    if lobby_url:
        threading.Thread(target=nr.keep_awake, args=(lobby_url, stop), daemon=True).start()

    with tempfile.TemporaryDirectory(prefix="cm0102-errors-") as work:
        shots = shots or work
        os.makedirs(shots, exist_ok=True)

        def log(name):
            p = os.path.join(shots, f"{name}-stderr.txt")
            return open(p, errors="replace").read() if os.path.exists(p) else ""

        def run(name, steps, env=None, online=True, timeout=240):
            p, err = nr.start(program, game, work, name, steps, shots, slow, env or {}, online)
            try:
                p.wait(timeout=timeout * slow)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait()
            err.close()
            check(p.returncode == 0, f"{name} ended with code {p.returncode}")
            return log(name)

        def box_has(text, kind, start):
            return f"{kind} box: {start}" in text

        try:
            if want("lobby-down"):
                t = run("lobby-down", by_row("lobby-down"), {"RECOMP_LOBBY_URL": closed_port(),
                                                             "RECOMP_TEST_NO_LAN_ROWS": "1"})
                rows = nr.list_rows(os.path.join(shots, "lobby-down-stderr.txt"))
                check(" The lobby does not answer, try again later" in rows, f"lobby-down: the list row: {rows}")
                check(box_has(t, "join", "The lobby, which lists the games online, does not answer."),
                      "lobby-down: Join gives the reason")
            if want("full-today"):
                full = nr.FakeLobby(True)
                t = run("full-today", by_row("full-today"), {"RECOMP_LOBBY_URL": full.url,
                                                             "RECOMP_TEST_NO_LAN_ROWS": "1"})
                check(box_has(t, "join", "Online games are full today, try again later. They open again at 00:00 UTC."),
                      "full-today: the box")
            if want("game-gone"):
                t = run("game-gone", typed("game-gone", "0.66.1.2"))
                check(box_has(t, "join", "This network game is no longer in the list."), "game-gone: the box")
            if want("host-refused"):
                t = run("host-refused", typed("host-refused", address))
                check(box_has(t, "join", "No network game runs at this address now."), "host-refused: the box")
            if want("timeout"):
                t = run("timeout", typed("timeout", "192.0.2.1"))
                check(box_has(t, "join", "The host did not answer. Check the address.") or
                      box_has(t, "join", "This computer cannot reach the host."),
                      "timeout: the box says that the host did not answer (or cannot be reached)")
            if want("create-port"):
                held = []
                for a in adapters():
                    s = socket.socket()
                    try:
                        s.bind((a, 6667))
                        s.listen(1)
                        held.append(s)
                    except OSError as e:
                        print(f"create-port: {a}:6667 not held ({e})")
                        s.close()
                try:
                    t = run("create-port", host_to_leagues("create-port", [
                        "60000 dump {d}/create-port-1-box.bmp", "61000 quit"]))
                finally:
                    for s in held:
                        s.close()
                check(bool(held) and box_has(t, "create", "Another program on this computer uses port 6667"),
                      f"create-port: the Unable To Create box gives the reason ({len(held)} adapters held)")
            if want("version"):
                host, herr = nr.start(program, game, work, "version-host", host_to_leagues("version-host", [
                    f"wait screen:{nr.NAME_BUILD}", "40000 quit"]), shots, slow, {})
                ready = nr.wait_for(os.path.join(shots, "version-host-stderr.txt"), f"screen {nr.NAME_BUILD}", host,
                                    240 * slow)
                check(ready, "version: the host reaches Enter Name")
                t = run("version", typed("version", address), {"RECOMP_TEST_VERSION": "0.0.1"}) if ready else ""
                host.wait(timeout=120 * slow)
                herr.close()
                check(box_has(t, "join", f"This network game runs Bosman Edition {version}, and you have 0.0.1."),
                      "version: the box names both versions")
            if want("lost"):
                env = {"RECOMP_LOBBY_URL": lobby_url}
                host, herr = nr.start(program, game, work, "lost-host", host_to_leagues("lost-host", [
                    f"wait screen:{nr.NAME_BUILD}", "400000 quit"]), shots, slow, env)
                ready = nr.wait_for(os.path.join(shots, "lost-host-stderr.txt"), f"screen {nr.NAME_BUILD}", host,
                                    240 * slow)
                check(ready, "lost: the host reaches Enter Name")
                pings = []
                if ready:
                    p, err = nr.start(program, game, work, "lost", joined_waits(address), shots, slow, env)
                    # The host's heartbeat comes every 30 s and carries the joiner's ping.
                    for _ in range(int(90 * slow)):
                        games = [g for g in nr.lobby_games(lobby_url) or [] if g.get("name") == "Bosman"]
                        if games and games[0].get("pingMs", 0) > 0:
                            pings.append(games[0]["pingMs"])
                            break
                        time.sleep(1)
                    time.sleep(2)
                    host.send_signal(signal.SIGKILL)   # a crash: no quit packet
                    try:
                        p.wait(timeout=240 * slow)
                    except subprocess.TimeoutExpired:
                        p.kill()
                        p.wait()
                    err.close()
                    check(p.returncode == 0, f"lost ended with code {p.returncode}")
                host.wait()
                herr.close()
                check(bool(pings) and 0 < pings[0] < 1000, f"lost: the lobby lists the joiner's ping: {pings}")
                t = log("lost")
                check("net box: Network Game: The connection to the host was lost." in t,
                      "lost: the joiner's box says that the connection was lost")
                check(f"screen {dropin_start()}" in t[t.find("net box:"):], "lost: then the start screen")
            for case, url, want_text in (("test-up", lobby_url, "Online play works. The lobby answered in "),
                                         ("test-down", closed_port(), "The lobby does not answer.")):
                if not want(case):
                    continue
                t = run(case, connection_test(case), {"RECOMP_LOBBY_URL": url})
                check(f"connection test: {want_text}" in t, f"{case}: the result")
                built = t.count("options box built, tab 3")
                check(built >= 3, f"{case}: the tab builds again with the result ({built} builds of the tab)")
        finally:
            stop.set()
            if lobby_proc:
                os.killpg(lobby_proc.pid, 9)
        for f in sorted(os.listdir(shots)):
            if f.endswith(".bmp"):
                subprocess.run(["sips", "-s", "format", "png", os.path.join(shots, f), "--out",
                                os.path.join(shots, f[:-4] + ".png")], capture_output=True)
    print("errors run: " + ("PASS" if not failed else f"FAIL ({len(failed)})"))
    return 1 if failed else 0


def dropin_start():
    return "0x81bab0"                       # the start screen (setup_build_setup_game_screen)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
