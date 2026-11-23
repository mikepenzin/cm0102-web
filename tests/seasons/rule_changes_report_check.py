"""Test for tools/release/rule_changes_report.py with a small fixture."""
import os, sys
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(here, '..', '..', 'tools', 'release'))
import rule_changes_report as r

H = 'id season real_season nation comp_file comp tier name clubs up down playoffs split format subs_used bench foreign work_permit registration windows notes sources'.split()
def row(i, season, real, name, clubs, bench='n/a', comp='league', tier='1', nation='Testland', foreign='n/a',
        comp_file='tl'):
    d = dict.fromkeys(H, 'n/a'); d.update(id=i, season=season, real_season=real, nation=nation,
        comp_file=comp_file, comp=comp, tier=tier, name=name, clubs=clubs, bench=bench, foreign=foreign, sources='x')
    return d

rows = [
    row('1', '2001/02', '2002', 'Top', '18', bench='5'),
    row('2', '2002/03', '2003', 'Top', 'unknown', bench='7', foreign='3 foreigners'),
    row('3', '2003/04', '2004', 'Top', '20', bench='7', foreign='3 foreigners'),
    row('4', '2004/05', '2005', 'Top', '20', bench='12', foreign='3 foreigners'),
    row('5', '2005/06', '2006', 'Top', '20', bench='12', foreign='unknown'),
    row('6', '2026/27', '2027', 'Top', '10'),
    row('7', '2001/02', '2002', 'Major League Soccer', '10', nation='USA', tier='1'),
    row('8', '2002/03', '2003', 'Major League Soccer', '14', nation='USA', tier='1'),
    # ArgPrm holds 20 clubs (create_table 0x405552), so the real 30 of 2015 needs a mark.
    row('9', '2013/14', '2013-14', 'Primera', '20', nation='Argentina', comp_file='arg_prm'),
    row('10', '2014/15', '2015', 'Primera', '30', nation='Argentina', comp_file='arg_prm'),
    row('11', '2017/18', '2017-18', 'Primera', '28', nation='Argentina', comp_file='arg_prm'),
    row('12', '2019/20', '2020', 'Primera', '18', nation='Argentina', comp_file='arg_prm'),
    # A count under the floor that a cup of the nation needs (FLOOR_COUNT of tools/recomp/league_sizes.py).
    # (0.7.2 slice 4: the divisions of the cups of Britain and Ireland have no floor, the cups follow them.)
    row('13', '2001/02', '2002', 'Belgian First', '18', nation='Belgium', comp_file='bel_first'),
    row('14', '2002/03', '2003', 'Belgian First', '16', nation='Belgium', comp_file='bel_first'),
    # The Serie C groups have no floor in the Serie C era (season_ita_cap): no mark. Before it the C1 floor is 18.
    row('15', '2012/13', '2013', 'Serie C1/A', '18', nation='Italy', comp='Serie C1 group A', comp_file='ita_ser_c1a'),
    row('16', '2013/14', '2014', 'Serie C1/A', '16', nation='Italy', comp='Serie C1 group A', comp_file='ita_ser_c1a'),
    row('19', '2014/15', '2015', 'Serie C/A', '20', nation='Italy', comp='Serie C group A', comp_file='ita_ser_c1a'),
    row('22', '2015/16', '2016', 'Serie C/A', '16', nation='Italy', comp='Serie C group A', comp_file='ita_ser_c1a'),
    # The second Greek group has no floor (gre_count); a C2 group of 2011/12 holds 20 (c2_clubs).
    row('20', '2013/14', '2014', 'SL2 South', '14', nation='Greece', comp='league group 2', comp_file='gre_second'),
    row('21', '2014/15', '2015', 'SL2 South', '13', nation='Greece', comp='league group 2', comp_file='gre_second'),
    row('23', '2010/11', '2011', 'C2/B', '18', nation='Italy', comp='Serie C2 group B', comp_file='ita_ser_c2b'),
    row('24', '2011/12', '2012', 'C2/B', '21', nation='Italy', comp='Serie C2 group B', comp_file='ita_ser_c2b'),
    # The Danish First Division plays its row (0.7.2 slice 5: DenCup takes Danish clubs with no division).
    row('17', '2010/11', '2011', 'Danish First', '16', nation='Denmark', comp_file='den_first'),
    row('18', '2011/12', '2012', 'Danish First', '12', nation='Denmark', comp_file='den_first'),
]
text = r.render(rows, '2025/26')
assert '- 2003/04 (real 2004): clubs changes from "18" to "20".' in text, text
assert '- 2002/03 (real 2003): bench changes from "5" to "7".' in text
assert 'The game keeps it at 9 (Q375).' in text
assert 'The game keeps 12 clubs (Q404).' in text
assert '- 2014/15 (real 2015): clubs changes from "20" to "30". The game keeps 20 clubs (league sizes).' in text, text
assert 'clubs changes from "30" to "28". The game keeps 20 clubs (league sizes).' in text
assert 'clubs changes from "28" to "18".\n' in text            # under the class count: the game plays it
assert text.count('clubs changes from "18" to "16". The game keeps 18 clubs (league sizes).') == 2   # Belgium, C1
assert 'clubs changes from "20" to "16".\n' in text            # Serie C era group: no mark
assert 'clubs changes from "14" to "13".\n' in text            # second Greek group: the game plays 13
assert 'clubs changes from "18" to "21". The game keeps 20 clubs (league sizes).' in text
assert '20 clubs at most in the Argentine Primera' in text      # the limits section names the league
assert 'clubs changes from "16" to "12".\n' in text            # Danish First Division: the game plays 12
assert text.count('clubs changes') == 11          # unknown skipped, 2026/27 cut off
assert 'foreign players changes' not in text      # unknown and equal values are no change
assert '—' not in text and '–' not in text
print('ok')
