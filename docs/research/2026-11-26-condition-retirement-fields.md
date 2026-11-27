# Condition, retirement and the other contract fields of the GK Save Game Editor

26 November 2026. Asked for the player editor of Squad Hub (`web/savedit.js`, `web/editor.js`), after
`docs/research/2026-11-26-player-editor-fields.md`: what Graeme Kelly's "GK Save Game Editor" edits on its "Playing
Condition" sub-tab, the "Retiral" lines of its Attributes 2 tab, and the Contract tab's Contract Details, Unhappiness,
Club Unhappiness and Loan Contract sub-tabs. For each field: the save member, offset, size and values, an address of
the clean 3.9.68 `cm0102.exe` that reads or writes it, the values on the save "Hertha BSC 2011-12-11" (game date
11 March 2012), what the game works out again, and what must be changed together.

Offsets are of the record in the save member. The code addresses read the memory form; a member's records are the
memory records written as they are (`Injury::save` 0x62b2d0, `PLAYER_REGEN::save_retire_dat` 0x7ad7c0,
`save_game_data` 0x82e944). A date is 8 bytes: day of the year (2 bytes, signed, 0-based as `readDate` reads it),
year (2), leap flag (4; `Date::Date` 0x549f4f writes 1 or 0).

## The staff state table (general.dat)

`save_game_data` writes the count `[0xae2d10]` (0x82e91b) and then `[0xae2d0c]`, count × 79 bytes (0x82e941). In
general.dat the count is at +4003 (158,855, the staff count) and the first record at +4007; record = staff id. Found by
the contract index: Joe Cole (7202) holds 5308 and Steven Gerrard (55141) 39653 at +0x00, the contract rows of the
earlier doc.

| Field | Offset | Size | Values | Code |
|---|---|---|---|---|
| Main contract | +0x00 | 4 | row of the contracts in contract.dat, -1 none | `CONTRACT_MANAGER::get_contract` 0x4cf5c9 |
| Loan contract | +0x04 | 4 | row of the contracts in contract.dat, -1 none | `CONTRACT_MANAGER::get_loan_contract` 0x4cf6f9 |
| Retirement state | +0x0b bits 4..5 | | 0 none, 1 announced (retiring at the end of the season), 2 retired | `player_regen_get_retirement_status` 0x7a7ec4 (`>> 4 & 3`); set to 1 at 0x7a657f, cleared at 0x7a5ae8, 0x7a5b24, 0x7a5bb0 |

Joe Cole and Gerrard: +0x04 -1, +0x0b 0x00. On the save 4 staff hold state 1 and 576 state 2.

## Playing Condition (injury.dat)

The fitness record is 31 bytes per staff id at the start of injury.dat (the earlier doc). GK's three radio buttons
("Lacking Match Practice?", "Exhausted", "Normal") are not a stored flag: the game works the words out from fatigue
and the season's appearances each time the player screen is drawn.

| Field | Offset | Size | Values | Code | Joe Cole | Gerrard |
|---|---|---|---|---|---|---|
| (last update) | +0x00 | 8 | date; set to today by the daily update | `Injury::update_player_fitness` 0x62cddb | 11.3.2012 | 10.3.2012 |
| Fitness | +0x08 | 2, signed | 0..10,000 | read 0x62c5e5; written 0x62cdd3 | 10,000 | 10,000 |
| Match condition | +0x0a | 2, signed | 0..10,000 | read 0x62c5e0; written daily 0x62cdd7 (capped 10,000 at 0x62cdb9), after a match `Injury::set_match_condition` 0x62bd3c | 9,867 | 8,853 |
| Fatigue ("jadedness") | +0x0c | 2, signed | about -28,000..1,456 on the save | `Injury::get_fatigue` 0x62c3ec; after a match 0x62bd89, 0x62bdc0; each day `Injury::recover_fatigue` 0x62c2f0 | -724 | 1,160 |
| Current injury | +0x0e | 4 | index into the injuries, -1 none | 0x62c49c | -1 | -1 |

The condition (`Injury::get_condition` 0x62c590): raw = fitness × match condition / 10,000 (0x62c5e9..0x62c5fb), at
least 100 (0x62c613), at most 9,000 while `Injury::get_player_injury_status` 0x62c400 returns 1 (injured, back in
training but not fully fit; 0x62c5fd..0x62c60a). As a percentage: raw × 0.01 + 0.5, truncated, at most 100
(0x62c631..0x62c64c; constants 0x96bac8 = 0.01, 0x968968 = 0.5). This is GK's "Playing Condition (%)". Joe Cole shows
99 %, Gerrard 89 %.

The player screen's "Condition" line (`staff_screens_build_profile_screen`, 0x875e80..0x87601b), with fatigue F and
A = the player's appearances this season in the stats category 6 "Senior Club", field 1 (`PlayerStats::get_stat`
0x7c1e30 called at 0x875f40; field 1 is record byte +0 at 0x7c21ad, the appearances, printed "%d (%d)" with the
substitute appearances by `PlayerStats::format_record_field` 0x7c280f):

1. injury status 2 (injured): "-" (0x875ebd).
2. condition above 95 % and F >= -750: "%d%%" (0x875efc..0x875f28).
3. F < -750 and A < 10: "%d%% - Lacking match practice" (0x875f5a..0x875f78).
4. F > 1,100 and A > 30: "%d%% - In need of a rest" (0x875f9a..0x875fb8). GK has no button for this one.
5. F > 1,225: "%d%% - Exhausted" (0x875fdd..0x875fef).
6. Otherwise "%d%%" (0x876009).

Joe Cole (F -724) is shown as plain 99 %. Gerrard (F 1,160) is "In need of a rest" if he has more than 30 appearances,
else plain 89 % (his appearances were not read). On the save, 54,302 of the 114,663 players hold F < -750 and 3,332
hold F > 1,225.

What the game recalculates: each day `update_player_fitness` moves fitness and match condition towards full
(the match condition gain is smaller while F > 1,000, 0x62cd30) and stamps +0x00; `recover_fatigue` lowers F by up to
13 a day (0x62c374, 0x62c391), sets a negative F to 0 for players under 19 (staff +0x18, 0x62c332..0x62c347), lowers F
by 1 a day while it is below -1,000 (0x62c317), and caps a goalkeeper's (player +0x0f = 20) F at 1,050 (0x62c3b5);
a match sets match condition and adds to F (`set_match_condition` 0x62bd10). So an edit lasts until the next day or
match, and moves from there.

Safe edits (write only these bytes; +0x00 and +0x0e unchanged):

- 100 % and Normal: fitness 10,000, match condition 10,000, fatigue 0 (any value from -750 to 1,100 gives Normal
  whatever the appearances). With a current injury (+0x0e not -1) the game still shows "-" or caps at 90 %.
- A percentage P: fitness 10,000 and match condition P × 100 (raw at least 100, so P at least 1).
- "Exhausted": fatigue 1,300 (above 1,225; the most on the save is 1,456). With more than 30 appearances the game
  shows "In need of a rest" instead, because that test comes first.
- "Lacking match practice": fatigue -1,000. Shown only while A < 10, which the editor cannot change.

## Retiral (retire.dat)

`PLAYER_REGEN::save_retire_dat` 0x7ad7c0 writes four tables, each an int count then the records: the pending
retirements (count this+8 at 0x7ad889, 26-byte records at 0x7ad917), then tables of 12 bytes (0x7ada21), 16 bytes
(0x7adb2b) and 16 bytes (0x7adc35). On the save: 10,077 pending retirements, then 576, 2 and 1,143 records; the
parts add up to the member's 287,250 bytes exactly. The 576 match the 576 staff in retirement state 2; the meaning
of the other three tables (the function map names them the regen queue and the reuse pools) was not read.

The pending retirement record (`retire_record_assign` 0x7a8780 gives the fields):

| Field | Offset | Size | Values | Code |
|---|---|---|---|---|
| Staff | +0x00 | 4 | staff id | `PLAYER_REGEN::find_retirement_entry` 0x7a7ee3 |
| Club | +0x04 | 4 | club id, -1 none; set to -1 at 0x7a5bcd when he has left the club | 0x7a5b4d |
| Announce date | +0x08 | 8 | the day the retirement is announced | 0x7a64ff..0x7a6521 |
| Retire date | +0x10 | 8 | the day he retires: GK's "Estimated Date Of Retiral" (inference) | 0x7a5a40..0x7a5a64 |
| Reason | +0x18 | 1 | 1 age (player, 0x7a7521..0x7a755a), 0x20 non-player job (0x7a740d), 0x10 long unemployed (`player_regen_check_unemployed_retirement` 0x7a813c), 8 injury (`schedule_injury_retirement` 0x7a801c, both dates the same); 2 also seen (0x7a7c91), meaning not confirmed | 0x7a5a8f |
| Announced | +0x19 | 1 | 0 not yet, 1 announced | written 0x7a6563 |

The leap flags of the two dates are not clean in this table (values such as 0x0c830c83 next to 0); the game compares
only day and year (0x7a5a57, 0x7a6510, comparator 0x7ae2c3), so a reader should take the leap year from the year, not
from the flag.

Reason counts on the save: 8,524 reason 1, 1,551 reason 0x20, one each of 2 and 8. Four records are announced
(+0x19 = 1, state 1), for example Patrick Suffo (staff 4207, club -1, announced 10.6.2011, retires 10.3.2012) and
Rafael Medina (4795, club 1952, announced 15.3.2011, retires 10.3.2012); the other 10,073 are not announced, state 0,
for example Xu Song (14684, club 4193, announce 10.3.2012, retire 12.9.2013; dates read with the leap year taken from the year). Joe Cole and Gerrard have no record.

What the game does with it, every day (`game_main_day_loop` calls `PLAYER_REGEN::process_retirements` 0x7a59e0 at
0x5cda73): it sorts the table by `player_regen_compare_retirements` 0x7ae2a0 (the announce date while that is still
ahead, else the retire date, then the staff id; qsort at 0x7a5a1c), then walks it:

- On the announce date (if not announced): posts the announcement news (`player_regen_post_retirement_announcement_news`
  0x7aa340, from 0x7a655c, unless the state is already 1), sets +0x19 = 1 and the state to 1 (0x7a6563..0x7a6585).
- On the retire date: drops the record and clears the state (0x7a5ab5, 0x7a5ae8, 0x7a5b24, 0x7a5bb0) in these cases:
  a player under 30 with no injury reason who has a club or any staff state bits 0..3; a player on loan
  (`get_loan_contract` 0x7a5ac6); a player in a transfer (0x7a5b02); a player at a club other than the record's who
  joined it less than 210 days ago, is under 38, is not listed (0x544520) and has contract +0x45 = 0
  (0x7a5b51..0x7a5b96). Otherwise he retires: news (`player_regen_post_retirement_news`
  0x7a92e0, from 0x7a5bdb), contract ended, and he is rebuilt or queued for a regen.
- On a regen day (`is_player_regen_day` 0x5d6250, call at 0x5cdaa9), `PLAYER_REGEN::find_new_retirements` 0x7a5790
  adds new records for staff of 30 or more (0x7a5806) with no state bits (0x7a5847), no record already (0x7a5862) and
  contract +0x49 clear (0x7a587e), by `player_regen_check_retirement` 0x7a7310 or the unemployed test.

The profile (`staff_screens_draw_transfer_tab`, 0x87f024..0x87f066) shows from the state alone: 2 "Retired from
professional football", 1 "Retiring at the end of the season" (or "Retiring from the playing staff ..." for a
player-coach, "Believes he has no future in professional football" under 30). GK's "This person is not retiring at the
end of season" is the state 0 case; GK's own wording.

Safe edits:

- Cancel a retirement: remove the staff member's record from the first table (shift the later records down, count
  minus one, the member 26 bytes shorter) and clear general.dat +4007 + id × 79 + 0x0b bits 4..5 (`& 0xcf`), as the game
  does at 0x7a5ae8 with `PLAYER_REGEN::remove_retirement` 0x7a8690. Do not touch a state 2 (retired) person. The
  announcement news, if posted, stays. On the next regen day `find_new_retirements` can add him again if he still
  passes `player_regen_check_retirement`.
- Move the date: write +0x10 (and, for a record not yet announced, +0x08 at or before +0x10). Keep the table sorted by
  the comparator above: the game sorts it again each day before it walks it, but `remove_retirement` uses a binary
  search (0x7a8698).
- Make someone retire (GK's "Update Retiral Date" for a person with no record): append a record (id, his club id or -1,
  announce date, retire date, reason 1 for a player or 0x20 for a non-player, announced 0) with the state left 0; the
  game announces it on the announce date. Not run.
- GK's "List all retiring staff": the records with +0x19 = 1, or state 1.

## Contract Details

| GK field | Member | Offset | Size | Values | Code | Joe Cole | Gerrard |
|---|---|---|---|---|---|---|---|
| Leaving On Bosman | contract | +0x49 | 1 | 1: he has decided to leave at the end of his contract | set by `CONTRACT_MANAGER::daily_update` 0x4c8a4d (with the news `post_bosman_departure_news` 0x4d6d70, text 0x9ab258 "has decided to leave ... at the end of his contract under the Bosman ruling") and 0x4c9b15; while set, no transfer request (`STAFF_CONTRACT::set_flags` 0x86762a, `apply_event_flags` 0x867784) and no new retirement (0x7a587e) | 0 | 0 |
| Release clause "Active?" | contract | +0x1c, +0x1e, +0x1f | 1 each | 0 none, 1 in the contract, 2 triggered | 2 written by `CONTRACT_MANAGER::trigger_non_promotion_clauses` 0x4ccfdd (+0x1c) and `trigger_relegation_clauses` 0x4cd0d7 (+0x1f, from 1); `STAFF_CONTRACT::has_active_release_clause` 0x867b50: +0x1c = 2, or +0x1d = 1, or +0x1f = 2 | 0 | 0 |
| Played In a Row | contract | +0x3e | 1 | 0..100 | `CONTRACT_MANAGER::update_after_match` 0x4cafdb..0x4cafdd (+1, capped 100, +0x3f set to 0) and 0x4cac2d | 0 | 0 |
| Missed In a Row | contract | +0x3f | 1 | 0..100 | 0x4cab99..0x4caba2 (+1, capped 100, +0x3e set to 0; at 5 or more the low morale path, 0x4caba5) | 1 | 8 |

On the save: +0x1c holds 1 on 3,429 contracts and 2 on 354, +0x1f 1 on 321 and 2 on 260, +0x1e 1 on 4 and never 2;
+0x49 is 1 on 14,253 contracts. No code was found that writes 2 to +0x1e (Non Playing) or +0x20.

Which of +0x3e and +0x3f is "played" is from the match code: the path that increments +0x3e is the one that first
calls the rating code (0x4cafc8); not checked on a screen.

## Unhappiness (contract +0x45)

Contract +0x45 is a 4-byte bit field of grievances: `STAFF_CONTRACT::apply_event_flags` 0x867700 ORs bits in
(0x86774b), `clear_grievances` 0x8674e0 and `clear_all_grievances` 0x867520 clear them, `reset_morale_state`
0x866ee0 sets it to 0; `contract_get_status_flags` 0x5d5a50 returns it. While it is not 0 the morale is held down
(`staff_adjust_morale` 0x5447f0). The happiness lines of the profile (0x87f400..0x884600) read each bit; the line the
game shows for each, and the GK checkbox it fits (inference from the text):

| Bit | The game's line (address of the test) | GK checkbox | Set by | On the save |
|---|---|---|---|---|
| 0x1 | "Wants to move to a bigger club as soon as possible" (0x88280a) | Wants To Move To A Bigger Club | | 2,410 |
| 0x2 | "Would like to negotiate a new contract", "Wants a new contract" (0x88292a, 0x882a40) | Would Like To Negotiate A New Contract | | 26,621 |
| 0x4 | "Is angry that his manager doesn't consider him to be a first team regular" (0x88325e) | Feels He Should Be A First Team Regular | | 3,930 |
| 0x8 | "Unhappy with his manager" (0x883628) | Unhappy With His Manager | | 113 |
| 0x10 | "Wants a long run in the first team", "Wants first team football" (0x8836a9, 0x883b7c) | Wants First Team Football | `contract_manager_lower_match_morale` 0x4cb721 | 27,615 |
| 0x20 | "Finding it hard to adjust to the ... lifestyle" (new arrival), else "Feels it is time to move on" (0x883c09) | Finding It Hard To Adjust To The Lifestyle; Feels That He Needs To Leave To Further His Career | `game_daily_staff_update` 0x5d323f | 258 |
| 0x40 | "Has personal problems", "Is being treated for depression" (0x883d70) | Has Personal Problems | 0x5d25b3 | 226 |
| 0x80 | "Is frustrated at not being allowed to move clubs" (0x8842e1) | Is Frustrated At Not Being Allowed To Move Clubs | `TRANSFER_MANAGER::unsettle_player_after_bid` 0x8c1594 | 9 |
| 0x100 | "Wants a new challenge" (0x88357c) | Wants A New Challenge | 0x5d2389 | 82 |
| 0x200 | "Feels he has been victimised by his manager" (0x883008) | Feels He Has Been Treated Unfairly | `Fine::fine_player` 0x5a5f6a | 230 |
| 0x400 | "Unhappy at treatment of teammate" (0x88310b) | Unhappy At The Treatment Of His Teammate | `Fine::fine_player` 0x5a6763 | 11 |
| 0x800 | "Thinks <disliked> is a disruptive influence on the team" (0x88150d) | Dislikes Teammate | `game_update_squad_relationships` 0x5d7110 | 523 |
| 0x1000 | "Dislikes the club's Assistant Manager" (0x881854) | Dislikes the Club's Assistant Manager | | 0 |
| 0x2000 | "Dislikes the club's Manager" (0x881f09), "Thinks the manager is tactically inept" (0x882c33) | Dislikes The Clubs Manager; Has Lost Confidence In His Manager's Ability | `Media::apply_player_story_response` 0x7413be | 0 |
| 0x4000 | "Expects a more important role in the team" (0x881451) | Feels He Deserves A More Important Role In The Team | `set_player_squad_status` 0x8ce6d1 | 0 |
| 0x8000 | "Unhappy with training" (0x884231) | Unhappy With Training | `TRAINING_MANAGER::update_player_training` 0x8b20f3 | 0 |
| 0x10000 | "Wants to leave the club immediately", "Wants to be transfer listed" (0x881331) | not matched | `handle_transfer_request` 0x4d87b3, the transfer list dialog 0x8f85b2 | 76 |
| 0x20000 | "Is homesick", "Wants to end career in ..." (0x88128e) | Is Homesick | 0x5d3190 | 290 |
| 0x40000 | "Has vented his feelings publicly" (0x88337d) | Has Vented His Feelings Publicly | `match_day_process_player_incidents` 0x6b7776 | 29 |
| 0x80000 | "Feels he has no support from his manager" (0x8813d6) | Feels That He Has No Support From His Manager | media story 0x73767f | 27 |
| 0x100000 | "Is having trouble handling the pressure ...", "Feels that he needs to be rested", "Believes that he is past his peak" (0x8840cd) | Worried About His Future At The Club/Jaded | media story 0x73749d | 18 |
| 0x200000 | "Is unhappy about his team-mate's remarks" (0x883e0e) | Is Unhappy About His Team-mate's Remarks | | 0 |
| 0x400000 | "Unsettled by the financial upheaval" (0x880902) | Upset By The Financial Upheaval | `FinanceTable::daily_update` 0x59c3b5 | 353 |
| 0x800000 | "Feels the manager is obstructing any chance of his leaving the club" (0x8831b1) | Feels The Manager Is Obstructing Any Chance Of His Leaving The Club | `review_ai_club_bid` 0x8c0e53 | 0 |
| 0x1000000 | "Feels he deserves to be paid more" (0x8811a4) | Feels He Deserves To Be Paid More | 0x5cfc3d | 0 |
| 0x2000000 | "Wants to play as a <position>" (0x882aff) | Wants To Play In Another Position | `match_day_process_player_incidents` 0x6b7618 | 0 |
| 0x4000000 | "Unhappy at being substituted" (0x882f24) | Unhappy At Being Subbed | 0x6b767d | 1 |
| 0x8000000 | "Feels he can no longer trust his manager" (0x882d8e) | Feels He Can No Longer Trust His Manager | `Media::process_transfer_squad_reactions` 0x758303 | 0 |
| 0x10000000 | "Not convinced the club's new manager can turn the club around" (0x8803ce) | not matched | `game_main_day_loop` 0x5cd8f5 | 589 |
| 0x20000000 | "Is unhappy at the sale of <star player>" (0x87f90e) | Unhappy At The Sale Of Teammate | `process_transfer_squad_reactions` 0x758b01 | 56 |
| 0x40000000 | "Is disappointed that the club sold promising youngster ..." (0x87f813) | Disappointed That The Club Sold One Of Its Most Promising Youngsters | | 1 |
| 0x80000000 | no line found | not matched | `process_transfer_squad_reactions` 0x758381 | 2 |

Joe Cole and Gerrard: 0. 59,279 contracts on the save hold a bit.

Safe edit: write the dword. Clearing bits is what the game's `clear_grievances` does, but the game also then runs
`check_transfer_request_withdrawal` 0x867240 and a staff update 0x549d60, which an edit of the save does not; a
transfer request already made stays. Setting a bit makes no transfer request (that is made by `apply_event_flags`
when the game sets it) and holds his morale down.

## Club Unhappiness (contract +0x40)

Contract +0x40 is a second 4-byte bit field: `STAFF_CONTRACT::set_flags` 0x867560 ORs bits in (0x8675e1; it asserts
the value is at most 0x1000 as a signed number), `clear_flags` 0x8676d0, `get_flags` 0x8676f0.

| Bit | The game's line (address of the test) | GK checkbox | On the save |
|---|---|---|---|
| 0x1 | "Irritated by <disliked staff>" (0x8818de) | not in GK's 13 | 24 |
| 0x2 | "Worried by the lack of managerial discipline" (0x883f92) | Worried At The Lack Of Managerial Discipline | 0 |
| 0x4 | "Dismayed by the lack of harmony at the club" (0x87fe93) | Dismayed At The Lack Of Harmony At The Club | 33 |
| 0x8 | "Thinks the clubs valuation of him is unrealistic" (0x8807d8) | Thinks That The Clubs Valuation Of Him Is Unrealistic | 0 |
| 0x10 | "Thinks the club should be topping the table" (0x880146) | Thinks the Club Should Be Topping The Table | 1,283 |
| 0x20 | "Feels that referees in <nation> are victimising him" (0x881bdf) | Feels That The Referees Are Victimising Him | 0 |
| 0x40 | "Feels his best position is playing as a <position>" (0x882cc8) | Feels That His Best Position Is (Current Pos) | 0 |
| 0x80 | "Feels the manager doesn't listen to his players" (0x882ead) | Feels That The Manager Doesn't Listen To His Players | 0 |
| 0x100 | "Thinks the team would perform better if players were in their strongest positions" (0x882e12) | Thinks The Team Would Perform Better If Players Were In Their Strongest Positions | 0 |
| 0x200 | "Feels there is too much competition for a first team place" (0x87fc28) | Feels There Is Too Much Competition For A First Team Place | 35 |
| 0x400 | "Believes the club needs more squad depth" (0x87ffe7) | Believes The Club Needs More Squad Depth | 42 |
| 0x800 | "Worried by rumours that the club is signing a replacement for him" (0x87fdf0) | Worried By Rumours That The Club Is Signing A Replacement For Him | 4 |
| 0x1000 | "Hopes he is joining a club which respects his style of play" and others (0x8809ee) | Thinks The Manager Should Accept His Style Of Play | 0 |
| 0x80000000 | "Is unhappy that the manager has signed another <position>" (0x87fcb0) | Unhappy that the club signed another (Current Pos) | 0 |

Joe Cole and Gerrard: 0. Safe edit: write the dword; the game's setters only OR bits in, no side effect was found
beyond the transfer request test in `set_flags` (0x867610..0x8676bf), which an edit does not run.

## Loan Contract

A loan is a second 80-byte record in contract.dat, found through the staff state +0x04 (above), not by a search by
staff id. On the save 1,848 staff have one; every loan record has type 6 (+0x35 low bits) and the main contract type 2.

| GK field | Offset (loan record) | Size | Values | Code | David Edwards (staff 49) |
|---|---|---|---|---|---|
| (loan club) | +0x04 | 4 | club id; the main record's +0x04 is the parent club | 0x87d624 reads the parent from the main contract | 7930 (parent 9344) |
| Weekly Wage Contribution | +0x0c | 4, signed | money; on the save half the main wage in most loans | as the wage, `CONTRACT_MANAGER::get_wage` 0x4d0c59 (inference: the loan club's share) | 66 (main 133) |
| Started, Expires | +0x25, +0x2d | 8 each | dates | the profile's "On loan from ... until ..." line (string 0xa865dc, 0x87d501) | 14.11.2011, 14.5.2012 |
| Can Play In Cup Matches | +0x1c | 1 | 0 no, 1 yes | the profile's "Cannot play in cup matches" when 0 (0x87d59d) | 1 |
| Can Play In Matches Against Current Club | +0x1d | 1 | 0 no, 1 yes | "Cannot play in matches against <parent club>" when 0 (0x87d604) | 1 |
| (dropdown) squad status | +0x4f bits 4..7 | | as in a main contract | `get_squad_status_text` 0x867f6f | 7 |

So in a loan record the bytes +0x1c and +0x1d are not release clauses. All 1,848 loans hold +0x1c = 1 and +0x1d = 1;
+0x36 equals +0x0c in every loan shown (the main contracts hold other values there, meaning not confirmed).

Safe edit: change +0x0c, +0x1c, +0x1d, +0x4f (high bits) and the dates of an existing loan record. The end date drives
the loan's end (`CONTRACT_MANAGER::end_loan`, the earlier doc's 0x4d0b33); moving it before today was not run. Making or
removing a loan (a new record, the state +0x04, the clubs' squads and finances) is not attempted.

## Not confirmed

- The players' "Senior Club" appearances (A) were not read from the save, so Gerrard's shown line is not known.
- Retire reason 2, and the meaning of the second, third and fourth tables of retire.dat.
- That GK's "Estimated Date Of Retiral" is the record's +0x10, and what GK shows for a person with no record.
- That the game loads a save whose retire.dat has grown or shrunk (not run).
- Which of +0x3e / +0x3f is played and which missed (from the match code only).
- A trigger for the Non Playing clause (+0x1e = 2) and the meaning of +0x20 = 2.
- That the loan record's +0x0c is the loan club's share of the wage; the meaning of +0x36, +0x3b, +0x4a (Joe Cole and
  Gerrard: +0x4a -1).
- Unhappiness bits with no GK match (0x10000, 0x10000000, 0x80000000) and the GK labels matched to 0x20 and 0x2000,
  which each show two lines; +0x45 bit 0x80000000 and +0x40 bit 0x1 have no line in GK's lists as given.
- GK's own mapping of its controls to these bytes: the game's side is read, GK's is inference.

## Scripts

Read with `web/savedit.js` (`SaveGame`, `file.read(member)`), scripts kept outside the repo. The code was read with a
capstone disassembly of the clean 3.9.68 `cm0102.exe`.
