#!/usr/bin/env python3
"""A save in the second half of a zone season of the Argentine Primera loads and completes the season (0.7.2 league
sizes, slice 15; spec docs/specs/2026-11-13-league-sizes-0.7.2-spec.md, slice 15 check 2).

Usage:
    python3 tests/seasons/arg_prm_save_run.py <cm0102_recomp> <game folder> [--keep <folder>] [--save-days n]
        [--play-days n] [--reuse]

The first program starts a 2024 game (the October 2024 database) with England and Argentina and changing rules on:
edition 2024 plays row 2024, the 2025 season of the rules table (Apertura and Clausura, each two zones of 15 with
the rival and drawn inter-zone rounds). It saves --save-days game days after the start (default 230: March 2025, in
the Clausura). A second program loads that save and plays --play-days (default 160) across the end of the season
and into edition 2025. The checks:
    first    the first run ends with code 0, no message box, the zone fixtures of both halves of 2024 in its log, and
             the save;
    load     the second run ends with code 0 and no message box and makes its save;
    played   at the start of edition 2025 (League::free_table 0x687970, RECOMP_SIZE_LOG) the table of 2024 holds 30
             clubs that each played 16 matches, so the Apertura table completed;
    next     edition 2025 makes its zones (row 2025) and both halves' fixtures after the load;
    champion the knockout winners of both halves of 2024 are the champions (gap row 48). --save-days 172 saves in the
             Apertura knockout (about 1 January 2025); give --play-days 220 then.
--promocion: a 2010 game (the September 2010 database) instead, saved between the two legs of the promoción of
edition 2010 (default --save-days 310, day 131 of 2011, between leg 1 on day 130 and leg 2 on day 134; --play-days 150). The checks: the first run played the
first legs and not the second; after the load both second legs are played, both ties have a result, and the
season change moves the losers and winners (the "chain" line), as league_size_run.py arg_promo_faults checks.
--reducido: a 2017 game (the October 2017 database) instead, saved during the Torneo Reducido of the second division
(item 291; default --save-days 279, day 109 of 2018, after the semi-finals are drawn and before the final; --play-days 120). The checks:
the first run drew round 2 and played no final; after the load the final is drawn and played, and the season change
sends its winner up (the "chain" line holds a club of the second division with status 0).
--zones: a 2024 game (the October 2024 database) instead, saved in the table of two zones of the second division (item 291;
default --save-days 150, --play-days 330). The checks: the first run made the zones and no Reducido; after the load the
zone winners' final and the Reducido are played, and exactly their two winners go up at the season change.
Exit code 1 on a failure. A run takes about an hour.
"""
import os
import re
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
sys.path.insert(0, os.path.join(HERE, "..", "fixtures"))


def faults_first(work):
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    fails = [x.strip() for x in log.split("\n") if "recomp: message box from" in x]
    for half in ("apertura", "clausura"):
        if not re.search(rf"recomp: league sizes: arg_prm fixtures 2024 {half}:( \|( \d+-\d+){{15}}){{16}}\s*$", log,
                         re.M):
            fails.append(f"first: no fixture line of 16 rounds of 15 matches for the {half} of 2024")
    if not os.path.exists(os.path.join(work, "user", "samesave.sav")):
        fails.append(f"first: no save ({work})")
    m = re.search(r"recomp: saved .* after (\d+) game days", log)
    print("first: " + (m.group(0) if m else "no save line"))
    return fails


def faults_load(again, code):
    log = open(os.path.join(again, "stderr.txt"), encoding="latin-1").read()
    fails = [x.strip() for x in log.split("\n") if "message box" in x]
    if code != 0:
        fails.append(f"load: the second run ended with code {code} ({again})")
    played = re.findall(r"recomp: league sizes: arg_prm played 2024: (\d+) clubs, (\d+) matches, played (\d+) to (\d+)",
                        log)
    if not played or played[-1] != ("30", "16", "16", "16"):
        fails.append(f"played: the 2024 table after the load: {played or 'no played line'}, want 30 clubs with 16 "
                     f"matches each")
    # Gap row 48: the knockouts of 2024 end after the load (a save in the Apertura knockout or in the Clausura), and
    # their winners are the champions of the halves.
    for half in ("apertura", "clausura"):
        if not re.search(rf"recomp: league sizes: arg_prm champion 2024 {half}: .*\(the knockout\)", log):
            fails.append(f"load: no champion of the {half} knockout of 2024 after the load")
    if not re.search(r"recomp: league sizes: arg_prm zones 2025: list 2025", log):
        fails.append("next: no zones of row 2025 (edition 2025) after the load")
    for half in ("apertura", "clausura"):
        if not re.search(rf"recomp: league sizes: arg_prm fixtures 2025 {half}:", log):
            fails.append(f"next: no {half} fixtures of 2025 after the load")
    return fails


def promo_first(work):
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    fails = [x.strip() for x in log.split("\n") if "recomp: message box from" in x]
    for st in (2, 3):
        if not re.search(rf"arg_prm knockout match 2010: stage {st} round \d+: \d+-\d+", log):
            fails.append(f"first: no first leg of promocion stage {st} before the save (save later)")
        if len(re.findall(rf"arg_prm knockout match 2010: stage {st} round \d+: \d+-\d+", log)) > 1:
            fails.append(f"first: both legs of promocion stage {st} before the save (save earlier)")
    return fails


def promo_load(work, code):
    import league_size_run as lsr
    fails = [] if code == 0 else [f"load: the run ended with code {code}"]
    path = os.path.join(work, "stderr.txt")
    if not os.path.exists(path):
        return fails + ["load: no log"]
    lines = open(path, encoding="latin-1").read().split("\n")
    fails += [x.strip() for x in lines if "recomp: message box from" in x]
    for st in (2, 3):
        if not any(re.search(rf"arg_prm knockout match 2010: stage {st} round \d+: \d+-\d+", x) for x in lines):
            fails.append(f"load: no second leg of promocion stage {st} after the load")
    if len([x for x in lines if re.search(r"arg_prm promocion 2010: tie \d+: winner", x)]) != 2:
        fails.append("load: not two promocion results of 2010 after the load")
    if not any(re.search(r"arg_prm chain 2010:", x) for x in lines):
        fails.append("load: no season change of 2010 after the load")
    return fails


def reducido_first(work):
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    fails = [x.strip() for x in log.split("\n") if "recomp: message box from" in x]
    if not re.search(r"arg_second reducido 2017: round 2 tie 2,", log):
        fails.append("first: the semi-finals of the Reducido are not drawn before the save (save later)")
    if re.search(r"arg_second reducido 2017: round 3 tie 1,", log):
        fails.append("first: the final of the Reducido is drawn before the save (save earlier)")
    return fails


def reducido_load(work, code):
    fails = [] if code == 0 else [f"load: the run ended with code {code}"]
    path = os.path.join(work, "stderr.txt")
    if not os.path.exists(path):
        return fails + ["load: no log"]
    log = open(path, encoding="latin-1").read()
    fails += [x.strip() for x in log.split("\n") if "recomp: message box from" in x]
    if not re.search(r"arg_second reducido 2017: round 3 tie 1,", log):
        fails.append("load: the final of the Reducido is not drawn after the load")
    won = re.search(r"arg_second reducido 2017: result 1 of club (\d+) in the final", log)
    if not won:
        fails.append("load: the final of the Reducido has no winner after the load")
    elif not re.search(rf"arg_prm chain 2017:.*second:.* {won.group(1)}:\d+:0\b", log):
        fails.append(f"load: the winner {won.group(1)} does not go up at the season change")
    return fails


def zones_first(work):
    log = open(os.path.join(work, "stderr.txt"), encoding="latin-1").read()
    fails = [x.strip() for x in log.split("\n") if "recomp: message box from" in x]
    if not re.search(r"arg_second zones 2024: 18 and 18 clubs, 34 rounds", log):
        fails.append("first: no zones of 2024")
    if re.search(r"arg_second reducido 2024: round 1 tie 1,", log):
        fails.append("first: the Reducido is drawn before the save (save earlier)")
    return fails


def zones_load(work, code):
    fails = [] if code == 0 else [f"load: the run ended with code {code}"]
    path = os.path.join(work, "stderr.txt")
    if not os.path.exists(path):
        return fails + ["load: no log"]
    log = open(path, encoding="latin-1").read()
    fails += [x.strip() for x in log.split("\n") if "recomp: message box from" in x]
    won = re.findall(r"arg_second reducido 2024: result 1 of club (\d+) in the final", log)
    if len(won) != 2:
        fails.append(f"load: {len(won)} winners of the final and the Reducido after the load, want 2")
    m = re.search(r"arg_prm chain 2024:.*second:(.*)", log)
    up = sorted(x.split(":")[0] for x in m.group(1).split() if x.split(":")[2] in ("0", "5")) if m else []
    if sorted(won) != up:
        fails.append(f"load: the clubs that go up are {up}, the winners {sorted(won)}")
    return fails


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    import rules_report_check as rrc
    import club_world_check as cwc
    program, game = os.path.abspath(argv[0]), os.path.abspath(argv[1])
    keep = rrc.arg(argv, "--keep", None)
    promo = "--promocion" in argv
    reducido = "--reducido" in argv
    zones = "--zones" in argv
    save_days = int(rrc.arg(argv, "--save-days", "150" if zones else "279" if reducido else "310" if promo else "230"))
    play_days = int(rrc.arg(argv, "--play-days", "330" if zones else "120" if reducido else "150" if promo else "160"))
    year = 2017 if reducido else 2010 if promo else 2024
    work = os.path.abspath(keep) if keep else tempfile.mkdtemp(prefix="cm0102-arg-prm-save-")
    again = os.path.join(work, "again")
    os.environ["RECOMP_SIZE_LOG"] = "1"     # the load run inherits it (fixtures.run)
    if "--reuse" not in argv:
        db = rrc.run_database([], year, os.path.join(work, "owndb"))
        code, _ = rrc.run(program, game, year + 1, year, db, ["England", "Argentina"], [], work, 7200,
                          {"changing_rules": "on"}, {"RECOMP_SIZE_LOG": "1", "RECOMP_SAVE_AFTER_DAYS": str(save_days)})
        if code != 0 and not os.path.exists(os.path.join(work, "user", "samesave.sav")):
            print(f"FAIL first: the run ended with code {code} and no save ({work})")
            return 1
    fails = (zones_first(work) if zones else reducido_first(work) if reducido else promo_first(work) if promo
             else faults_first(work))
    if not fails:
        if "--reuse" not in argv:
            os.makedirs(again, exist_ok=True)
            shutil.copyfile(os.path.join(work, "user", "samesave.sav"), os.path.join(again, "arg.sav"))
            r, saved = cwc.load_and_play(program, game, os.path.join(again, "arg.sav"), again, play_days, timeout=5400)
            code = r.returncode if saved else (r.returncode or 1)
        else:
            code = 0
        fails += (zones_load(again, code) if zones else reducido_load(again, code) if reducido
                  else promo_load(again, code) if promo else faults_load(again, code))
    for f in fails:
        print("FAIL", f)
    what = ("in the zones of 2024" if zones else "in the Reducido of 2017" if reducido else "between the promocion legs of 2010" if promo
            else "in the Clausura of 2024")
    print(("FAIL" if fails else "PASS") + f" arg_prm save {what} ({work})")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
