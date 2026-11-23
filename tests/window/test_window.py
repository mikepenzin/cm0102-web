#!/usr/bin/env python3
"""The window: remember_window, the smallest window, the picture fill and the widescreen hint (0.6.0
picture fill, docs/specs/2026-10-21-picture-fill-spec.md).

Usage: python3 tests/window/test_window.py build/cm0102_recomp /Users/max/Projects/cm0102/clean-3968
Each run uses a new temporary save folder. The runs do not go to full screen: the tests must not change
the screen of the person at the computer."""
import os
import re
import struct
import subprocess
import sys
import tempfile


def run(program, game, user, steps, **extra):
    env = dict(os.environ, RECOMP_USER_DIR=user, RECOMP_SCRIPT="; ".join(steps), RECOMP_NO_BOX="1", **extra)
    r = subprocess.run([program, "--game", game], env=env, capture_output=True, text=True, timeout=120)
    return r.stderr


def opened(err):
    m = re.search(r"window opened (-?\d+) (-?\d+) (\d+) (\d+) (\d)", err)
    return tuple(int(v) for v in m.groups()) if m else None


def smallest(err):
    m = re.findall(r"smallest window (\d+)x(\d+)", err)
    return tuple(int(v) for v in m[-1]) if m else None


def classic(user):
    with open(os.path.join(user, "settings.txt"), "w") as f:
        f.write("display = classic\ndatabase = 3.9.68\n")


def now_line(user):
    """The window line of the display report at quit: output size, picture, drawn size, the way."""
    text = open(os.path.join(user, "display-report.txt")).read()
    m = re.search(r"Now: window (\d+)x(\d+), output (\d+)x(\d+) pixels, picture (\d+)x(\d+) drawn at (\d+)x(\d+) "
                  r"\(scale [\d.]+, ([^)]*)\)", text)
    return (tuple(int(v) for v in m.groups()[:8]), m.group(9)) if m else (None, "")


def fills(sizes):
    """The picture fills the output in one direction and keeps its shape (within a pixel of rounding)."""
    _, _, ow, oh, pw, ph, dw, dh = sizes
    one_way = abs(dw - ow) <= 1 or abs(dh - oh) <= 1
    return one_way and dw <= ow + 1 and dh <= oh + 1 and abs(dw * ph - dh * pw) <= max(pw, ph)


def bmp(path):
    """A 24-bit BMP of the script step shot: width, height and a pixel reader (x, y from the top)."""
    data = open(path, "rb").read()
    start, = struct.unpack_from("<I", data, 10)
    w, h = struct.unpack_from("<ii", data, 18)
    row = (w * 3 + 3) & ~3

    def px(x, y):
        o = start + (h - 1 - y) * row + x * 3 if h > 0 else start + y * row + x * 3
        return data[o:o + 3]
    return w, abs(h), px


def picture_in_bars(path, sizes):
    """The bars at the sides are black and the picture between them is drawn (not black)."""
    w, h, px = bmp(path)
    bar = (sizes[2] - sizes[6]) // 2
    black = b"\0\0\0"
    bars_black = all(px(x, y) == black for x in (0, max(0, bar - 2), w - 1) for y in range(0, h, 37))
    inside = [px(x, y) for x in range(bar + 2, w - bar - 2, 23) for y in range(0, h, 23)]
    drawn = sum(1 for p in inside if p != black) > len(inside) // 2
    return bar > 2 and bars_black and drawn


def main(argv):
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    failures = []

    def check(name, ok):
        print(("ok   " if ok else "FAIL ") + name)
        if not ok:
            failures.append(name)

    # remember_window, in classic mode: its smallest window, 800x600, is below the sizes used here.
    with tempfile.TemporaryDirectory() as user:
        classic(user)
        shot = os.path.join(user, "shot.bmp")
        err = run(program, game, user, ["1000 click 400 300", "2000 resize 1000 700", "12000 shot " + shot, "12500 quit"])
        check("classic: the smallest window is 800x600", smallest(err) == (800, 600))
        saved = open(os.path.join(user, "window.txt")).read().split()
        check("quit stores the resized size", saved[2:4] == ["1000", "700"])
        sizes, way = now_line(user)
        check("classic 1000x700: the picture fills the window, with bars at the sides only",
              sizes is not None and fills(sizes) and abs(sizes[7] - sizes[3]) <= 1)
        check("classic 1000x700: sharp bilinear above a fit of 1", way.startswith("sharp: "))
        check("classic 1000x700: the window shows the picture between black bars", sizes is not None
              and os.path.exists(shot) and picture_in_bars(shot, sizes))
        o = opened(run(program, game, user, ["3000 quit"]))
        check("next start opens at the stored size", o is not None and o[2:4] == (1000, 700))

        with open(os.path.join(user, "window.txt"), "w") as f:
            f.write("99999 99999 1000 700 0\n")
        o = opened(run(program, game, user, ["3000 quit"]))
        check("a position off every screen opens at the picture size", o is not None and o[2:4] == (800, 600))

        with open(os.path.join(user, "window.txt"), "w") as f:
            f.write("100 100 500 400 0\n")
        run(program, game, user, ["3000 quit"])
        saved = open(os.path.join(user, "window.txt")).read().split()
        check("a remembered window below the smallest grows to 800x600", saved[2:4] == ["800", "600"])

        with open(os.path.join(user, "window.txt"), "w") as f:
            f.write("100 100 1000 700 0\n")
        sp = os.path.join(user, "settings.txt")
        text = open(sp).read()
        check("settings file has remember_window = on", "\nremember_window = on\n" in text)
        text = text.replace("\nremember_window = on\n", "\nremember_window = off\n")
        open(sp, "w").write(text.replace("\nsharp_picture = on\n", "\nsharp_picture = off\n"))
        o = opened(run(program, game, user, ["3000 quit"]))
        check("remember_window = off ignores the file", o is not None and o[2:4] == (800, 600))
        sizes, way = now_line(user)
        check("sharp_picture = off: linear", way == "linear" and sizes is not None and fills(sizes))

    # Widescreen: the smallest window is the size of a new window, and the hint on a short screen.
    with tempfile.TemporaryDirectory() as user:
        err = run(program, game, user, ["3000 resize 600 400", "4000 quit"])
        least = smallest(err)
        o = opened(err)
        check("widescreen: the smallest window is the size of a new window", least is not None and o is not None
              and least == o[2:4] and least[0] * 800 in range((least[1] - 1) * 1280, (least[1] + 2) * 1280))
        saved = open(os.path.join(user, "window.txt")).read().split()
        check("a resize below the smallest stops at the smallest", least is not None
              and (int(saved[2]), int(saved[3])) == least)
        sizes, _ = now_line(user)
        check("widescreen at the smallest: the picture fills the window", sizes is not None and fills(sizes))
        check("no hint on a screen 800 pixels high or more", "short screen hint" not in err)

    with tempfile.TemporaryDirectory() as user:
        err = run(program, game, user, ["3000 quit"], RECOMP_TEST_SCREEN_H="768")
        check("widescreen on a 768-pixel screen: the hint shows", "short screen hint" in err)
        hints = os.path.join(user, "hints.txt")
        check("hints.txt keeps that it showed", os.path.exists(hints)
              and "short_screen_hint = shown" in open(hints).read())
        report = open(os.path.join(user, "display-report.txt")).read()
        check("the display report names the short screen", "Screen less than 800 pixels high" in report)
        err = run(program, game, user, ["3000 quit"], RECOMP_TEST_SCREEN_H="768")
        check("the hint shows once", "short screen hint" not in err)
        classic(user)
        err = run(program, game, user, ["3000 quit"], RECOMP_TEST_SCREEN_H="768")
        report = open(os.path.join(user, "display-report.txt")).read()
        check("classic: no hint in the report", "Screen less than 800 pixels high" not in report)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
