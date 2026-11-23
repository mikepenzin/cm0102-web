"""Checks tests/seasons/future/competitions.tsv, the expected rule of every competition for games to 2300 (the
input of history_check.py): the columns, the values of each column, and the table years of the international finals
against src/recomp_rt/win/season_intl_table.c and the Gold Cup rows of season_nl_table.c.

Usage: python3 competitions_check.py
Exit 0 when the file is valid, 1 with one line per fault."""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
WIN = os.path.join(ROOT, 'src', 'recomp_rt', 'win')
COLUMNS = ['comp', 'name', 'kind', 'confed', 'has_host', 'table_years', 'cycle', 'next_year', 'end_year',
           'code_after_table', 'status', 'source', 'note']
KINDS = {'league', 'domestic_cup', 'super_cup', 'club_continental', 'club_world', 'intl_finals', 'intl_qual',
         'nations_league'}
CONFEDS = {'UEFA', 'CONMEBOL', 'CONCACAF', 'CAF', 'AFC', 'OFC', 'FIFA'}
STATUS = {'code as decided', 'fault'}
END = 2300


def table_years():
    """The history years of each class of the finals table, and of the Gold Cup."""
    years = {}
    text = open(os.path.join(WIN, 'season_intl_table.c'), encoding='utf-8').read()
    for name, year in re.findall(r'\{"(\w+)", \d+, (\d+),', text):
        years.setdefault(name, []).append(int(year))
    text = open(os.path.join(WIN, 'season_nl_table.c'), encoding='utf-8').read()
    years['gold_cup'] = [int(y) for y in re.findall(r'\{(\d{4}), \d+, \{\d+, \d+, \d+\}, "int-continental', text)]
    return years


def main():
    path = os.path.join(HERE, 'competitions.tsv')
    lines = [line.rstrip('\n') for line in open(path, encoding='utf-8') if line.strip() and not line.startswith('#')]
    faults = []
    if not lines or lines[0].split('\t') != COLUMNS:
        print('competitions.tsv: the header is not: ' + ' '.join(COLUMNS))
        return 1
    tables = table_years()
    seen = set()
    for n, line in enumerate(lines[1:], 2):
        cells = line.split('\t')
        if len(cells) != len(COLUMNS):
            faults.append(f'row {n}: {len(cells)} columns')
            continue
        row = dict(zip(COLUMNS, cells))
        comp = row['comp']
        where = f'{comp} (row {n})'
        if not re.fullmatch(r'[a-z0-9_]+', comp) or comp in seen:
            faults.append(f'{where}: the key is not a new lower-case key')
        seen.add(comp)
        if row['kind'] not in KINDS:
            faults.append(f'{where}: kind {row["kind"]}')
        confeds = row['confed'].split('|')
        if row['confed'] != 'nation' and not set(confeds) <= CONFEDS and len(confeds) != 1:
            faults.append(f'{where}: confed {row["confed"]}')
        if row['has_host'] not in ('0', '1'):
            faults.append(f'{where}: has_host {row["has_host"]}')
        if row['status'] not in STATUS:
            faults.append(f'{where}: status {row["status"]}')
        years = [int(y) for y in row['table_years'].split(',')] if row['table_years'] else []
        if years != sorted(set(years)):
            faults.append(f'{where}: table_years not sorted or twice')
        if comp in tables and years != sorted(tables[comp]):
            faults.append(f'{where}: table_years {years}, the C table has {sorted(tables[comp])}')
        if not row['cycle'].isdigit():
            faults.append(f'{where}: cycle {row["cycle"]}')
            continue
        cycle = int(row['cycle'])
        end = row['end_year']
        if end != 'none' and not (end.isdigit() and 1990 <= int(end) <= END):
            faults.append(f'{where}: end_year {end}')
        if cycle:
            if not row['next_year'].isdigit():
                faults.append(f'{where}: a cycle with no next_year')
            elif years and int(row['next_year']) != years[-1] + cycle:
                faults.append(f'{where}: next_year {row["next_year"]} is not the last table year + {cycle}')
        elif row['next_year'] or end == 'none':
            faults.append(f'{where}: cycle 0 needs an end_year and no next_year')
    for fault in faults:
        print('competitions.tsv: ' + fault)
    print(f'competitions.tsv: {len(lines) - 1} rows, {len(faults)} faults')
    return 1 if faults else 0


if __name__ == '__main__':
    sys.exit(main())
