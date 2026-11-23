# History research, part asia-1

This part covers Japan, South Korea, China and China PR. It has 128 rows in `asia-1.tsv`. 118 rows are `yes` and 10 are `not held`. No row is `unknown`.

"China PR" is the nation name in 3.9.68, October 2003 and September 2005. "China" is the name in the databases from November 2007 on. Each competition has a row per name. The China PR rows use the club names of the old databases. The China rows use the names of the new databases.

## Label conventions

All competitions in this part use a calendar year, apart from the two super cups. The Emperor's Cup also needs care.

| Competition | Convention | 3.9.68 rows that prove it |
|---|---|---|
| Chinese FA Cup | calendar year of the final | 1998 Shanghai Shenhua / Liaoning Fushun (1998 final), 2001 Dalian Shide / Beijing Guo'an (2001 final) |
| Chinese FA Super Cup | the season that the match opens, which is the edition year + 1 | 1996 Shanghai Shenhua / Shandong Luneng (1995 edition, Dec 1995), 1999 Shanghai Shenhua / Dalian Shide (1998 edition, Mar 1999), 2001 Dalian Shide / Chongqing Lifan (2000 edition, played 30 Dec 2000), 2002 Shanghai Shenhua / Dalian Shide (2001 edition, Feb 2002) |
| Chinese First Division A, B, Second Division | calendar season | A 1999 Shandong Luneng / Liaoning Fushun, B 2001 Shanghai Zhongyuan / Changchun Yatai, Second 1998 Guangzhou Baiyunshan / Beijing Kuanli, 2001 Dalian Sàidélóng / Liaoning Youth |
| Japanese Cup (J.League Cup) | calendar year | 2000 Kashima Antlers / Kawasaki Frontale, 2001 Yokohama F Marinos / Jubilo Iwata |
| Japanese Emperor's Cup | the season year, with the final on 1 January of the next year | 2001 Shimizu S-Pulse / Cerezo Osaka (final 1 Jan 2002), 1998 Yokohama Flugels / Shimizu S-Pulse (final 1 Jan 1999) |
| Japanese Super Cup | the year the match is played (February or March) | 2000 Jubilo Iwata / Nagoya Grampus Eight (4 Mar 2000), 2001 Shimizu S-Pulse / Kashima Antlers (3 Mar 2001) |
| Japanese J-League 1 and 2, Football League | calendar season | J1 2001 Kashima Antlers / Jubilo Iwata, J2 2001 Kyoto Purple Sanga / Vegalta Sendai, JFL 2001 Honda Football Club / Otsuka FC |
| Japanese U-18 Championship | calendar year. It is the Prince Takamado Cup U-18 | 1999 Jubilo Iwata / Shonan Bellmare, 2000 Shimizu Commercial / Maebashi Commercial, 2001 Kunimi / FC Tokyo |
| Japanese University League | calendar year. It is the Prime Minister's Cup (summer tournament), not the winter All Japan University Championship | 1996 Juntendo / Komazawa, 2001 Hannan / Komazawa. All six 3.9.68 rows from 1996 to 2001 match the Prime Minister's Cup finals. None match the winter championship |
| Korean League, League Cup, FA Cup, President Cup | calendar year | League 1999 Suwon / Pusan I.cons, FA Cup 2001 Taejon Citizen / Pohang Steelers (Nov 2001), President Cup 2000 Korea University / Kyunghee University |
| Korean Super Cup | the year the match is played (March) | 1999 Suwon / Anyang LG Cheetahs (20 Mar 1999), 2001 Anyang / Chonbuk (18 Mar 2001) |

Super cup labels in this part:

- Chinese FA Super Cup label 2003 is the 2002 edition (6 Feb 2003).
- Chinese FA Super Cup label 2004 is the 2003 edition (18 Jan 2004).
- From label 2005 the Chinese FA Super Cup is `not held`. No match took place between the 2003 edition and 2012.

Other 3.9.68 points:

- The 3.9.68 Korean League Cup rows choose one of the cups in years when there were two (1997 = the Pro-Specs Cup, 1999 and 2000 = the Daehan Fire Insurance Cup). From 2002 on there is only one cup per year.
- The 3.9.68 Korean President Cup row for 1999 (Songnam / Chonbuk) shows the reserve teams (Cheonan Ilhwa Chunma B, Jeonbuk Hyundai Dinos B) under the senior club names.

## Tier mapping

- Chinese First Division A is the top tier. It was the Jia-A League to 2003 and the Chinese Super League from 2004.
- Chinese First Division B is tier 2. It was Jia-B to 2003, the China League in 2004 and China League One later.
- Chinese Second Division is tier 3 (Yi League, later China League Two). Its champion and runner-up come from the final round.
- The Japanese Football League is the JFL of 1999 on, at tier 3.

## Points of doubt

1. **Chinese First Division A 2003.** Shanghai Shenhua won the title at the time. The title was revoked in February 2013 for match fixing, and current lists say "not awarded". The row gives Shanghai Shenhua, as every database of the period would show, and the note says so. If the game must follow current records, change the row to `not held` with no winner. **Decided 3 November 2026 (0.6.2 item 108, Max: do what real life does):** the row follows today's official record (see docs/research/2026-10-17-competition-history-fill.md, Q355).
2. **J-League 1 2002 and 2003.** One club won both stages, so no championship final took place. The runner-up comes from the overall table (Yokohama F. Marinos in 2002, Júbilo Iwata in 2003). The 3.9.68 row for 1996 (a single-stage season) has no runner-up. For these two years I give the table runner-up all the same.
3. **Korean League Cup 2004 and 2005.** These were round robins. The runner-up is second in the table, not a losing finalist.
4. **Korean League Cup 2003** is `not held`. The RSSSF 2003 page says "No league cups this year", and the Wikipedia list has no 2003 edition.
5. **Korean Super Cup 2003** is `not held`. The match of 16 March 2003 was cancelled because of Seongnam's AFC Champions League schedule.
6. **Club lineages that are inferences.** I mapped these real clubs to data names by inference:
   - Sichuan Guancheng (Chinese FA Cup 2004 runner-up) to Sichuan Quanxing.
   - Shanghai International (2003 runner-up) to Shaanxi Baorong Chanba FC in the China databases. The club moved to Xi'an in 2006.
   - Sagawa Express Tokyo (JFL 2002 runner-up) to Sagawa Soccer Club.
   - Korean Police (President Cup 2003 runner-up) to National Police Station.
   - Guangzhou Pharmaceutical and Chengdu Blades to Guangzhou Jili and Chengdu Wuniu in November 2007.

   These lineages come from RSSSF season pages:
   - Wuhan Hongjinlong became Donghu Gaoke (2002), then Guoce Lanxing (2003), then Wuhan Huanghelou (2004).
   - Dalian Saidelong became Zhuhai Anping (2003), then Zhuhai Zhongbang (2004).
   - Qingdao Yizhong Hainiu became Hademen (2002), then Beilaite (2003), then Zhongneng (2005).
   - Shenzhen Ping'an became Jianlibao (2003).
   - Henan Hetao went back to the name Henan Jianye (2004).
   - Xiamen Hongshi became Xiamen Shishi, then Xiamen Lanshi (Blue Lions) (2004).
7. **Guangzhou Rizhiquan.** In 2004 RSSSF calls Guangzhou Pharmaceutical "Guangzhou Rizhiquan" (the sponsor name). The 2008 League Two runner-up, Guangdong Sunray Cave, is a different club with the same sponsor. I did not map it to the data's Guangzhou Rizhiquan. It is marked `not in data`.
8. **The club names differ between the new China databases.** November 2007 uses older names: Shenzhen Pingan, Qingdao Yizhong, Wuhan Hongjinlong, Guangzhou Jili, Chengdu Wuniu. The other databases use Shenzhen Shangqingyin, Qingdao Zhongneng, Liaoning F.C., Guangzhou Pharmaceutical and Chengdu Blades. The China rows use the name in most databases, and the notes give the November 2007 name. Japan has the same problem: Tokyo Verdy 1969 and Nagoya Grampus Eight up to November 2007 and February 2008, then Tokyo Verdy and Nagoya Grampus. Korea has it too: Songnam and Taejon in the old databases, Seongnam and Daejon in the new ones. The rows use the spelling of the oldest database that needs the row, and the notes give the other spelling.

## The `ended` column

`ended` is the month of the final, or of the last league match day. The dates come from the round and final dates on the RSSSF season pages for China, Japan and South Korea. For the Emperor's Cup and the Japanese Super Cup they come from the Wikipedia final lists. The `sources` column gives each page as `ended=URL`. Points to note:

- The Emperor's Cup ends in January of the year after the label year. The game must not add the label 2009 row to a game that starts before January 2010.
- The Chinese FA Super Cup of label 2004 ended in January 2004 (18 Jan). The Korean League Cup of 2002 (Adidas Cup) ended in May 2002 because the World Cup came after it.
- I don't know the month for these 10 rows, and they hold `unknown`:
  - Japanese U-18 Championship 2002 to 2005.
  - Japanese University League 2002 to 2005.
  - Korean President Cup 2002 and 2003.

  None of the pages I read gives the date of these finals. The Japanese Wikipedia page says the Prime Minister's Cup was played from late August to early September up to 2019, but it gives no date for each year.
- J-League 2 has a bye round for 13 and 15 teams. The last round is Round 52 in 2006 and 2007 and Round 45 in 2008.

## Database rows that disagree

Most database rows inside the new label range hold an older season's result under the shifted label.

| Database | Competition | Label | Database holds | Real |
|---|---|---|---|---|
| September 2005 | Chinese First Division A (China PR) | 2005 | Shanghai Shenhua / Shanghai International (the 2003 result) | Dalian Shide / Shanghai Shenhua |
| September 2005 | Chinese First Division B (China PR) | 2005 | Shanghai International / Changchun Yatai | Xiamen Hongshi (Blue Lions) / Changchun Yatai |
| February 2008, November 2007 | Chinese First Division B (China) | 2007 | Xiamen Hongshi / Changchun Yatai (the 2005 result) | Guangzhou Pharmaceutical / Chengdu Blades |
| December 2008, March 2009, September 2008 | Chinese First Division B (China) | 2008 | Xiamen Hongshi / Changchun Yatai | Jiangsu Shuntian (Sainty) / Chongqing Lifan |
| September 2005 | Japanese J-League 1 | 2005 | Yokohama F Marinos / Jubilo Iwata (the 2003 result) | Gamba Osaka / Urawa Red Diamonds |
| September 2005 | Japanese J-League 2 | 2005 | AlbireX Niigata / Kawasaki Frontale | Kyoto Purple Sanga / Avispa Fukuoka |
| February 2008, November 2007 | Japanese J-League 1 | 2007 | Gamba Osaka / Urawa Red Diamonds (the 2005 result) | Kashima Antlers / Urawa Red Diamonds |
| December 2008 | Japanese J-League 1 | 2008 | Gamba Osaka / Urawa Red Diamonds | Kashima Antlers / Kawasaki Frontale |
| March 2009, September 2008 | Japanese J-League 2 | 2008 | Kyoto Sanga FC / Vegalta Sendai / Montedio Yamagata (the 2001 result) | Sanfrecce Hiroshima / Montedio Yamagata |
| February 2008, November 2007 | Japanese Super Cup | 2007 | Tokyo Verdy 1969 / Yokohama F Marinos (the 2005 match) | Gamba Osaka / Urawa Red Diamonds |
| December 2008 | Japanese Super Cup | 2008 | Tokyo Verdy / Yokohama F Marinos | Sanfrecce Hiroshima / Kashima Antlers |
| March 2009, September 2008 | Japanese Super Cup | 2008 | Kashima Antlers / Sanfrecce Hiroshima (winner and runner-up swapped) | Sanfrecce Hiroshima / Kashima Antlers (Sanfrecce won on penalties) |
| April 2010 SIM | Japanese Super Cup | 2009 | Kashima Antlers / Sanfrecce Hiroshima (the 2008 match) | Kashima Antlers / Gamba Osaka |
| February 2008, November 2007 | Korean League | 2007 | Seongnam Ilhwa Chunma / Suwon Samsung Blue Wings (the 2006 result) | Pohang Steelers / Seongnam Ilhwa Chunma |
| December 2008 | Korean League | 2008 | Seongnam Ilhwa Chunma / Suwon Samsung Blue Wings | Suwon Samsung Blue Wings / FC Seoul |
| March 2009, September 2008 | Korean League | 2008 | Seongnam Ilhwa Chunma / FC Seoul | Suwon Samsung Blue Wings / FC Seoul |

These rows agree with the real results:

- 3.9.68, October 2003 and September 2005: Chinese FA Super Cup label 2002, Shanghai Shenhua / Dalian Shide.

## Names that could not be matched (`not in data`)

- China, tier 3:
  - 2004: Shanghai Jiucheng (Shanghai The 9) and Yanbian Century. Yanbian Century may be the data's Jilin Yanbian, but I did not verify it.
  - 2005: Beijing Hongdeng.
  - 2007: Shanghai East Asia and Sichuan F.C.
  - 2008: Shenyang Dongjin and Guangdong Sunray Cave.
- China tier 3 2005 winner Nanchang Bayi Hengyuan:
  - Not in the China PR data.
  - In the China rows it is "Nanchang Bayi" (April 2010 SIM). That database does not hold this competition.
- China tier 3 2006 winner Beijing BIT: "Beijing Institute of Technology Football Club" exists only in databases that do not hold the Second Division.
- Japan:
  - Seiryo High School (U-18 2002 runner-up).
  - Kansai University (Prime Minister's Cup 2005 winner).

## Sources

Every source was read on 2026-10-17 with `curl` and checked by grep. The `sources` column of each row gives its URLs.

- Wikipedia:
  - Chinese FA Cup, Chinese FA Super Cup, List of Chinese football champions, China League One, China League Two.
  - J.League Cup, List of Emperor's Cup finals, Japanese Super Cup, List of Japanese football champions, J2 League, Japan Football League, Prince Takamado Cup.
  - List of South Korean football champions, Korean FA Cup, Korean League Cup, Korean Super Cup, Korean President's Cup.
- Japanese Wikipedia: the Prime Minister's Cup university tournament.
- RSSSF China season pages for 2002 to 2009. The 2002 to 2004 pages give the Jia-B and third-level results and the club renames. All of them give the `ended` dates.
- RSSSF Japan season pages (2002 to 2009) and South Korea season pages (2002 to 2005). These give the `ended` dates.
