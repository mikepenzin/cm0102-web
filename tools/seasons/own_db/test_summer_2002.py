"""Pipeline seam of Summer 2002: checks the built folder against the change list. Skips when the data is absent.

Usage: python3 -I test_summer_2002.py [<work root>]

<work root> holds changes/ (change_list.py), build/Summer 2002 (build_2002.py), build/summer-2002-work and
disc/data (the Xbox data). Default: xbox-0203-work beside the main checkout of the repo. The 3.9.68 Data folder is
clean-3968/Data beside it. The data stays outside the repo; this test reads it only.

Checks (spec docs/specs/2026-10-24-summer-2002-db-spec.md, Testing):
- every applied row of the change list is in the folder: club, join date, contract end and job of kinds 1 to 3
  (through ids.tsv, the staff ids after the duplicate merge; staff_slots.py places none of these people), ratings
  of kind 4, divisions of kind 5, the club name of kind 6, and an applied row of another kind fails;
- nothing else changes (check_untouched);
- duplicates.py check passes with kept-pairs.tsv, and no table has more rows than 3.9.68;
- Q38, Q39 (check_structure): the 20 data files, every 3.9.68 competition row with its name, every division
  with its 3.9.68 club count, and at least the 2001 class count (season_clubs.c CLASS_SIZE) in each class
  division, so the original makes every table at load;
- known facts: Celestini at Olympique Marseille, Fiorentina is Florentia Viola, Rooney at Everton with current
  ability 135, Ronaldo (the "Ronaldo" of the highest current ability, born 1976) at Real Madrid C.F.;
- two builds with different PYTHONHASHSEED values (python3 -P -s, as -I ignores the seed) give the same files as
  the folder.
"""
import collections
import contextlib
import csv
import filecmp
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "xbox0203"))
import cmdb  # noqa: E402
import cmdb1  # noqa: E402
import duplicates  # noqa: E402
from build_2002 import FREE_ROWS  # noqa: E402
from change_list import REPO  # noqa: E402
from read_xbox0203 import FM_JOB  # noqa: E402

GROUPS = "Turkish 2. Division Category B Group "
SIZES = {"A": 18, "B": 17, "C": 17}           # the 2002/03 groups (Q16)
MARKER = os.sep + os.path.join(".claude", "worktrees") + os.sep
MAIN = REPO[:(REPO + os.sep).index(MARKER)] if MARKER in REPO + os.sep else REPO
ROOT = os.path.dirname(MAIN)


def read_tsv(path):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter="\t", quoting=csv.QUOTE_NONE))


def i32(r, o):
    return cmdb1.u("<i", r, o)[0]


def iso(d):
    return d.isoformat() if d else "-"


def find(F, first, second, year=None):
    """Staff ids named first + second (cmdb.norm), with the year of birth when given."""
    fn, sn = cmdb.names(F, 13), cmdb.names(F, 14)
    out = []
    for r in F.rows(6):
        a, b = i32(r, 4), i32(r, 8)
        if cmdb.norm(fn.get(a, "")) == cmdb.norm(first) and cmdb.norm(sn.get(b, "")) == cmdb.norm(second):
            d = cmdb1.date_at(r, 0x10)
            if year is None or (d and d.year == year):
                out.append(i32(r, 0))
    return out


def club_of(F, t):
    c = i32(F.rows(6)[t], 0x39)
    names = {i32(r, 0): cmdb.cstr(r, 4, 51) for k in (0, 1) for r in F.rows(k)}
    return names.get(c, "-")


def check_changes(F, changes, work):
    bad = []
    ids = {int(r["old"]): int(r["new"]) for r in read_tsv(os.path.join(work, "ids.tsv"))}
    placed = {int(r["staff id"]) for r in read_tsv(os.path.join(work, "staff-slots.tsv"))}
    st, pl = F.rows(6), F.rows(10)
    n = 0
    for kind in ("1-moves", "2-releases", "3-dates"):
        for row in read_tsv(os.path.join(changes, kind + ".tsv")):
            if row["decision"] != "applied":
                continue
            t = ids[int(row["id_3968"])]
            r = st[t]
            club = i32(r, 0x39)
            if club != int(row["new_club_id"]) or t in placed:
                bad.append((kind, row["person"], "club", club, row["new_club_id"]))
            if (iso(cmdb1.date_at(r, 0x3e)), iso(cmdb1.date_at(r, 0x46))) != (
                    (row["joined_xbox"], row["cend_xbox"]) if row["id_xbox"] != "-1"
                    else (row["joined_3968"], row["cend_3968"])):           # Q40 holders keep their dates
                bad.append((kind, row["person"], "dates", cmdb1.date_at(r, 0x3e), cmdb1.date_at(r, 0x46)))
            if row["take_job"] == "yes":
                role, code = row["job_xbox"].split()
                if role == "staff" and FM_JOB.get(r[0x3d], 0) != int(code):
                    bad.append((kind, row["person"], "job", r[0x3d], row["job_xbox"]))
            n += 1
    for row in read_tsv(os.path.join(changes, "4-ratings.tsv")):
        if row["decision"] == "applied":
            p = pl[i32(st[ids[int(row["id_3968"])]], 0x91)]
            got = cmdb1.u("<hhhhh", p, 5)
            if (got[0], got[1], got[4]) != (int(row["ca_xbox"]), int(row["pa_xbox"]), int(row["world_xbox"])):
                bad.append(("ratings", row["person"], got))
            n += 1
    clubs = {i32(r, 0): r for r in F.rows(0)}
    for row in read_tsv(os.path.join(changes, "5-leagues.tsv")):
        if row["decision"] == "applied":
            if i32(clubs[int(row["id_3968"])], 0x57) != int(row["new_division_id"]):
                bad.append(("division", row["club"]))
            n += 1
    for kind in ("7-unclear",):
        bad += [(kind, "applied", r["person"]) for r in read_tsv(os.path.join(changes, kind + ".tsv"))
                if r["decision"] == "applied"]
    group_rows = {cmdb.cstr(r, 4, 51): i32(r, 0) for r in F.rows(12)}
    for row in read_tsv(os.path.join(changes, "6-names.tsv")):
        if row["decision"] == "applied" and row["item"] == "competition":
            name = row["value_xbox"].rsplit(" (", 1)[0]
            if not name.startswith(GROUPS) or name not in group_rows:
                bad.append(("Turkish group row", name))
            n += 1
        elif row["decision"] == "applied" and row["item"].startswith("member of " + GROUPS):
            name = row["item"][len("member of "):]
            if i32(clubs[int(row["id_3968"])], 0x60) != group_rows.get(name):
                bad.append(("Turkish group tag", row["value_xbox"], name))
            n += 1
        elif row["decision"] == "applied" and row["item"] != "club name":
            bad.append(("an applied name row that the build does not write", row["item"]))
        elif row["decision"] == "applied":
            r = clubs[int(row["id_3968"])]
            if f"{cmdb.cstr(r, 4, 51)} / {cmdb.cstr(r, 0x38, 26)}" != row["value_xbox"]:
                bad.append(("club name", row["value_xbox"]))
            n += 1
    print(f"applied rows checked: {n}")
    return bad


def check_untouched(F, T, changes, work):
    """Nothing else changes: a staff row that no applied row, merge or staff_slots.py placement touched keeps its
    3.9.68 fields (+0x04 to +0x61 and the links +0x91 to +0x9d) and ratings, and a club keeps every field outside
    its staff lists, its division (when an applied row sets it) and its names (the same)."""
    bad = []
    ids = {int(r["old"]): int(r["new"]) for r in read_tsv(os.path.join(work, "ids.tsv"))}
    placed = {int(r["staff id"]) for r in read_tsv(os.path.join(work, "staff-slots.tsv"))}
    count = {}
    for n in ids.values():
        count[n] = count.get(n, 0) + 1
    listed = {int(r["id_3968"]) for k in ("1-moves", "2-releases", "3-dates")
              for r in read_tsv(os.path.join(changes, k + ".tsv")) if r["decision"] == "applied"}
    rated = {int(r["id_3968"]) for r in read_tsv(os.path.join(changes, "4-ratings.tsv")) if r["decision"] == "applied"}
    ts, fs, tp, fp = T.rows(6), F.rows(6), T.rows(10), F.rows(10)
    checked = 0
    for t, n in ids.items():
        if count[n] > 1 or n in placed:
            continue
        a, b = ts[t], fs[n]
        if t not in listed and (bytes(a[4:0x61]) != bytes(b[4:0x61]) or bytes(a[0x91:0x9d]) != bytes(b[0x91:0x9d])):
            bad.append(("staff row changed", t, n))
        if t not in rated and a[0x5f] & 2 and bytes(tp[i32(a, 0x91)][5:0x45]) != bytes(fp[i32(b, 0x91)][5:0x45]):
            bad.append(("ratings changed", t, n))
        checked += 1
    div = {int(r["id_3968"]) for r in read_tsv(os.path.join(changes, "5-leagues.tsv")) if r["decision"] == "applied"}
    names6 = [r for r in read_tsv(os.path.join(changes, "6-names.tsv")) if r["decision"] == "applied"]
    named = {int(r["id_3968"]) for r in names6 if r["item"] == "club name"}
    tagged = {int(r["id_3968"]) for r in names6 if r["item"].startswith("member of " + GROUPS)}
    for a, b in zip(T.rows(0), F.rows(0)):
        c = i32(a, 0)
        keep = bytearray(a)
        keep[0x9b:0x230] = b[0x9b:0x230]
        if c in div:
            keep[0x57:0x5b] = b[0x57:0x5b]
        if c in named:
            keep[4:0x52] = b[4:0x52]
        if c in tagged:
            keep[0x60:0x64] = b[0x60:0x64]
        if bytes(keep) != bytes(b):
            bad.append(("club row changed", c))
    print(f"untouched staff rows checked: {checked}")
    return bad[:20]


def class_sizes():
    """{division name: club count of a 2001 start} of the classes in season_clubs.c CLASS_SIZE."""
    text = open(os.path.join(REPO, "src", "recomp_rt", "win", "season_clubs.c"), encoding="utf-8").read()
    body = text[text.index("static const ClassSize CLASS_SIZE[] = {"):]
    body = body[:body.index("};")]
    rows = re.findall(r'\{"([^"]+)", "([a-z_0-9]+)", (\d+), (\d+)\}', body)
    n = body.count('{"')
    if len(rows) != n:
        raise SystemExit(f"FAIL: CLASS_SIZE has {n} rows, the test reads {len(rows)}")
    return {m[0]: int(m[3]) for m in rows}


def check_structure(F, T, out):
    """Q39 and Q38: the folder holds the 20 data files, every competition of 3.9.68 keeps its row and name (the
    original finds 168 to 172 and others by name at load), every division keeps its 3.9.68 club count, and each
    division of a class in season_clubs.c CLASS_SIZE holds at least the count of a 2001 start, so create_table
    makes its table (0x56ed95..0x56edab) in the original."""
    bad = []
    dats = [f for f in os.listdir(out) if f.endswith(".dat")]
    if len(dats) != 20:
        bad.append(("data files", len(dats)))
    comps = lambda X: {i32(r, 0): cmdb.cstr(r, 4, 51) for r in X.rows(12)}
    cf, ct = comps(F), comps(T)
    renamed = {c for c in ct if ct[c] in FREE_ROWS and cf.get(c, "").startswith(GROUPS)}   # slice 5, step 6
    bad += [("competition row", c, ct[c], cf.get(c)) for c in sorted(ct) if cf.get(c) != ct[c] and c not in renamed]
    if len(renamed) != len(FREE_ROWS):
        bad.append(("Turkish group rows in the free rows", sorted(renamed)))
    # One competition array: club comps 0 to n - 1, then the nation comps from n (ids as 3.9.68), so no id clashes.
    club_ids = [i32(r, 0) for r in F.rows(12)]
    nation_ids = [i32(r, 0) for r in F.rows(16)]
    if club_ids != list(range(len(club_ids))) or nation_ids != list(range(len(club_ids), len(club_ids) + len(nation_ids))):
        bad.append(("competition ids clash or have gaps", len(club_ids), nation_ids[:3]))
    if nation_ids != [i32(r, 0) for r in T.rows(16)]:
        bad.append(("nation competition ids moved",))
    count = lambda X: collections.Counter(i32(r, 0x57) for r in X.rows(0) if i32(r, 0x57) >= 0)
    nf, nt = count(F), count(T)
    bad += [("division count", ct.get(d, d), nt[d], nf[d]) for d in sorted(set(nf) | set(nt)) if nf[d] != nt[d]]
    sizes, by_name = class_sizes(), {v: k for k, v in cf.items()}
    for name, n in sorted(sizes.items()):
        if name not in by_name or nf[by_name[name]] < n:
            bad.append(("class size", name, n, nf.get(by_name.get(name), 0)))
    print(f"classes checked: {len(sizes)}, divisions checked: {len(nt)}")
    return bad


def check_groups(F, T):
    """Slice 5 (Q16, Q29, Q43): the three Xbox group rows fill the free rows of 3.9.68 (FREE_ROWS, nation Turkey), 18,
    17 and 17 Turkish clubs carry their group tag (club +0x60, unused for them in 3.9.68) and no other club does. The
    tagged clubs keep their divisions (check_structure)."""
    bad = []
    old = {cmdb.cstr(r, 4, 51): i32(r, 0) for r in T.rows(12)}
    rows = {i32(r, 0): r for r in F.rows(12)}
    nations = {i32(r, 0): cmdb.cstr(r, 4, 51) for r in F.rows(4)}
    ids = []
    for (letter, size), free in zip(SIZES.items(), FREE_ROWS):
        cid = old.get(free, -1)
        r = rows.get(cid)
        if r is None or cmdb.cstr(r, 4, 51) != GROUPS + letter or nations.get(i32(r, 0x5d)) != "Turkey":
            bad.append(("Turkish group row", cid, r is not None and cmdb.cstr(r, 4, 51)))
        ids.append(cid)
    tags = collections.Counter(i32(r, 0x60) for r in F.rows(0) if i32(r, 0x60) in ids)
    if [tags[c] for c in ids] != list(SIZES.values()):
        bad.append(("Turkish group tags", dict(tags)))
    was = {i32(r, 0): i32(r, 0x60) for r in T.rows(0)}
    bad += [("group tag over a 3.9.68 tag", i32(r, 0)) for r in F.rows(0)
            if i32(r, 0x60) in ids and was.get(i32(r, 0)) not in (-1, -2)]
    nat = {i32(r, 0): nations.get(i32(r, 0x53)) for r in F.rows(0)}
    bad += [("group tag of a club not of Turkey", c) for c in (i32(r, 0) for r in F.rows(0) if i32(r, 0x60) in ids)
            if nat[c] != "Turkey"]
    refs = collections.Counter(i32(r, o) for t in (0, 1) for r in F.rows(t) for o in (0x57, 0x5b))
    refs.update(i32(r, 4) for t in (19, 20) for r in F.rows(t))
    bad += [("a group row has other references", c, refs[c]) for c in ids if refs[c]]
    print(f"Turkish groups checked: {sum(tags.values())} clubs in rows {ids}")
    return bad


def check_facts(F):
    bad = []
    names = {cmdb.cstr(r, 4, 51) for r in F.rows(0)}
    if "Florentia Viola" not in names or "Fiorentina" in names:
        bad.append(("Florentia Viola",))
    for first, second, year, club in (("Fabio", "Celestini", 1975, "Olympique Marseille"),
                                      ("Wayne", "Rooney", 1985, "Everton")):
        hits = find(F, first, second, year)
        if [club_of(F, t) for t in hits] != [club]:
            bad.append((second, [club_of(F, t) for t in hits]))
    rooney = find(F, "Wayne", "Rooney", 1985)
    if rooney and cmdb1.u("<h", F.rows(10)[i32(F.rows(6)[rooney[0]], 0x91)], 5)[0] != 135:
        bad.append(("Rooney's current ability",))
    cn, st, pl = cmdb.names(F, 15), F.rows(6), F.rows(10)
    ronaldo = [i32(r, 0) for r in st if i32(r, 0x0c) >= 0 and cn.get(i32(r, 0x0c)) == "Ronaldo" and r[0x5f] & 2]
    best = max(ronaldo, key=lambda t: cmdb1.u("<h", pl[i32(st[t], 0x91)], 5)[0], default=None)
    if best is None or not club_of(F, best).startswith("Real Madrid") or cmdb1.date_at(st[best], 0x10).year != 1976:
        bad.append(("Ronaldo", best is not None and club_of(F, best)))
    return bad


def same_builds(root, out, template):
    """Two builds with PYTHONHASHSEED 1 and 2 into a new folder give the files of out."""
    bad = []
    script = os.path.join(HERE, "xbox0203", "build_2002.py")
    with tempfile.TemporaryDirectory() as t:
        for seed in ("1", "2"):
            ch = os.path.join(t, "changes-" + seed)
            shutil.copytree(os.path.join(root, "changes"), ch)
            res = subprocess.run([sys.executable, "-P", "-s", script, ch, os.path.join(root, "disc", "data"),
                                  template, os.path.join(t, "out-" + seed), os.path.join(t, "work-" + seed),
                                  ],
                                 env={k: v for k, v in os.environ.items() if not k.startswith("PYTHON")} | dict(
                                     PYTHONHASHSEED=seed),
                                 capture_output=True, text=True, cwd=t)
            if res.returncode:
                return [("build with seed " + seed, res.stdout[-500:], res.stderr[-500:])]
            if not filecmp.cmp(os.path.join(ch, "kept-pairs.tsv"), os.path.join(root, "changes", "kept-pairs.tsv"),
                               shallow=False):
                bad.append(("kept-pairs.tsv differs with seed", seed))
        for seed in ("1", "2"):
            got = os.path.join(t, "out-" + seed)
            files = sorted(os.listdir(got))
            if files != sorted(os.listdir(out)):
                bad.append(("file list", seed))
            bad += [("differs", seed, f) for f in files if not filecmp.cmp(os.path.join(got, f), os.path.join(out, f),
                                                                           shallow=False)]
    return bad


def main(argv):
    root = argv[0] if argv else os.path.join(ROOT, "xbox-0203-work")
    out = os.path.join(root, "build", "Summer 2002")
    work = os.path.join(root, "build", "summer-2002-work")
    changes = os.path.join(root, "changes")
    template = os.path.join(os.path.dirname(root), "clean-3968", "Data")
    if not all(os.path.isdir(p) for p in (out, work, changes, template, os.path.join(root, "disc", "data"))):
        print("SKIP: the Summer 2002 data is not here")
        return 0
    F, T = cmdb1.open_folder(out), cmdb1.open_folder(template)
    bad = check_changes(F, changes, work) + check_untouched(F, T, changes, work) + check_facts(F)
    bad += check_structure(F, T, out)
    bad += check_groups(F, T)
    bad += [("more rows than 3.9.68", t) for t, e in F.idx.items() if e["count"] > T.idx[t]["count"]]
    with contextlib.redirect_stdout(io.StringIO()) as log:
        if duplicates.check(out, os.path.join(changes, "kept-pairs.tsv")):
            bad.append(("duplicates.py check", log.getvalue().splitlines()[-1]))
    bad += same_builds(root, out, template)
    for x in bad:
        print("FAIL", x)
    print("PASS" if not bad else f"FAILED: {len(bad)}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
