#!/usr/bin/env python3
"""Two-game run (0.7.0 slice 2, #45): a host game and a joining game on this computer play one network game.

Usage:
    python3 tests/smoke/network_run.py <cm0102_recomp> <game folder> [--address a.b.c.d] [--shots <folder>]
        [--joiners n] [--slow k]

The games use the same timed clicks as the smoke run (RECOMP_SCRIPT), each with its own empty user folder.

The host: Network Play, Start Network Game, the game name "Bosman", Select League(s) Next (the original makes
the network game here, setup_create_or_join_network_game 0x8388e0, and loads the database), then its own Enter
Name screen (0x820f50). The run starts the joiners when the host's screen log reaches that screen.

Each joiner: Network Play, Join Network Game, the game list (0x81f8d0), types the host's address in the Join
field and clicks Next. Max's decision Q32: the games meet through this computer's LAN address (--address,
default: the address of the default route), not 127.0.0.1, because the original's loopback test 0x8af290 holds
127.0.0.1:6667. The host opens Enter Name on the joiner's channel (setup_on_network_player_joined 0x838a80)
and sends it. The joiner types "Join" and "Two" and clicks Next, and the host sends Select Nationality
(setup_handle_enter_name_input 0x821520). After the joiners quit, the host's own manager types the same names on
its own Enter Name and clicks Next, as the joiner did.

The click repeat (research note, section 11): while the button is down, the client loop 0x7fdc00 sends a click
(opcode 6) for each new host screen (Display::poll_input 0x55f620 at 0x7fdf17). Without the runtime's fix, the
press of the join click reached the host's Enter Name as Next ("Names Are Blank"), and Next on Enter Name also
clicked Next on Select Nationality (Select Team). Checks 2, 5 and 6 catch it.

Checks:
    1 each game ends with code 0
    2 the joiner's Enter Name equals the host's own Enter Name (at most 1% of pixels differ: the text cursor),
      so the join gave no "Names Are Blank"
    3 the joiner's Enter Name after its typed names went to the host and back equals the host's own with the
      same names
    4 the traffic logs (RECOMP_NET_LOG) show the connection on both sides, with packets both ways
    5 after Next on Enter Name the joiner's screen equals the host's own Select Nationality (not Select Team)
    6 each joiner sends 4 click packets (opcode 6): no press is sent twice and none is lost, the press that
      ends a text entry on Next included

--slow k multiplies every step time and timeout by k, for the Windows build under wine in an emulated container.

--joiners n (default 1, at most 3) starts n joiners at once, for the traffic of n + 1 human managers. The run
prints each game's traffic log. With --shots the dumps and logs stay in that folder.

--lobby (0.7.0 slice 4) runs the lobby with the host and four joiners. The lobby is a local copy of the server
(server/, `npx wrangler dev`, so server/node_modules must exist: `cd server && npm ci`), or --lobby-url. Two fake
lobbies in this script stand in for the failures. The host has RECOMP_LOBBY_URL set and Online play on.
    join1  Online play on: the game list shows the host once (one row for each host, not one for each adapter,
           and not again as a lobby row). It clicks that row and joins through the LAN, as above.
    join2  Online play on, lobby rows only (RECOMP_TEST_NO_LAN_ROWS), another version (RECOMP_TEST_VERSION): the
           list shows the host's lobby row with the season, the day and the managers online. Join gives the box
           with the host's version.
    join3  Online play off, lobby address of a fake lobby that counts connections: it types the host's address
           and joins as above. The fake lobby must see no connection.
    join4  Online play on, a fake lobby that answers full_today: the list shows the "full today" row, and Join on
           it gives the box that says so.
--relay (0.7.0 slice 5) runs the host and n joiners (--joiners) through the relay of the lobby (the local copy, or
--lobby-url for the deployed server). Each joiner sees only lobby rows (RECOMP_TEST_NO_LAN_ROWS), clicks the
host's row and joins through the relay; the game sees an ordinary socket. Checks 1 to 6 hold as on the LAN (the
joiner's connection is to the row's 0.66 address, and the host's connections are from 0.66.255.<channel>), with
checks 7 and 10 and:
   11  each joiner joined through the relay and the host held one relay connection; the relay lines of the
       host's log give the frames that it sent (batches of about 50 ms)

--display H,J (0.7.0 item 212) puts the host in mode H and every joiner in mode J (classic or widescreen: Classic mode,
Widescreen mode; default classic,classic). When they differ, checks 2, 3 and 5 compare each joiner's dumps with those
of an earlier run whose host had the joiners' mode (--same-as <its --shots folder>): the host's mode must not change
what a joiner sees. --online-off turns Online play off in every game of a LAN run, so no game talks to the lobby.
tests/smoke/network_display_run.py runs the four combinations.

--game-name <word> names the host's game (default Bosman). --remote n (with --relay and --lobby-url) runs the host only and waits --remote-wait seconds (default 1800) at
its Enter Name for n joiners on other computers, for example the GitHub machines of
.github/workflows/online-join.yml (tests/smoke/relay_join_remote.py). The checks are 1, 4 (the host's side, n
connections), 7, 10 and 11 (the host's side). The host's dumps stay in --shots, for relay_join_remote.py
--compare with the joiners' dumps.
Lobby checks, besides checks 1 to 6 for join1 and join3:
    7  the lobby lists the game with the name, the version, the database, a season, a day, the managers online
       and no password, while the host plays
    8  join1's list has one row for the host's game; join2's list has its lobby row with the details
    9  join2 and join4 see the reason of the refused join; join3's lobby saw no connection
   10  the lobby lists no game after the host quit
"""
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smoke_run  # noqa: E402

# Cloudflare answers the deployed server's requests with "error code: 1010" (403) when they carry Python's own
# User-Agent "Python-urllib/3.x" (checked 2026-10-27); the game's "Bosman-Edition" passes. Every urlopen of the
# scripts that import this module sends this one.
_opener = urllib.request.build_opener()
_opener.addheaders = [("User-Agent", "Bosman-Edition-tests")]
urllib.request.install_opener(_opener)


START = [
    smoke_run.INTRO_CLICK,
    smoke_run.SETTINGS_WAIT,
    "8200 click 522 272",                   # settings: Background Changes Off
    "8500 click 662 524",                   # settings: Ok
    "wait setup",
    "1000 click 277 346",                   # Network Play
]
GAME_NAME = "Bosman"                        # --game-name: one word, for a run on the public lobby
NAME_BUILD = "0x820f50"                     # Enter Name of a new human manager (setup_add_human_manager)
def host_steps(joiners, later=None):
    # The joiners play after the host's dump; they start 4 s apart and each plays about 40 s. The host's own
    # manager types the same names after they quit. --remote: the host waits the given time for joiners on
    # other computers.
    later = later or 60000 + 8000 * (joiners - 1)
    return START + [
        "3500 click 277 177",               # Start Network Game
        f"6500 type {GAME_NAME}",           # Enter Network Game Name
        "8500 click 705 572",               # Next (ends the text entry)
        "10000 click 705 572",              # Next
        "wait leagues",
        "2000 click 705 572",               # Select League(s): Next makes the network game and loads the data
        f"wait screen:{NAME_BUILD}",
        "2000 dump {d}/host-1-name.bmp",
        f"{later} click 560 246",           # First Name
        f"{later + 500} type Join",
        f"{later + 2000} click 560 310",    # Second Name
        f"{later + 3000} type Two",
        f"{later + 4500} click 560 246",    # First Name
        f"{later + 5000} dump {{d}}/host-2-typed.bmp",
        f"{later + 6000} click 705 572",    # Next: Select Nationality
        f"{later + 9000} dump {{d}}/host-3-nation.bmp",
        f"{later + 10000} quit",
    ]


FIRST_ROW = "click 445 257"                 # the first row of the game list area (190, 234)-(700, 510), 6 rows (0x81fad4)


def joiner_steps(address, n, by_row=False):
    if by_row:   # the row's click copies its name into the Join field (0x81fd08..0x81fd63) and ends the entry
        join = [f"3500 {FIRST_ROW}", "7500 click 705 572"]
    else:
        join = [f"3500 type {address}", "6000 click 705 572", "7500 click 705 572"]
    return START + [
        "3500 click 612 243",               # Join Network Game
        "wait screen:0x81f8d0",             # the game list; the Join field takes the keys
        "3000 dump {d}/join%d-0-list.bmp" % n,
    ] + join + [                            # Next ends the text entry, then Next connects and runs 0x7fdc00
        # The press of that click is still down when the client loop polls the host's Enter Name. The runtime
        # keeps it from the host (user32.c, the poll at 0x7fdf17), so the host gives no "Names Are Blank".
        "11000 dump {d}/join%d-1-name.bmp" % n,
        "11500 click 560 246",              # First Name
        "12000 type Join",
        "14000 click 560 310",              # Second Name
        "15000 type Two",
        # The joiner draws its own text entry. A click sends the click packet (opcode 6, Display::send_widget_click
        # 0x568560) with the fields, and the host draws the names in the screen it sends back.
        "16500 click 560 246",
        "19000 dump {d}/join%d-2-typed.bmp" % n,
        "19500 type s",                     # First Name "Joins": the entry is open with changed text
        # Next ends the entry (the client loop's entry poll at 0x7fe05d) and the loop sends First Name, then
        # Next in the same press (poll 0x7fdf17). One press on one screen: Select Nationality, not Select Team.
        "20000 click 705 572",
        "23000 dump {d}/join%d-3-nation.bmp" % n,
        "24000 quit",
    ]


# The joiner's click packets: First Name (sent when Second Name ends its entry), Second Name, First Name and Next
# (both from the last press, which ends the entry of First Name on Next).
JOINER_CLIENT_CLICKS = 4


def refused_steps(n):
    """A joiner whose join is refused: the list, its first row, Next, the box."""
    return START + [
        "3500 click 612 243",
        "wait screen:0x81f8d0",
        "3000 dump {d}/join%d-0-list.bmp" % n,
        f"3500 {FIRST_ROW}",
        "7500 click 705 572",
        "11000 dump {d}/join%d-1-box.bmp" % n,
        "12000 quit",
    ]


def project_version():
    text = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "CMakeLists.txt")).read()
    return re.search(r"project\([^)]*VERSION ([0-9.]+)", text).group(1)


class FakeLobby:
    """A lobby on 127.0.0.1 that counts connections, and answers each request with full_today when full is set."""

    def __init__(self, full):
        self.full, self.count = full, 0
        self.sock = socket.socket()
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("127.0.0.1", 0))
        self.sock.listen(8)
        self.url = "http://127.0.0.1:%d" % self.sock.getsockname()[1]
        threading.Thread(target=self.serve, daemon=True).start()

    def serve(self):
        body = b'{"error":"full_today","message":"Online games are full today","retryAfterSeconds":3600}'
        while True:
            try:
                c, _ = self.sock.accept()
            except OSError:
                return
            self.count += 1
            c.settimeout(2)
            try:
                c.recv(4096)
                if self.full:
                    c.sendall(b"HTTP/1.1 503 Service Unavailable\r\nX-Bosman-Api: 1\r\nContent-Type: application/json"
                              b"\r\nContent-Length: %d\r\nConnection: close\r\n\r\n" % len(body) + body)
            except OSError:
                pass
            c.close()


def start_lobby(port):
    """npx wrangler dev of server/ on 127.0.0.1:port. The process, or None when it does not answer."""
    server = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "server")
    log = open(os.path.join(tempfile.gettempdir(), f"cm0102-lobby-{port}.log"), "w")
    p = subprocess.Popen(["npx", "wrangler", "dev", "--port", str(port), "--ip", "127.0.0.1"], cwd=server,
                         stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    for _ in range(90):
        try:
            if json.load(urllib.request.urlopen(f"http://127.0.0.1:{port}/v1/health", timeout=2)).get("ok"):
                return p
        except OSError:
            pass
        time.sleep(1)
    os.killpg(p.pid, 9)
    return None


def keep_awake(url, stop):
    """The local Workers runtime drops a Durable Object after about 10 s with no request, and the lobby keeps its
    games in memory only (server/README.md), so a heartbeat every 30 s finds its game gone. A list request every
    3 s keeps the local lobby awake, as other players' requests keep the real one."""
    while not stop.wait(3):
        lobby_games(url)


def lobby_games(url):
    try:
        return json.load(urllib.request.urlopen(url + "/v1/games", timeout=5))["games"]
    except (OSError, ValueError, KeyError):
        return None


def list_rows(stderr_path):
    """The rows that the runtime gave the game list, from a game's stderr."""
    if not os.path.exists(stderr_path):
        return []
    return re.findall(r'recomp: lobby: list row "([^"]*)" from', open(stderr_path, errors="replace").read())


def lan_address():
    """The address of the default route (no packet is sent)."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("192.0.2.1", 9))
        return s.getsockname()[0]
    finally:
        s.close()


DISPLAYS = {"host": "classic", "join": "classic"}   # --display: the host's display and the joiners' (item 212)


def is_local(address):
    """1 when address is an address of this computer: a socket can bind to it."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.bind((address, 0))
        return True
    except OSError:
        return False
    finally:
        s.close()


class AddressWatch:
    """Item 216: the games of a run meet on this computer's LAN address (Q32). When the network drops (Wi-Fi off for
    a moment, a new DHCP lease), the system removes that address and both games lose their connection, a failure
    of the machine and not of the game (10 November 2026: "en0 link INACTIVE", "DHCP en0: removing 192.168.0.82" at
    02:49:18, run-logs/i212/main-checks/drag). The watch tests every half second that the address is still this
    computer's, so a drop that brings the same address back is seen too. end() gives the reason, or "" ."""

    def __init__(self, address):
        self.address, self.lost, self.stop = address, None, threading.Event()
        threading.Thread(target=self.watch, daemon=True).start()

    def watch(self):
        while not self.stop.wait(0.5):
            if self.lost is None and not is_local(self.address):
                self.lost = time.strftime("%H:%M:%S")

    def end(self):
        self.stop.set()
        if self.lost is None and not is_local(self.address):
            self.lost = time.strftime("%H:%M:%S")
        if self.lost is None:
            return ""
        return f"the test address {self.address} went away during the run (at {self.lost}): the network changed, not the game; run again"


def check_address(check, watch):
    """The run's last check: the address stayed (see AddressWatch)."""
    lost = watch.end()
    check(not lost, lost or f"the test address {watch.address} stayed for the whole run")


def start(program, game, work, name, steps, shots, slow=1, env_extra=None, online=True, args=()):
    user = os.path.join(work, name)
    os.makedirs(user)
    display = DISPLAYS["host" if name == "host" or name.endswith("-host") else "join"]   # errors_run.py: "lost-host"
    with open(os.path.join(user, "settings.txt"), "w") as f:
        f.write(f"display = {display}\n")
        if not online:
            f.write("online_play = off\n")
    steps = smoke_run.scaled_steps(display, shots, steps)
    if slow != 1:   # --slow: every step time and wait limit (120 s by default) times this factor
        steps = [f"{s} {int(120000 * slow)}" if s.startswith("wait") else
                 " ".join([str(int(int(s.split()[0]) * slow))] + s.split()[1:]) for s in steps]
    env = dict(os.environ, HOME=user, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1",
               RECOMP_USER_DIR=user, RECOMP_SCREEN_LOG="1",
               RECOMP_NET_LOG=os.path.join(shots, f"{name}-net.log"), RECOMP_SCRIPT="; ".join(steps))
    env.pop("RECOMP_LOBBY_URL", None)
    env.update(env_extra or {})
    err = open(os.path.join(shots, f"{name}-stderr.txt"), "w", errors="replace")
    return subprocess.Popen([program, "--game", game, *args], env=env, stdout=subprocess.DEVNULL, stderr=err), err


def wait_for(path, text, proc, timeout):
    end = time.time() + timeout
    while time.time() < end and proc.poll() is None:
        with open(path, errors="replace") as f:
            if text in f.read():
                return True
        time.sleep(0.5)
    return False


def connections(log):
    """{peer: last line's fields} of the 'end' and 'close' lines of a traffic log."""
    out = {}
    if not os.path.exists(log):
        return out
    for line in open(log):
        p = line.split()
        if len(p) > 2 and p[1] in ("close", "end"):
            f = dict(x.split("=", 1) for x in p if "=" in x)
            nums = [int(x) for x in p if x.isdigit()]
            # out=B bytes P packets S sends in=B bytes P packets
            f["bytes_out"], f["packets_out"], f["sends"], f["packets_in"] = int(f["out"]), nums[0], nums[1], nums[2]
            f["bytes_in"] = int(f["in"])
            out[f["peer"]] = f
    return out


def click_packets(log, peer):
    """The count of click packets (opcode 6) sent on the connection to peer, from a traffic log."""
    if not os.path.exists(log):
        return None
    handle, count = None, 0
    for line in open(log):
        p = line.split()
        if len(p) > 3 and p[1] in ("close", "end") and f"peer={peer}" in p:
            handle = p[3]
    for line in open(log):
        p = line.split()
        if handle and p[:3] == ["net", "ops", handle] and len(p) > 3 and p[3] == "out":
            count = sum(int(x[2:].split("/")[0]) for x in p[4:] if x.startswith("6="))
    return count if handle else None


def main(argv):
    global GAME_NAME
    args = argv[1:]
    if len(args) < 2:
        print(__doc__)
        return 2
    program, game = os.path.abspath(args[0]), os.path.abspath(args[1])
    address, shots, joiners, slow, lobby, lobby_url, relay = lan_address(), None, 1, 1.0, False, None, False
    remote, remote_wait = 0, 1800
    same_as, online_off = None, False
    rest = args[2:]
    while rest:
        k = rest.pop(0)
        if k in ("--lobby", "--relay"):
            lobby, relay = True, relay or k == "--relay"
            continue
        if k == "--online-off":
            online_off = True
            continue
        if not rest:
            print(__doc__)
            return 2
        v = rest.pop(0)
        if k == "--address":
            address = v
        elif k == "--shots":
            shots = os.path.abspath(v)
        elif k == "--slow":
            slow = float(v)
        elif k == "--joiners":
            joiners = max(1, min(3, int(v)))
        elif k == "--lobby-url":
            lobby, lobby_url = True, v
        elif k == "--remote":
            remote = max(1, min(15, int(v)))
        elif k == "--remote-wait":
            remote_wait = int(v)
        elif k == "--game-name" and re.match(r"[A-Za-z0-9]{1,20}$", v):
            GAME_NAME = v
        elif k == "--display" and re.match(r"(classic|widescreen),(classic|widescreen)$", v):
            DISPLAYS["host"], DISPLAYS["join"] = v.split(",")
        elif k == "--same-as":
            same_as = os.path.abspath(v)
        else:
            print(__doc__)
            return 2
    if online_off and (lobby or relay or remote):
        print("--online-off is for a LAN run, not with --lobby, --relay or --remote")
        return 2
    failed = []

    def check(ok, text):
        print(("ok   " if ok else "FAIL ") + text)
        if not ok:
            failed.append(text)

    watch = AddressWatch(address)           # item 216: the network must keep the run's address
    # The joiners: (name, steps, environment, online play, plays the whole join).
    lobby_proc, quiet, full = None, None, None
    if lobby:
        if not lobby_url:
            port = 8790 + os.getpid() % 100
            lobby_proc = start_lobby(port)
            if not lobby_proc:
                print("FAIL the local lobby (npx wrangler dev in server/) does not answer")
                return 1
            lobby_url = f"http://127.0.0.1:{port}"
        quiet, full = FakeLobby(False), FakeLobby(True)
        other = "0.0.1"
    if remote and not (relay and lobby_url):
        print("--remote needs --relay and --lobby-url")
        return 2
    if remote:
        specs, host_env = [], {"RECOMP_LOBBY_URL": lobby_url}
    elif relay:
        specs = [(f"join{n}", joiner_steps(address, n, by_row=True),
                  {"RECOMP_LOBBY_URL": lobby_url, "RECOMP_TEST_NO_LAN_ROWS": "1"}, True, True)
                 for n in range(1, joiners + 1)]
        host_env = {"RECOMP_LOBBY_URL": lobby_url}
    elif lobby:
        specs = [("join1", joiner_steps(address, 1, by_row=True), {"RECOMP_LOBBY_URL": lobby_url}, True, True),
                 ("join2", refused_steps(2), {"RECOMP_LOBBY_URL": lobby_url, "RECOMP_TEST_NO_LAN_ROWS": "1",
                                              "RECOMP_TEST_VERSION": other}, True, False),
                 ("join3", joiner_steps(address, 3), {"RECOMP_LOBBY_URL": quiet.url}, False, True),
                 ("join4", refused_steps(4), {"RECOMP_LOBBY_URL": full.url, "RECOMP_TEST_NO_LAN_ROWS": "1"},
                  True, False)]
        host_env = {"RECOMP_LOBBY_URL": lobby_url}
    else:
        specs = [(f"join{n}", joiner_steps(address, n), {}, not online_off, True) for n in range(1, joiners + 1)]
        host_env = {}
    players = [x for x in specs if x[4]]
    listed = []

    with tempfile.TemporaryDirectory(prefix="cm0102-network-") as work:
        shots = shots or work
        os.makedirs(shots, exist_ok=True)
        print(f"host address {address}" + (f", lobby {lobby_url}" if lobby else ""))
        stop = threading.Event()
        if lobby:
            threading.Thread(target=keep_awake, args=(lobby_url, stop), daemon=True).start()
        steps = host_steps(remote, remote_wait * 1000) if remote else host_steps(len(specs))
        host, host_err = start(program, game, work, "host", steps, shots, slow, host_env, not online_off)
        ready = wait_for(os.path.join(shots, "host-stderr.txt"), f"screen {NAME_BUILD}", host, 240 * slow)
        check(ready, "the host reaches its Enter Name screen with the network game made")
        procs = []
        try:
            if ready and lobby:
                for _ in range(20):          # the host registers once the data is loaded
                    games = lobby_games(lobby_url) or []
                    listed = [g for g in games if g.get("name") == GAME_NAME]
                    if listed:
                        break
                    time.sleep(0.5)
            if ready:
                for n, (name, steps, env, online, _) in enumerate(specs):
                    # The original's loopback test 0x8af290 binds 127.0.0.1:6667 for a moment in each game. Two
                    # games on one computer that run it at the same time collide, and one turns network play off.
                    if n:
                        time.sleep(4 * slow)
                    p, err = start(program, game, work, name, steps, shots, slow, env, online)
                    procs.append((name, p, err))
            for _, p, _ in procs:
                try:
                    p.wait(timeout=180 * slow)
                except subprocess.TimeoutExpired:
                    pass
            if remote and ready:
                print(f"the host waits {remote_wait} s for {remote} joiners on other computers")
            try:
                host.wait(timeout=240 * slow + (remote_wait if remote else 0))
            except subprocess.TimeoutExpired:
                pass
            after = lobby_games(lobby_url) if lobby else None
        finally:
            for _, p, err in procs + [("host", host, host_err)]:
                if p.poll() is None:
                    p.kill()
                    p.wait()
                err.close()
            stop.set()
            if lobby_proc:
                os.killpg(lobby_proc.pid, 9)

        check(host.returncode == 0, f"host ended with code {host.returncode}")
        for name, p, _ in procs:
            check(p.returncode == 0, f"{name} ended with code {p.returncode}")
        dump = lambda f: os.path.join(shots, f)
        names = [x[0] for x in players]
        for name in names:
            for host_shot, join_shot, what in (("host-1-name.bmp", f"{name}-1-name.bmp", "Enter Name"),
                                                ("host-2-typed.bmp", f"{name}-2-typed.bmp", "Enter Name with the typed names"),
                                                ("host-3-nation.bmp", f"{name}-3-nation.bmp", "Select Nationality after Next")):
                if DISPLAYS["host"] != DISPLAYS["join"]:
                    # The host's screen has another size: the joiner's must equal that of a run whose host had the
                    # joiner's display (--same-as), so the host's display does not change what a joiner sees.
                    ref = os.path.join(same_as, join_shot) if same_as else None
                    missing = [f for f in ([ref] if ref else []) + [dump(join_shot)] if not os.path.exists(f)]
                    if ref and not missing:
                        share = smoke_run.differing_share(ref, dump(join_shot))
                        check(share <= 0.01, f"{name}'s {what} ({DISPLAYS['join']}, host {DISPLAYS['host']}) equals "
                                             f"the same joiner's under a {DISPLAYS['join']} host: {share:.2%} of pixels "
                                             f"differ")
                    else:
                        check(False, f"{name}'s {what}: " + (f"missing {', '.join(missing)}" if ref else
                                                             "no --same-as run to compare with"))
                elif os.path.exists(dump(host_shot)) and os.path.exists(dump(join_shot)):
                    share = smoke_run.differing_share(dump(host_shot), dump(join_shot))
                    check(share <= 0.01, f"{name}'s {what} equals the host's: {share:.2%} of pixels differ")
                else:
                    check(False, f"{name}'s {what}: dump missing")

        host_log = connections(os.path.join(shots, "host-net.log"))
        # A joiner's connection on the host: from its LAN address, or from 0.66.255.<channel> through the relay.
        joiner_peer = (lambda p: p.startswith("0.66.255.")) if relay else (
            lambda p: p.split(":")[0] == address and not p.endswith(":6667"))
        joined = [f for f in host_log.values() if joiner_peer(f["peer"])]
        want = remote or len(names)
        check(len(joined) == want and all(f["packets_in"] and f["packets_out"] for f in joined),
              f"host log: {len(joined)} joiner connections with packets both ways, {want} expected")
        for name in names:
            log = connections(os.path.join(shots, f"{name}-net.log"))
            # Through the relay, the joiner's connection is to the lobby row's address (0.66.x.y, lobby_rows.h).
            peer = next((p for p in log if p.startswith("0.66.") and p.endswith(":6667")), "0.66.?") if relay \
                else f"{address}:6667"
            c = log.get(peer)
            check(bool(c and c["packets_in"] and c["packets_out"]), f"{name} log: the connection to {peer}")
            clicks = click_packets(os.path.join(shots, f"{name}-net.log"), peer)
            check(clicks == JOINER_CLIENT_CLICKS, f"{name} sent {clicks} click packets, {JOINER_CLIENT_CLICKS} expected")

        if relay:
            herr = open(dump("host-stderr.txt"), errors="replace").read()
            check(herr.count("recomp: relay: the host is connected as") == 1,
                  "11 the host held one relay connection")
            for name in names:
                err = open(dump(f"{name}-stderr.txt"), errors="replace").read()
                check("through the relay" in err and "recomp: relay: joined" in err, f"11 {name} joined through the relay")
            for line in re.findall(r"recomp: relay: channel \d+ .*", herr):
                print("     " + line)
        if lobby:
            g = listed[0] if listed else {}
            check(len(listed) == 1 and g.get("version") == project_version() and g.get("database") == "3.9.68" and
                  isinstance(g.get("season"), int) and re.match(r"\d{4}-\d\d-\d\d$", g.get("day", "")) and
                  g.get("managersOnline", 0) >= 1 and g.get("password") is False and 0 <= g.get("freeSeats", -1) <= 15
                  and g.get("hostName", "").startswith("PC-"),
                  f"7 the lobby lists the host's game: {json.dumps(g)}")
        if lobby and not relay:
            rows1 = list_rows(dump("join1-stderr.txt"))
            check(len([r for r in rows1 if r.startswith("Bosman")]) == 1 and "Bosman" in rows1,
                  f"8 join1's list has the host once, as its LAN row: {rows1}")
            rows2 = list_rows(dump("join2-stderr.txt"))
            season = g.get("season", 0)
            # A failed join sends a new REQ (0x81fddc), so the list fills again after the box.
            check(rows2 and len(set(rows2)) == 1 and rows2[0].startswith("Bosman  %d/%02d " % (season, (season + 1) % 100))
                  and rows2[0].endswith(" online"), f"8 join2's list has the lobby row with its details: {rows2}")
            err2 = open(dump("join2-stderr.txt"), errors="replace").read()
            # "join box:" comes from the wrap of confirm_push 0x591840, as the box gets the text.
            check(f"join box: This network game runs Bosman Edition {project_version()}, and you have {other}" in err2,
                  "9 join2's box names the host's version")
            rows4 = list_rows(dump("join4-stderr.txt"))
            err4 = open(dump("join4-stderr.txt"), errors="replace").read()
            check(rows4 and set(rows4) == {" Online games are full today, try again later"} and
                  "join box: Online games are full today" in err4, f"9 join4 sees the full row and its box: {rows4}")
            check(quiet.count == 0, f"9 with Online play off, join3 made {quiet.count} lobby connections")
            check(full.count >= 1, f"join4's lobby got {full.count} requests")
        if lobby:
            check(after is not None and not [x for x in after if x.get("name") == GAME_NAME],
                  "10 the lobby lists no game after the host quit")

        for name in ["host"] + [x[0] for x in specs]:
            path = os.path.join(shots, f"{name}-net.log")
            if os.path.exists(path):
                print(f"--- {name} traffic")
                print(open(path).read().rstrip())
        for f in sorted(os.listdir(shots)) if shutil.which("sips") else []:
            if f.endswith(".bmp"):
                subprocess.run(["sips", "-s", "format", "png", dump(f), "--out", dump(f[:-4] + ".png")],
                               capture_output=True)
    check_address(check, watch)
    print("network run: " + ("PASS" if not failed else f"FAIL ({len(failed)})"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
