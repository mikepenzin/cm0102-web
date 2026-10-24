# Summer 2002: the second-look rows and the held staff rows

Spec: `docs/specs/2026-10-24-summer-2002-db-spec.md` (Q24, Q22). Work of 2026-10-24 on branch `s2002-accuracy`
(from `s2002-build`). The per-row research tables are outside the repo, in the shared notes folder of the session
(`s2002-holds/out-holds.tsv`), because they quote the Xbox and 3.9.68 values. Every
source below is public (Wikipedia, RSSSF, club and FA pages). No other database maker's work is used (Q351).

## 1. League rows (Q33 to Q38)

My first pass at the league rows (club-table edits) is replaced. The coordinating session's check
(`xbox-0203-work/league-check/all-results.tsv`, two public sources per row) covers all 494 rows, and Max answered
Q33 to Q38. `change_list.py --league-check` applies them. The club tables (`rules-0.6.0/*-clubs.tsv`) are not
edited. Their id errors went to the 0.6.0 session.

| Kind 5 | Applied | Kept | Second look | Rows |
|---|---|---|---|---|
| Before | 66 | 52 | 376 | 494 |
| After the check (Q34 to Q37) | 285 | 214 | 0 | 499 |
| After Q39 (swaps only) | 117 | 225 | 157 | 499 |

- All 216 "change" rows end in their checked division (a script compared each one). 214 apply, and 2 put back
  the 3.9.68 division of a row that the Xbox data had moved, so they show as kept.
- Q37 adds 5 rows for clubs that had no kind 5 row (JCO Le Creusot, Stade Lannionnais, Oulun Tervarit and
  others). "No league" is club.dat +0x57 = -2, the value of 3.9.68.
- The 81 club-table rows with no club are kept with rule Q35.

### Division sizes (Q39)

Max's answer Q39: every division keeps its 3.9.68 club count in the data, so the original's create_table always
finds the clubs it takes (`season_clubs.c` CLASS_SIZE, 0x56ed95..0x56edab). `change_list.balance` applies a
checked move only with a partner: a club that moves the other way between the same two divisions, else a chain of
three or more moves that ends where it starts. "No division" counts as one more place in a chain. Finland keeps
its North and South groups: the checked "Finnish First Division" is the club's 3.9.68 group, and Oulun
Luistinseura and Vaasan Palloseura go to the North group. Competition 118 stays empty, as in 3.9.68. The season
code makes the real sizes on the 2002/03 tile (slice 4).

Which move is held when there are more moves than partners (`move_key`, the coordinating session's rule):
1. A club that was not in the division it leaves in 2001/02 (the 3.9.68 row is out of date) moves first.
2. Going down, the club with the worst 2001/02 place moves first, so the relegated club with the best place stays
   up. Going up, the champion moves first, then the automatic places by place, then the play-off winners, so the
   lowest promotion place stays down. Places: `xbox-0203-work/league-check/places-2001-02.tsv` (RSSSF and
   Wikipedia; 66 of 76 contested clubs have a place). Calendar-year leagues use the 2002 table. For Ireland the
   check uses the 2003 season, so Dundalk, Galway United and Monaghan United, relegated in 2001/02, count as
   out-of-date rows, and Bray Wanderers (relegated in 2002/03) stays up.
3. With no known place, the 3.9.68 reputation decides, and the row says so.
4. A move within one level (a group change) looks for its chain first, before any swap, and is not held for
   balance. Two such moves are held only because no chain returns to their division: Biokhimik-Mordovia (Second
   Division Volga to Central) and Eintracht Frankfurt Amateure (Regional North to South). The rank (+0x69) alone
   is not the level (the A-League and the D3-Pro League share rank 6), so two divisions are one level only with
   the same rank and the same name but its last word.

Result: 117 moves apply, 110 of them in 55 swaps of two and 7 in longer chains. Every division
keeps its 3.9.68 count. `test_summer_2002.py` checks this, and that each CLASS_SIZE division holds at least its
2001 count. Run on the build of the first pass (before Q39), the check finds 33 faults, among them the Finnish
groups (3 of 10) and the Croatian First Division (13 of 16).

### The 157 moves with no partner

Each club keeps its 3.9.68 division. The checked division is in `5-leagues.tsv` (`division_new`). Most of these
moves have no club at all that moves the other way, so no order could apply them (for example the 35 Brazilian
clubs that join Série C from no division, or the Croatian clubs that drop to county leagues).

| Nation | Club | Id | 3.9.68 division | Checked division | Why this club is held |
|---|---|---|---|---|---|
| France (no nation in 3.9.68) | Choisy-le-Roy | 2516 | none | French Lower Division | no move of this kind applies: nothing moves the other way |
| France (no nation in 3.9.68) | JCO Le Creusot | 5508 | none | French Lower Division | no move of this kind applies: nothing moves the other way |
| France (no nation in 3.9.68) | Luçon | 6793 | none | French Lower Division | no move of this kind applies: nothing moves the other way |
| France (no nation in 3.9.68) | Stade Lannionnais | 6369 | none | French Lower Division | no move of this kind applies: nothing moves the other way |
| France (no nation in 3.9.68) | US Corte | 2755 | none | French Lower Division | no move of this kind applies: nothing moves the other way |
| Argentina | Central Córdoba de Rosario | 2841 | Argentine Second Division | Argentine Third Division | 20 of 24, relegated directly (Primera B Nacional 2001/02 general relegation (promedios) table, https://www.rsssf.org/tablesa/arg2-02.html) |
| Argentina | Independiente Rivadavia de Mendoza | 5347 | Argentine Second Division | Argentine Third Division | 22 of 24, relegated directly (Primera B Nacional 2001/02 general relegation (promedios) table, https://www.rsssf.org/tablesa/arg2-02.html) |
| Argentina | Platense | 8840 | Argentine Second Division | Argentine Third Division | 19 of 24, relegated directly (Primera B Nacional 2001/02 general relegation (promedios) table, https://www.rsssf.org/tablesa/arg2-02.html) |
| Argentina | Racing de Córdoba | 9267 | Argentine Second Division | Argentine Third Division | 18 of 24, relegated directly (Primera B Nacional 2001/02 general relegation (promedios) table, https://www.rsssf.org/tablesa/arg2-02.html) |
| Argentina | Villa Mitre de Bahía Blanca | 12489 | Argentine Second Division | Argentine Third Division | 21 of 24, relegated directly (Primera B Nacional 2001/02 general relegation (promedios) table, https://www.rsssf.org/tablesa/arg2-02.html) |
| Argentina | Belgrano de Mendoza | 1463 | Argentine Third Division | A Lower Division | no 2001/02 place known: ordered by 3.9.68 reputation |
| Argentina | Chaco For Ever | 4107 | Argentine Third Division | A Lower Division | no 2001/02 place known: ordered by 3.9.68 reputation |
| Argentina | Chacras de Coria | 2391 | Argentine Third Division | A Lower Division | 4 of 9, stayed but reorganised (Torneo Argentino A Interior 2001/02 Group A, https://www.rsssf.org/tablesa/arg3-int02.html) |
| Argentina | Cultural Argentino de General Pico | 2853 | Argentine Third Division | A Lower Division | no 2001/02 place known: ordered by 3.9.68 reputation |
| Argentina | Deportivo Merlo | 7273 | Argentine Third Division | A Lower Division | 21 of 22, relegated directly (Primera B Metropolitana 2001/02 final table, https://www.rsssf.org/tablesa/arg3-02.html) |
| Argentina | General Belgrano de La Pampa | 4340 | Argentine Third Division | A Lower Division | no 2001/02 place known: ordered by 3.9.68 reputation |
| Argentina | Huracán de Mendoza | 5214 | Argentine Third Division | A Lower Division | no 2001/02 place known: ordered by 3.9.68 reputation |
| Argentina | Ituzaingó | 5481 | Argentine Third Division | A Lower Division | 22 of 22, relegated directly (Primera B Metropolitana 2001/02 final table, https://www.rsssf.org/tablesa/arg3-02.html) |
| Argentina | Liniers de Bahía Blanca | 6609 | Argentine Third Division | A Lower Division | no 2001/02 place known: ordered by 3.9.68 reputation |
| Argentina | Patronato de Paraná | 8628 | Argentine Third Division | A Lower Division | 9 of 10, relegated directly (Torneo Argentino A Interior 2001/02 Group B, https://www.rsssf.org/tablesa/arg3-int02.html) |
| Argentina | San Martín de Tucumán | 10068 | Argentine Third Division | A Lower Division | 8 of 10, relegated via play-off (Torneo Argentino A Interior 2001/02 Group B, https://www.rsssf.org/tablesa/arg3-int02.html) |
| Argentina | Sportivo Barraca de Paso de los Libres | 10944 | Argentine Third Division | A Lower Division | no 2001/02 place known: ordered by 3.9.68 reputation |
| Brazil | Associação Portuguesa de Desportos | 9002 | Brazilian National First Division | Brazilian National Second Division | 23 of 26, relegated directly (Serie A 2002 first phase table (decides relegation), https://www.rsssf.org/tablesb/braz02.html) |
| Brazil | Botafogo Futebol Clube (SP) | 1788 | Brazilian National First Division | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Sociedade Esportiva Palmeiras | 8542 | Brazilian National First Division | Brazilian National Second Division | 24 of 26, relegated directly (Serie A 2002 first phase table (decides relegation), https://www.rsssf.org/tablesb/braz02.html) |
| Brazil | Sociedade Esportiva do Gama | 4289 | Brazilian National First Division | Brazilian National Second Division | 25 of 26, relegated directly (Serie A 2002 first phase table (decides relegation), https://www.rsssf.org/tablesb/braz02.html) |
| Brazil | Americano Futebol Clube (RJ) | 679 | Brazilian National Second Division | Brazilian National Third Division | 21 of 26, relegated directly (Serie B 2002 first phase table, https://www.rsssf.org/tablesb/braz2-02.html) |
| Brazil | Clube Atlético Bragantino | 1818 | Brazilian National Second Division | Brazilian National Third Division | 26 of 26, relegated directly (Serie B 2002 first phase table, https://www.rsssf.org/tablesb/braz2-02.html) |
| Brazil | Desportiva Capixaba | 3049 | Brazilian National Second Division | Brazilian National Third Division | no 2001/02 place known: ordered by 3.9.68 reputation |
| Brazil | Esporte Clube XV de Novembro | 12940 | Brazilian National Second Division | Brazilian National Third Division | 25 of 26, relegated directly (Serie B 2002 first phase table, https://www.rsssf.org/tablesb/braz2-02.html) |
| Brazil | Sampaio Corrêa Futebol Clube | 10024 | Brazilian National Second Division | Brazilian National Third Division | 23 of 26, relegated directly (Serie B 2002 first phase table, https://www.rsssf.org/tablesb/braz2-02.html) |
| Brazil | Serra Futebol Clube | 10357 | Brazilian National Second Division | Brazilian National Third Division | no 2001/02 place known: ordered by 3.9.68 reputation |
| Brazil | Tuna Luso Brasileira | 11861 | Brazilian National Second Division | Brazilian National Third Division | not in the division it leaves in the season before (the 3.9.68 row is out of date): 1 of 4, stayed but reorganised (Serie C 2002 first phase Group 2, https://www.rsssf.org/tablesb/braz3-02.html) |
| Brazil | Anápolis Futebol Clube | 706 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Associação Atlética Francana | 4156 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Associação Atlética Portuguesa (RJ) | 9003 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Associação Desportiva Cabofriense | 2109 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Associação Esportiva Catuense | 2301 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Associação Esportiva Caxiense | 2312 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Associação Olímpica de Itabaiana | 5466 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Atlético Clube Lagartense | 6319 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Atlético Roraima Clube | 9658 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Campinense Clube | 2173 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Caxias Futebol Clube | 2311 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Central Sport Club | 2337 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Centro Esportivo Nova Esperança | 8052 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Comercial Esporte Clube | 2676 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Esporte Clube São José | 10179 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Estrela do Norte Futebol Clube | 3680 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Ferroviário Atlético Clube | 3999 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Goiatuba Esporte Clube | 4466 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Goytacaz Futebol Clube | 4520 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Grêmio Esportivo Brasil | 1841 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Grêmio de Esportes Maringá | 4589 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Guarany Sporting Clube | 4660 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Iraty Sport Club | 5427 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Itapipoca Esporte Clube | 5474 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Ituiutaba Esporte Clube | 5479 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Juazeiro Social Clube | 5653 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Macaé Sports | 6935 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Maranhão Atlético Clube | 7072 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Marília Atlético Clube | 7100 | none | Brazilian National Second Division | no move of this kind applies: nothing moves the other way |
| Brazil | Palmas Futebol e Regatas | 8541 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Rio Branco Atlético Clube | 9568 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Sociedade Imperatriz de Desportos | 5333 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Souza Esporte Clube | 10821 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | São Gonçalo Futebol Clube | 10177 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Tupi Football Club | 11870 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Brazil | Uberaba Sport Club | 11987 | none | Brazilian National Third Division | no move of this kind applies: nothing moves the other way |
| Croatia | NK Cakovec | 2128 | Croatian First Division | Croatian Second Division North | no move of this kind applies: nothing moves the other way |
| Croatia | NK Hrvatski Dragovoljac | 5172 | Croatian First Division | Croatian Second Division South | no move of this kind applies: nothing moves the other way |
| Croatia | NK Marsonia | 7138 | Croatian First Division | Croatian Second Division North | no move of this kind applies: nothing moves the other way |
| Croatia | NK TSK | 11312 | Croatian First Division | Croatian Second Division South | no move of this kind applies: nothing moves the other way |
| Croatia | Ivancica | 5484 | Croatian Second Division North | Croatian Third Division North | no move of this kind applies: nothing moves the other way |
| Croatia | NK Bedem | 1431 | Croatian Second Division North | Croatian Third Division East | 13 of 16, relegated directly (2. HNL North 2001/02 final table, https://www.rsssf.org/tablesk/kroa02.html) |
| Croatia | NK Bjelovar | 1625 | Croatian Second Division North | Croatian Third Division North | no move of this kind applies: nothing moves the other way |
| Croatia | NK Mladost Molve | 7435 | Croatian Second Division North | Croatian Third Division North | no move of this kind applies: nothing moves the other way |
| Croatia | NK Papuk | 8582 | Croatian Second Division North | Croatian Third Division North | no move of this kind applies: nothing moves the other way |
| Croatia | NK Sloga (C) | 10671 | Croatian Second Division North | Croatian Third Division North | no move of this kind applies: nothing moves the other way |
| Croatia | HNK Segesta | 10300 | Croatian Second Division South | Croatian Third Division Central | no move of this kind applies: nothing moves the other way |
| Croatia | NK Mosor | 7581 | Croatian Second Division South | Croatian Third Division South | no move of this kind applies: nothing moves the other way |
| Croatia | NK Samobor | 10020 | Croatian Second Division South | Croatian Third Division Central | no move of this kind applies: nothing moves the other way |
| Croatia | NK Trnje | 11809 | Croatian Second Division South | Croatian Third Division Central | no move of this kind applies: nothing moves the other way |
| Croatia | NK Vrbovec | 12615 | Croatian Second Division South | Croatian Third Division Central | no move of this kind applies: nothing moves the other way |
| Croatia | NK Zminj | 13160 | Croatian Second Division South | Croatian Third Division West | no move of this kind applies: nothing moves the other way |
| Croatia | NK Metalac (S) | 7289 | Croatian Third Division Central | Croatian Lower Division | no move of this kind applies: nothing moves the other way |
| Croatia | NK Moslavac | 7579 | Croatian Third Division Central | Croatian Lower Division | no move of this kind applies: nothing moves the other way |
| Croatia | NK Moslavina | 7580 | Croatian Third Division Central | Croatian Lower Division | no move of this kind applies: nothing moves the other way |
| Croatia | NK Ostrc | 8395 | Croatian Third Division Central | Croatian Lower Division | no move of this kind applies: nothing moves the other way |
| Croatia | NK Radnik | 9300 | Croatian Third Division Central | Croatian Lower Division | no move of this kind applies: nothing moves the other way |
| Croatia | NK Spansko | 10839 | Croatian Third Division Central | Croatian Lower Division | no move of this kind applies: nothing moves the other way |
| Croatia | NK Lokomotiva Cestorad | 6716 | Croatian Third Division East | Croatian Lower Division | no move of this kind applies: nothing moves the other way |
| Croatia | NK Tomislav | 11649 | Croatian Third Division East | Croatian Lower Division | no move of this kind applies: nothing moves the other way |
| Croatia | NK Zrinski | 13174 | Croatian Third Division East | Croatian Lower Division | no move of this kind applies: nothing moves the other way |
| Croatia | Buducnost Podbrest | 1976 | Croatian Third Division North | Croatian Lower Division | 11 of 16, relegated directly (3. HNL North 2001/02 final table, https://www.rsssf.org/tablesk/kroa02.html) |
| Croatia | Drava | 3267 | Croatian Third Division North | Croatian Lower Division | 12 of 16, relegated directly (3. HNL North 2001/02 final table, https://www.rsssf.org/tablesk/kroa02.html) |
| Croatia | Kucan '99 | 6197 | Croatian Third Division North | Croatian Lower Division | 5 of 16, stayed but reorganised (3. HNL North 2001/02 final table, https://www.rsssf.org/tablesk/kroa02.html) |
| Croatia | NK Garic | 4309 | Croatian Third Division North | Croatian Lower Division | 15 of 16, relegated directly (3. HNL North 2001/02 final table, https://www.rsssf.org/tablesk/kroa02.html) |
| Croatia | NK Omladinac (C) | 8275 | Croatian Third Division North | Croatian Lower Division | 13 of 16, relegated directly (3. HNL North 2001/02 final table, https://www.rsssf.org/tablesk/kroa02.html) |
| Croatia | NK Pitomaca | 8832 | Croatian Third Division North | Croatian Lower Division | 10 of 16, relegated directly (3. HNL North 2001/02 final table, https://www.rsssf.org/tablesk/kroa02.html) |
| Croatia | NK Trnje (T) | 11810 | Croatian Third Division North | Croatian Lower Division | 9 of 16, relegated via play-off (3. HNL North 2001/02 final table, https://www.rsssf.org/tablesk/kroa02.html) |
| Croatia | DOSK | 2889 | Croatian Third Division South | Croatian Lower Division | no move of this kind applies: nothing moves the other way |
| Croatia | NK Orkan | 8339 | Croatian Third Division South | Croatian Lower Division | no move of this kind applies: nothing moves the other way |
| Croatia | NK Primorac | 9066 | Croatian Third Division South | Croatian Lower Division | no move of this kind applies: nothing moves the other way |
| Croatia | NK Mladost Fazana | 7432 | Croatian Third Division West | Croatian Lower Division | no move of this kind applies: nothing moves the other way |
| Croatia | NK Otocac | 8403 | Croatian Third Division West | Croatian Lower Division | no move of this kind applies: nothing moves the other way |
| Finland | Oulun Tervarit | 11488 | Finnish First Division North | Finnish Second Division North | 6 of 9, stayed but reorganised (Division One 2002 Northern Zone (final places 3-9; top 2 went to the promotion group), https://www.rsssf.org/tablesf/fin02.html) |
| Finland | TPV Tampere | 11307 | Finnish First Division North | Finnish Second Division West | no move of this kind applies: nothing moves the other way |
| Finland | FC HIFK | 4728 | Finnish First Division South | Finnish Lower Division | no move of this kind applies: nothing moves the other way |
| Finland | FC Jokerit | 3801 | Finnish First Division South | Finnish Premier Division | no move of this kind applies: nothing moves the other way |
| Finland | FC KooTeePee | 6105 | Finnish First Division South | Finnish Premier Division | no move of this kind applies: nothing moves the other way |
| Finland | IF Gnistan | 4454 | Finnish First Division South | Finnish Second Division South | no move of this kind applies: nothing moves the other way |
| Finland | Salon Palloilijat | 9979 | Finnish First Division South | Finnish Second Division West | no move of this kind applies: nothing moves the other way |
| Finland | Terjärv US | 11315 | Finnish Lower Division | Finnish Second Division North | no move of this kind applies: nothing moves the other way |
| Finland | FC Mikkeli | 3827 | Finnish Second Division East | Finnish Lower Division | no move of this kind applies: nothing moves the other way |
| Finland | Mikkelin Pallo | 7346 | Finnish Second Division East | Finnish Lower Division | no move of this kind applies: nothing moves the other way |
| Finland | Hangö Idrottsklubb | 3790 | none | Finnish Lower Division | no move of this kind applies: nothing moves the other way |
| France | ES Viry-Châtillon | 12535 | French CFA | French National | 5 of 18, play-off winner (CFA 2001/02 final table, group A, https://www.rsssf.org/tablesf/fran02.html) |
| France | FC Trélissac Périgord | 11780 | French CFA | French National | 2 of 18, play-off winner (CFA 2001/02 final table, group C, https://www.rsssf.org/tablesf/fran02.html) |
| France | Olympique Grand Rouen | 9705 | French CFA | French National | 4 of 18, play-off winner (CFA 2001/02 final table, group D, https://www.rsssf.org/tablesf/fran02.html) |
| France | ASO Arménienne Valence | 12208 | French National | French Second Division | 3 of 20, automatic promotion (National 2001/02 final table, https://www.rsssf.org/tablesf/fran02.html) |
| France | Toulouse FC | 11732 | French National | French Second Division | 4 of 20, automatic promotion (National 2001/02 final table, https://www.rsssf.org/tablesf/fran02.html) |
| France | Le Havre Athletic Club | 6414 | French Second Division | French First Division | 4 of 20, automatic promotion (Division 2 2001/02 final table, https://www.rsssf.org/tablesf/fran02.html) |
| France | OGC Nice | 7933 | French Second Division | French First Division | 3 of 20, automatic promotion (Division 2 2001/02 final table, https://www.rsssf.org/tablesf/fran02.html) |
| Germany | 1.FC Köln U23 | 5929 | A Lower Division | German Regional Division North | no move of this kind applies: nothing moves the other way |
| Germany | Eintracht Frankfurt Amateure | 4163 | German Regional Division North | German Regional Division South | a move within one level, with no chain of moves that returns to its division |
| Northern Ireland | Ards | 813 | Northern Irish League First Division | Northern Irish League Premier Division | no move of this kind applies: nothing moves the other way |
| Northern Ireland | Institute | 5373 | Northern Irish League First Division | Northern Irish League Premier Division | no move of this kind applies: nothing moves the other way |
| Poland | Jagiellonia Bialystok | 5551 | Polish Second Division | Polish Lower Division | 15 of 20, relegated directly (Second level (II liga) 2001/02 final table, https://www.rsssf.org/tablesp/pol02.html) |
| Poland | Polonia Bytom | 8923 | Polish Second Division | Polish Lower Division | no 2001/02 place known: ordered by 3.9.68 reputation |
| Republic of Ireland | Bray Wanderers A.F.C. | 1855 | Irish Premier Division | Irish First Division | 10 of 10, relegated directly (Premier Division 2002/03 final table, https://www.rsssf.org/tablesi/ier03.html) |
| Republic of Ireland | Dundalk F.C. | 3329 | Irish Premier Division | Irish First Division | not in the division it leaves in the season before (the 3.9.68 row is out of date): 10 of 12, relegated directly (Premier Division 2001/02 final table, https://www.rsssf.org/tablesi/ier02.html) |
| Russia | Metallurg Lipetsk | 7301 | Russian Second Division - Central | Russian First Division | no move of this kind applies: nothing moves the other way |
| Russia | Mosgaz Moscow | 7574 | Russian Second Division - Central | Russian Lower Division | no move of this kind applies: nothing moves the other way |
| Russia | Metallurg-Zapsib Novokuznetsk | 7304 | Russian Second Division - East | Russian First Division | no move of this kind applies: nothing moves the other way |
| Russia | Terek Grozny | 11467 | Russian Second Division - South | Russian First Division | no move of this kind applies: nothing moves the other way |
| Russia | Uralmash Yekaterinburg | 12117 | Russian Second Division - Ural | Russian First Division | no move of this kind applies: nothing moves the other way |
| Russia | Biokhimik-Mordovia Saransk | 1601 | Russian Second Division - Volga | Russian Second Division - Central | a move within one level, with no chain of moves that returns to its division |
| Russia | Baltika Kaliningrad | 1277 | Russian Second Division - West | Russian First Division | no move of this kind applies: nothing moves the other way |
| South Korea | Sangmoo | 10106 | Korean Amateur League | Korean League | no move of this kind applies: nothing moves the other way |
| Turkey | Batman Petrolspor | 1148 | Turkish 2. Division Category A | Turkish 2. Division Category B G5 | no move of this kind applies: nothing moves the other way |
| Turkey | Siirt JETPAspor | 10515 | Turkish 2. Division Category A | Turkish 2. Division Category B G5 | no move of this kind applies: nothing moves the other way |
| Turkey | Ispartaspor | 5459 | Turkish 2. Division Category B G2 | Turkish Lower Division | 7 of 8, relegated directly (2. Lig B (3rd level) 2001/02 second stage, classification group 2, https://www.rsssf.org/tablest/tur02.html) |
| Turkey | Vanspor A.S. | 12254 | Turkish 2. Division Category B G5 | Turkish Lower Division | 7 of 8, relegated directly (2. Lig B (3rd level) 2001/02 second stage, classification group 5, https://www.rsssf.org/tablest/tur02.html) |
| Turkey | Muglaspor | 7627 | none | Turkish 2. Division Category B G2 | no move of this kind applies: nothing moves the other way |
| Turkey | Çubukspor | 2844 | none | Turkish 2. Division Category B G4 | no move of this kind applies: nothing moves the other way |
| United States | Connecticut Wolves | 2702 | American A-League | none | no move of this kind applies: nothing moves the other way |
| United States | Hershey Wildcats | 5013 | American A-League | none | no move of this kind applies: nothing moves the other way |
| United States | Long Island Rough Riders | 6728 | American A-League | American D3-Pro League | no move of this kind applies: nothing moves the other way |
| United States | San Diego Flash | 10036 | American A-League | none | no move of this kind applies: nothing moves the other way |
| United States | Boston Bulldogs | 1780 | American D3-Pro League | none | no move of this kind applies: nothing moves the other way |
| United States | Chico Rooks | 2490 | American D3-Pro League | none | no move of this kind applies: nothing moves the other way |
| United States | Rhode Island Stingrays | 9517 | American D3-Pro League | none | no move of this kind applies: nothing moves the other way |
| United States | Stanislaus County Cruisers | 11056 | American D3-Pro League | none | no move of this kind applies: nothing moves the other way |
| United States | Tucson Fireballs | 11851 | American D3-Pro League | none | no move of this kind applies: nothing moves the other way |
| United States | Miami Fusion FC | 7347 | American Major League | none | no move of this kind applies: nothing moves the other way |
| United States | Tampa Bay Mutiny | 11353 | American Major League | none | no move of this kind applies: nothing moves the other way |

## 2. Held staff rows

25 rows: 16 moves and 3 date rows held by a full club list, and 6 rows where the Xbox job is player and the 3.9.68
record has no player data (Q22). Sources: `s2002-holds/out-holds.tsv`.

| Person | Held because | What the source shows | Proposal |
|---|---|---|---|
| Bortolo Mutti, Reggina manager | Luigi De Canio holds the place | Mutti managed Reggina from summer 2002, sacked 7 November 2002. De Canio started on 8 November. | Apply. De Canio leaves the place (no club). |
| José María Acebal, Sporting Gijón manager | Antonio Maceda holds it | Acebal managed Sporting at the start of 2002/03, sacked 7 October 2002. Maceda followed. | Apply. Maceda leaves the place (no club). |
| József Csábi, Vasas manager | Manu Ferrera holds it | Csábi managed Vasas from 8 July 2002 (Nemzeti Sport). Ferrera worked in Belgium (Aalst 2001/02). | Apply. Ferrera leaves the place (no club). |
| Vítor Paneira, Serzedelo manager | Manuel Miranda holds it | Wikipedia lists Paneira at Serzedelo for 2002/03, no month. Nothing on Miranda. | Apply, Miranda to no club. Weak: Max decides. |
| Neil Thompson, Boston United | Q22, no player data | Player-manager from 8 July 2002. | Keep 3.9.68 (manager). |
| Nigel Gibbs, Watford | Q22 | Coach from 2002, after his last match in April 2002. | Keep 3.9.68 (coach). |
| Stewart Kerr, Wigan | Q22 | Injured goalkeeper, retired 27 September 2002. | Keep 3.9.68. |
| Keith Branagan, Ipswich | Q22 | Goalkeeper, retired October 2002. | Keep 3.9.68. |
| Néstor Gorosito, Nueva Chicago | Q22 | Retired 2001. Manager of Nueva Chicago from 9 October 2002. | Keep 3.9.68 (no club in July). |
| Steve Owen, Bangor City | Q22 | Assistant at Bangor around 2006. Nothing for 2002. | Keep 3.9.68. |
| Tony Daley, Aston Villa coach | Full coach list (a newcomer of equal ability took the place by id) | Retired July 2002, fitness coach at Forest Green, undated Villa academy work. | Keep 3.9.68. |
| Benjamín Galindo, Guadalajara assistant | Full assistant place | Sources disagree (2002 or 2003/04). | Keep 3.9.68. |
| Manuel Matias, Valonguense manager | Full manager place | Wikipedia: Valonguense 2001/02, Valenciano 2002 to 2004. The Xbox row looks out of date. | Keep 3.9.68. |
| Dinis Vital, Lusitano GC manager | Full manager place | Last Lusitano spell 2000 to 2002. | Keep 3.9.68. |
| Colin Gie, Derek Almond, Maxime Roche, Joël Le Hir, Rémi Ribault, Gilles Watelle, Bob Jan Hillen, Jarosław Pers, Mikael Martinsson, Virgílio Martins, Eliseu | Full lists | No public source found. | Keep 3.9.68. |

Max said yes to the four "apply" rows (Q40, 2026-10-24). `change_list.HOLDERS_Q40` releases the four holders
(kind 2 rows with no Xbox row, dates kept), and the four managers then take the free places.

## 3. Found on the way (not changed)

- `fra-clubs.tsv` 2003/04 and 2005/06 CFA rows name 8880 (CEP Poitiers). The club is Stade Poitevin (8879 in
  3.9.68). Those rows name another season's database, so check the id there.
- `rus-tur-clubs.tsv` 2003/04 Lisma-Mordovia row names 1601. Check it in that season's database.
- `aus-jap-kor-clubs.tsv` names 2904 "Daegu FC" from 2006/07. In 3.9.68, 2904 is Daegu University.
- `fra-clubs.tsv` names 6846 "Sainte-Geneviève Sports" from 2007/08. In 3.9.68, 6846 is Lusitanos de Saint-Maur.
- `summary.md` (change list) still says the Turkish groups need a decision. Q29 settled it.
- `build_2002.py` stopped on Hangö IK, the first applied club with no 3.9.68 division. The check now reads a
  missing division as "none", the change list's word.
