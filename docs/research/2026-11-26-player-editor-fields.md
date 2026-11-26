# The player fields of the GK Save Game Editor in a saved game

26 November 2026. Asked for the player editor of Squad Hub (`web/savedit.js`, `web/editor.js`): a player should be able
to change what Graeme Kelly's "GK Save Game Editor" changes on its Edit Player screen (tabs Staff Details, Attributes 1,
Attributes 2, Contract). For each field: the save member, offset, size and values, an address of the clean 3.9.68
`cm0102.exe` that reads or writes it, and the value on the save "Hertha BSC 2011-12-11" (game date 11 March 2012) for
Joe Cole (staff 7202, Hertha BSC Berlin) and Steven Gerrard (staff 55141, Liverpool). The players of this save are
edited (every ability 200), so the values only show where a field is; the player's screenshots (Patrice Evra, a 2002
game) are not compared.

Already edited by Squad Hub, not repeated here: current and potential ability (player +0x05, +0x07), the twelve
position bytes, every attribute (`ATTRIBUTES` in `web/savedit.js`), date of birth and age (staff +0x10, +0x18),
Determination (staff +0x58). See `docs/research/2026-11-10-save-editor.md`.

Offsets are of the record in the save member. A link is a 4-byte signed id, -1 none (the save writes pointers as ids,
`docs/research/2026-11-26-club-editor-fields.md`). In memory the game keeps pointers where the save has ids (staff
+0x61 is the player record, +0x65 the preferences record, +0x39 the club); the code addresses below read the memory
form.

## The members

| Member | Record | Count on the save | Found by |
|---|---|---|---|
| `staff.dat` | 0x6e (110) | 158,855 | row = staff id |
| `player.dat` | 0x46 (70) | 115,260 | staff +0x61 |
| `Preferences.dat` | 0x34 (52) | 139,738 | staff +0x65 (= the staff id for both players) |
| `first_names.dat`, `second_names.dat`, `common_names.dat` | 0x3c (60) | 44,299; 104,413; 13,272 | staff +0x04, +0x08, +0x0c |
| `contract.dat` | see below; contracts of 0x50 (80) | 190,086 contracts | search by staff id at +0x00 |
| `injury.dat` | 31 bytes per staff first | 158,855 | staff id × 31 |

Joe Cole: player row 6117, preferences 7202, contract 5308. Gerrard: player 45672, preferences 55141, contract 39653.

## Staff Details tab

| GK field | Member | Offset | Size | Values | Code | Joe Cole | Gerrard |
|---|---|---|---|---|---|---|---|
| Nation | staff | +0x1a | 4 | nation id | `database_get_reputation_scope` 0x5408fd compares two people's +0x1a; `award_raise_winner_reputation` 0x412f6c compares it with the club's nation; `database_is_staff_foreign` 0x53d9a0 | 61 England | 61 England |
| 2nd Nation | staff | +0x1e | 4 | nation id, -1 none | `database_is_staff_foreign` 0x53d9af (a player is not foreign when either nation is the club's) | -1 | -1 |
| Caps | staff | +0x22 | 1, unsigned | 0..255 | `compare_players_by_caps_then_name` 0x4869a7 | 68 | 85 |
| (international) Goals | staff | +0x23 | 1, unsigned | 0..255 | `compare_players_by_international_goals_then_name` 0x486a67 | 9 | 8 |
| (national team) | staff | +0x24 | 4 | nat_club id (club ids go on after club.dat: 10972 = England), -1 none | `national_teams_player_in_squad` 0x76daae (with the main side 0x53b3d0) and 0x76dad5 (U21 side 0x53b4f0) | 10972 England | 10972 England |
| (job for the nation) | staff | +0x28 | 1 | the job codes below (11 Player) | | 11 | 11 |
| (joined, leaves the nation job) | staff | +0x29, +0x31 | 8 each | dates | | none, none | none, 10.6.2005 |
| Transfer Value | staff | +0x52 | 4, signed | money | written by `Shortlist_Manager::rebuild_club_shortlist` 0x84ae30 (= `contract_manager_calc_player_value` 0x4d0c60) and by `TRANSFER_MANAGER::set_player_squad_status` 0x8ce1e9 (the asking price the human sets); `contract_manager_calc_asking_price` returns it at 0x4d16dd for a human club whose contract has "Auto" off | 9,528,747 | 20,000,000 |
| Squads: Club Main | staff | +0x5e bit 0x01 | 1 | flag | `PLAYER_SEARCH::add_own_squad` 0x7b0f30; `VIRTUAL_STAFF::create_virtual_player` writes 1, 2, 4 or 8 (0x90e46c..0x90e482) | 1 | 1 |
| Squads: Club Reserve | staff | +0x5e bit 0x02 | | flag | 0x7b0f1b; the profile's "(Reserves)" line 0x870b2a | 0 | 0 |
| Squads: Nation U21 | staff | +0x5e bit 0x04 | | flag, with +0x24 the nation's U21 side | `national_teams_player_in_squad` 0x76dadf | 1 | 1 |
| Squads: Nation Main | staff | +0x5e bit 0x08 | | flag, with +0x24 the nation's first side | 0x76dab8; `NationalTeams::select_squad` 0x76cdc7 | 0 | 0 |
| Euro Registered? / Euro Cup-Tied? | staff | +0x6d | 1 | a state, not bits: 0 not registered, 2 registered, 1 added and 4 taken off on the squad numbers screen but not yet submitted, 8 and 16 "Ine" (ineligible); the save holds 0, 2, 4, 8, 16 | the screen 0x478e88..0x478fd0 ("In" 0x9910f8, "Out" 0x9910c8, "Ine" 0x98e994); `SQUAD_MANAGER::select_euro_registered_players` skips 8 and 16 (0x85aba1) and turns 1 into 0, 4 into 2 (0x85abc7); `CONTRACT_MANAGER::end_loan` turns 16 into 4 (0x4d0b33) | 2 | 2 |
| Classification | staff | +0x5f | 1 | 1 non-player (written by retirement 0x7a6362, `player_regen_regenerate_as_non_player` 0x7ab367, the extra staff 0x527545), 2 player (`PLAYER_REGEN::regenerate_as_youth_player` 0x7abb2e); the save also holds 0 and 3 | as written | 2 | 2 |
| Job | staff | +0x3d | 1 | 0 Unemployed, 1 Chairman, 2 Managing Director, 3 General Manager, 4 Director of Football, 5 Manager, 6 Assistant Manager, 7 Reserve Manager, 8 Coach, 9 Scout, 10 Physio, 11 Player, 12 Player/Manager, 13 Player/Assistant Manager, 15 Player/Coach, 16 Retired Player, 17 Media Pundit (14 none) | `database_get_job_name` 0x53ad0f (jump table 0x53b384); `database_is_player_job` 0x53b620: 11, 12, 13, 15 | 11 | 11 |

Positions (player +0x0f..+0x1a), in GK's order: +0x0f Goalie, +0x10 Sweeper, +0x11 Defender, +0x12 Def Mid, +0x13
Midfielder, +0x14 Att Mid, +0x15 Attacker, +0x16 Wing Back, +0x17 Right, +0x18 Left, +0x19 Centre, +0x1a Free Role
(`POSITIONS` in `web/savedit.js`). Code: `staff_get_natural_position_mask` 0x5481a0 reads +0x0f as the goalkeeper
(rating 20 gives mask 1, 0x5481b8), +0x10, +0x11, +0x12, +0x13.. as outfield positions and +0x17, +0x18, +0x19 as the
sides (each combined with +0x19, 0x5481c6..0x5481fa). Which of +0x17 / +0x18 is Right and which is Left is from the
data, not the code: Joe Cole (left-sided) holds Right 10, Left 16, Centre 20; Gerrard holds Right 20, Left 1, Centre 20.
Joe Cole: 1 1 1 1 20 20 10 1 | 10 16 20 16. Gerrard: 1 5 12 20 20 10 1 10 | 20 1 20 1.

A Nation B Team checkbox: not found. +0x5e holds only the bits 1, 2, 4 and 8 on the save (93,198 rows 1, 44,330 rows
2, 3,738 rows 5, 775 rows 9, ...), and no other byte of the staff row was found that marks a B team.

## Attributes 1 tab

| GK field | Member | Offset | Size | Values | Code | Joe Cole | Gerrard |
|---|---|---|---|---|---|---|---|
| Morale | player | +0x45 | 1 | 1..20 | `contract_manager_get_player_morale` 0x4cc2d7; written by `staff_adjust_morale` 0x544811 (clamps 1..20, and keeps it down while contract +0x45 is set, 0x5447f0); words of `contract_manager_write_morale_text` 0x4cc2e0: below 4 Very Low, below 8 Low, below 12 Ok, below 15 Good, below 18 Very Good, else Superb | 17 | 14 |
| Home Reputation | player | +0x09 | 2, signed | 0..10,000 | `database_get_staff_reputation_for_viewer` 0x540886 (scope 2, same nation); `award_raise_winner_reputation` 0x412f90 (cap 10,000 at 0x412f9d) | 9,391 | 9,895 |
| Current Reputation | player | +0x0b | 2, signed | 0..10,000 | 0x540893 (scope 0, same club or league) | 9,212 | 9,895 |
| World Reputation | player | +0x0d | 2, signed | 0..10,000 | 0x540879 (scope 4); 0x412f39 | 7,390 | 9,909 |
| Current / Potential Ability | player | +0x05, +0x07 | 2 each | already edited | | 200, 200 | 200, 200 |

For a non-player the same three are in nonplayer.dat +0x08, +0x0a, +0x0c (0x540810, 0x54081d, 0x540803).

Player +0x04 (1 byte; Joe 26, Gerrard 17) is not the squad number of a staff member of the database: see Squad Number.

## Attributes 2 tab (personality)

The eight bytes are staff +0x56..+0x5d, signed, 1..20: a new game fills any byte outside 1..20 at random
(`database_player_initialisation`, 0x53603a.. for +0x5c, 0x53607d for +0x5d) and moves Sportsmanship by the nation
(0x5360e7).

| GK field | Offset | Code | Joe Cole | Gerrard |
|---|---|---|---|---|
| Adaptability | +0x56 | `staff_compute_non_player_attribute` 0x54404f | 20 | 20 |
| Ambition | +0x57 | the board member's profile "Ambition" line 0x87b715; `contract_manager_get_deserved_squad_status` 0x4ca4ee (25 × ambition - 10 × loyalty + world reputation > 6,250) | 19 | 20 |
| Determination | +0x58 | 0x54376c (already edited); the scout's "determination" flag 0x7bdb24 | 20 | 19 |
| Loyalty | +0x59 | the scout's "loyalty" flag `player_search_assess_player_traits` 0x7bdaec; `staff_fade_staff_relationships` 0x547fb0 (odds of forgetting a favoured team-mate) | 20 | 20 |
| Pressure | +0x5a | the scout's "ability to handle pressure" flag 0x7bdb08, "unable to handle pressure" 0x7bdcb5 | 20 | 20 |
| Professionalism | +0x5b | the scout's "professional approach" flag 0x7bdb60, "unprofessional" 0x7bdd0c; the roll that raises Dirtiness after a red card 0x75a50f | 20 | 20 |
| Sportsmanship | +0x5c | lowered by one after the reply to a sending-off story (`Media::apply_sent_off_story_response` 0x75a53d); read by the referee, `MATCH_OFFICIAL::judge_tackle` 0x6e9d2f | 20 | 19 |
| Temperament | +0x5d | the scout's "maturity" flag 0x7bdb7d, "highly strung" 0x7bdd28 | 20 | 20 |

The names follow the alphabetical order the game uses for its other attribute lists (Adaptability +0x56 and
Determination +0x58 are confirmed by 0x543b10/0x543fd0); the code above fits each name, but no screen of the game names
+0x59..+0x5d, so the name of each is an inference from its use.

## Contract tab

The contract of a staff member is in `contract.dat`, not in the staff row. Layout of the member, from
`CONTRACT_MANAGER::save_to_disk` 0x4dc3a0 (checked: the parts add up to the member's 16,751,220 bytes exactly):

| Part | Size | What |
|---|---|---|
| int count, then count × 21 | 4 + 3 × 21 | talks records (this+0x28) |
| int count, then count × 80 | 4 + 190,086 × 80 | the contracts (this+0); a staff member's main contract is the one with his id at +0x00 (the game finds it through a table built at load, `CONTRACT_MANAGER::get_contract` 0x4cf5a0) |
| int count, then count × 14 | 4 + 6,186 × 14 | decision dates |
| int | 4 | (this+0x18) |
| u16 count, then count × 111 | 2 + 3,691 × 111 | contract offers |
| u16 count, then count × 111 | 2 + 9,441 × 111 | held offers |
| byte | 1 | |

A loan contract is a second record of the same layout (`CONTRACT_MANAGER::get_loan_contract` 0x4cf6d0); neither
player has one.

The 80-byte contract (`STAFF_CONTRACT::operator=` 0x4cc810 gives the field boundaries):

| GK field | Offset | Size | Values | Code | Joe Cole | Gerrard |
|---|---|---|---|---|---|---|
| (staff) | +0x00 | 4 | staff id | | 7202 | 55141 |
| Club | +0x04 | 4 | club id | the contract tab 0x87d4da | 4232 Hertha | 5583 Liverpool |
| (unknown) | +0x08 | 4 | not confirmed | | 1,408,430 | 2,211,003 |
| Wage | +0x0c | 4, signed | weekly | `CONTRACT_MANAGER::get_wage` 0x4d0c59 | 58,685 | 82,200 |
| Goal, Assist, Clean Sheet Bonus | +0x10, +0x14, +0x18 | 4 each | money, -1 none | the contract tab 0x87d735..0x87d741 shows a bonus above -1; names 0x85f830 (0 goal, 1 assist, 2 clean sheet) | -1, 1,558, -1 | -1, 2,793, -1 |
| Clauses | +0x1c..+0x20 | 1 each | 0 off, else on: +0x1c Non Promotion, +0x1d Minimum Fee, +0x1e Non Playing, +0x1f Relegation, +0x20 Manager Job Release Clause | names `staff_contract_get_release_clause_name` 0x867a00 (jump table 0x867a84); the tab 0x87d864 | all 0 | all 0 |
| Release clause fee | +0x21 | 4, signed | money, 0 none | the tab 0x87d870 | 0 | 0 |
| Contract start | +0x25 | 8 | date | the tab's "Started" 0x87d0e4 | 4.1.2012 | 30.10.2010 |
| Contract end | +0x2d | 8 | date | the expiry check of `CONTRACT_MANAGER::daily_update` 0x4c7ea0 | 4.6.2016 | 28.5.2014 |
| Contract type | +0x35 bits 0..5 (signed) | | 1 rolling monthly, 2 contract, 3 expired, 4 free, 5 trial, 6 and 7 loan | the tab 0x87cee4 (jump tables 0x87dd38, 0x87dd58); 0x85e8f0 | 2 | 2 |
| Part time | +0x35 bit 0x80 | | 1 part time | 0x87cee7 | 0 | 0 |
| (unknown) | +0x35 bit 0x40 | | not confirmed | | 1 | 0 |
| Squad Number | +0x3a | 1, signed | -50..50; the game shows the absolute value, 0 none | `SQUAD_MANAGER::get_squad_number` 0x85a7e8, `set_squad_number` 0x85a8da (refuses outside -50..50, 0x85a87f); the squad tab 0x45bae9 and the profile 0x870b66 show `abs` | -39 (shows 39) | -6 (shows 6) |
| Happiness | +0x3d | 1, signed | -100..100 | `set_player_squad_status` clamps it (0x8ce211, 0x8ce21c); `contract_manager_get_deserved_squad_status` reads it (0x4ca4c7) | 26 | 50 |
| (a second happiness) | +0x44 | 1, signed | -100..100 | `set_player_squad_status` 0x8ce25c..0x8ce2cc; meaning not confirmed | 100 | 100 |
| (unsettled) | +0x45 | 4 | not 0: morale is held down (0x5447f0); the transfer status screen passes "not 0" as an option (0x8f68f9) | | 0 | 0 |
| Transfer status | +0x4e | 1, bits | 0x01 Transfer Listed (`staff_is_transfer_listed` 0x5444f2, cleared by 0x84429c), 0x02 Listed for Loan (`staff_is_loan_listed` 0x544590, set 0x8442c4), 0x04 Auto asking price (the screen's "Auto" 0x8f6923; with it the asking price is calculated, without it a human club's staff +0x52 is the price, 0x4d16d7), 0x08 listed at his own request (0x544651, the screen 0x8f6889) | | 0x04 | 0x00 |
| Squad Status | +0x4f bits 4..7 | | 0 Undefined, 1 Key, 2 First Team, 3 Rotation, 4 Backup, 5 Prospect, 6 Youngster, 7 Not Needed | `STAFF_CONTRACT::get_squad_status_text` 0x867f6f (jump table 0x868000); written by `set_player_squad_status` 0x8ce8ac | 1 Key | 1 Key |
| (unknown) | +0x4f bits 0..3 | | not confirmed | | 10 | 10 |

Not For Sale: no bit of +0x4e was found that the game reads as "not for sale"; the save holds +0x4e values 0, 4, 5,
6, 12, 13, 14 only.

The staff row keeps copies of some of these; `CONTRACT_MANAGER::sign_contract` 0x4cf720 writes the end date and the wage there:

| Staff field | Offset | Size | Copy of | Code | Joe Cole | Gerrard |
|---|---|---|---|---|---|---|
| Club | +0x39 | 4 | the club of the contract | | 4232 | 5583 |
| Job | +0x3d | 1 | the job | `SQUAD_MANAGER::get_squad_number` reads it 0x85a7a5 | 11 | 11 |
| Date joined (the club) | +0x3e | 8 | not the contract start: the day he joined | read by `sign_contract` 0x4cfa7c | 12.1.2010 | 2.1.1996 |
| Contract end | +0x46 | 8 | contract +0x2d | 0x4cf958 | 4.6.2016 | 28.5.2014 |
| Wage | +0x4e | 4 | contract +0x0c | 0x4d002f | 58,685 | 82,200 |

## Names

| GK field | Member | Offset | Size | Values | Code | Joe Cole | Gerrard |
|---|---|---|---|---|---|---|---|
| First name | staff | +0x04 | 4 | row of first_names.dat | `database_format_staff_name` 0x53dba0 | 1646 "Joe" | 2080 "Steven" |
| Second name | staff | +0x08 | 4 | row of second_names.dat | | 2240 "Cole" | 28026 "Gerrard" |
| Common name | staff | +0x0c | 4 | row of common_names.dat; row 0 is the empty name | 0x53dc05; `database_get_empty_common_name` 0x53a5b0 | 0 "" | 0 "" |

A name row (60 bytes): the name +0x00 (Windows-1252, NUL-ended, 51 bytes), id +0x33 (= the row), nation +0x37 (-2 on
the empty common name), weight +0x3b (1 byte, 1..100, not the number of people using the row: "Joe" holds 1 and 83
staff use it).

`database_build_nation_name_index` 0x539750 builds, for each nation, the first row, the row count and the weight
total of its names, by walking the table and taking runs of rows with the same nation (0x539888); rows whose nation is
-1 or lower are skipped (0x53988c). The random names of new people (`database_pick_first_name_by_weight` 0x539b90 and
the second name pickers) draw from these runs.

The safe way to rename: point +0x04/+0x08/+0x0c at a row that already holds the wanted spelling (any nation). To add a
spelling: append a row at the end of the member with id = its row and nation -1, and raise the table's count in
`index.dat` (the entry's +0x37, as `tablesOf` reads it in `web/savedit.js`; tables 13, 14, 15). With nation -1 the new
row is outside every nation's run, so the name index and the new-player names are not changed. An appended row with
a real nation would start a second run of that nation, which the index does not expect. That the game loads a save
whose name table has grown is not confirmed (not run).

## Favourite and disliked (Preferences.dat)

| GK field | Offset | Size | Values | Code | Joe Cole | Gerrard |
|---|---|---|---|---|---|---|
| (id) | +0x00 | 4 | the row | | 7202 | 55141 |
| Favourite clubs | +0x04, +0x08, +0x0c | 4 each | club id, -1 none | `staff_favours_club` 0x547d77 | 4232 Hertha, -1, -1 | 5583 Liverpool, -1, -1 |
| Disliked clubs | +0x10, +0x14, +0x18 | 4 each | club id, -1 none | `staff_dislikes_club` 0x547d47 | -1, -1, -1 | 3106 Everton, 5903 Manchester United, -1 |
| Favourite staff | +0x1c, +0x20, +0x24 | 4 each | staff id, -1 none | `staff_is_favoured_by` 0x547827 | 131172 Fernando Cavenaghi, 90540, 158853 | 130845 Julio Arca, 52873 Michael Bridges, 42761 Maxim Tsigalko |
| Disliked staff | +0x28, +0x2c, +0x30 | 4 each | staff id, -1 none | `staff_is_disliked_by` 0x547857 | -1, -1, -1 | -1, -1, -1 |

## Condition and injury (injury.dat)

`Injury::save` 0x62b2d0 writes first one 31-byte fitness record per staff (count = the staff count, 0x62b366), then
the injuries (int count, 0x30 bytes each; 26,400 on the save), the news queue (0x16 bytes each), the outbreaks (0x11
bytes each) and a list. The fitness record (`Injury::init_records` 0x62d031..0x62d056):

| Field | Offset | Size | Values | Code | Joe Cole | Gerrard |
|---|---|---|---|---|---|---|
| (last update) | +0x00 | 8 | date | | 11.3.2012 | 10.3.2012 |
| Fitness | +0x08 | 2 | 0..10,000 | `Injury::get_condition` 0x62c5e5 | 10,000 | 10,000 |
| Match condition | +0x0a | 2 | 0..10,000 | 0x62c5e0; condition % = fitness × match condition / 10,000, at most 90 % while recovering | 9,867 | 8,853 |
| Fatigue | +0x0c | 2, signed | | `Injury::get_fatigue` 0x62c3d0 | -724 | 1,160 |
| Current injury | +0x0e | 4 | index into the injuries, -1 none | `Injury::reset_player` 0x62bfee | -1 | -1 |
| Recurring injury type | +0x12 | 1 | 255 none | | 255 | 255 |
| Training load | +0x1e | 1 | init 50 | `Injury::set_training_load` 0x62c673 | 68 | 81 |

Squad Hub can set fitness and match condition; giving or curing an injury means changing the injury array and its
news, which is not attempted.

## What the game works out again (an edit lasts until then)

- Transfer Value, staff +0x52: recalculated by `Shortlist_Manager::rebuild_club_shortlist` (0x84ae30) for every squad
  player, except a human club's player whose "Auto" bit (contract +0x4e 0x04) is off: then +0x52 is his asking price
  and stays.
- Morale (player +0x45), happiness (contract +0x3d, +0x44), reputations (player +0x09..+0x0e), condition and fatigue
  (injury.dat): moved by matches and the daily passes.
- Squad status (contract +0x4f): the AI sets it for computer clubs; `contract_manager_get_deserved_squad_status`
  compares it with what the player wants, so a status far from that makes him unhappy.
- Euro registration (+0x6d): set again by the European registration (`select_euro_registered_players`).
- Squad number: `SQUAD_MANAGER::assign_club_squad_numbers` and `assign_league_squad_numbers` hand out numbers; the
  meaning of the sign (both players hold a negative number) is not confirmed.
- Personality: a new game fills bytes outside 1..20; in a running game Sportsmanship falls after sending-off stories.

## What must be changed together

- Wage: contract +0x0c and staff +0x4e. Contract end: contract +0x2d and staff +0x46. The game reads the contract
  (`get_wage`, the daily expiry check); `sign_contract` keeps the staff copies equal, so an editor should write both.
- Club (staff +0x39) and job (+0x3d) are not plain fields: the club's squad and staff slots (club +0xd7, 50 squad
  ids, and the job slots +0xbf..+0x1cf), the contract's +0x04, the squad numbers and the club finances
  (`Finance::update_wage_bill`, called by `sign_contract` and `remove_contract` 0x4d0b50) all follow them. Squad Hub
  should not move a player between clubs; the game's own transfer does it.
- Nation (+0x1a): the national team fields (+0x24, +0x28, the +0x5e bits 4 and 8) and the nation's squad lists are
  built for the old nation; change the nation of a player who is in no national squad (+0x5e bits 4 and 8 clear,
  +0x24 -1), or clear those too.
- Transfer listed / loan listed (+0x4e bits 1, 2): the game's listing also finds interested clubs and posts news
  (`Shortlist_Manager::transfer_list_player` 0x842320); setting the bit alone lists him without that.
- A release clause: flag +0x1c..+0x20 and, for the Minimum Fee clause, the fee +0x21
  (`docs/research/2026-10-15-contract-clauses.md`).
- Squad status: write only the high four bits of +0x4f; the low four bits are kept (as `set_player_squad_status`
  does, 0x8ce8a4).

## Not confirmed

- Which of +0x17 / +0x18 is Right and which Left (from the data only).
- The names of +0x59..+0x5d (inferred from their uses, see above).
- A Nation B Team flag; a Not For Sale flag.
- The meaning of the +0x6d values 1 and 4 beyond "added / taken off, not yet submitted", and of 8 against 16.
- Contract +0x08, +0x35 bit 0x40, +0x36, +0x3b, +0x3e, +0x3f, +0x40, +0x49, +0x4a, the low four bits of +0x4f, and
  what +0x44 and +0x45 mean.
- The sign of a squad number; player +0x04.
- Classification values 0 and 3; staff +0x60.
- U21 caps and goals: no field of the staff, player or preferences record holds them.
- That the game loads a save whose name tables have grown.
- GK's own mapping of its checkboxes and boxes to these bytes: the game's side is read, GK's is inference.

## Scripts

Read with `web/savedit.js` (`SaveGame`, `file.read(member)`), scripts kept outside the repo. The code was read with a
capstone disassembly of the clean 3.9.68 `cm0102.exe`.
