# Rules table: Portugal (part `por`, slice 5)

Date 2026-10-16. Research only. Files: `por.tsv` (243 rows), `por-clubs.tsv` (2,765 rows), this note.

## Method

- Scope: the original's `por_prm`, `por_second`, `por_second_b`, `por_cup` and `por_super`, plus one nation rules row per season. New competitions (`comp_file` = `new`): Liga 3 (tier 3 from 2021/22), Terceira Divisão (tier 4 to 2012/13) and Taça da Liga (from 2007/08).
- Nine rows per season, 2001/02 to 2027/28: Primeira Liga, second tier, the `por_second_b` line, Liga 3, Terceira Divisão, Taça de Portugal, Supertaça, Taça da Liga, nation rules. A season in which a competition does not run has `format` = `not held`.
- The `por_second_b` line follows one lineage: Segunda Divisão B (to 2004/05), Segunda Divisão (2005/06 to 2012/13), Campeonato Nacional de Seniores (2013/14, 2014/15), Campeonato de Portugal (2015/16 on). From 2021/22 this line is the fourth tier, because Liga 3 takes the third. The build must decide whether `por_second_b` becomes the fourth tier or Liga 3 takes its code.
- Facts come from raw Wikipedia wikitext (`action=raw`), read on 2026-10-16, with the league tables parsed by script (club count, colour legend per position). The `sources` column names the page for each field.
- 2026/27 uses the season pages that exist. 2027/28 has no season pages. Its rows copy 2026/27 where no change is announced and say so (inference), or say `unknown`.
- Substitutes and bench: Liga Portugal press for the Liga competitions (Primeira Liga, second tier, Taça da Liga). For the FPF competitions (lower leagues, Taça de Portugal, Supertaça) no FPF rule text was found. Those rows give 3 to 2019/20 from the Laws of the Game and 5 from 2020/21 as an inference.

## Main sources

- Season pages on en.wikipedia: `YYYY–YY Primeira Liga`; `Segunda Liga`, `Liga de Honra`, `LigaPro` or `Liga Portugal 2`; `Segunda Divisão B` or `Segunda Divisão`; `Campeonato Nacional de Seniores` or `Campeonato de Portugal`; `Liga 3 (Portugal)`; `Terceira Divisão`; `Taça de Portugal`; `Taça da Liga`; `YYYY Supertaça Cândido de Oliveira`. Templates `YYYY–YY ... table` where a page transcludes its table.
- General pages: Primeira Liga (sizes, sponsor names), Liga Portugal 2 (names), Liga 3, Campeonato de Portugal, Taça da Liga (sponsor names), Taça de Portugal (replays abolished after 2001/02), Supertaça, Transfer window, Substitute (association football).
- Press: noticiasaominuto.com, 8 June 2020 (5 substitutes and bench 7 to 9 from round 26 of 2019/20); observador.pt, 15 May 2023 (bench 9 and 5 substitutes made permanent); abola.pt, 20 May 2026, and dnoticias.pt, 12 August 2026 (bench 12 from 2026/27, 22 under-23 players in the squad); record.pt, 4 December 2014 (8 locally trained players, 27 senior contracts); record.pt, 2 January 2019 (Taça da Liga: 2 locally trained starters); dnoticias.pt, 9 July 2024 (end of the "manifestação de interesse" route, windows of 12 and 4 weeks).
- A comparative study of foreign player quotas, updated 30 May 2008 (rdes.it): "No quota for non EU-players" for Portugal. A sports law guide, 2026 (chambers.com): no nationality quotas in Portugal, non-EU players need a residence visa.

## Non-EU players (GitHub issue 121, 11 November 2026)

- 2001/02 to 2005/06, Liga: at most 6 non-EU players registered and at most 4 on the 18-man match sheet (TSF and Público, April 2001; Record, 2001 to 2005; O Jogo and A Bola, 2003 to 2005; Jornal da Madeira, 2005). The FPF voted on 16 December 2000 to cut the limits and revoked the cut on 28 April 2001. The registered limit in 2003/04 has one secondary source. Cotonou did not apply: PALOP players still counted as non-EU. Brazilians did not count only with the individual equality status.
- 2006/07 on: no limit in the Liga and the Liga de Honra. The Liga general assembly of 30 June 2006 ended the foreign player status and its registration limit (Maisfutebol, Record, 30 June 2006; Correio da Manhã, 1 July 2006). The 2008 rdes.it study and the Liga regulations of 2013/14, 2024/25, 2025/26 and 2026/27 have no nationality limit.
- The game: a signing limit of 6, 5 and 4 by division (PORTUGAL_RULES::can_sign_player 0x7d3ef0, can_make_offer 0x7d4040) and a match limit of 4, 3 in the Second Division B (0x7d43c0). They match the Liga of 2001/02 to 2005/06.

## Change points found

| Season | Change |
|---|---|
| 2002/03 | Taça de Portugal replays abolished; FIFA windows compulsory; Primeira Liga named SuperLiga Galp Energia |
| 2005/06 | Primeira Liga 4 down, second tier 6 down, to cut both to 16; Segunda Divisão B becomes Segunda Divisão (4 series, play-off for 2 places) |
| 2006/07 | Primeira Liga and Liga de Honra 16 clubs |
| 2007/08 | Taça da Liga starts (32 clubs) |
| 2008/09 | Taça de Portugal semi-finals two legs; 8 rounds |
| 2009/10 | Terceira Divisão adds a Madeira series |
| 2012/13 | Second tier 22 clubs with B teams, which cannot be promoted |
| 2013/14 | Segunda Divisão and Terceira Divisão merge into the Campeonato Nacional de Seniores (80 clubs); Primeira Liga 15th in a play-off |
| 2014/15 | Primeira Liga back to 18; second tier 24 |
| 2015/16 | Third tier named Campeonato de Portugal; Taça da Liga first rounds single legs |
| 2016/17 | Second tier 22; Taça da Liga final four |
| 2017/18 | Second tier 20; Campeonato de Portugal 5 series of 16 |
| 2018/19 | Second tier 18; Primeira Liga 3 down (Gil Vicente reinstated); Taça de Portugal 4th substitute in extra time |
| 2019/20 | Primeira Liga 5 substitutes and bench 9 from June 2020; second tier abandoned; Campeonato de Portugal cancelled; Taça da Liga no extra time |
| 2020/21 | Primeira Liga 16th v second tier 3rd play-off; Campeonato de Portugal 96 clubs (transition); Taça da Liga 8 clubs |
| 2021/22 | Liga 3 starts (24 clubs); Campeonato de Portugal becomes tier 4; second tier 16th v Liga 3 play-off |
| 2022/23 | Taça da Liga 8 groups during the World Cup |
| 2023/24 | Liga 3 20 clubs; 5 substitutes and bench 9 made permanent |
| 2024/25 | Taça da Liga 8 clubs again; immigration route for non-EU players changed (June 2024) |
| 2026/27 | Bench 12 in Liga competitions; Campeonato de Portugal new relegation format |

## Facts with two values

- 2012/13 Taça de Portugal: infobox 165 entrants, text 162. The row uses 162.
- 2018/19 and 2019/20 Taça da Liga entrants: no number on the page. The row gives the sum of entrants per round (inference).
- 2019/20 Supertaça date: the page gives "August 2019" only.

## Club moves

`por-clubs.tsv` lists every real club of each division whose real size or structure differs from the season database. The database holds 6 Portuguese divisions in every season: Premier League 18, Second League 18, Second Division B North, Central and South (20 each), Third Division (94 to 2018, then 12 to 38), plus a pool "A Lower Division" and, from about 2021, a "Reserve" division.

- Listed: Primeira Liga 2007/08 to 2013/14 (16 clubs); second tier 2007/08 to 2017/18 (16, 22, 24, 22, 20); third tier where a zone is not 20 (2002/03 Centro, 2003/04 Norte) and every season from 2005/06 (4 series, then 3 zones of 16, then 8 series); Terceira Divisão every database season to 2012/13; Liga 3 and Campeonato de Portugal from 2021/22.
- Not listed: seasons with no database (2004/05, 2006/07, 2026/27, 2027/28). Seasons where the real size equals the database size (Primeira Liga and second tier outside the years above, third tier 2001/02).
- Matching is by name, by script, against that season's database. `db_club_id` is empty when no name match was clear. Club ids differ between databases (Sporting is 10915 in November 2007 and 2656 in October 2021).
- From 2013/14 to 2020/21 no national fourth tier exists. The database still has a Third Division of 94 clubs (to October 2018) for those seasons. Max decides what happens to those clubs.

## Missing clubs

Count of real clubs with no confident match in the database, after the reconcile of 2026-10-16. Names of up to 8 shown. The reconcile filled the 55 spelling misses that the check found and fixed three wrong matches (see "Reconcile"). Divisions with no missing club are left out.

| Season | Division | Real clubs | No match | Clubs |
|---|---|---|---|---|
| 2001/02 | Terceira Divisão | 117 | 17 | Águias Graça, União Nogueirense, GD Milheiroense, AD Sátão, GD Portalegrense, CR Alqueidão da Serra, CD São Vicente, AD Carregado, ... |
| 2002/03 | Terceira Divisão | 117 | 21 | FC Marinhas, Águias Graça, Fiães SC, AD São Pedro da Cova, FC Pampilhosa, GD Milheiroense, AD Sátão, UD Tocha, ... |
| 2003/04 | Terceira Divisão | 118 | 34 | Santa Maria FC, CA Cabeceirense, AD Ponte da Barca, CD Rebordelo, Fiães SC, Aliados Lordelo, AD São Pedro da Cova, União Nogueirense, ... |
| 2005/06 | Segunda Divisão | 58 | 8 | União Torcatense, Fiães SC, Aliados Lordelo, GD Tourizense, FC Pampilhosa, SL Nelas, RSC Queluz, Vitória Setúbal B |
| 2005/06 | Terceira Divisão | 115 | 40 | Brito FC, Atlético Cabeceirense, Mondinense FC, FC Vinhais, ADC Correlhã, AC Vila Meã, AD São Pedro da Cova, CR Ataense, ... |
| 2007/08 | Segunda Divisão | 55 | 8 | Vila Meã, Fiães, Eléctrico, Pampilhosa, Nelas, Sátão, Carregado, Messinense |
| 2007/08 | Terceira Divisão | 94 | 34 | Vieira SC, Mondinense FC, FC Marinhas, Vidago FC, GD Prado, Brito SC, Morais FC, Padroense FC, ... |
| 2008/09 | Segunda Divisão | 47 | 8 | Aliados Lordelo, Santana, FC Pampilhosa, GDR Monsanto, Eléctrico FC, AD Carregado, Real SC, Mineiro Aljustrelense |
| 2008/09 | Terceira Divisão | 93 | 29 | Vieira SC, GD Prado, FC Marinhas, Mondinense FC, FC Mãe de Água, Padroense FC, SC Coimbrões, AC Vila Meã, ... |
| 2009/10 | Segunda Divisão | 47 | 10 | Aliados FC Lordelo, Padroense FC, Vieira SC, FC Pampilhosa, Eléctrico FC, GDR Monsanto, Atlético de Reguengos, Aljustrelense, ... |
| 2009/10 | Terceira Divisão | 94 | 29 | Santa Maria FC, FC Marinhas, Morais FC, AC Vila Meã, SC Coimbrões, Fiães SC, CD Candal, GD Milheiroense, ... |
| 2010/11 | Segunda Divisão | 48 | 8 | CF Andorinha, Padroense FC, SC Coimbrões, Aliados FC Lordelo, FC Pampilhosa, Eléctrico FC, Atlético SC Reguengos, Real SC |
| 2010/11 | Terceira Divisão | 94 | 41 | SC Melgacense, Vieira SC, Santa Maria FC, UD Sousense, CD Candal, AC Vila Meã, Mondinense FC, FC Alpendorada, ... |
| 2011/12 | Segunda Divisão | 48 | 2 | Marítimo B, Atlético SC |
| 2011/12 | Terceira Divisão | 93 | 28 | SC Melgacense, FC Marinhas, AD Grijó, SC Mêda, FC Alpendorada, UD Sampedrense, AD Nogueirense, UD Tocha, ... |
| 2012/13 | Segunda Liga | 22 | 1 | Vitória de Guimarães B |
| 2012/13 | Terceira Divisão | 82 | 3 | GD Parada, Atlético S.C., Aljustrelense |
| 2013/14 | Campeonato Nacional de Seniores | 80 | 6 | Vitória de Guimarães B, Lusitano Vildemoinhos, Águias do Moradal, Carapinheirense, Almodôvar, Ferreiras |
| 2014/15 | Campeonato Nacional de Seniores | 80 | 1 | Atl. Reguengos |
| 2014/15 | Segunda Liga | 24 | 1 | Vitória de Guimarães B |
| 2015/16 | Campeonato de Portugal | 80 | 2 | Varzim B, Atlético de Reguengos |
| 2015/16 | LigaPro | 24 | 1 | Vitória de Guimarães B |
| 2016/17 | LigaPro | 22 | 1 | Vitória de Guimarães B |
| 2018/19 | Campeonato de Portugal | 72 | 5 | Chaves B, Mirandês, Mação, Santa Iria, Vasco da Gama Vidigueira |
| 2019/20 | Campeonato de Portugal | 72 | 1 | Chaves B |
| 2020/21 | Campeonato de Portugal | 96 | 10 | Águia Vimioso, Vidago, Brito, Rio Ave B, Vila Cortez, GRAP, Belenenses SAD B, Estrela da Amadora, ... |
| 2021/22 | Liga 3 | 24 | 1 | União de Santarém |
| 2022/23 | Campeonato de Portugal | 56 | 1 | Atlético |
| 2023/24 | Campeonato de Portugal | 56 | 1 | Alverca B |
| 2025/26 | Campeonato de Portugal | 56 | 3 | Chaves B, Ribeira Brava, Portimonense B |

The Primeira Liga, Liga de Honra and LigaPro lists not in this table have a match for every club.

The database count per real division also matters. Example: the real third tier from 2013/14 has 80 clubs, the database 60 in three divisions. The rest must come from the Third Division or the pool.

## Unknowns

- Bench size in Liga competitions before 2019/20 (7 in 2019/20 before June 2020; start of 7 not found).
- Bench size and the start of 5 substitutes in FPF competitions (lower leagues, Taça de Portugal, Supertaça).
- Non-EU limits of the Liga de Honra in 2001/02 to 2005/06 (5 registered and 4 usable in 2000/01). The Liga limits are known (issue 121, below).
- Start season of the 8 locally trained players rule (found in force in 2014/15). Squad registration in other seasons.
- Work permit rules before 2024.
- Exact transfer window dates per season; transfer windows in 2001/02.
- 2026/27 Campeonato de Portugal promotion places to Liga 3, and the 2026/27 Taça de Portugal format change.
- 2027/28: no season pages.

## Inferences

- 5 substitutes and bench 9 in 2020/21 to 2022/23 (the 2023 assembly made a temporary rule permanent).
- Liga rules on substitutes and bench apply to the second tier and the Taça da Liga, not only the Primeira Liga.
- No non-EU quota from 2014/15 to 2023/24: the Liga regulations of those seasons were not read; those of 2013/14 and 2024/25 have none.
- Lower-tier promotion and relegation counts read from table colours where the text gives none.
- 2027/28 rows copy 2026/27.

## Reconcile

Reconcile date 2026-10-16. The full list is in `por.reconcile.tsv` (20 rows: the 17 disputed fields and 3 follow-on fields). Third publishers come mostly from Arquivo.pt copies of Portuguese papers of the time. Two primary records helped: the FPF competition rules (RPO, April 2005 text) and FPF Comunicado Oficial 17 of 17 July 2007. I changed 18 fields in `por.tsv`, added each URL to `sources` and kept row order and ids.

| Decision | Fields |
|---|---|
| first | 4 (por-21 down, por-23 down, por-70 format, por-92 down) |
| second | 9 (por-3, por-6, por-12, por-30, por-57, por-102, por-119, por-173, por-223) |
| conflict | 2 (por-39 down, por-41 down) |
| found value accepted | 2 (por-164 bench 7, por-170 bench 7 as an inference) |
| found value rejected | 0 |

How I decided:

- por-3, por-12, por-21: from 2001/02 to 2003/04 the 16th of each Segunda Divisão B zone played a three-club play-off when the Açores champion went up (RPO 402.02). In 2001/02 one club stayed. In 2002/03 and 2003/04 one club went down. RSSSF counts the play-off loser in its relegation total, Wikipedia does so in 2002/03 only. The `down` fields now hold the direct places (12, 11, 11) and `playoffs` describes the play-off.
- por-23: Maisfutebol lists four clubs to district level in each series A to F, and RPO 502.02 says the same. I applied the same rule text to por-5, por-14 and por-32, which said "about 4".
- por-30: the Açores play-off (RPO 502.01) is a standing rule. A União says it is played "ano após ano".
- por-57: Comunicado Oficial 17 gives 20 places. 19 clubs went down because Série B had 13 clubs.
- por-92, por-119: the brief rule for `up` and `down` applies. Outcomes are in `notes`.
- por-173: the season started as LigaPro. The name Liga Portugal SABSEG dates from 16 October 2020.

Conflicts for Max:

- por-39 (2005/06 Segunda Divisão, down). First value 13 (3, 3, 3, 4). Second value 15 (3, 3, 3, 6). Maisfutebol (May 2006): an FPF rule change in mid-season sent one extra club down for each relegated B team. Vitória de Setúbal B went down, so União Micaelense also went down. That gives 5 in Série D and 14 in all. RSSSF also marks Benfica B, which stayed. Decide whether `down` holds 13 (rules at the start) or 14 (rules as changed). The value stays 13 for now.
- por-41 (2005/06 Terceira Divisão, down). First value "about 4 per mainland series". Second value A 6, B 5, C 5, D 4, E 6. I found no 2005/06 rule text. A Jornal Regional headline names three Série A clubs going to district level, RSSSF marks six. The value stays as first.

Club matches (`por-clubs.tsv`):

- 55 spelling misses filled from the season database, as the check listed.
- Three wrong matches fixed, and the check was wrong on two of them. Atlético CP in Liga 3 (2023/24 to 2025/26) is id 989 "Atlético CP" in October 2023, October 2024 and December 2025, not an empty id. Académica – SF (2015/16, 2016/17) is id 222 "Associação Académica de Coimbra/Secção de Futebol" in October 2015 and October 2016. Lusitano FCV (2014/15) is id 1911 "Lusitano Futebol Clube Viseu", as the check said.
- The missing-club table above is updated. 2013/14 Lusitano Vildemoinhos stays missing. Autumn 2013 has only "Lusitano Futebol Clube" (6845), and I don't know if that is the Viseu club.

Items for Max that the reconcile did not change:

- The Açores play-off of RPO 502.01 applies in every season to 2004/05. The `playoffs` fields of por-3, por-12 and por-21 do not name it.
- por-32 (2004/05 Terceira Divisão) `up` counts the Açores winner as promoted. Madalena went up only through the Açores play-off.

## Loose ends

- Açores play-off (por-3, por-12, por-21): the RPO (articles 401 and 502.01) caps the Açores clubs in the Segunda Divisão B at 3. Only when the cap stops the Açores series winner of the Terceira Divisão does it play the worst-placed Açores club of the Segunda Divisão B over two legs. The play-off is conditional, not yearly. The `playoffs` fields now state the rule. In 2001/02 to 2003/04 the 16th-place play-off took place, which RPO 402.02 holds only when the Açores winner goes up, so the Açores play-off was not needed (inference).
- por-32 (Terceira Divisão 2004/05) `up`: now 12 (top 2 of each mainland series). The Açores winner goes up direct only when the cap allows. In 2004/05 the cap applied and Madalena went up through the play-off against Lusitânia (por-30). The `playoffs` field now describes it.
- Database club 6845 "Lusitano Futebol Clube" is the Vila Real de Santo António club, not the Viseu club. Other databases name 6845 "Lusitano FC Vila Real de Santo António" or "Lusitano Futebol Clube VRSA". The Viseu club is 1911 ("Lusitano Futebol Clube Viseu", "Lusitano FCV"), and in Autumn 2013 id 1911 holds another club. Lusitano Vildemoinhos stays missing for 2013/14. No change to por-clubs.tsv.

## Source fill

Date: 2026-10-16. A source-fill pass added `field=URL` entries to `sources` for filled fact fields that had none. It changed no values and kept row order and ids. Each URL was read on this date, and the value was checked against the page text.

Method:

- For `name`, the pass took the pages that the row already cites, season pages first, and kept the first page that contains the name. For a name with a note in brackets, the base name had to match. For an Italian group (girone), the page also had to name that group.
- For a value of `none` in `split` or `playoffs`, the source is the season page with one league stage and no championship or relegation round.

Fields filled:

- `name`: 115 rows.
- `split` = `none`: 22 rows.
- `split` = second stage with promotion and relegation groups: 7 rows (por-57, 66, 111, 120, 129, 138, 183), each from its season page.
- `format` of Liga 3: 7 rows. 2021/22 to 2025/26 cite the season page ("home-and-away double round-robin", then promotion and relegation series). The rows with no season page cite the Liga 3 main page.
- `format` of por-235 (Primeira Liga 2027/28) and por-236 (Segunda Liga 2027/28): the main league pages, which say that each club plays the others twice, home and away.

Disagreements: none found.

Lint after the pass: 0 problems.

## Pass 2

Date 2026-10-17. Decision agent, by the decisions Q268 to Q331 of the spec. Row ids are kept. `rules_lint.py`: 0 problems before and after.

### Tiers (Q331, Q291, Q290)

The original runs `por_prm`, `por_second`, `por_second_b`, `por_cup` and `por_super`. The lowest tier is tier 3 (`por_second_b`).

- Deleted the 27 Terceira Divisão rows (tier 4, 2001/02 to 2027/28): por-5, 14, 23, 32, 41, 50, and so on, one per season.
- Deleted the 7 Campeonato de Portugal rows from 2021/22 (tier 4 from then): por-183, 192, 201, 210, 219, 228, 237.
- Liga 3 takes the code of `por_second_b` from 2021/22 (Q291). Its 7 rows (por-184, 193, 202, 211, 220, 229, 238) now have `comp_file` = `por_second_b` and `comp` = `Segunda Divisão B`, the label of that line. `name` stays Liga 3.
- Deleted the 20 Liga 3 rows of 2001/02 to 2020/21 (`not held`). The `por_second_b` line now has one row per season, so these rows had no use.
- Rows deleted from `por.tsv`: 54. The file now has 189 rows, 7 per season.
- Each tier 3 row now has a note: clubs that go down become clubs with no division, and clubs that come up come from clubs with no division.
- The clubs of the database division "Portuguese Third Division" have no real tier from 2013/14 to 2018/19. By Q290 they are clubs with no division for the club moves. The same applies to that division in every other season, because tier 4 is not in the game.
- The Taça de Portugal rows keep their real entrants. Entrants below tier 3 come from clubs with no division.

### Conflicts and open points

| Item | Decision | URL |
|---|---|---|
| por-39 `down` (2005/06 Segunda Divisão) | 14 (3, 3, 3, 5), as in real life. An FPF rule change during the season sent one extra club down for each relegated B team. Vitória de Setúbal B went down, so União Micaelense went down too. Benfica B stayed. The note keeps 13 as the value without the rule change. | https://arquivo.pt/wayback/20060511160502/https://maisfutebol.iol.pt/noticia.php?id=679394&div_id=1679 |
| por-41 `down` (2005/06 Terceira Divisão) | No decision needed. The row is deleted (Q331). | n/a |
| por-105 clubs (2012/13 Taça de Portugal): 165 or 162 | 162. The round table gives 100 + 46 + 16 new entries, and the team list gives 16 + 16 + 48 + 82. The infobox value 165 has no support on the page. | https://en.wikipedia.org/wiki/2012%E2%80%9313_Ta%C3%A7a_de_Portugal |
| `por_second_b` from 2021/22 | Liga 3 takes the code (Q291). | spec Q291 |
| Third Division clubs 2013/14 to 2020/21 | Clubs with no division (Q290). | spec Q290 |
| Açores play-offs (por-3, por-12, por-21, por-30) | The `playoffs` fields describe what happened. The Terceira Divisão club in the play-off comes from clubs with no division. por-32 is deleted. | rows' sources |

Conflicts settled: 2 (por-39 decided, por-41 removed with its row). Open points settled: 4.

### Club moves (Q279, Q280, Q290)

Each season database was compared tier by tier, from tier 1 to tier 3. Every database holds 18, 18 and 60 (3 x 20) clubs in tiers 1 to 3.

- Deleted 1,017 Terceira Divisão club rows and 285 Campeonato de Portugal club rows of 2021/22 to 2025/26. Rows deleted from `por-clubs.tsv`: 1,302.
- Added 80 club rows. The third tier of 2002/03 (59 clubs) and 2003/04 (59 clubs) differs from the database (60). Pass 1 listed only the short zone. Pass 2 adds the other two zones: 2002/03 Norte and Sul, 2003/04 Centro and Sul. Source: the season pages `2002–03 Segunda Divisão B` and `2003–04 Segunda Divisão B`, read raw on 2026-10-17. Matched by name, by script, against Original 3.9.68 and October 2003. Two clubs have no record in October 2003: FC Pampilhosa and CD Santo António (2003/04).
- Corrected 2 matches: Chaves B (2018/19, 2019/20) is id 8237 "GD Chaves Satelite", Portuguese Second Division B North, in October 2018 and October 2019. Inference: the database names the B team by its satellite club name, and puts it in tier 3 in the same seasons.
- All other seasons with a database already list every club of each tier whose count differs: tier 1 2007/08 to 2013/14, tier 2 2007/08 to 2017/18, tier 3 2005/06 on. Tiers 1 and 2 in the other seasons and tier 3 in 2001/02 have the database count.
- `por-clubs.tsv` now has 1,543 club rows.

### Missing clubs after Pass 2

Clubs of tiers 1 to 3 with no record in that season's database: 74.

| Season | Division | Real clubs | No match | Clubs |
|---|---|---|---|---|
| 2003/04 | Segunda Divisão B | 59 | 2 | FC Pampilhosa, CD Santo António |
| 2005/06 | Segunda Divisão | 58 | 8 | União Torcatense, Fiães SC, Aliados Lordelo, GD Tourizense, FC Pampilhosa, SL Nelas, RSC Queluz, Vitória Setúbal B |
| 2007/08 | Segunda Divisão | 55 | 8 | Vila Meã, Fiães, Eléctrico, Pampilhosa, Nelas, Sátão, Carregado, Messinense |
| 2008/09 | Segunda Divisão | 47 | 8 | Aliados Lordelo, Santana, FC Pampilhosa, GDR Monsanto, Eléctrico FC, AD Carregado, Real SC, Mineiro Aljustrelense |
| 2009/10 | Segunda Divisão | 47 | 10 | Aliados FC Lordelo, Padroense FC, Vieira SC, FC Pampilhosa, Eléctrico FC, GDR Monsanto, Atlético de Reguengos, Aljustrelense, Igreja Nova, Santana |
| 2010/11 | Segunda Divisão | 48 | 8 | CF Andorinha, Padroense FC, SC Coimbrões, Aliados FC Lordelo, FC Pampilhosa, Eléctrico FC, Atlético SC Reguengos, Real SC |
| 2011/12 | Segunda Divisão | 48 | 2 | Marítimo B, Atlético SC |
| 2012/13 | Segunda Liga | 22 | 1 | Vitória de Guimarães B |
| 2013/14 | Campeonato Nacional de Seniores | 80 | 6 | Vitória de Guimarães B, Lusitano Vildemoinhos, Águias do Moradal, Carapinheirense, Almodôvar, Ferreiras |
| 2014/15 | Campeonato Nacional de Seniores | 80 | 1 | Atl. Reguengos |
| 2014/15 | Segunda Liga | 24 | 1 | Vitória de Guimarães B |
| 2015/16 | Campeonato de Portugal | 80 | 2 | Varzim B, Atlético de Reguengos |
| 2015/16 | LigaPro | 24 | 1 | Vitória de Guimarães B |
| 2016/17 | LigaPro | 22 | 1 | Vitória de Guimarães B |
| 2018/19 | Campeonato de Portugal | 72 | 4 | Mirandês, Mação, Santa Iria, Vasco da Gama Vidigueira |
| 2020/21 | Campeonato de Portugal | 96 | 10 | Águia Vimioso, Vidago, Brito, Rio Ave B, Vila Cortez, GRAP, Belenenses SAD B, Estrela da Amadora, Oriental Dragon, Sporting CP B |
| 2021/22 | Liga 3 | 24 | 1 | União de Santarém |

Max decides the 63 clubs that are not reserve or B teams. No club record was added.

### Reserve and B teams (Q289)

The build adds these as new club records. 11 entries, 7 teams.

| Season | Division | Team | Note |
|---|---|---|---|
| 2005/06 | Segunda Divisão Série D | Vitória Setúbal B | |
| 2011/12 | Segunda Divisão Zona Norte | Marítimo B | |
| 2012/13 | Segunda Liga | Vitória de Guimarães B | |
| 2013/14 | Campeonato Nacional de Seniores Série B | Vitória de Guimarães B | |
| 2014/15 | Segunda Liga | Vitória de Guimarães B | |
| 2015/16 | LigaPro | Vitória de Guimarães B | |
| 2015/16 | Campeonato de Portugal Série B | Varzim B | |
| 2016/17 | LigaPro | Vitória de Guimarães B | |
| 2020/21 | Campeonato de Portugal Série B | Rio Ave B | November 2020 has 11162 "Rio Ave FC U23" in its Reserve division |
| 2020/21 | Campeonato de Portugal Série G | Belenenses SAD B | November 2020 has 11158 "Belenenses SAD U23" in its Reserve division |
| 2020/21 | Campeonato de Portugal Série G | Sporting CP B | November 2020 has 6294 "Sporting CP U23" in its Reserve division |

The U23 records play no league in the database. Pass 1 matched no B team to a U23 record, and Pass 2 keeps that rule.

## Second source, pass 2

Date 2026-10-17. Second-source agent. Input: the 155 check lines with verdict `no second source` whose row still exists and whose field holds a value. The 25 lines of the 2027/28 rows (por-235 to por-243) are inferences, so this pass does not check them. The other 130 lines are in `por.check2.tsv`.

Second sources: FPF rule texts and zerozero.pt tables, read today as pages archived by arquivo.pt, and two Lusa and Ogolo news reports of 2009 and 2010. The web search budget of the session ran out during the pass. Lines that no page read today could cover stay `no second source`.

| Verdict | Lines |
|---|---|
| agree | 35 |
| disagree, settled | 1 |
| disagree, still split (conflict) | 3 |
| no second source | 91 |

### Changes to `por.tsv`

- por-66 `up` (2008/09 Segunda Divisão): 4 changed to 2. FPF notice 239, the rules for 2008/09, sends up only the two finalists of the third phase. The four promotion-group winners play two-legged ties (A v B, C v D). Penafiel and Carregado went up as well in July 2009, because the league invited them to fill the places of Boavista, Gondomar and Vizela, which were demoted. Decision `second`. Sources: https://arquivo.pt/wayback/20090927082038/https://fpf.pt/portal/page/portal/portal_futebol/comunicados2007/comunicadosoficiais/co239_quadro_competitivo_ii_divisao_cn_fut_11.pdf and https://arquivo.pt/wayback/20091223193953/https://ogolo.clix.pt/articles/ligavitaliscarregadoepenafieloficialmenteconvidados
- por-66 `playoffs`: the third phase is added, because the new `up` value depends on it. Source: FPF notice 239.
- por-66, por-75 and por-93: a note on `down` (see the conflicts below).

### Conflicts (still split)

- por-66 `down` (2008/09): the first source gives 15 (4, 4, 3, 4). FPF notice 239 gives the 4th, 5th and 6th of each relegation series, 12 in all, with more 3rd-placed clubs if Azores clubs go down directly. Série C had only 11 clubs, so its relegation series had 5. The value 15 stays. Max decides.
- por-75 `down` (2009/10): the first source gives 13 (4, 5, 4). FPF notice 421, the rules from 2009/10, gives the 4 last clubs of each zone and, from the end of 2009/10, the 2 worst 12th-placed clubs, 14 in all. Valdevez was in the Norte zone in December 2009 (zerozero) but is not in the final table of the first source. Inference: the club left during the season, and this explains the difference. The value 13 stays. Max decides. Source: https://arquivo.pt/wayback/20090926113934/https://oriental.pt/conteudos/uploads/files/oriental/regulamentoprovasoficiaisfut11cniidiviso.pdf
- por-93 `down` (2011/12): the first source marks 4 per zone (12). FPF notice 421 gives 4 per zone and the 2 worst 12th-placed clubs. The zerozero legend for 2011/12 shows a relegation place for the worst 12th of the three zones. The value 12 stays. Max decides. Source: https://arquivo.pt/wayback/20120123103356/http://www.zerozero.pt/edicao.php?id_edicao=24169

### Other findings

- por-75 `playoffs`: the Lusa report and FPF notice 421 confirm a double round robin of the three zone winners. No page read today states the weighted starting points of the first source. The final zerozero table gives 6, 5 and 5 points, which equal the results. The verdict is `agree` for the mini-league. The weighting has no second source.
- Tier 3 club counts: zerozero confirms 4 x 14 in 2006/07, 12, 12, 11 and 12 in 2008/09, 3 x 16 in 2010/11, 2011/12 and 2012/13, and 8 x 10 in 2014/15. For 2013/14 and 2015/16, zerozero gave only part of the series (each with 10 clubs). These counts stay `no second source`.
- Lines that stay `no second source`: the Taça de Portugal entrant counts, the Taça da Liga entrant counts, the Taça de Portugal substitutes, the 2026/27 rows, the nation rules rows, the tier 3 substitutes from 2001/02 to 2003/04 and from 2006/07 on, and the tier 3 rows of 2013/14 to 2018/19 other than those named above. The FPF rules of 2005 (art. 104.06, three substitutes in official competitions) cover only 2004/05 and 2005/06.

`rules_lint.py`: 0 problems for `por.tsv` before and after.

## Closing

Date: 2026-10-17. Closing agent. `rules_lint.py`: 0 problems before and after. Q272: `down` holds the places by the rule text of the season, and the outcome is in `notes`.

| Row | Old `down` | New `down` | Rule text |
|---|---|---|---|
| por-66 (2008/09) | 15 (4, 4, 3, 4) | 12 (3 per relegation series) | FPF notice 239, rule 2: the 4th, 5th and 6th of each of the 4 relegation series go down. More 3rd-placed clubs go down if Azores clubs go down directly. |
| por-75 (2009/10) | 13 (4, 5, 4) | 14 | FPF notice 421, rule 2: the last 4 of each zone (12), and from the end of 2009/10 also the 2 worst 12th-placed clubs. |
| por-93 (2011/12) | 12 (4 per zone) | 14 | FPF notice 421, rule 2, as por-75. The rectified notice sets the zones of 2011/12, so it is in force that season. |

URLs, read on 2026-10-17:
- https://arquivo.pt/wayback/20090927082038/https://fpf.pt/portal/page/portal/portal_futebol/comunicados2007/comunicadosoficiais/co239_quadro_competitivo_ii_divisao_cn_fut_11.pdf
- https://arquivo.pt/wayback/20090926113934/https://oriental.pt/conteudos/uploads/files/oriental/regulamentoprovasoficiaisfut11cniidiviso.pdf

The three conflicts of the second-source pass on these rows are settled.

## Second source, retry

Date: 2026-10-17. Retry agent. Scope: the 91 lines of `por.check2.tsv` with the verdict `no second source`. The agent read archived pages first (arquivo.pt, archive.org, the AF Algarve file server that holds FPF notices, RSSSF) and used searches only where needed. Each changed line has a note that starts with "retry:".

| Result | Lines |
|---|---|
| agree | 53 |
| disagree, settled `first` | 1 |
| disagree, settled `second` | 1 |
| disagree, still split | 0 |
| still `no second source` | 36 |

File totals after the retry: 88 agree, 6 disagree, 36 no second source.

### Disagreements settled

- por-159 `subs_used` (Taça de Portugal 2018/19): the FPF cup rules of 2018/19 (start-of-season text, art. 44) give 3. The Record live report of V. Setúbal v Braga (18 December 2018, extra time) lists four V. Setúbal substitutions. Decision `first`: the value "3, plus a 4th in extra time" stays. Inference: the 4th substitute in extra time came in during the season. The source URL is added to the row.
- por-231 `format` (Taça de Portugal 2026/27): Record (3 August 2026) and RTP (January 2026) state single-match semi-finals from 2026/27. Decision `second`. The row now reads "single-match semi-finals (new in 2026/27)". Record also states that I Liga clubs enter in round 3 and clubs in European competitions in round 4. The row holds this text. Open point: the round count "8 rounds" has no second source for 2026/27.

### Other findings

- por-129 `down` (2015/16): the FPF rules as changed by notice 438 give 22, as the first source does. A copy on sjpf.pt (uploaded November 2015) holds the text before notice 438 and gives 20. The verdict is `agree`.
- por-186 `clubs` (2021/22): FPF notice CO-00070 (4 August 2021) gives 155. Notice CO-00074 (5 August 2021) gives 154 and replaces it. The verdict is `agree`.
- por-48 `down` (2006/07): FPF notice 43 sends down every club from 12th place in each series. It gives 15 for a planned 3 x 14 + 1 x 16. The season had 4 x 14, so the rule gives 12 (inference). The verdict is `agree`.
- por-105 `clubs` (2012/13): the first source page now gives 165 teams in its infobox, and the row holds 162. No second source was found. Max decides which value to keep.
- The FPF rules of 2005 (art. 104.06, three substitutes) were still on the FPF site in July 2007. They cover 2006/07 and 2007/08 as well.
- por-147 `playoffs` (2017/18, not in the retry scope): the FPF rules give the 5 series winners and the 3 best runners-up. The row text reads "top 5 of each series and 3 best runners-up". Max can have the wording fixed.

### Still without a second source

- Taça de Portugal entrant counts: 2002/03 to 2012/13 and 2015/16 to 2020/21 (17 lines). RSSSF and zerozero list the rounds but give no total. A count of the RSSSF lists for 2002/03 gives 224, which is an inference and not a stated total. Pages that give only the round 1 numbers are in the notes.
- Taça da Liga entrant counts: 2010/11 to 2015/16 and 2021/22. A club page gives the entrants per stage for 2010/11 and 2011/12 (16 + 6 + 2 + 8 = 32), but no total.
- 2026/27: Liga Portugal 2 (all five fields) and Liga 3 `up`, `down` and `playoffs`.
- Nation rules: por-63 `foreign` and por-234 `foreign`, `work_permit` and `registration`.

`rules_lint.py`: 0 problems for `por.tsv` before and after.

## Fix

Date: 2026-10-17. Fix agent. Two open points of the retry, both written into `por.tsv`.

- por-105 `clubs` (Taça de Portugal 2012/13): 162 stays. No FPF rule text or draw notice with a total was found (the archived FPF notice link returns a web page). The full round lists give 162. Round 1 has 50 matches and 30 byes, so 130 Segunda and Terceira Divisão clubs. Round 2 has 48 matches: 50 winners, 30 byes and 16 Segunda Liga clubs. Round 3 has 32 matches: 48 winners and 16 Primeira Liga clubs. The infobox value 165 has no support. Source: https://en.wikipedia.org/wiki/2012%E2%80%9313_Ta%C3%A7a_de_Portugal#First_round
- por-147 `playoffs` (Campeonato de Portugal 2017/18): the FPF rules of 2017/18, art. 85.2 to 85.7, set 80 clubs in 5 series of 16. The 5 series winners and the 3 best runners-up play two-legged quarter-finals and semi-finals. The 2 semi-final winners go up and meet in a final at a neutral ground. The last 6 of each series go down. The row now says "5 series winners and the 3 best runners-up". `up` (2) and `down` (30) agree with the rules and stay. The `format` text now names the play-off rounds. Source: https://arquivo.pt/wayback/20170807062340/https://fpf.pt/downloaddocument.ashx?id=9122

## Pass 3 (Q337)

Q337 (2026-10-17): a real play-off against a tier that is not in the game is played against a club with no division, as for the Scottish pyramid play-off (Q284). The rows below keep the real format in `playoffs`. Each row gets this note: "Q337: the lower-tier side is a club with no division in the game." No other field changed.

Rows: 30 (Açores play-off against the Açores champion of the Terceira Divisão), and 3, 12, 21 (the Açores rule in `playoffs` gives the same play-off when the cap applies).
