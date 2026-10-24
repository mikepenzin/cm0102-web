"""Build the Summer 2002 database: a copy of the 3.9.68 files with the applied rows of the change list.

Usage: python3 -I build_2002.py <changes> <xbox data> <3.9.68 Data> <out folder> <work folder>
                                [--exe <cm0102.exe>] [--no-turkish-groups]

<changes> is the folder of change_list.py (with Max's answers Q21 to Q28 applied). The two data folders are read
only, and the out folder (the new database, "Summer 2002") and the work folder (reports) must be new and outside
the repo. The data is untrusted input: every read and write is bounds-checked (cmdb1.py) and nothing from it runs.
Commit no Xbox data and nothing made from it. Spec: docs/specs/2026-10-24-summer-2002-db-spec.md, slice 3.

The build patches the format 1 rows in place and adds no row, so every count of index.dat stays, except the rows
that the duplicate merge removes. A change that would need a new row stops the build. Steps:
1. Staff (kinds 1 to 3, decision "applied"): club (+0x39, the new_club_id column), join date and contract end
   (+0x3e and +0x46, the 16 bytes of the Xbox row) and, where take_job is "yes" (Q22), the job byte (+0x3d). The
   Xbox row must have the flags (+0x5f) of the 3.9.68 row, else the job would need another kind of row.
2. Ratings (kind 4): current and potential ability, the three reputations, the 12 position bytes and the 42
   attribute bytes (player row +0x05 to +0x45) of the Xbox player row.
3. Club lists: each changed person leaves every staff list of his old club (club.dat +0x9b to +0x1e0, the
   duplicates.CLUB_STAFF lists) and joins the list of his job at his new club: squad +0xd7 (convert.in_squad),
   manager +0xcf, assistant +0xd3, coaches +0x19f, scouts +0x1b3, physios +0x1cf (convert.JOB_LISTS), chairman
   +0xbf (job 1) and directors +0xc3 (jobs 2 to 4), as change_list.SLOTS. change_list.py holds the rows whose
   list is full, so a full list here stops the build. convert.check_squad_lists then checks every club.
4. Divisions (kind 5, Q24, Q34 to Q37): club +0x57 is the new_division_id column (-2: no division).
5. Names (kind 6): the long name (+0x04, 51 bytes) and short name (+0x38, 26 bytes) of Fiorentina come from the
   Xbox row (Florentia Viola, Q5).
6. Turkish groups (Q11): the three Xbox "Category B Group" competitions need three free 3.9.68 rows of
   club_comp.dat. A free row has no club that points at it (club +0x57, +0x5b, +0x60), no history row (19, 20),
   and a name that is not a key name of the game: set_key_ids 0x601a40 finds key competitions by name and shows
   an error box for each that it does not find (0x6043ac to 0x60ba6e, for example 0x607547 for "Turkish 2.
   Division Group 1"). The proof needs cm0102.exe (--exe) and the key name chains of src/cm3/key_ids_table.c.
   With fewer than three free Turkish rows the build stops, unless --no-turkish-groups leaves the step out (the
   clubs then keep their 3.9.68 groups).
7. Duplicates (Q28): duplicates.merge_folder merges the template pairs by the 2006/07 rules and keeps the pairs
   that duplicates.kept_apart calls two people. It runs again until no other pair is left. The kept pairs, with
   their ids after the merge, go to <changes>/kept-pairs.tsv, the list that duplicates.py check accepts.
8. The folder is written, then staff_slots.py (first season 2002, Q12) fills empty staff slots until a new game
   with every league selected stays under the staff limit, with nobody whom the list changed, and duplicates.py
   check runs with kept-pairs.tsv. An applied row of a kind that no step writes stops the build.

Work folder: merge.tsv (each kept row and the rows merged into it), ids.tsv (old staff id, new staff id),
staff-slots.tsv, duplicates-check.txt and build.txt (the counts).
"""
import argparse
import collections
import csv
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OWN_DB = os.path.dirname(HERE)
REPO = os.path.realpath(os.path.dirname(os.path.dirname(os.path.dirname(OWN_DB))))
sys.path.insert(0, OWN_DB)
sys.path.insert(0, HERE)
import cmdb  # noqa: E402
import cmdb1  # noqa: E402
import duplicates  # noqa: E402
import staff_slots  # noqa: E402
from change_list import SLOTS, outside_repo, slot_keys  # noqa: E402
from cmdb1 import put, put_bytes, u  # noqa: E402
from convert import check_squad_lists  # noqa: E402

FIRST_SEASON = 2002                     # the dates are real dates (Q12)
DATES = (0x3e, 0x4e)                    # joined and contract end, 8 bytes each
RATINGS = (0x05, 0x45)                  # player row: abilities, reputations, positions, attributes
GROUPS = "Turkish 2. Division Category B Group "
KEY_TABLE = os.path.join(REPO, "src", "cm3", "key_ids_table.c")
SEASON_NATION = "Turkey"


def need(cond, msg):
    if not cond:
        raise SystemExit("build_2002: " + msg)


def i32(r, o):
    return u("<i", r, o)[0]


def read_tsv(path):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter="\t", quoting=csv.QUOTE_NONE))


def person(F, names, r):
    """The text of a person as change_list.who writes it."""
    fn, sn, cn = names
    a, b, c = u("<iii", r, 4)
    text = f"{fn.get(a, '')} {sn.get(b, '')}"
    return text + (f" ({cn[c]})" if c >= 0 and cn.get(c) else "")


def same_name(a, b):
    """The names of one person in the two sets: equal after cmdb.norm, without the common name (the match of
    change_list.py compares first name and surname)."""
    return cmdb.norm(a.split(" (")[0]) == cmdb.norm(b.split(" (")[0])


def names_of(F):
    return cmdb.names(F, 13), cmdb.names(F, 14), cmdb.names(F, 15)


# ---- steps 1 and 2: staff and ratings

def apply_staff(F, X, changes, log):
    """Kinds 1 to 3. Returns {staff id: old club} of the changed people."""
    st, xs = F.rows(6), X.rows(6)
    tn, xn = names_of(F), names_of(X)
    clubs = {i32(r, 0) for t in (0, 1) for r in F.rows(t)}
    changed, count = {}, collections.Counter()
    for kind in ("1-moves", "2-releases", "3-dates"):
        for row in read_tsv(os.path.join(changes, kind + ".tsv")):
            if row["decision"] != "applied":
                continue
            t, x, new = int(row["id_3968"]), int(row["id_xbox"]), int(row["new_club_id"])
            if x == -1:                                 # Q39: a holder leaves his place, no Xbox row, dates stay
                need(kind == "2-releases" and new == -1 and 0 <= t < len(st), f"{kind}: row {t} has no Xbox row")
                need(person(F, tn, st[t]) == row["person"], f"{kind}: staff {t} is not {row['person']}")
                changed.setdefault(t, i32(st[t], 0x39))
                put(st[t], "<i", 0x39, -1)
                count[kind] += 1
                continue
            need(0 <= t < len(st) and 0 <= x < len(xs), f"{kind}: row ids {t}, {x} are outside the staff tables")
            r, xr = st[t], xs[x]
            need(person(F, tn, r) == row["person"] and same_name(person(X, xn, xr), row["person"]),
                 f"{kind}: staff {t} or Xbox {x} is not {row['person']}: the change list is for other data")
            need(new < 0 or new in clubs, f"{kind}: {row['person']} goes to the unknown club {new}")
            old = i32(r, 0x39)
            if kind == "3-dates":
                need(new == old, f"{kind}: {row['person']} has another club in the data than in the list")
            if row["take_job"] == "yes":
                need(r[0x5f] == xr[0x5f], f"{kind}: {row['person']} has flags {r[0x5f]} in 3.9.68 and {xr[0x5f]} "
                                          "on Xbox: the job would need another kind of row")
                if r[0x3d] != xr[0x3d]:
                    changed.setdefault(t, old)
                    r[0x3d] = xr[0x3d]
                    count["job"] += 1
            if new != old:
                changed.setdefault(t, old)
                put(r, "<i", 0x39, new)
            put_bytes(r, DATES[0], bytes(xr[DATES[0]:DATES[1]]))
            count[kind] += 1
    log.append(f"staff: {count['1-moves']} moves, {count['2-releases']} releases, {count['3-dates']} date rows, "
               f"{count['job']} jobs")
    return changed


def apply_ratings(F, X, changes, log):
    st, xs, tp, xp = F.rows(6), X.rows(6), F.rows(10), X.rows(10)
    n = 0
    for row in read_tsv(os.path.join(changes, "4-ratings.tsv")):
        if row["decision"] != "applied":
            continue
        t, x = int(row["id_3968"]), int(row["id_xbox"])
        need(0 <= t < len(st) and 0 <= x < len(xs), f"ratings: row ids {t}, {x} are outside the staff tables")
        need(st[t][0x5f] & 2 and xs[x][0x5f] & 2, f"ratings: {row['person']} is not a player in both sets")
        a, b = i32(st[t], 0x91), i32(xs[x], 0x91)
        need(0 <= a < len(tp) and 0 <= b < len(xp), f"ratings: {row['person']} has no player row")
        old = bytes(tp[a][RATINGS[0]:RATINGS[1]])
        put_bytes(tp[a], RATINGS[0], bytes(xp[b][RATINGS[0]:RATINGS[1]]))
        need(u("<hh", tp[a], 5) == (int(row["ca_xbox"]), int(row["pa_xbox"])) and old != bytes(
            tp[a][RATINGS[0]:RATINGS[1]]), f"ratings: {row['person']} does not get the listed ratings")
        n += 1
    log.append(f"ratings: {n} players")


# ---- step 3: club lists

def fix_lists(F, changed, log):
    """The old club's lists lose each changed person, the new club's list of his job gets him."""
    st = F.rows(6)
    by_id = {i32(r, 0): r for r in F.rows(0)}
    touched = sorted({c for t, c in changed.items() if c >= 0} | {i32(st[t], 0x39) for t in changed
                                                                   if i32(st[t], 0x39) >= 0})
    for c in touched:
        need(c in by_id, f"club lists: club {c} is not in club.dat")
        r = by_id[c]
        for off, n in duplicates.CLUB_STAFF:
            if off == 0x9b:
                continue                # favourite people of the club, of any club
            ids = [i32(r, off + 4 * j) for j in range(n)]
            keep = [v for v in ids if v not in changed]
            if len(keep) != len(ids):
                keep = [v for v in keep if v >= 0] if n > 1 else keep
                keep += [-1] * (n - len(keep))
                for j in range(n):
                    put(r, "<i", off + 4 * j, keep[j])
    added = 0
    for t in sorted(changed):
        c = i32(st[t], 0x39)
        if c < 0:
            continue
        r = by_id[c]
        for key in slot_keys(st[t][0x5f], st[t][0x3d]):
            off, n = SLOTS[key]
            ids = [i32(r, off + 4 * j) for j in range(n)]
            need(-1 in ids, f"club lists: list {key} of club {c} is full, so staff {t} cannot join it")
            put(r, "<i", off + 4 * ids.index(-1), t)
            added += 1
    check_squad_lists(F.rows(0), F.rows(6))
    log.append(f"club lists: {len(touched)} clubs, {added} entries added")


# ---- steps 4 and 5: divisions and names

def apply_divisions(F, changes, log):
    comps = {i32(r, 0): cmdb.cstr(r, 4, 51) for r in F.rows(12)}
    by_id = {i32(r, 0): r for r in F.rows(0)}
    n = 0
    for row in read_tsv(os.path.join(changes, "5-leagues.tsv")):
        if row["decision"] != "applied":
            continue
        c, d = int(row["id_3968"]), int(row["new_division_id"])
        need(c in by_id and (d in comps or d == -2), f"leagues: club {c} or competition {d} is not in the data")
        v = i32(by_id[c], 0x57)
        have = comps.get(v, "none") if v >= 0 else "none"          # the change list's name of no division
        want = row.get("division_new") or row["division_xbox"]     # the checked division (Q34), else the Xbox one
        need(have == row["division_3968"] and comps.get(d, "none") == want,
             f"leagues: {row['club']} does not have the listed divisions")
        put(by_id[c], "<i", 0x57, d)
        n += 1
    log.append(f"divisions: {n} clubs")


def apply_names(F, X, changes, log):
    tc = {i32(r, 0): r for r in F.rows(0)}
    xc = {i32(r, 0): r for r in X.rows(0)}
    n = 0
    for row in read_tsv(os.path.join(changes, "6-names.tsv")):
        if row["decision"] != "applied":
            continue
        need(row["item"] == "club name", f"names: the build does not know the applied row {row['item']!r}")
        t, x = int(row["id_3968"]), int(row["id_xbox"])
        need(t in tc and x in xc, f"names: club {t} or Xbox club {x} is not in the data")
        r, xr = tc[t], xc[x]
        need(f"{cmdb.cstr(r, 4, 51)} / {cmdb.cstr(r, 0x38, 26)}" == row["value_3968"] and
             f"{cmdb.cstr(xr, 4, 51)} / {cmdb.cstr(xr, 0x38, 26)}" == row["value_xbox"],
             f"names: club {t} does not have the listed names")
        put_bytes(r, 4, bytes(xr[4:55]))
        put_bytes(r, 0x38, bytes(xr[0x38:0x52]))
        n += 1
    log.append(f"club names: {n}")


# ---- step 6: Turkish groups

def exe_strings(exe):
    """A reader of zero-ended strings at virtual addresses of a PE file."""
    b = open(exe, "rb").read()
    need(len(b) > 0x40 and b[:2] == b"MZ", f"{exe} is not an exe")
    pe = struct.unpack_from("<I", b, 0x3c)[0]
    need(pe + 0xf8 <= len(b) and b[pe:pe + 4] == b"PE\0\0", f"{exe} has no PE header")
    n, osz = struct.unpack_from("<H", b, pe + 6)[0], struct.unpack_from("<H", b, pe + 20)[0]
    base = struct.unpack_from("<I", b, pe + 24 + 28)[0]
    secs = [struct.unpack_from("<IIII", b, pe + 24 + osz + 40 * i + 8) for i in range(n)]

    def at(addr):
        rva = addr - base
        for vs, va, rs, ra in secs:
            if va <= rva < va + min(vs, rs):
                o = ra + rva - va
                end = b.find(b"\0", o)
                need(end >= 0, f"{exe}: no end of the string at {addr:#x}")
                return b[o:end].decode("latin1")
        raise SystemExit(f"build_2002: {exe}: {addr:#x} is in no section")
    return at


def key_names(exe):
    """The club comp names that the game's set_key_ids looks for (lower case)."""
    text = open(KEY_TABLE, encoding="utf-8").read()
    body = text.split("const key_id_entry_t g_key_club_comp_chain[] = {")
    need(len(body) == 2, f"{KEY_TABLE} has no club comp chain")
    at = exe_strings(exe)
    return {at(int(a, 16)).lower() for a in re.findall(r"\{(0x[0-9a-f]+),", body[1].split("};")[0])}


def free_rows(F, keys):
    """3.9.68 club_comp rows of Turkey (or of no nation, with a Turkish name) that nothing points at."""
    nations = {i32(r, 0): cmdb.cstr(r, 4, 51) for r in F.rows(4)}
    used = collections.Counter()
    for t in (0, 1):
        for r in F.rows(t):
            for o in (0x57, 0x5b, 0x60):
                used[i32(r, o)] += 1
    for t in (19, 20):
        for r in F.rows(t):
            used[i32(r, 4)] += 1
    out = []
    for r in F.rows(12):
        cid, name, nat = i32(r, 0), cmdb.cstr(r, 4, 51), i32(r, 0x5d)
        turkish = nations.get(nat) == SEASON_NATION or nat < 0 and name.startswith("Turkish")
        if turkish and not used[cid] and name.lower() not in keys:
            out.append(cid)
    return out


def apply_groups(F, X, exe, skip, log):
    groups = sorted((cmdb.cstr(r, 4, 51), i32(r, 0)) for r in X.rows(12) if cmdb.cstr(r, 4, 51).startswith(GROUPS))
    need(len(groups) == 3, f"Turkish groups: the Xbox data has {len(groups)} groups, not 3")
    if skip:
        log.append("Turkish groups: left out (--no-turkish-groups), the clubs keep their 3.9.68 groups")
        return
    need(exe, "Turkish groups: the proof of free rows needs --exe <cm0102.exe> (or --no-turkish-groups)")
    free = free_rows(F, key_names(exe))
    need(len(free) >= len(groups), f"Turkish groups: only {len(free)} free Turkish rows ({free}) for "
                                   f"{len(groups)} groups, and the build adds no rows (--no-turkish-groups "
                                   "leaves the groups out)")
    raise SystemExit("build_2002: Turkish groups: free rows exist, but the member clubs are not decided in the list")


# ---- step 7: duplicates

def merge(F, work, log):
    """Merges until no pair is left but the kept ones. Returns ({old id: new id}, [(id, id, reason)])."""
    n = F.table(6)[3]
    ids = {i: i for i in range(n)}
    rnd = 0
    while True:
        rnd += 1
        merged, cur, kept = duplicates.merge_folder(F, n, n, os.path.join(work, f"merge-{rnd}.tsv"),
                                                    duplicates.kept_apart)
        ids = {i: cur[j] for i, j in ids.items()}
        n = F.table(6)[3]
        log.append(f"duplicates round {rnd}: {merged} rows merged, {len(kept)} pairs kept apart")
        if not merged:
            return ids, kept
        need(rnd < 5, "duplicates: the merge does not settle")


def staff_refs(F):
    """Every staff id that the club lists, the favourites, the awards and the careers hold."""
    refs = []
    for t in (0, 1):
        for r in F.rows(t):
            refs += [i32(r, off + 4 * j) for off, k in duplicates.CLUB_STAFF for j in range(k)]
    for r in F.rows(6):
        refs += [i32(r, duplicates.FAV_STAFF_1[0] + 4 * j) for j in range(duplicates.FAV_STAFF_1[1])]
    for r in F.rows(9):
        refs += [i32(r, duplicates.NP_STAFF[0] + 4 * j) for j in range(duplicates.NP_STAFF[1])]
    refs += [i32(r, o) for r in F.rows(18) for o in duplicates.AWARD_STAFF_IDS]
    refs += [i32(r, 4) for r in F.rows(17)]
    return refs


def check_refs(F, beyond):
    """Every id is its row number, and every staff reference names a row. beyond: the values past the end that
    the template already holds (3.9.68 has 329 in the non-player rows +0x23, all above its staff count), which
    stay as they are."""
    st = F.rows(6)
    n = len(st)
    need(all(i32(r, 0) == k for k, r in enumerate(st)), "a staff id is not its row number")
    bad = [v for v in staff_refs(F) if v >= n and v not in beyond]
    need(not bad, f"{len(bad)} staff references point past the staff table, for example {bad[:5]}")


def write_kept(path, F, kept):
    st, _ = cmdb1.staff_view(F)
    with open(path, "w", encoding="utf-8", newline="") as h:
        h.write("id_a\tid_b\treason\tperson_a\tperson_b\n")
        for a, b, why in kept:
            h.write(f"{a}\t{b}\t{why}\t{duplicates.show(st[a])}\t{duplicates.show(st[b])}\n")


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("changes")
    ap.add_argument("xbox")
    ap.add_argument("template")
    ap.add_argument("out")
    ap.add_argument("work")
    ap.add_argument("--exe")
    ap.add_argument("--no-turkish-groups", action="store_true")
    a = ap.parse_args(argv)
    for p in (a.out, a.work):
        need(outside_repo(p), f"{p} must be outside the repo")
        need(not os.path.exists(p), f"{p} exists: it must be new")
    F, X = cmdb1.open_folder(a.template), cmdb1.open_folder(a.xbox)
    sizes = {f: len(b) for f, b in F.raw.items()}
    beyond = {v for v in staff_refs(F) if v >= F.table(6)[3]}
    counts = {t: e["count"] for t, e in F.idx.items()}
    log = []
    changed = apply_staff(F, X, a.changes, log)
    apply_ratings(F, X, a.changes, log)
    fix_lists(F, changed, log)
    apply_divisions(F, a.changes, log)
    apply_names(F, X, a.changes, log)
    apply_groups(F, X, a.exe, a.no_turkish_groups, log)
    for f, b in F.raw.items():
        need(len(b) == sizes[f], f"{f} changed its length before the merge: a row was added")
    os.makedirs(a.work)
    ids, kept = merge(F, a.work, log)
    check_refs(F, beyond)
    for t, e in F.idx.items():
        need(e["count"] <= counts[t], f"table {t} has more rows than 3.9.68")
    cmdb1.write_folder(F, a.out)
    with open(os.path.join(a.work, "ids.tsv"), "w", encoding="utf-8") as h:
        h.write("old\tnew\n")
        h.writelines(f"{i}\t{j}\n" for i, j in sorted(ids.items()))
    write_kept(os.path.join(a.changes, "kept-pairs.tsv"), cmdb1.open_folder(a.out), kept)
    for kind in ("7-unclear",):
        need(not any(r["decision"] == "applied" for r in read_tsv(os.path.join(a.changes, kind + ".tsv"))),
             f"{kind}: the build does not know applied rows of this kind")
    # People whom the list changed keep its values: staff_slots.py takes no one of them into its pool.
    listed = {ids[int(r["id_3968"])] for k in ("1-moves", "2-releases", "3-dates")
              for r in read_tsv(os.path.join(a.changes, k + ".tsv")) if r["decision"] == "applied"}
    staff_slots.main([a.out, os.path.join(a.work, "staff-slots.tsv"), str(staff_slots.TARGET), str(FIRST_SEASON)],
                     keep_out=listed)
    saved = sys.stdout
    with open(os.path.join(a.work, "duplicates-check.txt"), "w", encoding="utf-8") as h:
        sys.stdout = h
        try:
            bad = duplicates.check(a.out, os.path.join(a.changes, "kept-pairs.tsv"))
        finally:
            sys.stdout = saved
    with open(os.path.join(a.work, "duplicates-check.txt"), encoding="utf-8") as h:
        log.append(h.read().splitlines()[-1])
    G = cmdb1.open_folder(a.out)
    log.append("rows: " + ", ".join(f"table {t} {counts[t]} -> {e['count']}" for t, e in sorted(G.idx.items())
                                    if e["count"] != counts[t]))
    with open(os.path.join(a.work, "build.txt"), "w", encoding="utf-8") as h:
        h.write("\n".join(log) + "\n")
    print("\n".join(log))
    need(not bad, "duplicates: the check failed (work folder duplicates-check.txt)")


if __name__ == "__main__":
    main(sys.argv[1:])
