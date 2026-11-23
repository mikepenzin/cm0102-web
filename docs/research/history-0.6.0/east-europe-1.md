# History research: east-europe-1 (Poland, Croatia, Czech Republic)

Part `east-europe-1` of slice 11c. Rows are in `east-europe-1.tsv`: 112 rows, 0 `unknown`. All facts come from RSSSF pages read on 17 October 2026 with `curl` (the URL is in each row).

## Label conventions

### Poland: the year where the season ends

The 3.9.68 rows prove it:

- Polish First Division 2000 Polonia Warszawa, 2001 Wisla Krakow. Polonia won 1999/2000 and Wisla won 2000/01.
- Polish FA Cup 2001 Polonia Warszawa over Gornik Zabrze. That final was in 2001.
- Polish League Cup 2000 Polonia over Legia, 2001 Wisla over Zaglebie Lubin, 2002 Legia over Wisla. These are the finals of 2000, 2001 and 2002.

The Polish Super Cup uses the year of the match, which is played in July before the season. Proof: 3.9.68 2000 Polonia Warszawa over Amica Wronki was the July 2000 match between the 1999/2000 champion and the 2000 cup winner.

### Croatia: the year where the season starts

The Croatian rows of 3.9.68 are one year earlier than the usual convention:

- Croatian Cup 1998 NK Osijek over HNK Cibalia. That is the 1998/99 final.
- Croatian Cup 2000 NK Dinamo over HNK Hajduk. That is the 2000/01 final. The 1999/2000 final went to Hajduk, and 3.9.68 holds it as 1999.
- Croatian Cup 2001 NK Dinamo over NK Varteks. That is the 2001/02 final.
- Croatian First Division 2001 NK Zagreb, runner-up HNK Hajduk. That is 2001/02.
- Croatian Second Division 2000 NK Kamen Ingrad. That is 2000/01.
- Croatian Second Division North 2001 HNK Vukovar '91 and NK Belisce, South 2001 NK Istra and NK Uljanik. That is 2001/02.
- The five third-level groups, 2001: for example, Central NK Velika Mlaka and ZET. That is 2001/02.

So label Y holds season Y/Y+1, and the research range 2002 to 2009 covers the seasons 2002/03 to 2009/10. The last label, 2009, is the season 2009/10, which ends after the game starts. I researched it as the brief says.

The Croatian Super Cup uses the year of the match, in July, which is also the start year of the season. The 3.9.68 rows for 1995 to 1998 (HNK Hajduk 1995, NK Dinamo 1996 to 1998) have a winner and no runner-up. RSSSF says the Super Cup was not played in those years because one club won the double. So 3.9.68 shows a year with no match as a winner-only row for the double winner. All four labels in range (2002 to 2005) had a real match.

### Czech Republic: the year where the season ends

- Czech First Division 2001 AC Sparta Prague = 2000/01. 2002 FC Slovan Liberec = 2001/02.
- Czech FA Cup 2001 FK Viktoria Zizkov over AC Sparta Prague = the 2001 final.

## Structure changes and how I mapped them

- Polish Second Division: II liga for 2001/02 to 2004/05, the second level. The range stops at 2005.
- Polish League Cup: not played from 2002/03 to 2005/06. It came back as the Puchar Ekstraklasy for 2006/07 to 2008/09. Labels 2003 to 2006 are `not held`.
- Polish Super Cup: not played in 2002 (financial reasons), 2003 and 2005. The 2004 Super Cup was the last league match of 2003/04, Lech Poznan v Wisla Krakow, which PZPN also counted as the Super Cup.
- Croatian Second Division: two groups (North and South) from 2001/02 to 2005/06, then one league from 2006/07. So the single league is `not held` for 2002 to 2005, and North and South are `not held` for 2006 to 2009.
- Croatian third level: five groups (East, North, West, Centre, South) to 2005/06, then three groups (East, West, South) from 2006/07. Central, East, North and West only need 2002 to 2005. South continues, so 2006 to 2009 use the new group South.
- For the North and South second-level groups from 2002/03, the season had a first stage and then a play-off and a play-out group. The rows use the final table of the play-off group.

## Points of doubt

- Polish FA Cup 2005: Dyskobolia Grodzisk Wielkopolski won the final. On 2 September 2020 PZPN took the title away for match fixing, and the title stays unawarded. The row keeps the result on the day. Change it to `not held` if the game must follow the present official record. **Decided 3 November 2026 (0.6.2 item 108, Max: do what real life does):** the row follows today's official record (see docs/research/2026-10-17-competition-history-fill.md, Q355).
- Croatian Second Division North 2005: RSSSF marks the runner-up, Graficar Vodovod Osijek, as "expelled". I kept it as runner-up because it finished second.
- Croatian Second Division South 2004: Novalja and Hrvatski dragovoljac both had 59 points. RSSSF puts Novalja first on head-to-head.
- Croatian Third Division East 2003: RSSSF prints "Vinjevac", which I read as Visnjevac with a lost letter (an inference).
- Club identity in Pula. NK Uljanik became Pula 1856 in 2003 and Istra 1961 later. The older NK Istra is a different club. The rows use NK Uljanik for the Uljanik, Pula 1856 and Istra 1961 club, because that is the 3.9.68 name. The September 2005 database has NK Pula 1856, and the databases from November 2007 have Istra 1961.
- RNK Split (Radnicki Split) is NK Split in 3.9.68 and RNK Split from September 2008. The rows use NK Split.
- For Croatia I used the 3.9.68 names: NK Dinamo, HNK Hajduk, NK Varteks, NK Slaven Belupo, NK INKER (Inter Zapresic). The databases from November 2007 use NK Dinamo Zagreb, HNK Hajduk Split, NK Varteks Varazdin, NK Slaven Belupo Koprivnica, NK Inter Zapresic. For Poland the rows use Orlen Plock (3.9.68) for Wisla Plock, and Groclin Grodzisk (3.9.68) for Dyskobolia.

## Database rows that disagree with the real result

Rows where the database holds the same result under another spelling are not listed.

| Database | Competition | Label | Database holds | Real |
|---|---|---|---|---|
| 3.9.68 | Czech FA Cup | 2002 | SK Slavia Prague, FC Slovan Liberec | SK Slavia Prague, AC Sparta Prague |
| October 2003 | Czech FA Cup | 2003 | FK Viktoria Zizkov, AC Sparta Prague | FK Teplice, FK Jablonec 97 |
| October 2003 | Czech First Division | 2003 | FC Slovan Liberec, AC Sparta Prague | AC Sparta Prague, SK Slavia Prague |
| September 2005 | Czech Second Division | 2005 | FK Mlada Boleslav, FK Drnovice | FC MUS Most, FC Vysocina Jihlava (Mlada Boleslav and Drnovice are 2003/04) |
| October 2003 | Polish FA Cup | 2003 | Wisla Krakow, Amica Wronki | Wisla Krakow, Wisla Plock (Wisla over Amica is 2002) |
| April 2010 SIM | Polish FA Cup | 2009 | KP Legia Warszawa, Wisla Kraków | Lech Poznan, Ruch Chorzow (Legia over Wisla is 2008) |
| October 2003 | Polish First Division | 2003 | Legia Warszawa, Wisla Krakow | Wisla Krakow, Groclin Dyskobolia |
| September 2005 | Polish First Division | 2005 | Wisla Krakow SSA, KP Legia Warszawa SSA | Wisla Krakow, Groclin Dyskobolia (Legia was second in 2003/04) |
| November 2007, February 2008 | Polish First Division | 2007 | Legia Warszawa, Wisla Kraków | Zaglebie Lubin, GKS Belchatow (Legia and Wisla are 2005/06) |
| December 2008 | Polish First Division | 2008 | Legia Warszawa, Wisla Kraków | Wisla Krakow, Legia Warszawa (order reversed) |
| October 2003 | Polish League Cup | 2003 | Legia Warszawa, Wisla Krakow | not held |
| September 2005 | Polish League Cup | 2005 | KP Legia Warszawa SSA, Wisla Krakow SSA | not held |
| September 2009, April 2010 SIM, February 2010 ODB, June 2010 | Polish League Cup | 2009 | KP Legia Warszawa, KKS Lech Poznañ | Slask Wroclaw, Odra Wodzislaw |
| October 2003 | Polish Second Division | 2003 | Lech Poznan, Wilsa Plock | Gornik Polkowice, Swit Nowy Dwor Mazowiecki (Lech is 2001/02) |
| September 2005 | Polish Second Division | 2005 | MKS Pogon Szczecin, SSA Zaglebie Lubin | Korona Kielce, GKS Belchatow (Pogon and Zaglebie are 2003/04) |
| September 2005 | Polish Super Cup | 2005 | Wisla Krakow SSA, Amica Wronki SSA | not held |
| September 2005 | Croatian Cup | 2005 | HNK Rijeka, HNK Hajduk | HNK Rijeka, NK Varteks (Rijeka over Hajduk is 2004/05) |
| November 2007, February 2008 | Croatian Cup | 2007 | NK Dinamo Zagreb, NK Varteks Varazdin | NK Dinamo, HNK Hajduk |
| December 2008 | Croatian Cup | 2008 | NK Dinamo Zagreb, NK Varteks Varazdin | NK Dinamo, HNK Hajduk |
| September 2008, March 2009 | Croatian Cup | 2008 | NK Dinamo Zagreb, no runner-up | NK Dinamo, HNK Hajduk |
| September 2009, April 2010 SIM, February 2010 ODB, June 2010 | Croatian Cup | 2009 | NK Dinamo Zagreb, HNK Hajduk Split | HNK Hajduk, HNK Sibenik (Dinamo over Hajduk is 2008/09) |
| September 2005 | Croatian First Division | 2005 | HNK Hajduk, HNK Rijeka | NK Dinamo, HNK Rijeka |
| September 2008, March 2009 | Croatian First Division | 2008 | NK Dinamo Zagreb, NK Slaven Belupo Koprivnica | NK Dinamo, HNK Hajduk (Slaven second is 2007/08) |
| April 2010 SIM | Croatian First Division | 2009 | NK Dinamo Zagreb, NK Slaven Belupo Koprivnica | NK Dinamo, HNK Hajduk |
| February 2010 ODB, June 2010 | Croatian Second Division | 2009 | Istra 1961, NK Karlovac | NK Split (RNK Split), NK Pomorac (Istra 1961 and Karlovac are 2008/09) |
| September 2008, March 2009 | Croatian Second Division North | 2008 | NK Croatia Sesvete | not held (Croatia Sesvete won the single league in 2007/08) |
| September 2009, April 2010 SIM, February 2010 ODB, June 2010 | Croatian Second Division North | 2009 | NK Croatia Sesvete or NK Istra | not held |
| September 2008 to June 2010 (six databases) | Croatian Second Division South | 2008, 2009 | NK Hrvatski Dragovoljac | not held |

The 3.9.68 Polish Super Cup row for 2001 (outside the range) has Amica Wronki as runner-up. RSSSF has Wisla Krakow 4-3 over Polonia Warszawa.

Many of the later Croatian rows fit the end-year convention better than the start-year convention of 3.9.68. An example is Cup 2009 Dinamo over Hajduk, which is the 2008/09 final. Those databases did not follow one convention. The rows in `east-europe-1.tsv` follow 3.9.68.

## Names not in the data

- Polet Pribislavec (Croatian Third Division North 2004)
- Naftas Ivanic Grad (Central 2003). NK Naftas HASK Zagreb in later databases is a different, later club.
- Rudes Zagreb (Central 2003 and 2004)
- Dugopolje (South 2006 and 2009)
- Hrvace, Zagora Unesic (South 2007)

## Inferred matches

These names are inferences. Check them against the club's town in the data.

- NK Croatia for Croatia Djakovo (East 2005)
- NK Rudar for Rudar Labin (West 2004)
- NK Jadran for Jadran Porec (West 2005). NK Jadran (KS) is Kastel Sucurac.
- NK GOSK for GOSK Dubrovnik (South 2005). GOSK Adriachem is a different club.
- NK Granicar for Granicar Zupanja (East 2003 and 2004). The data has two clubs with this name.
- NK Medimurje (September 2005 database only) for Medimurje Cakovec (North 2003)
