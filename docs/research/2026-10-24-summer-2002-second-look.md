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
checked move only with a partner: a club that moves the other way between the same two divisions (higher
reputation first going up, lower first going down), else a chain of three or more moves that ends where it starts.
"No division" counts as one more place in a chain. Finland keeps its North and South groups: the checked "Finnish
First Division" is the club's 3.9.68 group, and Oulun Luistinseura and Vaasan Palloseura go to the North group.
Competition 118 stays empty, as in 3.9.68. The season code makes the real sizes on the 2002/03 tile (slice 4).

Result: 117 moves apply, 110 of them in 55 swaps of two and 7 in longer chains. Every division keeps its 3.9.68
count. `test_summer_2002.py` checks this, and that each CLASS_SIZE division holds at least its 2001 count. Run
on the build of the first pass (before Q39), the check finds 33 faults, among them the Finnish groups (3 of 10) and
the Croatian First Division (13 of 16).

### The 157 moves with no partner

Each club keeps its 3.9.68 division. The checked division is in `5-leagues.tsv` (`division_new`).

| Nation | Club | Id | 3.9.68 division | Checked division |
|---|---|---|---|---|
| France (no nation in 3.9.68) | Choisy-le-Roy | 2516 | none | French Lower Division |
| France (no nation in 3.9.68) | JCO Le Creusot | 5508 | none | French Lower Division |
| France (no nation in 3.9.68) | Luçon | 6793 | none | French Lower Division |
| France (no nation in 3.9.68) | Stade Lannionnais | 6369 | none | French Lower Division |
| France (no nation in 3.9.68) | US Corte | 2755 | none | French Lower Division |
| Argentina | Atlético Tucumán | 976 | Argentine Second Division | Argentine Third Division |
| Argentina | Central Córdoba de Rosario | 2841 | Argentine Second Division | Argentine Third Division |
| Argentina | Platense | 8840 | Argentine Second Division | Argentine Third Division |
| Argentina | Tigre | 11564 | Argentine Second Division | Argentine Third Division |
| Argentina | Villa Mitre de Bahía Blanca | 12489 | Argentine Second Division | Argentine Third Division |
| Argentina | Chaco For Ever | 4107 | Argentine Third Division | A Lower Division |
| Argentina | Chacras de Coria | 2391 | Argentine Third Division | A Lower Division |
| Argentina | Cultural Argentino de General Pico | 2853 | Argentine Third Division | A Lower Division |
| Argentina | Deportivo Merlo | 7273 | Argentine Third Division | A Lower Division |
| Argentina | General Belgrano de La Pampa | 4340 | Argentine Third Division | A Lower Division |
| Argentina | Huracán de Corrientes | 5213 | Argentine Third Division | A Lower Division |
| Argentina | Huracán de Mendoza | 5214 | Argentine Third Division | A Lower Division |
| Argentina | Ituzaingó | 5481 | Argentine Third Division | A Lower Division |
| Argentina | Liniers de Bahía Blanca | 6609 | Argentine Third Division | A Lower Division |
| Argentina | Patronato de Paraná | 8628 | Argentine Third Division | A Lower Division |
| Argentina | San Martín de Tucumán | 10068 | Argentine Third Division | A Lower Division |
| Argentina | Sportivo Barraca de Paso de los Libres | 10944 | Argentine Third Division | A Lower Division |
| Brazil | Associação Portuguesa de Desportos | 9002 | Brazilian National First Division | Brazilian National Second Division |
| Brazil | Botafogo Futebol Clube (SP) | 1788 | Brazilian National First Division | Brazilian National Third Division |
| Brazil | Botafogo de Futebol e Regatas | 1783 | Brazilian National First Division | Brazilian National Second Division |
| Brazil | Sociedade Esportiva Palmeiras | 8542 | Brazilian National First Division | Brazilian National Second Division |
| Brazil | ABC Futebol Clube | 56 | Brazilian National Second Division | Brazilian National Third Division |
| Brazil | Club Sportivo Sergipe | 10354 | Brazilian National Second Division | Brazilian National Third Division |
| Brazil | Clube Atlético Bragantino | 1818 | Brazilian National Second Division | Brazilian National Third Division |
| Brazil | Desportiva Capixaba | 3049 | Brazilian National Second Division | Brazilian National Third Division |
| Brazil | Esporte Clube XV de Novembro | 12940 | Brazilian National Second Division | Brazilian National Third Division |
| Brazil | Sampaio Corrêa Futebol Clube | 10024 | Brazilian National Second Division | Brazilian National Third Division |
| Brazil | Tuna Luso Brasileira | 11861 | Brazilian National Second Division | Brazilian National Third Division |
| Brazil | Anápolis Futebol Clube | 706 | none | Brazilian National Third Division |
| Brazil | Associação Atlética Francana | 4156 | none | Brazilian National Third Division |
| Brazil | Associação Atlética Portuguesa (RJ) | 9003 | none | Brazilian National Third Division |
| Brazil | Associação Desportiva Cabofriense | 2109 | none | Brazilian National Third Division |
| Brazil | Associação Esportiva Catuense | 2301 | none | Brazilian National Third Division |
| Brazil | Associação Esportiva Caxiense | 2312 | none | Brazilian National Third Division |
| Brazil | Associação Olímpica de Itabaiana | 5466 | none | Brazilian National Third Division |
| Brazil | Atlético Clube Lagartense | 6319 | none | Brazilian National Third Division |
| Brazil | Atlético Roraima Clube | 9658 | none | Brazilian National Third Division |
| Brazil | Campinense Clube | 2173 | none | Brazilian National Third Division |
| Brazil | Caxias Futebol Clube | 2311 | none | Brazilian National Third Division |
| Brazil | Central Sport Club | 2337 | none | Brazilian National Third Division |
| Brazil | Centro Esportivo Nova Esperança | 8052 | none | Brazilian National Third Division |
| Brazil | Comercial Esporte Clube | 2676 | none | Brazilian National Third Division |
| Brazil | Esporte Clube São José | 10179 | none | Brazilian National Third Division |
| Brazil | Estrela do Norte Futebol Clube | 3680 | none | Brazilian National Third Division |
| Brazil | Ferroviário Atlético Clube | 3999 | none | Brazilian National Third Division |
| Brazil | Goiatuba Esporte Clube | 4466 | none | Brazilian National Third Division |
| Brazil | Goytacaz Futebol Clube | 4520 | none | Brazilian National Third Division |
| Brazil | Grêmio Esportivo Brasil | 1841 | none | Brazilian National Third Division |
| Brazil | Grêmio de Esportes Maringá | 4589 | none | Brazilian National Third Division |
| Brazil | Guarany Sporting Clube | 4660 | none | Brazilian National Third Division |
| Brazil | Iraty Sport Club | 5427 | none | Brazilian National Third Division |
| Brazil | Itapipoca Esporte Clube | 5474 | none | Brazilian National Third Division |
| Brazil | Ituiutaba Esporte Clube | 5479 | none | Brazilian National Third Division |
| Brazil | Juazeiro Social Clube | 5653 | none | Brazilian National Third Division |
| Brazil | Macaé Sports | 6935 | none | Brazilian National Third Division |
| Brazil | Maranhão Atlético Clube | 7072 | none | Brazilian National Third Division |
| Brazil | Marília Atlético Clube | 7100 | none | Brazilian National Second Division |
| Brazil | Palmas Futebol e Regatas | 8541 | none | Brazilian National Third Division |
| Brazil | Rio Branco Atlético Clube | 9568 | none | Brazilian National Third Division |
| Brazil | Sociedade Imperatriz de Desportos | 5333 | none | Brazilian National Third Division |
| Brazil | Souza Esporte Clube | 10821 | none | Brazilian National Third Division |
| Brazil | São Gonçalo Futebol Clube | 10177 | none | Brazilian National Third Division |
| Brazil | Tupi Football Club | 11870 | none | Brazilian National Third Division |
| Brazil | Uberaba Sport Club | 11987 | none | Brazilian National Third Division |
| Croatia | NK Cakovec | 2128 | Croatian First Division | Croatian Second Division North |
| Croatia | NK Hrvatski Dragovoljac | 5172 | Croatian First Division | Croatian Second Division South |
| Croatia | NK Marsonia | 7138 | Croatian First Division | Croatian Second Division North |
| Croatia | NK TSK | 11312 | Croatian First Division | Croatian Second Division South |
| Croatia | Ivancica | 5484 | Croatian Second Division North | Croatian Third Division North |
| Croatia | NK Bjelovar | 1625 | Croatian Second Division North | Croatian Third Division North |
| Croatia | NK Mladost Molve | 7435 | Croatian Second Division North | Croatian Third Division North |
| Croatia | NK NASK-Nasicecement | 7679 | Croatian Second Division North | Croatian Third Division East |
| Croatia | NK Papuk | 8582 | Croatian Second Division North | Croatian Third Division North |
| Croatia | NK Sloga (C) | 10671 | Croatian Second Division North | Croatian Third Division North |
| Croatia | HNK Segesta | 10300 | Croatian Second Division South | Croatian Third Division Central |
| Croatia | NK Mosor | 7581 | Croatian Second Division South | Croatian Third Division South |
| Croatia | NK Samobor | 10020 | Croatian Second Division South | Croatian Third Division Central |
| Croatia | NK Trnje | 11809 | Croatian Second Division South | Croatian Third Division Central |
| Croatia | NK Vrbovec | 12615 | Croatian Second Division South | Croatian Third Division Central |
| Croatia | NK Zminj | 13160 | Croatian Second Division South | Croatian Third Division West |
| Croatia | NK Metalac (S) | 7289 | Croatian Third Division Central | Croatian Lower Division |
| Croatia | NK Moslavac | 7579 | Croatian Third Division Central | Croatian Lower Division |
| Croatia | NK Moslavina | 7580 | Croatian Third Division Central | Croatian Lower Division |
| Croatia | NK Ostrc | 8395 | Croatian Third Division Central | Croatian Lower Division |
| Croatia | NK Radnik | 9300 | Croatian Third Division Central | Croatian Lower Division |
| Croatia | NK Spansko | 10839 | Croatian Third Division Central | Croatian Lower Division |
| Croatia | NK Lokomotiva Cestorad | 6716 | Croatian Third Division East | Croatian Lower Division |
| Croatia | NK Tomislav | 11649 | Croatian Third Division East | Croatian Lower Division |
| Croatia | NK Zrinski | 13174 | Croatian Third Division East | Croatian Lower Division |
| Croatia | Drava | 3267 | Croatian Third Division North | Croatian Lower Division |
| Croatia | Kucan '99 | 6197 | Croatian Third Division North | Croatian Lower Division |
| Croatia | NK Garic | 4309 | Croatian Third Division North | Croatian Lower Division |
| Croatia | NK Ivanska | 5487 | Croatian Third Division North | Croatian Lower Division |
| Croatia | NK Omladinac (C) | 8275 | Croatian Third Division North | Croatian Lower Division |
| Croatia | NK Pitomaca | 8832 | Croatian Third Division North | Croatian Lower Division |
| Croatia | NK Trnje (T) | 11810 | Croatian Third Division North | Croatian Lower Division |
| Croatia | DOSK | 2889 | Croatian Third Division South | Croatian Lower Division |
| Croatia | NK Orkan | 8339 | Croatian Third Division South | Croatian Lower Division |
| Croatia | NK Primorac | 9066 | Croatian Third Division South | Croatian Lower Division |
| Croatia | NK Mladost Fazana | 7432 | Croatian Third Division West | Croatian Lower Division |
| Croatia | NK Otocac | 8403 | Croatian Third Division West | Croatian Lower Division |
| Finland | Oulun Tervarit | 11488 | Finnish First Division North | Finnish Second Division North |
| Finland | TPV Tampere | 11307 | Finnish First Division North | Finnish Second Division West |
| Finland | FC HIFK | 4728 | Finnish First Division South | Finnish Lower Division |
| Finland | FC Jokerit | 3801 | Finnish First Division South | Finnish Premier Division |
| Finland | FC KooTeePee | 6105 | Finnish First Division South | Finnish Premier Division |
| Finland | IF Gnistan | 4454 | Finnish First Division South | Finnish Second Division South |
| Finland | Salon Palloilijat | 9979 | Finnish First Division South | Finnish Second Division West |
| Finland | Terjärv US | 11315 | Finnish Lower Division | Finnish Second Division North |
| Finland | FC Mikkeli | 3827 | Finnish Second Division East | Finnish Lower Division |
| Finland | Mikkelin Pallo | 7346 | Finnish Second Division East | Finnish Lower Division |
| Finland | Hangö Idrottsklubb | 3790 | none | Finnish Lower Division |
| France | ES Viry-Châtillon | 12535 | French CFA | French National |
| France | FC Trélissac Périgord | 11780 | French CFA | French National |
| France | Stade Olympique Romorantin | 9648 | French CFA | French National |
| France | Clermont Foot Auvergne | 2598 | French National | French Second Division |
| France | Stade de Reims Champagne | 9467 | French National | French Second Division |
| France | Ajaccio AC | 330 | French Second Division | French First Division |
| France | OGC Nice | 7933 | French Second Division | French First Division |
| Germany | 1.FC Köln U23 | 5929 | A Lower Division | German Regional Division North |
| Germany | Eintracht Frankfurt Amateure | 4163 | German Regional Division North | German Regional Division South |
| Northern Ireland | Ards | 813 | Northern Irish League First Division | Northern Irish League Premier Division |
| Northern Ireland | Institute | 5373 | Northern Irish League First Division | Northern Irish League Premier Division |
| Poland | Polonia Bytom | 8923 | Polish Second Division | Polish Lower Division |
| Poland | Zaglebie Sosnowiec | 13068 | Polish Second Division | Polish Lower Division |
| Republic of Ireland | Bray Wanderers A.F.C. | 1855 | Irish Premier Division | Irish First Division |
| Republic of Ireland | Galway United | 4287 | Irish Premier Division | Irish First Division |
| Russia | Metallurg Lipetsk | 7301 | Russian Second Division - Central | Russian First Division |
| Russia | Mosgaz Moscow | 7574 | Russian Second Division - Central | Russian Lower Division |
| Russia | Metallurg-Zapsib Novokuznetsk | 7304 | Russian Second Division - East | Russian First Division |
| Russia | Terek Grozny | 11467 | Russian Second Division - South | Russian First Division |
| Russia | Uralmash Yekaterinburg | 12117 | Russian Second Division - Ural | Russian First Division |
| Russia | Biokhimik-Mordovia Saransk | 1601 | Russian Second Division - Volga | Russian Second Division - Central |
| Russia | Baltika Kaliningrad | 1277 | Russian Second Division - West | Russian First Division |
| South Korea | Sangmoo | 10106 | Korean Amateur League | Korean League |
| Turkey | Batman Petrolspor | 1148 | Turkish 2. Division Category A | Turkish 2. Division Category B G5 |
| Turkey | Siirt JETPAspor | 10515 | Turkish 2. Division Category A | Turkish 2. Division Category B G5 |
| Turkey | Ispartaspor | 5459 | Turkish 2. Division Category B G2 | Turkish Lower Division |
| Turkey | Agrispor | 282 | Turkish 2. Division Category B G5 | Turkish Lower Division |
| Turkey | Muglaspor | 7627 | none | Turkish 2. Division Category B G2 |
| Turkey | Çubukspor | 2844 | none | Turkish 2. Division Category B G4 |
| United States | Connecticut Wolves | 2702 | American A-League | none |
| United States | Hershey Wildcats | 5013 | American A-League | none |
| United States | Long Island Rough Riders | 6728 | American A-League | American D3-Pro League |
| United States | San Diego Flash | 10036 | American A-League | none |
| United States | Boston Bulldogs | 1780 | American D3-Pro League | none |
| United States | Chico Rooks | 2490 | American D3-Pro League | none |
| United States | Rhode Island Stingrays | 9517 | American D3-Pro League | none |
| United States | Stanislaus County Cruisers | 11056 | American D3-Pro League | none |
| United States | Tucson Fireballs | 11851 | American D3-Pro League | none |
| United States | Miami Fusion FC | 7347 | American Major League | none |
| United States | Tampa Bay Mutiny | 11353 | American Major League | none |

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
