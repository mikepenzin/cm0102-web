# Changelog

CM 01/02 Bosman Edition uses semantic versioning. A patch release (0.1.1) fixes bugs. A minor release (0.2.0) adds features. A major release (1.0.0 and later) can break saved games or settings. Releases below 1.0.0 are test releases.

## 0.7.3 (pre-release)

Leagues play their real promotion and relegation places in more nations, and closed leagues such as MLS and the A-League take and lose clubs in their real seasons. This release also adds squad numbers from 1 to 99 and fixes faults in the news, Save Game, history and competition screens. One new line in `settings.txt`: `squad_numbers_99`.

Saves from 0.7.2 and earlier load in 0.7.3. Do not open a season game saved by 0.7.3 in an earlier version.

### Squad numbers from 1 to 99

- New option `squad_numbers_99`, off by default, with the row "Squad numbers 1 to 99" on the new Squads tab of Bosman Edition Options. With it on, a new game keeps the database's squad numbers from 51 to 99, and a signing gets a number up to 99 when 1 to 50 are taken. The Squad Numbers screen shows 1 to 50 above the player list and 51 to 99 below it. With it off, no new number goes above 50 and the screen is the original. A number above 50 that a save already holds stays. In a network game the host's setting applies (issue 140).

### Real places and league sizes

- Norway, Sweden, Brazil and Portugal: a division that grows or shrinks at a season change sends its extra clubs by the real promotion and relegation places, not by the original's places. Examples: the Allsvenskan 2006/07 and the Tippeligaen 2007/08 send 1 club down, Série A 2003/04 sends 4 down and Série B sends 6 (issue 154, items 246 to 249 and 259).
- Portugal: the Primeira Liga and the Segunda Liga play their tie from 2013/14 in the seasons the rules table gives. A 2001 start with Portugal and Spain no longer stops at "por_second_b..cpp 419" at the 2003/04 season change. A club that the size step moves into a kept division takes a zone, also in the Belgian third division (issue 155, items 278, 279 and 287).
- With Portugal's extra divisions off, the Segunda Liga's surplus goes to no division, because the game holds no division below it.
- Northern Ireland: the NIFL Premiership and Championship play their real sizes and places, and both leagues start every season after 2018. Before, a 2018 start stopped both leagues at 2019/20, because two matches moved to the day of the Championship split were never played. The fix acts in every league with a split, in a season game (issue 153, items 271, 272 and 288).
- Croatia and Greece: the Druga HNL, the Greek second tier and the Super League play the places of their rows, and the play-off tie between two divisions plays in Greece in 2002/03, 2003/04 and 2018/19 to 2020/21 and in Croatia in 2006/07 to 2008/09 and 2013/14 to 2019/20 (issue 156, items 281 and 282).
- Japan and Finland: J1's 16th plays J2's 3rd over two legs in 2004 to 2008, J2 plays its four-club play-off in 2023, and the Veikkausliiga plays no relegation play-off in 2002, 2006 and 2011 to 2014 (issue 156, items 283 and 284).
- Argentina: the second division plays the 2001/02 form, the Torneo Reducido of 2014, 2017, 2018 and 2021, and the zones of 2019, 2020 and 2022 to 2025, with the real relegation places (issue 158, item 286).
- MLS and the A-League: a closed league loses its folded clubs and takes its new clubs in their real seasons. MLS holds 10 clubs in 2002 and 30 from 2025, with San Diego FC from 2025. The A-League starts in 2005/06 with 8 clubs and then follows the real count of each season. A new club uses its club in the database when there is one. A club with a human manager stays in the league (issue 157, items 269 and 285).
- Sweden: Division 1 plays two groups, Norra and Södra, from 2006, with 14 clubs in each and 16 from 2018. When the Allsvenskan grows, the best placed clubs of the two groups fill the Superettan (issue 106).
- Belgium: the Play-off II final and the Europa League play-off from 2009/10 to 2018/19, the First Division B clubs in Play-off II from 2016/17 to 2018/19, and the final round of the second division (issue 150).
- The Netherlands: the Eerste Divisie promotion and relegation knockout plays from 2005/06, with ten clubs to 2018/19, no club moves in 2019/20, seven clubs from 2020/21 and four rounds from 2025/26 (issue 150).
- Relegation play-offs over a play-off of four clubs: Norway, Ireland, J1 and J2, and Ligue 2 and Ligue 3 play them in their real seasons. The research note `docs/research/2026-11-18-relegation-playoffs-over-four.md` lists them (issue 160).
- All of these act only in a season game with changing rules on. The base game (2001/02 with changing rules off) stays the original.

### European cups

- League phase draw: from 2024/25 the Champions League, the Europa League and the Conference League draw the league phase from pots. The Champions League and the Europa League have four pots of nine, and each club meets two clubs of each pot, one at home and one away. The Conference League has six pots of six, and each club meets one club of each pot. A club never meets a club of its own nation and meets at most two clubs of another nation. The pots follow club reputation, highest first, because the game keeps no club coefficient. Only with changing rules on (issue 147).
- A Conference League holder takes a Europa League place, as the real access rule says.
- UEFA Cup 2001/02 to 2003/04: the qualifying rounds and the first round play on their real dates. Before, the first round of 2003/04 came the day after an international match day.
- AFC Champions League and other club matches: a club match no longer lands next to a national match that calls up the club's players. The original wrote no call-up count for the last club of each national squad. Only in a season game (item 300).
- A linked B or under-21 squad no longer takes the regulars of a club that plays the day before its match. Before, a Copa Sudamericana club of 1 September 2003 had three players in Argentina's under-21 squad for 2 September (item 299).

### History, hosts and nations

- Competition history: rows that were wrong in the season databases show the right result. Examples: the 2026 World Cup host is the USA, the Copa Libertadores 2022 runner-up is Athletico Paranaense, the Euro 2020 host is England and Euro 2024 has no third place. Each row has a source in `docs/research/history-rows/` (issue 145).
- The Conference League, the Nations League and the Greek Super Cup show their own history of winners (issues 145 and 165).
- OFC Nations Cup: the history holds the editions of 2008 to 2024 that a database lacks, with 2020 cancelled. The 2012 hosts are the Solomon Islands for the OFC Nations Cup and Equatorial Guinea for the Africa Cup of Nations.
- More history rows are corrected, each with a source: the DFB-Pokal 2015/16, Serie C and Lega Pro 2015 to 2019, the Olympics of 2012 and 2016, the Danish Cup 2018/19, the A-League 2019 and some Brazilian state championships. The League of Ireland's short 2002/03 season shows under 2002, and the President's Cup shows no edition from 2003 to 2014.
- Euro 2028, World Cup 2030 and Euro 2032 show their real hosts in the news, two hosts each where a tournament has more (issue 146).
- A player with a second nation no longer shows the wrong nation, such as Kevin De Bruyne as Burundi in the October 2026 database. A save that already holds the wrong nation keeps it (issue 141).

### Screens and news

- Save Game suggests "Nation YYYY-MM-DD" for a national team manager with no club. A manager with a club keeps the club's name (issues 139 and 164).
- News: a season game no longer shows "(null)" items or "ERROR: No league fate yet for UEFA Nations League". A text in the news filter no longer stops the game with "search_eng..cpp 881" (issues 161 and 162).
- The competition arrow on the history screens of the Intercontinental Cup and the other world club competitions lists their own group, not the European competitions (issue 163).
- A human move of a loaned player inside a club and its linked side keeps the player in one squad slot. The Action menu does not offer a move that would do nothing (issue 159).
- The site's roadmap shows Done only for issues closed as completed (issue 138).

### Leagues and cups that stopped

- Croatia: a game made in a season with one Croatian second tier no longer stops the HNL after a save and load. The Druga HNL South gets its table back at the load, and a 0.7.2 save in which the HNL already stalled changes its season on the first day after the load.
- Ireland: the Setanta Sports Cup starts its next edition on 20 June when the Irish season change falls in February. Before, from the calendar move of 2003 the cup missed editions and held the Irish league back.
- The Netherlands: a play-off leg that a clash moves stays before the next draw. Before, the winner could miss the next round and the Eerste Divisie season change came 60 days late.
- Northern Ireland: in a season game the Irish Cup no longer stops with the box "comp_util..cpp 241" when the lower division holds fewer than 12 clubs, as in 2008/09 in a 2001 start. Clubs of the two higher divisions fill the free places of that round.
- MLS: when a club joins and no club leaves, a club of the American lower divisions takes the free place, so the A-League keeps enough clubs for the US Open Cup.

### Faults of the original

- A new game on the October 2026 database could stop with the box "Database.cpp 13126" in August 2026. Two clubs of the database each have two key ids, and the original moved only the first one. In a season game every key id of a club now follows it (item 295).
- Argentine second division: the average points no longer read the play-off stages as league stages, which crashed the original (issue 132).
- Player condition between two summer matches a week apart: the original 3.9.68 game does the same, so it stays (issue 148).

### Planned changes of play in a season game

These change how a season game plays from the same save, by plan. The base game does not change.

- The league phase pots follow club reputation (above).
- A Conference League holder takes a Europa League place (above).
- The Greek Super Cup history rows in a game that starts in 2001 with changing rules on.
- The key id fix of item 295.
- The call-up count fix of item 300.

### Known limits

- Croatia: the two-group seasons 2001/02 to 2005/06 play no play-off between the group winners. Greece: the play-off groups of 2009/10 to 2012/13 and the rounds of 2013/14, 2014/15 and 2021/22 to 2025/26 are not played. Relegation play-offs against third-level clubs that the game does not hold are not played (item 293).
- Argentina: the two inter-zone derby matches of 2023 and 2025 are not played. In 2004 to 2006 the promoción takes the place of the Reducido. The clubs of each zone come from the club ids, because no source gives the lists of every season. The tie-break of the 2025 final is not known, and it goes to penalties (item 291).
- A 2001 start with changing rules off on a later database does not get the item 295 fix, so the box "Database.cpp 13126" can still show in that game.
- A club that joins MLS in a running game can start with a thin squad (item 294).
- The play-offs, splits and cup formats listed for 0.7.4 (issues 167 to 193) are not played yet.
- The Netherlands: the knockout of 2005/06 plays two legs where real life played some third matches.
- A 0.7.2 save loaded during a season from 2009/10 to 2018/19 plays the Belgian play-off rounds from its next season.
- The Friendly menu keeps the original's list of European competitions.
- A new game that starts in the season of a July finals tournament plays that edition again in its first year. A September 2011 start plays the Copa América 2011, which ended in July, with its own result (item 303).

### Tests

- The release set ran with seed 73. Each fault it found is fixed, and the rows of each fix ran again. Each league slice has season runs that check the count and the places played, and the same-save set shows that the base game is unchanged.

## 0.7.2 (pre-release)

Leagues play their real number of clubs, meetings and dates in a season game. This release also fixes loans on the season databases, history years, the Tactics click and the rules of several nations. There are no new lines in `settings.txt`.

A season game saved by 0.7.2 does not load safely in 0.7.1. Saves from 0.7.1 and earlier load in 0.7.2.

### Real league sizes

- In a season game each league plays the club count, the meetings and the dates of its real season in the season it starts in. Before, a league played the club count of its class in the original game. So Argentina, Serie A, Serie B, the Eerste Divisie, J1, J2, the K League and 16 other leagues held fewer clubs than in real life, and some leagues held more clubs than they have because a cup needed them. A league of any size now builds its table and its fixture dates from its row in the rules table. Each league also plays its real halves, zones, splits and periods.
- Cups of every nation in the set take the clubs of the real divisions, so no cup leaves out or doubles a club. Examples: the Welsh League Cup counts the clubs of one group, and the French Lower Division fills its 40 places.
- England and France: the Conference takes its places from the rows, with a six-club play-off from 2017/18 and four down places in 2019/20. Scotland has its cross play-offs from 2005/06 and its Premiership ladder from 2013/14.
- Italy: Serie A holds 20 clubs and 38 rounds from 2004/05, Serie B has its top-eight knockout in 2002 to 2005, and the C1 play-outs and the Serie A tie-break zones follow real life.
- The Netherlands, Belgium, Denmark and Finland: each league plays its real count and form. The Belgian First Division plays Play-off I with halved points.
- Croatia and Greece: the championship group and the Croatian sections follow the real seasons.
- Russia and Turkey: the Russian league moves to an autumn-to-spring season through one long 2011/12 season, and the Turkish leagues play their real counts.
- Ireland: the Irish Premier Division and First Division hold their real counts from 2002, the League of Ireland moves to a calendar-year season in 2003.
- Japan and Korea: J1 and J2 play their real counts and meetings, and the K League plays its splits and its exchange of clubs between divisions.
- Argentina: the Primera plays the form of each season. It has halves, one table, or two zones of 15 with the inter-zone rounds, 30 clubs in 2025. The second division and the promotion play-offs follow the same rules.
- Argentina: a second division edition plays the row of its own year, and the 2019/20 season has no relegation, as in real life (item 286).
- Brazil: Série A plays two meetings from 2003 and Série B from 2006, with the real dates.
- MLS: the league plays two conferences with an unbalanced schedule and the play-off, in place of three groups of four.
- The base game (2001/02 with changing rules off) stays the original. `RECOMP_LEAGUE_SIZES=off` keeps the original counts in a season game.
- Known limits, for 0.7.3:
  - Croatia, Greece, J1 and J2, the Veikkausliiga and the A-League: in a few seasons before a count change the places of promotion, relegation or play-off differ from real life. The counts are right, and 0.7.1 had the same lines (items 281 to 285). The Veikkausliiga plays a relegation play-off where the rows give none, and the A-League holds 13 clubs in every season from 2009/10.
  - Argentina: the Primera in 2013 and 2016 to 2018 and the second division in 2001/02 and 2018/19 have play-off or relegation details that differ from real life (item 286).
  - MLS: the club count follows the database, not the row. A 2001 start keeps the database's 12 clubs in 2002 (item 269).
  - Norway, Sweden, Brazil, Portugal and Northern Ireland: a division that grows or shrinks at a season change moves its extra clubs by the original game's promotion and relegation places, not the real ones. The real places (items 246 to 249, 259, 271 and 272), the Segunda B and Turkish zones and items 278, 279 and 287 wait for 0.7.3. They were held back because, together with other changes, they stopped the Northern Irish leagues from starting 2019/20 in a 2018 start (item 288).
- Not in 0.7.2: regional groups of one division, the Danish 2nd Divisions, the Finnish Ykkönen as one division and new divisions. A few relegation play-offs over a four-club play-off are noted for 0.7.3.

### Seasons, leagues and cups

- Poland: the Ekstraklasa plays its real format in a season game, no longer the 2001/02 two-group format (issue 92, item 221).
- Sweden: the Allsvenskan holds 16 clubs from 2008 in a season game (issue 106).
- Norway: a second team leaves the Second Division when its club plays too low. Up to 2014 only an Eliteserien club's second team plays there. From 2015 a second team plays below its club (item 232).
- Reserve and B teams follow the real rules of six more nations (items 239 to 244).
- Croatia: from 2013/14 only players from outside the EU and EEA count as foreign in the match squad, and the news says so (issue 135).
- Portugal: no limit on non-EU signings in the Liga and the Liga de Honra from 2006/07 (issue 121).
- Asian Cup: after the hosts, the entrants are the best sides by reputation (issue 136).
- European places in the first season of a game: a renamed nation (Turkey and others) takes its places, and from 2015/16 the cup runner-up gets none (issue 94, item 222).
- UEFA Cup: the news after a group stage names the group winner that goes on, not the cup winner (issue 119).
- Competition history on the season databases: about 14,800 rows in 42 databases were one year off in the database files (the Norwegian league 1963 to 2007 and the cup, the Swedish Cup to 2007, the League of Ireland before 2003, La Liga 1929 to 1936, the Scottish Challenge Cup 1990 to 2007, and some German, Italian and Finnish rows). A new game now shows them in their real years. The files stay unchanged. A saved game keeps the history it started with (item 233).
- Bench and substitutes: a competition without a sourced bench or substitute count keeps the latest sourced one. The EFL Cup names 9 substitutes and allows 5 from 2025/26, and the EFL Trophy 7 and 5. 104 more cells in 61 competitions hold researched values (item 274).
- The OFC preliminary round is built once and waits for the day after its section's season start, so a game with a level two-leg tie no longer stops at "Cup..cpp 1450" (item 245).
- New players of Wales and Ireland join English clubs in the October 2026 data, where no English club has a reputation of 2 or less. Before, they found no club (item 262).
- A player on loan to his club's own B side is no longer moved between the first team and the reserves by the computer. Before, a Cottbus player on loan to Cottbus II was added to the squad twice and the game stopped with "match_man..cpp 1225" on day 208 of 2007 (item 280).
- A reserve side with fewer than 11 fit players in a normal game keeps fewer players back, so the game no longer stops with "match_eng..cpp 612" (item 237). A friendly with nine youths no longer stops it with "match_eng..cpp 652" (item 262).

### Databases and new games

- Loans: a new game on an Update Team season database starts with the loans, bans and injuries of the database. The season downloads left out the file `player_setup.cfg` that holds them, and the dates in it were those of 2001/02. Both are fixed. In the 2026/27 game Grealish starts on loan at Everton. Start a new game to get them (item 229).
- The game knows the shipped 3.9.60 and 3.9.65 data sets (item 230). A new 2001 game on them no longer holds history rows twice (item 227).
- A new game with a few nations and the Spain or Portugal extra division starts without the boxes "spa_super..cpp 203" and "por_super..cpp 211" (item 226).

### Interface

- Tactics: a very quick click, for example a tap on a trackpad, opens the Tactics screen. The game now holds back the release of a click until it has seen the button down.

### Tests

- Each league-size slice has season runs that check the count and the rounds played, and the same-save set shows that the base game is unchanged.
- A release test set of 34 games and a background set replace the long runs of 0.7.0 as the release check. See `docs/TESTING.md`. The full set did not run for 0.7.2. The league-size seasons, the same-save set and the per-fix checks did.

## 0.7.1 (pre-release)

This release has two screen fixes. They change no game decision, so saves from 0.7.0 load and play the same.

- Invite link on the Network tab: while you host a game that is in the online list, the Network tab of Bosman Edition Options shows its game code and invite link, with a Copy link button. Before, the host could not see the code or the link (issue 137).
- Screen Mode works: Full or Windowed on the Game Settings screen now switches full screen, the same as F11 and Cmd+F. The game starts again in the mode you chose, and a window opens at its remembered size and place. A first start still opens in a window (issue 120).

## 0.7.0 (pre-release)

Play network games with friends online, on a Mac, Windows or Linux, with no patch, no VPN and no router setting. This release also fixes many faults in seasons and competitions that long test games to 2300 found. New lines in `settings.txt`: `online_play` and `game_password`. The `season` line can also be `2026/27`.

A save made by 0.7.0 does not load in 0.6.0 or 0.6.1: they stop with the box "Cup..cpp 5246". Saves from 0.6.0 and 0.6.1 load in 0.7.0.

### Online play

- Network Play works again. Bosman Edition answered every network call with "no network" before. The original's own screens run: Network Play, Start Network Game, Join Network Game and Select Network Game.
- The game list (Select Network Game) shows the games online and the games on your own network in one list, up to 50 games with the original's scrollbar. Each online row shows the season, the game date and the managers online. The original showed at most 9 games.
- A host's game goes into the online lobby when it starts. Friends on another network click the game in the list and join. A relay passes the traffic, so nobody opens a port on their router. Mac, Windows and Linux players play in one game.
- Invite links: a host can give friends the address of the game's page, `https://online.cm0102bosman.co.uk/join/` and the game code. The page shows the game, an "Open in Bosman Edition" button, the game code and a download link for a friend who does not have the game. A click on an invite link opens the game and joins that game. A game with a password still asks for it.
- On Windows and Linux, the game registers the `cm0102://` link for your user account only, while Online play is on. With Online play off, the game removes it at the next start. On a Mac the app itself holds the link.
- Game password (`game_password`): a host sets it on the Network tab of Bosman Edition Options. A friend who joins types it in a small box. A wrong password shows the box again.
- Kick: while you host, the Network tab lists the human managers online with their club and ping, and a Kick button for each. Their club goes on holiday.
- Drop in and drop out: when a human manager leaves or loses the connection, they show as Away, and their club goes on holiday as in the original. They join again and take their club back with their manager password.
- When the host quits, the host's game saves, and each human manager sees "The host left the game." and goes back to the start screen.
- A host that quits leaves the online game list at once.
- A host in widescreen, the default, no longer breaks the screens of every human manager who joins. The game sends every network screen position at the original's 800x600 size, so any mix of classic and widescreen works, with the drag on the tactics screen too.
- Always-on games: a host can leave a game open for a long time. It moves forward only when a human manager clicks Continue.
- Clear errors: each failure says what went wrong and what to do next, in place of "The network game could not be created" or a wait at "Contacting Server". A player on another version sees the host's version.
- Connection test: a Test button on the Network tab checks the lobby and the relay, and says that online play works or what is wrong.
- Ping: the game list shows the ping of each online game, and the Network tab shows each human manager's ping.
- Online play (`online_play`, on by default): a switch on the Network tab of Bosman Edition Options. With it off, the game list shows games on your own network only, and the game never contacts the online service.
- The online service is free and has a daily limit. When it is used up, the game list says "Online games are full today, try again later" until 00:00 UTC. Games on your own network or over a VPN still work.
- Privacy: the lobby keeps internet addresses in memory only while a game is open, and writes them to no log. It has no accounts.
- The secure connection to the lobby and the relay uses mbedTLS. Its licence is in each download.

### Seasons, leagues and cups

- Choose Season has a 2026/27 tile with the Update Team's October 2026 database. A 2026/27 game starts on 11 July 2026, with the 2026 World Cup and the 2025 Africa Cup of Nations in its history. The 2026/27 rules of every league were checked against the new database. A long game with changing rules on takes them in July 2026.
- Reserve and B teams follow the real rules of their nation in every season game: Norway, the Netherlands from 2013/14, Belgium from 2020/21, France, Italy, Finland, Denmark from 2005/06, Russia from 2015/16, Spain, and Portugal from 2012/13. A second team cannot go up into the division of its first team, and goes down when its first team comes down to it. The place goes to the next club in the table. A save from an earlier release is put right at the next season change.
- Each nation keeps the last rule of its league sizes, promotion, relegation, play-offs and foreign player limits after the rules table ends. Cup formats, super cups and substitutes no longer go back to the start season after 2099.
- Australia: the NSL plays its finals series and Grand Final every season. Before, every later season repeated the first Grand Final. This is a fault of the original game too.
- Belgium: the Second Division's 16th goes down directly from 2015/16, and no longer plays the third-tier promotion play-off. The three divisions of a 2025 start no longer stop at the 2026 season change.
- Brazil: Série A no longer loses 2 clubs every season from 2025/26, and Série B sends 4 clubs up. The state championships keep their rules after 2057, and the Campeonato Paraense plays every year.
- Ireland: the Premier Division holds 12 clubs and the First Division 10. The Irish Super Cup no longer stops after some decades, and a save where it stopped starts it again at the next season change. This is a fault of the original game too.
- Italy: a 2006/07 start plays the Supercoppa Italiana as Internazionale against AS Roma. Before, Juventus played it from Serie B. Serie C group winners go up to Serie B before the play-off winners.
- England: the Conference League Cup plays only in the seasons it really ran.
- Finland and Poland: the Veikkausliiga, Ykkösliiga, Ekstraklasa and I liga no longer stop in very long games.
- Japan and Argentina: a 2026 start plays the Emperor's Cup and the J.League Cup, and the Argentine history has the 2026 Apertura.

### European cups

- Spain's Champions League places go to the top four of La Liga. Before, some 2021, 2025 and 2026 starts gave them to clubs from lower in the table or from the Segunda División. A Champions League or Conference League place that no club can take stays empty, as UEFA leaves it, and the round gets a bye.
- Each season database has its own European places file. A 2005/06 start no longer puts Leeds United, a second-tier club, in the Champions League. Season folders unpacked by 0.6.0 or 0.6.1 get the file at the next start.
- UEFA coefficients use the real values in every season database and in our Summer 2002, Summer 2004 and Summer 2006 databases. Saves from earlier releases keep the season with no value.
- From 2009/10 a nation has at most 4 clubs in the Champions League. From 2015/16 it can have 5, when it holds both the Champions League and the Europa League. From 2024/25 it can have 7: 4 league places, both title holders and the European Performance Spot. A club over the limit takes a Europa League place, as Tottenham Hotspur did in 2012/13.
- UEFA Cup 2004/05 to 2008/09: the second qualifying round and the first round no longer fall on international dates. The spare cup place goes to the next club in the league. Before, it went to the losing FA Cup semi-finalists.
- The Setanta Cup plays from its first edition in 2004/05, with the format of each season. The All-Ireland cup from 2010/11 takes the Northern Irish clubs by league place and pairs clubs from the two leagues in the first round.
- Season news says "is played again" for a cup that comes back, such as the Setanta Sports Cup in 2010/11, and "first time" only for a new cup.

### Club cups outside Europe

- The Club World Cup host place goes to the continent's runner-up when the host's own club wins its continent, as in 2007 and 2008. A 2025 start has the 2025 Club World Cup in its history.
- The Suruga Bank Championship from 2008 to 2019 is the J.League Cup winner against the Copa Sudamericana winner of the year before. Before, every final was Kashima Antlers against Talleres.
- The Intercontinental Cup, the Recopa Sudamericana and the FIFA Intercontinental Cup of 2024 to 2026 take the real continental champions. Australian clubs no longer play the OFC club competition after Australia moved to Asia in 2006.
- With changing rules on, a Mexican guest club that wins the Copa Libertadores no longer plays the Intercontinental Cup, the Club World Cup, the Recopa Sudamericana or the FIFA Intercontinental Cup for South America. The losing finalist takes the place.
- The AFC Champions League, the OFC Champions League and the Oceania Champions Cup no longer stop after a final or after a load.
- South American and Asian club cups no longer play on a FIFA day or within two days before one.

### International football

- Every finals goes on after its last known edition (the World Cup 2030, Euro 2028 and the Olympics 2024), with its qualifying. The Africa Cup of Nations, Copa América, Asian Cup and OFC Nations Cup no longer stop early, and draw screens no longer show dates in 2199 or 2200.
- Announced hosts play: the World Cup 2034 in Saudi Arabia, Euro 2032 in Italy and Türkiye, the Olympics 2028 in the USA and 2032 in Australia, the Africa Cup of Nations 2027 in Uganda, and Mali in 2002 with changing rules on. Every finals in the history names its host.
- Competition history has no year twice, World Cup and Euro years sit in the right year with every Update Team database, and the missing editions of late starts are in. Starts from 2003 on the 3.9.60, 3.9.65 and 3.9.68 data show the real past editions.
- National sides take their nation's new name, such as Serbia, Czechia and Türkiye, in saves from earlier releases too. Kazakhstan moves from Asia to UEFA at the 2002/03 season change, with a news item.
- The "Asian Cup Quals" with European winners is now the UEFA Nations League.
- The OFC Nations Cup plays its real formats: 8 nations in 2 groups of 4 in 2002, 2012, 2016 and 2024, one group of 6 and a two-leg final in October 2004, and one group of 4 home and away in 2008. The hosts play. Where the data lacks the real entrants, the strongest nations of Oceania fill the places. The Asian Cup from 2004 to 2015 has 16 nations.
- World Cup qualifying takes every nation: all of UEFA from 2006, a first round of two-leg ties in Asia so that Brunei, Bhutan and other small nations play, every Oceania nation from 2006, and all 52 African nations. Timor-Leste joins the AFC from 2005/06.
- Qualifying stages play their matchdays in order on FIFA days, with both legs of a tie in one window. A friendly of the same nations within two days of a qualifier is cancelled, with a news item. South America plays on the real days of the 2006 to 2026 cycles, and CONCACAF on the real days of its 2026 qualifying. African groups end in the autumn of the year before the finals.
- The CONCACAF Nations League takes Martinique, Guadeloupe, French Guiana, Saint-Martin, Sint Maarten and Bonaire, as in real life. They play no World Cup, Olympics or Gold Cup. It no longer shares autumn windows with World Cup qualifying, and no longer stops after 2035 in a save from 0.6.
- The 2023-24 CONCACAF Nations League plays its first matchday on 7 September 2023, as in real life. Before, North American friendlies took the window and moved 11 of its matches off the FIFA days.
- In the Euros, Asian Cups and Africa Cups of Nations of 24 nations, and in the Asian Cups, Copa Américas and Gold Cups of 16, two groups no longer meet at one stadium on one day. Every group match plays before the knockout rounds, which start on time.
- With changing rules off, the Confederations Cup starts in mid June and lasts 11 days. Before, it played from 29 July on club and qualifier days, so clubs lost players to it. This is a fault of the original game too.
- Every senior national team gets a new manager when its manager leaves. West African nations no longer have nearly empty senior squads, and under-21 and under-23 sides no longer run out of players. These are faults of the original game too.

### Fixes from your reports

- A new game on the default 2001/02 data with Sweden's extra division no longer stops with the box "swe_second..cpp 656". The data has 11 clubs in one 2001 group, East Svealand, as in real life, and the game now plays that group with 11. On the 3.9.65 data, Katrineholms SK now plays Division 3, as in real life, so a new game no longer stops with the box "swe_second..cpp 643".
- Holiday: "Reject all offers below asking price" now turns down bids below the price, and loan requests follow the holiday settings. These are faults of the original game too.
- A game no longer hangs on the evening of 27 December 2001 at the 2002 World Cup draw. Saves made before it are mended at load. This is a fault of the original game too.
- Club info shows balances above £2,147,483,647 and the right debt, budget and finance news amounts. A very rich computer club keeps a sane transfer budget. International caps and goals no longer start again at 256. These are faults of the original game too.

### Error boxes

- These boxes no longer stop the game:
  - "match_eng..cpp 612" and "match_eng..cpp 652", when a side had fewer than 11 players for a European or international match, for example after a save was loaded
  - "match_eng..cpp 652" in a long season game, when a stadium clash moved a Champions League match before the squad deadline of 1 September. The match now keeps its day, and the weaker match moves.
  - "Cup..cpp 1278", in the Segunda División play-off final and in a Conference League tie
  - "match_man..cpp 1225", on 28 September 2020 in a 2001 game, when one player held 11 squad places of Hertha BSC Amateure
  - "Database..cpp 13471", at every 2005 start
  - "ire_prm..cpp 82", at a 2008/09 start with the 3.9.68 data and all nations
  - "ita_ser_c2a..cpp 551", at the June 2028 season change
  - "conmebol_liber..cpp 663", after you open a long "New rules" news item
  - "Date..cpp 52", in the Conference League Cup of 2024
  - "bel_third..cpp 1642", in a 2070 game, when a period winner of the Belgian Third Division finished in a relegation place. With changing rules on, the real rule now applies: that club loses its play-off place to the next club.
- A long penalty shoot-out with a side down to fewer players no longer reads an empty place. The best player left takes the kick.

### Saves, memory and speed

- Saves no longer fail at 999 parts ("unable to pack the temporary files") in long games. Season changes run after 2099, and saves after 2100 load.
- A 0.6.0 or 0.6.1 save whose finals wait for 2200, or whose qualifying has the old shape, is put right at load and plays the next World Cup, Euros and Olympics.
- Very long games no longer stop with "heap full" around 2222.
- Match results no longer depend on how busy the computer is.
- The Club History screen keeps the last 50 seasons of each club, and fixtures and results of seasons more than 50 years back are deleted. League and cup history stays whole. With this, a save of every nation stays under the 4 GB limit up to 2300. It applies in every game.
- Game days are about 20 % faster on a Mac with Apple silicon, and faster in long games: a July 2025 save plays a day in 6.9 seconds in place of 8.9.
- A new game is faster: 2025/26 with all nations takes 120 seconds in place of 142. The December 2025 database with four nations takes 51 seconds in place of 95.

## 0.6.1 (pre-release)

Fixes for 0.6.0: crashes at save and start, wrong names, and faster new games.

- Saving a season game no longer crashes. The crash came most often at the first save of a 2002/03 game with several nations, and at an autosave on Linux. The save it left gave the box "index..cpp 5809" at its load. The season data now goes into the save before the old save is replaced. A save that a crash left without its season data now gives the box "This save is incomplete. Load an earlier save." in place of 5809.
- Linux: the AppImage no longer crashes after its window opens on Arch, CachyOS and other current systems. It now carries SDL 2.32.10.
- Bosman Edition Options: the Save folder row shows the full path of the folder and has an Open folder button, which opens it in Finder, Explorer or your file manager. The Load Game screen has the same button.
- Select Team and other screens show the division short names of the season, for example PL, CH, L1 and L2 for England from 2004/05, in place of the 2001 names PRM, D1, D2 and D3.
- Attribute groups: a goalkeeper's profile shows the Goalkeeping group in every language, not only in English.
- Assistant: Pick best team no longer puts a player who cannot play into the eleven.
- A new game with all nations is two to three times faster. "Player histories" takes about 25 seconds in place of about 4 minutes.
- The Mac disk image shows the logo once, without a second "Bosman Edition" line under it.
- Choose Season: the 2001/02 tile has three versions of the official data. Each click on the chosen tile changes the version: 3.9.60, the original data, then 3.9.65, the December 2001 update, then 3.9.68, with the summer 2002 transfers. The tile shows the version in use, and Start writes it into the `database` line of `settings.txt`. The default is now 3.9.60. A `settings.txt` that already has a `database` line keeps its version.

## 0.6.0 (pre-release)

<!-- TODO slice 30: the publish date goes in the GitHub release and the posts; bump CMakeLists.txt to 0.6.0 in the same commit. -->

Play any season from 2001/02 to 2025/26 with that season's squads, leagues, cups and rules. In a long game the rules change each July as they did in real life. New lines in `settings.txt`: `season` and `changing_rules`.

- Choose Season (`season`): a screen with a tile for each season from 2001/02 to 2025/26, in rows for the 2000s, 2010s and 2020s. Click the season button at the top of Setup Game to open it. Each season has one database, and its tile starts it, so you never install a database by hand. The details under the tiles show the database and who made it, the European format of the season ("Europe: ...") and the start date. The line "More versions of each season are on the Update Team's forum" has a button that opens champman0102.co.uk.
- A database for every season is in the download: the official 3.9.68 data for 2001/02 and 2002/03, 21 databases of the CM 01/02 Update Team, and our own Summer 2004 and Summer 2006 for 2004/05 and 2006/07 (facts from public sources, ratings by the Update Team). Each season database is zipped and unpacks into the save folder the first time you pick its season. Unpacked, the Update Team databases take about 1.4 GB together.
- Custom database: a tile for a database folder of your own in `Databases`. You choose the folder and say which season it holds, and that season's rules apply. A `database = 3.9.60` or `3.9.65` line in `settings.txt` still starts that data set.
- A new game starts in July of its season. Every person has the right age and year of birth for that July, and contract, joined and history dates fit the season. For seasons before 2010/11, the competition history lists the real winners from 2001/02 up to the start season.
- Leagues by season: the number of clubs, promotion and relegation places, play-offs and splits of each season, for all divisions of the 27 nations in the game. New divisions at tiers the original already plays, such as the 3. Liga, the Primera Federación, Liga 3 and the single Serie C in Italy, and Brazil's state championships. When a season's database lacks a real top or second tier club, the game adds the club with a squad. Your results decide which clubs go up and down.
- Domestic cups by season: replays or none, single-leg or two-leg ties, group stages, four-club super cups, and cups that start, stop or come back.
- Substitutes, bench size, foreign player rules, work permits and transfer windows follow the season. The Brexit work permit rule applies from 1 January 2021 in England, Scotland, Wales and Northern Ireland. With changing rules on, England has no limit of three non-EU players in a match squad, as in real life.
- European cups of each season: two Champions League group stages until 2002/03, then one group stage and a round of 16; UEFA Cup groups from 2004/05; the Intertoto Cup until 2008; the Europa League and the Champions and League Paths from 2009/10; the Conference League and no away goals from 2021/22; a league phase of 36 clubs in all three cups from 2024/25. The places of each nation come from that season's UEFA access list.
- International football by year: the UEFA and CONCACAF Nations Leagues, Euro qualifying with Nations League paths, Euros of 24 nations from 2016, the 24-team Africa Cup of Nations and Asian Cup from 2019, the 2022 World Cup in November and December, the 48-team World Cup of 2026 with its qualifying, the Gold Cup and the Finalissima. The Confederations Cup ends, golden goal goes, Australia moves to Asia and nations carry their name of each year.
- Competitions and rounds carry their name of each season, such as the Championship or the Europa League, with no sponsor names.
- Changing rules (`changing_rules`, on by default): set it for each new game with the Changing rules button at the top of Setup Game or on the Choose Season screen. The save keeps it, and a game cannot change it after the start. With it on, each rule changes in the July it changed in real life, also in a game that starts in 2001/02, and a news item at the start of the season lists the changes. After 2025/26 the changes already announced with a date come in, such as the Championship play-offs for 3rd to 8th from 2026/27, and then the rules stay. With it off, the rules of the start season stay for the whole game. The Game tab of Bosman Edition Options sets the default.
- In a language other than English, the game no longer stops a new game or a load with the error box "Database..cpp 7784" when it uses the 3.9.60 or 3.9.65 data, a season database or a save of these. New games on 3.9.60 and 3.9.65 show club, nation, city, competition and stadium names in your language, from the language files of those versions. Season databases and older saves on 3.9.60 or 3.9.65 keep the English names.
- In a language other than English, a load and a new game no longer show the box "CD Not Required" in that language, for example "CD Non Richiesto".
- Seasons are written in full, for example "2009/10" in place of the original's "2009/0".
- Saves from 0.5.0 and earlier load with changing rules off.
- Setup Game: two new buttons sit at the top. The left one shows the season and its database, and a click opens Choose Season. The right one shows whether the rules change, and a click switches it. Hall Of Fame, Game Credits and Web Sites are on a new Extras screen.
- The start screens have the title "Championship Manager: Bosman Edition".
- Bosman Edition Options: the Screen tab is now called Graphics.
- Attribute groups: a goalkeeper's profile shows One On Ones and Consistency in the Goalkeeping group, and Set Pieces and Technique move into it from the Technical group. Both come from the game's own player data, which the original does not show. Attributes that your scouts do not know show as " -", as the others do.
- Bosman Edition Options: the Save folder row has a Change button that opens your computer's folder picker. The game uses the new folder from the next start. Saved games already in the old folder stay there.
- Team views: Best XI and Squad depth are one screen, in the look of the original game. The pitch lies on its side with a card for each of the 11 places: the Best XI player, then the next two for that place. Half stars, from 0.5 to 5, show how well each player fits the place. Every formation fits on the pitch in classic and widescreen. A player who cannot play the next match has a tag: Inj (injured), Int (away with the national team) or Out (banned, on loan or not allowed by the rules).
- Compare players has the look of the original comparison screen: the higher number of each pair in that player's colour, green or red, an equal pair in yellow, and the polygon on a dark panel.
- HD mode is gone. On a Mac screen it showed bars around the picture and small text, while widescreen fills a large screen with sharp, bigger pixels. A `display = hd` line in `settings.txt` now gives widescreen.
- Display report: if the picture looks wrong, press Ctrl+Shift+D (Cmd+Shift+D on a Mac). The game saves `display-report.txt` in the save folder, puts the same text on the clipboard and shows where the file is. The report lists your system, your screens and their scale, the window size and how the game scales its picture, and the last 10 changes of the window. The game also writes the file at each start and quit. Paths in it show `<user>` in place of your account name.
- Windows: text stays sharp with display scaling at 125 % or 150 %. Before, Windows stretched the whole window and blurred it. A new window opens at the size of the picture, as at 100 %.
- The picture fills the window at any size, larger or smaller, and keeps its shape. Black bars show only where the window has a different shape from the picture. With `sharp_picture` on, letters keep an even shape at every size. With it off, the picture is smoother and softer.
- The window cannot get smaller than the picture: 800x600 in classic mode, 1280x800 in widescreen. On a smaller screen the limit is the largest window that fits.
- Classic mode in full screen (F11, or Cmd+F on a Mac) changes the screen to 800x600, as the original does, when the screen has that mode. Macs have no 800x600 mode, so there the picture fills the screen.
- On a screen less than 800 pixels high, widescreen shows a message once at start, and the Display mode row of Bosman Edition Options says that classic mode suits the screen better.
- Saves from 2 GB up to 4 GB now load. The game wrote a wrong position for every part of a save after 2 GB, so the load stopped at an error box. This is a fault of the original game too. A save above 4 GB is refused: a box says the game is not saved, and your earlier save stays.
- Long games with many leagues no longer run out of memory. A Select All game from 2001 stopped in its ninth season, also in the base game.
- Full player search with every league loaded lists every player again. Before, it said "1 player found" over an empty list.
- The comparison screen fits long names, clubs and positions in their boxes, so no text is cut.
- Nations by season: the Netherlands Antilles become Curaçao from 2011/12, Kosovo plays in UEFA from 2016/17, and an old Yugoslavia record that some databases keep beside Serbia no longer plays, for example at the 2022 World Cup.
- Long test games in every start season, with a save and a load each July, found faults in the new season code. These are fixed in this release: error boxes on match days, in discipline, in staff and database checks and in the draws of European and World Cup qualifying, a Europa League play-off with no second leg, and saved season games that did not load. Among them: national squads short of players after a load, bans after Australia moved to Asia in 2006, a ban held by a player on loan between an Irish and a Northern Irish club in the All-Ireland cup, and empty places in some domestic cups, such as the 2003 Polish Cup and the Leinster and Munster Senior Cups.
- A game that starts after World Cup or Euro qualifying began plays the real finals: the nations that really qualified and the real play-off ties of the World Cups 2006 to 2022 and the Euros 2004 to 2024.
- World Cup qualifying in South America and CONCACAF ends on each cycle's real last matchday, so the play-offs between confederations always have both teams. A game that starts in 2025/26 has the real 2026 finalists and play-off results, and in the 2030 qualifying none of the six hosts plays a qualifier.
- Euro 2028 qualifying has its real play-off paths in March 2028, from the 2026-27 Nations League. The two best-ranked of England, the Republic of Ireland, Scotland and Wales that do not qualify take the host places.
- Bonaire, French Guiana, Guadeloupe, Martinique, Saint-Martin and Sint Maarten stay out of the CONCACAF Nations League, because the databases give them no players. Before, a load could draw them in.
- An under-21 squad no longer takes players from the senior squad, or from any other national squad, that plays in the same international window, so under-21 sides no longer go into a match short of players.
- Outside the base game, the MLS play-offs use extra time and penalties, as real MLS has since 2000. A level league match stays a draw. Before, a play-off shootout could go on until the game stopped with the error box "match_pl..cpp 4885".
- The Champions League and Europa League preliminary rounds play on their real days in late June, with extra time and penalties. Before, a level match was replayed and the winner's first qualifying tie came a week late.
- Europa League clubs register their squads again in February for the knockout rounds. Before, a club that came through the qualifying rounds kept its August squad and could stop the game with only 10 fit players.
- The Italian leagues no longer stop after 2015/16. Before, Serie A and Serie B stayed at 2015/16 and Italy never started the next season.
- The Coppa Italia Serie C fills all 32 places of its knockout round, with the seeds in their places. With changing rules on, it also plays on when its group size changes. Before, two places stayed empty and the cup could stay at one edition.
- With Serie C2 in the game, a Serie C2 group that ends in a later season no longer leaves an empty league that stopped the game with an error box.
- League One keeps 24 clubs in 2019/20. Bury left after the season start, and the division had stayed at 23 for the next seasons, so the FA Cup and the League Cup each lacked a club.
- League Two keeps 24 clubs from 2025/26. Before, a 2025/26 start could give it 20.
- In a 2025/26 start the FA Cup takes the right number of clubs from the National League and below. Before, a first-round tie could have an empty place or a club with a garbled name, and the game stopped with an error box.
- The four-club Supercopa de España: when one club holds two places, the next club in the league takes the second place, as in the real competition. Before, that semi-final never ended and the cup stayed at one edition.
- In a 2002/03 start, the Turkish Category B plays in three regional groups, as in the real season. Each group sends one club up and two down, and clubs keep their group at the season change. The 2002/03 club moves leave the Category B clubs in their groups, and a new club goes to its real group.
- The Leinster and Munster Senior Cups and the Polish Cup pick a fresh list of clubs each season. Before, a club from the last edition could stay in a place, and the cup stopped with the error box "Cup..cpp 1187" or stayed at one edition.
- The Russian Cup starts its new edition when the last final is played after 1 June. Before, it could stay at the old edition for a season.
- The Segunda División play-off is played every season. Before, the game often found no time for it and sent the third club up. The final has extra time and penalties on its second leg, and from 2020/21 its two legs are as far apart as in real life. Before, a level final went to replays and the game stopped with the error box "Cup..cpp 1450".
- The knockout rounds of the Champions League, Europa League and Conference League play on their real dates of each season from 2003/04. Seasons after the last published calendar keep the same weekdays.
- International friendlies are played on the real FIFA days, and the qualifying, group and league-phase matches of the UEFA cups on their real days. UEFA ties are no longer moved because players are away for friendlies, so the draws of the next round are no longer late. The first season of a game that starts in 2001/02 keeps the original game's calendar, so that the base game stays the same.
- World Cup and Euro qualifiers move to the nearest real FIFA match day that has no continental club match. In a game that starts in 2001/02 this begins in the second season.
- The Champions League qualifying draws keep the Champions Path and the League Path apart, as in the real competition, and the play-offs have both paths from 2018/19. The draws are made one path at a time, so they come out in a different order from older builds.
- The Danish leagues start each season on time. Before, a match that international call-ups, a fixture clash or a TV move pushed past the start day made the leagues wait a year.
- With changing rules off, the South group of the Greek Super League 2 starts each season. Before, it stayed in its first season.
- A player on loan keeps his contract with his parent club and stays on loan when his loan club takes him off its staff. Before, the game could stop with the error box "contract_manager..cpp 5941" when the loan ended.
- The club tables of each season point at the right club in 55 places, for example Yimpaş Yozgatspor in the 2002/03 Turkish Category A and Dijon FCO in France.
- A club no longer rotates its best players out of a match when fewer than 25 of its squad can play it. Before, AJ Auxerre could field 7 players in a 2005/06 UEFA Cup match, and the game stopped with an error box.
- When an AI club loans players out, it keeps at least 3 goalkeepers, so its reserve side always has a keeper. Before, a reserve match with no goalkeeper could stop the game with an error box.
- Japan's Emperor's Cup ends each edition and starts the next. Before, a tie made after its date could fall after the next round's match day, and the cup stayed at one edition.
- A Competitions menu with more than 24 competitions, such as Brazil's with its state championships, has a sub menu More for the rest.
- More faults found by the long test games are fixed: error boxes when a national squad named the same player twice, on the news of an EFL Trophy group match, at the 2019 Confederations Cup with changing rules off, at the 2022 Finalissima and in the Portuguese Cup of a 2021/22 start, and a Club World Championship whose knockout never ended after a load.
- A season database copied into the game's own Data folder starts a Select All game without the error box "Database.cpp 1844".
- The Linux download is now `CM0102-Bosman-Edition-<version>-x86_64.AppImage`, and it has the icon file that AppImage tools look for.
- The fix of 0.5.1 is in every download: a game with Brazil selected can save the São Paulo, Gaúcho, Goiás and Paraná state championships.
- The code is under GPL-3.0 from 0.6.0. Releases up to 0.5.0 stay CC0. The function and data maps stay CC BY 4.0.
- Same game: the base game is 2001/02 with the 3.9.68 data and changing rules off. In the base game the season code changes nothing: the same clicks with the season code turned off by a test switch give the same save. The new Setup Game moves the original's memory blocks, so a base game of 0.6.0 is not byte for byte the same as one of 0.5.0 (Q342).
- Limits in 0.6.0: the bench and the substitutes stay at 9 or fewer, because the match engine holds 9, so a bench of 12 plays as 9. The MLS stays at 12 clubs. Careers that a season database never updated stop in 2001. In databases from September 2009 most players joined their club in 2008. The award history keeps the years of the data. The 2000/01 Portuguese Second League runner-up is Varzim (three clubs ended on 64 points).

## 0.5.0 (pre-release)

An options box in the game, positions on the tactics screen, foreign players in colour, and fixes for rare faults of the original. Each new option has a line in `settings.txt` in the save folder.

- Bosman Edition Options: a box with two tabs, Screen and Game, that shows every option of the Bosman Edition. It opens from Setup Game and from Game Options in the side bar, after Game Settings. A change saves to `settings.txt` at once and keeps your own lines and comments. Display mode, All sound and Save folder take effect at the next start. Save folder is shown, and you change it in `settings.txt`.
- The tactics screen shows each player's position in the Condition view, before and during a match, in classic, widescreen and HD (`match_columns`).
- Foreign players in colour (`foreign_colours`): on the squad and tactics screens, the names of players who count against the foreign player limit of the club's next league or cup match are light violet. It works for every club. The game's "Fgn" status stays.
- Full player search (`full_player_search`, off by default): the player search lists every player, also players your manager does not know. With it on, a search changes later days, as opening a screen does.
- Fixes for rare faults of the original game: a news item with no text, some transfer offers, squad updates, staff records after a part-exchange deal, the competition table and the World Cup draw. The original crashed on these. The port went on with wrong data. Now it skips the bad item.
- The game no longer uses a full processor core while it waits for input (about 1% in place of 100%). Matches and holidays play the same.
- The Database button shows 3.9.68, 3.9.60 and 3.9.65 first, then your databases from the oldest season to the newest, each with its season and folder name, for example "2025/26 (May 2026)". In classic mode a long folder name shows the season alone. A folder whose name has no month and year comes last.
- The intro shows the CM 01/02 logo with every database, and a database in `Databases` no longer replaces the pictures of the intro.
- A new Bosman Edition logo, on the site and in the intro.

## 0.4.0 (pre-release)

New screens and an HD layout. Each new option has a line in `settings.txt` in the save folder. No feature changes the game's decisions while you make the same clicks.

- Team views on every squad screen, from the Assistant menu (`squad_views`): your best XI on a pitch in any of the game's formations, with the subs and other players beside it, and a squad depth table with the best players for each position. The numbers are the assistant's score for each position, from the attributes the profile shows. Players who cannot play the next match are left out of the eleven and shown in grey in the table.
- A player comparison with a polygon of eight strengths, on the original's comparison screen and on the Compare tab. Two goalkeepers get goalkeeper strengths.
- Attribute groups on the player profile (`attribute_groups`): Technical, Mental and Physical, with Goalkeeping first for goalkeepers.
- Print to PDF: Print To shows PDF in place of Printer, and each print saves a PDF in the `Prints` folder of the save folder. File still saves a text file, and Both saves both.
- An HD layout at 1920 × 1080 (`display = hd`). A new window shrinks to fit the screen.
- The CM 01/02 Update Team's databases from 2003 to 2026: copy a database into a folder in `Databases` in the save folder, and choose it with the Database button. The game still starts in July 2001.
- Message boxes are centred in widescreen and HD.
- The box "CD Not Required" no longer shows after a new game or a load.
- A clickable name no longer shows its underline under a box that covers it.

## 0.3.1 (pre-release)

A fix for the Assistant menu on the squad screen.

- Pick best team, Pick most match-fit team and Pick under-21 team now score each player by the key attributes of the position, as the profile shows them. For example, a goalkeeper is scored on Handling, Reflexes, Agility, Positioning and Jumping. The score also counts how well the player fits the position.
- The assistant finds the best eleven as a whole. In 0.3.0 it filled one position at a time, so a natural right back could sit on the bench while a centre back played at right back.
- A player plays out of position only when no free player suits that position.

## 0.3.0 (pre-release)

New features and a Linux version. Each option has a line in `settings.txt` in the save folder. No feature changes a match result or a decision of the game, except the team that you or your assistant pick.

- Linux: one AppImage for 64-bit Linux. It also runs on the Steam Deck in desktop mode.
- A Database choice on the Setup Game screen for each new game: the original 2001/02 data (3.9.60), the 2001/02 December update (3.9.65) or the 2002/03 data (3.9.68, the default). The `database` line holds the choice.
- An Assistant menu on the squad screen picks the best team, the most match-fit team or an under-21 team. It weighs each player's current ability and how well the player fits each position.
- Holiday until next match: an item below Go on Holiday ends the holiday on the day before your club's next match.
- A Fastest commentary speed, twice as fast as Very Fast. As in the original game, the commentary speed can change how a match plays out.
- Save Game suggests a name with your club and the game date, for example "Arsenal 2001-07-20".
- The game keeps the last three copies of each save in a `backups` folder next to your saves (`save_backups`).
- The window opens at the size and place it had when you quit, in full screen if it was (`remember_window`).
- Condition and morale have colours: red below 75%, amber from 75 to 89% and green from 90% (`condition_colours`).
- Windows: Ctrl+[ is Back and F11 switches full screen.
- Fixes: the underline in the news covers the whole name, and the "Replace this file?" text is centred in widescreen.

## 0.2.0 (pre-release)

Comfort features and sound. Each feature has a line in `settings.txt` in the save folder, and `off` turns it off. No feature changes a match result or a decision of the game.

- Sound: the game's own sounds play. The game is silent while its window is not in front. `sound = off` mutes it.
- The space bar presses Continue Game.
- Back: Cmd+[ on a Mac, or the back button of a mouse. Escape closes a popup. Cmd+F switches full screen on and off.
- The picture scales in whole pixels, so text stays sharp.
- `save_folder` keeps saved games in another folder, for example in iCloud Drive.
- Player and club names show a hand and an underline under the mouse. A click opens the profile.
- Attribute numbers have colours by value: 1 to 5 red, 6 to 10 amber, 11 to 15 light green, 16 to 20 bright green.
- In a match, widescreen only: the tactics screen shows each player's positions and condition beside the match rating, and the Player Ratings view shows the condition of both teams.
- Days pass faster while you are on holiday.
- Widescreen fixes: longer texts fit, and the name buttons in the news land on the names.

## 0.1.0 (pre-release)

The first test release.

- Championship Manager 01/02 at version 3.9.68: the 2008 free release with the last official patch. No fan patches or fan databases.
- Mac: one app for Apple silicon and Intel, macOS 11 or later.
- Windows: an installer for 64-bit Windows 10 and 11, with Start menu and desktop shortcuts.
- Widescreen at 1280 × 800 by default. The window can be resized and keeps the shape of the picture.
- The old 800 × 600 layout: open `settings.txt` in the save folder and change the line to `display = classic`.
- Saves and settings live in `~/Library/Application Support/cm0102` on a Mac and in `%APPDATA%\cm0102` on Windows.

Not in this release: sound, network play and printing.
