# Argentine Primera: zone tables and title knockouts (code research)

Used by src/recomp_rt/win/season_arg_stages.c (0.7.2 gap row 48). Read 15 November 2026 from the instruction listings of build/recomp/gen (the original 3.9.68 at virtual addresses). The format facts (bracket, venues, extra time, dates) are in the research notes of the commit message and in docs/research/arg-zones-0.7.2/README.md.

Date 2026-11-15. Original: `clean-3968/cm0102.exe` 3.9.68, virtual addresses. Method: the instruction listing in the comments of `build/recomp/gen/fn_*.c` (REA timed out on every decompile) and a small walker over the switch of `comp_util_get_stage_name` 0x4b4850 for the name texts. "Inf." marks an inference. No code was changed.

## 1. League_Stage

### 1.1 League_Stage::League_Stage 0x68b100 (thiscall, 22 arguments, ret 0x58)

Arguments are read at [esp+0x224+4*n] after the prologue (0x68b12a..0x68b27c).

| Arg | Field | Meaning | Evidence |
|---|---|---|---|
| a1 | (+0x49, +0x4a copied) | parent Comp | 0x68b22f, 0x68b248..0x68b261 |
| a2 | +0x3e s16 | club count of the table | 0x68b141 |
| a3 | | Club* list of a2 entries, or 0 for empty rows | 0x68b375..0x68b39b (League::add_club_row 0x687430 per club) |
| a4 | +0x3c s16 | meetings | 0x68b14d |
| a5 | +0x4 | competition record (the parent's record) | 0x68b173 |
| a6 | +0xba | date template (a7 x 0x41 bytes, copied, the caller frees its own) or 0 | 0x68b26f..0x68b2d9 |
| a7 | +0xa9 s16 | date count | 0x68b158 |
| a8, a9, a10 | +0xc2, +0xc3, +0xc4 | rule bytes of the parent (copied from the parent's same fields by every caller) | 0x68b166, 0x68b19d, 0x68b17f |
| a11 | +0xc5 (4 bytes) | tie-break codes (pointer, copied) | 0x68b1a5..0x68b1b3 |
| a12 | +0xbe (4 bytes) | places (pointer, copied) | 0x68b1be..0x68b1cc |
| a13 | +0x40 s16 | year | 0x68b1e4 |
| a14 | +0x44 s8 | stage index; must equal the slot in the parent's +0xc (routing compares it, 0x685ed8) | 0x68b206 |
| a15 | +0x3a s16 | stage name id (the screens label) | 0x68b211 |
| a16 | +0x51 | copied from parent +0x51 by ArgPrm, 0 by BelThird periods. Meaning unknown | 0x68b185 |
| a17 | +0x42 | 1 in every caller seen. Meaning unknown | 0x68b21d |
| a18 | +0xab s16 | 0 in every caller seen | 0x68b23a |
| a19 | +0xd9 s16 | parent +0xd9 (ArgPrm, BelThird group B) or 2 (BelThird periods) | 0x68b241 |
| a20 | +0xa7 s16 | matches of a club; -1 = (a2-1) x a4 at the first club row (0x687453..0x6874de) | 0x68b228 |
| a21 | +0x28 | parent +0x28 or 0 | 0x68b264 |
| a22 | +0xe1 | parent +0xe1 or 2 | 0x68b267 |

Also: +0x43 = 2 (stage type League_Stage, 0x68b236), vtable 0x96d0f0, +0x1c/+0x20/+0x2c/+0x30 = -1. With a6 = 0 the constructor makes a template of one date, today (0x68b3f3..0x68b495), and makes no fixtures; with a template it calls League::create_fixtures 0x6835c0 (0x68b3b9). At the end it sorts the table (0x6827d0 at 0x68b4a2).

Callers that show the forms:
- ArgPrm Clausura, stage 0 (0x405a30..0x405a94): (this, +0x3e, clubs, +0x3c, +4, tmpl(0), count, c2, c3, c4, &c5, &be, +0x40, 0, name, +0x51, 1, 0, +0xd9, -1, +0x28, +0xe1).
- ArgPrm aggregate, stage 1 (0x405b2a..0x405b83): (this, +0x3e, clubs, 2, +4, tmpl(1)=0, count, ..., +0x40, 1, name 0, +0x51, 1, 0, 0, -1, 0, +0xe1).
- BelThird group B, stage 0 (0x422b33..0x422b92): (this, +0x3e, list, 2, +4, tmpl(0), count, ..., 0, name 0x420, +0x51, 1, 0, +0xd9, -1, 0, 2).
- BelThird period table (0x422e8d..0x422ed4): (this, +0x3e, rows of group, 1, +4, 0, 0, ..., +0x40, +0x30 (new index), name 0x45d..0x462, 0, 1, 0, 2, 0xa, 0, 2). This is a table with no dates that gets results only by routing: the model for a zone table.

### 1.2 How BelThird makes group B from a subset

BelThird::create_group_b_stage 0x422970: one call of create_schedule(0) through vtable +0x3c (0x4229a4) for the dates and the name; a list of +0x3e (16) pointers (0x4229b3); a scan of the club table [0xae23b4] (0x245 bytes a club) for clubs with division +0x57 == this +4 and last division +0x60 == key [0x9cf5a8] (Third Division B) (0x422a1a..0x422aae); an assert if fewer than +0x3e are found (0x422add); then the constructor with a2 = +0x3e and a3 = the list (0x422b92), stored in stages[0] (0x422ba9), and +0x30 = 0 (0x422c2d). The league object itself holds only group A (16 rows, fill_league_table 0x4224e0). A stage's club count is its own (a2): it need not equal the parent's.

### 1.3 Results routing and one-sided matches

League::add_result 0x685d30 (thiscall (match, update_stats), ret 8; vtable +4 of League and League_Stage):

```
if (this+0x44 == 0xff && match+0x42 != 0xff) {          // the league object, a stage match
    i = match+0x42;
    if (i < 0 || i > this+0x30) assert, return 0;         // 0x685ddb..0x685de7: +0x30 bounds the routing
    return stages[i]->add_result(match, update_stats);   // 0x685de9..0x685dfa
}
if (this+0x44 != match+0x42 && this+0x44 != 0xfe) return 0;   // 0x685ed8..0x685eec
apply(match, rows +0xb1, count +0x3e, 1, ...)             // 0x685f0a -> 0x685f60
if (update_stats && this+0x44 != 0xfe)                    // 0x685f0f..0x685f4a
    objects[record]->+8 (Comp_Stats) -> 0x49f570(match, ...)
```

The stats go to the Comp_Stats of the object of the record (+4), which a stage shares with its parent. ArgPrm::add_result 0x405c50 therefore zeroes its +8 while it routes a result again to the aggregate table (0x405c65..0x405c8f); BelThird::add_result 0x422c50 does the same for its period tables (0x422ca6..0x422cc5).

0x685f60 (apply a result to the rows) finds the home row (club match+0x1c) and the away row (club match+0x20) each on its own, -1 when absent (0x685faf..0x685ff9). Every update is under a test of that row: home 0x685ffb, away 0x68604a, the result blocks 0x6860aa..0x686452, totals 0x686452 and 0x6864cf. The head-to-head update 0x6869a0 returns at once when either index is below 0 (0x6869a0..0x6869b2). So a League_Stage accepts an inter-zone result and updates only the row of its own club. The position history row +9 holds +0xa7 bytes and the sort writes it only while played <= +0xa7 (0x682af1..0x682b0f), so more matches than +0xa7 do not overrun it, but give a20 = the real matches of a zone club (16 for a zone of 15 with 2 inter-zone rounds) so that check_complete (1.4) waits for all of them.

Safe way (recommended): route, as BelThird does for its period tables.

```
// in a wrap of ArgPrm::add_result 0x405c50, after the original, only for a league match of a zone season:
half = (match+0x42 == 0xff) ? Apertura : (match+0x42 == 0) ? Clausura : none;   // cup stage matches: none
for zone stage z of that half (or only the two zones of the two clubs):
    save = obj+8; obj+8 = 0; old = match+0x42;
    match+0x42 = z;  League::add_result 0x685d30 (obj, match, 0);   // needs obj+0x30 >= z
    match+0x42 = old; obj+8 = save;
then League::sort_table 0x6827d0 (stage, 0) for the stages touched (Inf.: the original sorts stage tables in its daily update; not traced)
```

Alternative: rebuild the rows from the parent's rows and sort. Row (0x3b bytes, League::add_club_row 0x687430, 0x68751e..0x687563): +0 Club*, +4 position, +5 head-to-head block, +9 position history, then three blocks of 0xe bytes from +0xd (overall), +0x1b (home), +0x29 (away), each: +0 dword, +4 played, +5 won, +6 drawn, +7 lost, +8 s16 goals for, +0xa s16 goals against, +0xc s16 points (overall: +0x11 played, +0x12 W, +0x13 D, +0x14 L, +0x15 GF, +0x17 GA, +0x19 Pts; totals rebuilt from home + away at 0x686452..0x6864b1); +0x37 status (3 = relegated, 0x405e4f), +0x38, +0x39, +0x3a. Sort: League::sort_table 0x6827d0 (thiscall (rows or 0 for +0xb1), ret 4): tie-break codes +0xc5, compare +0xad, writes +4 and +9 (0x682ad1..0x682b1d). A rebuild loses the head-to-head data that tie-break code 4 reads (0x682850..0x682877), so routing is better.

### 1.4 Completion of a stage

League::check_complete 0x689ad0 (thiscall (check_stages, notify), ret 8): a stage (+0x44 != 0xff) is complete when +0xdd is set, else when every row has +0x11 >= +0xa7 (0x689b3d..0x689b64; +0xdd set at 0x689c06). For the league object with check_stages = 1, every stage 0..+0x30 must be complete (vtable +0x10, 0x689bbc..0x689bdd) and +0x30 >= +0x2c - 1 (0x689bdf..0x689be8). A League_Stage with no dates and a20 = 16 completes only when its 15 clubs have 16 results.

## 2. The stage array of ArgPrm

Fields: +0xc stages (array of pointers), +0x2c stage count, +0x30 index of the last live stage.

- ArgPrm::ArgPrm 0x404250: League ctor 0x682200 sets +0xc = 0 (0x682291); load first (0x4042a5). With no saved data: +0x44 = -1, +0x30 = -1, +0x2c = 2, +0xc = malloc(8) (0x4042b2..0x4042d2), then create_stages 0x405950 (0x4043e9), which stores stages[0] (0x405aac) and stages[1] (0x405b9b) and sets +0x30 = 1 (0x405c24).
- Save, League 0x6891c0: writes +0x2c, +0x30, +0x44 (0x6898d9..0x6898fe), then for i = 0..+0x30 the type byte +0x43; type 1 (Cup_Stage): +0x3e, +0x3c, each round's +0x1a, then Cup_Stage::save_to_file 0x523eb0; type 2 (League_Stage): +0x3e, +0xa9, then League_Stage::save_to_file 0x68ca40 (0x689903..0x6899f1). A slot <= +0x30 must hold a live stage (it reads [stage]+0x43 with no null test, 0x689926..0x68992f). Stages above +0x30 are not saved.
- Load, League 0x687b10: reads +0x2c, +0x30, +0x44 (0x688e10..0x688e35); if +0xc is 0 allocates +0x2c x 4 (0x688e44..0x688e5b); then for i = 0..+0x30 reads the type: 1: new 0xb2, Cup_Stage::Cup_Stage(count, rounds, ties) 0x5230e0, load 0x523380; 2: new 0xee, League_Stage::League_Stage(count, dates) 0x68b4f0, load 0x68b720 (0x688eb1..0x689017). The constructor calls load before it allocates its 8 bytes, so a saved game gets an array of its saved +0x2c.
- ArgPrm::~ArgPrm 0x4044b0: frees stages 0..+0x30 through vtable slot 0 with 1 (scalar deleting destructor; 0x404500..0x40451f), sets +0x30 = -1, frees +0xc (0x40452c).
- ArgPrm::start_new_season 0x405720: uses stages[0] for 0x68a980 (0x405753..0x405760), frees stages 0..+0x30 (0x4057b5..0x4057d5), +0x30 = -1 (0x4057df), keeps the +0xc array, later create_stages again (0x40590c). Cup_Stage slot 0 is 0x5230c0, so it frees a Cup_Stage too.
- League::update_for_date 0x6847c0 for the league object: every stage 0..+0x30 gets its update (vtable +0x14, 0x6848e5..0x684901). When the league's own table is complete (+0xdd = 1 after check_complete(0,0), 0x684994..0x6849a4), every stage 0..+0x30 must be complete (0x6849aa..0x6849cf); then if +0x30 < +0x2c - 1 it calls advance_stage, vtable +0x28 (0x6849d1..0x6849e3), else it closes the season (0x6849e8 onward, record_season_honours vtable +0x30 at 0x684a8f).
- ArgPrm::add_result 0x405c50 (vtable +4): League::add_result for the match, then again with match+0x42 = 1 and +8 = 0 (aggregate table). It does this for every match, whatever its +0x42.
- ArgPrm vtable 0x967324: +0x28 advance_stage = 0x5a8f60 (the empty function), +0x30 = 0x405ca0, +0x44 = 0x686940 (base result handler).

Answer: stages 2..5 (League_Stage, no dates) and later Cup_Stages can be appended, with these rules:
1. Make +0xc hold +0x2c slots (game malloc 0x944e46, free 0x9452ca), in a wrap of create_stages 0x405950 (the constructor and start_new_season call it). The constructor's 8 bytes hold 2.
2. Set +0x30 to the last index only after that stage exists (save and the destructor walk 0..+0x30 with no null test).
3. Every index <= +0x30 is a live stage, no gaps. Zone stages first (2..5), cup stages after, in the order they are built.
4. +0x2c is the number of stages of the full season. While +0x30 < +0x2c - 1 the season cannot close, and update_for_date calls advance_stage 0x5a8f60 each day once the league table and all built stages are complete. ArgPrm has nothing there: a wrap of 0x5a8f60 for ArgPrm must build the missing stage, or the season never ends. (season_uefa_cup.c already wraps 0x5a8f60 for season_playoffs_advance, which returns 0 for an unmarked league.)
5. Wrap ArgPrm::add_result 0x405c50: a cup stage match (+0x42 >= the first cup index) must not go to the aggregate table. The original routes it there with +0x42 = 1 (0x405c7a..0x405c7e), so knockout results would count in the aggregate and in the relegation averages.
6. Index 0 stays the Clausura (0x405760, 0x405e65..0x405e8f) and index 1 the aggregate (0x405e93..0x405ebf, ArgPrm::update_relegation_averages 0x406570). The row loop of record_season_honours walks stages 0 and 1 with the league's +0x3e only (0x405e23..0x405ec8), so zone stages there are not touched.
7. +0x30 bounds the routing in add_result (0x685ddb), so zone routing needs +0x30 >= 5 from the season start.

The Apertura is the league object, which is complete in mid-season. The advance_stage path only runs when every built stage is also complete (the Clausura is not), so the Apertura knockout needs its own trigger: Inf. a wrap of update_for_date 0x6847c0 for the ArgPrm object, after the original, that builds the stage once the Apertura rows are all played (the test of 0x689b3d..0x689b64) and the stage is not yet in the list. The Clausura knockout and the one-season final can be built in the advance_stage wrap. The date templates must leave room for the Apertura knockout between the halves: not checked.

ArgPrm::process_promotion_relegation_chain 0x4067f0 and update_relegation_averages 0x406570 were not read for stage use beyond the header notes of season_sizes_arg.c.

## 3. Stage names on the screens

comp_screens_add_stage_selector 0x49d380: for i = -1..+0x30 it takes the comp or stage (0x48d200: -1 = the league itself, else stages[i]), maps its name id +0x3a with comp_util_get_parent_stage_id 0x4b01d0, and when the result is not -1 joins the following stages with the same parent id into one "Stage" item with one "Group" item each (0x49d4b5..0x49d5ec). A Cup_Stage (+0x43 = 1) with no parent id gives one item per round reached (+0xac), labelled with the stage name and the round code +7 of each round (0x49d6fa..0x49d7f1); a name id of 0 lists the rounds by their codes only (0x49d73b). Texts: comp_util_get_stage_name 0x4b4850 (out, size, id, style), through comp_util_format_stage_round_name 0x4bffc0.

Parent ids (0x4b01d0): 0x3e8..0x3fb, 0x41f..0x42e, 0x413, 0x414, 0x47b, 0x3fc -> 0x41c "Group Stage"; 0x43d..0x444 -> 0x449 "Phase 1 Group Stage"; 0x445..0x448 -> 0x44a "Phase 2 Group Stage"; 0x44d..0x450 -> 0x451; 0x3fd..0x406 -> 0xa; 0x407..0x40b, 0x472 -> 0x14; 0x40c..0x40e -> 0x82; 0x473..0x474 -> 0x410; 0x475..0x479 -> 0x47a; 0x47e..0x47f -> 0x1e; others -1.

Ids in use: ArgPrm create_schedule 0x404570 gives 0x434 "Opening Stage" for stage -1 (0x4045af), 0x435 "Closing Stage" for stage 0 (0x404d9f) and 0 for stage 1 (0x40552b). BelThird gives 0x41f "Group A" (league) and 0x420 "Group B" (stage 0) (0x421a34..0x421a50), periods 0x45d..0x462 (0x422d37..0x422d6b), 0x458 "Promotion Playoff" (0x4220b9), 0x452 "Relegation Playoff" (0x4222a1), and 0 for the title decider (0x421fc1).

Texts found (style 0, ids 0..0x4ff walked through 0x4b4850):
- Groups: 0x41f..0x42e "Group A".."Group P"; 0x43d/0x43e "Phase 1 Group A/B"; 0x445/0x446 "Phase 2 Group A/B"; 0x47e/0x47f "Third Round Group A/B"; 0x44d.. "Promotion Group A..D"; 0x432 "First Stage", 0x433 "Second Stage", 0x434 "Opening Stage", 0x435 "Closing Stage", 0x46b "Championship", 0x44b "Championship Group".
- Zones: only regional names (0x40f Carribean Zone, 0x410 Central American Zone, 0x436 Interior Zone, 0x437 Metropolitan Zone). No "Zone A" or "Zone B" text exists.
- Rounds: 0x14 "Second Round" (the World Cup's round of 16 uses it), 0x78 "Quarter Final", 0x82 "Semi Final", 0x8c "Third Place Playoff", 0x96 "Final", 0xa0 "Playoff", 0xaa "Playoff Final", 0xf0 "Promotion Final"; +3/+4 are the first/second legs, +1/+2 the replays. No "Round of 16" or "Last 16" text exists.

Suggestion: the Apertura zones as 0x43d/0x43e and the Clausura zones as 0x445/0x446 (two families, so two separate "Stage" items), or 0x41f/0x420 for both if they are not next to each other in the list. "Zone A" or "Round of 16" needs a wrap of 0x4b4850.

## 4. A 16-club knockout as a Cup_Stage

### 4.1 Cup_Stage::Cup_Stage 0x522e00 (thiscall, 15 arguments, ret 0x3c)

a1 parent (copies +0x49, +0x4a), a2 club count +0x3e, a3 Club* list, a4 round count +0x3c, a5 record +4, a6 round table (a4 x 0x68, copied: the caller frees its own, 0x522f11..0x522f32), a7 year +0x40, a8 stage index +0x44, a9 +0x42, a10 name id +0x3a, a11 +0x51, a12 tree mode (nonzero: Cup_Stage::map_fixture_tree 0x5241e0 at 0x523003; 0: Cup::map_fixture_tree 0x518790 at 0x52303d), a13 seed by list order (entry +4 = list index when a15 is 0, 0x522fcc..0x522fdd), a14 +0x28, a15 seed bytes array (entry +4 = a15[i], 0x522fa6..0x522fc6). +0x43 = 1 (0x522e87), vtable 0x969468. Team entries +0xa7: 6 bytes (+0 Club*, +4 seed, +5 result).

Callers: BelThird title decider 0x423710: (this, 2, clubs, rounds, +4, table, +0x40, 7, 1, name, 0x14, 1, 0, 0, 0), then Cup::set_up_round(0) 0x51c800 (0x42384f) and +0x30 = 7. WorldCup::create_knockout_stage 0x92d770: (this, 16, list, rounds, +4, table, +0x40, 7, 4, name, 0x14, 1, 0, 0, 0) (0x92d93d..0x92d95d). season_playoffs.c uses a12 = 0.

### 4.2 The round table

The EngFirst table (0x571980, stage 0) has 2 rounds (two-leg semi-finals, final); it cannot simply be extended. The class that already makes a 16-club bracket of single matches is WorldCup::create_schedule 0x92c290, stage 7 (0x92c7f1..0x92ccca): 5 rounds of 0x68 (0x208 bytes), out1 = 5, out2 = 0 (no name).

| Round | +7 code | +0xd | +0x18 teams | +0x1a ties | +0x1c new | +0x21 legs | venue (0x521eb0 last arg) |
|---|---|---|---|---|---|---|---|
| 0 | 0x14 | 0x383 | 16 | 8 | 16 | 1 | 1 (0x92c8aa) |
| 1 | 0x78 | 0x383 | 8 | 4 | 0 | 1 | 1 |
| 2 | 0x82 | 0x383 | 4 | 2 | 0 | 1 | 1 |
| 3 | 0x96 | 0x83 | 2 | 1 | 0 | 1 | 4 (0x92cb8c) |
| 4 | 0x8c | 0x3 | 2 | 1 | 0 | 1 | 0x80 |

+0x17 = 0xa, +0xf = 0, +0x20 = 0, +0x22 = 0, money +0x5c/+0x60/+0x64 = 0. 0x383 = ET 0x2 + penalties 0x1 + golden goal 0x80 + match-day list 0x100 + fixed bracket 0x200. The World Cup table also sets match-day lists (0x521f10, 0x92c8c0..0x92c958) and its dates are for June. A third-place round, when it is the last round (code 0x8c), is skipped in the tree links (0x52456d..0x524589) and in the selector (0x49d61a..0x49d668).

Recommended table for ArgPrm (built in runtime code, game malloc 4 x 0x68, every byte written, since operator new does not zero it): 4 rounds with codes 0x14, 0x78, 0x82, 0x96; +0xd = 0x203 (fixed bracket, ET, penalties; add 0x80 for golden goal, not 0x100 unless match-day lists are set); +0xf 0; +0x11 0; +0x20 0 (no replay); +0x21 1; +0x22 0; +0x0b = 1 on rounds 0..2 for the home rule (4.3); +0x18/+0x1a/+0x1c/+0x1e = 16/8/16/0, 8/4/0/0, 4/2/0/0, 2/1/0/0; +0x17 4 before the quarter-finals, 6 from them (as the cup classes; meaning unknown); draw date (0x521e60) of each round after the matches of the round before; match dates by 0x521eb0 with venue 0 for rounds 0..2 and 4 for the final. Then Cup_Stage with a12 = 1 and a15 = the seed of each club (its zone place), stage index = the next free slot, then Cup::set_up_round(0) 0x51c800, then +0x30 = that index.

Fixed bracket with a12 = 1 (Cup_Stage::map_fixture_tree 0x5241e0): new entrant i goes to tie i/2, home slot first, then away (0x524a20..0x524b5b), so the list order gives the round-0 pairs and home sides: list = 1A, 8B, 4B, 5A, 2A, 7B, 3B, 6A, ... (any order of pairs). When the next round has 0x200, the winner of tie t goes to tie t/2 of the next round, home slot if free, else away (0x5245bf..0x5245dc, 0x5246c0..0x52475c): the winner of the even tie is home. One exception: when the stage's +0x24 is not -1, a tie whose away club's ground belongs to +0x24 is swapped (0x524a71..0x524ac6). With a12 = 0 (Cup::map_fixture_tree 0x518790) the home or away slot of each entrant is drawn at random even with 0x200 (0x519302..0x519311).

### 4.3 Home to the better place

Cup::draw_round 0x51c410 skips the random draw when +0xd has 0x200 (0x51c5e4..0x51c5f1) and always calls Cup::set_home_teams_for_round 0x51e870 (round, -1) (0x51c5f6..0x51c5fb). With round +0xb bit 0x1, that function calls vtable +0x90 (home club, away club) for each tie with both clubs known and swaps the two slots when it returns the away club (0x51e8b2..0x51e9a6). Cup_Stage +0x90 is 0x522360, which returns the club whose division record +0x69 is lower, or 0 when equal (0x522360..0x522398): no effect for two clubs of one division. Safest hook: wrap 0x522360 for an ArgPrm knockout stage (ecx is the stage, 0x51e921) and return the club with the lower seed byte (entry +4 in +0xa7). A tie whose clubs are not yet known at the draw date is skipped (0x51e8ed..0x51e8ff), so each draw date must come after the round before is played. Inf.: draw_round runs for each round on its draw date (Cup::set_up_round 0x51c800 "draws the round if needed"; not traced in full).

### 4.4 Neutral final, extra time, penalties

Venue (round +0x13, set by cup_set_round_match_date 0x521eb0 last argument at 0x521f08, passed to the fixture manager 0x85c260 at 0x51ceb9..0x51cec9): 0 = the ground of the home club (club +0x69, 0x85c52c..0x85c537); 1 = no ground (stadium 0, id -1, 0x85c5d5..0x85c5dd); 4 = the national stadium of the nation (nation +0x80, 0x85c727..0x85c738; an England-only branch first, 0x85c389..0x85c3fa). EngFirst's play-off final and the World Cup final use 4. Inf.: for Argentina, 4 gives the Argentine nation's stadium if the nation record has one; if not, the code takes other paths (0x85c73a..0x85c74f) that were not traced.

Extra time and penalties on the day: +0xd = 3 (0x2 ET, 0x1 penalties), +0x20 = 0, +0x21 = 1 (doc 2026-10-17-cup-round-record.md, section 3). +0xf is only for the deciding match (second leg or replay), so 0 here.

### 4.5 Results and the winner

A cup stage fixture carries the stage index (+0x44 of the stage) in +0x42. The fixture comes to ArgPrm::add_result 0x405c50, which routes it to stages[+0x42] through 0x685d30 (0x685de9..0x685dfa; Cup_Stage vtable +4 = Cup::process_fixture 0x51a150), and then wrongly again with +0x42 = 1 (see section 2, rule 5).

Results of each club reach the league through Cup::add_team_result_news 0x519690: the object of the stage's record (+4, which is ArgPrm) gets vtable +0x44 (club, code, stage id, &status, fixture, arg) (0x5196d0..0x51970c). ArgPrm's +0x44 is the base 0x686940: code 5 -> 0x868c50, code 0 -> 0x869480 (.., 100), code 3 -> 0x86a1c0, others nothing; it does not look at the stage id. season_playoffs.c notes code 1 for a final's winner (EngFirst handler 0x576c50). Which codes a cup stage sends for each round: I don't know. Safe: wrap 0x686940 for ArgPrm and a stage id of a knockout, and return 0 with no news (or post your own).

Winner when complete: Cup::check_complete 0x521e00 returns 1 when +0xb1 is set (0x521e04..0x521e15). The winner is the team entry (+0xa7, 6 bytes, count +0x3e) with +5 == 1, runner-up +5 == 2, third +5 == 3 (Cup::record_season_honours 0x519a90, 0x519abe..0x519af2). Do not call the stage's own record_season_honours (vtable +0x30 = 0x519a90): it would write a cup history row under the ArgPrm record. The original never calls it for ArgPrm stages (only self +0x30 at 0x684a8f and stages[0] +0x30 at 0x405edc).

One-season final and runners-up match: two Cup_Stages of 2 clubs and 1 round each, as BelThird's decider (create_schedule(7) 0x421f95..0x422084: 1 round, code 0x96, +0x1a 1, +0x1c 2, +0x18 2, legs 2, +0xf 3) but with legs 1, +0xd 3, venue 4. Name ids: 0x96 as the round code; the stage name 0 (rounds listed by code) or 0x46b "Championship". There is no text for a runners-up match; 0xa0 "Playoff" is the nearest.

## 5. Honours of the halves

ArgPrm::record_season_honours 0x405ca0 (vtable +0x30, called once at the season end from update_for_date 0x684a8f):
```
update_relegation_averages(0)                      // vtable +0x34, 0x405cb0
for the last two average records (+0x18, count +0x34): club -> +0x40 (code 3 = relegated), +0x68 set_club_progress (-2, 0x13/0x14)   // 0x405cb3..0x405e1e
mark row +0x37 = 3 for those two clubs in the league, stages[0] and stages[1]   // 0x405e23..0x405ec8
League::record_season_honours(this)                // 0x405ed0: Apertura champion
stages[0]->vtable+0x30()                           // 0x405edc: 0x684640 for the Clausura
```
League::record_season_honours 0x684640: champion = row 0 club (+0xb1 +0), runner-up = row 1 (+0x3b), third = row 2 (+0x76) when +0x3e > 2 (0x68464b..0x684663); clears club +0x5b for clubs that point at this record (0x684666..0x6846ab); finds a nation or area (0x6846bd..0x684766); then 0x4afce0(this, champion, runner-up, third, edi) writes the history row (0x684768..0x684771). It reads the champion only from the club pointers of rows 0, 1 and 2.

The safest hook: the existing wrap of 0x684640 in season_sizes_arg.c (line 633). When this is the ArgPrm league (the Apertura) or its stages[0] (the Clausura) and the knockout of that half is complete: write the winner and the beaten finalist into the club pointers of rows 0 and 1 (+0 and +0x3b of +0xb1) (and a semi-final loser into row 2, or leave it), call o_00684640, then restore the three pointers. Restore at once: the rows' other fields stay as they are, and the table is not sorted in between. Find the clubs from the stage entries (+5 == 1, 2). For the one season with a single final, give the final's winner the title of the season in the same way on the league object and skip the Clausura call (the wrap already skips a Clausura with no dates).

Not known: what else reads row 0 of the Apertura or Clausura table as a champion (for example continental places or news of the champion through +0x44 code 0). I don't know.

## Main risks

1. The season never closes if +0x2c is above +0x30 + 1 and nothing builds the missing stage (0x6849d1..0x6849e3; ArgPrm advance_stage is empty).
2. Knockout results go to the aggregate table and the averages unless the add_result wrap skips them (0x405c7a).
3. A slot <= +0x30 that is null crashes the save (0x689926) and the destructor; the constructor's array holds 2 slots (0x4042c1..0x4042d2).
4. Stats are counted twice if a zone routing call keeps +8 and passes update_stats = 1 (0x685f15..0x685f4a).
5. Home sides in later rounds follow the bracket (winner of the even tie at home) unless the +0x90 wrap and +0xb bit 0x1 are used.
6. No "Zone A" and no "Round of 16" text exists in 0x4b4850.
7. The Apertura knockout needs room in the calendar between the halves and its own trigger, since advance_stage waits for the Clausura.
8. The cup codes that reach the base +0x44 handler 0x686940 are not known, so league news may appear for knockout results until it is wrapped.
