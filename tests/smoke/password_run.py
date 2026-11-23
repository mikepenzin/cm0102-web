#!/usr/bin/env python3
"""The game password (0.7.0 slice 6, Q38): the host sets it on the Network tab, a joiner types it in the password box.

Usage:
    python3 tests/smoke/password_run.py <cm0102_recomp> <game folder> [--shots <folder>] [--slow k]

It uses the timed clicks of tests/smoke/network_run.py.

The host opens the Bosman Edition Options box from the start screen, clicks the Network tab, types "secret" in the
Game password field and closes the box. Then it makes the network game "Bosman" as in the two-game run.

join1 types the host's address and clicks Next. The join check gets "BOSMAN PASSWORD", and the password box opens
in place of "Unable To Join" (net_password.c). It types "wrong" and clicks Join: the join runs again and the box
opens again with "The password is wrong". It types "secret" and clicks Join: it joins, and the host sends its Enter
Name screen.

Checks:
    1 each game ends with code 0
    2 the host's settings.txt holds game_password = secret
    3 join1's box opens, then opens again for the wrong password, and the host refused two joins for the password
    4 join1's Enter Name after the right password equals the host's own (at most 1% of pixels differ)
"""
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import network_run as nr  # noqa: E402
import dropin_run as dr  # noqa: E402

FIELD = "click 445 291"            # the field of the password box
OFF_FIELD = "click 300 240"        # the text line above it: ends the text entry
JOIN = "click 565 399"             # Join


def host_steps():
    start = nr.START[:-1]          # up to the start screen, before Network Play
    return start + [
        "1500 click 277 425",      # Bosman Edition Options (menu B: left cell of the fourth row)
        "wait options",
        f"1500 {dr.NETWORK_TAB}",
        "3000 dump {d}/host-0-network.bmp",
        "3500 click 420 227",      # Game password
        "4500 type secret",
        "6000 click 200 227",      # the row's label: ends the text entry
        "7500 dump {d}/host-0-typed.bmp",
        "8000 click 444 524",      # Close
        "wait setup",
        "1000 click 277 346",      # Network Play
    ] + nr.host_steps(4)[len(nr.START):]


def join_steps(address):
    return nr.START + [
        "3500 click 612 243",      # Join Network Game
        "wait screen:0x81f8d0",
        f"3500 type {address}",
        "6000 click 705 572",      # Next ends the text entry
        "7500 click 705 572",      # Next: the host answers PASSWORD
        "11000 dump {d}/join1-1-box.bmp",
        f"11500 {FIELD}",
        "12000 type wrong",
        f"13500 {OFF_FIELD}",
        f"14500 {JOIN}",
        "18500 dump {d}/join1-2-wrong.bmp",
        f"19000 {FIELD}",
        "19500 type secret",
        f"21000 {OFF_FIELD}",
        f"22000 {JOIN}",
        "27000 dump {d}/join1-3-name.bmp",
        "28000 quit",
    ]


def main(argv):
    args = argv[1:]
    if len(args) < 2:
        print(__doc__)
        return 2
    program, game = os.path.abspath(args[0]), os.path.abspath(args[1])
    shots, slow = None, 1.0
    rest = args[2:]
    while len(rest) >= 2:
        k, v = rest.pop(0), rest.pop(0)
        if k == "--shots":
            shots = os.path.abspath(v)
        elif k == "--slow":
            slow = float(v)
    failed = []

    def check(ok, what):
        print(("ok   " if ok else "FAIL ") + what)
        if not ok:
            failed.append(what)

    address = nr.lan_address()
    with tempfile.TemporaryDirectory(prefix="cm0102-password-") as work:
        shots = shots or work
        os.makedirs(shots, exist_ok=True)
        host, host_err = dr.start(program, game, work, "host", host_steps(), shots, slow, {})
        procs = []
        try:
            ready = nr.wait_for(os.path.join(shots, "host-stderr.txt"), f"screen {nr.NAME_BUILD}", host, 300 * slow)
            check(ready, "the host reaches its Enter Name screen with the network game made")
            if ready:
                p, e = dr.start(program, game, work, "join1", join_steps(address), shots, slow, {})
                procs.append(("join1", p, e))
                p.wait(timeout=240 * slow)
            host.wait(timeout=300 * slow)
        except subprocess.TimeoutExpired:
            pass
        finally:
            for _, p, err in procs + [("host", host, host_err)]:
                if p.poll() is None:
                    p.kill()
                    p.wait()
                err.close()
        check(host.returncode == 0, f"1 host ended with code {host.returncode}")
        for name, p, _ in procs:
            check(p.returncode == 0, f"1 {name} ended with code {p.returncode}")
        settings = dr.text(os.path.join(work, "host", "settings.txt"))
        check("\ngame_password = secret\n" in settings, "2 the host's settings.txt holds game_password = secret")
        h, j = dr.text(os.path.join(shots, "host-stderr.txt")), dr.text(os.path.join(shots, "join1-stderr.txt"))
        first = j.find("recomp: password box built\n")
        again = j.find("recomp: password box built (wrong password)")
        check(0 <= first < again and h.count("a joiner was refused: a wrong password") == 2,
              "3 join1's box opens, then again for the wrong password; the host refused two joins")
        a, b = os.path.join(shots, "host-1-name.bmp"), os.path.join(shots, "join1-3-name.bmp")
        if os.path.exists(a) and os.path.exists(b):
            share = nr.smoke_run.differing_share(a, b)
            check(share <= 0.01, f"4 join1's Enter Name after the right password equals the host's: {share:.2%} differ")
        else:
            check(False, "4 join1's Enter Name: dump missing")
        for f in sorted(os.listdir(shots)):
            if f.endswith(".bmp"):
                subprocess.run(["sips", "-s", "format", "png", os.path.join(shots, f), "--out",
                                os.path.join(shots, f[:-4] + ".png")], capture_output=True)
    print("password run: " + ("PASS" if not failed else f"FAIL ({len(failed)})"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
