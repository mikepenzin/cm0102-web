"""Write the Summer 2002 change list: what the Xbox facts change in the 3.9.68 template, for Max to read first.

Usage: python3 -I change_list.py <facts-xbox> <facts-3968> <xbox data> <3.9.68 Data> <out folder> [<rules folder>]

The facts folders come from read_xbox0203.py. The script reads the data folders again for the club divisions
(club.dat +0x57) and the competition names (club_comp.dat). The facts do not hold them. The rules folder (default:
docs/research/rules-0.6.0 of the repo) gives the 2002/03 rows of the *-clubs.tsv files. The data is untrusted
input. Nothing from it runs. The out folder must be outside the repo. Commit no Xbox data and nothing made from it.

Rules (spec docs/specs/2026-10-24-summer-2002-db-spec.md, Decisions):
- Match: a person matches when his normalised first name, surname and date of birth (cmdb.norm) give exactly one
  row in each set. The date must be known (year above 1900) and the nation must be the same. Every other person
  is unclear. He keeps his 3.9.68 record (kind 7).
- Clubs: the club ids of the two sets differ, because rows are added, removed or sorted in another place. Clubs
  map by name and nation (map_clubs). Clubs of one name in several rows, and a club in one set only, map by squad.
- Kind 1 moves and kind 3 dates: a clear match takes the Xbox club, join date and contract end (Q3). The row is
  held when the job differs or when the Xbox data has no contract end in place of a known one.
- Kind 2 releases: a club in 3.9.68 and none on Xbox. The release applies only on a clear match (Q4, Q10, Q14).
  It also needs the club's squad in the Xbox data (release_guard), as the Xbox copy drops the people of many
  small clubs. A squad counts when the Xbox data has at least half as many people at the club as 3.9.68.
- Kind 4 ratings: the rating rows that differ for clear matches. Major player: current ability 140 or more, or
  world reputation 150 or more, in either set (Q8). World reputation is the i16 at +0x0d of the player row. The
  summary gives the proof and its limit. The rating changes apply (Q9). A row of a player who is not major says so.
- Kind 5 leagues: every club whose division differs between the 3.9.68 club.dat, the Xbox club.dat and the
  2002/03 club-table row, and every club-table row with no club id. All are held for Max (Q15).
- Kind 6 names and competitions: club names and nations that differ, clubs and competitions in one set only, and
  the clubs of the Xbox Turkish Category B groups (Q11). The Fiorentina name (Q5) and the three groups apply. The
  group of each club is a league decision, so it is held in kind 6 and in kind 5. Every other row is held.
- Duplicates (Q14): no change adds a row. duplicates.find (people.py rules and the same-club rule) runs on the
  3.9.68 people before and after the applied club changes. A change that makes a new pair is held, and so is a
  change of a row in a template pair, as both rows of one person must move together. This repeats until no new
  pair is left. The template pairs go to 8-template-pairs.tsv, as duplicates.py check stops the build on them.

Output: one TSV per kind (1-moves.tsv to 8-template-pairs.tsv) and summary.md, with counts and the held rows.
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

RULES = os.path.join(REPO, "docs", "research", "rules-0.6.0")
SEASON = "2002/03"
MAJOR_CA, MAJOR_WORLD = 140, 150
HOME, CURRENT, WORLD = 0, 1, 2              # index in the ratings "rep" tuple (+0x09, +0x0b, +0x0d)
SUMMER = (datetime.date(2002, 5, 1), datetime.date(2002, 9, 30))
MIN_SQUAD_MAP = 3                           # clear matches that map a club in one set only by its squad
PLACEHOLDER_JOIN = datetime.date(1996, 1, 2)    # a join date that both sets give to many people
EXAMPLES = ("Fabio Celestini", "Vitaly Kutuzov", "Stephen Lovell")    # named in the research doc
TURKISH_GROUPS = "Turkish 2. Division Category B"
APPLIED, HELD = "applied", "held"
WIPED = "the Xbox data has no contract end, 3.9.68 has one"


# ---- people

def person_key(p):
    """(first name, surname, date of birth) for matching, or None when the date is unknown."""
    if p["dob"] is None:
        return None
    return (cmdb.norm(p["fn"]), cmdb.norm(p["sn"]), p["dob"])


def name_key(p):
    return (cmdb.norm(p["fn"]), cmdb.norm(p["sn"]))


def match_people(T, X):
    """Clear matches and unclear people. T and X are people_all lists (3.9.68, Xbox).

    Returns (pairs, unclear): pairs {3.9.68 index: Xbox index}; unclear a list of (reason, [3.9.68 indexes],
    [Xbox indexes]) for each Xbox person that has no clear match and each name and date that is in more than one
    row. A 3.9.68 person with no Xbox row is not unclear: the Xbox copy lacks him and he keeps his record (Q2)."""
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
    for n in ("people_all", "clubs", "ratings"):
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


def build(ft, fx, div_t, comps_t, div_x, comps_x, table_rows):
    """All rows of the change list: {kind file: [row dict]}, and the notes for summary.md."""
    T, X = ft["people_all"], fx["people_all"]
    TC, XC = ft["clubs"], fx["clubs"]
    pairs, unclear = match_people(T, X)
    cmap, squad_maps, only_t, only_x = map_clubs(TC, XC, pairs, T, X)
    cmap_t = {t: x for x, t in cmap.items()}
    xsquads = collections.Counter(p["club"] for p in X if p["club"] is not None)
    tsquads = collections.Counter(p["club"] for p in T if p["club"] is not None)
    rows = collections.defaultdict(list)
    notes = dict(pairs=len(pairs), squad_maps=squad_maps, only_t=only_t, only_x=only_x,
                 placeholder=sum(1 for x in pairs.values() if X[x]["joined"] == PLACEHOLDER_JOIN))

    # Kinds 1 to 3: club, join date and contract end of clear matches.
    planned = []
    for t, x in sorted(pairs.items()):
        pt, px = T[t], X[x]
        new = cmap.get(px["club"]) if px["club"] is not None else None
        if px["club"] is not None and new is None:
            planned.append(("1-moves", t, x, None, HELD, "the Xbox club is not in 3.9.68"))
            continue
        job_differs = (pt["role"], pt["job"]) != (px["role"], px["job"])
        job_note = f"the job differs (3.9.68 {pt['role']} {pt['job']}, Xbox {px['role']} {px['job']}, FM codes)"
        if new != pt["club"] and job_differs:
            planned.append(("2-releases" if new is None else "1-moves", t, x, new, HELD,
                            job_note + ": Q3 takes the club and dates, not the job"))
        elif new != pt["club"]:
            if new is None:
                ok, why = release_guard(t, x, pairs, T, xsquads, tsquads, cmap_t)
                planned.append(("2-releases", t, x, None, APPLIED if ok else HELD, why))
            elif pt["cend"] and not px["cend"]:
                planned.append(("1-moves", t, x, new, HELD, WIPED))
            else:
                planned.append(("1-moves", t, x, new, APPLIED, "clear match: Xbox club, join date, contract end"))
        elif (pt["joined"], pt["cend"]) != (px["joined"], px["cend"]):
            if pt["cend"] and not px["cend"]:
                planned.append(("3-dates", t, x, new, HELD, WIPED))
            else:
                planned.append(("3-dates", t, x, new, APPLIED, "clear match, same club: Xbox join date, contract end"))
        elif job_differs:
            planned.append(("3-dates", t, x, new, HELD, job_note + ", same club and dates: Q3 does not take the job"))
    before = duplicate_pairs(T)
    in_pair = {i for k in before for i in k}
    planned = [(kind, t, x, new, HELD, "the 3.9.68 row is one of a template duplicate pair: " + pair_text(t, before))
               if dec == APPLIED and kind != "3-dates" and t in in_pair else (kind, t, x, new, dec, why)
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
    for kind, t, x, new, dec, why in planned:
        pt, px = T[t], X[x]
        jx = px["joined"]
        summer = bool(kind == "1-moves" and jx and SUMMER[0] <= jx <= SUMMER[1])
        rows[kind].append(dict(person=who(pt), born=d(pt["dob"]), id_3968=t, id_xbox=x,
                               club_3968=club_name(TC, pt["club"]), club_xbox=club_name(XC, px["club"]),
                               new_club_id=new if new is not None else -1,
                               joined_3968=d(pt["joined"]), joined_xbox=d(jx), cend_3968=d(pt["cend"]),
                               cend_xbox=d(px["cend"]), job_3968=f"{pt['role']} {pt['job']}",
                               job_xbox=f"{px['role']} {px['job']}", summer_2002="yes" if summer else "",
                               rule=why, decision=dec))
    touched = {t for kind, t, x, new, dec, why in planned}
    for (a, b), why in sorted(before.items()):
        rows["8-template-pairs"].append(dict(
            id_a=a, person_a=who(T[a]), born_a=d(T[a]["dob"]), club_a=club_name(TC, T[a]["club"]), id_b=b,
            person_b=who(T[b]), born_b=d(T[b]["dob"]), club_b=club_name(TC, T[b]["club"]), found=why,
            xbox_change="yes" if a in touched or b in touched else "",
            rule="Q14: duplicates.py check stops the build while a pair is left", decision=HELD))

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
                    rule="Q9: the Xbox rating changes are applied" + ("" if m else " (not a major player)"),
                    decision=APPLIED))
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
            rows["5-leagues"].append(dict(
                club=TC[t]["long"], id_3968=t, id_xbox=x if x is not None else -1, nation=TC[t]["nat"],
                division_3968=a, division_xbox=b, table_label=" | ".join(sorted({r["division"] for r in tab})) or "-",
                table_division=" | ".join(real) or "-", xbox_vs_table=vs,
                table_db_division=" | ".join(sorted({r["db_division"] or "none" for r in tab})) or "-",
                rule="Q15: every difference goes to Max", decision=HELD))
    notes["table_rows"] = len(table_rows)
    notes["table_no_id"] = 0
    for r in table_rows:
        if not r["db_club_id"].strip().lstrip("-").isdigit():
            notes["table_no_id"] += 1
            rd = real_division(r["division"])
            rows["5-leagues"].append(dict(
                club=r["club"], id_3968=-1, id_xbox=-1, nation="-", division_3968="-", division_xbox="-",
                table_label=r["division"], table_division=rd if rd in known else "?", xbox_vs_table="no club id",
                table_db_division=r["db_division"] or "none",
                rule="Q15: the club-table row names no club of the database", decision=HELD))

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
                rule="a nation change that the spec does not name", decision=HELD))
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
                rule="Q11: three Turkish Category B groups (slice 5 code)" if turk else
                "a competition row in 3.9.68 only, with no club: slice 3 may reuse it" if unused else
                "a competition change that the spec does not name", decision=APPLIED if turk else HELD))
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
                rule="Q11, Q14: unclear, the 3.9.68 record stays", decision=HELD))
    return rows, notes


FIELDS = {
    "1-moves": ["person", "born", "id_3968", "id_xbox", "club_3968", "club_xbox", "new_club_id", "joined_3968",
                "joined_xbox", "cend_3968", "cend_xbox", "job_3968", "job_xbox", "summer_2002", "rule", "decision"],
    "4-ratings": ["person", "born", "id_3968", "id_xbox", "club_3968", "major", "ca_3968", "ca_xbox", "pa_3968",
                  "pa_xbox", "world_3968", "world_xbox", "fields", "changes", "rule", "decision"],
    "5-leagues": ["club", "id_3968", "id_xbox", "nation", "division_3968", "division_xbox", "table_label",
                  "table_division", "xbox_vs_table", "table_db_division", "rule", "decision"],
    "6-names": ["item", "id_3968", "id_xbox", "value_3968", "value_xbox", "rule", "decision"],
    "7-unclear": ["reason", "person", "born", "ids_3968", "clubs_3968", "id_xbox", "club_xbox", "joined_xbox",
                  "cend_xbox", "differs", "ratings_differ", "rule", "decision"],
    "8-template-pairs": ["id_a", "person_a", "born_a", "club_a", "id_b", "person_b", "born_b", "club_b", "found",
                         "xbox_change", "rule", "decision"],
}
FIELDS["2-releases"] = FIELDS["3-dates"] = FIELDS["1-moves"]


def write_tsv(path, fields, rows):
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fields, delimiter="\t", lineterminator="\n", extrasaction="raise")
        w.writeheader()
        for r in rows:
            w.writerow({k: str(v).replace("\t", " ").replace("\n", " ") for k, v in r.items()})


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
         "value, the rule and the decision. A held row keeps its 3.9.68 value until you decide.", "",
         "## Counts", "", "| Kind | File | Rows | Applied | Held |", "|---|---|---|---|---|"]
    names = {"1-moves": "Club moves", "2-releases": "Releases", "3-dates": "Join and contract dates, same club",
             "4-ratings": "Rating changes", "5-leagues": "Club league differences",
             "6-names": "Club names and competitions", "7-unclear": "Unclear people",
             "8-template-pairs": "Template duplicate pairs"}
    for k, n in names.items():
        L.append(f"| {n} | {k}.tsv | {count(rows, k)} | {count(rows, k, APPLIED)} | {count(rows, k, HELD)} |")
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
    L += ["", "## Rows that need your decision", "", "A held row keeps its 3.9.68 value unless you decide.", ""]
    held = {k: [r for r in rows[k] if r["decision"] == HELD] for k in names}
    L.append(f"1. Releases held: {len(held['2-releases'])} ({reasons(held['2-releases'])}).")
    wiped = [n for n in EXAMPLES if any(r["rule"] == WIPED for r in find_rows(rows, "1-moves", n))]
    L.append(f"2. Moves held: {len(held['1-moves'])} ({reasons(held['1-moves'])}). The rows with no Xbox contract "
             f"end include: {', '.join(wiped) or 'none of the examples'}. Choose per row: the Xbox club with no "
             "contract end, the Xbox club with the 3.9.68 contract end, or no change.")
    L.append(f"3. Date rows held: {len(held['3-dates'])} ({reasons(held['3-dates'])}).")
    lg = rows["5-leagues"]
    a_b = sum(1 for r in lg if r["division_3968"] != r["division_xbox"])
    L.append(f"4. League differences: {len(lg)} rows. {a_b} differ between 3.9.68 and Xbox. Xbox against the club "
             "table: " + ", ".join(f"{v} {k}" for k, v in collections.Counter(r["xbox_vs_table"] for r in lg)
                                   .most_common()) + ". Answer per nation or per row with the division of the "
             "club. The most common patterns (3.9.68, Xbox, club table):")
    pat = collections.Counter((r["division_3968"], r["division_xbox"], r["table_division"]) for r in lg)
    L += [f"   - {n}: {a}, {b}, {c}" for (a, b, c), n in pat.most_common(12)]
    L.append(f"5. Names and competitions held: {len(held['6-names'])} ({reasons(held['6-names'])}). The members of "
             "the Turkish groups are held here and in the league rows, so one answer covers both.")
    rt = rows["4-ratings"]
    notmaj = sum(1 for r in rt if r["major"] == "no")
    unc_rt = sum(1 for r in rows["7-unclear"] if r["ratings_differ"])
    L.append(f"6. Rating changes: {len(rt)}, of them {notmaj} for players who are not major. The spec applies the "
             "Xbox rating changes (Q9), but research Q3 named major players only. Confirm that the changes of the "
             f"other players apply too. {unc_rt} more rating changes belong to unclear people (kind 7). They are "
             "held.")
    unc = rows["7-unclear"]
    only_xbox = sum(1 for r in unc if "only on Xbox" in r["reason"] or r["reason"] == "nation differs")
    L.append(f"7. Unclear people: {len(unc)} rows ({reasons(unc, 'reason')}). "
             f"{sum(1 for r in unc if r['differs'])} of them have another club, dates or ratings on Xbox. Not "
             f"listed: {sum(notes['unclear_same'].values())} people with an unknown date whose 3.9.68 namesake has "
             "the same club, dates and ratings. One group answer is possible: apply the rows \"unknown date of "
             "birth, the name is once in each set with the same nation\". People only on Xbox, with the one whose "
             f"nation differs: {only_xbox} rows (the research counted 26).")
    tp = rows["8-template-pairs"]
    L.append(f"8. Template duplicate pairs: {len(tp)}, of them {sum(1 for r in tp if r['xbox_change'])} with a "
             "row that the Xbox data changes (held). `duplicates.py check` stops the build while a pair is left. "
             "Slice 3 must merge them as the 2006/07 build did, or you allow some as two people.")
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
          "## Note for slice 3", "",
          "match.py, leagues.py and convert.py read format 2 only (cmdb.Folder). The changes are edits of existing "
          "rows. So slice 3 most likely patches the 3.9.68 format 1 files in place and adds no row. That needs:", "",
          "- A format 1 writer in cmdb1.py. It writes the staff rows (157 bytes) and the player rows (70 bytes) back "
          "at the same offsets, with the bounds checks of the reader.",
          "- Staff fields: club (+0x39, i32, the new_club_id column), join date (+0x3e) and contract end (+0x46). "
          "Dates are day of year and year (cmdb1.to_date in reverse). Day 365 stays as read.",
          "- Player fields: current and potential ability (+5, +7), the three reputations (+9 home, +0x0b current, "
          "+0x0d world), 12 position bytes from +0x0f and 42 attribute bytes from +0x1b.",
          "- club.dat: the long name (+4, 51 bytes) and short name (+0x38, 26 bytes) of Fiorentina. The club lists "
          "(+0x9b to +0x1e0, duplicates.CLUB_STAFF) of the old and the new club of each move and release, so that "
          "the squad lists agree with the staff rows (convert.check_squad_lists). The division (+0x57) of each "
          "league decision.",
          "- club_comp.dat: the Xbox groups are "
          + ", ".join(f"{n} (Xbox id {i}, {c} clubs)" for i, n, c in notes["groups"])
          + ". The 3.9.68 rows with no club and no Xbox row are "
          + (", ".join(f"{n} ({i})" for i, n in notes["unused_comps"]) or "none")
          + ". Slice 3 can give three unused rows the names of the groups and point the clubs' +0x57 at them. Then "
          "no row is added or removed. First check that no other table points at those rows (history, rules, "
          "nation data). Slice 5 owns the game code for the groups.",
          "- index.dat: no count changes, as no row is added.",
          "- The template pairs (item 8) need a format 1 merge, or `duplicates.py` must read format 1.", ""]
    return "\n".join(L)


def outside_repo(path):
    """True when path is outside this checkout and outside the main checkout of a worktree."""
    path = os.path.realpath(path)
    roots = {REPO}
    marker = os.sep + os.path.join(".claude", "worktrees") + os.sep
    if marker in REPO + os.sep:
        roots.add(REPO[:(REPO + os.sep).index(marker)])
    return not any(os.path.commonpath([path, r]) == r for r in roots)


def main(argv):
    need(len(argv) in (5, 6), "usage: change_list.py <facts-xbox> <facts-3968> <xbox data> <3.9.68 Data> <out> "
                              "[<rules folder>]")
    fxp, ftp, dx, dt, out = argv[:5]
    rules = argv[5] if len(argv) == 6 else RULES
    need(outside_repo(out), "the out folder must be outside the repo")
    fx, ft = load(fxp), load(ftp)
    div_x, comps_x = read_divisions(dx)
    div_t, comps_t = read_divisions(dt)
    need(set(div_t) == set(ft["clubs"]) and set(div_x) == set(fx["clubs"]),
         "the club ids of the data folders and the facts differ: rerun read_xbox0203.py")
    rows, notes = build(ft, fx, div_t, comps_t, div_x, comps_x, club_table_rows(rules))
    os.makedirs(out, exist_ok=True)
    for kind, fields in FIELDS.items():
        write_tsv(os.path.join(out, kind + ".tsv"), fields, rows[kind])
    text = summary(rows, notes)
    with open(os.path.join(out, "summary.md"), "w", encoding="utf-8") as f:
        f.write(text + "\n")
    print(text)


if __name__ == "__main__":
    main(sys.argv[1:])
