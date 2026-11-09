# Plan to complete the full technical specification

Planning date: 28 October 2026. Status: proposed execution plan; the phases below have not been completed by writing this plan.

## 1. Endpoint, scope and constraints

The endpoint is a verified technical document and its supporting schemas, rule tables and test fixtures from which an implementation agent can build the full selected game without inventing gameplay rules. Production Angular/Node application implementation is a subsequent project, described in chapter 06. Small parsers, emulation probes, reference builds and test-only translations are part of this research when needed to establish behavior.

The target is CM 01/02: Bosman Edition at the pinned repository revision, including its season selection, changing rules and enhancements, using the matched original 3.9.68 program. Include every player-facing feature and every background system needed to make them behave correctly. Preserve original, translator and Bosman behavior as separate layers; resolve which layer defines each target rule.

**Hopper is prohibited.** Use the existing translator, Capstone, original bytes, source/data inspection and isolated Unicorn execution. REA may be used only for a clearly specified supported operation that does not select or invoke Hopper. Do not spend a phase repairing Hopper permissions.

Keep supplied originals and existing saves unchanged. Run reference scenarios against disposable working copies and controlled save directories. Use the existing installation as a source of verified inputs; the older installed 0.5.1 host cannot validate newer repository enhancements. Obtain or build a reference matching the selected source before claiming runtime verification of those enhancements.

Do not add modern football rules, new online multiplayer, new AI features or redesigned simulation equations. Multiple human managers, ordinary game options and existing delegation still belong to the scope. Explicitly distinguish unavailable/stubbed networking in the current target from documented functionality in older original manuals.

## 2. Starting position

Reusable work already available:

- Source/data/function inventories, matched original and official patch identities, 46 patch-member checks and 106 Data-file checks.
- Seven substantive chapters covering architecture, gameplay inventory, competitions, data/UI, evidence, implementation planning and bounded original negotiation helpers.
- 11,752 mechanically translated routines with a searchable index; unsupported instructions, decode stops, traps and translator guards are recorded.
- 67,714 isolated cases/checks across 11 direct entry points, with bounded coverage. Most cases enumerate one small offer-state predicate.
- Extensive competition research tables and Bosman runtime source.

The remaining work is primarily semantic recovery and cross-system validation. Full match/quick-result behavior, player progression, complete negotiations, financial processing, managerial AI, original save-member semantics and end-to-end reference careers are not yet closed. Existing source names, emitted C and test counts are not completion metrics.

## 3. Effort, staffing and dependency model

Budget **60–120 person-days**, equivalent to **12–24 person-weeks** at five focused days per week. This is a low-confidence initial estimate of remaining research, verification, review and documentation effort. It is not a deadline, scheduled background job or guarantee that this chat will run continuously for that period. Exhaustive legacy behavior and difficult runtime dependencies may exceed the range.

With one specialist, person-days roughly correspond to working days before availability and external blockers. Independent workstreams may reduce elapsed time, but the critical path still includes the harness, complete match, cross-system integration and final review. Do not simply divide the estimate by the number of agents. Test runtime, review and integration costs remain.

| Phase | Focus | Person-days | Starts after | Exit milestone |
|---|---|---:|---|---|
| R0 | Scope ledger and measured pilot | 3–5 | Existing dossier | Scope pinned; recovery method demonstrated; estimate revised |
| R1 | Reference harness, world model and scheduler | 5–10 | R0 | Reproducible checkpoints and trusted foundational contracts |
| R2 | Complete match and quick results | 15–30 | R1 | Match specification covers all required phases and downstream effects |
| R3 | Training, health and player lifecycle | 7–14 | R1 | Development, injury and generation rules specified and tested |
| R4 | Transfers, contracts and finances | 9–18 | R1 | Entire transaction lifecycle and financial ledger explained |
| R5 | AI, scouting, board, news and manager career | 6–12 | R1; closes after R2–R4 | Human/AI/background decisions fully accounted for |
| R6 | All competitions and season changes | 5–10 | R1; closes after R2–R4 | Every target competition/rule variant has a verified disposition |
| R7 | Complete saves, UI actions and assets | 3–6 | R1; closes after R2–R6 | Persistent state and screen/action coverage reconciled |
| R8 | Integrated reference careers and independent review | 5–10 | R2–R7 | Cross-system scenarios pass; discrepancies resolved |
| R9 | Final Node/Angular contracts and agent handoff | 2–5 | R8 | Specification release gate passes |
| **Total** | | **60–120** | | |

R7's estimate is for final reconciliation: field/save/UI mapping begins in R1 and is maintained during every subsystem phase. R6 includes extending existing substantial competition work rather than starting research from nothing. Each estimate includes chapter updates and focused tests for that phase; R8 budgets cross-system validation rather than repeating all earlier tests.

Logical workstreams, if parallel work is explicitly arranged: simulation (R2/R3), management/economy (R4/R5), competitions/data (R6/R7), and an integrating owner for shared contracts and evidence. Every shared field, RNG stream, scheduler phase and save member has one accountable owner. A reviewer checks a feature independently from its author before closure.

## 4. R0 — Scope ledger and 3–5-day pilot

Actions:

1. Freeze the target source/artifact/rules identities and distinguish original, translator guards and Bosman enhancements. Inventory available season packs and missing reference assets.
2. Convert G01–G24, the screen/action catalog and competition families into a single requirement ledger. Give each requirement an owner, evidence links, dependency IDs, current status and acceptance scenarios.
3. Map original functions into features. Classify library/CRT/UI infrastructure and shared callbacks separately from gameplay, with a reason for any function considered outside the target. Do not drop unexplained routines merely because they lack names.
4. Select one nontrivial bounded match transition with an inspectable call chain. Recover its reads, writes, branches, randomness and return behavior. Add a small persisted-state example where feasible.
5. Execute original-instruction probes with ordinary, boundary and branch-distinguishing inputs; write complete implementable pseudocode; have another reader reconstruct the expected outputs from the document.
6. Measure hours per accepted behavior contract, dependency expansion, fixture preparation, failed harness attempts and review rework. Re-estimate the remaining phases using subsystem complexity, not functions-per-day alone.

Outputs: scope ledger; version/variant matrix; prioritized unknowns; one completed pilot feature packet; updated effort and critical-path estimates.

Gate: a nontrivial behavior is reproducibly specified and checked, and every target subsystem has explicit scope. If the pilot cannot isolate its dependencies, expand the harness phase or change the pilot to another representative boundary. Do not record a failed pilot as recovered gameplay or conceal the increase in effort.

## 5. R1 — Reference harness, canonical state and scheduler

Recover and specify numeric widths/overflow/rounding, dates, every relevant random stream/table, allocation-sensitive inputs, stable identity and table links. Complete the daily/slice scheduler's dependency boundary, including once-per-date versus once-per-slice gates, interruption/resumption, human turns and save/load reconstruction.

Build reusable research tools for controlled snapshots, action streams, return/state/event comparison and first-divergence reporting. Distinguish executable reference behavior from harness replacements for files, clocks, imports and UI. Check that a stub does not manufacture the result being tested. Use a native recompiled reference in a disposable environment where isolated execution cannot capture the required behavior.

Begin schema catalogs now: entity fields, units, signedness, enum/sentinel values, ownership, default construction, serialization and references. Trace writers and readers of each gameplay-relevant field. Map save members and screen actions incrementally as each field becomes understood.

Outputs: reproducible harness instructions; reference-build identity; state/field dictionary; scheduler pseudocode and transition graph; RNG/numeric contracts; snapshot and scenario formats.

Gate: the same recorded start and actions reproduce the same checkpoints under the declared environment; save/reload preserves the foundational state under test; output does not depend on undocumented harness defaults. Differences caused by allocation or timing are identified and captured rather than normalized away.

## 6. R2 — Match engine and quick results

Work from preparation through final settlement, following actual callers and state changes:

1. Fixture context: competition/stage, venue, weather, officials, attendance, eligibility, starters, bench and initial condition/morale.
2. Tactical interpretation: formation, with/without-ball zones, positional suitability, mentality, team/player instructions and inheritance, captain/set-piece order.
3. Simulation progression: clock, periods, movement, ball state, possession, action choice, passing, receiving, crossing, dribbling, challenges, headers, shots, saves and goals.
4. Incidents: fouls, advantage, offside, cards, injuries, restarts, substitutions, stoppages and added time.
5. Resolution: extra time, applicable tie rules, shootouts and exceptional termination states actually supported by the target.
6. Aftermath: ratings/statistics, condition/fitness, bans, morale, finance, records, history, news and competition updates.
7. Quick/background path: distinct outcome/statistics algorithm, detail selection and interleaving with watched matches.
8. Commentary/pacing: event-template selection, display versus simulation changes, speed-dependent scheduling and multiple human managers.

For each decision record the exact formula, constants/table lookup, all inputs, random draws and their order, clamping/rounding, branch guards and writes. Attribute names alone are insufficient: recover which attribute is used at which stage and how modifiers combine.

Outputs: match state schema; complete event taxonomy; ordered state machine; action/probability pseudocode; tactical-effect matrix; quick-result specification; post-match mutation contract; reference fixtures.

Gate: complete watched and quick matches have traceable state/event outputs across the specified scenario matrix. Boundary scenarios distinguish regulation/extra time/shootout, substitutes, injury/card effects and concurrent background processing. Every required reachable dependency has a specified body or explicitly mapped equivalent; a small helper test cannot close this phase.

## 7. R3 — Training, health and player lifecycle

Extend beyond the already tested training-load helper to actual training application, coaching allocation/quality, facilities, schedule/retraining effects, development limits, visible attributes versus ability, fitness/condition, fatigue and recovery. Recover injury/illness selection, durations, treatment, recurrence and medical opinions, including leave and rehabilitation actions.

Recover birthdays/age effects, growth/decline, retirement decisions, regenerated/new-player construction, names/nationalities, ability distributions, positions, personality and pool maintenance. Include player-to-staff transitions and other role changes supported by the reference. Trace each operation's daily/weekly/season cadence and save representation.

Outputs: player lifecycle state diagram; complete progression and health equations/tables; coach/physio interaction rules; generation schema/distributions; dated fixture sequences.

Gate: fixtures cover short-term effects and long-running convergence/limits, including save/reload and transfers between clubs/schedules. Supported inputs have explicit results and random behavior. Medical advice and generated player distributions are not inferred from manual prose alone.

## 8. R4 — Contracts, transfers and financial processing

Recover complete native status/enum meanings from their writers and consumers, extending the bounded helper predicates already established. Follow every user and AI transition: approach, bid, counter, hold/delay, reject, accept, negotiate terms, medical/permit/registration checks, future completion, failure/cancellation and expiry.

Cover permanent transfers, free agents, expiring contracts, renewals, loans, trials, exchanges, multiple competing offers, staff employment and player/staff dual roles. Specify all supported clauses and combinations, wage/bonus/status/length decisions, player willingness, club valuation/acceptance, response timing and AI budget checks.

Build the full game-finance ledger: units, cash versus budgets, all income/expense categories, wages/bonuses, fees/installments, compensation, sell-on/appearance triggers, facility/stadium spending, competition payments and any additional categories proven by code. Trace posting order, rounding, overflow and obligations across saves and seasons.

Outputs: typed offers/contracts/obligations; state-transition tables; exact valuation/negotiation algorithms; clause trigger catalog; ledger posting matrix and reconciliation fixtures.

Gate: full transaction histories reproduce reference outcomes and account changes; rejected/expired/canceled offers leave correct state; retries or restored checkpoints do not create duplicate postings. A signed contract, accepted bid, registration and completed move are separately represented.

## 9. R5 — AI, scouting, board, relationships and manager careers

Specify AI club management separately from Bosman assistant advice: team selection/rotation, formations, tactical changes/substitutions, recruitment targets, bidding/contracts, staff decisions, responses to performance and delegated holiday actions. Trace their inputs, priorities, thresholds, tie-breaks, randomness and scheduling.

Complete scout assignments, search populations, knowledge/visibility and decay, reports/accuracy, shortlists and recommendations. Recover board expectations/confidence, supporter/media interactions, requests/ultimata, job applications/offers, appointment/dismissal/resignation/retirement, unemployment and club/national-team dual roles. Include morale, unhappiness, disputes, fines, appeals, missed training and national duty.

Inventory every news class and action: how it is created, recipients, visibility, blocking status, expiry, available responses and effects. Separate presentation text from gameplay state and record any read-like operations that mutate state.

Outputs: AI decision trees; knowledge policy; board/career and relationship state machines; news/action catalog; holiday/multiple-manager control rules.

Gate: each target human or AI decision has a defined trigger, permissible choices, outcome calculation and durable effects. Hidden information stays distinct from estimates/opinions. Jobs, transfers, matches and news remain consistent after manager changes and save/reload.

## 10. R6 — All competitions, nations and season changes

Reconcile the 7,102-row corpus and supplementary inputs with actual original and Bosman implementation. Classify each of the reported 394 nation and 312 league encoding omissions: retained original default, intentional approximation, unsupported research encoding, genuine target limitation or unresolved implementation. Do not interpret every omission as a bug or dismiss them collectively.

Complete generic fixture/draw/stage engines and specific variants: entrant selection, seeding, group/league tables, head-to-head/mini-tables, tie-breaks, aggregate rules, play-offs, promotion/relegation, qualification/access lists, discipline, awards/prizes and histories. Cover domestic, continental club, national-team qualifiers/finals, Nations Leagues and new/restructured competitions present in the target.

Verify nation-dependent windows, work permits, nationality/eligibility, squad/bench/substitution limits and calendar-year versus cross-year seasons. Trace each competition's update date, rules freezing/changing, club/nation renames, database date shifts and post-last-known-season fallbacks. Inventory every selectable season data pack; missing packs remain a named blocker for affected scenarios.

Outputs: normalized, source-linked competition/rule packs; per-family algorithms; season-change order; version/fallback matrix; resolved gap ledger; representative era/boundary fixtures and coverage for every distinct encoded variant.

Gate: every target rule row and competition family has an explicit runtime disposition and test rationale. Exhaustive table-driven checks cover data variants; deep runtime cases cover each distinct algorithm and transition boundary. Modern regulations are not substituted for Bosman's actual behavior.

## 11. R7 — Save semantics, screen actions and asset completion

Finalize the mapping begun in R1: every required save member, field/link, pending operation, counter, RNG/scheduler state, cache and post-load rebuild. Specify supported legacy variants and web-save conversion, with original pointer/index relocation and exact Bosman pre-load ordering. Unknown bytes may remain only after proving they cannot affect required behavior or preserved round trips; raw preservation alone does not explain an active gameplay field.

Enumerate actual screen builders and action handlers against the manual/source inventory. For each screen document all fields, sorts/filters, tabs, contextual actions, visibility/permissions, enabled/disabled conditions, validation, navigation, confirmations and loading/empty/error/pending states. Include settings, multiple managers, notes/aliases, reserve controls, trials/leave/rehabilitation, FA/board actions, friendlies/tours and match controls.

Complete asset/font/picture/audio/language/commentary formats, conversion rules, source-to-world name mappings and provenance. Preserve existing data/asset licensing distinctions in the handoff.

Outputs: complete save/member schema; migration/restore order; screen/action matrix; asset/localization manifests; sample decoded states and round-trip fixtures.

Gate: every persisted gameplay field and every required user action maps to a documented rule and canonical entity. Restored reference scenarios continue correctly. Views cannot accidentally leak hidden data or trigger undocumented state mutations.

## 12. R8 — Integrated reference careers and independent implementability review

Execute progressively larger reference scenarios: new game and manager creation; ordinary day; full watched/background match day; transfer plus contract settlement; training/medical changes; month end; season transition; international qualification/finals; multiple seasons. Combine interactions that isolated tests miss, including transfers while on loan/injured, manager dismissal with pending decisions and season changes around outstanding fixtures.

Cover base/fixed-rules and changing-rules modes, each supported start season through data/structural checks, every distinct competition algorithm/era boundary through deep scenarios, active/background leagues and multiple human managers. Restore checkpoints before sensitive operations and compare with uninterrupted execution. Preserve traces and the earliest divergence.

Validate the specification, not an unbuilt production web app: check that documented pseudocode or small test-only translations reproduce reference outputs. Give a reviewer selected unseen inputs and the document/contracts; if they need to invent a rule or inspect the original to discover an omitted essential behavior, reopen that feature packet.

Outputs: scenario manifests/results; discrepancy resolutions; branch/variant coverage with exclusions justified; independent reconstruction review findings; final unresolved-obligation report.

Gate: no unexplained difference in required scenarios, no outcome-critical rule still unspecified, and no unresolved original/translator/Bosman conflict. Bounded tests must have a documented coverage rationale. Exhausting a finite test set is not proof of all possible states; static branch/dependency review supplies the complementary evidence.

## 13. R9 — Final web contracts and publication of the document

Reconcile the recovered behavior with Node.js/Angular design: complete entities/enums/units, command schemas, preconditions/errors, state transitions, atomicity/idempotency, projections and fog-of-war, event ordering, route/screen mapping, save schema/migrations and execution boundaries. Resolve the native-retention versus TypeScript replacement choice explicitly; retaining an opaque engine cannot be presented as explaining all its rules.

Resolve commentary-speed behavior and other proposed divergences. Faithful behavior remains the default. If a deliberate change is desired, document it as a separate versioned design decision requiring user selection; do not silently close a fidelity gap by changing the target.

Produce the consolidated document, editable chapters, machine-readable contracts/rules, evidence index, test vectors, reproduction instructions, glossary, dependency-ordered implementation tickets and agent starting prompt. Recheck framework compatibility when finalizing build guidance. A PDF may be added as a reading copy if requested; Markdown plus structured files remains the implementation authority.

Gate: all criteria in the next section pass and an independent reader confirms that required behavior is implementable without new gameplay design decisions.

## 14. Exact definition of a finished technical document

All of the following are required:

1. **Scope closure:** every required gameplay subsystem, supported variant, screen/action and background process has a requirement ID and disposition. No silently deferred feature.
2. **Algorithm closure:** every outcome-affecting rule has inputs, complete branch/transition logic, formulas/constants/tables, units/signedness, randomness, ordering, outputs and failure paths. No “use a realistic formula” or “implement original behavior” placeholder.
3. **State closure:** every required entity/field/enum is defined, with ownership, construction, visibility, persistence and references. Save/import/post-load behavior is explicit.
4. **Dependency closure:** required reachable functions/callbacks and translator guards are accounted for. Excluded CRT/platform/dead-code paths have evidence-backed reasons; unexplained names are not treated as exclusions.
5. **Evidence closure:** rules cite exact source/artifact identities. Original facts, Bosman changes, manual intent and web design remain distinguishable. Required conflicts are resolved.
6. **Validation closure:** primitive, subsystem and integrated scenarios pass within stated scope, with reference outputs and reproduction instructions. No known unexplained divergence remains on a required path.
7. **Agent handoff closure:** command/API/schema/UI contracts and implementation dependencies agree; a reviewer can implement representative features from the packet without inventing essential rules.
8. **Artifact closure:** links, identifiers, schema references, hashes and generated indexes validate. Large local-only generated references have reproducible generation instructions and prerequisites.

Only then change `full_game_implementation_ready` to `true`. If a required artifact, runtime path or behavior remains unresolved, report the affected requirements and keep the status false. Do not call a smaller scope complete without the user's explicit scope change.

## 15. Tracking and reporting

Track each requirement through `inventoried → dependencies traced → specified → reference tested → reviewed → closed`; allow a separate `blocked` state with an exact reason/next action. Store requirement ID, source/rule variant, owner, priority, dependencies, evidence, fixtures, reviewer and unresolved questions. Keep phase progress separate from function-name and test-case counts.

At each work checkpoint report newly closed behavior, evidence/test scope, remaining critical unknowns, blockers and the next deliverable. Re-estimate after R0, after the first complete match, and before R8. Count high-level behavior obligations and distinct algorithm/rule variants, not document words or thousands of combinations of one trivial predicate.

When a probe fails, distinguish harness/environment failure from game behavior, retain the failing input, and repair the harness before interpreting results. When a rule cannot be established, investigate its callers/writers, acquire a controlled reference scenario or identify the missing artifact. Never fill the gap by guesswork to meet a timeline.

This plan authorizes no automatic publication or external messaging. It specifies the remaining work; executing its phases is distinct from delivering this planning document.
