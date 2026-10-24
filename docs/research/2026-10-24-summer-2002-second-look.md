# Summer 2002: the second-look rows and the held staff rows

Spec: `docs/specs/2026-10-24-summer-2002-db-spec.md` (Q24, Q22). Work of 2026-10-24 on branch `s2002-accuracy`
(from `s2002-build`). The per-row research tables are outside the repo, in the shared notes folder of the session
(`s2002-leagues/out-*.tsv` and `s2002-holds/out-holds.tsv`), because they quote the Xbox and 3.9.68 values. Every
source below is public (Wikipedia, RSSSF, club and FA pages). No other database maker's work is used (Q351).

## 1. League rows (Q24)

### Counts

| | Applied | Kept | Second look | Rows |
|---|---|---|---|---|
| Before | 66 | 52 | 376 | 494 |
| After | 143 | 73 | 275 | 491 |

101 second-look rows are settled. Three rows left the list because a fixed club id now makes the three values
agree (Yozgatspor 13033, HNK Dubrovnik 3304, Metalac Osijek). The change list rule is unchanged, with one
addition: `SPLIT` in `change_list.py` lets the table row "Ykkönen" (one group from 2003) agree with the 3.9.68
divisions First Division North and South. It settles FC Korsholm.

### Club-table changes

Fixes of 2002/03 rows. These change the game's `Seasons/clubs.tsv` (8 moves of season 2002, the same count of
22,769 moves):

| Part | Row | Old id | New id | Proof |
|---|---|---|---|---|
| rus-tur | Category A, Yozgatspor | 13033 Yozgatspor | 13032 Yimpas Yozgatspor A.S. | RSSSF tur02: Yimpaş Yozgatspor 18th, relegated from the Süper Lig. tr.wikipedia 2002-03 3. Lig lists the other Yozgatspor in Group 2. |
| rus-tur | First Division, Lisma-Mordovia Saransk | 1601 Biokhimik-Mordovia | 11242 Svetotekhnika Saransk | Lisma-Mordovia is the renamed Svetotekhnika. Biokhimik-Mordovia played the 2003 Second Division Centre zone (ru.wikipedia). |
| gre-pol-cro | cro_a2b, GOŠK Dubrovnik | 3304 HNK Dubrovnik | 4259 NK GOSK | en.wikipedia 2002-03 2. HNL names GOŠK. hr.wikipedia 3. HNL Jug lists Dubrovnik. |
| gre-pol-cro | cro_a2a, Metalac Osijek | 7289 NK Metalac (S) | 7286 NK Metalac | 2. HNL North lists Metalac Osijek. The (S) club is Metalac Sisak, a county club. |
| gre-pol-cro | cro_a2b, TŠK Topolovac | none | 11312 NK TSK | The same club (HNK TŠK, 1. HNL in 2001/02). |
| fra | CFA, Racing Club de France | none | 9266 Racing Club de Paris | fr.wikipedia: the club was Racing Club de Paris from 1999 to 2005. |
| fra | CFA 2, AS Choisy-le-Roi | none | 2516 Choisy-le-Roy | The same club, another spelling. |
| fra | CFA 2, Stade Luçonnais | none | 6793 Luçon | The only Luçon club of 3.9.68. |

New 2002/03 rows (102): Croatia 37, Argentina 21, France 16, Turkey 10, Finland 10, Poland 4, United States 2,
Germany 1, Russia 1. Each row names the 3.9.68 competition as its label (for example `Croatian Third Division
North` or `Turkish Lower Division (real: 3. Lig)`). No line of `make_clubs.TARGETS` matches these labels, so the
game's club moves do not change. They only let Q24 settle the row.

Rows of medium confidence (the club name is common, or the place follows from absence in the complete lists of
the higher levels): about 15 Croatian county clubs, NK Primorac (read as Primorac Stobreč), Belgrano de Mendoza,
Sportivo Barraca, Huracán de Mendoza, Leppävaaran Pallo, STPS (FC Savonlinna) and the 9 French clubs whose exact
regional league is not read.

### The 275 rows left, and a proposal for each group

| Group | Rows | Proposal |
|---|---|---|
| A. Turkish Category B groups | 52 | No decision. The Xbox group agrees with the table. The rows wait for the group rows of slice 5 (Q29). |
| B. Club-table rows with no club in 3.9.68 | 79 | D1 |
| C. The table differs from 3.9.68 and from the Xbox data, and a source confirms the table | 106 | D2 |
| D. Irish Premier and First Division | 2 | D3 |
| E. Clubs that folded | 8 | D4 |
| F. No public source settles the row | 28 | D5 |

D1. Close group B as "kept: no club in the database". The rows are 48 French reserve teams, 16 French amateur
clubs, 14 Brazilian Série C 2003 clubs and Daegu FC (founded 2002). 3.9.68 has no row for them. The game finds a
club of that name or counts a missing club (Q359). Recommended.

D2. Add a rule to Q24: where the club table agrees with neither set and the table row has a cited source, the club
takes the table's division. Recommended. The rows:
- Brazil 61 and Russia 11: the table holds the 2003 season (calendar-year nations, `bra.md`, `rus-tur.md`). Both
  SI sets hold 2002. The game already moves these clubs into the 2003 lists at a 2002/03 start, so the data change
  only makes the file agree with the game.
- France 25: 11 clubs are in CFA in both sets but in CFA 2 in 2002/03 (fr.wikipedia CFA 2 2002-2003), 9 are in
  CFA in 2002/03 and in no CFA list in 3.9.68 or the Xbox data, plus Calais RUFC (CFA 2), Noisy-le-Sec (CFA),
  Lusitanos de Saint-Maur (regional), FC Dijon (National) and La Vitréenne (CFA 2).
- Finland 7 (2003 Kakkonen and Ykkönen, `nor-fin.md`), Croatia 1 (NK Imotska Krajina, county league), South
  Korea 1 (Sangmoo, K League 2003).
- If no: the rows keep the 3.9.68 league, as now.

D3. Bray Wanderers and Waterford United. The table holds the 2003 summer season (decision 1 of `wal-nir-irl.md`).
For the season from August 2002, Bray is in the Premier Division and Waterford in the First Division, which agrees
with both sets. Recommended: keep the 3.9.68 league and change nothing.

D4. Clubs that folded before the 2002 season take the Xbox "no division". Recommended. Miami Fusion and Tampa Bay
Mutiny (MLS contraction, January 2002), Hershey Wildcats (October 2001), San Diego Flash, Boston Bulldogs and
Tucson Fireballs (2001), FC Mikkeli (dissolved 2001), Riihimäen Palloseura (inactive from 2001). Sources are in
`out-arg-usa-kor.tsv` and `out-calendar.tsv`.

D5. These rows keep the 3.9.68 league (no change). Recommended.
- Turkey 17: in no 2002/03 professional list (Süper Lig, 2. Lig A, 2. Lig B, 3. Lig). 3.9.68 holds them in the
  Turkish Lower Division, which is right for a club below the 3. Lig.
- United States 3: Chico Rooks and Rhode Island Stingrays (fourth level, no 3.9.68 equivalent), Connecticut Wolves
  (sources disagree on the end of the club).
- Mosgaz Moscow (two rows), DOSK, NK Omladinac (C), Mikkelin Pallo, Mullingar Town, Saint Joseph's Boys: no source
  names the club.
- Chacras de Coria: merged into Luján de Cuyo in 2000. 3.9.68 holds both clubs.

### Risk: division sizes

The build moves clubs between divisions but adds no division. Some 3.9.68 divisions now hold far fewer clubs than
the game class takes (`season_clubs.c`, class counts):

| Division | Class | 3.9.68 | Build before | Build now |
|---|---|---|---|---|
| Finnish First Division North | 10 | 10 | 3 | 3 |
| Finnish First Division South | 10 | 10 | 3 | 2 |
| Turkish 2. Division Category B G1 to G5 | 10 | 10 | 9 | 7 or 8 |
| Croatian Second Division North and South | 16 | 16 | 19, 18 | 12, 13 |

Croatia and Argentina now match the real 2002/03 sizes, and their club-table lists fill the divisions in a season
game. The Ykkönen lists are left out of the game's club moves (`SKIP_GROUPS`), so nothing fills the Finnish First
Division groups. I don't know if a game starts with 3 or 2 clubs in a 10-club division. Slice 4 runs no game test
yet. Proposal D6: before the zip goes on the forum, `build_2002.py` checks each division with a class count and
holds the league rows that leave it short. The Finnish rows then keep the 3.9.68 league.

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

The four "apply" rows need one new step in the change list: a list of named holders who leave a place, with the
source of each. It is not built. Max's yes comes first.

## 3. Found on the way (not changed)

- `fra-clubs.tsv` 2003/04 and 2005/06 CFA rows name 8880 (CEP Poitiers). The club is Stade Poitevin (8879 in
  3.9.68). Those rows name another season's database, so check the id there.
- `rus-tur-clubs.tsv` 2003/04 Lisma-Mordovia row names 1601. Check it in that season's database.
- `aus-jap-kor-clubs.tsv` names 2904 "Daegu FC" from 2006/07. In 3.9.68, 2904 is Daegu University.
- `fra-clubs.tsv` names 6846 "Sainte-Geneviève Sports" from 2007/08. In 3.9.68, 6846 is Lusitanos de Saint-Maur.
- `summary.md` (change list) still says the Turkish groups need a decision. Q29 settled it.
- `build_2002.py` stopped on Hangö IK, the first applied club with no 3.9.68 division. The check now reads a
  missing division as "none", the change list's word.
