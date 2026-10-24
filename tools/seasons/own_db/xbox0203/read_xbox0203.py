"""Read the facts of a format 1 CM 01/02 data folder into pickles for the own_db converter.

Usage: python3 -I read_xbox0203.py <format 1 data folder> <facts folder>

The folder is the data of Championship Manager: Season 02/03 (Xbox, SI) or the 3.9.68 Data folder. Both hold the
staff table in format 1 (cmdb1.py). Nothing from the folder runs. The files are untrusted input: every read is
bounds-checked, and a folder that does not have the expected shape stops the script. Commit no Xbox data and
nothing made from it.

Output, in the shapes of read_fm2005.py and read_fm2007.py, so that match.py, leagues.py and convert.py take it:
- nations.pkl    {nation id: (name, second name)}
- clubs.pkl      {club id: dict(long, short, id2, off, nxt, nat)}. Clubs of club.dat and nat_club.dat. off is the
                 row, id2 is 0, nxt is the club's nation, nat is the nation id.
- people_all.pkl one dict per staff row in row order: fn, sn, cn (None for no common name), full (""), dob, nat,
                 team (the club id, -1 for none), job, joined, club (club id or None), wage (None), cend, pid (the
                 staff id), off (the row), role ("player" or "staff"). Dates are datetime.date or None.
                 Job uses FM's codes, as convert.py reads them: 1 for a player (and a player-manager), 16 manager,
                 20 assistant manager, 2 coach, 14 scout, 12 physio, 0 for a job that FM has no code for.
- teammap.pkl    {team id: club id}. CM keeps a reserve side as a club of its own, so a team is a club.
- clubhist.pkl   (competition, club, year, position, 0, 0, 0, 0, 0, 0, 0) for the first to fourth place of each
                 row of club_comp_history.dat. Years are as stored. Played, won, goals and points are not in the
                 file and are 0.
- comphist.pkl   (competition, year, (team 1 to 4, then 0xffffffff twice)) for each row of club_comp_history.dat
                 and nation_comp_history.dat. 0xffffffff is no team.
- ratings.pkl    {people_all index: dict(ca, pa, rep, row, pos, attrs)} for each player. pos is by FM's older
                 position names (keyplayers.FM_OLD) and attrs by FM attribute names (keyplayers.CM_ATTRS), so that
                 keyplayers.py reads them as it reads FM's. No loans.pkl or careers.pkl: CM data has no loans, and
                 careers are in the template.

Checks printed at the end: Henry (Arsenal, 181/189 on the Xbox data), Ronaldo (Real Madrid), Ferguson (Manchester
United) and Celestini (Olympique Marseille on the Xbox data).
"""
import collections
import os
import pickle
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OWN_DB = os.path.dirname(HERE)
sys.path.insert(0, OWN_DB)
import cmdb  # noqa: E402
import cmdb1  # noqa: E402
from cmdb1 import need, u  # noqa: E402
from keyplayers import CM_ATTRS, FM_OLD  # noqa: E402

NO_TEAM = 0xffffffff
FM_JOB = {5: 16, 6: 20, 8: 2, 9: 14, 10: 12}       # CM job byte (0x3d) -> FM job code; the reverse of convert.JOBS
FLAGS = {1, 2, 3}                                  # bit 1 non-player, bit 2 player


def names_of(F, tid):
    """{name id: text} of a name table (the id is at 0x33)."""
    out = {}
    for r in F.rows(tid):
        i = u("<i", r, 0x33)[0]
        need(i not in out, f"table {tid} has the name id {i} twice")
        out[i] = cmdb.cstr(r, 0, 51)
    return out


def table_ids(F, tid):
    """Row ids of a table whose rows start with their id, which must equal the row number."""
    ids = []
    for n, r in enumerate(F.rows(tid)):
        need(u("<i", r, 0)[0] == n, f"table {tid} row {n} has the id {u('<i', r, 0)[0]}")
        ids.append(n)
    return ids


def read_nations(F):
    out = {}
    for n, r in enumerate(F.rows(4)):
        need(u("<i", r, 0)[0] == n, f"nation row {n} has another id")
        out[n] = (cmdb.cstr(r, 4, 51), cmdb.cstr(r, 0x38, 26))
    return out


def read_clubs(F, nations):
    out = {}
    for tid in (0, 1):
        for n, r in enumerate(F.rows(tid)):
            cid, nat = u("<i", r, 0)[0], u("<i", r, 0x53)[0]
            need(cid not in out, f"club id {cid} is in the club tables twice")
            need(nat in nations or nat < 0, f"club {cid} has the unknown nation {nat}")
            out[cid] = dict(long=cmdb.cstr(r, 4, 51), short=cmdb.cstr(r, 0x38, 26), id2=0, off=n, nxt=nat, nat=nat)
    return out


def read_history(F, clubs):
    """(clubhist, comphist, dangling) from club_comp_history (19) and nation_comp_history (20)."""
    hist, comp, dangling = [], [], 0
    for tid in (19, 20):
        for r in F.rows(tid):
            cid, yr = u("<iH", r, 4)
            teams = u("<4i", r, 10)
            for t in teams:
                need(t >= -1, f"history row has the team {t}")
                dangling += t >= 0 and t not in clubs
            if tid == 19:
                hist += [(cid, t, yr, p + 1, 0, 0, 0, 0, 0, 0, 0) for p, t in enumerate(teams) if t >= 0]
            comp.append((cid, yr, tuple(t if t >= 0 else NO_TEAM for t in teams) + (NO_TEAM, NO_TEAM)))
    return hist, comp, dangling


def read_people(F, nations, clubs):
    fn, sn, cn = names_of(F, 13), names_of(F, 14), names_of(F, 15)
    n_player, n_non = F.table(10)[3], F.table(9)[3]
    people, ratings, links = [], {}, {}
    for n, r in enumerate(F.rows(6)):
        pid, a, b, c = u("<iiii", r, 0)
        need(pid == n, f"staff row {n} has the id {pid}")
        need(a in fn and b in sn, f"staff {pid} has the unknown name ids {a} and {b}")
        need(c < 0 or c in cn, f"staff {pid} has the unknown common name id {c}")
        nat = u("<i", r, 0x1a)[0]
        need(nat in nations or nat < 0, f"staff {pid} has the unknown nation {nat}")
        club = u("<i", r, 0x39)[0]
        need(club < 0 or club in clubs, f"staff {pid} has the unknown club {club}")
        flags, jobbyte = u("<B", r, 0x5f)[0], u("<B", r, 0x3d)[0]
        need(flags in FLAGS, f"staff {pid} has the flags {flags}")
        pl, npl = u("<i", r, 0x91)[0], u("<i", r, 0x99)[0]
        if flags & 2:
            need(0 <= pl < n_player, f"staff {pid} has the player row {pl} (table 10 has {n_player})")
        if flags & 1:
            need(0 <= npl < n_non, f"staff {pid} has the non-player row {npl} (table 9 has {n_non})")
        player = bool(flags & 2)
        job = 1 if player else FM_JOB.get(jobbyte, 0)
        people.append(dict(fn=fn[a], sn=sn[b], cn=cn[c] if c >= 0 else None, full="", dob=cmdb1.date_at(r, 0x10),
                           nat=nat, team=club, job=job, joined=cmdb1.date_at(r, 0x3e),
                           club=club if club >= 0 else None, wage=None, cend=cmdb1.date_at(r, 0x46), pid=pid,
                           off=n, role="player" if player else "staff"))
        if player:
            links[n] = pl
    prow = F.rows(10)
    for n, pl in links.items():
        p = prow[pl]
        ca, pa = u("<hh", p, 5)
        pos, att = bytes(p[0x0f:0x1b]), bytes(p[0x1b:0x1b + len(CM_ATTRS)])
        need(len(pos) == len(FM_OLD) and len(att) == len(CM_ATTRS), "player row is too short")
        ratings[n] = dict(ca=ca, pa=pa, rep=u("<hhh", p, 9), row=pl,
                          pos=dict(zip(FM_OLD, pos)), attrs={a: att[j] for j, a in enumerate(CM_ATTRS) if a})
    return people, ratings


def facts(path):
    F = cmdb1.open_folder(path)
    nations = read_nations(F)
    clubs = read_clubs(F, nations)
    people, ratings = read_people(F, nations, clubs)
    hist, comp, dangling = read_history(F, clubs)
    teammap = {c: c for c in clubs}
    return dict(nations=nations, clubs=clubs, people_all=people, teammap=teammap, clubhist=hist, comphist=comp,
                ratings=ratings), dangling


def show(out, name, first, second=None, year=None):
    """Print the people with these names (last word of the full name matches too)."""
    hits = [(i, x) for i, x in enumerate(out["people_all"])
            if cmdb.norm(x["sn"]) == cmdb.norm(second or first) and (second is None or cmdb.norm(x["fn"]) == cmdb.norm(first))
            or (second is None and cmdb.norm(x["cn"] or "") == cmdb.norm(first))]
    hits = [(i, x) for i, x in hits if year is None or (x["dob"] and x["dob"].year == year)]
    hits.sort(key=lambda t: -(out["ratings"].get(t[0]) or {}).get("ca", 0))
    for i, x in hits[:3]:
        club = out["clubs"][x["club"]]["long"] if x["club"] is not None else "-"
        r = out["ratings"].get(i)
        print(f"  {name}: {x['fn']} {x['sn']} born {x['dob']} club {club} job {x['job']} joined {x['joined']} "
              f"until {x['cend']}" + (f" ca/pa {r['ca']}/{r['pa']}" if r else ""))
    if not hits:
        print(f"  {name}: not found")


def main(argv):
    need(len(argv) == 2, "usage: read_xbox0203.py <format 1 data folder> <facts folder>")
    src, dst = argv
    out, dangling = facts(src)
    os.makedirs(dst, exist_ok=True)
    for n, v in out.items():
        with open(os.path.join(dst, n + ".pkl"), "wb") as f:
            pickle.dump(v, f)
    P = out["people_all"]
    print(f"nations {len(out['nations'])}, clubs {len(out['clubs'])}, people {len(P)} "
          f"({sum(x['club'] is not None for x in P)} with a club), players with ratings {len(out['ratings'])}, "
          f"table rows {len(out['clubhist'])}, competition winners {len(out['comphist'])}")
    print("history teams not in the club tables:", dangling)
    print("jobs", collections.Counter(x["job"] for x in P).most_common(10))
    yrs = [h[2] for h in out["clubhist"]]
    print("history years", min(yrs), "to", max(yrs))
    show(out, "Henry", "Thierry", "Henry", 1977)
    show(out, "Ronaldo", "Ronaldo")
    show(out, "Ferguson", "Alex", "Ferguson")
    show(out, "Celestini", "Fabio", "Celestini")


if __name__ == "__main__":
    main(sys.argv[1:])
