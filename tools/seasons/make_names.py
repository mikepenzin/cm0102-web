#!/usr/bin/env python3
"""Makes the names table of the game from the rules table (0.6.0 slice 15, Q226, Q238).

Usage:
    python3 -I tools/seasons/make_names.py --out <names.tsv>     write the table for the game
    python3 -I tools/seasons/make_names.py --check               read the parts, write nothing
    python3 -I tools/seasons/make_names.py --list                print every shown name, for a check by eye

Input: the `name` column of the rules table parts (tools/seasons/make_rules.py reads them).

Output: Seasons/names.tsv of the download, Windows-1252 (the text form of the game's data files), with tabs.
Line 1 is "#bosman-names<TAB>2<TAB><row count>", line 2 the column names, then one row per line:
    season   the start year (2001 for 2001/02)
    file     a source file of the original's class without ".cpp" (eng_first)
    n        0, 1, ...: the name of the n-th competition object of that file in the game (the groups of
             ger_regional, for example). An object after the last n takes the last name.
    long     the long name (51 characters at most: the record holds 52 bytes at +4)
    short    the short name (26 characters at most: 27 bytes at +56)
    abbr     the abbreviation (3 characters at most: 4 bytes at +0x53), which Select Team shows for the
             division of a club (0.6.1 item 2). Empty for a row that is not a league of a nation (is_league):
             a cup has no abbreviation in the data either.
The game gives each competition object the names of its file and season when a game starts in that season
(src/recomp_rt/win/season_names.c). Slice 16 gives the names of each later season through the same table.

The rows of a file in a season are those of tests/seasons/rules_report_check.py pick_row: the rows whose
comp_file is that file alone first, then rows of two files; of those the rows that are not a qualifying path;
the k-th object takes the k-th of these rows. A row whose name is empty, n/a or unknown gives no name, and
rows of comp_file new or na give none (no class of the original runs them).

The shown name (shown_name): the research name with these changes, so that the screens show a plain name:
    - a part in brackets at the end goes, unless it names a confederation (UEFA, CAF, AFC, CONCACAF, CONMEBOL,
      OFC) or a play-off: "Primera División (Liga BBVA)" gives "Primera División", "Intercontinental Cup
      (Toyota Cup)" gives "Intercontinental Cup";
    - the year of an edition goes: "2012 Africa Cup of Nations" gives "Africa Cup of Nations". The object of a
      competition can wait for another edition than the row's (the "idle" kind of the rules report), so a year
      in a name could be wrong. "UEFA Euro 2012" gives "UEFA European Championship";
    - a name in quotes at the end goes: 'Primera División "Julio H. Grondona"' gives "Primera División";
    - two editions in one row ("2020-21 Finals; 2022-23 UEFA Nations League (start)") give the later one;
    - a name over 50 characters (the limit of slice 16's season_change_rename) loses its part in brackets
      first ("FIFA World Cup qualification (inter-confederation play-offs)" gives "FIFA World Cup
      inter-confederation play-offs"), then it is cut after the last whole word that fits, never inside a
      bracket.
The stop rule also stops the tool on a long name over 50 or a short name over 26 characters, and on a row that
the game's loader rejects (a season outside 2001 to 2099, an empty file name, a number over 999, an empty long
or short name, a tab in a name, a long or short name that Windows-1252 cannot hold): the loader then drops the
whole file.
The short name is the shown name when it has 26 characters or fewer; else shorter words of SHORTER come in
("FIFA World Cup qualification (UEFA)" gives "WC qualifying (UEFA)"); else a known opening goes
("Campeonato Brasileiro Série A" gives "Série A", "League of Ireland Premier Division" gives "Premier
Division"); else the words that fit.
The abbreviation (abbreviation): a name of ABBREVIATIONS takes its entry ("Championship" gives "CH"). Else the
words of the short name after a nation's adjective ("Danish ") or an opening of ABBR_OPENINGS give one sign each: a
number word or an ordinal its digit ("First", "Prima", "1st", "One", "3." give 1), a single capital or a word
in capitals itself ("A", "CFA", "C1"), a small word ("de", "girone") none, another word its first letter in
capitals. A digit before a word of TIER_WORDS goes after its letter, as the data writes it ("First Division"
gives "D1", "Segunda División B" gives "D2B", "Primeira Liga" gives "L1"). A one-word name gives its first
three letters ("Eredivisie" gives "ERE"). Over 3 signs: the first two and the last.

The stop rule: a name that Windows-1252 cannot hold stops the tool with exit code 1.
"""
import argparse
import collections
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_rules  # noqa: E402

FORMAT = 2          # 0.6.1 item 2: the abbreviation column
LONG = 50           # slice 16 (season_change_rename) takes 50 characters; the record holds 52 bytes
SHORT = 26
KEEP_BRACKETS = re.compile(r"^(UEFA|CAF|AFC|CONCACAF|CONMEBOL|OFC)$|play-off", re.I)
# Shorter words for a short name, tried in order until the name fits.
SHORTER = [("FIFA World Cup inter-confederation play-offs", "WC play-offs"),
           ("FIFA World Cup qualification", "WC qualifying"),
           ("European Championship qualifying", "Euro qualifying"),
           ("Football at the Summer Olympics", "Olympic Games"),
           ("Supercoppa di Lega di ", "Supercoppa "), ("Supertaça Cândido de Oliveira", "Supertaça"),
           (' "Julio H. Grondona"', "")]
# Openings that a short name drops.
OPENINGS = ["Campeonato Brasileiro ", "Campeonato Nacional de ", "Campeonato ", "League of Ireland ",
            "Northern Ireland Football League ", "Football League ", "Russian Football ", "Irish League ",
            "Lega Pro ", "Belgian ", "J.League ", "FIFA ", "UEFA ", "AFC ", "CAF ", "CONCACAF ", "CONMEBOL ",
            "OFC "]


ABBR = 3
# Names whose usual abbreviation the rule below does not give.
ABBREVIATIONS = {"Championship": "CH", "Football League Championship": "CH", "Bundesliga": "BL",
                 "2. Bundesliga": "2BL", "3. Liga": "3L", "Superliga": "SL", "Danish Superliga": "SL",
                 "J.League Division 1": "J1", "J.League Division 2": "J2", "J1 League": "J1", "J2 League": "J2",
                 "J3 League": "J3", "CFA 2": "CF2", "Football League": "FL", "Football Conference": "CON",
                 "Conference National": "CON", "Conference Premier": "CON"}
NUMBERS = {"first": "1", "1st": "1", "one": "1", "prima": "1", "primera": "1", "primeira": "1", "eerste": "1",
           "prva": "1", "i": "1", "second": "2", "2nd": "2", "two": "2", "seconda": "2", "segunda": "2",
           "druga": "2", "tweede": "2", "ii": "2", "third": "3", "3rd": "3", "three": "3", "terza": "3",
           "tercera": "3", "terceira": "3", "iii": "3", "fourth": "4", "4th": "4", "four": "4", "fifth": "5",
           "5th": "5", "five": "5"}
# Openings that an abbreviation drops ("Football League One" gives "L1", "FA Premier League" gives "PL").
ABBR_OPENINGS = ["Northern Ireland Football League ", "Russian Football ", "Football ", "FA ", "EFL ", "TFF ",
                 "Irish League ", "League of Ireland "]
SMALL = {"de", "di", "da", "do", "del", "della", "of", "the", "la", "le", "e", "and", "girone", "group",
         "grupo", "série", "serie"}
TIER_WORDS = {"division", "división", "divisione", "divisão", "divisie", "divisions", "liga", "league", "ligue",
              "divisió"}
ADJECTIVES = ("Belgian", "Danish", "English", "French", "German", "Dutch", "Italian", "Scottish", "Swedish",
              "Portuguese", "Spanish", "Argentine", "Brazilian", "Japanese", "Finnish", "Irish", "Polish",
              "Croatian", "Northern Irish", "Greek", "Turkish", "Russian", "Welsh", "Chinese", "Czech",
              "Yugoslav", "Mexican", "Austrian", "Swiss", "Norwegian", "Korean", "Australian", "Indonesian",
              "Malaysian", "Indian", "Thai", "Singaporean", "Hong Kong", "Luxembourg", "South African")


def abbreviation(short):
    """The abbreviation of a league for its short name (at most ABBR characters)."""
    if short in ABBREVIATIONS:
        return ABBREVIATIONS[short]
    t = short
    for a in ADJECTIVES:
        if t.startswith(a + " ") and len(t) > len(a) + 1:
            t = t[len(a) + 1:]
            break
    for o in ABBR_OPENINGS:
        if t.startswith(o) and len(t) > len(o):
            t = t[len(o):]
            break
    if t in ABBREVIATIONS:
        return ABBREVIATIONS[t]
    words = [w for w in re.split(r"[ \-/]+", t) if w]
    signs = []                                         # (sign, word is a tier word)
    for w in words:
        low = w.lower().rstrip(".")
        if low in NUMBERS:
            signs.append((NUMBERS[low], False))
        elif low.isdigit():
            signs.append((low, False))
        elif low in SMALL:
            continue
        elif w.isupper() and w.isalnum():              # "HNL", "C1"
            signs.append((w, False))
        else:
            signs.append((w[0].upper(), low in TIER_WORDS))
    if len(signs) == 1 and len(words) == 1 and not signs[0][0].isdigit() and len(signs[0][0]) == 1:
        return t[:ABBR].upper()
    for i in range(len(signs) - 1):                    # "1 D" gives "D1", as the data writes it
        if signs[i][0].isdigit() and signs[i + 1][1]:
            signs[i], signs[i + 1] = signs[i + 1], signs[i]
    text = "".join(s for s, _ in signs) or t[:ABBR].upper()
    return text if len(text) <= ABBR else text[:ABBR - 1] + text[-1]


CONFEDERATIONS = {"UEFA", "CONMEBOL", "AFC", "CONCACAF", "OFC", "CAF", "FIFA"}


def is_league(row):
    """A league of a nation: a tier of 1 or more (0 is a cup or the rules of a nation) and a nation that is not a
    confederation (the tier of a continental cup is its level: UEFA Cup 2)."""
    tier = row["tier"]
    return (isinstance(tier, int) or str(tier).isdigit()) and int(tier) >= 1 and \
        row["nation"] not in CONFEDERATIONS


def cut(text, n):
    """The words that fit in n characters; an open bracket at the end goes with its words."""
    if len(text) <= n:
        return text
    words, out = text.split(" "), ""
    for w in words:
        if len(out) + len(w) + (1 if out else 0) > n:
            break
        out = f"{out} {w}" if out else w
    if out.count("(") > out.count(")"):
        out = out[:out.rindex("(")].rstrip()
    return out or text[:n]


def shown_name(name):
    """The long name that the game shows for a research name, or None for no name."""
    t = (name or "").strip()
    if not t or t.lower() in ("n/a", "unknown", "none") or t.lower().startswith("unknown"):
        return None
    if "; " in t:                                      # two editions in one row: the later one
        t = t.split("; ")[-1]
    m = re.match(r"^(.*\S)\s*\(([^()]*)\)$", t)
    if m and not KEEP_BRACKETS.search(m.group(2)):
        t = m.group(1)
    t = re.sub(r'\s*"[^"]*"$', "", t)                 # a name in quotes at the end (a person's name)
    t = re.sub(r"\bEuro (19|20)\d\d\b", "European Championship", t)
    t = re.sub(r"\b(19|20)\d\d([/\u2013-]\d{2,4})?\b", "", t)
    t = re.sub(r"\s+", " ", t).strip(" ,-")
    if len(t) > LONG:                                  # too long: first the part in brackets goes
        t = re.sub(r"\s*\(([^()]*)\)$", lambda b: " " + b.group(1) if "play-off" in b.group(1) else "", t)
        t = t.replace("FIFA World Cup qualification inter-confederation play-offs",
                      "FIFA World Cup inter-confederation play-offs")
    return cut(t, LONG) if t else None


def short_name(long):
    if len(long) <= SHORT:
        return long
    for old, new in SHORTER:
        if old in long:
            long = long.replace(old, new).strip()
            if len(long) <= SHORT:
                return long
    for o in OPENINGS:
        if long.startswith(o) and len(long) - len(o) <= SHORT:
            return long[len(o):]
    for o in OPENINGS:
        if long.startswith(o):
            return cut(long[len(o):], SHORT)
    return cut(long, SHORT)


def ordered_rows(found, file):
    """The rows of a file in a season, in the order of rules_report_check.py pick_row."""
    alone = [r for r in found if r["comp_file"] == file] or found
    return [r for r in alone if "qualifying" not in r["comp"].lower()] or alone


def table(rows):
    """[(season, file, n, long, short, abbr)] in a fixed order; abbr is None for a row that is not a league."""
    by = collections.defaultdict(list)
    for r in rows:
        if r["comp_file"] in ("new", "na"):
            continue
        for f in r["comp_file"].split(";"):
            by[(r["season"], f)].append(r)
    out = []
    for (season, file) in sorted(by):
        for n, r in enumerate(ordered_rows(by[(season, file)], file)):
            long = shown_name(r["name"])
            if long:
                short = short_name(long)
                out.append((season, file, n, long, short, abbreviation(short) if is_league(r) else None))
    return out


def names_by_key(rows):
    """{(season, file): [long or None, ...]} by n, for the check of the rules report."""
    out = collections.defaultdict(dict)
    for season, file, n, long, _, _ in table(rows):
        out[(season, file)][n] = long
    return out


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--out")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args(argv)
    if not (a.out or a.check or a.list):
        ap.error("give --out, --check or --list")
    rows = make_rules.read_rows()
    out = table(rows)
    for season, file, n, long, short, abbr in out:
        for text in (long, short) + ((abbr,) if abbr is not None else ()):
            if not text or not text.strip() or "\t" in text or "\n" in text:
                print(f"make_names.py: {season} {file} {n}: an empty name or a name with a tab: {long!r} / {short!r}")
                return 1
            try:
                text.encode("cp1252")
            except UnicodeEncodeError:
                print(f"make_names.py: {season} {file} {n}: {text!r} has a character that Windows-1252 cannot hold")
                return 1
        if not 2001 <= int(season) <= 2099 or not file.strip() or "\t" in file or not 0 <= n <= 999:
            print(f"make_names.py: {season} {file!r} {n}: a season, file or number that the game's loader rejects "
                  f"(season_names.c drops the whole file)")
            return 1
        if len(long) > LONG or len(short) > SHORT or long.count("(") != long.count(")") or \
                (abbr is not None and (len(abbr) > ABBR or " " in abbr)):
            print(f"make_names.py: {season} {file} {n}: {long!r} / {short!r} / {abbr!r} is too long or has an open "
                  "bracket")
            return 1
    if a.list:
        for season, file, n, long, short, abbr in out:
            print(f"{season}\t{file}\t{n}\t{long}\t{short}\t{abbr or ''}")
    if a.check:
        print(f"make_names.py: {len(out)} names from {len(rows)} rows")
    if a.out:
        lines = [f"#bosman-names\t{FORMAT}\t{len(out)}", "season\tfile\tn\tlong\tshort\tabbr"]
        lines += [f"{s}\t{f}\t{n}\t{lg}\t{sh}\t{ab or ''}" for s, f, n, lg, sh, ab in out]
        os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
        tmp = a.out + ".tmp"
        with open(tmp, "w", encoding="cp1252", newline="\n") as f:
            f.write("\n".join(lines) + "\n")
        os.replace(tmp, a.out)
        print(f"make_names.py: {len(out)} names written to {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
