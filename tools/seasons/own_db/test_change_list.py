"""Cases for the Summer 2002 change list (xbox0203/change_list.py): the person match, the club map, the release
guard, the job and contract holds and the duplicate guard. Usage: python3 -I test_change_list.py

The people and clubs are synthetic (invented names). No game data is in the repo.
"""
import datetime
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "xbox0203"))
import change_list as cl  # noqa: E402

D = datetime.date


def person(fn, sn, dob, club, nat=0, role="player", job=1, joined=D(2001, 7, 1), cend=D(2004, 6, 30)):
    return dict(fn=fn, sn=sn, cn=None, dob=dob, nat=nat, club=club, role=role, job=job, joined=joined, cend=cend)


def club(name, nat=0):
    return dict(long=name, short=name, nat=nat)


def squad(names, born, c):
    return [person("Sq", n, D(born, 1 + i, 3), c) for i, n in enumerate(names)]


# The two "Twin FC" rows are the other way round on Xbox. "Old Name" is "New Name" in another row. "Small Town"
# keeps no squad on Xbox. "Ghost" is on Xbox only.
TC = {0: club("Alpha"), 1: club("Beta"), 2: club("Twin FC"), 3: club("Twin FC"), 4: club("Old Name"),
      5: club("Small Town"), 6: club("Delta")}
XC = {0: club("Alpha"), 1: club("Beta"), 2: club("Twin FC"), 3: club("Twin FC"), 5: club("New Name"),
      6: club("Small Town"), 7: club("Delta"), 8: club("Ghost")}
T = [
    person("Ann", "Mover", D(1980, 1, 5), 0),          # 0: moves Alpha to Beta, applied
    person("Bob", "Free", D(1981, 2, 6), 6),           # 1: released at Delta, which keeps its squad, applied
    person("Cid", "Twin", D(1982, 3, 7), None),        # 2 and 3: one name and date in two rows, unclear
    person("Cid", "Twin", D(1982, 3, 7), 1),
    person("Dan", "Nodate", None, 0),                  # 4: unknown date of birth, unclear
    person("Eve", "Small", D(1983, 4, 8), 5),          # 5: released at a club with no squad on Xbox, held
    person("Fay", "Nation", D(1984, 5, 9), 0),         # 6: another nation on Xbox, unclear
    person("Gus", "Same", D(1985, 6, 10), 1),          # 7: a Gus Same at Beta
    person("Gus", "Same", D(1986, 6, 1), 0),           # 8: moves to Beta, a year from 7: a new pair, held
    person("Ian", "Far", D(1980, 9, 9), 1),            # 9: an Ian Far at Beta
    person("Ian", "Far", D(1984, 10, 10), 0),          # 10: moves to Beta, 4 years from 9: no pair, applied
    person("Jo", "Pair", D(1979, 3, 3), 0),            # 11 and 12: a template pair (one day apart)
    person("Jo", "Pair", D(1979, 3, 4), 2),
    person("Ken", "Job", D(1970, 1, 20), 0, role="staff", job=2),   # 13: coach, a player on Xbox, held
    person("Lee", "Ghost", D(1975, 2, 2), 0),          # 14: moves to Ghost, not in 3.9.68, held
    person("Kid", "Keep", D(1988, 1, 1), 0),           # 15: same club, a new contract end, applied
    person("Max", "Wipe", D(1989, 1, 1), 0),           # 16: same club, no contract end on Xbox, held
    person("Ned", "Wipe", D(1990, 2, 2), 0),           # 17: moves to Beta, no contract end on Xbox, held
] + squad(("Oak", "Pine", "Elm", "Ash"), 1990, 2) + squad(("Rook", "Crow", "Wren", "Kite"), 1991, 4) + \
    squad(("Moss", "Fern", "Reed"), 1992, 6)
X = [
    person("Ann", "Mover", D(1980, 1, 5), 1, joined=D(2002, 7, 1)),
    person("Bob", "Free", D(1981, 2, 6), None),
    person("Cid", "Twin", D(1982, 3, 7), 0),
    person("Dan", "Nodate", None, 1),
    person("Eve", "Small", D(1983, 4, 8), None),
    person("Fay", "Nation", D(1984, 5, 9), 0, nat=1),
    person("Gus", "Same", D(1986, 6, 1), 1, joined=D(2002, 8, 1)),
    person("Hal", "New", D(1987, 7, 2), 1),            # 7: only on Xbox
    person("Gus", "Same", D(1985, 6, 10), 1),
    person("Ian", "Far", D(1980, 9, 9), 1),
    person("Ian", "Far", D(1984, 10, 10), 1),
    person("Jo", "Pair", D(1979, 3, 3), 1),
    person("Ken", "Job", D(1970, 1, 20), 1),
    person("Lee", "Ghost", D(1975, 2, 2), 8),
    person("Kid", "Keep", D(1988, 1, 1), 0, cend=D(2006, 6, 30)),
    person("Max", "Wipe", D(1989, 1, 1), 0, joined=D(2002, 7, 1), cend=None),
    person("Ned", "Wipe", D(1990, 2, 2), 1, cend=None),
] + squad(("Oak", "Pine", "Elm", "Ash"), 1990, 3) + squad(("Rook", "Crow", "Wren", "Kite"), 1991, 5) + \
    squad(("Moss", "Fern", "Reed"), 1992, 7)


def main():
    bad = []
    pairs, unclear = cl.match_people(T, X)
    want = {0: 0, 1: 1, 5: 4, 7: 8, 8: 6, 9: 9, 10: 10, 11: 11, 13: 12, 14: 13, 15: 14, 16: 15, 17: 16}
    for t, x in want.items():
        if pairs.get(t) != x:
            bad.append(("match", t, pairs.get(t), x))
    for t in (2, 3, 4, 6, 12):
        if t in pairs:
            bad.append(("an unclear or absent person matched", t))
    reasons = {r for r, _, _ in unclear}
    for r in ("shared name and date", "unknown date of birth, the name is once in each set with the same nation",
              "nation differs", "only on Xbox"):
        if r not in reasons:
            bad.append(("no unclear row", r))

    cmap, squads_mapped, only_t, only_x = cl.map_clubs(TC, XC, pairs, T, X)
    if cmap != {0: 0, 1: 1, 2: 3, 3: 2, 5: 4, 6: 5, 7: 6}:
        bad.append(("club map", cmap))
    if squads_mapped != [(5, 4, 4, 4)] or only_x != [8] or only_t:
        bad.append(("squad map or clubs left over", squads_mapped, only_t, only_x))

    cmap_t = {t: x for x, t in cmap.items()}
    tsq = {6: 4, 5: 1}
    for t, x, xsq, ok in ((1, 1, {7: 3}, True), (1, 1, {7: 1}, False), (5, 4, {}, False), (2, 2, {7: 3}, False)):
        got = cl.release_guard(t, x, pairs, T, xsq, tsq, cmap_t)[0]
        if got != ok:
            bad.append(("release guard", t, xsq, got))
    if cl.release_guard(1, 1, pairs, T, {7: 3}, tsq, {})[0]:
        bad.append(("release guard: a club that is not on Xbox",))

    ft = dict(people_all=T, clubs=TC, ratings={})
    fx = dict(people_all=X, clubs=XC, ratings={})
    rows, notes = cl.build(ft, fx, {c: 0 for c in TC}, {0: "League"}, {c: 0 for c in XC}, {0: "League"}, [])
    dec = {(k, r["id_3968"]): (r["decision"], r["rule"]) for k in ("1-moves", "2-releases", "3-dates")
           for r in rows[k]}
    cases = [(("1-moves", 0), cl.APPLIED, "clear match"), (("2-releases", 1), cl.APPLIED, "clear match"),
             (("2-releases", 5), cl.HELD, "the Xbox data keeps no squad"),
             (("1-moves", 8), cl.HELD, "the move makes a new duplicate pair"),
             (("1-moves", 10), cl.APPLIED, "clear match"),
             (("1-moves", 11), cl.HELD, "the 3.9.68 row is one of a template duplicate pair"),
             (("1-moves", 13), cl.HELD, "the job differs"), (("1-moves", 14), cl.HELD, "the Xbox club is not"),
             (("3-dates", 15), cl.APPLIED, "clear match, same club"), (("3-dates", 16), cl.HELD, cl.WIPED),
             (("1-moves", 17), cl.HELD, cl.WIPED)]
    for key, want_dec, why in cases:
        got = dec.get(key)
        if not got or got[0] != want_dec or not got[1].startswith(why):
            bad.append(("decision", key, got, want_dec, why))
    if [r["id_a"] for r in rows["8-template-pairs"]] != [2, 11] or notes["dup_rounds"] != [1]:
        bad.append(("template pairs or rounds", rows["8-template-pairs"], notes["dup_rounds"]))
    held = {r["person"] for r in rows["7-unclear"]}
    if not {"Cid Twin", "Dan Nodate", "Fay Nation", "Hal New"} <= held:
        bad.append(("unclear rows", held))
    names = [r for r in rows["6-names"] if r["item"] == "club name"]
    if [(r["id_3968"], r["decision"]) for r in names] != [(4, cl.HELD)]:
        bad.append(("name rows", names))
    if cl.real_division("Turkish 2. Division Category B (real: 2. Lig Category B, Group A)") != \
            "Turkish 2. Division Category B Group A" or cl.real_division("Ligue 1") != "French First Division":
        bad.append(("real_division",))
    if cl.outside_repo(cl.REPO) or cl.outside_repo(os.path.join(cl.REPO, "x")) or not cl.outside_repo("/tmp/x"):
        bad.append(("outside_repo",))
    for x in bad:
        print("FAIL", x)
    print("PASS" if not bad else f"FAILED: {len(bad)}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
