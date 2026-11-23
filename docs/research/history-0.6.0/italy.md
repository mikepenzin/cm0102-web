# Italy: competition history 2002 to 2009

Part `italy`, slice 11c. 82 rows in `italy.tsv`, all `held = yes`, no `unknown` rows. All club competitions use first and second only. The source for every row is the Italian Wikipedia page of that season, read raw on 2026-10-17.

## Conventions

### Season-end year

The label year is the year in which the season ends. This applies to:

- Italian Serie A and Italian Serie B. 3.9.68: Serie A 2002 Juventus, AS Roma = 2001/02. Serie A 2000 Lazio, Juventus = 1999/2000. Serie B 2002 Como, Modena = 2001/02.
- Italian Cup (Coppa Italia). 3.9.68: 2002 Parma, Juventus = the 2001/02 final. 2001 Fiorentina, Parma = 2000/01.
- Italian Serie C Cup (Coppa Italia Serie C). 3.9.68: 2001 Prato = 2000/01 (Prato beat Lumezzane). 2002 Albinoleffe, Livorno = 2001/02.
- Italian C1 Super Cup (Supercoppa di Lega di Serie C). The two legs are in May at the end of the season, so the season-end year is also the year of play. 3.9.68: 2000 Siena (beat Crotone, May 2000), 2001 Modena (beat Palermo, May 2001), 2002 Ascoli, Livorno (May 2002).
- The Serie C1 and C2 groups (see below).

For the league rows, second is the table runner-up in Serie A and Serie B. 3.9.68 Serie B 1999 Hellas Verona, Torino and 2000 Vicenza, Atalanta match the tables.

### Italian Super Cup: year of play

The label year is the year in the edition name, which is the year of play (August). 3.9.68: 1998 Lazio, Juventus (August 1998), 2000 Lazio, Parma (September 2000), 2001 AS Roma, Fiorentina (19 August 2001). The 1995 edition was played in January 1996 and 3.9.68 labels it 1995, so the edition name sets the label. All editions from 2002 to 2009 were played in August of their year.

### Serie C1 and C2 groups: second is the play-off winner

In each group, first is the group champion. Second is the winner of the group promotion play-off, also when that club was not second in the table. 3.9.68 proves it:

- C1/A 1999 Alzano 1909 Virescit, Pistoiese. The 1998/99 table had Como 2nd. Pistoiese won the play-off.
- C1/A 2000 Siena, Cittadella Padova. Pisa was 2nd in the table. Cittadella won the play-off.
- C1/B 1999 Fermana, Savoia 1908. Palermo was 2nd in the table. Savoia won the play-off.
- C2/B 1998 S.P.A.L., Arezzo (Rimini 2nd in the table). C2/B 1999 Viterbese, Sandonà (Triestina 2nd). C2/B 2000 Sassari Torres, Vis Pesaro 1898 (Rimini 2nd). C2/A 2001 Padova, Triestina (Pro Patria 2nd).

The research rows follow this rule. The game group letters A, B and C match the Lega groups (gironi) A, B and C. In 2008/09 Serie C1 became Lega Pro Prima Divisione and Serie C2 became Lega Pro Seconda Divisione, with the same groups. The rows for label 2009 map to them.

## Points of doubt

1. Serie A 2004/05 (label 2005). Juventus finished first. In July 2006 the federation revoked the title (Calciopoli) and gave it to no club. The row holds Juventus, AC Milan as on the pitch. If the game must show the official record, the champion is "not assigned". Max must decide. The record has no field for this. **Decided 3 November 2026 (0.6.2 item 108, Max: do what real life does):** the row follows today's official record (see docs/research/2026-10-17-competition-history-fill.md, Q355).
2. Serie A 2005/06 (label 2006). The row holds the official result: Internazionale champion (title assigned in July 2006), AS Roma 2nd in the official table after the points penalties. On the pitch Juventus was first and AC Milan second. This row and row 1 use different rules. If Max wants one rule, change one of them. **Decided 3 November 2026 (0.6.2 item 108, Max: do what real life does):** the row follows today's official record (see docs/research/2026-10-17-competition-history-fill.md, Q355).
3. Serie B 2004/05 (label 2005). Genoa finished first on the pitch but was put last for match-fixing. Empoli is first in the official table, Torino second. Torino won the play-offs but was refused a Serie A licence.
4. Club identity. Fiorentina in C2/B 2002/03 is Florentia Viola, the club refounded in 2002. Napoli in C1/B 2005/06 is Napoli Soccer, the club refounded in 2004. The rows use the 3.9.68 names of the old clubs. Potenza in C2/C 2006/07 is Potenza SC, which the data holds as `Potenza` (the data also holds a separate `FC Potenza`). Real Marcianise (C2/C 2007/08) uses the later databases' name. The 3.9.68 `Marcianise` is possibly an older club (inference).
5. Outside the range, 3.9.68 Serie B 2001 holds Torino, Chievo Verona. The 2000/01 table had Piacenza 2nd and Chievo 3rd. I did not change it. It is noted here only.

## Database rows that disagree

Labels after the slice 11b shift. Rows that match are not listed.

| Database | Competition | Label | Holds | Real |
|---|---|---|---|---|
| October 2003 | Italian Serie A | 2003 | Juventus, AC Milan | Juventus, Internazionale |
| November 2007, February 2008 | Italian Serie A | 2007 | AS Roma, Juventus | Internazionale, AS Roma |
| April 2010 SIM | Italian Serie A | 2009 | Internazionale, AS Roma (2007/08) | Internazionale, Juventus |
| October 2003 | Italian Serie B | 2003 | Siena, Modena | Siena, Sampdoria |
| September 2005 | Italian Serie B | 2005 | Città di Palermo, Livorno (2003/04 with the 3rd club) | Empoli, Torino |
| April 2010 SIM | Italian Serie B | 2009 | Chievo Verona, Bologna F.C. 1909 (2007/08) | Bari, Parma |
| October 2003 | Italian Cup | 2003 | Parma, Juventus (2001/02) | AC Milan, AS Roma |
| November 2007, February 2008 | Italian Cup | 2007 | Internazionale, AS Roma | AS Roma, Internazionale |
| April 2010 SIM | Italian Cup | 2009 | AS Roma, Internazionale (2007/08) | Lazio, Sampdoria |
| October 2003 | Italian C1 Super Cup | 2003 | Ascoli, Livorno (2002) | Treviso, Avellino |
| September 2005 | Italian C1 Super Cup | 2005 | Ascoli, Livorno (2002) | Rimini, Cremonese |
| October 2003 | Italian Serie C Cup | 2003 | Albinoleffe, Livorno (2001/02) | Brindisi, Pro Patria |
| September 2005 | Italian Serie C Cup | 2005 | Albinoleffe, Livorno (2001/02) | Spezia 1906, Frosinone |
| September 2008, March 2009 | Italian Serie C1/A | 2008 | Sassuolo, Ancona | Sassuolo, Cittadella |
| April 2010 SIM | Italian Serie C1/A | 2009 | Sassuolo, Ancona (2007/08) | Cesena, Padova |
| September 2009, February 2010 ODB, June 2010 | Italian Serie C1/A | 2009 | Cesena, Pro Patria (table 2nd) | Cesena, Padova (play-off winner) |
| September 2008, March 2009 | Italian Serie C1/B | 2008 | Salernitana, Cittadella | Salernitana, Ancona |
| April 2010 SIM | Italian Serie C1/B | 2009 | Salernitana, Cittadella (2007/08) | Gallipoli, Crotone |
| September 2009, February 2010 ODB, June 2010 | Italian Serie C1/B | 2009 | Gallipoli Calcio SRL, Benevento Sporting (table 2nd) | Gallipoli, Crotone (play-off winner) |
| September 2008, March 2009 | Italian Serie C2/A | 2008 | Pergocrema, no second | Pergocrema, Lumezzane |
| April 2010 SIM | Italian Serie C2/A | 2009 | Pergocrema (2007/08) | Varese, Como |
| September 2008, March 2009 | Italian Serie C2/B | 2008 | Reggiana, no second | Reggiana, Portogruaro-Summaga |
| April 2010 SIM | Italian Serie C2/B | 2009 | Reggiana (2007/08) | Figline Valdarno, Giulianova |
| September 2008, March 2009 | Italian Serie C2/C | 2008 | Benevento Sporting, no second | Benevento Sporting, Real Marcianise |
| April 2010 SIM | Italian Serie C2/C | 2009 | Benevento Sporting (2007/08) | Cosenza, Pescina Valle del Giovenco |

Other notes on the database rows:

- The April 2010 SIM rows for label 2009 hold the 2007/08 results in every Italian competition. Its history is one season behind.
- The February 2010 ODB rows hold `T` in third and host for the Italian Cup, Serie A, Serie B and the C1 and C2 groups. Those fields are not used by club competitions.
- Some databases hold a third club in Serie A and Serie B. The record uses first and second only, so the research rows leave third empty.
- The 2009 C1 rows of September 2009, February 2010 ODB and June 2010 use the table runner-up. That disagrees with the 3.9.68 rule. Max can decide whether the play-off rule or the table rule applies from 2009. The research rows follow 3.9.68.

## Names

All first and second names match a full name in `clubs/Italy.tsv`. Where the data has two spellings, the rows use the 3.9.68 one: `Lazio`, `Napoli`, `Bologna 1909`, `Cittadella Padova`, `Fiorentina`, `Albinoleffe`, `Catania`, `Portogruaro-Summaga`. Gallipoli, Pergocrema, Figline Valdarno and Pescina Valle del Giovenco are not in 3.9.68, so the rows use the later databases' names. No name is unmatched.
