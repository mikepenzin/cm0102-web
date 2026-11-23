#!/usr/bin/env python3
"""Matches the researched history rows with each season database before 2010/11 (0.6.0 slice 11c, Q347).

Usage:
    python3 -I tools/seasons/history_fill.py <3.9.68 Data folder> <folder of database folders> [--rules <folder>]
        [--write]

Reads the research table docs/research/history-0.6.0/<part>.tsv (the research brief, kept outside the repo, gives the columns) and, for
each database, decides which researched rows the game adds at a new game in the database's season:

- the row is `held = yes`, its label year is 2002 or later, and the database has the competition;
- the edition ended before the start (ended before July of the start year), or it is a calendar-year edition
  whose label is the start year and whose last match is in September or later (the 3.9.68 data holds the 2001
  season of the calendar-year leagues in the same way; see the slice doc). A row with a game_season (Q277: an
  edition that the game plays in another season than in real life) ends at the end of that season (ended_of).

Each kept row is compared with the rows that the database holds for that competition and label year after the
year shift of slice 11b (tools/seasons/history_gaps.py): `add` (the database has no row), `agree` (the same first
and second clubs), `differ` (another result). The clubs and national sides are matched by their full name in the
database, then by their short name, within the nation of the competition first.

Gap rows (row CONCACAF place 1, lead 4, 27 October 2026): a database of 2010/11 or later holds its own history,
which the game keeps. Where it holds no row at all of a competition in GAP_COMPETITIONS, the kept researched rows
of that competition are added (`add`, nothing cleared). The US Open Cup: Con_Champ::pick_clubs 0x4c11a0 gives
CONCACAF place 1 to its newest winner (0x4afd80), and September 2010 ODB, December 2010 SIM, April 2011 SIM and
March 2012 SIM hold no US Open Cup row. Where such a database holds a row of a GAP_COMPETITIONS competition whose
winner is a club of another nation than the competition's, the row is cleared and the researched row of its label
added (`clear`, `add` with result `differ`): eleven databases, April 2021 to May 2026, name club 977, Atlanta of
Argentina, as the 2019 US Open Cup winner in place of Atlanta United FC, and the CONCACAF Champions Cup of 2020 to
2022 starts took it (0.6.2 open item 49). Only clubs of the club table are tested (a national side has no nation
field there). Note: a --write run on a database folder that lacks some databases of the first run leaves their
rows out of plan.tsv; the rows of item 49 were added from this function's output on test-fixtures/databases.

Official data sets (0.6.2 open item 149): 3.9.60, 3.9.65 and 3.9.68 keep the history of 2001 in every start year,
so the same rules give their plan for every start year from 2002 (3.9.68: 2003, after its 2002/03 tile) to the
year after the last researched edition ended. plan-official.tsv gives each row once, with the first and the last
start year for which it holds (last_start empty: every later one). 3.9.60 and 3.9.65 come from the folder of
databases (official versions, by signature).

--write writes docs/research/history-0.6.0/fill-report.tsv (counts per database) and unmatched.tsv (names that do
not match in a database), plan.tsv, plan-official.tsv and no-winner.tsv. The files hold competition and club names
only.
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
NOT_PARTS = {"gaps.tsv", "targets.tsv", "fill-report.tsv", "unmatched.tsv", "matched-by-id.tsv", "aliases.tsv", "plan.tsv", "not-same.tsv", "reconcile.tsv", "skip.tsv", "no-winner.tsv", "july-days.tsv", "plan-official.tsv"}
FIRST_FILL = 2001        # the first label year that the research fills: 2000/01 (Q354), then 2001/02 on
START_MONTH = 7          # a new game starts in July of its start year
CALENDAR_LAST_MONTH = 9  # a calendar-year edition of the start year that ends in this month or later counts
# Editions whose final is in July of a start year before the game's first day, with the day of the final. The
# research gives the month only. A new game's first news day is 10 July or later (seen: 10.7.2004 in Summer 2004,
# 15.7.2006 in Summer 2006), so a final on 9 July or earlier is before the start.
FIRST_DAY_MIN = 10
EARLY_JULY = {("European Football Championship", 2004): 4,   # https://en.wikipedia.org/wiki/UEFA_Euro_2004_final
              ("FIFA World Cup", 2006): 9}                    # https://en.wikipedia.org/wiki/2006_FIFA_World_Cup_final
# The day of the deciding match of every other researched edition that ended in July (0.6.2 open item 149):
# july-days.tsv, one source each (RSSSF, RSSSF Brasil, Wikipedia). Since every start year of an official data set
# gets rows, the final of 1 to 9 July of any such edition decides its first start year.
JULY_DAYS = os.path.join(HIST, "july-days.tsv")
for _r in hg.read_tsv(JULY_DAYS) if os.path.isfile(JULY_DAYS) else []:
    _m = re.fullmatch(r"\d{4}-07-(\d{2})", _r["final_date"])
    if _m:
        EARLY_JULY.setdefault((_r["competition"], int(_r["label"])), int(_m.group(1)))
# Finals that the game does not play in the July of its start year: the World Cup and the Euro. The finals code fills
# an edition only for a start in the finals' season or the one before (real_draws, season_intl_rules.c; Q388), and
# the 2010 gate saves of 0.6.0 hold no game row of the 2010 World Cup (GAP_FINALS below). So such an edition whose
# final is in July of the start year counts at that start (0.6.2 open item 149: World Cup 2010 in a 2010 game).
JULY_FINALS = ("FIFA World Cup", "European Football Championship")


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
    """(year, month) of the end of an edition, or a fallback from real_season when `ended` is unknown.

    game_season (Q277, 0.6.2 open item 170): a late edition plays at its normal time in the season it belongs to, so
    the game plays it in the season Y/Y+1 of the rules table (docs/research/rules-0.6.0/int-continental.tsv), not at
    its real end. Such an edition ends before the start in July Y+1: a game of Y+1 never plays it (season_intl_first,
    season_intl_rules.c, takes the first edition of the start season or later)."""
    g = re.fullmatch(r"(\d{4})/(\d{2})", (r.get("game_season") or "").strip())
    if g:
        return int(g.group(1)[:2] + g.group(2)), START_MONTH - 1
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
    if (y, m) == (start, START_MONTH) and r.get("competition") in JULY_FINALS:
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
# Repairs of a database of 2010/11 or later (0.6.2 slice 7, open item 20): (database, competition, label years
# whose rows are cleared, after the year shift). September 2010 ODB holds no World Cup 2010 row; its last row (1998
# in the file, 2007 after the nation shift of +9) names France alone, with no other place. GAP_FINALS then adds
# the researched 2010 row.
# Autumn 2013 holds a Trophee des Champions 2011 of Marseille against Lyon, a match that was not played: the 2011
# match was Marseille 5-4 Lille in Tangier (en.wikipedia.org, Trophee des Champions, read 7 November 2026; open item
# 176). The row goes, and LAST_ROW_MOVES puts the real 2011 row there.
REPAIRS = (("September 2010 ODB", "FIFA World Cup", (2007,)),
           ("Autumn 2013", "French Champions Trophy", (2011,)))
# Last rows one year early (0.6.2 open items 59 and 83): some databases of 2010/11 or later keep the last season of a
# competition at the label of the season before, beside it, and leave the label of the start empty. (database,
# competition, label after the shift, runner-up of the row that moves, its label). Each move names the real season
# of the row: Autumn 2013 holds 2011/12 (Dinamo Zagreb / Hajduk Split, Olympiacos / Panathinaikos) and 2012/13
# (Dinamo Zagreb / Lokomotiva, Olympiacos / PAOK) both at 2012; the final tables: en.wikipedia.org, 2012-13 Prva HNL
# and 2012-13 Super League Greece. Also the Northern Irish Premier Division (2012/13 Cliftonville / Crusaders), First
# Division (2012/13 Ards; 2011/12 Ballinamallard United) and League Cup (2012/13 Cliftonville 4-0 Crusaders on
# 26 January 2013; 2011/12 Crusaders 1-0 Coleraine): en.wikipedia.org, 2012-13 IFA Premiership, 2012-13 IFA
# Championship and 2012-13 Irish League Cup. The Southern League Premier Division and the Trophee des Champions of
# that database hold rows that name other clubs than the real ones, which no move mends (open item 59). The file order of such rows does not tell which is the newer (German Cup,
# October 2016), so each move is a researched row of this list.
# Open item 176 (sources read 7 November 2026). The Southern League Premier Division of Autumn 2013 holds 2012/13
# (Leamington, champions, and Gosport Borough, the play-off winners on 6 May 2013) at 2012 beside Woking / Dartford;
# en.wikipedia.org, 2012-13 Southern Football League. Its older rows name the Conference South clubs of each season,
# the database maker's own rows, which stay. The French Champions Trophy (Trophee des Champions, a match in July or
# August) holds the 2011 match (Marseille 5-4 Lille) and the 2012 match (Lyon against Montpellier, 2-2, 4-2 on
# penalties) both at 2012; en.wikipedia.org, Trophee des Champions. The Lille row moves to 2011, whose own row
# REPAIRS clears. A move may go to a label that REPAIRS clears.
LAST_ROW_MOVES = (("Autumn 2013", "Croatian First Division", 2012, "NK Lokomotiva Zagreb", 2013),
                  ("Autumn 2013", "Greek National A Division", 2012, "PAOK Salonika", 2013),
                  ("Autumn 2013", "Northern Irish League Premier Division", 2012, "Crusaders", 2013),
                  ("Autumn 2013", "Northern Irish League First Division", 2012, "", 2013),
                  ("Autumn 2013", "Northern Irish League Cup", 2012, "Crusaders", 2013),
                  ("Autumn 2013", "English Southern League Premier Division", 2012, "Gosport Borough", 2013),
                  ("Autumn 2013", "French Champions Trophy", 2012, "Lille Olympique SC", 2011))
# Finals that a database of 2010/11 or later may lack (0.6.2 slice 7, open item 20): each researched edition whose
# label is missing after the nation shift is added, when its final is before the start. A final in July of the
# start year counts: the game plays no finals in the July of its start (no game row of the 2010 World Cup in the
# 2010 gate saves of 0.6.0). October 2023 holds no World Cup 2022 row, April 2011 SIM and March 2012 SIM no World
# Cup or Euro row after 2000. October 2026 holds no World Cup 2026 row.
# The Africa Cup of Nations (0.6.2 slice 21, open item 170): every database of 2010/11 or later names it "African Cup
# of Nations", and many lack an edition before their start: October 2026 the 2025 row, October 2021 the 2021 row
# (played in 2020/21 under Q277, in January 2022 in real life), October 2023 the 2021 and 2023 rows. The research
# holds every edition from 2002 to 2025 (nations.tsv), so the rule holds for every database.
# The FIFA Club World Cup (0.6.2 open item 197): a database keeps the editions to the one before it was made, so
# October 2018 lacks 2017, October 2024 lacks 2023, and December 2025 and May 2026 lack 2025 (Chelsea / Paris
# Saint-Germain, 13 July 2025). The research holds every edition (intl-clubs-cwc.tsv).
# The OFC Nations Cup (0.7.3, release set full2): September 2010 ODB holds no 2008 row and October 2016 no 2016 row,
# so a 2010 or 2016 start has a gap before its first edition. The research holds every edition to 2024 (2020
# cancelled).
GAP_FINALS = ("FIFA World Cup", "European Football Championship", "African Cup of Nations",
              "FIFA Club World Championship", "Oceania Nations Cup")
# The same for one database only: October 2026 holds no row of the Argentine Apertura 2026 (Belgrano, 24 May 2026,
# before the start). This is open item 98. The years of a club competition get the club shift (comp_history), the
# years of a nation competition the nation shift.
GAP_FINALS_OF = {"October 2026": ("Argentine Premier Division",)}


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


def club_nation(d, i):
    """The nation name of club index i of a database, "" for a national side or none."""
    r = d.row(0, i) if 0 <= i < d.count(0) else None
    nat = s32(r, 0x53) if r else -1
    return d.text(4, nat) if nat >= 0 else ""


def nation_shifts(shifts_path):
    """{database: (nation shift, {competition id: shift})} of databases-date-shifts*.tsv."""
    comps = {}
    for r in hg.read_tsv(os.path.join(os.path.dirname(shifts_path), "databases-date-shifts-nation-comps.tsv")):
        comps.setdefault(r["database"], {})[int(r["competition_id"])] = int(r["shift"])
    return {r["database"]: (int(r.get("nation_history") or 0), comps.get(r["database"], {}))
            for r in hg.read_tsv(shifts_path)}


def gap_rows(name, start, folder, rows, ref, alias, plan, report, shift, nshift):
    """The gap rows of one database of 2010/11 or later (see the top). shift: the database's comp_history years;
    nshift: its nation shifts (nation_shifts)."""
    d = hg.Data(folder)
    held = {d.comp(cid)[1] for cid, *_ in d.history()}
    comps = {}
    for cid in range(d.count(12) + d.count(16)):     # the club competitions, then the nation competitions
        c = d.comp(cid)
        if c:
            comps.setdefault(c[1], (cid, c[2]))
    names = Names(d, ref, alias)
    added = differ = 0
    for comp in GAP_COMPETITIONS:
        if comp not in comps:
            continue
        wrong = None                                    # None: no row at all; else the labels of foreign winners
        if comp in held:
            wrong = {str(y + shift) for cid, y, a, *_ in d.history()
                     if cid == comps[comp][0] and club_nation(d, a) not in ("", comps[comp][1])}
            if not wrong:
                continue
        for r in rows:
            if r["competition"] != comp or r["nation"] != comps[comp][1] or not r.get("label", "").isdigit():
                continue
            if not kept(r, start) or (wrong is not None and r["label"] not in wrong):
                continue
            places = [resolve(names, r.get(f, ""), r["nation"], r["kind"]) for f in ("first", "second", "third", "host")]
            if places[0] is None or not places[0][0]:
                continue
            places = [v or ("", "") for v in places]
            if wrong is not None:
                plan.append("\t".join(["clear", name, str(start), comp, r["label"]] + [""] * (len(PLAN_COLS) - 5)))
            plan.append("\t".join(["add", name, str(start), comp, r["label"], r.get("part", ""),
                                   "add" if wrong is None else "differ"] + [x for v in places for x in v]))
            added += 1
            differ += wrong is not None
    cleared = set()
    for db, comp, clears in REPAIRS:
        if db != name or comp not in comps:
            continue
        for label in clears:
            plan.append("\t".join(["clear", name, str(start), comp, str(label)] + [""] * (len(PLAN_COLS) - 5)))
            cleared.add((comp, label))
    ns, own = nshift
    for db, comp, label, second, to in LAST_ROW_MOVES:
        if db != name or comp not in comps:
            continue
        cid, nation = comps[comp]
        sh = own.get(cid, ns) if d.comp(cid)[0] == "nation" else shift
        at = [(a, b, c, h) for c2, y, a, b, c, h in d.history() if c2 == cid and y + sh == label]
        if sum(1 for x in at if d.club(x[1]) == second) != 1 or len(at) < 2:
            raise SystemExit(f"{name}: {comp} {label} holds no pair of rows with the runner-up {second}")
        if (comp, to) not in cleared and any(c2 == cid and y + sh == to for c2, y, *_ in d.history()):
            raise SystemExit(f"{name}: {comp} already holds a row of {to}, so no row moves there")
        plan.append("\t".join(["clear", name, str(start), comp, str(label)] + [""] * (len(PLAN_COLS) - 5)))
        for x in at:
            places = [(d.club(i), club_nation(d, i)) if i >= 0 else ("", "") for i in x]
            plan.append("\t".join(["add", name, str(start), comp, str(to if d.club(x[1]) == second else label), "",
                                   "differ"] + [v for p in places for v in p]))
            added += 1
            differ += 1
    have = collections.defaultdict(set)         # the labels of each competition after its shift
    for cid, y, *_ in d.history():
        c = d.comp(cid)
        if c:
            have[c[1]].add(y + (own.get(cid, ns) if c[0] == "nation" else shift))
    for comp in GAP_FINALS + GAP_FINALS_OF.get(name, ()):
        if comp not in comps:
            continue
        for r in rows:
            if r["competition"] != comp or r["nation"] != comps[comp][1] or not r.get("label", "").isdigit() \
                    or r.get("held") != "yes":
                continue
            label = int(r["label"])
            if not (FIRST_FILL <= label <= start) or ended_of(r) > (start, START_MONTH) or \
                    (label in have[comp] and (comp, label) not in cleared):
                continue
            places = [resolve(names, r.get(f, ""), r["nation"], r["kind"]) for f in ("first", "second", "third", "host")]
            if places[0] is None or not places[0][0]:
                raise SystemExit(f"{name}: the repair row {comp} {r['label']} has no winner in the database")
            places = [v or ("", "") for v in places]
            plan.append("\t".join(["add", name, str(start), comp, r["label"], r.get("part", ""), "add"]
                                  + [x for v in places for x in v]))
            added += 1
    if added:
        report.append("\t".join(map(str, [name, start, added, added, added - differ, 0, differ, 0, 0, 0, 0, 0])))
        print(report[-1], flush=True)


def database_plan(name, start, folder, rows, ref, alias, skip, shifts, last, nshifts, official=False):
    """The plan rows of one database before 2010/11 (or an official data set) at a new game in start year `start`:
    (plan lines, report line, no-winner lines, unmatched names, names matched by id). official: the data of 2001,
    whose history the shift of slice 11b leaves in place in every start year (season_dates.c, plan_for_folder)."""
    plan, nowin, unmatched = [], [], collections.Counter()
    d = hg.Data(folder)
    comps = {}                      # the competitions that hold history rows in this database, by name
    for cid, *_ in d.history():
        c = d.comp(cid)
        if comps.get(c[1], cid) != cid:
            raise SystemExit(f"{name}: two competitions named {c[1]!r} hold history rows")
        comps[c[1]] = cid
    sh = {} if official else shifts.get(name, {})
    cs, ls = int(sh.get("comp_history", 0) or 0), int(sh.get("last_rows", 0) or 0)
    held = collections.defaultdict(list)
    ns, own = (0, {}) if official else nshifts.get(name, (cs, {}))
    for cid, y, a, b, c, h in d.history():
        if d.comp(cid)[0] == "nation":
            cs_row = own.get(cid, ns)
            yy = y + cs_row if cs_row else (y + ls if (y == hg.LAST_ROW and cid in last.get(name, ())) else y)
        else:
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
    via = {(name,) + x for x in names.via_id}
    report_line = ("\t".join(map(str, [name, start, n["range"], n["kept"], n["add"], n["agree"], n["differ"],
                                      n["nocomp"], n["um_first"], n["um_other"], n["cleared"], n["kept_db"]])))
    return plan, report_line, nowin, unmatched, via


OFFICIAL = ("3.9.60", "3.9.65", "3.9.68")     # the official data sets: history of 2001, not moved by the shift
FIRST_OFFICIAL_START = 2002


def official_databases(base, dbroot):
    """[(name, folder)] of the official data sets: 3.9.68 from its Data folder, 3.9.60 and 3.9.65 from the folder
    of databases (official versions, identified by their signatures)."""
    sigs = {(int(r["club_size"]), r["club_sha256"], int(r["staff_size"]), r["staff_sha256"]): r
            for r in hg.read_tsv(hg.SIGS)}
    out = {"3.9.68": base}
    for sub in sorted(os.listdir(dbroot)):
        p = os.path.join(dbroot, sub)
        r = hg.identify(p, sigs) if os.path.isdir(p) else None
        if r and r["role"] == "official version" and r["database"] in OFFICIAL:
            out.setdefault(r["database"], p)
    return [(n, out[n]) for n in OFFICIAL if n in out]


PLAN_COLS = ["action", "database", "start_year", "competition", "label", "part", "result", "first", "first_nation",
             "second", "second_nation", "third", "third_nation", "host", "host_nation"]
# plan-official.tsv: start_year is the first start year of the row, last_start its last (empty: no last one)
OFFICIAL_COLS = PLAN_COLS + ["last_start"]


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
    nshifts = nation_shifts(shifts_path)
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
        p, r, nw, um_, v = database_plan(name, start, folder, rows, ref, alias, skip, shifts, last, nshifts,
                                         official=name in OFFICIAL)
        plan += p
        report.append(r)
        print(r, flush=True)
        nowin += nw
        unmatched.update(um_)
        via |= v
    for name, start, folder in gap_databases(argv[1]):
        gap_rows(name, start, folder, rows, ref, alias, plan, report,
                 int(shifts.get(name, {}).get("comp_history", 0) or 0), nshifts.get(name, (0, {})))
    # The official data sets in every later start year (0.6.2 open item 149). Their history is that of 2001 in every
    # start year (the shift of slice 11b leaves it), so each plan row holds for a range of start years: from the first
    # start year where the edition ended before the start, on. plan-official.tsv gives each row once, with its first
    # and last start year (last empty: every later start year). The 2002/03 tile of 3.9.68 keeps its own entry above.
    official = ["\t".join(OFFICIAL_COLS)]
    last_start = max(ended_of(r)[0] for r in rows if r.get("held") == "yes" and r.get("label", "").isdigit()) + 1
    for name, folder in official_databases(argv[0], argv[1]):
        years = collections.defaultdict(list)       # plan row without its start year: the start years it holds
        seen_nowin, um_max = set(), collections.Counter()
        first = FIRST_OFFICIAL_START + (name == "3.9.68")
        for start in range(first, last_start + 1):
            p, r, nw, um_, v = database_plan(name, start, folder, rows, ref, alias, skip, shifts, last, nshifts,
                                             official=True)
            for line in p:
                f = line.split("\t")
                years["\t".join(f[:2] + f[3:])].append(start)
            report.append(r)
            for x in nw:
                if x not in seen_nowin:
                    seen_nowin.add(x)
                    nowin.append(x)
            for k, c in um_.items():
                um_max[k] = max(um_max[k], c)
            via |= v
        print(f"{name}: {len(years)} plan rows for the start years {first} to {last_start}", flush=True)
        unmatched.update(um_max)
        for key, ys in years.items():
            if ys != list(range(ys[0], ys[-1] + 1)):
                raise SystemExit(f"{name}: the plan row {key!r} holds for start years that are not one range: {ys}")
            f = key.split("\t")
            official.append("\t".join(f[:2] + [str(ys[0])] + f[2:] + ["" if ys[-1] == last_start else str(ys[-1])]))
    um = ["database\tnation\tname\trows"] + ["\t".join([a, b, c, str(k)]) for (a, b, c), k in sorted(unmatched.items())]
    print(f"{len(unmatched)} unmatched names (database, name)")
    vl = ["database\tname\tdatabase_name"] + ["\t".join(x) for x in sorted(via)]
    if "--write" in argv:
        for fn, lines in (("fill-report.tsv", report), ("unmatched.tsv", um), ("matched-by-id.tsv", vl),
                          ("plan.tsv", plan), ("no-winner.tsv", nowin), ("plan-official.tsv", official)):
            with open(os.path.join(HIST, fn), "w", encoding="utf-8") as f:
                f.write("\n".join(lines) + "\n")
        print("wrote fill-report.tsv, unmatched.tsv, matched-by-id.tsv and plan.tsv in", HIST)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
