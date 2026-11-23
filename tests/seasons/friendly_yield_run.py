#!/usr/bin/env python3
"""A qualifier outranks a friendly (0.6.2 item 208, season_friendly_dates.c, cancel_friendlies).

Usage:
    python3 tests/seasons/friendly_yield_run.py <cm0102_recomp> <game folder> --save <save> --database <folder>
        --keep <folder>
    python3 tests/seasons/friendly_yield_run.py --log <stderr.txt>      (check the log of an earlier run only)

The case: run set game a2021s1 (October 2021 database, 2021 start, changing rules on), the save of leg000
(samesave.sav, made in 2031), loaded Tier A and played from 2031 day 202 to 2032 day 200. The snap places the
game's friendlies on the FIFA days first, so before the fix a friendly held the FIFA day of a qualifier made later:
the 18 pairs of the CAF round with template day 170 of 2032 were planned for the FIFA day 156 (4 June) and went to
day 153, which is not a FIFA day, as each side had a game friendly on 156 (run-logs/0.6.2-s203/probe-lines.txt).
The original ranks a friendly below a competition match: add_fixture 0x5aa680 cancels a clashing friendly with
Friendly::cancel 0x5c6570 (at 0x5aa9d0). After the fix the snap does not count a friendly and cancels the
friendlies of the two nations within 2 days of the qualifier's day with 0x5c6570.

The checks, from the game's log (RECOMP_SCREEN_LOG names each move and each cancelled friendly):
- the run ends with code 0 and no message box;
- every CAF qualifier of template day 170 of 2032 moves to the FIFA day 156 of 2032 (at least one);
- at least one friendly is cancelled for a qualifier, each within 2 days of the qualifier's day.
Red on lead/merge-062ag b4966397 (the CAF pairs go to day 153, no friendly cancelled), green on the fix.
"""
import datetime
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))

CAF = re.compile(r"intl dates: FIFA World Cup qualification \(CAF\) (.+?) v (.+?) moves from day 170 to the (.+?) "
                 r"(\d+) of 2032")
CANCEL = re.compile(r"intl dates: the friendly (.+?) v (.+?) on day (\d+) of (\d+) is cancelled for the qualifier of "
                    r"(.+?) v (.+?) on day (\d+) of (\d+)$")


def day(y, d):
    return datetime.date(y, 1, 1) + datetime.timedelta(d)


def check(path):
    with open(path, encoding="latin-1") as f:
        lines = [x.rstrip("\n") for x in f]
    fails = []
    caf = [m for m in (CAF.search(x) for x in lines) if m]
    print(f"      CAF qualifiers of template day 170 of 2032: {len(caf)}")
    if not caf:
        fails.append("no CAF qualifier of template day 170 of 2032 moves")
    for m in caf:
        if m.group(3) != "FIFA day" or m.group(4) != "156":
            fails.append(f"CAF {m.group(1)} v {m.group(2)} goes to the {m.group(3)} {m.group(4)}, want the FIFA day 156")
    cancels = [m for m in (CANCEL.search(x) for x in lines) if m]
    print(f"      friendlies cancelled for a qualifier: {len(cancels)}")
    for m in cancels[:5]:
        print("      " + m.group(0))
    if not cancels:
        fails.append("no friendly cancelled for a qualifier")
    for m in cancels:
        fy, fd, qy, qd = int(m.group(4)), int(m.group(3)), int(m.group(8)), int(m.group(7))
        if abs((day(fy, fd) - day(qy, qd)).days) > 2:
            fails.append(f"friendly {m.group(1)} v {m.group(2)} on day {fd} of {fy} cancelled for a qualifier on day "
                         f"{qd} of {qy}, more than 2 days apart")
    boxes = [x for x in lines if "message box from" in x]
    if boxes:
        fails.append(f"message box: {boxes[0]}")
    return fails


def run(argv):
    import soak_run
    arg = lambda name: argv[argv.index(name) + 1]           # noqa: E731
    work = os.path.abspath(arg("--keep"))
    os.makedirs(work, exist_ok=True)
    for kv in ("RECOMP_SEASON_INTL_LOG=1", "RECOMP_SCREEN_LOG=1"):
        k, v = kv.split("=", 1)
        soak_run.LEG_ENV[k] = v
    os.environ["RECOMP_TIER_A"] = "1"
    o = {"program": os.path.abspath(argv[1]), "game": os.path.abspath(argv[2]), "start": 2021, "last": 2101,
         "seed": 1, "nations": ["all"], "database": os.path.abspath(arg("--database")), "rules": "on", "extra": [],
         "timeout": 21600}
    days = (2032 * 366 + 200) - (2031 * 366 + 202)
    code, took, box = soak_run.load_leg(o, work, os.path.abspath(arg("--save")), days)
    print(f"      run: code {code}, {round(took)} s, box {box}")
    fails = [] if code == 0 and not box else [f"run ended with code {code}, box {box}"]
    return fails + check(os.path.join(work, "stderr.txt"))


def main(argv):
    if "--log" in argv:
        fails = check(argv[argv.index("--log") + 1])
    elif len(argv) >= 3 and all(x in argv for x in ("--save", "--database", "--keep")):
        fails = run(argv)
    else:
        print(__doc__)
        return 2
    for f in fails:
        print("FAIL  " + f)
    print("PASS" if not fails else f"FAIL  {len(fails)} problems")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
