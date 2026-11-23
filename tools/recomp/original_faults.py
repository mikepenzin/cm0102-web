"""Faults of the original that long games reach (0.6.2 slice 11, Q23, Q24, Q27, open item 102;
src/recomp_rt/win/original_faults.c). The fixes act in every game, the base game too. The test switch
RECOMP_ORIGINAL_FAULTS=keep turns them all off. "keep:managers", "keep:money", "keep:age", "keep:olympics",
"keep:ire_super", "keep:nsl", "keep:holiday", "keep:callups", "keep:wc_pots", "keep:confed", "keep:club_regulars",
"keep:match_numbers", "keep:squad_moves", "keep:squad_loans", "keep:club_keys" or "keep:callup_counts" (or a list:
"keep:money,age") turns off one group or more.

National managers (fault 26, Q23). `club_can_be_managed` 0x544820 takes a senior national team (club id from club
count - 2 x nation count to club count - nation count, 0x544835..0x544857) only when its nation had more than 150
players at game creation (nation +0x88, 0x544859). `MANAGER_MANAGER::add_job_vacancy` 0x69ba50 returns at once for a
senior national team of 100 or fewer (0x69bafb). The count is made once, at game creation (0x5262fa..0x52630e). So
when the manager of a smaller nation leaves, nobody replaces him, and in a 2001 game every such team has lost its
manager by about 2019. The places where a national team loses or misses its manager take every senior national
team here:
- 0x7ab158: `player_regen_retire_from_non_playing_staff` 0x7aaeb0 removes a retiring national manager (0x7ab148) and
  opens a vacancy only for a manageable team (0x7ab14e).
- 0x68fe65: `MANAGER_MANAGER::daily_update` 0x68f9c0 appoints the manager of one national team to another and opens a
  vacancy for his old team only when it is manageable (0x68fe11..0x68fe5b).
- 0x69b6d8: `MANAGER_MANAGER::process_pending_departures` 0x69b650 removes a manager whose departure is due, for
  example at the end of his contract (0x69b6c8), and opens a vacancy only for a manageable club (0x69b6ce).
- 0x69d0a5: `MANAGER_MANAGER::fix_missing_jobs` 0x69d060, at each change of month, opens a vacancy for a club with no
  manager and no vacancy only when it is manageable (0x69d09b).
- 0x69bb02: the bound of 100 players in `add_job_vacancy`. Five calls open a vacancy with no test of 0x544820, so this
  bound alone stopped them for a small nation: 0x42b43b, 0x69ec9b, and the new search of `daily_update` when the
  chosen candidate is no longer free (0x68fbf4, 0x68fc51, 0x68fd6a). They now open it too.
The other 17 calls of 0x544820 (board confidence, news, the human job lists) keep the original's rule, so a small
nation's manager is not judged by a board, as before. Under-21 teams stay unmanaged, as in the original (0x544857).
A nation that no longer exists gets no vacancy (Q43): its record has no continent (+0x71), as East and West Germany,
the Soviet Union and Zaire in the 3.9.68 data and the old record of a renamed nation in a season game. While `game_run_to_start_date` 0x5cc850 makes a new game the fix is off,
so a new game starts as the original's (about 95 of 213 national teams of the 3.9.68 data have no manager) and
changes at its first month change in play.

The human manager's age (fault 33, Q27). `database_get_staff_age` 0x53ab90 works out the age from the birth date
(+0x10, +0x12) and a date as a 16-bit value in ax, but its callers keep al: the day loops store `movsx` of al at staff
+0x18 (0x5cca1d, 0x5cd3c4), about 1,400 readers take that byte, and other callers compare al at once (for example
0x4c85c5, 0x4d4dc2, 0x532f0b, 0x8d09a7). The human manager slots are the only staff that never retire, so their age
passes 127 (a manager born 1966, the default of `setup_add_human_manager` 0x820d40, in 2094) and the byte wraps to
-128: every age test then answers as for a child. Here 0x53ab90 itself gives 127 for any age above 127 (the wrap in
original_faults.c), so each test against a bound up to 127 gives the answer of the real age (for example the bound 35
of `MANAGER_MANAGER::is_staff_suitable_for_job` 0x69f931 and `manager_stats_screen_build` 0x6b0070). The byte cannot
hold the real age. The human's age is shown on no screen: the profile tabs that show an age ("Born ... (Age n)",
0x874cef, text 0xa85010) are greyed for a human manager. Below 128 every value is the original's.

Money (faults 27 and 28, Q24). The club balance is signed 64-bit (finance +0x00), but some readers keep 32 bits. The
trace is in docs/research/2026-10-30-money-64bit.md. Each patch gives the original's result whenever the original's
32-bit value holds the right number, so it acts only above 2,147,483,647, but for the top wage:
- `Finance::get_transfer_budget` 0x598800: this year's net spend (0x598f4f), pending fees plus that spend (0x598f8b,
  0x598f92) and the balance less 10m or 15m (0x599135, 0x599185, used at 0x5991d0, 0x5991d6 and 0x599201) keep
  64 bits. Its 23 callers that keep only eax get the budget clamped to 2,147,483,647, so a very rich AI club keeps a
  budget and bids.
- The money text: `Cash::get_display_amount` 0x43f460 reads a 32-bit Cash (`fild dword`). The screens that put a
  64-bit amount into a Cash give the full amount to the runtime (rt_cash_wide, for the reads of that site), and
  0x43f460 reads it from there: the club info balance (0x46399c, read once or twice: a long text is formatted again,
  ended at 0x4639ee), debt (0x463a14, ended at 0x463a6e) and budget (0x463b28, ended at 0x463b7a), the budget of the
  player search (0x808ef3) and of the bid screen (0x8ea2c0: the Cash keeps the clamped budget, because the fee
  colour reads it again at 0x8ea54c, 0x8eaf95 and 0x8eb3b0), and the manager's money spent and received on the
  manager stats screen (0x6b0366, 0x6b0399) and on the staff history tab (0x887fa7, 0x88808f).
- News: a news item keeps 50 32-bit slots (`NewsItem::set_slot` 0x785790 at item + 5 + 4 x slot), saved raw in
  news.dat. Two items carry a transfer budget in slot 2: the job offer to the human (`post_job_offer_to_human_news`
  0x6a4f80, store 0x6a5028) and the finance news (`finance_post_finance_boost_news` 0x59f300, store 0x59f36e). The
  slot keeps the clamped budget, and a budget above 2,147,483,647 also goes into the free slots 8 (low word) and 9
  (high word) of the item (0x6a5021, 0x59f36b). They stay 0 below it, as the item's constructor clears them
  (0x77bc1b..0x77bc29), so news.dat is the same. The text writers (0x6a5120, 0x59f3e0) read slot 2 (0x6a51dd,
  0x59f5ab) and show the full amount from slots 8 and 9 (0x6a533d, 0x59f5c7).
- `Finance::get_max_wage` 0x5968d0 adds 1.3 x the balance (ftol at 0x5968e3, only eax kept at 0x5968ee) to a wage term
  in 32 bits (0x597019) and divides by 52: from a balance of about 1.65 billion the sum wraps and the club's top wage
  goes wrong. The high word of that product is kept at 0x5968e8, and a sum above 2,147,483,647 is clamped to it. This
  fix acts from about 1.65 billion, where the original's sum has already wrapped. A debt keeps the original's result.

Olympic squads (open item 102). `NationalTeams::select_squad` 0x76c120 lets the Olympic pick (slot 2) take a player of
his nation's senior squad when `national_teams_can_change_squad` 0x76db00 allows it (0x76ce15, 0x76ce2d). 0x76db00
refuses while the senior squad's window runs, but only for a squad that the human's screens marked selected (+0x44,
0x76de03), never for an AI squad. So an Olympic pick made between an AI senior squad's pick and its match took the
squad's young players, and in a Tier A game Holland played a World Cup qualifier with 10 (box match_eng..cpp 612).
At 0x76ce1c, the jump that skips a candidate, the Olympic pick also skips a player of a senior squad whose window ends
today or later (rt_olympic_keeps_senior; the Olympics: fixture +0 is the key at 0x9cf7a0, as at 0x76ce2d). Other
picks of slot 1 or 2 skip such a player anyway (0x76ce38).

The Irish Super Cup (open item 127). `IreSuperCup::create_table` 0x63b0c0 fills its four places with the Irish clubs
of the Champions League (club +0x1db the key at 0x9cf6e8, 0x63b10a..0x63b17b), the UEFA Cup (0x9cf6f0, 0x63b1c7..
0x63b21a) and the Intertoto Cup (0x9cf6f8, 0x63b228..0x63b27b), then with Premier Division clubs (club +0x57 the
key at 0x9cf80c, 0x63b295..0x63b2df). The last loop takes the first Premier Division clubs in club order and does not
ask whether a club is already in the table. When Ireland has fewer than four European places, a club of the first
loops is often taken again: the run set game a2001offs3 drew "Athlone Town v Athlone Town" in 2057 and a2001offs1
"Bohemian F.C. v Bohemian F.C." in 2041. That match is never played, so the edition never completes, the season pass
never calls `IreSuperCup::start_new_season` 0x63b550, and `update_for_date` 0x63b300 never draws again (it draws only
while +0x45 is 0 and +0x40 is the year, 0x63b311..0x63b331): the cup is gone for the rest of the game. At 0x63b2ae
(the jump that skips a club of another division) the last loop also skips a club that is already in the table
(rt_ire_super_has).

Australian NSL stages (open item 128). The NSL plays three stages: the regular season, the finals series of the top
six (stage 0, `AusNsl::create_finals_series_stage` 0x412010) and the Grand Final of the top two (stage 1,
`AusNsl::create_grand_final_stage` 0x4121c0). The index of the current stage is at +0x30 and the stage count (2) at
+0x2c; the ctor 0x410e90 sets them to -1 and 2 (0x410efe, 0x410f01). `AusNsl::advance_stage` 0x411f70 steps the index and builds
stage 0 or 1, but only while the index is below count - 1 (else the box "aus_nsl.cpp 752", 0x2f0).
`AusNsl::start_new_season` 0x411de0 does not set the index back to -1 and does not free the stages, as the other
classes with stages do at the same point (`ArgPrm::start_new_season` 0x405720, `BraNatFirst::start_new_season`
0x42a640: each stage 0..index freed by its slot 0 with 1, then the index -1; the AusNsl destructor 0x4110e0 frees
the same stages). So from the second season the index
stays at 1: the game builds no finals series and no Grand Final, and `AusNsl::record_season_honours` 0x411d90 writes
the first season's Grand Final pair again from the old stage 1. At 0x411e1c, after the stats reset and before the
step of the year (0x411e1e), each stage 0..index is freed as there, its slot set to 0, and the index set to -1
(rt_aus_nsl_new_season). A save made in season 2 or later keeps the index of 1 until its next start_new_season.

Holiday offers (open item 132). "Go on holiday ?" keeps its settings in the human record (+0x9b23,
`HUMAN_MANAGER::get_holiday_option` 0x5fea50). Option 0 is 1 for "Reject all offers for players" and 2 for "Reject
all offers below asking price" (the build 0x6b2d80 draws the row bright for that value, the input 0x6b35c1 sets it).
A player's asking price is staff +0x52 (`TRANSFER_MANAGER::set_player_squad_status` 0x8ce1e9). Two faults:
- `TRANSFER_MANAGER::ai_respond_to_bid` 0x8c8090 answers a bid for a club on holiday. With option 1 it rejects
  (0x8c8266). With option 2 it rejects only a bid with part-exchange players (0x8c8233) and otherwise gives the
  computer's answer (0x8c8277), which accepts bids down to about 70% of the asking price. So "below asking price" did
  the same as no setting. At 0x8c8264 a bid whose fee (offer +0x1b) is below the asking price now takes the reject
  of option 1 (rt_holiday_below_asking). A player with no asking price (0) keeps the computer's answer.
- `shortlist_manager_predict_selling_club_response` 0x84af00 holds the tests of both options (0x84b05e..0x84b0a8: 1
  answers 2, reject; 2 takes the asking price), but only in its branch for a human club, which it finds with
  0x600a50 and ignore_holiday 0 (0x84b025). That call counts a club on holiday as a computer club, so the tests never
  run. The predictor decides loan requests for a club on holiday (`NewsItem::auto_respond` 0x784100 for news 4000,
  `TRANSFER_MANAGER::process_due_offer` 0x8c63a0 state 0) and the bids that computer clubs plan
  (0x83f3a0, 0x842e10). At 0x84b02f the computer branch now makes the same tests for a club whose manager is on
  holiday (rt_holiday_predict, with the human test of 0x8c81ea: ignore_holiday 1), and at 0x84af77 a loan request
  for such a club with option 1 gets the answer 2 (rt_holiday_refuses_loan). A release clause in force keeps the
  original's answer, as in the dead test (0x84b05e), and the bid reply accepts a bid that meets it before any setting
  (`transfer_manager_selling_club_must_accept` 0x8cd240 at 0x8c81bb), as a real club must. With no human on holiday
  (the count at [0xb63c98] + 0xc) and with the settings off, every answer is the original's.
Three callers (0x8c0527, 0x8c2e3d, 0x8c42fa) call 0x8c8090 with dry_run 1, and the reject at 0x8c8266 acts in a dry
run too. The original does this for "Reject all offers", and the fix does it for a bid below the asking price. In
`process_due_offer` 0x8c63a0, an offer in state 0x11 or 0x12 goes to 0x8c2f50 when offer +0x10 is set (0x8c6983), and
news 0xfa5 confirms an offer through 0x8c3310 (0x784cbc). Both come after a bid was accepted, so with "Reject all
offers" an offer for the club can reach them only when a release clause or the receivers made the club accept it.
Every player of a game has an asking price: none of 6,860 at the start, 6,842 a year later and 10,679 in a game of
2026 has 0 (tests/seasons/holiday_offers_run.py, RECOMP_TEST_LIST=census), so listed and unlisted players meet the
same rule.

Background match numbers (open item 155). The group match_numbers has no site here: it is the RT_WRAP code of
src/recomp_rt/win/match_numbers.c. The background matches of a match day play in steps of real time: between clicks
(the day loop's hook 0x5d61b0 calls MATCH_DAY::step_live_match 0x6b55c0 with no human match) and while a watched
match waits for a commentary line's delay on the millisecond clock. Each step (0x6b5ce0, 0x6b5a70) takes numbers
from the one random pool of 0x90cfc0, as do the human match and the screens. So the speed of the machine changes the
game. With the fix, the background matches of each match day take numbers from a second pointer and offset into the
pool, which starts when MATCH_DAY::MATCH_DAY 0x6b3c10 builds the day, and the main stream is the same on every
machine.

Senior call-ups (open item 143). `NationalTeams::select_squad` 0x76c120 picks a squad from the nation's pool. For a
candidate at a club whose nation is on another continent than his own (club +0x53, nation +0x71; 0x76cc3d..0x76cc59
and 0x76cc9f..0x76ccbd) it applies a release rule:
- a friendly (fixture +0 the key at 0x9cf668): refused when his club's reputation (+0x80) x 0.8 (0x9689b0) is over
  the opponent's (0x76ccdd..0x76cd3e)
- a 'B' or Under 21 International (0x9cf794, 0x9cf798): always refused (0x76ccc3)
- another record (qualifiers, finals): refused when his division's reputation (club +0x57, +0x69) x 0.7 (0x968e40)
  is over the competition's (0x76cc61..0x76cc9a)
A nation with its players abroad and weak opponents then picks almost nobody. In a game with every league (14.7.01,
the picks for 30.9.01) Mali refused 35 of 38 players and picked 1, Senegal 85 of 103 (7 picked), Guinea 45 of 49 (4),
Cape Verde Islands 27 of 31 (1). A pick sets the window end (record +0x3a) to its fixture's date. The next fixture
is checked 14 days before it, and it keeps the squad when that check comes at most 14 days after the window end
(0x54cdb0 at 0x76bb94, cmp 0xe at 0x76bb9e; the window moves on at 0x76bbe6..0x76bc09). So a fixture at most 28 days
after the last one keeps a short squad, and the top-up fills it with virtual players. Real clubs must release their
players to the senior national team for the matches on the dates of the international match calendar (FIFA
Regulations on the Status and Transfer of Players, annex 1). The game's rule has no such dates, so for the senior
slot (0, the byte argument at [esp + 0x8e4], 0x76cd9c) the refusals at 0x76cc9a, 0x76cd0b and 0x76cd3e now let the
candidate go on, as a player at a club of his own continent does (rt_callup_released). This holds for the human
manager's pick too (0x784630, forced 1). The pick keeps its other gates, among them a regular player of a club with a
competitive match on the date (NationalTeams::mark_clubs_with_fixtures 0x7775c0, the playing-time mood contract
+0x3d over -10, 0x76cd44..0x76cd73). 'B' and under-21 picks (and the Olympic squad in slot 2) keep the original's
rule.

The Confederations Cup in June (open items 157 to 159). The template of the class (0x58b560) plays the groups from 29
July (League_Stage date 0x68a160 at 0x58b601) and the final on 9 August (0x521eb0 at 0x58b984), and the year +0x40
steps by 2 for ever (0x58c050). Those are the days of the original's own Club World Championship and of the European
club qualifiers. NationalTeams::get_international_duty_club 0x7757d0 holds a player from the day before his side's
match to the day after, so clubs of those cups played without 3 to 15 of their players (run set pass 3, changing
rules off). The real editions started in mid-June (2003: 18 to 29 June). This group has no patch here: its fix is in
season_intl_date_shift (src/recomp_rt/win/season_intl_rules.c, confed_june_shift), which the stage builder 0x58bdb0
and the template already use for the date shift of every finals class. An edition that keeps the original's dates
moves its first group day to the Saturday on or before 18 June of its year, so all its matches are in June (2003: 14
to 25 June). It acts from the new game on: the base game builds its 2003 edition there. Tests: ctest season_intl and
tests/seasons/confed_dates_run.py.

A club keeps its regulars for a continental match (open item 160, group club_regulars: two patches, and
keep:club_regulars turns off both). NationalTeams::mark_clubs_with_fixtures
0x7775c0 marks each club with a match within a day of a national match (this +0x24, a byte per club): 1 a match of its
own league, 2 another competition of its nation, 3 a competition with no nation, a continental cup (0x77770f,
0x777734, 0x777779). The squad pick NationalTeams::select_squad 0x76c120 leaves out a regular of a marked club
(contract +0x3d, the playing-time mood, over -10: 0x76cd44..0x76cd73), and only a player who wants games goes. For a
continental match the original marks a club only when its nation has a league in the game (nation +0x11c, the
run-time league flags: 0x7776fc..0x777704 for the home club, 0x777721..0x777729 for the away club). A club of a nation
with no league (Bolivia, Tunisia) is never marked, so its regulars go on duty on the day of its continental match: in
run set pass 3 (changing rules off) Club Jorge Wilstermann played its Merconorte Cup match of 5 October 2011 without 3
players that Bolivia had picked for a friendly of that day. Real clubs release players only for the days of the
international match calendar (FIFA Regulations on the Status and Transfer of Players, annex 1, article 1), and the
continental confederations keep their cups off those days, so a club keeps its regulars for a continental match. The
two tests now mark every club with a nation of a continental match (rt_callup_mark_all); a club with no nation stays
unmarked (0x7776fa, 0x77771f). The marks change only the pick's gate: the
call-up move NationalTeams::reschedule_clashing_club_fixtures 0x7777e0 acts on marks 1 and 2 (0x7778b7..0x7778bd),
which need a league. A squad can also skip the gate: the squad check of a national fixture 0x76bab0 keeps a side's squad
for its next match when the check comes at most 14 days after the window end (0x54cdb0 at 0x76bb99 and 0x76be0c, cmp
0xe), so Bolivia's squad of 2 August 2011 played the friendly of 5 October with the 3 Wilstermann players. Now a
senior side picks again (select_squad 0x76c120) when one of its squad is a regular (mood over -10) of a club with mark
3, at the keep tests of the home side (0x76bba8) and the away side (0x76be1b) (rt_squad_kept), only when no match of
the squad's window is still to come (today on or after the window end, nation record +0x3a; 0x54cdb0 gives today
minus that day) and the fixture is the one of the day two weeks ahead, the day of the marks (daily_update 0x76b100
builds them for that day, 0x76b257..0x76b2bf; 0x76bab0 also runs for a new fixture, from 0x5aab15).
The B and under-21 squads have their own keep tests with the same 14 days (open item 299 of 0.6.2): the B squad
(window end record +0x4e, squad set +0x57) at 0x76bc76 (home) and 0x76bec7 (away), the under-21 squad (window end
+0x62, squad set +0x6b) at 0x76bd34 (home) and 0x76bf85 (away), each before its pick (select_squad with slot 1 at
0x76bdcf from 0x76bce9, slot 2 from 0x76bdbc, and the away calls from 0x76bf3a and 0x76c00e). They take the same new
pick (rt_squad_kept_at with the window of the squad, which reads the squad that pick replaces, counts a player only
with his squad's flag in staff +0x5e, and acts only for the friendly, 'B' and Under 21 keys that reach the gate): in release set row t2002 (seed 73) Argentina's under-21 squad
of its friendly of 19 August 2003 was kept on that day for the friendly of 2 September, and three regulars of a club
with a Copa Sudamericana match on 1 September were on duty for both matches.
That squad is the linked under-21 match's: add_fixture 0x5aa680 adds a linked B or under-21 match on the day before a
senior friendly (0x7e6720 at 0x5aab09), and 0x76bab0 keeps or picks its squad with the senior match's, on the day of
the marks two weeks before the senior match (0x7775c0 at 0x76b257, 0x76b28b and 0x76b2bf: that day and the days
either side). A club that plays the day before the linked match is not marked. So the linked part of the check (home
0x76bc4a to 0x76bdd4, away 0x76be9b to 0x76c026) also marks that day, and the marks come back as they were after it
and at the end 0x76c043 (rt_link_marks).
The marks and the new pick are one group: one rule of real life, and the reported case needs both. Test:
tests/seasons/club_regulars_run.py.

Moves to a linked side (open item 187, group squad_moves). A Spanish, German or Portuguese club can have a linked B side
(club_get_linked_b_team 0x540a50). Shortlist_Manager::assess_club_squad 0x83cb40 reassesses each squad every few weeks
and calls CONTRACT_MANAGER::move_to_reserves 0x4cc0f0 or move_to_first_team 0x4cc0b0, which act only when
CONTRACT_MANAGER::get_squad_placement 0x4cbb60 gives the other place (0x4cc104..0x4cc10a) and then call
CONTRACT_MANAGER::toggle_squad_placement 0x4cbd20. For a player whose own club (staff +0x39) is the B side and whose
contract is with the senior side, get_squad_placement answers from bit 0x40 of the contract's +0x35
(0x4cbcb8..0x4cbd07). Set gives 3 (on the B side), clear gives 4 (at the senior club). This holds only when the B side
does not link back to the senior side. Where it links back, the player's club gives 3 (0x4cbbfd) and the bit is not
read. toggle_squad_placement sets or clears that bit on its other paths (0x4cbdb6, 0x4cbe6a, and 0x4cbffe..0x4cc016 for
a player with a second contract), but its path for a player with one
contract (0x4cc03d..0x4cc091) only removes him from one side (database_remove_staff_from_club 0x53c190) and adds him to
the other (database_add_staff_to_club 0x53bd10). So after a move to the B side the placement still says 4, the next
assessment moves him again, and each move puts him in another squad slot of the B side. In a 2001 base game (with the
fixes of open item 138b) Hertha BSC Amateure held staff 1222 in 11 of its 50 slots by May 2011, and on 28 September 2020
MATCH_MAN::pick_team 0x6dbc50 picked him three times and stopped at its duplicate test (match_man..cpp 1225, 0x6dd3c1).

The original read an unset byte here: STAFF_CONTRACT::STAFF_CONTRACT 0x85e410 does not set +0x35, the contract terms are
built with it, and STAFF_CONTRACT::operator= 0x4cc810 copies +0x35 into the contract table (0x4cc88a..0x4cc8b9), whose
new slots were not cleared either. So in the original bit 0x40 of a new contract was whatever the stack or the heap
held, and the repeat happened or not by chance, in real life too. Open item 138b gives those bytes 0 (layout_reads.py),
so the bit starts clear and the repeat always happens. Here the path for a player with one contract sets the bit for a move
to the B side and clears it for a move back (0x4cc03d), so the placement follows each move. The direction comes from
the registers: at placement 4 the path leaves from the club asked about (edi = ebx, 0x4cbede), at placement 3 it goes
to it (ebp = ebx, 0x4cbed5). A bit that holds junk from an old save is then right after one move. A player with no
contract (index below 0) has no bit to set.

A loan to the linked B side (open item 280, group squad_loans). CONTRACT_MANAGER::sign_loan_contract 0x4d01e0
adds the player to the loan club (0x4d041f) and leaves him in his own club's squad (no call of 0x53c190), and no offer
code of TRANSFER_MANAGER tests the link, so the original can loan a player from a club to its own linked B side. He is
then in both squads, and each later move_to_reserves 0x4cc0f0 or move_to_first_team 0x4cc0b0 of
Shortlist_Manager::assess_club_squad 0x83cb40 (toggle_squad_placement 0x4cbd20) adds him to a second slot of the side
he goes to. In run C of the 0.7.2 full check (seven European nations with the regional and c2 extras) FC Energie
Cottbus II held one player in two squad slots on day 206 of 2007, and pick_team stopped at match_man..cpp 1225 on day
208. Here a wrap of toggle_squad_placement (original_faults.c) leaves a player alone in those two moves while he is on
loan from one side of a linked pair to the other. A move that the human makes is not changed.

A club in two key ids (open item 295, group club_keys). remap_club_key_id 0x60ba80 rewrites only the first key id that
holds a row, so the second key id of C.D. Logrones, C.D. Logrones B, C.D. Badajoz and C.D. Badajoz B keeps its old row
after database_trim_club_table 0x5267c0 (call 0x526d94). The stale key id then names another club, and
club_get_linked_b_team 0x540a50 can give that club a B side past the end of the table. Here every key id of the row
follows it in a season game (original_faults.c). The base game, a 2001 start with changing rules off, keeps the
original's key ids (decision L2).

The call-up count of the last club (open item 300, group callup_counts). NationalTeams::record_club_call_ups 0x7771c0
sorts a side's squad slots by club id (0x778ef0) and counts the players of each club in a run. It writes a club's count
to the club's call-up records (NationalTeams +0x20) only when the next counted player has another club
(0x7772f7..0x7773fe), and after the 50th slot it leaves (0x777426 to 0x77748b). So the club that sorts last in the
squad never gets a record. The records tell NationalTeams::can_schedule_club_fixture 0x776fc0 that a club has more
than two players on duty within a day of a fixture (clash 21, 0x5adb6b), and they feed the call-up move
NationalTeams::reschedule_clashing_club_fixtures 0x7777e0 (0x7778c3). In release set row t2006 (seed 73) Urawa Red
Diamonds, the club with the highest id in Japan's squad, had no record for Japan's friendly of 11 September 2007, so
find_new_fixture_date 0x5ab510 moved its AFC Champions League first leg of 30 August (made on 29 August) to
12 September, with 5 of its players in Japan's squad. Here the end of the loop (0x777426) goes back once to the write
of a run (0x7772f7) with the club of the open run, and the write leaves at 0x777401, where the slot counter at
esp + 0x13 is 50 only on that pass. Test: tests/seasons/callup_counts_run.py.
"""

# address: (the instruction in the original, C code)
INSNS = {
    0x7AB158: ("je 0x7ab167", "if (cc_e(F) && !rt_national_vacancy(edi, 1u)) goto L_7ab167;"),
    0x68FE65: ("je 0x68fe92", "if (cc_e(F) && !rt_national_vacancy(ebp, 2u)) goto L_68fe92;"),
    0x69D0A5: ("je 0x69d110", "if (cc_e(F) && !rt_national_vacancy(ebx, 3u)) goto L_69d110;"),
    0x69B6D8: ("je 0x69b6e3", "if (cc_e(F) && !rt_national_vacancy(esi, 4u)) goto L_69b6e3;"),
    0x69BB02: ("jle 0x69c525", "if (cc_le(F) && !rt_national_vacancy(esi, 0u)) goto L_69c525;"),
    0x598F4F: ("cdq", "edx = rt_budget_hi(eax, R32(esp + 0x44u));"),
    0x598F8B: ("cdq", "edx = rt_budget_hi(eax, R32(esp + 0x44u));"),
    0x598F92: ("cdq", "edx = rt_budget_hi(eax, ebx);"),
    0x599135: ("mov ecx, dword ptr [eax]", "ecx = rt_budget_floor(eax, 10000000u);"),
    0x599185: ("mov ecx, dword ptr [ecx]", "ecx = rt_budget_floor(ecx, 15000000u);"),
    0x5991D0: ("mov eax, ecx", "eax = rt_budget_floor_low(ecx);"),
    0x5991D6: ("cdq", "edx = rt_budget_floor_high(eax);"),
    0x599201: ("fiadd dword ptr [esp + 0x5c]", "ST(0) = ST(0) + (double)rt_budget_floor_wide(R32(esp + 0x5cu));"),
    0x43F460: ("fild dword ptr [ecx]", "fpush((double)rt_cash_amount(ecx));"),
    0x46399C: ("mov dword ptr [esp + 0x18], eax",
               "W32(esp + 0x18u, eax); rt_cash_wide(esp + 0x18u, eax, R32(ebp + 4u), 2u);"),
    0x4639EE: ("mov dx, word ptr [0xadab48]", "rt_cash_wide_end(); edx = (edx & 0xffff0000u) | R16(0xadab48u);"),
    0x463A14: ("mov dword ptr [esp + 0x18], eax",
               "W32(esp + 0x18u, eax); { uint64_t nb_ = 0u - R64(ebp); "
               "rt_cash_wide(esp + 0x18u, (uint32_t)nb_, (uint32_t)(nb_ >> 32), 2u); }"),
    0x463A6E: ("lea eax, [esp + 0xac]", "rt_cash_wide_end(); eax = (uint32_t)(esp + 0xacu);"),
    0x463B28: ("mov dword ptr [esp + 0x18], eax", "W32(esp + 0x18u, eax); rt_cash_wide(esp + 0x18u, eax, edx, 2u);"),
    0x463B7A: ("mov dx, word ptr [0xadab48]", "rt_cash_wide_end(); edx = (edx & 0xffff0000u) | R16(0xadab48u);"),
    0x808EF3: ("mov dword ptr [esp + 0x64], eax", "W32(esp + 0x64u, eax); rt_cash_wide(esp + 0x64u, eax, edx, 1u);"),
    0x8EA2C0: ("mov dword ptr [esp + 0x48], ebx", "W32(esp + 0x48u, rt_cash_wide_clamped(esp + 0x48u, ebx, edi, 1u));"),
    0x59F36B: ("push edx", "rt_news_wide(ecx, R32(esi), edx); t0 = edx; esp -= 4; W32(esp, t0);"),
    0x6A5021: ("push eax", "rt_news_wide(esp + 0x10u, R32(esi), eax); t0 = eax; esp -= 4; W32(esp, t0);"),
    0x59F5AB: ("push 3", "rt_news_read(edi, eax); t0 = 0x3u; esp -= 4; W32(esp, t0);"),
    0x59F5C7: ("mov dword ptr [esp + 0x18], ebx", "W32(esp + 0x18u, ebx); rt_cash_news(esp + 0x18u, ebx);"),
    0x6A51DD: ("push 3", "rt_news_read(esi, eax); t0 = 0x3u; esp -= 4; W32(esp, t0);"),
    0x6A533D: ("mov dword ptr [esp + 0x10], eax", "W32(esp + 0x10u, eax); rt_cash_news(esp + 0x10u, eax);"),
    0x6B0366: ("mov dword ptr [esp + 0x20], edi", "W32(esp + 0x20u, edi); rt_cash_wide(esp + 0x20u, edi, ecx, 1u);"),
    0x6B0399: ("mov dword ptr [esp + 0x3c], edi", "W32(esp + 0x3cu, edi); rt_cash_wide(esp + 0x3cu, edi, ecx, 1u);"),
    0x887FA7: ("mov dword ptr [esp + 0x34], eax",
               "W32(esp + 0x34u, eax); rt_cash_wide(esp + 0x34u, eax, R32(edi + 0xa8u), 1u);"),
    0x88808F: ("mov dword ptr [esp + 0x34], eax",
               "W32(esp + 0x34u, eax); rt_cash_wide(esp + 0x34u, eax, R32(edi + 0xb0u), 1u);"),
    0x76CE1C: ("je 0x76cf82", "if (cc_e(F) || rt_olympic_keeps_senior(esi, ebp, ebx, R32(esp + 0x8ecu))) goto L_76cf82;"),
    0x63B2AE: ("jne 0x63b2c6", "if (!cc_e(F) || rt_ire_super_has(esi, edi, ebx)) goto L_63b2c6;"),
    0x5968E8: ("mov edx, dword ptr [0xae23b4]", "rt_max_wage_high(edx, eax); edx = R32(0xae23b4u);"),
    0x597019: ("add ecx, edx", "ecx = rt_max_wage_sum(ecx, edx);"),
    0x411E1C: ("mov eax, dword ptr [esi]", "SYNC(); rt_aus_nsl_new_season(esi); RT_AFTER_CALL(); eax = R32(esi);"),
    0x84B02F: ("je 0x84b0ad", "if (cc_e(F)) { SYNC(); t0 = rt_holiday_predict(ebx, edi, ebp); RT_AFTER_CALL(); if (t0 == 1u) goto L_84b01c; "
                              "if (t0 == 2u) { esi = R32(ebp + 0x52u); goto L_84b0e7; } goto L_84b0ad; }"),
    0x84AF77: ("jne 0x84b174", "if (!cc_e(F)) goto L_84b174; SYNC(); t0 = rt_holiday_refuses_loan(ebx); RT_AFTER_CALL(); "
                               "if (t0) goto L_84b162;"),
    0x8C8264: ("jne 0x8c8277", "if (!cc_e(F) && !rt_holiday_below_asking(eax, esi)) goto L_8c8277;"),
    0x76CC9A: ("jmp 0x76cf82", "if (rt_callup_released(R8(esp + 0x8e4u))) goto L_76cd79; goto L_76cf82;"),
    0x76CD0B: ("je 0x76cf82", "if (cc_e(F) && !rt_callup_released(R8(esp + 0x8e4u))) goto L_76cf82;"),
    0x76CD3E: ("je 0x76cf82", "if (cc_e(F) && !rt_callup_released(R8(esp + 0x8e4u))) goto L_76cf82;"),
    0x777704: ("je 0x777713", "if (cc_e(F) && !rt_callup_mark_all()) goto L_777713;"),
    0x777729: ("je 0x77778f", "if (cc_e(F) && !rt_callup_mark_all()) goto L_77778f;"),
    0x76BBA8: ("je 0x76bc10", "if (cc_e(F) || !rt_squad_kept(ebp, R32(esi + 0x1cu), edi, esi)) goto L_76bc10;"),
    0x76BE1B: ("je 0x76be83", "if (cc_e(F) || !rt_squad_kept(ebp, R32(esi + 0x20u), edi, esi)) goto L_76be83;"),
    0x76BC4A: ("mov ax, word ptr [esi + 0x4d]", "SYNC(); rt_link_marks(ebp, esi, 1u); RT_AFTER_CALL(); "
                                                "eax = (eax & 0xffff0000u) | (R16(esi + 0x4du) & 0xffffu);"),
    0x76BDD4: ("mov ecx, dword ptr [esi + 0x20]", "SYNC(); rt_link_marks(ebp, esi, 0u); RT_AFTER_CALL(); ecx = R32(esi + 0x20u);"),
    0x76BE9B: ("mov ax, word ptr [esi + 0x4d]", "SYNC(); rt_link_marks(ebp, esi, 1u); RT_AFTER_CALL(); "
                                                "eax = (eax & 0xffff0000u) | (R16(esi + 0x4du) & 0xffffu);"),
    0x76C026: ("mov eax, dword ptr [esi + 0x1c]", "SYNC(); rt_link_marks(ebp, esi, 0u); RT_AFTER_CALL(); eax = R32(esi + 0x1cu);"),
    0x76C043: ("mov ecx, dword ptr [esp + 0x24]", "SYNC(); rt_link_marks(0u, 0u, 0u); RT_AFTER_CALL(); ecx = R32(esp + 0x24u);"),
    0x76BC76: ("je 0x76bce9", "if (cc_e(F) || !rt_squad_kept_at(ebp, R32(esi + 0x1cu), edi, esi, 0x4eu)) goto L_76bce9;"),
    0x76BEC7: ("je 0x76bf3a", "if (cc_e(F) || !rt_squad_kept_at(ebp, R32(esi + 0x20u), edi, esi, 0x4eu)) goto L_76bf3a;"),
    0x76BD34: ("je 0x76bdbc", "if (cc_e(F) || !rt_squad_kept_at(ebp, R32(esi + 0x1cu), edi, esi, 0x62u)) goto L_76bdbc;"),
    0x76BF85: ("je 0x76c00e", "if (cc_e(F) || !rt_squad_kept_at(ebp, R32(esi + 0x20u), edi, esi, 0x62u)) goto L_76c00e;"),
    0x777426: ("jmp 0x77748b", "if (rt_callup_count_last(esi)) goto L_7772f7; goto L_77748b;"),
    0x777401: ("mov esi, dword ptr [edi + 0x39]", "if ((int8_t)R8(esp + 0x13u) >= 0x32) goto L_77748b; esi = R32(edi + 0x39u);"),
    0x526D99: ("add esp, 8", "rt_club_key_twins(R32(esp), R32(esp + 4u), eax); esp += 8u;"),
    0x4CC03D: ("mov ecx, dword ptr [0xb74350]", "rt_squad_move_set(R32(esp + 0x10u), esi, edi == ebx); ecx = R32(0xb74350u);"),
    # Issue 132, log only (no change of the game): HolFirst::set_club_progress 0x5f73d0 (vtable 0x96b86c slot +0x68)
    # writes progress bytes +0x73..+0x75 of the club's Comp_Stats entry, which Comp_Stats::find_club_entry 0x4a4850
    # gives or NULL. The first lookup (call 0x5f73e1) is read at 0x5f742e, the Eredivisie one (call 0x5f74a7)
    # at 0x5f74ec and 0x5f74fe. No path to a NULL is known (run-logs/0.8.0-s817/
    # site-netherlands.md), so these lines log a hit for the run set and the game goes on as before.
    0x5F73E6: ("mov bp, word ptr [esi + 0x3e]",
               "if (eax == 0u) rt_crash_site(0x5f73e1u); ebp = (ebp & 0xffff0000u) | R16(esi + 0x3eu);"),
    0x5F74AC: ("mov bl, byte ptr [esp + 0x20]",
               "if (eax == 0u) rt_crash_site(0x5f74a7u); ebx = (ebx & 0xffffff00u) | R8(esp + 0x20u);"),
}
