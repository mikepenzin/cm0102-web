# Agent implementation plan and completion contract

This chapter plans implementation of the game. For the preceding research needed to finish the full technical document, use [chapter 08, specification completion plan](08-specification-completion-plan.md), phases R0–R9.

## 1. How an implementation agent must use this dossier

Read the main specification, then architecture, gameplay, competitions, data/UI, evidence, and original-negotiation chapters. Treat explicit source/binary observations as evidence and web-design sections as target decisions. Read the linked source at its pinned revision before copying a formula. Use the generated-function index for original bodies, and cross-check original instructions when a translator guard or decoding issue could change meaning.

The user selected **Bosman Edition with season selection and enhancements**. Do not implement only the 2001/02 base game and call the request complete. The original 3.9.68 program, the installed 0.5.1 host and the newer repository are three separate version boundaries. The supplied `242.exe` is the official patch, not an additional community-rules profile.

This dossier provides implementation contracts, source pointers, bounded recovered algorithms, asset/data validation, and a full subsystem checklist. It does **not** currently close all original formula and state-machine gaps. An agent can begin infrastructure, importers, source-backed features and further recovery. It cannot honestly declare a complete faithful TypeScript game by filling unresolved fields with plausible defaults.

## 2. Required working conventions

- Do not use Hopper, including through REA's automatic provider selection. The user explicitly prohibited it. Use the repository translator, Capstone, source/data inspection and isolated Unicorn checks; historical Hopper failures are not outstanding setup tasks.
- Keep gameplay outside Angular and transport handlers. Engine inputs are validated commands against an explicit world revision.
- Keep data/rules and engine versions immutable for a running save. Record migrations and deliberate divergences.
- Preserve original numeric semantics, date conventions, randomness, iteration, scheduling and save/load effects.
- Use typed unknown/sentinel states; never coerce missing research to zero or a modern football default.
- Preserve original code/data sources and legal/provenance notices; do not commit supplied game assets indiscriminately.
- Preserve user work. Make web changes in new directories; do not replace the native reference implementation.
- Each feature is a vertical slice: command, validation, transition, persistence, projection, Angular interaction, and comparison tests.
- A test file existing is not a passing test result. Record exact command/environment/result and scope.

## 3. Decision records to carry forward

| ADR | Decision | Consequence |
|---|---|---|
| W001 | Angular frontend and authoritative Node backend | Browser cannot determine outcomes or expose hidden state |
| W002 | One writer per game world | Command serialization preserves cross-club ordering |
| W003 | Versioned compatibility profile | Original, Bosman, rules and data identities are reproducible |
| W004 | Pure TypeScript is the desired replacement boundary; native oracle retained during recovery | Unresolved formulas cannot be guessed; an optional retained-engine product must be labelled clearly |
| W005 | Own game-date/numeric/RNG adapters | JavaScript defaults cannot silently alter legacy semantics |
| W006 | Immutable snapshots plus durable command journal | Resume/retries cannot duplicate gameplay effects |
| W007 | Canonical stable IDs; original IDs preserved separately | Names/addresses are not web entity identity |
| W008 | Source-defined fog-of-war in projections | Hiding a browser field is insufficient |
| W009 | Source-defined competition rules including Bosman approximations | Real-world rule improvements require another profile |
| W010 | No unrequested new online multiplayer requirement | Preserve proven current behavior; hot-seat/multiple managers still require complete coverage |
| W011 | Commentary timing/scheduler coupling is an open fidelity decision | Visual-speed independence cannot be claimed as unchanged behavior until proven |
| W012 | Full recovery remains an explicit release gate | Mechanical translation coverage is not implementation completeness |

## 4. Ordered work packages

Each package has a concrete output and a stop condition. Dependencies can overlap for read-only investigation, but implementation must not bypass a required predecessor's contract.

| ID | Package | Depends on | Deliverable and exit condition |
|---|---|---|---|
| P00 | Freeze reference artifacts | None | Verify original/patch/data/rules digests, reference commit, environment; record reproducible manifests |
| P01 | Translation and call-boundary audit | P00 | Resolve all required generated entries, unsupported sites, injected guards and callback/import effects for first vertical slice |
| P02 | Original harness | P00 | Versioned isolated/subsystem harness with explicit stubs, memory, clock, RNG and assertion outputs |
| P03 | Numeric/date/RNG core | P02 | Exact helper implementations, table identity, boundary vectors and state continuation tests |
| P04 | Contract package | P00 | Runtime validators, IDs/revisions, error codes, command receipts, projections and wire serialization |
| P05 | Web shell/API/session storage | P04 | Login/ownership, game selection, routes, command queue, progress, health and operational logging |
| P06 | Data importer | P00,P03 | All indexed source tables/ranges, IDs/links, raw unknown bytes, pack manifest, no silent repair |
| P07 | Asset/language conversion | P06 | Reproducible converted packs and row-aligned names, commentary templates, fonts/pictures/sound |
| P08 | Rule-pack importer | P00,P03 | All 20 core parts plus supplemental files, typed sentinels, source URLs, per-module lookup semantics |
| P09 | World/scheduler recovery | P01,P02,P03 | Ordered slice/day/week/month/season transition graph, stopping conditions, persisted cursors |
| P10 | New game initialization | P06,P08,P09 | Correct season, leagues/detail, random players, managers/club/nation choices and initial world |
| P11 | Save/checkpoint engine | P04,P09 | Atomic snapshots, hashes, crash recovery, migrations, previous-save retention |
| P12 | Legacy save adapter | P01,P06,P08,P11 | Container plus each supported member schema/post-load rebuild; apply Bosman season-prefix/rename state before original member loading as chapter 04 requires; reject unknown variants |
| P13 | Profiles/navigation/search | P05,P06,P07 | All person/club/competition projections, masks, search population, history and persistent view state |
| P14 | Squad/tactics | P03,P10,P13 | Formation, zones, inheritance, positions, set pieces/captain, reserves and eligibility validation |
| P15 | Bosman assistant/comparisons | P03,P13,P14 | Exact assignment/scoring/ties, under-21/fitness filters, Best XI, polygon and attribute groups |
| P16 | Match preparation | P01,P02,P08,P14 | Lineups, bench limits, officials, venue/weather/attendance, state initialization |
| P17 | Full match transitions | P16 | Complete branch/field/RNG model and event taxonomy; watched match equals reference checkpoints |
| P18 | Quick/background results | P09,P16 | Distinct algorithm, statistics and scheduling integration; same downstream effects as reference mode |
| P19 | Match presentation/actions | P05,P17,P18 | Prematch/match/postmatch screens, allowed tactical changes/substitutions and commentary pacing profile |
| P20 | Competition administration | P08,P09,P17,P18 | Draws, fixtures, rules, tables/mini-tables, ties, qualification, discipline, prizes and history |
| P21 | Domestic season structures | P20 | Every supported nation/tier/cup era, promotion/relegation, reserve links and update dates |
| P22 | International/continental structures | P20 | Nations/qualifiers/UEFA/world/other continents, eligibility, call-ups, seeding and access lists |
| P23 | Training/fitness/medical | P02,P03,P09 | Load plus actual gains/recovery/injury/retraining/coaches/physio, reports and UI |
| P24 | Contracts | P02,P03,P09,P13 | Exact terms, clauses, permissions, proposals, acceptance, expiry/compensation, dual roles |
| P25 | Transfers/loans/trials | P24,P08 | Complete native state transitions, work permits, obligations, exchanges, future moves and news |
| P26 | Finance | P03,P09,P20,P24,P25 | Exact posting categories/cadence/rounding, budgets versus cash and reconciliation |
| P27 | Scouting and knowledge | P09,P13,P23 | Assignments, reports, visibility, decay, population filters, shortlists and advice |
| P28 | Board/manager careers | P09,P26 | Expectations, confidence, requests, jobs, appointments/dismissals, resignation and retirement |
| P29 | News/media/relationships/discipline | P09,P20,P23,P25,P28 | Every action, blocker and cause-linked reaction, fines/appeals/FA/media and histories |
| P30 | AI managers and delegated work | P14,P17,P23–P29 | Recovered selection/tactics/transfers/contracts/staff/holiday decisions and repeatability |
| P31 | Player lifecycle/regeneration | P09,P23,P24,P29 | Ageing, retirement, growth/decline, new-player generation, names and pool maintenance |
| P32 | Bosman long-career changes | P21,P22,P31 | Seasonal names/data shifts/competition creation and every source-defined fallback beyond researched years |
| P33 | Entire screen/action completion | P13–P32 | Every chapter-04 action and manual-defined current feature reachable, persistent and tested |
| P34 | Full career validation | P11–P33 | New game→match→month→season→major tournaments→long career, repeated with save/reload |
| P35 | Operational hardening | P05,P11,P34 | Capacity, backpressure, corrupt import tests, privacy, recovery and measured budgets |
| P36 | Release gate | All required packages | No undisclosed behavioral gaps in shipped scope; evidence ledger and reproducible acceptance results |

Ranges such as P23–P29 mean all listed packages in the range. A numerical package order is not permission to leave a domain out: the dependency table is the work graph.

## 5. First vertical slice

Implement a real narrow path first: load a verified pack; create a game in a fixed season; appoint one manager; render the squad with masked attributes; choose a lineup; save; reload; prove no world/RNG mutation from navigation. This delivers importer, canonical identity, contracts, persistence and Angular scaffolding without prematurely inventing match outcomes.

Then add one verified complete match and one competition's post-match integration. A fake result generator must not masquerade as this milestone. Retain all unrelated leagues and background logic behind the reference engine if using the explicitly labelled retained-engine route; disabling them changes the world and must be reported.

Expand after the first full transaction is proven: training day, transfer case, month of play, season change, then other competition families. Every new slice must operate in the same persistent world rather than remain an isolated demo screen.

## 6. Recovery matrix and unsatisfied obligations

| Domain | Useful evidence now | Still required for complete faithful replacement |
|---|---|---|
| Inputs/versions | Matched original, patch and base Data, source hashes | Full season-database availability/version verification |
| Dates/RNG | Concrete source and test harness; original random table location | Complete replacement vectors and preservation of all call ordering |
| Main scheduler | Inspected original day-loop order/gates | All called subsystem bodies, reentrancy and runtime continuation |
| Assistant selection | Detailed source-backed scoring/assignment | End-to-end selection/eligibility integration and same-world checks |
| Training | Exact category/intensity/load, 1,024 original vectors | Attribute gains, development limits, injuries, coach/retraining effects |
| Match | Some exact helper mutations/scaling, original-generated bodies | Complete movement/action/probability/ratings/period/substitution/aftermath logic |
| Quick results | Separate function-map/source leads | Full algorithm and influence on other shared state |
| Transfers/contracts | Data hints, Bosman loan fix, bounded original helper chapter | Complete offer state machine, AI responses, fees/clauses, permits and settlement |
| Finance | Ownership/atomicity contract, original source leads | Every posting formula/category/cadence and budget recalculation |
| Board/AI/media | Manual requirements and body locations | Exact scoring, decisions, reactions, durations and random draws |
| Competitions | Extensive source/tables, original comparator/points vectors | Original generic engines/callbacks, all country/stage boundaries, gap resolution |
| Nation restrictions | Source-specific encoders/lookups | 394 source-reported nation-rule encoding omissions and runtime fallback audit |
| League rules | Source-specific encoders/lookups | 312 source-reported league-rule encoding omissions and actual behavior audit |
| Saves | Container, Bosman extension, import constraints | Complete original member semantics and all post-load reconstruction |
| UI | Manual/source-backed action and setting inventories | Runtime field/action parity for every screen and state |
| Career | Season hooks/history/renaming and lifecycle leads | Multi-season original comparison, growth/regeneration, manager/club transitions |

The encoding omissions are not automatically missing gameplay features: some fields intentionally retain original rules, some descriptions cannot be fully encoded, and some may represent genuine gaps. Classify each using code and fixtures; do not blanket-ignore or blanket-count them as defects.

## 7. Feature dossier template

Use this schema for every implementation ticket or agent handoff:

```text
Feature ID / user-visible purpose:
Target profile and scope:
Original digest / repository commit / rule-pack identity:
Evidence status: source | static-binary | isolated-execution | full-scenario
Original entry points and source locations:
Typed inputs, units, bounds, signedness, sentinels:
Preconditions and authorization:
Complete branch/state-transition table:
Formulas, constants, tables, rounding and random draws:
Ordered read/write set and emitted events/news:
Scheduler phase, cadence, reentrancy, interruption rules:
Persistence/import/post-load behavior:
UI fields/actions, empty/error/blocked/pending states:
Expected examples and boundary cases:
Differential fixtures with provenance and command:
Deliberate divergences and version gates:
Unknowns with specific next probes:
Done criteria and test results:
```

A ticket containing only a name such as “implement realistic transfers” fails this contract. A vague “use the original formula” also fails unless the formula, dependencies and input/output states are supplied or an explicitly retained engine boundary handles it.

## 8. Repository completion checks

Before calling the full conversion finished:

1. Every gameplay inventory row G01–G24 has implementation ownership, explicit behavior and passing acceptance fixtures.
2. Every current UI action has a reachable route/command and a correct blocked/error state; no placeholder panels or silently ignored controls.
3. Every supported season/competition rule row has a known importer/runtime disposition, including sentinels and inherited original defaults.
4. All required original function dependencies are implemented or explicitly retained behind a working, tested engine boundary; unresolved generated-code traps cannot sit on a required path.
5. Every save restores canonical state, RNG, pending decisions, job cursors and post-load caches without divergence.
6. A complete career passes the multi-season and major-tournament matrix with both watched and background processing.
7. Privacy, replay/idempotency, corruption handling and resource limits are measured under realistic worlds.
8. The release description names any retained native dependencies and any deliberate deviations. A web UI alone is not described as a full TypeScript rewrite.

No automatic deployment, publication, purchase or external messaging is part of this documentation request. Implementation completion and release publication are separate actions.

## 9. Copyable starting instruction for a future agent

> Implement the CM 01/02: Bosman Edition web conversion in this repository using Angular and Node.js. Read `docs/web-port/CM0102-WEB-TECHNICAL-SPEC.md` and its evidence files first. Preserve the selected Bosman scope and pinned original/data identities. Start with P00–P15 dependencies and the first vertical slice; continue through the work graph. Do not invent unresolved gameplay algorithms, remove hard features, or mark a mock simulator as complete. Recover the required original bodies and validate them against the isolated/subsystem reference harness, or explicitly retain the verified engine boundary where that architecture is chosen. Keep all existing native/reference files intact. Update the evidence ledger and report actual tests, remaining gaps and any deliberate behavior changes. Completion requires the full criteria in chapter 06 section 8, not just a running development server.
