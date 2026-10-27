# Rules table part rus-tur: Russia and Turkey (slice 7)

Research for release 0.6.0. Date: 2026-10-16. Files:

- `rus-tur.tsv`: 324 rows, 12 competitions x 27 seasons (2001/02 to 2027/28).
- `rus-tur-clubs.tsv`: 1,595 club rows for the divisions whose real size differs from the season database.
- This file: method, decisions, missing clubs, matches to check and unknowns.

## Scope

| comp_file | Competition | Notes |
|---|---|---|
| rus_prm | Russian Premier League (tier 1) | 16 clubs in every season |
| rus_first | First Division, then FNL, then First League (tier 2) | size changes often |
| rus_cup | Russian Cup | |
| new | Russian Super Cup | from 2003 |
| new | Russian Premier League Cup | held once, 2003 |
| n/a | Russia nation rules | foreign limits, registration, windows, UEFA ban |
| tur_first | Süper Lig (tier 1) | 18, 21, 20, 19, 20, 19, 18 |
| tur_second | 2. Lig Category A, Lig A, TFF 1. Lig (tier 2) | the original's "Category A" |
| tur_second_b | 2. Lig Category B, Lig B, TFF 2. Lig (tier 3) | the original's "Category B" groups |
| tur_cup | Turkish Cup | |
| new | Turkish Super Cup | revived 2006, four clubs from 2025 |
| n/a | Turkey nation rules | foreign limits, registration, windows |

Out of scope: the Russian third tier (Second Division zones, later PFL and Second League) and the Turkish fourth tier (3. Lig). The original runs no league code for them, so they are not "same tier" additions. The databases still hold their clubs. Max can add them if he wants.

## Method

- Season pages of the English Wikipedia, read raw (`action=raw`) and as rendered HTML (REST API) on 2026-10-16. Turkish Wikipedia for the third tier in 2001/02 to 2004/05, 2006/07, 2008/09, 2017/18 to 2021/22 and 2024/25. Russian Wikipedia for the Russian foreign limits.
- Club counts come from the league table of each season page (rendered HTML), checked against the match count.
- Foreign limits come from news pages (sport-express.ru, TASS via news.mail.ru, aspor.com.tr, hurriyet.com.tr, medyascope.tv, goal.com Turkey).
- No WebFetch summary was used for a number. Every number was read from the page text with a script.
- `up` is direct places. A "2 + 1" in `up` means two direct places and one play-off place.
- For the Russian third tier `down` counts come from the table markers of the season page. The regulation count can differ where clubs withdrew or failed licensing. The notes say so.

## Decisions taken in the table

1. **Calendar years.** Russia played March to November until 2010. Per the brief, row 2001/02 holds 2002, up to row 2009/10 with 2010.
2. **The long season.** The 2011-12 season ran from 12 March 2011 to 22 May 2012 (44 rounds in the Premier League). Row 2010/11 holds its first stage (2011, 30 rounds, no promotion or relegation at the end of 2011). Row 2011/12 holds the second stage (spring 2012: groups of 8, two down, play-offs). The 2012/13 row is the first autumn-spring season, from July 2012. The FNL follows the same pattern (38 rounds, then a promotion group of 8 and a relegation group).
3. **Russian Super Cup.** Editions 2003 to 2011 were in March, before the calendar season, so edition N sits in row N-1/N. From 2012 the edition is in July, so edition N sits in row N/N+1. Row 2011/12 has no edition. Row 2001/02 has none (first held 2003).
4. **Russian Premier League Cup.** Held once, in 2003. By the calendar rule it sits in row 2002/03, although the final was in September 2003.
5. **Russian club lists.** For a calendar-year season the clubs of the row's real season are listed. Example: row 2010/11 lists the 2011-12 FNL, while the September 2010 database holds the clubs of 2010.
6. **UEFA ban.** UEFA suspended all Russian clubs on 28 February 2022. The nation rules rows say so from 2021/22. From 2022/23 the league and cup give no European places.
7. **Turkish Super Cup.** Edition N sits in row N/N+1 (curtain raiser). The 2011 edition was cancelled. The 2023 edition was played on 7 April 2024.

## Season databases used for club moves

Russia: Premier Division 16 in every database, real 16 in every season, so no list. First Division 18 in every database. Lists for 2002/03, 2003/04, 2005/06, 2007/08 to 2013/14 and 2015/16 to 2021/22 (real 17, 19, 20 or 22). No list for 2001/02, 2014/15 and 2022/23 to 2025/26 (real 18).

Turkey: Premier Division 18 in every database. Lists for 2020/21 to 2024/25. Category A 20 in every database. Lists for every database season with a real size of 17, 18 or 19 (2002/03 to 2023/24). No list for 2001/02, 2024/25 and 2025/26 (real 20). Category B is 5 groups of 10 in every database. No list for 2001/02 and 2007/08 (real 5 x 10). Lists for 2002/03, 2003/04, 2008/09 and 2009/10 (first-stage groups) and 2010/11 to 2025/26 (White and Red groups).

Seasons without a database (2004/05, 2006/07, 2026/27, 2027/28) have no list.

## Missing clubs (for Max)

No database record was found for these clubs. Each division below lacks this club after the moves.

| Season | Division | Club |
|---|---|---|
| 2005/06 | Russian First Division | Spartak Nizhny Novgorod |
| 2008/09 | Russian First Division | Krasnodar (founded 2008), Nizhny Novgorod |
| 2009/10 | Category A (TFF 1. Lig) | Hacettepe |
| 2010/11 | Category A (TFF 1. Lig) | TKİ Tavşanlı Linyitspor |
| 2023/24 | Category A (TFF 1. Lig) | Yeni Malatyaspor (October 2023 holds only its U19 side) |
| 2002/03 | Category B, Group C | Siirtspor |
| 2003/04 | Category B | Sidespor, Yeni Kırşehirspor, Osmaniyespor, Siirtspor |
| 2008/09 | Category B | Altınordu, Karsspor |
| 2009/10 | Category B | TKİ Tavşanlı Linyitspor, Pursaklarspor, Karsspor, İskenderun Demir Çelikspor |
| 2010/11 | Category B | Yeni Malatyaspor, Körfez İskenderun, Hacettepe, Pursaklarspor, 1461 Trabzon |
| 2011/12 | Category B | 1461 Trabzon |
| 2012/13 | Category B | Yeni Malatyaspor |
| 2015/16, 2016/17 | Category B | Tepecikspor |
| 2017/18 | Category B | Eyüpspor |
| 2018/19 | Category B | Başkent Akademi |
| 2021/22 | Category B | 1461 Trabzon |
| 2023/24 | Category B | Kastamonuspor 1966 |
| 2005/06 | Category B | all clubs: the reconcile found the 51 clubs on RSSSF tur06.html; they are not yet in rus-tur-clubs.tsv |

Some of these may exist under an old name. Possible matches that the script did not accept: Siirtspor = "Siirt JETPAspor", Konya Anadolu Selçukspor = "Anadolu Selçukluspor", Eyüpspor = "Eyüp Sultan Spor", İskenderun Demir Çelikspor or Körfez İskenderun = "Iskenderunspor" (probably a different club), TKİ Tavşanlı Linyitspor = "Tavsanli Linyitspor" (accepted in other seasons). The rule of Q244 (clubs with no division) was not applied here. Max decides.

## Matches to check

The script matched names by tokens and string likeness, then I checked the low scores by hand. Some matches rest on a club rename or on the stable club id across databases. The second agent should check these:

- Russia, by id or rename: SOYUZ-Gazprom Izhevsk = Gazovik-Gazprom Izhevsk (4321); Oryol = FC Orel (8326); Sibir Novosibirsk = Chkalovets-1936 Novosibirsk (2512, same id as Sibir later); Dynamo St. Petersburg 2010 = Petrotrest St. Petersburg (3132, the same id is "FC Dynamo Saint Petersburg" in September 2010); Yenisey Krasnoyarsk 2011 = Metallurg Krasnoyarsk (7299; September 2011 adds a new record 14063); Lisma-Mordovia = Biokhimik-Mordovia; Metallurg-Kuzbass = Metallurg-Zapsib; Ural Sverdlovsk Oblast = Uralmash or Ural Yekaterinburg; Salyut Belgorod = Salyut-Energia; Mashuk-KMV = Mashuk Pyatigorsk.
- Turkey: Van BB and Belediye Vanspor were matched to "Vanspor A.S." (2008/09 to 2010/11), probably a different club. Şekerspor 2012/13 = Çamlıdere Şekerspor; Çorum 2019/20 = Yeni Çorumspor; Fatih Karagümrük 2017/18 = Ottocool Karagümrük; Kastamonuspor 1966 2022/23 = GMG Kastamonuspor; Diyarbakır BB Diskispor = DISKIspor; Kayseri Erciyesspor = Erciyesspor; Gümüşhanespor = Gümüşhane Doğanspor; Kırşehirspor = Yeni Kırşehirspor; Afjet Afyonspor = Afyonspor.
- Sponsor names were accepted (for example Medical Park Antalyaspor, Vestel Manisaspor, Torku Konyaspor).

## Disagreements and weak facts

- **Turkish foreign limit 2004/05.** Medyascope gives "6" from 2004/05. The Goal Turkey gallery gives 2005/06 as the first season with 6.
- **Turkish foreign limit 2020/21.** A July 2020 TFF plan (aspor) set 14 registered and 8 on the pitch. Medyascope says the limit of 8 on the pitch began in 2021/22. Goal says the second half of 2020/21 allowed 16 in a 35-man list with no pitch limit.
- **Turkish 2026/27 age rule.** aspor (TFF statement) says at least 4 of 14 born on or after 1 January 2004 (the text is cut after "01."). The English 2026-27 Süper Lig page says U23, born 2003 or later.
- **TFF 1. Lig foreign limit 2022/23.** The Hürriyet text with "8 registered, 7 in the list, 5 on the pitch" follows the words "TFF 1."; that it applies to the 1. Lig is an inference.
- **Russian 7+4 from 2012.** Russian Wikipedia says the league returned to 7+4 "in 2012". Row 2012/13 as the first season is an inference.
- **TFF 2. Lig 2009/10.** The TFF 2. Lig page describes five groups of ten, but the season page has four groups (11, 11, 12, 11). The table uses the season page.
- **Süper Lig 2025/26 relegation.** The infobox lists three relegated clubs. The table template marks four places. The table uses three.
- **TFF 1. Lig 2010/11.** The infobox lists three relegated, the table marks two. The table uses three.

## Unknowns

- Bench size for Russia in every season. Bench size for Turkey before 2015/16 and after 2020/21.
- Substitutes for Russia in 2020/21 and from 2022/23. Substitutes for Turkey from 2020/21. The IFAB made five substitutes permanent as an option in 2022, so five is likely. No league text was found.
- FNL foreign limit between 2002 and 2019 (the 1999 rule of 5 is the last one found before 2020).
- Work permits for both nations.
- Exact transfer window dates (only the summer 2020 Russian window was found).
- Turkish foreign pitch limit for 2023/24 to 2025/26.
- Third-tier relegation counts in many Turkish seasons.
- Russian Cup entrants before 2022/23.
- All 2027/28 facts. No 2027-28 season page exists yet. Inference: the 2026/27 rules stay (Q248).

## Sources read

- English Wikipedia: Russian Premier League; Russian Premier League seasons 2002 to 2026-27; Russian First Division 2002 to 2010; Russian Football National League and Russian First League 2011-12 to 2026-27; Russian Cup (football) and its seasons 2001-02 to 2026-27; Russian Super Cup and its 2003, 2011 and 2012 editions; Russian Premier League Cup; Süper Lig and its seasons 2001-02 to 2026-27; TFF 1. Lig and its seasons; TFF 2. Lig and its seasons 2007-08 to 2016-17, 2022-23, 2023-24, 2025-26 and 2026-27; Turkish Cup and its seasons; Turkish Super Cup; Substitute (association football); Transfer window.
- Turkish Wikipedia: 2. Futbol Ligi B Kategorisi 2001-02 to 2004-05; 2006-07 Lig B; 2. Lig 2008-09, 2017-18 to 2021-22 and 2024-25.
- Russian Wikipedia: Чемпионат России по футболу (foreign limit section); Лимит на легионеров в футболе.
- News: sport-express.ru (23 March 2022, 24 May 2022, 4 December 2021, 2020 restart review); news.mail.ru (TASS, 2026 limit order); aspor.com.tr (8 July 2020, 22 July 2024, 21 May 2026); hurriyet.com.tr (18 June 2022); medyascope.tv (11 January 2024); goal.com Turkey foreign limit gallery; tgrthaber.com (2020 five substitutes).

## Reconcile

Reconcile date 2026-10-16. The full list is in `rus-tur.reconcile.tsv` (60 rows). I changed 65 fields in 46 rows of `rus-tur.tsv` and added the URL of each new value to `sources`. I changed 16 rows of `rus-tur-clubs.tsv`. Row order and ids are unchanged.

| Decision | Rows |
|---|---|
| second | 26 (plus 3 club-match rows) |
| first | 4 (plus 1 club-match row) |
| third (primary record gives a third value) | 1 |
| conflict | 2 |
| found value accepted | 20 (19 from the check file, 1 from the check notes: rus-tur-210) |
| found value rejected | 1 (rus-tur-242 down) |
| consequential change | 1 (rus-tur-242 up) |

How I decided:

- **Russian First Division `down` (rows 42, 43, 45, 47, 48, 51).** The brief counts places by the rules of the season. I read the FNL regulation of each season on 1fnl.ru. They give the last 5 places in 2015/16, 2016/17 and 2018/19, the last 6 places in 2020/21, the last 4 in 2021/22 and the last 3 in 2024/25. The first source counted the clubs that went down in effect. Those counts are now in `notes`.
- **Row 47 (2020/21).** The regulation (art. 3.3) relegates the last 6 of 22 clubs, 17th to 22nd. This agrees with neither source. RSSSF marks 18th to 22nd because Akron (17th) kept their place later. No source except the first names a relegation for match fixing.
- **Rows 38 and 40.** I accepted the found values with the withdrawals moved to `notes`: 4 places in 2011/12, 2 on the table in 2013/14. I did not find the 2013/14 regulation.
- **Row 1.** Lenta.ru confirms the 2002 golden match. The row now gives the tie rule and the match.
- **Row 46.** Transfermarkt shows 27 matches for every FNL club in 2019/20. There was no restart.
- **Rows 74 and 75.** Transfermarkt and RSSSF both have a round of 16 after the group stage. The group stage replaced the round of 32 only.
- **Row 139.** Sportbox and Sovetsky Sport give at most 5 foreigners on the pitch in 2005. I kept the first value and put the Sport-Express reading in `notes`.
- **Row 163.** The TFF history page says the league took the name Süper Lig in 2002/03. The 2001/02 name is now 1. Lig.
- **Row 173.** Hürriyet confirms the Spor Toto Cup of 2012. It had no effect on places. I added it to `split` with that remark.
- **Rows 221 to 225 (third tier, 2005/06 to 2009/10).** RSSSF numbers each classification table from 3, because places 1 and 2 of each first-stage group went to the promotion group. A "3rd-placed" classification club is the classification group winner. With this reading the first value is right for 2007/08 and 2008/09. For 2005/06 the play-off had promotion group 3rd to 8th and two clubs from each classification group. For 2009/10 it had promotion group 3rd to 6th. The TFF archive pages show the same play-off clubs.
- **Row 223 name.** The TFF 2. Lig archive lists "2. Lig B Kategorisi" champions up to 2006/07 and "TFF 2. Lig" champions from 2007/08.
- **Rows 238 and 239.** The TFF archive shows play-off round 1 with the 3rd to 6th of each group. Runners-up entered in round 3.
- **Row 242 (2026/27).** Sporbursa (12 July 2026) reports the TFF statute after Altınordu withdrew: 35 clubs (18 and 17), 2nd to 5th in the play-offs, one final per group, White 3 and Red 2 relegated. `up` is now "2 + 2" to match the two finals.
- **Rows 248 to 253.** The check cited the RSSSF pages of 2002/03 to 2007/08 for these rows. The rows are 2005/06 to 2010/11. I read the right RSSSF pages and the TFF cup archive for 2005/06, 2007/08 and 2010/11. All give two-legged quarter-finals.
- **Rows 298 and 323.** Habertürk gives 5+1+2 for 2001/02. Medyascope (21 May 2026) gives 1 January 2003 for the 2026/27 age rule.

### Conflicts

1. **rus-tur-196 `name` (TFF 1. Lig 2007/08).** The season page gives TFF 1. Lig, with the sponsor name Bank Asya 1. Lig from 16 January 2008. RSSSF heads the season "Bank Asya 2. Lig". The TFF archive files the season under its present 1. Lig name, so it does not settle the question. I found no other publisher. The first value stays. Max decides.
2. **rus-tur-301 `foreign` (Süper Lig 2004/05).** Medyascope and Habertürk give 6 foreigners from 2004/05. Goal and Türkiye Gazetesi give 6 from 2005/06, so 2004/05 stays 5+1+2. The first value stays, with both readings in `notes`. Max decides.

### Club matches

- **Gölbaşıspor, 2011/12 to 2013/14.** The TFF tables name the club Kızılcahamamspor A.Ş. in these seasons and Gölbaşıspor A.Ş. from 2014/15. The databases hold it as id 14267 (September 2011) and id 3847 (September 2012, Autumn 2013, and October 2014 as "Gölbasispor"). I matched the three rows.
- **Kocaeli Birlik Spor, 2010/11 to 2013/14.** The rows were matched to Kocaelispor (6061). The TFF tables name the club Körfez Spor or Körfez Futbol Kulübü. It took the name Kocaeli Birlik Spor in 2014/15 (id 5933). Kocaelispor is a different club and played in the same division in 2010/11 and 2011/12. I matched the four rows to 5933.
- **Körfez İskenderun Spor, 2010/11 to 2013/14.** The TFF name of these seasons is İskenderun Demir Çelikspor. I changed the club name only.
- **Konya Anadolu Selçukspor and Amed, 2012/13 to 2014/15.** The TFF tables name them Anadolu Selçukluspor and Diyarbakır Büyükşehir Belediyespor. The databases hold them as 4984 and 2883. I matched the five rows and took them off the missing-clubs table.
- **Dynamo St. Petersburg, 2009/10 (real 2010).** The real 2010 Dynamo and Petrotrest are different clubs. RSSSF shows that Petrotrest played in no professional zone in 2009. In September 2009 the database has id 3132 as its only St Petersburg record that fits, and September 2010 calls the same id "FC Dynamo Saint Petersburg". I kept the match. The September 2009 name "Petrotrest" is a database label.

### For Max (not changed)

- rus-tur-241 (TFF 2. Lig 2025/26) still says 2nd to 6th and one final. The TFF archive (pageID 1769) shows 8 clubs in play-off round 1 and two finals, and Transfermarkt marks 2nd to 5th. So `up` may be "2 + 2" and the format may be wrong. The check agreed with the row, so I did not change it.
- rus-tur-47 and rus-tur-51 `subs_used` are still `unknown`. The FNL regulations of 2020/21 (art. 10.24) and 2024/25 (art. 10.18) allow 5 substitutes.
- rus-tur-211 (TFF 1. Lig 2022/23 foreign limit). The TFF decision of 2 June 2021 (aspor) planned 5 registered and 4 on the pitch for 2022/23. The row has 8, 7 and 5 from a later decision text. I did not check it further.
- The 51 clubs of the 2005/06 Category B are on RSSSF tur06.html. They are not yet in `rus-tur-clubs.tsv`.

## Loose ends

- rus-tur-241 (TFF 2. Lig 2025/26): each group has its own play-off and final, so `up` is now "2 + 2". The White final (10 May 2026) went to Muğlaspor on penalties against Elazığspor (beIN Sports). The Red final went to Mardin 1969 against Muşspor (season page bracket). The bracket shows semi-finals of 2nd to 5th only, so the place of the 6th-placed clubs is unknown.
- FNL regulations, rows 47 (2020/21) and 51 (2024/25): I read both regulation PDFs. Neither gives a number of substitutes, so `subs_used` stays unknown.
- Turkish third tier 2005/06 (2. Lig B): the September 2005 database has 50 clubs in 5 groups of 10, the real league 51 clubs in groups of 11, 10, 10, 10, 10. That is a resize, so the 51 clubs are now in rus-tur-clubs.tsv (source RSSSF tur06). 7 have no database club: Alanyaspor, Yeni Kırşehirspor, Ünyespor, Giresunspor, Pazarspor, İskenderun Demir Çelik, Osmaniyespor. Matches to check: Anadolu Üsküdar (RSSSF "1980") matched to 42 "Anadolu Üsküdar 1908 Spor" (11908 "Üsküdar Anadolu" has no division); Gençlerbirliği ASAŞ matched to 148 "ASASspor"; Siirtspor to 10515 "Siirt JETPAspor". Adanaspor was expelled after 2 matches.

## Source fill

Method: I ran the rules lint and took each filled fact field with no `field=URL` entry. I read each page on 2026-10-16 with curl and checked the value against the page text or league table. For `split` = none and `format` = double round robin, the season page must show one table for the division with every club on 2 x (clubs - 1) matches, and no championship or relegation round. For a stopped season (2019/20, 2020/21), a table with fewer matches counts if the page says the season was stopped. I added sources only. I changed no value, row order or id.

- Sources added: 421 field entries in 268 rows.
- Russian Super Cup `clubs` = 2: https://en.wikipedia.org/wiki/Russian_Super_Cup ("Number of clubs 2", the champions and the cup winners). `name`: the match page of the real year (for example https://en.wikipedia.org/wiki/2019_Russian_Super_Cup).
- Second League Category A and B 2001/02 to 2004/05 `name`: the English pages give "Second League Category A" and "Category B" (https://en.wikipedia.org/wiki/TFF_2._Lig). The Turkish pages give "2. Futbol Ligi A Kategorisi" and "2. Futbol Ligi B Kategorisi". The row writes "2. Lig A Kategorisi".
- Süper Lig `playoffs` = none: the season pages name play-offs only for the TFF First League, for UEFA rounds and as the last tie-breaker.
- 2026/27 rows (rus-tur-26, 53, 188, 215): the season page shows one table, so it covers `split`.

No source found (added to `notes`):

- rus-tur-324 (2027/28 nation rules): `foreign` reads "Süper Lig: unknown". No page gives the 2027/28 quota.

Disagreements: none. Notes on the pages:

- 2005 Premier League (rus-tur-4): the page table gives Dynamo Moscow 20 matches and every other club 30. This looks like an error on the page.
- 2011/12 Süper Lig (rus-tur-173): the page calls the post-season groups "European play-offs". The row puts them in `split` and keeps `playoffs` = none.
- 2011–12 first stage (rus-tur-10, rus-tur-37): the pages put the relegation and promotion play-offs after the second stage. That agrees with `playoffs` = none for the first stage.

## Pass 2

Date 2026-10-17. Decision agent. Rules: the "Pass 2" section of the 0.6.0 research brief (kept outside the repo) and Q268 to Q331 of the spec. Row ids and row order are unchanged. `rules_lint.py` gives 1 problem for this part before and after (rus-tur-324 `foreign`, no 2027/28 source).

### 1. No new tiers and no new nations (Q331, Q288)

The original runs `rus_prm` and `rus_first` for Russia (lowest tier 2) and `tur_first`, `tur_second` and `tur_second_b` for Turkey (lowest tier 3). The part holds no row and no club row below these tiers, and no competition that needs another nation. Deleted: 0 rows, 0 club rows. No real tier of these competitions ended in 2001/02 to 2027/28, so no row changed to `not held`.

### 2. Conflicts and open points

Each point is settled by what happened in real life (Q272 to Q276, Q278, Q293).

| Row | Field | Decision | Source |
|---|---|---|---|
| rus-tur-196 | name | TFF 1. Lig (first value). The league was renamed before the 2007–08 season. Bank Asya 1. Lig is the sponsor name from 16 January 2008. | https://en.wikipedia.org/wiki/TFF_1._Lig and https://tr.wikipedia.org/wiki/1._Lig |
| rus-tur-301 | foreign | 5+1+2 (second value). Milliyet, Goal and Türkiye Gazetesi give the cut to at most 6 foreigners from the start of 2005/06. | https://www.milliyet.com.tr/turk-futbolunda-yabanci-kurali-ve-yurt-disina-transfer-olan-turk-futbolcular-molatik-11333/ |
| rus-tur-241 | playoffs, format | 2nd to 5th of each group, two-legged semi-finals (30 April and 4 May 2026), one final per group (Red 9 May, White 10 May). `up` stays "2 + 2" and `down` stays 7 (White 4, Red 3). | https://www.politikam.com/nesine-2-lig-play-off-son-takimlar-senaryolar-2026 and https://www.evrensel.net/haber/5985688/profesyonel-futbol-liglerinde-dusenler-ve-cikanlar-belli-oldu-dusen-ve-yukselen-takimlarin-sayisi-kac-hangi-illerin-liglerde-takimi-yok |
| rus-tur-211 | foreign | 8 registered, 7 in the list, 5 on the pitch. The June 2022 TFF text names the TFF 1. Lig, so this is no longer an inference. It replaced the plan of June 2021. | https://www.hurriyet.com.tr/sporarena/son-dakika-tffden-yabanci-kurali-aciklamasi-42086773 |
| rus-tur-317 | foreign | 14 in the squad and no pitch limit in 2020/21. The TFF postponed the plan of 8 on the pitch on 29 July 2020 for one year. | https://www.tff.org/Resources/Tamsaha/200/140-141/ |
| rus-tur-199 | down | 2 (16th and 17th). The league started with 17 clubs after the TFF dismissed Ankaraspor on 4 August 2010. | https://en.wikipedia.org/wiki/2010%E2%80%9311_TFF_1._Lig and https://www.rsssf.org/tablest/tur2011.html |
| rus-tur-187 | down | 3 (unchanged). The 2026 news names three relegated clubs. | Evrensel, as above |
| rus-tur-47, rus-tur-51 | subs_used | Stays `unknown`. I read both FNL regulations on 2026-10-17. Neither gives a number of substitutes. | the 1fnl.ru PDFs in the rows |
| rus-tur-162 | notes | UEFA decided the Russian suspension only up to 2026/27. No decision for 2027/28 is known. By Q293 Russia gets no places while the suspension lasts. | none (no decision published) |

Not changed: rus-tur-139 (the reconcile settled it), rus-tur-147 to 149 and rus-tur-305 (inferences that no source settles). I found no page that gives the first season of the Russian 7+4 rule or of the Turkish 6+2 rule.

### 3. Club moves (Q279, Q280, Q290)

I went down every tier for every season with a database. The pool for each move is clubs with no division, clubs in lower or spare divisions, and clubs of a higher division that the season lists in full.

- **Cascade lists added (290 club rows).** Some real clubs of a listed tier stood in a tier above that had no list. The older databases hold the clubs of the season before, so relegated clubs still stand in the top division. That tier then lost a club, so it now has a full list too:
  - Russian Premier League, 7 seasons: 2002/03 (real 2003), 2003/04 (2004), 2005/06 (2006), 2007/08 (2008), 2008/09 (2009), 2009/10 (2010), 2010/11 (2011–12). 16 clubs each.
  - Süper Lig, 6 seasons: 2002/03, 2003/04, 2005/06, 2007/08, 2008/09, 2009/10. 18 clubs each.
  - TFF 2. Lig 2007/08: 50 clubs in five first-stage groups. Category A took Sakaryaspor, Eskişehirspor and Boluspor from the November 2007 Category B. Group membership comes from the RSSSF round 1 fixtures (https://www.rsssf.org/tablest/tur08.html), the club names from the season page.
  - TFF 1. Lig 2024/25: 20 clubs. The Süper Lig list took Bodrum from the October 2024 Category A.
- **Wrong matches fixed (38 club rows).** The script matched some clubs to a club with a like name. Each fix gave one database club to two real clubs in a season:
  - Gaziantep BB, which is not Gaziantepspor (4240, five seasons).
  - İstanbul B.B., which is not İstanbulspor (2023).
  - B.B. Ankaraspor and Ankaraspor, which are not Ankara Demirspor (745, four seasons).
  - Kayseri Erciyesspor, which is not Kayserispor (3608).
  - Dynamo St. Petersburg, which is not Zenit-2 (3132).
  - Sivasspor, which is not Sivas Belediyespor (10573).
  - Manisaspor, which is not Manisa BB (7048).
  - Denizli Belediyespor, which is not Denizlispor. No record exists, so the row is now empty.
- **Missing clubs found in the pool (Q279).**
  - Siirtspor 2002/03 and 2003/04: 10515 "Siirt JETPAspor". Siirtspor was named Siirt Jetpa S.K. (https://en.wikipedia.org/wiki/Siirtspor).
  - Hacettepe 2009/10 and 2010/11: 2938.
  - 1461 Trabzon 2010/11, 2011/12 and 2021/22: 6345. The club was Değirmenderespor until 2008, then Trabzon Karadenizspor (https://en.wikipedia.org/wiki/1461_Trabzon_F.K.). Later databases call 6345 "1461 Trabzon".
  - Yeni Malatyaspor 2010/11 and 2012/13: 6991 "Malatya Belediyespor". The club was founded as Malatya Belediyespor (https://en.wikipedia.org/wiki/Yeni_Malatyaspor). Yeni Malatyaspor 2023/24 Category A: 6992 in the spare division.
  - Tepecikspor 2015/16 and 2016/17: 1995.
  - Eyüpspor 2017/18: 3747 "Eyüp Sultan Spor". The same id is "Eyüpspor" in later databases.
  - Kastamonuspor 1966 2023/24: 6438.
  - Alanyaspor and Giresunspor 2005/06: 522 and 4407.
- **Matches to check.** These rest on the id lineage or a database flag, not on the name:
  - TKİ Tavşanlı Linyitspor 2009/10 and 2010/11: 11399, which the databases of those years call "Tavsancilspor". From Autumn 2013 the same id is "TKI Tavsanli Linyitspor".
  - 522 and 4407 (September 2005) carry nation -2. 6345 (October 2021) and 6992 (October 2023) carry nation -1. The build must set the nation to Turkey, or Max decides.
  - Afyonkarahisarspor 2007/08 is matched to 270 "Afyonspor".
  - Tepecik Belediyespor 2007/08 is matched to 11275 "Tepecik Firatpenspor".
- After the fixes no database club serves two real clubs in a season. No listed tier takes a club from a tier without a list.

### 4. Missing clubs (for Max)

29 club rows still have no database record:

| Season | Division | Clubs |
|---|---|---|
| 2005/06 | Russian First Division | Spartak Nizhny Novgorod |
| 2008/09 | Russian First Division | Krasnodar, Nizhny Novgorod |
| 2003/04 | Category B | Sidespor, Yeni Kırşehirspor, Osmaniyespor |
| 2005/06 | Category B | Yeni Kırşehirspor, Ünyespor, Pazarspor, İskenderun Demir Çelik, Osmaniyespor |
| 2007/08 | Category B | Bozüyükspor, Bugsaşspor, Yeni Kırşehirspor, Araklıspor, Değirmenderespor, Pazarspor, Arsinspor, İskenderun Demir Çelikspor |
| 2008/09 | Category B | Denizli Belediyespor, Altınordu, Karsspor |
| 2009/10 | Category B | Denizli Belediyespor, Pursaklarspor, Karsspor, İskenderun Demir Çelikspor |
| 2010/11 | Category B | İskenderun Demir Çelikspor, Pursaklarspor |
| 2018/19 | Category B | Başkent Akademi |

Without a list in 2007/08, Category B would keep the 47 clubs that the November 2007 database leaves there after the moves up. With the list, 42 real clubs are found and 8 are missing.

### 5. Reserve and B teams (Q289)

The tiers of this part need three reserve teams: Zenit-2, Spartak-2 and Krasnodar-2 (FNL 2015/16 to 2022/23). Every season database that needs one holds it: 670, 10863 or 5596, and 6705. I checked the October 2022 First Division too. It has no list but holds Krasnodar-2. The Turkish leagues have no reserve teams. Reserve teams to add: 0.

### 6. Part rules

None of the part rules of the brief applies to Russia or Turkey.

## Second source, pass 2

Date 2026-10-17. Second-source agent. I took every line of `rus-tur.check.tsv` with the verdict `no second source` whose field still holds a value. That gives 409 lines. I read each second page on 2026-10-17, with curl and a text search of the page or PDF. The results are in `rus-tur.check2.tsv`. Row ids and row order are unchanged. `rules_lint.py` gives 1 problem for this part before and after (rus-tur-324 `foreign`).

| Verdict | Lines |
|---|---|
| agree | 313 |
| disagree, settled | 14 |
| disagree, still split | 2 |
| no second source | 80 |

### Main second sources

- IFAB Laws of the Game, one edition per season from 2001/02 to 2019/20 (downloads.theifab.com). Law 3 gives at most three substitutes in official competitions up to 2016/17. From 2017/18 the limit of three applies only to competitions with top-division first teams. Below the top division a national FA may allow up to five, so the Law does not confirm 3 for the second and third tiers in 2017/18 and 2018/19.
- FNL and First League regulations on 1fnl.ru and rfs.ru (2011-12, 2012-13, 2015-16, 2016-17, 2018-19, 2020-21 to 2026-27). The 2011-12 text is on nskhuman.ru.
- RPL regulations 2021-22 to 2026-27 and Russian Cup regulations on rfs.ru.
- RFS regulations on the status and transfer of players, one edition per season from May 2011 (rfs.ru, category 10).
- TFF statutes of the Süper Lig, TFF 1. Lig, TFF 2. Lig, Turkish Cup and Super Cup (tff.org, Statüler page), and the TFF league and cup archive pages.
- RSSSF season and cup pages; Sportbox and Sovetsky Sport histories of the Russian foreign limit; aspor (29 May 2020) for the TFF restart instruction.

### Disagreements settled

The second source is the rule text of the season. No other publisher states these numbers, so the rule text settles each one (decision `second`).

| Row | Field | First value | New value | Source |
|---|---|---|---|---|
| rus-tur-37, 38 | subs_used | 3 | 4 | FNL regulation 2011-12, art. 8.10 (http://nskhuman.ru/fbdocs/fnlregl.php?numstat=8) |
| rus-tur-39 | subs_used | 3 | 4 | FNL regulation 2012-13, art. 8.11 (1fnl.ru) |
| rus-tur-42 | subs_used | 3 | 4 | FNL regulation 2015-16, art. 8.20 (1fnl.ru) |
| rus-tur-43 | subs_used | 3 | 4 | FNL regulation 2016-17, art. 8.20 (1fnl.ru) |
| rus-tur-45 | subs_used | 3 | 4 | FNL regulation 2018-19, art. 8.20 (1fnl.ru) |
| rus-tur-47 to 53 | foreign | 4 registered, 3 on the pitch | 4 registered, at most 4 in the matchday list and on the pitch (from 2020); EAEU players not counted | FNL and First League regulations 2020-21 to 2026-27, art. 8.26 to 8.28 and 10.13 to 10.19 |
| rus-tur-269 | format | Süper Lig clubs enter in the fifth round | Süper Lig clubs enter in round 3 (6 clubs), round 4 (4 clubs placed 6th to 9th and 3 promoted clubs) and round 5 (cup holders and 4 European qualifiers). The rest of the value is unchanged. | TFF Turkish Cup statute 2026-27, art. 1 to 3 |

### Disagreements still split (for Max)

- rus-tur-156 (Russia 2021/22) `foreign` and `registration`. The row gives a 25-man squad with at least 17 Russians. The RPL regulation 2021-22 on rfs.ru (documentId 932) gives at most 28 players in form 3A, at most 8 legionnaires and at least 6 home-grown. That text includes the amendments of 10 March, 1 April and 4 April 2022. The start-of-season text of July 2021 is not online, so I cannot tell when 28 began. Sovetsky Sport gives 8 foreigners and no pitch limit but no squad size. The first value stays.

### Values found and added

These fields held `unknown`, or lacked a rule that the second source states. Each new value has its URL in `sources`.

- `foreign`, FNL, on-pitch limit added: rus-tur-37, 38 (2011-12 regulation) and rus-tur-39 (2012-13): at most 3 on the pitch. rus-tur-42, 43, 45 (2015-16, 2016-17, 2018-19): at most 5 in the matchday list and 3 on the pitch.
- `bench` 7: rus-tur-37, 38, 39, 42, 43 (FNL regulations), rus-tur-175 (Süper Lig 2013-14), rus-tur-204, 205, 207 (TFF 1. Lig 2015-16, 2016-17, 2018-19), rus-tur-232, 233, 234 (TFF 2. Lig 2016-17 to 2018-19).
- `bench` 10: rus-tur-183 to 188 (Süper Lig 2021-22 to 2026-27), rus-tur-206, 208 to 215 (TFF 1. Lig 2017-18, 2019-20 to 2026-27), rus-tur-236 to 242 (TFF 2. Lig 2020-21 to 2026-27).
- `bench` 12: rus-tur-21 to 26 (RPL 2021-22 to 2026-27), rus-tur-45, 47 to 53 (FNL and First League).
- `subs_used` 5 (in three windows): rus-tur-22 to 26 (RPL), rus-tur-49 to 53 (First League), rus-tur-182 to 188 (Süper Lig), rus-tur-209 to 215 (TFF 1. Lig), rus-tur-236 to 242 (TFF 2. Lig). rus-tur-47 (FNL 2020-21): 5; the regulation names no windows.
- `registration`, rus-tur-161 (Russia 2026/27): form 3A at most 25, at most 12 foreigners, at least 12 home-grown (RPL regulation 2026-27).
- `notes`, rus-tur-262 (Turkish Cup 2019/20): one more substitute in extra time of a knockout match after the June 2020 restart (aspor, 29 May 2020).

### rus-tur-324 `foreign` (Süper Lig 2027/28)

No source found. The TFF Süper Lig statute 2026-27 sets the foreign rule for 2026-27 only: 14 foreigners in the A list, at least 4 born on or after 1 January 2003. The 2025-26 statute names 2026-27 as its last step. No page gives a 2027/28 rule. The value stays `unknown`.

### Points for Max

- FNL substitutes before 2011. The FNL regulations of 2011-12 to 2018-19 allow 4 substitutes. No First Division regulation of 2002 to 2010 is online. Rows rus-tur-28 to 36 keep 3 on the IFAB Law alone. The rows of 2013/14, 2014/15 and 2017/18 (rus-tur-40, 41, 44) keep 3 with no second source, although the regulations on both sides give 4.
- FNL foreign limit 2019/20 (rus-tur-46). No regulation of 2019-20 found. The value stays 5 registered.
- Turkish second and third tiers 2017/18 and 2018/19 `subs_used` 3. The TFF statutes of those seasons give no number, and the IFAB Law allowed up to 5 below the top division. No second source.
- Russian transfer windows before 2011 and in 2012/13 and 2013/14, and all Turkish windows: no second source. The TFF transfer regulation gives no period lengths.
- Rows for 2027/28 (names, Super Cup, League Cup `not held`): no page of 2027/28 exists yet, except the Russian Cup regulation, which covers 2025-26 to 2028-29 (rus-tur-81).
- The web search budget of the session ran out during the work. The last lines rest on pages that I reached directly (RSSSF, rfs.ru, 1fnl.ru, tff.org). A later pass with search may find second sources for the 80 lines without one.

## Second source, retry

Date 2026-10-17. Second-source retry agent. The pass 2 agent ran out of web searches and left 80 lines of `rus-tur.check2.tsv` at `no second source`. I tried each of them again. I read archive.org copies of 1fnl.ru, rfpl.org and tff.org first, then RSSSF and tff.org pages, and used searches only for Turkish news on transfer periods. I updated each line in place. Row ids and row order are unchanged. `rules_lint.py` gives 1 problem for this part before and after (rus-tur-324 `foreign`).

| Verdict (the 80 retried lines) | Lines |
|---|---|
| agree | 38 |
| disagree, settled | 9 |
| disagree, still split | 0 |
| still no second source | 33 |

The whole file now holds 351 agree, 25 disagree and 33 no second source.

### New second sources

- RSSSF Russian cup history page (ruscuphist.html), Super Cup list, and RSSSF Russia 2010 (cup details).
- FNL regulations 2013-14, 2014-15 and 2017-18 (archive.org copies of 1fnl.ru) and 2019-20 (1fnl.ru).
- RFS status regulation as amended to 4 July 2014 (archive.org copy of 1fnl.ru). Art. 14 gives at most 12 weeks and at most 4 weeks.
- Premier League regulations 2003, 2005, 2006, 2009 and 2010 (archive.org copies of rfpl.org), and the RFS status regulations of 2003 and 2006 (rfpl.org copies).
- TFF Futbol Müsabaka Talimatı, August 2017 and 2018 texts, and the TFF instruction for the 2. Lig and 3. Lig play-offs of July 2020 (archive.org copies of tff.org).
- tff.org champions page (pageID 371) for the name 2. Lig B Kategorisi.
- Turkish newspapers with the TFF transfer period dates: Fanatik (2012), Türkiye (2016), A Spor (2017), beIN Sports (2018, 2024), Goal Türkiye (2019-20), Diken (2023), Milliyet (2023), CNN Türk (2025), BirGün (2026), Milli Gazete (Super Cup 2026).

### Disagreements settled

The second source is the rule text of the season in each case except rus-tur-138. No third publisher states these numbers, so the rule text settles each one (decision `second`).

| Row | Field | First value | New value | Source |
|---|---|---|---|---|
| rus-tur-40 | subs_used | 3 | 4 | FNL regulation 2013-14, art. 8.21 |
| rus-tur-41 | subs_used | 3 | 4 | FNL regulation 2014-15, art. 8.20 |
| rus-tur-44 | subs_used | 3 | 4 | FNL regulation 2017-18, art. 8.20 |
| rus-tur-137 | windows | between seasons (max 12 weeks), mid-season (max 4 weeks) | 25 February to 13 March and 1 July to 31 August 2003 | Premier League regulation 2003, art. 12 |
| rus-tur-138 | windows | as above | between seasons (dates unknown), mid-season 1 July to 31 August 2004 | Gazeta.ru, July 2004. No third publisher. The 2003 and 2005 regulations set the same summer period. |
| rus-tur-139 | windows | as above | 1 February to 10 March and 1 July to 31 August 2005 | Premier League regulation 2005, art. 11.3 and 11.4 |
| rus-tur-140 | windows | as above | 1 February to 15 March and 1 July to 31 August 2006 | Premier League regulation 2006, art. 11.3 and 11.4 |
| rus-tur-143 | windows | as above | 16 February to 12 March and 1 to 31 August 2009 | Premier League regulation 2009, art. 12.3 and 12.4 |
| rus-tur-144 | windows | as above | 8 February to 8 April and 1 to 31 August 2010 | Premier League regulation 2010, art. 12.2 and 12.3 |

The Russian summer period of 2003 to 2006 lasted about 9 weeks, and the August period of 2009 and 2010 lasted 31 days. Both are longer than the 4 weeks of the FIFA rule. The dates are those of the Premier League. The First Division dates may differ.

### Values found and added

- rus-tur-40, 41 `foreign`: at most 3 legionnaires on the pitch (FNL regulations 2013-14 and 2014-15). The registration count of 5 stays without a second source.
- rus-tur-44, 46 `foreign`: at most 5 legionnaires registered, at most 5 in the matchday list and 3 on the pitch (FNL regulations 2017-18 and 2019-20).
- `bench`: rus-tur-40 and 41 get 7, rus-tur-44 gets 11, rus-tur-46 gets 12 (same regulations).
- rus-tur-46 `subs_used`: 3 changed to 4 (a fifth when a young player replaces a young player), from the FNL regulation 2019-20, art. 8.20. This field was not one of the 80 lines. Its earlier check covered only the restart. I changed it because the regulation of the season states 4.

### Still no second source (33 lines)

- Names of 2027/28 (rus-tur-27, 54, 108, 135, 189, 216, 243, 270, 297) and rus-tur-135 `format` `not held`. No page of 2027/28 exists yet.
- FNL `foreign`, 5 registered, in 2010/11 to 2014/15 (rus-tur-37 to 41). The FNL regulations of those seasons give 3 on the pitch and no registration count. Sportbox confirms only the 1999 rule of 5.
- Russian windows of 2007 and 2008 (rus-tur-141, 142). The RFS status regulation of 18 December 2006 sets the first period from the last match of the season to the day before the new championship, with no 12-week cap stated. No dates found.
- Russian windows 2027/28 (rus-tur-162), Turkish windows 2027/28 and `foreign` 2027/28 (rus-tur-324).
- Turkish windows 2002/03 to 2011/12, 2013/14 to 2015/16 (rus-tur-299 to 308, 310 to 312). No page with both periods of those seasons found. For 2013/14 the second period was 7 January to 3 February 2014, and for 2015/16 the first period was 9 June to 31 August 2015.

### Points for Max

- The windows rows of Russia before 2011 now hold real Premier League dates where a regulation exists (2003, 2005, 2006, 2009, 2010). Rows 2004, 2007 and 2008 keep the FIFA rule or partial dates.
- FNL substitutes: the regulations of 2011-12 to 2019-20 all allow 4. The rows of 2013/14, 2014/15, 2017/18 and 2019/20 now say 4. The rows of 2002 to 2010 (rus-tur-28 to 36) keep 3 with no regulation found.
