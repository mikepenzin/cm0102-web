#!/usr/bin/env python3
"""Makes src/recomp_rt/win/season_size_wraps.c: one RT_WRAP for each set_up_rules of the league classes of
tools/recomp/league_sizes.py (0.6.0 slice 18).

Usage:
    python3 -I tools/seasons/make_size_wraps.py [--out src/recomp_rt/win/season_size_wraps.c] [--check]

set_up_rules (vtable +0x8c) writes the promotion and relegation places of a league (+0xbe promotion, +0xbf
promotion play-off, +0xc0 relegation play-off, +0xc1 relegation; League::has_automatic_promotion 0x684790,
League::has_relegation 0x6847b0). The constructor and start_new_season call it. Each wrap calls the original
and then season_sizes_rules(this), which sets the places of the season in a season game
(src/recomp_rt/win/season_sizes.c).

The grown classes (league_sizes.py GROWN, 0.7.2 league sizes slice 2, D21) get a table of their rows and one wrap of
create_schedule each (thiscall: stage, &count, &code, &flag; ret 0x10): the original, then
season_sizes_schedule(this, stage, &count), which gives the template the rounds of the season's count (D4, D5).
Both modes stop when the cap of a grown class is under the largest count of its rows in the encoded rules table.

The file is committed, because the recompiler reads the RT_WRAP lines of src/recomp_rt/win/*.c. --check
writes nothing and exits with code 1 when the committed file differs from what the tool makes (ctest
season_size_wraps). Both modes stop when a floor is under a fixed row that another class reads (league_sizes.py
ROW_READS, open item 113). RT_WRAP stops the build when an address is not a translated function.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "recomp"))
from league_sizes import (CAPS, CLASSES, FIXED_EXCHANGE, FLOOR_COUNT, GROWN, HELD, ROW_READS,  # noqa: E402
                          SCHEDULE_TAKEN)

OUT = os.path.join(ROOT, "src", "recomp_rt", "win", "season_size_wraps.c")
ENCODED = os.path.join(ROOT, "docs", "research", "2026-10-18-league-rules-encoded.tsv")
MODES = {"shuffled": "SEASON_TEMPLATE_SHUFFLED", "sequential": "SEASON_TEMPLATE_SEQUENTIAL",
         "mirrored": "SEASON_TEMPLATE_MIRRORED", "halves": "SEASON_TEMPLATE_HALVES"}


def make():
    lines = [
        "/* The set_up_rules functions of the league classes of tools/recomp/league_sizes.py, wrapped for the",
        " * promotion and relegation places of each season (season_sizes.h). Made by tools/seasons/make_size_wraps.py:",
        " * do not edit by hand. */",
        '#include "win.h"',
        '#include "season_sizes.h"',
        '#include "season_template.h"',
        "",
        "/* Each wrap: the original (thiscall, no arguments), then season_sizes_rules(this). The recompiler reads",
        " * the RT_WRAP lines as they are (tools/recomp/recomp.py), so they are written out in full. */",
    ]
    seen = set()
    for cls, _file, rules, _stores in CLASSES:
        if rules in seen:
            raise SystemExit(f"make_size_wraps.py: {cls}: set_up_rules {rules:#x} twice")
        seen.add(rules)
        lines += ["",
                  f"/* {cls}::set_up_rules */",
                  f"void o_{rules:08x}(void);",
                  f"RT_WRAP({rules:08x}, rules_{rules:06x})",
                  "{",
                  "    uint32_t self = cpu.ecx;",
                  f"    o_{rules:08x}();",
                  "    if (rt_unwinding) return;",
                  "    season_sizes_rules(self);",
                  "}"]
    files = sorted({f for _c, f, _r, _s in CLASSES})
    lines += ["", "/* The source files of these classes (season_sizes.h). */",
              "const char *const season_size_files[] = {"]
    lines += [f'    "{f}",' for f in files]
    lines += ["};", f"const int season_size_file_count = {len(files)};",
              "/* The largest count of each file (0: the class counts the clubs of the data). */",
              "const int season_size_caps[] = {"]
    lines += [f"    {CAPS.get(f, 0)},       /* {f} */" for f in files]
    lines += ["};", "/* The smallest count of each file: the largest where a cup of the nation needs it (league_sizes.py FLOORS). */",
              "const int season_size_floors[] = {"]
    lines += [f"    {FLOOR_COUNT.get(f, 0)},       /* {f} */" for f in files]
    lines += ["};", "/* The count whose places and exchange stay the class's (league_sizes.py FIXED_EXCHANGE). */",
              "const int season_size_fixed[] = {"]
    lines += [f"    {FIXED_EXCHANGE.get(f, 0)},       /* {f} */" for f in files]
    lines += ["};"]
    names = {f: c for c, f, _r, _s in CLASSES}
    lines += ["", "/* The grown classes (league_sizes.py GROWN): file, the rows that create_table allocates, the cap, the",
              " * mode of the date template. */",
              "const SeasonGrown season_grown[] = {"]
    lines += [f'    {{"{f}", {HELD[f]}, {cap}, {MODES[mode]}}},' for f, (cap, _s, mode) in sorted(GROWN.items())]
    lines += ["};", f"const int season_grown_count = {len(GROWN)};",
              "",
              "/* Each wrap: the original, then season_sizes_schedule(this, stage, &count). */"]
    for f, (_cap, sched, _mode) in sorted(GROWN.items(), key=lambda x: x[1][1]):
        if f in SCHEDULE_TAKEN:
            lines += ["", f"/* {names[f]}::create_schedule {sched:#x}: wrapped in {SCHEDULE_TAKEN[f]} */"]
            continue
        lines += ["",
                  f"/* {names[f]}::create_schedule */",
                  f"void o_{sched:08x}(void);",
                  f"RT_WRAP({sched:08x}, schedule_{sched:06x})",
                  "{",
                  "    uint32_t self = cpu.ecx, count_at = rt_arg(1);",
                  "    int stage = (int8_t)(rt_arg(0) & 0xff);",
                  f"    o_{sched:08x}();",
                  "    if (rt_unwinding) return;",
                  "    season_sizes_schedule(self, stage, count_at);",
                  "}"]
    return "\n".join(lines) + "\n"


def cap_faults(path=ENCODED):
    """The grown files whose cap is under the largest count of their rows in the encoded rules table."""
    most = {}
    with open(path, encoding="utf-8") as f:
        cols = f.readline().rstrip("\n").split("\t")
        for line in f:
            row = dict(zip(cols, line.rstrip("\n").split("\t")))
            if row.get("comp_file") in GROWN and row.get("clubs", "").isdigit():
                most[row["comp_file"]] = max(most.get(row["comp_file"], 0), int(row["clubs"]))
    return [f"{f}: cap {cap} is under {most.get(f, 0)} clubs of its rows" if most.get(f, 0) > cap
            else f"{f}: no rows in {os.path.basename(path)}"
            for f, (cap, _s, _m) in sorted(GROWN.items()) if most.get(f, 0) > cap or f not in most]


def row_read_faults(floors=None):
    """The files whose floor is not above a fixed row that another class reads (ROW_READS, open item 113)."""
    floors = FLOOR_COUNT if floors is None else floors
    return [f"{f}: floor {floors.get(f, 0)} is under {need} clubs, and {why}"
            for f, (need, why) in sorted(ROW_READS.items()) if floors.get(f, 0) < need]


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args(argv)
    text = make()
    faults = row_read_faults() + cap_faults()
    # The check itself: one count under each row must give a fault.
    faults += [f"{f}: the row check finds no fault with a floor of {need - 1}" for f, (need, _w) in ROW_READS.items()
               if not row_read_faults({**FLOOR_COUNT, f: need - 1})]
    for line in faults:
        print(f"make_size_wraps.py: {line}")
    if faults:
        return 1
    if a.check:
        have = open(a.out, encoding="utf-8").read() if os.path.exists(a.out) else ""
        if have != text:
            print(f"make_size_wraps.py: {a.out} differs from the class table: run the tool again")
            return 1
        print(f"make_size_wraps.py: {a.out} matches {len(CLASSES)} classes")
        return 0
    with open(a.out, "w", encoding="utf-8") as f:
        f.write(text)
    print(f"make_size_wraps.py: {len(CLASSES)} wraps written to {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
