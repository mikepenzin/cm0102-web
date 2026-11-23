#!/usr/bin/env python3
"""Release test 1 of the online play spec (Q29, slice 9): two and four games on this Mac play through the lobby and
the relay.

Usage:
    python3 tests/online/release_mac_run.py <cm0102_recomp> [<game folder>] [--server real|local|<address>]
        [--out <folder>]

--server real (the default) is https://online.cm0102bosman.co.uk. local starts a local copy of server/
(`npx wrangler dev`, as tests/smoke/network_run.py --lobby does); use it before a run against the real server.
The game folder defaults to /Users/max/Projects/cm0102/clean-3968. --out defaults to a new folder under
run-logs/s9-tests/.

The runs, each a tests/smoke/network_run.py run in its own folder of --out:
    With the relay in the build (slice 5, the text "recomp: relay: joined" in the program):
        relay-2   --relay --joiners 1: the host and 1 joiner, the joiner sees lobby rows only and joins through
                  the relay
        relay-4   --relay --joiners 3: the host and 3 joiners, the same
    Without it (online-0.7.0 before slice 5), the stage that runs today, always against the local copy, because
    the game's lobby client of that build speaks only http:// (lobby_http.c):
        lobby     --lobby: the host is in the lobby; join1 joins through its row over the LAN; join2 (another
                  version) and join4 (full today) are refused; join3 has Online play off
        lan-4     --joiners 3: the host and 3 joiners over the LAN, no lobby
Each run waits for room first (online_common.wait_for_room: no HOLD file, load under 24, fewer than 15 games, no
network run of another 0.7.0 worktree).

Result (exit code 0, 3, 1):
    PASS     every run passes, the build has the relay and the server is the real one: release test 1 passes
    STAGE    every run passes, but the build has no relay or the server is local: not yet the release test
    FAIL     a run fails (its output is in <out>/<run>.log, its dumps and traffic logs in <out>/<run>/)
"""
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import online_common as oc  # noqa: E402


def main(argv):
    args = argv[1:]
    if not args or args[0].startswith("--"):
        print(__doc__)
        return 2
    program = os.path.abspath(args.pop(0))
    game = os.path.abspath(args.pop(0)) if args and not args[0].startswith("--") else oc.GAME
    server, out = "real", None
    while args:
        k = args.pop(0)
        if not args:
            print(__doc__)
            return 2
        v = args.pop(0)
        if k == "--server":
            server = v
        elif k == "--out":
            out = os.path.abspath(v)
        else:
            print(__doc__)
            return 2
    url = oc.REAL_LOBBY if server == "real" else None if server == "local" else server.rstrip("/")
    if url and not oc.lobby_ok(url):
        print(f"FAIL the lobby {url} does not answer /v1/health")
        return 1
    relay = oc.relay_in_build(program)
    out = out or os.path.join(oc.RUN_LOGS, time.strftime("release1-%Y%m%d-%H%M"))
    os.makedirs(out, exist_ok=True)
    if not relay and url and url.startswith("https://"):
        # Before slice 5 the game's lobby client speaks only http:// (lobby_http.c), so the stage of today runs
        # against the local copy (run release1-20261028-0109: the real server never saw the host's game).
        print(f"the build has no secure connection (slice 5): the lobby stage uses the local copy, not {url}")
        url = None
    where = ["--lobby-url", url] if url else ["--lobby"]
    if relay:
        runs = [("relay-2", ["--relay", "--joiners", "1"] + where),
                ("relay-4", ["--relay", "--joiners", "3"] + where)]
    else:
        runs = [("lobby", where), ("lan-4", ["--joiners", "3"])]
    print(f"build {program}: {'relay' if relay else 'no relay (before slice 5)'}; server {url or 'local copy'}; "
          f"out {out}", flush=True)
    failed = []
    for name, extra in runs:
        oc.wait_for_room()
        t0 = time.time()
        with open(os.path.join(out, name + ".log"), "w") as log:
            code = subprocess.run([sys.executable, os.path.join(oc.REPO, "tests", "smoke", "network_run.py"),
                                   program, game, "--shots", os.path.join(out, name)] + extra,
                                  stdout=log, stderr=subprocess.STDOUT).returncode
        text = open(os.path.join(out, name + ".log"), errors="replace").read()
        fails = [line for line in text.splitlines() if line.startswith("FAIL")]
        result = "PASS" if code == 0 else "FAIL"
        detail = f"{(time.time() - t0) / 60:.1f} min" + ("; " + fails[0][5:] if fails else "")
        oc.put_result("release1", name, result, detail, os.path.join(out, name))
        if code:
            failed.append(name)
    if failed:
        verdict = "FAIL"
    elif relay and server == "real":
        verdict = "PASS"
    else:
        verdict = "STAGE"
    reason = {"PASS": "release test 1 passes",
              "STAGE": "every run passes, but " + ("the build has no relay" if not relay else "the server is not "
                                                   "the real one") + ": not yet the release test",
              "FAIL": "failed: " + ", ".join(failed)}[verdict]
    oc.put_result("release1", "all", verdict, reason, out)
    return {"PASS": 0, "STAGE": 3, "FAIL": 1}[verdict]


if __name__ == "__main__":
    sys.exit(main(sys.argv))
