#!/usr/bin/env python3
"""Writes a Markdown report of every rule change between consecutive seasons.

Reads the main rules files of docs/research/rules-0.6.0 (the files that are not
-clubs, check, check2, reconcile, databases or academy files). For each nation and
competition it lists the season where a field changes, with the old and new text.

A field value of unknown, n/a or empty is no information. It is skipped, and the
next known value is compared with the last known one, so a change is listed once
and an unknown season never shows as a change.

Usage: rule_changes_report.py [--rules DIR] [--out FILE] [--last-season 2026/27]
"""
import argparse
import collections
import csv
import glob
import os
import re
import sys

SKIP_PARTS = ('-clubs', '.check', '.reconcile', 'databases', 'academy')
FIELDS = [
    ('tier', 'tier'), ('name', 'name'), ('clubs', 'clubs'), ('up', 'promotion'),
    ('down', 'relegation'), ('playoffs', 'play-offs'), ('split', 'split'),
    ('format', 'format'), ('subs_used', 'substitutes used'), ('bench', 'bench'),
    ('foreign', 'foreign players'), ('work_permit', 'work permits'),
    ('registration', 'squad registration'), ('windows', 'transfer windows'),
]
GAME_BENCH_LIMIT = 9
MLS_CLUBS = 12

# The club counts that the league code of the original holds (0.6.0 slice 18, docs/research/2026-10-18-league-sizes.md):
# CAPS is the largest count of each class, FLOOR_COUNT the smallest where a cup of the nation needs the clubs. A real
# count outside them stays a `clubs` line of the rules report (tests/seasons/rules_report_check.py). For example
# ArgPrm::create_table stores 20 (0x405552): the real 30 clubs of 2015 need the mark.
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'recomp'))
from league_sizes import CAPS, FLOOR_COUNT  # noqa: E402
# The groups of the Serie C era (comp "Serie C group A" and "B", from 2014/15) hold up to 20 clubs with no floor
# (season_ita_cap and group_row, src/recomp_rt/win/season_divisions_ita.c). The C1 groups before it keep their cap and
# floor.
OPEN_COMPS = {'Serie C group A', 'Serie C group B'}
# The second Greek group (comp "league group 2") is a second gre_second object: gre_count caps it at 14 with no floor
# (src/recomp_rt/win/season_divisions.c). Only the National B record has the floor.
NO_FLOOR_COMPS = {('gre_second', 'league group 2')}
# The two C2 groups of 2011/12 to 2013/14 hold at most 20 clubs each (c2_clubs, season_divisions_ita.c).
C2_FILES, C2_SEASONS, C2_CAP = {'ita_ser_c2a', 'ita_ser_c2b'}, {'2011/12', '2012/13', '2013/14'}, 20

LIMITS = """\
## What the game will not do

A row of the rules files states the real rule. The game limits some of them, so the report lists the real change and marks the limit. Three limits apply:

- The bench and the substitutes used stay at 9 or fewer (Q375). The match engine clamps at 9. A real rule of more than 9 shows in the report, and the game plays 9.
- The MLS stays at 12 clubs (Q404). The group code of the original holds 12. MLS expansion clubs get no place.
- Each league holds at most the clubs that its league code in the original holds, for example 20 clubs at most in the Argentine Primera División, 18 in Serie A and 16 in the J1 League. Some leagues also keep a smallest count, because a cup of their nation needs their clubs. A real count outside these limits shows in the report, marked "The game keeps N clubs (league sizes)". The real clubs that do not fit stay in the division below or in no division.

Sources: docs/specs/2026-10-16-seasons-0.6.0-spec.md, docs/research/2026-10-18-league-sizes.md.
"""


def is_unknown(v):
    v = (v or '').strip().lower()
    return v == '' or v.startswith('unknown') or v.startswith('n/a')


def clean(v):
    return v.strip().replace('—', '-').replace('–', '-')


def main_files(folder):
    for f in sorted(glob.glob(os.path.join(folder, '*.tsv'))):
        b = os.path.basename(f)
        if not any(p in b for p in SKIP_PARTS):
            yield f


def load(folder):
    rows = []
    for f in main_files(folder):
        with open(f, encoding='utf-8', newline='') as fh:
            reader = csv.DictReader(fh, delimiter='\t', quoting=csv.QUOTE_NONE)
            # The date and access tables (uefa-ko-dates.tsv, nations-league-dates.tsv, nations-league-tiers.tsv,
            # uefa-access.tsv) are not rules files. Each skipped file is named, so a broken header shows.
            if 'nation' in (reader.fieldnames or []) and 'comp_file' in reader.fieldnames:
                rows.extend(reader)
            else:
                print('skipped (no nation and comp_file columns):', os.path.basename(f), file=sys.stderr)
    return rows


def season_key(s):
    m = re.match(r'(\d{4})', s)
    return int(m.group(1)) if m else 0


def tier_key(t):
    return int(t) if t.isdigit() else 99


def note(field, new, nation, comp, name, comp_file='', season=''):
    """Returns a short game-limit remark for a new value, or ''."""
    if field in ('bench', 'subs_used'):
        nums = [int(x) for x in re.findall(r'\d+', new)]
        if nums and max(nums) > GAME_BENCH_LIMIT:
            return ' The game keeps it at %d (Q375).' % GAME_BENCH_LIMIT
    if field == 'clubs' and 'major league soccer' in (name or '').lower():
        nums = [int(x) for x in re.findall(r'\d+', new)]
        if nums and max(nums) > MLS_CLUBS:
            return ' The game keeps %d clubs (Q404).' % MLS_CLUBS
    if field == 'clubs' and comp_file in CAPS and comp not in OPEN_COMPS:
        # The count is the first number of the text: a later number is a note ("20 (... 40 clubs ...)").
        m = re.match(r'\s*(\d+)', new)
        n = int(m.group(1)) if m else None
        cap = CAPS[comp_file]
        floor = 0 if (comp_file, comp) in NO_FLOOR_COMPS else FLOOR_COUNT.get(comp_file, 0)
        if n is not None and n > cap:
            return ' The game keeps %d clubs (league sizes).' % cap
        if n is not None and n < floor:
            return ' The game keeps %d clubs (league sizes).' % floor
    if field == 'clubs' and comp_file in C2_FILES and season in C2_SEASONS:
        m = re.match(r'\s*(\d+)', new)
        if m and int(m.group(1)) > C2_CAP:
            return ' The game keeps %d clubs (league sizes).' % C2_CAP
    return ''


def changes(rows, last_season=None):
    """Returns {nation: {comp key: (title, [(season, real, text)])}}."""
    dup = set()
    seen = collections.Counter()
    for r in rows:
        seen[(r['nation'], r['comp_file'], r['comp'], r['tier'], r['season'])] += 1
    for k, n in seen.items():
        if n > 1:
            dup.add(k[:4])
    groups = collections.defaultdict(list)
    for r in rows:
        k = (r['nation'], r['comp_file'], r['comp'], r['tier'])
        if k in dup:
            k += (r['name'],)
        groups[k].append(r)
    out = collections.defaultdict(dict)
    for k, rs in groups.items():
        rs.sort(key=lambda r: season_key(r['season']))
        if last_season:
            rs = [r for r in rs if season_key(r['season']) <= season_key(last_season)]
        last = {}
        events = []
        for r in rs:
            for field, label in FIELDS:
                v = r[field]
                if is_unknown(v):
                    continue
                v = clean(v)
                if field in last and last[field] != v:
                    real = clean(r['real_season']) if r['real_season'] not in ('', 'n/a', r['season']) else ''
                    text = '%s changes from "%s" to "%s".' % (label, last[field], v)
                    events.append((r['season'], real, text + note(field, v, r['nation'], r['comp'], r['name'], r['comp_file'], r['season'])))
                last[field] = v
        if events:
            title = r['name'] if r['comp'] != 'nation rules' else 'Nation rules'
            if r['name'] != 'n/a' and r['comp'] != 'nation rules':
                title = '%s (%s)' % (r['name'], r['comp'])
            out[k[0]][k] = (title, events)
    return out


def render(rows, last_season=None):
    ch = changes(rows, last_season)
    lines = ['# Rule changes by nation and season, 0.6.0 (draft)', '',
             'Every change between consecutive seasons in docs/research/rules-0.6.0, from 2001/02 to %s. '
             'Calendar-year leagues show the real year after the season. A value of unknown is skipped.' % (last_season or 'the last season'),
             '', LIMITS]
    total = 0
    for nation in sorted(ch):
        lines += ['## ' + nation, '']
        comps = sorted(ch[nation].items(), key=lambda kv: (kv[0][2] != 'nation rules', tier_key(kv[0][3]), kv[1][0]))
        for k, (title, events) in comps:
            lines += ['### ' + title, '']
            for season, real, text in events:
                lines.append('- %s%s: %s' % (season, ' (real %s)' % real if real else '', text))
                total += 1
            lines.append('')
    lines.insert(3, '%d changes in total.' % total)
    return '\n'.join(lines).rstrip() + '\n'


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.normpath(os.path.join(here, '..', '..'))
    ap = argparse.ArgumentParser()
    ap.add_argument('--rules', default=os.path.join(root, 'docs', 'research', 'rules-0.6.0'))
    ap.add_argument('--out', default=os.path.join(root, 'docs', 'release', '0.6.0-rule-changes.md'))
    ap.add_argument('--last-season', default='2026/27')
    a = ap.parse_args()
    text = render(load(a.rules), a.last_season)
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, 'w', encoding='utf-8') as fh:
        fh.write(text)
    print('wrote', a.out)


if __name__ == '__main__':
    sys.exit(main())
