#!/usr/bin/env python3
"""A joiner on another computer (0.7.0 slice 5, #45): joins a game that a host on this Mac runs, through the lobby
and the relay of the deployed server. The release test of the spec: GitHub's Windows and Linux machines join a
game that this Mac hosts over the internet (.github/workflows/online-join.yml).

Usage:
    python3 tests/smoke/relay_join_remote.py <cm0102_recomp> <game folder> --lobby-url <address>
        [--name Bosman] [--wait s] [--shots <folder>] [--slow k] [--id name]
    python3 tests/smoke/relay_join_remote.py --compare <host shots> <joiner shots>

The host: python3 tests/smoke/network_run.py <exe> <game> --relay --lobby-url <address> --remote n --shots <dir>.

The joiner waits at most --wait seconds (default 1800) for the lobby to list a game with the name, then plays the
joiner's timed clicks of network_run.py: Network Play, Join Network Game, the first row of the list (the host's
lobby row: the joiner sees lobby rows only), Next, the names, Next. On the public lobby another game can sort
first, so give the host's game a name of its own (network_run.py --game-name, and --name here). Checks:
    1 the game ends with code 0
    2 the first row of its list is the host's game, and it joined through the relay
    3 its traffic log shows the connection to the row's 0.66 address with packets both ways
    4 it sent 4 click packets (opcode 6)
    5 the three dumps exist (Enter Name, Enter Name with the names, Select Nationality)
--compare checks the joiner's three dumps against the host's (network_run.py checks 2, 3 and 5).
"""
import json
import os
import platform
import sys
import tempfile
import time
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import network_run  # noqa: E402
import smoke_run  # noqa: E402

SHOTS = (("host-1-name.bmp", "1-name.bmp", "Enter Name"),
         ("host-2-typed.bmp", "2-typed.bmp", "Enter Name with the typed names"),
         ("host-3-nation.bmp", "3-nation.bmp", "Select Nationality after Next"))


def compare(host_dir, join_dir):
    failed = 0
    for host_shot, suffix, what in SHOTS:
        joins = sorted(f for f in os.listdir(join_dir) if f.endswith("-" + suffix))
        if not joins:
            print(f"FAIL no joiner dump of {what}")
            failed += 1
        for f in joins:
            share = smoke_run.differing_share(os.path.join(host_dir, host_shot), os.path.join(join_dir, f))
            ok = share <= 0.01
            failed += not ok
            print(("ok   " if ok else "FAIL ") + f"{f}: {what} equals the host's: {share:.2%} of pixels differ")
    print("compare: " + ("PASS" if not failed else f"FAIL ({failed})"))
    return 1 if failed else 0


def main(argv):
    args = argv[1:]
    if len(args) == 3 and args[0] == "--compare":
        return compare(args[1], args[2])
    if len(args) < 2:
        print(__doc__)
        return 2
    program, game = os.path.abspath(args[0]), os.path.abspath(args[1])
    lobby, name, wait, shots, slow = None, "Bosman", 1800, None, 1.0
    me = "join-" + platform.system().lower()
    rest = args[2:]
    while len(rest) >= 2:
        k, v = rest.pop(0), rest.pop(0)
        if k == "--lobby-url":
            lobby = v.rstrip("/")
        elif k == "--name":
            name = v
        elif k == "--wait":
            wait = int(v)
        elif k == "--shots":
            shots = os.path.abspath(v)
        elif k == "--slow":
            slow = float(v)
        elif k == "--id":
            me = v
        else:
            print(__doc__)
            return 2
    if not lobby or rest:
        print(__doc__)
        return 2
    failed = []

    def check(ok, text):
        print(("ok   " if ok else "FAIL ") + text)
        if not ok:
            failed.append(text)

    end = time.time() + wait
    game_seen = None
    while time.time() < end and not game_seen:
        try:
            games = json.load(urllib.request.urlopen(lobby + "/v1/games", timeout=10))["games"]
            game_seen = next((g for g in games if g.get("name") == name), None)
        except (OSError, ValueError, KeyError):
            pass
        if not game_seen:
            time.sleep(5)
    if not game_seen:
        print(f"FAIL the lobby listed no game named {name} within {wait} s")
        return 1
    print(f"the lobby lists {json.dumps(game_seen)}")

    with tempfile.TemporaryDirectory(prefix="cm0102-remote-") as work:
        shots = shots or work
        os.makedirs(shots, exist_ok=True)
        # The dumps are named join1-*.bmp by the steps; the files are renamed after the run.
        env = {"RECOMP_LOBBY_URL": lobby, "RECOMP_TEST_NO_LAN_ROWS": "1"}
        p, err = network_run.start(program, game, work, me, network_run.joiner_steps(None, 1, by_row=True), shots,
                                   slow, env)
        try:
            p.wait(timeout=300 * slow)
        except Exception:
            p.kill()
            p.wait()
        err.close()
        check(p.returncode == 0, f"1 the game ended with code {p.returncode}")
        text = open(os.path.join(shots, f"{me}-stderr.txt"), errors="replace").read()
        rows = network_run.list_rows(os.path.join(shots, f"{me}-stderr.txt"))
        check(bool(rows) and rows[0].startswith(name + "  "), f"2 the first row of its list is the host's game: {rows[:3]}")
        check("through the relay" in text and "recomp: relay: joined" in text, "2 it joined through the relay")
        log_path = os.path.join(shots, f"{me}-net.log")
        log = network_run.connections(log_path)
        peer = next((x for x in log if x.startswith("0.66.") and x.endswith(":6667")), None)
        c = log.get(peer) if peer else None
        check(bool(c and c["packets_in"] and c["packets_out"]), f"3 the connection to {peer} has packets both ways")
        clicks = network_run.click_packets(log_path, peer) if peer else None
        check(clicks == network_run.JOINER_CLIENT_CLICKS,
              f"4 it sent {clicks} click packets, {network_run.JOINER_CLIENT_CLICKS} expected")
        for _, suffix, what in SHOTS:
            src = os.path.join(shots, "join1-" + suffix)
            ok = os.path.exists(src)
            if ok:
                os.replace(src, os.path.join(shots, f"{me}-{suffix}"))
            check(ok, f"5 the dump of {what}")
        for line in text.splitlines():
            if "recomp: relay:" in line or "join box:" in line:
                print("     " + line)
    print("remote join: " + ("PASS" if not failed else f"FAIL ({len(failed)})"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
