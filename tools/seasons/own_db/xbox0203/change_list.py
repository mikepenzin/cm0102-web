"""Write the Summer 2002 change list: what the Xbox facts change in the 3.9.68 template, for Max to read first.

Usage: python3 -I change_list.py <facts-xbox> <facts-3968> <xbox data> <3.9.68 Data> <out folder> [<rules folder>]

The facts folders come from read_xbox0203.py. The data folders are read again for the club divisions (club.dat
+0x57) and the competition names (club_comp.dat), which the facts do not hold. The rules folder (default: the
repo's docs/research/rules-0.6.0) gives the 2002/03 rows of the *-clubs.tsv files. The data is untrusted input:
nothing from it runs, and the out folder must be outside the repo. Commit no Xbox data and nothing made from it.

Rules (spec docs/specs/2026-10-24-summer-2002-db-spec.md, Decisions):
- Match: a person matches when the normalised first name, surname and date of birth (cmdb.norm) give exactly one
  row in each set, the date is known (year above 1900) and the nation is the same. Every other person is unclear
  and is held at his 3.9.68 record (kind 7).
- Clubs: club ids differ between the sets (the Xbox club.dat drops two French clubs and sorts Florentia Viola in
  another place, so the rows after them shift), so clubs map by name and nation (map_clubs). A club that is only
  in one set maps by its squad: most of its clear matches come from one club of the other set.
- Kind 1 moves and kind 3 dates: a clear match takes the Xbox club, join date and contract end (Q3). Applied.
- Kind 2 releases: a club in 3.9.68 and none on Xbox. Applied only on a clear match (Q4, Q10, Q14) and only when
  the Xbox data keeps the squad of that club (release_guard): the Xbox copy drops the people of about 3,800 small
  clubs, so a release at a club with no squad on Xbox is held.
- Kind 4 ratings: the rating rows that differ for clear matches. Major player: current ability 140 or more, or
  world reputation 150 or more, in either set (Q8). World reputation is the i16 at +0x0d of the player row: the
  original compares it with 7500 (`cmp word [eax+0xd], 0x1d4c` at 0x419bf2, awol_post_news 0x419ad0, the news to
  all managers for a player with a high world reputation), and orders national squads by the i16 at +0x09 as home
  reputation (0x778edd, compare_players_by_home_reputation_desc 0x778ed0). So +0x0b is current reputation. The
  data agrees: home >= current >= world for 93% of the 3.9.68 players. Rating changes are applied (Q9: the Xbox
  rating changes are applied). A change of a player who is not major is marked so in the row.
- Kind 5 leagues: every club whose division differs between the 3.9.68 club.dat, the Xbox club.dat and the
  2002/03 club-table row. Held for Max (Q15).
- Kind 6 names and competitions: club names that differ (Q5, Fiorentina to Florentia Viola), competitions in one
  set only, and the clubs of the Xbox Turkish Category B groups (Q11). Applied for Fiorentina and the groups, held
  for any other name.
- Duplicates (Q14): no applied change adds a row. The check runs duplicates.find (people.py rules and the same-club
  rule) on the 3.9.68 people before and after the applied club changes. A change that makes a new pair is held,
  and a change of a row that is already in a template pair is held, as both rows of one person must move together.

Output: one TSV per kind (1-moves.tsv to 7-unclear.tsv) and summary.md, with counts and the rows held for Max.
"""
import collections
import csv
import datetime
import os
import pickle
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OWN_DB = os.path.dirname(HERE)
REPO = os.path.dirname(os.path.dirname(os.path.dirname(OWN_DB)))
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
TURKISH_GROUPS = "Turkish 2. Division Category B"
APPLIED, HELD = "applied", "held"


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
            unclear.append(("shared name and date (3.9.68 only)", ts, []))
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

    Steps: the same long name, short name and nation; the same long name and nation; the same long name. When one
    key is in several rows of each set (the two "CSKA Kiev" rows, which the Xbox data has the other way round),
    the rows pair by squad: the pair with the most clear matches first, then the rest in id order (both club tables
    are in name order). Clubs left over map by squad: most of the Xbox club's clear matches (at least MIN_SQUAD_MAP
    and more than half) come from one 3.9.68 club that is also left over."""
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

def release_guard(t, x, pairs, T, X, xsquads, cmap_t):
    """(applied?, reason) for a 3.9.68 person at a club whom the Xbox data has with no club."""
    if pairs.get(t) != x:
        return False, "no clear match"
    xc = cmap_t.get(T[t]["club"])
    if xc is None:
        return False, "the club is not in the Xbox data"
    if xsquads.get(xc, 0) == 0:
        return False, "the Xbox data keeps no squad at that club"
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
    for n, r in enumerate(F.rows(12)):
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
    rows = collections.defaultdict(list)
    notes = dict(pairs=len(pairs), squad_maps=squad_maps, only_t=only_t, only_x=only_x,
                 placeholder=sum(1 for x in pairs.values() if X[x]["joined"] == PLACEHOLDER_JOIN))

    # Kinds 1 to 3: club, join date and contract end of clear matches.
    after = {}          # 3.9.68 index -> club id after the applied changes
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
                ok, why = release_guard(t, x, pairs, T, X, xsquads, cmap_t)
                planned.append(("2-releases", t, x, None, APPLIED if ok else HELD, why))
            else:
                planned.append(("1-moves", t, x, new, APPLIED, "clear match: Xbox club, join date, contract end"))
        elif (pt["joined"], pt["cend"]) != (px["joined"], px["cend"]):
            planned.append(("3-dates", t, x, new, APPLIED, "clear match, same club: Xbox join date, contract end"))
        elif job_differs:
            planned.append(("3-dates", t, x, new, HELD, job_note + ", same club and dates: Q3 does not take the job"))
    for kind, t, x, new, dec, why in planned:
        if dec == APPLIED and kind in ("1-moves", "2-releases"):
            after[t] = new
    before = duplicate_pairs(T)
    after_pairs = duplicate_pairs(T, after)
    new_pairs = {k: v for k, v in after_pairs.items() if k not in before}
    in_pair = {i for k in before for i in k}
    in_new = {i for k in new_pairs for i in k}
    notes["dup_before"], notes["dup_new"] = len(before), len(new_pairs)
    for kind, t, x, new, dec, why in planned:
        pt, px = T[t], X[x]
        if dec == APPLIED and kind != "3-dates" and t in in_new:
            dec, why = HELD, "the move makes a new duplicate pair: " + "; ".join(
                f"{a} and {b} ({v})" for (a, b), v in new_pairs.items() if t in (a, b))
        elif dec == APPLIED and t in in_pair:
            dec, why = HELD, "the 3.9.68 row is one of a template duplicate pair: " + "; ".join(
                f"{a} and {b} ({v})" for (a, b), v in before.items() if t in (a, b))
        if dec == HELD:
            after.pop(t, None)
        jx = px["joined"]
        summer = bool(kind == "1-moves" and jx and SUMMER[0] <= jx <= SUMMER[1])
        rows[kind].append(dict(person=who(pt), born=d(pt["dob"]), id_3968=t, id_xbox=x,
                               club_3968=club_name(TC, pt["club"]), club_xbox=club_name(XC, px["club"]),
                               new_club_id=new if new is not None else -1,
                               joined_3968=d(pt["joined"]), joined_xbox=d(jx), cend_3968=d(pt["cend"]),
                               cend_xbox=d(px["cend"]), job_3968=f"{pt['role']} {pt['job']}",
                               job_xbox=f"{px['role']} {px['job']}", summer_2002="yes" if summer else "",
                               rule=why, decision=dec))
    final_new = {k: v for k, v in duplicate_pairs(T, after).items() if k not in before}
    notes["dup_final_new"] = final_new

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
    notes["table_no_id"] = [r for r in table_rows if not r["db_club_id"].strip().lstrip("-").isdigit()]

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
                rule="a nation change that the spec does not name" + (
                    f" (the Xbox data gives nation -2 to {sum(c['nat'] == -2 for c in XC.values())} clubs, among "
                    "them Miami Fusion and Tampa Bay Mutiny, which folded in January 2002)"
                    if XC[x]["nat"] == -2 else ""), decision=HELD))
    for t in only_t:
        rows["6-names"].append(dict(item="club only in 3.9.68", id_3968=t, id_xbox=-1, value_3968=TC[t]["long"],
                                    value_xbox="-", rule="the template keeps all its clubs (Q2)", decision=HELD))
    for x in only_x:
        rows["6-names"].append(dict(item="club only on Xbox", id_3968=-1, id_xbox=x, value_3968="-",
                                    value_xbox=XC[x]["long"], rule="no new clubs: slice 3 edits rows",
                                    decision=HELD))
    nt, nx = collections.Counter(comps_t.values()), collections.Counter(comps_x.values())
    used_t, used_x = collections.Counter(div_t.values()), collections.Counter(div_x.values())
    for name in sorted(set(nt) | set(nx)):
        if nt[name] != nx[name]:
            it = next((k for k, v in comps_t.items() if v == name), -1)
            ix = next((k for k, v in comps_x.items() if v == name), -1)
            turk = name.startswith(TURKISH_GROUPS)
            unused = it >= 0 and ix < 0 and used_t[it] == 0 and name.startswith("Turkish 2. Division Group")
            rows["6-names"].append(dict(
                item="competition", id_3968=it, id_xbox=ix,
                value_3968=f"{name} ({used_t[it]} clubs)" if it >= 0 else "-",
                value_xbox=f"{name} ({used_x[ix]} clubs)" if ix >= 0 else "-",
                rule="Q11: three Turkish Category B groups (slice 5 code)" if turk else
                "Q11: an unused Turkish group row (no club), which the Xbox data drops" if unused else
                "a competition change that the spec does not name", decision=APPLIED if turk or unused else HELD))
    for cid, name in sorted(comps_x.items(), key=lambda kv: kv[1]):
        if name.startswith(TURKISH_GROUPS) and nx[name] != nt[name]:
            for x in sorted(c for c, v in div_x.items() if v == cid):
                t = cmap.get(x)
                rows["6-names"].append(dict(
                    item=f"member of {name}", id_3968=t if t is not None else -1, id_xbox=x,
                    value_3968=dname(div_t, comps_t, t) if t is not None else "-", value_xbox=XC[x]["long"],
                    rule="Q11: the clubs of the Xbox group", decision=APPLIED))

    # Kind 7: unclear people. A row of an unknown date of birth whose 3.9.68 namesake has the same club and dates
    # changes nothing, so it is only counted.
    def same_state(t, x):
        xc = cmap.get(X[x]["club"]) if X[x]["club"] is not None else None
        return (xc, X[x]["joined"], X[x]["cend"]) == (T[t]["club"], T[t]["joined"], T[t]["cend"])
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
                differs="yes" if differs else "", rule="Q11, Q14: unclear, the 3.9.68 record stays", decision=HELD))
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
                  "cend_xbox", "differs", "rule", "decision"],
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


def summary(rows, notes):
    L = ["# Summer 2002 change list", "",
         "Made by `tools/seasons/own_db/xbox0203/change_list.py` from the Xbox facts (Championship Manager: Season "
         "02/03, SI) and the 3.9.68 template. Each TSV row names the person or club, the 3.9.68 value, the Xbox "
         "value, the rule and the decision. A held row keeps its 3.9.68 value until you decide.", "",
         "## Counts", "", "| Kind | File | Rows | Applied | Held |", "|---|---|---|---|---|"]
    names = {"1-moves": "Club moves", "2-releases": "Releases", "3-dates": "Join and contract dates, same club",
             "4-ratings": "Rating changes", "5-leagues": "Club league differences",
             "6-names": "Club names and competitions", "7-unclear": "Unclear people"}
    for k, n in names.items():
        L.append(f"| {n} | {k}.tsv | {count(rows, k)} | {count(rows, k, APPLIED)} | {count(rows, k, HELD)} |")
    mv = rows["1-moves"]
    summer = sum(1 for r in mv if r["summer_2002"] == "yes")
    L += ["", f"Clear matches: {notes['pairs']}. Major players among them: {notes['major']}.",
          f"Moves with an Xbox join date from 1 May to 30 September 2002 (a real 2002 summer transfer by date): "
          f"{summer} of {len(mv)}. Moves from no club to a club: "
          f"{sum(1 for r in mv if r['club_3968'] == '-')}. Club to club: "
          f"{sum(1 for r in mv if r['club_3968'] != '-')}, of them "
          f"{sum(1 for r in mv if r['club_3968'] != '-' and r['summer_2002'])} in that window. The join date 2 January "
          f"1996 is a placeholder in both sets ({notes['placeholder']} of the matched Xbox people have it): "
          f"{sum(1 for r in mv if r['joined_xbox'] == '1996-01-02')} moves have it.", ""]
    L += ["## World reputation", "",
          "The player row holds three reputations as i16 at +0x09, +0x0b and +0x0d. +0x0d is world reputation: the "
          "original compares it with 7500 before it sends a player's news to all managers (`cmp word [eax+0xd], "
          "0x1d4c` at 0x419bf2 in awol_post_news 0x419ad0). +0x09 is home reputation: the national squad pools sort "
          "by it (0x778edd in compare_players_by_home_reputation_desc 0x778ed0). So +0x0b is current reputation. "
          "The data agrees: home, current and world fall in that order for 93% of the 3.9.68 players. The files "
          "store 0 to 200, but the code compares with 7500. I did not find where the scale changes. The offset "
          "order is proved, the scale is not.", ""]
    L += ["## Examples", ""]
    for name in ("Fabio Celestini", "Vitaly Kutuzov", "Stephen Lovell"):
        hits = find_rows(rows, "1-moves", name) or find_rows(rows, "7-unclear", name)
        for r in hits[:2]:
            if "club_3968" in r:
                L.append(f"- {r['person']} (born {r['born']}): {r['club_3968']} to {r['club_xbox']}, joined "
                         f"{r['joined_xbox']}, contract to {r['cend_xbox']}. {r['decision'].capitalize()}.")
            else:
                L.append(f"- {r['person']}: unclear ({r['reason']}). Held.")
        if not hits:
            L.append(f"- {name}: not in the moves.")
    L += ["", "## Rows that need your decision", ""]
    held_rel = [r for r in rows["2-releases"] if r["decision"] == HELD]
    why = collections.Counter(r["rule"].split(" (")[0] for r in held_rel)
    L.append(f"1. Releases held: {len(held_rel)} ({', '.join(f'{v} {k}' for k, v in why.most_common())}). The "
             "default keeps the 3.9.68 club.")
    held_mv = [r for r in rows["1-moves"] if r["decision"] == HELD]
    why = collections.Counter(r["rule"].split(" (")[0].split(":")[0] for r in held_mv)
    L.append(f"2. Moves held: {len(held_mv)} ({', '.join(f'{v} {k}' for k, v in why.most_common()) or 'none'}).")
    lg = rows["5-leagues"]
    a_b = sum(1 for r in lg if r["division_3968"] != r["division_xbox"])
    L.append(f"3. League differences: {len(lg)} clubs. {a_b} differ between 3.9.68 and Xbox, the rest only against "
             "the 2002/03 club tables. Xbox against the club table: "
             + ", ".join(f"{v} {k}" for k, v in collections.Counter(r["xbox_vs_table"] for r in lg).most_common())
             + ". Answer per nation or per row: which division the club plays in. Most common patterns "
             "(3.9.68, Xbox, club table):")
    pat = collections.Counter((r["division_3968"], r["division_xbox"], r["table_division"]) for r in lg)
    L += [f"   - {n}: {a}, {b}, {c}" for (a, b, c), n in pat.most_common(12)]
    nm = [r for r in rows["6-names"] if r["decision"] == HELD]
    L.append(f"4. Names and competitions held: {len(nm)} (club names other than Fiorentina, clubs in one set only, "
             "other competition differences).")
    notmaj = sum(1 for r in rows["4-ratings"] if r["major"] == "no")
    L.append(f"5. Rating changes: {len(rows['4-ratings'])}, of them {notmaj} for players who are not major. The spec "
             "applies the Xbox rating changes (Q9), but research Q3 named major players only. Confirm that the "
             "changes of the other players apply too.")
    unc = rows["7-unclear"]
    why = collections.Counter(r["reason"] for r in unc)
    only_xbox = sum(1 for r in unc if "only on Xbox" in r["reason"] or r["reason"] == "nation differs")
    L.append(f"6. Unclear people: {len(unc)} rows ({', '.join(f'{v} {k}' for k, v in why.most_common())}). "
             f"{sum(1 for r in unc if r['differs'])} of them have another club or dates on Xbox. Not listed: "
             f"{sum(notes['unclear_same'].values())} people with an unknown date whose 3.9.68 namesake has the same "
             "club and dates. The default keeps the 3.9.68 record. One group answer is possible: apply the rows "
             "\"unknown date of birth, the name is once in each set with the same nation\". People only on Xbox, with "
             f"the one whose nation differs (the research also counted 26): {only_xbox} "
             "rows, all held.")
    L += ["", "## Duplicates (Q14)", "",
          f"Template pairs by the duplicates.py rules: {notes['dup_before']}. New pairs that the planned club changes "
          f"would make: {notes['dup_new']}. Those changes and the changes of rows in a template pair are held. New "
          f"pairs after the applied changes: {len(notes['dup_final_new'])}. No applied change adds a row.", "",
          "## Clubs", "",
          f"Club ids differ between the sets, so clubs map by name and nation. Mapped by squad: "
          + (", ".join(f"Xbox {x} to 3.9.68 {t} ({n} of {m} people)" for x, t, n, m in notes["squad_maps"]) or "none")
          + f". Clubs only in 3.9.68: {len(notes['only_t'])}. Only on Xbox: {len(notes['only_x'])}.",
          f"Club table rows of 2002/03: {notes['table_rows']}, of them {len(notes['table_no_id'])} with no club id "
          "(not compared).", "",
          "## Note for slice 3", "",
          "match.py, leagues.py and convert.py read format 2 only (cmdb.Folder). The changes are edits of existing "
          "rows, so slice 3 most likely patches the 3.9.68 format 1 files in place and adds no row. That needs:",
          "",
          "- a format 1 writer in cmdb1.py: the staff rows of 157 bytes and the player rows of 70 bytes, written "
          "back at the same offsets, with the bounds checks of the reader;",
          "- staff fields: club (+0x39, i32, the new_club_id column), join date (+0x3e) and contract end (+0x46) "
          "as day of year and year (cmdb1.to_date in reverse, day 365 stays as read);",
          "- player fields: current and potential ability (+5, +7), the three reputations (+9 home, +0x0b current, "
          "+0x0d world), 12 position bytes from +0x0f and 42 attribute bytes from +0x1b;",
          "- club.dat: the long name (+4, 51 bytes) and short name (+0x38, 26 bytes) of Fiorentina, the club lists "
          "(+0x9b to +0x1e0, as duplicates.CLUB_STAFF) of the old and the new club of each move and release, so "
          "that the squad lists agree with the staff rows (convert.check_squad_lists), and the division (+0x57) "
          "of each league decision;",
          "- club_comp.dat: the Xbox data drops the five unused rows \"Turkish 2. Division Group 1\" to \"Group 5\" "
          "(3.9.68 ids 168 to 172, no club plays in them) and adds the three groups at the end, so its competition "
          "ids after 167 are 5 lower. Slice 3 can give three of the unused 3.9.68 rows the names of the Xbox groups "
          "and point the clubs' +0x57 at them. Then no row is added or removed and index.dat keeps every count. "
          "First check that no other table points at rows 168 to 172 (history, rules, nation data). Slice 5 owns "
          "the game code for the groups.",
          "- index.dat: no count changes, as no row is added.", ""]
    return "\n".join(L)


def main(argv):
    need(len(argv) in (5, 6), "usage: change_list.py <facts-xbox> <facts-3968> <xbox data> <3.9.68 Data> <out> "
                              "[<rules folder>]")
    fxp, ftp, dx, dt, out = argv[:5]
    rules = argv[5] if len(argv) == 6 else RULES
    need(not os.path.abspath(out).startswith(REPO + os.sep), "the out folder must be outside the repo")
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
