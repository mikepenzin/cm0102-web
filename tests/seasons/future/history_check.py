#!/usr/bin/env python3
"""History checker for the future runs (step 2 of lead-notes future testing): a saved game in, one TSV of the
history rows of every competition out, then the checks of those rows.

Usage:
    python3 -I tests/seasons/future/history_check.py <save.sav> [<save.sav> ...] [--out <folder>]
        [--rules <competitions.tsv>] [--known <known.tsv>] [--label <name>] [--nations <nations.tsv>]
        [--access <access.tsv>] [--table <season_intl_table.c>] [--health] [--tier-a]

--tier-a: the save is of a Tier A game (RECOMP_TIER_A=1; a save does not record it): the checks of real results
(same-final) are skipped, as run_set.py skips them for a Tier A row.

--out writes <label>.history.tsv, <label>.faults.tsv, <label>.entrants.tsv and with --health <label>.health.tsv.

The rows are the ones that the winners tab of each competition shows (0x4991f0 draws winner, runner-up, third
and host): the club comp history and the nation comp history of the save (club_comp_history.dat and
nation_comp_history.dat, 26 bytes a row: +0x00 id, +0x04 competition id, +0x08 u16 label year, +0x0a first,
+0x0e second, +0x12 third, +0x16 host, -1 for none; docs/research/2026-10-17-competition-history-fill.md). A row
with a negative competition is a free row. Club and national side ids index club.dat and then nat_club.dat
(581 bytes a record, +0x53 nation). A nation (nation.dat, 290 bytes) has its continent at +0x71. A competition
(club_comp.dat and nation_comp.dat, 107 bytes, the record id is the competition id) has its continent at +0x59
and its nation at +0x5d.

The save date is the game date that the save writes into general.dat (GENERAL_DATE). A save with no general.dat
uses the latest date of the news list (news.dat: 100 items of 223 bytes after an 8-byte head, the s16 day of the
year from 0 and the s16 year at +0xc9 of each item). The news date can be days before the game date: in the 0.7.3
release set (t2003, leg 1) the game saved on day 244 of 2004, and its last news was of day 240. The start
year and changing rules are the words +8 and +0xc of bosman_season.dat (docs/research/2026-10-17-season-save-part.md,
section 4); a save without that part is a 2001 game with changing rules off.

The edition year of a finals class: its .tmp part holds the word after the bytes ff ff ff ff 07 03 xx xx, seven
zero bytes, one byte and three zero bytes (inference from the parts of a 2028 save: FIFA World Cup 2030, the
classes after their table 2200, the ended cups 2999). An edition in progress writes a longer part, where the
pattern is absent; then the check reads nothing.

Checks (one row of <label>.faults.tsv each, check names in the first column):
    duplicate     a competition holds the same label year twice (three times for a split season: two rows a year in
                  most of the 10 years before the start, or an Argentine or Mexican competition with two rows in any
                  year before it: the game plays two titles a year there; the formats of SEVERAL_TITLES)
    no-host       a row with no host in a competition whose rows have hosts (more than half of its rows)
    confederation a winner or runner-up from another confederation, in a row from the start year on. The
                  confederation is the rules file's, else the competition's continent, else the continent of
                  80% or more of the winners before the start year (at least 3); none: no check
    same-final    the same two finalists more than 2 editions in a row (nation competitions, and club
                  competitions with no nation; with --rules every competition whose kind is not a league)
    gap           a gap in the cycle from the start year on: two editions further apart than the cycle and one
                  year. The cycle is the rules file's, else the most common step of the last 6 editions before
                  the start year
    (a competition whose rules cycle is 0 has ended: it gets none of the three checks below)
    missing       no edition after the last one up to the save date (the last edition + cycle + 1 < the save
                  year), for a competition with rows from the start year on or an international one; not for a
                  competition whose class waits on 2999 (ended) or whose rules end year has passed
    next-edition  the class waits for the year 2199 or 2200 (the idle year of rt_intl_next_year), or for a
                  year more than one cycle and one year after the save date. Not when 2199 or 2200 is within
                  one cycle and one year of the save date: a save in 2199 of a four-year cup is then due in 2200
Checks of the competition record (with --rules):
    comp-continent the record's continent is not a confederation of its rules (open item 21)
    comp-name     the short name (+0x38, 26 bytes, the name of the history screen) is another competition than the
                  long name; the rows are then also checked against the confederation of the short name, under
                  that name (open item 4: the UEFA Nations League record named "Asian Cup Quals")
    A winner whose nation moves confederation after the row's year (nations.tsv: Australia, OFC to 2005) passes.

Nations (--nations, default nations.tsv next to this file), for the changes due at the save date:
    nation-name   a live record (with a continent) under the old name of a rename
    side-name     a national side named with the old name while its nation holds the new one
    nation-confed a nation in another confederation than a move gives

Entrants: the fixtures parts give the teams of each edition (an edition: fixtures with no gap of more than 150
days, or after a final: a day of one match and then 25 days or more with none; its year the year of the last
match). The list gives each team's route in: title holder (the winner of
the edition before), host, the winner of a route competition, a winner or place 2 or 3 in the history of the
edition's year or the year before, else "qualifier". Checks, for the editions whose last match is before the
save date and that no longer run (the last edition of a competition with a match fewer than 14 days before the save
date and more than one match on its last day may go on: the July qualifying of a Champions League, open item 70).
A confederation check for every edition:
    entrant-confed entrants from another confederation than the rules give (also the short name's); in the World
                  Cup qualifying up to 2 teams of each other confederation pass (the play-off between
                  confederations); a nation with a due move to a confederation of the competition passes, and so
                  does the winner of a route competition (a Mexican winner of the Copa Sudamericana in the Recopa),
                  and a club now in the confederation that its nation moved to after the edition's first day
                  (nations.tsv; an edition that began before the move, as the Asian Club Championship of 2001/02
                  with Kazakh clubs, its row year 2002). A club still in the confederation it leaves is checked
    entrant-count  a finals edition with another count of teams than the finals table of the season code
                  (season_intl_table.c; after its last row the count of the last row), or than access.tsv
    entrant-host   a host of the finals table that the data holds is not an entrant; after the table, the host of
                  the edition's history row
    entrant-dead   entrants of a nation under the old name of a due rename, in an edition after the rename's
                  season (the dead record beside the live one,
                  or a record the rename missed); a nation with no continent in the data (Bonaire, Guadeloupe)
                  is no fault
    entrant-missing a live UEFA nation with a national side is not an entrant of a European World Cup qualifying
                  edition of at least 30 teams that began in the game: every member enters, less the European
                  hosts of its World Cup, less Russia from 2026 and less a nation that joins UEFA later (open item
                  57). Only where the season code builds the whole pool (uefa_all): not the 2002 campaign, not a
                  2001 game with changing rules off, from 2026 only with changing rules on or a 2025 start.
                  The hosts: the finals table (access.tsv), the save's host table (HostCountry.tmp), and after
                  the table (open item 76) a pair at most of UEFA nations that played the finals without the
                  qualifying, one of them the history row's host (finals_hosts; the pool leaves the hosts out:
                  season_wcq26.c europe_nations, the original 0x919ee3)
    duty-clash    a continental club match within one day of a match of a national side that holds 3 or more
                  players of one of the clubs (they are on duty, 0x7757d0; open item 93). For an Olympic match of the
                  under-21 sides the fault is only a club that keeps fewer than 11 players it can field: not on duty,
                  at the club, not injured (open item 107), with the virtual players of its free squad slots for a
                  club with a nation (open item 164). The squad count of a senior or 'B' side applies only to its
                  matches up to 14 days after the save, the squads already picked (open items 157 to 162). The
                  Olympic count has no such limit. Fixtures still to be played; not in a 2001 game with changing
                  rules off
    entrant-holder the title holder is not an entrant (access.tsv "holder": the Champions League from 2006, the
                  Libertadores), when the edition before is at most one cycle and one year before; not for an
                  edition of the start year or before (it began before the game, the holder may be out)
    entrant-limit  a nation has more Champions League entrants than the association limit of the season (open item
                  70; season_uefa_cl_cap: 4 to 2014/15, 5 from 2015/16, 7 from 2024/25), in an edition of the
                  season code's access list (from 2009/10) that began after the start year, of 500 days or less;
                  not in a base game (changing rules off), which plays the original's format
    entrant-route  an entrant of a fixed-route competition (access.tsv: UEFA Super Cup, Recopa, Suruga Bank,
                  Intercontinental Cup, Finalissima, FIFA Intercontinental Cup) won none of its route competitions
                  in the year of the match or the edition before, when that edition is at most one cycle and one
                  year before (checked when every route competition has history rows, for an edition after the
                  start year: an edition of the start year may come from the data); a route competition with no
                  row written in the game (a year after the start) in the year or one cycle before did not play
                  (its nations are not selected), and its place goes to a drawn club, as the original draws one
                  (WorldClubChamp::pick_entrant 0x92ae60, World_Club_Cup 0x92baa8): each such route lets one
                  entrant of its confederation come by no route (slice 9a), of its nation for a national
                  competition (the J.League Cup place of the Suruga Bank, open item 77)
    save-part      a fixtures part whose size does not fit its count (not read)

World health (--health), Tier B: <label>.health.tsv holds the save size and part count, the players, under-21 and
under-23 players (age from the birth date at the save date), the squads of the senior and under-21 sides, the
clubs and national sides with a manager, and the club balances (lowest, highest, outside 32 bits), then one row
per live nation. Time per game day is not in a save. Checks:
    empty-squad   a senior side with no player in its squad while 23 players or more hold the nationality, or an
                  under-21 side with none while 11 or more of them are under 21 (Q17). Only a side with a match in
                  the 14 days after the save or one played in the 14 days before it: the pick runs 14 days ahead
                  of a fixture, so a side with no fixture has no squad (open item 195)
    no-youth      a nation with 23 players or more and none under 21 or none under 23 (Q31: the under-21 and
                  Olympic squads then empty). A fault that fails a Tier B run (Q35, open item 52)
    overflow      an age field that is not the age from the birth date (more than 1 year apart), or a balance
                  of 2^62 or more
    full-squad    a club with all 50 squad slots (club +0xd7) filled and fewer than 8 players aged 21 or under
                  outside its 14 best by current ability: the top-up 0x90dae0 has no slot for a virtual player,
                  the reserve pick 0x6dbc50 first keeps 14 first-team players out, and the rule "3 over 21"
                  (0x8cf7e0 type 3, the age field +0x18 against the limit at 0x8cf9a8) leaves fewer than 11
                  (open item 78). It reports, it does not fail a run

--rules (default: competitions.tsv next to this file, when it is there; lines with # are notes): per key (column comp) the columns
kind, confed (FIFA or nation: no check, else codes joined by |), has_host (0: no host check, 1: every row from
the start on needs one), cycle (0: the competition ended) and end_year (a year or none). --names (default
history_names.tsv next to this file) gives the key of each competition name of a save; a name with no key keeps
the rules above. --known: a TSV with check, competition (* for any text), year
(or *) and a note, and two optional columns: rules ("on" or "off": the row matches only a game with that
changing-rules setting) and accepted ("design" or "real": the note cites a source that shows the game matches the
original or real life; the fault is printed as "accepted", not a fault). A fault that matches another row is printed
as "known". Exit code 1 when a fault is not known or accepted.
"""
import collections
import csv
import datetime
import fnmatch
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "save"))
import savfile  # noqa: E402

CLUB, NATION, COMP, CONT, HIST = 0x245, 0x122, 0x6B, 0xC6, 0x1A
HOST_ROW, HOST_FIXED = 0x22, 0xFD                # the host table's rows (HostCountry.tmp) and a fixed row's choice
SLOTS = 50                  # club squad slots, +0xd7 (VIRTUAL_STAFF 0x90d8f0 counts 0x32)
FIRST_TEAM = 14             # players the reserve pick 0x6dbc50 keeps for the first team
PLAYER = 0x46               # player.dat row
IDLE_YEARS = (2199, 2200)
ENDED_YEAR = 2999
EDITION = re.compile(rb"\xff\xff\xff\xff\x07\x03..\x00{7}.\x00\x00\x00(..)", re.S)
PART = re.compile(r"^(.*)_(\d+)\.tmp$")
SEASON = re.compile(r"^\d{4}(-\d{2})? ")
FIXTURES = re.compile(r"^fixtures_\d+\.tmp$")
FIXTURE = 0x4F
RUNNING = 14                                  # days: an edition with a match this close to the save date may go on
GAP = 150                                     # days between two editions, at the least (the Champions League has
                                              # a winter break of 69 days, 105 in the World Cup year 2022)
FINAL_GAP = 25                                # days with no match after a final, at the least
MOVED = 3                                     # days: add_fixture moves a clashing fixture this far (open item 201)
RECENT = 60                                   # a team of the round after a break played this many days before it
PAUSE = 300                                   # days: the longest pause of a home-and-away group (the OFC Nations Cup
                                              # of 2008 from 21 November 2007 to 14 June 2008, 206 days)
# The game date 0xae2c90 (s16 day of the year from 0, s16 year) in general.dat: save_game_data 0x82e430 writes 4 bytes
# of 0xdd7110 (0x82e796), 0xf74 bytes of its stack (0x82e7cb), 4 bytes of 0xdd7120 (0x82e7e8), 1 byte of 0xdd7114
# (0x82e804, size and count edi = 1), 8 bytes of 0xdd6ec8 (0x82e821), then 8 bytes of 0xae2c90 (0x82e83e).
GENERAL_DATE = 4 + 0xF74 + 4 + 1 + 8
# continent ids of the data (Africa, Asia, Europe, North America, Oceania, South America in every database)
CONF_IDS = ["CAF", "AFC", "UEFA", "CONCACAF", "OFC", "CONMEBOL"]


class LazySav:
    """The members of a .sav, read one at a time (a long game saves 200 MB and more)."""

    def __init__(self, path):
        self.f = open(path, "rb")
        head = self.f.read(savfile.HEADER_SIZE)
        self.version, _, count = struct.unpack_from("<3i", head, 0)
        if self.version not in (1, 2, 3, 4) or not 0 <= count <= savfile.MAX_ENTRIES:
            raise ValueError(f"{path}: not a saved game")
        self.f.seek(0, 2)
        end = self.f.tell()
        ms = []
        for k in range(count):
            o = 12 + k * savfile.ENTRY_SIZE
            off, size = struct.unpack_from("<2I", head, o)
            ms.append((off, size, head[o + 8:o + savfile.ENTRY_SIZE].split(b"\0")[0].decode("latin-1")))
        order = sorted(ms)
        self.members = {}
        for i, (off, size, name) in enumerate(order):
            nxt = order[i + 1][0] if i + 1 < len(order) else end
            self.members[name] = (off, size, nxt - off)
        # The files of merged parts (0.6.2 slice 13, tools/save/savfile.py): read once, kept in memory.
        self.files = {}
        for name in [n for n in self.members if n.lower().endswith(savfile.MERGED)]:
            self.files.update(savfile.split_merged(self.read(name)))

    def names(self):
        return list(self.members) + list(self.files)

    def has(self, name):
        """1 when the save holds the file, as a member or in a merged part."""
        return name in self.members or name in self.files

    def size(self, name):
        """The size of a file, as a member or in a merged part."""
        return len(self.files[name]) if name in self.files else self.members[name][1]

    def read(self, name):
        if name in self.files:
            return self.files[name]
        off, size, raw = self.members[name]
        self.f.seek(off)
        data = self.f.read(raw)
        return savfile.decode(data, size)[0] if self.version in (2, 4) else data[:size]


def s32(b, o):
    return struct.unpack_from("<i", b, o)[0]


def text(b, o, n=51):
    return b[o:o + n].split(b"\0")[0].decode("latin-1")


def save_date(sav):
    """The game date of a save, (year, day of the year from 0): the words at GENERAL_DATE of general.dat, else the
    latest date of the news list."""
    if sav.has("general.dat"):
        general = sav.read("general.dat")
        if len(general) >= GENERAL_DATE + 4:
            d, y = struct.unpack_from("<hh", general, GENERAL_DATE)
            if 0 <= d < 366 and 1900 < y < 3000:
                return y, d
    news = sav.read("news.dat")
    dates = [struct.unpack_from("<hh", news, o) for o in range(8 + 0xC9, len(news) - 3, 223)]
    dates = [(y, d) for d, y in dates if 0 <= d < 366 and 1900 < y < 3000]
    return max(dates) if dates else (0, 0)


class Game:
    def __init__(self, path):
        sav = LazySav(path)
        self.path = path
        cont = sav.read("continent.dat")
        self.continents = {s32(cont, i * CONT): CONF_IDS[s32(cont, i * CONT)] if 0 <= s32(cont, i * CONT) < 6
                           else text(cont, i * CONT + 4, 26) for i in range(len(cont) // CONT)}
        nat = sav.read("nation.dat")
        self.nations = {}
        for i in range(len(nat) // NATION):
            r = nat[i * NATION:(i + 1) * NATION]
            self.nations[s32(r, 0)] = (text(r, 4), self.continents.get(s32(r, 0x71), ""))
        self.clubs, self.sides, self.side_names = {}, set(), []
        self.divisions = {}                       # club id: (division, last division, European mark, last place)
        for part in ("club.dat", "nat_club.dat"):
            data = sav.read(part)
            for i in range(len(data) // CLUB):
                r = data[i * CLUB:(i + 1) * CLUB]
                self.clubs[s32(r, 0)] = (text(r, 4), s32(r, 0x53))
                if part == "club.dat" and len(r) >= 0x1df:
                    self.divisions[s32(r, 0)] = (s32(r, 0x57), s32(r, 0x5b), s32(r, 0x1db), r[0x5f])
                if part == "nat_club.dat":
                    self.sides.add(s32(r, 0))
                    self.side_names.append((text(r, 4), s32(r, 0x53)))
        self.comps = {}
        for kind, part in (("club", "club_comp.dat"), ("nation", "nation_comp.dat")):
            data = sav.read(part)
            for i in range(len(data) // COMP):
                r = data[i * COMP:(i + 1) * COMP]
                name = text(r, 4)
                if name:
                    self.comps[s32(r, 0)] = {"kind": kind, "name": name, "short": text(r, 0x38, 26),
                                             "continent": s32(r, 0x59), "nation": s32(r, 0x5d),
                                             "level": struct.unpack_from("<h", r, 0x69)[0]}
        self.rows = []
        for part in ("club_comp_history.dat", "nation_comp_history.dat"):
            data = sav.read(part)
            for i in range(len(data) // HIST):
                rid, cid, year, a, b, c, h = struct.unpack_from("<iiHiiii", data, i * HIST)
                if cid >= 0 and cid in self.comps:
                    self.rows.append((cid, year, a, b, c, h))
        self.date = save_date(sav)
        self.start, self.changing = 2001, 0
        if sav.has("bosman_season.dat"):
            part = sav.read("bosman_season.dat")
            if len(part) >= 16:
                self.start, self.changing = struct.unpack_from("<ii", part, 8)
            self.data_names = data_names(sav, part)
        else:
            self.data_names = {}
        # the host table (HostCountry.tmp: a u16 count, then rows of 0x22 bytes; HostCountry save 0x5fb320, load
        # 0x5fb160; row: +0 the competition's key id value (its record id), +4 s16 year, +6 six s32 nation ids, the
        # three host pairs, +0x1e the choice: 0 to 2 the drawn pair, 0xfd fixed, 0xfe not decided; season_intl.c)
        self.host_rows = []
        if sav.has("HostCountry.tmp"):
            part = sav.read("HostCountry.tmp")
            count = struct.unpack_from("<H", part, 0)[0] if len(part) >= 2 else 0
            for i in range(min(count, (len(part) - 2) // HOST_ROW)):
                o = 2 + i * HOST_ROW
                comp, year = struct.unpack_from("<ih", part, o)
                self.host_rows.append((comp, year, list(struct.unpack_from("<6i", part, o + 6)), part[o + 0x1e]))
        self.fixture_list, self.unread = None, []
        self.sav, self.fixture_parts = sav, sorted(n for n in sav.names() if FIXTURES.match(n))
        self.editions, self.parts = {}, set()
        for n in sav.names():
            m = PART.match(n)
            if not m or int(m.group(2)) not in self.comps:
                continue
            self.parts.add(int(m.group(2)))
            if sav.size(n) > 1 << 20:
                continue
            found = EDITION.search(sav.read(n))
            if found:
                self.editions[int(m.group(2))] = struct.unpack("<H", found.group(1))[0]

    def club(self, cid):
        """(name, nation, confederation) of a club or national side id; empty for -1."""
        if cid < 0 or cid not in self.clubs:
            return ("", "", "")
        name, nat = self.clubs[cid]
        nname, conf = self.nations.get(nat, ("", ""))
        return (name, nname, conf)


def tsv(path):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t"))


class Rules:
    """The expected rules of competitions.tsv, found by competition name through history_names.tsv."""

    def __init__(self, path=None, names=None):
        self.rows = {r["comp"]: r for r in tsv(path)} if path else {}
        self.names = [(re.compile(r["pattern"], re.I), r["comp"]) for r in tsv(names)] if path and names else []

    def key(self, name):
        """The key of a competition name in the names file, or None (a key needs no row in the rules file)."""
        bare = SEASON.sub("", name)
        return next((k for pat, k in self.names if pat.fullmatch(bare)), None)

    def get(self, name):
        """{} or {key, kind, confs (a set, or None: any), has_host (None, 0 or 1), cycle, end}."""
        key = self.key(name)
        r = self.rows.get(key)
        if not r:
            return {}
        confed = r.get("confed", "")
        cycle = number(r.get("cycle"))
        return {"key": key, "kind": r.get("kind", ""), "has_host": number(r.get("has_host")),
                "confs": set(confed.split("|")) if confed and set(confed.split("|")) <= set(CONF_IDS) else None,
                "cycle": cycle or None, "end": number(r.get("end_year")), "ended": cycle == 0,
                "years": sorted(int(y) for y in re.findall(r"\d{4}", r.get("table_years", "")))}


def no_place(entrants, free):
    """entrant-route: the indexes of entrants [(nation, confederation)] that come by no route and find no free place.
    A free place is (nation, confederations) of a route that did not play: a club of that nation for a national
    route, else of one of the confederations (None: any). The places go by a maximum matching (augmenting paths),
    not first fit: a place open to two confederations (the Copa Libertadores, CONMEBOL or CONCACAF) or to any (a
    route with no rules row) must not take the club that another place needs (open item 228)."""
    fits = [[k for k, (rn, cs) in enumerate(free) if (nat == rn if rn else cs is None or conf in cs)]
            for nat, conf in entrants]
    holder = {}                                               # free place index: entrant index

    def place(i, seen):
        for k in fits[i]:
            if k not in seen:
                seen.add(k)
                if k not in holder or place(holder[k], seen):
                    holder[k] = i
                    return True
        return False

    return [i for i in range(len(entrants)) if not place(i, set())]


def read_known(path):
    if not path or not os.path.exists(path):
        return []
    return [r for r in tsv(path) if r.get("check")]


def rules_of(game):
    """The changing-rules setting of a game, as the column rules of history_known.tsv names it."""
    return "on" if game.changing else "off"


def year_matches(spec, year, save=None):
    """Does the year column of a known row cover a fault of this year in a save of year save? "*" covers every year,
    a year that year, and a range "A-B" the years A to B when the save is also in A to B (a fault of a gap in real
    history lasts only while the real gap lasts, so a save after B is not covered). A range needs the save year."""
    if spec in ("*", str(year)):
        return True
    m = re.fullmatch(r"(\d{4})-(\d{4})", spec)
    return bool(m) and save is not None and int(m[1]) <= year <= int(m[2]) and int(m[1]) <= save <= int(m[2])


def known_status(known, check, comp, year, rules=None, save=None):
    """"accepted", "known" or None for a fault. The competition of a known row may hold * for any text (fnmatch). A row
    with a rules condition ("on" or "off" in the column rules) matches only a game with that changing-rules setting
    (rules: "on" or "off"; None matches no conditional row). A row whose column accepted is "design" or "real"
    cites a source that shows the game matches the original or real life: accepted, not a fault (Q12). Another matching
    row is known: a fault that waits for its fix. An accepted row wins over a known one. The year of a row may be a
    range A-B (see year_matches), so that one sourced row accepts a "missing" fault of a real gap for the whole gap."""
    found = None
    for k in known:
        if k["check"] != check or not fnmatch.fnmatchcase(comp, k["competition"]) or not year_matches(k["year"], year, save):
            continue
        if (k.get("rules") or "") not in ("", rules):
            continue
        if (k.get("accepted") or "") in ("design", "real"):
            return "accepted"
        found = "known"
    return found


def is_known(known, check, comp, year, rules=None, save=None):
    return known_status(known, check, comp, year, rules, save) is not None


def number(v):
    try:
        return int(v)
    except (TypeError, ValueError):
        return None


def table(game):
    """[(competition id, kind, name, year, first, second, third, host)] sorted by competition and year; each
    place is (name, nation, confederation)."""
    out = []
    for cid, year, a, b, c, h in game.rows:
        comp = game.comps[cid]
        out.append((cid, comp["kind"], comp["name"], year, game.club(a), game.club(b), game.club(c), game.club(h)))
    return sorted(out, key=lambda r: (r[2], r[0], r[3]))


def idle_is_next(edition, save_year, cyc):
    """True when the year 2199 or 2200 is the real next edition of a class with a cycle, not its idle year: the
    save is in the year before it or in the same year (a save in 2199 of a four-year cup with editions in 2196 and
    2200). The idle year of rt_intl_next_year is a fault only when the edition is more than one cycle and one year
    after the save date (open item 179)."""
    return bool(cyc) and edition <= save_year + cyc + 1


def cycle_of(years, start):
    """The most common step of the last 6 editions before the start year, or None."""
    before = sorted(y for y in set(years) if y < start)[-7:]
    steps = [b - a for a, b in zip(before, before[1:]) if b > a]
    if not steps:
        return None
    best = collections.Counter(steps).most_common()
    return max(s for s, n in best if n == best[0][1]) if len(steps) > 1 else steps[0]


# Competitions with several rows of one label by their format (docs/research/2026-10-17-competition-history-fill.md,
# "Two rows per label year"): (name pattern, first label year, rows a year).
# Guest clubs of another confederation that real rules let into a competition: {key: [(nation, first, last
# label year)]} (open item 178). The Recopa Sudamericana of a year takes the winners of the Copa Libertadores and
# the Copa Sudamericana of the year before. Mexican clubs played the Copa Libertadores from 1998 to 2016 ("In
# 1998, Mexican teams were invited to compete and competed regularly from 2000 until 2016", en.wikipedia.org,
# Copa Libertadores) and the Copa Sudamericana from 2005 to 2008. The Recopa was not held from 1999 to 2002 and came
# back in 2003 (en.wikipedia.org, Recopa Sudamericana), so a Mexican club can play the Recopa of 2003 to 2017.
# Pachuca, winner of the 2006 Copa Sudamericana, played the 2007 Recopa against Internacional (en.wikipedia.org,
# 2007 Recopa Sudamericana).
GUESTS = {"recopa": [("Mexico", 2003, 2017)]}


def guest(key, nation, year):
    """1 when real rules let a club of nation into the competition of key in year (GUESTS)."""
    return any(n == nation and a <= year <= b for n, a, b in GUESTS.get(key, ()))


SEVERAL_TITLES = ((re.compile(r"(UEFA )?Inter-?Toto Cup", re.I), 0, 3),               # one row per joint winner
                  (re.compile(r"Danish (2nd|Second) Divisions?", re.I), 2006, 2))       # East and West from 2006
# Years of a competition with more titles than its format gives (competition, label year): rows. The AFA named Rosario
# Central "Campeón de Liga 2025" for the annual table, beside the Apertura and Clausura winners (20 November 2025;
# lacapital.com.ar/c10230859 and canal26.com, read 1 November 2026). The October 2026 database holds the three rows.
# New South Wales played two top-tier seasons that started in 2000 (open item 177): the winter season 2000 (Super
# League, Blacktown City won the grand final) and the first summer season 2000/01 (Premier League, Bonnyrigg White
# Eagles won the grand final); rsssf.org tablesa/aus00.html and aus01.html, read 7 November 2026. The base data of
# 2001 holds both at label 2000, the start year of a season (docs/research/history-0.6.0/aus-rsa.md).
TITLES_OF_YEAR = {("Argentine Premier Division", 2025): 3,
                  ("New South Wales Super League", 2000): 2}
# Open item 176 (Autumn 2013 rows, sources read 7 November 2026). The Belgian Third Division played in two groups:
# R. Mouscron-Peruwelz won group A and KFC Dessel Sport group B in 2011/12 (nl.wikipedia.org, Derde klasse 2011-12
# (voetbal Belgie)); the game renames the record Belgian National Division 1, later Belgian Division 1. The CFA of
# 2012/13 played in four groups, won by USL Dunkerque, RC Strasbourg, US Colomiers and Lucon VF
# (fr.wikipedia.org, Championnat de France amateur 2012-2013).
TITLES_OF_YEAR.update({("Belgian Third Division", 2012): 2, ("Belgian National Division 1", 2012): 2,
                       ("Belgian Division 1", 2012): 2, ("French CFA", 2013): 4})


# The League of Ireland's calendar move (docs/research/2026-11-15-ireland-calendar-2003.md, decision 8): the 2001/02
# season keeps the original's label, the year it ends (2002), and the short season 2002/03 (5 July 2002 to 8 February
# 2003) takes the label 2002 too, in the game's rows (season_calendar_label, src/recomp_rt/win/season_calendar.c) and
# in the researched rows (tools/seasons/make_history_table.py, label_of). Real seasons: docs/research/history-0.6.0/
# ireland.md and ireland.check.md (RSSSF, Wikipedia, read 17 October 2026). The League Cup was not held in 2002/03
# (decision 5), so it has one row. The names are those of the save (the records of the Northern Irish league keep
# the database names "Irish First Division" and "Irish League Cup", so those names are not in the list).
TWO_SEASONS = ("League of Ireland Premier Division", "League of Ireland First Division", "FAI Cup")


def two_seasons(name, year, start, changing):
    """2 for label 2002 of a League of Ireland competition in a game that has the calendar move: changing rules on, or
    a start in 2003 or later (decision 2: a 2001 or 2002 start with changing rules off stays winter); else 1."""
    return 2 if name in TWO_SEASONS and year == 2002 and (changing or start >= 2003) else 1


def titles_allowed(name, year, start, most, several, played=True):
    """The rows a competition may hold in a label year: most (1, or 2 for a split season), the count of its
    SEVERAL_TITLES format from that format's first year, or the count of TITLES_OF_YEAR. TITLES_OF_YEAR counts only
    before the start year: those rows come from the database, and a game writes its own rows with its own format.
    played False: the game does not play the competition (no .tmp part), so every row comes from the database and
    TITLES_OF_YEAR counts in every year."""
    return max(most, several[0] if year >= several[1] else 1,
               TITLES_OF_YEAR.get((name, year), 1) if year < start or not played else 1)


def filed_titles(per_year, year, start):
    """The rows a record may hold in a label year when the database files several championships under it (open item
    176: Autumn 2013 files the champions of Alagoas, Ceara, Paraiba and others under the Brazilian Northeast record,
    3 or 4 rows every year). k is the largest count of 2 or more that more than half of the 10 years before the start
    with rows reach. Before the start the record may hold k rows. In the start year it may hold at most the rows of
    the year before: at the creation the game writes the row of its own championship (League::record_season_honours
    0x684640, 0x4afce0 at 0x684771) and the data row of that championship goes (season_history.c). After the start
    the game writes one row a year. 1 for a record with no such years."""
    years = [per_year.get(y, 0) for y in range(start - 10, start) if per_year.get(y, 0)]
    k = max((k for k in range(2, max(years, default=0) + 1) if sum(1 for n in years if n >= k) * 2 > len(years)),
            default=1)
    if k == 1 or year > start:
        return 1
    return k if year < start else min(k, per_year.get(start - 1, 0) or k)


def data_names(sav, part):
    """{competition id: its name in the data} of each record that the season code renamed, from bosman_season.dat
    (season.c, season_change_part_write): 16 bytes, the news (s32 count; per text s32 season, s32 posted, s32
    length and the text), then the names (s32 count; per name s32 index into the club comp records, then the
    nation comp records, and 51 bytes). {} for a part of 16 bytes (no names) or one that does not read."""
    try:
        at = 20
        for _ in range(s32(part, 16)):
            at += 12 + s32(part, at + 8)
        count, at = s32(part, at), at + 4
        ids = [s32(d, o) for d in (sav.read("club_comp.dat"), sav.read("nation_comp.dat"))
               for o in range(0, len(d) - COMP + 1, COMP)]
        out = {}
        for _ in range(count):
            out[ids[s32(part, at)]] = part[at + 4:at + 55].split(b"\0")[0].decode("cp1252")
            at += 55
        return out
    except (struct.error, IndexError, ValueError):
        return {}


FRIENDLY = re.compile(r"^(Friendly|Reserve|'B' International|Under 21 International)")


def check(game, rules=None, changes=()):
    """[(check, competition, year, detail)]. changes: the rows of nations.tsv; a nation that moves to another
    confederation after a row's year (Australia to the AFC in 2006) is not checked in that row."""
    rules = rules or Rules()
    moved = collections.defaultdict(list)
    for c in changes:
        if c["kind"] == "confed":
            moved[c["nation"]].append(c["season"])
    by = collections.defaultdict(list)
    for r in table(game):
        by[r[0]].append(r)
    raw = collections.defaultdict(list)
    for cid, year, a, *_ in game.rows:
        raw[cid].append(a)
    save_year, start = game.date[0], game.start
    faults = []
    for cid, rs in by.items():
        comp = game.comps[cid]
        name = comp["name"]
        if FRIENDLY.match(name):
            continue
        rule = rules.get(name)
        if rule and not game.changing:
            rule = dict(rule, years=[])               # the table's years are those of changing rules (not_held)
        sides = sum(1 for a in raw[cid] if a in game.sides)
        teams = "nation" if sides * 2 > len(raw[cid]) else "club"
        international = comp["nation"] < 0
        years = [r[3] for r in rs]
        game_rows = [r for r in rs if r[3] >= start]
        # the same year twice (from the year before the start: older duplicates are rows of the database); a split
        # season (Apertura and Clausura) holds two rows a year
        per_year = collections.Counter(years)
        before = [per_year[y] for y in range(start - 10, start) if per_year[y]]
        split_nation = game.nations.get(comp["nation"], ("", ""))[0] in ("Argentina", "Mexico")
        most = 2 if before and (sum(1 for k in before if k == 2) * 2 > len(before) or
                                (split_nation and any(per_year[y] == 2 for y in per_year if y < start))) else 1
        several = next((k, f) for pat, f, k in SEVERAL_TITLES if pat.fullmatch(name)) \
            if any(pat.fullmatch(name) for pat, f, k in SEVERAL_TITLES) else (1, 0)
        for y, n in sorted(per_year.items()):
            if n > max(titles_allowed(name, y, start, most, several, cid in game.parts),
                       two_seasons(name, y, start, game.changing),
                       filed_titles(per_year, y, start)) and y >= start - 1:
                pairs = "; ".join(f"{r[4][0]} / {r[5][0]}" for r in rs if r[3] == y)
                faults.append(("duplicate", name, y, f"{n} rows: {pairs}"))
        # a missing host where the competition has hosts
        hosted = [r for r in rs if r[7][0]]
        if rule.get("has_host") == 1 or (rule.get("has_host") is None and len(hosted) * 2 > len(rs)):
            for r in game_rows:
                if not r[7][0]:
                    faults.append(("no-host", name, r[3], f"{r[4][0]} / {r[5][0]}, no host ({len(hosted)} of "
                                                         f"{len(rs)} rows have one)"))
        # a winner or runner-up from the wrong confederation
        if not international:
            conf = game.nations.get(comp["nation"], ("", ""))[1]
        elif "confs" in rule:
            conf = "|".join(sorted(rule["confs"])) if rule["confs"] else ""
        else:
            conf = game.continents.get(comp["continent"], "")
            older = [r[4][2] for r in rs if r[3] < start and r[4][2]]
            if not conf and len(older) >= 3:
                top, n = collections.Counter(older).most_common(1)[0]
                conf = top if n * 5 >= len(older) * 4 else ""
        # the competition record on another continent than its rules (open item 21: the UEFA Nations League built
        # on the record of the Asian Cup qualifiers, continent Asia)
        record = game.continents.get(comp["continent"], "")
        if rule.get("confs") and record in CONF_IDS and record not in rule["confs"]:
            faults.append(("comp-continent", name, save_year, f"the record has continent {record}, the competition "
                                                              f"is {'|'.join(sorted(rule['confs']))}"))
        checks = [(name, conf)] if conf else []
        # the short name (+0x38, the name of the history screen) of another competition than the long name
        # (open item 4: the UEFA Nations League record keeps the short name "Asian Cup Quals"): the rows are
        # checked against the rule of the short name too, under that name
        short = rules.get(comp["short"]) if comp["short"] else {}
        if short and rule and short["key"] != rule["key"]:
            faults.append(("comp-name", name, save_year, f"the short name {comp['short']} is the competition "
                                                         f"{short['key']}, the name is {rule['key']}"))
            if short.get("confs"):
                checks.append((comp["short"], "|".join(sorted(short["confs"]))))
        for shown, conf in checks:
            for r in game_rows:
                for role, p in (("winner", r[4]), ("runner-up", r[5])):
                    if p[0] and p[2] and p[2] not in conf.split("|") and \
                            not any(r[3] < y for y in moved.get(p[1], ())) and not guest(rule.get("key"), p[1], r[3]):
                        faults.append(("confederation", shown, r[3], f"{role} {p[0]} ({p[1]}, {p[2]}), "
                                                                     f"the competition is {conf}"))
        # the same two finalists more than 2 editions in a row
        finals = rule["kind"] != "league" if rule.get("kind") else (teams == "nation" or international)
        if finals:
            run, last = 0, None
            for r in sorted(rs, key=lambda r: r[3]):
                pair = frozenset((r[4][0], r[5][0])) if r[4][0] and r[5][0] else None
                run = run + 1 if pair and pair == last else 1
                last = pair
                if run == 3 and r[3] >= start:
                    faults.append(("same-final", name, r[3], f"{r[4][0]} and {r[5][0]} in 3 editions to {r[3]}"))
        # a gap in the cycle and editions missing up to the save date: for a competition that the game runs
        # (it has a .tmp part) and that was held near the start
        cyc = rule.get("cycle") or cycle_of(years, start)
        end = rule.get("end")
        edition = game.editions.get(cid)
        ys = sorted(set(years))
        if rule.get("ended"):
            continue                                  # ended by the rules: no later edition is expected
        if cyc and ys and cid in game.parts and ys[-1] >= start - 2 * cyc:
            for a, b in zip(ys, ys[1:]):
                due = [y for y in range(a + cyc, b, cyc)
                       if not not_held(rule, a, y, cyc) and not before_start(rule.get("key"), [y], start)]
                if b >= start and a >= start - cyc and b - a > cyc + 1 and due and not (end and a >= end):
                    faults.append(("gap", name, b, f"no edition between {a} and {b} (cycle {cyc})"))
            ended = edition == ENDED_YEAR or (end is not None and end < save_year)
            due = due_edition(rule, ys[-1], cyc, start)
            if not ended and due + 1 < save_year:
                faults.append(("missing", name, due,
                               f"last edition {ys[-1]}, cycle {cyc}, the save is in {save_year}"))
        # the next draw or start more than one cycle after the save date
        if edition in IDLE_YEARS and not idle_is_next(edition, save_year, cyc):
            faults.append(("next-edition", name, edition, f"the class waits for {edition} (save in {save_year})"))
        elif edition and edition != ENDED_YEAR and cyc and edition > save_year + cyc + 1:
            faults.append(("next-edition", name, edition, f"next edition {edition}, the save is in {save_year}, "
                                                          f"cycle {cyc}"))
    faults += euro_places(game, rules)
    return sorted(faults, key=lambda f: (f[1], f[0], f[2]))


EURO_PLACE_MAX = 6      # the lowest league place with a Champions League place (5 in England 2025/26, one more for a
                        # European Performance Spot); a title holder's place is not counted, and each title holder
                        # placed above a club in its division moves the limit down one place (open item 206): the
                        # holders enter the league phase (UEFA Champions League regulations 2026/27, Art. 3.03) and
                        # the spot goes to the highest club of those that do not (Art. 3.08). In 2030/31 of pass 4b
                        # the Premier League's 5th and 6th held the Europa and Champions League titles, so 7th got it.


def euro_places(game, rules=None):
    """[(check, competition, year, detail)]: euro-place, a club with a Champions League place whose division of the
    last season is not its nation's top division, or whose last place there is below EURO_PLACE_MAX (the places come from the last season's table: Uefa_Seeding's
    collect_european_clubs 0x904040 reads club +0x5b and +0x5f), unless it won a UEFA competition in the year of the
    save (a Champions League or Europa League title holder's place). Read from 21 June (day 172, the day after the
    places, Uefa_Seeding on the UEFA cups' update day 171) to the end of August of the save's year (open item 94: a Spanish season change held for the Segunda's play-off came after the places,
    and the Champions League took Segunda clubs and the bottom of the Primera)."""
    rules = rules or Rules()
    if not game.divisions or not 172 <= game.date[1] <= 243:
        return []
    year = game.date[0]
    cl = {cid for cid, c in game.comps.items() if c["kind"] == "club" and rules.key(c["name"]) == "uefa_cl"}
    holders = {a for cid, y, a, *_ in game.rows if y == year and rules.key(game.comps[cid]["name"]) in
               ("uefa_cl", "uefa_el")}
    top = {}                                  # nation id: the highest level of its divisions that hold clubs
    for div, last, mark, place in game.divisions.values():
        c = game.comps.get(div)
        if c and c["kind"] == "club" and c["nation"] >= 0:
            top[c["nation"]] = max(top.get(c["nation"], -1), c["level"])
    faults = []
    for club, (div, last, mark, place) in sorted(game.divisions.items()):
        name, nation = game.clubs.get(club, ("", -1))
        c = game.comps.get(last)
        if mark not in cl or club in holders or not c or c["nation"] not in top:
            continue
        nname = game.nations.get(nation, ('', ''))[0]
        if c["level"] < top[c["nation"]]:
            faults.append(("euro-place", game.comps[mark]["name"], year,
                           f"{name} ({nname}): a place from the last division {c['name']}, not the nation's top "
                           f"division"))
        elif place - sum(1 for h in holders if h in game.divisions and game.divisions[h][1] == last and
                         game.divisions[h][3] < place) > EURO_PLACE_MAX:
            faults.append(("euro-place", game.comps[mark]["name"], year,
                           f"{name} ({nname}): a place from position {place} of {c['name']}"))
    return faults


def read_nations(path):
    """The rows of nations.tsv: [{kind, season, from (month, day), nation, to}]."""
    if not path or not os.path.exists(path):
        return []
    out = []
    for r in tsv(path):
        month, day = (int(x) for x in r["from"].split("-"))
        try:
            datetime.date(int(r["season"]), month, day)
        except ValueError:
            raise SystemExit(f"{path}: {r['nation']} {r['season']}: the date {r['from']} does not exist")
        out.append({"kind": r["kind"], "season": int(r["season"]), "from": (month, day), "nation": r["nation"],
                    "to": r["to"]})
    return out


def due(game, change):
    """1 when a change of nations.tsv holds at the save date: with changing rules on from its date, with changing
    rules off only when its season is not after the start year (the data of a later start holds it)."""
    if not game.changing:
        return change["season"] <= game.start
    month, day = change["from"]
    year = game.date[0]
    start = (datetime.date(change["season"], month, day) - datetime.date(change["season"], 1, 1)).days
    return change["season"] <= game.start or (year, game.date[1]) >= (change["season"], start)


def check_nations(game, changes):
    """[(check, nation, season, detail)]: a live record (with a continent) under a name that a due rename
    replaced, and a nation in another confederation than a due move gives. A dead record (no continent) beside
    the live one of its later name is no fault."""
    faults = []
    live = {}
    for nid, (name, conf) in game.nations.items():
        if conf:
            live.setdefault(name, []).append(conf)
    year = game.date[0]
    for c in changes:
        if not due(game, c):
            continue
        if c["kind"] == "rename" and c["nation"] in live:
            faults.append(("nation-name", c["nation"], c["season"],
                           f"a live record named {c['nation']} in {year}; from {c['season']} the name is {c['to']}"))
        if c["kind"] == "rename":
            for side, nid in game.side_names:
                nation, conf = game.nations.get(nid, ("", ""))
                if side == c["nation"] and conf and nation != side:
                    faults.append(("side-name", side, c["season"], f"the national side of {nation} "
                                                                   f"is named {side} in {year}; from {c['season']} "
                                                                   f"the name is {c['to']}"))
                    break
        if c["kind"] == "confed":
            for conf in live.get(c["nation"], []):
                if conf != c["to"]:
                    faults.append(("nation-confed", c["nation"], c["season"],
                                   f"{c['nation']} is in {conf} in {year}; from {c['season']} it is in {c['to']}"))
    return faults


# ---- Entrants ----

def fixtures(game):
    """{competition id: [(day, home, away)]} from the fixtures parts (fixtures_<year>.tmp: s32 count, then records
    of 0x4f bytes: +0 competition id, +0xc home, +0x10 away, +0x28 s16 year, +0x2a s16 day of the year from 0;
    docs/research/2026-10-15-holiday-next-match-inventory.md). day: a day number (date.toordinal). A part whose
    size is not 4 + count * 0x4f is not read and is named in game.unread."""
    if game.fixture_list is not None:
        return game.fixture_list
    out = collections.defaultdict(list)
    for n in game.fixture_parts:
        b = game.sav.read(n)
        count = s32(b, 0) if len(b) >= 4 else -1
        if count < 0 or 4 + count * FIXTURE != len(b):
            game.unread.append(n)
            continue
        for i in range(count):
            o = 4 + i * FIXTURE
            cid = s32(b, o)
            home, away = struct.unpack_from("<ii", b, o + 0xC)
            y, d = struct.unpack_from("<hh", b, o + 0x28)
            if cid in game.comps and 1900 < y < 3000 and 0 <= d < 366:
                out[cid].append((datetime.date(y, 1, 1).toordinal() + d, home, away))
    game.fixture_list = out
    return out


CL_LIMITS = [(2024, 7), (2015, 5), (2009, 4)]   # season_uefa_cl_cap: the first season of each association limit


def limit_faults(name, year, first, nations):
    """[(check, competition, year, detail)]: the nations of a Champions League edition (its first match day, an
    ordinal, gives the season) with more entrants than the association limit of the season (open item 70)."""
    season = datetime.date.fromordinal(first).year
    cap = next((c for f, c in CL_LIMITS if season >= f), None)
    over = sorted((n, k) for n, k in collections.Counter(nations).items() if cap and k > cap)
    return [("entrant-limit", name, year, f"{n} has {k} entrants, the limit of {season}/{(season + 1) % 100:02d} "
                                          f"is {cap}") for n, k in over]


def editions_of(fx):
    """[(year, first day, last day, teams)]: the fixtures of a competition split after a final (a day with one match
    and then FINAL_GAP days or more with none) and at a gap of more than GAP days, each an edition with the year of
    its last match. A winter break comes after a day of many matches, so a club cup from July to May stays one
    edition. A day with one match that ends a group stage is no final (open item 70: the knockout rounds of
    February after a group stage whose last day in December had one match). It counts as the end of a group stage
    when the match day before it had more than two matches (a semi-final day has one or two) and every team of the
    next day played in the RECENT days before the break. Nor is it a final when the next day has one match too and
    its teams played in the RECENT days before: a knockout round that goes on after a break (the Copa Libertadores
    of 2041, single-match days on 5, 9 and 15 June, then 10, 17 and 24 July). A new edition starts with a draw of
    more ties. Nor is a gap of more than GAP days (up to PAUSE) a new edition when a home-and-away group pauses before
    it has played one round robin: the day before it had two matches or more, the edition has fewer matches than a
    round robin of its teams, and the next day's matches are of teams of the edition only, each a home team against an
    away team that it has not met at home in the edition (the OFC Nations Cup of 2008, 0.6.2 open item 45)."""
    per_day = collections.defaultdict(list)
    for day, home, away in fx:
        per_day[day].append((home, away))
    out, seen, before, met = [], {}, None, set()
    for day in sorted(per_day):
        teams = {t for pair in per_day[day] for t in pair if t >= 0}
        last = out[-1][2] if out else None
        recent = last is not None and teams and all(last - seen.get(t, last - RECENT - 1) <= RECENT for t in teams)
        group_end = before is not None and len(per_day[before]) > 2 and recent
        paused = len(per_day[day]) == 1 and recent         # a knockout round that goes on after a break
        # a lone match up to MOVED days after a day of more than two matches is a group match that the game moved
        # (open item 201: the Club World Cup took the FIFA day of a June qualifier), not a final
        moved = last is not None and before is not None and len(per_day[before]) > 2 and last - before <= MOVED
        n = len(out[-1][3]) if out else 0
        group_paused = (last is not None and GAP < day - last <= PAUSE and len(per_day[last]) >= 2 and
                        len(met) < n * (n - 1) // 2 and
                        all(p[0] in out[-1][3] and p[1] in out[-1][3] and p not in met for p in per_day[day]))
        if not out or (day - last > GAP and not group_paused) or \
                (day - last >= FINAL_GAP and len(per_day[last]) == 1 and not group_end and not paused and not moved):
            out.append([0, day, day, set()])
            seen, last, met = {}, None, set()
        out[-1][2] = day
        out[-1][3].update(teams)
        seen.update((t, day) for t in teams)
        met.update(per_day[day])
        before = last
    return [(datetime.date.fromordinal(e[2]).year, e[1], e[2], e[3]) for e in out]


def finals_table(path=None):
    """{class: [(year, teams, [host names, each "a/b" for the names a database can use])]} of the finals table
    of the season code (season_intl_table.c), sorted by year."""
    path = path or os.path.join(ROOT, "src", "recomp_rt", "win", "season_intl_table.c")
    out = collections.defaultdict(list)
    if os.path.exists(path):
        text_ = open(path, encoding="latin-1").read()
        for cls, year, teams, hosts in re.findall(r'\{"(\w+)", \d+, (\d+), (\d+), \d+, \d+, \d+, \d+, "([^"]*)"',
                                                  text_):
            out[cls].append((int(year), int(teams), [h for h in hosts.split(";") if h]))
    for rows in out.values():
        rows.sort()
    if os.path.exists(path) and not out:
        raise ValueError(f"{path}: no row of the finals table read (has its struct changed?)")
    return out


_SEASONS = {}


def finals_seasons(path=None):
    """{class: {year: season}} of the finals table of the season code (season_intl_table.c): the season of each
    edition (2025 for the World Cup of 2026, whose finals end the 2025/26 season; 2022 for that of 2022)."""
    path = path or os.path.join(ROOT, "src", "recomp_rt", "win", "season_intl_table.c")
    if path not in _SEASONS:
        out = collections.defaultdict(dict)
        if os.path.exists(path):
            for cls, season, year in re.findall(r'\{"(\w+)", (\d+), (\d+), \d+, \d+, \d+, \d+, \d+, "',
                                                open(path, encoding="latin-1").read()):
                out[cls][int(year)] = int(season)
        _SEASONS[path] = out
    return _SEASONS[path]


def edition_season(key, year):
    """The season of a finals class's edition of a year, or None for a competition outside the finals table or a
    year of the table without an edition: the table's season, and after the table the season of the last table edition moved on by the same years
    (season_intl_rules.c season_intl_first: the class continues on its cycle)."""
    rows = finals_seasons().get(key) if key else None
    if not rows:
        return None
    if year in rows:
        return rows[year]
    last = max(rows)
    return year - (last - rows[last]) if year > last else None       # None: no edition of the table


NEWEST_DATA = 2026     # the season of the newest season database (October 2026, rules_report_check.SEASON_DATABASES)


def before_start(key, years, start):
    """1 when every edition of years is of a season before the start season, in a game that starts after the newest
    database (open item 169). Such an edition is history that only the database can hold: the game plays a finals
    class from the first edition of its start season (rt_intl_first_year, the class constructor's year from
    season_intl_first), and no database holds an edition after its own season (December 2025 in a 2098 date jump).
    A start up to NEWEST_DATA plays the database of its own season, which must hold them (with the history fill:
    the Africa Cup of Nations 2021 in a 2021/22 start, open item 170)."""
    if start <= NEWEST_DATA:
        return False
    seasons = [edition_season(key, y) for y in years]
    return bool(seasons) and all(s is not None and s < start for s in seasons)


def not_held(rule, after, year, cyc):
    """1 when the cycle's year after a row of `after` is a year without an edition by the rules table (open item
    173): inside the table's years, not one of them, and no table year lies between `after` and the next year on
    the cycle. The OFC Nations Cup of 2020 was cancelled (OFC, 21 April 2020): the table holds 2016 and 2024, so
    2020 is not held. A table year near the cycle's year is held: the Asian Cup of 2007 for the cycle's 2008 after
    2004, the Africa Cup of Nations of 2026 (its final in January 2026, the history label 2025) for 2025 after 2023.
    The table holds the years of changing rules: a game with changing rules off has rule["years"] empty."""
    table = rule.get("years") or []
    return bool(table) and table[0] < year < table[-1] and year not in table and \
        not any(after < t < year + cyc for t in table)


def due_edition(rule, last, cyc, start):
    """The first edition after the last row that the game must write: the next one on the cycle that is held
    (not_held) and not of a season before the start season (before_start)."""
    due = last + cyc
    while not_held(rule, last, due, cyc) or before_start(rule.get("key"), [due], start):
        due += cyc
    return due


def read_access(path):
    """{key: [(from, to or None, teams or None, [route keys, or "holder"])]} of access.tsv."""
    out = collections.defaultdict(list)
    if path and os.path.exists(path):
        for r in tsv(path):
            routes = [x.split(":", 1)[1] if x.startswith("winner:") else x
                      for x in r.get("routes", "").split(";") if x.startswith("winner:") or x == "holder"]
            out[r["comp"]].append((int(r["from"]), number(r["to"]), number(r["teams"]), routes))
    return out


# The original's team count of a finals class whose base game plays editions of a year the finals table gives
# another count: the OFC Nations Cup plays 6 (0x799e42) every two years, the table 8 in 2002, 2012, 2016 and 2024
# (open item 45). The season code changes the count only in a season game (season_intl_ofc.c).
BASE_TEAMS = {"oceania_nations": 6}


class Access:
    """The access rules: the finals table of the season code and access.tsv."""

    def __init__(self, table=None, access=None):
        self.table = table if table is not None else {}
        self.access = access if access is not None else {}

    def rule(self, key, year, base=False):
        """(teams or None, [host names], [route keys]) of an edition. base: a base game (2001, changing rules off),
        where a class of BASE_TEAMS keeps the original's count in every edition (season_intl_on() is 0)."""
        rows = self.table.get(key)
        if rows:
            row = next((r for r in rows if r[0] == year), None)
            teams = BASE_TEAMS[key] if base and key in BASE_TEAMS else None
            if row:
                return teams or row[1], row[2], []
            if year > rows[-1][0]:
                return teams or rows[-1][1], [], []      # after the table: the count of the last row
            return teams, [], []
        for start, end, teams, routes in self.access.get(key, ()):
            if start <= year and (end is None or year <= end):
                return teams, [], routes
        return None, [], []


def runner_up_places(editions, confed_of, home="CONMEBOL"):
    """[(year, club)]: the runner-up that takes the place of a Libertadores winner from outside CONMEBOL (a Mexican
    guest, open item 219: the best CONMEBOL club takes the place in the Intercontinental Cup, the Recopa and the other
    cups of the Libertadores route, as in real life). editions: [(year, winner, runner-up)]; confed_of(club) names the
    confederation. In a game without the fix the guest keeps the place: both clubs count as route holders."""
    return [(y, ru) for y, w, ru in editions if w >= 0 and ru >= 0 and confed_of(w) != home and confed_of(ru) == home]


def cut_by_oldest_part(start, oldest, eds, year):
    """Is the edition `year` the one that the oldest fixtures part cuts? A save keeps 50 years of fixtures, so an edition
    that began in the year before the oldest part has lost its first matches: it shows only its knockout teams, from
    the first weeks of the oldest year, fewer than the next edition (pass 5: the Africa Cup of Nations 2026 with 16 of
    24 entrants from 10 January, the Champions League holders of 2021 and 2023, Grasshopper Club Zurich 2032 and
    2041). Only in an aged save (oldest part later than the start). eds: [(year, first day, last day, teams)] of the
    competition."""
    here = next((e for e in eds if e[0] == year), None)
    return bool(here) and oldest > start and year <= oldest and \
        here[1] < datetime.date(oldest, 3, 1).toordinal() and \
        any(len(t) > len(here[3]) for y, f, l, t in eds if y > oldest)


def entrants(game, rules, access, changes=()):
    """[(competition id, name, year, first day, last day, team id, team, nation, confederation, route)] and the
    faults of the entrant checks."""
    rules = rules or Rules()
    fxs = fixtures(game)
    by_key = collections.defaultdict(list)                    # key: [(year, winner id)]
    held = collections.defaultdict(list)                      # club: [(year, competition name, place)]
    for cid, year, a, b, c, h in game.rows:
        name = game.comps[cid]["name"]
        key = rules.key(name)
        if key:
            by_key[key].append((year, a))
        for place, t in ((1, a), (2, b), (3, c)):
            if t >= 0:
                held[t].append((year, name, place))
    hosts = {(cid, year): h for cid, year, a, b, c, h in game.rows if h >= 0}
    # Item 219: the Libertadores winner outside CONMEBOL gives its place to the runner-up (pass 6b: Sporting Cristal in
    # the Intercontinental Cup 2004 and the Recopa 2005 after the América of 2004).
    lib = [(year, a, b) for cid, year, a, b, c, h in game.rows if rules.key(game.comps[cid]["name"]) == "libertadores"]
    by_key["libertadores"] += runner_up_places(lib, lambda t: game.club(t)[2])
    # The save keeps 50 years of fixtures: an edition (labelled by the year it ends) whose year is that of the oldest
    # fixtures part has lost the matches of the year before, so its entrants are only the knockout teams and its title
    # holder, out in the group stage, seems missing (open item 70; pass 5: a2013s1 F.C. Barcelona 2023, a2001nation
    # Chemnitzer FC 2021). Such an edition is not judged for the holder.
    oldest_part = min((int(m.group(1)) for n in game.fixture_parts for m in [re.search(r"(\d{4})", n)] if m), default=0)
    cut_editions = set()                                      # (competition name, year) cut by the oldest part
    route_nation = {}                                         # key of a national competition: its nation's name
    for comp in game.comps.values():
        key = rules.key(comp["name"])
        if key and comp.get("nation", -1) >= 0 and comp["nation"] in game.nations:
            route_nation[key] = game.nations[comp["nation"]][0]
    moved, due_to = collections.defaultdict(list), collections.defaultdict(set)    # moved: (day of a move, to)
    renamed = {ch["nation"]: (ch["season"], ch["to"]) for ch in changes if ch["kind"] == "rename" and due(game, ch)}
    for ch in changes:
        if ch["kind"] == "confed":
            moved[ch["nation"]].append((datetime.date(ch["season"], *ch["from"]).toordinal(), ch["to"]))
            if due(game, ch):
                due_to[ch["nation"]].add(ch["to"])            # reported once, by check_nations

    def winners(key, year):
        """The winners of a route competition in the year of the match and in its last edition before that year,
        when that edition is at most one cycle and one year before (a super cup in August follows a final in May of
        its year, the Finalissima of 2022 the Euro of 2020; a route competition that stopped gives no winner)."""
        cycle = rules.rows.get(key, {}).get("cycle")
        cycle = number(cycle) or 1
        before = max([y for y, w in by_key.get(key, ()) if y < year], default=0)
        years = {year} | ({before} if year - before <= cycle + 1 else set())
        return {w for y, w in by_key.get(key, ()) if y in years}

    def played(key, year):
        """1 when the game wrote a row of route competition key (a year after the start) in the window of winners():
        else its nations are not selected and a club of its confederation comes by the original's draw (slice 9a).
        A route that stopped by a fault of the game also counts as not played; the check `missing` reports it."""
        cycle = number(rules.rows.get(key, {}).get("cycle")) or 1
        return any(game.start < y <= year and year - y <= cycle + 1 for y, w in by_key.get(key, ()))

    save_day = datetime.date(game.date[0], 1, 1).toordinal() + game.date[1] if game.date[0] > 1900 else 0
    live = {n for n, conf in game.nations.values() if conf}
    # open item 76: the World Cup finals for the hosts after the table: its competition id (the key of its host
    # rows) and the nations of each edition's entrants
    wc_cid, wc_nations = None, {}
    host_keys = {c for c, _, _, _ in game.host_rows}
    for cid, fx in fxs.items():
        if rules.get(game.comps[cid]["name"]).get("key") == "world_cup":
            if wc_cid is None or (cid in host_keys and wc_cid not in host_keys):   # the record of the host rows
                wc_cid = cid
            for y, _, _, teams in editions_of(fx):
                wc_nations.setdefault(y, set()).update(game.club(t)[1] for t in teams if game.club(t)[2] == "UEFA")
    wc_rows = access.table.get("world_cup") or []
    wc_last = wc_rows[-1][0] if wc_rows else 9999
    listing, faults = [], []
    for cid, fx in sorted(fxs.items()):
        comp = game.comps[cid]
        name = comp["name"]
        if FRIENDLY.match(name):
            continue
        rule = rules.get(name)
        key = rule.get("key")
        short = rules.get(comp["short"]) if comp["short"] else {}
        confs = [(name, rule.get("confs"))]
        if short and short.get("key") != key and short.get("confs"):
            confs.append((comp["short"], short["confs"]))
        eds = editions_of(fx)
        for y_, f_, l_, t_ in eds:
            if cut_by_oldest_part(game.start, oldest_part, eds, y_):
                cut_editions.add((name, y_))
        for n, (year, first, last, teams) in enumerate(eds):
            # the last edition with a match fewer than RUNNING days before the save date may still run: the July
            # qualifying of a Champions League, which the title holder enters later in the group stage (open item
            # 70). A last day of one match is a final: the edition has ended (the World Cup final in mid July). A
            # final comes after a day of at most two matches (semi-finals, a third place match), as in editions_of.
            # A lone match after a day of more is a moved qualifying tie: the 2141/42 Champions League of pass 4
            # had 12 matches on 19 July and one on 20 July 2141.
            days = collections.Counter(d for d, h, a in fx if first <= d <= last)
            before = max((d for d in days if d < last), default=None)
            final = days[last] == 1 and (before is None or days[before] <= 2)
            done = last <= save_day and not (n == len(eds) - 1 and save_day - last < RUNNING and not final)
            want, host_names, routes = access.rule(key, year, game.start == 2001 and not game.changing) if key \
                else (None, [], [])
            won = {r: winners(r, year) for r in routes if r != "holder"}
            holder = "holder" in routes
            # the World Cup qualifying holds the play-off between confederations: 2 teams of other confederations
            # in a campaign (a tie such as Bahrain against Trinidad and Tobago in 2005), or an edition of at most 6
            # teams (the play-off tournament of 2026, in the record of the OFC qualifying: 2 of CONCACAF and 1 each of
            # AFC, CAF, CONMEBOL and OFC)
            playoff = (key or "").startswith("wc_qual_")
            foreign, dead, no_route = collections.defaultdict(list), [], []
            prev = [w for y, w in sorted(by_key.get(key, ())) if y < year][-1:] if key else []
            host = hosts.get((cid, year), -1)
            host_nations = [game.clubs.get(host, ("", -1))[1]] if host >= 0 else []
            host_alias = {x for h in host_names for x in h.split("/")}
            if key == "world_cup":                        # a co-host after the table (open item 76)
                host_alias |= save_hosts(game, cid, year)
            for t in sorted(teams, key=lambda t: game.club(t)):
                tname, nation, conf = game.club(t)
                route = []
                if t in prev:
                    route.append("title holder")
                if game.clubs.get(t, ("", -1))[1] in host_nations or nation in host_alias:
                    route.append("host")
                route += [f"winner {r}" for r in won if t in won[r]]
                route += [f"{'winner' if p == 1 else 'place ' + str(p)} {c} {y}" for y, c, p in held.get(t, ())
                          if y in (year - 1, year) and c != name]
                listing.append((cid, name, year, first, last, t, tname, nation, conf, "; ".join(route) or "qualifier"))
                by_route = any(t in won[r] for r in won)              # a route winner enters from any confederation
                if nation in renamed and year > renamed[nation][0]:
                    dead.append(f"{tname} ({nation}, {renamed[nation][1]} from {renamed[nation][0]})")
                for shown, cs in confs:
                    if cs and conf and conf not in cs and not by_route and not due_to[nation] & cs and \
                            not any(first < m and conf == to for m, to in moved.get(nation, ())):
                        foreign[(shown, "|".join(sorted(cs)))].append(f"{tname} ({conf})")
                if won and done and year > game.start and not by_route and all(by_key.get(r) for r in won):
                    no_route.append((nation, conf, f"{tname} ({nation}) won none of {', '.join(won)} in {year} or the edition "
                                           f"before"))
            # each route that did not play leaves one place to a drawn club of its confederation
            free = [(route_nation.get(r), set(c.split("|")) if (c := rules.rows.get(r, {}).get("confed")) else None)
                    for r in won if not played(r, year)]
            for i in no_place([(nat, conf) for nat, conf, text in no_route], free):
                faults.append(("entrant-route", name, year, no_route[i][2]))
            if dead:
                faults.append(("entrant-dead", name, year, f"{len(dead)} entrants of a nation under an old name: "
                                                           f"{', '.join(dead[:6])}"))
            if holder and done and year > game.start and year > oldest_part and prev and prev[0] not in teams and \
                    year - max(y for y, w in by_key[key] if y < year) <= (rule.get("cycle") or 1) + 1:
                faults.append(("entrant-holder", name, year, f"the title holder {game.club(prev[0])[0]} is not an "
                                                             f"entrant"))
            for (shown, cs), teams_ in sorted(foreign.items()):
                if playoff and (len(teams_) <= 2 or len(teams) <= 6):
                    continue
                faults.append(("entrant-confed", shown, year, f"{len(teams_)} entrants from another confederation "
                                                              f"than {cs}: {', '.join(teams_[:6])}"
                                                              f"{' ...' if len(teams_) > 6 else ''}"))
            # an edition of more than 500 days holds two seasons (2019/20 ended in August 2020, 425 days after its
            # start, and 2020/21 began before that): its counts are not those of one season
            # with changing rules off the base game plays the original's format: no limit of the season rules
            if key == "uefa_cl" and game.changing and done and datetime.date.fromordinal(first).year > game.start and \
                    last - first <= 500:
                faults += limit_faults(name, year, first, [game.club(t)[1] for t in teams])
            if want and done and len(teams) != want:
                faults.append(("entrant-count", name, year, f"{len(teams)} entrants, the access rule has {want}"))
            if done and not host_names and host >= 0 and rule.get("kind") == "intl_finals" and \
                    game.clubs.get(host, ("", -1))[1] not in {game.clubs.get(t, ("", -1))[1] for t in teams}:
                faults.append(("entrant-host", name, year, f"the host {game.club(host)[0]} of the history row is "
                                                           f"not an entrant"))
            if done:                                      # a host of the table that the data holds plays
                names = {game.nations.get(game.clubs.get(t, ("", -1))[1], ("", ""))[0] for t in teams}
                for h in host_names:
                    alias = set(h.split("/"))
                    if not names & alias and alias & live:
                        faults.append(("entrant-host", name, year, f"the host {h} is not an entrant"))
            if key == "wc_qual_uefa" and done and len(teams) >= MISSING_MIN and \
                    first >= datetime.date(game.start, 7, 1).toordinal() and uefa_all(game, year):
                entered = {game.club(t)[1] for t in teams}
                wc = year + (2 - year % 4) % 4
                more = save_hosts(game, wc_cid, wc) if wc_cid is not None else set()
                if wc > wc_last:
                    h = hosts.get((wc_cid, wc), -1)
                    more |= finals_hosts(wc_nations.get(wc, set()), entered, game.club(h)[1] if h >= 0 else "")
                missing = uefa_missing(game, access, changes, year, datetime.date.fromordinal(first).year, entered,
                                       more)
                if missing:
                    faults.append(("entrant-missing", name, year, f"{len(missing)} live UEFA nations do not play: "
                                                                  f"{', '.join(missing[:6])}"
                                                                  f"{' ...' if len(missing) > 6 else ''}"))
    faults = [f for f in faults if not (f[0].startswith("entrant-") and (f[1], f[2]) in cut_editions)]
    return listing, faults + duty_clashes(game)


DUTY_MIN = 3                                  # players of one club in the squad of a side that plays a day apart
SIDE_MIN = 11                                 # players a club needs to field a side (box match_eng.cpp 612 or 652)
TOP_UP_MOST = 25                              # virtual players the top-up 0x90dae0 adds at most (0x90ddbc, 0x90dde0)
PICK_AHEAD = 14                               # days: NationalTeams::daily_update picks the squads of this day ahead
OLYMPIC_NAMES = ("football tournament of the olympic games", "olympic games")   # set the key 0x9cf7a0
FITNESS = 0x1F                                # injury.dat: a fitness record per staff id (Injury::load 0x62ab00)


def olympics(game):
    """The competition ids of the Olympics: the records whose name in the data sets the key 0x9cf7a0. set_key_ids
    0x601a40 compares each record's long name with fixed names (match_club_comp_key_id 0x60efd0, _stricmp); the
    two names of that key are at 0x9db0d0 and 0x9db0c0 (src/cm3/key_ids_table.c, global 0x9cf7a0). A record that
    the season code renamed keeps its name in the data in bosman_season.dat (game.data_names), as the wrap of
    0x60efd0 gives it at a load (season_change.c)."""
    return {cid for cid, c in game.comps.items()
            if game.data_names.get(cid, c["name"]).lower() in OLYMPIC_NAMES}


def duty_clashes(game):
    """[(check, competition, year, detail)]: duty-clash, a continental club match within one day of a match of a
    national side that holds DUTY_MIN or more players of one of the two clubs in its squad (open item 93 of 0.6.2).
    The player is then on duty for his side (NationalTeams::get_international_duty_club 0x7757d0 searches the
    side's fixtures from the day before to the day after, 0x775842..0x7758a8), and a club with many of them fields
    fewer than 11 (box match_eng.cpp 612 or 652). A continental club match: a competition with no nation and two
    clubs, not a friendly.
    The Olympics (open item 107) play the under-21 sides (the last rows of nat_club.dat: club id at least the count
    [0xae235c] minus [0xae2350], 0x540a2a..0x540a40), which carry the nation's name. Their squad is the
    under-21 pick (NationalTeams::select_squad 0x76c120, slot 2 with flag 4: 0x76c26d calls 0x53b4f0, 0x76c279;
    players aged 23 or under, 0x76cee3), so the players' +0x24 is the under-21 side. The Olympics are fixture type 1
    (0x669ae0), so 0x7757d0 holds them, as the original does. The real tournament overlaps the UEFA qualifiers too
    (2024: 24 July to 9 August, second qualifying round 23/24 and 30/31 July), and clubs need not release players
    for it (it is not on the FIFA international match calendar). So an Olympic match (olympics(), by its key) is a
    fault only when a club with a player in an Olympic side that plays within a day keeps fewer than SIDE_MIN
    players it can field, as VIRTUAL_STAFF::count_available_players 0x90d8f0 counts them: in its squad slots, not on
    duty for any side that plays within a day (0x7757d0), at the club (staff +0x39, the test after it) and with no
    injury (the fitness record's injury +0xe is -1, 0x62c49c; a player who recovers, status 1 of 0x62c400, counts
    as injured here, so the count errs toward a fault), plus the virtual players of the top-up (open item 164).
    VIRTUAL_STAFF::fill_fixture_squads 0x90d2f0 tops up both clubs of each fixture before the match
    (VIRTUAL_STAFF::top_up 0x90dae0 at 0x90d3bd and 0x90d3f4). The top-up counts the available players by position
    with 0x90d8f0 (a player on duty is not available, 0x90d9c3), and for a club with a nation (+0x53, 0x90db33) that
    is not an MLS All-Stars side (0x9d00a8, 0x9d00a4; they play no continental match) it adds virtual players to
    each of the 8 positions below its target, at most TOP_UP_MOST in all (0x90ddbc, 0x90dde0). The club targets are
    fixed, 4, 4, 4, 8, 4, 8, 4, 8 (row 0 set by the constructor 0x90d140 at 0x90d255..0x90d2b5, 44 players), so a
    club that keeps fewer than 11 is short by more than TOP_UP_MOST. Each virtual player (0x90e0a0) goes into the
    first empty squad slot (0x90e714..0x90e729), so the club gets one per free slot, at most TOP_UP_MOST. The cap
    only changes the number in the text: a club with TOP_UP_MOST free slots always reaches SIDE_MIN. Wasquehal (Tier
    A game a2006s1, UEFA Super Cup on 9 August 2016) kept 10 of 15 with 5 in the French Olympic side, had 25 virtual
    players at its team pick (0x6db500 at 0x6b8866) and picked 18. Not modelled: a club with a human manager whose
    count of +0x5e players is 20 or more can count an add that places no player (0x90df68, when 0x600a50 skips
    0x90dd92); it then also needs 10 or more players injured or banned. Not counted: bans (0x55d270, the ruling
    bodies' tables of discipline.dat, not read) and the European squad list
    (EUROPE_RULES::get_ineligibility_code 0x5832e0: staff +0x6d 2 for a player aged 20 or more in the original's
    four cups, except some rounds; a July save holds almost
    no registration yet). So the count can be above the match's: item 93's club keeps 12 registered players at the
    October 2029 save and fielded 9 on 22 November. Squads: club.dat +0xd7, SLOTS staff ids (-1 for none); the
    national side of a player's squad staff.dat +0x24 (the squads of the save date). Only fixtures still to be
    played, and none in the start season of a 2001 game, which keeps the original's dates (season_friendly_dates.c
    keeps_original). The squads of the save are those of the matches up to PICK_AHEAD days after it:
    NationalTeams::daily_update 0x76b100 picks the squads of the day two weeks ahead (0x54c770 with 2 at 0x76b126; in a
    replay of a2001offs2, the pick of day 213 of 2041 is for Japan's match of day 227), and the original's call-up move
    0x7777e0 acts on them that day. A later match is not judged: its squad is not picked yet, and the pick leaves out
    the regulars of a club with a match within a day (NationalTeams::select_squad 0x76c120, 0x76cd44..0x76cd73). In
    run set pass 3 (a2001offs1) Argentina's under-21 side held 3 San Lorenzo players in the July 2041 save for its
    match of 3 September; its own pick held 2, and the senior side's for 4 September none. A club match on its
    nation's match day is no fault in itself: clubs play on international match days, and a clash is a fault only
    when a picked player is due in two places. The Olympic count above has no PICK_AHEAD limit: Olympic squads are not
    read from these picks. One fault per competition and year, with its first case.
    duty-clash-linked-u21 is the same fault for the squad of an under-21 side (the second side of its nation in
    nat_club.dat, 0x540a2a..0x540a40) whose senior side plays the next day: the linked under-21 match that add_fixture
    0x5aa680 adds the day before a senior match (ReserveComp::add_b_or_under21_fixture 0x7e6720, called at 0x5aab09),
    whose squad the senior match's check 0x76bab0 picks. That pick leaves out a club's regulars when the club has a
    match within a day (with open item 299 of 0.6.2 the day before the linked match too), but takes its players of
    playing-time mood -10 or less (the gate of select_squad, contract +0x3d, 0x76cd6f): original design, item 162.
    The mood moves after the pick, so the save does not show it. In release set row t2002 (seed 73) a replay with a
    log at the pick (run-logs/0.7.3-s073-19/r299/rep8) showed the re-pick of day 231 of 2003 leave out the Huracán
    regular (mood 100) and keep 4 Huracán players of mood -16, -96, -91 and -50, whose moods at the save of day 243
    were 6, -99, -6 and 20. The check name lets history_known.tsv accept this one case as design, and no other
    duty clash."""
    sav = game.sav
    staff = sav.read("staff.dat") if sav.has("staff.dat") else b""
    injury = sav.read("injury.dat") if sav.has("injury.dat") else b""
    people = {s32(staff, o): o for o in range(0, len(staff) - STAFF + 1, STAFF)}

    def hurt(x):
        o = FITNESS * x + 0xE
        return 0 <= o <= len(injury) - 4 and s32(injury, o) != -1

    clubs = sav.read("club.dat")
    called = {}                                   # club id: Counter of national side ids of its squad
    able = {}                                     # club id: [the national side id, or -1, of each player it can field]
    virtual = {}                                  # club id: the virtual players the top-up adds (None: no nation)
    for o in range(0, len(clubs) - CLUB + 1, CLUB):
        cid = s32(clubs, o)
        slots = struct.unpack_from(f"<{SLOTS}i", clubs, o + 0xD7)
        virtual[cid] = min(sum(1 for x in slots if x < 0), TOP_UP_MOST) if s32(clubs, o + 0x53) >= 0 else None
        ids = [x for x in slots if x >= 0 and x in people]
        called[cid] = collections.Counter(s32(staff, people[x] + 0x24) for x in ids)
        able[cid] = [s32(staff, people[x] + 0x24) for x in ids
                     if s32(staff, people[x] + 0x39) == cid and not hurt(x)]
    first = datetime.date(game.start + 1, 7, 1).toordinal() if game.start == 2001 else 0
    today = max(first, datetime.date(game.date[0], 1, 1).toordinal() + game.date[1] if game.date[0] > 1900 else 0)
    games = olympics(game)
    national = collections.defaultdict(set)       # day: national side ids that play a match that is not the Olympics
    olympic = collections.defaultdict(set)        # day: national side ids that play an Olympic match
    club_matches = []
    for cid, fx in fixtures(game).items():
        comp = game.comps.get(cid, {})
        for day, home, away in fx:
            if day < today:                       # played: its squads were others
                continue
            for t in (home, away):
                if t in game.sides:
                    (olympic if cid in games else national)[day].add(t)
            if comp.get("nation", -1) < 0 and not FRIENDLY.match(comp.get("name", "")) and home in called and \
                    away in called:
                club_matches.append((day, cid, home, away))
    by_nation = collections.defaultdict(list)    # nation: its side ids (the senior side's is the lower, nat_club.dat)
    for sid in game.sides:
        by_nation[game.club(sid)[1]].append(sid)
    faults, seen = [], set()
    for day, cid, home, away in sorted(club_matches):
        name = game.comps[cid]["name"]
        year = datetime.date.fromordinal(day).year
        if (name, year) in seen:
            continue
        near = (day - 1, day, day + 1)
        hit = next(((c, side, d) for d in near if d <= today + PICK_AHEAD for side in sorted(national.get(d, ()))
                    for c in (home, away) if called[c][side] >= DUTY_MIN), None)
        when = datetime.date.fromordinal(day)
        if hit:
            c, side, d = hit
            seen.add((name, year))
            nation = game.club(side)[1]
            linked = side == max(by_nation.get(nation, [side])) and min(by_nation.get(nation, [side])) in \
                national.get(d + 1, ())
            faults.append(("duty-clash-linked-u21" if linked else "duty-clash", name, year, f"{game.club(c)[0]} on {when} has "
                                                     f"{called[c][side]} players in the squad of {game.club(side)[0]}, "
                                                     f"which plays on {datetime.date.fromordinal(d)}"))
            continue
        busy = set().union(*(national.get(d, set()) | olympic.get(d, set()) for d in near))
        games_near = set().union(*(olympic.get(d, set()) for d in near))
        for c in (home, away):
            away_now = sorted(s for s in games_near if called[c][s])
            keeps = sum(1 for s in able[c] if s not in busy)
            if away_now and keeps + (virtual[c] or 0) < SIDE_MIN:
                seen.add((name, year))
                squads = ", ".join(f"{called[c][s]} of {game.club(s)[0]}" for s in away_now)
                more = virtual[c] or 0
                faults.append(("duty-clash", name, year, f"{game.club(c)[0]} on {when} keeps {keeps} players it can "
                                                         f"field and {more} virtual player{'' if more == 1 else 's'}, "
                                                         f"with players in the Olympic squads ({squads})"))
                break
    return faults


MISSING_MIN = 30                              # the group stage of a European qualifying (a play-off edition is less)


def uefa_all(game, year):
    """1 when the season code puts every UEFA member in the European World Cup qualifying edition of this year: a
    season game (not 2001 with changing rules off, season_intl_on), for the World Cups of 2006 to 2022
    (season_wcq26.c europe_wide) and from 2026 with changing rules on or a start from 2025 (season_wcq26, Q232).
    The 2002 campaign keeps the original's holder and cap."""
    wc = year + (2 - year % 4) % 4
    if game.start == 2001 and not game.changing:
        return 0
    return 2006 <= wc <= 2022 or (wc >= 2026 and (game.changing or game.start >= 2025))


def finals_hosts(finals, entered, history_host):
    """The UEFA nations of a World Cup's finals (`finals`) that did not play its qualifying (`entered`), taken as its
    hosts (open item 76): a season game has no holder place after 2006 (rt_intl_wc_holder), so such a nation came by
    a host place. Only for a World Cup after the finals table, whose host row the save no longer holds (the row moves
    on once its finals are past, HostCountry::daily_update 0x5faafa); only when the history row's host is among them
    and they are at most a pair. Else none, so a nation that a section fault put in the finals stays a fault."""
    out = set(finals) - set(entered)
    return out if history_host in out and len(out) <= 2 else set()


def save_hosts(game, comp, year):
    """The hosts in the save's host table of competition id `comp` in `year`, as season_intl_host_ids reads them
    (season_intl.c): the first row of that competition and year; a fixed row gives the nations of its six fields, a
    drawn row its pair, a row not decided none. Ids outside the nation table are left out."""
    for c, y, ids, choice in game.host_rows:
        if c != comp or y != year:
            continue
        if choice == HOST_FIXED:
            pick = ids
        elif 0 <= choice <= 2:
            pick = ids[2 * choice:2 * choice + 2]
        else:
            pick = []
        return {game.nations[i][0] for i in pick if i in game.nations}
    return set()


def uefa_missing(game, access, changes, year, first_year, entered, more_hosts=()):
    """The live UEFA nations with a national side that are not entrants of a European World Cup qualifying edition
    (open item 57 of 0.6.2): every UEFA member enters, less the European hosts of its World Cup (the first World
    Cup year from the edition's year), less Russia from the 2026 cycle (Q29) and less a nation that joins UEFA
    after the edition's first year (nations.tsv: Kosovo from 2016). The hosts are those of the finals table
    (access.tsv) and more_hosts: after the table (open item 76) the caller gives the hosts of the save's host table
    and the nations that played the finals without the qualifying."""
    wc = year + (2 - year % 4) % 4
    hosts = {x for h in access.rule("world_cup", wc)[1] for x in h.split("/")} | set(more_hosts)
    later = {c["nation"] for c in changes if c["kind"] == "confed" and c["to"] == "UEFA" and c["season"] > first_year}
    sides = {nid for _, nid in game.side_names}
    out = []
    for nid, (nation, conf) in sorted(game.nations.items()):
        if conf != "UEFA" or nid not in sides or nation in entered or nation in hosts or nation in later:
            continue
        if nation == "Russia" and wc >= 2026:
            continue
        out.append(nation)
    return sorted(out)


# ---- World health (Tier B) ----

STAFF = 0x6E
FINANCE = 0x167
MONEY_32 = 1 << 31                            # the 32-bit readers of the balance (open items 27 and 28)
MONEY_WRAP = 1 << 62                          # no club reaches it: a balance beyond it has wrapped
SQUAD = 23                                    # a nation with this many players fills a squad; with no player under
                                              # 21 or under 23 its under-21 or Olympic squad is empty (Q17, Q31)
ELEVEN = 11                                   # under-21 players that fill an under-21 side


def side_days(game):
    """{national side id: [day numbers (date.toordinal) of its matches]} from the fixtures parts, played and to be
    played. A side with no fixture has no squad pick (open item 195): NationalTeams::select_squads_for_fixture
    0x76bab0 runs only for fixtures 14 days ahead (daily_update 0x76b100)."""
    out = collections.defaultdict(list)
    for fx in fixtures(game).values():
        for day, home, away in fx:
            for t in (home, away):
                if t in game.sides:
                    out[t].append(day)
    return out


def plays_near(days, today, span=None):
    """Does a side with these match days play within span days after the save day (today, an ordinal), or did it
    play within span days before it? span is PICK_AHEAD (14 days) by default."""
    span = PICK_AHEAD if span is None else span
    return any(today - span <= d <= today + span for d in days)


def health(game):
    """The world-health numbers of a save: (summary dict, [per nation row], faults). Squads: staff with a national
    side at +0x24 and the job Player (11) at +0x28 (database_get_job_name 0x53ad00).
    staff.dat (0x6e bytes a record): +0x10 s16 day of birth from 0, +0x12 s16 year of birth (both read by the
    research of slice 2 and checked on 83,140 players of a 2028 save against +0x18), +0x18 s16 age (picker.c:70),
    +0x1a nation (season_national_fill.c:13), +0x61 the player data, in a save its row or -1 (picker.c:69).
    Managers: club and national side +0xcf, the manager's staff id or -1 (database_get_club_manager 0x53ca90); the
    senior side of nation n is row n of nat_club.dat. Balance: finance.dat, 0x167 bytes a team, clubs first,
    +0x00 s64 (the finance record, 0x596587; run-logs/future-audit/audit.md)."""
    sav = game.sav
    staff = sav.read("staff.dat") if sav.has("staff.dat") else b""
    year, day = game.date
    players, u21, u23 = collections.Counter(), collections.Counter(), collections.Counter()
    squad = collections.Counter()
    age_bad, ages = [], {}
    for o in range(0, len(staff) - STAFF + 1, STAFF):
        if s32(staff, o + 0x24) >= 0 and staff[o + 0x28] == 11:    # in the squad of a national side (job Player)
            squad[s32(staff, o + 0x24)] += 1
        if s32(staff, o + 0x61) < 0:
            continue
        bday, byear = struct.unpack_from("<hh", staff, o + 0x10)
        age = struct.unpack_from("<h", staff, o + 0x18)[0]
        if not 1800 < byear < 3000:
            continue
        real = year - byear - (1 if day < bday else 0)        # the age at the save date, from the birth date
        if year and abs(age - real) > 1:
            age_bad.append((s32(staff, o), age, real))
        n = s32(staff, o + 0x1a)
        ages[s32(staff, o)] = (age, s32(staff, o + 0x61))
        players[n] += 1
        u21[n] += real < 21
        u23[n] += real < 23
    clubs = sav.read("club.dat")
    sides = sav.read("nat_club.dat")
    club_count = len(clubs) // CLUB
    managed = sum(1 for o in range(0, club_count * CLUB, CLUB) if s32(clubs, o + 0xCF) >= 0)
    side_manager, side_ids = {}, collections.defaultdict(list)
    for o in range(0, len(sides) - CLUB + 1, CLUB):
        n = s32(sides, o + 0x53)
        side_manager.setdefault(n, s32(sides, o + 0xCF) >= 0)    # the first row of a nation is its senior side
        side_ids[n].append(s32(sides, o))                        # then its under-21 side
    finance = sav.read("finance.dat") if sav.has("finance.dat") else b""
    balances = [struct.unpack_from("<q", finance, k * FINANCE)[0]
                for k in range(min(club_count, len(finance) // FINANCE))]
    live = {nid: (name, conf) for nid, (name, conf) in game.nations.items() if conf}
    days = side_days(game)
    today = datetime.date(year, 1, 1).toordinal() + day if 1900 < year < 3000 else 0
    rows, faults = [], []
    for nid, (name, conf) in sorted(live.items(), key=lambda x: x[1]):
        has_side = nid in side_manager
        ids = side_ids.get(nid, []) + [None, None]
        rows.append((name, conf, players[nid], u21[nid], u23[nid], squad[ids[0]] if ids[0] is not None else "",
                     squad[ids[1]] if ids[1] is not None else "",
                     int(side_manager.get(nid, False)) if has_side else ""))
        if ids[0] is not None and players[nid] >= SQUAD and squad[ids[0]] == 0 and \
                plays_near(days.get(ids[0], ()), today):
            faults.append(("empty-squad", name, year, f"the senior side of {name} has no player in its squad, "
                                                      f"{players[nid]} players hold the nationality"))
        if ids[1] is not None and u21[nid] >= ELEVEN and squad[ids[1]] == 0 and \
                plays_near(days.get(ids[1], ()), today):
            faults.append(("empty-squad", name + " under-21", year, f"the under-21 side of {name} has no player in "
                                                                    f"its squad, {u21[nid]} players are under 21"))
        if players[nid] >= SQUAD and (u21[nid] == 0 or u23[nid] == 0):
            faults.append(("no-youth", name, year, f"{name}: {players[nid]} players, {u21[nid]} under 21, "
                                                   f"{u23[nid]} under 23"))
    if age_bad:
        sid, age, real = age_bad[0]
        faults.append(("overflow", "age", year, f"{len(age_bad)} players with an age field that is not the age "
                                                f"from the birth date (staff {sid}: {age}, not {real})"))
    player_rows = sav.read("player.dat") if "player.dat" in sav.members else b""

    def ability(sid):                                         # current ability, player row +0x05
        row = ages.get(sid, (99, -1))[1]
        if not 0 <= row < len(player_rows) // PLAYER:
            return -1
        return struct.unpack_from("<h", player_rows, row * PLAYER + 5)[0]

    for o in range(0, club_count * CLUB, CLUB):
        ids = [s32(clubs, o + 0xD7 + 4 * j) for j in range(SLOTS)]
        if all(x >= 0 for x in ids):
            rest = sorted(ids, key=ability, reverse=True)[FIRST_TEAM:]
            young = sum(1 for x in rest if ages.get(x, (99, -1))[0] <= 21)
            if young < 8:
                name = text(clubs, o + 4)
                faults.append(("full-squad", name, year, f"{name}: all {SLOTS} squad slots filled, {young} players "
                                                         f"aged 21 or under outside its {FIRST_TEAM} best"))
    wrapped = [b for b in balances if abs(b) >= MONEY_WRAP]
    if wrapped:
        faults.append(("overflow", "balance", year, f"{len(wrapped)} clubs with a balance of 2^62 or more, "
                                                    f"for example {wrapped[0]}"))
    summary = {
        "save_year": year, "save_day": day, "save_bytes": os.path.getsize(game.path), "save_parts": len(sav.members),
        "players": sum(players.values()), "under_21": sum(u21.values()), "under_23": sum(u23.values()),
        "clubs": club_count, "clubs_with_manager": managed,
        "national_sides": sum(1 for n in side_manager if n in live),
        "national_sides_with_manager": sum(1 for n, m in side_manager.items() if m and n in live),
        "balance_min": min(balances, default=0), "balance_max": max(balances, default=0),
        "balances_over_32_bits": sum(1 for b in balances if not -MONEY_32 <= b < MONEY_32),
        "time_per_game_day": "not in a save (the Tier B runner measures it)",
    }
    return summary, rows, faults


def write_health(summary, rows, path):
    with open(path, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        for k, v in summary.items():
            w.writerow(["#", k, v])
        w.writerow(["nation", "confederation", "players", "under_21", "under_23", "senior_squad", "under_21_squad",
                    "national_manager"])
        w.writerows(rows)


def place(p):
    return f"{p[0]} ({p[1]})" if p[0] and p[1] else p[0]


def write(game, label, out, faults, known):
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, label + ".history.tsv"), "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(["competition_id", "kind", "competition", "year", "winner", "runner_up", "third", "host",
                    "winner_conf", "runner_up_conf"])
        for cid, kind, name, year, a, b, c, h in table(game):
            w.writerow([cid, kind, name, year, place(a), place(b), place(c), place(h), a[2], b[2]])
    with open(os.path.join(out, label + ".faults.tsv"), "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(["check", "competition", "year", "known", "detail"])
        for c, name, y, d in faults:
            w.writerow([c, name, y, known_status(known, c, name, y, rules_of(game), game.date[0]) or "new", d])


RESULT_CHECKS = {"same-final"}               # checks of real results: Tier A's results come from reputation


def tier_a_faults(faults):
    """The faults that count in a Tier A game: none of RESULT_CHECKS (spec, Decisions: checks that depend on real
    results count only in Tier B). A save does not record the Tier A switch (tier_a.c reads it from the
    environment), so the caller says so: run_set.py by the row's tier, the command line by --tier-a."""
    return [f for f in faults if f[0] not in RESULT_CHECKS]


def write_entrants(listing, path):
    with open(path, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(["competition_id", "competition", "year", "first", "last", "team_id", "team", "nation", "conf",
                    "route"])
        for cid, name, year, first, last, *rest in listing:
            w.writerow([cid, name, year, datetime.date.fromordinal(first), datetime.date.fromordinal(last), *rest])


def main(argv):
    args = [a for a in argv[1:]]
    opts = {k: 1 for k in ("--health", "--tier-a") if k in args}
    args = [a for a in args if a not in ("--health", "--tier-a")]
    for k in ("--out", "--rules", "--names", "--known", "--label", "--nations", "--access", "--table"):
        if k in args:
            i = args.index(k)
            opts[k] = args[i + 1]
            del args[i:i + 2]
    if not args:
        print(__doc__)
        return 2
    default = os.path.join(HERE, "competitions.tsv")
    rules = Rules(opts.get("--rules", default if os.path.exists(default) else None),
                  opts.get("--names", os.path.join(HERE, "history_names.tsv")))
    known = read_known(opts.get("--known", os.path.join(HERE, "history_known.tsv")))
    access = Access(finals_table(opts.get("--table")),
                    read_access(opts.get("--access", os.path.join(HERE, "access.tsv"))))
    nations = read_nations(opts.get("--nations", os.path.join(HERE, "nations.tsv")))
    new = 0
    for path in args:
        game = Game(path)
        label = opts.get("--label") if len(args) == 1 and "--label" in opts else \
            re.sub(r"[^A-Za-z0-9.-]+", "_", os.path.relpath(path, os.path.dirname(os.path.dirname(path))))
        listing, entry = entrants(game, rules, access, nations)
        summary, rows, sick = health(game) if "--health" in opts else ({}, [], [])
        sick += [("save-part", n, game.date[0], "the size does not fit its count, not read") for n in game.unread]
        faults = sorted(check(game, rules, nations) + check_nations(game, nations) + sorted(set(entry)) + sick,
                        key=lambda f: (f[1], f[0], f[2]))
        if "--tier-a" in opts:
            faults = tier_a_faults(faults)
        if "--out" in opts:
            write(game, label, opts["--out"], faults, known)
            write_entrants(listing, os.path.join(opts["--out"], label + ".entrants.tsv"))
            if summary:
                write_health(summary, rows, os.path.join(opts["--out"], label + ".health.tsv"))
        fresh = [f for f in faults if not is_known(known, f[0], f[1], f[2], rules_of(game), game.date[0])]
        new += len(fresh)
        y, d = game.date
        print(f"{label}: start {game.start}{'' if game.changing else ' (changing rules off)'}, save day {d} of {y}, {len(game.rows)} history rows, "
              f"{len({r[0] for r in game.rows})} competitions, {len(faults)} faults ({len(fresh)} new)")
        if summary:
            print("    health: " + ", ".join(f"{k} {v}" for k, v in summary.items()))
        for c, name, yr, detail in faults:
            print(f"    {known_status(known, c, name, yr, rules_of(game), game.date[0]) or 'FAULT'}  {c}  {name} {yr}: {detail}")
    return 1 if new else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
