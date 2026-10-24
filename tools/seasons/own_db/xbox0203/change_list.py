"""Write the Summer 2002 change list: what the Xbox facts change in the 3.9.68 template, for Max to read first.

Usage: python3 -I change_list.py <facts-xbox> <facts-3968> <xbox data> <3.9.68 Data> <out folder> [<rules folder>]

The facts folders come from read_xbox0203.py. The script reads the data folders again for the club divisions
(club.dat +0x57) and the competition names (club_comp.dat). The facts do not hold them. The rules folder (default:
docs/research/rules-0.6.0 of the repo) gives the 2002/03 rows of the *-clubs.tsv files. The data is untrusted
input. Nothing from it runs. The out folder must be outside the repo. Commit no Xbox data and nothing made from it.

Rules (spec docs/specs/2026-10-24-summer-2002-db-spec.md, Decisions, with Max's answers Q21 to Q28 of
2026-10-24). A row is "applied" (the build takes it), "kept" (an answer keeps the 3.9.68 value) or "held" (no
answer takes it, the 3.9.68 value stays).
- Match: a person matches when his normalised first name, surname and date of birth (cmdb.norm) give exactly one
  row in each set. The date must be known (year above 1900) and the nation must be the same. Q26: a person with
  an unknown date of birth whose name is once in each set with the same nation is a clear match too. Every other
  person is unclear and keeps his 3.9.68 record (kind 7). People only on Xbox are not added.
- Clubs: the club ids of the two sets differ, because rows are added, removed or sorted in another place. Clubs
  map by name and nation (map_clubs). Clubs of one name in several rows, and a club in one set only, map by squad.
  The squad map uses the dated matches only.
- Kind 1 moves and kind 3 dates: a clear match takes the Xbox club, join date and contract end (Q3). Q21: a move
  with no Xbox contract end takes the Xbox club with no contract end (a date row of that kind is held). Q22: a row
  whose job differs takes the Xbox job too (take_job), but a job that needs a player row where the 3.9.68 row has
  none (or a non-player row) is held, as the build adds no rows.
- Kind 2 releases: a club in 3.9.68 and none on Xbox. The release applies only on a clear match (Q4, Q10, Q14).
  It also needs the club's squad in the Xbox data (release_guard), as the Xbox copy drops the people of many
  small clubs. A squad counts when the Xbox data has at least half as many people at the club as 3.9.68. Q23:
  without the squad, or without the club, the 3.9.68 club stays.
- Full lists (slot_holds): a person who would join a club list of his job (squad, manager, coaches, chairman and
  so on) that is full in 3.9.68, and whose holders stay, is held. The game needs each person in the list of his
  job (convert.check_squad_lists). Q40: four holders that the Xbox data lacks, and that a public source puts
  elsewhere in July 2002, leave their place (HOLDERS_Q40, kind 2 rows with no Xbox row), so the Xbox manager
  takes it: Mutti at Reggina, Acebal at Sporting Gijón, Csábi at Vasas and Paneira at Serzedelo.
- Kind 4 ratings: the rating rows that differ for clear matches. Major player: current ability 140 or more, or
  world reputation 150 or more, in either set (Q8). World reputation is the i16 at +0x0d of the player row. The
  summary gives the proof and its limit. Q25: the rating changes of all clear matches apply, major or not.
- Kind 5 leagues: every club whose division differs between the 3.9.68 club.dat, the Xbox club.dat and the
  2002/03 club-table row, and every club-table row with no club id (Q15). Q24 (league_decision): the Xbox league
  where the Xbox data and the club table agree, the 3.9.68 league where the club table agrees with 3.9.68. The
  rest go to leagues-second-look.tsv, sorted by nation, and keep the 3.9.68 league in this build.
  With --league-check <all-results.tsv> (the check of every league row with two public sources) Max's answers
  Q33 to Q37 settle the rows (league_check): each "change" row takes the checked game division, rows that the
  sources confirm are kept, and Q36 and Q37 name the rest by club id. division_new holds the division taken.
- Kind 6 names and competitions: club names and nations that differ, clubs and competitions in one set only, and
  the clubs of the Xbox Turkish Category B groups (Q11). The Fiorentina name (Q5) applies. The three groups are
  held: 3.9.68 has two free Turkish competition rows, not three (build_2002.py, step 6). The group of each club is
  a league decision, so it is held in kind 6 and in kind 5. Q27: a club with no nation on
  the Xbox keeps its 3.9.68 nation. Every other row is held.
- Duplicates (Q14): no change adds a row. duplicates.find (people.py rules and the same-club rule) runs on the
  3.9.68 people before and after the applied club changes. A change that makes a new pair is held, and so is a
  change of a row in a template pair, as both rows of one person must move together. This repeats until no new
  pair is left. The template pairs go to 8-template-pairs.tsv. Q28: the build merges each pair by the 2006/07
  rules, except a pair that duplicates.kept_apart calls two people ("kept").

Output: one TSV per kind (1-moves.tsv to 8-template-pairs.tsv), leagues-second-look.tsv and summary.md, with
counts and the rows still held.
"""
import collections
import csv
import datetime
import os
import pickle
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OWN_DB = os.path.dirname(HERE)
REPO = os.path.realpath(os.path.dirname(os.path.dirname(os.path.dirname(OWN_DB))))
sys.path.insert(0, OWN_DB)
import cmdb  # noqa: E402
import cmdb1  # noqa: E402
import duplicates  # noqa: E402
from cmdb1 import need, u  # noqa: E402
from convert import JOB_LISTS, PLAYER_JOBS  # noqa: E402

RULES = os.path.join(REPO, "docs", "research", "rules-0.6.0")
SEASON = "2002/03"
MAJOR_CA, MAJOR_WORLD = 140, 150
HOME, CURRENT, WORLD = 0, 1, 2              # index in the ratings "rep" tuple (+0x09, +0x0b, +0x0d)
SUMMER = (datetime.date(2002, 5, 1), datetime.date(2002, 9, 30))
MIN_SQUAD_MAP = 3                           # clear matches that map a club in one set only by its squad
PLACEHOLDER_JOIN = datetime.date(1996, 1, 2)    # a join date that both sets give to many people
EXAMPLES = ("Fabio Celestini", "Vitaly Kutuzov", "Stephen Lovell")    # named in the research doc
TURKISH_GROUPS = "Turkish 2. Division Category B"
APPLIED, HELD, KEPT = "applied", "held", "kept"
WIPED = "the Xbox data has no contract end, 3.9.68 has one"
JOB_Q22 = "Q22: the Xbox job too"
GROUPS_WAIT = ("Q11: three Turkish Category B groups (slice 5 code). Held: 3.9.68 has only two free Turkish "
               "competition rows, and the build adds no rows (build_2002.py, step 6)")
NO_ROW = ("Q22 takes the Xbox job, but the 3.9.68 row has no player or non-player row for it and slice 3 adds "
          "no rows")


# ---- people

def person_key(p):
    """(first name, surname, date of birth) for matching, or None when the date is unknown."""
    if p["dob"] is None:
        return None
    return (cmdb.norm(p["fn"]), cmdb.norm(p["sn"]), p["dob"])


def name_key(p):
    return (cmdb.norm(p["fn"]), cmdb.norm(p["sn"]))


def match_people(T, X, q26=False):
    """Clear matches and unclear people. T and X are people_all lists (3.9.68, Xbox).

    Returns (pairs, unclear): pairs {3.9.68 index: Xbox index}; unclear a list of (reason, [3.9.68 indexes],
    [Xbox indexes]) for each Xbox person that has no clear match and each name and date that is in more than one
    row. A 3.9.68 person with no Xbox row is not unclear: the Xbox copy lacks him and he keeps his record (Q2).
    q26: a person with an unknown date of birth whose name is once in each set with the same nation is a clear
    match (Q26), so he is in pairs and not in unclear."""
    bt, bx = collections.defaultdict(list), collections.defaultdict(list)
    for i, p in enumerate(T):
        k = person_key(p)
        if k:
            bt[k].append(i)
    for i, p in enumerate(X):
        k = person_key(p)
        if k:
            bx[k].append(i)
    names_t = collections.defaultdict(list)
    for i, p in enumerate(T):
        names_t[name_key(p)].append(i)
    pairs, unclear = {}, []
    for k, xs in bx.items():
        ts = bt.get(k, [])
        if len(xs) == 1 and len(ts) == 1:
            if T[ts[0]]["nat"] == X[xs[0]]["nat"]:
                pairs[ts[0]] = xs[0]
            else:
                unclear.append(("nation differs", ts, xs))
        elif not ts:
            same_name = names_t.get(name_key(X[xs[0]]), [])
            unclear.append(("only on Xbox" if not same_name else "only on Xbox, the name is in 3.9.68 with "
                            "another date", same_name, xs))
        else:
            unclear.append(("shared name and date", ts, xs))
    for k, ts in bt.items():
        if len(ts) > 1 and k not in bx:
            unclear.append(("shared name and date, 3.9.68 only", ts, []))
    names_x = collections.Counter(name_key(p) for p in X)
    for i, p in enumerate(X):
        if p["dob"] is None:
            ts = names_t.get(name_key(p), [])
            if not ts:
                why = "unknown date of birth, only on Xbox"
            elif len(ts) == 1 and names_x[name_key(p)] == 1 and T[ts[0]]["nat"] == p["nat"]:
                if q26 and ts[0] not in pairs:
                    pairs[ts[0]] = i
                    continue
                why = "unknown date of birth, the name is once in each set with the same nation"
            else:
                why = "unknown date of birth, the name is in several rows"
            unclear.append((why, ts, [i]))
    return pairs, unclear


# ---- clubs

def map_clubs(TC, XC, pairs=None, TP=None, XP=None):
    """{Xbox club id: 3.9.68 club id}, the squad maps [(Xbox id, 3.9.68 id, people from it, people)], the
    3.9.68 clubs left over and the Xbox clubs left over.

    Steps: the same long name, short name and nation, then the same long name and nation, then the same long name.
    When one key is in several rows of each set, the rows pair by squad. The pair with the most clear matches goes
    first, and the rest pair in id order (both club tables are in name order). A club left over maps by squad when
    most of its clear matches (at least MIN_SQUAD_MAP and more than half) come from one club that is also left
    over."""
    came = collections.defaultdict(collections.Counter)
    for t, x in (pairs or {}).items():
        if XP[x]["club"] is not None and TP[t]["club"] is not None:
            came[XP[x]["club"]][TP[t]["club"]] += 1
    out, left_t, left_x = {}, set(TC), set(XC)
    for key in (lambda c: (c["long"], c["short"], c["nat"]), lambda c: (c["long"], c["nat"]), lambda c: c["long"]):
        bt, bx = collections.defaultdict(list), collections.defaultdict(list)
        for i in sorted(left_t):
            bt[key(TC[i])].append(i)
        for i in sorted(left_x):
            bx[key(XC[i])].append(i)
        for k, xs in bx.items():
            ts = bt.get(k, [])
            if len(ts) != len(xs):
                continue
            by_squad = sorted(((-came[x][t], x, t) for x in xs for t in ts if came[x][t]))
            for _, x, t in by_squad:
                if x in left_x and t in left_t:
                    out[x] = t
                    left_t.discard(t)
                    left_x.discard(x)
            for x, t in zip([x for x in xs if x in left_x], [t for t in ts if t in left_t]):
                out[x] = t
                left_t.discard(t)
                left_x.discard(x)
    squad = []
    if pairs and left_x:
        for xc in sorted(left_x):
            if not came[xc]:
                continue
            tc, n = came[xc].most_common(1)[0]
            total = sum(came[xc].values())
            if tc in left_t and n >= MIN_SQUAD_MAP and 2 * n > total:
                out[xc] = tc
                left_t.discard(tc)
                squad.append((xc, tc, n, total))
        left_x -= set(out)
    return out, squad, sorted(left_t), sorted(left_x)


def club_name(C, c):
    return C[c]["long"] if c is not None and c in C else "-"


# ---- releases and the duplicate guard

def release_guard(t, x, pairs, T, xsquads, tsquads, cmap_t):
    """(applied?, reason) for a 3.9.68 person at a club whom the Xbox data has with no club. xsquads and tsquads
    count the people at each club of the Xbox and the 3.9.68 data."""
    if pairs.get(t) != x:
        return False, "no clear match"
    tc = T[t]["club"]
    xc = cmap_t.get(tc)
    if xc is None:
        return False, "the club is not in the Xbox data"
    if 2 * xsquads.get(xc, 0) < tsquads.get(tc, 0):
        return False, (f"the Xbox data keeps no squad at that club ({xsquads.get(xc, 0)} people there, "
                       f"{tsquads.get(tc, 0)} in 3.9.68)")
    return True, "clear match, the Xbox data keeps the squad of that club"


def staff_rows(P, clubs_after=None):
    """People in the shape of cmdb.staff_view, for duplicates.find. clubs_after {index: club id} changes clubs."""
    out = []
    for i, p in enumerate(P):
        d = p["dob"]
        dob = (d.timetuple().tm_yday - 1 - (d.month > 2 and d.year % 4 == 0), d.year) if d else (0, 1900)
        club = p["club"] if p["club"] is not None else -1
        if clubs_after and i in clubs_after:
            club = clubs_after[i] if clubs_after[i] is not None else -1
        player = p["role"] == "player"
        out.append(dict(id=i, first=p["fn"], second=p["sn"], common=p["cn"] or "", dob=dob, nation=p["nat"],
                        club=club, kind=2 if player else 1, np=0 if player else -1))
    return out


def pair_text(t, pairs):
    return ", ".join(f"{a} and {b} ({v})" for (a, b), v in sorted(pairs.items()) if t in (a, b))


def duplicate_pairs(P, clubs_after=None):
    rows = staff_rows(P, clubs_after)
    saved = duplicates.staff_view
    duplicates.staff_view = lambda F: (rows, {})
    try:
        pairs, _ = duplicates.find(None)
    finally:
        duplicates.staff_view = saved
    return pairs


# ---- raw data: divisions and competitions

def read_divisions(path):
    """({club id: division id at +0x57}, {competition id: name}) of a format 1 data folder."""
    F = cmdb1.open_folder(path)
    comps = {}
    for r in F.rows(12):
        cid = u("<i", r, 0)[0]
        need(cid not in comps, f"{path}: competition id {cid} is in club_comp.dat twice")
        comps[cid] = cmdb.cstr(r, 4, 51)
    div = {}
    for tid in (0, 1):
        for r in F.rows(tid):
            cid, d = u("<i", r, 0)[0], u("<i", r, 0x57)[0]
            need(d < 0 or d in comps, f"{path}: club {cid} has the unknown division {d}")
            div[cid] = d
    return div, comps


# The division labels of the 2002/03 club-table rows that are not game division names, and the game division of
# each. A label "<game division> (real: ...)" is that game division. "?" marks a label with no game division.
REAL_DIVISIONS = {
    "Major League Soccer": "American Major League", "Primera División": "Argentine Premier Division",
    "Primera B Nacional": "Argentine Second Division", "K League": "Korean League",
    "Série A (2003)": "Brazilian National First Division", "Série B (2003)": "Brazilian National Second Division",
    "Série C (2003)": "Brazilian National Third Division", "Ligue 1": "French First Division",
    "Ligue 2": "French Second Division", "Championnat National": "French National", "CFA": "French CFA",
    "CFA 2": "French Lower Division", "Regionalliga Süd": "German Regional Division South",
    "cro_a1": "Croatian First Division", "cro_a2a": "Croatian Second Division North",
    "cro_a2b": "Croatian Second Division South", "pol_second": "Polish Second Division",
    "Veikkausliiga": "Finnish Premier Division", "Ykkönen": "Finnish First Division",
    "Segunda Divisão B Zona Centro": "Portuguese Second Division B Central",
    "Segunda Divisão B Zona Norte": "Portuguese Second Division B North",
    "Segunda Divisão B Zona Sul": "Portuguese Second Division B South",
}


def real_division(label):
    """The game division name of a club-table label, or "?"."""
    if label in REAL_DIVISIONS:
        return REAL_DIVISIONS[label]
    if " (real: " in label:
        game, real = label.split(" (real: ", 1)
        group = real.rstrip(")").rsplit(", Group ", 1)
        return game + " Group " + group[1] if len(group) == 2 else game
    return label


def league_decision(a, b, vs, real, comp_ids):
    """(decision, rule, 3.9.68 competition id or -1) of a league row by Q24. a and b are the 3.9.68 and Xbox
    division names, vs the Xbox division against the 2002/03 club table, real the table's game divisions."""
    if vs == "agrees" and a != b:
        if b in comp_ids:
            return APPLIED, "Q24: the Xbox data and the club table agree, the Xbox league", comp_ids[b]
        return HELD, ("Q24 second look: the Xbox league agrees with the club table, but 3.9.68 has no "
                      "competition of that name (the Turkish groups wait for the group rows)"), -1
    if a in real:
        return KEPT, "Q24: the club table agrees with 3.9.68, the 3.9.68 league stays", -1
    return HELD, "Q24 second look: no rule settles the row, the 3.9.68 league stays in this build", -1


def club_table_rows(rules):
    out = []
    for f in sorted(os.listdir(rules)):
        if not f.endswith("-clubs.tsv"):
            continue
        with open(os.path.join(rules, f), newline="", encoding="utf-8") as h:
            for r in csv.DictReader(h, delimiter="\t", quoting=csv.QUOTE_NONE):
                if r["season"] == SEASON:
                    out.append(dict(r, part=f[:-len("-clubs.tsv")]))
    return out


# ---- the change list

def load(folder):
    out = {}
    for n in ("people_all", "clubs", "ratings", "nations"):
        with open(os.path.join(folder, n + ".pkl"), "rb") as f:
            out[n] = pickle.load(f)
    return out


def d(x):
    return x.isoformat() if x else "-"


def who(p):
    return f"{p['fn']} {p['sn']}" + (f" ({p['cn']})" if p["cn"] else "")


def major(rt, rx):
    return any(r and (r["ca"] >= MAJOR_CA or r["rep"][WORLD] >= MAJOR_WORLD) for r in (rt, rx))


def rating_diffs(rt, rx):
    out = []
    for f in ("ca", "pa"):
        if rt[f] != rx[f]:
            out.append((f, rt[f], rx[f]))
    for j, f in enumerate(("home reputation", "current reputation", "world reputation")):
        if rt["rep"][j] != rx["rep"][j]:
            out.append((f, rt["rep"][j], rx["rep"][j]))
    for grp in ("pos", "attrs"):
        for f in rt[grp]:
            if rt[grp][f] != rx[grp].get(f):
                out.append((f, rt[grp][f], rx[grp].get(f)))
    return out


# The club lists of club.dat that hold staff by job (convert.JOB_LISTS, plus chairman and directors): key, offset
# and size. "p" is the squad list (convert.in_squad).
HOLDERS_Q40 = {
    106378: ("Luigi De Canio took over Reggina on 8 November 2002, after Bortolo Mutti, the manager from summer 2002 "
             "(Gli Eroi del Calcio)"),
    143366: ("Antonio Maceda took over Sporting Gijón after José María Acebal was sacked on 7 October 2002 "
             "(Wikipedia)"),
    38259: "Manu Ferrera was not at Vasas: József Csábi was its manager from 8 July 2002 (Nemzeti Sport)",
    5905: "Vítor Paneira managed Serzedelo in 2002/03 (Wikipedia, no month given)",
}
SLOTS = {"p": (0xd7, 50), "m": (0xcf, 1), "a": (0xd3, 1), "c": (0x19f, 5), "s": (0x1b3, 7), "ph": (0x1cf, 3),
         "chair": (0xbf, 1), "dir": (0xc3, 3)}
OTHER_JOBS = {1: "chair", 2: "dir", 3: "dir", 4: "dir"}
FULL = ("the club's list for that job is full in 3.9.68 and its holders stay: the newcomers of the highest ability "
        "take the free places")


def slot_keys(flags, job):
    """The SLOTS keys of a staff row with these flags (+0x5f) and job byte (+0x3d)."""
    out = ["p"] if flags == 2 or (flags == 3 and job in PLAYER_JOBS) else []
    if flags in (1, 3):
        k = JOB_LISTS.get(job, OTHER_JOBS.get(job))
        if k:
            out.append(k)
    return out


def slot_holds(planned, raw):
    """Holds the applied rows whose person would join a full club list. raw = (3.9.68 folder, Xbox folder),
    cmdb1.Folder1. A holder of a list who leaves it (another club or job) frees his place first. A held row keeps
    the 3.9.68 club, dates and job, which can fill a place again, so the check repeats until nothing changes.
    When newcomers outnumber the free places, those of the highest 3.9.68 ability (current ability of a player,
    the non-player ability +0x04 of the others) take them: Celestini goes to Olympique Marseille, whose 3.9.68
    squad keeps three players that the Xbox data lacks (Q2).
    Returns the planned rows and the number held."""
    T, X = raw
    st, xs, tp, tn = T.rows(6), X.rows(6), T.rows(10), T.rows(9)

    def ability(t):
        """Current ability of a player (player row +0x05), else the ability of a non-player (+0x04)."""
        r = st[t]
        if r[0x5f] & 2 and 0 <= u("<i", r, 0x91)[0] < len(tp):
            return u("<h", tp[u("<i", r, 0x91)[0]], 5)[0]
        n = u("<i", r, 0x99)[0]
        return u("<h", tn[n], 4)[0] if r[0x5f] & 1 and 0 <= n < len(tn) else 0
    lists = {}
    for r in T.rows(0):
        c = u("<i", r, 0)[0]
        for k, (o, n) in SLOTS.items():
            lists[(c, k)] = [v for v in u(f"<{n}i", r, o) if v >= 0]
    held = 0
    while True:
        final = {}                                  # staff id -> (club, job) after the applied rows
        for kind, t, x, new, dec, why in planned:
            if dec == APPLIED:
                job = xs[x][0x3d] if why.startswith(JOB_Q22) else st[t][0x3d]
                final[t] = (new if new is not None else -1, job)
        place = collections.defaultdict(list)
        for (c, k), ids in lists.items():
            for v in ids:
                fc, fj = final.get(v, (u("<i", st[v], 0x39)[0], st[v][0x3d]))
                if fc == c and k in slot_keys(st[v][0x5f], fj):
                    place[(c, k)].append(v)          # a holder who stays
        come = collections.defaultdict(list)
        for t, (c, j) in sorted(final.items()):
            for k in slot_keys(st[t][0x5f], j):
                if c >= 0 and t not in place[(c, k)]:
                    come[(c, k)].append(t)
        over = set()
        for key, ts in come.items():
            room = SLOTS[key[1]][1] - len(place[key])
            ts = sorted(ts, key=lambda t: (-ability(t), t))
            over |= set(ts[max(room, 0):])
        if not over:
            return planned, held
        held += len(over)
        planned = [(kind, t, x, new, HELD, FULL) if dec == APPLIED and t in over else (kind, t, x, new, dec, why)
                   for kind, t, x, new, dec, why in planned]


def build(ft, fx, div_t, comps_t, div_x, comps_x, table_rows, raw=None, holders=None):
    """All rows of the change list: {kind file: [row dict]}, and the notes for summary.md."""
    T, X = ft["people_all"], fx["people_all"]
    TC, XC = ft["clubs"], fx["clubs"]
    strict, _ = match_people(T, X)
    pairs, unclear = match_people(T, X, q26=True)
    q26 = set(pairs) - set(strict)
    cmap, squad_maps, only_t, only_x = map_clubs(TC, XC, strict, T, X)      # the club map uses dated matches
    cmap_t = {t: x for x, t in cmap.items()}
    xsquads = collections.Counter(p["club"] for p in X if p["club"] is not None)
    tsquads = collections.Counter(p["club"] for p in T if p["club"] is not None)
    rows = collections.defaultdict(list)
    notes = dict(pairs=len(pairs), q26=len(q26), squad_maps=squad_maps, only_t=only_t, only_x=only_x,
                 placeholder=sum(1 for x in pairs.values() if X[x]["joined"] == PLACEHOLDER_JOIN))

    # Kinds 1 to 3: club, join date and contract end of clear matches, with Max's answers Q21 to Q23 and Q26.
    planned = []
    for t, x in sorted(pairs.items()):
        pt, px = T[t], X[x]
        new = cmap.get(px["club"]) if px["club"] is not None else None
        tag = "Q26 clear match" if t in q26 else "clear match"
        if px["club"] is not None and new is None:
            planned.append(("1-moves", t, x, None, HELD, "the Xbox club is not in 3.9.68"))
            continue
        job_differs = (pt["role"], pt["job"]) != (px["role"], px["job"])
        job_note = f"the job differs (3.9.68 {pt['role']} {pt['job']}, Xbox {px['role']} {px['job']}, FM codes)"
        kind = "1-moves" if new != pt["club"] and new is not None else "2-releases" if new != pt["club"] else \
            "3-dates"
        if job_differs and pt["role"] != px["role"]:
            planned.append((kind, t, x, new, HELD, job_note + ": " + NO_ROW))
            continue
        q22 = JOB_Q22 + ", " if job_differs else ""
        if kind == "2-releases":
            ok, why = release_guard(t, x, pairs, T, xsquads, tsquads, cmap_t)
            planned.append((kind, t, x, None, APPLIED if ok else HELD, q22 + (why if ok else "Q23: " + why +
                                                                                ", the 3.9.68 club stays")))
        elif kind == "1-moves":
            if pt["cend"] and not px["cend"]:
                planned.append((kind, t, x, new, APPLIED, q22 + "Q21: " + WIPED + ", the Xbox club with no "
                                "contract end"))
            else:
                planned.append((kind, t, x, new, APPLIED, q22 + tag + ": Xbox club, join date, contract end"))
        elif (pt["joined"], pt["cend"]) != (px["joined"], px["cend"]):
            if pt["cend"] and not px["cend"]:
                planned.append((kind, t, x, new, HELD, q22 + WIPED + ", same club: no answer takes it (Q21 is "
                                "for moves)"))
            else:
                planned.append((kind, t, x, new, APPLIED, q22 + tag + ", same club: Xbox join date, contract end"))
        elif job_differs:
            planned.append((kind, t, x, new, APPLIED, q22 + "same club and dates"))
    before = duplicate_pairs(T)
    in_pair = {i for k in before for i in k}
    planned = [(kind, t, x, new, HELD, "the 3.9.68 row is one of a template duplicate pair: " + pair_text(t, before))
               if dec == APPLIED and t in in_pair else (kind, t, x, new, dec, why)
               for kind, t, x, new, dec, why in planned]
    notes["dup_before"], notes["dup_rounds"] = before, []
    while True:
        after = {t: new for kind, t, x, new, dec, why in planned if dec == APPLIED and kind != "3-dates"}
        new_pairs = {k: v for k, v in duplicate_pairs(T, after).items() if k not in before}
        if not new_pairs:
            break
        notes["dup_rounds"].append(len(new_pairs))
        in_new = {i for k in new_pairs for i in k}
        planned = [(kind, t, x, new, HELD, "the move makes a new duplicate pair: " + pair_text(t, new_pairs))
                   if dec == APPLIED and kind != "3-dates" and t in in_new else (kind, t, x, new, dec, why)
                   for kind, t, x, new, dec, why in planned]
    # Q40 (Max, 2026-10-24): holders that the Xbox data lacks and that a public source puts elsewhere in July 2002
    # leave their place, so the Xbox newcomer can take it. They have no Xbox row (x is None) and become free.
    listed = {t for kind, t, x, new, dec, why in planned}
    for t, why in sorted((holders or {}).items()):
        need(T[t]["club"] is not None and T[t]["club"] >= 0, f"Q40: staff {t} has no club in 3.9.68")
        need(t not in listed, f"Q40: staff {t} already has a row of the change list")
        planned.append(("2-releases", t, None, None, APPLIED, "Q40 (Max): " + why))
    if raw:
        planned, notes["full_lists"] = slot_holds(planned, raw)
    for kind, t, x, new, dec, why in planned:
        pt = T[t]
        px = X[x] if x is not None else dict(pt, club=None, role="-", job="-")    # Q40: no Xbox row, dates stay
        jx = px["joined"]
        summer = bool(kind == "1-moves" and jx and SUMMER[0] <= jx <= SUMMER[1])
        rows[kind].append(dict(person=who(pt), born=d(pt["dob"]), id_3968=t, id_xbox=x if x is not None else -1,
                               club_3968=club_name(TC, pt["club"]), club_xbox=club_name(XC, px["club"]),
                               new_club_id=new if new is not None else -1,
                               joined_3968=d(pt["joined"]), joined_xbox=d(jx), cend_3968=d(pt["cend"]),
                               cend_xbox=d(px["cend"]), job_3968=f"{pt['role']} {pt['job']}",
                               job_xbox=f"{px['role']} {px['job']}", summer_2002="yes" if summer else "",
                               take_job="yes" if dec == APPLIED and why.startswith(JOB_Q22) else "",
                               rule=why, decision=dec))
    touched = {t for kind, t, x, new, dec, why in planned}
    srows = staff_rows(T)
    for (a, b), why in sorted(before.items()):
        two = duplicates.kept_apart(srows[a], srows[b])
        rows["8-template-pairs"].append(dict(
            id_a=a, person_a=who(T[a]), born_a=d(T[a]["dob"]), club_a=club_name(TC, T[a]["club"]), id_b=b,
            person_b=who(T[b]), born_b=d(T[b]["dob"]), club_b=club_name(TC, T[b]["club"]), found=why,
            xbox_change="yes" if a in touched or b in touched else "",
            rule=("Q28: two people (" + two + "), kept apart on kept-pairs.tsv") if two else
            "Q28: one person, the build merges the rows (duplicates.merge_folder)",
            decision=KEPT if two else APPLIED))

    # Kind 4: ratings.
    RT, RX = ft["ratings"], fx["ratings"]
    for t, x in sorted(pairs.items()):
        if t in RT and x in RX:
            diffs = rating_diffs(RT[t], RX[x])
            if diffs:
                m = major(RT[t], RX[x])
                rows["4-ratings"].append(dict(
                    person=who(T[t]), born=d(T[t]["dob"]), id_3968=t, id_xbox=x, club_3968=club_name(TC, T[t]["club"]),
                    major="yes" if m else "no", ca_3968=RT[t]["ca"], ca_xbox=RX[x]["ca"], pa_3968=RT[t]["pa"],
                    pa_xbox=RX[x]["pa"], world_3968=RT[t]["rep"][WORLD], world_xbox=RX[x]["rep"][WORLD],
                    fields=len(diffs), changes="; ".join(f"{f} {a}>{b}" for f, a, b in diffs),
                    rule=("the 3.9.68 row is one of a template duplicate pair: " + pair_text(t, before)) if t in in_pair
                    else ("Q25" + (", Q26 clear match" if t in q26 else "") + ": the Xbox rating changes apply"
                          + ("" if m else " (not a major player)")),
                    decision=HELD if t in in_pair else APPLIED))
    order = [r["rep"][HOME] >= r["rep"][CURRENT] >= r["rep"][WORLD] for r in RT.values()]
    local = [r for r in RT.values() if r["rep"][HOME] >= 50]
    notes["rep_order"] = round(100 * sum(order) / max(1, len(order)))
    notes["rep_local"] = (sum(r["rep"][WORLD] < r["rep"][HOME] for r in local), len(local))
    notes["major"] = sum(1 for t, x in pairs.items() if t in RT and x in RX and major(RT[t], RX[x]))

    # Kind 5: divisions. The club tables name 3.9.68 club ids.
    def dname(div, comps, c):
        v = div.get(c, -1)
        return comps.get(v, "none") if v >= 0 else "none"
    table_by_club = {}
    for r in table_rows:
        if r["db_club_id"].strip().lstrip("-").isdigit():
            table_by_club.setdefault(int(r["db_club_id"]), []).append(r)
    known = set(comps_t.values()) | set(comps_x.values())
    names_t = collections.Counter(comps_t.values())
    comp_ids = {v: k for k, v in comps_t.items() if names_t[v] == 1}    # 3.9.68 competition id of a unique name
    for t in sorted(TC):
        x = cmap_t.get(t)
        a = dname(div_t, comps_t, t)
        b = dname(div_x, comps_x, x) if x is not None else "(no club)"
        tab = table_by_club.get(t, [])
        real = sorted({rd if rd in known else "?" for rd in (real_division(r["division"]) for r in tab)})
        if tab and b in real:
            vs = "agrees"
        elif tab and "?" in real:
            vs = "label not mapped"
        else:
            vs = "differs" if tab else "-"
        if a != b or vs not in ("agrees", "-"):
            dec, why, new_div = league_decision(a, b, vs, real, comp_ids)
            rows["5-leagues"].append(dict(
                club=TC[t]["long"], id_3968=t, id_xbox=x if x is not None else -1, nation=TC[t]["nat"],
                division_3968=a, division_xbox=b, table_label=" | ".join(sorted({r["division"] for r in tab})) or "-",
                table_division=" | ".join(real) or "-", xbox_vs_table=vs,
                table_db_division=" | ".join(sorted({r["db_division"] or "none" for r in tab})) or "-",
                new_division_id=new_div, rule=why, decision=dec))
    notes["table_rows"] = len(table_rows)
    notes["table_no_id"] = 0
    for r in table_rows:
        if not r["db_club_id"].strip().lstrip("-").isdigit():
            notes["table_no_id"] += 1
            rd = real_division(r["division"])
            rows["5-leagues"].append(dict(
                club=r["club"], id_3968=-1, id_xbox=-1, nation="-", division_3968="-", division_xbox="-",
                table_label=r["division"], table_division=rd if rd in known else "?", xbox_vs_table="no club id",
                table_db_division=r["db_division"] or "none", new_division_id=-1,
                rule="Q24 second look: the club-table row names no club of the database", decision=HELD))

    # Kind 6: names and competitions.
    for x, t in sorted(cmap.items(), key=lambda kv: kv[1]):
        if (TC[t]["long"], TC[t]["short"]) != (XC[x]["long"], XC[x]["short"]):
            fio = TC[t]["long"] == "Fiorentina"
            rows["6-names"].append(dict(
                item="club name", id_3968=t, id_xbox=x, value_3968=f"{TC[t]['long']} / {TC[t]['short']}",
                value_xbox=f"{XC[x]['long']} / {XC[x]['short']}",
                rule="Q5: Fiorentina becomes Florentia Viola" if fio else "a name change that the spec does not name",
                decision=APPLIED if fio else HELD))
        if TC[t]["nat"] != XC[x]["nat"]:
            rows["6-names"].append(dict(
                item="club nation", id_3968=t, id_xbox=x, value_3968=TC[t]["nat"], value_xbox=XC[x]["nat"],
                **(dict(rule="Q27: the Xbox club has no nation, the 3.9.68 nation stays", decision=KEPT)
                   if XC[x]["nat"] < 0 else dict(rule="a nation change that the spec does not name",
                                                 decision=HELD))))
    for t in only_t:
        rows["6-names"].append(dict(item="club only in 3.9.68", id_3968=t, id_xbox=-1, value_3968=TC[t]["long"],
                                    value_xbox="-", rule="the template keeps all its clubs (Q2)", decision=HELD))
    for x in only_x:
        rows["6-names"].append(dict(item="club only on Xbox", id_3968=-1, id_xbox=x, value_3968="-",
                                    value_xbox=XC[x]["long"], rule="no new clubs: slice 3 edits rows",
                                    decision=HELD))
    notes["unused_comps"], notes["groups"] = [], []
    nt, nx = collections.Counter(comps_t.values()), collections.Counter(comps_x.values())
    used_t, used_x = collections.Counter(div_t.values()), collections.Counter(div_x.values())
    for name in sorted(set(nt) | set(nx)):
        if nt[name] != nx[name]:
            it = next((k for k, v in comps_t.items() if v == name), -1)
            ix = next((k for k, v in comps_x.items() if v == name), -1)
            turk = name.startswith(TURKISH_GROUPS) and ix >= 0
            unused = it >= 0 and ix < 0 and used_t[it] == 0
            rows["6-names"].append(dict(
                item="competition", id_3968=it, id_xbox=ix,
                value_3968=f"{name} ({used_t[it]} clubs)" if it >= 0 else "-",
                value_xbox=f"{name} ({used_x[ix]} clubs)" if ix >= 0 else "-",
                rule=GROUPS_WAIT if turk else
                "a competition row in 3.9.68 only, with no club: slice 3 may reuse it" if unused else
                "a competition change that the spec does not name", decision=HELD))
            if unused:
                notes["unused_comps"].append((it, name))
            if turk:
                notes["groups"].append((ix, name, used_x[ix]))
    for cid, name in sorted(comps_x.items(), key=lambda kv: kv[1]):
        if name.startswith(TURKISH_GROUPS) and nx[name] != nt[name]:
            for x in sorted(c for c, v in div_x.items() if v == cid):
                t = cmap.get(x)
                rows["6-names"].append(dict(
                    item=f"member of {name}", id_3968=t if t is not None else -1, id_xbox=x,
                    value_3968=dname(div_t, comps_t, t) if t is not None else "-", value_xbox=XC[x]["long"],
                    rule="Q11 and Q15: a club of the Xbox group, a league decision (5-leagues.tsv)", decision=HELD))

    # Kind 7: unclear people. A row of an unknown date of birth whose 3.9.68 namesake has the same club, dates and
    # ratings changes nothing. It is only counted.
    def ratings_differ(t, x):
        return t in RT and x in RX and bool(rating_diffs(RT[t], RX[x]))

    def same_state(t, x):
        xc = cmap.get(X[x]["club"]) if X[x]["club"] is not None else None
        return (xc, X[x]["joined"], X[x]["cend"]) == (T[t]["club"], T[t]["joined"], T[t]["cend"]) and \
            not ratings_differ(t, x)
    notes["unclear_same"] = collections.Counter()
    for reason, ts, xs in unclear:
        for x in xs or [None]:
            px = X[x] if x is not None else None
            differs = x is not None and not any(same_state(t, x) for t in ts)
            if reason.startswith("unknown date") and ts and not differs:
                notes["unclear_same"][reason] += 1
                continue
            rows["7-unclear"].append(dict(
                reason=reason, person=who(px or T[ts[0]]), born=d((px or T[ts[0]])["dob"]),
                ids_3968=",".join(map(str, ts)) or "-", clubs_3968=" | ".join(club_name(TC, T[t]["club"]) for t in ts)
                or "-", id_xbox=x if x is not None else -1, club_xbox=club_name(XC, px["club"]) if px else "-",
                joined_xbox=d(px["joined"]) if px else "-", cend_xbox=d(px["cend"]) if px else "-",
                differs="yes" if differs else "",
                ratings_differ="yes" if x is not None and any(ratings_differ(t, x) for t in ts) else "",
                rule="Q26: unclear, the 3.9.68 record stays", decision=HELD))
    return rows, notes


FIELDS = {
    "1-moves": ["person", "born", "id_3968", "id_xbox", "club_3968", "club_xbox", "new_club_id", "joined_3968",
                "joined_xbox", "cend_3968", "cend_xbox", "job_3968", "job_xbox", "summer_2002", "take_job", "rule",
                "decision"],
    "4-ratings": ["person", "born", "id_3968", "id_xbox", "club_3968", "major", "ca_3968", "ca_xbox", "pa_3968",
                  "pa_xbox", "world_3968", "world_xbox", "fields", "changes", "rule", "decision"],
    "5-leagues": ["club", "id_3968", "id_xbox", "nation", "nation_name", "division_3968", "division_xbox", "table_label",
                  "table_division", "xbox_vs_table", "table_db_division", "new_division_id", "division_new", "rule", "decision"],
    "6-names": ["item", "id_3968", "id_xbox", "value_3968", "value_xbox", "rule", "decision"],
    "7-unclear": ["reason", "person", "born", "ids_3968", "clubs_3968", "id_xbox", "club_xbox", "joined_xbox",
                  "cend_xbox", "differs", "ratings_differ", "rule", "decision"],
    "8-template-pairs": ["id_a", "person_a", "born_a", "club_a", "id_b", "person_b", "born_b", "club_b", "found",
                         "xbox_change", "rule", "decision"],
}
FIELDS["2-releases"] = FIELDS["3-dates"] = FIELDS["1-moves"]


def write_tsv(path, fields, rows):
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fields, delimiter="\t", lineterminator="\n", extrasaction="raise",
                           quoting=csv.QUOTE_NONE, quotechar=None)
        w.writeheader()
        for r in rows:
            w.writerow({k: str(v).replace("\t", " ").replace("\n", " ").replace("\r", " ") for k, v in r.items()})


def count(rows, kind, dec=None):
    return sum(1 for r in rows[kind] if dec is None or r["decision"] == dec)


def find_rows(rows, kind, name):
    n = cmdb.norm(name)
    return [r for r in rows[kind] if n in cmdb.norm(r["person"])]


def reasons(rows, key="rule"):
    """Counts of the reasons of rows, with the detail in brackets and after a colon left out."""
    why = collections.Counter(r[key].split(" (")[0].split(":")[0] for r in rows)
    return ", ".join(f"{v} {k}" for k, v in why.most_common()) or "none"


def summary(rows, notes):
    L = ["# Summer 2002 change list", "",
         "Made by `tools/seasons/own_db/xbox0203/change_list.py` from the Xbox facts (Championship Manager: Season "
         "02/03, SI) and the 3.9.68 template. Each TSV row names the person or club, the 3.9.68 value, the Xbox "
         "value, the rule and the decision. Max's answers Q21 to Q28 (2026-10-24) are applied, so the decisions "
         "are final: applied rows go into the build, kept rows keep the 3.9.68 value by an answer, and held rows "
         "keep the 3.9.68 value because no answer takes them.", "",
         "## Counts", "", "| Kind | File | Rows | Applied | Kept | Held |", "|---|---|---|---|---|---|"]
    names = {"1-moves": "Club moves", "2-releases": "Releases", "3-dates": "Join and contract dates, same club",
             "4-ratings": "Rating changes", "5-leagues": "Club league differences",
             "6-names": "Club names and competitions", "7-unclear": "Unclear people",
             "8-template-pairs": "Template duplicate pairs"}
    for k, n in names.items():
        L.append(f"| {n} | {k}.tsv | {count(rows, k)} | {count(rows, k, APPLIED)} | {count(rows, k, KEPT)} | "
                 f"{count(rows, k, HELD)} |")
    mv = rows["1-moves"]
    summer = sum(1 for r in mv if r["summer_2002"] == "yes")
    club_to_club = [r for r in mv if r["club_3968"] != "-"]
    L += ["", f"Clear matches: {notes['pairs']}. Major players among them: {notes['major']}.", "",
          f"Moves with an Xbox join date from 1 May to 30 September 2002 (a real 2002 summer transfer by date): "
          f"{summer} of {len(mv)}. Moves from no club to a club: {len(mv) - len(club_to_club)}. Club to club: "
          f"{len(club_to_club)}, of them {sum(1 for r in club_to_club if r['summer_2002'])} in that window. The join "
          f"date {PLACEHOLDER_JOIN.isoformat()} is a placeholder in both sets. {notes['placeholder']} of the matched "
          f"Xbox people have it, and {sum(1 for r in mv if r['joined_xbox'] == PLACEHOLDER_JOIN.isoformat())} moves "
          "have it.", ""]
    agree, local = notes["rep_order"], notes["rep_local"]
    L += ["## World reputation", "",
          "The player row holds three reputations as i16 at +0x09, +0x0b and +0x0d. The code reads +0x0d as world "
          "reputation. It compares it with 7500 before it sends a player's news to all managers (`cmp word "
          "[eax+0xd], 0x1d4c` at 0x419bf2 in awol_post_news 0x419ad0). The national squad pools sort by +0x09 as "
          "home reputation (0x778edd in compare_players_by_home_reputation_desc 0x778ed0). That makes +0x0b the "
          "current reputation.", "",
          "The limit: the code reads a player record in memory. The files store 0 to 200, but the code compares with "
          "7500. I did not find where the scale changes, so I did not prove that the memory record has the file "
          "layout.", "",
          f"The data supports the order. Home, current and world reputation fall in that order for {agree}% of the "
          f"3.9.68 players. Of the {local[1]} players with a home reputation of 50 or more, {local[0]} have a lower "
          "world reputation. The rule \"world reputation 150 or more\" uses +0x0d.", ""]
    L += ["## Examples", ""]
    for name in EXAMPLES:
        hits = find_rows(rows, "1-moves", name) or find_rows(rows, "7-unclear", name)
        for r in hits[:2]:
            if "club_3968" in r:
                L.append(f"- {r['person']} (born {r['born']}): {r['club_3968']} to {r['club_xbox']}, joined "
                         f"{r['joined_xbox']}, contract to {r['cend_xbox']}. {r['decision'].capitalize()}.")
            else:
                L.append(f"- {r['person']}: unclear ({r['reason']}). Held.")
        if not hits:
            L.append(f"- {name}: not in the moves.")
    L += ["", "## Answers (Q21 to Q28) and the rows still held", ""]
    held = {k: [r for r in rows[k] if r["decision"] == HELD] for k in names}
    allp = [r for k in ("1-moves", "2-releases", "3-dates") for r in rows[k]]
    by_q = lambda q: sum(1 for r in allp if r["decision"] == APPLIED and q in r["rule"])  # noqa: E731
    L.append(f"1. Q21 (a move with no Xbox contract end takes the Xbox club with no contract end): {by_q('Q21')} "
             "moves applied.")
    L.append(f"2. Q22 (the Xbox job too): {sum(1 for r in allp if r['take_job'])} rows applied with the Xbox job. "
             f"Held: {sum(1 for r in allp if NO_ROW in r['rule'])}, where the Xbox job needs a player row that the "
             "3.9.68 row lacks (slice 3 adds no rows).")
    L.append(f"   Full club lists: {notes.get('full_lists', 0)} applied rows are held because the person would join "
             "a club list (squad, manager, coaches and so on) that is full in 3.9.68 and whose holders stay.")
    L.append(f"3. Q23 (no Xbox squad, or no Xbox club): {sum(1 for r in allp if r['rule'].startswith('Q23') or 'Q23:' in r['rule'])} "
             "releases keep the 3.9.68 club.")
    lg = rows["5-leagues"]
    second = [r for r in lg if r["decision"] == HELD]
    L.append(f"4. Q24 (leagues): {count(rows, '5-leagues', APPLIED)} clubs take a new league (the Xbox league, or "
             f"with the league check the checked one), {count(rows, '5-leagues', KEPT)} rows keep the 3.9.68 league "
             "(the club table or the check agrees with it, or the row names no club), and "
             f"{len(second)} rows go to leagues-second-look.tsv (they keep the 3.9.68 league in this build). Per "
             "nation: " + ", ".join(f"{n} {c}" for n, c in sorted(collections.Counter(
                 r["nation_name"] for r in second).items(), key=lambda kv: (-kv[1], kv[0]))) + ".")
    if "swaps" in notes:
        L.append(f"   Q39 (every division keeps its 3.9.68 club count): {notes['swaps']} swaps and chains applied, "
                 f"{notes['unpaired']} moves held with no partner (on leagues-second-look.tsv).")
    if "league_left" in notes:
        L.append(f"   Q33 to Q37 (the league check with two sources per row, --league-check): applied in the counts "
                 f"above. Checked rows that no answer settles: {notes['league_left']}.")
    L.append(f"5. Q25 (rating changes of clear matches): {count(rows, '4-ratings', APPLIED)} applied, "
             f"{sum(1 for r in rows['4-ratings'] if r['major'] == 'no')} of them for players who are not major, and "
             f"{sum(1 for r in rows['4-ratings'] if 'Q26' in r['rule'])} of them for Q26 clear matches (the list "
             f"before the answers had the dated matches only). Held: {count(rows, '4-ratings', HELD)}, rows of "
             "template duplicate pairs.")
    L.append(f"6. Q26 (unknown date of birth, the name once in each set with the same nation): {notes['q26']} more "
             f"clear matches. They give {sum(1 for r in allp if r['rule'].startswith('Q26') or 'Q26 clear' in r['rule'])} "
             f"rows of kinds 1 to 3. Unclear rows left: {len(rows['7-unclear'])}, all held.")
    L.append("   Turkish groups (Q11): the Xbox groups are " + ", ".join(f"{n} ({c} clubs)" for _, n, c in
                                                                    notes["groups"]) + ". They are held: the "
             "build needs three free 3.9.68 competition rows and adds no rows. The 3.9.68 rows with no club and no "
             "Xbox row are " + (", ".join(f"{n} ({i})" for i, n in notes["unused_comps"]) or "none") + ". They "
             "are not free: the game finds them by name (an error box at load when one is missing) and clubs hold "
             "them as last division (build_2002.py, step 6). Only two Turkish rows are free. This needs your "
             "decision (spec, Open decisions).")
    L.append(f"7. Q27 (no Xbox nation): {sum(1 for r in rows['6-names'] if r['rule'].startswith('Q27'))} clubs keep "
             "the 3.9.68 nation.")
    tp = rows["8-template-pairs"]
    L.append(f"8. Q28 (template pairs): {count(rows, '8-template-pairs', APPLIED)} pairs are one person and the "
             f"build merges them, {count(rows, '8-template-pairs', KEPT)} are two people by duplicates.kept_apart. "
             "The build writes kept-pairs.tsv with the ids after the merge.")
    L.append("")
    L.append(f"Still held: moves {len(held['1-moves'])} ({reasons(held['1-moves'])}); releases "
             f"{len(held['2-releases'])} ({reasons(held['2-releases'])}); dates {len(held['3-dates'])} "
             f"({reasons(held['3-dates'])}); names and competitions {len(held['6-names'])} "
             f"({reasons(held['6-names'])}).")
    L += ["", "## Duplicates (Q14)", "",
          f"Template pairs by the duplicates.py rules: {len(notes['dup_before'])}. A change of a row in a template "
          "pair is held. Rounds of new pairs that the applied club changes made, each held before the next round: "
          f"{', '.join(map(str, notes['dup_rounds'])) or 'none'}. After the last round no applied change makes a "
          "new pair. No change adds a row.", "",
          "## Clubs", "",
          "Club ids differ between the sets, so clubs map by name and nation. Mapped by squad: "
          + (", ".join(f"Xbox {x} to 3.9.68 {t} ({n} of {m} people)" for x, t, n, m in notes["squad_maps"]) or "none")
          + f". Clubs only in 3.9.68: {len(notes['only_t'])}. Only on Xbox: {len(notes['only_x'])}. Club table rows "
          f"of 2002/03: {notes['table_rows']}, of them {notes['table_no_id']} with no club id (held in kind 5).", "",
          "## The build (slice 3)", "",
          "`xbox0203/build_2002.py` patches a copy of the 3.9.68 files in place with the applied rows of these "
          "files and adds no row. Its header gives the steps and the checks."]
    return "\n".join(L)


# ---- Max's league answers (Q33 to Q37, 2026-10-24) on the checked rows of league-check/all-results.tsv

Q37 = {3776: "none", 5508: "lower", 6369: "lower", 3847: "lower", 9522: "lower", 5331: "lower", 7574: "lower",
       7575: "lower", 11488: "Finnish Second Division North", 2490: "none", 7639: "none", 10994: "none",
       2059: "keep", 9569: "keep", 5682: "keep", 5683: "keep", 6846: "keep"}     # 6846 Lusitanos: Q36
LOWER = {"France": "French Lower Division", "Finland": "Finnish Lower Division", "Croatia": "Croatian Lower Division",
         "Russia": "Russian Lower Division"}
NO_DIVISION = -2                                    # club.dat +0x57 of a club with no division in 3.9.68


def league_check(rows5, path, div_t, comps_t, TC, nations):
    """Applies the checked league rows (each with two sources) to the kind 5 rows by Max's answers:
    Q34 every "change" row takes its game division (a new row when the club has none), "ok_as_built" keeps the
    built value, Q36 and Q37 settle the "max" rows by club id, Q35 rows with no club need nothing. Q33 (the 2003
    season for the calendar-year leagues) is in the check itself. Returns the rows left unsettled."""
    with open(path, newline="", encoding="utf-8") as h:
        checked = list(csv.DictReader(h, delimiter="\t", quoting=csv.QUOTE_NONE))
    ids = {v: k for k, v in comps_t.items()}
    by_id = {int(r["id_3968"]): r for r in rows5 if int(r["id_3968"]) >= 0}
    left = []

    def name_of(c):
        v = div_t.get(c, -1)
        return comps_t.get(v, "none") if v >= 0 else "none"

    def settle(c, target, why):
        if target == "none":
            new, name = NO_DIVISION, "none"
        else:
            need(target in ids, f"league check: no 3.9.68 competition {target!r}")
            new, name = ids[target], target
        r = by_id.get(c)
        if r is None:
            r = dict(club=TC[c]["long"], id_3968=c, id_xbox=-1, nation=TC[c]["nat"],
                     nation_name=nations.get(TC[c]["nat"], ("-",))[0], division_3968=name_of(c), division_xbox="-",
                     table_label="-", table_division="-", xbox_vs_table="-", table_db_division="-")
            rows5.append(r)
            by_id[c] = r
        r["division_new"] = name
        if name == r["division_3968"]:
            r.update(new_division_id=-1, rule=why + ", the 3.9.68 league stays", decision=KEPT, division_new="")
        else:
            r.update(new_division_id=new, rule=why, decision=APPLIED)

    for row in checked:
        raw = row["id_3968"].strip()
        if not raw.isdigit():
            if raw.rstrip("?").isdigit() and int(raw.rstrip("?")) in Q37:
                raw = raw.rstrip("?")                   # an id the check was unsure of, which Q37 names
            elif raw in ("not in database", "-1"):
                continue                                # Q35: no club in the database (and the reserve sides)
            else:
                left.append(row)                        # an unsure id that no answer names
                continue
        c, dec = int(raw), row["decision"]
        need(c in TC, f"league check: club {c} is not in 3.9.68")
        if c in Q37:
            how = Q37[c]
            nat = nations.get(TC[c]["nat"], ("-",))[0]
            if how == "keep":
                settle(c, name_of(c), "Q36/Q37 (Max)")
            else:
                # A club with no nation in 3.9.68 (JCO Le Creusot, Stade Lannionnais) takes the checked division.
                if how == "lower":
                    how = LOWER.get(nat, row["game_division"].strip())
                    need(how.endswith("Lower Division"), f"league check: no Lower Division for club {c}")
                settle(c, how, "Q37 (Max)")
        elif dec == "change":
            settle(c, row["game_division"].strip(), "Q34: two sources agree on " + row["real_league_2002_03"].strip())
        elif dec == "ok_as_built":
            r = by_id.get(c)
            if r is not None and r["decision"] == HELD:
                settle(c, name_of(c), "Q34: two sources agree with the build")
        else:
            left.append(row)
    for r in rows5:
        if int(r["id_3968"]) < 0 and r["decision"] == HELD:
            r.update(rule="Q35: the club-table row names no club of the database, no action", decision=KEPT)
        r.setdefault("division_new", r["division_xbox"] if r["decision"] == APPLIED else "")
    return left


# ---- Q39 (Max, 2026-10-24): every division keeps its 3.9.68 club count

FIN_GROUPS = ("Finnish First Division North", "Finnish First Division South")
FIN_GROUP_OF = {8116: "Finnish First Division North",       # Oulun Luistinseura, Oulu, up from Kakkonen North
                12164: "Finnish First Division North"}      # Vaasan Palloseura, Vaasa, down from the Veikkausliiga
NONE_NODE = "none"


def read_ranks(path):
    """({competition id: rank +0x69}, {club id: reputation +0x80}) of a format 1 data folder."""
    F = cmdb1.open_folder(path)
    ranks = {u("<i", r, 0)[0]: u("<h", r, 0x69)[0] for r in F.rows(12)}
    rep = {u("<i", r, 0)[0]: u("<h", r, 0x80)[0] for t in (0, 1) for r in F.rows(t)}
    return ranks, rep


def read_places(path):
    """{club id: row} of the 2001/02 final places (league-check/places-2001-02.tsv: place, table_size, how,
    table_name, source, in_left_division)."""
    with open(path, newline="", encoding="utf-8") as h:
        return {int(r["id_3968"]): r for r in csv.DictReader(h, delimiter="\t", quoting=csv.QUOTE_NONE)}


def move_key(c, up, places, rep):
    """The order in which balance takes the moves of one kind (the first are applied, the last held), by the rule
    of Q39: a club that was not in the division it leaves (the 3.9.68 row is out of date) first; then, going down,
    the club with the worst 2001/02 place first (the relegated club with the best place stays up); going up, the
    champion, then the automatic places by place, then the play-off winners (the lowest promotion place stays
    down); then the clubs with no known place, by 3.9.68 reputation. Returns (key, note of the order)."""
    p = places.get(c)
    out_of_date = bool(p) and p.get("in_left_division") == "no"
    if p and p["place"].isdigit() and p["table_size"].isdigit():
        frac = int(p["place"]) / int(p["table_size"])
        how = p["how"]
        if up:
            v = 0.0 if how == "champion" else (1.0 + frac if "play-off" in how else frac)
        else:
            v = -frac
        where = f"{p['place']} of {p['table_size']}, {how} ({p['table_name']}, {p['source']})"
        if out_of_date:
            return (0, v, c), "not in the division it leaves in the season before (the 3.9.68 row is out of date): " + where
        return (1, v, c), where
    if out_of_date:
        return (0, 0.0, c), f"not in the division it leaves in the season before ({p['table_name']}, {p['source']})"
    return (2, -rep.get(c, 0) if up else rep.get(c, 0), c), "no 2001/02 place known: ordered by 3.9.68 reputation"


def balance(rows5, div_t, comps_t, ranks, rep, places=None):
    """Q39: applies the league moves only as swaps, so that every division keeps its 3.9.68 club count (the
    original's create_table takes a fixed count, season_clubs.c CLASS_SIZE). A move pairs with a move the other
    way between the same two divisions (higher reputation first up, lower first down), then the moves left form
    chains (a to b, b to c, c to a; "no division" counts as one more node). A move with no partner is held and
    the club keeps its 3.9.68 league. Finland keeps its North and South groups: the checked "Finnish First
    Division" (one Ykkönen group from 2003) is the club's 3.9.68 group, or FIN_GROUP_OF. Returns the held rows."""
    ids = {v: k for k, v in comps_t.items()}

    def node(d):
        return d if d >= 0 else NONE_NODE

    def rank(n):
        return ranks.get(n, -999) if n != NONE_NODE else -1000

    def same_level(a, b):
        """Two groups of one level: the same rank (+0x69) and the same name but its last word (Russian Second
        Division - Central and - Volga, German Regional Division North and South). The rank alone is not the
        level: the American A-League and D3-Pro League share rank 6."""
        if NONE_NODE in (a, b) or a not in ranks or b not in ranks or ranks[a] != ranks[b]:
            return False
        stem = lambda n: comps_t[n].replace(" - ", " ").rsplit(" ", 1)[0]
        return stem(a) == stem(b)

    moves = []
    for r in rows5:
        if r["decision"] != APPLIED:
            continue
        c = int(r["id_3968"])
        src, dst = node(div_t.get(c, -2)), node(int(r["new_division_id"]))
        if dst != NONE_NODE and comps_t[dst] == "Finnish First Division":
            if src != NONE_NODE and comps_t[src] in FIN_GROUPS:
                r.update(new_division_id=-1, division_new="", decision=KEPT,
                         rule=r["rule"] + "; Q39: Finland keeps its North and South groups, the club stays in its "
                         "3.9.68 group")
                continue
            need(c in FIN_GROUP_OF, f"Q39: no Finnish group for club {c}")
            dst = ids[FIN_GROUP_OF[c]]
            r.update(new_division_id=dst, division_new=FIN_GROUP_OF[c],
                     rule=r["rule"] + "; Q39: the Finnish First Division group " + FIN_GROUP_OF[c])
        if src == dst:
            r.update(new_division_id=-1, division_new="", decision=KEPT, rule=r["rule"] + ", the 3.9.68 league stays")
            continue
        up = rank(dst) > rank(src)
        key, why = move_key(c, up, places or {}, rep)
        moves.append(dict(row=r, src=src, dst=dst, key=key, why=why,
                          level=same_level(src, dst)))
    left = sorted(moves, key=lambda m: m["key"])
    taken = []

    def find(out, start, n, path, on_path):
        """A chain of moves from n back to start (each division once on the path), or None."""
        for m in out.get(n, []):
            if m["dst"] == start:
                return path + [m]
            if m["dst"] not in on_path:
                got = find(out, start, m["dst"], path + [m], on_path | {m["dst"]})
                if got:
                    return got
        return None

    def chains(left, level_only):
        """Takes chains (two moves or more) from left until none is left; with level_only, only chains that hold a
        move within one level, which is never held for balance (they come before every other pairing)."""
        while True:
            out = collections.defaultdict(list)
            for m in left:
                out[m["src"]].append(m)
            cycle = None
            if level_only:
                for m in left:
                    if m["level"]:
                        cycle = find(out, m["src"], m["dst"], [m], {m["src"], m["dst"]})
                        if cycle:
                            break
            else:
                for start in sorted(out, key=str):
                    cycle = find(out, start, start, [], {start})
                    if cycle:
                        break
            if not cycle:
                return left
            taken.append(cycle)
            used = {id(m) for m in cycle}
            left = [m for m in left if id(m) not in used]

    left = chains(left, True)
    # Swaps between two divisions, in key order.
    by_pair = collections.defaultdict(list)
    for m in left:
        by_pair[(m["src"], m["dst"])].append(m)
    for (a, b), there in sorted(by_pair.items(), key=lambda kv: str(kv[0])):
        if str(a) < str(b):
            for x, y in zip(there, by_pair.get((b, a), [])):
                taken.append([x, y])
    used = {id(m) for cyc in taken for m in cyc}
    left = chains([m for m in left if id(m) not in used], False)
    for cyc in taken:
        names = [m["row"]["club"] for m in cyc]
        for m in cyc:
            others = ", ".join(n for n in names if n != m["row"]["club"])
            m["row"]["rule"] += ("; Q39: swap with " if len(cyc) == 2 else "; Q39: chain of moves with ") + others
    for m in left:
        r = m["row"]
        r.update(new_division_id=-1, decision=HELD,         # division_new keeps the checked division, for the list
                 rule=r["rule"] + "; Q39 held: no swap partner, so the division would change its club count; "
                 "the 3.9.68 league stays" + ("" if m["level"] else "; order: " + m["why"]))
    # Every division keeps its count.
    delta = collections.Counter()
    for r in rows5:
        if r["decision"] == APPLIED:
            delta[node(div_t.get(int(r["id_3968"]), -2))] -= 1
            delta[node(int(r["new_division_id"]))] += 1
    need(all(v == 0 for k, v in delta.items() if k != NONE_NODE), f"Q39: a division changes its count: {delta}")
    return [m["row"] for m in left], len(taken)


def outside_repo(path):
    """True when path is outside this checkout and outside the main checkout of a worktree."""
    path = os.path.realpath(path)
    roots = {REPO}
    marker = os.sep + os.path.join(".claude", "worktrees") + os.sep
    if marker in REPO + os.sep:
        roots.add(REPO[:(REPO + os.sep).index(marker)])
    return not any(os.path.commonpath([path, r]) == r for r in roots)


def main(argv):
    check = places = None
    if "--league-check" in argv:
        i = argv.index("--league-check")
        need(i + 1 < len(argv), "--league-check needs a file")
        check, argv = argv[i + 1], argv[:i] + argv[i + 2:]
    if "--places" in argv:
        i = argv.index("--places")
        need(i + 1 < len(argv), "--places needs a file")
        places, argv = argv[i + 1], argv[:i] + argv[i + 2:]
    need(len(argv) in (5, 6), "usage: change_list.py <facts-xbox> <facts-3968> <xbox data> <3.9.68 Data> <out> "
                              "[<rules folder>] [--league-check <all-results.tsv>] [--places <places-2001-02.tsv>]")
    fxp, ftp, dx, dt, out = argv[:5]
    rules = argv[5] if len(argv) == 6 else RULES
    need(outside_repo(out), "the out folder must be outside the repo")
    fx, ft = load(fxp), load(ftp)
    div_x, comps_x = read_divisions(dx)
    div_t, comps_t = read_divisions(dt)
    need(set(div_t) == set(ft["clubs"]) and set(div_x) == set(fx["clubs"]),
         "the club ids of the data folders and the facts differ: rerun read_xbox0203.py")
    rows, notes = build(ft, fx, div_t, comps_t, div_x, comps_x, club_table_rows(rules),
                        (cmdb1.open_folder(dt), cmdb1.open_folder(dx)), HOLDERS_Q40)
    nations = ft["nations"]
    for r in rows["5-leagues"]:
        r["nation_name"] = nations[r["nation"]][0] if r["nation"] in nations else "(no club id)"
    if check:
        notes["league_left"] = len(league_check(rows["5-leagues"], check, div_t, comps_t, ft["clubs"], nations))
    for r in rows["5-leagues"]:
        r.setdefault("division_new", r["division_xbox"] if r["decision"] == APPLIED else "")
    ranks, rep = read_ranks(dt)
    unpaired, notes["swaps"] = balance(rows["5-leagues"], div_t, comps_t, ranks, rep,
                                       read_places(places) if places else None)
    notes["unpaired"] = len(unpaired)
    os.makedirs(out, exist_ok=True)
    for kind, fields in FIELDS.items():
        write_tsv(os.path.join(out, kind + ".tsv"), fields, rows[kind])
    second = sorted((r for r in rows["5-leagues"] if r["decision"] == HELD),
                    key=lambda r: (r["nation_name"], r["club"], r["id_3968"]))
    write_tsv(os.path.join(out, "leagues-second-look.tsv"), FIELDS["5-leagues"], second)
    text = summary(rows, notes)
    with open(os.path.join(out, "summary.md"), "w", encoding="utf-8") as f:
        f.write(text + "\n")
    print(text)


if __name__ == "__main__":
    main(sys.argv[1:])
