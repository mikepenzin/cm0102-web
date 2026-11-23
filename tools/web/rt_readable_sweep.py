#!/usr/bin/env python3
"""The browser's memory bound in the runtime's season code (skill bosman-update; rt.h, rt_readable).

The original's code tests a pointer with `p >= 0x10000u && p < 0xfff00000u`. In the browser that range holds the
engine's own memory (host/web.c), so a sum of money taken for a pointer could read or write it: this fork tests with
rt_readable (rt.h) instead. A Bosman Edition update brings new files with the old test; this script rewrites them:
a helper whose body is the old test, `X >= 0x10000u && X < 0xfff00000u` and `X < 0x10000u || X >= 0xfff00000u`.
It prints the files it changed and any line that still holds 0xfff00000u outside rt.h and rt.c (to read by hand).

    python3 tools/web/rt_readable_sweep.py
"""
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "src", "recomp_rt")
changed, left = [], []
for d, _, names in os.walk(ROOT):
    for n in names:
        if not n.endswith((".c", ".h")) or n in ("rt.h", "rt.c"):
            continue
        path = os.path.join(d, n)
        s = open(path, encoding="utf-8", errors="surrogateescape").read()
        if "0xfff00000u" not in s:
            continue
        o = s
        s = re.sub(r"\{ return p >= 0x10000u && p < 0xfff00000u; \}", "{ return rt_readable(p); }", s)
        s = re.sub(r"\b([A-Za-z_][\w.\->\[\]]*) >= 0x10000u && \1 < 0xfff00000u", r"rt_readable(\1)", s)
        s = re.sub(r"\b([A-Za-z_][\w.\->\[\]]*) < 0x10000u \|\| \1 >= 0xfff00000u", r"!rt_readable(\1)", s)
        if s != o:
            open(path, "w", encoding="utf-8", errors="surrogateescape").write(s)
            changed.append(os.path.relpath(path))
        for i, line in enumerate(s.splitlines(), 1):
            if "0xfff00000u" in line:
                left.append(f"{os.path.relpath(path)}:{i}: {line.strip()}")
print(f"{len(changed)} files changed")
for c in changed:
    print("  " + c)
if left:
    print("still holding 0xfff00000u (read each by hand):")
    for l in left:
        print("  " + l)
sys.exit(0)
