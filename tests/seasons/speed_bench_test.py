#!/usr/bin/env python3
"""ctest of tests/seasons/speed_bench.py without a game: the readers of the log, the spread rule, the result rows and
the timed run of a fake program that prints the lines of a game.

Usage:
    python3 -I tests/seasons/speed_bench_test.py
"""
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import speed_bench as sb  # noqa: E402

FAKE = r'''
import sys, time
def say(s):
    sys.stderr.write(s + "\n"); sys.stderr.flush()
mode = sys.argv[1]
if mode == "holiday":
    for day in range(0, 36, 7):
        say("recomp: game date %d/2001 at a human turn" % (day + 194))
        time.sleep(0.4)
    say("recomp: saved /x/samesave.sav after 35 game days (result 1)")
elif mode == "box":
    say('recomp: message box from 0x1234 "Error"')
    time.sleep(30)
elif mode == "hang":
    time.sleep(30)
else:
    say('recomp: progress done "Loading database" 7300 ms, 5 calls, 1 ms in the box')
    say('recomp: progress done "Creating Shortlists" 112600 ms, 9 calls, 2 ms in the box')
sys.exit(3 if mode == "bad" else 0)
'''


def main():
    bad = []

    def check(name, got, want):
        if got != want:
            bad.append(f"{name}: got {got!r}, want {want!r}")

    lines = ['recomp: progress at 10 ms "Loading database" 0 of 5',
             'recomp: progress done "Loading database" 7300 ms, 5 calls, 1 ms in the box',
             'recomp: progress done "Player histories" 20900 ms, 1544 calls, 80 ms in the box',
             'recomp: progress done "Initialising game data" 1700 ms, 3 calls, 0 ms in the box', "noise"]
    check("steps", sb.progress_steps(lines), [("Loading database", 7300), ("Player histories", 20900),
                                              ("Initialising game data", 1700)])
    check("new seconds", sb.new_game_seconds(lines), 29.9)
    check("no steps", sb.new_game_seconds(["noise"]), None)

    d = lambda day: sb.DATE + f"{day}/2001 at a human turn"       # noqa: E731
    timed = [(1.0, "x"), (2.0, d(194)), (50.0, d(201)), (90.0, d(208)), (140.0, d(215)), (190.5, d(222)),
             (230.0, d(229)), (260.0, "recomp: saved")]
    check("holiday seconds", sb.holiday_seconds(timed), (188.5, 28))
    check("holiday too few", sb.holiday_seconds(timed[:5]), (None, 4))
    year = [(0.0, d(360)), (10.0, d(367)), (20.0, d(374)), (30.0, d(381)), (40.0, d(388))]   # over New Year
    check("holiday year end", sb.holiday_seconds(year), (40.0, 28))

    check("spread", round(sb.spread_percent([100.0, 104.0, 102.0]), 6), 4.0)
    check("spread one", sb.spread_percent([5.0]), 0.0)
    check("judge pass", sb.judge([("ok", 100.0), ("ok", 104.9), ("ok", 102.0)], 5)[0], True)
    check("judge spread", sb.judge([("ok", 100.0), ("ok", 105.0)], 5)[0], False)
    check("judge result", sb.judge([("ok", 100.0), ("box", 0.0)], 5)[0], False)

    with tempfile.TemporaryDirectory() as tmp:
        fake = os.path.join(tmp, "fake.py")
        with open(fake, "w") as f:
            f.write(FAKE)
        py = sys.executable

        def run(mode, timeout=20):
            return sb.run_timed([py, "-I", fake, mode], dict(os.environ), os.path.join(tmp, mode + ".log"), timeout)

        code, timed, box = run("holiday")
        secs, got = sb.holiday_seconds(timed)
        check("fake holiday code", code, 0)
        check("fake holiday days", got, 28)
        check("fake holiday span", secs is not None and 1.4 < secs < 3.0, True)
        check("fake holiday saved", any(sb.SAVED.match(line) for _, line in timed), True)
        code, timed, box = run("new")
        check("fake new", sb.new_game_seconds([line for _, line in timed]), 119.9)
        check("fake bad code", run("bad")[0], 3)
        code, _, box = run("box")
        check("fake box", (code, box is not None), ("box", True))
        check("fake timeout", run("hang", 2)[0], "timeout")

        keep = os.path.join(tmp, "keep")
        os.makedirs(keep)
        sb.append_row(keep, {"part": "new", "run": 1, "seconds": "12.3", "result": "ok"})
        sb.append_row(keep, {"part": "new", "run": 2, "seconds": "12.5", "result": "ok"})
        with open(os.path.join(keep, "bench.tsv")) as f:
            rows = [r.split("\t") for r in f.read().splitlines()]
        check("rows", (len(rows), rows[0] == sb.HEAD, rows[2][sb.HEAD.index("seconds")]), (3, True, "12.5"))
        data = os.path.join(tmp, "data")
        with open(data, "wb") as f:
            f.write(b"abc")
        check("sha256", sb.sha256(data), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
    for b in bad:
        print("FAIL", b)
    print("PASS" if not bad else f"FAILED: {len(bad)}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
