# The Setanta Cup in the game (0.6.2 item 145)

Date 2026-11-05. The All-Ireland cup (`new_all_ireland`, rules table `wal-nir-irl.tsv`) now plays every real edition from its first, the Setanta Cup of 2005 (row 2004/05). Before, the game first made the cup in 2010/11, because it did not play the group editions (`2026-10-18-cups-more.md`, section 4).

## In the game

| Row | Real edition | Clubs | Groups | Knockout | Group days | Final |
|---|---|---|---|---|---|---|
| 2004/05 | 2005 | 6: top 2 of each league and the FAI Cup and Irish Cup winners | 2 of 3, pots by strength | the 2 winners in the final | 15 March to 16 May 2005 (6 of the 10 real days) | 21 May 2005 |
| 2005/06 | 2006 | 8: top 4 of each league | 2 of 4, two of each league in each | top 2, semi-finals (winner v runner-up of the other group, winner at home) | 20 February to 27 March 2006 | 22 April 2006 |
| 2006/07 | 2007 | 8: top 4 of each league | as 2005/06 | as 2005/06 | 26 February to 16 April 2007 | 12 May 2007 |
| 2007/08 | 2008 | 8: top 4 of each league | as 2005/06 | as 2005/06 | 26 February to 21 April 2008 | 10 May 2008 (inference, see below) |
| 2008/09 | none | | | | | |
| 2009/10 | 2009-10 | 9: top 4 of each league and the holders | 3 of 3, pots by strength | the 3 winners and the best runner-up, two-legged semi-finals | 29 August 2009 to 13 March 2010 | 15 May 2010 |
| 2010/11 on | 2011 on | the knockout of 0.6.0 (slice 21c) | | | | |

How it is built:

- The rows are in `season_cups_more.c`: `new_cups` (the clubs: `picks` per league by last place, `extra` for the cup winners or the holders) and `group_formats` (the groups, the group days, the knockout and the draw day). The group stage is the engine of `season_cup_groups.c`, which the Scottish League Cup, the EFL Trophy and the Turkish Cup already use. The cup is an ItaSuper object (constructor 0x65f670) on the Cup base, so its slots are the ones that engine wraps: slot 10 0x5a8f60 (advance_stage), slot 12 0x519a90 (the honours from the knockout stage), slot 6 0x51fc00 (load), slot 17 0x519890 and map_fixture_tree 0x518790. The constructor (0x65f739) and start_new_season (slot 35 0x858e70, called at 0x65fd7d) ask for the round table with stage -1 and the cup's own +0x3c, so the wrap of create_round_table 0x65f920 (`season_super_cups.c`) calls `season_cup_groups_table` after the new cup's rounds.
- The class's round table stays, but a group season does not play it: Cup::update_for_date 0x51f890 draws no base round once +0xb1 is set (0x51fa01). Its dates are the final, the semi-finals and the group days.
- The clubs are picked on the draw day, as in real life. The cup also picks when its season is made (the class's constructor and start_new_season need a list), but that pick comes too early: the module makes the cup in the entry of the Republic of Ireland (20), before the Northern Irish Premier Division records its final places (NirPrm::start_new_season 0x78ee30, chain 0x689c20, League::record_final_positions 0x68aa30, which writes club +0x5b and +0x5f at 0x68aa5d). Found by the code review of this item: the first run took Northern Irish clubs in list order. On the draw day both leagues hold the places of the season just ended. The game's rows put the League of Ireland season of a calendar year in the row of the season before (row 2003/04 is the 2004 season), so the places are those of the 2004 League of Ireland and of the Irish League of 2003/04, as in real life. The cup winners are the last winners of the FAI Cup (key 0x9cf840, "Irish Senior Challenge Cup", the cup of Ireland in key_nation_build_table 0x667150) and of the Irish Cup (0x9cf8c0), read with comp_util_get_latest_placed_club 0x4afd80. The holders are the last winner of the cup's own record. A club with a place already gives its place to the next club of its league (the holders' place: the next club of the League of Ireland).
- The knockout editions from 2010/11 (0.6.0) had the same fault (open item 148). They pick again on the draw day of their first round: a wrap of Cup::draw_round 0x51c410, which Cup::update_for_date 0x51f890 calls on that day (0x51fa7d), writes the new pick into the same slots of the team list before the draw. map_fixture_tree 0x518790 only clears byte +5 of each entry (0x5188c9) and keeps no club pointer, so the tree stays. The two leagues now take turns in the list by place, so the byes go to the first two of each league: the two champions, as in 2011, and the two runners-up where real life had the two cup winners (before, all four byes went to League of Ireland clubs). The first round of 2010/11 to 2013/14 pairs one League of Ireland club with one Irish League club in each tie, as in the real editions of 2011 to 2014 (rows with `NEW_CUP_PAIRED`). The wrap of Cup::draw_round draws that round by league before the original runs: each league's clubs in a random order (utils_random 0x90cfc0), tie t gets the t-th of each, the home side at random. A wrap of Cup::randomize_round_teams 0x51e220 then skips the original's open shuffle for that one call, and draw_round goes on (home sides, second legs, the draw news). The nation draw 0x51d9f0 of the original does not fit, because it keeps clubs apart by nation and Derry City, a League of Ireland club, is of Northern Ireland. The later rounds stay open draws, as in real life (2011: Cliftonville v Crusaders; 2013: Derry City v Drogheda United). A game with one of the two nations keeps the open draw.
- 2005/06 to 2007/08 draw each league's clubs in their own pots (`nation_pots`), so each group gets two of each league.
- The group draw waits for the real draw day (`draw`): 13 December 2004 (not found, inference), 12 December 2005, 7 December 2006, 17 January 2008, 28 July 2009.
- The news: "The Setanta Cup is played for the first time." in 2004/05 (it is no longer in item 135's list of competitions played before the table), "not played in the 2008/09 season" and "played again in the 2009/10 season". The sentences of every new cup now name the game season: "not played in 2008" read as if the 2008 edition had not been played.
- A game with only one of the two nations has no clubs of the other league: a group edition is then not made (the knockout from 2010/11 keeps its rule of 0.6.0, which fills the places from the other league).
- Changing rules off: the start season's format every season (open item 131), as before.

Decisions taken by Max's rule "do whatever real life does", with the closest sourced rule where a fact has one source or none:

- The entry rule of 2006 to 2008 is not in two sources. The counts (4 of each league) are; the game takes the top four of each league.
- The 2008 edition was planned at its draw (17 January 2008) with three match days to mid-April, three in September and the knockout in October; it ended on 1 November 2008. The game plays one edition a season, and the season change starts the cup's next season (with changing rules off, a 2007 game plays an edition every season), so an edition cannot run on into the autumn. A gap: the 2008 days after 14 April and its knockout dates are an inference from 2007.
- The 2009-10 group days are those of the schedule of the draw of 28 July 2009 (Irish Times): 29 August, 19 September, 3 October and 9 November 2009, 27 February and 13 March 2010.
- 2009-10 gaps: the Irish League clubs were its 1st, 2nd, 5th and 6th of 2008/09 (Glentoran, Linfield, Coleraine, Cliftonville), for a reason not found; the game takes the top four. The draw of July 2009 took the 2008 League of Ireland table, while the game's draw, after its season change, takes the season just ended (row 2008/09, the 2009 season). The pots by strength keep the two champions apart; the real draw put Bohemians and Glentoran in one group (draw rule not found).
- Both semi-finals of an edition play on one day (one date a round); the real ties were on two days (2006: 10 and 11 April, 2007: 23 and 30 April).
- Withdrawals and expulsions (Shelbourne 2007, Derry City and Cork City 2010) are outcomes, not rules (Q272).
- Extra time and penalties in every knockout tie: stated for 2006 and 2007, not found for the other editions.
- The paired first round of 2011 to 2014: the rule is in two sources for 2011 (BBC 8661137, Irish Times format, Irish Examiner draw) and 2013 (RTE draw and details, BBC 20634582, The42). For 2012 no source states the rule, and for 2014 only Wikipedia states it (the Setanta page it cites says only that four clubs of each league went into the pot). In both seasons all four first-round ties of two publishers are one club of each league (2012: Wikipedia, Irish Examiner; 2014: Wikipedia, BBC 25307928), and no source gives another rule, so the game pairs them too. A gap: the 2012 and 2014 rule rests on the ties.
- The byes of 2011 to 2013 (the two champions and the two cup winners by the 2011 announcement; in practice the holders and other seeds, Crusaders and Glentoran in 2011 for a reason not found) are not built: the game gives them to the first two of each league. The 2014 edition (8 clubs, no byes) had Ballinamallard United and Coleraine in place of Cliftonville and Linfield, who declined (an outcome, Q272).

## Research

All pages read on 5 November 2026 with curl. Source keys:

- W05 https://en.wikipedia.org/wiki/2005_Setanta_Cup
- W06 https://en.wikipedia.org/wiki/2006_Setanta_Sports_Cup
- W07 https://en.wikipedia.org/wiki/2007_Setanta_Sports_Cup
- W08 https://en.wikipedia.org/wiki/2008_Setanta_Sports_Cup
- W0910 https://en.wikipedia.org/wiki/2009%E2%80%9310_Setanta_Sports_Cup
- W11 https://en.wikipedia.org/wiki/2011_Setanta_Sports_Cup
- WM https://en.wikipedia.org/wiki/Setanta_Sports_Cup
- R https://www.rsssf.org/tablesa/alliercuphist.html
- BBC n means http://news.bbc.co.uk/sport1/hi/football/irish/n.stm (old BBC Sport NI pages, still live)

All Wikipedia keys (W..) count as one publisher. "Two sources" below means two different publishers.
The pages en.wikipedia.org/wiki/2009_Setanta_Sports_Cup and 2010_Setanta_Sports_Cup do not exist as season pages. The 2009 and 2010 matches are in W0910.

### 2005 (Setanta Cup)

1. Clubs: 6. LoI: Shelbourne, Cork City, Longford Town. Irish League: Linfield, Glentoran, Portadown. Sources: W05, R, BBC 4213851.
2. Qualification: Shelbourne as 2004 LoI champions, Longford Town as 2004 FAI Cup winners (W05, BBC 4213851). Cork City as 2004 LoI runners-up, Linfield as 2003-04 IL champions, Glentoran as 2003-04 Irish Cup winners, Portadown as 2003-04 IL runners-up: W05 only. R says only that the six were the European qualifiers of each league. Per league: 3 and 3. Rule from WM: top two in each league plus the FAI Cup and Irish Cup winners (WM is the same publisher as W05).
3. Groups: 2 groups of 3, home and away, 4 matches per club (W05, BBC 4213851, R). Group 1: Linfield, Glentoran (IL), Longford Town (LoI). Group 2: Shelbourne, Cork City (LoI), Portadown (IL) (W05, BBC 4213851, R). The mix was not fixed: Group 1 was 2 IL plus 1 LoI, Group 2 was 2 LoI plus 1 IL. W05 says the draw kept the two league winners apart (one source).
4. Through: the group winners only, to a single-match final (W05, BBC 4213851). Final venue rule (Tolka Park unless two NI clubs, then Windsor Park): W05 only. Extra time and penalties rule for 2005: not found.
5. Dates: first match 15 March 2005, last group match 16 May 2005 (W05, R, BBC 4352395, BBC 4557065). Group match dates: 15 Mar, 22 Mar, 28 Mar, 4 Apr, 11 Apr, 19 Apr, 25 Apr (two matches), 2 May, 10 May, 16 May (two matches) (W05, R). No semi-finals. Final 21 May 2005 at Tolka Park, Dublin (W05, R, BBC 4568525).
6. Final: Shelbourne 0-2 Linfield. Winner Linfield, runner-up Shelbourne (W05, R, BBC 4568525).

### 2006 (Setanta Sports Cup)

1. Clubs: 8. LoI: Cork City, Drogheda United, Derry City, Shelbourne. IL: Linfield, Glentoran, Portadown, Dungannon Swifts. Sources: W06, R, BBC 4522784, Irish Examiner https://www.irishexaminer.com/sport/soccer/arid-30234714.html, BBC 4716974. Per league: 4 and 4.
2. Qualification: Cork City as eircom League champions and Glentoran as IL champions (Irish Examiner above). Drogheda United as FAI Cup holders (Irish Examiner above, RTE https://www.rte.ie/sport/2006/0411/drogheda.html). Linfield, Derry City, Shelbourne, Portadown, Dungannon Swifts: route not confirmed. WM says the League of Ireland Cup and Irish League Cup winners were added in 2006 (one source).
3. Groups: 2 groups of 4, home and away, 6 matches per club (W06, R, Irish Examiner fixture list). Group A (R calls it Group 1): Cork City, Drogheda United (LoI), Portadown, Dungannon Swifts (IL). Group B (R: Group 2): Linfield, Glentoran (IL), Derry City, Shelbourne (LoI) (W06, R, BBC 4522784, Irish Examiner). Each group was 2 LoI plus 2 IL (all four sources). Whether the draw required this: not stated in any source.
4. Through: top two per group (W06, R, Irish Examiner "introduced semi-finals"). Semi-finals: Group A winner v Group B runner-up and Group B winner v Group A runner-up, group winners at home, single match (W06 states the rule; R shows Cork City v Shelbourne at Cork and Linfield v Drogheda at Windsor Park, which fits). Extra time then penalties, no replay: W06 states it. The final went to extra time (R, RTE https://www.rte.ie/sport/2006/0422/droghedaunited.html, Irish Times https://www.irishtimes.com/sport/leech-strikes-late-for-drogheda-1.1042701).
5. Dates: draw 12 December 2005 (W06, BBC 4522784). Group match days: 20 Feb, 27 Feb (Group A) / 28 Feb (Group B), 6 Mar (A) / 13 Mar (B), 14 Mar (A) / 20 Mar (B), 21 Mar (A) / 27 Mar (B), 27 Mar (A) / 3 Apr (B). First 20 February 2006, last 3 April 2006 (W06, R, Irish Examiner fixture list, BBC 4734252, BBC 4874190). Semi-finals 10 April 2006 (Cork City 2-0 Shelbourne) and 11 April 2006 (Linfield 0-1 Drogheda United) (W06, R, RTE 0411). Final 22 April 2006, Tolka Park, Dublin (W06, R, RTE 0422, Irish Times).
6. Final: Cork City 0-1 Drogheda United after extra time. Winner Drogheda United, runner-up Cork City (W06, R, RTE 0422, Irish Times).

### 2007 (Setanta Sports Cup)

1. Clubs: 8. LoI: Drogheda United, Derry City, Cork City, St Patrick's Athletic (replaced Shelbourne). IL: Linfield, Glentoran, Portadown, Dungannon Swifts. Sources: W07, R, BBC 6218596, BBC 6315425.
2. Qualification: the draw had the same eight clubs as 2006 (W07, BBC 6218596 lists the same eight). Drogheda United as holders (BBC 6218596). Shelbourne as Premier Division champions (BBC 6315425). Shelbourne withdrew on 30 January 2007 because of financial problems (W07, BBC 6315425). St Patrick's Athletic took their place (W07, BBC 6315425). W07 says they got it as 2006 FAI Cup runners-up, ahead of Sligo Rovers (one source). Other routes: not confirmed.
3. Groups: 2 groups of 4, home and away. Group A / Group 1: Linfield, Glentoran (IL), Drogheda United, Derry City (LoI). Group B / Group 2: Portadown, Dungannon Swifts (IL), Cork City, St Patrick's Athletic (LoI) (W07, R, BBC 6218596, BBC 6315425). Each group 2 LoI plus 2 IL. Fixed-mix rule: not stated.
4. Through: top two per group. Semi-finals: Group 2 winner v Group 1 runner-up, Group 1 winner v Group 2 runner-up (BBC 6218596, W07). Group winners at home (W07; R shows St Patrick's Athletic v Drogheda at Richmond Park and Linfield v Cork City at Windsor Park). Single match, extra time then penalties (W07 rule; R shows the semi-final St Pat's 0-1 Drogheda aet and the final on penalties).
5. Dates: draw 7 December 2006 (W07, BBC 6218596). Group match days: 26 Feb, 5 Mar, 12 Mar, 19/20 Mar, 26 Mar, 2 Apr, 16 Apr (Group 2) / 17 Apr (Group 1). First 26 February 2007 (W07, R, BBC 6315425, BBC 6394887), last 17 April 2007 (W07, R). Semi-finals 23 April 2007 and 30 April 2007 (W07, R). Final 12 May 2007, Windsor Park, Belfast (W07, R; the date also in BBC 6218596).
6. Final: Linfield 1-1 Drogheda United after extra time, Drogheda won 4-3 on penalties. Winner Drogheda United, runner-up Linfield (W07, R).

### 2008 (Setanta Sports Cup)

1. Clubs: 8. LoI: Drogheda United, Cork City, Derry City, St Patrick's Athletic. IL: Linfield, Glentoran, Cliftonville, Dungannon Swifts (W08, R, BBC 7194404, BBC 7707102, Irish Times draw https://www.irishtimes.com/sport/setanta-sports-cup-1.929855).
2. Qualification: Drogheda United as eircom League champions (BBC 7194404, Irish Times https://www.irishtimes.com/sport/soccer/cliftonville-first-up-for-drogheda-1.1219943). Linfield as IL double winners (BBC 7194404). Cliftonville as third in the IL (BBC 7194404). Derry City through a play-off of the 2007 League of Ireland Cup winners against the 2007 First Division winners, Derry City 2-1 Cobh Ramblers on 19 November 2007 (WM, BBC 7102860). Cork City, St Patrick's Athletic, Glentoran, Dungannon Swifts: route not confirmed.
3. Groups: 2 groups of 4, home and away. Group 1: Drogheda United, Cork City (LoI), Cliftonville, Dungannon Swifts (IL). Group 2: Derry City, St Patrick's Athletic (LoI), Linfield, Glentoran (IL) (W08, R, BBC 7194404, Irish Times draw). Each group 2 LoI plus 2 IL. WM says two clubs from each league were drawn in each group (one publisher; the draw lists of BBC and Irish Times fit it).
4. Through: top two per group. Semi-finals: Group 1 winner v Group 2 runner-up and Group 2 winner v Group 1 runner-up (Irish Times draw page lists "Winner Group Two v Runner-up Group One"; W08). Group winners at home (W08; R and BBC 7668443 show Drogheda v Glentoran at United Park and Derry v Cork at the Brandywell). Single match. Extra time and penalties: not stated for 2008.
5. Dates: draw 17 January 2008 (W08, Irish Times draw page dated 18 Jan 2008). Split schedule: three rounds by mid-April, a break, then the rest from 1 September (W08, Irish Times draw, RTE preview https://www.rte.ie/sport/soccer/2008/0220/228324-setantapreview/). Group match days: 26 Feb, 10 Mar, 18 Mar, 7 Apr, 14 Apr, 15 Apr, 1 Sep, 2 Sep, 15 Sep, 22 Sep, 23 Sep, 6 Oct. First 26 February 2008 (W08, R, Irish Times draw). Last 6 October 2008 (W08 match list, R). Semi-finals 13 October 2008 and 14 October 2008 (W08, R, BBC 7668443). Final 1 November 2008, Turners Cross, Cork (W08, BBC 7704471, BBC 7679430). The plan at the draw was semi-finals 6 and 7 October and the final 18 October (Irish Times draw, BBC 7194404).
6. Final: Cork City 2-1 Glentoran. Winner Cork City, runner-up Glentoran (W08, R, BBC 7704471).

### 2009 (calendar year)

No separate 2009 edition was played. R lists finals for 2008 and 2010 only. The edition that started in 2009 is the 2009-10 edition below (W0910, R, Irish Times 2009 draw). Its group matches from 29 August to 10 November 2009 fell in calendar 2009.
The draw, set for 25 June 2009, was postponed at the request of Setanta Sports because of the company's problems (Irish Examiner https://www.irishexaminer.com/sport/soccer/arid-30415897.html, W0910, WM). The competition was confirmed on 19 July and the draw made on 28 July 2009 at FAI headquarters, Abbotstown (W0910; RTE https://www.rte.ie/sport/soccer/2009/0728/252569-setanta/ and Irish Times https://www.irishtimes.com/sport/soccer/draw-for-expanded-setanta-cup-made-1.1228320 confirm the Abbotstown draw). The format change (group stage August to November, semi-finals and final in spring) had been announced in March 2009 (BBC https://news.bbc.co.uk/sport2/hi/northern_ireland/7952255.stm).
Cancellations: I found no source for a cancelled 2006 edition. The 2006 edition was played (W06, R, BBC, RTE, Irish Times). R lists 2015 as cancelled, which is outside the range.

### 2009-10 (Setanta Sports Cup, final in 2010)

1. Clubs: 9. LoI: Cork City, Sligo Rovers, Bohemians, St Patrick's Athletic, Derry City. IL: Cliftonville, Glentoran, Coleraine, Linfield (W0910, R, RTE 2009 draw, Irish Examiner https://www.irishexaminer.com/sport/soccer/arid-30420430.html). Per league: 5 and 4.
2. Qualification: top four of each league plus the holders, Cork City (W0910, WM; same publisher, one source). Bohemians as LoI champions (RTE 2009 draw). Derry City were expelled in January 2010 after going into administration (BBC 8461699, W0910). Cork City were wound up in February 2010 and their remaining matches awarded 0-3 (BBC 8530623, R, W0910).
3. Groups: 3 groups of 3, home and away, 4 matches per club (W0910, R, Irish Times 2009 draw, RTE 2009 draw). Group 1: Sligo Rovers, Cork City (LoI), Cliftonville (IL). Group 2: Bohemians (LoI), Glentoran, Coleraine (IL). Group 3: Linfield (IL), St Patrick's Athletic, Derry City (LoI) (same sources). The mix was not fixed: 2+1, 1+2, 1+2 LoI plus IL.
4. Through: three group winners plus the best runner-up (W0910, R, RTE 2009 draw). Semi-finals two legs (Irish Times 2009 draw, Irish Examiner 2009 draw, W0910, R). Ties: Bohemians (Group 2 winner) v Linfield (Group 3 winner), St Patrick's Athletic (best runner-up, Group 3) v Sligo Rovers (Group 1 winner) (W0910, R). The rule for the semi-final pairing: not found. Final a single match. Extra time and penalties: not stated.
5. Dates: first match 29 August 2009 (R, BBC 8228667, W0910 match list). Group match days played: 29 Aug, 19 Sep, 22 Sep, 3 Oct, 6 Oct, 3 Nov 2009, then 26 Feb, 27 Feb, 13 Mar 2010. Last group match 13 March 2010 (R, W0910, BBC 8566405). Second phase relaunched 26 February 2010 (4ni https://www.4ni.co.uk/northern-ireland-news/107008/cross-border-soccer-kicks-off, BBC 8540080). Semi-finals: first legs 3 April and 13 April 2010, second legs 17 April and 19 April 2010 (W0910, R). Final 15 May 2010, Tallaght Stadium, Dublin (W0910, RTE https://www.rte.ie/sport/soccer/2010/0504/264681-setanta/, 4ni, BBC 8661137 for the date).
6. Final: Bohemians 1-0 St Patrick's Athletic. Winner Bohemians, runner-up St Patrick's Athletic (W0910, R).

### 2010 (calendar year)

No separate 2010 edition. The 2009-10 knockout rounds and final were played in 2010 (see above). The 2011 format was announced on 4 May 2010 (BBC https://news.bbc.co.uk/sport2/hi/football/irish/8661137.stm, RTE 0504, Irish Times https://www.irishtimes.com/news/new-format-for-2011-setanta-sports-cup-1.1246074).

### 2011 (Setanta Sports Cup)

1. Clubs: 12, 6 per league (W11, BBC 8661137, RTE 0504, Irish Times format, Irish Examiner draw https://www.irishexaminer.com/sport/soccer/arid-30488424.html). LoI: Shamrock Rovers, Sligo Rovers, Dundalk, Bohemians, St Patrick's Athletic, UCD (replaced Sporting Fingal). IL: Linfield, Glentoran, Crusaders, Cliftonville, Portadown, Lisburn Distillery (W11, R, Irish Examiner draw, Irish Examiner quarter-final draw https://www.irishexaminer.com/sport/soccer/arid-30495699.html).
2. Qualification: top four in each league plus the winners of "both major domestic cup competitions" (BBC 8661137, RTE 0504, Irish Times format). Byes to the quarter-finals for the two league champions, the FAI Cup winners and the Irish Cup (JJB Sports Cup) winners (BBC 8661137, RTE 0504, Irish Times format). Seeded clubs in practice: Shamrock Rovers, Sligo Rovers, Crusaders, Glentoran (W11). Shamrock Rovers had a bye as LoI champions (W11, Shamrock Rovers FC https://www.shamrockrovers.ie/news/on-this-day-14-may-2011/). Sporting Fingal withdrew on 9 February 2011 for financial reasons and UCD replaced them (W11 only for reason and date; the Irish Examiner draw lists Sporting Fingal, R lists UCD).
3. No group stage. Knockout: first round (8 clubs, each tie one LoI club v one IL club), quarter-finals, semi-finals, all two legs, then a single-match final (W11, R, BBC 8661137, Irish Examiner draw). No league split rule in later rounds: Cliftonville v Crusaders was an all-IL quarter-final and Sligo v Shamrock Rovers an all-LoI semi-final (W11, R).
4. Ties: R1 Linfield v Dundalk, UCD v Lisburn Distillery, Cliftonville v St Patrick's Athletic, Bohemians v Portadown. QF Glentoran v Dundalk, Cliftonville v Crusaders, Lisburn Distillery v Shamrock Rovers, Sligo Rovers v Portadown. SF Cliftonville v Dundalk, Sligo Rovers v Shamrock Rovers (W11, R, Irish Examiner QF draw, Irish Times SF draw https://www.irishtimes.com/sport/soccer/setanta-cup-semi-final-draw-made-1.1282081). Extra time and penalties: not stated.
5. Dates: draw 13 December 2010 at Belfast City Hall (W11). R1 first legs 14 Feb, 21 Feb, 1 Mar; second legs 28 Feb, 1 Mar, 7 Mar 2011. QF first legs 7, 14, 14, 28 Mar; second legs 21, 21, 22 Mar, 4 Apr. SF first legs 4 and 11 April, second legs 18 and 19 April 2011 (W11, R; SF dates also Irish Times SF draw and Irish Examiner https://www.irishexaminer.com/sport/soccer/arid-30498378.html). Final 14 May 2011, Tallaght Stadium (W11, R date, Shamrock Rovers FC).
6. Final: Dundalk 0-2 Shamrock Rovers. Winner Shamrock Rovers, runner-up Dundalk (W11, R, Shamrock Rovers FC).

### 2012, 2013 and 2014: the first-round draw (open item 148, read 5 November 2026)

- 2012: 12 clubs, draw 25 January 2012; seeds with byes Shamrock Rovers (holders), Crusaders, Linfield, Sligo Rovers. First round: Bray Wanderers v Glentoran, Bohemians v Portadown, Lisburn Distillery v Derry City, Cliftonville v St Patrick's Athletic (https://en.wikipedia.org/wiki/2012_Setanta_Sports_Cup, Irish Examiner http://web.archive.org/web/2012/http://www.irishexaminer.com/breakingnews/sport/draw-for-first-round-of-setanta-sports-cup-announced-537275.html). No source states the pairing rule.
- 2013: 12 clubs, draw 6 December 2012; byes Crusaders (holders), Derry City (FAI Cup), Linfield and Sligo Rovers (champions). First round, League of Ireland club at home first: Cork City v Cliftonville, Shamrock Rovers v Coleraine, St Patrick's Athletic v Glentoran, Drogheda United v Portadown. Quarter-final Derry City v Drogheda United (open draw). Sources: https://en.wikipedia.org/wiki/2013_Setanta_Sports_Cup, https://www.bbc.co.uk/sport/football/20634582, https://www.rte.ie/sport/soccer/2012/1206/357254-draw-made-for-setanta-sports-cup, https://www.rte.ie/sport/soccer/2012/1107/344612-setanta-cup-details-announced, https://www.the42.ie/setanta-sports-cup-draw-2013-705559-Dec2012/.
- 2014: 8 clubs, 4 of each league, no byes; draw 11 December 2013. Quarter-finals: Shamrock Rovers v Glentoran, Crusaders v Sligo Rovers, Dundalk v Coleraine, Ballinamallard United v St Patrick's Athletic (https://en.wikipedia.org/wiki/2014_Setanta_Sports_Cup, https://www.bbc.co.uk/sport/football/25307928, Setanta https://web.archive.org/web/20140121175350/http://www.setanta.com/ie/setanta-sports-cup-draw-3/). Only Wikipedia states that the leagues were kept apart.
- 2011 ties, for the same check: Cliftonville v St Patrick's Athletic, Linfield v Dundalk, Bohemians v Portadown, Sporting Fingal (replaced by UCD) v Distillery (W11, comeheretome.com 13 December 2010, extratime.ie archive).

### Gaps (one source or none)

- 2005: league routes of Cork City, Linfield, Glentoran, Portadown (W05 only; R says "European qualifiers"). Draw kept league winners apart (W05 only). Final venue rule (W05 only). No extra time or penalties rule found.
- 2006: routes of Linfield, Derry City, Shelbourne, Portadown, Dungannon Swifts. League Cup winners added (WM only). Extra time and penalties rule stated only in W06. No source says the draw required 2 LoI plus 2 IL per group.
- 2007: St Patrick's Athletic chosen as 2006 FAI Cup runners-up (W07 only). Routes of Linfield, Glentoran, Derry City, Cork City, Portadown, Dungannon Swifts.
- 2008: routes of Cork City, St Patrick's Athletic, Glentoran, Dungannon Swifts. Two-per-league-per-group rule (WM only). Extra time and penalties not stated.
- 2009-10: qualification rule (top four plus holders) is Wikipedia only. The cited eircomloi.ie page and the Wayback Machine copies of RTE pages could not be read (Wayback refused automated requests). Semi-final pairing rule not found. Nominal dates of the awarded matches (10 Nov 2009, 22 Feb 2010, 13 Mar 2010) are W0910 only. Derry City v Linfield (awarded) has no date in any source.
- 2011: which clubs were the four seeds and why Crusaders and Glentoran were seeded (W11 only; under the stated rule the IL seeds would be the IL champions and Irish Cup winners). Sporting Fingal withdrawal date and reason (W11 only). Draw date 13 December 2010 (W11 only; a blog post of that date also mentions the draw, https://comeheretome.com/2010/12/13/hit-the-north/). Extra time, away goals and penalties rules not found.
- Extratime.com and the Irish Football Club Project archive were not readable (Cloudflare challenge and Wayback block).

### Disagreements

- 2006, Drogheda United v Cork City, 27 March 2006: W06 gives 0-2, R gives 0-3. W06 table gives Cork 11 goals for, R gives 12. No third source found.
- 2006 and 2007 group names: W06 and W07 use Group A and B, the Irish Examiner (2006) also A and B. R and BBC 6218596 (2007) use Group 1 and 2.
- 2007, Dungannon Swifts v St Patrick's Athletic: W07 19 March 2007, R prints "19-3-06" (an obvious typo for 2007).
- 2007, Shelbourne's route: BBC 6315425 says Shelbourne were Premier Division champions. W07 implies they came in on league place.
- 2008, St Patrick's Athletic 0-1 Derry City: W08 15 September 2008. R, BBC https://news.bbc.co.uk/sport1/hi/football/irish/7632677.stm, RTE https://www.rte.ie/sport/soccer/2008/0923/238581-stpats_derry and Irish Times https://www.irishtimes.com/sport/soccer/derry-advance-to-setanta-semis-1.1223753 give 23 September 2008. Use 23 September.
- 2008 group stage end: W08 text says matches ran to 23 September 2008, but its own list and R show the last round on 6 October 2008. The Irish Times draw fixture list had planned the last round for 22 and 23 September.
- 2008 Group 2 table: R gives Derry City 10 points from 4 wins, 1 draw, 1 loss. W08 gives 13, which is the correct sum.
- 2008 final date: planned 18 October (BBC 7194404, Irish Times draw), played 1 November (W08, R, BBC 7704471).
- 2009-10 start: W0910 lead says 28 August 2009. Its own list, R and BBC 8228667 give the first match as 29 August 2009.
- 2009-10, Sligo Rovers 2-2 Cork City: W0910 13 September 2009. R and RTE https://www.rte.ie/sport/soccer/2009/0919/255020-sligo_cork give 19 September 2009.
- 2009-10, Derry City 1-1 St Patrick's Athletic: W0910 and RTE https://www.rte.ie/sport/soccer/2009/0922/255141-derrycity_stpats give 22 September 2009. R gives 23 September.
- 2009-10, Sligo Rovers 3-0 Cliftonville: W0910 26 February 2010. R gives 27 February. The BBC report (8541171) was published on Saturday 27 February 2010.
- 2009-10 final team order: W0910 lists St Patrick's Athletic first (nominal home side), R lists Bohemians first. Same result, neutral venue.
- 2011 byes: W11 says the four highest ranked clubs. BBC 8661137, RTE 0504 and Irish Times say the two league champions, the FAI Cup winners and the Irish Cup winners. WM adds the two League Cups to the qualification list. The 2010 FAI Cup and league route for Sligo Rovers and Shamrock Rovers fits the BBC rule. Crusaders and Glentoran I could not explain.
- 2011, Bohemians v Portadown first leg: the Irish Examiner fixture release planned it a week after 14 February. W11 and R give 1 March 2011.
