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
| After | 285 | 214 | 0 | 499 |

- All 216 "change" rows end in their checked division (a script compared each one). 214 apply, and 2 put back
  the 3.9.68 division of a row that the Xbox data had moved, so they show as kept.
- Q37 adds 5 rows for clubs that had no kind 5 row (JCO Le Creusot, Stade Lannionnais, Oulun Tervarit and
  others). "No league" is club.dat +0x57 = -2, the value of 3.9.68.
- The 81 club-table rows with no club are kept with rule Q35.

### Division sizes after the build (Q34)

`xbox-0203-work/build/summer-2002-work/division-sizes.tsv` compares each division with the 2002/03 club count of
the rules table. 48 of 57 match. The 9 that differ:

| Division | Built | 2002/03 | Why |
|---|---|---|---|
| Finnish First Division (118) | 13 | 14 | The check puts the Ykkönen clubs in competition 118, "Finnish First Division". The game classes are First Division North and South (10 each, `season_clubs.c`), which now hold 0 clubs. I don't know if the game plays competition 118. Slice 3 already moved 7 clubs there. |
| Russian First Division | 23 | 22 | Metallurg Krasnoyarsk (7299) and SKA Rostov (9886) have no check row and no table row. Lisma-Mordovia (11242) is missing: the table row names 1601. |
| Brazilian National Second Division | 25 | 24 | Clube Malutrom (7015) has no check row and no table row. |
| Brazilian National Third Division | 99 | 93 | Clubs with no check row stay. 3.9.68 holds 56. |
| French National | 21 | 20 | Lusitanos de Saint-Maur stays by Q36. |
| French CFA | 51 | 72 | 24 reserve sides and amateur clubs that the database lacks (Q35). |
| French Lower Division | 143 | 128 | The 3.9.68 Lower Division also holds the regional clubs below CFA 2. |
| Korean League | 11 | 12 | Daegu FC is not in the database. |
| German Regional Division North and South | 18 + 19 | 19 | The rules row counts the Regionalliga Süd only (19, a match). |

No move is dropped.

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

Max said yes to the four "apply" rows (Q39, 2026-10-24). `change_list.HOLDERS_Q39` releases the four holders
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
