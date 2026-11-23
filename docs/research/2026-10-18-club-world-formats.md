# The formats of the FIFA Club World Championship (0.6.0 slice 26, part b)

Date 2026-10-18. Research only. Original: `clean-3968/cm0102.exe` 3.9.68, virtual addresses. Evidence: capstone disassembly of the clean exe (the `cm0102` venv). "Inference" marks a statement that the code does not prove. "Unknown" marks what I could not find. Months in date calls are 0-based (`Date::Date` 0x549f70 takes day, month, year, weekday). Round records, round ids and match flags use the words and fields of `2026-10-17-cup-round-record.md` (sections 1 and 2).

The question: a design that changes only World_Club_Champ (constructor 0x929140, vtable 0x97119c) in a season game, so that it plays the format of the rules table:

| Seasons | Clubs | Real format (club-world.tsv) |
|---|---|---|
| 2005/06, 2006/07 | 6 | Quarter-finals of 4 clubs, UEFA and CONMEBOL winners enter in the semi-finals, final, third place and fifth place matches |
| 2007/08 to 2019/20, 2021/22 to 2023/24 | 7 | Play-off (host champion v OFC winner), then as above. No fifth place match in 2007/08 and 2023/24, and penalties with no extra time in the place matches from 2011/12 |
| 2020/21 | 6 | Knockout of the continental winners and the host champion (OFC withdrew), February 2021 |
| 2024/25 | 32 | 8 groups of 4, top 2 to a one-match round of 16, quarter-finals, semi-finals, final, no third place match, 14 June to 13 July 2025 |

## 1. Recommended design

Two new forms, both built from classes and functions of the original. Every wrap calls the original (`o_<addr>`) in the base game and in a season with the original form, so the base game is the same game (Q152).

**Form K, the knockout (6 or 7 clubs, 19 seasons).** On the start date the runtime picks the clubs in a fixed order, gives the parent League a table with no clubs, and makes one `Cup_Stage` (0x522e00) at stage slot 0 with stage index 2. The round table holds the rounds with byes: new entrants in later rounds through round fields +0x1c and +0x1e, which every Cup of the original supports (section 3.2). The third place match is the last round, with round id 140 (0x8c). The original Cup code routes the semi-final losers to it (section 3.3). Leave out the fifth place match: no class of the original has one (section 3.4), and the history records places 1 to 3 only.

**Form G, the groups (32 clubs, 2024/25).** The parent is group A, seven `League_Stage` groups sit at stage slots 0 to 6 and the knockout `Cup_Stage` at slot 7, as in the Copa Libertadores. The runtime calls the Libertadores stage builders 0x4c37a0, 0x4c3b10 and 0x4c3d10 and its advance_stage 0x4c3a80 on the World_Club_Champ object. They read only fields of the League base and the object's own vtable +0x3c (section 4).

**Build Form K first.** It covers 19 of the 20 seasons of the table and needs fewer wraps. Its one untested path is a parent League with no clubs (section 6, risk 1). Make a probe run of one 7-club edition before the rest of the build.

### 1.1 Functions to wrap, smallest first

| Order | Site | Mechanism | Form | What it does |
|---|---|---|---|---|
| 1 | 0x929c39, 0x929c3b (month and day of the start date) | `PATCHES` in `tools/recomp/seasons.py` (already `rt_club_world_month`, `rt_club_world_day`) | K, G | The start date of the form's season: the pick and draw day, about one week before the first match (section 5.4) |
| 2 | 0x929390 `create_schedule` (vtable +0x3c) | `RT_WRAP` | K, G | Returns the round table of the form: K stage 2 (the knockout), G index -1 to 6 (single round-robin of 3 rounds) and 7 (knockout of 4 rounds). `o_` for the original form |
| 3 | 0x40f590 (vtable +0x30, the history thunk, shared by 4 vtables: 0x967644, 0x967c74, 0x96b430, 0x9711cc) | `RT_WRAP` with a test of the vtable 0x97119c | K, G | Forwards to the knockout stage of the form (slot 0 for K, slot 7 for G) in place of slot 2 (0x40f593) |
| 4 | 0x92aba0 `set_predicted_positions` (vtable +0x5c) | `RT_WRAP` | K, G | It reads 12 entrants by fixed offsets (0x92ac41..0x92ad1e). A loop over the form's entrants with `Comp_Stats::set_club_final_position` 0x4a2540 |
| 5 | 0x92a6b0 `update_for_date` (vtable +0x14) | `RT_WRAP` | K, G | On the start date (the same test as 0x92a73d..0x92a765): the picks, the stages, +0x45 = 1, then `o_00092a6b0`, which then skips its own start branch (0x92a755) and runs `League::update_for_date` 0x6847c0 |
| 6 | 0x929dd0 `advance_stage` (vtable +0x28) | `RT_WRAP` | G | Calls the Libertadores advance 0x4c3a80 (+0x30 + 1, at 7 the knockout 0x4c3d10) |
| 7 | 0x929c70 (vtable +0x44), 0x9298a0 (+0x48), 0x92ad50 (+0x68) | `RT_WRAP` | G | They test the stage index for `< 2` (groups) and `== 2` (knockout) (0x929c74, 0x9298a6, 0x92ad5e). Give them 0 for stages 0 to 6 and 2 for stage 7, then call `o_` |
| 8 | Every address the runtime calls | `CALLBACKS` in `tools/recomp/callbacks.py`, and the runtime file in `CALLBACK_FILES` (`season_kind_club_world.c` is not in that list now) | K, G | First eight bytes in section 7 |

No `INSNS` entry is needed beyond the start date. The fixed sizes of the original (12 entrants at 0x92a8bb, 3 stage slots at 0x9291e2, 16 knockout clubs at 0x4c3d30 and 0x4c3e7d) stay, and the wraps take other paths around them.

## 2. The present structure (question 1)

### 2.1 Fields of the parent

| Field | Meaning | Evidence |
|---|---|---|
| +0xc | Array of stage pointers, 3 slots (operator new 0x944e46 of 0xc bytes) | 0x9291d7..0x9291f3 |
| +0x2c | Stage count, 3 | 0x9291e2 |
| +0x30 | Current stage, -1 at a new edition | 0x9291df, 0x929da3 |
| +0x3e | Club count of the parent's own group, 4 | 0x929aae |
| +0xb1 | The parent's own table (0xec bytes = 4 rows of 0x3b) | 0x929aa9..0x929abc |
| +0xba | The parent's own round table, from vtable +0x3c with stage -1 | 0x929bdd..0x929c2c |
| +0xee, +0xf2 | The entrant list (12 pointers) and its count, freed after the draw | 0x92a8bb..0x92a8cd, 0x92a862..0x92a877 |
| +0x3c | Each pair meets once in a group | 0x929be1 |
| +0xbe | One club goes on from each group (inference: the qualifier count that 0x4c3d10 reads) | 0x929c03 |
| +0x45 | Drawn, set to 1 on the start date | 0x92a87d, cleared at 0x929db9 |
| +0xb6, +0xb8 | Start date: day of the year, year offset | 0x929c4b, 0x929c52 |
| +0x49, +0x4a | 7 substitutes named, 3 used | 0x929c1c, 0x929bc4 |
| +0x50 | 0xff: no ruling body | 0x9291c3 |

### 2.2 `create_schedule` 0x929390 (vtable +0x3c)

Thiscall, `ret 0x10`: (char stage, short *rounds, short *code, int *is_league). It returns a round table from operator new.

- **Stage -1, 0 and 1** (`cmp cl, 2; jge` at 0x9293a2): a League round table of 0xc3 bytes (0x9293db). `*rounds` = 3, `*code` = 0x3e9 + stage, `*is_league` = 1 (0x9293b8..0x9293e0). Three dates with `league_round_set_date` 0x68a160: 29 July, 2 August, 5 August (0x929435, 0x929460, 0x92948c), all with year offset 0. The pairings of each round are set with 0x68a1c0, and they differ by stage (stage -1 at 0x9295b7, stage 0 at 0x92953d, stage 1 at 0x9294c4).
- **Stage 2** (0x929632): a Cup round table of 0x138 bytes, three rounds of 0x68 (0x929655). `*rounds` = 3, `*code` = 0, `*is_league` = 0 (0x929645..0x92965f). The rounds:

| Round | Draw, match | Round id (+0x07) | +0x09 | +0x0d flags | +0x17 | +0x18 teams | +0x1a ties | +0x1c new | +0x1e first | +0x20 replays | +0x21 legs | +0x60, +0x64 prize |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 semi-finals | 7 Aug, 9 Aug (0x9296e9, 0x929700) | 0x82 (130) | 0 | 0x83 | 0x17 | 4 | 2 | 4 | 0 | 0 | 1 | 0, 0 |
| 1 final | 10 Aug, 12 Aug (0x92975f, 0x929776) | 0x96 (150) | 1 | 0x83 | 0x17 | 2 | 1 | 0 | 0 | 0 | 1 | 6,000,000, 5,000,000 |
| 2 third place | 10 Aug, 12 Aug (0x9297f5, 0x92980c) | 0x8c (140) | 2 | 0x83 | 0x17 | 2 | 1 | 0 | 0 | 0 | 1 | 4,000,000, 3,000,000 |

Writes: round 0 at 0x929705..0x92974b, round 1 at 0x929780..0x9297da, round 2 at 0x929814..0x929884. Flags 0x83 are extra time, penalties and golden goal (cup-round-record section 2). Every round is one match.

- **Stage 3 or more**: returns 0 (0x929632 `jne 0x9296cc`). This matters for a Form G call with stages 3 to 7 (section 4).

### 2.3 The three group builders and the update

`update_for_date` 0x92a6b0 (thiscall, `ret 8`: Date *today, int arg2). Before the start date it goes straight to `League::update_for_date` 0x6847c0 (0x92a718). On the start date, with +0x45 = 0 and arg2 = 0 (0x92a73d..0x92a765), it runs, in this order:

1. `pick_entrants` 0x92a8b0 (0x92a76d), section 5.
2. vtable +0x5c `set_predicted_positions` 0x92aba0 (0x92a776). It sorts the 12 entrants by 0x4b07d0 (0x92ac27..0x92ac30), gives Comp_Stats +8 the predicted places 1, 2, 3, 4, 5, 5, 7, 7, 7, 10, 10, 10 with 0x4a2540 (0x92ac43..0x92ad22), and shuffles the list with 0x4afb40 (0x92ad36).
3. 0x929aa0 (0x92a77b): sorts the entrants with the compare 0x92b400 (qsort 0x945333 at 0x929b49), sets +0x3e = 4, makes the table +0xb1, and adds entrants 0, 3, 6 and 9 to the parent with `League::add_club` 0x687430 (stride 12 bytes = 3 pointers at 0x929b66..0x929b6e). Then 0x684230. So the parent itself is group A.
4. `League::create_fixtures` 0x6835c0 (0x92a7b6) and the table sort 0x6827d0 (0x92a7f2) for the parent.
5. 0x929e60 (0x92a85d): two `League_Stage` objects, group B with entrants 1, 4, 7, 10 (0x929ec5..0x929edf) at stage slot 0 (0x929f7e), group C with entrants 2, 5, 8, 11 (0x929fcb..0x929fe5) at slot 1 (0x92a08a). Each gets its round table from vtable +0x3c with stage 0 or 1 (0x929ef1, 0x929ff8). After each, +0x30 = 0 or 1 and 0x684230 on the stage (0x929fc1..0x929fc6, 0x92a103..0x92a10d).
6. Frees +0xee, sets +0xf2 = 0 and +0x45 = 1 (0x92a862..0x92a87d).

### 2.4 How a stage is made

**`League_Stage::League_Stage` 0x68b100**, thiscall, 22 arguments (`ret 0x58` at 0x68b4c1). From the calls at 0x929f1c..0x929f66 and 0x4c3be9..0x4c3c3b, first argument first: parent, club count, club array, times each pair meets (World_Club_Champ 1 at 0x929f5e, Libertadores the parent's +0x3c at 0x4c3c2b), record (+4), round table, round count, +0xc2, +0xc3, +0xc4, &+0xc5, &+0xbe, year (+0x40), stage index, code, +0x51, 2, 0, +0xd9, -1, 0, 2. It stores the club count at +0x3e (0x68b141), the meetings at +0x3c (0x68b14d), the stage index at +0x44 (0x68b206) and a round table at +0xba (0x68b288). It copies the round table (the callers free theirs at 0x929f80 and 0x4c3c5c). The meaning of the arguments after the stage index: unknown, I copied them from the callers.

**`Cup_Stage::Cup_Stage` 0x522e00**, thiscall, 15 arguments (`ret 0x3c` at 0x5230b9), on the Cup base 0x518640 (0x522e25), vtable 0x969468 (0x522e80). First argument first:

| Arg | Stored | World_Club_Champ (0x92a350..0x92a36d) | Libertadores (0x4c3e62..0x4c3e7f) |
|---|---|---|---|
| 1 parent | reads its +0x49, +0x4a (0x522ea5, 0x522eb5) | the object | the object |
| 2 club count | +0x3e (0x522f38) | 4 | 16 |
| 3 club array | the club list +0xa7, 6 bytes per club (0x522f95..0x522f9d) | 4 clubs | 16 clubs |
| 4 round count | +0x3c (0x522e9d) | 3 | 4 |
| 5 record | +4 | +4 | +4 |
| 6 round table | copied into +0xa3, count x 0x68 bytes (0x522ec2..0x522f32) | stage 2 table | stage 7 table |
| 7 year | +0x40 | +0x40 | +0x40 |
| 8 stage index | +0x44 (0x522e5b) | 2 | 7 |
| 9 | +0x42 | 2 | 2 |
| 10 code | +0x3a | 0 | from vtable +0x3c |
| 11 | +0x51 | 0x14 | 0x14 |
| 12 | 0: `Cup::map_fixture_tree` 0x518790 (0x52303d), else 0x5241e0 | 0 | 0 |
| 13, 15 | club list +4 bytes (0x522fbf..0x522fe3) | 0 | 0 |
| 14 | +0x28 prize fund | 0 | 0 |

Then the builders start the first round. World_Club_Champ calls `Cup::set_up_round` 0x51c800 with round 0 (0x92a414). The Libertadores calls `Cup::draw_round` 0x51c410 with round 0 (0x4c3f1e).

### 2.5 From the groups to the knockout

`League::update_for_date` 0x6847c0 on the parent (+0x44 = 0xff):

- It runs vtable +0x14 of every stage from 0 to +0x30, with no null test (0x6848e5..0x684901).
- When the parent has a table (+0xb1, 0x68495e), its own `check_complete(0, 0)` sets +0xdd (0x684994), and every stage from 0 to +0x30 is complete (0x6849aa..0x6849cf), it calls vtable +0x28 `advance_stage` when +0x30 < +0x2c - 1 (0x6849d1..0x6849e0). Else, once (+0x4c = 0), the season end: `Club_History::record_comp_season` 0x442e80 (0x6849fa), honours (vtable +0x30, 0x684a8f) and records 0x4a19f0 (0x684a99).
- With +0xb1 = 0 the whole block is skipped (0x68495e `je 0x684b15`), so a parent with no table never advances and never ends.

`advance_stage` 0x929dd0: +0x30 + 1 (0x929e4c). When the new value is 2 it calls 0x92a140 (0x929e4f..0x929e54). At the last stage it shows the error box (0x929ddd..0x929e32).

0x92a140 (the knockout builder):

1. The array of 4 clubs: the leader of the parent table (row 0 of +0xb1, 0x92a1d5..0x92a1e2), of stage 0 (0x92a1e4..0x92a1f1) and of stage 1 (0x92a1f4..0x92a202).
2. The best runner-up: a temporary `League_Stage` of 3 clubs (0x92a21c..0x92a259) gets row 1 (+0x3b) of the three tables (0x92a26f..0x92a2eb), the sort 0x6827d0 (0x92a2f0) ranks it, and its row 0 goes into slot 3 (0x92a2f5..0x92a303). Then it is deleted (0x92a30e).
3. vtable +0x3c with stage 2 (0x92a323), `Cup_Stage` (0x92a370) into stage slot 2 (0x92a388), `set_up_round(0)` (0x92a414), +0x30 = 2 (0x92a41f).
4. 2,500,000 (0x2625a0) to each club that goes out in the groups: rows 2 and 3 of each table and the runners-up that are not the best (0x92a426..0x92a68e).

### 2.6 What `all_stages_complete` and the history need

`all_stages_complete` 0x48ce10: +0x1c is -1 (0x48ce13), vtable +0x10 `check_complete(1, 1)` is true (0x48ce25), and every object of the +0x20 chain is complete (0x48ce30..0x48ce54).

`League::check_complete` 0x689ad0 on the parent needs:

- a table (+0xb1 not 0, 0x689add), +0xb5 = 0 (0x689ae9) and +0x45 = 1 (0x689af1)
- every row of the parent table with played (row +0x11) at least +0xa7 matches, where +0xa7 = (+0x3e - 1) x +0x3c when it is -1 (0x689b1d..0x689b64)
- with arg 1 and the parent (+0x44 = 0xff): every stage from 0 to +0x30 complete, with no null test (0x689bbc..0x689bdd), and +0x30 >= +0x2c - 1 (0x689bdf..0x689be8)

The history, vtable +0x30 = 0x40f590: `mov eax, [ecx+0xc]; mov ecx, [eax+8]; jmp [vtable+0x30]`. It forwards to the stage in slot 2, so the knockout must be in slot 2 or the thunk must be wrapped. The Cup version, `Cup` slot 12 0x519a90, takes the clubs with fates 1, 2 and 3 from the club list (+5 of each 6-byte entry, 0x519aca..0x519aeb) and writes them with 0x4afce0 (0x519c40, inference: the history row).

The other slots that know the stage index (the Cup sends its +0x44 to the parent through `g_comp_object_table[record id]` vtable +0x44, 0x5196d0..0x51970c):

| Slot | Function | Test |
|---|---|---|
| 17 (+0x44) staff records | 0x929c70 | stage < 2 with fate 6 or 0xfe (group), stage == 2 with fates 1, 2, 3 (0x929c74..0x929d35) |
| 18 (+0x48) fate news | 0x9298a0 | stage < 2 (0x9298a6), stage == 2 (0x929962) |
| 26 (+0x68) stage codes in Comp_Stats | 0x92ad50 | stage < 2 maps group places (0x92ad5e), else raw codes |

### 2.7 Season change

`start_new_season` 0x929d40: `League` reset 0x687970, free +0xba, `Comp_Stats::archive_season_and_reset(1)` 0x4a1c50, delete the stages from 0 to +0x30 with vtable 0 (0x929d7d..0x929d9c), +0x40 + 2 (0x929d9e), +0x30 = -1 (0x929da3), new hosts 0x5fab70, +0x45 = 0, then `set_up_rules` (vtable +0x8c, 0x929dc0). It does not free +0xc and does not change +0x2c. The destructor 0x9292d0 also frees the stages from 0 to +0x30 (0x929320..0x92933f) and then frees +0xc with 0x9452ca (0x929349). So a new stage array must come from the game's operator new 0x944e46.

Save and load: `League::load` 0x687b10 reads +0x2c, +0x30 and +0x44 (0x688e11..0x688e35) and allocates +0xc with +0x2c slots when +0xc is 0 (0x688e3a..0x688e5b). The League base constructor sets +0xc to 0 (0x682291), and the World_Club_Champ constructor calls the load (0x9291ca) before it makes the 3-slot array (0x9291e9). So a save with another stage count loads with that count (inference from the code, not run).

## 3. A knockout with byes (question 2)

### 3.1 How a Cup_Stage holds its rounds

A `Cup_Stage` is a Cup (base 0x518640, all Cup code applies). It holds its own copy of the round table at +0xa3 (+0x3c rounds of 0x68 bytes), the club list at +0xa7 (6 bytes per club: pointer, +4 seed byte, +5 fate) and, after `map_fixture_tree` 0x518790, one tie array per round at +0xad (0x35 bytes per tie, 0x5187a3..0x5188ae). Per round, the fields are those of `2026-10-17-cup-round-record.md` section 1: draw date (+0x00, year offset +0x02), match date (+0x03, +0x05), round id +0x07, round index +0x09, draw rules +0x0b, match flags +0x0d, deciding match flags +0x0f, +0x11, venue flags +0x13, +0x17, teams +0x18, ties +0x1a, new entrants +0x1c, first entrant +0x1e, replays +0x20, legs +0x21, days to the second leg +0x22, prizes +0x5c, +0x60, +0x64. The setters are `cup_set_round_draw_date` 0x521e60 (writes +0x00 and +0x02 at 0x521ea6..0x521ea9) and `cup_set_round_match_date` 0x521eb0 (+0x03 at 0x521ef6).

Extra time and penalties: flag 0x2 extra time and 0x1 penalties in +0x0d of a one-match round. World_Club_Champ writes 0x83 (with golden goal 0x80). The Libertadores writes legs 2, +0x0d = 4 and +0x0f = 1 in every knockout round, the final too (0x4c343e..0x4c3466, 0x4c34bb..0x4c34eb, 0x4c3558..0x4c3591, 0x4c3611..0x4c3643).

### 3.2 Clubs that enter in a later round

Every Cup supports it. `map_fixture_tree` 0x518790 first links the winners of each round to the ties of the next (0x518b8c..0x51913d, it reads the next round's ties at +0x82 = +0x68 + 0x1a). Then, for each round with +0x1c new entrants (`cmp word ptr [eax+ebp+0x1c], 0` at 0x519265), it writes club list indexes from +0x1e on into the empty tie slots of that round (0x51941f..0x519504). The draw `Cup::draw_round` 0x51c410 waits (moves its draw date one day) while a tie of the round has an empty slot or a slot with no club yet (0x51c47c..0x51c4b1, 0x51c4e9..0x51c5cf). The competition screen lists the living clubs with no tie in the round as "Teams given bye" (string 0x99c33c, 0x493b4d..0x493b82).

Examples in the original:

- English FA Cup, `create_round_table` 0x56f320: round 1 has 0x34 new entrants from index 0x38 (0x56f4ac, 0x56f4bc). Round 2 and round 3 also have new entrants from index 0x6c (0x56f5b7..0x56f5be, 0x56f66c..0x56f675).
- UEFA Cup: the Champions League writes its third qualifying round losers into UEFA Cup slots from count - 0x18 (0x585290, `2026-10-16-sweep-europe.md` section 2.3). Inference: those slots are the new entrants of a later UEFA Cup round.
- Asian cups: not read for this question.

The code that adds a club to a later round is `map_fixture_tree` 0x518790 at 0x519265..0x519504, from the club list in the order that the builder gives it. A builder puts the clubs that enter late at the club list indexes of that round's +0x1e.

### 3.3 The third place match

The Cup code has one: a last round with round id 140 (0x8c).

- `map_fixture_tree` leaves it out of the winner tree (0x518b9f..0x518bab) and writes +0x5b of the rounds before it (0x519549..0x519570, meaning unknown).
- `Cup::process_fixture` puts the loser of a tie of round (count - 3), the semi-final, into the tie of the last round (0x51b129..0x51b158). So the round order must be: semi-finals, final, third place. The loser of the round before the final is not routed anywhere else.
- In a 0x8c round the winner gets fate 3 and the loser fate 4. In other rounds the final gives fates 1 and 2 (0x51af63..0x51af9c, 0x51b2d3..0x51b31f).
- The update `Cup::update_for_date` 0x51f890 handles the 0x8c round at its draw (0x51f99e, 0x51face).

The round id 140 is written by these create functions: African Cup of Nations 0x401925, Asian Club Championship 0x40b0d3, Asian Cup Winners' Cup 0x40d1d0, Asian Cup 0x40f0bf, Confederations Cup 0x58b98c, Copa America 0x5e0ee4, 0x5e4da6 and 0x766204 (class not read), Oceania Club Championship 0x797627, Oceania Nations Cup 0x79927a, Olympics 0x79fe14, World_Club_Champ 0x929822, World Cup 0x92cc4a. The 2002 World Cup third place has flags 0x83 (0x92cbb9, `2026-10-16-sweep-international.md` section 7).

### 3.4 A fifth place match

I found none. The Cup code routes losers only from round (count - 3) to a last round with id 140 (0x51b129..0x51b158), and no other compare of a round id in the Cup code (0x518000..0x523800) names a place match (0x82, 0x96, 0x8c only: 0x519ff4, 0x519ffe and the 0x8c sites above). A fifth place match needs the two quarter-final losers in a separate stage. The parent makes a new stage only in `advance_stage`, when every current stage is complete (section 2.5), and the quarter-final losers are known before that. So it needs new code in the Cup process path. The history records places 1 to 3 only (0x519aca..0x519aeb). Recommendation: leave it out, and say so in the rules report notes.

## 4. The 32-club shape (question 3)

### 4.1 Conmebol_Liber

Constructor 0x4c2f50, vtable 0x968bf4, League base. +0x2c = 8 stage slots (0x4c2fbc..0x4c2fdf), +0x3e = 0 and +0xb1 = 0 until the draw (0x4c302b..0x4c302f).

| Function | What it does | Can a World_Club_Champ object call it |
|---|---|---|
| `create_schedule` 0x4c31c0 (vtable +0x3c) | Stage 0 to 6 (`cmp al, 7` at 0x4c31ca): a League table of 6 rounds (`*rounds` = 6 at 0x4c3208), a double round-robin. Stage 7: a Cup table of 4 rounds (0x4c3383), all two legs (section 3.1): ids 0x14, 0x78, 0x82, 0x96 (0x4c3430, 0x4c34ad, 0x4c3541, 0x4c35f8) | No. Two legs and 6 group rounds. The World_Club_Champ wrap of 0x929390 gives its own tables |
| 0x4c37a0 | Parent = group A: +0x3e = 4, table +0xb1, entrants 0 to 3 with 0x687430, then 0x684230 (0x4c37ae..0x4c3870) | Yes. It reads +0xee only |
| 0x4c3b10 (stage i, `ret 4`) | Group i + 1 at stage slot i from entrants 4(i+1) to 4(i+1)+3 (0x4c3b81..0x4c3ba7). Round table from the object's vtable +0x3c with stage i (0x4c3bba). `League_Stage` with the parent's +0x3e and +0x3c (0x4c3c2b, 0x4c3c31). +0x30 = i (0x4c3ce1) | Yes, with the wrap of 0x929390 for stages 3 to 6 (the original returns 0 there, section 2.2). Needs +0xc with at least 7 slots |
| 0x4c3d10 | Knockout: 16 slots (0x4c3d30). The top +0xbe of the parent (0x4c3da7..0x4c3dd3), then the top +0xbe of stages 0 to 6 with 0x6826d0 (0x4c3ddd..0x4c3e23), in the order A1, A2, B1, B2 and so on. vtable +0x3c with 7 (0x4c3e38), `Cup_Stage` with 16 clubs and stage index 7 (0x4c3e62..0x4c3e82) into slot 7 (+0xc + 0x1c, 0x4c3e9e), `draw_round(0)` 0x51c410 (0x4c3f1e) | Yes, with +0xbe = 2 (World_Club_Champ has 1, 0x929c03) and +0xc with 8 slots |
| 0x4c3a80 `advance_stage` | +0x30 + 1, at 7 calls 0x4c3d10 (0x4c3afb..0x4c3b04) | Yes, with +0x2c = 8 |
| 0x4c3f40 | The picks (called at 0x4c447f) | Not read. Inference: South American picks, of no use here |
| 0x4c4410 `update_for_date` | Draws on the start date only when arg2 is not 0 (0x4c442d..0x4c4434), the opposite of World_Club_Champ (0x92a75e). Then 0x4c3f40, 0x4c37a0, 0x6835c0, 0x6827d0(0), 0x4c3b10 for 0 to 6, vtable +0x5c (0x4c447d..0x4c45cf) | No. Use the World_Club_Champ wrap with the same sequence |
| 0x637750 (slot 12) | History from slot 7 (`mov ecx, [eax+0x1c]` at 0x637753). Shared by 5 vtables | A model for the wrap of 0x40f590 |
| 0x4c4080 (slot 17) | Stage 7 tests, and the write of the Libertadores winner into the Intercontinental Cup (0x4c41a6..0x4c41e1) | No. Wrap 0x929c70 with a stage map |
| 0x4c3680 (slot 18), 0x4c4610 (slot 26) | Stage 7 tests (0x4c3684, 0x4c4626), no key id read | Inference: usable, but a stage map in the World_Club_Champ wraps is smaller |

What differs for a World_Club_Champ object: +0xbe (1, needs 2), +0xc (3 slots, needs 8), +0x2c (3, needs 8), vtable +0x3c (no tables for stages 3 to 7), the history thunk (slot 2, needs 7), slots 17, 18 and 26 (stage 2, needs 7), and the update's arg2 test. The asserts of these functions name `conmebol_liber.cpp` (0x9a7de4). Inference: harmless, the boxes come only on an allocation failure.

The 2025 rounds: groups one round-robin (each pair meets once, the World_Club_Champ +0x3c = 1, 3 rounds), knockout one match with extra time and penalties (flags 0x3, no golden goal), no third place round. 0x4c3d10 gives 16 clubs to 4 rounds with a random draw. The real bracket was fixed (A1 v B2). Inference: +0x0d bit 0x200 gives a fixed tree in club list order (cup-round-record section 2). Not checked.

### 4.2 The World Cup

The World Cup builders cannot be called. Its knockout builder 0x92d770 calls the ruling body of object +0x50 at the start (0x92d78e..0x92d7a0), and World_Club_Champ has +0x50 = 0xff (0x9291c3), so the index is -1. Its sizes and kick-off tables are fixed for 32 nations (`2026-10-16-sweep-international.md` section 11: 0x92bffa, 0x92ea9d, 0x92ccd0, 0x92d770).

## 5. The picks (question 4)

### 5.1 `pick_entrants` 0x92a8b0

+0xf2 = 12, +0xee = 12 pointers (0x92a8bb..0x92a8cd). The host: `HostCountry::find_hosts` 0x5fa730 with the record id, +0x40 and the default flag 1 (0x92a944..0x92a961). With no host row it shows the error box and takes a random nation (0x92a96a..0x92a9d9). Then 12 calls of `pick_entrant` 0x92ae60:

| Slot | Continent (arg 1) | Nation (arg 2) | Record override (arg 5) | Address |
|---|---|---|---|---|
| 0, 1 | Africa 0x9cfa08 | -1 | -1 | 0x92a9f2, 0x92aa14 |
| 2, 3 | Asia 0x9cfa0c | -1 | -1 | 0x92aa37, 0x92aa5a |
| 4 | Europe 0x9cfa10 | -1 | -1 | 0x92aa7d |
| 5 | Europe | -1 | UEFA Cup 0x9cf6f0 | 0x92aaa6 |
| 6, 7 | North America 0x9cfa14 | -1 | -1 | 0x92aaca, 0x92aaee |
| 8 | Oceania 0x9cfa18 | -1 | -1 | 0x92ab12 |
| 9, 10 | South America 0x9cfa1c | -1 | -1 | 0x92ab36, 0x92ab5a |
| 11 | -1 | the host nation | -1 | 0x92ab7c |

### 5.2 `pick_entrant` 0x92ae60

Thiscall, `ret 0x14`: (continent, nation, Club **picked, short picked_count, record override). It returns a club that is not yet in `picked[0..count)`.

- **Continent** (arg 1 not -1): the record at league table entry +0x1e of the continent's entry (0x92ae86..0x92aeb4), else the Asian Super Cup 0x9cf7d0 for Asia, the Oceania Club Championship 0x9cf7cc for Oceania, the CONCACAF Champions Cup 0x9cf72c for North America (0x92aebf..0x92aee5). Arg 5 replaces it (0x92aef7..0x92af0e). When the continent has a selected nation (0x6699d0, 0x92af42) or +0x40 is 2001 (0x92af4e), it takes place 0 of the latest history row (0x4afd80, 0x92af59), or place 1 when place 0 is already picked (0x92af7d). When that is also picked, or there is no record, it takes a random club of the continent: clubs with reputation (+0x80) at least 5000, down in steps of 1500 to above -2000 until it has 6 (0x92afb1..0x92b033), sorted by 0x4b07d0 and shuffled among the top three times min(4, n) by 0x4afbd0 (0x4afc02..0x4afc57), then the first that is not picked (0x92b058..0x92b093).
- **Host** (arg 1 = -1): the league table entry of the nation (entry +0 = nation, 0x92b14a..0x92b171), the first competition of its array for which 0x4b0700 is true (0x92b1e9..0x92b21c), then place 0 of its latest history row, place 1, place 2 when the earlier ones are picked (0x92b257..0x92b2a3). The history is used when the nation of the record has byte +0x11c set or +0x40 is 2001 (0x92b23a..0x92b251). Else a random club of the nation, as above (0x92b173..0x92b34f). With no club at all, the error box at 0x92b38c, then any random club (0x92b3d4).

### 5.3 What each form needs

| Form | Order of the entrant list (club list order of the Cup_Stage) | With `pick_entrant` |
|---|---|---|
| 6 (2005/06, 2006/07) | QF clubs: AFC, CAF, CONCACAF, OFC (indexes 0 to 3, round 0 +0x1e = 0). SF entrants: UEFA, CONMEBOL (indexes 4, 5, round 1 +0x1e = 4) | One call per continent. No host call |
| 6 (2020/21) | QF: host, AFC, CAF, CONCACAF. SF: UEFA, CONMEBOL | Host call (arg 1 = -1), no OFC call |
| 7 | Play-off: host, OFC (0, 1). QF: AFC, CAF, CONCACAF (2 to 4). SF: UEFA, CONMEBOL (5, 6) | One call per continent plus the host |
| 32 | Groups by fours (A1..A4, B1..B4 and so on, as 0x4c37a0 and 0x4c3b10 read them) | 12 Europe, 6 South America, 4 Asia, 4 Africa, 4 North America, 1 Oceania, 1 host. The second call of a continent gives the runner-up, every later call a random club of the top reputations (section 5.2). The real slots (the Champions League winners of 2021 to 2024, then the ranking, at most 2 per nation) need a history reader of more than the latest row. Which runtime code can read older rows: unknown |

The pick must not go through `set_predicted_positions` 0x92aba0 (it shuffles with 0x4afb40 at 0x92ad36) or 0x929aa0 (it sorts with 0x92b400). The wraps of section 1.1 replace both for Form K and Form G, so the list keeps the builder's order. A 32-club group draw by pots with one club per confederation in a group (two for UEFA) is new runtime code. Inference: it should take its random numbers from the game's 0x90cfc0, as the original picks do.

### 5.4 When to pick

The picks happen on the start date (0x92a76d). They read the latest history row of each continental record, so a continental competition that ends after the start date gives last season's winner (inference). The real editions: December of +0x40 (2005 to 2019 and 2023), February of +0x40 + 1 (2020/21, 2021/22, year offset 1), 14 June to 13 July of +0x40 + 1 (2024/25). The end dates of the game's continental competitions in each month: unknown. Recommendation: put the start date one week before the first match.

## 6. Risks

1. **Form K: a parent League with no clubs.** The parent must have a table (+0xb1 not 0) to advance and to end (0x68495e, 0x689add). With +0x3e = 0 the row loops do nothing (0x689b43, 0x442fbd), but `League::update_for_date` reads row 0 +0x11 (0x68484e), so the table needs one zeroed row. What `League::create_fixtures` 0x6835c0, the table sort 0x6827d0, 0x682e10 (pays the prize fund +0x28 by place, 0x682ed9..0x682f1f) and the competition screens do with 0 clubs: unknown. Do not call 0x6835c0 in Form K. The other way, the entrants in the parent table with +0x3c = 0 meetings (complete at once by 0x689b27..0x689b36), puts table places of 0 matches into the club history (`record_comp_season` 0x442e80 walks the parent table, 0x442fbd..0x442fe6, inference). Make a probe run first.
2. **Form K: stage slot 0, stage index 2.** The parent loops over stages 0 to +0x30 with no null test (0x6848ee, 0x689bc5), so a knockout alone must sit in slot 0 with +0x2c = 1 and +0x30 = 0. The Cup_Stage argument 8 sets +0x44 = 2, so slots 17, 18 and 26 work unchanged. Only the history thunk 0x40f590 reads the array slot (0x40f593).
3. **Form G: the stage array.** Replace +0xc with 8 slots from 0x944e46 and free the 3-slot array with 0x9452ca before the build. Set +0x2c = 8 and +0xbe = 2 (the wrap of `set_up_rules` or the start branch). `start_new_season` keeps +0xc and +0x2c (section 2.7), so a later form must set them again.
4. **2024/25 and the update day.** The edition plays 14 June to 13 July 2025, and entry 2 updates on 20 June. The object is not complete then, so the pass does not call `start_new_season` in 2025 (0x5d5ccb), and the next call is on 20 June 2026. The table holds no edition after 2024/25, so the object then waits for NEVER (slice 26 part a). The matches of June 2025 count in game season 2024 and those of July 2025 in 2025 (slice 16 rule). The effect on the clubs' own season change in late June: unknown.
5. **Fixture clashes.** `FIXTURE_MANAGER::find_fixture_clashes` 0x5ad780 treats this competition by its id (0x5ad92e). The original plays it in July and August. In December the clubs have league matches. What the clash code does then: unknown.
6. **Hosts.** `HostCountry::redraw_world_club_and_confed_hosts` 0x5fab70 gives a random nation (`2026-10-16-sweep-international.md` section 6), not the real hosts. A host nation with no club in the database gives the error box at 0x92b38c. From 0.6.2 (open item 125) the season code gives each edition the host of its row (`real_host` in `season_kind_club_world.c`). When the AFC winner of a knockout with a host place is a club of the host (2007, 2008: Japan), the host place goes to the AFC runner-up, place 1 of the record's latest history row (`runner_up`). The 2022 case (Wydad Casablanca, so Al Ahly) cannot happen in the game: it has no African club competition, so the CAF pick is a random club.
7. **The rules report.** It reads +0x3e of the parent (the parent group): 4 in the original and in Form G, 0 in Form K. It cannot show 6, 7 or 32. The report needs another count for this class, for example the club count of the knockout stage plus the group clubs (inference).
8. **Round names.** The round id selects the name (comp_util ids: 0x14, 0x78 QF, 0x82 SF, 0x8c third place, 0x96 final). Which id names a play-off: unknown.
9. **Prize money.** 0x92a140 pays 2,500,000 to each club out in the groups (0x92a43f). The Libertadores knockout builder pays none. Form G gets no group prize unless the wrap pays it. The round prizes come from the round table (+0x60, +0x64).
10. **Same game.** Every wrap must call `o_` when the season has the original form (the base game, a 2001 start with changing rules off, plays the 12-club form in 2003 and every odd year).

## 7. Addresses that a build calls (first eight bytes for `callbacks.py`)

| Address | First 8 bytes | Use |
|---|---|---|
| 0x92ae60 | b844210000e8a6a7 | `pick_entrant` |
| 0x522e00 | 6aff683b78950064 | `Cup_Stage::Cup_Stage` |
| 0x51c800 | 6aff68d976950064 | `Cup::set_up_round` |
| 0x51c410 | 64a1000000006aff | `Cup::draw_round` |
| 0x4a2540 | 81ec000200005657 | `Comp_Stats::set_club_final_position` |
| 0x4afb40 | 56570fbf74241033 | shuffle of the entrants |
| 0x5fa730 | 8b01535556668b71 | `HostCountry::find_hosts` |
| 0x90cfc0 | 51568b74240c85f6 | random number |
| 0x944e46 | ff350830de00ff74 | operator new |
| 0x9452ca | 558bec51568b7508 | free |
| 0x4c37a0 | 81ec04020000568b | Form G: group A |
| 0x4c3b10 | 64a1000000006aff | Form G: groups B to H |
| 0x4c3d10 | 6aff68be6a950064 | Form G: knockout |
| 0x4c3a80 | 8b512c8b413081ec | Form G: advance_stage |
| 0x6835c0 | 6aff6881c9950064 | Form G: `League::create_fixtures` of the parent |
| 0x6827d0 | 83ec4c535556578b | Form G: table sort |
| 0x521e60, 0x521eb0 | 8b4c24148b442418 | round dates (if the runtime builds round tables through them) |
| 0x68a160 | 8b4c24148b442418 | League round date |
| 0x68a1c0 | 81ec000200005356 | League round pairings |

Wrapped (`RT_WRAP`): 0x929390 (81ec000200005356), 0x92a6b0 (6aff680b5f960064), 0x92aba0 (81ec00020000568b), 0x40f590 (8b410c8b48088b11), and for Form G 0x929dd0 (8b512c8b413081ec), 0x929c70 (8a44240c3c027d4e), 0x9298a0 (8a44240c56573c02), 0x92ad50 (8b4424048b490853).

## 8. Unknowns

- The behaviour of `League::create_fixtures` 0x6835c0, 0x6827d0, 0x682e10 and the screens for a parent with 0 clubs (risk 1).
- The meaning of `League_Stage` arguments after the stage index, of round fields +0x17 (World_Club_Champ 0x17, Libertadores 7 and 8) and +0x5b (written at 0x519570).
- Whether +0x0d bit 0x200 gives a fixed bracket in club list order.
- The round id of a play-off round name.
- How a runtime picker reads continental winners older than the latest history row.
- Which code writes league table entry +0x1e (also open in `2026-10-18-club-world-idle.md`), so whether the Asian pick is the Asian Club Championship or the Asian Super Cup.
- The end dates of the game's continental competitions, for the start date of section 5.4.
- The fixture clash handling for a December edition (risk 5).
- 0x4c3f40 (the Libertadores picks) and the classes of 0x5e4da6 and 0x766204: not read.
