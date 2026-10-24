"""Duplicate players in an own season database: find them, merge them, and fail the build when one is left.

usage:
    python3 -I duplicates.py merge <db folder> <template folder> <converter folder> <report.tsv>
    python3 -I duplicates.py check <db folder> [<allowed.tsv>]

A forum report on the 2006/07 database (21 October 2026) found players twice: two Sergio Ramos at Real Madrid, and
Lúcio at Bayern and again as a free agent. Three steps of the build make a second row of a person whom the
template already holds:
- fill.py copies an FM player that match.py did not match from the older database. Its "have" test compares the
  exact names and date, so a name split another way ("Lucimar | da Silva Ferreira" against "Lucimar da Silva |
  Ferreira") or a date one day off passes. Its third test (club and one surname word) took the older database's
  Sergio Ramos for FM's Felipe Ramos of Real Madrid Castilla.
- match.py drops a template person whom two FM people tie for (Lúcio and Márcio Carioca, both "Ferreira" and the
  date one day either way), so Lúcio came in again through fill.py while the template's Lúcio lost his club.
- keyplayers.py adds a new row for a key player whom the template holds as a free agent (Hossam Ghaly).
The template itself has some people twice (Paulo Jorge at Benfica and a free Paulo Jorge with the same date).

One person (find): the same nationality, with names and dates that agree as people.py says (names joined, a common
name, another order or an extra word; dates up to 2 days apart, as the templates store some dates one day later
than FM), or the same joined name at the same club for two players (kind 2) at most 3 years apart (the copy of
Sergio Ramos had Felipe Ramos's date, 1.5 years later; Dunfermline's coach Scott Thomson, born 1963, and its player
Scott Thomson, born 1969, are two people). A date is unknown when its year is 1900 or less, and for a person with no
first name when it is 1 January or 1 February (the templates' placeholder dates of such rows).
fill.py now looks for the FM player in the template first with the same rules, so the build makes fewer such rows;
the merge stays for the template's own pairs and for what that misses.

Merge: the rows of one person keep one row, the keeper: a key player's row (its ratings were chosen for it), else a
template row, else a fill row; then a row at a club; then the lowest id. The keeper takes the club, job, dates and
contract (+0x39..+0x5e) of a fill or key row at a club (FM's club on 31 August), else of any row at a club when he
has none. Every reference to a dropped row goes to the keeper: club lists and slots (club.dat, nat_club.dat:
+0x9b..+0x1e0; a list of a club that is not the keeper's club or national side loses the entry), favourite people
of the preferences rows
(+0x1c) and non-player rows (+0x23), the award history (the staff id of each of its three places,
AWARD_STAFF_IDS). The dropped row's careers
(staff_history) go to the keeper when he has none, else they are removed. Then the last staff row moves into the
dropped row's place (its id and every reference to it change), with its preferences row, and the tables behind
the staff table in staff.dat move up. Ids stay equal to the row numbers, as the game's id pass needs
(src/cm3/database_links.c).

Check: no pair is left, except pairs in allowed.tsv (two ids per line: people who share a name and a birthday, or
the pairs of Summer 2002 that kept_apart keeps as two people), every pair of allowed.tsv is still a pair, and the
squad lists agree with the staff rows (convert.check_squad_lists). The merge runs the list check too. The check
reads format 2 and format 1 (the 3.9.68 layout of Summer 2002, cmdb1.py). For format 1 the build calls
merge_folder (xbox0203/build_2002.py): the favourite people are in the staff row (+0x79) there, as format 1 has no
preferences table, and kept_apart keeps the pairs that are two people.
"""
import collections
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cmdb1  # noqa: E402
from cmdb import Folder, staff_view, norm  # noqa: E402
from convert import check_squad_lists  # noqa: E402
from people import PLACEHOLDER_DAYS, days_apart_ok, names_agree  # noqa: E402

CLUB_SIZE = 581
CLUB_STAFF = [(0x9b, 6), (0xbf, 1), (0xc3, 3), (0xcf, 1), (0xd3, 1), (0xd7, 50), (0x19f, 5), (0x1b3, 7), (0x1cf, 3),
              (0x1e0, 20)]
PREF_STAFF, NP_STAFF = (0x1c, 6), (0x23, 8)
FAV_STAFF_1 = (0x79, 6)         # format 1: the favourite people in the staff row (convert_staff, database.c)
# Award history (staff_comp_history, 58 bytes): three places of 16 bytes from +0x0a, each the first name id, the
# second name id, the staff id and the club id (docs/research/2026-10-17-season-date-shifts.md): staff ids at these.
AWARD_STAFF_IDS = (0x0a + 8, 0x0a + 24, 0x0a + 40)
PLACEMENT = (0x39, 0x5f)        # club, job, joined, contract end and the contract fields after it


def i32(b, o):
    return struct.unpack_from("<i", b, o)[0]


def known(x):
    d, y = x["dob"]
    if y <= 1900:
        return False
    return not (not norm(x["first"]) and d in (0, 31))


def days(x):
    return x["dob"][1] * 365.25 + x["dob"][0]


def joined(x):
    return norm(x["first"] + " " + x["second"])


def same_club(a, b):
    """Two players of one joined name at one club: at most 3 years apart, or a date unknown."""
    if not (joined(a) and joined(a) == joined(b) and a["club"] >= 0 and a["club"] == b["club"]):
        return None
    if known(a) and known(b):
        ok = a["kind"] == b["kind"] == 2 and 2 < abs(days(a) - days(b)) <= 3 * 366
        return "same club" if ok else None
    return "same club, a date unknown"


def find(F):
    """{(id, id): reason} for the player rows of one person (people.py: names and dates; or same_club)."""
    st, _ = cmdb1.staff_view(F) if isinstance(F, cmdb1.Folder1) else staff_view(F)
    pl = [x for x in st if x["np"] >= 0]
    pairs = {}
    bynat = collections.defaultdict(list)
    for x in pl:
        if known(x):
            bynat[x["nation"]].append(x)
    for rows in bynat.values():
        rows.sort(key=days)
        for i in range(len(rows)):
            a = rows[i]
            for j in range(i + 1, len(rows)):
                b = rows[j]
                d = days(b) - days(a)
                if d > 2:
                    break
                n = names_agree(a["first"], a["second"], a["common"], b["first"], b["second"], b["common"])
                if n and days_apart_ok(n, d, a["dob"][0] in PLACEHOLDER_DAYS or b["dob"][0] in PLACEHOLDER_DAYS):
                    pairs.setdefault(tuple(sorted((a["id"], b["id"]))), f"names {n}, dates {d:.0f} days apart")
    byname = collections.defaultdict(list)
    for x in pl:
        if joined(x) and x["club"] >= 0:
            byname[(joined(x), x["nation"], x["club"])].append(x)
    for rows in byname.values():
        for i in range(len(rows)):
            for j in range(i + 1, len(rows)):
                w = same_club(rows[i], rows[j])
                if w:
                    pairs.setdefault(tuple(sorted((rows[i]["id"], rows[j]["id"]))), w)
    return pairs, st


def table_count(path, tid):
    return Folder(path).idx[tid]["count"]


def set_count(F, tid, count):
    ib = F.raw["index.dat"]
    struct.pack_into("<i", ib, F.idx[tid]["pos"] + 55, count)


def cut_rows(F, tid, n):
    """Removes the last n rows of a table and moves up the tables behind it in the same file."""
    e = F.idx[tid]
    size = F.table(6)[2] if tid == 6 else {22: 52, 17: 17}[tid]
    end = e["off"] + e["count"] * size
    b = F.raw[e["file"]]
    del b[end - n * size:end]
    set_count(F, tid, e["count"] - n)
    for t, o in F.idx.items():
        if t != tid and o["file"] == e["file"] and o["off"] >= end:
            struct.pack_into("<i", F.raw["index.dat"], o["pos"] + 59, o["off"] - n * size)
    F.reindex()


def kept_apart(a, b):
    """The reason two rows of a template pair are two people, or None (Q28, Summer 2002). The pair rules match
    people whom the template holds twice, and a few such pairs are two people:
    - both dates known and more than 2 days apart, and not one typing slip apart (one character of the ISO dates
      differs, two digits swap places, or day and month swap): Andy Evans of Frickley Athletic, 1975 and 1978;
    - common names that differ where one carries a number (Toni and Toni II, Rui I and Rui II, Pinto and Pinto II).
    Two other common names alone do not make two people: one person has Luís Carlos and Luiz Carlos, or Rodrigão
    and Rodrigo, on the same birthday. a and b are staff_view dicts."""
    def iso(x):
        d, y = x["dob"]
        m = max(k for k in range(12) if CUM[k] <= min(d, 364))
        return f"{y:04d}-{m + 1:02d}-{min(d, 364) - CUM[m] + 1:02d}"
    if known(a) and known(b) and abs(days(a) - days(b)) > 2:
        p, q = iso(a), iso(b)
        diff = [k for k in range(len(p)) if p[k] != q[k]]
        swap = len(diff) == 2 and diff[1] == diff[0] + 1 and p[diff[0]] == q[diff[1]] and p[diff[1]] == q[diff[0]]
        if not (len(diff) == 1 or swap or (p[:4] == q[:4] and p[5:7] == q[8:] and p[8:] == q[5:7])):
            return f"dates {p} and {q}"
    ca, cb = norm(a["common"]).split(), norm(b["common"]).split()
    numbered = lambda w: [v for v in w if v in NUMERALS]  # noqa: E731
    if (numbered(ca) or numbered(cb)) and ca != cb:
        return f"common names {a['common'] or '-'} and {b['common'] or '-'}"
    return None


NUMERALS = {"i", "ii", "iii", "iv", "v", "1", "2", "3", "4"}
CUM = [0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334]


def open_db(path):
    """cmdb.Folder for a format 2 folder, cmdb1.Folder1 for format 1 (the 3.9.68 layout of Summer 2002)."""
    return cmdb1.open_folder(path) if cmdb1.staff_format(path) == 1 else Folder(path)


def merge(db, template, conv, report):
    F = Folder(db)
    t_count, c_count = table_count(template, 6), table_count(conv, 6)
    n, _, _ = merge_folder(F, t_count, c_count, report)
    if not n:
        return 0
    for f, b in F.raw.items():
        with open(os.path.join(db, f), "wb") as h:
            h.write(b)
    G = Folder(db)
    check_squad_lists(G.rows(0), G.rows(6))
    if any(i32(r, 0) != n for n, r in enumerate(G.rows(6))):
        sys.exit("duplicates: a staff id is not its row number after the merge")
    return n


def merge_folder(F, t_count, c_count, report, apart=None):
    """The merge on the folder F in memory (format 2 cmdb.Folder or format 1 cmdb1.Folder1). Rows below t_count
    are the template's and rows from c_count on are key rows (format 1: both are the row count). apart(a, b)
    gives the reason why the rows of a pair are two people, or None: such a pair stays, and no merge joins two
    rows that it keeps apart. Returns (rows merged, {old id: new id} of every row, [(new id, new id, reason)] of
    the pairs kept apart)."""
    fmt1 = isinstance(F, cmdb1.Folder1)
    pairs, st = find(F)
    byid = {x["id"]: x for x in st}
    reasons = {k: apart(byid[k[0]], byid[k[1]]) for k in sorted(pairs)} if apart else {}
    kept = {k: w for k, w in reasons.items() if w}
    parent, members = {}, {}

    def root(a):
        while parent.get(a, a) != a:
            a = parent[a]
        return a
    for a, b in sorted(pairs):
        if (a, b) in kept:
            continue
        ra, rb = root(a), root(b)
        if ra == rb:
            continue
        ga, gb = members.get(ra, {ra}), members.get(rb, {rb})
        clash = next(((x, y) for x, y in kept if x in ga and y in gb or x in gb and y in ga), None)
        if clash:
            kept[(a, b)] = f"a merge would join {clash[0]} and {clash[1]}, which are two people"
            continue
        lo, hi = min(ra, rb), max(ra, rb)
        parent[hi] = lo
        members[lo] = ga | gb
        members.pop(hi, None)
    groups = collections.defaultdict(list)
    for a, b in pairs:
        if (a, b) in kept:
            continue
        for x in (a, b):
            groups[root(x)].append(x)

    def rank(i):
        src = 0 if i >= c_count else 1 if i < t_count else 2
        return (src, 0 if byid[i]["club"] >= 0 else 1, i)
    staff = F.rows(6)
    keep_of, lines, moved = {}, [], set()
    for ids in groups.values():
        ids = sorted(set(ids), key=rank)
        k = ids[0]
        # The club of 31 August: a fill or key row has FM's club; a template row has FM's club only when match.py
        # matched it, so a fill or key row at a club goes first, then any row at a club when the keeper has none.
        donor = next((i for i in ids[1:] if i >= t_count and byid[i]["club"] >= 0), None) if k < c_count else None
        if donor is None and byid[k]["club"] < 0:
            donor = next((i for i in ids[1:] if byid[i]["club"] >= 0), None)
        if donor is not None:
            a, z = PLACEMENT
            if i32(staff[k], 0x39) != i32(staff[donor], 0x39):
                moved.add(k)
            staff[k][a:z] = bytes(staff[donor][a:z])
        if i32(staff[k], 0x24) < 0:
            for i in ids[1:]:
                if i32(staff[i], 0x24) >= 0:
                    staff[k][0x24:0x29] = bytes(staff[i][0x24:0x29])
                    break
        for i in ids[1:]:
            keep_of[i] = k
        lines.append((k, ids[1:], donor))
    cur = {i: i for i in byid}                      # old id -> the id of its row now
    write_report(report, lines, byid)
    if not keep_of:
        return 0, cur, sorted((a, b, w) for (a, b), w in kept.items())
    club_of = {k: i32(staff[k], 0x39) for k in set(keep_of.values())}
    nat_of = {k: i32(staff[k], 0x24) for k in set(keep_of.values())}
    del staff                   # no row view may stay open: cut_rows shortens the file's buffer
    # Favourite people: format 2 keeps them in the preferences table (22), format 1 in the staff row (0x79).
    favourites = ((6, FAV_STAFF_1), (9, NP_STAFF)) if fmt1 else ((22, PREF_STAFF), (9, NP_STAFF))

    def redirect(ids_map, drop_from_other_clubs):
        """Club lists, favourites and awards: an id of ids_map becomes its new id."""
        for t in (0, 1):
            for r in F.rows(t):
                cid = i32(r, 0)
                for off, n in CLUB_STAFF:
                    cur_ids = [i32(r, off + 4 * j) for j in range(n)]
                    if not any(v in ids_map or (v in moved and drop_from_other_clubs) for v in cur_ids):
                        continue
                    out = []
                    for v in cur_ids:
                        if drop_from_other_clubs and t == 0 and v in moved and club_of.get(v) != cid:
                            continue        # a keeper who took the club of a dropped row leaves his old club
                        if v in ids_map:
                            nv = ids_map[v]
                            here = t == 1 and nat_of.get(nv) == cid or t == 0 and club_of.get(nv) == cid
                            if drop_from_other_clubs and not here:
                                continue
                            v = nv
                        if v < 0 or v not in out:
                            out.append(v)
                    if n > 1:
                        out = [v for v in out if v >= 0]
                    out += [-1] * (n - len(out))
                    for j in range(n):
                        struct.pack_into("<i", r, off + 4 * j, out[j])
        for tid, (off, n) in favourites:
            for r in F.rows(tid):
                for j in range(n):
                    v = i32(r, off + 4 * j)
                    if v in ids_map:
                        struct.pack_into("<i", r, off + 4 * j, ids_map[v])
        for r in F.rows(18):
            for o in AWARD_STAFF_IDS:
                v = i32(r, o)
                if v in ids_map:
                    struct.pack_into("<i", r, o, ids_map[v])

    redirect(keep_of, True)
    # Careers: the rows of one dropped row go to the keeper when he has none (the first dropped row that has some),
    # the others go.
    e = F.idx[17]
    hb = F.raw[e["file"]]
    hrow = [bytes(hb[e["off"] + n * 17:e["off"] + (n + 1) * 17]) for n in range(e["count"])]
    has = collections.Counter(i32(r, 4) for r in hrow)
    giver = {}
    for d in sorted(keep_of):
        if has[keep_of[d]] == 0 and has[d] and keep_of[d] not in giver:
            giver[keep_of[d]] = d
    rows = []
    for r in hrow:
        s = i32(r, 4)
        if s in keep_of:
            if giver.get(keep_of[s]) != s:
                continue
            r = r[:4] + struct.pack("<i", keep_of[s]) + r[8:]
        rows.append(bytearray(r))
    for n, r in enumerate(rows):
        struct.pack_into("<i", r, 0, n)
    removed_hist = e["count"] - len(rows)
    F.raw[e["file"]] = bytearray(bytes(hb[:e["off"]]) + b"".join(rows) + bytes(hb[e["off"] + e["count"] * 17:]))
    set_count(F, 17, len(rows))
    for t, o in F.idx.items():
        if t != 17 and o["file"] == e["file"] and o["off"] > e["off"]:
            struct.pack_into("<i", F.raw["index.dat"], o["pos"] + 59, o["off"] - removed_hist * 17)
    F.reindex()
    for d, k in keep_of.items():
        cur[d] = k
    # The rows: the last staff row (and its preferences row) moves into each dropped place, highest place first.
    if fmt1:
        move_rows_1(F, keep_of, cur, redirect)
    else:
        move_rows_2(F, keep_of, cur, club_of, nat_of, redirect)
    staff_count = F.idx[6]["count"]
    if any(c >= staff_count for c in cur.values()):
        sys.exit("duplicates: a row id points past the staff table after the merge")
    kept_now = sorted((min(cur[a], cur[b]), max(cur[a], cur[b]), w) for (a, b), w in kept.items())
    print(f"duplicates: {len(groups)} people with more than one row, {len(keep_of)} rows merged, "
          f"{removed_hist} career rows of merged rows removed, {len(kept)} pairs kept apart")
    return len(keep_of), cur, kept_now


def move_rows_2(F, keep_of, cur, club_of, nat_of, redirect):
    """Format 2: the last staff row (and its preferences row) moves into each dropped place, highest place first."""
    se, pe = F.idx[6], F.idx[22]
    sb, pb = F.raw[se["file"]], F.raw[pe["file"]]
    back = {}                                   # place -> the old id of the row that moved there
    for x in sorted(keep_of, reverse=True):
        se, pe = F.idx[6], F.idx[22]
        last = se["count"] - 1
        so, po = se["off"], pe["off"]
        if x != last:
            sb[so + x * 110:so + (x + 1) * 110] = sb[so + last * 110:so + (last + 1) * 110]
            struct.pack_into("<i", sb, so + x * 110, x)
            if i32(sb, so + x * 110 + 0x65) == last:
                pb[po + x * 52:po + (x + 1) * 52] = pb[po + last * 52:po + (last + 1) * 52]
                struct.pack_into("<i", pb, po + x * 52, x)
                struct.pack_into("<i", sb, so + x * 110 + 0x65, x)
            if last in club_of:
                club_of[x], nat_of[x] = club_of.pop(last), nat_of.pop(last)
            for d, k in keep_of.items():
                if k == last:
                    keep_of[d] = x
            redirect({last: x}, False)
            h = F.idx[17]
            for n in range(h["count"]):
                o = h["off"] + n * 17 + 4
                if i32(F.raw[h["file"]], o) == last:
                    struct.pack_into("<i", F.raw[h["file"]], o, x)
            back[x] = back.pop(last, last)
        else:
            back.pop(last, None)
        if pe["count"] == se["count"]:
            cut_rows(F, 22, 1)
        cut_rows(F, 6, 1)
    fin = {old: place for place, old in back.items()}
    for i, c in cur.items():
        cur[i] = fin.get(c, c)


def move_rows_1(F, keep_of, cur, redirect):
    """Format 1: the same moves as move_rows_2 (the last row into each dropped place, highest place first), worked
    out first and then made in one pass: one redirect of every reference and one cut of the table. Format 1 has
    no preferences rows to move (the favourites are in the staff row)."""
    se = F.idx[6]
    size = F.table(6)[2]
    n = se["count"]
    at = list(range(n))                         # at[place] = the row that is there now
    for x in sorted(keep_of, reverse=True):
        last = n - 1
        if x != last:
            at[x] = at[last]
        at.pop()
        n -= 1
    sb, so = F.raw[se["file"]], se["off"]
    old = bytes(sb[so:so + se["count"] * size])
    move = {}
    for place, row in enumerate(at):
        if place != row:
            sb[so + place * size:so + (place + 1) * size] = old[row * size:(row + 1) * size]
            struct.pack_into("<i", sb, so + place * size, place)
            move[row] = place
    if move:
        redirect(move, False)
        h = F.idx[17]
        hb = F.raw[h["file"]]
        for k in range(h["count"]):
            o = h["off"] + k * 17 + 4
            v = i32(hb, o)
            if v in move:
                struct.pack_into("<i", hb, o, move[v])
    for i, c in cur.items():
        cur[i] = move.get(c, c)
    cut_rows(F, 6, se["count"] - n)


def show(x):
    return f"{x['id']} {x['first']} | {x['second']} | {x['common']} {x['dob']} club {x['club']}"


def write_report(report, lines, byid):
    with open(report, "w", encoding="utf-8") as h:
        h.write("kept\tmerged into it\tclub taken from\n")
        for k, ids, donor in sorted(lines):
            h.write(f"{show(byid[k])}\t{'; '.join(show(byid[i]) for i in ids)}\t{donor if donor is not None else ''}\n")


def check(db, allowed=None):
    ok = set()
    if allowed and os.path.exists(allowed):
        for line in open(allowed, encoding="utf-8"):
            p = line.split("\t")
            if len(p) >= 2 and p[0].strip().isdigit():
                ok.add(tuple(sorted((int(p[0]), int(p[1])))))
    F = open_db(db)
    check_squad_lists(F.rows(0), F.rows(6))
    pairs, st = find(F)
    byid = {x["id"]: x for x in st}
    left = {k: w for k, w in pairs.items() if k not in ok}
    stale = sorted(ok - set(pairs))
    for (a, b), w in sorted(pairs.items()):
        print(f"{'allowed' if (a, b) in ok else 'DUPLICATE'}\t{w}\t{show(byid[a])}\t{show(byid[b])}")
    for a, b in stale:
        print(f"STALE\tthe allowed list names {a} and {b}, which are no pair")
    print(f"duplicate check: {len(pairs)} pairs, {len(pairs) - len(left)} allowed, {len(left)} left"
          + (f", {len(stale)} allowed pairs not found" if stale else ""))
    return 1 if left or stale else 0


if __name__ == "__main__":
    a = sys.argv[1:]
    if a and a[0] == "merge" and len(a) == 5:
        merge(a[1], a[2], a[3], a[4])
    elif a and a[0] == "check" and len(a) in (2, 3):
        sys.exit(check(a[1], a[2] if len(a) == 3 else None))
    else:
        sys.exit(__doc__)
