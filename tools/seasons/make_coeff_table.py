#!/usr/bin/env python3
"""Makes src/recomp_rt/win/season_coeffs_table.c, the real UEFA season coefficients of the season databases whose
values are missing, wrong or one season off (0.6.2 open item 106).

Usage:
    python3 -I tools/seasons/make_coeff_table.py --measure <folder of database folders>
        writes docs/research/2026-11-03-database-coefficients.tsv from the databases and the researched values, and
        prints the season that each of the six coefficient slots of each season database holds
    python3 -I tools/seasons/make_coeff_table.py            writes the C table from the TSV
    python3 -I tools/seasons/make_coeff_table.py --check    exits 1 when the C table is not the one the TSV gives

A nation record (nation.dat, 0x122 bytes) holds six season coefficients, doubles at +0xe0 to +0x108, oldest first.
At a new game in start year S the game expects the seasons (S-6)/(S-5) to (S-1)/S there: the first season ranks
the nations by +0xe0 to +0x100 (UefaSeeding::roll_coefficients 0x903b50 makes no shift in the first season,
0x903b6f), the European Performance Spots of the first season read +0x108 (season_uefa_seeding.c), and each
later season change shifts the six down one place and writes the game's own points to +0x108 (0x903b50). So
+0x108 is the season before the start, which counts in the five-season totals of the next five seasons.

Every season database and extra database gets the real values (Max's decisions of 3 and 4 November 2026, "do
whatever real life does"). OFFSETS names those whose values are one season off, with the slot offset that --measure
found: -1 when all six are one season old, +1 when they are one season early. The others have the offset 0: the
values sit in the right slots, with a few wrong values or with 0 at +0x108 (the season before the start missing).
The values come from docs/research/2026-11-03-uefa-association-coefficients.tsv (kassiesa.net country rankings 2000
to 2026, read 3 November 2026; UEFA's own coefficient data agrees from 2006/07 on). --measure checks that most
nonzero values of each database are the real values at its offset, so the table fixes the fault that was measured.

--measure reads each database as players have it from before 4 November 2026. Our own Summer 2002, Summer 2004 and
Summer 2006 hold the real values from that day (own_db/coeffs.py), so --measure takes their earlier zips
(own-dbs/old, "before the real coefficients", EARLIER_NATIONS), and the rows fix a player's copy of an earlier zip.

A row is a nation whose six values are not the real ones: its id in the database, its name, the six values the game
must find there (a check that the loaded nation.dat is the known one) and the six real values. Yugoslavia in a
database that also has Serbia (a record of a former nation) keeps its values. Russia takes its real values too: UEFA
kept Russia in its ranking after the suspension of 28 February 2022 (frozen at 4.333 a season from 2022/23), and the
game gives a suspended nation no places (season_uefa_seeding.c). The column db_agree counts the changed seasons
whose real value another season database holds in its right slot. The values are written with repr(), which a C
compiler reads as the same double.
"""
import csv
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import history_gaps as hg  # noqa: E402

ROOT = hg.ROOT
TSV = os.path.join(ROOT, "docs", "research", "2026-11-03-database-coefficients.tsv")
REAL = os.path.join(ROOT, "docs", "research", "2026-11-03-uefa-association-coefficients.tsv")
OUT = os.path.join(ROOT, "src", "recomp_rt", "win", "season_coeffs_table.c")
NATION_TABLE, UEFA = 4, 2
CONTINENT_AT, NAME_AT, COEFFS_AT = 0x71, 0x04, 0xe0
# database: the slot offset of its values (see above). Every other season database and extra version has the
# offset 0 (--measure checks it).
OFFSETS = {"Summer 2002": -1, "September 2005": -1, "Summer 2006": +1, "September 2008": -1,
           "September 2011 ODB": -1, "October 2019": -1, "November 2020": -1, "October 2021": +1,
           # extra versions
           "December 2008": -1, "March 2009": -1, "June 2010": +1, "March 2012 ODB": -1, "March 2020": -1,
           "April 2021": -1, "April 2022": +1, "April 2023": -1}
ROLES = ("season database", "extra version")
# Our own databases: the SHA-256 of the nation.dat that --measure must read, that of the zips before 4 November 2026
# (own-dbs/old, "before the real coefficients"). The zips of that day hold the real values, so they give no rows.
EARLIER_NATIONS = {
    "Summer 2002": "3868c523f26cea7732850ad0d5948605a9e5c53855e255b9f318b170c0634ffe",
    "Summer 2004": "91fd79645926acee218b15ddf275b1380703b85dc1a84030ac66aa584ba8005f",
    "Summer 2006": "f3bb1dd0b8fad018a15568c8297e27f60cd434b41792d485a2f2e94b71d2e10b"}
# UEFA nations of a database that are not UEFA members (no researched values): their six real values are 0
NOT_MEMBERS = {"Greenland"}
# the database's name of a nation: the researched name
REAL_NAME = {"Holland": "Netherlands", "Republic of Ireland": "Ireland", "FYR of Macedonia": "North Macedonia",
             "Yugoslavia": "Serbia"}
COLUMNS = ["database", "nation_id", "nation", "seasons", "found", "real", "db_agree", "source"]


def season(end):
    return f"{end - 1}/{end % 100:02d}"


def fail(text):
    raise SystemExit("make_coeff_table.py: " + text)


def known_databases(dbroot):
    """{name: (data season start or None, role, folder)} of the database folders in dbroot."""
    sigs = {(int(r["club_size"]), r["club_sha256"], int(r["staff_size"]), r["staff_sha256"]): r
            for r in hg.read_tsv(hg.SIGS)}
    out = {}
    for sub in sorted(os.listdir(dbroot)):
        p = os.path.join(dbroot, sub)
        if not os.path.isdir(p):
            continue
        r = hg.identify(p, sigs)
        inner = [os.path.join(p, s) for s in os.listdir(p) if os.path.isdir(os.path.join(p, s))]
        if not r and len(inner) == 1:          # a folder that holds the unpacked zip's own folder
            p = inner[0]
            r = hg.identify(p, sigs)
        if r:
            start = int(r["data_season"][:4]) if r["data_season"][:4].isdigit() else None
            out[r["database"]] = (start, r["role"], p)
    return out


def uefa_nations(folder):
    """{name: (nation id, [six doubles])} of the UEFA nations of a database."""
    d = hg.Data(folder)
    out = {}
    for i, r in enumerate(d.rows(NATION_TABLE)):
        if hg.s32(r, CONTINENT_AT) != UEFA:
            continue
        name = r[NAME_AT:NAME_AT + 51].split(b"\0")[0].decode("latin-1")
        if name in out:
            fail(f"{folder}: the UEFA nation {name!r} appears twice")
        if hg.s32(r, 0) != i:
            fail(f"{folder}: nation record {i} has the id {hg.s32(r, 0)}")
        out[name] = (i, list(struct.unpack_from("<6d", r, COEFFS_AT)))
    return out


def real_values():
    """{(researched name, season end year): (value, source)}."""
    out = {}
    for r in hg.read_tsv(REAL):
        out[(r["nation"], int(r["season"][:4]) + 1)] = (float(r["coefficient"]), r["source"])
    return out


def real_six(real, name, start, where):
    """The real values of the six seasons (start-6)/(start-5) to (start-1)/start of the database's nation name. A
    season before the nation's first researched season (before it joined UEFA) is 0; a gap after it fails."""
    if name in NOT_MEMBERS:
        return [0.0] * 6
    rname = REAL_NAME.get(name, name)
    seasons = [e for n, e in real if n == rname]
    if not seasons:
        fail(f"{where}: no researched values of {name}")
    out = []
    for e in range(start - 5, start + 1):
        if (rname, e) in real:
            out.append(real[(rname, e)][0])
        elif e < min(seasons):
            out.append(0.0)
        else:
            fail(f"{where}: no researched value of {name} for {season(e)}")
    return out


def season_slots(dbs):
    """{(nation, season end year): {value}} of the season databases whose six slots are right (not in OFFSETS
    with a nonzero offset), for the db_agree column."""
    out = {}
    for name, (y, role, folder) in dbs.items():
        if not y or role not in ROLES or OFFSETS.get(name):
            continue
        for n, (_, v) in uefa_nations(folder).items():
            for k in range(6):
                if v[k] or (k < 5):
                    out.setdefault((n, y - 5 + k), set()).add(v[k])
    return out


def measure(dbroot):
    if not os.path.isdir(dbroot):
        fail(f"{dbroot} is not a folder")
    dbs = known_databases(dbroot)
    real = real_values()
    held = season_slots(dbs)
    rows = []
    targets = [r["database"] for r in hg.read_tsv(hg.SIGS) if r["role"] in ROLES]
    for target in targets:
        if target not in dbs:
            fail(f"no folder of the database {target!r} in {dbroot}")
        offset = OFFSETS.get(target, 0)
        if target in EARLIER_NATIONS and hg.sha(os.path.join(dbs[target][2], "nation.dat")) != EARLIER_NATIONS[target]:
            fail(f"{target}: not the nation.dat of the zip before 4 November 2026 (own-dbs/old)")
        y = dbs[target][0]
        nations = uefa_nations(dbs[target][2])
        ends = [y - 5 + k for k in range(6)]
        lined = total = 0
        for name in sorted(nations, key=lambda n: nations[n][0]):
            nid, found = nations[name]
            if name == "Yugoslavia" and "Serbia" in nations:
                continue
            rname = REAL_NAME.get(name, name)
            want = real_six(real, name, y, target)
            for k in range(6):
                if found[k]:
                    total += 1
                    lined += found[k] == real.get((rname, ends[k] + offset), (None,))[0]
            if want == found:
                continue
            changed = [k for k in range(6) if want[k] != found[k]]
            agree = sum(want[k] in held.get((name, ends[k]), ()) for k in changed)
            pages = sorted({real[(rname, e)][1].rsplit("/", 1)[1] for e in ends if (rname, e) in real})
            rows.append({"database": target, "nation_id": str(nid), "nation": name,
                         "seasons": f"{season(ends[0])}..{season(ends[5])}",
                         "found": " ".join(map(repr, found)), "real": " ".join(map(repr, want)),
                         "db_agree": f"{agree}/{len(changed)}",
                         "source": "researched: kassiesa.net " + ", ".join(pages) + " (read 2026-11-03)"})
        if not total or lined < 0.8 * total:
            fail(f"{target}: only {lined} of {total} nonzero values are the real ones at the offset {offset}")
        print(f"{target}: {lined} of {total} nonzero values are the real ones at the offset {offset:+d}; "
              f"{sum(r['database'] == target for r in rows)} nations change")
    with open(TSV, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, COLUMNS, delimiter="\t", lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    print(f"wrote {TSV} ({len(rows)} rows)")
    report(dbs)


def report(dbs):
    """For each season database, the season of each of its six coefficient slots. The seasons come from a
    timeline of the values of four nations: it starts from October 2026 (+0xe0 holds 2020/21 there: England
    24.357, the real value) and takes each database whose nonzero slots line up with it at one offset (two slots
    or more agree, none disagrees), until no database adds a season. A slot is "-" when it is 0 and "?" when it
    is not in the timeline (the values of no other database)."""
    probe = ("England", "Spain", "Italy", "Germany")
    vals = {}
    for name, (y, role, folder) in dbs.items():
        if role == "season database" and y:
            a = uefa_nations(folder)
            vals[name] = (y, [tuple(a[p][1][k] for p in probe) for k in range(6)])

    def same(u, v):
        return sum(abs(x - y) < 0.05 for x, y in zip(u, v)) >= 3

    timeline = {2021 + k: v for k, v in enumerate(vals["October 2026"][1]) if any(v)}   # season end year: values
    grew = True
    while grew:
        grew = False
        for name, (y, slots) in vals.items():
            for first in range(1990, 2030):
                pairs = [(first + k, v) for k, v in enumerate(slots) if any(v)]
                hit = [e for e, v in pairs if e in timeline and same(v, timeline[e])]
                bad = [e for e, v in pairs if e in timeline and not same(v, timeline[e])]
                if len(hit) >= 2 and not bad:
                    for e, v in pairs:
                        if e not in timeline:
                            timeline[e] = v
                            grew = True
                    break

    def season_of(v):
        if not any(v):
            return "-"
        for e, t in sorted(timeline.items()):
            if same(v, t):
                return f"{e - 1}/{e % 100:02d}"
        return "?"
    for y, name in sorted((v[0], k) for k, v in vals.items()):
        got = [season_of(v) for v in vals[name][1]]
        want = [f"{y - 6 + k}/{(y - 5 + k) % 100:02d}" for k in range(6)]
        # +0xe8..+0x108 go into the totals of the next five seasons, so each must hold its own season
        wrong = [f"+0x{COEFFS_AT + 8 * k:x} holds {got[k]}, not {want[k]}" for k in range(1, 6) if got[k] != want[k]]
        print(f"{name:20s} {y}/{(y + 1) % 100:02d}: {' '.join(got)}  " + ("; ".join(wrong) if wrong else "right"))


def table_text():
    rows = hg.read_tsv(TSV)
    starts = {r["database"]: int(r["data_season"][:4]) for r in hg.read_tsv(hg.SIGS) if r["data_season"][:4].isdigit()}
    real_vals = real_values()
    by_db = {}
    for r in rows:
        if r["database"] not in starts:
            fail(f"{r['database']!r} is not in databases-signatures.tsv")
        nid = int(r["nation_id"])
        found, real = r["found"].split(), r["real"].split()
        if len(found) != 6 or len(real) != 6:
            fail(f"{r['database']} {r['nation']}: not six values")
        if not 0 <= nid < 1000 or not all(0 <= float(v) < 100 for v in found + real):
            fail(f"{r['database']} {r['nation']}: a value out of range")
        if any(repr(float(v)) != v for v in found + real):
            fail(f"{r['database']} {r['nation']}: a value is not in the repr() form")
        if [float(v) for v in real] != real_six(real_vals, r["nation"], starts[r["database"]], r["database"]):
            fail(f"{r['database']} {r['nation']}: the real values are not those of {os.path.basename(REAL)}")
        if any(q in r["nation"] for q in '"\\?') or len(r["nation"].encode("latin-1")) > 50:
            fail(f"{r['database']}: the nation name {r['nation']!r} cannot go into the table")
        by_db.setdefault(r["database"], []).append((nid, r["nation"], found, real))
    lines = [
        "/* The real UEFA season coefficients of the season databases (season_coeffs.h). Made by",
        " * tools/seasons/make_coeff_table.py from docs/research/2026-11-03-database-coefficients.tsv. Do not edit. */",
        '#include "season_coeffs.h"',
        "",
    ]
    dbs = []
    for db, items in by_db.items():
        if len(items) > 256:
            fail(f"{db}: {len(items)} rows, more than the undo record of season_coeffs.c holds (256)")
        ident = "".join(c if c.isalnum() else "_" for c in db)
        lines.append(f"static const SeasonCoeff coeffs_{ident}[] = {{")
        for nid, nation, found, real in items:
            lines.append(f'    {{{nid}, "{nation}", {{{", ".join(found)}}},')
            lines.append(f'        {{{", ".join(real)}}}}},')
        lines.append("};")
        dbs.append((db, ident, len(items)))
    lines += ["", "const SeasonCoeffDb season_coeff_dbs[] = {"]
    for db, ident, n in dbs:
        lines.append(f'    {{"{db}", coeffs_{ident}, {n}}},')
    lines += ["};", "", f"const int season_coeff_dbs_count = {len(dbs)};", ""]
    return "\n".join(lines)


def main(argv):
    if argv[:1] == ["--measure"] and len(argv) == 2:
        measure(argv[1])
        return 0
    want = table_text()
    if argv == ["--check"]:
        have = open(OUT, encoding="latin-1").read() if os.path.exists(OUT) else ""
        if have != want:
            print("make_coeff_table.py: season_coeffs_table.c is not the file that the TSV gives. "
                  "Run tools/seasons/make_coeff_table.py.", file=sys.stderr)
            return 1
        print("season_coeffs_table.c matches the coefficient table")
        return 0
    if argv:
        print(__doc__, file=sys.stderr)
        return 2
    with open(OUT, "w", encoding="latin-1", newline="\n") as f:
        f.write(want)
    print(f"wrote {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
