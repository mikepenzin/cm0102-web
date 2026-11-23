#!/usr/bin/env python3
"""Two joiners click Finish at the same moment (0.7.0, slice 9's quit leg of 2026-10-29: the host stayed on Manager
Status after its Finish, with one joiner shown "Playing").

Usage:
    python3 tests/smoke/finish_run.py <cm0102_recomp> <game folder> [--shots <folder>] [--slow k] [--runs n]
        [--delta ms]

--delta ms: each joiner's Finish comes this much after the one before (in step time, scaled by --slow).
--joiners 3: join3 joins too, 40 s after join2, with the fourth club (slice 9's quit leg has three joiners).

The host makes the network game "Bosman" and its manager with Arsenal, and waits on Manager Status. join1 and join2
join, make their managers at the second and third club of Select Team, and click Finish on Manager Status at the same
moment: join2 starts 40 s after join1 (so the host makes the managers one after the other) and clicks 40 s earlier
by its own clock. Then the host clicks Finish. Each
Finish marks its channel done (the input 0x820950 returns -5 at 0x820d06, and the screen loop sets the done flag at
0x7fd25a..0x7fd27b); the screen loop ScreenManager::monitor 0x7fc7e0 ends when every channel is done (0x7fd525..
0x7fd555), and the game starts.

Checks, for each run:
    1 the host reaches its first news (both joiners' Finish took effect) and ends with code 0
"""
import os
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import network_run as nr  # noqa: E402
import dropin_run as dr  # noqa: E402

FINISH_MS = 180000                 # join1's Finish by its own clock
JOIN2_AFTER_MS = 40000             # join2 starts this much later, so that the host makes the two managers one after
                                   # the other; it clicks Finish this much earlier by its own clock


def join_steps(n, address, first, second, club_y, finish_ms):
    return nr.START + [
        "3500 click 612 243",               # Join Network Game
        "wait screen:0x81f8d0",
        f"3500 type {address}",
        "6000 click 705 572",               # Next ends the text entry
        "7500 click 705 572",               # Next connects
        "12000 click 560 246",              # First Name
        f"12500 type {first}",
        "14000 click 560 310",              # Second Name
        f"14500 type {second}",
        "16000 click 560 246",              # First Name: ends the entry of Second Name
        "20000 click 705 572",              # Enter Name: Next
        "24500 click 705 572",              # Select Nationality: Next
        f"29000 click 155 {club_y}",        # Select Team
        "33500 click 722 14",               # Take Control
        "38000 click 705 572",              # Please Confirm: Yes
        f"{finish_ms - 2000} dump {{d}}/join{n}-1-status.bmp",
        f"{finish_ms} click 705 572",       # Manager Status: Finish
        f"{finish_ms + 6000} dump {{d}}/join{n}-2-waiting.bmp",
        "400000 quit",
    ]


JOINERS = (("Join", "Two", 187), ("Join", "Three", 211), ("Join", "Four", 235))


def one_run(program, game, shots, slow, address, delta_ms=0, joiners=2):
    """1 when the host reached its first news."""
    with tempfile.TemporaryDirectory(prefix="cm0102-finish-") as work:
        os.makedirs(shots, exist_ok=True)
        host, host_err = dr.start(program, game, work, "host", dr.host_steps(220000 + 40000 * joiners), shots, slow,
                                  {})
        procs = []
        try:
            if nr.wait_for(os.path.join(shots, "host-stderr.txt"), f"screen {dr.STATUS_BUILD}", host, 300 * slow):
                for n, (first, second, y) in enumerate(JOINERS[:joiners], 1):
                    if n > 1:
                        time.sleep(JOIN2_AFTER_MS / 1000 * slow)
                    finish = FINISH_MS + 40000 * (joiners - 2) - (JOIN2_AFTER_MS - delta_ms) * (n - 1)
                    p, e = dr.start(program, game, work, f"join{n}",
                                    join_steps(n, address, first, second, y, finish), shots, slow, {})
                    procs.append((p, e))
            host.wait(timeout=900 * slow)
        except subprocess.TimeoutExpired:
            pass
        finally:
            for p, e in procs + [(host, host_err)]:
                if p.poll() is None:
                    p.kill()
                    p.wait()
                e.close()
        ok = host.returncode == 0 and os.path.exists(os.path.join(shots, "host-3-news.bmp"))
        for f in sorted(os.listdir(shots)):
            if f.endswith(".bmp"):
                subprocess.run(["sips", "-s", "format", "png", os.path.join(shots, f), "--out",
                                os.path.join(shots, f[:-4] + ".png")], capture_output=True)
        return ok, host.returncode


def main(argv):
    args = argv[1:]
    if len(args) < 2:
        print(__doc__)
        return 2
    program, game = os.path.abspath(args[0]), os.path.abspath(args[1])
    shots, slow, runs, delta, joiners = os.path.abspath("finish-run"), 1.0, 1, 0, 2
    rest = args[2:]
    while len(rest) >= 2:
        k, v = rest.pop(0), rest.pop(0)
        if k == "--shots":
            shots = os.path.abspath(v)
        elif k == "--slow":
            slow = float(v)
        elif k == "--runs":
            runs = int(v)
        elif k == "--delta":
            delta = int(v)
        elif k == "--joiners":
            joiners = max(2, min(3, int(v)))
    address = nr.lan_address()
    failed = 0
    for i in range(1, runs + 1):
        ok, code = one_run(program, game, os.path.join(shots, f"run{i}"), slow, address, delta, joiners)
        print(("ok   " if ok else "FAIL ") + f"run {i}: the host reaches its first news after both joiners' Finish "
              f"(code {code})")
        failed += not ok
    print("finish run: " + ("PASS" if not failed else f"FAIL ({failed} of {runs})"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
