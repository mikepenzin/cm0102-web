# Rows left as the data has them (open item 233)

`unverified.tsv` lists 2,048 history rows that `tools/seasons/history_years_check.py` finds one year off, or cannot place, but leaves as the data has them. The game does not move them. Each group below names what was tried. The sourced lists are in `refs/`; the search notes of each edition are in their `note` column.

| Competition | Rows | Databases | Why the rows stay |
|---|---|---|---|
| Irish Dublin City Cup | 1551 | 39 | RSSSF and Wikipedia give the season of each edition (1930s to 1970s) but no final date, and no other source was found (`refs/irish-cups.tsv`, 41 rows without a label). The 3.9.68 data and the season databases label the editions one year apart, and without the date of the final neither year can be proved. |
| Irish League Cup | 323 | 46 | RSSSF gives the date of every final from 1973/74 to 1999/2000 (`refs/irish-cups.tsv`). 855 rows of the data already hold the year of their final. The other rows would move onto a year that another final holds: in 1975, 1987 and 1998 two finals fell in one calendar year (for example 23 February and 12 December 1998), so the year would show two rows. |
| Irish First Division | 38 | 2 | September 2010 ODB and February 2011 ODB: the 2003 row has no club name, so the run of 1986 to 2002 would move onto the year 2004 that a right row holds. |
| Brazilian Northeast State Championship | 34 | 26 | The sourced list covers Ceara, Rio Grande do Norte, Paraiba, Maranhao and Alagoas from 1958 to 1996 (`refs/brazil-states.tsv`). Left: the Amazonas rows Nacional AM and Santos AM and the Auto EC row of 1957 to 1959 in October 2003 and September 2005 (no Amazonas list was searched), Campinense 1974 and 1975 (the 1975 Paraiba title was shared by Botafogo and Treze, so no move can be proved) and America RN 1974 (the move would put two RN rows on one year). |
| Belgian Super Cup | 33 | 6 | The 1979 to 1985 rows: the data holds Club Brugge / Anderlecht in 1985 to 1988 in an order that fits no single shift (the pair met in 1986, 1988, 1991, 1994 and 2004). The 1989 to 2011 rows are corrected. |
| Finnish First Division | 25 | 1 | April 2010 SIM: the run would move onto 1974, which a right row holds. |
| German First Division | 22 | 1 | December 2008: the 1923 to 1943 rows are one year early (sourced, `refs/fin-ger-irl-usa.tsv`), but the post-war rows are four years early (1944 Nurnberg / Kaiserslautern is the 1948 final, 1945 Mannheim / Dortmund the 1949 final), beyond the one-year moves of the tool. Moving the pre-war run would meet the 1944 row. |
| Brazilian Minas Gerais Lower Division | 22 | 1 | April 2015: the data holds the 2010 Ipatinga row three times, so the run would put three rows on 2009. |

Rows that the lists name as no real edition are cleared only when a corrected row needs their year: Belgian Second Division KSK Beveren / Beerschot (2013, three databases), Belgian Super Cup Tubantia Borgerhout / Uccle Sport (2012, three databases). Other rows with a wrong runner-up (Belgian Second Division 2014 Westerlo / Mouscron, US Open Cup 2009 Columbus Crew) keep their names: the fix moves years only.
