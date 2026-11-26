# The non-player fields of the GK Save Game Editor in a saved game

26 November 2026. Asked for the Managers tab of Squad Hub (`web/savedit.js` `managerAttributes`,
`writeManagerAttributes`, `nonPlayers`; `web/editor.js` `showManager`): a player should be able to change a manager or
any other non-player staff member as fully as Graeme Kelly's "GK Save Game Editor" does on its Edit Player/Staff screen
(Attributes 1 → Non-Player, and the Staff Details, Attributes 2 and Contract tabs it shares with players). For each
field: the save member, offset, size and values, an address of the clean 3.9.68 `cm0102.exe` that reads or writes it,
and values on the save "Hertha BSC 2011-12-11" (game date 11 March 2012). The human managers of that save (Klaus
Kastelmann, Mike Penzin) were edited (ability 200, reputations 10,000), so the samples are AI staff: Arsène Wenger
(staff 70871, Arsenal manager, non-player record 13482), Glenn Ferguson (27642, Bury coach, 9488), Peter Hill-Wood
(55897, Arsenal chairman), Sven-Göran Eriksson (20000, England manager, no club).

Already known, not repeated: the twelve attributes the game shows and their formula
(`docs/research/2026-11-26-manager-attributes.md`); the staff row, `Preferences.dat` and `contract.dat` layouts
(`docs/research/2026-11-26-player-editor-fields.md`). Offsets are of the record in the save member; a link is a
4-byte signed id, -1 none.

## The record (nonplayer.dat)

68 bytes (0x44) a record, row = id; 44,810 records on the save, 44,800 linked from a staff row (+0x69), every linked
id at its index. The memory table is `[0xae23c0]`, its count `[0xae2368]` (`save_database` 0x52e33b..0x52e40f steps
0x44). The field boundaries are from the database reader of the old format, which reads each field by size
(`db_files_read_players_v1` 0x54e9a0 in the function map: its name and that of 0x54f150 are swapped; 0x54e9a0 fills
the 68-byte records): +0x00 4, +0x04 2, +0x06 2, +0x08/+0x0a/+0x0c 2 each, +0x0e..+0x22 one byte each (21),
+0x23..+0x3f eight times 4, +0x43 1 (0x54ea07..0x54ef95).

`database_non_player_to_attribute_vector` 0x53a8b0 copies the 21 bytes in two groups and returns the split 13
(0x53a934): first the 13 bytes kept as they are (+0x0e, +0x0f, +0x12, +0x13, +0x14, +0x15, +0x16, +0x1a, +0x1c,
+0x1d, +0x1f, +0x20, +0x22), then the 8 relative to current ability (+0x11, +0x10, +0x17, +0x18, +0x19, +0x1b, +0x1e,
+0x21). This matches `staff_compute_non_player_attribute` 0x543fd0 for the bytes it shows. A relative byte r is shown
as the rating of r at the record's ability (`non_player_attribute_rating` 0x544410 and 0x543fd0, `rating` in
`web/savedit.js`), 1..20; a raw byte is 1..20 itself.

Names: the game's own words are given where a screen names the byte (the staff profile, `staff_screens_draw_profile_tab`
0x87a510, its chairman part 0x87b0f8..0x87b844 and its style part 0x87b844..0x87bd40). The 21 bytes are in alphabetical
order of their names (Attacking, Business, Coaching, Coaching Goalkeepers, Coaching Technique, Directness, Discipline,
Free Roles, Interference, Judgement, Judging Potential, Man Handling, Marking, Motivating, Offsides, Patience,
Physiotherapy, Pressing, Resources, Tactics, Youngsters), the same order the game uses for its other attribute lists;
every byte a screen names fits that order. +0x15 is the one byte no screen names.

| Field | Offset | Size | Values | Shown | Code | Wenger | G. Ferguson | Hill-Wood |
|---|---|---|---|---|---|---|---|---|
| (id) | +0x00 | 4 | = the row | | `save_database` 0x52e33b | 13482 | 9488 | |
| Current Ability | +0x04 | 2, signed | 1..200 (the save also holds 0 and values down to -119) | as Squad Hub does | 0x544419 | 190 | 38 | 180 |
| Potential Ability | +0x06 | 2, signed | 1..200 | | `player_regen_regenerate_as_non_player` 0x7ab4bc | 200 | 50 | |
| Home Reputation | +0x08 | 2, signed | 0..10,000 (save: -2,134..10,000; 16,854 hold 1) | hidden | `database_get_staff_reputation_for_viewer` 0x540810 (scope 2, same nation) | 8,752 | 2,078 | 3,150 |
| Current Reputation | +0x0a | 2, signed | 0..10,000 | hidden | 0x54081d (scope 0, same club or league) | 7,576 | 2,349 | 3,150 |
| World Reputation | +0x0c | 2, signed | 0..10,000 | hidden | 0x540803 (scope 4); scope 3 = half home + half world (0x540829) | 8,491 | 513 | 3,150 |
| Attacking | +0x0e | 1, raw | 1..20 | as words: "Preferred Style" (≥15 attacking, ≤5 defensive, with Directness) | the profile 0x87ba8b; the opposition report 0x7be828; the match engine reads the manager's +0x0e with Tactics 0x6ed363 | 17 | 7 | 11 |
| Business | +0x0f | 1, raw | 1..20 (30,988 records hold 1, most non-chairmen) | chairman profile "Business Acumen": the number (0x87b1eb) and words (0x87b22c: ≥15 astute, ≥10 competent, ≥5 average, else poor) | weekly merchandising income `FinanceTable::add_weekly_merchandising_income` 0x59b7fe; `Finance::get_transfer_budget` 0x598b24 | 12 | 3 | 6 |
| Coaching Outfield Players | +0x10 | 1, relative | signed | shown (attribute 3) | 0x5440ec | -14 (17) | 17 (11) | 5 (19) |
| Coaching Goalkeepers | +0x11 | 1, relative | signed | shown (attribute 2) | 0x544059 | -50 (12) | -8 (8) | -55 (11) |
| Coaching Technique (coaching style) | +0x12 | 1, raw | 1..20 | "Coaching Style": ≤5 Fitness-based, ≥15 Technique-based, else General (0x87af1a) | `score_staff_for_job` 0x69d7da | 10 | 1 | 8 |
| Directness | +0x13 | 1, raw | 1..20 | in "Preferred Style" (≥15 direct, ≤5 patient, 0x87ba93) | opposition report 0x7be82f | 8 | 10 | 14 |
| Level of Discipline | +0x14 | 1, raw | 1..20 | shown (attribute 7); chairman "Discipline" number and words 0x87b2cd | 0x544186; `match_eng_update_post_match_reputations` 0x6d1de0 | 10 | 13 | 20 |
| (Free Roles, not confirmed) | +0x15 | 1, raw | 1..20 | hidden | only `score_staff_for_job` 0x69d867 (the chairman's byte against the candidate's) | 17 | 2 | 12 |
| Interference | +0x16 | 1, raw | 1..20 | chairman "Interference" number and words 0x87b3a4 (≥15 "Very hands on, liable to interfere in transfer decisions") | `score_staff_for_job` 0x69d76c (how much the chairman's own style counts in choosing a manager); `FinanceTable::try_board_takeover` 0x59ea41 | 15 | 2 | 13 |
| Judging Player Ability | +0x17 | 1, relative | signed | shown (attribute 5) | 0x54414e; 0x6d1dba | -19 (16) | -8 (8) | 5 (19) |
| Judging Player Potential | +0x18 | 1, relative | signed | shown (attribute 6) | 0x54416a | -9 (18) | 17 (11) | 5 (19) |
| Man Management / Man Handling | +0x19 | 1, relative | signed | shown (attribute 8); chairman "Man Handling" words from the rating (0x87b46f, 0x544410: ≥15 good, ≥3 average, else poor) | 0x544190 | 6 (20) | -13 (7) | -5 (18) |
| Marking | +0x1a | 1, raw | 1..20 | ≥15: "Likes to deploy a man-marking system" (0x87bc48) | `score_staff_for_job` 0x69d894 | 4 | 10 | 9 |
| Motivating | +0x1b | 1, relative | signed | shown (attribute 9) | 0x5441af | -4 (19) | -23 (6) | 5 (19) |
| Offsides | +0x1c | 1, raw | 1..20 | ≥15: "Likes to play the offside trap" (0x87bbd9) | (no other reader found) | 15 | 5 | 20 |
| Patience | +0x1d | 1, raw | 1..20 | chairman "Patience" number and words 0x87b56c (≥15 very patient, >5 patient, else impatient) | `MANAGER_MANAGER::weekly_board_confidence_update` 0x698891 (a value below 1 is set to 10, 0x698897) | 16 | 2 | 10 |
| Physiotherapy | +0x1e | 1, relative | signed (save: -95..125) | shown (attribute 10) only for a physio (job 10, club or national) or a rating of 10 or more (0x87ad6b..0x87ad8c) | 0x5441ce | -90 (7) | -18 (7) | -30 (14) |
| Pressing | +0x1f | 1, raw | 1..20 | ≥15: "Likes his players to close down the opposition" (0x87bb8f) | `Media::pick_ai_manager_response` 0x736034 | 17 | 3 | 20 |
| Resources | +0x20 | 1, raw | 1..20 | chairman "Resources" number and words 0x87b642 (≥15 vastly wealthy, >5 rich, else wealthy) | `FinanceTable::try_cash_injection` 0x59e687; `try_board_debt_payment` 0x59de2f | 12 | 4 | 12 |
| Tactical Knowledge | +0x21 | 1, relative | signed | shown (attribute 11) | 0x5441ed; match engine 0x6ed36b | 6 (20) | -8 (8) | -20 (15) |
| Working With Youngsters | +0x22 | 1, raw | 1..20 | shown (attribute 12) | 0x54420c | 20 | 9 | 20 |
| (eight staff links) | +0x23, +0x27, +0x2b, +0x2f, +0x33, +0x37, +0x3b, +0x3f | 4 each | staff id, -1 none | hidden | see below | all -1 | all -1 | all -1 |
| Preferred Formation | +0x43 | 1 | 0 none ("-"), 1 5-3-2 Sweeper, 2 5-3-2, 3 4-4-2, 4 4-3-3, 5 4-2-4, 6 3-5-2, 7 4-1-2-1-2, 8 4-5-1, 9 3-4-3 | "Preferred Formation" (0x87b90d) | opposition report 0x7be772 (other values read as 4-4-2); `Shortlist_Manager::assess_club_squad` 0x83ce14 | 3 | 3 | 3 |

The number in brackets is the rating the game shows (the formula of 0x543fd0 at the record's ability). The save's
distribution: every raw byte holds 1..20 and nothing else; the relative bytes hold -95..70 (Physiotherapy -95..125),
7,700 records hold -80 in each, the "cannot" value.

Shown on the staff profile (0x87ad4e..0x87ae05): for a non-player who is not a chairman, the twelve attributes, Coaching
Style, then Preferred Formation, Preferred Style and the "Other" lines (Pressing, Offsides, Marking). For a chairman
(job 1), the six chairman lines (Business Acumen, Discipline, Interference, Man Handling, Patience, Resources) and
Ambition (staff +0x57, 0x87b715, with Resources 0x87b75d), only when the byte `[[0xae2d0c] + id × 0x4f + 0xb]` has
bit 0x40 (0x87b117); its meaning is not confirmed. Attacking, Business, Discipline, Interference, Patience and
Resources are shown as numbers on the chairman part (format "%d", 0x87b1f0), so they are raw 1..20 numbers there too.

### The eight staff links (+0x23..+0x3f)

Eight 4-byte pointers to staff rows: the load resolves each id below the staff count to the staff row
`[0xae23bc] + id × 110` and any other value to none (inside `resolve_database_indexes` 0x5315a0, 0x5325ab..0x5326f1);
the save writes each as the row's id or -1 (`save_database` 0x52e341..0x52e3ff); the size trim remaps them
(`database_trim_staff_tables` 0x524d70, 0x525c8c..0x525d09); a new game clears all eight (`database_player_initialisation`
0x53884f..0x538864). No other reader or writer was found (a search of every load of staff +0x69 and of the eight
offsets together). Every linked record of the save holds -1 in all eight. What they name is not confirmed; an editor
should leave them.

## What the game works out again

- Reputations (+0x08..+0x0c): `game_daily_staff_update` lowers them in one branch, keeping current and home at least
  35 × ability and world at least 5 × ability (0x5d3710..0x5d3776; when that branch runs is not confirmed);
  `MANAGER_MANAGER::reset_board_for_new_manager` moves the new manager's home reputation part of the way to the
  club's reputation (0x694d26..0x694d47); `award_raise_winner_reputation` raises them for an award (0x4130bd, 0x4130e8).
- Chairman bytes: a board takeover gives the new chairman random Patience, Business, Interference and Resources, 1..20
  (`FinanceTable::try_board_takeover` 0x59ebd8..0x59ec2c); a finance boost lowers Resources by 1 when above 5
  (`try_finance_boost_event` 0x59e3fe..0x59e407); the weekly board update sets a Patience below 1 to 10 (0x698897).
- Preferred Formation: a new game sets it for a few named managers (Alex Ferguson 4-4-2 with Directness 7 and
  Attacking 17, 0x5386fd..0x538717, and others by name to 0x5387f0) and draws one at random when it is 0 or 10 and over
  (0x5387f6..0x538849); a running game does not change it (no other writer found). A retired player made a non-player
  draws one, 0 to 9 (`player_regen_regenerate_as_non_player` 0x7ab94c..0x7ab9cc).
- A new game fills raw bytes outside 1..20 at random (0x5380e5..0x5381fd for Directness, Attacking, Motivating,
  Pressing, Tactics; more to 0x538604) and gives an unemployed non-player a job from his bytes (0x538867..0x5388cb).
  Within a running game no change of the other bytes was found except through regeneration.
- Changing Current Ability changes every relative rating; `writeManagerAttributes` already rewrites the relative
  bytes so the shown numbers stay.

## The staff row, Preferences.dat and the contract for a non-player

The staff row fields of the player note apply unchanged; checked on non-players:

| Field | Offset | Non-player use | Code | Samples |
|---|---|---|---|---|
| Nation, 2nd Nation | +0x1a, +0x1e | as for players | `database_is_staff_foreign` 0x53d9a0 | Wenger 70, -1; Ferguson 129, -1 |
| Caps, Goals | +0x22, +0x23 | his caps as a player; a non-player made from a retired player keeps them, the database non-player starts at 0 (`player_regen_regenerate_as_non_player` clears them, 0x7ab378) | 0x4869a7 | Ferguson 5, 0; Wenger 0, 0 |
| National team, job there | +0x24, +0x28 | the nat_club id and the job for it (5 Manager, 16 retired; 86 non-players hold 5); with no club the game takes the job from here (0x7ab598..0x7ab59f), the profile tests +0x28 = 10 for a national physio (0x87ad76) | `national_teams_player_in_squad` 0x76daae | Eriksson 10972 England, job 5, club -1, staff job +0x3d 5 |
| Job | +0x3d | 0..17 as in the player note; a non-player holds 0..17 except 7 (save: 4,223 managers, 2,816 chairmen, 12,337 scouts, 10,484 coaches, 7,703 physios) | `database_get_job_name` 0x53ad0f | Wenger 5, Ferguson 8, Hill-Wood 1 |
| Classification | +0x5f | 1 non-player | 0x7ab367 | 1 |
| Personality (Attributes 2) | +0x56..+0x5d | every non-player has them; Adaptability and Determination are two of the twelve (0x54404f, 0x544144), Ambition is on the chairman profile (0x87b715) | | Wenger 20, 20, 20, 17, 20, 20, 20, 20 |
| Transfer Value | +0x52 | held for non-players too | | Ferguson 46,750; Wenger 0 |
| Squads | +0x5e | bit 0x01 set for non-players at a club too | | Wenger 1 |
| Preferences | +0x65 → Preferences.dat | as for players | `staff_favours_club` 0x547d77 | Wenger row 70871 |
| Non-player link | +0x69 | the nonplayer.dat row; -1 for a pure player; 647 staff rows have both a player and a non-player record (player-managers, players with coaching data such as Gerrard) | 0x543fd0 | Wenger 13482 |

Contract: a non-player's contract is a `contract.dat` record of the same 80-byte layout, found by staff id, when he
has one. On the save 23,436 of the 39,902 non-players at a club have one; the rest (most managers and chairmen of
the original database: 2,839 managers, 2,545 chairmen, 3,291 scouts) have none. With none, the contract tab shows
"N/A" for the type (0x87cf6d) and "-" for the wage (0x87d014), and the staff row's wage (+0x4e) and contract end
(+0x46) are the database's values, which nothing updates (Wenger: wage 0, end 1.6.2002 in a 2012 game, joined
28.9.1996). Glenn Ferguson has one: club 1693, wage 275 (staff +0x4e 275), start 24.12.2011, end 13.6.2015 (staff
+0x46 the same), type 2, squad number 0, squad status 5, no bonuses, no clauses. Squad Hub can edit an existing
non-player contract as a player's; making a contract for a non-player who has none means adding a record to
contract.dat and the table built at load (`CONTRACT_MANAGER::get_contract` 0x4cf5a0), which is not attempted.

## What an editor should do

- Raw bytes: write 1..20. Relative bytes: write the byte that shows the wanted 1..20 at the current ability
  (`storedFor`), as Squad Hub does for the twelve; the eight relative bytes are all among the twelve.
- Reputations: 0..10,000 each; they move with the game (above).
- Preferred Formation: 0..9.
- Business, Interference, Patience, Resources matter for a chairman (job 1); Attacking, Directness, Pressing, Offsides,
  Marking, Coaching Technique and Preferred Formation for a manager. All are kept for every non-player.
- Leave the eight staff links (+0x23..+0x3f) as they are.

## Not confirmed

- +0x15: the name Free Roles (from the alphabetical order only; its only reader is `score_staff_for_job`).
- The name Coaching Technique for +0x12 (the screen calls it Coaching Style).
- What the eight staff links +0x23..+0x3f are for.
- The bit 0x40 of `[[0xae2d0c] + id × 0x4f + 0xb]` that the chairman lines need.
- When the daily reputation decline (0x5d3710) runs.
- Current Ability values 0 and below on the save (seen, not explained).
- GK's own mapping of its boxes to these bytes: the game's side is read, GK's is inference.

## Scripts

Read with `web/savedit.js` (`SaveGame`, `nonPlayers`, `rating`), scripts kept outside the repo. The code was read with
a capstone disassembly of the clean 3.9.68 `cm0102.exe`.
