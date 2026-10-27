# Play-offs, splits and calendar changes in existing divisions (0.6.0 slice 19)

Date 2026-10-18. Original: `clean-3968/cm0102.exe` 3.9.68, virtual addresses. Spec: `docs/specs/2026-10-16-seasons-0.6.0-spec.md` (slice 19; Q247, Q249, Q272 to Q282, Q284, Q292, Q337, Q338). Brief: the build brief (kept outside the repo), slice 19. Code: `tools/seasons/encode_leagues.py`, `src/recomp_rt/win/season_league.h`, `season_league_table.c`. Tests: `tests/seasons/test_encode_leagues.py` (ctest `season_encode_leagues`), the rules report (`tests/seasons/rules_report_check.py`).

## The encoding

The play-off, split and calendar columns of the rules table are text. `tools/seasons/encode_leagues.py` reads the 2538 league rows (a numbered tier and a `comp_file` of the original) and writes one line per row to `2026-10-18-league-rules-encoded.tsv`:

- `po_up`: the places of the division in a play-off for a place in the tier above ("3-6", "2,3"). A club of the tier above in the same tie counts in `po_down` of its own row.
- `po_down`: the places of the division in a play-off to keep its place.
- `po_other`: a play-off that moves no club between tiers: `title` (a finals series, a championship of stage winners), `europe`, `tiebreak` (a match only when clubs are level).
- `split`: "after N: A+B, points P", or `stages` (Apertura and Clausura), `zones`, `groups`, or `gap`.
- `calendar`: `split` (July to May), `calendar` (one year) or `transition` (Ireland 2002/03, Russia 2010/11 and 2011/12).

How the tool reads a text: it splits it into clauses at ";" and at the end of a sentence. The places of the division are the places before the other side of a tie ("16th v 2. Bundesliga 3rd" gives 16), a range at the start ("3rd to 6th"), a "pos" list ("pos 2,3,12") or "top N". The words promotion and relegation give the direction. A clause with both words or neither goes by place: a range by its first place, a list place by place, the top half up and the bottom half down. A place in the top half of a top division is a title play-off. Words for Europe, a title or a tie-break make the clause `po_other`. 120 texts that this does not read right are encoded by hand in `BY_HAND` and `BY_START` (each read against its row), and 12 forms with no encoding are in `KNOWN_GAPS` with the reason. The tool stops on any other text, so a changed research text never goes unseen.

Result: 967 rows with play-off places, 169 with a split, 631 calendar-year rows. `2026-10-18-league-rules-gaps.tsv` lists 264 texts (of 261 rows) that the tool reads only in part or not at all, each with its reason:

| Reason | Lines |
|---|---|
| the places of each group or zone of a division of groups | 83 |
| the period winners of Belgium and Holland (the game has no periods) | 54 |
| a reserve team leaves its place to the next club (Spain, Portugal, Croatia) | 28 |
| a stage of groups (Brazilian Série C, see format) | 19 |
| places of the groups after a split (Croatia, Denmark, Greece, Japan 2026) | 18 |
| the winners of zones or series (a second stage that the game does not play) | 15 |
| the table of averages of Argentina | 11 |
| the Serie B gap rule (no play-off when the 3rd leads the 4th by more than 9 points) | 9 |
| the play-off places are not given in the text | 9 |
| the split cannot be read (no round count, or sizes in words) | 7 |
| other single rules (a licence, a final of tournament winners, a regional rule) | 11 |

## The rules report

The `playoffs` difference of `rules_report_check.py` compared whether a division has play-off places with whether the text is not "none". So the MLS, the A-League and the J-League finals, the European play-offs and the Italian tie-break all showed as differences, and a wrong count did not. It now compares the promotion play-off places (+0xbf) and the relegation play-off places (+0xc0) of the division with the counts of `po_up` and `po_down`. For a text that the tool reads only in part, it compares only whether the division has play-off places.

## The original's play-off code

Evidence: the instruction listing of the translated code (`build/recomp/gen/fn_*.c`) and the vtables of `clean-3968/cm0102.exe`. A research agent read the functions on 2026-10-18; its notes are summed up here with the addresses.

**Places and statuses.** `League::update_positions_and_status` 0x684cd0 (vtable +0x94) reads +0xbe..+0xc1 as one dword (0x684db0) and marks a zone for each place: +0xbe places from the top (zone 0), then +0xbf (zone 1), 0x684e11..0x684e7b; +0xc1 places from the bottom (zone 3), then +0xc0 (zone 2), 0x684e8e..0x684ef5. The status byte of a row (+0x37) becomes 5 champion (0x685149), 0 promoted (0x6851a5), 1 promotion play-off place (0x685213), 2 relegation play-off place (0x685281), 3 relegated (0x68525e) or 0xfe nothing to play for (0x685327). `league_promote_and_relegate` 0x689c80 moves rows of status 3 down and 0 or 5 up (0x689d1a..0x689e2c). Rows of status 1 and 2 never move.

**When a play-off is played.** `League::update_for_date` 0x6847c0 calls `check_complete(0, 0)` (0x684994). When the table is complete and the stage index +0x30 is below the stage count +0x2c less 1, it calls `advance_stage` (vtable +0x28) at 0x6849e0. The season of the competition ends only when every stage is complete (`check_complete(1, 1)`, 0x689bbc..0x689be8), so `start_new_season` waits for the play-off.

**England.** `EngFirst::advance_stage` 0x572ff0 calls `create_playoff_stage` 0x573080 when +0x30 becomes 0 (0x573073); EngSecond 0x5769e0 to 0x576a70, EngThird 0x578330 to 0x5783c0. The builders read +0xbf only as the array size (0x57309f) and the club count of the Cup_Stage (0x57317b, ctor 0x522e00). The clubs are fixed rows: 6th, 3rd, 5th, 4th (EngFirst 0x5730f3..0x573126, EngSecond 0x576ae4..0x576b17), 7th, 4th, 6th, 5th (EngThird 0x578434..0x578464). So +0xbf must stay 4 on these classes. The result handler +0x44 0x576c50 gives the final winner status 0 (0x576d9e) and the losers 0xfe (0x576dc2).

**The other builders** (each hard-codes its club count):

| Builder | Clubs | Reads the statuses | Note |
|---|---|---|---|
| `ScoFirst::create_playoff_group` 0x7ef340 | 2nd and 3rd, and the SPL club of the relegation play-off place | status 1 (0x7ef3e1), SPL status 2 (0x7ef48a) | only in the 1999 rules: +0xbf 2 when the year is 1999 (0x7eee5d, 0x7eee9a), +0x2c 1 (0x7ee022, 0x7eef75), ScoPrm +0xc0 1 (0x7f297a, 0x7f299b) |
| `BelSecond::create_playoff_group` 0x420230 | 4 period winners, topped up from the table (0x42028e) | status 1 | League_Stage of 4 |
| `BelThird::create_promotion_playoff_stage` 0x423880 | 3 + 3 period winners and the BelSecond 16th (0x423a80) | status 1 | Cup_Stage of 7 |
| `HolFirst::create_play_off_groups` 0x5f6720 | 6 HolFirst clubs (0x5f677e) and the HolPrm clubs of status 2 (0x5f68ad) | yes | two League_Stages of 4 |
| `ItaSerB::create_tiebreak_playoff_stage` 0x646bb0 | clubs level on points across a zone line (0x646c61) | no | a tie-break, not a promotion play-off |
| `SpaSecondB::create_promotion_playoff_groups` 0x857570 | the top 4 of each of the 4 groups (0x8576cf..0x8576fb) | no | four League_Stages of 4 |
| `SweSecond::create_relegation_play_off_stage` 0x892bb0 | 6 clubs of status 2 and 12 random clubs of the tier below (0x892c41..0x892d4e) | status 2 | Cup_Stage of 18 |
| `NorFirst::create_play_off_stage` 0x790e20 | the last own row of status 1 (0x790e97), the NorPrm row of status 2 (0x790f3c) | yes | Cup_Stage of 2 |
| `FinFirst::create_play_off_stages` 0x594080 | fixed rows (0x594295..0x5942ce) | no | Cup_Stage of 4 |
| `PolSecond::create_play_off_stage` 0x7cc290 | own status 1 rows (0x7cc30a), PolFirst status 2 rows (0x7cc34f) | yes | Cup_Stage of 4 |
| `IreFirst::create_promotion_playoff_stage` 0x635dc0 | 1 + 1 (0x635e37, 0x635edc) | yes | Cup_Stage of 2 |
| `NirFirst::create_playoff_stage` 0x78c0a0 | 1 + 1 (0x78c117, 0x78c1bc) | yes | Cup_Stage of 2 |
| `RusPrm::create_title_playoff_stage` 0x7eba90 | 1st and 2nd when level on points (0x7ebabb) | no | a title decider |
| `UsaMls::create_playoff_stage` 0x9094c0 | 3 division winners and the best 5 others (0x9095b7, 0x909692) | no | championship, no promotion |
| Italian C1 and C2 builder (C1a 0x64ba70) | fixed rows 2nd to 5th (0x64bb1a..0x64bb43) | +0xbf as the count (0x64bb99) | +0xbf must stay 4 |

**A class with no builder.** SpaFirst, SpaSecond, FraFirst, FraSecond, GreSecond, GerSecond, PorSecond and EngConf have `advance_stage` +0x28 = 0x5a8f60 (a bare return), the base +0x44 0x686940 and the base +0x94 0x684cd0. Places set at +0xbf or +0xc0 there only label rows with status 1 or 2 (no news: 0x686940 sends news for 5, 0 and 3 only). The rows do not move and the season completes. With the direct places cut to leave room for the play-off winner, one club fewer goes up or down. So a play-off cannot be added by a count: it needs a builder.

**The Scottish split.** `ScoPrm::update_for_date` 0x7f2d20 calls `ScoPrm::create_split_stages` 0x7f2d90 when +0x3c is 3 (0x7f2d2d) and every club has played 33 (0x7f2d56). Rows 0 to 5 and 6 to 11 each get a League_Stage of 6 clubs and 1 meeting (0x7f2e90..0x7f2ed7) with dates from vtable +0x3c with 0 or 1 (0x7f2e63, 0x7f2f41). The stages are deleted at once (0x7f2ef5, 0x7f2fd3): inference, the constructor adds the fixtures to ScoPrm's own list. The clubs stay in the one table, so points carry over. Then +0x3c becomes 4 (0x7f2feb) and the table order takes the comparator 0x7f3220 (top six above the bottom six; the load sets it again, 0x7f0bf1..0x7f0c08). The function is not virtual and needs the split dates of the class's +0x3c, so another class needs new code.

## What this slice applies

The play-offs that the original plays stay as they are in every season: the English Championship, League One and League Two (4 clubs), the Norwegian, Irish and Northern Irish ties, the Dutch, Belgian, Swedish, Finnish, Polish and Italian C1 and C2 play-offs. Slice 18 keeps the row's promotion and relegation counts: where the class has play-off places, the row's count may hold the play-off winner (`season_sizes_rules`).

New game code: the rule kind `playoffs` (`src/recomp_rt/win/season_playoffs.c`). It gives a promotion play-off of 4 clubs to a league whose class has no play-off code, when the row's play-off text gives 4 places in a row just below the direct places. Examples: the Segunda from 2010/11 and the Turkish second tier. Every other play-off of the table on a class with no builder plays in its nearest form: the play-off place goes to the next club of the table (Serie B 2004/05: 3 up directly). This is Max's recommended answer to the stop of the brief ("the nearest form only where the code cannot").

### The play-off kind

1. Which leagues: the division above relegates the direct places and one more (its +0xc1 is direct + 1, read through its record id +0x1c and g_comp_object_table 0xadadfc). The class's advance_stage (vtable +0x28) is the bare return 0x5a8f60 and its result handler (+0x44) is the base League one 0x686940. The row's `po_up` is 4 places that start at the row's `up` or `up` + 1 (the English part counts only the direct places in `up`, the others count the winner too). A text with "two winners" (Série B from 2025) is two ties, not one, so it is left out.
2. After each set_up_rules (vtable +0x8c; `season_sizes_rules` of slice 18 calls `season_playoffs_rules` at its end): +0xbe the direct places, +0xbf 4, +0x2c 1, +0x30 -1. A one-slot stage array (+0xc) is made when the class has none.
3. On the day the table is complete, `League::update_for_date` 0x6847c0 calls advance_stage (0x6849d1..0x6849e3). The wrap of 0x5a8f60 (`season_uefa_cup.c`) calls `season_playoffs_advance` first. It builds the stage as `EngFirst::create_playoff_stage` 0x573080 does: the round table of EngFirst +0x3c 0x571980 (stage 0, two-leg semi-finals and a one-match final), new dates with weekday -1 (0x521e60, 0x521eb0), the clubs 6th v 3rd and 5th v 4th (counted without a reserve side, below), `Cup_Stage::Cup_Stage` 0x522e00 and `Cup::set_up_round` 0x51c800. +0x30 becomes 0 only after the stage exists (save rule 0x689918).
4. The wrap of 0x686940 sends a stage result of such a league to the English handler 0x576c50. The final's winner gets status 0 (0x576d9e), so the promotion chain moves it up (0x689c80; for Spain through 0x850700, which calls 0x689c80 and reads the status there, 0x689cc1..0x689cc7).
5. The final. EngFirst's final is one match with venue flag 4 (0x572b49..0x572b5b); outside England `Stadium::set_fixture_venue` 0x85c260 gives such a match a neutral venue (0x85c5a7). That fits the Turkish second tier ("final for the third place", one match at a neutral venue). A row whose text says "two-legged final" (the Segunda from 2010/11) gets a two-leg final at the clubs' grounds: venue flag 0, the final round's +0x21 2 and +0x22 3 (as BraChampCup, 0x425f99), and the tie rule +0xd of the semi-finals without their fixed pairs flag 0x200. The model is the semi-finals of the same table (0x572b1c, 0x572b20, 0x572af3) and the two-leg final of `BraChampCup::create_schedule` 0x425c10 (+0x89 2 at 0x425f92, venue 0 at 0x425f38). Inference: +0x21 is the count of legs and +0x22 the days between them.
6. A reserve side. A club whose paired side (`club_get_linked_b_team` 0x540a50: the fixed key id pairs of Spain, Germany and Portugal, 0x540af6..0x540b10, 0x540b1b, 0x541871) plays in the division above (record id +0x1c) does not play the play-off: the next row takes its place, as the rows say ("reserve teams excluded, next club moves up"). Which side of the pair is the reserve one does not matter: a senior club's reserve side never plays in the division above it.
7. The play-off takes 12 days, 15 with a two-leg final. When the update day (+0x15) of the league's table entry comes that soon or sooner after the last match day, the first play-off place goes up directly (status 0) and +0x2c becomes 0, so the season closes.
8. The season change waits for the play-off. `game_season_update_pass` 0x5d5ae0 calls start_new_season only on the entry's update day (+0x15 against the day of the year at 0x5d5bc8) and only for a competition whose vtable +0xc is true (0x5d5ccb). For a league that is 0x48ce10: false when the league has a division above (+0x1c >= 0, 0x48ce13), else its own check_complete(1, 1) and that of each division below along +0x20 (0x48ce30..0x48ce48). So the pass starts only the top division, and a lower division that is not complete holds the whole chain. The game can move a play-off match: in a test run the Segunda final of 2011/12, due on 13 June 2012, was played after Spain's update day (20 June), and all three Spanish leagues stayed in 2011 (year +0x40) in 2012/13. So an entry with a marked league that is not complete (its check_complete(1, 1), vtable +0x10: the table and every stage, 0x689bbc..0x689be8) waits. The hold around the pass (`season_playoffs_hold` and `season_playoffs_release`, called by the wrap of 0x5d5ae0 in `season_change.c`) gives the entry no update day on its day, and gives it today on the first later day that the league is complete. A waiting entry is one past its update day with a marked league whose year +0x40 is before the game year, so no state is saved. A first form read the entry's season year +0x35 instead, and a game loaded in the wait never changed its season (inference: a load sets +0x35 from the game year, as `create_spanish_comps` does at 0x83761b). At most 60 days; then the change comes as the original's. Cause of the late final: unknown (inference: the matches of Spanish clubs move around Spain's Euro 2012 matches).
9. Save and load: the base League save 0x6891c0 and load 0x687b10 write and read +0x2c, +0x30 and each stage (Cup_Stage type 1: 0x523eb0, 0x5230e0, 0x523380). The places +0xbe..+0xc1 are in the save (0x689589..0x689592).
10. News: one sentence in the first season of a play-off ("now has promotion play-offs").
11. Base game and `RECOMP_SEASON_CODE=off`: the kind runs only in a season game (`season_game_changes`), so no league gets +0x2c 1 and both wraps pass to the original. Test switch: `RECOMP_PLAYOFFS=off` (same-save run `playoffs-off`).

### Leagues left out

- A league whose division above relegates fewer clubs than the direct places and one more. The winner then stays down. Observed in a first build, which played every play-off of this shape: the Conference 2002/03 (League Two keeps 1 down, its floor in slice 18) and the Greek second tier 2010/11 (GrePrm 2 down; 3 clubs had status 0 and only the 2 direct clubs went up). Inference: the chain 0x689c80 moves as many clubs up as come down. Decision (recommended answer, gap 1 of the build): such a league keeps its class's places and has no play-off, as in the original. The places of the division above are those of its last set_up_rules.
- J2 and the Argentine second tier: no set_up_rules wrap of slice 18 (`season_size_wraps.c`), so no play-off.

### Known limits

- Spain, B teams in the direct places. A reserve side in a direct place stays the original's case: `SpaFirst::resolve_b_team_clashes` 0x850970 (called at 0x84fed1) reads the linked club of each row (0x540a50 at 0x850a09) and its division (club +0x57) and group (club +0x60) (0x850a3a, 0x850b11, 0x850cb9, 0x850b30..0x850b6b), and for a clash sets a row's status to 0xfb (0x850ab6). The play-off places skip a reserve side (point 6 above).
- Spain, a level tie. In the Segunda a tie level after both legs goes to the higher-placed club, with no extra time or penalties. The kind plays the tie rule of the semi-finals of EngFirst's table (+0xd), the nearest form.
- A stage object that a class does not free at start_new_season stays in the heap (one small object per season). A free could free it twice, so the kind does not free it.

### Review round notes (slice 19 review, notes 4 to 9)

- `balanced()` (note 4) reads the division above from its last set_up_rules. The order in which `start_new_season` runs the divisions of a chain was not read: 0x48ce10 starts the top division, and whether its start_new_season calls the lower divisions after its own set_up_rules is unknown. If the lower one runs first, a season in which the division above changes its relegation count plays one season with the decision of the season before. The new game pass of `season_sizes.c` runs `season_playoffs_rules` again, so a new game is right.
- After WAIT_DAYS (note 5) the change comes without the stage being complete, and the chain of 0x48ce10 stays one season behind while a lower division is not complete (the test above). The comment in the code says so now. The cap is not reached by a 12-day play-off.
- The news (note 8) compares only the shape of the season before, not `balanced`. A league whose play-off starts because the division above changed its relegation count gets no news. Not built: the kind has no memory of the decision of the season before.
- Fixed in the code: the buffers of competitions of an entry hold 256 (the cap of `season_change_entry_comps`), not 64 (note 7); the table and the list are freed when the stage object cannot be made (note 6); the header names the callers of advance_stage (note 9).

## Design: a split on another class

Call 0x7f2d90 with the other league as `this` (it is a thiscall of a League) only when its vtable +0x3c answers stage 0 and 1 with the split dates, its table holds 12 clubs and its +0xa3 takes the comparator. Each class needs its own date branch, so this is new code per class. The rows with "points halved" (Belgium Play-off I to 2019/20, Poland 2013/14 to 2016/17, Turkey 2011/12) also need a write of the points at the split.

## Design: a calendar move

A calendar move changes when a league table entry has its update day (+0x15) and its start and end dates (+0x2a, +0x32..+0x37, +0x39..+0x46, set by `key_nation_build_table` 0x667150 from the game year; calendar-year leagues take year + 1). The fixture template of the class (+0x3c) holds the dates of its own calendar, so a move needs a new template too. The year tests of `finance_get_days_to_season_start` 0x59b56b, `MATCH_PLAYER::setup` 0x6eb28d (Japan) and `FIXTURE_MANAGER` 0x5aee52, 0x5af218..0x5af2b9 (`2026-10-17-first-season-branches.md` table 3) follow the calendar. In `season_change.c` the season of a change comes from its date (a change from 1 October starts season Y + 1), so a move from a calendar year to July (Ireland 2003, Argentina 2016/17) gives one season with two changes, and the news of that season must come once. Not built.

## Decisions for Max (taken with the recommended answer)

- **A play-off that the code cannot hold** (every new play-off of the brief: the Championship of 6 from 2026/27, the National League from 2002/03 and 6 from 2017/18, Serie B from 2004/05, Segunda from 2010/11, Ligue 1 from 2016/17, the 3rd v 16th tie of Germany, the Scottish play-offs from 2005/06 and 2013/14, Q284 and Q337): the nearest form, the play-off place goes to the next club of the table. The exception is a promotion play-off of 4 clubs just below the direct places on a class with no builder (the Segunda from 2010/11, the Turkish second tier): the play-off kind plays it.
- **A play-off whose winner the division above cannot take** (the Conference: League Two relegates 1; the Greek second tier: GrePrm relegates 2): no play-off, the class's places as in the original (gap 1 of the build, recommended answer, 2026-10-19).
- **No time for a play-off:** when the season change is 12 days or less after the last match day (15 with a two-leg final), the first play-off place goes up directly.
- **The final** (slice 19 review 2, should-fix 2): two legs at the clubs' grounds where the row says "two-legged final" (the Segunda from 2010/11), else one match at a neutral venue (the Turkish second tier). A Spanish tie level after both legs is not given to the higher-placed club: the tie rule of EngFirst's semi-finals plays (nearest form).
- **Reserve sides** (slice 19 review 2, should-fix 3, the recommended fix): a club whose paired side plays in the division above does not play the play-off, and the next row takes its place.
- **The Championship play-offs of 6 from 2026/27:** the builder takes exactly 4 clubs, so 3rd to 6th play as before.
- **Splits** (Scotland as before; Belgium from 2009/10 with Play-off I and points halved, Denmark from 2016/17, Greece from 2019/20, Poland 2013/14 to 2019/20, Croatia, Wales from 2010/11, Northern Ireland from 2008/09, Korea, Finland): one table with the row's club count and places, as slice 18 gives.
- **Calendar moves** (Ireland 2002/03 to 2003, Russia 2010/11 and 2011/12, Argentina 2014/15, 2016/17 and 2020/21, Japan 2025/26, the MLS 2026/27): each league keeps the calendar of its class. Russia plays its 2011/12 long season as two normal seasons, with the counts of the rows.
- **Apertura and Clausura** (Argentina to 2012/13) and the Brazilian double round robin from 2003 (slice 18 doc: one meeting at 0x42a5af, 0x42cb83): as the class plays them.

## Tests

Commands run from the repo with `G=/Users/max/Projects/cm0102/clean-3968` and `D=/Users/max/Projects/cm0102/test-fixtures/databases`. Group (b) is `--nations all,-argentina,-australia,-belgium,-brazil,-croatia,-denmark,-finland,-france,-germany,-greece,-holland,-italy,-japan,-northern_ireland,-norway`.

- `python3 -I tests/seasons/test_encode_leagues.py` (ctest `season_encode_leagues`): PASS. It checks the play-off shapes of the brief (the Championship play-offs of 4 and of 6 from 2026/27, the National League, Serie B from 2004/05, the Segunda from 2010/11, Ligue 1 from 2016/17, the German 3rd v 16th, the Scottish play-offs from 2005/06 and 2013/14, an Irish "pos" list), the splits of Scotland, Belgium, Poland and Denmark, the calendars of Argentina, Ireland and Russia, that an unknown text stops the tool, and that the outputs are up to date.
- Rules report, group (b), `--year 2011 --last 2012 --database "$D/September 2011 ODB"`: both seasons close. The Turkish second tier plays its play-off in both seasons: 3 clubs go up and 3 come down. The Segunda's final of 2011/12 comes after Spain's update day: the entry waits 4 days ("entry 30 changes its season on day 174 of 2012"), then SpaFirst gets 3 clubs and all Spanish leagues reach 2012. Without the wait, all Spanish leagues stayed in 2011; with `RECOMP_PLAYOFFS=off` they reach 2012.
- Save and load in the wait: a group (b) 2011 game saved on day 172 of 2012 (`RECOMP_SAVE_AFTER_DAYS=341`, the Segunda final not played) and loaded: the final is played after the load and Spain changes its season on day 174.
- Repeatability: two equal group (b) 2011 games give equal saves on day 172 of 2012 (inside the play-off wait).
- England with Greece, `--year 2010 --last 2011 --database "$D/September 2010 ODB"`: both seasons close. Before the balance rule the Greek play-off was played and its winner stayed down (GrePrm relegates 2). Now the Greek second tier has no play-off.
- England with the Conference, `--year 2002 --last 2003 --extra conference`: both seasons close, no Conference play-off, the champion goes up.
- Group (b) from 2010 (September 2010 ODB) stops on 30 July 2010 at an error box with the kind on (Database.cpp 13471) and off (fix_man.cpp 3272). Not caused by this slice; slice 18 saw the same box in that group.
- `--start-only --update-expected` of every `-start` expected file of 2002 to 2020 (37 files) on the merged build: PASS.
- Same-save set (Q342) on the final build: `smoke_run.py build/cm0102_recomp $G --same-save --jobs 2 --runs again,off,season-code-off,playoffs-off,kinds-base,rules-on,rules-off` with `--save start.sav`, `--save prematch.sav`, a new game and `--save yeovil.sav --days 7`: PASS (every run equal to normal; rules-on of the new game differs only in the accepted parts).
