#!/usr/bin/env python3
"""Club id check (0.6.0, O151): each row of docs/research/rules-0.6.0/*-clubs.tsv names a club record of its
season's database (db_club_id, db_club). A wrong id sends the real club's place to another club (Yozgatspor 13033
for 13032, Dijon FCO 3776 for 3087, GOSK Dubrovnik 3304 for 4259).

Usage:
    python3 -I tests/seasons/club_ids_check.py [--databases <folder>] [--flags <file.tsv>] [--known]

The databases come from <folder> (default /Users/max/Projects/cm0102/test-fixtures/databases, one folder per
database, found as in make_clubs.py). The season database of the row's season is used; a season with none (2002/03)
takes the 3.9.68 data, whose ids those rows cite. Without the folder the check prints SKIP and ends with code 77
(ctest skip).

Each row is checked against the record of its id in that database:
    gone     the id is not a club record
    name     the record's name is not the row's db_club (accent and case aside)
    division the record's division is not the row's db_division
    nation   the record's nation is not the nation of the row's db_division
    exact    another club record of the nation has exactly the real name, and this one has not
    real     no distinctive word of the real name is in the record's long name, short name or town, or another
             club record of the nation holds more of those words than this one does
    nodiv    the record is in no division while another record of the nation fits the real name as well and is in one
    swap     two rows of one clubs file and season fit each other's record better than their own (two clubs of one
             town with their ids crossed)
    blip     the id differs from the id of the same real club in the season before and in the season after, which
             agree with each other
    twice    one id stands for two different real names in one season, or one real name for two ids
    ambig    the record's name is held by two club records of the database
A flag that a review showed to be a false alarm is listed in club_ids_known.tsv (flag, club, id, reason). The
check ends with code 1 when a flag is not listed there. --flags writes every flag to a file.
"""
import csv
import difflib
import glob
import os
import re
import struct
import sys
import tempfile
import zipfile
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "seasons", "own_db"))
from cmdb import Folder, cstr, norm  # noqa: E402

PARTS = os.path.join(ROOT, "docs", "research", "rules-0.6.0")
DEFAULT_DB = "/Users/max/Projects/cm0102/test-fixtures/databases"
OWN_DB = "/Users/max/Projects/cm0102/own-dbs"      # our own season databases (Summer 2004, Summer 2006) as zips
NEEDED = ("index.dat", "club.dat", "club_comp.dat", "nation.dat", "stadium.dat", "city.dat")
BASE = "Original 3.9.68"

# Words that name a kind of club or a legal form, not the club.
GENERIC = set("""fc sc ac as cf cd sd ud ca club clube football futbol futebol calcio sport sports sporting spor de del la
le les los el las das do da dos di al uc us ss sv sk fk nk hnk hk bk if ik ff fotboll association athletic
atletico athletics a s v e r k b t united utd city town rovers wanderers fussball verein turn und tsv vfb vfl vfr
spvgg sg bsc fsv sportverein sportclub kv kvc krc rc ks gks mks tks zks lks wks cks gkp zs fo aa ad ae pae
ofi apo ao gs oac ofk ok tj sfc afc fcm st the of and""".split())

# Flags that a review showed to be right (flag, real club name, club id: reason): club_ids_known.tsv.
def known():
    out = {}
    with open(os.path.join(HERE, "club_ids_known.tsv"), newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            out[(r["flag"], r["club"], int(r["id"]))] = r["reason"]
    return out


def unpack(path):
    """The files of a zipped database that the check reads, in a new temporary folder; None without the zip."""
    if not os.path.exists(path):
        return None
    folder = tempfile.mkdtemp(prefix="club-ids-")
    with zipfile.ZipFile(path) as z:
        for info in z.infolist():
            name = os.path.basename(info.filename)
            if name.lower() in NEEDED:
                with open(os.path.join(folder, name), "wb") as f:
                    f.write(z.read(info))
    return folder


def database_folders(root, own=OWN_DB):
    """{season: folder} of the season databases, and the base: the folders are found by the first words of
    their names, as in make_clubs.py."""
    out, missing = {}, set()
    with open(os.path.join(PARTS, "databases-signatures.tsv"), newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            if r["role"] != "season database":
                continue
            hits = sorted(d for d in os.listdir(root) if d == r["database"] or d.startswith(r["database"] + " "))
            if hits:
                out[int(r["data_season"][:4])] = os.path.join(root, hits[0])
                continue
            unpacked = unpack(os.path.join(own, r["database"] + ".zip"))
            if unpacked:
                out[int(r["data_season"][:4])] = unpacked
            else:
                missing.add(int(r["data_season"][:4]))
    base = sorted(d for d in os.listdir(root) if d == BASE or d.startswith(BASE + " "))
    return out, (os.path.join(root, base[0]) if base else None), missing - set(out)


class Database:
    """The club records of one database folder: {id: (long name, short name, nation, town, division)}, the nation of each
    division by name, and the club ids by normal name."""

    def __init__(self, path):
        f = Folder(path)
        i32 = lambda r, o: struct.unpack_from("<i", r, o)[0]  # noqa: E731
        nation = {i32(r, 0): cstr(r, 4, 51) for r in f.rows(4)}
        city = {i32(r, 0): cstr(r, 4, 51) for r in f.rows(21)}
        stadium = {i32(r, 0): i32(r, 0x38) for r in f.rows(5)}
        division = {i32(r, 0): cstr(r, 4, 51) for r in f.rows(12)}
        self.clubs = {}
        for r in f.rows(0):
            self.clubs[i32(r, 0)] = (cstr(r, 4, 51), cstr(r, 0x38, 26), nation.get(i32(r, 0x53), "?"),
                                     city.get(stadium.get(i32(r, 0x69), -1), ""), division.get(i32(r, 0x57), "none"))
        self.division_nation = {}
        for r in f.rows(12):
            self.division_nation.setdefault(cstr(r, 4, 51), nation.get(i32(r, 0x5d), "?"))
        self.by_name = defaultdict(list)
        self.by_exact = defaultdict(list)             # (nation, normal long or short name) -> ids
        self.by_word = defaultdict(set)               # (nation, word) -> ids
        for i, c in self.clubs.items():
            self.by_name[(norm(c[0]), c[2])].append(i)
            for t in {norm(c[0]), norm(c[1])}:
                self.by_exact[(c[2], t)].append(i)
            for t in (c[0], c[1], c[3]):
                for w in norm(t).split():
                    self.by_word[(c[2], w)].add(i)

    def rival(self, nation, mine):
        """(count of the words, id) of the records of the nation that hold a word of mine exactly."""
        count = defaultdict(int)
        for w in mine:
            for i in self.by_word.get((nation, w), ()):
                count[i] += 1
        return [(n, i) for i, n in count.items()]


def words(text):
    return [w for w in norm(text).split() if w not in GENERIC and len(w) > 1]


def close(a, b):
    return a == b or (len(a) >= 4 and len(b) >= 4 and (a.startswith(b) or b.startswith(a))) or \
        difflib.SequenceMatcher(None, a, b).ratio() >= 0.8


def score(mine, have):
    return sum(1 for w in mine if any(close(w, h) for h in have))


def real_flag(real, rec, db):
    """A reason when the real name and the record's names do not fit, else None."""
    have = set()
    for t in (rec[0], rec[1], rec[3]):
        have.update(norm(t).split())
    mine = words(real)
    if not mine:
        return None
    mine_score = score(mine, have)
    if mine_score == 0:
        return "no distinctive word of the real name is in the record"
    if mine_score < len(mine):
        # Another record of the nation that holds more of the words by exact spelling.
        best = max(db.rival(rec[2], mine), default=(0, None), key=lambda x: x[0])
        if best[0] > mine_score:
            return f"record {best[1]} {db.clubs[best[1]][0]!r} holds {best[0]} of {len(mine)} words, this one {mine_score}"
    return None


def fit(real, rec):
    """How well the real name fits a record: the share of its words in the record's names, 0 to 1."""
    mine = words(real)
    if not mine:
        return 1.0
    have = set()
    for t in (rec[0], rec[1], rec[3]):
        have.update(norm(t).split())
    return score(mine, have) / len(mine) + 0.01 * (norm(real) == norm(rec[0]))


def swap_flags(batch):
    """batch: [(row, record)] of one file and season. Pairs whose names share a word and fit better crossed."""
    out = []
    for a in range(len(batch)):
        ra, ca = batch[a]
        wa = set(words(ra["club"]))
        for b in range(a + 1, len(batch)):
            rb, cb = batch[b]
            if ra["db_club_id"] == rb["db_club_id"] or not wa & set(words(rb["club"])):
                continue
            own = fit(ra["club"], ca) + fit(rb["club"], cb)
            crossed = fit(ra["club"], cb) + fit(rb["club"], ca)
            if crossed > own + 0.5:
                out.append((ra, rb))
    return out


RESERVE = {"b", "c", "ii", "iii", "u19", "u21", "u23", "amateure", "reserves", "reserve", "2", "3"}


def reserve(name):
    return bool(RESERVE & set(norm(name).split()))


def blip_flags(table):
    """table: {(file, season): [row]}. A row whose club has the same id X in the seasons on both sides and another
    id here. The same club is the row of the nearest name (word prefixes of 5 letters, then 0.8 alike)."""
    index = {}
    for key, batch in table.items():
        by_prefix = defaultdict(list)
        for r in batch:
            for w in set(norm(r["club"]).split()):
                by_prefix[w[:5]].append(r)
        index[key] = by_prefix

    def twin(file, season, r):
        mine = norm(r["club"])
        best, top = None, 0.8
        for w in set(mine.split()):
            for o in index.get((file, season), {}).get(w[:5], ()):
                ratio = difflib.SequenceMatcher(None, mine, norm(o["club"])).ratio()
                if ratio > top:
                    best, top = o, ratio
        return best

    out = []
    for (file, season), batch in table.items():
        year = int(season[:4])
        before, after = f"{year - 1}/{(year) % 100:02d}", f"{year + 1}/{(year + 2) % 100:02d}"
        for r in batch:
            a, b = twin(file, before, r), twin(file, after, r)
            if a and b and reserve(a["club"]) == reserve(r["club"]) and a["db_club_id"] == b["db_club_id"] != r["db_club_id"] and a["db_club_id"] not in ("", "-1"):
                out.append((r, a))
    return out


def division_name(text):
    """A division name without the nation suffix of the research ("Welsh Premier Division (Wales)"), and the words
    for no division in one."""
    text = re.sub(r" \([A-Z][A-Za-z ]+\)$", "", text.strip())
    return "none" if text.lower().startswith(("none", "no division", "no nation")) or not text else text


def rows():
    out = []
    for path in sorted(glob.glob(os.path.join(PARTS, "*-clubs.tsv"))):
        with open(path, newline="", encoding="utf-8") as f:
            for n, r in enumerate(csv.DictReader(f, delimiter="\t"), 2):
                r["file"], r["line"] = os.path.basename(path), n
                out.append(r)
    return out


def check(root):
    folders, base, missing = database_folders(root)
    if not base:
        return None
    dbs = {}

    def db_of(season):
        key = folders.get(season, base)
        if key not in dbs:
            dbs[key] = Database(key)
        return dbs[key]

    flags = []
    batches = defaultdict(list)
    by_file = defaultdict(list)
    ids = defaultdict(lambda: defaultdict(set))       # (season, id) -> {real names: rows}
    names = defaultdict(lambda: defaultdict(set))     # (season, nation, real) -> ids
    for r in rows():
        try:
            season = int(r["season"][:4])
            cid = int(r["db_club_id"])
        except ValueError:
            continue
        if cid < 0:
            continue
        if season in missing:
            continue
        db = db_of(season)
        where = f"{r['file']}:{r['line']}"
        rec = db.clubs.get(cid)

        def flag(kind, why):
            flags.append((kind, r["season"], cid, r["club"], r["db_club"], r["division"], where, why))
        if rec is None:
            flag("gone", "no club record with this id")
            continue
        if norm(rec[0]) != norm(r["db_club"]):
            flag("name", f"the record is {rec[0]!r}")
        nat = db.division_nation.get(r["db_division"])
        if nat and nat != "?" and rec[2] != nat and not r["db_division"].startswith("none"):
            flag("nation", f"the record is {rec[0]!r} of {rec[2]}, the division {r['db_division']!r} is of {nat}")
        if rec[4] == "none":
            mine_words = words(r["club"])
            for n, other in db.rival(rec[2], mine_words):
                o = db.clubs[other]
                if other != cid and o[4] != "none" and fit(r["club"], o) >= fit(r["club"], rec) and n >= len(mine_words) - 1:
                    flag("nodiv", f"record {other} {o[0]!r} is in {o[4]!r}; this one {rec[0]!r} is in none")
                    break
        want = norm(r["club"])
        if want and norm(rec[0]) != want and norm(rec[1]) != want:
            for other in db.by_exact.get((rec[2], want), ()):
                if other != cid:
                    o = db.clubs[other]
                    flag("exact", f"record {other} {o[0]!r} in {o[4]!r} is named so; this one is {rec[0]!r} in {rec[4]!r}")
                    break
        if division_name(rec[4]) != division_name(r["db_division"]):
            flag("division", f"the record is in {rec[4]!r}, the row says {r['db_division']!r}")
        why = real_flag(r["club"], rec, db)
        if why:
            flag("real", f"{why}; the record is {rec[0]!r} / {rec[1]!r} / {rec[3]!r} of {rec[2]}")
        if len(db.by_name[(norm(rec[0]), rec[2])]) > 1:
            flag("ambig", f"the name {rec[0]!r} is held by ids {db.by_name[(norm(rec[0]), rec[2])]}")
        batches[(r["file"], r["season"])].append((r, rec))
        by_file[(r["file"], r["season"])].append(r)
        ids[(r["season"], cid)][r["club"]].add(where)
        names[(r["season"], r["club"])][cid].add(where)
    for (season, cid), by_real in ids.items():
        if len(by_real) > 1:
            flags.append(("twice", season, cid, " | ".join(sorted(by_real)), "", "", "",
                          "one id stands for several real names"))
    for (season, real), by_id in names.items():
        if len(by_id) > 1:
            flags.append(("twice", season, min(by_id), real, "", "", "", f"one real name has ids {sorted(by_id)}"))
    for (part, season), batch in batches.items():
        for ra, rb in swap_flags(batch):
            flags.append(("swap", season, int(ra["db_club_id"]), ra["club"], ra["db_club"], ra["division"],
                          f"{part}:{ra['line']}", f"crossed with {rb['club']!r} ({rb['db_club_id']} {rb['db_club']!r})"))
    for r, a in blip_flags(by_file):
        db = db_of(int(r["season"][:4]))
        mine, other = db.clubs.get(int(r["db_club_id"])), db.clubs.get(int(a["db_club_id"]))
        # Ids move between databases; the id of the seasons around counts only when its record here fits as well.
        if mine and other and other[2] == mine[2] and fit(r["club"], other) >= fit(r["club"], mine):
            flags.append(("blip", r["season"], int(r["db_club_id"]), r["club"], r["db_club"], r["division"],
                          f"{r['file']}:{r['line']}",
                          f"the seasons on both sides have id {a['db_club_id']}, here {other[0]!r} in {other[4]!r}"
                          f" (this one {mine[0]!r} in {mine[4]!r})"))
    return flags


def main(argv):
    root = argv[argv.index("--databases") + 1] if "--databases" in argv else DEFAULT_DB
    if not os.path.isdir(root):
        print(f"SKIP  no database folder {root}")
        return 77
    flags = check(root)
    if flags is None:
        print(f"SKIP  no 3.9.68 data in {root}")
        return 77
    if "--flags" in argv:
        with open(argv[argv.index("--flags") + 1], "w", encoding="utf-8") as f:
            f.write("flag\tseason\tid\tclub\tdb_club\tdivision\tat\twhy\n")
            for x in flags:
                f.write("\t".join(map(str, x)) + "\n")
    listed = known()
    new = [x for x in flags if (x[0], x[3], x[2]) not in listed]
    for x in new:
        print("FAIL  " + "  ".join(str(c) for c in x if c != ""))
    print("PASS" if not new else f"FAILED: {len(new)} flags ({len(flags) - len(new)} known)")
    return 1 if new else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
