#!/usr/bin/env python3
"""Makes the season database zips of a download (0.6.0 slice 14).

Usage:
    python3 -I tools/release/make_season_zips.py --season-dbs <folder of source zips> --out <new folder>
        [--signatures <databases-signatures.tsv>] [--only <name> ...]

--season-dbs  a folder with source zips: the zips of the Update Team databases (test-fixtures/databases/zips),
              and again for the folder with the zips of our own season databases (2002/03, 2004/05 and 2006/07,
              Q351). Give it once for each folder. A source zip may have any file name and any extra files. The tool
              recognises each zip by the sizes and SHA-256 of its index.dat, club.dat and staff.dat, never by
              its name.
--out         the folder for the new zips. It must not exist or must be empty.
--signatures  the signatures table. Default: docs/research/rules-0.6.0/databases-signatures.tsv.
--only        make only the zips of these databases (for tests). Without it every database with the
              role "season database" is made, and a missing source zip stops the tool.

For each season database the tool writes <out>/<name>.zip. The zip holds one folder <name> with the 20 data
files of the game (the list of src/recomp_rt/win/dbfolders.c) in lower case, and nothing else: no readme and
no name of a person from the source zip. <name> is the neutral name of the signatures table, for example
"October 2003". The zips are the same for the same input: the tool sets every date and every attribute.
Then the tool reads each new zip back and checks it against the signatures table. Any mismatch stops the tool
with exit code 1, and no zip is left in <out>.
"""
import argparse
import csv
import hashlib
import os
import re
import shutil
import sys
import zipfile

DATA_FILES = [
    "city.dat", "club.dat", "club_comp.dat", "club_comp_history.dat", "colour.dat", "common_names.dat",
    "continent.dat", "first_names.dat", "index.dat", "nat_club.dat", "nation.dat", "nation_comp.dat",
    "nation_comp_history.dat", "officials.dat", "second_names.dat", "stadium.dat", "staff.dat",
    "staff_comp.dat", "staff_comp_history.dat", "staff_history.dat"]
SEASON_ROLE = "season database"
NEUTRAL_NAME = re.compile(r"^[A-Za-z0-9][A-Za-z0-9. ]*$")
STAMP = (2001, 7, 1, 0, 0, 0)
CHUNK = 1 << 20


class Stop(Exception):
    pass


def read_signatures(path):
    rows = []
    with open(path, newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            r["index_size"], r["club_size"], r["staff_size"] = (int(r[k]) for k in ("index_size", "club_size", "staff_size"))
            if not NEUTRAL_NAME.match(r["database"]):
                raise Stop(f"signatures table: the name {r['database']!r} is not a plain name")
            rows.append(r)
    names = [r["database"] for r in rows]
    if len(set(names)) != len(names):
        raise Stop("signatures table: a database name appears twice")
    return rows


def hash_entry(zf, info):
    h = hashlib.sha256()
    with zf.open(info) as f:
        while True:
            b = f.read(CHUNK)
            if not b:
                break
            h.update(b)
    return info.file_size, h.hexdigest()


def data_entries(zf):
    """{lower case file name: ZipInfo} of the files of the zip. The folder part is dropped. A name that
    appears twice (different case or folder) stops the tool for the files that matter."""
    out = {}
    for info in zf.infolist():
        if info.is_dir():
            continue
        base = info.filename.replace("\\", "/").rsplit("/", 1)[-1].lower()
        if base in DATA_FILES:
            if base in out:
                raise Stop(f"{zf.filename}: {base} appears twice")
            out[base] = info
    return out


def signature_of(zf):
    """(index, club, staff) as (size, sha256) pairs, or None when the zip lacks one of the three."""
    entries = data_entries(zf)
    if not all(k in entries for k in ("index.dat", "club.dat", "staff.dat")):
        return None, entries
    return tuple(hash_entry(zf, entries[k]) for k in ("index.dat", "club.dat", "staff.dat")), entries


def row_matches(row, sig):
    want = ((row["index_size"], row["index_sha256"]), (row["club_size"], row["club_sha256"]),
            (row["staff_size"], row["staff_sha256"]))
    return tuple(sig) == want


def find_sources(folders, rows):
    """{database name: path of its source zip}. A zip that matches no row stops the tool."""
    found = {}
    for path in sorted(os.path.join(d, n) for d in folders for n in os.listdir(d)):
        name = os.path.basename(path)
        if not name.lower().endswith(".zip") or not os.path.isfile(path):
            continue
        with zipfile.ZipFile(path) as zf:
            sig, _ = signature_of(zf)
        if sig is None:
            raise Stop(f"{name}: no index.dat, club.dat and staff.dat in the zip")
        match = [r for r in rows if row_matches(r, sig)]
        if not match:
            club_staff = [r for r in rows if (r["club_size"], r["club_sha256"]) == tuple(sig[1])
                          and (r["staff_size"], r["staff_sha256"]) == tuple(sig[2])]
            if club_staff:
                raise Stop(f"{name}: club.dat and staff.dat are those of {club_staff[0]['database']}, "
                           "but index.dat differs from the signatures table")
            raise Stop(f"{name}: the zip matches no database of the signatures table")
        db = match[0]["database"]
        if db in found:
            raise Stop(f"{name} and {os.path.basename(found[db])} are the same database {db}")
        found[db] = path
    return found


def write_zip(src_path, row, dest):
    tmp = dest + ".tmp"
    try:
        with zipfile.ZipFile(src_path) as src, zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as out:
            entries = data_entries(src)
            missing = [f for f in DATA_FILES if f not in entries]
            if missing:
                raise Stop(f"{os.path.basename(src_path)}: missing {', '.join(missing)}")
            for f in DATA_FILES:
                zi = zipfile.ZipInfo(f"{row['database']}/{f}", STAMP)
                zi.compress_type = zipfile.ZIP_DEFLATED
                zi.external_attr = 0o644 << 16
                zi.create_system = 3
                with src.open(entries[f]) as r, out.open(zi, "w") as w:
                    shutil.copyfileobj(r, w, CHUNK)
        os.replace(tmp, dest)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


def check_zip(path, row):
    with zipfile.ZipFile(path) as zf:
        bad = zf.testzip()
        if bad:
            raise Stop(f"{path}: the entry {bad} fails its CRC")
        names = sorted(i.filename for i in zf.infolist())
        want = sorted(f"{row['database']}/{f}" for f in DATA_FILES)
        if names != want:
            raise Stop(f"{path}: the zip holds {names}, not the 20 data files of {row['database']}")
        sig, _ = signature_of(zf)
        if sig is None or not row_matches(row, sig):
            raise Stop(f"{path}: the new zip does not match the signatures table for {row['database']}")


def main(argv):
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--season-dbs", action="append", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--signatures", default=os.path.join(here, "..", "..", "docs", "research", "rules-0.6.0",
                                                        "databases-signatures.tsv"))
    ap.add_argument("--only", action="append", default=[])
    a = ap.parse_args(argv)
    made = []
    try:
        rows = read_signatures(a.signatures)
        wanted = [r for r in rows if r["role"] == SEASON_ROLE]
        if a.only:
            unknown = [n for n in a.only if n not in [r["database"] for r in wanted]]
            if unknown:
                raise Stop(f"--only: not a season database: {', '.join(unknown)}")
            wanted = [r for r in wanted if r["database"] in a.only]
        if not wanted:
            raise Stop("the signatures table has no season database")
        for d in a.season_dbs:
            if not os.path.isdir(d):
                raise Stop(f"{d} is not a folder")
        if os.path.exists(a.out) and os.listdir(a.out):
            raise Stop(f"{a.out} is not empty")
        sources = find_sources(a.season_dbs, rows)
        missing = [r["database"] for r in wanted if r["database"] not in sources]
        if missing:
            raise Stop("no source zip for: " + ", ".join(missing))
        os.makedirs(a.out, exist_ok=True)
        total = 0
        for r in wanted:
            dest = os.path.join(a.out, r["database"] + ".zip")
            write_zip(sources[r["database"]], r, dest)
            made.append(dest)
            check_zip(dest, r)
            size = os.path.getsize(dest)
            total += size
            print(f"{r['database']}.zip  {size:>10}  season {r['data_season']}")
        print(f"{len(made)} zips, {total} bytes ({total / 1e6:.1f} MB)")
    except (Stop, OSError, zipfile.BadZipFile) as e:
        for p in made:
            try:
                os.unlink(p)
            except OSError:
                pass
        print(f"make_season_zips.py: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
