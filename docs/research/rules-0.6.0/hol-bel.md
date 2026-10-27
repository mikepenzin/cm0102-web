# Rules table: Holland and Belgium (slice 6, part hol-bel)

Date 2026-10-16. Research only. Brief: the 0.6.0 research brief (kept outside the repo).

Files:

- `hol-bel.tsv`: 297 rows, 11 competitions x 27 seasons (2001/02 to 2027/28). Pass 2 deleted 54 rows.
- `hol-bel-clubs.tsv`: 1,348 club rows after Pass 2, for every tier whose club count differs from its season database or that a club move touches.
- This file: method, decisions, missing clubs and unknowns.

## Scope

| comp | comp_file | Rows |
|---|---|---|
| nation rules (Holland) | n/a | 27 |
| Eredivisie | hol_prm | 27 |
| Eerste Divisie | hol_first | 27 |
| Dutch third tier: Hoofdklasse to 2009/10, Topklasse 2010/11 to 2015/16, Tweede Divisie from 2016/17 | new | 27 |
| KNVB Cup | hol_cup | 27 |
| Johan Cruyff Shield | hol_super | 27 |
| nation rules (Belgium) | n/a | 27 |
| Belgian top flight | bel_first | 27 |
| Belgian second tier | bel_second | 27 |
| Belgian third tier | bel_third | 27 |
| Belgian fourth tier | unknown | 27 |
| Belgian Cup | bel_fa_cup | 27 |
| Belgian Super Cup | bel_super | 27 |

- The databases hold four Belgian tiers (data comps 0 to 3 and 429 to 432) but the function map has no `bel_fourth` file. I could not tell which file runs the fourth-tier divisions, so `comp_file` is `unknown` for those rows. Slice 1 can settle it.
- The Belgian fifth tier from 2016/17 (Third Amateur Division, Division 3) is out of scope. The original has four Belgian tiers.
- The Dutch Hoofdklasse before 2010/11 had no promotion to the Eerste Divisie. Its rows say so. I listed no clubs for it.

## Method

- Wikipedia season pages, raw wikitext read on 2026-10-16. The league tables (`Sports table` module) gave club counts, outcomes and club lists. Lead and format sections gave the rules.
- Dutch-language Wikipedia gave the Belgian second tier to 2008/09, the third tier to 2015/16, the fourth tier to 2015/16, the 2017/18 Second Amateur Division and the 2016/17 to 2019/20 top-flight club lists. English pages hold these tables only in part or not at all.
- Substitutes and bench: KNVB regulations (2020 decision, 2025/26 regulations), the Eredivisie news page, Belgian news pages (seneweb 2020, Sporza 2022), the Wikipedia article on substitutes for the three-substitute rule of 1995, and Super Cup match rules.
- Nation rules: IND, Everaert, VI and Voetbalprimeur for the Dutch salary criterion; Inside World Football for the Belgian homegrown quota; a 2020 Belgian Chamber bill for the Belgian non-EU minimum salary; the Wikipedia transfer window article.
- Club moves: the database of each season start is the one in the spec table. October 2003 is for 2003/04, September 2005 for 2005/06, and so on. 2001/02 and 2002/03 use 3.9.68. 2004/05, 2006/07, 2026/27 and 2027/28 have no database and no club list.
- Club names were matched to database clubs by a normalised name comparison and an alias list. I checked each match below exact by hand. A club matches only inside its own nation.

## Decisions to check (inference, settled in Pass 2)

1. **Spare club pool.** Q244 says to take clubs "that have no division". I counted clubs in division -1 and in "A Lower Division" (357, 433) as the pool. "A Lower Division" is a placeholder division with no league. Pool sizes: Holland 105 to 173 clubs, Belgium 12 to 118 clubs, by database.
2. **Belgian reform of 2016.** The databases keep the old layout: First 18, Second 18, Third A and B 16 each, Fourth A to D 16 each. The real pyramid from 2016/17 has First Division A 16, First Division B 8, First Amateur Division 16 and Second Amateur Division 3 x 16. The club list maps each real division to a suggested database division in `db_division` of the real club rows: tier 3 to "Belgian Third Division A", tier 4 groups to Fourth A, B and C. "Belgian Third Division B" and "Belgian Fourth Division D" then stay empty. From 2024/25 Division 1 has two groups (VV 16, ACFF 12). I mapped them to Third A and Third B. Max decides the mapping.
3. **Belgian top flight 16 clubs.** In the 16-club seasons (2009/10 to 2019/20, 2023/24 to 2025/26) the database still has 18 clubs in the top flight. Two clubs must move down. The club list shows which.
4. **Jong teams.** The databases name Dutch reserve teams in several ways ("Ajax Amsterdam U21", "Jong AFC Ajax", "PSV Eindhoven U21"). Belgian U23 teams appear as "KAA Gent B", "KRC Genk B", "Club Brugge KV II", "RSC Anderlecht B". I matched them by alias. "Jong Westerlo" (2024/25) matched "Jong Westel", which I take to be the same club with a cut name.
5. **Renamed clubs.** Some database records changed name across databases (for example id 7938 is "FC Nieuwkerken Sint-Niklaas", then "Sportkring Sint-Niklaas"). I matched by the name in that season's database. KRC Harelbeke (2016) matched "SW Harelbeke" or "Racing Harelbeke". RWDM47 matched "RWD Molenbeek (2015)" or "RWDM Brussels". Treat these as inference.
6. **Bench 12 in Dutch professional football** is sourced for 2016/17, 2017/18 (Shield rules), 2019/20 to 2020/21 (KNVB 2020) and 2025/26 (KNVB regulations). 2018/19 and 2021/22 to 2024/25 are inference from the seasons around them, and the rows say so.
7. **Substitutes before 2020.** "3" rests on the Laws of the Game (three since 1995). I found no league rule that sets a lower number.

## Missing clubs (for Max, superseded: Pass 2 deleted every division in this table)

Real clubs with no record in that season's database. All other real clubs have a record, often in another division.

| Season | Division | Clubs with no record |
|---|---|---|
| 2010/11 | Topklasse Sunday | VVSB |
| 2011/12 | Topklasse Saturday | SVZW, Montfoort |
| 2011/12 | Topklasse Sunday | WKE, VVSB |
| 2012/13 | Topklasse Sunday | WKE, VVSB |
| 2013/14 | Topklasse Sunday | VVSB, Leonidas, WKE |
| 2014/15 | Topklasse Saturday | Ajax (amateurs) |
| 2014/15 | Topklasse Sunday | VVSB, WKE, Hercules, EDO |
| 2015/16 | Topklasse Sunday | VVSB, WKE |
| 2016/17 | Tweede Divisie | VVSB |
| 2005/06 | Belgian Fourth Division A | VG Oostende, RRC Péruwelz |
| 2009/10 | Belgian Fourth Division A | Football Couillet-La Louvière |
| 2012/13 | Belgian Fourth Division B | RUS Genly-Quévy 89, RSC Templeuvois |
| 2012/13 | Belgian Fourth Division C | Merksem-Antwerpen Noord SC, K. Ternesse VV Wommelgem |
| 2016/17 | Second Amateur Division ACFF | Waremme |
| 2017/18 | Second Amateur Division VV B | SC City Pirates Antwerpen |
| 2018/19 | Second Amateur Division VV B | City Pirates, Heur-Tongeren |
| 2023/24 | Division 2 ACFF | Rochefort |
| 2024/25 | Division 2 VV B | Jong STVV |

- Several of these clubs exist in other databases (VVSB as "VVSB Noordwijkerhout", Leonidas, Hercules, Templeuvois, Genly-Quévy, Ternesse, VG Oostende). So a fix by data is possible, but no club records may be added without Max (Q244).
- The Topklasse and Tweede Divisie are new divisions. The database has no Dutch third tier, so every club in those lists must come from the pool.

## Database oddities seen

- October 2003 puts Excelsior in the Dutch Premier Division. Real 2003/04: Eerste Divisie.
- October 2018 has KSK Tongeren with no nation (-1). November 2020 has the same id as "KFC Heur Tongeren".
- The Belgian fourth-tier counts in November 2020 to December 2025 vary (13 to 26 per group). They do not follow the real 16 or 18.

## Unknowns

- Dutch foreign player quota: no source found. Non-EU players need a work permit with a salary criterion (150% of the average Eredivisie salary). Amounts found for 2018/19, 2023/24, 2024/25 and 2025/26 only. Start year of the criterion unknown.
- Belgian non-EU minimum salary: only the 2020 figure (EUR 84,896). Belgian squad registration before 2011: unknown.
- Bench size: Dutch professional football before 2016/17; all Belgian leagues and cups; Dutch and Belgian amateur tiers.
- Transfer window dates: only Netherlands 2025/26 and Belgium 2023/24 (summer). Other seasons say "two FIFA windows from 2002/03". 2001/02 unknown.
- KNVB Cup entrants for 2011/12, 2014/15, 2015/16, 2025/26 and 2026/27.
- Hoofdklasse club counts before 2006/07 (no page fetched).
- Belgian third tier 2023/24 relegation; Belgian fourth tier 2019/20 relegations.
- 2027/28: no season pages. Q248 keeps the 2026/27 rules. Belgium: the 2026/27 page reports that the big clubs want 14 or fewer clubs in the long term, with no decision.

## Points for the build slices

- Eredivisie: 18 clubs every season. Direct relegation 1 to 2018/19, 2 from 2020/21. 2019/20 abandoned, no relegation.
- Promotion play-offs: groups of four to 2004/05; a 10-club knockout 2005/06 to 2018/19; 16th plus six Eerste Divisie clubs from 2020/21. The original's `hol_first` already builds the 2001 group format. `HolFirst::create_play_off_groups` (`0x5f6720`, called by `HolFirst::advance_stage` at stage 4) tops the period winners up to six Eerste Divisie clubs from the table. It adds the two Eredivisie clubs with play-off status 2 and builds two groups of four. Stage 4 is comp code 0x418 and stage 5 is 0x419, with 6 rounds each (`HolFirst::create_schedule`, `0x5f5790`). An earlier version of this note said "groups of eight". That came from a wrong first-pass function name.
- Eerste Divisie: 18, 19 or 20 clubs by season. Jong teams from 2013/14. No relegation to 2008/09. From 2017/18, no relegation to the Tweede Divisie for first teams.
- Belgian top flight: 18 to 2008/09, 16 with play-offs 2009/10 to 2019/20, 18 in 2020/21 to 2022/23, 16 in 2023/24 to 2025/26, 18 with no play-offs from 2026/27 (announced February 2025 and confirmed on the 2026/27 page).
- Belgian second tier: 18 or 19 clubs to 2014/15, 17 in 2015/16, 8 in 2016/17 to 2021/22 (two tournaments to 2019/20), 12 in 2022/23 with U23 teams and a split, 16 in 2023/24 and 2024/25, 17 in 2025/26, 15 in 2026/27.
- Belgian Cup: two-legged quarter-finals 2002/03 to 2014/15; two-legged semi-finals every season except 2020/21.
- Five substitutes: Netherlands from 2020/21; Belgium from 1 January 2021.

## Reconcile

Date: 2026-10-16. File: `hol-bel.reconcile.tsv`. Third sources are transfermarkt (KNVB Cup round lists of every season, Belgian second-tier club lists), NOS (2017 Johan Cruyff Shield), Sporza (2021/22 First Division B), the Pro League communication of June 2020 (through web.archive.org), KNVB news (summer window 2025) and the KNVB regulations 2025/26. The `up` and `down` decisions use the brief rule: places by the rules of the season, with actual outcomes in `notes`.

| Decision | Fields |
|---|---|
| first | 2 |
| second | 10 |
| conflict | 2 |
| found accepted | 11 |
| found rejected | 1 |

### KNVB Cup: one method

Every KNVB Cup row now counts the clubs of the main draw. The main draw starts at the round that the KNVB calls the first round. The amateur qualifying rounds (from 2016/17, plus one match in 2005/06) are in `format`, with the total of clubs in all rounds. Clubs in Europe that enter in the second round count in the main draw. Each row cites the transfermarkt round list of its season.

| Season | Main draw | Season | Main draw |
|---|---|---|---|
| 2001/02 | 86 | 2014/15 | 85 |
| 2002/03 | 86 | 2015/16 | 85 |
| 2003/04 | 88 | 2016/17 | 64 |
| 2004/05 | 85 | 2017/18 | 64 |
| 2005/06 | 86 | 2018/19 | 64 |
| 2006/07 | 88 | 2019/20 | 60 |
| 2007/08 | 87 | 2020/21 | 60 |
| 2008/09 | 88 | 2021/22 | 59 |
| 2009/10 | 88 | 2022/23 | 59 |
| 2010/11 | 93 | 2023/24 | 59 |
| 2011/12 | 92 | 2024/25 | 58 |
| 2012/13 | 92 | 2025/26 | 58 |
| 2013/14 | 82 | 2026/27 | 58 |

- Before 2016/17 the KNVB Cup had no qualifying rounds, except one match in 2005/06 (AFC v Be Quick '28). Amateur clubs qualified through the district cups and their league places. Their first round, which held only amateur clubs, is part of the main draw. The `format` text says where the professional clubs enter.
- Changed counts: 2005/06 87 to 86, 2010/11 88 to 93, 2016/17 63 to 64, 2019/20 64 to 60, 2020/21 64 to 60, 2022/23 46 to 59, 2023/24 110 to 59, 2024/25 110 to 58. 2011/12, 2014/15, 2015/16, 2025/26 and 2026/27 were unknown and now have values.
- 2019/20 and 2020/21: Wikipedia gives 64 in the infobox, but the first round had 28 matches and 4 clubs had a bye to the second round. The count is 60.
- 2027/28 stays unknown.

### Other changes to hol-bel.tsv

- hol-bel-86 (2007/08 Belgian top flight): `down` 2 (17th, 18th), `playoffs` none.
- hol-bel-87 (2007/08 Belgian second tier, 19 clubs): `down` 2 (18th, 19th), 17th to the third division play-off. `playoffs`: runner-up and three period winners, no top-flight club.
- hol-bel-100 (2008/09 Belgian second tier, 19 clubs): `down` 2 (18th, 19th), 15th to 17th to the third division final round. `playoffs`: two-legged qualifying ties, then a group of four with the top-flight 15th and 16th.
- 12 other second-tier rows (hol-bel-9 to hol-bel-178) copy the play-off text. Their phrase "the top-flight 17th (2005/06 to 2008/09)" now says 2005/06 and 2006/07 only.
- hol-bel-108 (2009/10 Hoofdklasse): `up` 24 direct places. `playoffs` now holds the seven play-off places (31 clubs up in all).
- hol-bel-214 (2017 Johan Cruyff Shield): value kept. `notes` records that RSSSF and a later KNVB article say extra time was played.
- hol-bel-256 and hol-bel-269: `down` kept at 1. `notes` give the outcomes.
- hol-bel-313: summer window 2025 now 23 June to 2 September (KNVB statement).
- Found values written: Hoofdklasse 2001/02 to 2005/06 `clubs` 84, Netherlands 2007/08 and 2008/09 `foreign`.
- `hol-bel-clubs.tsv`: no change. No decision touched a club list.

### Conflicts for Max

1. **hol-bel-256, 2020/21 Belgian First Division B, `down`.** First value 1 (8th). Second value: no relegation. Only Wikipedia states the rule that the bottom club goes down. The Pro League text of June 2020 sets the 8-club format but gives no number of relegation places. Club NXT finished 8th and, as a U23 team, could not go down. The amateur leagues were abandoned, so no club went down. Proposal: keep 1, with the outcome in notes (done).
2. **hol-bel-313, Netherlands 2025/26, `windows`.** Three opening dates: 11 June (first agent, cited to the KNVB regulations, which give no dates), 16 June (Voetbalprimeur, 3 June 2025) and 23 June (KNVB news, 3 September 2025). The close date, 2 September 23:59, is settled. Proposal: 23 June, the KNVB statement (now in the TSV).

### Other findings

- hol-bel-329 (2026/27 Tweede Divisie `subs_used`) stays unknown. The KNVB page gives 5 substitutes in 4 moments for category A, but it does not name the Tweede Divisie.
- hol-bel-313 winter window "3 to 31 January" has no source that states it. KNVB news says the winter 2026 window closed on 3 February 23:59 (https://www.knvb.nl/node/71461, found in a search summary, not read raw). Not changed.
- The 2001/02 and 2002/03 transfermarkt round lists hold only 46 clubs. They lack the group stage. The counts of 86 rest on Wikipedia only.

## Loose ends

- hol-bel-313 (Netherlands 2025/26) `windows`: the winter window is now 2 January to 3 February 2026 23:59. The KNVB news of 4 February 2026 says clubs could sign players until 3 February 23:59 (https://www.knvb.nl/node/71461, read raw today). Voetbalprimeur gives the opening date 2 January 2026 (https://www.voetbalprimeur.nl/nieuws/1706663/wanneer-opent-en-sluit-de-winterse-transfermarkt.html). It gives the close as "dinsdag 2 februari", but 2 February 2026 was a Monday and 3 February a Tuesday. I kept the KNVB close date. The opening date has one source only.

## Source fill

Date: 2026-10-16. A source-fill pass added `field=URL` entries to `sources` for filled fact fields that had none. It changed no values and kept row order and ids. Each URL was read on this date, and the value was checked against the page text.

Method:

- For `name`, the pass took the pages that the row already cites, season pages first, and kept the first page that contains the name. For a name with a note in brackets, the base name had to match. For an Italian group (girone), the page also had to name that group.
- For a value of `none` in `split` or `playoffs`, the source is the season page with one league stage and no championship or relegation round.

Fields filled: 297 `name`.

- Rows hol-bel-329 and hol-bel-340 to hol-bel-351 (2026/27 and 2027/28 with no season page) and hol-bel-193 (2015/16 fourth tier) cite the main competition page. That page gives the current name.
- hol-bel-219 (2017/18) cites the English season page of the Belgian Second Amateur Division. The cited Dutch page uses the Dutch name.

Disagreements: none found.

Lint after the pass: 0 problems.

## Pass 2

Date: 2026-10-17. Decision agent. Brief: section "Pass 2" of the 0.6.0 research brief (kept outside the repo), decisions Q268 to Q331 in the spec. Row ids are kept. Lint after the pass: 0 problems (0 before).

### 1. No new tiers (Q331, Q288)

The original runs `hol_prm` and `hol_first` for Holland and `bel_first`, `bel_second` and `bel_third` for Belgium. The lowest tier is 2 for Holland and 3 for Belgium.

- The original plays no Belgian fourth tier. `BelThird::create_relegation_playoff_stage` (0x423ba0) collects the clubs of the four fourth-division records (globals 0x9cf7d4 to 0x9cf7e0), sorts them and stores the top four as promoted clubs. It plays no fourth-tier match. `bel_first_move_third_division_clubs` (0x41ec90) then moves the clubs. The fourth divisions are a pool, not a competition.
- Deleted from `hol-bel.tsv`: 54 rows.
  - Dutch third tier (Hoofdklasse, Topklasse, Tweede Divisie), `comp_file` `new`: hol-bel-4, 17, 30, 43, 56, 69, 82, 95, 108, 121, 134, 147, 160, 173, 186, 199, 212, 225, 238, 251, 264, 277, 290, 303, 316, 329, 342.
  - Belgian fourth tier, `comp_file` `unknown`: hol-bel-11, 24, 37, 50, 63, 76, 89, 102, 115, 128, 141, 154, 167, 180, 193, 206, 219, 232, 245, 258, 271, 284, 297, 310, 323, 336, 349.
- Deleted from `hol-bel-clubs.tsv`: 934 club rows. 372 rows of the Topklasse and Tweede Divisie, 562 rows of the Belgian fourth tier (Fourth Division, Second Amateur Division, Division 2).
- No row needs a nation that a game cannot select. No tier of these two nations ends inside 2001/02 to 2027/28, so no row became `not held`.
- `hol-bel.check.tsv` (326 lines) and `hol-bel.reconcile.tsv` (7 lines) still name the deleted ids. Those lines are obsolete. I did not change the files of the check agents.
- The rows of the kept tiers still describe the real promotion and relegation with the deleted tiers (for example the Eerste Divisie `down` with the Tweede Divisie). Those are the real rules. The game has no Dutch third tier, so the build cannot relegate a Dutch club below the Eerste Divisie. Belgian third-tier relegation goes to the database fourth divisions, as in the original.

### 2. Conflicts and open points

| Row or point | Decision | URL |
|---|---|---|
| hol-bel-256, 2020/21 First Division B `down` | 1 (8th). The season started with the rule that the bottom club goes down. The Pro League cancelled relegation on 16 March 2021, during the season (Q272, Q278). Outcome in `notes`. | https://en.wikipedia.org/wiki/2020%E2%80%9321_Belgian_First_Division_B |
| hol-bel-313, Netherlands summer window 2025 | 23 June to 2 September 23:59. The KNVB news page states "Tussen 23 juni en 2 september" (read raw today). "dates disputed" removed from `windows`. | https://www.knvb.nl/node/70959 |
| Spare club pool | Q279: clubs with no division, "A Lower Division", the database Belgian fourth divisions and "Reserve" (87, December 2025) form the pool. | n/a |
| Belgian 2016 mapping | Q292 approved. From 2016/17 the real tier 3 (First Amateur Division, then National Division 1) maps to "Belgian Third Division A", and "Belgian Third Division B" stays empty. From 2024/25 Division 1 VV maps to Third A and Division 1 ACFF to Third B. The fourth tier is gone (Q331). | n/a |
| Top flight of 16 clubs | The club lists move the extra clubs down (section 3). | n/a |
| Jong and U23 teams | Matches checked. 10 wrong matches fixed (section 4). | n/a |
| Renamed clubs | Kept. A record keeps its id across renames (for example 4159 Royal Boussu Dour Borinage, later Seraing United). | n/a |
| Bench 12 inferences, substitutes before 2020 | No conflict. The inferences stay marked in the rows for the second-source pass. Three substitutes before 2020 follow the Laws of the Game. | n/a |

### 3. Club moves (Q279, Q280, Q290)

Method: for each season with a database, every tier from 1 down to the lowest tier. A tier is listed when its real count differs from the database count, or when a listed tier takes a club from it or leaves a club in it. The trace ran until no tier changed. Each database has First 18, Second 18, Third A 16 and Third B 16 (October 2003: Third B 17) for Belgium, and 18 and 18 for Holland.

Added: 124 club rows, each from a page read today.

| Season | Tier | Reason | Rows | Source |
|---|---|---|---|---|
| 2003/04 | Eredivisie | October 2003 has Excelsior in the Premier Division and FC Volendam in the First Division. Real 2003/04: the other way round. | 18 | https://en.wikipedia.org/wiki/2003%E2%80%9304_Eredivisie |
| 2007/08 | Belgian Third Division A | UR Namur moves up from Third B, so tier 3 is listed whole. | 16 | https://nl.wikipedia.org/wiki/Derde_klasse_2007-08_%28voetbal_Belgi%C3%AB%29 |
| 2010/11 to 2014/15 | Belgian Second Division | Two clubs leave the 16-club top flight and two clubs go from the database Second Division to tier 3 each season. | 90 | `https://nl.wikipedia.org/wiki/Tweede_klasse_<season>_%28voetbal_Belgi%C3%AB%29` |

Corrected club matches: 12 rows.

- HFC Haarlem 2003/04 and 2005/06: 4724 "HFC Haarlem" (A Lower Division) to 4760 "Haarlem" (Dutch First Division). Inference: the First Division record is the professional club. The other record is the amateur club of the same city.
- 10 reserve-team rows of the kept tiers pointed at the first team: SL16 FC (2022/23 to 2025/26) to 10438 "Standard de Liège 16 FC", OH Leuven U-23 (2022/23 to 2025/26) to 10379 "Oud-Heverlee Leuven U23", Union SG B (2024/25, 2025/26) to 9450 "Union Saint-Gilloise U23".

Result of the trace:

- Every listed tier now holds its real count, and no database club is used twice in a season.
- Every club that a listed tier takes from another kept tier appears in that tier's list.
- Clubs left over in the lowest tier (Eerste Divisie, Belgian tier 3) are real clubs of a lower level. They go to the pool (database Belgian fourth divisions or no division). The build decides the record.
- 2020/21: AFC Tubize and Royal Excelsior Virton leave the database Second Division and are in no kept tier. Virton was sent to the Second Amateur Division after its licence was refused (https://en.wikipedia.org/wiki/2020%E2%80%9321_Belgian_First_Division_B). Tubize is in none of the real lists of tiers 1 to 3 for 2020/21. Both go to the pool.
- Real clubs with no database record in the kept tiers: 0. The 31 clubs of the earlier "Missing clubs" table were all in the deleted tiers.

### 4. Reserve and B teams (Q289)

Every reserve team of a kept tier has a record in its season database: the Jong teams of the Eerste Divisie from 2013/14, Club NXT from 2020/21, and the U23 teams of the Challenger Pro League and the Belgian third tier from 2022/23. Reserve teams to add: 0.

### 5. Part rules

None of the part rules (Brazil, UEFA, Scotland) applies to this part. Q277 changes no row.

## Second source, pass 2

Date: 2026-10-17. Second-source agent. File: `hol-bel.check2.tsv`. Lint after the pass: 0 problems (0 before).

Scope: the check file has 971 lines with verdict `no second source`. 634 of them name a row that Pass 2 deleted, or a field that holds `unknown`, `n/a` or an inference. I skipped those. The other 337 lines are in `hol-bel.check2.tsv`.

| Verdict | Lines |
|---|---|
| agree | 220 |
| disagree, settled | 1 |
| disagree, still split | 1 |
| no second source | 115 |

### Main second sources

- Three substitutes from 1995 (all `subs_used` = 3 rows, 2001/02 to 2019/20): https://nachspielzeiten.de/?p=4495. The page gives the Laws of the Game, not a league rule.
- Belgian five substitutes in 2024/25 and 2025/26: the RBFA rulebook, Book B, art. B6.59 (https://www.voetbalinbelgie.be/downloads/2024-2025-URBSFA_Reglement_LIVRE_B.pdf and the 2025/26 edition). Older editions are not online.
- Dutch bench of 12 in 2016/17: KNVB news of 11 July 2016 (https://www.knvb.nl/node/19132).
- Holland 2026/27: the KNVB promotion and relegation rules, the play-off rules and the European ticket rules of 1 August 2026, and the Eurojackpot KNVB Beker rules 2026/27.
- Belgium 2026/27 second tier: Voetbalkrant, 2 June 2026.
- Belgian second tier 2001/02 to 2014/15, Belgian Cup 2001/02 to 2004/05, First Division B 2020/21 and 2021/22: RSSSF season pages. These pages do hold the Belgian Cup rounds, which the first check missed.
- Belgian foreign players and the non-EU minimum salary to 2020/21: L'Avenir, 14 December 2020.
- Belgian squad registration 2025/26: RBFA rulebook 2025/26, art. B4.114 and B6.108.

### Changes to hol-bel.tsv

- hol-bel-328 (2026/27 Eerste Divisie), `playoffs`. Old value: "six clubs: two seeded into the quarter-finals, four in the first round, with the Eredivisie 16th; one Eredivisie place". New value: seven clubs for one Eredivisie place. Six Eerste Divisie first teams play a two-legged first round of three ties. The Eredivisie 16th enters in the two-legged semi-finals. The final has two legs. Decision: second. The KNVB play-off rules for 2026/27 (art. 2, 5 and 6) are the primary rule text: https://www.knvb.nl/downloads/bestand/30197/doc. The 2025/26 rules had four rounds, so hol-bel-315 stays as it is. No other row copies the text.

### Still split, for Max

- hol-bel-12 (2001/02 Belgian Cup), `format`. The first source (English Wikipedia) says that the quarter-finals were "one leg with replay". It cites the RSSSF page for that season. That RSSSF page lists four single quarter-final matches with no replay, and one of them went to extra time (Mouscron 5-2 Genk aet): https://www.rsssf.org/tablesb/belg02.html. I found no third publisher. The UEFA.com page about the Mouscron cup run says nothing about it. I kept the value. Proposal: "quarter-finals one leg, extra time and penalties", because the RSSSF list of matches is the primary record.

### Other findings

- The RBFA rulebooks 2024/25 and 2025/26 give a Belgian bench of 9 for 1A, 1B and the Belgian Cup, and 12 for the 2025/26 cup final. The `bench` fields of those rows hold `unknown`. I did not change them, because this pass covers only filled values.
- The RBFA rulebook gives the squad rule (8 trained by Belgian clubs in a list of 25, art. B4.114) and the match-sheet rule (6, art. B6.108). For seasons before 2024/25, football-legal.com says only that the URBSFA brought in home-grown rules in 2011.
- The Belgian third tier from 2009/10 has no second publisher that I could read. RSSSF stops at the second level, Soccerway loads its tables by script, and worldfootball.net and transfermarkt block the request. The names from 2010/11 to 2015/16 agree with Soccerway.
- Most `no second source` lines are benches and Super Cup substitutes. Only knvb.nl and Wikipedia state them.

## Closing

Date: 2026-10-17. Closing agent. `rules_lint.py`: 0 problems before and after.

- Belgian `bench`, unknown changed to 9: hol-bel-307 (Pro League), 308 (Challenger Pro League) and 311 (Belgian Cup) for 2024/25, and hol-bel-320 and 321 for 2025/26. hol-bel-324 (Belgian Cup 2025/26) is "9 (12 in the final)". Source: the RBFA rulebooks, Book B, art. B6.59, table of substitutes. The 2024/25 table gives 9 for the first team of 1A and 1B and 9 for the Belgian Cup. The 2025/26 table gives 9 for 1A and 1B, 9 for the cup to the semi-finals and 12 for the final. URLs, read on 2026-10-17: https://www.voetbalinbelgie.be/downloads/2024-2025-URBSFA_Reglement_LIVRE_B.pdf and https://www.voetbalinbelgie.be/downloads/2025-2026-URBSFA_Reglement_LIVRE_B.pdf
- Division 1 and the Super Cup bench fields stay `unknown`.
