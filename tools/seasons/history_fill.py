#!/usr/bin/env python3
"""Matches the researched history rows with each season database before 2010/11 (0.6.0 slice 11c, Q347).

Usage:
    python3 -I tools/seasons/history_fill.py <3.9.68 Data folder> <folder of database folders> [--rules <folder>]
        [--write]

Reads the research table docs/research/history-0.6.0/<part>.tsv (brief.md there gives the columns) and, for
each database, decides which researched rows the game adds at a new game in the database's season:

- the row is `held = yes`, its label year is 2002 or later, and the database has the competition;
- the edition ended before the start (ended before July of the start year), or it is a calendar-year edition
  whose label is the start year and whose last match is in September or later (the 3.9.68 data holds the 2001
  season of the calendar-year leagues in the same way; see the slice doc).

Each kept row is compared with the rows that the database holds for that competition and label year after the
year shift of slice 11b (tools/seasons/history_gaps.py): `add` (the database has no row), `agree` (the same first
and second clubs), `differ` (another result). The clubs and national sides are matched by their full name in the
database, then by their short name, within the nation of the competition first.

Gap rows (row CONCACAF place 1, lead 4, 27 October 2026): a database of 2010/11 or later holds its own history,
which the game keeps. Where it holds no row at all of a competition in GAP_COMPETITIONS, the kept researched rows
of that competition are added (`add`, nothing cleared). The US Open Cup: Con_Champ::pick_clubs 0x4c11a0 gives
CONCACAF place 1 to its newest winner (0x4afd80), and September 2010 ODB, December 2010 SIM, April 2011 SIM and
March 2012 SIM hold no US Open Cup row.

--write writes docs/research/history-0.6.0/fill-report.tsv (counts per database) and unmatched.tsv (names that do
not match in a database). The files hold competition and club names only.
"""
import collections
import difflib
import glob
import os
import re
import sys
import unicodedata

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import history_gaps as hg  # noqa: E402
from history_gaps import s32  # noqa: E402

HIST = os.path.join(hg.ROOT, "docs", "research", "history-0.6.0")
NOT_PARTS = {"gaps.tsv", "targets.tsv", "fill-report.tsv", "unmatched.tsv", "matched-by-id.tsv", "aliases.tsv", "plan.tsv", "not-same.tsv", "reconcile.tsv", "skip.tsv", "no-winner.tsv"}
FIRST_FILL = 2001        # the first label year that the research fills: 2000/01 (Q354), then 2001/02 on
START_MONTH = 7          # a new game starts in July of its start year
CALENDAR_LAST_MONTH = 9  # a calendar-year edition of the start year that ends in this month or later counts
# Editions whose final is in July of a start year before the game's first day, with the day of the final. The
# research gives the month only. A new game's first news day is 10 July or later (seen: 10.7.2004 in Summer 2004,
# 15.7.2006 in Summer 2006), so a final on 9 July or earlier is before the start. Only start years 2004 and 2006
# have such a final (Euro 2004, World Cup 2006); for the other databases nothing changes.
FIRST_DAY_MIN = 10
EARLY_JULY = {("European Football Championship", 2004): 4,   # https://en.wikipedia.org/wiki/UEFA_Euro_2004_final
              ("FIFA World Cup", 2006): 9}                    # https://en.wikipedia.org/wiki/2006_FIFA_World_Cup_final


def research_rows():
    out = []
    for p in sorted(glob.glob(os.path.join(HIST, "*.tsv"))):
        b = os.path.basename(p)
        if b in NOT_PARTS or b.endswith(".check.tsv") or b.endswith(".check2.tsv") or b.endswith(".reconcile.tsv"):
            continue
        rows = hg.read_tsv(p)
        if rows and not {"kind", "competition", "label", "held", "first"} <= set(rows[0]):
            continue                    # not a part of the research table
        for r in rows:
            r["file"] = b
            out.append(r)
    return out


def ended_of(r):
    """(year, month) of the end of an edition, or a fallback from real_season when `ended` is unknown."""
    m = re.fullmatch(r"(\d{4})-(\d{2})", (r.get("ended") or "").strip())
    if m:
        return int(m.group(1)), int(m.group(2))
    years = [int(y) for y in re.findall(r"\d{4}", r.get("real_season", ""))]
    two = re.search(r"(\d{4})/(\d{2})\b", r.get("real_season", ""))
    if two:
        return int(two.group(1)[:2] + two.group(2)), 5       # a season Y/Y+1 ends in May (inference)
    if years:
        return max(years), 11                                 # a calendar year ends in November (inference)
    return int(r["label"]), 12


def kept(r, start):
    if r.get("held") != "yes":
        return False
    label = int(r["label"])
    if not (FIRST_FILL <= label <= start):
        return False
    y, m = ended_of(r)
    if (y, m) < (start, START_MONTH):
        return True
    day = EARLY_JULY.get((r.get("competition"), label))
    if (y, m) == (start, START_MONTH) and day is not None and day < FIRST_DAY_MIN:
        return True
    single = re.fullmatch(r"\s*(\d{4})\s*", r.get("real_season", ""))
    return bool(single) and int(single.group(1)) == label == start == y and m >= CALENDAR_LAST_MONTH


def norm(s):
    s = unicodedata.normalize("NFKD", s)
    s = "".join(c for c in s if not unicodedata.combining(c)).lower()
    return re.sub(r"[^a-z0-9]+", " ", s).strip()


def similar(a, b):
    return difflib.SequenceMatcher(None, norm(a), norm(b)).ratio()


def skips():
    """{(database, competition)} from skip.tsv: competitions whose name means another competition in that
    database, so the research rows do not fit it."""
    p = os.path.join(HIST, "skip.tsv")
    return {(r["database"], r["competition"]) for r in hg.read_tsv(p)} if os.path.isfile(p) else set()


def not_same():
    """{(research name, database name)} from not-same.tsv: pairs that the id match must not join."""
    p = os.path.join(HIST, "not-same.tsv")
    return {(r["name"], r["database_name"]) for r in hg.read_tsv(p)} if os.path.isfile(p) else set()


def aliases():
    """{research name: [other names]} from aliases.tsv (columns name, alias)."""
    out = collections.defaultdict(list)
    p = os.path.join(HIST, "aliases.tsv")
    if os.path.isfile(p):
        for r in hg.read_tsv(p):
            out[r["name"]].append(r["alias"])
    return out


class Names:
    """Club and national side ids of one database by name.

    Order: the full name, an alias of aliases.tsv, the short name; then the id that the name has in the 3.9.68
    data, when the club of that id in this database has a similar name (the Update Team databases keep most
    3.9.68 club ids and rename clubs: 3.9.68 7136 "Olympique Marseille" is "Olympique de Marseille" in
    September 2009). Within the nation of the competition first."""

    SIMILAR = 0.6

    def __init__(self, d, ref=None, alias=None):
        self.d, self.ref, self.alias = d, ref, alias or {}
        self.by = self.index(d)
        self.ref_by = self.index(ref) if ref else {}
        self.via_id = []                                   # (research name, database name) matched by id
        self.not_same = not_same()

    @staticmethod
    def index(d):
        by = collections.defaultdict(list)                 # (kind, name) -> [(nation, id)]
        nclub = d.count(0)
        for i, r in enumerate(d.rows(0)):
            nat = s32(r, 0x53)
            nn = d.text(4, nat) if nat >= 0 else ""
            full = r[4:55].split(b"\0")[0].decode("latin-1")
            short = r[0x38:0x52].split(b"\0")[0].decode("latin-1")
            by[("club", full)].append((nn, i))
            by[("short", short)].append((nn, i))
        for i, r in enumerate(d.rows(1)):
            full = r[4:55].split(b"\0")[0].decode("latin-1")
            by[("side", full)].append(("", nclub + i))
        return by

    @staticmethod
    def pick(by, names, nation, kind):
        for name in names:
            keys = [("side", name), ("club", name)] if kind == "nation" else [("club", name), ("short", name)]
            for k in keys:
                c = by.get(k)
                if c:
                    same = [i for n, i in c if n == nation]
                    return (same or [i for n, i in c])[0]
        return None

    def find(self, name, nation, kind):
        if not name or name == "none":
            return -1
        names = [name] + self.alias.get(name, [])
        i = self.pick(self.by, names, nation, kind)
        if i is not None or not self.ref:
            return i
        j = self.pick(self.ref_by, [name], nation, kind)
        if j is not None and similar(self.d.club(j), name) >= self.SIMILAR and \
                (name, self.d.club(j)) not in self.not_same:
            self.via_id.append((name, self.d.club(j)))
            return j
        return None


def resolve(names, name, nation, kind):
    """(database full name, its nation) of a researched name, ("", "") for an empty place, None if not found."""
    i = names.find(name, nation, kind)
    if i is None:
        return None
    if i < 0:
        return "", ""
    d = names.d
    r = d.row(0, i) if i < d.count(0) else d.row(1, i - d.count(0))
    nat = s32(r, 0x53) if i < d.count(0) else -1
    return d.club(i), d.text(4, nat) if nat >= 0 else ""


GAP_COMPETITIONS = ("US Open Cup",)


def gap_databases(dbroot):
    """[(name, start year, folder)] of the databases of 2010/11 and later, in season order."""
    sigs = {(int(r["club_size"]), r["club_sha256"], int(r["staff_size"]), r["staff_sha256"]): r
            for r in hg.read_tsv(hg.SIGS)}
    out = []
    for sub in sorted(os.listdir(dbroot)):
        p = os.path.join(dbroot, sub)
        r = hg.identify(p, sigs) if os.path.isdir(p) else None
        if not r or r["data_season"] in ("none", "n/a") or r["role"] in ("official version", "official base"):
            continue
        start = int(r["data_season"][:4])
        if start >= 2010:
            out.append((r["database"], start, p))
    return sorted(out, key=lambda x: (x[1], x[0]))


def gap_rows(name, start, folder, rows, ref, alias, plan, report):
    """The gap rows of one database of 2010/11 or later (see the top)."""
    d = hg.Data(folder)
    held = {d.comp(cid)[1] for cid, *_ in d.history()}
    comps = {}
    for cid in range(d.count(12)):
        c = d.comp(cid)
        if c:
            comps.setdefault(c[1], (cid, c[2]))
    names = Names(d, ref, alias)
    added = 0
    for comp in GAP_COMPETITIONS:
        if comp in held or comp not in comps:
            continue
        for r in rows:
            if r["competition"] != comp or r["nation"] != comps[comp][1] or not r.get("label", "").isdigit():
                continue
            if not kept(r, start):
                continue
            places = [resolve(names, r.get(f, ""), r["nation"], r["kind"]) for f in ("first", "second", "third", "host")]
            if places[0] is None or not places[0][0]:
                continue
            places = [v or ("", "") for v in places]
            plan.append("\t".join(["add", name, str(start), comp, r["label"], r.get("part", ""), "add"]
                                  + [x for v in places for x in v]))
            added += 1
    if added:
        report.append("\t".join(map(str, [name, start, added, added, added, 0, 0, 0, 0, 0, 0, 0])))
        print(report[-1], flush=True)


PLAN_COLS = ["action", "database", "start_year", "competition", "label", "part", "result", "first", "first_nation",
             "second", "second_nation", "third", "third_nation", "host", "host_nation"]


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    shifts_path, last_path = hg.SHIFTS, hg.LAST
    if "--rules" in argv:
        f = argv[argv.index("--rules") + 1]
        shifts_path, last_path = (os.path.join(f, os.path.basename(p)) for p in (hg.SHIFTS, hg.LAST))
    shifts = {r["database"]: r for r in hg.read_tsv(shifts_path)}
    last = {r["database"]: set(map(int, r["competition_ids"].split())) for r in hg.read_tsv(last_path)}
    rows = research_rows()
    ref = hg.Data(argv[0])
    alias = aliases()
    skip = skips()
    via = set()
    dbs = [("3.9.68", 2002, argv[0])] + hg.databases(argv[1])
    report = ["\t".join(["database", "start_year", "researched_rows_in_range", "kept", "add", "agree", "differ",
                         "no_competition", "skipped_first_unmatched", "unmatched_other", "database_rows_cleared",
                         "database_rows_kept_in_range"])]
    plan = ["\t".join(PLAN_COLS)]
    nowin = ["database\tcompetition\tlabel\tpart\twinner\tnation"]
    unmatched = collections.Counter()
    for name, start, folder in dbs:
        d = hg.Data(folder)
        comps = {}                      # the competitions that hold history rows in this database, by name
        for cid, *_ in d.history():
            c = d.comp(cid)
            if comps.get(c[1], cid) != cid:
                raise SystemExit(f"{name}: two competitions named {c[1]!r} hold history rows")
            comps[c[1]] = cid
        sh = shifts.get(name, {})
        cs, ls = int(sh.get("comp_history", 0) or 0), int(sh.get("last_rows", 0) or 0)
        held = collections.defaultdict(list)
        for cid, y, a, b, c, h in d.history():
            yy = y + cs if cs else (y + ls if (y == hg.LAST_ROW and cid in last.get(name, ())) else y)
            held[(cid, yy)].append((d.club(a), d.club(b)))
        names = Names(d, ref if name != "3.9.68" else None, alias)
        n = collections.Counter()
        covered, keep_db = set(), set()
        for r in rows:
            if not (r.get("label", "").isdigit() and FIRST_FILL <= int(r["label"]) <= start):
                continue
            n["range"] += 1
            cid = comps.get(r["competition"])
            # the nation of the research row must be the nation of the competition in this database ("China"
            # and "China PR" are one nation under two names in different databases, each with its own rows)
            if cid is not None and d.comp(cid)[2] != r["nation"]:
                cid = None
            if cid is None or (name, r["competition"]) in skip:
                n["nocomp"] += 1
                continue
            covered.add((r["competition"], int(r["label"])))
            if not kept(r, start):
                continue
            n["kept"] += 1
            places = [resolve(names, r.get(f, ""), r["nation"], r["kind"]) for f in ("first", "second", "third", "host")]
            for f, v in zip(("first", "second", "third", "host"), places):
                if v is None:
                    unmatched[(name, r["nation"], r[f])] += 1
                    n["um_first" if f == "first" else "um_other"] += 1
            if places[0] is None or not places[0][0]:
                # no winner in this database: the row is not added and the database keeps its rows of that year
                nowin.append("\t".join([name, r["competition"], r["label"], r.get("part", ""), r["first"], r["nation"]]))
                keep_db.add((r["competition"], int(r["label"])))
                continue
            places = [v or ("", "") for v in places]
            have = held.get((cid, int(r["label"])))
            res = "add" if not have else "agree" if (r["first"], r["second"]) in have else "differ"
            n[res] += 1
            plan.append("\t".join([ "add", name, str(start), r["competition"], r["label"], r.get("part", ""), res]
                                  + [x for v in places for x in v]))
        for comp, label in sorted(covered - keep_db):
            plan.append("\t".join(["clear", name, str(start), comp, str(label)] + [""] * (len(PLAN_COLS) - 5)))
            n["cleared"] += len(held.get((comps[comp], label), []))
        n["kept_db"] = sum(len(v) for (cid, y), v in held.items() if FIRST_FILL <= y <= start) - n["cleared"]
        via |= {(name,) + x for x in names.via_id}
        report.append("\t".join(map(str, [name, start, n["range"], n["kept"], n["add"], n["agree"], n["differ"],
                                          n["nocomp"], n["um_first"], n["um_other"], n["cleared"], n["kept_db"]])))
        print(report[-1], flush=True)
    for name, start, folder in gap_databases(argv[1]):
        gap_rows(name, start, folder, rows, ref, alias, plan, report)
    um = ["database\tnation\tname\trows"] + ["\t".join([a, b, c, str(k)]) for (a, b, c), k in sorted(unmatched.items())]
    print(f"{len(unmatched)} unmatched names (database, name)")
    vl = ["database\tname\tdatabase_name"] + ["\t".join(x) for x in sorted(via)]
    if "--write" in argv:
        for fn, lines in (("fill-report.tsv", report), ("unmatched.tsv", um), ("matched-by-id.tsv", vl),
                          ("plan.tsv", plan), ("no-winner.tsv", nowin)):
            with open(os.path.join(HIST, fn), "w", encoding="utf-8") as f:
                f.write("\n".join(lines) + "\n")
        print("wrote fill-report.tsv, unmatched.tsv, matched-by-id.tsv and plan.tsv in", HIST)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
