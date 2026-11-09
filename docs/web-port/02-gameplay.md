# Gameplay, simulation, and management specification

This chapter covers CM 01/02: Bosman Edition at repository revision `cf2e8fa5f4cc816f252611a0e9c81e66d088358e`. It separates executable rules available in this repository from original-game algorithms that still require recovery. It is a contract for implementation and investigation, **not a claim that all original gameplay has been recovered**.

Evidence labels used throughout:

- **[S] Source:** implementation read in this checkout. A wrapper establishes what the wrapper does, not the internals of the original function it calls.
- **[R] Research:** inherited research or generated function-map hypothesis, not independently established by reading the original instructions in this investigation.
- **[B] Binary:** instructions/constants directly read from the hash-matched original; static evidence only unless a completed execution probe is explicitly reported.
- **[M] Manual:** documented player-facing behavior in the supplied 67-page English manual; the manual predates some official-patch/Bosman behavior and is not a source for unstated formulas.
- **[D] Design:** required structure or proposed behavior of the web implementation. This is not evidence about the original.
- **[U] Unresolved:** exact algorithm, constants, ordering, or representation cannot yet be implemented faithfully from the evidence examined.

No complete game or match runtime comparison was performed for this chapter. Bounded isolated training-load and match-helper comparisons passed, as recorded in sections 5.5 and 6.4. Existing repository test source is evidence of intended checks, not evidence that those other checks passed in this investigation. Original addresses below identify analysis targets or callbacks. They do not imply that their bodies were recovered here unless explicitly labeled binary evidence.

During the investigation a matching original was found at `/Applications/CM 01-02 Bosman Edition.app/Contents/Resources/game/cm0102.exe`, SHA-256 `0c78e7da6d0bcb4603fa179449bbe6d1b6b5e1c627a22c445d14d489bd9bff86`. The binary observations in sections 3.3, 5.5 and 6.4 use that artifact. Its surrounding installed Bosman application is version 0.5.1; the newer wrappers in this checkout remain separate evidence. The first sandboxed isolated-function Unicorn attempt terminated with SIGILL (exit 132); a subsequently permitted execution completed all 1,024 training-load cases with no mismatch. This does not validate any other training behavior or the full application.

## 1. Fidelity boundary

The hand port under `src/cm3` and `src/si` is not a complete football simulation. Most original decisions are supplied by code generated from the user's original `cm0102.exe`. Bosman wrappers enhance or repair selected paths. A JavaScript implementation of the wrappers alone cannot play the complete game.

There are three distinct deliverables:

1. **Web shell with retained simulation:** Angular presents the game and Node.js owns sessions while an engine adapter executes the recompiled game. This can retain unrecovered behavior, provided the adapter, scheduling and original artifacts are available and validated.
2. **Behavioral port to TypeScript:** recovered algorithms replace original functions incrementally, behind the same adapter, with differential tests.
3. **Replacement football simulator:** newly designed formulas replace unknown ones. This is a different behavioral target and must be explicitly identified as such; it cannot satisfy exact CM 01/02 fidelity merely by using the same screens and player names.

The default implementation path should preserve the original engine boundary until each replacement subsystem passes its compatibility gate. Never fill an unresolved transfer acceptance formula, shot probability, training gain or board confidence formula with a plausible football heuristic and report it as recovered logic.

## 2. Complete gameplay surface and recovery coverage

The following is a requirements inventory. Candidate systems named from maps remain research leads. Every row needs both a player-visible flow and background processing, even when the current implementation milestone uses the retained engine.

| ID | Subsystem | Required behavior to preserve | Current evidence / gap |
|---|---|---|---|
| G01 | New game | Chosen season, data set, changing rules, active leagues, database size, manager creation, club/nation employment, initial injuries/contracts/fixtures | Setup and season wrappers available; original complete world initialization [U] |
| G02 | Game calendar | Morning/afternoon/evening, human turns, due tasks, match days, competition updates, weekly/monthly/yearly work | Date arithmetic [S]; top-level original loop order and gates [B], inner subsystem rules [U] |
| G03 | Squad | First/reserve teams, player availability, squad status, shirt/selection slots, captain, staff roles | Assistant selection [S]; ordinary selection and eligibility delegated to original [U] |
| G04 | Tactics | Formation, player positions, team/player instructions, with-ball/without-ball shape, set pieces, ordered takers, substitutions, presets and user tactics | Position masks and assistant scoring [S]; instruction effects [U] |
| G05 | Match setup | Fixture rules, lineups, officials, weather, venue, attendance, watched/quick mode, pre-match state | Callback sites and research leads; original preparation [U] |
| G06 | Full match engine | Movement, possession, passes, crosses, challenges, shots, saves, goals, fouls, advantage, offside, restarts, injuries, fatigue, cards, ratings | Grid guard and some metadata [S]; decisive algorithms [U] |
| G07 | Match periods | Regulation time, added time, half-time, extra time, golden-goal or other competition-specific conditions, penalty shootout, abandonment if supported | Original maps [R]; exact transition predicates [U] |
| G08 | Quick/background match | Lightweight results and simulated statistics; correct rule application; statistics and downstream effects | Separate candidate path in map [R]; formulas [U] |
| G09 | Match output | Commentary, team/player statistics, incidents, result, records, player condition and morale, news | Speed wrapper [S]; event taxonomy and effect order [U] |
| G10 | Fitness/injury | Condition versus match fitness/fatigue, recovery, training load, injuries/illness, treatment, recurrence, medical reports and retirement | Initialization guard [S]; rates and recovery formulas [U] |
| G11 | Training | Named schedules, category intensities, coach assignment, position/side retraining, progress and attribute changes | Categories, intensities and load formula [B]; progress coefficients and caps [U] |
| G12 | Player lifecycle | Ability/potential, development and decline, birthdays, retirement, regeneration, conversion to non-player roles | Candidate regeneration and staff data paths [R]; all generation distributions [U] |
| G13 | Scouting/knowledge | Assignments, region/competition/player scouting, reports, shortlist, filtering, attribute visibility, knowledge expiry | Assistant respects original visibility getter [S]; discovery/expiry rules [U] |
| G14 | Transfers | Cash/loan/exchange offers, clauses, negotiations, bids, offers on hold, permits, eligibility, transfer completion/cancellation | Loan fix [S]; offer state table, evaluations and fees [U] |
| G15 | Contracts | Wage, bonuses, length, role/status, clauses, renewal, expiry, free transfers, compensation, staff/player dual roles | Contract/loan indices [S]; negotiation and legal predicates [U] |
| G16 | Finances | Cash, wage and transfer budgets, transaction ledger, installments/clauses, income/expenditure, debt and board restrictions | Database fields/maps [R]; posting amounts and cadence [U] |
| G17 | Board/manager career | Confidence, expectations, requests, ultimata, appointment, job offers, dismissal, resignation, reputation | Manager map [R]; scores and decisions [U] |
| G18 | Relationships/morale | Selection reactions, transfer reactions, unhappiness, discipline/fines, disputes, missed training, media | Related maps [R]; exact causes, magnitudes, expiry [U] |
| G19 | AI managers | Recruitment, squad assessment, tactics, selection, rotation, substitutions, staff hiring and firing | Season rotation safeguard [S]; decision policy [U] |
| G20 | National teams | Eligibility, call-ups, withdrawals/replacements, release to clubs, tournaments and dual club/nation management | Cross-subsystem requirement; see competition chapter; selection/eligibility policy [U] |
| G21 | Competition aftermath | Tables, qualification, relegation/promotion, discipline, prize money, awards, records/history, season changes | Season wrappers and tables; see competition chapter |
| G22 | Holiday/delegation | Holiday settings, delegated transfers/team choices, return date, holiday until next match | Next-match return and faster-days wrappers [S]; ordinary holiday decision logic [U] |
| G23 | Persistence/history | Save/reload all managers and world state, resumable match/scheduled actions if supported, historical results and records | Requires cross-chapter schema and original save recovery |
| G24 | Multiple human managers | Independent knowledge and decisions, shared world, turn blockers and match control | Native human-slot references [S]; complete hot-seat/network semantics [U] |

## 3. Simulation execution contract

### 3.1 State and command ownership [D]

The server owns authoritative game state. Angular submits intentions; it never sends a proposed new bank balance, transfer outcome, injury duration, match result, ability, or random roll. Every command uses the architecture chapter's canonical envelope with `gameId`, `managerId`, `commandId`, `expectedRevision`, a typed payload, and a recorded logical decision point. Authentication must establish control of that manager independently of the body field.

At most one authoritative mutation runs for a world at a time. This includes matches in the same world: independent worker threads must not consume a shared original random stream in nondeterministic completion order. Different worlds may run concurrently. A task resumes from its persisted logical cursor, not from the browser's reconnect time.

The transport reports `queued`, `committed` or `rejected`, as defined in the architecture chapter. A committed domain operation may produce a `pendingDecision` blocker for a manager; that is game state, not an alternative transport status. Rejection includes a stable reason and any changed precondition the client must refresh. A repeated `commandId` returns the first committed outcome. A stale revision produces no random draws and no domain mutation. Save/export and view queries must not quietly advance the world.

### 3.2 Time model [S/D]

The hand port stores a date as zero-based day-of-year, year and leap flag. `src/si/date.c:4–40` uses month starts `[-1,30,58,89,119,150,180,211,242,272,303,333]`, with the leap-year table incremented after February. Months passed to the constructor are zero-based; day-of-month is one-based. `is_leap` uses the Gregorian 4/100/400 rule. The original function addresses represented by this source include `0x549f70`, `0x54a130`, `0x54a3f0`, `0x54a2d0`, `0x54c5b0` and `0x54c8e0`.

Preserve date semantics at the engine boundary. The web domain may expose `YYYY-MM-DD`, but use a pure game calendar; browser timezone, daylight saving and the server wall clock must not change fixture dates or a player's age. Date-only values are not JavaScript instants.

`date_to_weekday` selects the nearest requested weekday, allowing motion backward, and wraps differences greater than three days (`src/si/date.c:88–101`). It is not “next weekday on or after.” The weekday formula uses integer division truncated toward zero and signed remainder. `date_add_days`/`date_sub_days` take signed 16-bit counts; the source explicitly notes the `-32768` mutual-recursion edge case (`:141–154`). The new domain API should reject unsupported counts before conversion rather than reproduce unbounded recursion; compatibility adapters should record this guard as a divergence on invalid input.

### 3.3 Day progression [B/R/U]

Inherited research in `docs/research/2026-10-14-assistant-and-days-inventory.md`, section 2a, describes original `0x5cccd0` as a three-slice loop:

1. Build the slice's match-day object.
2. Offer match-day screens to relevant human managers.
3. Play matches.
4. Run competition day functions and staff-record updates; show results.
5. Apply date/slice-dependent daily work and selected periodic work.
6. Run human turns, including news and team screens.
7. Move to the next slice; after three slices advance the date.

The following direct instruction inspection supersedes that older outline where they differ. Full disassembly of the loop portion beginning at `0x5cd2b4` is retained in [original-day-loop.txt](evidence/original-day-loop.txt). The containing original function is `0x5cccd0`; generated mechanical C is in `build-web-evidence/original/gen/fn_013.c` (function starts at line 25727 at generation time). Generated translation coverage is not equivalent to validated semantic understanding.

**Top-level observed ordering:**

1. `0x5cd2b4–0x5cd2f9`: allocate/create a match-day object through `0x6b3c10(date,slice)`, call `0x6b42a0`, then `0x6b4360`. The first screen routine `0x6b42a0` was separately inspected: when object `+0x2b` is zero it returns immediately; otherwise it builds a selected human context, sets global `0xb67a48`, pushes `0x717cc0` and executes the screen loop `0x7fc7e0`, then clears the flag. It does not prove that every scheduled fixture is watched.
2. `0x5cd2fe–0x5cd32a`: traverse competition pointer array `0xadadfc` up to count `0xadadf8`; for every nonnull object call vtable slot `+0x14` with the date and zero. This happens **after match execution and before the date/slice duplicate-update guard**.
3. `0x5cd32c–0x5cd35e`: call `0x86dda0(date)`, call match-day screen routine again, destroy/free match-day object. Set its global pointer to zero.
4. `0x5cd361–0x5cd3a2`: compare last-updated date at `0xae2c78` with current date at `0xae2c90`, and last-updated slice at `0xae2cfc` with current byte at `0xae2c88`. If both match, skip updates directly to the human-turn loop. This is a per-date-and-slice guard, not a blanket once-per-day guard.
5. On changed date/slice, `0x5cd3a8–0x5cd3da`: scan every 110-byte staff record, call `0x53ab90(staff,date)` and write its signed result to staff `+0x18`. Thus this cached age field is refreshed for every changed slice.
6. `0x5cd3dc–0x5cd406`: call `0x8c5880(date,slice)` on the transfer manager, then `0x4c7ac0(date,slice,1)` on the contract manager. Original function bodies still determine which offers/contracts are due.
7. `0x5cd40b–0x5cd412`: if slice is not 2, skip the evening block to step 10 below. In slice 2 call `0x795360`, then enter additional periodic and daily processing.
8. Evening periodic blocks: weekday helper `0x54a3f0(date)` equal to **1** enables the club-rating block at `0x5cd437–0x5cd575`; separately, signed day-of-year `% 7 == 0` enables the club/squad/contract-reaction block at `0x5cd5a0–0x5cd99e`. These are different gates. The latter traverses clubs and up to fifty squad slots with many additional predicates; its effects cannot be collapsed into one generic morale increment.
9. Evening ordered routines are listed below. They include a day-of-year `% 30 == 0` conditional and other predicates; this is not the first day of a calendar month. Some routines contain their own calendar or competition checks, which must be recovered independently.
10. `0x5cdbbe–0x5cdbee`: call `0x448e30`, then `0x7f8750`; copy current date to last-updated date via `0x417c10`; save current slice to `0xae2cfc`. These two calls occur for **every changed slice**, including morning and afternoon. Their map labels are club-record news and scouting update, but their bodies still define actual work.
11. `0x5cdbf4–0x5cdc14`: call human-turn function `0x5cde40` until its low-byte result is zero. This also runs when step 4 skipped repeated updates.
12. `0x5cdc16–0x5cdc49`: increment slice. When it exceeds 2, reset to zero and add one day with `0x54c5b0` followed by date assignment `0x417c10`.
13. `0x5cdc4e–0x5cdd9a`: if global `0x9cd1f8` is nonzero **and the new slice is zero**, run an additional club shortlist/recruitment/media block. It consumes random numbers and traverses a club-order index. The block calls `0x83a730` and `0x84a020` conditionally, then `0x754a30` and `0x757c70`. Preserve this post-date-increment location. The meaning of the enabling global is unresolved here.
14. `0x5cdd9f–0x5cddd4`: format/display the date and slice, pump `0x68d520`, and branch to the loop's preceding control point `0x5cd246`. That control point and startup/exit/network paths are outside the retained loop excerpt, so this is not a complete reconstruction of `0x5cccd0`.

**Evening call order, directly read at `0x5cd9a4–0x5cdb2c`.** Semantic descriptions in the right column are inherited map labels unless a separate section verifies their body. The ordered addresses and explicit predicates are binary observations.

| Order | Original entry | Map label / additional observed predicate |
|---|---|---|
| 1 | `0x922ee0` | Weather update |
| 2 | `0x62b7b0` | Injury daily update |
| 3 | `0x556240(date)` | Discipline daily update |
| 4 | `0x9035a0(1)` | UEFA seeding season update |
| 5 | `0x413540(date)` | Award processing |
| 6 | `0x5ce060(1)` | Competition date update |
| 7 | `0x5ce2d0` | Staff daily update |
| 8 | `0x5ce0b0` | Squad morale by leadership |
| 9 | `0x5fa850(date)` | Host-country update |
| 10 | `0x5d5ae0` | Season update pass |
| 11 | `0x5d5f20` | Nation coefficients |
| 12 | `0x76b100(date)` | National teams |
| 13 | `0x839e60(0)` | Position-list rebuild, only when day-of-year `%30 == 0` |
| 14 | `0x83a780` | Position-list sort |
| 15 | `0x59b9c0(date)` | Finance table |
| 16 | `0x7a59e0` | Retirement processing |
| 17 | `0x7a65d0` | Regeneration queue |
| 18 | `0x8b1730(date)` | Counter update; directly verified below |
| 19 | `0x5d6250(day)` then `0x7a5790` conditionally | Retirement-discovery gate; directly verified below |
| 20 | `0x53db40(0)` | Club reputation sort |
| 21 | `0x79bf40` | Official availability reset |
| 22 | `0x84be20` | Simulated statistics matches |
| 23 | `0x68f9c0` | Manager/board daily update |
| 24 | `0x85b820(date)` | Stadium update |
| 25 | `0x4190d0` | Missed-training/AWOL update |
| 26 | `0x569ed0` | Dispute hearings |
| 27 | `0x5a5820` | Fines |
| 28 | `0x5d6d10` | Squad relationships |
| 29 | `0x5d6aa0` | Missing training checks |
| 30 | `0x5afce0` conditionally | Only if byte `0x9cc270 != 0` and pointer `0xae2a60 != 0` |

An additional optional branch at `0x5cdb2c–0x5cdbbb`, gated first by byte `0x9b5980`, performs date checks and can call `0x58a890` before step 10. Its feature meaning is not resolved; retain the original path rather than deleting it because it appears peripheral.

Two small called helpers were read completely and are in [original-gameplay-helpers.txt](evidence/original-gameplay-helpers.txt): `0x8b1730` increments a 16-bit field at training-manager `+0x688d` only when signed day-of-year `%7 == 0`; `0x5d6250` returns true exactly for day-of-year **0, 80, 173 or 250**. The main loop uses that boolean to call `0x7a5790`. These day numbers are not fixed month/day pairs across leap and non-leap years. Neither helper performs the actual attribute development or regeneration itself.

Required scheduler trace: for each call record `(date, slice, phase, subsystem, entityId, rngBefore, rngAfter, mutationDigest)`. Recover the order at new game, ordinary matchless day, two-match day, end of month, calendar-year rollover, competition season change, transfer deadline, contract expiry and injury-recovery day. Resume after every phase in a save/load differential test.

### 3.4 Random-number semantics [S]

The implemented original-style generator is in `src/si/random.c:21–62`; declarations and original addresses are in `include/random.h`. Its table contains **51,000 signed 32-bit entries** read from the original program's file offset `0x6a2454`, corresponding to original address `0xaa2454`. The table itself is absent from the repository.

The C runtime generator is:

```text
holdrand = u32(holdrand * 214013 + 2531011)
rand()   = (holdrand >> 16) & 0x7fff
```

`random_seed(seed)` substitutes `time(NULL)` for seed zero, seeds `holdrand`, sets `index = rand() % 51000`, `end = 50999`, and `offset = rand() % 65535`. For nonzero `n`, `random_number(n)` increments `index` before reading. If `index > end`, it becomes zero and a new `offset = rand() & 65535` is generated. Set `value = offset + table[index]` using the source's integer semantics. For `-65535 <= n <= 65535`, return `value % n` for positive `n`, or its negation for negative `n`. For `n == 0`, return zero **without consuming the stream**.

For larger magnitudes the source computes a double-scaled low term and recursively draws a high term; the unsigned casts, signed conversions, truncation, overflow and final clamp at `n - 1` all matter. Port `src/si/random.c:45–50` literally with explicit `u32`/`i32` helpers and `Math.trunc`; do not substitute `Math.random`, a generic seed library, `%` normalization, or `Math.floor` for negative division. Use `Math.imul` for low 32-bit multiplication. Store all four state fields plus table identity in every reproducible snapshot.

`tests/diff/cases_random.py` supplies differential cases for seed, signed bounds, large magnitudes, unseeded state, and crossing the table end. Its fixture sets x87 precision to 53 bits (`FPU_CONTROL = 0x27f`). These tests must be adapted to the TypeScript implementation and run against the same hashed original before claiming parity. The existence of the C implementation and tests does not substitute for the missing table.

### 3.5 A material fidelity issue: commentary speed [S/R]

`src/recomp_rt/win/commentary.c:1–15,66–89` adds Fastest = 10 to the original delay factors Very Fast = 20, Fast = 55, Medium = 90, Slow = 125 and Very Slow = 150. The wrapper temporarily exposes 20 during commentary stepping and 10 while producing a line, so Fastest inherits Very Fast's extra-pause behavior.

The source comment explicitly records that the original steps other matches while a commentary line waits, so changing speed can change results. The inherited analysis in `docs/research/2026-10-15-commentary-speed-inventory.md` identifies the wait/step paths (`0x6d7b00`, `0x6b55c0`, `0x6b5ce0`, `0x6b98c0`). Consequently **“animation speed never changes simulation results” is not presently a verified compatibility claim**.

For the web port, use a versioned `simulationSchedulingPolicy`. Exact compatibility requires reproducing and testing original scheduling, including logical delay inputs. A decoupled policy can make playback speed presentation-only, but it is a deliberate behavioral change with its own save version and test baselines. Never let arbitrary HTTP latency, browser focus, reconnects or CPU load become accidental substitute scheduling inputs.

## 4. Squad selection and tactics

### 4.1 Position vocabulary and masks [S]

The assistant reads the eleven position masks at tactic offset `+0x12d`, two bytes per slot. Position bits are GK `0x1`, sweeper `0x2`, D `0x4`, DM `0x8`, M `0x10`, AM `0x20`, F `0x40`; side bits are left `0x80`, right `0x800`, and center `0x100|0x200|0x400`. Source: `src/recomp_rt/win/picker.c:58–84,188–230,439–454`.

For candidate/slot fit, select the highest allowed position rating; first position in the array wins a tie. For D/DM wide slots, consider wing-back rating too. Except for GK, select the corresponding side rating (left has precedence over right, then center) and return `min(positionRating, sideRating)`. No side restriction defaults to 20. A goalkeeper slot rejects a player whose GK rating is below 10. These rules are the Bosman assistant's scoring rules, not a recovered match-engine effectiveness formula.

### 4.2 Assistant selection: exact available algorithm [S]

Source: `src/recomp_rt/win/picker.c:133–354,407–552` and `src/recomp_rt/win/pick_score.h:14–34`.

1. Determine active tactic (or an explicit eleven-mask formation), rejecting invalid kinds other than 1/2/3.
2. For the first team, find the first fixture from today through 30 days ahead using original fixture search. Reserve-team view uses no fixture. Read named bench count from fixture byte `+0x4d` low nibble, clamp to nine; absent a fixture, use five.
3. Scan up to 50 squad entries including reserve-team players. Skip null/non-player entries. Exclude unavailable candidates when computing a pick. Injury-status callback returning 2 excludes them; national-duty callback returning nonzero excludes them; if a fixture exists, original availability callback `0x45cbe0` must allow them.
4. Read condition, age and displayed attributes. The compute path suppresses original per-player daily fitness update `0x62c680` while querying getters. It must not consume random numbers or advance fitness merely by opening an assistant view. Values may therefore be the record's last updated values.
5. Sort candidates by condition descending, then original staff address ascending. A TypeScript port must preserve a stable equivalent original order, not locale name order or database query order.
6. Choose the preferred group: best-team includes everyone; under-21 includes age strictly less than 21; most-match-fit includes condition at least 90, but also admits enough of the condition-sorted remainder to fill the eleven plus bench, and a natural goalkeeper (GK rating at least 15) if none has been admitted.
7. Score each candidate against each slot using the keys below. Solve the eleven as one maximum-weight bipartite assignment using the Hungarian algorithm. Then solve substitute slots using only remaining candidates. Do not greedily fill positions one at a time.
8. Store the selection only for the explicit Pick command. Preview/Best XI must not write it. Storing clears the previous selection through original callbacks, writes each occupied slot, then chooses the captain using original `0x6e1cf0`; that captain scoring body remains unresolved. First selected slot wins equal captain scores in this wrapper.

| Role | Key displayed attributes |
|---|---|
| Goalkeeper | Handling, Reflexes, Agility, Positioning, Jumping |
| Center defender / sweeper | Tackling, Marking, Heading, Positioning, Strength, Jumping |
| Wide defender | Tackling, Marking, Pace, Positioning, Crossing |
| Defensive midfielder | Tackling, Positioning, Passing, Teamwork, Work Rate |
| Center midfielder | Passing, Decisions, Teamwork, Work Rate, Technique |
| Wide midfielder | Crossing, Dribbling, Pace, Passing, Technique |
| Attacking midfielder | Passing, Creativity, Decisions, Technique, Flair |
| Forward | Finishing, Off the Ball, Anticipation, Technique, Pace |

Hidden attributes are represented as zero to this scorer and omitted from both sum and count. With `sum` over known keys, `count` their number and `fit` as above:

```text
value = trunc(sum * fit * 60 / count)       if count > 0
value = -2                               if all keys hidden
value = -1                               if invalid for goalkeeper slot
rank  = 0 for value -2; -1 for other negative; otherwise value + 1
shown = 0 (render '-') for value -2
shown = clamp(trunc((value + 600) / 1200), 1, 20) otherwise
```

The assignment weights are `W_OUT = 1,000,000`, `W_LOW = 21,000,000`, `W_EMPTY = 441,000,000`. An invalid assignment and an empty slot weigh `-W_EMPTY`. A valid assignment weighs `rank - 1 - (fit < 10 ? W_LOW : 0) - (preferred ? 0 : W_OUT)`. This gives priority to filled slots, then avoiding low fit, then preferred-group membership, then football score. Hidden-only candidates rank below scored candidates at equal fit/group. Substitute slot masks in order are GK, D, M, M, F, M, D, F, M.

Concrete check: known MC keys `[15,14,12,13,16]`, fit 18 produce `value = 70*18*60/5 = 15120`, raw score 12.6, shown score 13. If only Passing=15 and Decisions=14 are known, use count 2, not 5; `value=15660`, shown 13. If none are known, show `-` and do not infer a score from current/potential ability. Current ability is read only for a test dump in this implementation, not used by the selection objective.

Important boundary: availability, displayed attribute conversion, fog-of-war tests, next-fixture selection and captain score still delegate to the original. Reimplementing the assignment solver is insufficient to reproduce the full command until these dependencies are verified.

### 4.3 Tactical editing contract [D/U]

Persist distinct committed tactics and an editable working copy. Changes must target a defined decision point and become visible to the engine exactly once. A command must not silently overwrite a tactic changed in another browser. Preserve eleven slot assignments, position masks, per-slot instructions, team instructions, set-piece assignments and taker orders as independently typed data. Keep unknown native bytes until decoded instead of dropping them during import/export.

Required recovery for every tactical instruction: exact enum labels/values, default, valid combinations, edit-time restrictions, engine read sites, effect formula, recalculation timing and persistence representation. With-ball and without-ball grids must retain their full spatial data; a formation label such as “4-4-2” is not a complete tactic. Existing preset research (`docs/research/2026-10-16-tactics-inventory.md`) is an input to this recovery, not proof of its in-memory normalization.

Substitution commands require player-out/player-in, target slot, instruction changes, decision cursor and competition rule version. Validate bench membership, eligibility, prior use, dismissal/injury status and remaining substitutes using recovered rules. Apply removals/insertions, captain/taker changes and condition/statistics updates atomically. No newly invented “five substitutes” global default: bench size and permitted substitutions are separate competition data.

## 5. Match engine specification boundary

### 5.1 Proposed match aggregate [D]

Each match owns fixture/rule version, home/away sides, venue/environment, officials, selected and substituted players, tactical state, phase/clock, possession and ball state, player positions, incidents, statistics, commentary cursor, result state, RNG linkage and commit status. Original-memory layouts and domain representation should be connected by an explicit codec, not exposed directly through HTTP.

The domain phase model is a proposed interface: `awaitingLineups -> ready -> firstHalf -> halfTime -> secondHalf -> regulationEnded -> {finished | extraTimeFirstHalf -> extraTimeInterval -> extraTimeSecondHalf -> extraTimeEnded -> {finished | shootout}}`. It must support suspension/abandonment only after the original rules are recovered. This model is not a recovered enumeration. Original phase codes and callbacks may require additional intermediate states.

Match completion must be a one-time transaction linking score/incidents to fixture, table, histories, fitness, discipline, finances, morale and news. Retrying an interrupted completion must never award prize money twice, record two appearances or serve a suspension twice. Distinguish a final match score, aggregate tie score and penalty score; do not add shootout goals to ordinary goal statistics unless the original explicitly does so.

### 5.2 Required algorithm inventory [R/U]

These original-map entries are **analysis targets only**. Before using their named purpose in an implementation, read and annotate the original body and its callees as required by `CLAUDE.md`.

| Concern | Starting analysis targets | Deliverable required before TypeScript replacement |
|---|---|---|
| Initialization / stepping | `match_eng.cpp.md`: `0x6b7f20`, `0x6b98c0` | Argument layouts, full initialization writes, tick units, status codes, all loop exits |
| Quick simulation | `match_eng.cpp.md`: `0x6beeb0` | Event distributions, goal/card/injury/condition generation, minute order, result postprocessing |
| Period completion | `match_eng.cpp.md`: `0x6bd930` | Exact period and added-time rules, competition flags, extra-time/shootout decision |
| Team/player effective strength | `match_eng.cpp.md`: `0x6bbb60`, `0x6c0c60`; `match_pl.cpp.md`: `0x6f55c0` | Attribute transformations, fatigue/morale effects, all caps and rounding |
| Action ordering / movement | `match_eng.cpp.md`: `0x6bd760`, `0x6bce80`, `0x6c2be0` | Grid dimensions, occupancy, possession-dependent targets, tie-breaks and random draw order |
| Passing / challenges | `match_eng.cpp.md`: `0x6c6a30`, `0x6c87e0`, `0x6c9cd0` | Candidate generation, scoring, distance/pressure effects, success and interception formulas |
| Shooting / goalkeeping | `match_eng.cpp.md`, `match_pl.cpp.md` | Shot selection, on/off-target, blocked/save/rebound/goal paths, keeper decisions and assists |
| Fouls / cards / advantage | `match_eng.cpp.md`: `0x6bab90`, `0x6bac40`, `0x6bba80` | Pending-foul record, advantage cancellation, severity, referee influence, card rules |
| Offside / restarts | `match_eng.cpp.md`: `0x6c3aa0`, `0x6c4290`, `0x6c4470`, `0x6c4820`, `0x6c4ec0`, `0x6c50d0`, `0x6c51e0` | Position transforms, taker choice, legal placement, possession reset, statistics |
| Shootout | `match_eng.cpp.md`: `0x6be520`, `0x6be710` | Eligible takers after dismissals, order, termination, sudden death, all counter limits |
| In-match management | `match_man.cpp.md` | AI substitutions/tactics, human tactical changes, captain and set-piece recomputation |
| Event/statistic generation | `match_events.cpp.md`, `match_official.cpp.md`, `player_stats.cpp.md`, `simulated_stats.cpp.md` | Full event type table, parameters, effect ordering, statistic definitions |

The current evidence does not establish the probabilities, attribute weights, tactical coefficients, match tick duration or full field dimensions. Do not infer them from the 2D view, a familiar CM tactic, or names in generated maps.

### 5.3 Verified Bosman safeguards [S]

`src/recomp_rt/win/season_match_grid.c:31–64` wraps original movement `0x6f46b0` only in a season game. It checks an occupied player's cell and per-side occupancy. A genuine player whose old cell count is zero is marked as in no cell before invoking the original move; an object that is not a valid match player returns without moving. The guard's validation includes side 0/1, nonnegative x, y from 0 through 11, slot below 12 and a readable engine/fixture. This does **not** establish a safe maximum x or complete grid dimensions.

The same file's test wrapper (`:66–92`) identifies fixture `+0x4d` high nibble as substitutes used, and preserves the low nibble. Source comments report original initialization caps at nine. Keep the test-only forced-substitute setting out of normal gameplay configuration. The shootout instrumentation (`:95–146`) reads eleven taker entries per side and signed-byte kick counters; it diagnoses problematic states but does not fix or define shootout termination.

### 5.4 Match acceptance scenarios [D]

Mandatory differential fixtures include: both teams unchanged; one tactical instruction changed; one attribute changed; home/away swapped; neutral venue; absent/low-rated goalkeeper; minimum eligible lineup; injury before kickoff and in play; first/second yellow and direct red; substitute already used; halftime change; extra-time substitution limit; two-leg aggregate draw; away-goal rule variants where supported; extra-time/shootout; long shootout; stoppage-time goal; canceled/postponed fixture; watched versus quick modes; commentary speed changes; multiple human managers watching; simultaneous background fixtures; save/reload at each safe boundary.

For each compare not only score, but ordered RNG calls, incidents, positions where observable, substitutions, ratings, condition/injury records, disciplinary state, player/team statistics, standings, money, news and next-day state. Matching a final score is insufficient.

### 5.5 Small original match rules recovered directly [B]

The following is instruction-level recovery, not reliance on the function-map prose. Field semantics still need the larger object schema.

**Pending-foul insertion, `0x6bb9a0–0x6bba06`.** If the 32-bit field at engine `+0x4761` is already nonzero, the function returns without writing anything. Otherwise it writes the nine arguments as follows: argument 1 low word to `+0x475f`; argument 2 to `+0x4761`; argument 3 to `+0x4765`; argument 4 low byte to `+0x4769`; arguments 5–9 low bytes to `+0x475a…+0x475e`. Thus the occupied pending record cannot be replaced by a later call through this function. The function returns with 36 bytes of arguments removed. This does not establish how a pending incident is awarded or whether an argument can be null in valid play.

**Pending-foul reset, `0x6bba10–0x6bba4c`.** Set the word at `+0x475f` to `-1`; pointers at `+0x4761` and `+0x4765` to zero; bytes at `+0x4769` and `+0x475a` to zero; bytes at `+0x475b…+0x475e` to `0xff`. Retaining these sentinels matters for a binary-compatible adapter; a domain port may express them as explicit absent values only after all readers are mapped.

**Match-player reaction scaling, `0x716450–0x7164aa`.** The function reads a float factor argument, multiplies the float at player `+0x1a2`, and applies a floor `(25 - signedByte(staff +0x5d)) * 2` through the staff pointer at player `+0x69`, then an upper cap of 100. The map calls `+0x1a2` anger and the staff byte temperament, but those semantic names remain inherited until the field producers are traced. For ordinary finite valid inputs, the numerical rule is `min(100, max(value*factor, 2*(25-byte)))`. The x87 code stores intermediate values as float32; a JavaScript port needs the corresponding rounding points, not just unrestricted double arithmetic. Invalid/NaN behavior needs separate treatment if exposed by imported corrupt state. This is a reaction-scaling rule, not the entire foul or violence probability model.

The more involved original reaction check at `0x7164b0` was inspected but is not promoted here to an implementation formula: it depends on a floating-point helper, a five-way dispatch and RNG semantics, and the complete meaning of its levels and target relationship still needs caller evidence.

**Bounded execution verification:** a permitted isolated Unicorn execution compared reaction scaling for 800 synthetic finite-input combinations (raw staff byte 1–20; eight starting values; five factors) and performed eight pending-record insertion, occupied-record preservation and reset checks. All passed with zero mismatches and x87 control word `0x27f`. This validates these local formulas/mutations for the tested inputs, not their semantic field names, caller timing, invalid pointers, NaNs, full match behavior or the complete application. Evidence: [instructions](evidence/original-gameplay-helpers.txt), [probe script](evidence/match-helper-probe.py), [probe result](evidence/match-helper-probe.json).

## 6. Fitness, injury, training and player lifecycle

### 6.1 Fitness and illness [S/U]

The assistant's read path demonstrates that condition/injury getters can invoke a daily update (`picker.c:114–142`). The web implementation must not assume every getter is pure when using retained native code. Read models should be produced at a controlled phase or with the same suppression semantics; random-dependent updates from repeated Angular queries would corrupt compatibility.

`src/recomp_rt/win/injury_init.c:39–72` supplies a season-game-only initialization fix. During original `0x62cf70`, it tracks the staff row of the outer `0x62d250` injury call. A nested injury call targeting a later, not-yet-initialized row returns `-1`; ordinary calls run the original. Preserve that condition if reproducing Bosman behavior. Do not generalize the research comment “about one player in a hundred” into an exact probability: the original initializer must be read to establish it.

Required injury model fields [D]: type, onset date/slice, source, affected player, severity/duration representation, recovery milestone, treatment, recurrence/history linkage, availability, condition/fatigue effects and illness/outbreak linkage if confirmed. Separate current injury, historical injury, availability and fitness; none is a reliable substitute for the others.

Unresolved algorithm targets [R]: `injury.cpp.md` entries `0x62b7b0`, `0x62c080`, `0x62c2f0`, `0x62c680`, `0x62d140`, plus `physio.cpp.md`. Recover daily versus on-read update semantics, training/match load, age/natural fitness and staff/facility effects, probability tables, injury-code meanings and treatment decisions. Test same-day repeated reads, rest days, congested fixtures, leap-day recovery, illness spread during initialization, injured transfer, national duty and season boundary.

### 6.2 Training [D/R/U]

Represent schedule definition separately from assignments and historical progress. A schedule has a stable ID/name and an exact recovered vector of training categories/intensities, plus any position/side retraining configuration. Coach-to-category assignment, workload and facility context must be included where the original reads them. Preserve edits as commands; reject deleting a schedule still assigned unless the original's reassignment behavior is explicitly implemented.

Candidate recovery entry points in `training_manager.cpp.md`: `0x8b1750` player training update, `0x8b2be0` apply progress, `0x8b37c0` attribute change, `0x8b41d0` coaching factor and `0x8b42d0` position training. These names are map hypotheses. The specification still needs every formula, timing interval, cap, random draw, injury/fatigue link and cancellation/reversion path. There is no source evidence here for an invented “weekly +1 attribute” rule.

Training tests: no-training baseline, minimum/maximum intensity, one category changed, coach absent/overloaded, facilities changed, young/old player, differing potential/current ability, injured player, position versus side retraining, transfer between schedules, save/reload before progress application and long-run saturation. Verify visible attributes and hidden values separately.

### 6.3 Development, retirement and regeneration [D/R/U]

Current ability, potential ability and displayed attributes are distinct values. A simple arithmetic mean of the displayed attributes must not replace ability, and current ability must not leak into the assistant's shown score. Preserve personal identity and history across club changes and any player-to-staff transition. A regenerated player requires a new identity even if an original record slot is reused internally; store legacy slot identity separately from web entity identity.

Investigation targets: `player_regen.cpp.md` retirement discovery `0x7a5790`, retirement processing `0x7a59e0`, regeneration queue `0x7a65d0`, injury retirement `0x7a7f20`, and initialization/player setup paths. Exact retirement dates, age rules, ability/potential generation, national pools, names, clubs, reuse/reset semantics and non-player conversion remain [U]. Avoid modern Football Manager assumptions about newgens or youth intake dates.

Long-horizon acceptance: no dangling contracts/shortlists/selections after retirement; retained historical results; finite reuse pools; correct identity reset; no old injury or ban assigned to a regenerated identity; stable save/load after many seasons; no unintended attribute visibility change.

### 6.4 Original training enums and load calculation [B]

Instructions in `0x8b5520–0x8b568d` select literal strings by category index; inspected strings at `0xa8cf74`, `0xa8cf48`, `0xa8cf1c`, `0xa8cef4`, `0xa8cec0` identify these categories in exact index order:

| Category index | Category | Schedule byte offset |
|---|---|---|
| 0 | Fitness | `+4` |
| 1 | Tactics | `+5` |
| 2 | Shooting | `+6` |
| 3 | Skills | `+7` |
| 4 | Goalkeeping | `+8` |

`0x8b5690–0x8b57d4` maps intensity bytes `0 -> None`, `10 -> Light`, `25 -> Medium`, `50 -> Intensive` using strings at `0xa8d060`, `0xa8d034`, `0xa8d004`, `0xa8cfd4`. Other values take the function's error path. `0x8b4f30–0x8b4f47` writes the requested byte at `this + 4 + signedCategory` and clears the 32-bit field `+0x93`; it does not perform bounds validation. The web command boundary must validate the category and enum before invoking it. `0x8b4f50–0x8b4f59` reads the indexed byte.

`0x8b4f60–0x8b4ff9` computes training load. Read signed bytes F/T/S/K/G from those offsets. Constants directly decoded from the original are `-1.3` at `0x96f878`, `0.5` at `0x968968`, `-0.5` at `0x969540`. Each multiplication is separately converted to integer through `0x945640–0x945666`, which sets the x87 rounding control to truncate toward zero and restores the previous control word.

For the four valid intensity levels, the exact ordinary-input formula is:

```text
load = min(100,
    trunc(0.5 * G)
  - trunc(-1.3 * F)
  - trunc(-0.5 * K)
  - trunc(-0.5 * S)
  - trunc(-0.5 * T))
```

The comparison/return uses the signed low 16 bits. Do not combine fractional terms before truncation: all-Medium yields `12 + 32 + 12 + 12 + 12 = 80`, not 82. All-Intensive produces 165 before the cap and returns 100. Fitness-only Medium returns 32. An isolated Unicorn run of the original function compared **all 1,024 combinations** of the five categories' four valid levels against this formula and reported **zero mismatches**, with x87 control word `0x27f`. The original application was not launched and no live save was used. This load is only one training/fitness input; it is not the attribute-progress formula or a probability of injury. Evidence: [instructions](evidence/original-training-load.txt), [probe script](evidence/training-load-probe.py), [probe result](evidence/training-load-probe.json). The script records the local reference path; relocate it deliberately when rerunning elsewhere, while preserving hash validation.

The six-slot coaching factor routine `0x8b41d0` was also inspected. It uses an assignment table and a float divisor table depending on nonzero assignments, then a multiplier and floor; its dependent table producer and semantic field types still need full verification. It is deliberately not presented as a complete coach-quality formula.

## 7. Transfers, contracts and eligibility

### 7.1 Transaction model [D]

Transfer negotiation, contract negotiation, loan, registration and work permit are related but distinct aggregates. An accepted club bid is not a completed transfer. A signed contract is not necessarily current competition eligibility. A loan does not replace the parent-club contract. Support multiple competing offers and a recorded reason for rejection, collapse or delay.

A proposed semantic offer lifecycle is `draft -> submitted -> clubDecision -> {rejected | countered | accepted} -> contractNegotiation -> {failed | termsAgreed} -> requiredChecks -> {failed | scheduled | executable} -> completed`, with cancellation/expiry/hold transitions where recovered. These are **web semantic states, not original numeric state codes**. Recover the native state byte table before mapping it; retain any original states that do not fit this simplification.

An offer contains: buyer/seller, subject staff ID, permanent/loan kind, immediate fee, deferred payments, percentage/appearance/international or other clauses that the original supports, exchange players, proposed timing, deadlines, linked contract offers, permit/registration state and negotiation history. The set of actual clause types, values, rounding rules, legal combinations and payment triggers must come from original code/data. Do not add modern clauses merely because they are common elsewhere.

Completion must atomically settle all relevant club accounts, obligations and previous clauses; update current/parent/loan club relationships; maintain contracts and squad/registration slots; update eligibility, history and news; invalidate competing offers as the original does. Preserve scheduled future transfers separately from current employment. Failure must not leave half-completed exchanges or deducted fees.

### 7.2 Verified contract/loan representation and Bosman fix [S]

`src/recomp_rt/win/loan_release.c:48–77` reads a 79-byte per-staff row whose first two signed indices are main contract and loan contract (`-1` for absent). Contracts are 80-byte rows; `+4` is club ID. This is direct wrapper code evidence, not a full contract schema.

For a season game, when a staff member loses a staff job at the loan club, the wrapper at `0x690890` preserves the main contract if the loan contract belongs to that club and the main contract belongs elsewhere (`:112–139`). It skips only the specific original main-contract removal call from `0x69089e`; it does not globally suppress contract removal.

The end-loan safety path at `0x4d07e0` applies when the staff member has a loan but no main contract and box fixes are enabled (`:142–164`). It calls staff removal and loan-contract removal rather than entering the original failing path. The final plain-player/club state depends on the called original function, so verify it in a differential fixture rather than guessing “return to parent club.” Source commentary describes a released player/coach remaining at the loan club without a contract; this is inherited explanation of the callback behavior.

Regression fixture: player/coach on loan loses coaching job, keeps playing loan and parent contract, reaches loan expiry, and subsequently transfers. Include an already-corrupt save with missing parent contract. Keep Bosman season-game behavior and base-game compatibility separate in test baselines.

### 7.3 Recovery tasks [R/U]

Read original bodies behind `transfer_manager.cpp.md` `0x8c5880` (time-slice update), `0x8c63a0` (due-offer dispatch), `0x8c8090` (AI bid response), `0x8c7950` (permit evaluation), `0x8c9390` (completion) and `0x8c9720` (move). Read `contract_manager.cpp.md`, `transfer_offer.cpp.md`, `staff_contracts.cpp.md`, national rule objects and eligibility callers. Names are leads only.

Required outputs: exact state/enum tables; full offer transition matrix; salary/fee currency unit; all response formulas and delays; contract expiration inclusivity; expiring/free-agent approach rules; compensation; registration and loan restrictions; work-permit eligibility/appeal; nationality changes; interaction with bans/national duty; AI budgets and shortlist rules; save/load representation. Country/season rule tables alone do not recover the original generic offer algorithm.

Boundary cases: duplicate submission; simultaneous accepted bids; exchange player becomes unavailable; buyer loses funds before completion; transfer window closes/reopens; agreement before window for later move; contract expires on transfer day; transfer during loan; transfer after injury; player/staff dual job; manager dismissal while offers exist; fee installment and sell-on chain; registration full; work permit refused; nationality changes before ban release; future agreement canceled; save/reload at every state.

## 8. Finances, board, relationships and scouting

### 8.1 Finance contract [D/U]

Use integer money in a documented canonical unit and currency ID. Display exchange rates/formatting must not silently revalue obligations. Every monetary mutation has a ledger reason, game date/slice, club, counterparty where applicable, amount, source object and idempotency key. Maintain cash balance, available transfer budget and wage commitments separately; a transfer fee is not automatically equal to budget decrement.

The full original financial model is unresolved. Recover all actual income/expenditure categories from source and save data: candidate categories include gate receipts, competition payments, wages/bonuses, transfer fees/clauses/installments, staff compensation and facilities/stadium spending. Do not assume taxation, debt interest, sponsorship formulas or insolvency rules without finding their implementation. `cash.cpp.md` is not evidence that it owns the whole finance subsystem; follow references from club/manager/transfer/competition routines to actual financial writes.

Acceptance requires reconciliation (`opening + posted movements = closing`) in canonical units, no double posting on retries, exact truncation/rounding, correct recurring cadence, and correct budget impact under transfers, renewals, dismissals, promotion/relegation and season change.

### 8.2 Board and managerial career [D/R/U]

Manager status must support employment, unemployment, resignation, dismissal and offers. Club and national-team roles are separate when the original permits both. Board confidence, public reputation, supporter/media reactions and contractual job status must not be collapsed into one score.

Candidate analysis targets in `manager_manager.cpp.md`: `0x68f9c0` daily update, `0x695240` post-match board update, `0x697c40` weekly confidence, `0x69aba0` dismissal consideration, and job appointment/departure paths. For each board request and ultimatum recover eligibility, response rules, cost, delay, effects, expiry and news; inspect `ultimatum.cpp.md`. Exact formulas and thresholds remain [U].

Test severe underperformance, overperformance, rival/cup/important games, insolvency pressure where supported, rejected request, ultimatum, job application, concurrent job offer, dismissal during pending transfers, human manager's last club lost and continued unemployed play. The server must never silently end a session because a manager has no club.

### 8.3 Morale, discipline and relationships [D/R/U]

Model an individual reaction as a cause-linked record with start date, effect and expiration/resolution where recovered, not a single untraceable UI number. Player selection, transfer decisions, fines, missed training, disputes and media may interact. Recover trigger ordering and whether effects are permanent, decaying, stacked or overwritten. Candidate maps: `media.cpp.md`, `Fine.cpp.md`, `dispute.cpp.md`, `awol.cpp.md`, `news.cpp.md` and relevant staff/player functions.

`src/recomp_rt/win/discipline_ban.c:27–43` implements a fallback international-ban lookup by staff ID when the normal nation-index lookup failed. A nation change must not orphan an existing ban. Its held-ban helper (`:49–57`) is season-game-only when enabled. These are concrete robustness fixes, not a full ban accumulation/serving algorithm. Recover governing-body scope, competition carry-over, card thresholds, appeals, cancellation, international versus domestic bans, and what happens to postponed fixtures before implementing discipline.

### 8.4 Scouting and information visibility [S/D/U]

`picker.c:373–390` applies the original visibility test `0x543280` before calling the shown-attribute getter `0x5431e0`, caps shown values at 20, and treats no viewer as seeing all. Do not expose the no-viewer path to a normal web client. The server must project player data for the authenticated manager: return nullable/masked attributes, not hidden values plus a client-side visibility flag. Search, sorting, comparison, assistant scores, exports and aggregation must all obey the same visibility boundary.

There is a deliberate newer Bosman exception: `src/recomp_rt/win/groups_layout.c:19–24` adds One On Ones and Consistency to the goalkeeper profile even though the original does not show those fields. Follow the exact profile getter/visibility logic documented in the data/UI chapter. Do not turn the old glossary's broad “no hidden values shown” statement into a blanket rule that removes this shipped source feature, and do not add these two values to the assistant scoring keys.

Scouting assignments/reports/knowledge form persistent game state. Scouting recommendation is a staff opinion, not the true hidden ability. Exact knowledge gain/decay, report accuracy, assignment duration, costs and scouting attribute effects remain unresolved; inspect `scout_manager.cpp.md`, `coach.cpp.md`, `fog_of_war.cpp.md`, `search_eng.cpp.md` and filter/edit-session maps.

Test unknown player, partially known player, own-club player, national-team knowledge, disabled attribute masking, different managers viewing one player, expired report, player transfer, shortlist removal, foreign-club Best XI and masked search sorting. Hidden values must never appear in browser payloads, logs available to players, CSV exports or inferred numeric scores.

## 9. AI and holiday

### 9.1 Separate assistant advice from AI club management [S/U]

The Bosman deterministic assistant selection above is not the original AI team-selection algorithm. Original match selection is called at `0x6dbc50` and related callbacks, and the complete policy remains unavailable. Recruitment, formation choice, tactical responses, substitutions, contract offers and staff changes require separate recovered policies; do not reuse the assistant's visible-attribute scoring as a universal AI model.

`src/recomp_rt/win/season_pick_rotation.c:50–102` suppresses rotation for a constrained season-game first-team path when fewer than 25 squad players are counted as able to play the current fixture. Its preconditions include pick mode 3, club reputation at least 6000, readable fixture/squad and a non-special fixture. Counting excludes injury status 2 and requires eligibility code zero. It makes the specific original next-fixture query return zero fixtures; it does not rewrite the selection itself. The source's explanation of the original retaining fourteen players is research commentary, while the threshold/guard implementation is directly visible.

### 9.2 Holiday semantics [S/R/U]

The ordinary holiday options and their AI effects require original recovery. Inherited research identifies settings for rejecting transfer offers, restricting purchases, applying for higher jobs, retaining tactics and retaining selection. Preserve those distinctions in the data model, and record delegated actions exactly as human actions for history and repeatability.

`src/recomp_rt/win/holiday_next.c:163–234` adds “Holiday until next match.” It refuses initiation when no club fixture is found in the next 60 days. At a human turn, if marked on holiday and the next fixture is today or tomorrow, it clears holiday. A pending action becomes active through the ordinary confirmation/setter path. If retaining current selection is enabled for a club manager, it clears current selection and restores valid same-club players from sixteen saved slots. The native implementation's sixteen-slot restore is a compatibility detail; do not silently broaden it to twenty while claiming exact parity.

`src/recomp_rt/win/holiday.c:41–63` implements faster days by subtracting 2000 milliseconds from the holiday stamp read. In fixed-clock tests it retains wall-clock stamps outside saved manager records and writes zero into the record. Preserve the distinction between removing a presentation hold and skipping actual game processing. A web “simulate until” command must run all world logic and stop at the recovered return condition; it must not jump the date directly.

## 10. Implementation contracts for every unresolved subsystem [D]

Before assigning a pure-TypeScript subsystem to an implementation agent, its rule packet must contain all of the following:

1. Original artifact SHA-256 and Bosman commit/rule versions.
2. Entry points, calling conventions and complete reachable dependency boundary.
3. Input/output types, units, bounds, signedness, sentinels and enum tables.
4. Full state read/write set, including apparently incidental counters, caches and random state.
5. Ordered transition table with every branch predicate, side effect and failure path.
6. All numeric formulas, tables, rounding, overflow, comparison inclusivity and random draw sites.
7. Calendar/slice scheduling and interaction with human decision points.
8. Save/load schema and migration rules, plus behavior on missing or corrupt references.
9. Visibility and permissions policy for every read/write surface.
10. Differential fixtures with input snapshot, action stream, expected output/mutations and provenance.
11. Explicit divergences, each with rationale, version gate and tests.

If any required item is unknown, the agent must retain the original engine route for that operation or mark the feature blocked for faithful replacement. A function name, UI screenshot, generated summary or domain intuition is not sufficient evidence for an equation.

## 11. Gameplay release gates

| Gate | Required evidence |
|---|---|
| GP-A: Pure helpers | Date arithmetic and RNG outputs/state match the original on boundary and randomized fixtures; actual random table hashed |
| GP-B: Advisory screens | Repeated views do not mutate world or RNG; hidden attributes stay hidden; assistant assignment and ties match source |
| GP-C: Decisions | Every squad/tactic/contract/transfer command has authoritative validation, stable rejection and idempotency |
| GP-D: One match | Complete watched match, injuries/cards/substitutions, final effects and restart/reload agree at declared checkpoints |
| GP-E: One month | Background matches, wages/contracts/training/fitness/scouting/news interact in verified order |
| GP-F: Season transition | Competition changes, promotion/relegation, histories, finance, contracts and player lifecycle produce a coherent next season |
| GP-G: Long career | Multi-season runs retain referential integrity, bounded state growth and repeatable saved-state continuation |
| GP-H: Full replacement claim | Every G01–G24 row and original dependency covered or explicitly retained; all remaining differences disclosed |

Until GP-H, label the product accurately as a web interface to the retained game or a partially verified reimplementation, according to what actually runs. This chapter provides exact available helper/enhancement rules and a full recovery checklist; it does not supply the missing original match, training, transfer, financial and AI algorithms.

## 12. Player-facing gameplay requirements from the supplied manual [M]

This section fills the interaction vocabulary while the numeric implementation remains under recovery. Page numbers are the printed/PDF pages of the supplied English manual, extracted during this investigation to `/private/tmp/cm0102-manual.txt`; the durable artifact identity/location belongs in the evidence manifest. These are paraphrases of documented product behavior. The manual is not proof of official-patch or newer Bosman constants, and advice such as “high stamina helps pressing” is not an equation.

### 12.1 Tactical controls (pages 40–53)

The game exposes team mentality, passing style, tackling intensity, pressing, offside trap, counter-attacking and men-behind-ball instructions. Mentality includes Defensive, Normal and Attacking; passing includes Short, Mixed, Direct and Long; tackling includes Easy, Normal and Hard. Player-specific controls can inherit Team settings where offered. Preserve all actual enum values after code recovery rather than deriving integers from this list order.

The tactical editor has Overview, With the Ball and Without the Ball views. The latter two divide play into twelve ball zones (six per half); for each zone, managers set their players' locations depending on possession. These **tactical editing zones are not evidence for the dimensions of the simulation movement grid**. Dragging positions in Overview updates the associated formation shape. Reflect provides symmetric setups on the opposite flank; Show Runs visualizes movement between selected zones.

Player instructions documented on pages 49–53 are Passing, Tackling, Pressing, Pass To (all areas/central/left/right), attacking and defensive set-piece participation, Free Role, Forward Runs, Run With Ball, Hold Up Ball, Long Shots, Marking (Zonal/Man to Man/None), Cross Ball and Through Balls. Left/right free-kick and corner takers are separately selectable. The UI must preserve overrides and inheritance distinctly; a displayed Team selection cannot be persisted as an arbitrary copy of the team's current numeric value or it will stop following later team changes.

Qualitative intent: pressing reduces an opponent's time/space but increases fatigue; offside traps rely on coordinated defensive movement; counter-attacking transitions quickly after regaining possession; men behind the ball trades attacking presence for defensive numbers. Passing/tackling instructions affect risk and style. Player-specific instructions influence movement/action choices. These intentions must be reconciled with original coefficients/call paths before asserting quantitative effects.

### 12.2 Training and staff (pages 22–23,53–55)

A player participates in one training routine at a time. Routines cover Fitness, Tactics, Skills, Shooting and Goalkeeping, plus New Position and New Side retraining controls. None/Light/Medium/Intensive are the visible intensity choices; their numeric mapping and load are recovered in section 6.4. Original training views mark improved attributes green, worsened attributes red and unchanged attributes cyan; this is progress coloring, distinct from Bosman's value-based attribute coloring.

Coaches can be assigned to training areas. The manual's advice to use no more than three areas per coach is guidance, **not an enforced maximum established here**. It describes initial assignment of coaches to all areas and optional assistant-organized training. It says position retraining preserves the original position but a later retrained position can replace the earlier retrained one; verify that behavior in `0x8b42d0` and its dependencies.

Assistant managers handle reserves and provide advice. Coaches and physios provide player reports. The manual describes medical opinions influencing transfers and the highest Physiotherapy-rated physio's opinion carrying weight where several exist; retain a transfer medical-check step in the requirements while recovering its actual implementation. The manual describes employing up to seven scouts, subject to club finances, consistent with the seven-scout field in the club structure. Scouts support league/region, international competition, individual player and club assignments; their reports are opinions and can be wrong.

### 12.3 Transfers and loans (pages 55–59)

Player search supports filters, player approach and shortlist actions. The manual describes the quick-search population as players from selected leagues plus highly reputed players elsewhere, so a web search over every imported row would not automatically reproduce original discovery limits. Recover that filtering boundary.

Buying uses a club bid followed by player contract negotiation; free agents are approached directly and can be invited for trials. The bid editor includes immediate fee, monthly installments, sell-on and appearance clauses, transfer date, public/private bid and player exchange. The manual documents six-to-24-month installments, sell-on up to 25%, and international-appearance thresholds in ten-match steps from ten to fifty. Treat these as **manual-version ranges requiring verification against 3.9.68 and Bosman**, not unconditional constants for every season rule set. It describes sell-on in terms of later profit; inspect settlement code to establish the exact basis, deductions and integer rounding.

Loan negotiation asks for duration, permitted cup participation and wage percentage. Both player and club respond through news. Selling/rejecting an offer can have morale consequences; making a bid public may attract competing bidders and unsettle a player. Transfers can be delayed; determine allowed delay count and termination conditions from code. Include trial acceptance/expiry/eligibility, loan cup permissions and wage sharing in implementation coverage even though they are not captured by a simple permanent-transfer state machine.

### 12.4 Club and manager continuity (pages 20–23)

Boards communicate seasonal expectations and react to performance. The manual lists title challenge, European qualification, promotion, play-offs, respectable position, avoiding relegation, battling relegation and squad improvement as expectation categories. These are message/intention categories, not recovered scoring thresholds.

Managers may approach the board for resources or patience, and repeated requests can affect their position. Stadium expansion is considered at season end; the manual mentions league capacity requirements and attendance above 90% of capacity as possible triggers, subject to money. This threshold must be verified in original code before it becomes an unconditional automation rule, and expansion spending must interact with available transfer resources.

Retiring a human manager removes that manager from participation but the world continues. The manual explicitly allows adding a new manager to that existing world. Therefore manager retirement, deletion of an authenticated web account and deletion of a saved world must be separate operations.

### 12.5 Player administration and contracts (pages 36–40)

Senior/reserve assignment, transfer listing, loan availability and promised squad role are separate player-administration controls. The manual says reserve matches require eleven actual squad players, excluding the supplemental greyed-out players; this is a specific case to verify against the original match scheduler rather than assuming those supplemental players always solve insufficient squads.

A free agent can be invited for a two-to-four-week trial under the documented rules. During a trial the manual permits selection in reserves and friendlies, and describes automatic departure at expiry unless a contract has been accepted. Record trial membership independently from employment and registration so the eligibility check can distinguish these cases. Verify exact offered durations and expiry inclusivity against the original.

Contract negotiations include basic pay, duration, goal/assist bonuses and release-related terms. The manual mentions relegation release and matching the highest wage at the club; those require distinct obligation triggers rather than a single boolean “release clause.” It does not enumerate every clause, numeric limit or acceptance criterion. Renewal can be declined before terms are negotiated, with a reason provided. Ending a player's contract by free release can involve paying out the remaining term and board reactions; original payout and exception rules remain to be recovered.

Disciplinary actions include official warning and fines denominated in weeks of wages, with the manual describing fines up to a month. It lists no stated reason, unprofessional behavior, prior-match dismissal, violent behavior and poor performance as selectable reasons. Treat the reason as part of the action record: its consistency with recent events can affect the reaction. Exact sanctions and response formulas remain unresolved.

Player histories include seasonal playing records, transfers/fees, achievements, injuries and bans. These must survive employment changes and retirement. Captain selection is a distinct responsibility; penalty takers have an ordered list. Squad-number assignment occurs at season start and when needed for new arrivals, with manual or automatic assignment. A shirt number is presentation/registration metadata rather than a playing-strength modifier according to the manual; test for any original competition restrictions before applying numbers.

Nicknames are viewer-specific in network play according to the manual. Store them on a manager/viewer preference record rather than globally renaming the shared staff entity. This prevents one human manager's personal labels from overwriting another's view or the canonical imported data.
