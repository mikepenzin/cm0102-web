#!/usr/bin/env python3
"""The holiday settings for offers (open item 132): "Reject all offers for players" and "Reject all offers below asking
price" (tools/recomp/original_faults.py, group holiday).

Usage:
    python3 tests/seasons/holiday_offers_run.py <cm0102_recomp> <game folder> --keep <folder> [--days 50]
        [--keep-faults]

Each run loads start.sav of the test fixtures (Arsenal, 14.7.01), opens "Go on holiday ?", sets the settings, clicks
Yes and saves --days game days later (tests/fixtures/fixtures.py play). RECOMP_TEST_LIST=odd puts every odd squad slot
of Arsenal on the transfer list when the holiday starts, so bids come for listed and unlisted players.
RECOMP_TRANSFER_LOG=1 logs each bid and loan reply for a human club and its result, and each completed move, with the
fee (offer +0x1b), the player's asking price (staff +0x52) and whether a release clause is in force
(src/recomp_rt/win/transfer_log.c). The runs:
    reject  "Reject all offers for players", "Do not buy any players" and "Use current match tactics", the settings
            of the player's report
    below   "Reject all offers below asking price"
The checks:
    - reject: no bid or loan request for an Arsenal player is accepted, no Arsenal player moves, and bids came
    - below: no bid for an Arsenal player below his asking price is accepted (a bid that meets a release clause is
      left out: 0x8cd240 accepts it before any setting, as a real club must)
    - below: the setting (the reject at 0x8c8266) rejects only bids below the asking price, so a listed and an
      unlisted player meet the same rule, and at least one such bid came and was rejected
    - below: at least one listed player was sold
    - no message box (RECOMP_SCREEN_LOG=1: "recomp: message box from"), and each run ends with code 0
The players of a game all have an asking price: none of 6,860 at the start, 6,842 a year later and 10,679 in a game
of 2026 has 0 (RECOMP_TEST_LIST=census). A player with 0 keeps the computer's answer.
--keep-faults runs with RECOMP_ORIGINAL_FAULTS=keep:holiday (the original's rule) and passes only when the below check
finds accepted bids below the asking price: the proof that it finds the fault (on 2e1eef3b, 25 such bids in 50 days).
The reject run passes on the original's rule too: the bid reply rejected every bid there (0x8c8266). With that rule
loan requests get the computer's answer, but in these 50 days no loan request comes for an Arsenal player.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FIXTURES = os.path.join(HERE, "..", "fixtures", "fixtures.py")

OPEN = ["2500 click 45 165", "4500 click 150 427"]     # the manager menu, Go on Holiday
YES = ["10000 click 705 572"]                            # Please Confirm: Yes
# Rows of the settings, 800x600: Reject all 305, below asking 327, do not buy 349, tactics 437.
RUNS = {
    "reject": OPEN + ["6000 click 250 305", "7000 click 250 349", "8000 click 250 437"] + YES,
    "below": OPEN + ["6000 click 250 327"] + YES,
}
LINE = re.compile(r"recomp: transfer log: ([\w-]+) day (\d+)/(\d+) from 0x([0-9a-f]+): player (\d+) from (.*?) to (.*?), "
                  r"flags 0x([0-9a-f]+), fee (-?\d+), asking (-?\d+), loan (\d+), state (\d+), clause (\d+), "
                  r"seller human (\d+)")
SETTING_REJECT = 0x8C8272     # the return address of the reject of the settings at 0x8c826d (0x8c8266..0x8c8272)


def play(program, game, keep, name, days, keep_faults):
    out = os.path.join(keep, name)
    os.makedirs(out, exist_ok=True)
    steps = os.path.join(out, "steps.txt")
    with open(steps, "w") as f:
        f.write("\n".join(RUNS[name]) + "\n")
    env = dict(os.environ, RECOMP_TRANSFER_LOG="1", RECOMP_TEST_LIST="odd", RECOMP_SCREEN_LOG="1")
    if keep_faults:
        env["RECOMP_ORIGINAL_FAULTS"] = "keep:holiday"
    r = subprocess.run([sys.executable, FIXTURES, "play", program, game, "start.sav", steps, out, "--days", str(days),
                        "--set", "faster_days=on"], env=env, capture_output=True, text=True)
    log = open(os.path.join(out, "stderr.txt"), errors="replace").read() if os.path.exists(
        os.path.join(out, "stderr.txt")) else ""
    return r.returncode, log


def rows(log, club="Arsenal"):
    out = []
    for m in LINE.finditer(log):
        if m.group(6) == club:
            out.append({"what": m.group(1), "site": int(m.group(4), 16), "player": int(m.group(5)), "buyer": m.group(7),
                        "listed": int(m.group(8), 16) & 1, "fee": int(m.group(9)), "ask": int(m.group(10)),
                        "loan": int(m.group(11)), "clause": int(m.group(13))})
    return out


def check(name, log):
    """The failures of one run, as (kind, text); kind "below" is the fault that --keep-faults must show."""
    fails = []
    r = rows(log)
    bids = [x for x in r if x["what"] == "respond"]
    accepts = [x for x in r if x["what"] in ("accept", "loan-accept")]
    moves = [x for x in r if x["what"] == "final"]
    rejects = [x for x in r if x["what"] in ("reject", "loan-reject")]
    loans = [x for x in r if x["what"] in ("loan-accept", "loan-reject")]
    listed = [x for x in moves if x["listed"]]
    print(f"{name}: {len(bids)} bids and {len(loans)} loan replies for Arsenal players, {len(accepts)} accepted, "
          f"{len(rejects)} rejected, {len(moves)} moved ({len(listed)} listed)")
    if not bids:
        fails.append(("run", f"{name}: no bid came for an Arsenal player (is RECOMP_TRANSFER_LOG in this build?)"))
    if name == "reject":
        for x in accepts + moves:
            fails.append(("reject", f"reject: player {x['player']} to {x['buyer']}: {x['what']} (fee {x['fee']}, "
                                    f"clause {x['clause']})"))
        return fails
    for x in accepts:
        if not x["loan"] and not x["clause"] and 0 < x["fee"] < x["ask"]:
            fails.append(("below", f"below: the bid of {x['buyer']} for player {x['player']} (listed {x['listed']}) "
                                   f"was accepted at {x['fee']}, below the asking price {x['ask']}"))
    for x in rejects:
        if x["site"] == SETTING_REJECT and x["fee"] >= x["ask"]:
            fails.append(("run", f"below: the setting rejected the bid of {x['buyer']} for player {x['player']} at "
                                 f"{x['fee']}, not below the asking price {x['ask']}"))
    if not [x for x in rejects if x["site"] == SETTING_REJECT and 0 < x["fee"] < x["ask"]]:
        fails.append(("below", "below: no bid below the asking price came and was rejected by the setting"))
    if not listed:
        fails.append(("run", "below: no listed player was sold"))
    return fails


def main(argv):
    if len(argv) < 3 or "--keep" not in argv:
        print(__doc__)
        return 2
    program, game = os.path.abspath(argv[1]), os.path.abspath(argv[2])
    keep = os.path.abspath(argv[argv.index("--keep") + 1])
    days = int(argv[argv.index("--days") + 1]) if "--days" in argv else 50
    keep_faults = "--keep-faults" in argv
    fails = []
    for name in RUNS:
        code, log = play(program, game, keep, name, days, keep_faults)
        if code != 0:
            fails.append(("run", f"{name}: the run ended with code {code}"))
        if "recomp: message box from" in log:
            fails.append(("run", f"{name}: a message box"))
        fails += check(name, log)
    for kind, text in fails:
        print("FAIL: " + text)
    if keep_faults:
        # The original's rule: the below check must find accepted bids below the asking price.
        found = any(kind == "below" for kind, _ in fails)
        print("PASS: the checks find the fault" if found else "FAIL: the checks do not find the fault")
        return 0 if found else 1
    print("PASS" if not fails else f"{len(fails)} failures")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
