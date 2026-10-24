#!/usr/bin/env python3
"""Season database test on real data (0.6.0 slice 14): zips, first pick, recognition.

Usage:
    python3 -I tests/seasons/seasons_db_run.py <seasons_db_test> <folder of source zips>
        [--databases <folder of unpacked databases>] [--own-dbs <folder of our own source zips>]

<seasons_db_test> is the test program that ctest builds (build/seasons_db_test). <folder of source zips> is
test-fixtures/databases/zips. With --databases (test-fixtures/databases) the run also checks the version
list: a known extra version in Databases shows on its season, and a folder of unknown content shows as Custom.
With --own-dbs (the folder of the release input --own-dbs, which holds Summer 2002.zip) the run also checks the
2002/03 tile (Summer 2002 spec, slice 4): its zip shows as in the download with no other version, the first pick
unpacks Summer 2002, and a copy of the 3.9.68 data in Databases is no version of 2002/03 (Q417).

The run makes two season zips with tools/release/make_season_zips.py (October 2003 and September 2012) in a
new folder, and checks:
  1. the zips equal a second run (same bytes), and hold only the 20 data files in the folder "<name>"
  2. scan: the two seasons are "in the download", the others have the status the table gives
  3. prepare unpacks one zip into Databases/<name>, the folder equals the signature (identify)
  4. a second prepare uses the unpacked copy: nothing is unpacked, no file changes
  5. a folder under another name counts (recognition by content), and an edited folder is Custom
  6. an edited folder with the name of the season is not replaced: the new copy gets "<name> (2)"
  7. a damaged zip leaves no folder that looks whole and no temporary folder, and a retry works
  8. a stale temporary folder of an earlier stop is removed
"""
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
MAKE = os.path.join(ROOT, "tools", "release", "make_season_zips.py")
TSV = os.path.join(ROOT, "docs", "research", "rules-0.6.0", "databases-signatures.tsv")
NAMES = ["October 2003", "September 2012"]
YEAR = {"October 2003": 2003, "September 2012": 2012}
FILES = ["city.dat", "club.dat", "club_comp.dat", "club_comp_history.dat", "colour.dat", "common_names.dat",
         "continent.dat", "first_names.dat", "index.dat", "nat_club.dat", "nation.dat", "nation_comp.dat",
         "nation_comp_history.dat", "officials.dat", "second_names.dat", "stadium.dat", "staff.dat",
         "staff_comp.dat", "staff_comp_history.dat", "staff_history.dat"]
STATUS = {"OFFICIAL": 0, "UNPACKED": 1, "IN_DOWNLOAD": 2, "NO_DATABASE": 3, "MISSING": 4}
PREPARE = {"OK": 0, "OFFICIAL": 1, "NO_DATABASE": 2, "NO_ZIP": 3, "FAILED": 4}

failures = 0


def check(cond, text):
    global failures
    print(("ok    " if cond else "FAIL  ") + text, flush=True)
    if not cond:
        failures += 1


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def run(tool, *args):
    r = subprocess.run([tool, *args], capture_output=True, text=True)
    return r.stdout.strip(), r.returncode


def prepare(tool, game, save, year):
    out, code = run(tool, "prepare", game, save, str(year))
    m = re.match(r"^status=(\d+) name=(.*) unpacked=(\d) error=(.*)$", out)
    if not m:
        return -1, "", 0, f"unreadable output {out!r}"
    return int(m.group(1)), m.group(2), int(m.group(3)), m.group(4)


def scan(tool, game, save):
    out, _ = run(tool, "scan", game, save)
    rows = {}
    custom = []
    for line in out.splitlines():
        if line.startswith("custom "):
            custom.append(line[7:])
            continue
        label, rest = line.split(" ", 1)
        status = int(rest.split("status=")[1].split(" ")[0])
        extras = rest.split("extras=")[1]
        rows[label] = (status, extras)
    return rows, custom


def tree_state(folder):
    return {n: (os.path.getsize(os.path.join(folder, n)), os.stat(os.path.join(folder, n)).st_mtime_ns)
            for n in sorted(os.listdir(folder))}


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    tool, zips = os.path.abspath(argv[0]), os.path.abspath(argv[1])
    dbs = os.path.abspath(argv[argv.index("--databases") + 1]) if "--databases" in argv else None
    own = os.path.abspath(argv[argv.index("--own-dbs") + 1]) if "--own-dbs" in argv else None
    work = tempfile.mkdtemp(prefix="cm0102-seasons-")
    try:
        only = [a for n in NAMES for a in ("--only", n)]
        out1, out2 = os.path.join(work, "z1"), os.path.join(work, "z2")
        for o in (out1, out2):
            r = subprocess.run([sys.executable, "-I", MAKE, "--season-dbs", zips, "--out", o, *only],
                               capture_output=True, text=True)
            check(r.returncode == 0, f"make_season_zips.py {os.path.basename(o)}: {r.stdout.splitlines()[-1:] or r.stderr[-200:]}")
        import zipfile
        for n in NAMES:
            a, b = (os.path.join(o, n + ".zip") for o in (out1, out2))
            check(os.path.exists(a) and sha(a) == sha(b), f"{n}.zip is the same on a second run")
            with zipfile.ZipFile(a) as z:
                names = sorted(z.namelist())
            check(names == sorted(f"{n}/{f}" for f in FILES), f"{n}.zip holds the 20 data files in the folder {n}")

        # The tool stops on a zip that does not match the signatures table, and leaves no zip.
        src_zip = os.path.join(out1, "October 2003.zip")
        for what in ("club.dat", "index.dat"):
            tampered = os.path.join(work, "tampered-" + what)
            os.makedirs(tampered)
            with zipfile.ZipFile(src_zip) as zin, zipfile.ZipFile(os.path.join(tampered, "some name.zip"), "w", zipfile.ZIP_DEFLATED) as zout:
                for info in zin.infolist():
                    data = zin.read(info)
                    if info.filename.endswith("/" + what):
                        data = data[:-1] + bytes([data[-1] ^ 1])
                    zout.writestr(info.filename, data)
            bad_out = os.path.join(work, "bad-out-" + what)
            r = subprocess.run([sys.executable, "-I", MAKE, "--season-dbs", tampered, "--out", bad_out, "--only", "October 2003"],
                               capture_output=True, text=True)
            check(r.returncode == 1 and (not os.path.exists(bad_out) or os.listdir(bad_out) == []),
                  f"make_season_zips.py stops on a changed {what}: {r.stderr.strip()[:90]}")

        game, save = os.path.join(work, "game"), os.path.join(work, "save")
        os.makedirs(os.path.join(game, "Seasons"))
        os.makedirs(save)
        for n in NAMES:
            shutil.copy(os.path.join(out1, n + ".zip"), os.path.join(game, "Seasons", n + ".zip"))

        rows, custom = scan(tool, game, save)
        check(len(rows) == 25 and rows["2001/02"][0] == STATUS["OFFICIAL"], "scan: 25 seasons, 2001/02 official")
        check(rows["2002/03"][0] == STATUS["MISSING"], "scan: 2002/03 (Summer 2002) has no zip in this game folder")
        check(rows["2003/04"][0] == STATUS["IN_DOWNLOAD"] and rows["2012/13"][0] == STATUS["IN_DOWNLOAD"],
              "scan: the two zips show as in the download")
        check(rows["2004/05"][0] == STATUS["MISSING"] and rows["2006/07"][0] == STATUS["MISSING"],
              "scan: 2004/05 (Summer 2004) and 2006/07 (Summer 2006) have no zip in this game folder")
        check(rows["2005/06"][0] == STATUS["MISSING"], "scan: a season without its zip shows as missing")

        # 3. first pick
        status, name, unpacked, err = prepare(tool, game, save, 2003)
        folder = os.path.join(save, "Databases", "October 2003")
        check(status == PREPARE["OK"] and name == "October 2003" and unpacked == 1, f"prepare 2003: unpacked once ({status} {name!r} {err})")
        check(sorted(os.listdir(folder)) == sorted(FILES), "the unpacked folder holds the 20 data files and nothing else")
        out, _ = run(tool, "identify", folder)
        check(out == "October 2003", f"identify: the unpacked folder is October 2003 ({out})")
        rows_tsv = {}
        with open(TSV, encoding="utf-8") as f:
            head = f.readline().rstrip("\n").split("\t")
            for line in f:
                v = dict(zip(head, line.rstrip("\n").split("\t")))
                rows_tsv[v["database"]] = v
        check(sha(os.path.join(folder, "club.dat")) == rows_tsv["October 2003"]["club_sha256"] and
              sha(os.path.join(folder, "staff.dat")) == rows_tsv["October 2003"]["staff_sha256"],
              "the unpacked club.dat and staff.dat equal the signatures table")
        check(not os.path.exists(os.path.join(save, "unpack-tmp")), "no temporary folder is left, also not the empty parent")

        # 4. second pick
        before = tree_state(folder)
        status, name, unpacked, err = prepare(tool, game, save, 2003)
        check(status == PREPARE["OK"] and unpacked == 0 and name == "October 2003", "prepare 2003 again: no unpack")
        check(tree_state(folder) == before, "prepare 2003 again: no file changed")
        rows, custom = scan(tool, game, save)
        check(rows["2003/04"][0] == STATUS["UNPACKED"], "scan: 2003/04 shows as unpacked")

        # The recognition cache: a file counts only when it is 2 seconds older than the last change, so age it.
        cache = os.path.join(save, "Databases", ".identify-cache")
        check(not os.path.exists(cache), "fresh files (changed a moment ago) are not kept in the cache")
        old = time.time() - 60
        names_ = ("club.dat", "staff.dat")
        for n in names_:
            os.utime(os.path.join(folder, n), (old, old))
        out, _ = run(tool, "identify", folder)
        text = open(cache, encoding="utf-8").read() if os.path.exists(cache) else ""
        check(out == "October 2003" and text.startswith("table ") and "October 2003" in text and text.count("\n") == 2,
              "the result of the recognition is kept in Databases/.identify-cache with a table fingerprint")
        # One byte changed, same size: the cached folder is read again (the time changed) and is Custom.
        club = os.path.join(folder, "club.dat")
        with open(club, "r+b") as f:
            f.seek(1000)
            b0 = f.read(1)
            f.seek(1000)
            f.write(bytes([b0[0] ^ 1]))
        out, _ = run(tool, "identify", folder)
        check(out == "custom", "one changed byte with the same size makes the folder Custom, also after a cached result")
        out, _ = run(tool, "identify", folder)
        check(out == "custom", "and again (a file changed a moment ago is not trusted from the cache)")
        with open(club, "r+b") as f:
            f.seek(1000)
            f.write(b0)
        os.utime(club, (old, old))
        out, _ = run(tool, "identify", folder)
        check(out == "October 2003", "with the byte put back the folder is known again")
        # A cache of another table is ignored
        lines = open(cache, encoding="utf-8").read().split("\n")
        lines[0] = "table 00000000"
        lines[1] = lines[1].replace("October 2003", "-")
        with open(cache, "w", encoding="utf-8") as f:
            f.write("\n".join(lines))
        out, _ = run(tool, "identify", folder)
        check(out == "October 2003", "a cache file of another table version is ignored")
        with open(cache, "w", encoding="utf-8") as f:
            f.write("garbage without tabs\nmore\tgarbage\n")
        out, _ = run(tool, "identify", folder)
        check(out == "October 2003", "a damaged cache file changes nothing")
        os.unlink(cache)
        out, _ = run(tool, "identify", folder)
        check(out == "October 2003" and os.path.exists(cache), "without the cache file the folder is read again and the file comes back")
        # A folder outside a Databases folder gets no cache file.
        outside = os.path.join(work, "outside")
        os.makedirs(outside)
        shutil.copytree(folder, os.path.join(outside, "Copy"))
        for n in names_:
            os.utime(os.path.join(outside, "Copy", n), (old, old))
        out, _ = run(tool, "identify", os.path.join(outside, "Copy"))
        check(out == "October 2003" and os.listdir(outside) == ["Copy"], "a folder outside Databases gets no cache file")

        # 5. recognition by content
        moved = os.path.join(save, "Databases", "My own copy")
        os.rename(folder, moved)
        status, name, unpacked, err = prepare(tool, game, save, 2003)
        check(status == PREPARE["OK"] and name == "My own copy" and unpacked == 0, f"a renamed folder counts as the season database ({name!r})")
        rows, custom = scan(tool, game, save)
        check(rows["2003/04"][0] == STATUS["UNPACKED"] and custom == [], "scan: the renamed folder is not Custom")
        with open(os.path.join(moved, "club.dat"), "ab") as f:
            f.write(b"\0")
        out, _ = run(tool, "identify", moved)
        check(out == "custom", "an edited club.dat makes the folder Custom")
        rows, custom = scan(tool, game, save)
        check(custom == ["My own copy"] and rows["2003/04"][0] == STATUS["IN_DOWNLOAD"], "scan: the edited folder is listed as Custom")

        # 6. a folder with the season's name but other data is never replaced
        shutil.copytree(moved, os.path.join(save, "Databases", "October 2003"))
        status, name, unpacked, err = prepare(tool, game, save, 2003)
        check(status == PREPARE["OK"] and name == "October 2003 (2)" and unpacked == 1, f"an edited folder with the name stays; the copy is {name!r}")
        check(os.path.getsize(os.path.join(save, "Databases", "October 2003", "club.dat")) ==
              int(rows_tsv["October 2003"]["club_size"]) + 1, "the edited folder was not touched")

        # 7. a damaged zip
        bad = os.path.join(game, "Seasons", "September 2012.zip")
        data = open(bad, "rb").read()
        with open(bad, "wb") as f:
            f.write(data[: len(data) // 2])
        status, name, unpacked, err = prepare(tool, game, save, 2012)
        check(status == PREPARE["FAILED"] and err != "", f"a cut zip fails with a message ({err})")
        check(not os.path.exists(os.path.join(save, "Databases", "September 2012")), "a cut zip leaves no folder in Databases")
        tmp = os.path.join(save, "unpack-tmp")
        check(not os.path.exists(tmp), "a cut zip leaves no temporary folder")
        flipped = bytearray(data)
        flipped[len(data) // 3] ^= 0x55
        with open(bad, "wb") as f:
            f.write(flipped)
        status, name, unpacked, err = prepare(tool, game, save, 2012)
        check(status == PREPARE["FAILED"] and not os.path.exists(os.path.join(save, "Databases", "September 2012")) and not os.path.exists(tmp),
              f"a zip with a changed byte fails and leaves nothing ({err})")
        # 8. a stale temporary folder (an hour old) of a run that stopped, then a good zip
        stale = os.path.join(save, "unpack-tmp", "September 2012.1-1")
        os.makedirs(stale)
        with open(os.path.join(stale, "club.dat"), "wb") as f:
            f.write(b"half")
        long_ago = time.time() - 7200
        os.utime(stale, (long_ago, long_ago))
        with open(bad, "wb") as f:
            f.write(data)
        status, name, unpacked, err = prepare(tool, game, save, 2012)
        check(status == PREPARE["OK"] and unpacked == 1 and name == "September 2012" and not os.path.exists(os.path.join(save, "unpack-tmp")),
              "a retry with the good zip works, removes the stale temporary folder and the empty parent")
        out, _ = run(tool, "identify", os.path.join(save, "Databases", "September 2012"))
        check(out == "September 2012", "the retry gives September 2012")
        check(prepare(tool, game, save, 2001)[0] == PREPARE["OFFICIAL"] and prepare(tool, game, save, 2026)[0] == PREPARE["NO_DATABASE"],
              "prepare 2001 is official, prepare 2026 has no database")
        os.unlink(os.path.join(game, "Seasons", "October 2003.zip"))
        status, name, unpacked, err = prepare(tool, game, save, 2003)
        check(status == PREPARE["OK"] and unpacked == 0, "an unpacked season starts without its zip")

        # the 2002/03 tile with Summer 2002 (slice 4 of the Summer 2002 spec)
        plain = "/Users/max/Projects/cm0102/clean-3968/Data"     # the 3.9.68 data of this Mac (handover)
        if own and not os.path.isdir(plain):
            print(f"note  {plain} is not there: the 2002/03 check is skipped")
        elif own:
            out3 = os.path.join(work, "z3")
            r = subprocess.run([sys.executable, "-I", MAKE, "--season-dbs", own, "--out", out3, "--only", "Summer 2002"],
                               capture_output=True, text=True)
            check(r.returncode == 0, f"make_season_zips.py Summer 2002: {r.stdout.splitlines()[-1:] or r.stderr[-200:]}")
            shutil.copy(os.path.join(out3, "Summer 2002.zip"), os.path.join(game, "Seasons", "Summer 2002.zip"))
            base = os.path.join(save, "Databases", "Plain 3.9.68")
            shutil.copytree(plain, base)
            rows, custom = scan(tool, game, save)
            check(rows["2002/03"] == (STATUS["IN_DOWNLOAD"], "") and "Plain 3.9.68" not in custom,
                  f"scan: 2002/03 has Summer 2002 in the download and no other version ({rows['2002/03']})")
            status, name, unpacked, err = prepare(tool, game, save, 2002)
            check(status == PREPARE["OK"] and name == "Summer 2002" and unpacked == 1,
                  f"prepare 2002: Summer 2002 unpacked once ({status} {name!r} {err})")
            out, _ = run(tool, "identify", os.path.join(save, "Databases", "Summer 2002"))
            check(out == "Summer 2002", f"identify: the unpacked folder is Summer 2002 ({out})")
            out, _ = run(tool, "identify", base)
            check(out == "3.9.68", f"identify: the plain data is still 3.9.68 ({out})")
            rows, custom = scan(tool, game, save)
            check(rows["2002/03"] == (STATUS["UNPACKED"], ""), f"scan: 2002/03 shows as unpacked ({rows['2002/03']})")
            shutil.rmtree(base)
        else:
            print("note  no --own-dbs: the 2002/03 check is skipped")

        # versions and Custom, with databases from the fixtures
        if dbs:
            extra_name = "February 2008"
            src = os.path.join(dbs, extra_name)
            if os.path.isdir(src):
                shutil.copytree(src, os.path.join(save, "Databases", "Some other name"))
                rows, custom = scan(tool, game, save)
                check(rows["2007/08"][1] == extra_name, f"scan: {extra_name} shows as a version of 2007/08 ({rows['2007/08'][1]})")
                check("Some other name" not in custom, "scan: a known extra version is not Custom")
            else:
                print(f"note  {src} is not there: version check skipped")
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print("seasons_db_run: " + ("all checks passed" if not failures else f"{failures} checks failed"))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
