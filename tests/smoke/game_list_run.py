#!/usr/bin/env python3
"""Game list run (0.7.0 slice 4a, Q31, Q33): the game list of a joining game holds 50 games, and a join of the
last game reaches the right host.

Usage (as root, in a Linux container with network namespaces, see tests/smoke/game_list_wine.sh):
    python3 tests/smoke/game_list_run.py <cm0102_recomp> <game folder> [--shots <folder>] [--slow k]

The original keeps a game for each sender address of a "CM3 LIST SERVER ACK" (Network::receive_packet 0x77af20,
the address from format_ipv4_address at 0x77b10d, a second ACK from the same address dropped at 0x77b145..0x77b1b1).
So 50 games need 50 sender addresses. The run makes a network namespace "fake" with a link to the joiner's
namespace: the joiner has 10.50.0.1/24, the fake host has 10.50.0.101 to 10.50.0.150, one for each game. A Mac
cannot do this without root, so the run uses the Windows build under wine, as network_wine.sh does.

The fake host (this file with --fake-host) answers the joiner's "CM3 LIST SERVER REQ" with one ACK from each
address, in a fixed shuffled order, in the 119-byte layout (research note, section 2): the tag of 19 bytes, the
host name in bytes 19..68 and the game name "Game NN" in bytes 69..118. Game NN comes from 10.50.0.(100 + NN).
When the joiner's dump of the picked row exists, it sends a "CM3 LIST SERVER BYE" from five of the addresses.
It takes the joiner's TCP connection on port 6667 and writes the address that the joiner connected to.

The joiner: Network Play, Join Network Game, the game list (0x81f8d0). The list shows 6 rows a page with the
area's scrollbar (Area::layout 0x403350). Each click on the scrollbar's down arrow moves the list one row.

Checks:
    1 the joiner ends with code 0
    2 with 50 games, the list moves on click 44 of the down arrow and not on click 45: the list has 50 rows
    3 after the 5 BYEs (45 games), the list moves on click 39 and not on click 40, and on the last page the
      row above Game 50 is no longer Game 49 (a BYE) while the last row stays Game 50
    4 the joiner picks the last row (Game 50) and clicks Next: the fake host gets the TCP connection on
      10.50.0.150, the address of Game 50
"""
import os
import random
import select
import socket
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import network_run  # noqa: E402
import smoke_run  # noqa: E402

GAMES = 50
NET = "10.50.0."
JOINER = NET + "1"
FAKE_FIRST = 100                     # Game NN comes from 10.50.0.(FAKE_FIRST + NN)
GONE = (7, 13, 22, 38, 49)           # the games that send a BYE
PORT = 6667
TAG_ACK, TAG_BYE, TAG_REQ = b"CM3 LIST SERVER ACK", b"CM3 LIST SERVER BYE", b"CM3 LIST SERVER REQ"

# The game list (setup_build_select_network_game_screen 0x81f8d0): the list area is x 190..700, y 234..510,
# 6 rows a page (0x81fa9f..0x81fad4). The box of the rows, without the scrollbar, for the dump compares.
LIST_BOX = (192, 236, 660, 508)
ROW_5, ROW_6 = (192, 422, 660, 460), (192, 468, 660, 506)    # the fifth and sixth rows of a page
DOWN_ARROW = (690, 500)              # the scrollbar's down arrow
LAST_ROW = (420, 485)                # the sixth row of the page
NEXT = (705, 572)
STEP = 500                           # ms between two clicks of the arrow
PICKED = "list-picked.bmp"           # the dump after the pick; its file starts the BYEs


def packet(tag, host, name):
    """The 119-byte ACK or BYE: the tag with no zero, 50 bytes of host name, 50 bytes of game name."""
    pad = lambda t: t.encode()[:49].ljust(50, b"\0")
    return tag + pad(host) + pad(name)


def fake_host(out, slow):
    """Runs in the namespace "fake" until the run ends it, or after 15 minutes times slow."""
    order = list(range(1, GAMES + 1))
    random.Random(10).shuffle(order)   # Game 50 comes 46th: the original's 9 lack it
    socks = {}
    for n in order:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.bind((NET + str(FAKE_FIRST + n), 0))
        socks[n] = s
    listen = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    listen.bind(("", PORT))
    tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    tcp.bind(("", PORT))
    tcp.listen(4)
    log = open(os.path.join(out, "fake-host.log"), "w", buffering=1)
    picked = os.path.join(out, PICKED)
    joiner = None
    byes_sent = False
    held = []
    end = time.time() + 900 * slow
    while time.time() < end:
        r, _, _ = select.select([listen, tcp], [], [], 0.5)
        if listen in r:
            data, sender = listen.recvfrom(2000)
            if data.startswith(TAG_REQ):
                joiner = sender
                log.write(f"req from {sender[0]}:{sender[1]}\n")
                for n in order:
                    socks[n].sendto(packet(TAG_ACK, f"fake{n}", f"Game {n:02d}"), sender)
                log.write(f"acks {len(order)}\n")
        if tcp in r:
            conn, peer = tcp.accept()
            local = conn.getsockname()[0]
            log.write(f"connect from {peer[0]} to {local}\n")
            with open(os.path.join(out, "fake-connect.txt"), "w") as f:
                f.write(local + "\n")
            held.append(conn)       # held open until the joiner quits
        if joiner and not byes_sent and os.path.exists(picked):
            time.sleep(1)           # the dump is written
            for n in GONE:
                # The BYE goes to the broadcast address, as Network::~Network 0x77a890 sends it.
                socks[n].setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
                socks[n].sendto(packet(TAG_BYE, f"fake{n}", ""), (NET + "255", PORT))
            byes_sent = True
            log.write(f"byes {len(GONE)}\n")
    log.close()


def scroll(clicks, first_dump, last_dump, name):
    """Clicks of the down arrow, with a dump of the list after each click from first_dump to last_dump."""
    steps = []
    for i in range(1, clicks + 1):
        steps.append(f"{STEP} click {DOWN_ARROW[0]} {DOWN_ARROW[1]}")
        if first_dump <= i <= last_dump:
            steps.append(f"{STEP // 2} dump {{d}}/{name}-{i}.bmp")
    return steps


def relative(steps):
    """The step times as times since the last wait (the script's times run from the last wait)."""
    out, t = [], 0
    for s in steps:
        head, rest = s.split(" ", 1)
        if head == "wait":
            out.append(s)
            t = 0
            continue
        t += int(head)
        out.append(f"{t} {rest}")
    return out


def joiner_steps():
    before = GAMES - 6          # clicks that move the list: rows below the first page
    after = GAMES - len(GONE) - 6
    return network_run.START + relative([
        "3500 click 612 243",               # Join Network Game
        "wait screen:0x81f8d0",            # the game list
        "6000 dump {d}/list-0.bmp",         # all ACKs are in: the list's first page
        *scroll(before + 1, before - 1, before + 1, "list-a"),
        f"{STEP} click {LAST_ROW[0]} {LAST_ROW[1]}",   # the last row: Game 50 into the Join field
        f"{STEP} dump {{d}}/{PICKED}",      # the fake host sends the BYEs
        "5000 dump {d}/list-b-0.bmp",       # the list after the BYEs, rebuilt at its first page
        *scroll(after + 1, after - 1, after + 1, "list-b"),
        f"{STEP} click {LAST_ROW[0]} {LAST_ROW[1]}",   # the last row: Game 50
        f"{STEP} dump {{d}}/list-c-picked.bmp",
        f"{STEP} click {NEXT[0]} {NEXT[1]}",           # Next: join Game 50
        "6000 dump {d}/joined.bmp",
        "2000 quit",
    ])


def setup_namespace():
    """The namespace "fake" with the 50 addresses, linked to this namespace's 10.50.0.1/24."""
    def ip(*args):
        subprocess.run(["ip", *args], check=True)
    subprocess.run(["ip", "netns", "del", "fake"], capture_output=True)
    subprocess.run(["ip", "link", "del", "veth-join"], capture_output=True)
    ip("netns", "add", "fake")
    ip("link", "add", "veth-join", "type", "veth", "peer", "name", "veth-fake")
    ip("link", "set", "veth-fake", "netns", "fake")
    ip("addr", "add", JOINER + "/24", "broadcast", NET + "255", "dev", "veth-join")
    ip("link", "set", "veth-join", "up")
    for n in range(1, GAMES + 1):
        ip("netns", "exec", "fake", "ip", "addr", "add", f"{NET}{FAKE_FIRST + n}/24", "broadcast", NET + "255",
           "dev", "veth-fake")
    ip("netns", "exec", "fake", "ip", "link", "set", "veth-fake", "up")
    ip("netns", "exec", "fake", "ip", "link", "set", "lo", "up")


def moved(shots, name, n):
    """Whether the list differs between the dumps after clicks n - 1 and n."""
    a, b = os.path.join(shots, f"{name}-{n - 1}.bmp"), os.path.join(shots, f"{name}-{n}.bmp")
    if not (os.path.exists(a) and os.path.exists(b)):
        return None
    return smoke_run.differing_share(a, b, LIST_BOX) > 0


def main(argv):
    args = argv[1:]
    if args[:1] == ["--fake-host"]:
        fake_host(args[1], float(args[2]))
        return 0
    if len(args) < 2:
        print(__doc__)
        return 2
    program, game = os.path.abspath(args[0]), os.path.abspath(args[1])
    shots, slow = None, 1.0
    rest = args[2:]
    while len(rest) >= 2:
        if rest[0] == "--shots":
            shots = os.path.abspath(rest[1])
        elif rest[0] == "--slow":
            slow = float(rest[1])
        rest = rest[2:]
    failed = []

    def check(ok, text):
        print(("ok   " if ok else "FAIL ") + text)
        if not ok:
            failed.append(text)

    with tempfile.TemporaryDirectory(prefix="cm0102-gamelist-") as work:
        shots = shots or work
        os.makedirs(shots, exist_ok=True)
        for f in os.listdir(shots):     # files of an earlier run would pass a check or start the BYEs
            if f.endswith((".bmp", ".png", ".txt", ".log")):
                os.remove(os.path.join(shots, f))
        setup_namespace()
        fake = subprocess.Popen(["ip", "netns", "exec", "fake", sys.executable, "-I", os.path.abspath(__file__),
                                 "--fake-host", shots, str(slow)])
        time.sleep(1)
        join, err = network_run.start(program, game, work, "join", joiner_steps(), shots, slow)
        try:
            join.wait(timeout=900 * slow)
        except subprocess.TimeoutExpired:
            join.kill()
            join.wait()
        err.close()
        fake.kill()
        fake.wait()

        check(join.returncode == 0, f"joiner ended with code {join.returncode}")
        before, after = GAMES - 6, GAMES - len(GONE) - 6
        for name, n, games in (("list-a", before, GAMES), ("list-b", after, GAMES - len(GONE))):
            last, end = moved(shots, name, n), moved(shots, name, n + 1)
            check(last is True and end is False,
                  f"{games} games: the list moves on click {n} ({last}) and not on click {n + 1} ({end})")
        a, b = os.path.join(shots, f"list-a-{before}.bmp"), os.path.join(shots, f"list-b-{after}.bmp")
        if os.path.exists(a) and os.path.exists(b):
            five, six = smoke_run.differing_share(a, b, ROW_5), smoke_run.differing_share(a, b, ROW_6)
            check(five > 0 and six == 0, f"the last page after the BYEs: row 5 changes ({five:.1%}), row 6 stays "
                  f"Game {GAMES} ({six:.1%})")
        else:
            check(False, "the last pages before and after the BYEs: dump missing")
        path = os.path.join(shots, "fake-connect.txt")
        got = open(path).read().strip() if os.path.exists(path) else None
        want = NET + str(FAKE_FIRST + GAMES)
        check(got == want, f"the join of the last row reaches {want} (Game {GAMES}): {got}")
        for f in ("fake-host.log", "join-net.log"):
            p = os.path.join(shots, f)
            if os.path.exists(p):
                print(f"--- {f}")
                print(open(p).read().rstrip())
    print("game list run: " + ("PASS" if not failed else f"FAIL ({len(failed)})"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
