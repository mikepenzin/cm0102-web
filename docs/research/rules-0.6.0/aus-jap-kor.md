# Rules table part aus-jap-kor: Australia, Japan, South Korea

Slice 8 of `docs/specs/2026-10-16-seasons-0.6.0-spec.md`. Research of 2026-10-16. Files:

- `aus-jap-kor.tsv`: 459 rows, 17 competitions by 27 seasons (2001/02 to 2027/28), with one `nation rules` row per nation per season.
- `aus-jap-kor-clubs.tsv`: 1,656 club rows for the divisions whose real size differs from the season database, or that the databases do not hold.

## Method

1. I read each competition's main Wikipedia article and the season page of every season. I fetched the pages as raw HTML with `curl` and read the table rows directly. The Wikipedia API refused the requests (HTTP 429), so I used the normal page URLs. The URLs in the table are the pages that I read.
2. I counted the clubs of each season from the league table on its season page. Where a page timeline and the table disagree, the table wins (J3 2018 and 2019, see below).
3. For rules that Wikipedia does not give (substitutes, bench, foreign players, squad registration), I read league press releases and news pages. Those URLs are in the `sources` column.
4. A script matched the real clubs to the clubs of each season database by name, with a list of old and new spellings (for example Chunnam Dragons for Jeonnam Dragons, Pusan I.cons for Busan IPark, Vent Foret Kofu for Ventforet Kofu). I checked every match that is not exact by hand and removed the wrong ones.

## Season mapping

- Australia plays October to May, so the row label is the real season.
- Japan and South Korea play calendar years. Following the brief, the 2001/02 row holds the year 2002, up to the 2024/25 row with 2025.
- Japan moves to autumn-spring. The 2025/26 row holds the one-off half-season of February to June 2026 (the J1 100 Year Vision League and the J2/J3 100 Year Vision League). The 2026/27 row holds the first autumn-spring season, August 2026 to May 2027.
- South Korea stays on calendar years: the 2025/26 row holds 2026, the 2026/27 row holds 2027 and the 2027/28 row holds 2028.
- The Australia Cup and the Australian Championship are calendar-year competitions. I put the edition of year Y in the Y/Y+1 row, because their proper rounds run from July to December, at the start of that A-League season. This is my decision against the letter of the brief's calendar rule, which Max can reverse.
- The Japanese cups follow the edition numbers. The 105th Emperor's Cup was 2025 and the 106th runs August 2026 to 1 January 2027, so the 2025/26 row has no Emperor's Cup. The J.League Cup has the same gap (33rd edition 2025, 34th edition 2026/27). Both "not held" rows are inferences from the numbering.

## Change points

### Australia

- NSL: 13 clubs in 2001/02, 2002/03 and 2003/04. Top six to the finals in all three seasons. In 2002/03 the top six played a home-and-away championship series, and the top two met in the grand final. The NSL folded after 2003/04. No national league in 2004/05.
- A-League from 2005/06, renamed A-League Men from 2021/22. Clubs: 8 (2005/06 to 2008/09), 10, 11 (2010/11), 10 (2011/12 to 2018/19), 11, 12 (2020/21 to 2023/24), 13 (2024/25), 12 (2025/26 and 2026/27). No promotion or relegation.
- Finals: top four with a page play-off to 2008/09. Top six from 2009/10, with three formats (double chance to 2011/12, three weeks without double chance to 2020/21, and two-legged semi-finals from 2021/22).
- Australia Cup (FFA Cup to 2021) from 2014. 32-club proper, single matches, extra time and penalties. The 2020 edition was cancelled.
- Australian Championship: a national second tier of 16 NPL clubs from October 2025, with groups of four and a knockout. It has no promotion to the A-League. The original has no code for it.

### Japan

- J1 clubs: 16 (2002 to 2004), 18 (2005 to 2020), 20 (2021), 18 (2022, 2023), 20 (2024 on).
- J1 stages: two stages to 2004, single table 2005 to 2014, two stages with a championship stage in 2015 and 2016, then a single table again.
- J1 relegation: 2, then 0 in 2004 (16th to a play-off), 2 plus a play-off (2005 to 2008), 3 (2009 to 2017), 2 plus the J1 entry play-off (2018, 2019, 2022), 0 (2020), 4 (2021), 1 (2023), 3 (2024 on).
- J2 clubs: 12 (to 2005), 13, 15, 18, 19, 20, 22 (2012 to 2023), 20 (2024 on). J2 promotion play-offs for 3rd to 6th from 2012.
- J3 from 2014. Clubs: 12, 13, 16, 17, 17, 18, 18 in 2020 (FC Tokyo U-23 withdrew before the first match), 15, 18, 20 (2023 on). Relegation to the JFL from 2023, only against JFL clubs with a J3 licence.
- Foreign players: 3 to 2008, 3 plus 1 AFC from 2009, partner nations exempt from 2017, and from 2019 no limit on registration with 5 per J1 matchday squad (4 in J2 and J3).
- Substitutes: 3 to 2019, 5 from 2020. Bench: 7 in 2024, 9 from 2025.
- Squad registration: no upper limit on professional players from the 2026 special season. The limit was 27 Pro A players in principle in 2025, and 25 A-contract players in 2008.

### South Korea

- K League: one division to 2012 with no relegation to 2011. Clubs: 10 (2002), 12, 13, 13, 14, 14, 14, 15, 15, 16, 16 (2012).
- Formats: single table (2002, 2003), two stages and a four-club championship (2004 to 2006), six-club championship play-off (2007 to 2011), split into top 8 and bottom 8 in 2012.
- From 2013: K League Classic (K League 1 from 2018) and K League Challenge (K League 2 from 2018). K League 1 has 14 clubs in 2013, 12 from 2014, and 14 from 2027.
- Promotion-relegation play-off from 2013. From 2022, two series: 10th against the K League 2 play-off winner, and 11th against the K League 2 runner-up.
- The League Cup ran in 2002, was not held in 2003, ran 2004 to 2011, and ended in 2012.
- The Super Cup ran 2002 to 2006, stopped in 2007 and came back in 2026.
- The Korean FA Cup became the Korea Cup in 2024. Its finals were two-legged from 2016 to 2022.
- Foreign players: from 3 on the pitch with 7 registered in 2002, down to 3 and 3 (2006 to 2008), then 3 plus 1 AFC (2009), plus 1 ASEAN (2020). No limit on registration from 2026, with a matchday limit of 5 in K League 1 and 4 in K League 2.
- Substitutes: 3 to 2020. From 2021, 5 if U-22 players are used. In K League 1 from 2026, 5 without that condition. Bench: 7 in 2023, 9 from 2024 in K League 1, and 9 from 2026 in K League 2.

## Inferences (marked in the table)

- NSL: no relegation in 2001/02 to 2003/04.
- A-League: 3 substitutes before the July 2020 change. A bench of 5 between 2013/14 and 2025/26 (the source dates are 2013 and 2025). 5 visa players between 2008 and 2025 (the sources date from 2008 to 2013 and from 2025).
- Australian Championship 2026/27 and 2027/28: same format as 2025.
- Japan: 3 foreign players before 2008. 5 substitutes in 2021 to 2023 (the sources give 2020 and 2024).
- Korea: the 2013 split of top 7 and bottom 7 (from 14 clubs and 38 matches). A K League 2 bench of 7 before 2025. The Super Cup in 2027 and 2028.
- Club name matches across a rename or a move: Gimcheon Sangmu to Sangju Sangmu, Sangju Sangmu Phoenix to Gwangju Sangmu Bulsajo, Jeju United to Bucheon SK (2005/06 database), FC Seoul to Anyang LG Cheetahs, Suwon FC to Suwon City, Reilac Shiga to MIO Biwako Shiga, Paju Frontier to Paju Citizen, Korean Police to National Police Agency.

## Unknowns

- Transfer windows and work permits in all three nations. No page that I read gave them by season.
- Substitutes and bench in the NSL and in the A-League after 2019/20 (substitutes) and before 2013/14 (bench source from 2013).
- Japan and Korea bench sizes before 2023 or 2024. The sources state only the size before each change.
- Substitutes and bench in the Emperor's Cup and the Korean FA Cup. These are FA competitions, so the league rules may not apply.
- Entrant counts of the Emperor's Cup (except 2005 to 2007 and 2026/27) and of the Korean FA Cup.
- The 2027/28 season in all three nations (2028 in Korea), the K League 2 size in 2027 ("15 or 16") and the 2027 Korean promotion and relegation rules. The KFA plans one combined system from 2027.
- The Japanese Super Cup for the autumn-spring seasons.
- The rules of foreign players in the NSL and in the first three A-League seasons.

## Club moves

The databases hold J1 with 16 clubs, J2 with 12, the NSL division with 13 and the Korean League with 10 in every season. No database holds J3, K League 2 or the Australian Championship. So every season start lists the real clubs of these divisions in the clubs file:

- J1 and J2: from 2005/06. They are equal to the database counts in 2001/02 to 2003/04.
- J3: from 2013/14.
- K League: from 2002/03.
- K League 2: from 2012/13.
- A-League: 2005/06 to 2023/24 and 2025/26. In 2024/25 the count is 13 and equal to the database.
- Australian Championship: 2025/26.

The 2001/02 to 2003/04 rows use the databases of those seasons (3.9.68 and October 2003). The 2004/05 and 2006/07 seasons have no database.

### Real clubs that no database club matches

| Season | Division | Clubs |
|---|---|---|
| 2002/03 | K League | Daegu FC |
| 2003/04 | K League | Daegu FC, Incheon United |
| 2005/06 | K League | Incheon United, Daegu FC, Gyeongnam FC |
| 2005/06 | A-League | Central Coast Mariners, Queensland Roar |
| 2005/06 | J2 League | Tokushima Vortis |
| 2007/08 | J2 League | Roasso Kumamoto |
| 2008/09 | K League | Gangwon FC |
| 2009/10 | K League | Gangwon FC |
| 2010/11 | K League | Gwangju FC |
| 2011/12 | J2 League | Matsumoto Yamaga |
| 2012/13 | K League 2 | FC Anyang, Goyang Hi FC |
| 2013/14 | J3 League | J.League U-22 Selection |
| 2013/14 | K League 2 | Ansan Police |
| 2014/15 | J3 League | J.League U-22 Selection |
| 2014/15 | K League 2 | Seoul E-Land, Ansan Police |
| 2015/16 | J3 League | Kagoshima United, Gamba Osaka U-23, FC Tokyo U-23, Cerezo Osaka U-23 |
| 2016/17 | J3 League | Kagoshima United, FC Tokyo U-23, Cerezo Osaka U-23, Gamba Osaka U-23 |
| 2016/17 | K League 2 | Asan Mugunghwa, Ansan Greeners |
| 2018/19 | J3 League | Cerezo Osaka U-23, FC Tokyo U-23, Gamba Osaka U-23 |
| 2019/20 | J3 League | Gamba Osaka U-23, Cerezo Osaka U-23 |
| 2019/20 | K League 2 | Chungnam Asan |
| 2025/26 | J2 League | Iwaki FC |
| 2025/26 | K League 2 | Yongin FC |

Max decides these cases. No club records were added.

### Question for Max on Q244

The rule takes clubs for a short division only from clubs with no division. In these databases, most real J2, J3, K League and K League 2 clubs sit in other divisions, not in "none":

- "A Premier Division" and "A Lower Division": spare divisions in which the Update Team parks real league clubs beyond the 16, 12 or 10 places.
- The Japanese Football League, the Japanese Administrative Division and the Korean Amateur League.
- The Australian state leagues, for the Australian Championship clubs.

The clubs file names the database division of each club in `db_division`. For example, 20 of the 20 J3 clubs of 2025/26 and 15 of the 17 K League 2 clubs sit in such divisions. Max decides whether a club move may take clubs from these divisions. If the answer is no, these divisions lack clubs in nearly every season.

## Code notes

- The original's foreign limits (Japan 3, South Korea 3, Australia 6) come from `2026-10-16-season-change-points.md`. The real values are in the `nation rules` rows.
- In 2002, J1 league matches still had golden-goal extra time with 3, 2 and 1 points (J2 dropped it in 2002 and J1 in 2003).
- The Japanese 2026 half-season uses regional groups and same-rank play-offs. Penalty shoot-outs settle draws, and no club goes up or down. The original has no code for this format.

## Reconcile

Date: 2026-10-16. File: `aus-jap-kor.reconcile.tsv` (39 rows). Third sources: the A-Leagues site, the J.League data site and press release, RSSSF (for the Emperor's Cup 2020 and the club renames), Dailian, Kyunghyang, The Fact, Sidae Ilbo, OSEN, Xportsnews and Kyeonggi Ilbo. For the Emperor's Cup 2002 and 2003 I found no third publisher. The JFA archive redirects and worldfootball.net refused the request. I read the full RSSSF match lists as the primary record.

| Decision | Rows |
|---|---|
| first | 1 (plus 5 club matches confirmed) |
| second | 17 |
| conflict | 1 |
| found accepted | 15 |

### Changes to aus-jap-kor.tsv

- All 16 disagreements go to the second value (rows 42, 43, 44, 136, 137, 154, 178, 181, 185, 321, 411, 423, 428, 429, 438, 439). The 2020/21 A-League bench (row 43) is an inference between the 2019 and 2022 rule texts.
- Emperor's Cup entrants: 80 in 2002 to 2004 and 88 in 2012 to 2019, 2022, 2023 and 2024. 2020 has 52 and 2021 has 90. The format text of rows 136 to 158 now gives the round in which each group enters. The check note on 2015 and 2016 was incomplete. In those years the J2 clubs and the two promoted J1 clubs played round 1, 12 J1 clubs entered in round 2 and the 4 ACL clubs in round 4. Rows 142 to 145 (2008 to 2011) still have no count.
- J.League Cup bench: 7 up to 2023 and 9 in 2024 (J.League press release). Row 184 is now 7. Rows 163 to 183 now say "unknown (7 in 2023)".
- Row 262 (J3 2020): clubs 18. The withdrawal of FC Tokyo U-23 on 5 June 2020, before any match, is in notes. The club is out of the 2019/20 J3 list in `aus-jap-kor-clubs.tsv`, which now has 18 rows for that season.
- Rule for `up`/`down`: row 335 keeps 2, now given as 15th and 16th, with Sangju Sangmu in notes. Row 343 changes from 2 to 1 (12th), with Sangju Sangmu in notes.

### Club matches

All five matches that had no second source are now confirmed: Jeju United and Bucheon SK, Suwon FC and Suwon City, Korean Police and National Police Agency, Paju Frontier and Paju Citizen, Reilac Shiga and MIO Biwako Shiga. No change to the clubs file was needed for them.

One finding for Max: database club 1962 is "Bucheon SK" in the 2002 to 2005 databases and "Bucheon FC 1995" in the later ones. Bucheon FC 1995 is a different club from Bucheon SK. The clubs file matches by the name in each database, so Jeju United uses 1962 only in 2005/06 and 9323 or 13040 later.

### Conflicts for Max

1. **aus-jap-kor-343, K League 1 2020, `down`.** First value 2 (12th and Sangju Sangmu), second check agreed. Under the brief default the value is 1, and Sangju Sangmu (4th) is in notes. RSSSF says Sangju "will be relegated regardless of its final position" because of its move, and the league fixed this before the season. If a drop fixed in advance counts as a rule of the season, the value is 2.

### Other findings

- Rows 45 to 47 (A-League 2022/23 to 2024/25) keep a bench of 5. The U-23 rule of 2019 to 2022 may still apply there. I did not check it.
- Row 430 (Korea Cup 2026) copies the 2024 text without the two-legged semi-finals. I did not check the 2026 format.

## Loose ends

- aus-jap-kor-45 to 47 (A-League Men 2022/23 to 2024/25): the competition regulations were not found. ESPN match rosters (about 40 sampled matches per season) show 7 substitutes named and up to 5 used per team, with two team-matches of 6 used (likely concussion substitutes). `bench` 7 and `subs_used` 5 are inferences from these data. The U-23 rule of October 2019 (6 or 7 substitutes when enough eligible U-23 players are named) is the rule source.
- aus-jap-kor-430 (Korea Cup, year 2026): the KFA changed the cup to a summer-to-spring calendar. The 2026–27 edition (63 clubs, K League 1 to K5, every tie a single match, 5 substitutes plus concussion substitutes) runs from 20 June 2026 to the final on 5 June 2027 (KFA, 21 May 2026). The row now holds it. Row 431 (year 2027) is labelled "2027" and probably holds the 2027–28 edition: Max decides the label.


## Source fill

Method: the lint of 2026-10-16 listed each filled fact field without a `field=URL` entry. For each one, I read the pages the row already cites today (curl, text search for the value). If no cited page states the value, I read the general page of the competition. I added `field=URL` to `sources` only where the page text holds the value. I changed no values.

Sources added: 49 fields (name 49).

- `name` = none on not-held rows cites the page that gives the dates of the competition: https://en.wikipedia.org/wiki/K_League_2 (founded 2013) for aus-jap-kor-352 to 362, https://en.wikipedia.org/wiki/Korean_League_Cup (abolished 2012, last edition 2011) for aus-jap-kor-443 to 459, and https://en.wikipedia.org/wiki/2026–27_J.League_Cup (previous edition 2025, next 2027-28) for aus-jap-kor-187.

Disagreements: none. Fields with no source: none.

## Pass 2

Date: 2026-10-17. Decision agent. Rules: the section "Pass 2" of the 0.6.0 research brief (kept outside the repo) and Q268 to Q331 of the spec. `rules_lint.py` gives 0 problems before and after.

### No new tiers (Q331, Q288)

The original runs one Australian league (`aus_nsl`), J1 and J2 (`jap_j1`, `jap_j2`) and one Korean league (`kor_league`). These are the lowest tiers. J3, K League 2 and the Australian Championship do not replace an existing tier, so they leave the game.

- `aus-jap-kor.tsv`: 81 rows deleted. They are rows 78 to 104 (Australian Championship), 244 to 270 (J3 League) and 352 to 378 (K League 2). 378 rows stay. The other ids do not change.
- `aus-jap-kor-clubs.tsv`: 398 club rows deleted (the J3 League, K League 2 and Australian Championship rows). 1,258 club rows stay.
- No competition of this part needs a nation that a game cannot select.
- The check and reconcile files still name the deleted ids. I did not change them.

Q272 keeps the real rule places. The game has no tier below J2 or the K League. So I added a note to each J2 row with relegation (227 to 234, 236 to 240, 242) and to each K League row from 2013 (336 to 350). The note says that no club goes down in the game. Row 241 (J2 2025/26) now says that the build decides how the 20 J3 clubs take part in the shared half-season.

Notes for the build: the J.League Cup from 2024 and the Emperor's Cup take J3 clubs, the Korea Cup takes K League 2 and K3 to K5 clubs, and the Australia Cup takes state league clubs. These clubs have no division in the game.

### Conflicts and open points

1. **Row 343, K League 1 2020, `down`.** Settled on 1 (12th). In real life Busan IPark (12th) and Sangju Sangmu (4th) went down. Sangju went down because the club moved to Gimcheon, a case that the game cannot copy (Q272). The value keeps the place by the rules, and the notes keep Sangju. Source: https://www.rsssf.org/tabless/skor2020.html. With no K League 2 in the game, no club goes down in any case.
2. **Row 431, Korea Cup label.** The cup runs summer to spring from the 2026–27 edition, which starts on 20 June 2026 (row 430). By Q276 the 2027–28 edition goes in the 2026/27 row. Row 431 now has `real_season` "2027–28 (inference)" and row 432 "2028–29 (inference)". Source: https://en.wikipedia.org/wiki/2026%E2%80%9327_Korea_Cup.
3. **Australia Cup in the Y/Y+1 row.** Q275 confirms it. No change.
4. **Q244, the pool of club moves.** Q279 says yes. The pass 1 matches already took clubs from the spare and lower divisions ("A Premier Division", "A Lower Division", the JFL, the Japanese Administrative Division, the Korean Amateur League). No change.
5. **Rows 45 to 47, A-League bench.** The loose ends section settles them by ESPN match rosters. No change.
6. **Emperor's Cup and J.League Cup in 2025/26.** They stay `not held`. The 105th Emperor's Cup ended in 2025 and the 106th starts in August 2026 (row of 2026/27).

### Club moves (Q279, Q280, Q290)

I went down J1 and J2, the K League and the A-League in each season with a database. The tiers whose real count differs from the database count already list every real club. No club moved up leaves a gap above the lowest tier, because each tier that a club leaves is already listed. No database division of these nations is a real tier that ended (Q290).

I searched each season database again for the 15 real clubs that had no match. Two are the same club under an older name:

| Season | Club | Database club | Source |
|---|---|---|---|
| 2005/06 | Tokushima Vortis | 8406 Otsuka FC (Japanese Football League) | https://en.wikipedia.org/wiki/Tokushima_Vortis |
| 2007/08 | Roasso Kumamoto | 7705 NTT Kumamoto FC (Japanese Football League) | https://en.wikipedia.org/wiki/Roasso_Kumamoto |

Both pages give the old name as the founding name of the club. The clubs file now holds both matches.

These 13 real clubs still have no record in the database of that season. Max decides them:

| Season | Division | Clubs |
|---|---|---|
| 2002/03 | K League | Daegu FC |
| 2003/04 | K League | Daegu FC, Incheon United |
| 2005/06 | K League | Incheon United, Daegu FC, Gyeongnam FC |
| 2005/06 | A-League | Central Coast Mariners, Queensland Roar |
| 2008/09 | K League | Gangwon FC |
| 2009/10 | K League | Gangwon FC |
| 2010/11 | K League | Gwangju FC |
| 2011/12 | J2 League | Matsumoto Yamaga |
| 2025/26 | J2 League | Iwaki FC |

Gwangju Sangmu Bulsajo (10106) is a different club from Gwangju FC. Iwaki Furukawa FC (7364) is a different club from Iwaki FC.

### Reserve and B teams (Q289)

None. J1, J2, the K League and the A-League have no reserve or B teams in any season. The U-23 sides and the J.League U-22 Selection played only in J3, which leaves the game.

## Second source, pass 2

Date: 2026-10-17. Second-source agent. File: `aus-jap-kor.check2.tsv` (208 lines). I took each check line with verdict `no second source` whose row still exists and whose field holds a value that is not `unknown` and not marked as an inference. 93 lines name rows that pass 2 deleted, and I skipped them. `rules_lint.py` gives 0 problems before and after.

| Verdict | Lines |
|---|---|
| agree | 115 |
| disagree, settled | 6 |
| disagree, still split | 1 |
| no second source | 86 |

The session web search limit stopped the search part of the work before the K League, K League Super Cup, Korean League Cup and K League foreign-player lines. These 61 lines, and some others, have `no second source` for that reason. A later pass can take them again.

### Changes to aus-jap-kor.tsv

- aus-jap-kor-8 (Australia 2008/09), `foreign`: 5 changes to 4. The Roar (26 Oct 2010) says the quota was four in seasons 1 to 4 and five from 2009/10. The PFA table of 9 Dec 2008 (https://pfa.net.au/?p=4365) gives 4 for 2008/09 and 5 for 2009/10. The notes of rows 9 to 25 still say "5-visa rule cited from the 2008-2013 collective agreement". That statement is wrong for 2008/09.
- aus-jap-kor-25 (Australia 2025/26), `registration`: "20 to 26 players" changes to "18 to 23 players". The A-Leagues site quotes the APL regulations (26 Aug 2025 and 17 Sep 2025).
- aus-jap-kor-48 (A-League Men 2025/26), `bench`: 5 changes to 7. Sydney FC (16 Oct 2025) says the squad is "trimmed to 18 on match day". ESPN rosters of three 2025/26 matches show 7 substitutes for each team.
- aus-jap-kor-139 (Emperor's Cup 2005), `format`: 12 J2 clubs and 2 JFL seeds in round 3, not 13 and 1 (RSSSF match lists. J2 had 12 clubs in 2005).
- aus-jap-kor-142 (Emperor's Cup 2008), `format`: 82 clubs. 15 J2 clubs and 1 JFL seed play from round 3, and the university champion and 47 prefectural winners play from round 1 (RSSSF match lists).
- aus-jap-kor-159 (Emperor's Cup 2025), `format`: 6 J2 clubs played in round 1, and 19 J1 and 14 J2 clubs entered in round 2. One J1 club entered later (an inference from the round counts). The JFA rules give 88 clubs and extra time then penalties. The notes now give the final date, 22 November 2025.

### Still split

- aus-jap-kor-74 (Australia Cup 2024/25), `format`: the first source gives 759 entrants. Football Australia gives 758. I read no third publisher because the Football NSW copy refused the request. The value stays, and the notes give both counts.

### Other findings for Max

- Rows 5 to 7 (Australia 2005/06 to 2007/08) hold `unknown` for `foreign`. The Roar (2010) and the PFA table (2008) give four visa players for those seasons.
- Rows 21 to 24: the A-Leagues site (16 Jan 2022) gives a squad of 18 to 23 players in 2021/22. The `registration` fields of those rows say `unknown`.
- Row 295 (Japanese Super Cup 2026): I found no 2026 edition on the J.League site. Max must check whether a 2026 edition took place.
- Row 380 (K League Super Cup 2003): RSSSF holds the 2002 and 2004 editions but no 2003 edition. Max must check whether a 2003 edition took place.
- The Japanese Super Cup rows use the generic name. The sponsor name was FUJI XEROX SUPER CUP up to 2021 and FUJIFILM SUPER CUP from 2022. I gave these lines `agree`.

## Closing

Date: 2026-10-17. Closing agent. `rules_lint.py`: 0 problems before and after. Every page below was read on 2026-10-17.

- Rows 5 to 7 (Australia 2005/06 to 2007/08), `foreign`: unknown changed to "max 4 foreign (visa) players per squad". The Roar (27 October 2010, https://web.archive.org/web/2020/https://www.theroar.com.au/2010/10/27/arrivederci-a-league-imports/) says each squad could sign up to 4 imported players in seasons 1 to 4, and 5 from 2009/10. The PFA table of 9 December 2008 (https://pfa.net.au/?p=4365) gives 4 for 2008/09. The live Roar page answers 403, so I read the archive copy.
- Row 295 (Japanese Super Cup 2026): `format` changed to `not held`, and `subs_used` and `bench` to n/a. The sponsor's page (https://www.fujifilm.com/fb/company/event/ffsc) says the 2025 match was the last, because of the move to an autumn-spring season. Soccer King (30 January 2025) quotes the J.League chairman the same way. Wikipedia lists a 2026 edition (Kashima Antlers v Machida Zelvia) with the date TBA, and no match took place to 17 October 2026. If it is played later, it goes in this row (Q277). The old note "the 2026 edition opened the special half-season" was wrong and is replaced.
- Row 380 (K League Super Cup 2003): `format` changed to `not held`, and `subs_used` and `bench` to n/a. On 21 February 2003 the league postponed the match of 16 March (Seongnam Ilhwa Chunma v Suwon Samsung Bluewings) without a new date, and it never took place (Yonhap via Daum, https://v.daum.net/v/M0IPVbK9gD, and https://en.wikipedia.org/wiki/K_League_Super_Cup, where the match shows as cancelled).

## Second source, retry

Date: 2026-10-17. Second-source retry agent. I took the 86 lines of `aus-jap-kor.check2.tsv` with verdict `no second source` again, with the 62 Korean lines first. I read Korean and Japanese press pages directly (Nocut News, Newsis, Goal Korea, Ilgan Sports, Dailian, Sports Kyunghyang, Xportsnews, Jeonbuk Ilbo, The Fact, Money Today, Korea Times, Soccer King) and used 16 web searches. `rules_lint.py` gives 0 problems for this part before and after.

| Result | All lines | Korean lines |
|---|---|---|
| agree | 42 | 36 |
| disagree, settled | 1 | 1 |
| disagree, still split | 0 | 0 |
| still no second source | 43 | 25 |

The file now holds 157 `agree`, 8 `disagree` and 43 `no second source` lines.

### Changes to aus-jap-kor.tsv

- aus-jap-kor-403 (K League Super Cup 2026), `format`: "extra time and penalties" changes to "no extra time, a draw after 90 minutes goes to penalties". Goal Korea, Nocut News (19 Jan 2026) and Jeonbuk Ilbo (19 Jan 2026) all state it. Decision: second. Rows 404 and 405 copy the format, so they change the same way. The `sources` of the three rows add `format=` with the Goal Korea and Nocut News URLs.

### Notes on the verdicts

- K League 1 `subs_used` 3 for 2002 to 2019 (rows 325 to 342): Nocut News (23 Feb 2024) says substitutes went from 3 to 5 in 2021. It gives no start year, and the first source makes the same kind of statement.
- K League Super Cup 2003 (row 380): Xportsnews (2 Mar 2005) lists every Super Cup to 2005 with no 2003 match, and Goal Korea (2026) counts seven editions from 1999 to 2006. The `not held` value now has a second source.
- J1 foreign rule 2021 to 2025 (rows 128 to 132): the Korea Times (21 May 2025) confirms the J1 part only. The J2 and J3 limit and the partner-nation exemption rest on the first source.
- Still without a second source: the Korean foreign limits of 2002 to 2004 (rows 298 to 300), the substitutes of the Korean Super Cup (rows 379, 381 to 383) and League Cup (rows 433, 435 to 442), the League Cup `not held` rows 457 to 459, and the names of future seasons. Korean pages state the K League limit of 3 before 2021, not the rule of its cups.

## Pass 3 (Q337)

Q337 (2026-10-17): a real play-off against a tier that is not in the game is played against a club with no division, as for the Scottish pyramid play-off (Q284). The rows below keep the real format in `playoffs`. Each row gets this note: "Q337: the lower-tier side is a club with no division in the game." No other field changed.

Rows: 336 to 342 and 344 to 349 (K League 1 against K League 2 clubs). The pass 2 note said that no club plays the promotion-relegation play-off in the game. It now says that no club goes down directly, and Q337 gives the play-off.

Not changed: 241 (J2/J3 100 Year Vision League, 2025/26). Its play-off sets the final order and is not for a place, so Q337 does not apply. Its pass 2 note that the build decides how the J3 clubs take part stays. No J2 row has a play-off against J3 in `playoffs` or `notes`.
