#!/usr/bin/env python3
"""Writes the real UEFA season coefficients into the nation table of one of our own season databases (0.6.2 open
item 106, Max's decision of 4 November 2026).

Usage:
    python3 -I coeffs.py <database folder> <start year>            writes the values, prints the nations it changes
    python3 -I coeffs.py <database folder> <start year> --check    exits 1 when a value is not the real one

The template of each own database brings the coefficients of its own season: Summer 2002 the 3.9.68 values
(1995/96 to 2000/01), Summer 2004 those of October 2003 (a few wrong values), Summer 2006 those of November 2007
(2001/02 to 2006/07, one season early). A game in start year S needs the seasons (S-6)/(S-5) to (S-1)/S at nation
+0xe0 to +0x108 (tools/seasons/make_coeff_table.py gives the reasons). This step writes them for every UEFA nation,
from docs/research/2026-11-03-uefa-association-coefficients.tsv, with the same names and rules as
make_coeff_table.py. Run it on the finished database folder, before the duplicate check. It changes nothing else
in nation.dat.
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
import history_gaps as hg  # noqa: E402
import make_coeff_table as mct  # noqa: E402


def nation_file(folder):
    """(file name, offset, row count) of the nation table, from index.dat."""
    idx = open(os.path.join(folder, "index.dat"), "rb").read()
    for e in range(22):
        o = 8 + e * 0x43
        tid, cnt, off, _ = struct.unpack_from("<iiii", idx, o + 0x33)
        if tid == mct.NATION_TABLE:
            return idx[o:o + 51].split(b"\0")[0].decode("latin-1"), off, cnt
    mct.fail(f"{folder}: index.dat has no nation table")


def main(argv):
    try:
        return run(argv)
    except SystemExit as e:              # make_coeff_table.fail: a database or research fault
        print(e, file=sys.stderr)
        return 1


def run(argv):
    if len(argv) not in (2, 3) or not argv[1].isdigit() or argv[2:] not in ([], ["--check"]):
        print(__doc__, file=sys.stderr)
        return 2
    folder, start, check = argv[0], int(argv[1]), argv[2:] == ["--check"]
    real = mct.real_values()
    nations = mct.uefa_nations(folder)
    name, off, cnt = nation_file(folder)
    path = os.path.join(folder, name)
    data = bytearray(open(path, "rb").read())
    if len(data) < off + cnt * hg.SIZES[mct.NATION_TABLE]:
        mct.fail(f"{path}: shorter than {cnt} nation rows")
    changed = []
    for n, (nid, found) in sorted(nations.items(), key=lambda x: x[1][0]):
        if n == "Yugoslavia" and "Serbia" in nations:
            continue                     # the record of a former nation keeps its values
        want = mct.real_six(real, n, start, folder)
        if want != found:
            changed.append(n)
            struct.pack_into("<6d", data, off + nid * hg.SIZES[mct.NATION_TABLE] + mct.COEFFS_AT, *want)
    if check:
        print(f"{folder}: {len(changed)} UEFA nations do not hold the real values" + (f": {', '.join(changed)}"
                                                                                      if changed else ""))
        return 1 if changed else 0
    if changed:
        with open(path, "wb") as f:
            f.write(data)
    print(f"{path}: the real six seasons {mct.season(start - 5)} to {mct.season(start)} written for "
          f"{len(changed)} UEFA nations")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
