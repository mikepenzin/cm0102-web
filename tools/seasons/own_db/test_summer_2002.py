"""Pipeline seam of Summer 2002: checks the built folder against the change list. Skips when the data is absent.

Usage: python3 -I test_summer_2002.py [<work root>]

<work root> holds changes/ (change_list.py), build/Summer 2002 (build_2002.py), build/summer-2002-work and
disc/data (the Xbox data). Default: xbox-0203-work beside the main checkout of the repo. The 3.9.68 Data folder is
clean-3968/Data beside it. The data stays outside the repo; this test reads it only.

Checks (spec docs/specs/2026-10-24-summer-2002-db-spec.md, Testing):
- every applied row of the change list is in the folder: club, join date, contract end and job of kinds 1 to 3
  (through ids.tsv, the staff ids after the duplicate merge; a free non-player whom staff_slots.py placed is at
  the club and dates of his staff-slots.tsv row), ratings of kind 4, divisions of kind 5, the club name of kind 6;
- duplicates.py check passes with kept-pairs.tsv, and no table has more rows than 3.9.68;
- known facts: Celestini at Olympique Marseille, Fiorentina is Florentia Viola, Rooney at Everton with current
  ability 135, Ronaldo (the "Ronaldo" of the highest current ability, born 1976) at Real Madrid C.F.;
- two builds with different PYTHONHASHSEED values (python3 -P -s, as -I ignores the seed) give the same files as
  the folder.
"""
import contextlib
import csv
import filecmp
import io
import os
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
from change_list import REPO  # noqa: E402
from read_xbox0203 import FM_JOB  # noqa: E402

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
            if club != int(row["new_club_id"]) and not (int(row["new_club_id"]) == -1 and t in placed):
                bad.append((kind, row["person"], "club", club, row["new_club_id"]))
            if t not in placed and (iso(cmdb1.date_at(r, 0x3e)), iso(cmdb1.date_at(r, 0x46))) != (
                    row["joined_xbox"], row["cend_xbox"]):
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
    for row in read_tsv(os.path.join(changes, "6-names.tsv")):
        if row["decision"] == "applied" and row["item"] == "club name":
            r = clubs[int(row["id_3968"])]
            if f"{cmdb.cstr(r, 4, 51)} / {cmdb.cstr(r, 0x38, 26)}" != row["value_xbox"]:
                bad.append(("club name", row["value_xbox"]))
            n += 1
    print(f"applied rows checked: {n}")
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
                                  "--no-turkish-groups"], env=dict(os.environ, PYTHONHASHSEED=seed),
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
    bad = check_changes(F, changes, work) + check_facts(F)
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
