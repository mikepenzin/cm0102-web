# The Turkish Category B in three groups with Summer 2002 (research and design, 2026-10-24)

Spec: `docs/specs/2026-10-24-summer-2002-db-spec.md`, slice 5 (Q11, Q16, Q19, Q29, Q39). Code:
`src/recomp_rt/win/season_tur_groups.c`, `tools/recomp/tur_groups.py`, step 6 of
`tools/seasons/own_db/xbox0203/build_2002.py`. Test: `tests/seasons/turkish_groups_run.py`. Original:
`clean-3968/cm0102.exe` 3.9.68, virtual addresses.

## Relegation count (Q19)

The 2002/03 count is known, so the 2003/04 rule is not needed. The tr.wikipedia page "2002-03 2. Futbol Ligi B
Kategorisi" lists 18, 17 and 17 clubs, the three group winners promoted (Karşıyaka, Türk Telekom, Kayseri
Erciyesspor) and the last two of each group relegated to the 3. Lig (Gaziosmanpaşa and Kütahyaspor, Çubukspor and
Tokatspor, Hakkarispor and Silopi Cudispor): 6 in all. RSSSF `tablest/tur03.html` covers only the two top levels.
Row rus-tur-218 of `docs/research/rules-0.6.0/rus-tur.tsv` now holds the count with that source. The 2003/04 rule
(rus-tur-219, 2 a group) is the same, so the game uses one rule for every season of a Summer 2002 game.

## The original

- Keys (`src/cm3/key_ids_table.c`, slot = (address - 0x9cf178) / 4): Lower Division 0x9cf8cc, Premier Division
  0x9cf8d8, Category A 0x9cf8dc, Category B 0x9cf8e0, "Category B G1" to "G5" 0x9cf8e4 to 0x9cf8f4 (club_comp
  rows 289 to 293 of 3.9.68), "Turkish 2. Division Group 1" to "5" 0x9cf900 to 0x9cf910 (rows 168 to 172).
- 3.9.68 puts the 50 clubs of the tier in the divisions G1 to G5 (club +0x57), 10 each. TurFirst::create_table
  0x8fddc0, in the first season (+0x40 is the start year, the compare at 0x8fddce, `tools/recomp/seasons.py`),
  moves each into Category B and keeps the old division as its group (club +0x60).
- TurSecondB (vtable 0x970694, constructor 0x8ff4c0): 11 stage slots (+0x2c = 0xb at 0x8ff52a), set_up_rules
  0x900460 (0 up, 2 to the promotion group at +0xbf, 0 down), create_table 0x9002c0 (10 rows at 0x9002cd, count
  10 at 0x9002d2, the clubs whose group is G1), create_group_stage 0x900780 for slots 0 to 3 (G2 to G5 at
  0x9007df..0x90080e; 10 clubs at 0x900812, 0x90089d, 0x900932, 0x9009e4), then advance_stage 0x9006c0 builds the
  promotion group (slot 4), five classification groups (5 to 9) and a play-off (10).
- The functions that read five tables or the later stages: set_predicted_positions 0x901950, get_club_array
  0x901800 (50 clubs), record_final_positions 0x901ba0, record_season_honours 0x901c60 (a jump to slot 4),
  update_staff_records_for_status 0x9012d0, build_fate_news_text 0x9015f0, set_club_progress 0x901ac0.
- create_schedule 0x8ff7e0: stage -1 has 18 dates from 13 August to 10 December, stage 0 has 22, stage 4 (the
  promotion group) 18 dates in the spring (0x492 bytes at 0x8ffbc4).
- TurFirst::start_new_season 0x8fe040: the chain 0x8fe250 (Premier with Category A at 1, 1; Category A with
  Category B's own table and slots 0 to 3 at 1, 0, read with no test), then 0x8fe3a0 (10 clubs of the Lower
  Division up by 0x4afbd0 and 0x6830b0, then each relegated row of the five tables down by 0x6831a0 with +0x60
  cleared), then the deal 0x8fe580 (up to 50 clubs sorted by 0x53fe50, ten to each of G1 to G5).
- TurSecond (Category A) set_up_rules 0x8ff280 sets 3 up and 3 down, so three group winners keep it at its size.
- The destructor 0x8ff720 and start_new_season 0x900500 free the stages 0 to +0x30 only.

## The data side

Q39 binds the data: every division keeps its 3.9.68 club count, and the five rows 168 to 172 stay (the game
finds them by name at load and shows an error box for one that is missing, 0x607547). The original game must
still load Summer 2002 (the forum zip is for it until 0.6.1). So the clubs stay in G1 to G5, 10 each.

Club +0x60 is the regional group field of the data (Argentine zones, Croatian third division groups and others).
3.9.68 leaves it at -2 for every Turkish club, and the original game overwrites it for the clubs of G1 to G5 in
its first-season merge. So it can carry the real group without any effect on the original game.

The build puts the three Xbox rows "Turkish 2. Division Category B Group A", "B" and "C" into the free 3.9.68 rows
287 ("Turkish 2. Divison Category B Group 1"), 288 ("Turkish 2. Division") and 233 ("Korean All-Star Cup"), and
tags the 52 member clubs with their row (Q43). Each free row has no club that points at it (+0x57, +0x5b, +0x60),
no history row (club and nation comp history +0x04) and a name that is no key name (`2026-10-24-summer-2002-turkish-
group-rows.md`); the build checks the first two again. No row is added, because the game keeps one competition
array, club comps then nation comps, with ids in that order: in 3.9.68 the club comps are 0 to 432 and the nation
comps 433 to 451 (nation_comp.dat +0x00), and the nation comp history (+0x04) names the nation comps by those ids.
A first build added three rows at the end of club_comp.dat (ids 433 to 435). They took the ids of the first three
nation comps, so the nation comp history pointed three rows too early (found by slice 4's history_fill.py, 13 rows
lost), and a 2002/03 game stopped at fifa_rankings..cpp 222 in August 2002 (0x58d1ad), also with the groups off. The Xbox groups are the real 2002/03 groups club by club (checked by name against
`xbox-0203-work/league-check/B-turkey-result.tsv` and the 2002/03 rows of `rus-tur-clubs.tsv`: 18, 17, 17, no
difference). The colour, continent and nation ids of the Xbox rows name the same rows in 3.9.68, so the rows are
copied as they are but their ids. Only club.dat (bytes +0x60 to +0x63 of 52 clubs) and club_comp.dat (the three
rows) differ from the build without the groups; index.dat and staff.dat are the same.
`tools/seasons/own_db/test_summer_2002.py` fails on any clash of club and nation comp ids.

This is why the data side reuses rows and the code makes the groups: the key rows G1 to G5 cannot change their
counts (Q39 and the original game), the Xbox rows give the names Q16 asks for, and the game code needs only a
tag that it reads once.

## The game side

With the season code on and the three tag rows present, TurFirst::create_table (wrap of 0x8fddc0) puts each
tagged club of G1 to G5, the Lower Division, Category B or no division into G1, G2 or G3 before the original's
merge, and each untagged club of G1 to G5 into the Lower Division. A tagged club elsewhere stays (in a 2002/03
season game the club moves have already put the real Category A clubs in place, so the five tagged clubs that the
data holds in Category A for Q39 come to Category B). The tags are cleared.

TurSecondB then plays its three-group form, known from the clubs at construction (no club of Category B in G4 or
G5, each of G1 to G3 holding 2 to 18) and from +0x2c == 2 after it (saved with the game):

| Function | The form |
|---|---|
| create_table 0x9002c0 | the G1 clubs (Group A): `rt_tur_table_clubs` and `rt_tur_table_bytes` at 0x9002d2 and 0x9002cd |
| create_group_stage 0x900780 | slots 0 and 1 for G2 and G3 (Groups B and C), the size through `rt_tur_stage_*` at 0x900812, 0x90089d, 0x900932, 0x9009e4; slots 2 and 3 not built; +0x2c = 2 after slot 1, so League::update_for_date 0x6847c0 calls no advance_stage (0x6849d1..0x6849e3) |
| set_up_rules 0x900460 | each group 1 up, 2 down (the stages get the same after they are built) |
| create_schedule 0x8ff7e0 | 2 (n - 1) dates for n clubs (2 n for an odd n): the 18 of stage -1, then the spring dates of stage 4 (34 for 18 and 17) |
| 0x901950, 0x901ac0 | PorSecondB's 0x7d2cd0 and BelThird's 0x4241b0, the three-table code of those classes |
| 0x901c60, 0x9012d0, 0x9015f0 | League's 0x684640, 0x686940 and Comp's 0x48c6d0, as PorSecondB: the honours of the record go to Group A, the own table |
| 0x901800, 0x901ba0 | the clubs and final positions of the three tables |
| 0x9006c0 | builds nothing |
| chain 0x8fe250 | the original's calls less slots 2 and 3 |
| 0x8fe3a0 | the pool of the Lower Division first (every club, as 0x8fe3a6..0x8fe474, so no club that goes down comes back), then the relegated rows of the three tables down; at more clubs than the season's size, the lowest places too; then the pool fills the tier back to its size (0x4afbd0, 0x6830b0) |
| deal 0x8fe580 | the original's sort (0x53fe50), then three parts: 18, 17, 17 for 52 |

Any other database has no tag rows, so every wrap runs the original (the form needs the three rows by name). A
load keeps the places of each table: League::load reads +0xbe..+0xc1 at 0x6886d7 and the save writes them at
0x689589 (League_Stage 0x68c113 and 0x68cc27). `RECOMP_TURKISH_GROUPS=off` is the test
switch (the same-save run `turkish-groups-off`).

The lines `recomp: turkish groups: ...` on stderr give the groups of each season and each move at a season
change; `tests/seasons/turkish_groups_run.py` reads them.

## Differences from the real league

- The honours of the division (its champion in the history) go to the Group A winner, as PorSecondB records its
  North zone. The real tier had three winners and no champion.
- The groups after 2002/03 come from the original's sort by longitude, not the real regions, and the form stays
  three groups in every later season (the real tier had five groups again from 2005/06 and two from 2010/11; the
  spec leaves a Turkish structure change out of scope).
