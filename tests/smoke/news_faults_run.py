#!/usr/bin/env python3
"""News faults of a long game (issues 161 and 162 of 0.7.3).

Usage:
    python3 tests/smoke/news_faults_run.py <cm0102_recomp> <game folder> <save> <out folder>

<save> is the reported game of 17 November 2222. It stays outside git. The run loads it, opens the News screen and
types in the Filter box, which makes the game write the text of every news item. Checks:
    - no box "search_eng.cpp 881"
    - no item is left with no text
    - the two match items of 31 March 2222 take the shared match text
    - the Nations League item of 14 November 2222 takes the group news
    - the filter changes the list
The pictures stay in <out folder>: news.png (the screen) and filter.png (the list after the filter).
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fixtures"))
import fixtures  # noqa: E402

LOADED = "wait screen:0x717db0"     # the screen that the reported save opens on
STEPS = ["1500 click 45 165",       # the manager's menu
         "3000 click 125 261",      # News
         "wait news",
         "2000 dump {d}/news.bmp",
         "3000 click 550 230",      # the Filter box
         "4000 type Football",
         "6000 key 13",             # Return applies the filter
         "8000 dump {d}/filter.bmp",
         "9500 quit"]


def box_shown(path):
    """The title bar of the box "search_eng.cpp 881": dark blue across the middle of the 800x600 screen."""
    with open(path, "rb") as f:
        data = f.read()
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    stride = (width * 3 + 3) // 4 * 4
    row = offset + (height - 1 - 254) * stride
    blue = sum(data[row + 3 * x] > 90 and data[row + 3 * x + 1] < 40 and data[row + 3 * x + 2] < 40
               for x in range(340, 480))
    return blue > 100


def main(argv):
    if len(argv) != 5:
        print(__doc__)
        return 2
    program, game, save, out = (os.path.abspath(a) for a in argv[1:])
    os.makedirs(out, exist_ok=True)
    failed = []

    def check(ok, text):
        print(("ok   " if ok else "FAIL ") + text)
        if not ok:
            failed.append(text)

    with tempfile.TemporaryDirectory(prefix="cm0102-news-") as work:
        user = os.path.join(work, "user")
        os.makedirs(user)
        os.symlink(save, os.path.join(user, os.path.basename(save)))
        fixtures.write_settings(user, "classic", [])
        load = [LOADED if s == "wait first-news" else s for s in fixtures.LOAD_STEPS]
        env = {"RECOMP_RULES_TABLE": os.path.join(os.path.dirname(program), "seasons", "rules.tsv")}
        r, _ = fixtures.run(program, game, "classic", user, load + [s.replace("{d}", out) for s in STEPS], env, 900)
    with open(os.path.join(out, "stderr.txt"), "w") as f:
        f.write(r.stderr)
    for name in ("news", "filter") if shutil.which("sips") else ():
        subprocess.run(["sips", "-s", "format", "png", os.path.join(out, name + ".bmp"), "--out",
                        os.path.join(out, name + ".png")], capture_output=True)
    log = r.stderr
    pictures = [os.path.join(out, n + ".bmp") for n in ("news", "filter")]
    check(r.returncode == 0, f"the game ended with code {r.returncode}")
    check("rules table: none" not in log, "the game read the rules table")
    check(os.path.exists(pictures[1]) and not box_shown(pictures[1]), "no box from the News filter")
    check("still empty" not in log, "every news item has a text")
    check("competition 437 wrote no text: shared text: " in log, "the Oceania match items have the match text")
    check("(OFC) Semi Final" not in log, "the finals of 31 March are not named semi-finals")
    check("(OFC) Final\n" in log and "Final Final" not in log, "the headline names the final once")
    check("have won the FIFA World Cup qualification" not in log, "a final of the tournament does not win the section")
    check("nations league: group news of fate 7" in log, "the Nations League item has the group news")
    check(all(os.path.exists(p) for p in pictures) and open(pictures[0], "rb").read() != open(pictures[1], "rb").read(),
          "the filter changes the list")
    print("PASS" if not failed else "FAIL")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
