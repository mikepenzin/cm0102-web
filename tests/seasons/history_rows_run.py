#!/usr/bin/env python3
"""The corrected competition history rows of issues 145 and 165 (docs/research/history-rows/).

Usage:
    python3 tests/seasons/history_rows_run.py <cm0102_recomp> <game folder> <folder of databases> [--jobs 2]
        [--keep <folder>] [--only <run>,...]

Each run is a new game with England and Greece, seed 1, played for 3 days to a save (played2026: through the World
Cup of 2026, which the game plays itself). The test reads the rows of the
save (tests/seasons/future/history_check.py), so a row that the game drops at the creation counts as missing. The
expected rows below are written here by hand from the sources of docs/research/history-rows/, apart from the table
that the game uses.

The runs: the databases that hold the reported faults, with changing rules on and off, a database with club hosts on
old editions, a 2001 game with changing rules on, and the base game, which takes no row (Q152).
"""
import concurrent.futures
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "future"))
sys.path.insert(0, os.path.join(HERE, "..", "smoke"))
import history_check  # noqa: E402
import soak_run  # noqa: E402

WC, EURO, LIB, CWC, GREEK = ("FIFA World Cup", "European Football Championship", "South American Copa Libertadores",
                             "European Cup Winners Cup", "Greek Super Cup")
EURO_NAMES = (EURO, "UEFA European Championship")      # the season names rename the record
ECL = ("UEFA Europa Conference League", "UEFA Conference League", "Conference League")
NL = ("Asian Cup Qualifying", "UEFA Nations League")
GREEK_OLD = {1980: ("Olymp", "Kastori"), 1987: ("Olymp", "OFI"), 1988: ("Panathinaikos", "Laris"),
             1989: ("AEK", "Panathinaikos"), 1992: ("Olymp", "AEK"), 1993: ("Panathinaikos", "AEK"),
             1994: ("Panathinaikos", "AEK"), 1996: ("AEK", "Panathinaikos")}
ECL_ROWS = {2022: ("Roma", "Feyenoord"), 2023: ("West Ham", "Fiorentina"), 2024: ("Olymp", "Fiorentina"),
            2025: ("Chelsea", "Betis"), 2026: ("Crystal Palace", "Rayo")}
NL_ROWS = {2019: ("Portugal", "Holland", "England", "Portugal"), 2021: ("France", "Spain", "Italy", "Italy"),
           2023: ("Spain", "Croatia", "Italy", "Holland"), 2025: ("Portugal", "Spain", "France", "Germany")}


def upto(rows, last):
    return {y: r for y, r in rows.items() if y <= last}


def greek(start):
    """The Greek Super Cup rows of a start year: each edition that ended before the game starts."""
    rows = dict(GREEK_OLD)
    if start >= 2008:
        rows[2007] = ("Olymp", "Laris")
    if start >= 2026:
        rows[2025] = ("Olymp", "OFI")
    if start >= 2027:
        rows[2026] = ("OFI", "AEK")
    return rows


# name: database (None: 3.9.68), start year, changing rules, the checks
RUNS = {
    "oct2026": ("October 2026", 2026, "on", {
        "hosts": {(WC, 2026): "United States", (WC, 2022): "Qatar", (EURO, 2024): "Germany", (EURO, 2020): "England"},
        "third": {(EURO, 2024): "", (EURO, 2020): "", (WC, 2026): "England"},
        "second": {(LIB, 2022): "Paranaense", (LIB, 2021): "Flamengo", (LIB, 2025): "Palmeiras"},
        "ecl": ECL_ROWS, "nl": NL_ROWS, "greek": greek(2026), "no_cwc_from": 2000, "no_club_hosts": True,
        "log": "Conference League holder Crystal Palace"}),
    "dec2025": ("December 2025", 2025, "on", {
        "hosts": {(EURO, 2024): "Germany", (EURO, 2020): "England"}, "third": {(EURO, 2024): ""},
        "second": {(LIB, 2022): "Paranaense"},
        "ecl": upto(ECL_ROWS, 2025), "nl": NL_ROWS, "greek": greek(2025), "no_cwc_from": 2000}),
    "oct2022": ("October 2022", 2022, "on", {
        "hosts": {(EURO, 2020): "England"}, "ecl": upto(ECL_ROWS, 2022), "nl": upto(NL_ROWS, 2021),
        "greek": greek(2022), "no_cwc_from": 2000}),
    "oct2021": ("October 2021", 2021, "on", {
        "hosts": {(EURO, 2020): "England"}, "ecl": {}, "nl": upto(NL_ROWS, 2021), "greek": greek(2021)}),
    "aut2013": ("Autumn 2013", 2013, "on", {
        "hosts": {(WC, 2010): "South Africa", (EURO, 2012): "Ukraine"}, "ecl": {}, "nl": {}, "greek": greek(2013),
        "no_club_hosts": True}),
    "off2026": ("October 2026", 2026, "off", {
        "hosts": {(WC, 2026): "United States", (EURO, 2020): "England"}, "second": {(LIB, 2022): "Paranaense"},
        "ecl": ECL_ROWS, "nl": NL_ROWS, "greek": greek(2026), "no_cwc_from": 2000}),
    # Data fixes of plan-fixes.tsv (docs/research/history-0.6.0/): one row a label, from its sources.
    "oct2016": ("October 2016", 2016, "on", {"nations": ["Germany", "Italy"], "rows": {
        ("DFB-Pokal", 2015): ("Wolfsburg", "Dortmund"), ("DFB-Pokal", 2016): ("Bayern", "Dortmund"),
        ("Lega Pro girone A", 2016): ("Cittadella", "Pordenone"), ("Lega Pro girone B", 2015): ("Ascoli", ""),
        ("Lega Pro girone B", 2016): ("Ars et Labor", "Pisa"), ("Lega Pro girone C", 2015): ("Salernitana", "Benevento"),
        ("Lega Pro girone C", 2016): ("Benevento", "Foggia")}}),
    "sep2012": ("September 2012", 2012, "on", {"rows": {
        ("English Isthmian League Premier Division", 2011): ("Sutton United", ""),
        ("English Isthmian League Premier Division", 2012): ("Billericay", "")}}),
    "sep2010": ("September 2010 ODB", 2010, "on", {"nations": ["Brazil"], "rows": {
        ("Campeonato Pernambucano", 2009): ("Sport Club do Recife", "Capibaribe"),
        ("Campeonato Paranaense", 2009): ("Atlético Paranaense", "")}}),
    "oct2015": ("October 2015", 2015, "on", {"nations": ["Italy"], "rows": {
        ("Lega Pro girone B", 2015): ("Ascoli", ""), ("Lega Pro girone C", 2015): ("Salernitana", "Benevento")}}),
    "oct2018": ("October 2018", 2018, "on", {"nations": ["Italy"], "rows": {
        ("Serie C girone B", 2015): ("Ascoli", ""), ("Serie C girone C", 2015): ("Salernitana", "Benevento"),
        ("Serie C girone A", 2016): ("Cittadella", "Pordenone"), ("Serie C girone B", 2016): ("SPAL", "Pisa"),
        ("Serie C girone C", 2016): ("Benevento", "Foggia"), ("Serie C girone A", 2017): ("Cremonese", "Alessandria"),
        ("Serie C girone B", 2017): ("Venezia", "Parma"), ("Serie C girone C", 2017): ("Foggia", "Lecce")}}),
    "oct2017": ("October 2017", 2017, "on", {"nations": ["Italy"], "rows": {
        ("Football at the Summer Olympics", 2012): ("Mexico", "Brazil"),
        ("Football at the Summer Olympics", 2016): ("Brazil", "Germany"),
        ("Serie C girone A", 2016): ("Cittadella", "Pordenone"), ("Serie C girone B", 2016): ("Ars et Labor", "Pisa"),
        ("Serie C girone C", 2016): ("Benevento", "Foggia"), ("Serie C girone A", 2017): ("Cremonese", "Alessandria"),
        ("Serie C girone B", 2017): ("Venezia", "Parma"), ("Serie C girone C", 2017): ("Foggia", "Lecce")}}),
    "oct2019": ("October 2019", 2019, "on", {"nations": ["Denmark", "Italy"], "nl": upto(NL_ROWS, 2019), "nl_europe": True, "rows": {
        ("Danish Cup", 2018): ("Bröndby", "Silkeborg"), ("Danish Cup", 2019): ("Midtjylland", "Bröndby"),
        ("Serie C girone B", 2019): ("Pordenone", "Triestina"), ("Serie C girone C", 2019): ("Juve Stabia", "Trapani"),
        ("Supercoppa Serie C", 2018): ("Padova", "Livorno"), ("Supercoppa Serie C", 2019): ("Pordenone", "Entella")}}),
    "nov2020": ("November 2020", 2020, "on", {"nations": ["Australia"], "rows": {("A-League", 2019): ("Sydney FC", "Perth Glory")}}),
    "oct2022": ("October 2022", 2022, "on", {"nations": ["Brazil"], "rows": {
        ("Campeonato Paulista Série A1", 2021): ("São Paulo", "Palmeiras"),
        ("Campeonato Baiano", 2019): ("Esporte Clube Bahia", "Bahia de Feira"),
        ("Campeonato Baiano", 2021): ("Alagoinhas", "Bahia de Feira")}}),
    "played2026": ("December 2025", 2025, "on", {"days": 400, "hosts": {(WC, 2026): "United States"}}),
    "r2001": (None, 2001, "on", {"ecl": {}, "nl": {}, "greek": greek(2001)}),
    "base": (None, 2001, "off", {"ecl": {}, "nl": {}, "greek": {}}),
}


def has(name, part):
    return part.lower() in name.lower() if part else not name


def check(save, want):
    game = history_check.Game(save)
    fails = []

    def rows_of(names):
        names = EURO_NAMES if EURO in names else names
        ids = {c for c, v in game.comps.items() if v["name"] in names}
        return [r for r in game.rows if r[0] in ids]

    def name(cid):
        return game.club(cid)[0]

    def one(comp, year):
        got = [r for r in rows_of((comp,)) if r[1] == year]
        if len(got) != 1:
            fails.append(f"{comp} {year}: {len(got)} rows")
        return got[0] if len(got) == 1 else None

    for field, at in (("second", 3), ("third", 4), ("hosts", 5)):
        for (comp, year), text in want.get(field, {}).items():
            r = one(comp, year)
            if r and not has(name(r[at]), text):
                fails.append(f"{comp} {year} {field}: {name(r[at])!r}, wanted {text!r}")
    for (comp, year), parts in want.get("rows", {}).items():
        r = one(comp, year)
        for at, text in enumerate(parts, 2):
            if r and not has(name(r[at]), text):
                fails.append(f"{comp} {year} place {at - 1}: {name(r[at])!r}, wanted {text!r}")
    for key, names in (("ecl", ECL), ("nl", NL), ("greek", (GREEK,))):
        if key not in want:
            continue
        got = {}
        for r in rows_of(names):
            if r[1] in got:
                fails.append(f"{names[0]} {r[1]}: two rows")
            got[r[1]] = r
        if sorted(got) != sorted(want[key]):
            fails.append(f"{names[0]}: years {sorted(got)}, wanted {sorted(want[key])}")
        for year, parts in want[key].items():
            for at, text in enumerate(parts, 2):
                if year in got and not has(name(got[year][at]), text):
                    fails.append(f"{names[0]} {year} place {at - 1}: {name(got[year][at])!r}, wanted {text!r}")
    if want.get("nl_europe"):
        confs = sorted({game.continents.get(v["continent"], "") for v in game.comps.values() if v["name"] in NL})
        if confs != ["UEFA"]:
            fails.append(f"{NL[0]} record continent: {confs}, wanted UEFA")
    if want.get("no_club_hosts"):
        bad = sorted({(game.comps[r[0]]["name"], r[1], name(r[5])) for r in game.rows
                      if r[2] in game.sides and r[5] >= 0 and r[5] not in game.sides})
        if bad:
            fails.append(f"a club as host: {bad[:6]}")
    if "no_cwc_from" in want:
        late = sorted(r[1] for r in rows_of((CWC,)) if r[1] >= want["no_cwc_from"])
        if late:
            fails.append(f"{CWC}: rows of {late}")
    return fails


def run(key, program, game, dbs, work):
    db, start, rules, want = RUNS[key]
    leg = os.path.join(work, key)
    o = {"program": program, "game": game, "rules": rules, "seed": 1, "start": start, "last": 2300, "extra": [],
         "database": os.path.join(dbs, db) if db else None, "nations": ["England", "Greece"] + want.get("nations", []), "timeout": 3600}
    code, took, box = soak_run.new_game_leg(o, leg, want.get("days", 3))
    save = os.path.join(leg, "user", "samesave.sav")
    fails = [] if code == 0 and not box else [f"the run ended with code {code}, box {box}"]
    fails += check(save, want) if os.path.exists(save) else ["no save"]
    if "log" in want:
        with open(os.path.join(leg, "stderr.txt"), encoding="latin-1") as f:
            if want["log"] not in f.read():
                fails.append(f"no log line {want['log']!r}")
    return key, fails, took


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2
    program, game, dbs = (os.path.abspath(a) for a in argv[1:4])
    jobs = int(argv[argv.index("--jobs") + 1]) if "--jobs" in argv else 2
    work = argv[argv.index("--keep") + 1] if "--keep" in argv else tempfile.mkdtemp(prefix="history-rows-")
    only = argv[argv.index("--only") + 1].split(",") if "--only" in argv else list(RUNS)
    failed = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        for f in concurrent.futures.as_completed([ex.submit(run, k, program, game, dbs, work) for k in only]):
            key, fails, took = f.result()
            print(f"{'ok  ' if not fails else 'FAIL'}  {key} ({took:.0f} s)" + "".join(f"\n      {x}" for x in fails),
                  flush=True)
            failed += bool(fails)
    print(f"logs: {work}")
    print("PASS" if not failed else f"FAILED: {failed} runs")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
