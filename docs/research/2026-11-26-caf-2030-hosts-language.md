# A Spanish game stops in March 2028: Database.cpp 13471, then fix_man.cpp 3272

26 November 2026. A player's error report (browser, JSPI engine, a 2025/26 game with changing rules on, the database
and language in Spanish), with the save it loaded. Reproduced with the native engine and the save
(`RECOMP_SEASON_INTL_LOG=1`, the Seasons tables of the build); the call chain from gdb at the error box.

## What the game did

On 27 March 2028 (day 87) the CAF qualifying of the 2030 World Cup made its group stage:

`Cup::update_for_date` 0x51f890 → `WcAfricanCup::advance_stage` 0x910920 → `WcAfricanCup::create_group_stages`
0x9109e0 → `League_Stage::League_Stage` 0x68b100 → `League::create_fixtures` 0x6835c0 →
`FIXTURE_MANAGER::add_fixture` 0x5aa680 (through `fixture_season_end_add_fixture`) →
`FIXTURE_MANAGER::find_fixture_clashes` 0x5ad780 → `comp_active_key_nation_of_club` 0x48dee0 →
`database_is_national_team` 0x53b900 with no club: the box Database.cpp 13471, and after it fix_man.cpp 3272
(`collect_day_clashes` 0x5adba0).

The competition at the builder's entry (`this` +0xb6 pool, +0x3e first round, +0xba exempt, +0xb2 slots): pool 50,
first round 20, exempt 0, 30 slots filled. The first round's 10 winners make 40 teams: the 8 groups of 5 of the 2030
shape (`season_wcq26.c`, O265: 9 places less the African host Morocco). The builder read the group count again
(`rt_wcq26_africa_groups` at 0x910a4a) and got 9: "CAF reads 0 hosts on day 87 of 2028: 9 direct places". It dealt
45 places to 40 teams; an empty slot sorts last (0x4b07d0), so five places of the last groups held no club.

## Why the hosts changed

The 2030 World Cup row of the host table (`season_intl.c`, `HOSTS` 0xb5e134, key 0x1d5) held nation ids 150, 8,
145 and 201: Portugal, Argentina, Paraguay, Uruguay. Morocco (126) and Spain (172) were missing. The rows are written
from the rules table's host names, in English ("Morocco;Spain;Portugal;Argentina;Paraguay;Uruguay"), by
`nation_by_names`, which compares them with the nation names in memory. In this game they are Spanish: "Marruecos",
"España". The four names that are the same in Spanish were found; the other two were dropped. The first round had
been sized earlier, while the row still named Morocco (20 = 2 x (50 - 40)); the rows written again at a later season
change lost it.

Every lookup of `nation_by_names` (7 sites in `season_intl.c`) has the same fault in a game whose nation names are
not English: hosts of other competitions are dropped too, mostly without a stop.

## The fix (this fork; for the Bosman Edition as well)

1. `nation_by_names` (`season_intl.c`): when no nation in memory has the name, the English names of the game
   folder's own `Data/nation.dat` (0x122-byte records, the name at +4), by record: the ids are the same in the data of
   every language of the release (Morocco 126 and Spain 172 in the English data and in this Spanish save). A game in
   English finds every name in memory first, so it does not change.
2. `rt_wcq26_africa_groups_dealt` (`season_wcq26.c`, 0x910a4a in `tools/recomp/seasons.py`): the group builder deals
   no more groups than the pool's teams fill (+0xb6 - +0x3e / 2, in fives). A game whose counts agree does not
   change; a save whose host row is already wrong (this one) gets 8 groups and goes on.

A game in English, and every game whose first round and group count agree, makes the same decisions as before
(Q152): both changes act only where the old code dropped a host or left a place empty.
