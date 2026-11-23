#!/usr/bin/env python3
"""The loans of a season database at the start of a new game (open item 229).

Usage:
    python3 tests/seasons/loans_start_run.py <cm0102_recomp> <game folder> <folder of databases> [--keep <folder>]
        [--jobs n]

<folder of databases> is test-fixtures/databases: it needs "October 2026" and "December 2025" as the Update Team
gives them (with player_setup.cfg). Each run starts a new game (England, Arsenal, changing rules on), plays one day
and saves. The loans are counted in contract.dat of the save (CONTRACT_MANAGER::load_from_disk 0x4db5c0: int n1, n1
rows of 0x15, int n2, n2 contracts of 0x50, +0 the staff index (the load writes it, 0x4db5c0), +4 the club id, +0x2d
and +0x2f the end day and year, +0x35 the type in its low 6 bits): a loan is a contract of type 6, next to the
player's main contract (type 2).

The checks:
    y2026       October 2026 in 2026/27: more than 100 loans; Jack Grealish has a contract with Manchester City and
                a loan with Everton that ends in 2027; the log line "recomp: player setup: the dates of" fires.
    y2026-orig  the same with RECOMP_PLAYER_SETUP_DATES=original: no loans (the fault of 0.7.0 and 0.7.1).
    y2025       December 2025 in 2025/26: more than 100 loans, Grealish on loan at Everton to 2026.
    y2001       October 2026 in 2001/02 (no season game): the same loans as y2001-orig, where the switch keeps the
                original; nothing moves (no log line).
    bans        y2026 against y2026-orig: the players of the database's DRUGS, MISCONDUCT and FALSE_PASSPORT lines that
                the game holds have another discipline row (discipline.dat, Discipline::save 0x555dd0: first one row
                of 0x1d bytes per staff) in at least one case, and only such players and loaned players differ there
                more than a handful (the bans of the file reach the game; before, the year check at 0x7a3d78 refused
                them).
    y2010sim    December 2010 SIM in 2010/11, a database with real years in its file: the same loans and discipline
                rows as y2010sim-orig (its 26 bans of 2010 do not move). Its LOAN lines end in 2011, which the
                parser refuses in both runs, so the loans are 0 in both; its retirements and future transfers are
                not checked here.
The ban check sees the bans of the players an England game holds (one in October 2026, a MISCONDUCT line), so it
does not test each ban type on its own.
At most --jobs (2) games run at the same time.
"""
import collections
import concurrent.futures
import os
import shutil
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tests", "smoke"))
import season_core_run as scr  # noqa: E402
import smoke_run  # noqa: E402

LOG = "recomp: player setup: the dates of"
LOAN = 6                                    # contract type of a loan (contract +0x35, low 6 bits)
failures = 0


def check(cond, text):
    global failures
    print(("ok    " if cond else "FAIL  ") + text, flush=True)
    if not cond:
        failures += 1


def find_db(dbs, name):
    """The folder of the database name under dbs: the folder itself or its one subfolder (a zip's folder)."""
    d = os.path.join(dbs, name)
    if os.path.exists(os.path.join(d, "staff.dat")):
        return d
    subs = [os.path.join(d, s) for s in os.listdir(d) if os.path.isdir(os.path.join(d, s))]
    return subs[0] if len(subs) == 1 else d


def loans(save):
    """(number of loans, {(staff index, name): [(club name, end year)]} of the players on loan)."""
    files = smoke_run.save_files(save)
    b = files["contract.dat"][0]
    n1 = struct.unpack_from("<i", b, 0)[0]
    o = 4 + n1 * 0x15
    n2 = struct.unpack_from("<i", b, o)[0]
    o += 4
    rows = [(struct.unpack_from("<i", b, o + i * 0x50)[0], struct.unpack_from("<i", b, o + i * 0x50 + 4)[0],
             struct.unpack_from("<H", b, o + i * 0x50 + 0x2f)[0], b[o + i * 0x50 + 0x35] & 0x3f) for i in range(n2)]
    per = collections.defaultdict(list)
    for staff, club, end, kind in rows:
        if staff >= 0:
            per[staff].append((club, end, kind))
    on_loan = {s: [(c, y) for c, y, _ in cs] for s, cs in per.items() if any(k == LOAN for _, _, k in cs)}

    def names(part):
        d = files[part][0]
        return {struct.unpack_from("<i", d, i + 0x33)[0]: d[i:i + 51].split(b"\0")[0].decode("latin-1")
                for i in range(0, len(d) - 59, 60)}
    first, second, common = names("first_names.dat"), names("second_names.dat"), names("common_names.dat")
    cd = files["club.dat"][0]
    clubs = {struct.unpack_from("<i", cd, i)[0]: cd[i + 4:i + 55].split(b"\0")[0].decode("latin-1")
             for i in range(0, len(cd) - 580, 581)}
    sd = files["staff.dat"][0]
    out = {}
    for s, cs in on_loan.items():
        f, sn, cm = struct.unpack_from("<iii", sd, s * 0x6e + 4)
        out[(s, common.get(cm) or f"{first.get(f, '?')} {second.get(sn, '?')}")] = [(clubs.get(c, c), y) for c, y in cs]
    return len(on_loan), out


def ban_players(cfg):
    """(first, second) names of the DRUGS, MISCONDUCT and FALSE_PASSPORT lines of a player_setup.cfg."""
    out = set()
    for line in open(cfg, encoding="latin-1"):
        f = line.split('"')
        if len(f) > 7 and f[1] in ("DRUGS", "MISCONDUCT", "FALSE_PASSPORT"):
            out.add((f[3], f[7]))
    return out


def staff_by_name(save):
    files = smoke_run.save_files(save)

    def names(part):
        d = files[part][0]
        return {struct.unpack_from("<i", d, i + 0x33)[0]: d[i:i + 51].split(b"\0")[0].decode("latin-1")
                for i in range(0, len(d) - 59, 60)}
    first, second = names("first_names.dat"), names("second_names.dat")
    sd = files["staff.dat"][0]
    out = {}
    for i in range(len(sd) // 0x6e):
        f, sn = struct.unpack_from("<ii", sd, i * 0x6e + 4)
        out.setdefault((first.get(f, ""), second.get(sn, "")), i)
    return out


def discipline_rows(save):
    """The per-staff rows of discipline.dat (0x1d bytes each, one per staff of staff.dat)."""
    files = smoke_run.save_files(save)
    d, n = files["discipline.dat"][0], len(files["staff.dat"][0]) // 0x6e
    return [d[i * 0x1d:(i + 1) * 0x1d] for i in range(n)]


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game, dbs = (os.path.abspath(a) for a in argv[:3])
    keep = os.path.abspath(argv[argv.index("--keep") + 1]) if "--keep" in argv else None
    jobs = int(argv[argv.index("--jobs") + 1]) if "--jobs" in argv else 2
    work = keep or tempfile.mkdtemp(prefix="cm0102-loans-")
    os.makedirs(work, exist_ok=True)
    oct26, dec25 = find_db(dbs, "October 2026"), find_db(dbs, "December 2025")
    sim10 = find_db(dbs, "December 2010 SIM")
    runs = [("y2026", 2026, oct26, {}), ("y2026-orig", 2026, oct26, {"RECOMP_PLAYER_SETUP_DATES": "original"}),
            ("y2025", 2025, dec25, {}), ("y2001", 2001, oct26, {}),
            ("y2001-orig", 2001, oct26, {"RECOMP_PLAYER_SETUP_DATES": "original"}),
            ("y2010sim", 2010, sim10, {}), ("y2010sim-orig", 2010, sim10, {"RECOMP_PLAYER_SETUP_DATES": "original"})]

    def one(r):
        name, year, db, env = r
        if os.path.exists(os.path.join(work, name)):
            shutil.rmtree(os.path.join(work, name))
        path, err, t = scr.play(program, game, work, name, {"changing_rules": "on"}, scr.new_game_steps(), 1,
                                env_extra={"RECOMP_START_SEASON": str(year), **env}, database=db)
        return name, path, err
    res, saves = {}, {}
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        for name, path, err in ex.map(one, runs):
            check(path is not None, f"{name}: the game starts and saves")
            res[name] = (loans(path) if path else (0, {}), err or "")
            saves[name] = path
    try:
        (n26, l26), e26 = res["y2026"]
        g = next((v for (_, n), v in l26.items() if n == "Jack Grealish"), [])
        check(n26 > 100, f"y2026: {n26} loans at the start of 2026/27")
        check(sorted(c for c, _ in g) == ["Everton", "Manchester City"] and ("Everton", 2027) in g,
              f"y2026: Jack Grealish has a contract with Manchester City and a loan at Everton to 2027 ({g})")
        check(LOG in e26, "y2026: the log line of the date move fires")
        (n26o, _), e26o = res["y2026-orig"]
        check(n26o == 0 and LOG not in e26o, f"y2026-orig: no loans with RECOMP_PLAYER_SETUP_DATES=original ({n26o})")
        (n25, l25), _ = res["y2025"]
        g = next((v for (_, n), v in l25.items() if n == "Jack Grealish"), [])
        check(n25 > 100 and ("Everton", 2026) in g, f"y2025: {n25} loans, Grealish at Everton to 2026 ({g})")
        (n01, l01), e01 = res["y2001"]
        (n01o, l01o), _ = res["y2001-orig"]
        check(n01 > 100 and l01 == l01o and LOG not in e01,
              f"y2001: a 2001 start keeps the original ({n01} loans, {n01o} with the switch, no log line)")
        banned = ban_players(os.path.join(oct26, "player_setup.cfg"))
        rows, rows_o = discipline_rows(saves["y2026"]), discipline_rows(saves["y2026-orig"])
        idx = staff_by_name(saves["y2026"])
        hit = sorted(n for n in banned if n in idx and rows[idx[n]] != rows_o[idx[n]])
        loaned = {s for s, _ in l26}
        other = [i for i in range(min(len(rows), len(rows_o))) if rows[i] != rows_o[i] and i not in loaned and
                 i not in {idx[n] for n in banned if n in idx}]
        check(len(hit) >= 1 and len(other) <= 5,
              f"bans: {len(hit)} of {sum(n in idx for n in banned)} banned players in the game have a new discipline "
              f"row ({hit[:5]}), {len(other)} other rows differ")
        (n10, l10), e10 = res["y2010sim"]
        (n10o, l10o), _ = res["y2010sim-orig"]
        same = discipline_rows(saves["y2010sim"]) == discipline_rows(saves["y2010sim-orig"])
        check(l10 == l10o and same, f"y2010sim: real years do not move ({n10} loans, {n10o} with the switch, "
              f"discipline rows {'equal' if same else 'differ'})")
    finally:
        if not keep:
            shutil.rmtree(work, ignore_errors=True)
    print(f"{failures} failure(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
