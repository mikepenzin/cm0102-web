#!/usr/bin/env python3
"""Network screens in both modes (0.7.0 item 212): the four two-game runs of tests/smoke/network_run.py with the host
and the joiner each in Classic mode or Widescreen mode, Online play off (LAN, no lobby).

Usage:
    python3 tests/smoke/network_display_run.py <cm0102_recomp> <game folder> --out <folder> [--slow k]

A host in Widescreen mode sent its widened coordinates and the joiner widened them again (net_scale.c). The runs:
    widescreen,widescreen  the joiner's screens equal the host's (checks 2, 3 and 5 of network_run.py)
    classic,classic        the same
    classic,widescreen     the widescreen joiner's screens equal those of the widescreen,widescreen run
    widescreen,classic     the classic joiner's screens equal those of the classic,classic run
and every check of network_run.py besides (codes, connections, 4 click packets). Each run keeps its dumps and logs in
<folder>/<host>-<joiner>. Exit 0 when the four pass.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
RUNS = [("widescreen", "widescreen", None), ("classic", "classic", None),
        ("classic", "widescreen", "widescreen-widescreen"), ("widescreen", "classic", "classic-classic")]


def main(argv):
    if len(argv) < 5 or "--out" not in argv:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    out = os.path.abspath(argv[argv.index("--out") + 1])
    slow = ["--slow", argv[argv.index("--slow") + 1]] if "--slow" in argv else []
    failed = []
    for host, join, ref in RUNS:
        name = f"{host}-{join}"
        shots = os.path.join(out, name)
        os.makedirs(shots, exist_ok=True)
        args = [sys.executable, "-I", os.path.join(HERE, "network_run.py"), program, game, "--shots", shots,
                "--display", f"{host},{join}", "--online-off"] + slow
        if ref:
            args += ["--same-as", os.path.join(out, ref)]
        print(f"--- host {host}, joiner {join}", flush=True)
        r = subprocess.run(args, capture_output=True, text=True)
        with open(os.path.join(shots, "run.log"), "w") as f:
            f.write(r.stdout + r.stderr)
        for line in r.stdout.splitlines():
            if line.startswith(("ok ", "FAIL", "network run")):
                print(line, flush=True)
        if r.returncode != 0:
            failed.append(name)
    print(f"network display run: {'PASS' if not failed else 'FAIL (' + ', '.join(failed) + ')'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
