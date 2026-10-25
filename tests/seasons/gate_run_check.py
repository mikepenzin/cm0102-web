#!/usr/bin/env python3
"""Check of tests/seasons/gate_run.py with no game (O369 runner hang).

Usage:
    python3 tests/seasons/gate_run_check.py [<saved soak folder> <start item>]

1. alive() of an ended child: a child that exits is a zombie until it is reaped, and os.kill(pid, 0) succeeds for it.
   alive() must say 0.
2. A replay of run() on a saved soak folder (default: the 2004 soak of the O369 retest, run-logs/own-db-input-lead4/
   gate/gate-2004, whose last leg passes 1 September 2008). The folder is linked into a new run folder, and the soak
   that run() starts is replaced by a child that only sleeps. run() must see the end, stop the child, reap it and
   print its result row, within 180 s.
Exit code 0 when both pass.
"""
import contextlib
import io
import os
import subprocess
import sys
import tempfile
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gate_run as gr  # noqa: E402

SAVED = "/Users/max/Projects/cm0102/run-logs/own-db-input-lead4/gate/gate-2004"


def main(argv):
    saved = os.path.abspath(argv[1]) if len(argv) > 1 else SAVED
    item = argv[2] if len(argv) > 2 else "2004"
    failed = []

    p = subprocess.Popen([sys.executable, "-c", "pass"])
    time.sleep(1)
    ok = not gr.alive(p.pid)
    print(("ok    " if ok else "FAIL  ") + "alive() is 0 for an ended child (a zombie before the fix)", flush=True)
    if not ok:
        failed.append("zombie")

    real_popen = gr.subprocess.Popen

    def fake(cmd, *a, **kw):                # the soak: a child that sleeps until run() stops it (ps, pgrep stay)
        if any("soak_run.py" in str(c) for c in cmd):
            cmd = [sys.executable, "-c", "import time; time.sleep(600)"]
        return real_popen(cmd, *a, **kw)

    with tempfile.TemporaryDirectory(prefix="gate-run-check-") as run:
        os.symlink(saved, os.path.join(run, f"gate-{item}"))
        os.makedirs(os.path.join(run, "owndb"), exist_ok=True)
        o = {"program": "/bin/true", "run": run, "max": 1, "machine": 99, "results": None, "name": "check",
             "databases": "/Users/max/Projects/cm0102/test-fixtures/databases",
             "own": "/Users/max/Projects/cm0102/own-dbs"}
        gr.subprocess.Popen = fake
        gr.database = lambda y, o: ([], "saved")                     # no unpack: the soak does not really run
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            t = threading.Thread(target=gr.run, args=(item, o), daemon=True)
            t.start()
            t.join(180)
        gr.subprocess.Popen = real_popen
        row = out.getvalue().strip()
        ok = not t.is_alive() and "| PASS |" in row
        print(("ok    " if ok else "FAIL  ") + f"run() on {saved} wrote its row: {row or '(none, the runner still waits)'}",
              flush=True)
        if not ok:
            failed.append("replay")
    print("PASS" if not failed else "FAILED: " + ", ".join(failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
