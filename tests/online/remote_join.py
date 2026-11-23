#!/usr/bin/env python3
"""The joiner of release test 3 (Q29, slice 9): a game on another computer (a GitHub Windows or Linux machine)
joins a game that this Mac hosts, through the lobby and the relay. .github/workflows/online-release-join.yml runs
it; tests/online/github_join_run.py is the Mac's side.

Usage:
    python3 tests/online/remote_join.py <cm0102_recomp> <game folder> --lobby-url <address> --name <game name>
        [--wait s] [--shots <folder>] [--id <name>] [--slow k]

The joiner waits at most --wait seconds (default 2400) for the lobby to list the game --name. It sees lobby rows
only (RECOMP_TEST_NO_LAN_ROWS). The game list sorts its rows by name with a plain byte compare (0x77b1fc..0x77b222,
research note section 12), so the script clicks the row at the place of --name among the listed names in that order; the host's name starts with "00" so that it is the first
row. A place below the sixth row (the list shows 6 rows, 0x81fad4) is a FAIL before the game starts. Then it plays
the joiner's timed clicks of tests/smoke/network_run.py: Next, the names, Next.

Checks (each line "ok" or "FAIL", the last line "remote join: PASS" or "remote join: FAIL (n)"):
    1 the game ends with code 0
    2 it joined through the relay
    3 its traffic log has the connection to a 0.66 address with packets both ways
    4 it sent 4 click packets (opcode 6)
    5 its three dumps exist; github_join_run.py compares them with the host's
The dumps are named <id>-1-name.bmp, <id>-2-typed.bmp and <id>-3-nation.bmp; result.txt holds the output.
"""
import json
import os
import platform
import sys
import tempfile
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
import network_run as nr  # noqa: E402

ROW_Y0, ROW_H, ROWS = 257, 46, 6          # the first row's click (network_run.FIRST_ROW) and the row height
SUFFIXES = ("1-name", "2-typed", "3-nation")


def row_steps(n, place):
    """network_run.joiner_steps with the click on row place (0 is the first row)."""
    steps = nr.joiner_steps(None, n, by_row=True)
    first = nr.FIRST_ROW
    return [s.replace(first, f"click 445 {ROW_Y0 + ROW_H * place}") if s.endswith(first) else s for s in steps]


def main(argv):
    args = argv[1:]
    if len(args) < 2 or args[0].startswith("--"):
        print(__doc__)
        return 2
    program, game = os.path.abspath(args[0]), os.path.abspath(args[1])
    o = {"lobby": None, "name": None, "wait": "2400", "shots": None, "id": "join-" + platform.system().lower(),
         "slow": "1"}
    rest = args[2:]
    while len(rest) >= 2:
        k, v = rest.pop(0), rest.pop(0)
        key = {"--lobby-url": "lobby", "--name": "name", "--wait": "wait", "--shots": "shots", "--id": "id",
               "--slow": "slow"}.get(k)
        if not key:
            print(__doc__)
            return 2
        o[key] = v
    if rest or not o["lobby"] or not o["name"]:
        print(__doc__)
        return 2
    lobby, me, slow = o["lobby"].rstrip("/"), o["id"], float(o["slow"])
    shots = os.path.abspath(o["shots"] or tempfile.mkdtemp(prefix="cm0102-remote-"))
    os.makedirs(shots, exist_ok=True)
    lines = []

    def say(text):
        print(text, flush=True)
        lines.append(text)

    failed = []

    def check(ok, text):
        say(("ok   " if ok else "FAIL ") + text)
        if not ok:
            failed.append(text)

    def finish():
        say("remote join: " + ("PASS" if not failed else f"FAIL ({len(failed)})"))
        open(os.path.join(shots, "result.txt"), "w").write("\n".join(lines) + "\n")
        return 1 if failed else 0

    end, games = time.time() + int(o["wait"]), []
    while time.time() < end:
        try:
            games = json.load(urllib.request.urlopen(lobby + "/v1/games", timeout=10))["games"]
        except (OSError, ValueError, KeyError):
            games = []
        if any(g.get("name") == o["name"] for g in games):
            break
        time.sleep(5)
    mine = [g for g in games if g.get("name") == o["name"]]
    check(len(mine) == 1, f"the lobby lists the game {o['name']} once")
    if len(mine) != 1:
        return finish()
    say(f"the lobby lists {json.dumps(mine[0])}")
    # The list shows games of every version. A row's text starts with the name; Python sorts str by code point, as
    # the byte compare does for these Latin-1 names.
    names = sorted(g.get("name", "") for g in games)
    place = names.index(o["name"])
    check(place < ROWS, f"the game is row {place + 1} of the list, within the {ROWS} rows the list shows")
    if place >= ROWS:
        return finish()

    with tempfile.TemporaryDirectory(prefix="cm0102-remote-") as work:
        env = {"RECOMP_LOBBY_URL": lobby, "RECOMP_TEST_NO_LAN_ROWS": "1"}
        p, err = nr.start(program, game, work, me, row_steps(1, place), shots, slow, env)
        try:
            p.wait(timeout=300 * slow)
        except Exception:
            p.kill()
            p.wait()
        err.close()
    check(p.returncode == 0, f"1 the game ended with code {p.returncode}")
    text = open(os.path.join(shots, f"{me}-stderr.txt"), errors="replace").read()
    check("recomp: relay: joined" in text, "2 it joined through the relay")
    log_path = os.path.join(shots, f"{me}-net.log")
    log = nr.connections(log_path)
    peer = next((x for x in log if x.startswith("0.66.") and x.endswith(":6667")), None)
    c = log.get(peer) if peer else None
    check(bool(c and c["packets_in"] and c["packets_out"]), f"3 the connection to {peer} has packets both ways")
    clicks = nr.click_packets(log_path, peer) if peer else None
    check(clicks == nr.JOINER_CLIENT_CLICKS, f"4 it sent {clicks} click packets, {nr.JOINER_CLIENT_CLICKS} expected")
    for suffix in SUFFIXES:
        src = os.path.join(shots, f"join1-{suffix}.bmp")
        ok = os.path.exists(src)
        if ok:
            os.replace(src, os.path.join(shots, f"{me}-{suffix}.bmp"))
        check(ok, f"5 the dump {suffix}")
    for line in text.splitlines():
        if "recomp: relay:" in line or "join box:" in line or "list row" in line:
            say("     " + line)
    return finish()


if __name__ == "__main__":
    sys.exit(main(sys.argv))
