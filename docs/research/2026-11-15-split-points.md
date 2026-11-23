# Split formats and points at the split (0.7.2 league sizes, gap rows 21 and 22)

Research of 15 November 2026 for the gap fixes of slice 7 (lead-notes/sizes-gaps.md rows 21 and 22). Two research
agents read the season pages and the tables. The full tables with every URL are kept outside the repo in
run-logs/0.7.2-sz07/gaps/research-nir-wal.tsv and research-points.tsv.

## Row 21: the meetings of the Northern Irish Premiership

The spec text of slice 7 said "2 meetings in place of 4". The rules row says 3 meetings, then 1 + 1. The row is right.

- 2002/03 (Irish League, 12 clubs): 3 meetings, 33 matches, then sections of 6 that meet once more, 38 matches.
  Sources: https://en.wikipedia.org/wiki/2002%E2%80%9303_Irish_League and RSSSF.
- 2003/04 to 2007/08 (16 clubs): 2 meetings, 30 matches, no split.
- 2008/09 to 2025/26 (12 clubs): 3 meetings, 33 matches, then sections of 6 that meet once more, 38 matches.
  Checked on the season pages of 2008/09, 2009/10, 2017/18, 2019/20, 2020/21, 2022/23 and 2025/26
  (https://en.wikipedia.org/wiki/2008%E2%80%9309_IFA_Premiership and later). 2019/20 stopped after 31 rounds, before
  the split.

The spec now says 3 meetings. The 2002/03 split text gave no section meetings ("play 5 more (38)"), so that row had no
split. It now reads "(38 matches)", and the encoder gives 1 + 1, as for 2008/09.

## Row 22: the points at the split

Every league that the rules table splits keeps the points in full, where a source shows the rule.

| League | Seasons | Points | Evidence |
|---|---|---|---|
| Scottish Premier League, Premiership | 2001/02 to 2026/27 | kept | Stated on https://en.wikipedia.org/wiki/Scottish_Premiership. 2027/28 keeps the last known rule. |
| IFA and NIFL Premiership | 2002/03, 2008/09 to 2026/27 | kept | Cumulative totals in the season tables, for example Larne 73 then 83 in 2025/26. |
| NIFL Championship | 2016/17 to 2018/19, 2021/22 to 2025/26 | kept | Cumulative totals, for example Limavady 71 then 81 in 2025/26. |
| Welsh Premier League, Cymru Premier | 2010/11 to 2027/28 | kept | The Cymru Premier page says the phase 1 points are brought forward. The 2026/27 page says results carry over. |
| K League | 2012 to 2026 (real seasons) | kept | Cumulative totals (https://rsssf.org/tabless/skor2012.html and the season pages). |
| Danish Superliga | 2019/20 | kept | Stated on https://en.wikipedia.org/wiki/2019%E2%80%9320_Danish_Superliga. |
| Super League Greece | 2019/20 to 2022/23 | kept | Cumulative totals (https://rsssf.org/tablesg/grk2022.html). |
| Super League Greece 2 | 2020/21 | kept | Cumulative totals on the season page. |
| Ekstraklasa | 2019/20 | kept | https://en.wikipedia.org/wiki/Ekstraklasa: 2016/17 was the last season with half points. |
| Prva HNL and Druga HNL | 2002/03 to 2005/06 | kept | Cumulative totals (https://rsssf.org/tablesk/kroa03.html and the season pages). |

The split text of each of these rows now ends "points kept in full", and the encoder writes "points kept".

## Rows left as they were

- Super League Greece 2026/27: places 1 to 4 and 9 to 14 keep their points, places 5 to 8 get half (the 2024/25 and
  2025/26 pages, https://en.wikipedia.org/wiki/2025%E2%80%9326_Super_League_Greece). No source gives the rounding. The
  rules table has one points value a split, so the row stays "unknown". Slice 11 owns it.
- NIFL Championship 2019/20 (22 + 10 or 33 + 5 not settled), 2026/27 and 2027/28 (no points rule found). 2020/21 has
  no split since gap 42 (22 matches).

## A correction: no format errors in other slices

A first reading of these sources matched the rows by their season column (for example 2011/12), not by their real
season (column 3, 2012 for the K League). Matched by the real season, the K League, Veikkausliiga and Russian First
Division rows agree with the sources:

- K League: row aus-jap-kor-334 is real 2011 (one table, then play-offs), 335 real 2012 (8 + 8 after 30, 44 matches),
  336 real 2013 (7 + 7 after 26), 337 real 2014 (6 + 6 after 33), 343 real 2020 (after 22, 27 matches) and 344 real
  2021 (after 33). Rows 335, 337, 343 and 344 now also say "points kept in full" (the tables of those seasons).
- Veikkausliiga: row nor-fin-177 is real 2019 (22 rounds, then 6 + 6). Slice 10 owns the rows of real 2018 and 2020.
- Russian First Division: row rus-tur-37 is the first stage of the long season 2011/12, which split in 2012.
