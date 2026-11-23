#!/usr/bin/env python3
"""Invite links join the game (0.7.0, Q17, #45): a host in the lobby, and a joiner that gets cm0102://join/<code>.

Usage:
    python3 tests/smoke/invite_run.py <cm0102_recomp> <game folder> [--lobby-url URL] [--shots <folder>]
        [--slow k] [--only a,b]

It uses the timed clicks of tests/smoke/network_run.py and a local copy of the lobby (server/, `npx wrangler dev`),
or --lobby-url. The joiner sees lobby rows only (RECOMP_TEST_NO_LAN_ROWS), so it joins through the relay. The
joiner clicks nothing on the way: the runtime walks the original's screens (src/recomp_rt/win/invite.c), Network
Play, Join Network Game, then the game's row and Next. Each case runs one host and one joiner, one case after the
other. Two network runs on one computer collide on port 6667: start it through
/Users/max/Projects/cm0102/lead-notes/tools/net-lock.sh.

Cases:
    link      the joiner starts with the link as its argument (Windows and Linux; the Mac's Apple event goes to the
              same call). It joins, plays Enter Name and Select Nationality, and its screens equal the host's.
    running   the joiner runs already, on the start screen, and a second copy starts with the link: the second
              copy gives the link to the first and ends (host_link_pass), as a click on a link does while the
              game runs. The first copy joins as above.
    password  the host's game has the password "secret": the link opens the password box of slice 6, the joiner
              types the password and joins.
    gone      the link names a game code that is not open: the joiner's "Unable To Join" box says that the game
              is no longer in the list.
    version   the joiner runs another version (RECOMP_TEST_VERSION): the box names both versions.
"""
import os
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import network_run as nr  # noqa: E402
import password_run as pr  # noqa: E402
import smoke_run  # noqa: E402

LIST_SCREEN = "screen 0x81f8d0"         # the game list in the screen log, which the runtime opens
SETTINGS_OK = 8500                      # nr.START: Ok on the settings screen, after its wait
WALK_MS = 5000                          # the walk from the start screen to the join takes about 2 s


def shifted(steps, ms):
    return [f"{int(s.split()[0]) + ms} " + " ".join(s.split()[1:]) for s in steps]


def joiner_steps(n, password=False, later=0):
    """The joiner: the settings screen, then nothing while the runtime walks to the join; then the steps after it.
    The walk can pass the start screen and the game list before a wait for them starts, so the times count from
    the settings screen, the last screen that the script waits for."""
    steps = nr.START[:-2]
    if password:
        return steps + shifted([
            "9000 dump {d}/join%d-1-box.bmp" % n,
            f"9500 {pr.FIELD}",
            "10000 type secret",
            f"11500 {pr.OFF_FIELD}",
            f"12500 {pr.JOIN}",
            "18000 dump {d}/join%d-1-name.bmp" % n,
            "19000 quit",
        ], SETTINGS_OK + WALK_MS + later - 1000 - 9000)
    tail = nr.joiner_steps(None, n, by_row=True)
    return steps + shifted(tail[tail.index("11000 dump {d}/join%d-1-name.bmp" % n):], SETTINGS_OK + WALK_MS + later - 11000)


def lobby_code(url, name, timeout):
    end = time.time() + timeout
    while time.time() < end:
        for g in nr.lobby_games(url) or []:
            if g.get("name") == name:
                return g["code"]
        time.sleep(0.5)
    return None


def main(argv):
    args = argv[1:]
    if len(args) < 2:
        print(__doc__)
        return 2
    program, game = os.path.abspath(args[0]), os.path.abspath(args[1])
    lobby_url, shots, slow, only = None, None, 1.0, None
    rest = args[2:]
    while rest:
        k = rest.pop(0)
        if not rest:
            print(__doc__)
            return 2
        v = rest.pop(0)
        if k == "--lobby-url":
            lobby_url = v
        elif k == "--shots":
            shots = os.path.abspath(v)
        elif k == "--slow":
            slow = float(v)
        elif k == "--only":
            only = v.split(",")
        else:
            print(__doc__)
            return 2
    failed = []

    def check(ok, what):
        print(("ok   " if ok else "FAIL ") + what)
        if not ok:
            failed.append(what)

    lobby_proc = None
    if not lobby_url:
        port = 8690 + os.getpid() % 100
        lobby_proc = nr.start_lobby(port)
        if not lobby_proc:
            print("FAIL the local lobby (npx wrangler dev in server/) does not answer")
            return 1
        lobby_url = f"http://127.0.0.1:{port}"
    env = {"RECOMP_LOBBY_URL": lobby_url, "RECOMP_TEST_NO_LAN_ROWS": "1"}
    shots = shots or tempfile.mkdtemp(prefix="cm0102-invite-")
    print(f"lobby {lobby_url}, shots {shots}")
    every = ("link", "running", "password", "gone", "version")
    if only and set(only) - set(every):
        print(f"unknown case: {sorted(set(only) - set(every))}")
        return 2
    cases = [c for c in every if not only or c in only]
    try:
        for case in cases:
            run(case, program, game, lobby_url, env, os.path.join(shots, case), slow, check)
    finally:
        if lobby_proc:
            os.killpg(lobby_proc.pid, 9)
    print("invite run: " + ("PASS" if not failed else f"FAIL ({len(failed)})"))
    return 1 if failed else 0


def run(case, program, game, lobby_url, env, shots, slow, check):
    os.makedirs(shots, exist_ok=True)
    work = tempfile.mkdtemp(prefix="cm0102-invite-work-")
    name = "Invite" + case.capitalize()
    nr.GAME_NAME = name
    host_steps = pr.host_steps() if case == "password" else nr.host_steps(1)   # they type nr.GAME_NAME
    stop = __import__("threading").Event()
    __import__("threading").Thread(target=nr.keep_awake, args=(lobby_url, stop), daemon=True).start()
    host = join = second = None
    errs = []
    try:
        if case != "gone":
            host, e = nr.start(program, game, work, "host", host_steps, shots, slow, {"RECOMP_LOBBY_URL": lobby_url})
            errs.append(e)
            ready = nr.wait_for(os.path.join(shots, "host-stderr.txt"), f"screen {nr.NAME_BUILD}", host, 300 * slow)
            check(ready, f"{case}: the host reaches its Enter Name screen")
            code = lobby_code(lobby_url, name, 30 * slow) if ready else None
            check(code is not None, f"{case}: the lobby lists the host's game ({code})")
        else:
            code = "ZZZZ2222"
        if code:
            link = f"cm0102://join/{code}"
            # running: the second copy starts about 2 s after the start screen and takes a few seconds itself
            steps = joiner_steps(1, password=case == "password", later=6000 if case == "running" else 0)
            if case in ("gone", "version"):
                steps = nr.START[:-2] + [f"{SETTINGS_OK + WALK_MS} dump {{d}}/join1-1-box.bmp",   # Unable To Join
                                         f"{SETTINGS_OK + WALK_MS + 1000} quit"]
            jenv = dict(env, RECOMP_TEST_VERSION="0.0.1") if case == "version" else env
            join, e = nr.start(program, game, work, "join1", steps, shots, slow, jenv,
                               args=() if case == "running" else (link,))
            errs.append(e)
            if case == "running":
                at_start = nr.wait_for(os.path.join(shots, "join1-stderr.txt"), "screen 0x81bab0", join, 120 * slow)
                check(at_start, "running: the joiner reaches the start screen")
                time.sleep(2)
                user = os.path.join(work, "join1")
                e2 = open(os.path.join(shots, "second-stderr.txt"), "w")
                errs.append(e2)
                second = subprocess.Popen([program, "--game", game, link], stdout=subprocess.DEVNULL, stderr=e2,
                                          env=dict(os.environ, HOME=user, RECOMP_USER_DIR=user, SDL_VIDEODRIVER="dummy",
                                                   SDL_AUDIODRIVER="dummy", RECOMP_NO_BOX="1", **env))
                second.wait(timeout=60)
                check(second.returncode == 0 and "the copy of the program that runs took the link" in
                      open(os.path.join(shots, "second-stderr.txt"), errors="replace").read(),
                      "running: the second copy gives the link to the first and ends")
            join.wait(timeout=240 * slow)
        if host:
            host.wait(timeout=300 * slow)
    except subprocess.TimeoutExpired:
        pass
    finally:
        for p in (join, host, second):
            if p and p.poll() is None:
                p.kill()
                p.wait()
        for e in errs:
            e.close()
        stop.set()
        shutil.rmtree(work, ignore_errors=True)
    jerr = open(os.path.join(shots, "join1-stderr.txt"), errors="replace").read() \
        if os.path.exists(os.path.join(shots, "join1-stderr.txt")) else ""
    dump = lambda f: os.path.join(shots, f)
    if join:
        check(join.returncode == 0, f"{case}: the joiner ended with code {join.returncode}")
    if host:
        check(host.returncode == 0, f"{case}: the host ended with code {host.returncode}")
    check("recomp: invite: the game opens Join Network Game" in jerr and LIST_SCREEN in jerr
          and 'recomp: invite: Join "' in jerr, f"{case}: the runtime opened the game list and pressed Join")
    if case in ("link", "running"):
        check("recomp: relay: joined" in jerr, f"{case}: the joiner joined through the relay")
        for host_shot, join_shot, what in (("host-1-name.bmp", "join1-1-name.bmp", "Enter Name"),
                                            ("host-3-nation.bmp", "join1-3-nation.bmp", "Select Nationality")):
            if os.path.exists(dump(host_shot)) and os.path.exists(dump(join_shot)):
                share = smoke_run.differing_share(dump(host_shot), dump(join_shot))
                check(share <= 0.01, f"{case}: the joiner's {what} equals the host's: {share:.2%} differ")
            else:
                check(False, f"{case}: {what}: dump missing")
        peer = next((p for p in nr.connections(dump("join1-net.log")) if p.startswith("0.66.")), None)
        clicks = nr.click_packets(dump("join1-net.log"), peer) if peer else None
        check(clicks == nr.JOINER_CLIENT_CLICKS, f"{case}: the joiner sent {clicks} click packets")
    elif case == "password":
        check("recomp: password box opened" in jerr and "recomp: password box: the join runs again" in jerr,
              "password: the password box opened and the join ran again")
        check("recomp: relay: joined" in jerr, "password: the joiner joined through the relay")
        if os.path.exists(dump("host-1-name.bmp")) and os.path.exists(dump("join1-1-name.bmp")):
            share = smoke_run.differing_share(dump("host-1-name.bmp"), dump("join1-1-name.bmp"))
            check(share <= 0.01, f"password: the joiner's Enter Name equals the host's: {share:.2%} differ")
        else:
            check(False, "password: Enter Name: dump missing")
    elif case == "gone":
        check("join box: This network game is no longer in the list" in jerr,
              "gone: the box says that the game is no longer in the list")
    else:
        check("join box: This network game runs Bosman Edition" in jerr and "you have 0.0.1" in jerr,
              "version: the box names both versions")


if __name__ == "__main__":
    sys.exit(main(sys.argv))
