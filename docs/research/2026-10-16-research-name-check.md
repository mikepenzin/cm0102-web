# Name check of the 0.6.0 research docs

Date 2026-10-16. This check compares every function name and address in the 0.6.0 research docs with the Opus names of the function map. Where a claim rests on a wrong earlier name, I read the code and fixed the claim.

## Scope and method

- Docs: `rules-0.6.0/` (all 118 files), the 0.6.0 research brief (kept outside the repo), `2026-10-16-season-database-table.md` and the four `2026-10-16-sweep-*.md` docs. They cite 994 distinct functions at 3,474 places.
- Names: `function-map/names.tsv` in the working tree, with the committed waves to sweep-w17 (b57d7fb) and the uncommitted rows of sweep-w18: 3,960 Opus rows. The map changed twice while this check ran. A later wave can turn more skipped rows into Opus rows.
- Evidence: the dossiers in `funcmap-work/dossiers/` and the clean 3.9.68 exe (vtables and instructions read with pefile and capstone).
- A row with model `haiku` or `slot copy` is not yet an Opus name. I skipped those and list them below.

## The two open points

Both are research errors in `2026-10-16-sweep-europe.md` (format table, section 4). The function map is right. The sweep counted the group stage objects only and left out the group that each competition holds in its own table.

| Competition | Research said | Code says | Evidence |
|---|---|---|---|
| Club World Championship | 2 groups of 4, 8 clubs | 12 entrants in 3 groups of 4. The 3 group winners and the best runner-up go to a 4-club stage of 3 rounds (semi-finals, third place match, final) | 0x92a8b0 sets +0xf2 = 12 and fills 12 slots. 0x929aa0 builds the own 4-club group from entrants 0, 3, 6, 9. 0x929e60 builds two League_Stage groups from entrants 1, 4, 7, 10 and 2, 5, 8, 11. 0x92a140 takes the 3 winners and the best of a 3-row runners-up table into a 4-club Cup_Stage. 0x929390 gives stage 2 three rounds |
| Merconorte Cup | 3 groups | 16 entrants in 4 groups of 4. The 4 group winners go to a 4-club stage of 2 rounds (semi-final, final) | 0x4c2560 sets +0xf2 = 16. 0x4c1de0 builds the own group from entrants 0 to 3. 0x4c2140 builds groups from entrants 4n+4 to 4n+7. 0x4c1ed0 sets 1 qualifier a group (+0xbe = 1). 0x4c2330 takes it from the own table and 3 stages |

The same miscount was in four more rows of that table. I fixed them too:

- Copa Libertadores: 8 groups of 4, not 7 (0x4c37a0 own group, 0x4c4410 calls 0x4c3b10 seven times). The top 2 of each group go to a 16-club knockout (0x4c3d10).
- Copa Mercosur: 5 groups of 4, not 4 (0x4c4e60 own group, 0x4c5c90 calls 0x4c51d0 four times). 5 winners and 3 best runners-up go to the quarter-finals (0x4c53d0).
- Oceania Club Championship: 2 groups of 4, not "1 group stage" (0x7978c0, 0x797e80, 0x798050).
- Asian Club Championship and Asian Cup Winners' Cup: 32 entrants in two zones of 16, not 16 (0x40b950 and 0x40d830 set +0xb6 = 0x20).

## Changes made

`2026-10-16-sweep-europe.md`

- Format table: the six rows above, with the new evidence addresses.
- Club World Championship entrants: 2 each from Africa, Asia, Europe, North America and South America, 1 from Oceania, 1 from the host nation (0x92a8b0).
- 0x6699d0: `nation_has_active_entry` became `key_nation_continent_has_selected_nation`. The claim now says that a creator runs when a selected nation of that continent exists.
- Vtable slot table: slot 2 is `Cup::start_new_season` (0x518780), not `Cup::release_data`. Proof: 0x518780 is at slot 2 of the vtables at 0x9693c8, 0x969468 and 0x96d8ac. Slots 14, 22 and 24 now carry the Opus names. Slot 24 is `Comp::allows_fixture_clash`: the base allows a date clash for a round code under 1000 that ends in zero, and the three UEFA cups never allow one.
- Section 7: the Champions League group draws are `EuropeanCup::draw_phase1_groups` and `draw_phase2_groups`.
- Unknowns: the Club World Championship host is partly answered (0x5fab70, see "Function map rows").
- `key_nation_init_table` became `key_nation_build_table`.

`2026-10-16-sweep-season-end.md`

- Step 5 of the season end: 0x5d6280 is `game_update_league_attendances`, not `game_update_staff_wages`. It sets club attendances from final position, promotion, relegation and division reputation. It does not touch wages.
- Step 4: 0x59cdd0 is `FinanceTable::end_season_for_nation`.
- Transfer rules: +0x13 and +0x14 are the foreign player signing caps, read by `TRANSFER_RULES::can_sign_player` 0x8e8560 and `can_make_offer` 0x8e8800. The doc said "meaning unknown".
- Vtable 0x970458: slots +0xc, +0x10, +0x18, +0x2c and +0x30 carry the Opus names (`can_make_offer`, `can_sign_player`, `get_ineligibility_code`, `post_deadline_news`, `write_deadline_news_text`).
- Section 4.3: the doc said that four first-pass functions "apply the limits". Now: 0x8cf880 checks the matchday squad limits, 0x8e8560 and 0x8e8800 check the signing caps, and 0x8cd450 only keeps a club within 50 players and its staff slots.
- Section 4.4: 0x8c7330 is `TRANSFER_MANAGER::update_work_permits`. A permit 5 or more game years old sets the staff record's nation field +0x1e to the permit's nation and leaves the list. The doc said only that it "drops records more than 4 years old". 0x8c7f4a lies in `update_work_permit_on_move` 0x8c7e20.
- Eight renames with the same meaning: 0x8cf470, 0x8e8c40, 0x8e8ed0, 0x8c6bc0, 0x8c7070, 0x54a130, 0x58a890, 0x48f2d0.

`2026-10-16-sweep-rulebooks.md`

- RulingBody vtable 0x96f104: slot +0x0 (0x7e7ed0) is `RulingBody::start_new_season`. It clears booking counts at the season start and releases old bans. Slot +0x4 (0x7e7d50) is `RulingBody::daily_update`. It applies queued bans. The doc called them "process pending entries" and "process pending rulings by date".
- Section 3.2: 0x8e8560 is `TRANSFER_RULES::can_sign_player`. The non-EU cap refuses with code 0x15 (the doc gave 0x14 for both caps) and runs only when the rules object's nation is in Europe.
- Appendix tables: 1,193 function cells renamed by address from first-pass names to Opus names (446 functions). Each cell was matched to the function that holds the row's address, so a shared name such as `Cup::Cup` changed only for its own function.
- Nine rows marked "not a competition": 0x7ab40b and 0x7abdbd (a date word into a staff record), 0x8bf3b4 (a transfer record), 0x8c6e10 and 0x8c6e42 (contract offers) and 0x4cc707, 0x4cc70d, 0x4cecfd, 0x4ced1b (contract offer status bytes). The doc listed them as club counts, substitutes or ruling bodies.

`2026-10-16-sweep-international.md`

- Hosts: 0x5f9970, 0x5fa7d0 and 0x5fa850 carry the Opus names. 0x5fab70 runs only from `WorldClubChamp::start_new_season`, so every second year. It redraws the Club World Championship and Confederations Cup host rows and swaps the World Cup Oceania section host. The doc called it `rotate_yearly_hosts`.

`rules-0.6.0/int-continental.md` and `int-continental.tsv`

- The Copa América runs in `goldcup.cpp`. The record named "Copa America" sets global 0x9cf7a8 (compare at 0x61035b), and setup 0x830ac0 passes it to the goldcup constructor 0x5e0660. No name in the matcher 0x60efd0 is a Gold Cup. The doc mapped the CONCACAF Gold Cup to `goldcup` and said that no file runs the Copa América. 27 Gold Cup rows now have `comp_file` = `new`. 27 Copa América rows now have `goldcup`. 17 notes said "The original has no Copa América code". They now say that the original runs it in odd years.

`rules-0.6.0/wal-nir-irl.md`

- 0x63a740 is `IrePrm::create_table`: it accepts only 10 clubs (4 meetings) or 12 (3 meetings). Claim confirmed, with the meetings added.
- 0x78ac90 is `NirCup::collect_entrants`: a 32-club list of 12 random clubs from up to 30 of one division, then every club of two other divisions. The doc said "fills up to 30 top clubs".

`rules-0.6.0/spa.md`

- Spanish non-EU limit (0x859570): the First Division and the competition with key id 0x9cf740 get 4 in 2000 and 3 in every later season. Other competitions get 3 in 2001 and 2002 and 2 later. The note gave only the second rule.

the 0.6.0 research brief (kept outside the repo)

- The brief said that the function map has first-pass names and that its work folder is gone. It now says which rows are checked names.

## Claims confirmed

- `rules-0.6.0/club-world.md`: the eleven constructors and globals, the odd-year start of the Club World Championship (0x9291b2), the Recopa lookup without code (0x60f799), the Libertadores branch on year above 2002 (0x4c64fe).
- `rules-0.6.0/hol-bel.md`: `HolFirst::create_play_off_groups` 0x5f6720, `HolFirst::advance_stage` and `HolFirst::create_schedule` 0x5f5790 agree with the Opus rows.
- `rules-0.6.0/int-continental.md`: 0x7e29e0 is `RbSouthAmerica::RbSouthAmerica`.
- The sweep docs' own Opus names that differ from the map only in form (for example `Eur_Super_Cup::pick_clubs` and `EurSuperCup::select_teams`, `national_teams::fill_squad` and `NationalTeams::select_squad`, `load_2002_campaign` and `EuroChampQual::create_groups_and_results`). The claims agree with the Opus purposes. I left these names as written.
- `League::update_for_date`, `League::check_complete`, `League::pay_prize_money`, `game_season_update_pass`, `game_main_day_loop`, `Comp::all_stages_complete` and the UEFA seeding names in the season-end steps.
- `2026-10-16-season-database-table.md` cites no function. Nothing to check.

## Skipped: no Opus name yet

These rows are still `haiku` or `slot copy`. The docs keep the first-pass names.

- Season end: 0x447880, 0x7c1350, 0x86d6e0 (step 4), 0x556210, 0x84b940 (step 5), 0x442e80, 0x447430, 0x4a19f0 (step 1), 0x828110 and 0x830520 (setup), 0x53d740 (work permits).
- Europe and international: 0x60efd0 (the name matcher), the creators 0x830fe0, 0x8312e0, 0x831650, 0x8318f0, 0x831ba0, 0x831eb0, 0x830620, 0x830ac0, and 0x8386f0, 0x839340, 0x5ca4f0, 0x85a9b0.
- Rule book appendix: 104 cells in 42 functions of `tur_*`, `usa_*`, `wel_*`, `swe_prm`, `swe_second`, `wel_cup` and `Human_manager::load_human_record`, `player_build_skill_vector`.
- `rules-0.6.0/wal-nir-irl.md`: 0x925b30 (`wel_first`).

## Function map rows that I think are wrong

I did not edit the generated docs.

1. 0x5fab70 `HostCountry::pick_club_world_champ_hosts` is too narrow. The body gives a random host to rows whose competition is 0x9cf7bc (Club World Championship) or 0x9cf964 (Confederations Cup). It also swaps the host of row 0x9cf76c (World Cup Oceania section) between two nations. A better name: `HostCountry::redraw_world_club_and_confed_hosts`.
2. `goldcup.cpp`: every row calls the class `GoldCup` and the purposes say "Gold Cup". The class runs the Copa América (0x9cf7a8, see above). The sweep-international doc already calls it `copa_america`.
3. 0x53d740 (haiku, `transfer_offer_is_blocked`): in `can_sign_player` the non-EU count takes a foreign player for whom 0x53d740 is false. So 0x53d740 tests EU status. Read it before the next wave names it.
4. The `slot copy` rows named `::release_data` (0x8fcdb0, 0x90a760, 0x90b790, 0x924c50) fill vtable slot 2. Slot 2 is `start_new_season` (0x518780 in Cup vtables, 0x4c1440 in the ConChamp vtable). The swept files already have `start_new_season` there.

## Files

Every in-scope doc is untracked: `rules-0.6.0/`, the 0.6.0 research brief (kept outside the repo), `2026-10-16-season-database-table.md` and the four sweep docs. This report is untracked too. I committed nothing. The `hol-bel.md` fix of the other check is still uncommitted.
