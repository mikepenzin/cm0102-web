"""Cases for the Summer 2002 change list (xbox0203/change_list.py): the person match, the club map, the release
guard and the duplicate guard. Usage: python3 -I test_change_list.py

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


def club(name, nat=0, short=None):
    return dict(long=name, short=short or name, nat=nat)


# 3.9.68 clubs 0 to 4. On Xbox the "Twin FC" rows are the other way round, "Old Name" is "New Name" with another
# row number, and "Small Town" keeps no squad.
TC = {0: club("Alpha"), 1: club("Beta"), 2: club("Twin FC"), 3: club("Twin FC"), 4: club("Old Name"),
      5: club("Small Town")}
XC = {0: club("Alpha"), 1: club("Beta"), 2: club("Twin FC"), 3: club("Twin FC"), 5: club("New Name"),
      6: club("Small Town")}
T = [
    person("Ann", "Mover", D(1980, 1, 5), 0),          # 0: moves Alpha to Beta
    person("Bob", "Free", D(1981, 2, 6), 0),           # 1: released, Alpha keeps a squad on Xbox
    person("Cid", "Twin", D(1982, 3, 7), None),        # 2 and 3: one name and date in two rows
    person("Cid", "Twin", D(1982, 3, 7), 1),
    person("Dan", "Nodate", None, 0),                  # 4: unknown date of birth
    person("Eve", "Small", D(1983, 4, 8), 5),          # 5: released at a club with no squad on Xbox
    person("Fay", "Nation", D(1984, 5, 9), 0, nat=0),  # 6: another nation on Xbox
    person("Gus", "Same", D(1985, 6, 10), 1),          # 7: second Gus Same at Beta, about a year older
    person("Gus", "Same", D(1986, 6, 1), 0),           # 8: moves to Beta: a new same-club pair, held
] + [person("Sq", n, D(1990, 1, 1 + i), 2) for i, n in enumerate(("Oak", "Pine", "Elm", "Ash"))] + [
    person("Sq", n, D(1991, 3, 1 + 9 * i), 4) for i, n in enumerate(("Rook", "Crow", "Wren", "Kite"))] + [
    person("Kid", "Keep", D(1988, 1, 1), 0)]           # 17: stays at Alpha
X = [
    person("Ann", "Mover", D(1980, 1, 5), 1, joined=D(2002, 7, 1)),
    person("Bob", "Free", D(1981, 2, 6), None),
    person("Cid", "Twin", D(1982, 3, 7), 0),
    person("Dan", "Nodate", None, 1),
    person("Eve", "Small", D(1983, 4, 8), None),
    person("Fay", "Nation", D(1984, 5, 9), 0, nat=1),
    person("Gus", "Same", D(1986, 6, 1), 1, joined=D(2002, 8, 1)),
    person("Hal", "New", D(1987, 7, 2), 1),            # only on Xbox
] + [person("Sq", n, D(1990, 1, 1 + i), 3) for i, n in enumerate(("Oak", "Pine", "Elm", "Ash"))] + [
    person("Sq", n, D(1991, 3, 1 + 9 * i), 5) for i, n in enumerate(("Rook", "Crow", "Wren", "Kite"))] + [
    person("Kid", "Keep", D(1988, 1, 1), 0), person("Gus", "Same", D(1985, 6, 10), 1)]


def main():
    bad = []
    pairs, unclear = cl.match_people(T, X)
    want = {0: 0, 1: 1, 5: 4, 8: 6, 7: 17}
    for t, x in want.items():
        if pairs.get(t) != x:
            bad.append(("match", t, pairs.get(t), x))
    for t in (2, 3, 4, 6):
        if t in pairs:
            bad.append(("an unclear person matched", t))
    reasons = {r for r, _, _ in unclear}
    for r in ("shared name and date", "unknown date of birth, the name is once in each set with the same nation",
              "nation differs", "only on Xbox"):
        if r not in reasons:
            bad.append(("no unclear row", r))

    cmap, squad, only_t, only_x = cl.map_clubs(TC, XC, pairs, T, X)
    if cmap != {0: 0, 1: 1, 2: 3, 3: 2, 5: 4, 6: 5}:
        bad.append(("club map", cmap))
    if squad != [(5, 4, 4, 4)] or only_x or only_t:
        bad.append(("squad map or clubs left over", squad, only_t, only_x))

    cmap_t = {t: x for x, t in cmap.items()}
    squads = {0: 2, 1: 3}
    for t, x, ok in ((1, 1, True), (5, 4, False), (2, 2, False)):
        got = cl.release_guard(t, x, pairs, T, X, squads, cmap_t)[0]
        if got != ok:
            bad.append(("release guard", t, got))
    if cl.release_guard(1, 1, pairs, T, X, squads, {})[0]:
        bad.append(("release guard: a club that is not on Xbox",))

    ft = dict(people_all=T, clubs=TC, ratings={})
    fx = dict(people_all=X, clubs=XC, ratings={})
    divs_t = {c: 0 for c in TC}
    divs_x = {c: 0 for c in XC}
    rows, notes = cl.build(ft, fx, divs_t, {0: "League"}, divs_x, {0: "League"}, [])
    dec = {(k, r["id_3968"]): r["decision"] for k in ("1-moves", "2-releases") for r in rows[k]}
    for key, want_dec in ((("1-moves", 0), cl.APPLIED), (("2-releases", 1), cl.APPLIED),
                          (("2-releases", 5), cl.HELD), (("1-moves", 8), cl.HELD)):
        if dec.get(key) != want_dec:
            bad.append(("decision", key, dec.get(key), want_dec))
    if notes["dup_final_new"]:
        bad.append(("an applied change makes a duplicate pair", notes["dup_final_new"]))
    held = {r["person"] for r in rows["7-unclear"]}
    if not {"Cid Twin", "Dan Nodate", "Fay Nation", "Hal New"} <= held:
        bad.append(("unclear rows", held))
    names = [r for r in rows["6-names"] if r["item"] == "club name"]
    if [(r["id_3968"], r["decision"]) for r in names] != [(4, cl.HELD)]:
        bad.append(("name rows", names))
    if cl.real_division("Turkish 2. Division Category B (real: 2. Lig Category B, Group A)") != \
            "Turkish 2. Division Category B Group A" or cl.real_division("Ligue 1") != "French First Division":
        bad.append(("real_division",))
    for x in bad:
        print("FAIL", x)
    print("PASS" if not bad else f"FAILED: {len(bad)}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
