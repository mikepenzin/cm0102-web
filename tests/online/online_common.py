"""Shared parts of the 0.7.0 release tests (slice 9 of docs/specs/2026-10-27-online-play-0.7.0-spec.md, #45).

The four release tests (Q29) and the match day measurement use these:
    wait_for_room       the machine rules: no HOLD file, load under 24, fewer than 15 games, no network run of
                        another 0.7.0 worktree (two network runs on one Mac collide on port 6667)
    relay_in_build      whether a build can join through the relay (slice 5)
    lobby_ok            whether a lobby address answers /v1/health as ours
    host_steps          the host steps of tests/smoke/network_run.py with another game name and a longer wait
    put_result          one line of the results file, run-logs/s9-tests/results.tsv
"""
import json
import os
import subprocess
import sys
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(REPO, "tests", "smoke"))
import network_run as nr  # noqa: E402

HOLD = "/Users/max/Projects/cm0102/run-logs/HOLD"
RUN_LOGS = "/Users/max/Projects/cm0102/run-logs/s9-tests"
REAL_LOBBY = "https://online.cm0102bosman.co.uk"
GAME = "/Users/max/Projects/cm0102/clean-3968"
NAME_DUMP = "dump {d}/host-1-name.bmp"


def processes():
    """[(pid, ppid, command)] of every process."""
    out = subprocess.run(["ps", "-axo", "pid=,ppid=,command="], capture_output=True, text=True).stdout
    rows = []
    for line in out.splitlines():
        p = line.split(None, 2)
        if len(p) == 3 and p[0].isdigit() and p[1].isdigit():
            rows.append((int(p[0]), int(p[1]), p[2]))
    return rows


def port_6667_users():
    """PIDs with a TCP socket that listens on port 6667 (a network game's host)."""
    out = subprocess.run(["lsof", "-nP", "-t", "-iTCP:6667", "-sTCP:LISTEN"], capture_output=True,
                         text=True).stdout
    return {int(x) for x in out.split() if x.isdigit()}


def other_network_games(rows=None):
    """Processes that this process did not start and that listen on port 6667 (a network host). Slice 5's note:
    two network runs on one Mac both listen on port 6667 of the same adapters, and the second host fails."""
    rows = rows or processes()
    parent = {pid: ppid for pid, ppid, _ in rows}
    me = os.getpid()

    def mine(pid):
        for _ in range(64):
            if pid == me:
                return True
            if pid not in parent or pid <= 1:
                return False
            pid = parent[pid]
        return False
    # Since 29 October other 0.7.0 games may run beside ours (up to 14), and lead-notes/tools/net-lock.sh keeps
    # network hosts apart; so only a listener on port 6667 counts.
    listeners = port_6667_users()
    return [pid for pid, _, _ in rows if pid in listeners and not mine(pid)]


def room(max_games=15):
    """None when a run may start, else the reason to wait."""
    if os.path.exists(HOLD):
        return "the HOLD file exists"
    load = os.getloadavg()[0]
    if load >= 24:
        return f"the load is {load:.1f}"
    rows = processes()
    games = sum(1 for _, _, c in rows if c.split(" ", 1)[0].endswith("cm0102_recomp"))
    if games >= max_games:
        return f"{games} games run"
    others = other_network_games(rows)
    if others:
        return f"another worktree's network run plays (PID {others[0]})"
    return None


def wait_for_room(max_games=15, quiet=False):
    """Waits until room() gives None. Prints the reason once a minute."""
    n = 0
    while True:
        why = room(max_games)
        if not why:
            return
        if not quiet and n % 2 == 0:
            print(f"waiting: {why}", flush=True)
        n += 1
        time.sleep(30)


NET_LOCK_SH = "the maintainer's notes"


class NetLock:
    """A turn at the lead's lock (the maintainer's notes one 0.7.0 game run at a time, first come, first
    served, at most 2 games). It starts net-lock.sh with a command that touches a file and then waits on its
    input: the turn is ours once the file exists, and closing the input ends the command and frees the lock.
    Use it around one short run: with NetLock("what"): ..."""

    def __init__(self, what):
        self.what = what

    def __enter__(self):
        mark = os.path.join(RUN_LOGS, f"net-lock-turn-{os.getpid()}")
        if os.path.exists(mark):
            os.remove(mark)
        self.proc = subprocess.Popen([NET_LOCK_SH, "sh", "-c", f'touch "{mark}"; cat >/dev/null', self.what],
                                     stdin=subprocess.PIPE)
        while not os.path.exists(mark):
            if self.proc.poll() is not None:
                raise SystemExit("online_common: net-lock.sh ended before our turn")
            time.sleep(5)
        os.remove(mark)
        return self

    def __exit__(self, *exc):
        self.proc.stdin.close()
        self.proc.wait()


def free_port():
    """A TCP port on 127.0.0.1 that is free now, for a local lobby. A port from the process id (8590 + pid % 100)
    clashed with another session's lobby (run matchday8, 2026-10-29)."""
    import socket
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def relay_in_build(program):
    """True when the build has slice 5's relay join (its log text "recomp: relay: joined")."""
    try:
        with open(program, "rb") as f:
            return b"recomp: relay: joined" in f.read()
    except OSError:
        return False


def lobby_ok(url):
    """True when url/v1/health answers {"ok": true, "api": 1} with the header X-Bosman-Api: 1."""
    try:
        r = urllib.request.urlopen(url.rstrip("/") + "/v1/health", timeout=10)
        return r.headers.get("X-Bosman-Api") == "1" and json.load(r).get("ok") is True
    except (OSError, ValueError):
        return False


def host_steps(name="Bosman", wait_ms=60000):
    """The host steps of network_run.py with the game name (one word: the type step reads one word, user32.c) and the time from the host's Enter Name dump to its
    own names (wait_ms, 60 s in network_run for one joiner). The steps after the dump move by the difference."""
    steps = nr.host_steps(1)
    out, shift = [], None
    for s in steps:
        if s == "6500 type Bosman":
            s = f"6500 type {name}"
        if shift is not None:
            t, rest = s.split(" ", 1)
            s = f"{int(t) + shift} {rest}"
        if s.endswith(NAME_DUMP):
            shift = wait_ms - 60000
        out.append(s)
    if " " in name:
        raise SystemExit("online_common: the game name must be one word")
    if shift is None:
        raise SystemExit("online_common: network_run.host_steps has no Enter Name dump")
    return out


def put_result(test, run, result, detail, folder):
    """Prints a result and adds it to run-logs/s9-tests/results.tsv (date, test, run, result, detail, folder)."""
    os.makedirs(RUN_LOGS, exist_ok=True)
    path = os.path.join(RUN_LOGS, "results.tsv")
    new = not os.path.exists(path)
    with open(path, "a") as f:
        if new:
            f.write("time\ttest\trun\tresult\tdetail\tfolder\n")
        f.write("\t".join([time.strftime("%Y-%m-%d %H:%M"), test, run, result, detail.replace("\t", " "),
                           folder]) + "\n")
    print(f"{test} {run}: {result}  {detail}", flush=True)
