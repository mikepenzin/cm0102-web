# Season database table (slice 3)

Date 2026-10-16. Slice 3 of `docs/specs/2026-10-16-seasons-0.6.0-spec.md` (Q234, Q250, Q253, Q256, Q263, Q264). Brief: `docs/research/research-brief-0.6.0.md`. For Max to approve.

## Result

The final table is the same as the starting table in the spec. No forum post reports a content fault in any season database. The September 2011 ODB database stays for 2011/12 (see "2011/12 check").

| Season | Season database | Style | Topic | Versions on the tile |
|---|---|---|---|---|
| 2001/02 | 3.9.68 (base game) | n/a | [t=66](https://champman0102.co.uk/viewtopic.php?t=66) | 3.9.60 [t=65](https://champman0102.co.uk/viewtopic.php?t=65), 3.9.65 [t=276](https://champman0102.co.uk/viewtopic.php?t=276) |
| 2002/03 | Summer 2002 (our own, Summer 2002 spec) | n/a | none | none |
| 2003/04 | October 2003 | n/a | [t=266](https://champman0102.co.uk/viewtopic.php?t=266) | none |
| 2004/05 | Summer 2004 (our own, Q351) | n/a | none | none |
| 2005/06 | September 2005 | n/a | [t=38](https://champman0102.co.uk/viewtopic.php?t=38) | none |
| 2006/07 | Summer 2006 (our own, Q351) | n/a | none | none |
| 2007/08 | November 2007 | n/a | [t=39](https://champman0102.co.uk/viewtopic.php?t=39) | February 2008 |
| 2008/09 | September 2008 | n/a | [t=41](https://champman0102.co.uk/viewtopic.php?t=41) | December 2008, March 2009 |
| 2009/10 | September 2009 | n/a | [t=43](https://champman0102.co.uk/viewtopic.php?t=43) | February 2010 ODB, April 2010 SIM, June 2010 |
| 2010/11 | September 2010 ODB | ODB | [t=47](https://champman0102.co.uk/viewtopic.php?t=47) | December 2010 SIM, February 2011 ODB, April 2011 SIM |
| 2011/12 | September 2011 ODB | ODB | [t=49](https://champman0102.co.uk/viewtopic.php?t=49) | March 2012 ODB, March 2012 SIM |
| 2012/13 | September 2012 | n/a | [t=52](https://champman0102.co.uk/viewtopic.php?t=52) | March 2013 |
| 2013/14 | Autumn 2013 | n/a | [t=53](https://champman0102.co.uk/viewtopic.php?t=53) | March 2014 |
| 2014/15 | October 2014 | n/a | [t=55](https://champman0102.co.uk/viewtopic.php?t=55) | April 2015 |
| 2015/16 | October 2015 | n/a | [t=57](https://champman0102.co.uk/viewtopic.php?t=57) | April 2016 |
| 2016/17 | October 2016 | n/a | [t=58](https://champman0102.co.uk/viewtopic.php?t=58) | April 2017 |
| 2017/18 | October 2017 | n/a | [t=60](https://champman0102.co.uk/viewtopic.php?t=60) | March 2018 |
| 2018/19 | October 2018 | n/a | [t=61](https://champman0102.co.uk/viewtopic.php?t=61) | March 2019 |
| 2019/20 | October 2019 | n/a | [t=63](https://champman0102.co.uk/viewtopic.php?t=63) | March 2020 |
| 2020/21 | November 2020 | n/a | [t=1381](https://champman0102.co.uk/viewtopic.php?t=1381) | April 2021 |
| 2021/22 | October 2021 | n/a | [t=3478](https://champman0102.co.uk/viewtopic.php?t=3478) | April 2022 |
| 2022/23 | October 2022 | n/a | [t=4442](https://champman0102.co.uk/viewtopic.php?t=4442) | April 2023 |
| 2023/24 | October 2023 | n/a | [t=5580](https://champman0102.co.uk/viewtopic.php?t=5580) | April 2024 |
| 2024/25 | October 2024 | n/a | [t=6452](https://champman0102.co.uk/viewtopic.php?t=6452) | none |
| 2025/26 | December 2025 | n/a | [t=10207](https://champman0102.co.uk/viewtopic.php?t=10207) | May 2026 |

This gives 21 season databases, as the spec says. Summer 2004 and Summer 2006 (2026-10-17, Q351) make 23: our own 2004/05 and 2006/07 databases, made outside the repo from Football Manager 2005 and 2007 facts with the ratings of September 2005 and November 2007 (tools/seasons/own_db). Summer 2004 stores its dates the Update Team way (Q381), so its date-shift row has the shifts of a 2004/05 Update Team database. Summer 2006 was made from Football Manager 2007 facts with the November 2007 ratings and positions. It also stores its dates the Update Team way, 5 years early, so its date-shift row is 5 for birth, joined, contract and careers and 0 for the rest. The release script takes the source zips with `--own-dbs`. The 2006/07 rows of the club tables (`rules-0.6.0/<part>-clubs.tsv`) hold the real clubs of each division, so that slice 18 can give each division its real size: the 3.9.68 code holds Serie A and Ligue 1 at 18, for example. Summer 2002 (2026-10-24, spec `docs/specs/2026-10-24-summer-2002-db-spec.md`) makes 24: the 3.9.68 data with the facts of SI's Xbox data of Championship Manager: Season 02/03. Its dates are real dates (Q12), and its contract ends keep the 3.9.68 years (the Xbox data has the same contract ends as 3.9.68 for 72,580 of 72,593 people at the same club), so its date-shift row is the 3.9.68 rule of the 2002/03 tile: contract +1 and 0 for the rest. Games of 2002/03 saved on the plain 3.9.68 data still load. The Empty database ([t=275](https://champman0102.co.uk/viewtopic.php?t=275)) has no season. It holds no clubs and cannot start a game.

## Differences from the starting table

There is no difference in the choice of database. These points need Max's attention:

1. 2002/03 does not follow the rule of Q263. No database is dated August 2002 to June 2003. The 3.9.68 data comes from Q233.
2. October 2003 and September 2005 are not Update Team databases. Each topic names one person as the creator ([t=266](https://champman0102.co.uk/viewtopic.php?t=266), [t=38](https://champman0102.co.uk/viewtopic.php?t=38)). The September 2005 topic says that it first came out on another site. The credit of Q147 ("the Update Team for every database") is wrong for these two seasons. Max decides the credit text. Rule 2 does not let us name the creators.
3. Autumn 2013: the topic gives no month ([t=53](https://champman0102.co.uk/viewtopic.php?t=53)). The month is unknown. It is the only 2013/14 database dated before March 2014, so the rule picks it.
4. The topic texts of October 2021 and October 2022 say that they update to 2020/21 and 2021/22 ([t=3478](https://champman0102.co.uk/viewtopic.php?t=3478), [t=4442](https://champman0102.co.uk/viewtopic.php?t=4442)). The data shows the summer transfers of 2021 and 2022 (the files put a 2021 PSG signing at PSG and a 2022 Manchester City signing at Manchester City). Inference: the season in the text is a copy slip. The table keeps both.

## How the forum was read

- The Databases forum (f=73) has 49 topics on two index pages ([start=0](https://champman0102.co.uk/viewforum.php?f=73&start=0), [start=25](https://champman0102.co.uk/viewforum.php?f=73&start=25)). The page start=50 is the same as start=25.
- Every topic is locked. No topic has a second page.
- 38 topics have three posts, all by the uploader: the description, the install steps and a link to the support thread. 10 topics (April 2021 and later, except November 2020) have one post. November 2020 has three posts by two team accounts, with the same three texts.
- Result: the 49 topics hold no reply from a player, so they hold no fault report. The third post of each old topic sends reports to the support thread "Installing a database FAQ" ([t=274](https://champman0102.co.uk/viewtopic.php?t=274)).
- I read all 30 pages of t=274 (733 posts, 2020-05-04 to 2026-09-12) and searched them for each database name with words such as error, crash, missing, wrong, duplicate and fix. The forum search is closed to guests.
- The raw pages are in `/Users/max/Projects/cm0102/research-0.6.0/forum/` (outside the repo).

## Faults per database

Release month is the month in the topic title. "Topic date" is the date of the first post. Topics 38 to 63 and 265 to 276 are reposts from the move of the site in February and May 2020, so their topic date is not the release date. The real release date of those databases is unknown beyond the month in the title.

| Database | Style | Topic date | Faults reported | Fix posted |
|---|---|---|---|---|
| 3.9.60 | n/a | 2020-02-06 ([t=65](https://champman0102.co.uk/viewtopic.php?t=65)) | The topic says one club is missing from the Swedish Second Division, so a game with all leagues fails | yes: the topic file is the fixed version, and our file is that one |
| 3.9.65 | n/a | 2020-05-05 ([t=276](https://champman0102.co.uk/viewtopic.php?t=276)) | The topic says one club too many is in the Swedish Second Division | yes: the topic file is the fixed version, and our file is that one |
| 3.9.68 | n/a | 2020-02-06 ([t=66](https://champman0102.co.uk/viewtopic.php?t=66)) | none | n/a |
| October 2003 | n/a | 2020-05-01 ([t=266](https://champman0102.co.uk/viewtopic.php?t=266)) | none | n/a |
| September 2005 | n/a | 2020-02-06 ([t=38](https://champman0102.co.uk/viewtopic.php?t=38)) | none | n/a |
| November 2007 | n/a | 2020-02-06 ([t=39](https://champman0102.co.uk/viewtopic.php?t=39)) | none | n/a |
| February 2008 | n/a | 2020-02-06 ([t=40](https://champman0102.co.uk/viewtopic.php?t=40)) | none | n/a |
| September 2008 | n/a | 2020-02-06 ([t=41](https://champman0102.co.uk/viewtopic.php?t=41)) | One player reports a dead download link in 2022 ([p=66865](https://champman0102.co.uk/viewtopic.php?p=66865#p66865)). No content fault. We hold the file. | unknown |
| December 2008 | n/a | 2020-05-01 ([t=270](https://champman0102.co.uk/viewtopic.php?t=270)) | none | n/a |
| March 2009 | n/a | 2020-02-06 ([t=42](https://champman0102.co.uk/viewtopic.php?t=42)) | none | n/a |
| September 2009 | n/a | 2020-02-06 ([t=43](https://champman0102.co.uk/viewtopic.php?t=43)) | none | n/a |
| February 2010 ODB | ODB | 2020-02-06 ([t=44](https://champman0102.co.uk/viewtopic.php?t=44)) | none | n/a |
| April 2010 SIM | SIM | 2020-05-01 ([t=273](https://champman0102.co.uk/viewtopic.php?t=273)) | none | n/a |
| June 2010 | n/a | 2020-02-06 ([t=46](https://champman0102.co.uk/viewtopic.php?t=46)) | none | n/a |
| September 2010 ODB | ODB | 2020-02-06 ([t=47](https://champman0102.co.uk/viewtopic.php?t=47)) | none | n/a |
| December 2010 SIM | SIM | 2020-02-06 ([t=45](https://champman0102.co.uk/viewtopic.php?t=45)) | none | n/a |
| February 2011 ODB | ODB | 2020-02-06 ([t=48](https://champman0102.co.uk/viewtopic.php?t=48)) | none | n/a |
| April 2011 SIM | SIM | 2020-05-01 ([t=272](https://champman0102.co.uk/viewtopic.php?t=272)) | none | n/a |
| September 2011 ODB | ODB | 2020-02-06 ([t=49](https://champman0102.co.uk/viewtopic.php?t=49)) | none found (see "2011/12 check") | n/a |
| March 2012 ODB | ODB | 2020-02-06 ([t=50](https://champman0102.co.uk/viewtopic.php?t=50)) | none | n/a |
| March 2012 SIM | SIM | 2020-02-06 ([t=51](https://champman0102.co.uk/viewtopic.php?t=51)) | The topic itself says that some leagues other than England, Germany and Brazil lack transfers | no |
| September 2012 | n/a | 2020-02-06 ([t=52](https://champman0102.co.uk/viewtopic.php?t=52)) | none | n/a |
| March 2013 | n/a | 2020-05-01 ([t=269](https://champman0102.co.uk/viewtopic.php?t=269)) | none | n/a |
| Autumn 2013 | n/a | 2020-02-06 ([t=53](https://champman0102.co.uk/viewtopic.php?t=53)) | none | n/a |
| March 2014 | n/a | 2020-02-06 ([t=54](https://champman0102.co.uk/viewtopic.php?t=54)) | none | n/a |
| October 2014 | n/a | 2020-02-06 ([t=55](https://champman0102.co.uk/viewtopic.php?t=55)) | none | n/a |
| April 2015 | n/a | 2020-02-06 ([t=56](https://champman0102.co.uk/viewtopic.php?t=56)) | none | n/a |
| October 2015 | n/a | 2020-02-06 ([t=57](https://champman0102.co.uk/viewtopic.php?t=57)) | none | n/a |
| April 2016 | n/a | 2020-05-01 ([t=271](https://champman0102.co.uk/viewtopic.php?t=271)) | none | n/a |
| October 2016 | n/a | 2020-02-06 ([t=58](https://champman0102.co.uk/viewtopic.php?t=58)) | none | n/a |
| April 2017 | n/a | 2020-02-06 ([t=59](https://champman0102.co.uk/viewtopic.php?t=59)) | none | n/a |
| October 2017 | n/a | 2020-02-06 ([t=60](https://champman0102.co.uk/viewtopic.php?t=60)) | none | n/a |
| March 2018 | n/a | 2020-05-01 ([t=268](https://champman0102.co.uk/viewtopic.php?t=268)) | none | n/a |
| October 2018 | n/a | 2020-02-06 ([t=61](https://champman0102.co.uk/viewtopic.php?t=61)) | none | n/a |
| March 2019 | n/a | 2020-02-06 ([t=62](https://champman0102.co.uk/viewtopic.php?t=62)) | none | n/a |
| October 2019 | n/a | 2020-02-06 ([t=63](https://champman0102.co.uk/viewtopic.php?t=63)) | none | n/a |
| March 2020 | n/a | 2020-05-01 ([t=267](https://champman0102.co.uk/viewtopic.php?t=267)) | none | n/a |
| November 2020 | n/a | 2020-11-24 ([t=1381](https://champman0102.co.uk/viewtopic.php?t=1381)) | Three players report error boxes at the start of a new game ([p=5680](https://champman0102.co.uk/viewtopic.php?p=5680#p5680), [p=9004](https://champman0102.co.uk/viewtopic.php?p=9004#p9004), [p=9005](https://champman0102.co.uk/viewtopic.php?p=9005#p9005)). Replies point to the install order, an outside patch or a non-English language. No reply traces a fault to the data. | no data fix needed |
| April 2021 | n/a | 2021-04-14 ([t=2863](https://champman0102.co.uk/viewtopic.php?t=2863)) | Error boxes at start ([p=24756](https://champman0102.co.uk/viewtopic.php?p=24756#p24756)). Replies say the exe does not match the data or a file is missing ([p=25002](https://champman0102.co.uk/viewtopic.php?p=25002#p25002)). Install fault. | no data fix needed |
| October 2021 | n/a | 2021-10-25 ([t=3478](https://champman0102.co.uk/viewtopic.php?t=3478)) | One player gets a database error box many times in August of the first season ([p=43496](https://champman0102.co.uk/viewtopic.php?p=43496#p43496)). It then stops ([p=43616](https://champman0102.co.uk/viewtopic.php?p=43616#p43616)). Cause unknown. One crash at the end of a season ([p=44135](https://champman0102.co.uk/viewtopic.php?p=44135#p44135)) goes away with a Windows setting ([p=44146](https://champman0102.co.uk/viewtopic.php?p=44146#p44146)). | no |
| April 2022 | n/a | 2022-04-09 ([t=3941](https://champman0102.co.uk/viewtopic.php?t=3941)) | none | n/a |
| October 2022 | n/a | 2022-10-23 ([t=4442](https://champman0102.co.uk/viewtopic.php?t=4442)) | Topic text names the wrong season (point 4 above) | n/a |
| April 2023 | n/a | 2023-04-06 ([t=4896](https://champman0102.co.uk/viewtopic.php?t=4896)) | none | n/a |
| October 2023 | n/a | 2023-10-19 ([t=5580](https://champman0102.co.uk/viewtopic.php?t=5580)) | none | n/a |
| April 2024 | n/a | 2024-04-22 ([t=6040](https://champman0102.co.uk/viewtopic.php?t=6040)) | none | n/a |
| October 2024 | n/a | 2024-10-24 ([t=6452](https://champman0102.co.uk/viewtopic.php?t=6452)) | A zip for an outside tool is reported broken and fixed ([p=124730](https://champman0102.co.uk/viewtopic.php?p=124730#p124730)). Not the Update Team download. | n/a |
| December 2025 | n/a | 2025-12-18 ([t=10207](https://champman0102.co.uk/viewtopic.php?t=10207)) | One crash after an outside patch ([p=151565](https://champman0102.co.uk/viewtopic.php?p=151565#p151565)). Not a data fault. | n/a |
| May 2026 | n/a | 2026-05-12 ([t=10604](https://champman0102.co.uk/viewtopic.php?t=10604)) | One player sees "league missing" errors, then cannot install ([p=159740](https://champman0102.co.uk/viewtopic.php?p=159740#p159740)). One error with a non-English language ([p=160544](https://champman0102.co.uk/viewtopic.php?p=160544#p160544)). Neither is traced to the data. | no |
| Empty database | n/a | 2020-05-05 ([t=275](https://champman0102.co.uk/viewtopic.php?t=275)) | The topic says that a new game fails until the database holds 200 people | n/a |

A general fault from the FAQ: "one of our older data updates" leaves Yugoslavia and Wales out of the European Championship qualifiers, because too many nations are in Europe ([p=978](https://champman0102.co.uk/viewtopic.php?p=978#p978)). The FAQ does not say which database. The April 2015 change list says that Wales and Serbia play in the qualifiers ([t=56](https://champman0102.co.uk/viewtopic.php?t=56)). Inference: the fault is in some databases before April 2015. Which ones is unknown. Slice 10 (international) should check the nation count of Europe in each season database.

The smoke sweep of 2026-10-16 loads all 48 non-empty databases to the first news screen (`docs/research/2026-10-16-update-team-databases.md`).

## 2011/12 check

- No post in t=49 or t=274 reports duplicate players in September 2011 ODB. I found no source for the note in the starting table.
- Data check: I counted people in `staff.dat` who share first name, second name, common name, birth date and nation. The field offsets are inferred from the record size (110 bytes) and from a name lookup that gives the right clubs for known transfers. Such pairs can be real namesakes or one owner listed at two clubs.
- Counts of such groups: September 2010 ODB 52, February 2011 ODB 51, September 2011 ODB 56, March 2012 ODB 61, March 2012 SIM 30, September 2012 49. September 2011 ODB is not an outlier among the ODB databases of its era. No group holds a well-known player.
- Season check: September 2011 ODB puts the 2011 summer signings at their new clubs (for example a Manchester City striker signed in July 2011).
- Result: keep September 2011 ODB for 2011/12. If Max knows the source of the duplicate report, I can check it against the data.

## Season check of each season database

A name lookup in `staff.dat` and `club.dat` confirmed that each season database holds that season's summer transfers. It used one to three well-known moves of each summer, from 2003 to 2025. Each season had at least one move that the lookup found, and every move found was at the new club. Some names were not found because the data spells them in another way. The 2003/04 check found the 2003 Real Madrid and Chelsea signings at their new clubs.

## Content signature (Q264)

The file is `docs/research/rules-0.6.0/databases-signatures.tsv`. It has one row per database (49) with the size and SHA-256 of `index.dat`, `club.dat` and `staff.dat`, the data season, the style and the role. The rows name each database by month and year only. The folder names are not in the file.

Uniqueness across the 49 folders:

| Key | Unique values |
|---|---|
| SHA-256 of index.dat | 49 |
| SHA-256 of club.dat | 49 |
| SHA-256 of staff.dat | 49 |
| size of index.dat | 6 |
| size of club.dat | 48 (March 2014 and March 2018 match) |
| size of staff.dat | 49 |
| sizes of club.dat and staff.dat | 49 |

Each zip in `test-fixtures/databases/zips` holds the same three files as its folder (CRC-32 compared).

Proposed recognition:

1. For each folder in `Databases`, read the sizes of `club.dat` and `staff.dat`. If the pair is not in the table, the folder is a Custom database. This step reads no file contents.
2. If the pair matches a row, compute the SHA-256 of `club.dat` and `staff.dat`. If both match, the folder is that known database. Otherwise it is a Custom database.
3. Do not use `index.dat` in the match. It holds stray bytes from the tool that wrote it, and the editor rewrites it on each save. Keep its hash in the table for reference only.
4. Cache the result per folder by file size and modification time, so the hash runs once per folder.

An edited known database then becomes a Custom database. That is correct, because its squads are no longer the known ones.
