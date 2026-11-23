#!/usr/bin/env python3
"""Measures the date shifts of each known database against the 3.9.68 data (0.6.0 slice 11b, Q344).

Usage:
    python3 -I tools/seasons/measure_date_shifts.py <3.9.68 Data folder> <folder of database folders>
        [--write] [--only <name>,<name>] [--detail <name>]

--write writes docs/research/rules-0.6.0/databases-date-shifts.tsv (one row per known database: the decided
shifts, then the measures), databases-date-shifts-last-rows.tsv (the competitions whose 2001 history row
moves), databases-date-shifts-careers.tsv (one bit per staff row: the careers that keep their years) and
databases-date-shifts-nation-comps.tsv (the nation competitions whose rows need another shift than the other
nation rows of their database).
Without it the tool prints the rows. tools/seasons/make_dates_table.py makes the C table from them. The
decisions and the evidence: docs/research/2026-10-17-season-date-shifts.md.

The Update Team databases expect the original start in 2001, so they store dates early by some years. The
shift of a field is the number of years that the game adds so that the date is the real date again. The tool
measures it from the data, by field, against the 3.9.68 data (the base game, whose dates are real):

- birth: staff dates of birth (staff.dat +0x10). People with the same first name, second name and day of
  birth in both. The shift is the most common difference of the years.
- joined: the date joined (+0x3e) of the matched people who are at the same club (by name) in both.
- officials: officials dates of birth (officials.dat +0x0c), as birth.
- staff_history: the career rows (staff_history.dat +0x08) of the matched players who were born in 1976 or
  later, by club name and year (a measure only). The careers move with the birth shift; careers_kept lists the
  careers whose rows match the 3.9.68 rows of the same person better without the shift.
- comp_history: club and nation competition history (+0x08), by competition, year and the first club.
- nation_history: the nation competition history alone (nation_comp_history.dat), the same way (0.6.2 slice 7,
  open items 20 and 39). The Update Team databases keep the nation rows at another offset than the club rows
  in many databases from 2010/11 on: October 2023 keeps the World Cup of 2018 at 1996, two years earlier than
  its club rows, and October 2014 one year later. April 2011 SIM and March 2012 SIM keep the real years.
  A nation competition whose own best shift (at least NATION_COMP_MIN rows, NATION_COMP_SHARE of them) is
  another than the shift of its database gets a row of its own (World Cup and Euros of April 2015, April 2016
  and October 2015).
- staff_comp_history: the award history (+0x08), by award, year and the winner's names.
- comp_history and last rows: when the best shift of the competition history is 0, the last champions sit in
  the 2001 row; updated_last_rows names the competitions whose 2001 row moves.
- the contract end (+0x46) and the dates of the national job (+0x29, +0x31) have no common rows to compare.
  Their shift is the data season minus 2001: every Update Team database keeps the contract years of a 2001
  start (contract_years gives the most common contract end years of each database).

A database is named by its content (docs/research/rules-0.6.0/databases-signatures.tsv), never by its folder
name. Folders that the table does not know are left out. The tool reads the folders only.
"""
import collections
import csv
import hashlib
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
RULES = os.path.join(ROOT, "docs", "research", "rules-0.6.0")
SIGS = os.path.join(RULES, "databases-signatures.tsv")
OUT = os.path.join(RULES, "databases-date-shifts.tsv")
OUT_LAST = os.path.join(RULES, "databases-date-shifts-last-rows.tsv")
OUT_KEEP = os.path.join(RULES, "databases-date-shifts-careers.tsv")
OUT_NATION = os.path.join(RULES, "databases-date-shifts-nation-comps.tsv")

SIZES = {0: 0x245, 1: 0x245, 2: 0x3a, 3: 0xc6, 4: 0x122, 5: 0x4e, 6: 0x9d, 7: 0x2b, 9: 0x44, 10: 0x46,
         11: 0x65, 12: 0x6b, 13: 0x3c, 14: 0x3c, 15: 0x3c, 16: 0x6b, 17: 0x11, 18: 0x3a, 19: 0x1a, 20: 0x1a,
         21: 0x38}
UNSET = 1900          # the year of a date that the data leaves empty
FIRST = 2001          # the start year that the Update Team databases expect
LAST_ROW = 2001       # the history year where those databases keep the last champions
# The last rows that the report shows, for the check against the real champions.
CHECKS = (("club", "English Premier Division"), ("club", "European Champions Cup"),
          ("club", "Italian Serie A"), ("club", "Spanish First Division"), ("nation", "FIFA World Cup"),
          ("nation", "European Football Championship"))
OFFICIAL = ("official version", "official base")
NATION_COMP_MIN = 3    # matched rows that a shift of one nation competition needs
NATION_COMP_SHARE = 0.6   # renamed nations (West Germany, Soviet Union) do not match: about 70 % of the rows do
KEEP_MIN = 1000       # fewer kept careers than this are not listed: they all shift with the birth shift


def s32(b, o):
    return struct.unpack_from("<i", b, o)[0]


def s16(b, o):
    return struct.unpack_from("<h", b, o)[0]


def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


class Data:
    """The tables of one data folder, as the files hold them (index.dat order, record sizes of the loader)."""

    def __init__(self, folder):
        idx = open(os.path.join(folder, "index.dat"), "rb").read()
        files = {}
        self.t = {}
        for e in range(22):
            o = 8 + e * 0x43
            name = idx[o:o + 51].split(b"\0")[0].decode("latin-1")
            tid, cnt, off, fmt = struct.unpack_from("<iiii", idx, o + 0x33)
            if tid not in SIZES:
                continue
            rs = 0x6e if (tid == 6 and fmt == 2) else SIZES[tid]
            if name not in files:
                files[name] = open(os.path.join(folder, name), "rb").read()
            self.t[tid] = (files[name][off:off + cnt * rs], cnt, rs)

    def count(self, tid):
        return self.t.get(tid, (b"", 0, 1))[1]

    def row(self, tid, i):
        buf, cnt, rs = self.t.get(tid, (b"", 0, 1))
        return buf[i * rs:(i + 1) * rs] if 0 <= i < cnt else None

    def rows(self, tid):
        buf, cnt, rs = self.t.get(tid, (b"", 0, 1))
        for i in range(cnt):
            yield i, buf[i * rs:(i + 1) * rs]

    def text(self, tid, i, at=0):
        r = self.row(tid, i) if i is not None else None
        return r[at:at + 51].split(b"\0")[0].decode("latin-1") if r else ""

    def person(self, tid, i):
        """(first name, second name) of a staff or official row."""
        r = self.row(tid, i)
        if not r:
            return None
        return self.text(13, s32(r, 4)), self.text(14, s32(r, 8))

    def club(self, i):
        """The name of a club id: the clubs, then the national sides (nat_club.dat)."""
        if 0 <= i < self.count(0):
            return self.text(0, i, 4)
        return self.text(1, i - self.count(0), 4)


def mode(diffs):
    """(shift, matches, share) of a list of year differences, or (None, 0, 0) when nothing matched."""
    c = collections.Counter(diffs)
    if not c:
        return None, 0, 0.0
    s, n = c.most_common(1)[0]
    return s, len(diffs), n / len(diffs)


def birth_keys(d, tid, date_at):
    """(first, second, day) -> (year, row) for rows with a set date of birth; keys seen twice are dropped."""
    out, twice = {}, set()
    for i, r in d.rows(tid):
        year = u16(r, date_at + 2)
        if year <= UNSET:
            continue
        p = d.person(tid, i)
        if not p or not p[0] or not p[1]:
            continue
        k = (p[0], p[1], s16(r, date_at))
        if k in out:
            twice.add(k)
        out[k] = (year, i)
    for k in twice:
        del out[k]
    return out


def best_shift(ref_set, db_set, lo=-5, hi=40):
    """The shift s that makes the most (year + s, key) of db_set fall in ref_set: (s, matches, share). The
    share is the matches of s against the rows of db_set whose key the reference has."""
    keys = {k for y, k in ref_set}
    usable = [(y, k) for y, k in db_set if k in keys]
    best = (None, 0)
    for s in range(lo, hi + 1):
        n = sum(1 for y, k in usable if (y + s, k) in ref_set)
        if n > best[1]:
            best = (s, n)
    return best[0], best[1], best[1] / max(1, len(usable))


def comp_rows(d, kind):
    """{competition id: (name, set of (year, winner))} of the club or nation competition history. The nation
    competitions follow the club competitions in one id range (club_comp.dat, then nation_comp.dat)."""
    th, tc, base = (19, 12, 0) if kind == "club" else (20, 16, d.count(12))
    out = {}
    for i, r in d.rows(th):
        cid = s32(r, 4)
        name = d.text(tc, cid - base, 4)
        w = d.club(s32(r, 0xa))
        if name and w:
            out.setdefault(cid, (name, set()))[1].add((u16(r, 8), w))
    return out


def award_rows(d):
    """{award name: set of (year, winner)}; each place is 16 bytes from +0x0a: first name id, second name id,
    staff id, club id."""
    out = collections.defaultdict(set)
    for i, r in d.rows(18):
        name = d.text(11, s32(r, 4), 4)
        w = (d.text(13, s32(r, 0xa)) + " " + d.text(14, s32(r, 0xe))).strip()
        if name and w:
            out[name].add((u16(r, 8), w))
    return out


def measure(ref, d):
    res = {}
    rk, dk = birth_keys(ref, 6, 0x10), birth_keys(d, 6, 0x10)
    common = [k for k in dk if k in rk]
    res["birth"] = mode([rk[k][0] - dk[k][0] for k in common])

    def club_of(dd, r):
        c = s32(r, 0x39)
        return dd.text(0, c, 4) if 0 <= c < dd.count(0) else None
    jd = []
    for k in common:
        a, b = ref.row(6, rk[k][1]), d.row(6, dk[k][1])
        ca = club_of(ref, a)
        if ca and ca == club_of(d, b) and u16(a, 0x40) > UNSET and u16(b, 0x40) > UNSET \
                and s16(a, 0x3e) == s16(b, 0x3e):
            jd.append(u16(a, 0x40) - u16(b, 0x40))
    res["joined"] = mode(jd)

    rko, dko = birth_keys(ref, 7, 0x0c), birth_keys(d, 7, 0x0c)
    res["officials"] = mode([rko[k][0] - v[0] for k, v in dko.items() if k in rko])

    young = {k for k in common if rk[k][0] >= 1976}

    def career(dd, keys, rows):
        of = {rows[k][1]: k for k in keys}
        out = set()
        for i, r in dd.rows(17):
            k = of.get(s32(r, 4))
            if k is not None:
                out.add((u16(r, 8), (k, dd.club(s32(r, 0xa)))))
        return out
    res["staff_history"] = best_shift(career(ref, young, rk), career(d, young, dk))

    # Careers that keep the years of the 3.9.68 data: the person's rows (year, club, the three bytes after
    # the club) match more 3.9.68 rows of the same person without the birth shift than with it.
    sb = res["birth"][0] or 0

    def rows_by_staff(dd):
        h = collections.defaultdict(set)
        for i, r in dd.rows(17):
            h[s32(r, 4)].add((u16(r, 8), dd.club(s32(r, 0xa)), r[0xe:0x11]))
        return h
    keep = set()
    if sb:
        hr, hd = rows_by_staff(ref), rows_by_staff(d)
        for k in common:
            if rk[k][0] - dk[k][0] != sb:
                continue
            a, b = hr.get(rk[k][1]), hd.get(dk[k][1])
            if not a or not b:
                continue
            n0 = sum(1 for x in b if x in a)
            ns = sum(1 for y, c, st in b if (y + sb, c, st) in a)
            if n0 > ns:
                keep.add(dk[k][1])
    res["careers_kept"] = keep
    res["staff_count"] = d.count(6)

    contracts = collections.Counter(u16(r, 0x48) for i, r in d.rows(6) if s32(r, 0x39) >= 0 and u16(r, 0x48) > UNSET)
    res["contract_years"] = [y for y, n in contracts.most_common(3)]

    res["club"], res["nation"] = comp_rows(d, "club"), comp_rows(d, "nation")
    rc, rn = comp_rows(ref, "club"), comp_rows(ref, "nation")
    allr, alld = set(), set()
    for mine, theirs in ((res["club"], rc), (res["nation"], rn)):
        names = {v[0]: v[1] for v in theirs.values()}
        for name, rows in mine.values():
            if name in names:
                allr |= {(y, (name, w)) for y, w in names[name]}
                alld |= {(y, (name, w)) for y, w in rows}
    res["comp_history"] = best_shift(allr, alld)
    res["ref_club"], res["ref_nation"] = rc, rn
    # The nation rows alone, and each nation competition alone.
    names = {v[0]: v[1] for v in rn.values()}
    nr, nd, res["nation_comps"] = set(), set(), {}
    for cid, (name, rows) in res["nation"].items():
        if name in names:
            r = {(y, (name, w)) for y, w in names[name]}
            dd = {(y, (name, w)) for y, w in rows}
            nr |= r
            nd |= dd
            res["nation_comps"][cid] = (name, best_shift(r, dd))
    res["nation_history"] = best_shift(nr, nd)

    ra, da = award_rows(ref), award_rows(d)
    alla, alld = set(), set()
    for name, rows in da.items():
        if name in ra:
            alla |= {(y, (name, w)) for y, w in ra[name]}
            alld |= {(y, (name, w)) for y, w in rows}
    res["staff_comp_history"] = best_shift(alla, alld)
    return res


def updated_last_rows(m):
    """The competitions whose history row of LAST_ROW holds the last champions of the database: the
    competition is not in the 3.9.68 data, or its winner of LAST_ROW is another club, or the 3.9.68 data has a
    row of LAST_ROW + 1 that the database dropped. Otherwise the row is the 2000/01 champion of the 3.9.68
    data and stays."""
    out = []
    for kind in ("club", "nation"):
        ref = {v[0]: v[1] for v in m["ref_" + kind].values()}
        for cid, (name, rows) in sorted(m[kind].items()):
            w = {x for y, x in rows if y == LAST_ROW}
            if not w:
                continue
            r = ref.get(name)
            if r is None:
                out.append(cid)
                continue
            rw = {x for y, x in r if y == LAST_ROW}
            dropped = any(y == LAST_ROW + 1 for y, x in r) and not any(y == LAST_ROW + 1 for y, x in rows)
            if not (w & rw) or dropped:
                out.append(cid)
    return out


def decide(sig, m):
    """The shifts that the game applies, from the measures. Returns (row of values, last row ids)."""
    season = int(sig["data_season"][:4])
    if sig["role"] in OFFICIAL:
        return dict(birth=0, joined=0, contract=0, officials=0, staff_history=0, comp_history=0, last_rows=0,
                    staff_comp_history=0, careers_kept=0, nation_history=0), [], []
    contract = season - FIRST
    v = dict(birth=m["birth"][0] or 0, contract=contract, officials=m["officials"][0] or 0,
             staff_comp_history=0)
    j = m["joined"]
    v["joined"] = j[0] if (j[0] is not None and j[1] >= 1000 and j[2] >= 0.9) else contract
    # Careers follow the dates of birth. The careers that the database left as the 3.9.68 data has them
    # keep their years (careers_kept), when there are enough of them to matter.
    v["staff_history"] = v["birth"]
    v["careers_kept"] = len(m["careers_kept"]) if len(m["careers_kept"]) >= KEEP_MIN else 0
    c = m["comp_history"]
    v["comp_history"] = c[0] if c[0] is not None else 0
    last = []
    if v["comp_history"] == 0 and contract > 0:
        last = updated_last_rows(m)
    v["last_rows"] = contract if last else 0
    # The nation rows: their own shift where the competition history moves at all (the databases from 2010/11),
    # and a row for each nation competition whose own shift is another.
    n = m["nation_history"]
    v["nation_history"] = n[0] if v["comp_history"] and n[0] is not None else v["comp_history"]
    comps = []
    if v["comp_history"]:
        for cid, (name, (s, k, share)) in sorted(m["nation_comps"].items()):
            if s is not None and s != v["nation_history"] and k >= NATION_COMP_MIN and share >= NATION_COMP_SHARE:
                comps.append((cid, s, name))
    return v, last, comps


def signatures():
    out = {}
    with open(SIGS, newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            out[(int(r["club_size"]), r["club_sha256"], int(r["staff_size"]), r["staff_sha256"])] = r
    return out


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def identify(folder, sigs):
    c, s = os.path.join(folder, "club.dat"), os.path.join(folder, "staff.dat")
    if not (os.path.isfile(c) and os.path.isfile(s)):
        return None
    return sigs.get((os.path.getsize(c), sha(c), os.path.getsize(s), sha(s)))


def fmt(x):
    s, n, share = x
    return ["%+d" % s if s is not None else "none", str(n), "%.2f" % share]


DECIDED = ["birth", "joined", "contract", "officials", "staff_history", "careers_kept", "comp_history",
           "last_rows", "staff_comp_history", "nation_history"]
MEASURED = ["birth", "joined", "officials", "staff_history", "comp_history", "staff_comp_history",
            "nation_history"]


def last_champions(m, v, last, nation):
    """The last row of the CHECKS competitions after the shift: 'name year winner'."""
    out = []
    for kind, name in CHECKS:
        for cid, (n, rows) in m[kind].items():
            if n != name:
                continue
            y, w = max(rows)

            def moved(year):
                if v["comp_history"]:
                    return year + next((s for c, s, x in nation if c == cid), v["nation_history"]) \
                        if kind == "nation" else year + v["comp_history"]
                return year + v["last_rows"] if (year == LAST_ROW and cid in last) else year
            y, w = max((moved(a), b) for a, b in rows)
            out.append(f"{name} {y} {w}")
    return "; ".join(out)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    ref = Data(argv[0])
    detail = argv[argv.index("--detail") + 1] if "--detail" in argv else None
    only = argv[argv.index("--only") + 1].split(",") if "--only" in argv else None
    sigs = signatures()
    found = []
    for sub in sorted(os.listdir(argv[1])):
        p = os.path.join(argv[1], sub)
        if not os.path.isdir(p):
            continue
        r = identify(p, sigs)
        if r is None or r["data_season"] == "none" or (only and r["database"] not in only):
            continue
        found.append((r, p))
    found.sort(key=lambda x: (x[0]["data_season"], x[0]["database"]))
    cols = ["database", "data_season", "role"] + DECIDED
    for k in MEASURED:
        cols += [f"measured_{k}", f"{k}_matches", f"{k}_share"]
    cols += ["contract_years", "last_champions"]
    lines, last_lines = ["\t".join(cols)], ["database\tcompetition_ids"]
    keep_lines = ["database\tstaff_rows\tkept_bitmap"]
    nation_lines = ["database\tcompetition_id\tshift\tcompetition"]
    for r, p in found:
        m = measure(ref, Data(p))
        v, last, nation = decide(r, m)
        row = [r["database"], r["data_season"], r["role"]] + ["%d" % v[k] for k in DECIDED]
        for k in MEASURED:
            row += fmt(m[k])
        row += [" ".join(map(str, m["contract_years"])), last_champions(m, v, last, nation)]
        nation_lines += [f"{r['database']}\t{cid}\t{s}\t{name}" for cid, s, name in nation]
        lines.append("\t".join(row))
        if v["careers_kept"]:
            bits = bytearray((m["staff_count"] + 7) // 8)
            for i in m["careers_kept"]:
                bits[i >> 3] |= 1 << (i & 7)
            keep_lines.append(f"{r['database']}\t{m['staff_count']}\t{bits.hex()}")
        if last:
            last_lines.append(r["database"] + "\t" + " ".join(map(str, last)))
        print(lines[-1], flush=True)
        if detail == r["database"]:
            for cid in last:
                n = (m["club"].get(cid) or m["nation"].get(cid))[0]
                print(f"  last row moves: {cid} {n}")
    if "--write" in argv:
        with open(OUT, "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
        with open(OUT_LAST, "w", encoding="utf-8") as f:
            f.write("\n".join(last_lines) + "\n")
        with open(OUT_KEEP, "w", encoding="utf-8") as f:
            f.write("\n".join(keep_lines) + "\n")
        with open(OUT_NATION, "w", encoding="utf-8") as f:
            f.write("\n".join(nation_lines) + "\n")
        print(f"wrote {OUT}, {OUT_LAST}, {OUT_KEEP} and {OUT_NATION}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
