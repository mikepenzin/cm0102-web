"""Checks the 0.6.0 rules table files in docs/research/rules-0.6.0.

Usage: rules_lint.py [folder]
Each <part>.tsv (the 20 parts that tools/seasons/make_rules.py reads, not the data files of NOT_PARTS) must have the 22 columns of the brief, unique ids, seasons
2001/02 to 2027/28, and a source for every filled fact field (`all=` covers a row).
A value that points to another row ("see nation rules", "as Champions League") needs no source. Nor does the name of a nation rules row or of a row that is not held.
Prints one line per problem and a count per part. Exit code 1 if any problem.
"""
import csv, os, re, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'seasons'))
from make_rules import NOT_PARTS  # noqa: E402   the data files of the folder that are not parts of the table

COLUMNS = ('id season real_season nation comp_file comp tier name clubs up down playoffs split format '
           'subs_used bench foreign work_permit registration windows notes sources').split()
FACTS = 'name clubs up down playoffs split format subs_used bench foreign work_permit registration windows'.split()
SEASONS = {f'{y}/{(y + 1) % 100:02d}' for y in range(2001, 2028)}
EMPTY = ('', 'n/a', 'unknown', 'not held')

def covered(sources):
    keys = set()
    for part in sources.split(' ; '):
        key = part.split('=', 1)[0].strip().lower() if '=' in part else ''
        keys.update(k.strip() for k in re.split(r'[,/+ ]+', key) if k.strip())
    return keys

def lint(folder):
    bad = 0
    for name in sorted(os.listdir(folder)):
        if not name.endswith('.tsv') or '.' in name[:-4] or name.endswith(NOT_PARTS):
            continue
        path = os.path.join(folder, name)
        problems = []
        with open(path, encoding='utf-8') as f:
            reader = csv.reader(f, delimiter='\t', quoting=csv.QUOTE_NONE)
            header = next(reader)
            if header != COLUMNS:
                problems.append(f'header differs: {header}')
            ids = set()
            for n, row in enumerate(reader, 2):
                if len(row) != len(COLUMNS):
                    problems.append(f'line {n}: {len(row)} columns')
                    continue
                r = dict(zip(COLUMNS, row))
                if r['id'] in ids:
                    problems.append(f"line {n}: duplicate id {r['id']}")
                ids.add(r['id'])
                if r['season'] not in SEASONS:
                    problems.append(f"{r['id']}: season {r['season']!r}")
                keys = covered(r['sources'])
                if 'all' in keys:
                    continue
                for fact in FACTS:
                    value = r[fact].strip().lower()
                    if value in EMPTY or value.startswith(('unknown', 'n/a', 'see ', 'as ')) or fact in keys:
                        continue
                    if fact == 'name' and (r['comp'] == 'nation rules' or r['format'].strip().lower() == 'not held'):
                        continue
                    problems.append(f"{r['id']}: {fact} has no source")
        for p in problems:
            print(f'{name}: {p}')
        print(f'{name}: {len(problems)} problems')
        bad += len(problems)
    return bad

if __name__ == '__main__':
    here = os.path.dirname(os.path.abspath(__file__))
    folder = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, '..', '..', 'docs', 'research', 'rules-0.6.0')
    sys.exit(1 if lint(folder) else 0)
