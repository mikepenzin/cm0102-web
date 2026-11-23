#!/usr/bin/env python3
"""Match day with two human managers (0.7.0 slice 9): the traffic of a network game that plays days and a match, and
an estimate of the relay's requests on the Cloudflare free plan. Slice 2 measured only the setup screens (research
note, section 11).

Usage:
    python3 tests/online/match_day_run.py <cm0102_recomp> [<game folder>] --keep <folder> [--minutes 25]
        [--via lan|relay] [--lobby-url <address>] [--save <prematch.sav>] [--team-row 1]

The host restores prematch.sav (tests/fixtures: Test Smoke at Arsenal, 16.7.01, a friendly at Basel on 20.7.01) as
a network game and takes its manager back. One joiner joins (through the LAN address, or the relay with --via
relay), clicks Add Manager, makes a manager with the second club of Select Team and clicks Finish; then the host
clicks Finish and takes Holiday until next match, so that the assistant answers the Henry bid that blocks
Continue Game, and the host is back for the friendly. Then for --minutes both
games repeat their inputs: Space (Continue Game), a click at (45, 120) (Continue Game, and on the match screen
Kick Off, Re-start and Continue Game) and a click at (705, 573) (Yes of "Proceed to the match?"), every 6 s. The
joiner's round (every 10 s) is Space, Yes (705, 573) at once, then (45, 120), then the answer to a bid for its
players (Respond at 172, 230, Reject at 502, 573, then the Yes). Each game
dumps its screen once a minute.

The traffic logs (RECOMP_NET_LOG) give the bytes and packets of each 10 s. The run prints:
    - the host's dates and its match screens (0x718e90) with their times
    - bytes and packets each way: in total, per minute, the busiest 10 s, and during the match
    - the relay messages: with the relay, the frames that the relay lines count; on the LAN, a lower bound (one
      message for each 10 s with bytes, each way) and an upper bound (one for each packet, at most 200 in 10 s,
      the 50 ms batches of slice 5)
Result: PASS when both games end with code 0, the host showed its match screen, the date passed 20.7.01 and the
joiner's connection carried bytes both ways; FAIL else. The figures go to the research note by hand.
"""
import os
import re
import shutil
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import online_common as oc  # noqa: E402
import season_net_run as snr  # noqa: E402

nr = oc.nr

MATCH_BUILD = "0x718e90"
FIXTURES = "/Users/max/Projects/cm0102/test-fixtures"
PERIOD = 6000
BATCHES_IN_10S = 200                     # one batch each 50 ms at most


# The joiner's club gets bids for its players, which block its Continue Game, and the host then shows "Waiting
# For Players" (run matchday3, 2026-10-27). The joiner answers each as smoke_run.MATCH_STEPS answers the Henry
# bid: Respond on the news item, Reject, Yes. (502, 573) is also No on "Proceed to the match?", so each round
# clicks Yes (705, 573) before Respond and Reject (run matchday4: No first kept the joiner from its own friendly,
# and the host waited on Latest Scores). The host is on holiday to its match, but on its match day a new bid for
# Henry blocked it (run matchday12), so the host's loop answers bids too.
BID = ["click 172 230", "click 502 573"]          # Respond, Reject (Back on most other screens)


def loop_steps(t0, minutes, who, bids=False):
    steps, period = [], PERIOD + (4000 if bids else 0)
    for i in range(int(minutes * 60000 // period)):
        t = t0 + i * period
        if bids:
            # Space (Continue Game) opens "Proceed to the match?" on the joiner's channel; Yes must come next. A
            # click at (45, 120) while it is open reaches its input 0x727c30 as widget 129, which closes it with
            # no match (-9), so the squad screen came back each round (run matchday7, 2026-10-29, probe of
            # 0x725ca0, 0x725f70 and 0x727c30). Then (45, 120) is Kick Off on the match screen.
            # Run matchday8: with Space first and Yes 1.5 s later the squad screen still came back, so the joiner
            # clicks Continue Game at (45, 120) in place of Space (a guess: the key gives a second input, 129).
            steps += [f"{t} click 45 120", f"{t + 1500} click 705 573", f"{t + 3000} click 45 120",
                      f"{t + 5000} {BID[0]}", f"{t + 7000} {BID[1]}"]
            if i < 40 and i % 4 == 0:      # the screen that Continue Game opens on the joiner
                steps.append(f"{t + 1000} dump {{d}}/{who}-continue-{i:03d}.bmp")
        else:
            steps += [f"{t} press space", f"{t + 1500} click 45 120"]
        steps += [f"{t + period - 2000} click 705 573"]
        if i % 10 == 0:
            steps.append(f"{t + period - 1000} dump {{d}}/{who}-loop-{i // 10:03d}.bmp")
    return steps + [f"{t0 + int(minutes * 60000) + 3000} quit"]


def ticks(path):
    """[(t ms of CLOCK_MONOTONIC, peer, bytes out, packets out, bytes in, packets in)] of the tick lines of a
    traffic log (net_core.c log_line: "net tick t=<ms> h=<n> peer=<a:p> secs=<s> out=<b> bytes <p> packets <s>
    sends in=<b> bytes <p> packets", the counts of the last 10 s)."""
    out = []
    if not os.path.exists(path):
        return out
    for line in open(path):
        p = line.split()
        if len(p) > 2 and p[1] == "tick":
            f = dict(x.split("=", 1) for x in p if "=" in x)
            nums = [int(x) for x in p if x.isdigit()]
            if len(nums) >= 3:
                out.append((int(f["t"]), f["peer"], int(f["out"]), nums[0], int(f["in"]), nums[2]))
    return out


def main(argv):
    if len(argv) < 2 or "--keep" not in argv:
        print(__doc__)
        return 2
    arg = snr.arg
    program = os.path.abspath(argv[1])
    game = os.path.abspath(argv[2]) if len(argv) > 2 and not argv[2].startswith("--") else oc.GAME
    keep = os.path.abspath(arg(argv, "--keep"))
    minutes = float(arg(argv, "--minutes", 25))
    relay = oc.relay_in_build(program)
    o = {"program": program, "game": game, "nations": "England", "seed": "1", "slow": 1.0,
         "via": arg(argv, "--via", "relay" if relay else "lan"), "address": nr.lan_address()}
    if o["via"] == "relay" and not relay:
        print("match_day_run.py: the build has no relay (slice 5)")
        return 2
    if os.path.exists(keep):
        print(f"match_day_run.py: {keep} exists")
        return 2
    shots = os.path.join(keep, "shots")
    os.makedirs(shots)
    save = arg(argv, "--save", os.path.join(FIXTURES, "prematch.sav"))
    os.makedirs(os.path.join(keep, "host"))
    shutil.copyfile(save, os.path.join(keep, "host", "prematch.sav"))
    lobby_proc, stop = None, threading.Event()
    o["lobby"] = arg(argv, "--lobby-url")
    if not o["lobby"]:
        port = oc.free_port()
        lobby_proc = nr.start_lobby(port)
        if not lobby_proc:
            print("FAIL the local lobby (npx wrangler dev in server/) does not answer")
            return 1
        o["lobby"] = f"http://127.0.0.1:{port}"
        threading.Thread(target=nr.keep_awake, args=(o["lobby"], stop), daemon=True).start()
    failed = []

    def check(ok, what):
        print(("ok   " if ok else "FAIL ") + what, flush=True)
        if not ok:
            failed.append(what)

    finish = 90000
    host = snr.host_restore() + [
        f"{finish} click 705 572",          # Manager Status: Finish
        "wait news 600000",
    ] + snr.at(3000, snr.HOLIDAY_PRE) + [
        # Holiday until next match: the assistant answers the Henry bid, which blocks Continue Game (the bid
        # steps of smoke_run.MATCH_STEPS missed in a network game: the joiner's news item moves the list).
        "wait popup 10000",
        "1500 click 150 447",               # Holiday until next match
        "4000 click 705 572",               # Please Confirm: Yes
        "6000 dump {d}/host-holiday.bmp",
    ] + loop_steps(10000, minutes, "host", bids=True)     # back from holiday it must answer a new Henry bid
    settings = {"display": "classic", "faster_days": "on"}
    oc.wait_for_room()
    t_start = time.time()
    mono0 = time.clock_gettime(time.CLOCK_MONOTONIC) * 1000      # the clock of the traffic log
    hp, herr = snr.start(o, keep, "host", host, shots, {"RECOMP_FIXED_CLOCK": "1"}, settings)
    herr_path = os.path.join(shots, "host-stderr.txt")
    jp = jerr = None
    try:
        ready = snr.wait_text(herr_path, f"screen {snr.STATUS}", hp, 600)
        check(ready, "the host restores prematch.sav and shows Manager Status")
        if ready:
            time.sleep(15)
            env = {"RECOMP_TEST_NO_LAN_ROWS": "1"} if o["via"] == "relay" else {}
            # The joiner's steps count from its game list, about 15 s after its start: Finish 47 s after that
            # (join_new's earliest time with Add Manager), about 77 s after the host's Manager Status, before the
            # host's Finish at 90 s; then the loop.
            # --team-row: the club of Select Team (1, Barnsley: its pick held an unavailable player, matchday11).
            steps = snr.join_new(o, 1, int(arg(argv, "--team-row", 1)), 60000 - 30000, 10 ** 9, add=True)
            steps = [s for s in steps if not s.endswith(" quit") and "-4-game" not in s]
            # The joiner's new manager has no team picked, so on the day of its own friendly Continue Game showed
            # "Team Selection: You have not selected any outfield players" with Ok, and the match never started
            # (run matchday10, 2026-10-29). Its squad screen's Assistant > Pick best team picks it (best_xi_run.py).
            steps += [
                f"{finish + 5000} click 45 165",            # the manager's menu
                f"{finish + 7500} click 150 156",           # the club's squad
                f"{finish + 11000} click 592 135",          # Assistant
                f"{finish + 13000} click 540 159",          # Pick best team
                f"{finish + 16000} dump {{d}}/join1-picked.bmp",
                f"{finish + 17000} click 358 573",          # Back
            ]
            steps += loop_steps(finish + 30000, minutes, "join1", bids=True)
            jp, jerr = snr.start(o, keep, "join1", steps, shots, env, {"display": "classic"})
        for p in (jp, hp):
            if p:
                try:
                    p.wait(timeout=minutes * 60 + 900)
                except subprocess.TimeoutExpired:
                    p.kill()
                    p.wait()
    finally:
        stop.set()
        for p, e in ((jp, jerr), (hp, herr)):
            if p and p.poll() is None:
                p.kill()
                p.wait()
            if e:
                e.close()
        if lobby_proc:
            os.killpg(lobby_proc.pid, 9)
    h = snr.text(herr_path)
    check(hp.returncode == 0, f"the host ended with code {hp.returncode}")
    check(jp is not None and jp.returncode == 0, f"the joiner ended with code {jp.returncode if jp else None}")
    j = snr.text(os.path.join(shots, "join1-stderr.txt"))
    check("join1-2-status.bmp" in j and "timed out" not in j, "the joiner made its manager and reached Manager "
                                                               "Status")
    states = re.findall(r"state at (\d+) ms, date (\d+)/(\d+): screen (0x[0-9a-f]+)", h)
    matches = [int(ms) // 1000 for ms, _, _, scr in states if scr == MATCH_BUILD]
    days = sorted(set((int(y), int(d)) for _, d, y, _ in states))
    check(bool(matches), f"the host showed its match screen ({len(matches)} times, from {matches[:1]} s to "
                         f"{matches[-1:]} s of its run)")
    check(bool(days) and days[-1] > (2001, 200), f"the date passed 20.7.01 (day 200): last {days[-1:] }")
    for f in sorted(os.listdir(shots)):
        if f.endswith(".bmp"):
            subprocess.run(["sips", "-s", "format", "png", os.path.join(shots, f), "--out",
                            os.path.join(shots, f[:-4] + ".png")], capture_output=True)

    # The traffic of the joiner's connection, as the host sees it.
    log = os.path.join(shots, "host-net.log")
    conns = nr.connections(log)
    joined = [c for c in conns.values() if not c["peer"].endswith(":6667") and not c["peer"].startswith("127.")]
    check(len(joined) == 1 and joined[0]["bytes_in"] and joined[0]["bytes_out"],
          f"the host's log has the joiner's connection with bytes both ways: {len(joined)}")
    peer = joined[0]["peer"] if joined else None
    rows = [r for r in ticks(log) if r[1] == peer]
    secs = int(joined[0]["secs"]) if joined else 0
    if joined:
        c = joined[0]
        print(f"--- traffic of the joiner's connection on the host, {secs} s")
        print(f"     host to joiner: {c['bytes_out']} bytes, {c['packets_out']} packets "
              f"({c['bytes_out'] * 60 / max(secs, 1) / 1024:.1f} KB a minute)")
        print(f"     joiner to host: {c['bytes_in']} bytes, {c['packets_in']} packets")
        busiest = max(rows, key=lambda r: r[2], default=None)
        if busiest:
            print(f"     busiest 10 s: {busiest[2]} bytes, {busiest[3]} packets out at t={busiest[0]}")
        lo = sum(1 for r in rows if r[2]) + sum(1 for r in rows if r[4])
        hi = sum(min(r[3], BATCHES_IN_10S) for r in rows) + sum(min(r[5], BATCHES_IN_10S) for r in rows)
        print(f"     relay messages, estimate from the traffic log ({o['via']}): {lo} to {hi} in {secs} s "
              f"({lo * 3600 / max(secs, 1):.0f} to {hi * 3600 / max(secs, 1):.0f} an hour; 20 messages bill as 1 "
              f"request)")
        if matches:
            # The host's state lines count ms from its start (SDL_GetTicks64), the traffic log the monotonic
            # clock: the host started at mono0. Ticks cover the 10 s before their time.
            m0, m1 = mono0 + matches[0] * 1000, mono0 + matches[-1] * 1000
            during = [r for r in rows if m0 <= r[0] <= m1 + 10000]
            span = (m1 - m0) / 1000 + 10
            print(f"     during the match screens ({matches[0]} to {matches[-1]} s of the host's run, {span:.0f} s): "
                  f"{sum(r[2] for r in during)} bytes and {sum(r[3] for r in during)} packets out, "
                  f"{sum(r[4] for r in during)} bytes and {sum(r[5] for r in during)} packets in")
        print("     each minute (bytes out / in): " + ", ".join(
            f"{sum(r[2] for r in rows if k <= (r[0] - rows[0][0]) // 60000 < k + 1)}/"
            f"{sum(r[4] for r in rows if k <= (r[0] - rows[0][0]) // 60000 < k + 1)}"
            for k in range((rows[-1][0] - rows[0][0]) // 60000 + 1)) if rows else "none")
    for line in h.splitlines() + snr.text(os.path.join(shots, "join1-stderr.txt")).splitlines():
        if "recomp: relay:" in line and ("closed after" in line or "frames" in line):
            print("     " + line)
    print(f"--- host traffic log ({os.path.basename(log)}), opcodes")
    for line in open(log) if os.path.exists(log) else []:
        if line.startswith("net ops"):
            print("     " + line.rstrip())
    print(f"run: {(time.time() - t_start) / 60:.0f} min, folder {keep}")
    print("match day: " + ("PASS" if not failed else f"FAIL ({len(failed)})"))
    oc.put_result("match-day", o["via"], "PASS" if not failed else "FAIL", "; ".join(failed[:2]), keep)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
