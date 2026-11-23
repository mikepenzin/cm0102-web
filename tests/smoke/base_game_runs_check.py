"""Item 153 (0.6.2): the runs of the same-save check that hold for the base game only (BASE_GAME_RUNS in smoke_run.py).

season-code-off and kinds-base are defined for the base game (start 2001, changing rules off). With --custom-db and a
data set that starts in its own season, compare_saves must first prove that the normal game is in that season, then
print n/a for those two runs. A check whose runs are all n/a fails, and every other case compares them as before.
The saves here are small containers of version 1 (tools/save/savfile.py) with or without the part bosman_season.dat
(season.c). expected_season reads a small table of the form of databases-signatures.tsv.

Run: python3 -I tests/smoke/base_game_runs_check.py (ctest same_save_base_game)."""
import contextlib
import hashlib
import io
import os
import struct
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smoke_run  # noqa: E402

ENTRY_SIZE = 0x10C


def write_save(path, parts):
    """A save of version 1 (parts stored as they are) with the parts [(name, bytes)]."""
    head = struct.pack("<3i", 1, 0, len(parts))
    offset = 12 + ENTRY_SIZE * len(parts)
    rows, body = b"", b""
    for name, data in parts:
        rows += struct.pack("<II", offset + len(body), len(data)) + name.encode("latin-1").ljust(ENTRY_SIZE - 8, b"\0")
        body += data
    with open(path, "wb") as f:
        f.write(head + rows + body)


def season_part(year, rules):
    return struct.pack("<4I", 1, 16, year, rules)


def compare(runs, saves, want):
    """compare_saves on runs [(name, save key)] with the normal run first: (failures, printed text)."""
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        failed = smoke_run.compare_saves([(n, {}) for n, _ in runs], {n: saves[k] for n, k in runs},
                                         {n: None for n, _ in runs}, season_want=want)
    return failed, out.getvalue()


def main():
    failed = 0

    def expect(label, ok, text=""):
        nonlocal failed
        print(f"{'ok  ' if ok else 'FAIL'}  {label}" + ("" if ok else f"\n{text}"))
        failed += not ok

    with tempfile.TemporaryDirectory(prefix="cm0102-base-game-") as work:
        general = ("general.dat", b"x" * 40)
        saves = {}
        for key, parts in {
            "base": [general],
            "base-part": [general, ("bosman_season.dat", season_part(2001, 0))],
            "2025": [general, ("bosman_season.dat", season_part(2025, 0))],
            "2025-other": [("general.dat", b"y" * 40), ("bosman_season.dat", season_part(2025, 0))],
            "base-other": [("general.dat", b"y" * 40)],
            "rules-on": [general, ("bosman_season.dat", season_part(2001, 1))],
        }.items():
            saves[key] = os.path.join(work, key + ".sav")
            write_save(saves[key], parts)

        expect("season of a save without the part", smoke_run.season_of_save(saves["base"]) == (2001, 0))
        expect("season of a 2025 save", smoke_run.season_of_save(saves["2025"]) == (2025, 0))
        expect("season of a save with changing rules on", smoke_run.season_of_save(saves["rules-on"]) == (2001, 1))

        # expected_season: a small table with a season data set, an extra version and the official base.
        table = os.path.join(work, "signatures.tsv")
        sets = {"Season": ("season database", "2025/26"), "Extra": ("extra version", "2002/03"),
                "Base": ("official base", "2001/02"), "Unknown": None}
        lines = ["database\tstyle\tdata_season\trole\tindex_size\tindex_sha256\tclub_size\tclub_sha256\tstaff_size"
                 "\tstaff_sha256"]
        for name, row in sets.items():
            folder = os.path.join(work, name)
            os.makedirs(folder)
            club, staff = (name + " club").encode(), (name + " staff data").encode()
            with open(os.path.join(folder, "club.dat"), "wb") as f:
                f.write(club)
            with open(os.path.join(folder, "Staff.dat"), "wb") as f:         # the game finds a name in any case
                f.write(staff)
            if row:
                lines.append(f"{name}\tn/a\t{row[1]}\t{row[0]}\t0\t-\t{len(club)}\t{hashlib.sha256(club).hexdigest()}"
                             f"\t{len(staff)}\t{hashlib.sha256(staff).hexdigest()}")
        with open(table, "w") as f:
            f.write("\n".join(lines) + "\n")
        for name, want in {"Season": (2025, 0), "Extra": (2002, 0), "Base": (2001, 0), "Unknown": (2001, 0)}.items():
            got = smoke_run.expected_season(os.path.join(work, name), table)
            expect(f"expected season of {name}: {got}", got == want)
        expect("the real table knows December 2025 as 2025/26", any(
            line.startswith("December 2025\t") and "\t2025/26\tseason database\t" in line
            for line in open(smoke_run.SIGNATURES)))

        base_runs = [("normal", "2025"), ("season-code-off", "base-other"), ("kinds-base", "2025-other")]
        smoke_run.CUSTOM_DB = os.path.join(work, "Season")
        n, text = compare(base_runs + [("again", "2025")], saves, (2025, 0))
        expect("custom 2025/26: the base-game runs are n/a, again is compared", n == 0 and
               text.count("n/a   ") == 2 and "ok    again" in text, text)
        n, text = compare(base_runs, saves, (2025, 0))
        expect("custom 2025/26: only base-game runs left fails (no run is compared)", n == 1 and
               "no run is compared" in text, text)
        n, text = compare([("normal", "base"), ("again", "base"), ("season-code-off", "base")], saves, (2025, 0))
        expect("custom 2025/26 data set, normal game in 2001: fails", n == 1 and "the data set gives 2025/26" in text,
               text)
        n, text = compare([("normal", "base"), ("season-code-off", "base-other"), ("kinds-base", "base-part")], saves,
                          (2001, 0))
        expect("custom base game: the base-game runs are compared and fail when they differ",
               n == 2 and "n/a" not in text and "FAIL  season-code-off" in text and "FAIL  kinds-base" in text, text)
        n, text = compare([("normal", "rules-on"), ("season-code-off", "rules-on")], saves, (2001, 1))
        expect("custom game with changing rules on: the base-game runs are n/a", "n/a   season-code-off" in text, text)

        smoke_run.CUSTOM_DB = None
        n, text = compare([("normal", "2025"), ("season-code-off", "base-other"), ("kinds-base", "2025-other")],
                          saves, None)
        expect("no custom data set: the base-game runs are compared as before", n == 2 and "n/a" not in text, text)

    print("PASS" if not failed else "FAIL")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
