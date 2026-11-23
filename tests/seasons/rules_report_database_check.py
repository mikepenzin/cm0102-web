#!/usr/bin/env python3
"""ctest of the database of a rules report run, with no game (0.6.2 open item 129).

Usage:
    python3 -I tests/seasons/rules_report_database_check.py

The expected files of a start year Y (rules_report_expected/Y-...) are written on the database of Y
(docs/research/2026-10-21-expected-files-060.md): the folder whose name starts with SEASON_DATABASES[Y]
(November 2007 for 2007), our own Summer 2004 and Summer 2006 for 2004 and 2006, and the base data (3.9.68) for
2001 and 2002. A run with --year 2007 and no --database played the base data and compared it with the November
2007 file: the Brazilian state championships held 12, 5 and 5 clubs in place of 13, 6 and 6 (batch P, rr2007-a).
run_database() must give each year its database from the --run arguments, keep an explicit --database, and stop
when a year has no folder or zip.
Exit code 0 when every case passes.
"""
import os
import sys
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rules_report_check as rrc  # noqa: E402


def main():
    fails = []

    def expect(name, got, want):
        if got != want:
            fails.append(f"{name}: got {got!r}, want {want!r}")

    def case(name, argv, year, want, unpack_to):
        try:
            expect(name, rrc.run_database(["rules_report_check.py", "--run", "p", "g"] + argv, year, unpack_to), want)
        except (AttributeError, TypeError, SystemExit) as e:
            fails.append(f"{name}: {type(e).__name__} {e}")

    def stops(name, argv, year, unpack_to, words):
        try:
            rrc.run_database(["rules_report_check.py", "--run", "p", "g"] + argv, year, unpack_to)
            fails.append(f"{name}: no stop")
        except SystemExit as e:
            expect(f"{name} message names {words}", words in str(e), True)
        except (AttributeError, TypeError) as e:
            fails.append(f"{name}: {type(e).__name__} {e}")

    with tempfile.TemporaryDirectory() as root:
        def folder(*parts, index=True):
            path = os.path.join(root, *parts)
            os.makedirs(path)
            if index:
                open(os.path.join(path, "index.dat"), "wb").close()
            return path

        dbs, own, unpack = os.path.join(root, "dbs"), os.path.join(root, "own"), os.path.join(root, "unpack")
        os.makedirs(own)
        os.makedirs(unpack)
        nov2007 = folder("dbs", "November 2007")
        sep2010 = folder("dbs", "September 2010 ODB")
        folder("dbs", "October 2026", index=False)
        oct2026 = folder("dbs", "October 2026", "October 2026 Data Update")
        mine = folder("Mine")
        with zipfile.ZipFile(os.path.join(own, "Summer 2004.zip"), "w") as z:
            z.writestr("Summer 2004/index.dat", b"")
        where = ["--databases", dbs, "--own-dbs", own]

        case("2007", where, 2007, nov2007, unpack)
        case("2010 (a name with a suffix)", where, 2010, sep2010, unpack)
        case("2026 (index.dat in a subfolder)", where, 2026, oct2026, unpack)
        case("--database wins", where + ["--database", mine], 2007, mine, unpack)
        case("2001 base data", where, 2001, None, unpack)
        case("2002 base data (its expected files)", where, 2002, None, unpack)
        case("no --year", where, None, None, unpack)
        case("2004 own database, unpacked", where, 2004, os.path.join(unpack, "Summer 2004"), unpack)
        expect("2004 index.dat unpacked", os.path.isfile(os.path.join(unpack, "Summer 2004", "index.dat")), True)
        stops("2006 with no Summer 2006.zip", where, 2006, unpack, "Summer 2006")
        stops("2008 with no September 2008 folder", where, 2008, unpack, "September 2008")
        stops("no databases folder", ["--databases", os.path.join(root, "none")], 2007, unpack, "November 2007")
        for y in range(2003, 2027):
            if y not in rrc.SEASON_DATABASES and y not in rrc.OWN_DATABASES:
                fails.append(f"{y}: in neither table")

    for f in fails:
        print("FAIL  " + f)
    print("PASS" if not fails else f"FAILED: {len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
