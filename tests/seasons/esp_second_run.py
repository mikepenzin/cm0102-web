#!/usr/bin/env python3
"""Spain and Portugal: B and C sides newer than the key pairs of 0x540a50 (open items 121 and 122, 0.7.0;
season_esp_second.c).

Usage:
    python3 tests/seasons/esp_second_run.py <cm0102_recomp> <game folder> [--only esp-on,esp-off,esp-old,por-on,por-off,pool-on,pool-off]
        [--jobs n] [--keep <folder>]

Each run is a season jump of tests/seasons/rules_report_check.py with one nation and its third tier (RECOMP_TEST_EXTRA),
changing rules off, across the nation's season changes. RECOMP_TEST_ESP_SECOND_PUT moves clubs to rows of their own
tables at the first season change, before the original reads the tables (the statuses stay with the rows), so the
cases do not hang on match results. The report's club lines at each season start give the divisions.
    esp-on   April 2023, Spain, 2022 to 2023. Villarreal CF B (no key pair) plays the Segunda, Villarreal CF La Liga.
             The put gives Villarreal CF row 20 (relegation) and Villarreal CF B row 2 (promotion). With the fix:
             the season's table marks Villarreal CF B (the link at 0x855c17), it does not go up, and it goes down
             with Villarreal CF; La Liga and the Segunda keep their sizes
    esp-off  the same with RECOMP_ESP_SECOND=off (the original's pairs): Villarreal CF B goes up into La Liga, which
             proves that the test sees the fault
    esp-old  an old save made in memory: RECOMP_TEST_ESP_SECOND_OLD=1 plays as the original to the end of the first
             change (the put sends Villarreal CF B up beside Villarreal CF), 2022 to 2024. At the second change
             the put gives it row 1 of La Liga (so the table does not send it down), it goes down from La Liga, and
             La Liga and the Segunda keep their sizes
    por-on   May 2026, Portugal, 2025 to 2026. Vitória Guimarães SC B (no key pair) plays the third tier (Liga 3),
             Vitória Guimarães SC the Primeira Liga. The put gives Vitória row 18 (relegation) and its B side row 1
             of its table. With the fix the B side stays in the third tier while its club comes down to the
             Segunda Liga (0x7d0320 at 0x7d03b1), and the next club takes its place of the title
    por-off  the same with RECOMP_ESP_SECOND=off: the B side goes up into the Segunda Liga beside its club
    pool-on  the base game (3.9.68), Portugal, changing rules on, 2001 to 2002: the pool of the Second Division B
             of the database's zones (por_prm_exchange_second_b_with_lower 0x7cff70). RECOMP_TEST_ESP_SECOND_REP=1
             gives Sporting Clube Olhanense B (A Lower Division) the highest reputation; its club plays the Second
             Division B. With the fix it stays out of the pool and does not join its club's division
    pool-off the same with RECOMP_ESP_SECOND=off: Olhanense B goes up into the Second Division B
All runs end with code 0 and show no message box of the game.
"""
import concurrent.futures
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rules_report_check as rrc  # noqa: E402
import uefa_formats_run as ufr  # noqa: E402

DB = "/Users/max/Projects/cm0102/test-fixtures/databases"
LOG = {"RECOMP_ESP_SECOND_LOG": "1"}


def jump(name, program, game, work, db, nation, extra, year, last, env, options=None):
    code, _ = rrc.run(program, game, last, year, os.path.join(DB, db) if db else None, [nation], [extra], work, 10800,
                      options, dict(LOG, **env))
    fails = [] if code == 0 else [f"{name}: the jump ended with code {code}"]
    found, log = ufr.boxes(work)
    fails += [f"{name}: message box {b}" for b in found]
    return fails, log, read_report(os.path.join(work, "report.tsv"))


def read_report(path):
    """{season: {"div": {club name: division file}, "size": {division file: clubs}}} at each season start."""
    out, names = {}, {}
    if not os.path.exists(path):
        return out
    with open(path, encoding="cp1252", errors="replace") as f:
        rows = [line.rstrip("\n").split("\t") for line in f]
    for r in rows:
        if r[0] == "comp" and len(r) > 5 and r[2] == "start":
            names[(int(r[1]), r[4])] = r[5]
    for r in rows:
        if r[0] == "club" and len(r) > 5 and r[2] == "start":
            s = out.setdefault(int(r[1]), {"div": {}, "size": {}})
            file = names.get((int(r[1]), r[3]), r[3])
            s["div"][r[5]] = file
            s["size"][file] = s["size"].get(file, 0) + 1
    return out


def where(report, season, club):
    return report.get(season, {}).get("div", {}).get(club)


def sizes(report, season, files):
    return [report.get(season, {}).get("size", {}).get(f, 0) for f in files]


ESP = dict(db="April 2023", nation="Spain", extra="spain", year=2022)
POR = dict(db="May 2026", nation="Portugal", extra="portugal", year=2025)
ESP_PUT = {"RECOMP_TEST_ESP_SECOND_PUT": "Villarreal CF:20,Villarreal CF B:2"}
VITORIA, VITORIA_B = "Vitória Guimarães SC", "Vitória Guimarães SC B"


def game_text(text):
    """A name as the game holds it (Windows-1252 bytes), for an environment value."""
    return text.encode("cp1252").decode("utf-8", "surrogateescape")


POR_PUT = {"RECOMP_TEST_ESP_SECOND_PUT": game_text(f"{VITORIA}:18,{VITORIA_B}:1")}


def esp(name, program, game, work, off):
    env = dict(ESP_PUT, **({"RECOMP_ESP_SECOND": "off"} if off else {}))
    fails, log, rep = jump(name, program, game, work, last=2023, env=env, **ESP)
    if not re.search(r"esp second: test: Villarreal CF B takes row 2", log):
        fails.append(f"{name}: the put of Villarreal CF B was not made (no Spanish season change seen)")
    first, then = where(rep, 2022, "Villarreal CF B"), where(rep, 2023, "Villarreal CF B")
    senior = where(rep, 2023, "Villarreal CF")
    print(f"{name}: Villarreal CF B {first} to {then}, Villarreal CF {where(rep, 2022, 'Villarreal CF')} to {senior}; "
          f"sizes 2022 {sizes(rep, 2022, ['spa_first', 'spa_second'])}, 2023 {sizes(rep, 2023, ['spa_first', 'spa_second'])}")
    if first != "spa_second":
        fails.append(f"{name}: Villarreal CF B starts in {first}, not the Segunda (the case is not seen)")
    if off:
        if then != "spa_first":
            fails.append(f"{name}: Villarreal CF B did not go up with the switch off ({then}): the test does not see "
                         "the fault")
        return fails
    if "plays below Villarreal CF (call at 0x855c1c)" not in log:
        fails.append(f"{name}: SpaSecond::create_table found no club above Villarreal CF B (no 0xfc mark)")
    if senior != "spa_second":
        fails.append(f"{name}: Villarreal CF is in {senior} in 2023, not the Segunda (the put did not relegate it)")
    if then in ("spa_first", "spa_second"):
        fails.append(f"{name}: Villarreal CF B plays {then} in 2023 (it should go down with Villarreal CF)")
    if sizes(rep, 2023, ["spa_first", "spa_second"]) != sizes(rep, 2022, ["spa_first", "spa_second"]):
        fails.append(f"{name}: La Liga or the Segunda changed size")
    return fails


def esp_old(program, game, work):
    env = {"RECOMP_TEST_ESP_SECOND_PUT": "Villarreal CF B:2@1,Villarreal CF B:1@2", "RECOMP_TEST_ESP_SECOND_OLD": "1"}
    fails, log, rep = jump("esp-old", program, game, work, last=2024, env=env, **ESP)
    mid, end = where(rep, 2023, "Villarreal CF B"), where(rep, 2024, "Villarreal CF B")
    print(f"esp-old: Villarreal CF B 2023 {mid}, 2024 {end}; La Liga 2023 {sizes(rep, 2023, ['spa_first'])}, "
          f"2024 {sizes(rep, 2024, ['spa_first'])}; Villarreal CF 2023 {where(rep, 2023, 'Villarreal CF')}")
    if mid != "spa_first" or where(rep, 2023, "Villarreal CF") != "spa_first":
        fails.append(f"esp-old: no old save made (Villarreal CF B in {mid} in 2023, beside its club in La Liga)")
    if end == "spa_first":
        fails.append("esp-old: Villarreal CF B still plays La Liga in 2024")
    if "Villarreal CF B goes down from" not in log:
        fails.append("esp-old: no old-save move logged")
    if sizes(rep, 2024, ["spa_first", "spa_second"]) != sizes(rep, 2023, ["spa_first", "spa_second"]):
        fails.append(f"esp-old: La Liga or the Segunda changed size: 2023 {sizes(rep, 2023, ['spa_first', 'spa_second'])}"
                     f", 2024 {sizes(rep, 2024, ['spa_first', 'spa_second'])}")
    return fails


def por(name, program, game, work, off):
    env = dict(POR_PUT, **({"RECOMP_ESP_SECOND": "off"} if off else {}))
    fails, log, rep = jump(name, program, game, work, last=2026, env=env, **POR)
    if not re.search(r"esp second: test: Vit.{1,2}ria Guimar.{1,2}es SC B takes row 1", log):
        fails.append(f"{name}: the put of {VITORIA_B} was not made (no Portuguese season change seen)")
    first, then = where(rep, 2025, VITORIA_B), where(rep, 2026, VITORIA_B)
    senior = where(rep, 2026, VITORIA)
    print(f"{name}: B side {first} to {then}, club {where(rep, 2025, VITORIA)} to {senior}")
    if first != "por_second_b" or where(rep, 2025, VITORIA) != "por_prm":
        fails.append(f"{name}: the case is not seen (B side in {first}, club in {where(rep, 2025, VITORIA)})")
    if senior != "por_second":
        fails.append(f"{name}: the club plays {senior} in 2026, not the Segunda Liga (the put did not relegate it)")
    if off:
        if then != "por_second":
            fails.append(f"{name}: the B side did not go up with the switch off ({then}): the test does not see the "
                         "fault")
        return fails
    if then in ("por_prm", "por_second"):
        fails.append(f"{name}: the B side plays {then} in 2026 beside or above its club")
    if not re.search(r"esp second: Vit.{1,2}ria Guimar.{1,2}es SC B held status 5 of tier 3", log):
        fails.append(f"{name}: the title place of the B side was not handed on (the original holds it at 0x7d05a7 "
                     "with no club in its place)")
    return fails


OLHANENSE, OLHANENSE_B = "Sporting Clube Olhanense", "Sporting Clube Olhanense B"


def pool(name, program, game, work, off):
    env = {"RECOMP_TEST_ESP_SECOND_REP": "1", **({"RECOMP_ESP_SECOND": "off"} if off else {})}
    fails, log, rep = jump(name, program, game, work, None, "Portugal", "portugal", None, 2002, env,
                           {"changing_rules": "on"})
    first, then = where(rep, 2001, OLHANENSE_B), where(rep, 2002, OLHANENSE_B)
    club = where(rep, 2002, OLHANENSE)
    print(f"{name}: Olhanense B {first} to {then}, Olhanense {where(rep, 2001, OLHANENSE)} to {club}")
    if where(rep, 2001, OLHANENSE) != "por_second_b" or first == "por_second_b":
        fails.append(f"{name}: the case is not seen (Olhanense {where(rep, 2001, OLHANENSE)}, B side {first})")
    if off:
        if then != "por_second_b":
            fails.append(f"{name}: Olhanense B did not go up with the switch off ({then}): the test does not see the "
                         "fault")
        return fails
    if "Sporting Clube Olhanense B stays out of the Second Division B pool" not in log:
        fails.append(f"{name}: Olhanense B was not held out of the pool")
    if then == "por_second_b" and club == "por_second_b":
        fails.append(f"{name}: Olhanense B plays the Second Division B beside its club in 2002")
    return fails


RUNS = {"esp-on": lambda p, g, w: esp("esp-on", p, g, w, False),
        "esp-off": lambda p, g, w: esp("esp-off", p, g, w, True),
        "esp-old": esp_old,
        "por-on": lambda p, g, w: por("por-on", p, g, w, False),
        "por-off": lambda p, g, w: por("por-off", p, g, w, True),
        "pool-on": lambda p, g, w: pool("pool-on", p, g, w, False),
        "pool-off": lambda p, g, w: pool("pool-off", p, g, w, True)}


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    program, game = argv[1], argv[2]
    only = rrc.arg(argv, "--only", ",".join(RUNS)).split(",")
    jobs = int(rrc.arg(argv, "--jobs", "1"))
    keep = rrc.arg(argv, "--keep", None) or tempfile.mkdtemp(prefix="esp-second-")
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        futures = {n: ex.submit(RUNS[n], program, game, os.path.join(keep, n)) for n in only}
        fails = []
        for n in only:
            got = futures[n].result()
            print(f"{n}: {'PASS' if not got else 'FAIL'}")
            fails += got
    for f in fails:
        print("  " + f)
    print("PASS" if not fails else "FAIL")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
