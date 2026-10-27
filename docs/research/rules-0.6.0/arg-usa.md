# Rules table part arg-usa: Argentina and the United States

Slice 8 of `docs/specs/2026-10-16-seasons-0.6.0-spec.md`. Research of 2026-10-16. Brief: the 0.6.0 research brief (kept outside the repo).

Files:

- `arg-usa.tsv`: 378 rows, 14 competitions times 27 seasons (2001/02 to 2027/28).
- `arg-usa-clubs.tsv`: 1,316 rows, the real clubs of each resized division at each season start that has a database.
- This file: method, scope, findings, missing clubs and unknowns.

## Scope

| Comp | comp_file | Rows |
|---|---|---|
| Primera División (tier 1) | `arg_prm` | 27 |
| Primera B Nacional, Primera Nacional from 2019/20 (tier 2) | `arg_second` | 27 |
| Copa Argentina | new | 27 |
| Supercopa Argentina | new | 27 |
| Trofeo de Campeones | new | 27 |
| Copa de la Superliga and Copa de la Liga Profesional (league cup) | new | 27 |
| Supercopa Internacional | new | 27 |
| Argentina nation rules | `argentina_rules` | 27 |
| Major League Soccer | `usa_mls` | 27 |
| MLS All-Star Game | `usa_mls_all_stars` | 27 |
| U.S. Open Cup | `usa_open_cup` | 27 |
| Leagues Cup | new | 27 |
| Campeones Cup | new | 27 |
| United States nation rules | `usa_rules` | 27 |

Left out, for Max to decide:

- The US second tier (A-League, later USL First Division and USL Championship). The original holds "American A-League" and "American D3-Pro League" as data divisions with no competition code, and MLS has no promotion. I did not research them.
- The Argentine third tier (Primera B Metropolitana, Torneo Argentino A, later Federal A). The original holds "Argentine Third Division" as a data division with no code.
- The North American SuperLiga (2007 to 2010) and the Canadian Championship. They may belong to slice 9.

## Method

- Season pages of English Wikipedia, read as raw wikitext with `curl` and `grep` on 2026-10-16. Each row names its pages in `sources`. Where one page gives every fact of the row, the row uses `all=`.
- Other pages read today: mediotiempo.com (foreign player rule, December 2017), lapaginamillonaria.com (substitutes, June 2022), si.com (MLS 2020 substitutes and matchday roster), mlssoccer.com (2025 competition guidelines).
- Calendar years follow the brief. A calendar-year season sits in the row of the game season that ends in that year: MLS 2002 is the 2001/02 row. Argentina played August to June until 2013/14, then calendar years 2015 and 2016 (first half), then August to June from 2016/17 to 2019/20, then calendar years from 2021. So the 2014/15 row holds 2015, the 2015/16 row holds 2016, and the 2020/21 row holds 2021. The bridge tournaments (2014 Torneo de Transición, 2020 Copa Diego Maradona, the 2020 Primera Nacional transition) are in the notes of the next row.
- MLS moves to a summer to spring calendar. The 2026/27 row holds the 2027 "Sprint Season" (February to May 2027, 14 games). The 2027/28 row holds the first summer to spring season (July 2027 to April 2028, playoffs May 2028, winter break mid-December to early February).
- `up` and `down` give direct places. Play-offs, promoción and Torneo Reducido are in `playoffs`. Where a page gives only a total, `down` says "infobox".
- A fact that I worked out and did not read is marked "inference".
- Club lists come from the club table of each season page, matched to the season database by exact name, by a list of known name forms, or not at all. I checked every match whose names differ by hand. A fuzzy first pass gave wrong matches (Sarmiento de Junín to Sarmiento of Resistencia, Racing de Córdoba to Racing Club), so the final pass uses exact names and a reviewed alias list only.

## Findings for the build

- **Argentine relegation by average (promedios).** Points per game over the last three top-flight seasons (four seasons in 2016 to 2017/18). Up to 2011/12: 19th and 20th of the average table go down, and 17th and 18th play a two-leg promoción against Primera B Nacional clubs. 2012/13 and 2013/14: bottom three go down, no promoción. From 2022/23: one club down by average and one club down as the bottom of the season aggregate table. No relegation in 2019/20, 2020/21 and 2023/24.
- **Argentine sizes.** 20 clubs to 2013/14 (the database size), then 30, 30, 30, 28, 26, 24, 26, 28, 28, 28, 30, 30. The second tier has 25 clubs in 2001/02 (the database size), 20 from 2002/03 to 2012/13, then 22, 22, 22, 23, 25, 25, 32, 35, 37, 37, 38, 36, 36.
- **Argentine split seasons.** Apertura and Clausura to 2011/12 (two champions). Inicial and Final with a Superfinal in 2012/13 and 2013/14. Apertura and Clausura return in 2025 with two zones of 15 and knockouts from the round of 16.
- **Argentine cups the original lacks.** Copa Argentina from 2011/12 (single matches at neutral venues), Supercopa Argentina from 2012, Copa de la Superliga 2019 and 2020, Copa de la Liga Profesional 2020 to 2024, Trofeo de Campeones from 2019, Supercopa Internacional from 2022.
- **MLS.** 10 clubs in 2002 to 2004, then 12, 12, 13, 14, 15, 16, 18, 19, 19, 19, 20, 20, 22, 23, 24, 26, 27, 28, 29, 29, 30, 30, 30. Two conferences every season. Playoffs of 8 (to 2010), 10 (2011 to 2014), 12 (2015 to 2018), 14 (2019, 2021, 2022), 18 (2020, 2023 to 2026), 16 (2027). The database has 12 MLS clubs in every season.
- **MLS roster rules.** Designated Players: none before 2007, 1 (tradable, maximum 2) from 2007, 2 plus a taxed third in 2010, up to 3 from 2011. International roster slots: 8 per club from 2008, tradable.
- **Substitutes.** 3 until 2019/20 in both nations (inference from Law 3). MLS: 5 subs and a matchday roster of 23 in 2020, 5 subs and 9 named in 2025. Argentina: 5 subs in three windows by June 2022.
- **Original's All-Star code** picks two sides from the two conferences. Real MLS played that format only in 2004 in this period. Other years are MLS All-Stars against a guest club or Liga MX All-Stars.
- **Foreign players.** The original holds 4 for Argentina and 4 for the United States. Argentina: 5 registered and 4 in the match before 2018, 6 registered and 5 in the match from January 2018. The earlier history is unknown.

## Database oddities for Max

- Every season database keeps the 12 MLS records of 2001 in the MLS division, with the old names: Miami Fusion FC, Tampa Bay Mutiny, Dallas Burn, Kansas City Wizards, NY/NJ Metrostars and Washington DC United. The newer MLS clubs sit in "American A-League" or other divisions. Inter Miami CF and Los Angeles FC have no first-team record in the databases from 2023/24 to 2025/26 (only their reserve sides). I do not know whether the Update Team uses the Miami Fusion and Tampa Bay records for them.
- 45 club rows have no record in the season database. MLS: Houston Dynamo (6 seasons), Seattle Sounders FC (7), Inter Miami CF (5), Los Angeles FC (4), Real Salt Lake (4), Toronto FC (4), Chivas USA (3), New York City FC (3), Philadelphia Union (3), Portland Timbers (3), Atlanta United FC (2019/20). Argentina: Atlético Paraná and Central Córdoba (SdE) in 2014/15. Rows with an empty `db_club_id` in the clubs file show each case.
- Record ids move between databases (for example id 10292 is "Seattle Sounders" in September 2011 and "New York City FC" in October 2019). The clubs file uses each season's own database.

## Missing clubs

Strict reading of Q244: a short division takes clubs only from clubs with no division. No Argentine or US club in these lists has "none" as its database division, so every gap stays open. Almost all real clubs exist in the database, in the second or third division. Max decides whether club moves may take clubs from those divisions.

"Short after Q244" = real clubs minus clubs already in that database division. The second tier also has more database clubs (25) than real clubs (20) in many seasons; those extra clubs must move out.

| Season | Division | Real | DB count | In DB division | From no division | In other DB divisions | Not in DB | Short after Q244 |
|---|---|---|---|---|---|---|---|---|
| 2014/15 | Primera División | 30 | 20 | 20 | 0 | 10 | 0 | 10 |
| 2015/16 | Primera División | 30 | 20 | 20 | 0 | 10 | 0 | 10 |
| 2016/17 | Primera División | 30 | 20 | 20 | 0 | 10 | 0 | 10 |
| 2017/18 | Primera División | 28 | 20 | 18 | 0 | 10 | 0 | 10 |
| 2018/19 | Primera División | 26 | 20 | 20 | 0 | 6 | 0 | 6 |
| 2019/20 | Primera División | 24 | 20 | 20 | 0 | 4 | 0 | 4 |
| 2020/21 | Primera División | 26 | 20 | 20 | 0 | 6 | 0 | 6 |
| 2021/22 | Primera División | 28 | 20 | 20 | 0 | 8 | 0 | 8 |
| 2022/23 | Primera División | 28 | 20 | 20 | 0 | 8 | 0 | 8 |
| 2023/24 | Primera División | 28 | 20 | 19 | 0 | 9 | 0 | 9 |
| 2024/25 | Primera División | 30 | 20 | 19 | 0 | 11 | 0 | 11 |
| 2025/26 | Primera División | 30 | 20 | 19 | 0 | 11 | 0 | 11 |
| 2002/03 | Primera B Nacional | 20 | 25 | 16 | 0 | 4 | 0 | 4 |
| 2003/04 | Primera B Nacional | 20 | 25 | 13 | 0 | 7 | 0 | 7 |
| 2005/06 | Primera B Nacional | 20 | 25 | 12 | 0 | 8 | 0 | 8 |
| 2007/08 | Primera B Nacional | 20 | 25 | 20 | 0 | 0 | 0 | 0 |
| 2008/09 | Primera B Nacional | 20 | 25 | 20 | 0 | 0 | 0 | 0 |
| 2009/10 | Primera B Nacional | 20 | 25 | 19 | 0 | 1 | 0 | 1 |
| 2010/11 | Primera B Nacional | 20 | 25 | 20 | 0 | 0 | 0 | 0 |
| 2011/12 | Primera B Nacional | 20 | 25 | 20 | 0 | 0 | 0 | 0 |
| 2012/13 | Primera B Nacional | 20 | 25 | 20 | 0 | 0 | 0 | 0 |
| 2013/14 | Primera B Nacional | 22 | 25 | 20 | 0 | 2 | 0 | 2 |
| 2014/15 | Primera B Nacional | 22 | 25 | 10 | 0 | 10 | 2 | 12 |
| 2015/16 | Primera B Nacional | 22 | 25 | 14 | 0 | 8 | 0 | 8 |
| 2016/17 | Primera B Nacional | 23 | 25 | 15 | 0 | 8 | 0 | 8 |
| 2019/20 | Primera Nacional | 32 | 25 | 21 | 0 | 11 | 0 | 11 |
| 2020/21 | Primera Nacional | 35 | 25 | 19 | 0 | 16 | 0 | 16 |
| 2021/22 | Primera Nacional | 37 | 25 | 17 | 0 | 20 | 0 | 20 |
| 2022/23 | Primera Nacional | 37 | 25 | 18 | 0 | 19 | 0 | 19 |
| 2023/24 | Primera Nacional | 38 | 25 | 16 | 0 | 22 | 0 | 22 |
| 2024/25 | Primera Nacional | 36 | 25 | 14 | 0 | 22 | 0 | 22 |
| 2025/26 | Primera Nacional | 36 | 25 | 14 | 0 | 22 | 0 | 22 |
| 2001/02 | Major League Soccer | 10 | 12 | 10 | 0 | 0 | 0 | 0 |
| 2002/03 | Major League Soccer | 10 | 12 | 10 | 0 | 0 | 0 | 0 |
| 2003/04 | Major League Soccer | 10 | 12 | 10 | 0 | 0 | 0 | 0 |
| 2007/08 | Major League Soccer | 14 | 12 | 10 | 0 | 0 | 4 | 4 |
| 2008/09 | Major League Soccer | 15 | 12 | 10 | 0 | 3 | 2 | 5 |
| 2009/10 | Major League Soccer | 16 | 12 | 10 | 0 | 3 | 3 | 6 |
| 2010/11 | Major League Soccer | 18 | 12 | 10 | 0 | 6 | 2 | 8 |
| 2011/12 | Major League Soccer | 19 | 12 | 10 | 0 | 7 | 2 | 9 |
| 2012/13 | Major League Soccer | 19 | 12 | 10 | 0 | 7 | 2 | 9 |
| 2013/14 | Major League Soccer | 19 | 12 | 10 | 0 | 7 | 2 | 9 |
| 2014/15 | Major League Soccer | 20 | 12 | 10 | 0 | 8 | 2 | 10 |
| 2015/16 | Major League Soccer | 20 | 12 | 10 | 0 | 8 | 2 | 10 |
| 2016/17 | Major League Soccer | 22 | 12 | 10 | 0 | 10 | 2 | 12 |
| 2017/18 | Major League Soccer | 23 | 12 | 10 | 0 | 10 | 3 | 13 |
| 2018/19 | Major League Soccer | 24 | 12 | 10 | 0 | 12 | 2 | 14 |
| 2019/20 | Major League Soccer | 26 | 12 | 10 | 0 | 13 | 3 | 16 |
| 2020/21 | Major League Soccer | 27 | 12 | 10 | 0 | 15 | 2 | 17 |
| 2021/22 | Major League Soccer | 28 | 12 | 10 | 0 | 16 | 2 | 18 |
| 2022/23 | Major League Soccer | 29 | 12 | 10 | 0 | 17 | 2 | 19 |
| 2023/24 | Major League Soccer | 29 | 12 | 10 | 0 | 17 | 2 | 19 |
| 2024/25 | Major League Soccer | 30 | 12 | 10 | 0 | 18 | 2 | 20 |
| 2025/26 | Major League Soccer | 30 | 12 | 10 | 0 | 18 | 2 | 20 |

The full list of clubs per row is in `arg-usa-clubs.tsv`. Seasons 2004/05 and 2006/07 have no database and no club rows. Seasons where the real count equals the database count (Primera 2001/02 to 2013/14, B Nacional 2001/02, 2017/18 and 2018/19, MLS 2005/06) have no club rows.

## Unknowns

- 2026/27 and 2027/28 for Argentina: no format announced in a page read today. Q248 keeps the 2026 rules.
- Bench sizes: Argentina in every season except 2021/22 (12, from a sentence about a proposal to go from 12 to 15, so treat it with care), MLS except 2020 (12) and 2025 (9).
- Argentina: foreign rule before 2016/17, work permits, squad registration (lista de buena fe) and transfer windows.
- United States: international slots before 2008, work permits, transfer windows before 2026 (the league page gives the current windows only).
- Extra time and penalties in the U.S. Open Cup, the Supercopa Argentina and the Copa Argentina (the Copa Argentina match reports show penalties straight after 90 minutes: inference).
- Supercopa Argentina 2025 and 2026 results and format, Leagues Cup and All-Star Game for 2027 and 2028.
- 2024 and 2025 U.S. Open Cup entrant totals.
- The 2019 Trofeo de Campeones de la Superliga: the page says it was played once and gives no year, so the 2018/19 row is an inference.
- A LPF 2025 regulations PDF was fetched for substitutes and foreign players, but it holds no text layer, so it gave nothing.

## Reconcile

Third pass of 2026-10-16 over the 7 `disagree` verdicts and the 43 `found` values of `arg-usa.check.tsv`. Every decision and its URL is in `arg-usa.reconcile.tsv`. Row order and ids in `arg-usa.tsv` are unchanged. `arg-usa-clubs.tsv` needs no change: no decision moves a club, and the check matched the four lists it sampled.

### Counts

| Decision | Fields |
|---|---|
| first | 0 |
| second | 7 |
| conflict | 0 |
| found: accepted | 41 |
| found: rejected | 2 |

### Disagreements

1. arg-usa-34 and arg-usa-35, `down` (Primera B Nacional 2007/08 and 2008/09). Both sources agree on 2 direct places. The field said "2 relegated in total", which is an outcome and was wrong. `down` now holds the 2 direct places only. The notes give the outcome: 3 relegated each season, the third by a relegation play-off (Nueva Chicago lost to Los Andes in 2008, Los Andes lost to Deportivo Merlo in 2009). Third sources: El Litoral (28 June 2008) and Diario de Cuyo.
2. arg-usa-127, `format` (Trofeo de Campeones 2020). River beat Banfield in the semi-final on 22 February 2023. The final against Boca was never played. Third source: La Página Millonaria (22 February 2025). The first agent's own page now lists 2020 as "not held".
3. arg-usa-186 and arg-usa-187, `format` (Supercopa Internacional). The match of 26 September 2026 at Santiago del Estero (Rosario Central 3-1 Estudiantes) is filed as the 2025 edition in row 186. Central qualified as champion of the 2025 annual table (Perfil, 27 September 2026), and rows 183 to 185 already file by edition. Row 187 (2026 edition) is now `unknown`. The venue list in rows 183 to 185 and 187 now says "2025 Santiago del Estero". English Wikipedia labels the match 2026, so Max may want to confirm the filing by edition.
4. arg-usa-235, `bench` (MLS 2020). No third publisher found. The primary record, the 2020 MLS competition guidelines, gives 20 players on the match roster with nine substitutes. The AP figure of 23 is kept in the notes.
5. arg-usa-376, `windows` (United States 2026). The league's roster rules give 26 January to 26 March and 13 July to 2 September 2026.

### Found values

- Accepted, with each page read today: MLS benches of 2021 to 2024 and 2026 (9), Argentine benches of 2021 (12, Boletín 5853) and of the 2024 Primera Nacional deciders (12, Boletín 6579), the 2021 Argentine registration and window, the 2007 MLS foreign rule, MLS windows of 2020 to 2023 and 2025, U.S. Open Cup totals for 2023 to 2025 and 2027, and extra time with penalties in 16 Open Cup rows.
- Accepted as inferences: bench 7 in arg-usa-12, -13, -15, -69 and -70, from the named substitutes in title or final line-ups.
- Rejected: arg-usa-16 (the page has line-ups of the cup final only, not of the league) and arg-usa-68 (the page has no line-up of the 2014/15 Copa Argentina final; its 7 + 7 line-ups are of a league match of December 2014). Both stay `unknown`.

### Effect on the Unknowns list above

Bench sizes for MLS 2021 to 2024 and 2026, the 2021 Argentine squad list and window, the 2020 to 2025 MLS windows (except 2024) and the 2024 and 2025 Open Cup totals are now filled. The bullets above describe the state before this pass.

### Conflicts

None. Point for Max to confirm: the Supercopa Internacional rows file each match by edition, not by the year of play (item 3).

### Seen but not changed

Boletín 5853 art. 33.1 states up to 5 substitutions for the 2021 tournaments. Rows arg-usa-20 and -155 mark that value as an inference. It was not a disagreement, so the field is unchanged.


## Source fill

Method: the lint of 2026-10-16 listed each filled fact field without a `field=URL` entry. For each one, I read the pages the row already cites today (curl, text search for the value). If no cited page states the value, I read the general page of the competition. I added `field=URL` to `sources` only where the page text holds the value. I changed no values.

Sources added: 137 fields (name 137).

- Rows for 2025/26 to 2027/28 with `unknown` facts cite the general page of the competition for `name`, for example https://en.wikipedia.org/wiki/Supercopa_Argentina and https://en.wikipedia.org/wiki/U.S._Open_Cup.

Disagreements: none. Fields with no source: none.

## Pass 2

Decision pass of 2026-10-17, by the decisions Q268 to Q331 of the spec. Row ids in `arg-usa.tsv` stay as they were. The lint gives 0 problems before and after this pass. `arg-usa.check.tsv` and `arg-usa.reconcile.tsv` still name the deleted rows. I did not change them.

### Tiers and nations (Q331, Q288)

- The lowest tier of the original's competition files is tier 2 for Argentina (`arg_prm`, `arg_second`) and tier 1 for the United States (`usa_mls`). No row and no club row of this part is below those tiers. The Argentine third tier and the US second tier stay out.
- Deleted: the 27 Leagues Cup rows and the 27 Campeones Cup rows (arg-usa-298 to arg-usa-351), 54 rows in all. Both need Mexican clubs. No club rows belonged to them. The SuperLiga and the Canadian Championship were never in the table.
- What the original does with Canadian clubs: the 3.9.68 data holds Montreal Impact, Toronto Lynx and Vancouver Whitecaps in "American A-League" with the nation United States. Every season database does the same for Toronto FC, Vancouver Whitecaps FC and Montreal Impact (CF Montréal), and for their reserve sides. Canada has its own clubs only from the later databases (Canadian Premier League), and Canada is not a nation a game can select.
- MLS rows: no change to the rules. The Canadian MLS clubs stay in MLS, because the data holds them as United States clubs. Each MLS row from 2006/07 says so in `notes`. Rows arg-usa-238 and -239 say that the game does not pause MLS for the Leagues Cup.
- U.S. Open Cup rows: Canadian clubs never entered the real cup. The 2007 page names only "US-based" MLS and USL First Division sides (https://en.wikipedia.org/wiki/2007_U.S._Open_Cup), and the Canadian Championship page says that no domestic cup was open to Canadian professional clubs from 1993 to 2007 (https://en.wikipedia.org/wiki/Canadian_Championship). The original's entrant step (`UsaOpenCup::collect_entrants`, 0x90b530, medium-confidence name) draws ten clubs from each of two US lower divisions. In the 3.9.68 data those hold the three Canadian A-League clubs, so the original can draw them (inference). Decision by real life: the build leaves clubs based in Canada out of the Open Cup. Each held Open Cup row says so in `notes`. Row arg-usa-295 says that its Leagues Cup exception does not apply.
- MLS All-Star Game: the original plays the two conferences against each other in every season. The real opponents of 2002 (Guadalajara) and of 2021, 2022, 2024, 2025 and 2026 (Liga MX All-Stars) need Mexico. Those rows now say that the game plays the original's match between the conferences.

### Conflicts and open points

The reconcile pass left no conflict. I settled the open points of this file as follows.

1. Supercopa Internacional filed by edition (Reconcile item 3): kept. Q277 puts a late edition at its normal time in the season it belongs to, so the 2025 edition (played 26 September 2026) stays in row arg-usa-186. Source: https://www.rsssf.org/tablesa/arg2025.html
2. Trofeo de Campeones de la Superliga (arg-usa-126): the inference on the year is now a fact. Played on 14 December 2019 at Mar del Plata, Racing 2-0 Tigre. It pairs the Superliga 2018-19 and Copa de la Superliga 2019 winners, so it stays in 2018/19 as a late edition (Q277). Source: https://www.rsssf.org/tablesa/arg2019.html
3. Primera 2013/14 tie-break (arg-usa-13): `playoffs` stays "none". The notes now give the rule of a one-match decider for a tie on the average for the last relegation place (2014: Colón 0-1 Atlético de Rafaela). Source: https://www.rsssf.org/tablesa/arg2014.html
4. Argentine foreign rule of 2026 (arg-usa-214): 6 foreign contracts, 5 on the match sheet. The Vélez protest about Boca in September 2026 did not change the rule. Source: https://www.perfil.com/noticias/deportes/boca-puede-quedarse-afuera-de-la-copa-argentina-por-un-reclamo-de-velez.phtml
5. US second tier, Argentine third tier, SuperLiga and Canadian Championship ("Left out, for Max to decide"): settled by Q331 and Q288. They stay out.
6. Missing clubs after Q244: settled by Q279. Clubs in other database divisions are now in the pool, so only clubs with no record stay missing (below).

### Club moves (Q279, Q280, Q290)

Q290 names no ended tier for these nations. 122 club rows added to `arg-usa-clubs.tsv` (1,316 to 1,438 rows):

| Season | Division | Database | Clubs | Reason |
|---|---|---|---|---|
| 2002/03 | Primera División | 3.9.68 | 20 | B Nacional takes 2 clubs from the database Premier Division |
| 2003/04 | Primera División | October 2003 | 20 | B Nacional takes 4 clubs from the database Premier Division |
| 2005/06 | Primera División | September 2005 | 20 | B Nacional takes 4 clubs from the database Premier Division |
| 2017/18 | Primera B Nacional | October 2017 | 25 | Primera takes 10 clubs from the database Second Division |
| 2018/19 | Primera B Nacional | October 2018 | 25 | Primera takes 6 clubs from the database Second Division |
| 2005/06 | Major League Soccer | September 2005 | 12 | Count equal (12), but the database keeps Miami Fusion and Tampa Bay Mutiny, which folded after 2001. Added by real life (MLS 2006) |

Sources: the Wikipedia season page of each list (in the `source` column). Every list now covers each tier from the top down to the lowest tier in each season with a database.

Missing clubs (no record in that season's database, for Max). The build cannot fill these places without new club records:

- Argentina 2014/15: Club Atlético Paraná, Central Córdoba de Santiago del Estero.
- MLS: Chivas USA (2005/06, 2007/08 to 2009/10), Houston Dynamo (2005/06, 2007/08 to 2010/11, 2012/13, 2013/14), Real Salt Lake (2005/06, 2007/08, 2011/12, 2012/13, 2014/15), Toronto FC (2007/08, 2017/18, 2018/19, 2020/21), Philadelphia Union (2009/10 to 2011/12), Seattle Sounders FC (2013/14 to 2015/16, 2017/18, 2018/19, 2020/21, 2021/22), New York City FC (2015/16, 2016/17, 2022/23), Portland Timbers (2016/17, 2019/20, 2022/23), Los Angeles FC (2017/18, 2023/24 to 2025/26), Atlanta United FC (2019/20), Inter Miami CF (2019/20, 2021/22, 2023/24 to 2025/26).
- 48 club rows in all have an empty `db_club_id`. Several of these clubs have only a reserve side in the database (for example Inter Miami CF II, Seattle Sounders 2, Portland Timbers 2, Atlanta United 2, Toronto FC II).

### Reserve and B teams (Q289)

None. No tier of this part has reserve or B teams in real life (Argentine tiers 1 and 2, MLS).

## Second source, pass 2

Pass of 2026-10-17 over the check lines with verdict `no second source` whose field still holds a value. I skipped 58 lines of the 54 deleted rows (Leagues Cup and Campeones Cup), 431 lines whose field now says `unknown` or `n/a` (or starts with "unknown"), and 25 lines whose value is an inference. That left 69 lines. Each verdict, URL and note is in `arg-usa.check2.tsv`. Each settled disagreement is also in `arg-usa.reconcile.tsv`, with the note "pass 2". Row order and ids in `arg-usa.tsv` are unchanged. `arg-usa-clubs.tsv` needs no change. The lint gives 0 problems before and after this pass.

### Counts

| Verdict | Lines |
|---|---|
| agree | 33 |
| disagree, settled | 10 |
| disagree, still split | 0 |
| no second source | 26 |

All 10 disagreements are on the U.S. Open Cup and all 10 are settled `second`. Each has a third publisher.

### Changes to arg-usa.tsv

The first pass copied one Open Cup text into every row from 2002 to 2011: "40 clubs in the competition proper, 8 from each level", with MLS qualifying games. The general Wikipedia page says only that the cup had 40 teams before 2012. The pages of each year give other facts.

1. arg-usa-271 (2002), `clubs` 40 to 32. The Richmond Kickers release of June 2002 gives "the full bracket of 32 teams". RSSSF lists the same rounds. `format` now says: 16 clubs (7 D3 Pro League, 9 amateur) in round 1, 8 A-League clubs in round 2, 8 MLS clubs in round 3. Six MLS clubs entered on 2001 results and two by the league table on 1 June 2002, with no qualifying games (TheCup.us).
2. arg-usa-272 (2003), `clubs` 40 to 34. TheCup.us and RSSSF list 33 matches. `format`: all 10 MLS clubs enter (2 in round 3, 8 in round 4), sudden-death extra time.
3. arg-usa-273 (2004), `format`: all 10 MLS clubs enter (Dallas and Columbus in round 3, the other 8 in round 4) (RSSSF, TheCup.us). `clubs` 40 agrees.
4. arg-usa-274 (2005) and arg-usa-275 (2006), `clubs` 40 to 42. RSSSF says the field grew to 42 teams with a qualifying round. The Press Democrat of 14 June 2006 says all 12 MLS teams are in the 42-team field. `format`: 4 MLS clubs in round 3, 8 in round 4, extra time then penalties.
5. arg-usa-280 (2011), `format`: 8 MLS, 11 USL Pro, 9 PDL, 4 NPSL and 8 USASA clubs, with no Division II club (TheCup.us, US Soccer release). `clubs` 40 agrees.
6. Consistency fixes to the same copied text. These lines were not part of this check. arg-usa-276 (2007): the 32 lower-division clubs were 10 USL-1, 6 USL-2, 8 PDL and 8 USASA (MLS release of 2007). arg-usa-279 (2010): 9 USSF D-2, 6 USL-2, 8 PDL and 9 USASA and US Club Soccer clubs (US Soccer release). I did not change arg-usa-277 (2008: 8 clubs from each level is correct) or arg-usa-278 (2009: the release gives 24 professional and 16 amateur clubs only).

Each changed row has new `clubs=` and `format=` entries in `sources`.

### Agreements of note

- Copa Argentina totals for 2011/12 (186) and 2012/13 (224) are in Diario de Cuyo.
- The 2026 Primera Nacional (arg-usa-52) agrees on every field (442 Perfil, Cadena 3). Cadena 3 calls the extra games one interzonal round on matchday 7. It does not say "derby".
- The MLS Designated Player rule agrees for 2011, 2012, 2014, 2015, 2017, 2019 and 2024. The pages are club sites, the Inquirer and archived copies of the league's roster rules. The first source is Wikipedia.
- The Open Cup totals of 2012 to 2016 agree. The pages give exact totals for "80 or more": 80 in 2014, 91 in 2015 and 92 announced for 2016. I did not change those values.

### No second source

- Argentine substitutes of 2022 (arg-usa-21, -48, -75, -156): only the first source's publisher states the rule.
- Copa Argentina totals of 2013/14, 2014/15, 2016/17 and 2017/18.
- Fields of future editions that no page names yet: the 2027 Copa Argentina entry rule (arg-usa-80), the Trofeo de Campeones of 2027 and 2028, the Copa de la Liga Profesional after 2024, the MLS All-Star Game of 2027 and 2028, the 2027/28 MLS club count, and the MLS roster rules of 2027 and 2027/28.
- MLS 2025 substitutes and bench (arg-usa-240): only mlssoccer.com, the first source's publisher.
- The MLS DP rule of 2013, 2016 and 2018, and the 2018 Open Cup total. The session's web search limit stopped the last searches.

## Second source, retry

On 2026-10-17 a retry agent took the 26 lines of `arg-usa.check2.tsv` with verdict `no second source`. It read league pages through archive.org copies first and used web searches only where it had to. Each changed line in `arg-usa.check2.tsv` has a new verdict, `second_url` and note. The notes of the other lines start with "Retry".

| Result | Lines |
|---|---|
| agree | 7 |
| disagree, settled | 0 |
| disagree, split | 0 |
| still no second source | 19 |

No value in `arg-usa.tsv` changed.

### New agreements

- MLS Designated Player rule of 2013, 2016 and 2018 (arg-usa-363, -366, -368). Archived copies of the league's roster rules pages for each year give two DP slots, a third for 150,000 dollars, and the Young DP charges of 150,000 and 200,000. The first source is Wikipedia.
- Copa Argentina 2016/17 (arg-usa-70): Elonce (6 January 2017) gives 99 clubs.
- Copa Argentina 2017/18 (arg-usa-71): 100 clubs is the sum of two stated counts. Goal lists 52 direct entrants to the final phase. Todo Jujuy gives 48 Federal A and B clubs in the preliminary phase.
- 2018 U.S. Open Cup (arg-usa-287): the US Soccer release of 14 March 2018 gives 94 teams, so "80 or more" holds.
- MLS 2025 substitutes (arg-usa-240): Goal gives five substitutions in three windows plus one concussion substitute.

### Still no second source

- Argentine substitutes of 2022 (arg-usa-21, -48, -75, -156). Searches found only the first source and IFAB pages.
- Copa Argentina totals of 2013/14 (261) and 2014/15 (270). For 2013/14, Minuto Uno gives the preliminary rounds but not the total.
- MLS 2025 bench of 9 (arg-usa-240).
- The 2027 Copa Argentina entry rule (arg-usa-80). La Voz de San Justo confirms the 2027 edition but gives only the Federal A places.
- The 2027/28 MLS club count (arg-usa-243). Flashscore gives 30 teams for the 2027 Sprint Season only.
- Fields of future editions that no page names yet: Trofeo de Campeones 2027 and 2028, Copa de la Liga Profesional 2027 and 2028, MLS All-Star Game 2027 and 2028, and the MLS roster rules of 2027 and 2027/28.

## Pass 3 (Q337)

Q337 (2026-10-17): a real play-off against a tier that is not in the game is played against a club with no division, as for the Scottish pyramid play-off (Q284). The rows below keep the real format in `playoffs`. Each row gets this note: "Q337: the lower-tier side is a club with no division in the game." No other field changed.

Rows: 34, 35, 36, 37, 38 (Primera B Nacional bottom clubs against the Primera B Metropolitana or Torneo Argentino A reducido winner).
