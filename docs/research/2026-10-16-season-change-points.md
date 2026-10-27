# Season rules: change points 2001 to 2026

Date 2026-10-16. Research for the season screen (0.5.0, Q165 in
the season screen decision notes (kept outside the repo)). No code changes.

Goal: each season that the player chooses (2001/02 to 2025/26) gets the rules and league structures of that
year. The rules change at known change points, not every year. The official 2001/02 and 2002/03 games stay as
they are.

Sources of evidence:

- [code]: the original 3.9.68 cm0102.exe in `clean-3968`, disassembled with objdump. A virtual address (VA)
  equals the file offset + 0x400000 for .text (section raw offset 0x1000 = VA 0x401000). This file gives
  VAs.
- [data]: the clean 3.9.68 data and two Update Team databases in `../test-fixtures/databases`.
- [real]: the real-world fact. Wikipedia is the reference for most rows. Rows marked "check" come from my
  knowledge and need a check before the spec. Rows with a URL were checked on 2026-10-16.

## 2. What the original ties to the 2001 start

### First-season branches [code]

- Many 2001 constants (0x7d1) in the exe are **first-season branches**. A year change that moves every 2001
  constant moves them too, so the first chosen season always gets the 2001/02 layout. The table below maps
  each one to its source file from the original's assert strings [code]:

| VA | Source file | Code | Meaning (inference) |
|---|---|---|---|
| 0x4224ec | bel_third.cpp | `cmp word [edi+0x40], 2001` | Belgian 3rd tier first season |
| 0x56fc5f | eng_fa_cup.cpp | `cmp word [esi+0x40], 2001` | FA Cup 2001 schedule |
| 0x5bab83, 0x5bac2f, 0x5bace5 | fra_cup.cpp | `sub bx, 2001` then 0xb8 or 0xb6 | French cup entrants: 184 in 2001, 182 later |
| 0x5bb6a7, 0x5bc2bd, 0x5bc41c, 0x5bc8ae | fra_first.cpp | `cmp [reg+0x40], 2001` | Ligue 1: 18 clubs in 2001/02, 20 from 2002/03 |
| 0x5bf0aa | fra_second.cpp | same | Ligue 2 first season |
| 0x5c070a | fra_third.cpp | same | National first season |
| 0x5db23e | ger_first.cpp | same | Bundesliga first season |
| 0x7d240c | por_second_b.cpp | same | Portuguese 3rd tier |
| 0x856cdc | spa_second_b.cpp | same | Segunda B |
| 0x8fddce | tur_first.cpp | same | Turkish top division |
| 0x9041ec | uefa_seeding.cpp | `cmp [ebx+0x439], 2001` | Read euro.cfg only in the first season |
| 0x903b6f | (UEFA, no assert) | `cmp [0xae2c92], 2001` | UEFA coefficients |
| 0x9291b2 | world_club_champ.cpp | `cmp ax, 2003`, odd years | Club World Championship from 2003, every two years |
| 0x5836c4, 0x632e30 | european_awards.cpp, international_awards.cpp | `cmp ax, 2001` | Awards first season |
| 0x667150 (33 sites) | key_nation.cpp | `push 2001` | Season update day of each league table entry (+0x15). Corrected by `2026-10-16-sweep-season-end.md` |
| 0x830520 (9 sites) | Setup.cpp | `push 2001` | Update days (+0x15) and a year word (+0x1b) per league table entry. 0x830520 is the start date picker. Corrected by `2026-10-16-sweep-season-end.md` |

- Other year tests in the original [code]: Spain rules `0x8595AD`: non-EU limit 4 in 2000, 3 in 2001 and
  2002, 2 after. World Cup `0x92dfc4` and two more: special case for 2002. Korea `0x66b1f1`: 2002.
  CONMEBOL seeding `0x4c64fe`: after 2002. News `0x781671`: 2002 and 2003 (inference from the sweep: news type ids, not years).
- Finding: the original already knows a few transitions (France 18 to 20, Spain non-EU 3 to 2, Club World
  Championship 2003), always relative to its own 2001 start.

### Sizes, substitutes and foreign rules [code]

- **Sweden.** Allsvenskan size: `mov word [esi+0x3e], 14` at 0x890442, with the allocation `push 0x33a` at
  0x89043b. A size change is two bytes.
- **Serbia.** The exe holds the Yugoslavia name bytes at 0x9b12a4. The data holds the rest.
- **Substitutes.** +0x49 is max_bench in the competition object (0x56f224 is the end of a function that sets
  it). The match data has a subs word `[edx+0x4d]` (0x48f32d): low nibble named, high nibble used.
  Per-competition values are bytes in each competition's code, for example Italy 0x63c64c (3 used) and
  0x63c650 (7 named), and the Champions League at 0x57a16e.
- **Foreign rules.** The rules object byte +5 holds the non-EU or foreign limit. England writes it at
  `0x579c63 mov byte [edx+5], 3` ("3 non-EU"). Original values: Argentina 4 foreign, Australia 6, Brazil 2,
  Croatia 4, England 3 non-EU (shared with Denmark and Sweden), France 5/3/3 non-EU, Germany 3 non-EU,
  Greece 4 non-EU, Japan 3, Norway 2 non-EU, Poland 5, Portugal 4/4/3 non-EU, Russia 3, South Korea 3,
  Turkey 6, USA 4. Belgium, Holland, Ireland, Northern Ireland, Scotland and Wales have none. See
  `2026-10-16-foreign-and-tactics-positions.md` for the full list of fillers.
- **European registration.** 25 players, at 0x85b05a and 0x85b0be.

### Names [data]

Competition names by year (for example English First Division to Championship, French First Division to
Ligue 1, UEFA Cup to Europa League, Inter-Toto Cup to Europa League Qualifying) are a data change in
club_comp.dat, nation_comp.dat and the .lng files.

## 3. What the Update Team databases hold

Compared: clean 3.9.68 `Data`, `October 2014`, `May 2026`. Record sizes from the database inventory.

- **Competition names are not updated.** club_comp.dat in October 2014 has the same 433 names as 3.9.68.
  May 2026 still has "English First Division", "UEFA Cup", "Inter-Toto Cup", "European Cup Winners Cup",
  "FIFA Club World Championship". The names need a separate change.
- **Club counts stay at the 2001/02 layout.** Clubs per division (club.dat, division id at +0x57):

| Division | 3.9.68 | Oct 2014 | May 2026 | Real 2025/26 |
|---|---|---|---|---|
| English Premier | 20 | 20 | 20 | 20 |
| English First, Second, Third | 24 each | 24 | 24 | 24 |
| English Conference | 22 | 22 | 22 | 24 |
| Italian Serie A | 18 | 18 | 18 | 20 |
| Italian Serie B | 20 | 20 | 20 | 20 |
| Italian C1/A, C1/B, C2/A, C2/B, C2/C | 18 each | 18 | 18 | Serie C 3 x 20 |
| French First | 18 | 18 | 18 | 18 |
| German Regional North, South | 18, 18 | 18, 18 | 18, 18 | 3. Liga 20 + 5 Regionalligas |
| Swedish Premier | 14 | 14 | 14 | 16 |
| Norwegian Premier | 14 | 14 | 14 | 16 |
| Dutch First | 18 | 18 | 18 | 20 |
| Belgian First | 18 | 18 | 18 | 16 |
| Russian Premier | 16 | 16 | 16 | 16 |

  The exe fixes the sizes, so the databases keep them. A restructure must then move clubs
  between divisions.
- **Spare records for restructures.** May 2026 has new club_comp records in reused ids: "Italian Serie C"
  (410) and "Italian Serie C/C" (411), "German Regional Division Bayern" (406), "German Lower Division"
  (287), Swiss, Serbian, Austrian Second, Romanian, Ukrainian and others. All have 0 clubs in the data. They
  are spare. 104 club comps of 3.9.68 (Hong Kong, India, Singapore, Swedish Third Divisions
  and so on) are gone.
- **nation_comp.dat.** 3.9.68 and October 2014: the same 19 records. May 2026: 23. New: "UEFA Nations
  League" (455), "CONCACAF Nations League" (456), "Gold Cup" (453), "African Cup of Nations Qualifying"
  (454). Confederations Cup (437) is still there. A record does not make the competition run: the original
  has no Nations League code. [code: no string match]
- **euro.cfg.** The first-season European seeding (read only when the start year is 2001, `0x9041ec`).
  `*Nation` headers in coefficient order, then clubs. 3.9.68: 40 nations (England 8, Italy 8, Germany 10,
  France 10, Yugoslavia 4). October 2014: 53 nations (Montenegro, San Marino, Andorra added). May 2026:
  55 nations (Serbia, Kosovo, Gibraltar, North Macedonia), and some nations list all their clubs (France 151,
  Norway 107). Slice 12 fixed the hang on an unknown nation header.

So: names, clubs, seeding and history are data. Sizes, formats, rules and new competitions are exe work.

## 4. Change points

Columns: **Season** is the first season with the change. **Game today** is what 3.9.68 does with the year
moved. **Need**: D = data change only (database, names, euro.cfg), E = exe rule change,
E+ = new competition code (large), N = not modelled by the game at all. **Where** points to the code area.

### 4a. UEFA

| # | Change | Season | Real source | Game today | Need | Where |
|---|---|---|---|---|---|---|
| U1 | Champions League second group stage dropped, round of 16 added | 2003/04 | Wikipedia "2003-04 UEFA Champions League" | Two group stages | E+ | CL code behind 0x904210 |
| U2 | UEFA Cup group stage (8 groups of 5) | 2004/05 | Wikipedia "2004-05 UEFA Cup" | Knockout only | E+ | 0x904580 slot |
| U3 | Intertoto Cup last held (11 winners to UEFA Cup) | 2008 last, gone from 2009/10 | Wikipedia "UEFA Intertoto Cup" | Intertoto runs (club_comp 330) | E | Europe nation setup |
| U4 | Europa League replaces UEFA Cup (48-team groups) | 2009/10 | Wikipedia "UEFA Europa League" | UEFA Cup | E+ (name D) | 0x904580 |
| U5 | CL Champions Path and League Path in qualifying | 2009/10 | Wikipedia "2009-10 UEFA Champions League" | One qualifying path | E | 0x904210, 0x903CC0 |
| U6 | Super Cup leaves Monaco, single match at changing venues | 2013 | Wikipedia "UEFA Super Cup" | Monaco or fixed | E (small) | - |
| U7 | Top four associations get 4 direct CL places | 2018/19 | Wikipedia "UEFA coefficient" | Places by coefficient | E | 0x903CC0, 0x903b50 |
| U8 | Conference League (third competition), EL cut to 32 | 2021/22 | Wikipedia "UEFA Europa Conference League" | None | E+ | 0x904E30 (Intertoto slot) |
| U9 | Away goals rule abolished in UEFA competitions | 2021/22 | Wikipedia "Away goals rule" | Away goals used | E (small) | - |
| U10 | League phase, 36 clubs, in CL, EL and UECL; European Performance Spots | 2024/25 | Wikipedia "2024-25 UEFA Champions League" | Groups of 4 | E+ | - |
| U11 | UEFA squad list 25 with home-grown players (4+4 by 2008/09) | 2006/07 | Wikipedia "Homegrown Player Rule (UEFA)" check | 25 registered (0x85B056), no home-grown | E | 0x85B050 |
| U12 | Places per nation follow coefficients each year | every year | UEFA coefficient tables | Computed by exe from history | D (history) + E | 0x903CC0; euro.cfg only for the first season |

### 4b. FIFA and national teams

| # | Change | Season | Real source | Game today | Need | Where |
|---|---|---|---|---|---|---|
| F1 | Club World Championship returns, annual from 2005; "Club World Cup" from 2006 | 2005/06 | Wikipedia "FIFA Club World Cup" | Every two years from 2003 | E | 0x9291b2 world_club_champ.cpp |
| F2 | Intercontinental Cup ends | 2004 last | Wikipedia "Intercontinental Cup (football)" | Check: not traced | E | - |
| F3 | Club World Cup 32 clubs, every four years; FIFA Intercontinental Cup yearly | 2024/25 (CWC June 2025) | Wikipedia "2025 FIFA Club World Cup" | Annual small CWC | E+ | - |
| F4 | World Cup holders lose automatic place | 2006 | Wikipedia "2006 FIFA World Cup qualification" | Check | E (small) | world_cup.cpp |
| F5 | World Cup hosts 2006 Germany, 2010 South Africa, 2014 Brazil, 2018 Russia, 2022 Qatar, 2026 USA/Canada/Mexico | per edition | Wikipedia "FIFA World Cup hosts" | Host chosen by exe | E (small) | Host code near 0x5f99a1 to 0x5f9d63 (inference) |
| F6 | World Cup in November and December | 2022 | Wikipedia "2022 FIFA World Cup" | Summer | E | - |
| F7 | World Cup 48 teams, new qualifiers and intercontinental play-off | 2026 (season 2025/26) | Wikipedia "2026 FIFA World Cup" | 32 teams | E+ | - |
| F8 | Euro hosts 2004 Portugal, 2008 Austria/Switzerland, 2012 Poland/Ukraine | per edition | Wikipedia "UEFA European Championship" | Host chosen by exe | E (small) | - |
| F9 | Euro 24 teams | 2016 | Wikipedia "UEFA Euro 2016" | 16 teams | E | - |
| F10 | Euro 2020 played in 2021, pan-European | 2020/21 | Wikipedia "UEFA Euro 2020" | n/a | E (dates) | - |
| F11 | Confederations Cup last held | 2017 | Wikipedia "FIFA Confederations Cup" | Runs (nation_comp 437) | E (switch off) | not traced |
| F12 | UEFA Nations League | 2018/19 | Wikipedia "UEFA Nations League" | None (record 455 in May 2026 only) | E+ | none in original |
| F13 | Euro qualifying with Nations League play-off paths | 2018/19 | Wikipedia "UEFA Euro 2020 qualifying" | Groups + play-offs | E | world/euro quals code |
| F14 | Copa America years 2004, 2007, 2011, 2015, 2016, 2019, 2021, 2024 | per edition | Wikipedia "Copa América" | Fixed cycle | E (small) | - |
| F15 | Africa Cup of Nations 24 teams, June and July | 2019 | Wikipedia "2019 Africa Cup of Nations" | 16 teams, January | E | - |
| F16 | Africa Cup of Nations every four years from 2028; African Nations League from 2029 | 2027/28 | [ESPN, Dec 2025](https://africa.espn.com/espn/story/_/id/47367241/caf-move-afcon-every-four-years-launch-new-african-nations-league-patrice-motsepe) | Every two years | E (small) | - |
| F17 | Asian Cup 24 teams | 2019 | Wikipedia "2019 AFC Asian Cup" | 16 | E | not traced |
| F18 | Australia moves from OFC to AFC | 2006 | Wikipedia "Football Australia" | OFC | D + E | - |
| F19 | Serbia and Montenegro (2003), Serbia (2006), Montenegro (2007 UEFA), Kosovo and Gibraltar join UEFA | 2003 to 2016 | Wikipedia articles of each FA | Yugoslavia | D + E (names in exe) | Name bytes at 0x9b12a4 |
| F20 | Golden goal abolished (silver goal 2003/04 only) | 2004/05 | Wikipedia "Golden goal" | Golden goal in some cups | E (small) | Code near 0x40afe1 (inference) |

### 4c. Rules

| # | Change | Season | Real source | Game today | Need | Where |
|---|---|---|---|---|---|---|
| R1 | FIFA transfer windows, summer and January, in all of Europe | 2002/03 | Wikipedia "Transfer window" | Per-nation rules (date lists in the rules objects, inference) | E | rules constructors via 0x8D26B0; code near 0x40a6aa (inference) |
| R2 | Kolpak ruling: Cotonou nations count as EU; Malaja (France, 2002) | 2003/04 | Wikipedia "Kolpak ruling" check | Not modelled | E | rules byte +5 and nation EU flag |
| R3 | Premier League bench 5 to 7 named | 2008/09 | Wikipedia "Substitute (football)" check | 5 named | E (small) | +0x49 max_bench, 0x56f224 |
| R4 | Premier League 25-man squad, 8 home-grown | 2010/11 | Wikipedia "Premier League" | None | E | not traced; new rule |
| R5 | Bundesliga ends non-EU limit, local-player rule | 2006/07 | Wikipedia "Bundesliga" check | 3 non-EU | E (small) | Germany rules |
| R6 | Five substitutes (IFAB temporary), UEFA 12 on the bench | 2020/21 | [Tottenham, Oct 2020](https://www.tottenhamhotspur.com/news/2020/october/jose-on-new-substitutions-rule-in-uefa-europa-league/) | 3 used | E (small) | +0x4a max_subs; match subs word (0x48f32d) |
| R7 | Premier League: 5 subs, 9 named | 2022/23 | [manutd.com, 2022](https://www.manutd.com/en/News/Detail/Premier%20League%20to%20reintroduce%20five%20substitutes%20for%202022%2023%20season) | 3 used | E (small) | as R6, per competition |
| R8 | Championship: 5 subs, 9 named; League One and Two 5 from 7 | 2023/24 | Yorkshire Evening Post, search 2026-10-16 | 3 used | E (small) | - |
| R9 | Serie A, La Liga, Bundesliga, Ligue 1: 5 subs | 2020/21 | Wikipedia "Substitute (football)" check | 3 used | E (small) | Per-competition subs bytes |
| R10 | Brexit: Governing Body Endorsement points for all foreign players in the UK, no under-18 transfers from the EU | 2020/21 (1 Jan 2021) | Wikipedia "Brexit and association football" check | Work permit for non-EU only (England 3 non-EU) | E | England rules 0x579B30, byte +5 |
| R11 | Spain: non-EU 3 per squad | 2003/04 on | Wikipedia "La Liga" check | 3 in 2001 and 2002, then 2 (0x8595AD) | E (small) | spain_rules.cpp 0x859400 |
| R12 | Italy: non-EU signings from abroad per season | changes yearly | FIGC rules check | Not enforceable in game | N or E | Italy rules 0x661380 |
| R13 | Points deductions for administration (Football League 10 points) | 2004/05 | Wikipedia "English Football League" check | None | E | - |
| R14 | Away goals abolished in domestic cups (League Cup) | 2018/19 to 2021/22 | Wikipedia "EFL Cup" check | Used | E (small) | cup code |

### 4d. Leagues and cups of the 35-entry league table

The 27 nations of the league table: Argentina, Australia, Belgium, Brazil, Croatia, Denmark, England,
Finland, France, Germany, Greece, Holland, Republic of Ireland, Italy, Japan, Northern Ireland, Norway,
Poland, Portugal, Russia, Scotland, South Korea, Spain, Sweden, Turkey, United States, Wales. Entries 0 to 7
are continents.

| # | Nation | Change | Season | Real source | Game today | Need | Where |
|---|---|---|---|---|---|---|---|
| L1 | England | Two up from the Conference (play-off) | 2002/03 | Wikipedia "National League (English football)" | 1 up? check | E | eng_conf |
| L2 | England | Charity Shield renamed Community Shield | 2002 | Wikipedia "FA Community Shield" | Charity Shield | D | Data names |
| L3 | England | Football League rename: Championship, League One, League Two | 2004/05 | Wikipedia "EFL Championship" | First, Second, Third | D | Data names |
| L4 | England | Conference North and South created | 2004/05 | Wikipedia "National League North" | None (Isthmian/Southern/Northern Premier exist) | E+ | - |
| L5 | England | Conference 22 to 24 clubs | 2006/07 | Wikipedia "National League" | 22 | E (count) | Code near 0x56ecbc (inference) |
| L6 | England | Football League becomes EFL; EFL Trophy with academy sides and groups | 2016/17 | Wikipedia "EFL Trophy" | Vans Trophy knockout | E (D for name) | - |
| L7 | England | FA Cup replays: quarter-finals dropped 2016/17, fifth round 2019/20, all from first round proper 2024/25 | 2016/17, 2019/20, 2024/25 | Wikipedia "FA Cup" | Replays | E (small) | eng_fa_cup.cpp 0x56fab0 |
| L8 | England | League Cup: no extra time before the final | 2018/19 | Wikipedia "EFL Cup" | Extra time | E (small) | not traced |
| L9 | England | National League play-offs 6 clubs | 2017/18 | Wikipedia "National League" | 4 | E | - |
| L10 | England | Championship play-offs 6 clubs | 2026/27 | [Flashscore, 2026](https://www.flashscore.com/news/soccer-championship-championship-play-offs-expanded-to-six-teams-from-2026-27-season/UZl6bZFt) | 4 | E | - |
| L11 | Scotland | SFL promotion and relegation play-offs | 2005/06 | Wikipedia "Scottish Football League" | Direct | E | - |
| L12 | Scotland | SPFL: Premiership, Championship, League One, League Two; Premiership play-off | 2013/14 | Wikipedia "Scottish Professional Football League" | SPL + SFL | D (names) + E (play-off) | Data names; sco_* |
| L13 | Scotland | Pyramid play-off with Highland and Lowland Leagues | 2014/15 | Wikipedia "Scottish League Two" | No relegation from bottom | E | - |
| L14 | Scotland | League Cup group stage; Challenge Cup with Welsh, NI and B teams | 2016/17 | Wikipedia "Scottish League Cup" | Knockout | E | - |
| L15 | Italy | Serie A 18 to 20 clubs | 2004/05 | Wikipedia "Serie A" | 18 | E | ita_ser_a.cpp |
| L16 | Italy | Serie B 22 clubs (24 in 2003/04), back to 20 | 2003/04, 2019/20 | Wikipedia "Serie B" | 20 | E | - |
| L17 | Italy | Serie B promotion play-offs | 2004/05 check | Wikipedia "Serie B" | None | E | - |
| L18 | Italy | Lega Pro Prima and Seconda Divisione (names) | 2008/09 | Wikipedia "Lega Pro Prima Divisione" | C1, C2 | D | Data names |
| L19 | Italy | Lega Pro single division 3 x 20, named Serie C from 2017/18 | 2014/15 | Wikipedia "Serie C" | C1/A, C1/B, C2/A-C | E+ | - |
| L20 | Italy | Coppa Italia single-leg final; Supercoppa 4 clubs in Saudi Arabia | 2007/08, 2023/24 | Wikipedia "Coppa Italia", "Supercoppa Italiana" | Two-leg final, 2 clubs | E | - |
| L21 | Spain | Segunda División promotion play-offs | 2010/11 | Wikipedia "Segunda División" | None | E | - |
| L22 | Spain | Supercopa 4 clubs in Saudi Arabia; Copa del Rey single-leg ties | 2019/20 | Wikipedia "Supercopa de España" | 2 clubs, two legs | E | - |
| L23 | Spain | Segunda B replaced by Primera RFEF (2 x 20) and Segunda RFEF (5 x 18) | 2021/22 | Wikipedia "Primera Federación" | Segunda B 4 x 20 | E+ | - |
| L24 | France | Ligue 1 18 to 20 clubs; Division 1 and 2 renamed Ligue 1 and 2 | 2002/03 | Wikipedia "Ligue 1" | 18 in the first season, then 20 (fra_first.cpp 2001 branch) | D (names) + E (first-season branch) | 0x5bb6a7 etc. |
| L25 | France | Relegation play-off 18th v Ligue 2 | 2016/17 check | Wikipedia "Ligue 1" | 3 down | E | fra_first.cpp |
| L26 | France | Coupe de la Ligue abolished | after 2019/20 | Wikipedia "Coupe de la Ligue" | Runs | E (switch off) | Jump at 0x834302 (inference) |
| L27 | France | 4 relegated, then Ligue 1 18 clubs | 2022/23, 2023/24 | Wikipedia "2022-23 Ligue 1" | 20 | E | Code near 0x5bc47a and 0x5bab90 (inference) |
| L28 | France | Ligue 2 18 clubs with play-offs; CFA renamed National 2 (2017) | 2024/25 | Wikipedia "Ligue 2" | 20 | E (+D names) | - |
| L29 | Germany | Ligapokal ends; DFL-Supercup official | 2007 last, 2010 | Wikipedia "DFL-Supercup" | Ligapokal runs | E | - |
| L30 | Germany | 3. Liga (20), Regionalliga 3 groups, relegation play-offs | 2008/09 | Wikipedia "3. Liga" | Regionalliga North, South | E+ | - |
| L31 | Germany | Regionalliga 5 groups (Nord, Nordost, West, Südwest, Bayern) | 2012/13 | Wikipedia "Regionalliga" | 2 groups | E+ | May 2026 record 406 Bayern |
| L32 | Netherlands | Jong teams in Eerste Divisie; Eerste Divisie 20; Tweede Divisie created (2016/17) | 2013/14 check | Wikipedia "Eerste Divisie" | 18 | E | - |
| L33 | Portugal | Primeira Liga 16 clubs, then 18 again | 2006/07, 2014/15 | Wikipedia "Primeira Liga" | 18 | E (count) | por_first |
| L34 | Portugal | Taça da Liga starts | 2007/08 | Wikipedia "Taça da Liga" | None | E+ | "Portuguese League Cup" record in May 2026 |
| L35 | Portugal | Liga 3 created, Campeonato de Portugal 4th tier | 2021/22 | Wikipedia "Liga 3 (Portugal)" | Second Division B | E+ | - |
| L36 | Belgium | Pro League 16 with play-offs; 18 (2020/21); 16 again (2023/24) | 2009/10 | Wikipedia "Belgian Pro League" | 18, no play-offs | E | - |
| L37 | Sweden | Allsvenskan 14 to 16 clubs; Division 1 becomes third tier (2006) | 2008 | Wikipedia "Allsvenskan" | 14 | E (count) | 0x89043b, 0x890442 |
| L38 | Norway | Tippeligaen 14 to 16 clubs; Eliteserien name 2017 | 2009 | Wikipedia "Eliteserien" | 14 | E (count) + D | nor_premier |
| L39 | Denmark | Superliga 14 with split, back to 12 | 2016/17, 2020/21 | Wikipedia "Danish Superliga" | 12 | E | - |
| L40 | Russia | Calendar moves to autumn to spring | 2012/13 | Wikipedia "Russian Premier League" | Calendar year | E | - |
| L41 | Turkey | Süper Lig 21, 20, 19, 18 clubs | 2021/22 to 2025/26 | Wikipedia "Süper Lig" check | 18 | E (count) | tur_first.cpp 0x8fddc0 |
| L42 | Greece | Super League replaces Alpha Ethniki; 14 clubs with split | 2006/07, 2019/20 | Wikipedia "Super League Greece" | 16 | E | - |
| L43 | Poland | Ekstraklasa split (2013/14 to 2019/20), 18 clubs | 2021/22 | Wikipedia "Ekstraklasa" | 16 | E | - |
| L44 | Croatia | HNL 12 with split, later 10 clubs, 4 rounds | 2002/03, 2013/14 check | Wikipedia "Croatian Football League" | 16? check | E | - |
| L45 | Wales | Welsh Premier 12 clubs with split | 2010/11 | Wikipedia "Cymru Premier" | 18? check | E | - |
| L46 | Northern Ireland | IFA Premiership 12 clubs with split | 2008/09 | Wikipedia "NIFL Premiership" | 16? check | E | - |
| L47 | Ireland | Summer season (February to November) | 2003 | Wikipedia "League of Ireland" | Winter season | E | - |
| L48 | Brazil | Série A double round robin (24, 22, then 20 clubs); Série D (2009) | 2003, 2006 | Wikipedia "Campeonato Brasileiro Série A" | Groups and play-off | E+ | - |
| L49 | Argentina | Apertura and Clausura end; 30-club league | 2012/13, 2015 | Wikipedia "Argentine Primera División" | Apertura/Clausura | E+ | - |
| L50 | Australia | NSL replaced by A-League | 2005/06 | Wikipedia "A-League Men" | NSL | E + D | - |
| L51 | Japan | J1 18 (2005), J1 and J2 20 (2024), J3 (2014) | 2005, 2014, 2024 | Wikipedia "J1 League" | J1 16 | E | - |
| L52 | South Korea | Promotion and relegation, K League 1 and 2 | 2013 | Wikipedia "K League 1" | One league | E+ | - |
| L53 | USA | MLS growth (10 to 30 clubs), conferences | many | Wikipedia "Major League Soccer" | 12 clubs | E | - |

Not modelled at all by the game (N): VAR (2017/18 on), goal-line technology, concussion substitutes
(2021), the Premier League winter break (2019/20), financial fair play and the Premier League profit rules
(2011 on, except the points deductions in R13), loan limits of FIFA (2022), the five-minute cooling break.

## 5. Eras

The start of each era is a season where several E or E+ change points meet. Inside an era, counts and names
that differ by year (clubs per division, hosts, competition names) come from a small per-season table and
the database, not from a new era.

| Era | Seasons | Defining rules | Changes that fall inside and need the per-season table |
|---|---|---|---|
| 0 Original | 2001/02, 2002/03 | The original, no change (Q165) | none |
| 1 2000s | 2003/04 to 2008/09 | CL one group stage + round of 16 (U1); UEFA Cup groups (U2, from 2004/05); Intertoto; transfer windows (R1); no golden goal (F20); Serie A 20, Serie B 22 (L15, L16); Ligue 1 20 (L24); English names (L3); 3 subs, 5 or 7 named | 2003/04 has no UEFA Cup groups yet and Serie A 18; Conference North/South from 2004/05; Allsvenskan 16 from 2008; 3. Liga from 2008/09 |
| 2 2010s | 2009/10 to 2019/20 | Europa League, no Intertoto (U3, U4); CL two paths (U5); 3. Liga and Regionalliga (L30); Allsvenskan and Eliteserien 16 (L37, L38); PL 25-man squad (R4); 7 on the bench | SPFL from 2013/14; Serie C 3 x 20 from 2014/15; Regionalliga 5 groups from 2012/13; 4 CL places for the top four from 2018/19; Nations League from 2018/19 |
| 3 Early 2020s | 2020/21 to 2023/24 | 5 subs (R6 to R9); Conference League (U8, from 2021/22); no away goals (U9); Brexit rules (R10); Primera and Segunda RFEF (L23); Liga 3 (L35) | 2020/21 has no Conference League; Ligue 1 18 from 2023/24; PL 9 named from 2022/23; Championship 9 named from 2023/24 |
| 4 League phase | 2024/25, 2025/26 | League phase in CL, EL and UECL (U10); Club World Cup 32 (F3); no FA Cup replays (L7); 48-team World Cup (F7); Ligue 2 18 (L28) | - |

**Recommendation: five eras (0 to 4), with four boundaries: 2003/04, 2009/10, 2020/21, 2024/25.**

- Fewer eras lose a whole UEFA format. The four boundaries are the four UEFA format changes (2003, 2009,
  2021 with 5 subs a year before, 2024). The UEFA code is the largest single cost and runs in every game.
- The off-by-one seasons (2003/04 without UEFA Cup groups, 2020/21 without Conference League) can use the
  era rules and a per-season switch. That keeps each chosen season within one change of real life for the
  top competitions.
- The season-screen colours (2000s amber, 2010s green, 2020s blue) can stay by decade. Era 2 starts in
  2009/10, one season before the green tiles. Max decides whether the tiles follow the decade or the era.
- Counts by year (L5, L15, L16, L27, L37, L38, L41 and the like) go in a per-season table that the
  recompiled game reads. Most counts in the original are one or two bytes (0x890442 for Sweden), so a
  table lookup at those sites is cheaper than one era per count.

## 6. Unknowns

- What the original does with transfer windows per nation in 3.9.68. No accessor was traced here.
- How the original picks the European nation competitions (by id or by name), and so whether a data record
  such as "UEFA Nations League" (455) can ever run without new code. No code found.
- Whether the Confederations Cup, Intercontinental Cup and Cup Winners' Cup records run in a moved year. Not
  traced.
- Rows marked "check" in section 4 come from my knowledge, not from a page read today.
- A year change that moves every 2001 constant also moves the first-season branches (section 2). Whether the season
  screen should keep or skip each branch is a decision for each one (France: skip from 2002/03 on).
