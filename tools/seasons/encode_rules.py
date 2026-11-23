#!/usr/bin/env python3
"""Encodes the nation rules of the rules table for the game (0.6.0 slice 17).

Usage:
    python3 -I tools/seasons/encode_rules.py --write     write the three outputs below
    python3 -I tools/seasons/encode_rules.py --check     read the parts, stop on a text the tool does not know,
                                                         and check that the committed outputs are up to date

Input: the "nation rules" rows of the 20 parts of docs/research/rules-0.6.0/ (one row per nation and season, 28
nations, 2001/02 to 2027/28), read with make_rules.read_rows(). The text columns are foreign, work_permit,
registration and windows.

Outputs (committed):
    docs/research/2026-10-17-nation-rules-encoded.tsv   one line per nation and season: the structured value of
                                                        each column
    docs/research/2026-10-17-nation-rules-gaps.tsv      one line per text the tool could not encode, with the
                                                        reason (for Max)
    src/recomp_rt/win/season_nation_table.c             the C table of the runtime (season_nation_rules)

The structured values (the same words in the TSV and in the C table):
    windows        keep      no new information: the game keeps the windows it has (the last encoded ones)
                   none      no transfer windows: the club can sign at any time
                   D.M-D.M[,D.M-D.M...]
                             the windows of the season, day.month of the start and the end (1.7-31.8,1.1-31.1).
                             A start month after the end month runs over the new year.
    squad_non_eu, squad_foreign, sign_non_eu, sign_foreign
                   keep      no new information
                   none      no limit
                   N         the limit. squad_*: players of that kind in the match squad of a first division
                             fixture (the number of an existing slot of the restriction record, +5 non-EU, +2
                             foreign). sign_*: players of that kind a club may hold when it signs another
                             (the rules object's caps +0x14 and +0x13 of TRANSFER_RULES).
                             squad_non_eu N with squad_foreign none changes the kind of a game rule of
                             foreign players to non-EU in every competition of the nation (Croatia from
                             2013/14, season_nation_squad_record).
    permit         keep, or brexit (players of the EU/EEA need a permit as non-EU players: UK nations, from 1
                   January 2021, or from the start of the season when the season starts after that date)

What the tool does not encode is listed in the gaps file, each with a reason (the reasons are the keys of
GAP_REASONS): the game has no code for it ("needs new code"), the text has no single value for the season,
or the tool does not know the form of the text. A text of "unknown" or "n/a" is not a gap: it keeps the game's
values (status keep). The tool stops, with exit code 1, on a text that it neither knows as an encoding nor as a
reason, so a changed research text never goes unseen.
"""
import argparse
import csv
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import make_rules  # noqa: E402

OUT_TSV = os.path.join(ROOT, "docs", "research", "2026-10-17-nation-rules-encoded.tsv")
OUT_GAPS = os.path.join(ROOT, "docs", "research", "2026-10-17-nation-rules-gaps.tsv")
OUT_C = os.path.join(ROOT, "src", "recomp_rt", "win", "season_nation_table.c")
DATA_MAP = os.path.join(ROOT, "docs", "research", "data-map", "shared.md")

MONTHS = ["january", "february", "march", "april", "may", "june", "july", "august", "september", "october",
          "november", "december"]
MONTH_RE = "|".join(m[:3] + "[a-z]*" for m in MONTHS)

# Nation name of the table -> g_key_nation_<name> of the data map (the global holds the nation's record id).
KEY_NATIONS = {
    "Argentina": "argentina", "Australia": "australia", "Belgium": "belgium", "Brazil": "brazil",
    "Croatia": "croatia", "Denmark": "denmark", "England": "england", "Finland": "finland", "France": "france",
    "Germany": "germany", "Greece": "greece", "Holland": "holland", "Italy": "italy", "Japan": "japan",
    "Northern Ireland": "northern_ireland", "Norway": "norway", "Poland": "poland", "Portugal": "portugal",
    "Republic of Ireland": "republic_of_ireland", "Russia": "russia", "Scotland": "scotland", "Spain": "spain",
    "Sweden": "sweden", "Turkey": "turkey", "United States": "united_states", "Wales": "wales", "South Korea": "south_korea",
}

# Nations whose window records the game gives a meaning by the window's index (sweep-rulebooks 3.1): a rewrite of
# the windows would give that meaning to another window.
FIXED_WINDOWS = {
    "Argentina": "window 1 allows two signings (0x40a884, 0x40a995)",
    "France": "windows 1 and 2 allow one purchase and one free agent (0x5c1830, 0x5c1930)",
}
# Nations whose windows the runtime rewrites only for the listed indexes (the others keep their record).
PARTIAL_WINDOWS = {"Italy": "window 2 is for free transfers only (0x661520, 0x661630): kept as it is"}

GAP_REASONS = {
    "pitch": "a limit on the pitch: the game limits the match squad only",
    "new_code": "needs new code: the game has no such rule",
    "kind": "the limit is of a kind the game's restriction for this nation does not hold",
    "per_division": "different numbers for different divisions that one rules object cannot hold",
    "mid_season": "the rule changes during the season",
    "hardcoded": "the game holds this number in code (not in the rules object): needs a checked code patch",
    "partial": "the text gives only part of the windows or dates",
    "fixed_index": "the game gives a meaning to the window index of this nation",
    "form": "the tool does not know the form of this text",
    "no_value": "the text has no single value for the season",
}


def is_keep(text):
    t = text.strip().lower()
    return (t in ("unknown", "n/a", "") or t.startswith("unknown (") or t.startswith("n/a (")
            or t.startswith("unknown;") or t.startswith("unknown ("))


# ---------------------------------------------------------------- windows

DATE = r"(\d{1,2})(?:st|nd|rd|th)?"
RANGE = re.compile(
    r"(?<![\d:])" + DATE + r"\s*(" + MONTH_RE + r")?\s*(?:\d{4})?\s*(?:to|-|–)\s*" + DATE + r"\s*(" + MONTH_RE
    + r")\b(?:\s*\d{4})?", re.I)
STRAY = re.compile(r"(?<![\d:])\d{1,2}(?:st|nd|rd|th)?\s*(?:" + MONTH_RE + r")\b", re.I)


def month_number(word):
    for i, m in enumerate(MONTHS):
        if word.lower().startswith(m[:3]):
            return i + 1
    raise ValueError(word)


def parse_windows(nation, text):
    """(status, value, reason): status is 'ok', 'keep' or 'gap'."""
    t = text.strip()
    low = t.lower()
    if is_keep(low) or re.search(r"dates unknown|dates: unknown|exact dates unknown|dates vary|\(inference\)$", low) \
            and not RANGE.search(t):
        return "keep", "keep", ""
    if low.startswith("no transfer windows"):
        return "ok", "none", ""
    if low.startswith("no fifa windows yet") or low.startswith("none ("):
        return "keep", "keep", ""        # the game's own windows for 2001/02 (England one window)
    if re.search(r"closed on|could still|international moves|first period|second period|unemployed", low):
        return "gap", "", "partial"
    # England 2002 form: "summer: end of season to 31 Aug (...); winter: 1-31 Jan". The season ends in May, so the
    # summer window starts on 1 June (inference).
    m = re.match(r"summer: end of season to " + DATE + r" (" + MONTH_RE + r")\b.*?; winter: " + DATE + r"-" + DATE
                 + r" (" + MONTH_RE + r")\s*(?:\d{4})?$", t, re.I)
    if m:
        return "ok", "1.6-%d.%d,%d.%d-%d.%d" % (int(m.group(1)), month_number(m.group(2)), int(m.group(3)),
                                                   month_number(m.group(5)), int(m.group(4)),
                                                   month_number(m.group(5))), ""
    m = re.match(r"summer: end of season to " + DATE + r" (" + MONTH_RE + r") \d{4}; winter: " + DATE + r"-" + DATE
                 + r" (" + MONTH_RE + r")\s*\d{4}$", t, re.I)
    if m:
        return "ok", "1.6-%d.%d,%d.%d-%d.%d" % (int(m.group(1)), month_number(m.group(2)), int(m.group(3)),
                                                   month_number(m.group(5)), int(m.group(4)),
                                                   month_number(m.group(5))), ""
    # Italy: "summer to 31 August 2007; winter 3-31 January 2008", "summer 1 July-31 August 2012; winter from 3
    # January 2013, four weeks". The summer window of Italy opens on 1 July (the game's own record).
    m = re.match(r"summer to " + DATE + r" (" + MONTH_RE + r") \d{4}; winter " + DATE + r"-" + DATE + r" (" + MONTH_RE
                 + r") \d{4}$", t, re.I)
    if m and nation == "Italy":
        return "ok", "1.7-%d.%d,%d.%d-%d.%d" % (int(m.group(1)), month_number(m.group(2)), int(m.group(3)),
                                                   month_number(m.group(5)), int(m.group(4)),
                                                   month_number(m.group(5))), ""
    m = re.match(r"summer " + DATE + r" (" + MONTH_RE + r")-" + DATE + r" (" + MONTH_RE + r") \d{4}; winter from "
                 + DATE + r" (" + MONTH_RE + r") \d{4}, four weeks$", t, re.I)
    if m:
        d, mo = int(m.group(5)), month_number(m.group(6))
        end = d + 27
        em = mo
        if end > 31 and mo == 1:
            end, em = end - 31, 2
        return "ok", "%d.%d-%d.%d,%d.%d-%d.%d" % (int(m.group(1)), month_number(m.group(2)), int(m.group(3)),
                                                    month_number(m.group(4)), d, mo, end, em), ""
    # A window for domestic moves only is left out: the game has no window of that kind, and the other windows
    # hold. A text in brackets is a remark ("incoming from abroad to 25 August"), not a window. A single first day
    # before the next range ("1 July, then 15 July to 5 October") is a window of one day.
    t = "; ".join(c for c in t.split(";") if not re.search(r"domestic|complementary", c, re.I))
    t = re.sub(r"\([^)]*\)", " ", t)
    t = re.sub(r"(\d{1,2} (?:" + MONTH_RE + r")), then (\d{1,2} (?:" + MONTH_RE + r") to)", r"\1 to \1; \2", t, flags=re.I)
    ranges = []
    rest = t
    for m in RANGE.finditer(t):
        d1, m1, d2, m2 = int(m.group(1)), m.group(2), int(m.group(3)), m.group(4)
        mon2 = month_number(m2)
        mon1 = month_number(m1) if m1 else mon2
        ranges.append((d1, mon1, d2, mon2))
        rest = rest.replace(m.group(0), " ", 1)
    if not ranges:
        if re.search(r"unknown|dates", low):
            return "gap", "", "partial"
        return "gap", "", "form"
    if STRAY.search(rest):
        return "gap", "", "partial"
    if re.search(r"unknown", low):
        return "gap", "", "partial"
    if len(ranges) < 2:
        return "gap", "", "partial"
    if len(ranges) > 6:
        return "gap", "", "form"
    return "ok", ",".join("%d.%d-%d.%d" % r for r in ranges), ""


# ---------------------------------------------------------------- foreign players

NONE_SQUAD = ("none", "none", "keep", "keep")       # squad_non_eu, squad_foreign, sign_non_eu, sign_foreign


def f(non_eu="keep", foreign="keep", sign_non_eu="keep", sign_foreign="keep"):
    return (non_eu, foreign, sign_non_eu, sign_foreign)


def parse_foreign(nation, season, text):
    """(status, (squad_non_eu, squad_foreign, sign_non_eu, sign_foreign), reason, permit)."""
    t = text.strip()
    low = t.lower()
    permit = "keep"
    if "gbe" in low or "right to work in the uk" in low:
        permit = "brexit"
    if is_keep(low):
        return "keep", f(), "", permit
    if nation in ("England", "Wales", "Northern Ireland", "Republic of Ireland") and re.match(
            r"no (nationality )?quota", low):
        return "ok", f("none", "none"), "", permit
    if nation == "Scotland" and low.startswith("none:"):
        return "keep", f(), "", permit
    if nation == "Belgium" and low.startswith("no limit on foreign players"):
        return "ok", f("none", "none"), "", permit
    if nation == "Holland" and low.startswith("no limit on foreign players"):
        return "ok", f("none", "none"), "", permit
    if nation == "Portugal" and re.match(r"no (quota for non-eu|nationality quota)", low):
        # From 2006/07 the Liga and the Liga de Honra have no foreign player status: no limit at a signing and none
        # on the match sheet (issue 121). sign_non_eu lifts the original's own limit of 0x7d3ef0 and 0x7d4040 for
        # those two divisions. sign_foreign stays "keep": the base cap +0x13 is a signed byte in TRANSFER_RULES::
        # can_make_offer 0x8e8800 (0x8e89a3..0x8e89ad, jg), so 255 would refuse every offer for a foreign player.
        return "ok", f("none", "none", "none"), "", permit
    if nation == "Portugal" and low.startswith("liga: at most 6 non-eu players registered and at most 4"):
        # 2001/02 to 2005/06: the original's limits, 6 at a signing in the first division (0x7d3f35) and 4 on the
        # match sheet (0x7d43c0).
        return "keep", f(), "", permit
    if nation == "Poland" and low.startswith("ekstraklasa: no limit on non-eu"):
        return "ok", f("none", "none"), "", permit
    if nation == "Poland" and re.search(r"max \d non-eu players on the pitch", low):
        return "gap", f(), "pitch", permit
    if nation == "Poland" and low.startswith("ekstraklasa: no non-eu limit"):
        return "ok", f("none", "none"), "", permit
    if nation == "Germany":
        m = re.match(r"bundesliga and 2\. bundesliga: (?:no limit on eu/uefa players; )?up to (\d) non-european", low)
        if m:
            n = int(m.group(1))
            if "under contract" in low:
                return "ok", f(n, "keep", n, "keep"), "", permit
            return "ok", f(n), "", permit
        if low.startswith("no limit on foreign players"):
            return "ok", f("none", "none", "none", "none"), "", permit
    if nation == "France":
        m = re.match(r"(?:ligue 1: at most |)(\d) (?:foreign \(non-eu\)|non-eu)", low)
        if m:
            return "ok", f(int(m.group(1))), "", permit
    if nation == "Brazil":
        m = re.match(r"(?:s.rie a match squad: at most |at most )(\d) foreign players", low)
        if m:
            return "ok", f("keep", int(m.group(1))), "", permit
    if nation == "Australia":
        m = re.match(r"max (\d) foreign \(visa\) players per squad", low)
        if m:
            return "ok", f("keep", int(m.group(1)), "keep", int(m.group(1))), "", permit
    if nation == "Turkey":
        if low.startswith("süper lig: 6+1 at the start"):
            return "gap", f(), "mid_season", permit
        m = re.match(r"s.per lig: (\d+)\+(\d+)(?:\+(\d+))?(?!\d)", low)
        if m and season <= 2014:
            a, b = int(m.group(1)), int(m.group(2))
            c = int(m.group(3)) if m.group(3) else None
            sign = "keep"
            if c is not None:
                sign = a + b + c
            elif "no limit on foreigners signed" in low:
                sign = "none"
            else:
                s = re.search(r"(\d+) foreigners signed", low)
                if s:
                    sign = int(s.group(1))
            m8 = re.search(r": (\d+) foreigners in the matchday squad", low)
            return "ok", f("keep", int(m8.group(1)) if m8 else a + b, "keep", sign), "", permit
        m = re.match(r"s.per lig: (\d) foreigners may play", low)
        if m:
            return "ok", f("keep", int(m.group(1))), "", permit
        m = re.match(r"s.per lig: (?:at most )?(\d+) foreigners? (?:in the (?:\d+-man )?squad|registered|signed|in the a list)", low)
        if m:
            return "ok", f("keep", "keep", "keep", int(m.group(1))), "", permit
        m = re.match(r"s.per lig: (?:at most )?(\d+) foreign players on the roster", low)
        if m:
            return "ok", f("keep", "keep", "keep", int(m.group(1))), "", permit
        m = re.match(r"s.per lig: \d+\+\d+: at most (\d+) foreigners in the a list", low)
        if m:
            return "ok", f("keep", "keep", "keep", int(m.group(1))), "", permit
        return "gap", f(), "form", permit
    if nation == "Russia":
        if low.startswith("premier league: no limit"):
            return "ok", f("keep", "none", "keep", "none"), "", permit
        m = re.match(r"premier league: at most (\d+) foreigners on the pitch and (\d+) in the matchday squad", low)
        if m:
            return "ok", f("keep", int(m.group(2))), "", permit
        m = re.match(r"premier league: at most (\d+) foreigners in the (?:25-man )?season squad", low)
        if m:
            return "ok", f("keep", "keep", "keep", int(m.group(1))), "", permit
        if re.match(r"premier league: at most \d+ foreigners on the pitch", low):
            return "gap", f(), "pitch", permit
    if nation == "Japan":
        if low.startswith("no limit on foreign players registered"):
            m = re.search(r"max (\d) in j1", low)
            return "ok", f("keep", int(m.group(1)) if m else "none", "keep", "none"), "", permit
        m = re.match(r"3 foreign players may play; at most (\d) registered", low)
        if m:
            return "ok", f("keep", 3, "keep", int(m.group(1))), "", permit
        m = re.match(r"3 foreign players may play", low)
        if m:
            return "ok", f("keep", 3), "", permit
        m = re.match(r"3 foreign plus 1", low)
        if m:
            return "ok", f("keep", 4), "", permit
    if nation == "South Korea":
        m = re.match(r"3 on the pitch, (\d) registered", low)
        if m:
            return "ok", f("keep", 3, "keep", int(m.group(1))), "", permit
        m = re.match(r"3 plus 1 afc plus 1 asean player", low)
        if m:
            return "ok", f("keep", 5), "", permit
        m = re.match(r"3 plus 1 afc player", low)
        if m:
            return "ok", f("keep", 4), "", permit
        m = re.match(r"k league 1: (\d) plus 1 afc registered, 3 plus 1 on the pitch", low)
        if m:
            return "ok", f("keep", 4, "keep", int(m.group(1)) + 1), "", permit
        m = re.match(r"k league 1: up to (\d) registered, (\d) on the pitch", low)
        if m:
            return "ok", f("keep", int(m.group(2)), "keep", int(m.group(1))), "", permit
        m = re.match(r"no limit on foreign players registered; matchday k league 1 max (\d)", low)
        if m:
            return "ok", f("keep", int(m.group(1)), "keep", "none"), "", permit
    if nation == "Argentina":
        m = re.match(r"(\d) registered, (\d) (?:in the match|on the match sheet)", low)
        if m and "until december 2017" not in low:
            return "ok", f("keep", int(m.group(2)), "keep", int(m.group(1))), "", permit
        if "until december 2017" in low:
            return "gap", f(), "mid_season", permit
    if nation == "United States":
        m = re.match(r"up to (\d) foreign players", low)
        if m:
            return "ok", f("keep", int(m.group(1)), "keep", int(m.group(1))), "", permit
        m = re.match(r"(\d) international roster slots per club", low)
        if m:
            return "ok", f("keep", int(m.group(1)), "keep", int(m.group(1))), "", permit
    if nation == "Croatia":
        m = re.match(r"no quota on foreigners, but max (\d) may be fielded", low)
        if m:
            return "ok", f("keep", int(m.group(1))), "", permit
        if "from 18 october 2016" in low:
            return "gap", f(), "mid_season", permit
        # From 2013/14 the HNS counts only players not of the EU or EEA (Glasnik 28/2013, issue 135). The game's
        # rule is a foreign limit (0x518620); a non-EU number with no foreign limit moves it to the non-EU slot
        # (season_nation_squad_record).
        m = re.match(r"hnl(?: and cup(?: final stages)?)?: (\d) (foreign|non-eu) players per (?:team|match)", low)
        if m:
            eu_free = m.group(2) == "non-eu" or "eu/eea nationals and croatian dual nationals not counted" in low
            return "ok", (f(int(m.group(1)), "none") if eu_free else f("keep", int(m.group(1)))), "", permit
    if nation == "Greece":
        m = re.match(r"super league: (\d) non-eu players; beta ethniki", low)
        if m:
            return "ok", f(int(m.group(1))), "", permit
        m = re.match(r"super league 1: up to \d non-eu/eea players \(max 5 over 23\), up to (\d) on the match sheet", low)
        if m:
            return "ok", f(int(m.group(1))), "", permit
        if low.startswith("super league 1:"):
            return "gap", f(), "pitch", permit
    if nation == "Sweden":
        m = re.match(r"of the 16 players on the matchday list, (\d) may come from outside europe", low)
        if m:
            return "ok", f(int(m.group(1))), "", permit
        if "on the pitch" in low:
            return "gap", f(), "pitch", permit
        return "gap", f(), "kind", permit
    if nation == "Spain":
        return "gap", f(), "hardcoded", permit
    if nation == "Italy":
        return "gap", f(), "new_code", permit
    if nation == "Norway":
        return "keep", f(), "", permit
    if nation == "Denmark":
        return "keep", f(), "", permit
    if nation == "Finland":
        return "keep", f(), "", permit
    return "gap", f(), "form", permit


# ---------------------------------------------------------------- work permits and registration


def parse_permit(nation, text, foreign_permit):
    low = text.strip().lower()
    if foreign_permit == "brexit":
        return "brexit"
    if nation in ("England", "Scotland", "Wales", "Northern Ireland") and re.search(
            r"(from 1 jan(?:uary)? 2021 (?:gbe|eu players)|^gbe points|all players without uk status|^all non-uk|"
            r"^from 1 january 2021 eu players)", low):
        return "brexit"
    if nation in ("Wales", "Northern Ireland") and "gbe points system applies" in low:
        return "brexit"
    return "keep"


PERMIT_KEEP_NOTE = {
    "england": "the FIFA ranking criteria of the permit (2001, 2015) are not coded in the game",
}


def classify_permit_gap(nation, text, season):
    low = text.strip().lower()
    if is_keep(low) or low.startswith("n/a"):
        return None
    if parse_permit(nation, text, "keep") == "brexit":
        return None
    return "new_code"


def classify_registration_gap(nation, text):
    low = text.strip().lower()
    if is_keep(low) or low.startswith("no squad size") or low.startswith("no upper limit") or low.startswith("no squad cap"):
        return None
    return "new_code"


# ---------------------------------------------------------------- tables


def season_label(s):
    return "%d/%02d" % (s, (s + 1) % 100)


def nation_rows():
    rows = [r for r in make_rules.read_rows() if r["comp"] == "nation rules"]
    by = {}
    for r in rows:
        by.setdefault(r["nation"], {})[r["season"]] = r
    return by


def encode():
    """-> (encoded rows, gap rows, problems)"""
    out, gaps, problems = [], [], []
    for nation, seasons in sorted(nation_rows().items()):
        if nation == "World (FIFA)":
            continue
        for season in sorted(seasons):
            r = seasons[season]
            wst, wval, wreason = parse_windows(nation, r["windows"])
            if wst == "ok" and nation in FIXED_WINDOWS:
                wst, wval, wreason = "gap", "", "fixed_index"
            fst, fvals, freason, fpermit = parse_foreign(nation, season, r["foreign"])
            permit = parse_permit(nation, r["work_permit"], fpermit)
            if nation in ("Wales", "Northern Ireland") and permit == "brexit" and season < 2020:
                permit = "keep"
            row = {"nation": nation, "season": season, "windows": wval or "keep",
                   "squad_non_eu": str(fvals[0]), "squad_foreign": str(fvals[1]),
                   "sign_non_eu": str(fvals[2]), "sign_foreign": str(fvals[3]), "permit": permit}
            out.append(row)
            if wst == "gap":
                gaps.append((nation, season, "windows", wreason, r["windows"]))
            if fst == "gap":
                gaps.append((nation, season, "foreign", freason, r["foreign"]))
            g = classify_permit_gap(nation, r["work_permit"], season)
            if g:
                gaps.append((nation, season, "work_permit", g, r["work_permit"]))
            g = classify_registration_gap(nation, r["registration"])
            if g:
                gaps.append((nation, season, "registration", g, r["registration"]))
    for g in gaps:
        if g[3] not in GAP_REASONS:
            problems.append("gap reason %r is not in GAP_REASONS" % g[3])
    return out, gaps, problems


def check_windows(row):
    if row["windows"] in ("keep", "none"):
        return
    for w in row["windows"].split(","):
        m = re.fullmatch(r"(\d{1,2})\.(\d{1,2})-(\d{1,2})\.(\d{1,2})", w)
        if not m:
            raise SystemExit("encode_rules.py: bad window %r in %s %d" % (w, row["nation"], row["season"]))
        d1, m1, d2, m2 = (int(x) for x in m.groups())
        for d, mo in ((d1, m1), (d2, m2)):
            if not (1 <= mo <= 12 and 1 <= d <= (29 if mo == 2 else 30 if mo in (4, 6, 9, 11) else 31)):
                raise SystemExit("encode_rules.py: bad date %d.%d in %s %d" % (d, mo, row["nation"], row["season"]))


def key_globals():
    """{data map name: address} of g_key_nation_* in docs/research/data-map/shared.md."""
    found = {}
    with open(DATA_MAP, encoding="utf-8") as fh:
        for line in fh:
            m = re.match(r"\| (0x[0-9a-f]+) \| 4 \| s32 \| `g_key_nation_(\w+)`", line)
            if m:
                found[m.group(2)] = int(m.group(1), 16)
    return found


TSV_COLUMNS = ["nation", "season", "windows", "squad_non_eu", "squad_foreign", "sign_non_eu", "sign_foreign", "permit"]


def tsv_text(rows):
    lines = ["# Structured nation rules (0.6.0 slice 17). Made by tools/seasons/encode_rules.py; the values are "
             "described in its docstring.", "\t".join(TSV_COLUMNS)]
    for r in rows:
        lines.append("\t".join([r["nation"], str(r["season"])] + [r[c] for c in TSV_COLUMNS[2:]]))
    return "\n".join(lines) + "\n"


def gaps_text(gaps):
    lines = ["# Texts of the nation rules that the encoder (tools/seasons/encode_rules.py) does not encode. For Max: "
             "the reason says why. Columns: nation, season, column, reason, text.",
             "nation\tseason\tcolumn\treason\ttext"]
    for n, s, c, reason, text in gaps:
        lines.append("\t".join([n, season_label(s), c, reason, text.replace("\t", " ")[:300]]))
    return "\n".join(lines) + "\n"


def num(v):
    """The C value: -1 keep, 255 none (no limit), else the limit, 0 to 254 (255 would read as no limit)."""
    if v == "keep":
        return -1
    if v == "none":
        return 255
    n = int(v)
    if not 0 <= n <= 254:
        raise SystemExit("encode_rules.py: a limit of %d is outside 0 to 254" % n)
    return n


def c_text(rows):
    keys = key_globals()
    names = sorted({r["nation"] for r in rows})
    lines = ["/* The structured nation rules (0.6.0 slice 17). Made by tools/seasons/encode_rules.py from the rules table",
             " * (docs/research/rules-0.6.0): do not edit by hand. Values: season_nation.h. */",
             '#include "season_nation.h"', "",
             "const SeasonNation season_nations[] = {"]
    for n in names:
        key = KEY_NATIONS.get(n)
        addr = keys.get(key) if key else None
        if addr is None:
            raise SystemExit("encode_rules.py: no g_key_nation_ global for %r in the data map" % n)
        lines.append('    {"%s", 0x%xu, %s},' % (n, addr, "SEASON_NATION_PARTIAL_WINDOWS" if n in PARTIAL_WINDOWS
                                                  else "0"))
    lines += ["};", "const int season_nation_count = %d;" % len(names), "",
              "const SeasonNationRule season_nation_rules[] = {"]
    state = {}
    for r in rows:
        # A row that changes nothing of what the earlier rows of the nation gave is left out: the runtime folds
        # the rows of a nation in order, a value of keep leaves the rule as it is.
        have = state.setdefault(r["nation"], {})
        changed = False
        for c in TSV_COLUMNS[2:8]:
            if r[c] != "keep" and have.get(c) != r[c]:
                have[c] = r[c]
                changed = True
        if not changed:
            continue
        wins = [] if r["windows"] in ("keep", "none") else r["windows"].split(",")
        wcount = -1 if r["windows"] == "keep" else 0 if r["windows"] == "none" else len(wins)
        wtxt = ", ".join("{%s}" % ", ".join(re.split(r"[.-]", w)) for w in wins) or "{0}"
        lines.append('    {"%s", %d, %d, {%s}, %d, %d, %d, %d, %d},' % (
            r["nation"], r["season"], wcount, wtxt, num(r["squad_non_eu"]), num(r["squad_foreign"]),
            num(r["sign_non_eu"]), num(r["sign_foreign"]), 1 if r["permit"] == "brexit" else 0))
    first = lines.index("const SeasonNationRule season_nation_rules[] = {") + 1
    lines += ["};", "const int season_nation_rule_count = %d;" % (len(lines) - first), ""]
    return "\n".join(lines)


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--summary", action="store_true")
    args = ap.parse_args(argv)
    rows, gaps, problems = encode()
    for r in rows:
        check_windows(r)
    if problems:
        for p in problems:
            print("encode_rules.py: " + p, file=sys.stderr)
        return 1
    texts = {OUT_TSV: tsv_text(rows), OUT_GAPS: gaps_text(gaps), OUT_C: c_text(rows)}
    if args.summary or not (args.write or args.check):
        kinds = {}
        for _, _, col, reason, _ in gaps:
            kinds[(col, reason)] = kinds.get((col, reason), 0) + 1
        win = sum(1 for r in rows if r["windows"] not in ("keep",))
        print("%d rows, %d with windows, %d gaps" % (len(rows), win, len(gaps)))
        for k, n in sorted(kinds.items()):
            print("  %-13s %-13s %d" % (k[0], k[1], n))
        return 0
    if args.write:
        for path, text in texts.items():
            with open(path, "w", encoding="utf-8", newline="\n") as fh:
                fh.write(text)
            print("wrote " + os.path.relpath(path, ROOT))
        return 0
    bad = 0
    for path, text in texts.items():
        have = open(path, encoding="utf-8").read() if os.path.exists(path) else None
        if have != text:
            print("encode_rules.py: %s is not up to date: run --write" % os.path.relpath(path, ROOT), file=sys.stderr)
            bad += 1
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
