#!/usr/bin/env python3
"""Finds the competition history rows of the season databases whose year is wrong after the game's year shift
(0.7.2 open item 233, forum report of 13 November 2026).

Usage:
    python3 -I tools/seasons/history_years_check.py <3.9.68 Data folder> <folder of database folders> [--write]
        [--only <database>,...]

For each known season or extra database, the tool reads the club and nation competition history
(club_comp_history.dat, nation_comp_history.dat), gives each row the year that a new game in the season of the
database shows (the shifts of season_dates.c: docs/research/rules-0.6.0/databases-date-shifts.tsv, its last-rows
and nation-comps files), and compares that year with two references:

- the 3.9.68 data (the base game, rows up to 2002): the label of each edition, or the label plus the convention
  offset of the competition (below);
- the sourced research rows of docs/research/history-0.6.0/ (the 2000/01 to 2010/11 editions): the research
  label, or the year where the edition ended (`ended`), with the source URLs of the row.

A competition's convention offset is the most common difference between the year where an edition ended and its
research label (Turkey, Greece, Croatia: +1, the 3.9.68 data labels their seasons by the year where they start;
most competitions: 0). A row is right when the reference holds its first and second club in its shown year
(either form). A row is wrong when the reference holds another winner in its shown year and holds the row's first
and second club (both, when the reference names a second) within 3 years of it: the right year is then that
reference year, in the form of the year where the edition ended. Rows without a match stay unchecked (most rows
after 2010: no reference). Rows that the game clears or drops are left out: a row shown after the start year, and
in a database before 2010/11 a row of 2001 or later (the research fill of season_history.c writes those labels).

--write writes docs/research/history-years/wrong-rows.tsv (unverified.tsv: the rows of UNVERIFIED, which the
game leaves) (one row per wrong data row: database, kind, row index
in its file, competition id, data year, shown year, right year, first, second, reference) and summary.tsv (one row
per database and competition: the count and the year differences). tools/seasons/make_history_years_table.py
makes the C table season_history_years_table.c from wrong-rows.tsv.
"""
import collections
import functools
import csv
import glob
import os
import re
import sys
import unicodedata

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import measure_date_shifts as mds  # noqa: E402

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
RULES = os.path.join(ROOT, "docs", "research", "rules-0.6.0")
HIST = os.path.join(ROOT, "docs", "research", "history-0.6.0")
OUT = os.path.join(ROOT, "docs", "research", "history-years")
NEAR = 1                    # the most years between the shown year and the right year
# Competitions whose rows the tool finds but whose year the reference cannot settle: they go to unverified.tsv and the
# game leaves them. The Irish cups: the 3.9.68 label of an edition may be the year where it starts or where its final
# was, and no research row gives the final dates before 2002. The Brazilian Northeast record holds the champions of
# several states (open items 176, 189): its rows match another state's champion of a nearby year. A competition with
# a full sourced list in refs/ (full_lists) leaves this set: the list settles each row.
UNVERIFIED = {"Irish League Cup", "Irish Dublin City Cup", "Irish Blaxnit Cup", "Irish Dublin and Belfast Inter-City Cup",
              "Brazilian Northeast State Championship"}
MULTI = {"Brazilian Northeast State Championship"}
BLOCK = 3                   # the fewest wrong rows of one offset in one competition of one database
REFS = os.path.join(OUT, "refs")


def all_pairs():
    """{competition: [(label or None, first, second)]} of every row of refs/*.tsv, dated or not."""
    out = collections.defaultdict(list)
    for f in sorted(glob.glob(os.path.join(REFS, "*.tsv"))):
        for r in read_tsv(f):
            if r["note"].startswith("no real edition"):
                continue
            L = int(r["label"]) if r["label"].strip().isdigit() else None
            for a, b in ((r.get("data_first") or r["first"], r.get("data_second") or r["second"]),
                         (r["first"], r["second"])):
                out[r["competition"]].append((L, a, b))
    return out


def no_edition():
    """{competition: [(first, second, source)]}: the data pairs that the research names as no real edition (note
    "no real edition: ..."): such a row is cleared when a corrected row takes its year."""
    out = collections.defaultdict(list)
    for f in sorted(glob.glob(os.path.join(REFS, "*.tsv"))):
        for r in read_tsv(f):
            if r["note"].startswith("no real edition"):
                out[r["competition"]].append((r["first"], r["second"], r["source_url"]))
    return out


def full_lists():
    """{competition: [(label, first, second, source)]} of refs/*.tsv: the full sourced list of the editions of a
    competition (label: the year where the edition ended, its final or last match day). Such a list is the only
    reference of its competition: it wins over the 3.9.68 rows and their conventions, a single wrong row moves (no
    run of BLOCK rows is needed), and a year may hold as many rows as the list gives editions in it (the Brazilian
    Northeast record holds the champions of several states). A row without a label (no source found) is left out."""
    out = collections.defaultdict(list)
    for f in sorted(glob.glob(os.path.join(REFS, "*.tsv"))):
        for r in read_tsv(f):
            if not r["label"].strip().isdigit() or not r["first"] or r["first"] == "not held":
                continue
            for a, b in ((r.get("data_first") or r["first"], r.get("data_second") or r["second"]),
                         (r["first"], r["second"])):
                out[r["competition"]].append((int(r["label"]), a, b, r["source_url"]))
    return out

REF68_LAST = 2002           # the 3.9.68 rows up to this label are real results (2002 Arsenal: 2001/02)
STOP = {"fc", "sk", "bk", "cf", "sc", "ac", "fk", "if", "club", "cd", "de", "the", "afc", "ff", "ik", "bv", "sv",
        "vfb", "vfl", "fbc", "sporting", "clube", "futebol", "football", "calcio", "as", "ss", "us", "ud", "rc",
        "rcd", "ca", "cs", "fotball", "fotballklubb", "il", "da", "do", "and", "of", "united", "city", "kv", "kfc",
        "rsc", "fotbal", "foot", "ball", "nk", "hnk", "gnk", "sk", "bc", "fbk", "ifk", "aik", "fotboll",
        # words that many clubs of one nation share (review of item 233: "Koninklijke" made two Belgian clubs one)
        "koninklijke", "royal", "royale", "esporte", "sociedade", "associacao", "esportiva", "recreativa",
        "atletico", "athletic", "athletico", "sportclub", "voetbalclub", "palloseura"}


# Words that tell two clubs of one town apart (Club Brugge / Cercle Brugge, Manchester City / United): two names are
# one club only when they hold the same of these words (review of item 233)
DISTINCT = {"cercle", "city", "united", "inter", "internazionale", "rangers", "rovers", "athletic", "albion",
            "wanderers", "dynamo", "dinamo", "lokomotiv", "spartak", "olympic", "olympique", "racing", "union"}


def raw_words(s):
    s = unicodedata.normalize("NFKD", s).encode("ascii", "ignore").decode().lower()
    return set(re.split(r"[^a-z0-9]+", s)) & DISTINCT


def norm(s):
    s = s.replace("ø", "o").replace("Ø", "O").replace("æ", "ae")
    s = unicodedata.normalize("NFKD", s).encode("ascii", "ignore").decode().lower()
    return {t for t in re.split(r"[^a-z0-9]+", s) if len(t) > 2 and t not in STOP}


def read_tsv(path):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter="\t"))


ALIAS = collections.defaultdict(set)
for _r in read_tsv(os.path.join(HIST, "aliases.tsv")):
    ALIAS[_r["name"]].add(_r["alias"])
    ALIAS[_r["alias"]].add(_r["name"])


@functools.lru_cache(maxsize=None)
def same(a, b):
    """1 when two club or national side names name the same side (aliases.tsv, then the name words)."""
    if not a or not b:
        return False
    if a == b:
        return True
    for x in {a} | ALIAS[a]:
        for y in {b} | ALIAS[b]:
            if raw_words(x) != raw_words(y):
                continue
            A, B = norm(x), norm(y)
            if A and B and (A <= B or B <= A):
                return True
            common = A & B
            if common and max(len(t) for t in common) >= 5 and len(common) >= min(len(A), len(B)) / 2:
                return True
    return False


def research():
    """{competition: [(ended year, label, first, second, sources)]}, and {competition: convention offset}."""
    out, diffs = collections.defaultdict(list), collections.defaultdict(list)
    for f in sorted(glob.glob(os.path.join(HIST, "*.tsv"))):
        rows = read_tsv(f)
        if not rows or "held" not in rows[0] or "ended" not in rows[0]:
            continue
        for r in rows:
            if r["held"] != "yes" or not r["first"] or not r["label"].isdigit():
                continue
            m = re.match(r"^(\d{4})", r.get("ended") or "")
            if not m:
                continue
            ended, label = int(m.group(1)), int(r["label"])
            out[r["competition"]].append((ended, label, r["first"], r["second"], r.get("sources", "")))
            if not r.get("part"):
                diffs[r["competition"]].append(ended - label)
    conv = {c: collections.Counter(d).most_common(1)[0][0] for c, d in diffs.items()}
    return out, conv


def double_labels():
    """{(competition, label)} that the research gives two or more rows (the parts of a split season, the Irish
    leagues of 2003: the 2002/03 transition season and the 2003 season)."""
    n = collections.Counter()
    for f in sorted(glob.glob(os.path.join(HIST, "*.tsv"))):
        rows = read_tsv(f)
        if not rows or "held" not in rows[0] or "ended" not in rows[0]:
            continue
        for r in rows:
            if r["held"] == "yes" and r["first"] and r["label"].isdigit():
                n[(r["competition"], int(r["label"]))] += 1
    return {k for k, v in n.items() if v > 1}


DOUBLE = double_labels()
FULL = full_lists()
PAIRS = all_pairs()
NO_EDITION = no_edition()


def history_rows(d):
    """[(kind, file index, competition id, name, year, first, second)] of the club rows, then the nation rows."""
    out = []
    for kind, th, tc, base in (("club", 19, 12, 0), ("nation", 20, 16, d.count(12))):
        for i, r in d.rows(th):
            cid = mds.s32(r, 4)
            name = d.text(tc, cid - base, 4) if cid >= base else ""
            if not name:
                continue
            out.append((kind, i, cid, name, mds.u16(r, 8), d.club(mds.s32(r, 0xa)), d.club(mds.s32(r, 0xe))))
    return out


def shifts():
    dec = {r["database"]: r for r in read_tsv(os.path.join(RULES, "databases-date-shifts.tsv"))}
    nat = collections.defaultdict(dict)
    for r in read_tsv(os.path.join(RULES, "databases-date-shifts-nation-comps.tsv")):
        nat[r["database"]][int(r["competition_id"])] = int(r["shift"])
    last = {r["database"]: {int(x) for x in r["competition_ids"].split()}
            for r in read_tsv(os.path.join(RULES, "databases-date-shifts-last-rows.tsv"))}
    return dec, nat, last


def shown_year(v, nat, last, kind, cid, year):
    """The year that a new game in the season of the database shows (season_dates.c apply, N = D)."""
    ch, nh, lr = int(v["comp_history"]), int(v["nation_history"]), int(v["last_rows"])
    if lr and year == mds.LAST_ROW and cid in last:
        return year + lr
    if kind == "nation":
        return year + nat.get(cid, nh)
    return year + ch


def check_db(name, D, v, nat, last, d, ref68, res, conv, skip):
    """([wrong rows], [rows left for review]) of one database. Each row: (database, season, kind, file row,
    competition id, competition, data year, shown year, right year, first, second, reference).

    A row whose pair the list names as no real edition, or (in a competition with a full list) whose pair is no row of
    the list, dated or not, nor of the 3.9.68 or research references in any year (review: the 1948 final Nurnberg /
    Kaiserslautern lies after the German list's 1944), and whose winner is no first or second of an edition within
    2 years, is B: it moves inside a run, never
    joins one at an end, and is cleared (right year 0) when it stands on a year that a moved row takes.
    Each row is right (R), wrong (W, with its right year), near a reference with no other result on its year (S, with
    that year) or unchecked (U). A run is the rows of one competition from
    a W row to the next W row of the same offset, in the order of their shown years, with no R row and no W row of
    another offset between them: every row of the run, and the S rows of that offset next to its ends, moves by that offset (a U row inside it with the reference
    "moved with ..."). A run of fewer than BLOCK W rows stays. When the moves would put more rows on a year than it
    held (two on a label that the research gives two rows), or move a row after the start year, the competition goes
    to the review list and nothing of it moves."""
    rows = collections.defaultdict(list)
    for kind, i, cid, comp, year, first, second in history_rows(d):
        if comp in skip:
            continue
        s = shown_year(v, nat, last.get(name, set()), kind, cid, year)
        rows[comp].append([kind, i, cid, comp, year, s, first, second])
    wrong, review = [], []
    for comp, lst in rows.items():
        c = conv.get(comp, 0)
        refs = []                       # (accepted years, right year, first, second, reference)
        # a full list wins in the years it covers (German First Division: 1903 to 1945 only)
        lo_full = min((L for L, f, sc, src in FULL.get(comp, ())), default=0)
        hi_full = max((L for L, f, sc, src in FULL.get(comp, ())), default=-1)
        refs = [({L}, L, f, sc, src) for L, f, sc, src in FULL.get(comp, ())]
        for L, f, sc in ref68.get(("club", comp), ()) or ref68.get(("nation", comp), ()):
            if not lo_full - 1 <= L + c <= hi_full + 1:
                refs.append(({L, L + c}, L + c, f, sc, f"3.9.68 data: {comp} {L} {f} / {sc}"))
        for ended, label, f, sc, src in res.get(comp, ()):
            if not lo_full - 1 <= ended <= hi_full + 1:
                refs.append(({label, ended}, ended, f, sc, src))
        if not refs:
            continue
        lst.sort(key=lambda x: (x[5], x[1]))
        marks = []
        for kind, i, cid, comp_, year, s, first, second in lst:
            if s > D or (int(v["comp_history"]) == 0 and s >= 2001):
                marks.append(("X", 0, ""))      # cleared or dropped by the game: never moved
                continue
            mine = [r for r in refs if same(r[2], first) and (not r[3] or not second or same(r[3], second))]
            if any(first == a and second == b for a, b, src in NO_EDITION.get(comp, ())):
                marks.append(("B", 0, ""))
                continue
            if comp in FULL and lo_full <= s <= hi_full and first and second and \
                    not any(same(a, first) and same(b, second) for L, a, b in PAIRS[comp]) and \
                    not any(same(f, first) and same(sc, second) for L, f, sc in
                            ref68.get(("club", comp), ()) or ref68.get(("nation", comp), ())) and \
                    not any(same(f, first) and same(sc, second) for e, L, f, sc, src in res.get(comp, ())) and \
                    not any(same(a, first) or same(b, first) for L, a, b in PAIRS[comp] if L and abs(L - s) <= 2):
                marks.append(("B", 0, ""))      # no real edition of the full list (Beveren / Beerschot 2013)
                continue
            if any(s in r[0] for r in mine):
                # the same pair in a year next to it too (Rosenborg / Molde 1998 and 1999): no proof either way
                twice = any(s - 1 in r[0] or s + 1 in r[0] for r in mine if s not in r[0])
                marks.append(("U", 0, "") if twice else ("R", 0, ""))
                continue
            other_at_s = any(s in r[0] for r in refs if not same(r[2], first))
            near = sorted((abs(r[1] - s), r[1], r[4]) for r in mine if 0 < abs(r[1] - s) <= NEAR)
            if other_at_s and near and len({y for dist, y, src in near if dist == near[0][0]}) == 1:
                marks.append(("W", near[0][1] - s, near[0][2]))
            elif near and len({y for dist, y, src in near if dist == near[0][0]}) == 1:
                marks.append(("S", near[0][1] - s, near[0][2]))     # no other result on its year: joins a run only
            else:
                marks.append(("U", 0, ""))
        runs = []
        j = 0
        while j < len(lst):
            if marks[j][0] != "W":
                j += 1
                continue
            delta, k, last_w, n_w = marks[j][1], j, j, 0
            while k < len(lst) and marks[k][0] in ("W", "U", "S", "B") and (marks[k][0] != "W" or marks[k][1] == delta):
                if marks[k][0] == "W":
                    last_w, n_w = k, n_w + 1
                k += 1
            lo, hi = j, last_w
            # The ends: an unchecked row on the year that the run's end takes moves with it (Swedish Cup 2002
            # Djurgarden / AIK, which the research does not hold, shown at 2003 before the moved 2003 row), and so does
            # a row whose only near reference lies at the run's offset (Spanish First Division 1936 Athletic Bilbao,
            # shown at 1937, where the 3.9.68 data holds no other row), one by one.
            def joins(t, end):
                return marks[t][0] == "S" and marks[t][1] == delta or \
                    marks[t][0] == "U" and lst[t][5] == lst[end][5] + delta
            while lo > 0 and joins(lo - 1, lo):
                lo -= 1
            while hi + 1 < len(lst) and joins(hi + 1, hi):
                hi += 1
            if n_w >= (1 if comp in FULL else BLOCK):
                run = {}
                for t in range(lo, hi + 1):
                    ref = marks[t][2] if marks[t][0] in ("W", "S") else \
                        f"moved with the wrong rows of {lst[lo][5]} to {lst[hi][5]} of {comp} (shown years)"
                    run[t] = (delta, ref)
                runs.append(run)
            j = last_w + 1
        if not runs:
            continue
        # several rows on one year only where the record holds several championships (the Brazilian Northeast
        # record, one per state); two finals in one calendar year (Irish League Cup 1998) would show the year twice
        room = collections.Counter(L for L, f, sc, src in FULL.get(comp, ())[::2]) if comp in MULTI else \
            collections.Counter()
        before = collections.Counter(x[5] for x in lst)

        def plan(moves):
            """(rows out, 1 when the moves are allowed). A row of no real edition on a year that a moved row takes is
            cleared (a free row, as the duplicate clear of season_history.c): else the year would show two rows. No
            year may hold more rows than before (two on a label that the research gives two rows, or for MULTI as many
            as the full list gives editions), and no row may move after the start year: the creation drops it
            (season_history_drop_after_start; review of open item 233: Belgian Second Division 2014 Westerlo)."""
            targets = {lst[t][5] + d for t, (d, ref) in moves.items()}
            clears = {t for t, x in enumerate(lst) if t not in moves and marks[t][0] == "B" and x[5] in targets}
            after = collections.Counter(x[5] + moves.get(t, (0,))[0] for t, x in enumerate(lst) if t not in clears)
            out = [(name, D, x[0], x[1], x[2], comp, x[4], x[5], x[5] + moves[t][0], x[6], x[7], moves[t][1])
                   for t, x in enumerate(lst) if t in moves]
            out += [(name, D, x[0], x[1], x[2], comp, x[4], x[5], 0, x[6], x[7],
                     f"cleared: no real edition of {comp} holds {x[6]} / {x[7]} (" + next(
                         (src for a, b, src in NO_EDITION.get(comp, ()) if a == x[6] and b == x[7]),
                         FULL[comp][0][3] if comp in FULL else "") + ")")
                    for t, x in enumerate(lst) if t in clears]
            ok = not any(n > max(1 + ((comp, y) in DOUBLE), room[y], before[y]) for y, n in after.items()) and \
                not any(x[8] > D for x in out)
            return out, ok

        # each run on its own (Belgian Super Cup: 1989 to 2011 moves, 1979 to 1985 meets the 1986 row and stays)
        accepted = {}
        for run in runs:
            out, ok = plan({**accepted, **run})
            if ok:
                accepted.update(run)
            else:
                review += plan(run)[0]
        if accepted:
            wrong += plan(accepted)[0]
    return wrong, review


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    ref = mds.Data(argv[0])
    only = argv[argv.index("--only") + 1].split(",") if "--only" in argv else None
    ref68 = collections.defaultdict(list)
    for kind, i, cid, comp, year, first, second in history_rows(ref):
        if year <= REF68_LAST:
            ref68[(kind, comp)].append((year, first, second))
    res, conv = research()
    several = {comp for (kind, comp), rows in ref68.items()
               if max(collections.Counter(y for y, f, sc in rows).values()) > 1}
    several |= {comp for comp, rows in res.items() if max(collections.Counter(l for e, l, f, sc, src in rows).values()) > 1}
    several -= {comp for comp, label in DOUBLE if sum(1 for c, l in DOUBLE if c == comp) == 1}   # one double label only
    dec, nat, last = shifts()
    sigs = mds.signatures()
    found = []
    for sub in sorted(os.listdir(argv[1])):
        p = os.path.join(argv[1], sub)
        inner = [os.path.join(p, x) for x in os.listdir(p)] if os.path.isdir(p) and \
            not os.path.exists(os.path.join(p, "index.dat")) else []
        inner = [x for x in inner if os.path.exists(os.path.join(x, "index.dat"))]
        if len(inner) == 1:
            p = inner[0]                # a folder that holds the database folder (October 2026 Data Update)
        r = mds.identify(p, sigs) if os.path.isdir(p) else None
        if not r or r["data_season"] in ("none", "n/a") or r["role"] in mds.OFFICIAL or r["database"] not in dec:
            continue
        if only and r["database"] not in only:
            continue
        found.append((r["database"], int(r["data_season"][:4]), p))
    found.sort(key=lambda x: (x[1], x[0]))
    allw, unver = [], []
    for name, D, p in found:
        w, rev = check_db(name, D, dec[name], nat[name], last, mds.Data(p), ref68, res, conv, several - set(FULL))
        allw += [x for x in w if x[5] not in UNVERIFIED or x[5] in FULL]
        unver += [x for x in w if x[5] in UNVERIFIED and x[5] not in FULL] + rev
        by = collections.Counter((x[5], x[8] - x[7]) for x in w)
        print(f"{name} ({D}): {len(w)} wrong rows" + "".join(f"; {c} {d:+d} x{n}" for (c, d), n in sorted(by.items())))
    if "--write" in argv:
        os.makedirs(OUT, exist_ok=True)
        cols = ["database", "data_season", "kind", "file_row", "competition_id", "competition", "data_year",
                "shown_year", "right_year", "first", "second", "reference"]
        with open(os.path.join(OUT, "wrong-rows.tsv"), "w", encoding="utf-8") as f:
            f.write("\t".join(cols) + "\n")
            for x in allw:
                f.write("\t".join(str(v) for v in x) + "\n")
        with open(os.path.join(OUT, "unverified.tsv"), "w", encoding="utf-8") as f:
            f.write("\t".join(cols) + "\n")
            for x in unver:
                f.write("\t".join(str(v) for v in x) + "\n")
        summ = collections.defaultdict(lambda: [0, collections.Counter()])
        for x in allw:
            summ[(x[0], x[5])][0] += 1
            summ[(x[0], x[5])][1][x[8] - x[7]] += 1
        with open(os.path.join(OUT, "summary.tsv"), "w", encoding="utf-8") as f:
            f.write("database\tcompetition\trows\tright_minus_shown\n")
            for (db, comp), (n, c) in sorted(summ.items()):
                f.write(f"{db}\t{comp}\t{n}\t{' '.join(f'{d:+d}x{k}' for d, k in sorted(c.items()))}\n")
        print(f"wrote {len(allw)} rows ({len(unver)} unverified) to {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
