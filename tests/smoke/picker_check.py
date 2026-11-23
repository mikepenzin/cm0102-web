#!/usr/bin/env python3
"""Picker check: the Assistant's picks from a saved game against the best possible assignment.

Usage:
    python3 tests/smoke/picker_check.py <cm0102_recomp> <game folder> <save>|new:<row> [--dir <fixtures folder>]
        [--expect SLOT=Name]... [--never SLOT=Name]... [--keep <file>]

The run copies the save (from the fixtures folder, tests/fixtures/fixtures.py) into a new empty user folder
and loads it. With new:<row> in place of a save, the run plays a new game (the smoke run's steps, England, 3.9.68)
with the club of that row of Select Team, for example new:1 for Barnsley. At the first human turn the runtime (RECOMP_PICK_CHECK, src/recomp_rt/win/picker.c) computes
the three picks (best, most match-fit, under-21) of the player's club without writing them, dumps the
candidates, the value of each candidate in each slot and the picks, then runs "Pick best team" and ends.

Checks for each pick:
    - the eleven and the subs have the best total of all assignments. The order of a total: fewest empty
      slots, then fewest players with a fit below 10, then fewest players from outside the pick's first
      group (kinds 2 and 3, Q142), then the highest score. This script finds the best total by its own method (a
      search over the slots taken), not the runtime's.
    - each fit and score equals this script's own sum from the dumped ratings and shown attributes. The
      score (Q138, Q139) is the average of the key shown attributes of the slot's position (KEYS) times
      fit / 20. The dump holds it as the sum of the key attributes x fit x 60 / their count, an integer;
      the table shows it divided by 1200.
    - no player with a fit below 10 plays while a free player, of any group, has 10 or more there (Q142)
    - no player is picked twice, and the subs come from the players left over
    - --expect DR=Warner: a slot of that label holds the player (full name or surname); --never: no such
      slot does. SAVE_CHECKS adds the checks of yeovil.sav and start.sav.
    - SAVE_ATTRS: one player's shown attributes, against a wrong attribute index in the runtime
    - no candidate is a virtual player: the dump's "staff" lines give each candidate's staff id and the staff
      count [0xae2364]. A virtual player (id at or above the count, VIRTUAL_STAFF 0x90d4f0) is a temporary
      row of a short squad: the squad screen shows him grey (0x45b984) and a match counts only real players
      (0x6dbaa1). On a new game as Barnsley (new:1, 14.7.01) 0.6.0 picked the virtual "Cooper, J" at ML, and
      Continue Game later said "You have only selected ten outfield players" (0.6.1 item 7).
--keep copies the dump to a file. It has one "cand" line per candidate: name, the position ratings, the 31
shown attributes, current ability, age, condition, first group, then the fit and the value of each slot.

Examples (the Yeovil save is a copy of Max's game; never use his own save):
    picker_check.py build/cm0102_recomp ../clean-3968 yeovil.sav
    picker_check.py build/cm0102_recomp ../clean-3968 start.sav
"""
import os
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fixtures"))
import fixtures  # noqa: E402
import smoke_run  # noqa: E402

ABLE = 10
KINDS = {1: "best", 2: "most match-fit", 3: "under-21"}

# The shown attributes in the order of the game's getter 0x5431e0 (1 to 31; names from 0x5428a0).
ATTRS = ["Acceleration", "Aggression", "Agility", "Anticipation", "Balance", "Bravery", "Creativity",
         "Crossing", "Decisions", "Determination", "Dribbling", "Finishing", "Flair", "Handling", "Heading",
         "Influence", "Jumping", "Long Shots", "Marking", "Off The Ball", "Pace", "Passing", "Positioning",
         "Reflexes", "Set Pieces", "Stamina", "Strength", "Tackling", "Teamwork", "Technique", "Work Rate"]
# The key attributes of each position (spec 2026-10-15-features-0.4.0, Q138 to Q140).
KEYS = {
    "GK": ["Handling", "Reflexes", "Agility", "Positioning", "Jumping"],
    "DC": ["Tackling", "Marking", "Heading", "Positioning", "Strength", "Jumping"],
    "DLR": ["Tackling", "Marking", "Pace", "Positioning", "Crossing"],
    "DMC": ["Tackling", "Positioning", "Passing", "Teamwork", "Work Rate"],
    "MC": ["Passing", "Decisions", "Teamwork", "Work Rate", "Technique"],
    "MLR": ["Crossing", "Dribbling", "Pace", "Passing", "Technique"],
    "AM": ["Passing", "Creativity", "Decisions", "Technique", "Flair"],
    "FC": ["Finishing", "Off The Ball", "Anticipation", "Technique", "Pace"],
}
# The checks of the best pick for each test save (Q137, Q141), added to --expect and --never.
SAVE_CHECKS = {
    "yeovil.sav": (["GK=Dean Williams", "AMC=Moukoko", "DR=Warner", "DC=Sodje", "FC=Georcelin", "FC=Alford"],
                   ["DMC=O'Brien"]),
    "start.sav": (["FC=Henry", "FC=Bergkamp"], []),
    "new:1": ([], ["ML=Cooper"]),     # Barnsley: the virtual "Cooper, J" of 0.6.0 (item 7)
}
# Shown attributes of one player for each test save: a wrong getter index fails here (Influence 16 and
# Jumping 17 differ for D. Williams).
SAVE_ATTRS = {"yeovil.sav": ("Dean Williams", {"Agility": 20, "Bravery": 20, "Determination": 20,
                                                "Influence": 14, "Jumping": 15})}
# Position bits of a slot mask and the index of their rating in the dumped ratings (player data +0x0f..).
POSITIONS = [(0x1, 0), (0x2, 1), (0x4, 2), (0x8, 3), (0x10, 4), (0x20, 5), (0x40, 6)]


def own_fit_value(cand, mask):
    """Fit and value of a candidate in a slot, computed here from the dump's ratings and attributes."""
    r = cand["ratings"]
    best_bit, pos = 0, 0
    for bit, i in POSITIONS:
        if mask & bit and r[i] > pos:
            best_bit, pos = bit, r[i]
    if not mask & 1 and mask & 0x880 and mask & 0xc and r[7] > pos:     # wing-back rating for D or DM wide
        best_bit, pos = (0x4 if mask & 0x4 else 0x8), r[7]
    side = 20
    if not mask & 1:
        side = r[9] if mask & 0x80 else r[8] if mask & 0x800 else r[10] if mask & 0x700 else 20
    fit = min(pos, side)
    if mask & 1 and r[0] < ABLE:
        return fit, -1
    wide = bool(mask & 0x880)
    role = {0x1: "GK", 0x2: "DC", 0x4: "DLR" if wide else "DC", 0x8: "DMC", 0x10: "MLR" if wide else "MC",
            0x20: "AM", 0x40: "FC"}.get(best_bit, "FC")
    keys = KEYS[role]
    return fit, sum(cand["attrs"][ATTRS.index(k)] for k in keys) * fit * 60 // len(keys)


def label(mask):
    """The position text of a slot mask, for example DMC or FC."""
    if mask & 1:
        return "GK"
    pos = "".join(t for bit, t in ((0x2, "SW"), (0x4, "D"), (0x8, "DM"), (0x10, "M"), (0x20, "AM"), (0x40, "F"))
                  if mask & bit)
    side = "L" if mask & 0x80 else "R" if mask & 0x800 else "C" if mask & 0x700 else ""
    return pos + side


def parse(path):
    """The dump: {kind: {"masks": [...], "cands": [...], "team": [...]}} and the picked count."""
    picks, cur, picked = {}, None, None
    for line in open(path, encoding="latin-1"):
        f = line.rstrip("\n").split("\t")
        if f[0] == "pick":
            cur = picks[int(f[1])] = {"masks": [int(m, 16) for m in f[2].split()], "cands": [], "team": []}
        elif f[0] == "cand":
            # cand, index, name, ratings (player data +0x0f to +0x19, hex), the 31 shown attributes, ability,
            # age, condition, first, fits..., values... (one per slot; -1: cannot)
            n = len(cur["masks"])
            nums = [int(x) for x in f[5:]]
            cur["cands"].append({"name": f[2], "ratings": [int(x, 16) for x in f[3].split()],
                                 "attrs": [int(x) for x in f[4].split()], "ability": nums[0], "age": nums[1], "condition": nums[2],
                                 "first": nums[3], "fit": nums[4:4 + n], "value": nums[4 + n:4 + 2 * n]})
        elif f[0] == "staff":
            # staff, candidate index, staff id, staff count
            cur["cands"][int(f[1])].update(id=int(f[2]), count=int(f[3]))
        elif f[0] == "team":
            cur["team"] = [int(x) for x in f[1].split()]
        elif f[0] == "picked":
            picked = int(f[1])
    return picks, picked


def key(cand, slot):
    """The order of one slot's filler, as a tuple (higher is better). None: the player cannot take it."""
    v = cand["value"][slot]
    if v < 0:
        return None
    return (0, -(cand["fit"][slot] < ABLE), -(1 - cand["first"]), v)


EMPTY = (-1, 0, 0, 0)


def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def best_total(cands, slots, used):
    """The best total of filling slots with the candidates not in used: a search over the subsets of
    slots taken after each candidate (dynamic programming), independent of the runtime's method."""
    n = len(slots)
    full = (1 << n) - 1
    best = {0: (0, 0, 0, 0)}
    for i, c in enumerate(cands):
        if i in used:
            continue
        nxt = dict(best)
        for taken, total in best.items():
            for k, s in enumerate(slots):
                if taken >> k & 1:
                    continue
                kk = key(c, s)
                if kk is None:
                    continue
                t = taken | 1 << k
                cand = add(total, kk)
                if t not in nxt or cand > nxt[t]:
                    nxt[t] = cand
        best = nxt
    result = None
    for taken, total in best.items():
        empties = n - bin(taken).count("1")
        t = add(total, tuple(e * empties for e in EMPTY))
        if result is None or t > result:
            result = t
    return result


def total(cands, team, slots):
    t = (0, 0, 0, 0)
    for s in slots:
        p = team[s]
        t = add(t, EMPTY if p < 0 else key(cands[p], s))
    return t


def check_pick(kind, pick, expect, never):
    masks, cands, team = pick["masks"], pick["cands"], pick["team"]
    eleven, subs = list(range(min(11, len(masks)))), list(range(11, len(masks)))
    problems = []
    for c in cands:
        for s, m in enumerate(masks):
            own = own_fit_value(c, m)
            if own != (c["fit"][s], c["value"][s]):
                problems.append(f"{c['name']} in {label(m)}: fit and value {c['fit'][s]}, {c['value'][s]}, "
                                f"expected {own[0]}, {own[1]}")
    if problems:
        return problems[:5]
    picked = [p for p in team if p >= 0]
    if len(picked) != len(set(picked)):
        problems.append("a player is picked twice")
    for s, p in enumerate(team):
        if p >= 0 and key(cands[p], s) is None:
            problems.append(f"{cands[p]['name']} cannot take slot {s} ({label(masks[s])})")
    if problems:
        return problems
    got, want = total(cands, team, eleven), best_total(cands, eleven, set())
    if got != want:
        problems.append(f"eleven total {got}, best possible {want}")
    in_eleven = {team[s] for s in eleven if team[s] >= 0}
    got, want = total(cands, team, subs), best_total(cands, subs, in_eleven)
    if got != want:
        problems.append(f"subs total {got}, best possible from the players left {want}")
    for s in eleven:
        p = team[s]
        if p < 0 or cands[p]["fit"][s] >= ABLE:
            continue
        for q, c in enumerate(cands):
            if q not in in_eleven and c["value"][s] >= 0 and c["fit"][s] >= ABLE:
                problems.append(f"{cands[p]['name']} plays {label(masks[s])} at fit {cands[p]['fit'][s]} "
                                f"while {c['name']} (fit {c['fit'][s]}) is free")
    if kind == 1:
        for rule, want_in in [(e, True) for e in expect] + [(n, False) for n in never]:
            slot_label, _, name = rule.partition("=")
            hit = any(team[s] >= 0 and label(masks[s]) == slot_label and
                      name in (cands[team[s]]["name"], cands[team[s]]["name"].split()[-1]) for s in eleven)
            if hit != want_in:
                problems.append(f"{name} {'not ' if want_in else ''}at {slot_label}")
    for c in cands:
        if "id" not in c:
            return problems + [f"{c['name']}: no staff id in the dump"]
        if c["id"] >= c["count"]:
            problems.append(f"{c['name']} is a virtual player (staff id {c['id']}, staff count {c['count']})")
    return problems


def show(kind, pick):
    masks, cands, team = pick["masks"], pick["cands"], pick["team"]
    print(f"\n{KINDS[kind]} ({len(cands)} candidates)")
    print(f"  {'slot':5} {'player':26} {'fit':>3} {'CA':>4} {'score':>6}")     # score: key average x fit / 20
    for s, p in enumerate(team):
        if s == 11:
            print("  subs")
        if p < 0:
            print(f"  {label(masks[s]):5} (empty)")
            continue
        c = cands[p]
        print(f"  {label(masks[s]):5} {c['name'][:26]:26} {c['fit'][s]:>3} {c['ability']:>4} {c['value'][s] / 1200:>6.1f}")


def main(argv):
    args = argv[1:]
    folder, keep, expect, never, rest = fixtures.DEFAULT_DIR, None, [], [], []
    i = 0
    while i < len(args):
        if args[i] in ("--dir", "--keep"):
            if args[i] == "--dir":
                folder = args[i + 1]
            else:
                keep = os.path.abspath(args[i + 1])
            i += 2
        elif args[i] in ("--expect", "--never"):
            (expect if args[i] == "--expect" else never).append(args[i + 1])
            i += 2
        else:
            rest.append(args[i])
            i += 1
    if len(rest) != 3:
        print(__doc__)
        return 2
    program, game, name = os.path.abspath(rest[0]), os.path.abspath(rest[1]), rest[2]
    expect += SAVE_CHECKS.get(name, ([], []))[0]
    never += SAVE_CHECKS.get(name, ([], []))[1]
    row = int(name[4:]) if name.startswith("new:") else None
    save = os.path.join(folder, name)
    if row is None and not os.path.exists(save):
        print(f"no saved game {save}")
        return 2
    with tempfile.TemporaryDirectory(prefix="cm0102-picker-") as work:
        user = os.path.join(work, "user")
        os.makedirs(user)
        if row is None:
            shutil.copyfile(save, os.path.join(user, name))
            load = fixtures.LOAD_STEPS
        else:
            load = [s.replace("click 155 163", f"click 155 {163 + 24 * row}")   # Select Team: rows 24 apart
                    for s in smoke_run.STEPS if "dump" not in s]
            load = load[:load.index("wait first-news") + 1]
        fixtures.write_settings(user, "classic", [])
        dump = os.path.join(work, "pick.txt")
        steps = smoke_run.scaled_steps("classic", work, load) + ["600000 quit"]
        r, took = fixtures.run(program, game, "classic", user, steps, {"RECOMP_PICK_CHECK": dump}, 900)
        if not os.path.exists(dump) or r.returncode != 0 or "picker check of club" not in r.stderr:
            print(r.stderr[-2000:])
            print(f"FAIL: no complete pick dump (code {r.returncode})")
            return 1
        picks, picked = parse(dump)
        if keep:
            shutil.copyfile(dump, keep)
    failed = 0
    for kind in sorted(picks):
        show(kind, picks[kind])
        for p in check_pick(kind, picks[kind], expect, never):
            print(f"  FAIL ({KINDS[kind]}): {p}")
            failed += 1
    if name in SAVE_ATTRS and 1 in picks:
        who, want = SAVE_ATTRS[name]
        c = next((c for c in picks[1]["cands"] if c["name"] == who), None)
        got = {k: c["attrs"][ATTRS.index(k)] for k in want} if c else None
        if got != want:
            print(f"FAIL: {who} shows {got}, expected {want}")
            failed += 1
    if sorted(picks) != [1, 2, 3]:
        print(f"FAIL: picks {sorted(picks)} in the dump")
        failed += 1
    if not picked:
        print(f"FAIL: Pick best team stored {picked} players")
        failed += 1
    print(f"\n{name}: {'FAIL' if failed else 'ok'} after {took:.0f} s")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
