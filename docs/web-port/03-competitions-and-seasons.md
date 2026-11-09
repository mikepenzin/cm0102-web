# Competition, calendar, and season specification

This chapter specifies the Bosman Edition target at repository commit `cf2e8fa5f4cc816f252611a0e9c81e66d088358e`. It covers the base game, season games, domestic and international competitions, qualification, registration restrictions, season changes, and their persistence contracts. It is part of the web implementation specification, not a claim that every original competition algorithm has been recovered.

## Evidence labels and fidelity boundary

- **V — verified source:** the executable body of a repository function or the original instruction bytes was inspected, as identified by the citation. This establishes the inspected code path, not that the full game has been run successfully.
- **R — inherited research:** research tables, headers, comments, and previous investigation reports describe behavior. Original addresses are useful investigation targets but have not independently been disassembled in this pass.
- **D — web design:** a concrete implementation contract proposed for the Node.js domain model.
- **U — unresolved:** exact original behavior or a path through an external original function remains unverified.

Paths below are relative to the repository root. `path:L123` means the source location at the pinned commit. Functions named `o_00xxxxxx` call the translated original. A wrapper body can establish its own changes; it does not establish the algorithm of the original call it delegates to. Function-map names alone are not implementation evidence.

The Bosman Edition preserves several deliberate approximations. A web port targeting that edition must preserve these as versioned compatibility rules; it must not silently replace them with modern football regulations. A separate enhanced-rules profile can change them later. After the initial source pass, the original was located in the installed 0.5.1 application; section 12 records direct original-code verification. The installed application version is older than this repository's season enhancements and is therefore not a runtime oracle for those enhancements.

## 1. Rule data that already exists

### 1.1 Authoritative repository inputs

The research corpus has 20 main TSV parts, totaling **7,102 rows**, validated by `python3 -I tools/seasons/make_rules.py --check` during this investigation. This validates encoding and structural consistency, not sporting accuracy or runtime coverage.

| Part | Rows | Coverage |
|---|---:|---|
| `arg-usa.tsv` | 324 | Argentina and United States leagues, cups, league cups, super cups, all-star game, nation rules |
| `aus-jap-kor.tsv` | 378 | Australia NSL/A-League/cup; Japan J1/J2/cups; South Korea league/cups; nation rules |
| `bra.tsv` | 1,053 | Brazil national tiers, state championships, regional cups, domestic cup and super cup |
| `club-world.tsv` | 432 | Continental club competitions outside UEFA, intercontinental competitions, Club World Cup |
| `den-swe.tsv` | 324 | Denmark and Sweden leagues, cups, nation rules |
| `eng.tsv` | 324 | Five English league classes, domestic cups/trophies, Community Shield, nation rules |
| `fra.tsv` | 243 | French first through lower tiers, national/league cups, super cup |
| `ger.tsv` | 243 | German first/second/third tiers and regional classes, cups and super cup |
| `gre-pol-cro.tsv` | 486 | Greece, Poland, Croatia leagues/cups/super cups |
| `hol-bel.tsv` | 297 | Holland and Belgium leagues/cups/super cups |
| `int-continental.tsv` | 297 | Continental national-team finals/qualifiers, Nations Leagues, Finalissima |
| `int-world.tsv` | 325 | World Cup and all qualifying confederations, Olympics, Confederations Cup, hosts/nation rules |
| `ita.tsv` | 432 | Italian first/second/Serie C tiers and groups, domestic/lower-tier cups and super cups |
| `nor-fin.tsv` | 270 | Norway and Finland leagues/cups/super cups |
| `por.tsv` | 189 | Portugal leagues, cup, league cup, super cup |
| `rus-tur.tsv` | 324 | Russia and Turkey leagues/cups/super cups |
| `sco.tsv` | 216 | Four Scottish tiers, national/league/challenge cups |
| `spa.tsv` | 162 | Spanish first/second and lower grouped tiers, cup and super cup |
| `uefa.tsv` | 216 | Champions League, UEFA Cup/Europa League, Conference League, Intertoto, Super Cup, access lists |
| `wal-nir-irl.tsv` | 567 | Wales, Northern Ireland, Republic of Ireland and All-Ireland competitions |

All paths in this table are under `docs/research/rules-0.6.0/`. Retain every row, source URL, note, original text field, and ID when importing. Supplementary inputs include `*-clubs.tsv`, `eng-academy.tsv`, `uefa-access.tsv`, `uefa-ko-dates.tsv`, `nations-league-tiers.tsv`, `nations-league-dates.tsv`, database signatures and date shifts. These are **not** interchangeable with the main table.

The native nation-rule table contains **27 nations** (`src/recomp_rt/win/season_nation_table.c:5`). The runtime's **35 league-table entries** are scheduling/ownership entries, not a declaration of 35 playable domestic nations. `aut-sui-clubs.tsv` exists as supplemental club data; there is no main Austria/Switzerland season-rule part among these 20 files. Do not infer complete domestic simulation for a nation from a database club record.

### 1.2 Encoded row contract

**V:** `tools/seasons/make_rules.py:1`, `src/recomp_rt/win/season_rules.c:17`, `season_rules.h:19` define the import format:

```text
id, season, nation, comp_file, comp, tier, name,
clubs, up, down, subs_used, bench, playoffs, split, format,
clubs_text, up_text, down_text, subs_used_text, bench_text,
playoffs_text, split_text, format_text, real_season,
foreign, work_permit, registration, windows, notes, sources
```

Counts are either `0..100000` or one of six distinct states: `na`, `unknown`, `not_held`, `see_each`, `as_cl`, `text`. Native sentinel values are `-1..-6` respectively. An absent rule is not zero; `unknown` is not `not_held`; `text` cannot be coerced to a complete numeric rule. The encoder sometimes extracts only the first number or sums `N direct + M via ...`; the original text must remain available to distinguish direct places from play-off places.

`comp_file` may hold a semicolon-delimited list. Lookups match a complete token. A new competition identified by `comp_file = new` also has a synthetic `new_` name derived from lowercase competition text with spaces changed to underscores. Source: `season_rules.c:209–243`.

The native loader rejects the entire table on structural corruption: wrong format/version/header/column order, missing final newline, invalid counts, invalid season, unexpected row count. Bounds: 100,000 rows and a 64 MiB allocation limit. Missing/damaged table results in an empty table and native fallback behavior. **D:** the web importer should fail validation visibly and prohibit creating a supposedly season-faithful game from incomplete required rule data; never quietly treat the empty table as all competitions absent.

### 1.3 Do not flatten all lookups into one fallback rule

Different native modules implement different policies:

| Lookup | Actual policy | Source |
|---|---|---|
| Raw rule row | Exact season and matching competition token | `season_rules.c:237` |
| Nation state | Ordered cumulative application of all rows up to season; `KEEP` leaves earlier value | `season_nation.c:13` |
| UEFA format | First row before range; latest row at/before season; last row after range | `season_uefa_lookup.c:9` |
| UEFA access list | None before first; last season after last; bracket fallback to next bracket, then final bracket | `season_uefa_lookup.c:92` |
| Cup round format | Latest row not later than season, else original round | `season_cups.c:6` |
| League size/play-off raw row | Search requested season backward at most 10 seasons | `season_sizes.c:95`, `season_playoffs.c:176` |
| Substitute counts | Exact-season rows; first usable value for each field | `season_subs_rule.c:9` |
| New competition rules | Latest matching competition row up to season | `season_comps.c:170` |

The documented intention “last known rules remain” is therefore broader than some actual implementations. In particular, a league lookup may return no row after a ten-year gap. This is a fidelity issue to test for long saves, not a reason to invent new rules.

## 2. Web domain contracts

**D:** use explicit serializable objects with IDs, not native pointers, positional source names, or Angular component state. Preserve native IDs separately for comparison/import.

```ts
type RuleValue<T> =
  | { kind: 'value'; value: T }
  | { kind: 'unknown' | 'notApplicable' | 'notHeld' | 'inherit' }
  | { kind: 'described'; text: string };

interface RuleEvidence {
  rowIds: string[];
  sourcePaths: string[];
  status: 'sourceVerified' | 'inheritedResearch' | 'proposed' | 'unresolved';
}

interface CompetitionEdition {
  id: string;
  competitionId: string;
  editionYear: number;
  ruleSeason: number;
  rulesVersion: string;
  status: 'notHeld' | 'pendingEntrants' | 'drawPending' | 'active' |
          'waitingForDependentStage' | 'finished' | 'archived';
  entrants: string[];
  stages: Stage[];
  currentStageId?: string;
  finalPositions: Record<string, number>;
}

interface Stage {
  id: string;
  kind: 'league' | 'groups' | 'knockout' | 'leaguePhase';
  entrants: string[];
  roundIds: string[];
  fixtureIds: string[];
  rankingRuleId: string;
  qualificationRuleId: string;
  expectedMatchesPerEntrant?: number;
  completion: 'pending' | 'active' | 'finished';
}

interface FixtureRuleSnapshot {
  ruleVersion: string;
  bench: number;
  substitutionsUsed: number;
  extraTime: boolean;
  penalties: boolean;
  awayGoals: 'none' | 'afterRegulation' | 'afterExtraTime';
  legs: 1 | 2;
  replayLimit: number;
  eligibilityRuleId: string;
}
```

Each fixture snapshots its applicable rules at creation. Later season changes must not retroactively rewrite completed fixtures. If the native path intentionally changes an unplayed fixture, record the change as an explicit domain event with before/after rules. Keep competition identity separate from season-specific display name; rename chains must not break history, keys, saved references, or navigation.

Required events: `CompetitionEditionCreated`, `EntrantsResolved`, `DrawCompleted`, `FixtureScheduled`, `FixtureRescheduled`, `MatchResultAccepted`, `StandingUpdated`, `StageCompleted`, `QualificationGranted`, `PromotionDecided`, `RelegationDecided`, `CompetitionFinished`, `SeasonChangeDeferred`, `SeasonRulesApplied`, `CompetitionRenamed`, `SeasonNewsAdded`.

The exact native point where each event becomes authoritative needs differential verification. Persist the transaction boundary and RNG state together. An edition cannot move to `finished` while required ties, replay fixtures, or qualification inputs are unresolved.

## 3. Season selection and lifecycle

### 3.1 Four separate concepts

1. **Start year:** e.g. 2010 for 2010/11.
2. **Data set:** selected roster/database content and its own source date.
3. **Changing rules:** whether later seasons change format/rules.
4. **Competition edition year:** may be a calendar year different from the season-row key.

The user-facing release offers seasons 2001/02–2025/26 according to its README; native save and test inputs accept start years 2001–2099. The research table also includes later rows. Acceptance of a year by the parser does not prove that a roster data set exists for it or that its rules are historical facts.

**V:** `season_set_new_game` validates the range, stores next-game values, and only changes the current start-screen date if no game is running (`season.c:120`). Creating a new game applies the selected start year and changing-rules value, resets rule state, prepares data-date shifts and international changes, executes the original creation, then initializes season hooks (`season.c`, wrapper `0x828110`). Loading reads season metadata **before** original loading. Ending a game resets to the next-game settings (`0x82ff40` wrapper).

**V:** a base game is start year 2001 with changing rules off. No season rule kinds execute in that mode (`season_change.c:635`). `RECOMP_SEASON_CODE=off` is a reference-test bypass, not a normal web gameplay option. A later-year game with changing rules off still applies its start-season rules, then freezes that format profile.

### 3.2 New game pipeline

**D:** implement a transactional pipeline with intermediate diagnostic snapshots:

1. Resolve selected season, database manifest, league selection, manager setup and rule profile.
2. Validate all required data files and their identities.
3. Load records and resolve IDs before any trimming or league construction.
4. Apply database-specific date shifts; never use a single blind year offset for all fields.
5. Apply start-season club/division membership and nation/competition identity changes.
6. Trim/load the selected simulation population using verified original rules when available.
7. Create competition objects and parent/child stage references.
8. Run the start-season calendar to the actual playable start date with the appropriate season hooks.
9. Construct fixtures with immutable rule snapshots and persist RNG state.
10. Publish the initial manager state only after initialization completes.

Steps 3–8 are intentionally ordered: `season_clubs.c` hooks the beginning of original `database_trim_staff_tables 0x524d70`; changing club memberships after fixtures or trimming can produce incorrect entrants and missing players. The exact original trimming and startup simulation algorithms remain **U** until the original bodies are inspected.

### 3.3 Competition-specific season change

**V:** `season_change.c:740–820` wraps original daily season pass `0x5d5ae0`. The game has no single universal “1 July rollover.” Each league-table entry has its own update day; the code checks all 35 entries. It derives season S as game year plus one when the stored day-of-year is at least 273. The season point is the first due-entry pass with day-of-year 151–272 and a newer season point. These are literal numeric thresholds in source; do not silently replace them with Gregorian date predicates without checking leap-year behavior.

The verified hook order is:

```text
prepare friendly-date constraints
temporarily hold entries awaiting play-offs
find entries whose update day is today
if required: apply once-per-season hooks
apply entry hooks for due entries
for each original competition start:
    before hooks
    original start_new_season unless skipped
    after hooks (started=true/false)
after hooks for due competitions that did not start
send/update season news
restore held entry dates
```

Rule-kind registration order is part of behavior: new competitions; names; Italian Serie C; substitutes; nation rules; domestic cup formats; cup breaks; new cups; size news; UEFA; club-world; new club cups; promotion play-offs; internationals; new divisions (`season_change.c:60`). Preserve this sequence before attempting parallel computation.

Start-season hooks are distinct from later changes. For a game beginning after 2001, entries due before the playable start date are processed during creation; later entries process on their dates. Entries with update days from the October threshold have already begun their start-season edition, so new-game handling runs entry/after hooks without re-running original `start_new_season`. Start-season hooks produce no season-change news. Later hooks require changing rules on (`season_change.c:703–729`, `764–808`).

### 3.4 Save/load rules state

**V/R boundary:** `season.c` implements a `bosman_season.dat` part. Header fields are little-endian version, byte length, start year, changing-rules flag. Version 1 is 16 bytes; version 2 adds news texts, rename mappings, and season-point state through `season_change.c:430–511`. A save with no part loads as 2001/changing-rules-off. A damaged part blocks loading before the original loader runs. Known fields from later versions are accepted only within valid declared length.

**D:** web saves must include start year, changing-rules mode, rules-version identity, edition states, active stages, fixed draws/fixtures, season-point markers, historical names, season news, deferred-rollover state, promotion decisions, and RNG state. Rebuild only derived caches on load. Never regenerate a draw or rerun promotion because a client reconnects.

## 4. Domestic leagues, standings, and movements

### 4.1 Match and table invariants

**D:** a standings row must retain played, wins, draws, defeats, goals for/against, points, deduction/adjustment history, home/away splits, head-to-head inputs where applicable, and stage-specific carryover values. The original's exact per-league points/tie-break/deduction logic is **U** in this pass. Do not default all leagues to points → goal difference → goals scored and claim parity.

Implement distinct rankers selected by rule ID. Ranking can produce ties requiring play-offs rather than an immediate arbitrary ordering. Stable UI ordering is separate from sporting qualification. Store official penalties as events with timing and scope, not permanent mutations of raw match results.

For an ordinary league of N clubs meeting M times, the proposed fixture validator expects each pair M times, each club `(N-1)*M` matches, and `M*(N+(N%2)-1)` match days. **V:** the Bosman `rt_league_dates` uses the latter expression for supported reduced-size leagues, clamps meetings to 1–4 with fallback 2, and only shortens the original date count (`season_sizes.c:193`). Special split/stage/league-phase competitions require explicit expected-match counts; do not apply the ordinary formula to them.

### 4.2 Size rules and native limits

**V:** `season_sizes.c:95–206` distinguishes requested table count from actual club records in a division. Classes have floors/caps. `rt_league_clubs` uses actual members, capped by the class and Italian special handling. It resets a stale ordinary expected-match count when size changes, including when a division returns to its old size. A special expected count such as a split league's count must remain intact.

Notable behavior:

- England's third-tier class has a floor of 23 when the **start year** is 2019 (`floor_of`). The start-year condition is literal and is not a general rule for every 2019 edition reached in a long save.
- Denmark's first-division hook may retain database membership rather than the smaller research count so its cup entrant collection does not underfill (`season_sizes_keeps_database`).
- MLS is capped at 12 in the edition's compatibility model; the research's expanded club count is not automatically playable. Source: `docs/release/0.6.0-rule-changes.md:7`, class capacities in `tools/recomp/league_sizes.py` (**R**, original class capacity requires original verification).
- In later seasons, simulation results determine the clubs that move; historical real-life club lists are initialization data, not a script forcing future winners or relegations.

**D:** expose `requestedCount`, `effectiveCount`, `compatibilityReason`, and `evidence` in internal rule reports. Product UI can show the effective rule. Never mutate club memberships merely to make a report match a researched count.

### 4.3 Promotion/relegation transaction

Required implementation sequence (**D**, to be checked against original `0x689c80`): complete all dependent tables/stages; determine direct and play-off outcomes; apply reserve-side eligibility; resolve vacancy/size constraints; create a single movement plan; validate no club moves twice and all destination counts are valid; atomically apply memberships; archive final standings; construct next editions.

**V:** native added promotion play-offs are narrow, not a universal play-off interpreter. `season_playoffs.c:150–202,582–611` requires a plain League class, four consecutive promotion play-off places immediately below direct places, a compatible upper league, and usable encoded data. It explicitly rejects “two winners” formats. Classes with original play-off builders keep those builders. A text encoding successfully does not mean this generic path runs it.

For the added four-club form, pairs are bottom qualifying place versus first qualifying place and the middle two. The new stage is based on English two-leg semi-finals plus a final. Source includes special Segunda final gaps: 3 days before 2020; then 7, 8, 6, 7, 6, 6 for 2020–2025, retaining 6 thereafter (`final_gap`, `season_playoffs.c:97`). Do not derive these from an assumed weekly schedule.

The routine also protects season change when a play-off overruns its update day. `season_playoffs_hold` temporarily hides that update day and exposes the first later completed day, with a 60-day waiting boundary (`season_playoffs.c:679`). At the boundary the original completion check can still refuse the chain. The plain-league unfinished-season safety net is logged as a failed soak-test condition; it is not proof of successful recovery. Save/reload during this wait is a mandatory test.

### 4.4 Splits, grouped divisions, reserve sides

The encoded `SeasonLeagueRule` has `po_up`, first play-off place, `po_down`, `exact`, `split_after`, `split_top`, calendar kind and split-points policy. `exact=0` means a partial encoding. A row describing “points halved” is not itself a verified implementation of rounding, tie priority, or later reversal of rounding.

Country-specific reconstruction entry points:

| Family | Required separate behavior | Existing implementation/research anchors |
|---|---|---|
| Scotland | 33-match split followed by groups; different upper/lower-tier play-offs | `season_league_table.c`, Scottish original class maps; `test_encode_leagues.py:71` |
| Belgium/Poland/Denmark | Split timing, group membership, points carryover/halving, promotion constraints | `season_league_table.c`, `2026-10-18-league-rules-gaps.tsv` |
| Argentina | Apertura/Clausura and zone structures, average-table relegation, transition calendars | `season_arg_zones.c`, `season_arg_zones_table.c`, `arg-usa.tsv` |
| Brazil | National tiers plus concurrent state championships; state allocation and regional membership | `season_bra_states.c`, `bra.tsv`, `bra-clubs.tsv` |
| Germany | Existing regional class repurposed for 3. Liga, links to upper/lower divisions | `season_divisions.c:190–538` |
| Italy | Serie C1/C2 to three-group Serie C, changed group capacities and promotion stages | `season_divisions_ita.c`, `ita_serie_c_run.py` |
| Spain/Portugal | Grouped-tier class reuse, reserve-side eligibility, parent-club constraints | `season_divisions.c:574–995` |
| Greece/Turkey | Added Greek second-tier group; Turkish Category B groups for the selected database | `season_divisions.c:1096`, `season_tur_groups.c` |

Rows above are discovery and implementation scopes, not assertions that every listed behavior is fully reconstructed. Every original ranker, group-to-division assignment, and reserve eligibility check must be read and differential-tested before parity is claimed.

## 5. Domestic cups and super cups

### 5.1 Round-format operations

**V:** `season_cups.c:22` sets these exact modes while preserving non-rule bits from the first-match flags:

| Code | Legs | Replay | Deciding behavior |
|---|---:|---:|---|
| `.` / unrecognized | unchanged | unchanged | Leave all fields unchanged |
| `E` | 1 | 0 | Extra time then penalties |
| `P` | 1 | 0 | Penalties after regulation |
| `R` | 1 | 1 | Replay; extra time then penalties in replay |
| `S` | 1 | 1 | Replay; direct penalties in replay |
| `L` | 2 | 0 | Away goals, extra time, penalties |
| `N` | 2 | 0 | No away goals; extra time, penalties |
| `Q` | 2 | 0 | No away goals; penalties without extra time |
| `H` | 2 | 0 | Away goals only after extra time |

A newly introduced replay gets gap 10 days if there were previously none. A single-leg round converted to two legs gets gap 7 days; an existing two-leg gap is retained. The function changes only specified fields; `E` and `P` do not rewrite the stored deciding-match flags because those are not used by their single-leg mode. Native bit values: penalties `0x1`, extra time `0x2`, no away goals `0x4`, away goals after extra time `0x4000`; mask `0x4007`. These are serialization compatibility facts, not recommended web bitfields.

### 5.2 General new-cup arithmetic

**V:** `season_cups_more.c:201` supports 2–32 entrants. Let P be the largest power of two not exceeding N. If N=P, round count is `log2(P)` and first-round ties P/2. Otherwise round count is `log2(P)+1` and first-round ties N−P; other entrants receive byes. Preserve entrant ordering and draw RNG separately; arithmetic alone does not specify who receives a bye.

Round codes are 150 final, 130 semi-final, 120 quarter-final; earlier rounds use `10*(r+1)`. Group-size distribution is `floor(N/G)` plus one for the first `N%G` groups; each group must have 2–10 clubs. Group match days equal `(N odd ? N : N−1)*meetings` (`season_cups_more.c:274`).

### 5.3 Four-club super cups

**V:** `season_super_four_pick` starts with league champion and runner-up. Cup winner and runner-up are added; missing/duplicate cup qualifiers are replaced by the next distinct league club, scanning from third. The semi-final order is league champion versus cup-runner-up slot, then cup-winner slot versus league runner-up. Failure to find four distinct clubs returns 0; the native caller can retain the original two-club format. Sources: `season_cups_more.c:88`, `season_super_cups.c`.

### 5.4 Edition-specific departures from researched real rules

**V/R:** these limitations are encoded in `season_cups_more.c:107–261` and must be visible in the fidelity register:

- All-Ireland cup group formats before the supported knockout years are not played by this new-cup implementation; supported 2010–2012 editions use 12 clubs and 2013 uses 8.
- New National League Cup from 2024 is disabled in this table because the implementation lacks required under-21 academy entrants.
- EFL Trophy groups use the game-held clubs rather than adding absent academy teams.
- Scottish League Cup penalty-win bonus points are not implemented by these hooks; ordinary draws remain draws.
- Turkish Cup group counts adapt to the original class's available entrant count, rather than reproducing every researched real format.
- Several domestic super-cup dates are explicitly author inferences in comments and table constants.

A web agent should reproduce the target's effective behavior first. “Fixing” one of these changes the target; track it as an optional divergence with separate tests.

## 6. UEFA club competitions

### 6.1 Data and participation

`season_uefa_table.c` contains the generated format rows and access-list rows; source schema is `season_uefa.h`. They describe rounds, entrants, groups, meetings, advancing clubs, losers entering another competition, away-goals mode, suspension lists, Super Cup venue/date/extra time, and league phases. `season_uefa_lookup.c` implements effective-season and access-bracket selection.

**D:** represent qualification as a directed graph of slots. A slot can be sourced from domestic rank, domestic cup holder, continental holder, fair-play allocation, prior-round winner/loser, or a replacement policy. Resolve duplicate qualifications in the target's verified order; do not allocate the same club twice. Maintain separate association rank, club seeding rank, historical holder, current domestic standing and entry round.

Original association-coefficient calculation and seeding comparator `0x586f60` are **U** in this pass. The league-phase wrapper calls that comparator; it does not supply its implementation. Do not invent an Elo or coefficient model as a substitute.

### 6.2 Formats across seasons

The target includes old two-group-stage Champions League, later group-plus-round-of-16 format, changed UEFA Cup/Europa League groups, qualification paths, Conference League, suspended-association handling, and league phases. Use `season_uefa_row` as the format selector rather than scattered year checks. Specific format thresholds and counts are in the committed generated rows, not in the README.

In a new Conference League object, the native code reuses the UEFA Cup class and copies its competition level 18. Thus team selection and fixture priority may treat it like the Europa League; it is not automatically a separately tuned prestige model (`season_comps.c:51–60`, **R** for original downstream readers, **V** for definition).

### 6.3 Verified 36-club league-phase draw

**V:** `season_uefa_league_draw.c:1–295` provides a portable implementation suitable for differential translation.

Inputs: 36 entrants already ordered by the caller's seeding, each entrant's nation ID, an RNG callback returning an integer in `[0,n)`, and either 8 or 6 match days. Eight-day mode has four pots of nine; six-day mode has six pots of six.

Eight-day mode:

1. Within each pot, construct a permutation with no fixed points or two-cycles; each club hosts one pot peer and visits another.
2. Between each distinct pair of pots, construct two bijections, one in each hosting direction, excluding repeated opponents.
3. Randomize the block order and backtracking candidate order.

Six-day mode:

1. Within each pot, construct a perfect matching.
2. Between each pair of pots, construct one bijection.
3. Orient cycles across paired pots so every club gets three home and three away fixtures.

Initially reject same-nation opponents and more than two opponents of any other nation. Always reject self-play and repeated opponents. Search uses at most 20,000 recursive steps per relevant block/day search. After 200 whole-draw attempts the “at most two from a nation” constraint is relaxed; after 400 the same-nation constraint is relaxed too. Stop after 600 attempts, or on a negative RNG result. An out-of-range nonnegative RNG result is clamped to n−1; n≤1 returns zero without an RNG call. This fallback is Bosman behavior, not a complete formal reproduction of UEFA draw constraints.

The match-day allocation repeatedly finds a perfect matching among undated edges, starting from the lowest free club; up to 50 date-allocation attempts precede a new opponent draw. Success must give every club 8 or 6 distinct opponents and exactly half its matches at home. Store `relax` as diagnostic evidence.

`season_uefa_league.c:180–241` integrates the draw through the original fixture creator with a fixed list and explicit expected matches of 8 or 6, avoiding a 35-match round-robin completion requirement. Its tie-break setup uses goal difference then goals scored; later real-world criteria are not supplied by this implementation.

### 6.4 Verified league-phase progression

**V:** ranks 1–8 advance to round of 16, 9–24 enter knockout play-offs, 25–36 exit. Play-off pair groups are:

| Seeded pair | Unseeded pair | Round-of-16 seeded pair |
|---|---|---|
| 15/16 | 17/18 | 1/2 |
| 9/10 | 23/24 | 7/8 |
| 13/14 | 19/20 | 3/4 |
| 11/12 | 21/22 | 5/6 |

For each row, three binary RNG calls assign the two seeded clubs, the two unseeded clubs, and two round-of-16 seeds to the corresponding bracket halves. The unseeded club hosts the first leg. Preserve the exact bracket slot order, not merely valid rank ranges. Sources: `season_uefa_league_draw.c:297`, `season_uefa_league.c:255`.

### 6.5 Scheduling and transitions to test

Dates have competition, season, round kind, draw day/month/year offset, first-leg day/month/year offset, and leg gap. Relevant sources: `season_uefa_dates.c`, `season_uefa_days.c`, `season_uefa_dates_table.c`, `season_uefa_days_cl.inc`, `season_uefa_days_uc.inc`, `uefa-ko-dates.tsv`. Some dates beyond the last table use adjusted weekdays and a final-date bound; this needs exact translation, not generic weekly placement.

Mandatory transitions: two-group to one-group Champions League; new qualification paths; end of Intertoto; Conference League creation; no-away-goals editions; suspended association; league phases; frozen-rule saves progressing past each transition; save/load before draw, between legs, and between league phase and knockout.

## 7. Other continental and world club competitions

Every family below must retain its own format and eligibility; no universal “continental cup” preset is sufficient.

| Competition family | Rule/input key | Implementation anchors |
|---|---|---|
| Libertadores, Mercosur, Merconorte, Sudamericana, Recopa | `libertadores`, `mercosur`, `merconorte`, `sudamericana`, `recopa` | `season_club_cups.c` (including its tables), `club-world.tsv` |
| Asian Champions, Cup Winners, Super Cup, ACL Two | `asian_champions`, `asian_cup_winners`, `asian_super_cup`, `acl_two` | `season_club_cups.c`, `season_kind_club_world.c` |
| CONCACAF Champions and Inter-American | `concacaf_champions`, `inter_american_cup` | Original class plus `season_kind_club_world.c`, `season_club_cups.c` |
| Oceania Champions | `oceania_champions` | `season_kind_club_world.c`, `season_cup_breaks.c` |
| Intercontinental, Club World Cup, new FIFA Intercontinental | `intercontinental_cup`, `club_world_cup`, `fifa_intercontinental_cup` | `season_kind_club_world.c`, `season_club_cups.c` |
| Suruga Bank | `suruga_bank` | New-competition definition in `season_club_cups.c` |

**U:** exact entrant replacement, regional restrictions, host-club qualifications, cross-competition priorities, underlying match-result handling, and all edition draw paths have not been exhaustively traced in this pass. Implement these by following the actual `SeasonCompDef` and handler branch for each edition, and preserve unsupported/not-held rows explicitly. The research row is necessary context but not executable truth.

## 8. International national-team competitions

### 8.1 Required model distinctions

**D:** national-team membership differs from club employment. Persist eligible nationalities, declared nation where represented in the target, senior/under-age squad, call-up/release status, availability, competition squad registration, qualifying group and edition. A host allocation is separate from a normal qualifier slot. Multiple hosts cannot consume one another's qualification slots.

Core families: World Cup, European Championship and qualification, Copa América, Africa Cup of Nations, AFC Asian Cup, OFC Nations Cup, CONCACAF Gold Cup, Confederations Cup, Olympics, UEFA Nations League, CONCACAF Nations League, Finalissima, and every World Cup confederation qualification path. The `African Nations League` research row is not evidence of an implemented competition object.

### 8.2 Edition rules

`SeasonIntlEdition` holds file/class, rule-season key, edition year, finalists count, golden-goal flag, finals start year/month/day, host names, and source row (`season_intl.h:17`). Month numbering here differs from native Date and some round tables; normalize explicitly at import. Relevant bodies are `season_intl_rules.c`, `season_intl.c`, `season_intl_euro.c`, `season_intl_cups.c`, `season_intl_copa.c`, `season_intl_qual.c`, `season_intl_euroq.c`.

The actual lookup distinguishes effective frozen rules from edition occurrence. A 2010-start game with changing rules off must still schedule future editions rather than repeatedly running the same date. Fixed hosts and early-start real qualification/draw records are separate from simulated future qualification. Do not force known historic finalists after the user's simulated history has diverged.

Nation identity needs temporal aliases and confederation membership. Existing code addresses nation rename chains, duplicate old/dead records, Kosovo membership, and Australia's confederation change. Record ID is the identity; display text matching is an import compatibility mechanism.

### 8.3 Nations League algorithms that are actually present

**V:** `season_nl_rules.c:34–48` orders overall rows by league ascending, group position ascending, points descending, goal difference descending, goals for descending, nation ID ascending. This is the target's algorithm; do not insert real-world criteria absent from this function.

`season_nl_next_leagues`:

1. Sort rows by that ranking.
2. Walk from the bottom to apply group-winner promotion, last-place relegation, or configured lowest-ranked relegations.
3. Promote the configured best runners-up in ranking order.
4. Form each destination league ordered as relegated-from-above, retained, promoted-from-below.
5. Fill destination league capacities; the final league takes all remaining nations.

`season_nl_paths` excludes already-qualified nations; each path takes its own league's group winners then remaining nations in rank order. Empty places are filled from remaining best-ranked nonqualified nations, path A first. Semi-finals are first versus fourth and second versus third (`season_nl_rules.c:118–151`). These helpers do not prove full Nations League scheduling, draw, or qualifier integration; test the callers in `season_nl.c`, `season_nl_euro.c`, `season_nl_wc.c`, and `season_gold_cup.c`.

### 8.4 World Cup of 48 teams

**V:** source constants in `season_wc48.c:66` specify 48 teams, 12 groups, and a 32-team knockout. The top two of each group qualify; a temporary table containing all third-placed teams is sorted by original `League::sort_table 0x6827d0`, and eight qualify (`season_wc48.c:604`). Section 12 recovers the generic original comparison machinery; its complete World Cup call context still requires verification. `season_wc48_thirds.c` contains 495 masks for choosing eight of twelve third-place groups; the table must be imported and validated in full, not approximated with an arbitrary pairing.

The hardcoded round-of-32 bracket at `season_wc48.c:583` includes winners, runners-up, and third-place slots. Preserve its slot order because subsequent knockout pairing uses adjacent winners. It is not enough to pair any valid winner with any third-place team. `season_wcq26.c` and `season_wcq26.h` govern the associated qualification expansion; every confederation must be covered, including inter-confederation play-offs and multiple host exceptions.

**U:** runtime correctness of every 48-team draw and qualifier/save combination was not established in this pass. Use `tests/seasons/season_wc48_run.py` and source-bound fixtures against a supplied original/recompiled installation.

## 9. Eligibility, transfer windows, and substitutes

### 9.1 Nation state

**V:** `season_nation.c:13` initializes windows and caps to `KEEP=-1`. It walks rows in table order, replacing each field only when not `KEEP`; Brexit is sticky once a row enables it. Window count zero means signing at any time. Cap 255 means no limit. These are three different concepts. Window bounds have day 1–31 and month 1–12; native bounds store month 0–11. The maximum encoded windows is six.

**V:** `season_kind_nation.c:192–260` applies signing caps and windows and rebuilds sorted deadline bounds. Italy is marked `PARTIAL_WINDOWS`; preserve its untouched existing window rather than replacing the full native rule from a partial table. Exact native deadline comparison inclusivity and time-of-day are **U** until original routines are inspected. Test each opening/closing day and adjacent days.

### 9.2 Match-squad restrictions

**V:** the restriction wrapper at original `0x8cf6b0` calls the original first, then overwrites existing active foreign/non-EU restriction slots. Numeric limits apply to first-division fixtures. “No limit” applies to all competition classes of the nation. `set_slots` does not create a restriction in a slot that was `0xff`. Source: `season_kind_nation.c:382–434`.

This is an overlay on the original's restriction record, not a complete modern registration engine. Do not equate foreign with non-EU, club signing cap with match-day cap, or a permit requirement with a permit grant.

### 9.3 Work-permit change

**V:** `season_brexit_blocks` requires a UK club nation, year at/after the configured rule year, EU status, enabled season rule, and neither nationality being a UK nation or Republic of Ireland (`season_nation.c:40`). The runtime wrapper applies this only to explicitly identified offer/completion call sites, not every roster evaluation. It asks the original permit-decision routine when required. Existing players are not automatically removed from team selection. Default rule year is 2021.

**U:** the original permit grant/appeal algorithm, detailed historical eligibility and edge cases remain external. This chapter describes game code, not current immigration law.

### 9.4 Substitutions

**V:** exact-season lookup takes the first nonnegative `subs_used` and `bench` values separately across matching rows, clamps each to 9, and reports a bitmask indicating which are present (`season_subs_rule.c`). Fixture packing stores bench in low nibble and used substitutes in next nibble; wrappers establish where to apply the overlay, but original match-engine enforcement still requires verification.

The Greek cup wrapper changes the count after original fixture packing. For original round 10 it retains original bench while applying used-substitute count; other rounds can replace bench (`season_kind_subs.c:139`). The French cup original has a round-specific bench path. A single competition-wide value cannot faithfully erase those special cases.

**D:** separately model bench count, used-substitute limit, extra-time allowance, substitution-window limits, and replacements with no count where applicable. For Bosman compatibility, unsupported extra distinctions must remain explicitly unsupported or delegated, not invented from real football rules.

## 10. Full implementation acceptance matrix

Each test fixture must identify original program digest, recompiled commit, data-set digest, rules digest, start year, changing-rules mode, league selection, save identity and RNG seed/state. Compare domain state and results, not screenshots alone.

| ID | Scenario | Required assertion |
|---|---|---|
| C-001 | Base game with all season features bypassed | Same simulation-relevant outputs as baseline; no season metadata written unless required |
| C-002 | Start season later than 2001, changing rules off | Start-season changes applied exactly once; later years retain effective format |
| C-003 | Changing rules on across every table change point | Effective competition rules and entrants match target for each edition |
| C-004 | January/June/October-threshold/December update entries | Correct season key, update ordering and leap-year boundary behavior |
| C-005 | Save/load before and after an update day | No duplicate changes/news/fixtures; no missed transition |
| C-006 | Missing, malformed, truncated, duplicate-row rule input | Deterministic diagnostic; no silent unknown-to-zero coercion |
| C-007 | Unknown values and rows after last known season | Native-specific fallback preserved, including ten-year lookup boundary |
| C-008 | Odd and even club counts | Correct byes, match counts and completion; no self-play or duplicate fixture |
| C-009 | Shrink then expand a division | Expected-match count resets correctly; season can finish |
| C-010 | Linked reserve/B side qualifies for promotion/play-off | Correct eligibility and replacement; no parent/child conflict |
| C-011 | Play-off postponed beyond update day, then reload | Entry waits and resumes without losing results or drawing twice |
| C-012 | Two-leg aggregate tie under L/N/Q/H | Correct away-goal/extra-time/penalty order; tested by exact target outputs |
| C-013 | Replay-required draw | Exactly one replay when allowed, correct gap and deciding flags |
| C-014 | Four-club super cup with duplicate qualifiers | Four unique clubs, correct league fallback and semi-final slots |
| C-015 | Cup stops, resumes, or never returns | No phantom fixtures; historical object/records remain readable |
| C-016 | UEFA duplicate domestic/cup/title-holder qualifiers | Correct allocation and no duplicate entrant |
| C-017 | UEFA qualification loser transfer | Correct destination competition/path/round and preserved ordering |
| C-018 | 36-club draw for many seeded RNG streams | 8/6 opponents, equal home/away, one match/day, relaxation logged |
| C-019 | UEFA league phase complete | 1–8/9–24/25–36 progression; correct fixed bracket |
| C-020 | Nations League edition capacity change | Exact helper ordering, promotions/relegations and qualifier exclusion |
| C-021 | 48-team World Cup, every thirds mask | 495 valid masks, eight distinct thirds, 32 unique knockout entrants |
| C-022 | Multiple hosts and qualification already in progress | No lost/duplicated qualification slots; target late-start behavior |
| C-023 | Transfer-window boundaries, Italy partial windows | Correct opening/closing behavior; preserved untouched rules |
| C-024 | Foreign/non-EU limits and dual nationality | Distinguish signing and match limits, preserve original disabled slots |
| C-025 | Permit transition at year boundary | Correct exemptions and call-site scope, existing players retained |
| C-026 | Bench/used values above 9 and Greek special round | Target clamp and special-round behavior |
| C-027 | Every supported nation and competition family | At least one whole edition and save/load at each stage boundary |
| C-028 | Long save across all selected nations | No stale editions, unresolved dependencies, duplicate histories or counts drifting outside rules |

Existing harness inventory: `tests/seasons/season_core_run.py`, `season_change_run.py`, `season_rules17_run.py`, `season_cups_run.py`, `cups_more_run.py`, `cup_breaks_run.py`, `new_divisions_check.py`, `ita_serie_c_run.py`, `bra_states_run.py`, `turkish_groups_run.py`, `uefa_formats_run.py`, `uefa_access_run.py`, `uefa_2021_run.py`, `uefa_2024_run.py`, `club_world_check.py`, `club_cups_check.py`, `season_intl_run.py`, `season_nl_run.py`, `season_wc48_run.py`, `rules_report_check.py`, `full_play_run.py`, `soak_run.py`. Their existence is not evidence they passed in this session.

Checks actually run for this chapter: rules import check, `test_encode_leagues.py` (PASS), and `test_encode_rules.py` (ok). These test encoder consistency and committed generated outputs only. Direct Capstone disassembly of selected original routines succeeded. An initial sandboxed Unicorn execution terminated with signal 4 before returning results; a subsequent permitted retry by the coordinating agent passed all **15 isolated original-instruction vectors** (nine comparator cases and six points cases). See [probe results](evidence/competition-probe-results.json) and [the exact probe](evidence/competition-probe.py). These are synthetic routine checks, not a TypeScript differential test or a full game/season run.

## 11. Remaining research and implementation gates

The corpus contains **394 nation-rule encoding gap rows** in `docs/research/2026-10-17-nation-rules-gaps.tsv` and **312 league-rule gap rows** in `docs/research/2026-10-18-league-rules-gaps.tsv` at the pinned commit. They include unknown windows, partial limits, midseason changes, average-table positions, points-gap conditions and other text not fully encoded. “Encoder PASS” means these omissions are acknowledged, not resolved.

| Gate | Can be implemented now? | Required evidence before claiming parity |
|---|---|---|
| Rule import, provenance, typed unknowns | Yes | Import every input and preserve its identity |
| Pure Bosman helpers documented above | Yes | Differential outputs against compiled C helpers |
| Season orchestration and save contracts | Yes, structure and hooks | Original daily pass/start-new-season behavior and runtime ordering |
| Original league table/rank/result algorithms | Not fully specified | Read original bodies; tie, points and disciplinary vectors per class |
| All native cup/qualifying draws | Not fully specified | Round builders, entrant collectors, RNG-call ordering and stage callbacks |
| Cross-competition calendar collisions | Not fully specified | Original postponement/rescheduling and priority routines, full-play traces |
| All promotion/reserve/vacancy cases | Not fully specified | Original movement and eligibility bodies plus transition saves |
| Modern rules beyond Bosman limits | Separate feature | Explicit design decision, never labeled original/Bosman parity |
| Every international qualification path | Not fully specified | Confed-specific flows, late-start and host vectors |

Implementation order: import and provenance → pure helper parity → base competition state machine → one full domestic league/cup chain → save/reload parity → per-country special classes → UEFA old formats → newer UEFA formats/league phase → other continental club competitions → international qualifiers/finals → long-season transition matrix. Mark an implementation task complete only when the associated evidence and acceptance tests pass. A prose overview or correctly rendered table is not completion of the underlying game logic.

## 12. Direct original-code verification: standings and points

### 12.1 Artifact identity and method

The original inspected was `/Applications/CM 01-02 Bosman Edition.app/Contents/Resources/game/cm0102.exe`, SHA-256 `0c78e7da6d0bcb4603fa179449bbe6d1b6b5e1c627a22c445d14d489bd9bff86`. `tools/recomp/pe.py:Image` verifies that digest before mapping virtual addresses. Capstone decoded 32-bit x86 bytes at the following addresses. The branch descriptions begin with direct static evidence; the bounded dynamic vectors described below additionally execute selected original instructions in Unicorn. Neither is an observed full game session or a REA Evidence record.

| Routine/range, end exclusive | Byte-range SHA-256 | Coverage |
|---|---|---|
| `0x683210–0x6833ee` | `1e7f78eddde6f87a0bc601f696d0c5e9a7bd5862495492218ef1ab961255a495` | Ordinary table comparator body |
| `0x683410–0x6834f0` | `6da1e0ebe4ffc29520c2bbafe0e1737d7b470f7f8b602521cf72eaceb05bc730` | Head-to-head table comparator body |
| `0x6834f0–0x6835a2` | `6d08215550e4a70622f50d9cf87e401edb59c20037c272a815697a5a722ada33` | Mini-table comparator body |
| `0x68aad0–0x68ab63` | `e162c03fee7aa9a7493a44534cad633f353ba6b401a2038ef9ab43cb2578fb9e` | Points-return routine body |

The jump tables at `0x6833f0` and `0x6835a4` were read as five little-endian addresses, not decoded as instructions. `League::sort_table 0x6827d0–0x682b2c` was also read to establish comparator selection. Function-map labels helped locate the addresses; the following conclusions come from their bytes.

The subsequent complete recompiler generation provides reviewable C translations of these exact functions: [sort-table translation](../../build-web-evidence/original/gen/fn_017.c#L153148), [ordinary comparator translation](../../build-web-evidence/original/gen/fn_017.c#L155732), [head-to-head comparator translation](../../build-web-evidence/original/gen/fn_017.c#L156343), [mini-table comparator translation](../../build-web-evidence/original/gen/fn_017.c#L156681), and [points-return translation](../../build-web-evidence/original/gen/fn_018.c#L25742). These are local generated evidence, not portable source included in a clean checkout. This pass inspected the generated points routine and checked the five function bodies for exceptional translation markers; none contained an unhandled/unsupported/decode/abort/trap marker. This bounded check does not certify the recompiler, runtime helpers, indirect callbacks, or the whole game.

### 12.2 Ordinary table comparator, `0x683210`

The current league is stored in global `0xb679fc`; optional custom comparator is global `0xb67a00`. A stat-block selector at league `+0xad` produces byte offset `14*selector`. Row pointer fields at `+0` refer to clubs. Rows with null club pointers compare equal; row arguments themselves are expected valid.

Order of operations verified from instructions:

1. If custom comparator exists, call it. If nonzero, call it again and return that second result. This duplicate invocation is literal behavior; preserve if the callback can have side effects. Its implementations remain unresolved.
2. Compare **signed 16-bit** points at row `+0x19+14*selector`, descending.
3. If points tie and both selected played-count bytes at `+0x11+14*selector` are zero, return club A ID minus club B ID immediately.
4. Scan at most four criterion bytes at league `+0xc5`. A zero byte ends the scan. Criteria use the following table.
5. With selector zero and both clubs' played counts equal league `+0xa7`, compare nonnull club `+0x69` pointed records by descending record ID before final fallback. The semantic identity of that record is unresolved in this pass.
6. Final fallback is ascending club ID.

| Criterion byte | Verified computation | Direction |
|---|---|---|
| 1 | signed16 goals-for `+0x15+14*s` minus signed16 goals-against `+0x17+14*s` | Higher first |
| 2 | signed16 `+0x15+14*s` | Higher first |
| 3 | signed8 `+0x12+14*s` (wins field) | Higher first |
| 4 | signed8 row `+4` scratch ranking key | Higher first |
| 5 | signed16 row `+0x31` (away goals-for position within the repeated stat blocks) | Higher first |
| 6 | Only when selector zero: immediately return signed8 A[`+4`] − B[`+4`] | Lower first; no later tie fallback |
| Other nonzero byte | Skip to next criterion | None |

Do not truncate goals for/against to eight bits merely because some UI/diagnostic code reads a byte: this comparator explicitly uses `movsx` from **word** operands. The scratch key at row `+4` is later overwritten with final table position. Its role depends on sorting phase.

Representative instruction evidence: points `0x683275–0x68328d`; no-games fallback `0x683292–0x6832c1`; criteria dispatch `0x6832c2–0x683367`; final special record comparison `0x683371–0x6833ac`; club-ID fallback `0x6833ae–0x6833c5`; criterion 6 `0x6833c6–0x6833d5`.

### 12.3 Head-to-head comparator and sorting orchestration

`0x683410` applies custom comparator, points and no-games ID fallback similarly. Thereafter it compares row `+4` signed bytes descending. If equal it compares club-name byte strings from club `+4` lexicographically; it does not use a locale-aware web collation.

`0x6834f0` compares a mini-table by signed16 points, then up to four rule bytes: 1 goal difference, 2 goals for, 3 wins, 5 away goals; it skips criterion 4, avoiding recursive head-to-head ranking. If no criterion distinguishes rows it returns zero.

`League::sort_table 0x6827d0`:

- If expected matches is -1, club count positive, and league `+0x45` nonnull, set expected matches to `(clubCount−1)*meetings` using the native 16-bit calculation.
- Select supplied rows or the league's table at `+0xb1`.
- Check up to four criterion bytes. Criterion 4 with available head-to-head storage invokes `0x682b30`; if criterion 4 is first, select the head-to-head comparator for the initial sort. A later criterion 4 still causes head-to-head preprocessing but ordinary comparator selection.
- Copy league `+0xa3` into the custom-comparator global.
- For head-to-head-first sorting, perform additional adjacent-row passes after the initial sort, inspecting direct head-to-head wins/goals and sometimes the ordinary comparator. Therefore replacing the whole path with a single JavaScript comparator is not established as equivalent.
- For selector zero, unless league byte `+0x44` equals `0xfe`, write zero-based final positions to row `+4` and update nonnull position-history buffers for valid played-count indices.

The construction of mini-tables by `0x682b30`, every competition's custom callback, and all secondary-pass semantics still need full reconstruction. This is a substantial narrowing of the ranking unknown, not complete standings parity. Preserve each comparator's zero/equal result: do not add a universal stable ID tie-break where the original returns zero. Its CRT `qsort` implementation and surrounding traversal can affect tied-row order. The browser may have a separate deterministic display order, but that must not feed qualification or promotion decisions.

### 12.4 Points-return routine, `0x68aad0`

This routine returns an **8-bit value in AL**. It takes club and fixture arguments with league in ECX. Win points come from league byte `+0xc2`, draw points from `+0xc3`.

Verified branch behavior:

1. If fixture score bytes `+0x43/+0x44` differ, signed-byte comparisons determine the home/away winning club, which receives win points; other clubs receive zero.
2. If tied, and league flags at `+0xab` include bit `0x2`, compare score bytes `+0x47/+0x48`; a non-tie uses the same win/zero branch.
3. If still unresolved and flags include either `0x1` or `0x20`, compare bytes `+0x49/+0x4a`. A non-tie gives the winning club **draw points**, the loser zero.
4. Otherwise return draw points.

The field interpretation as regulation/extra-time/shootout scores is consistent with the repository map, but exact writers and aggregate-result handling must still be traced. The unusual draw-points result in step 3 is a direct branch fact. Do not replace it with an assumed universal three-point shootout win or a modern bonus-point rule.

Bounded dynamic evidence **COMP-ORIGINAL-PROBE-001**: a permitted Unicorn retry executed the original instructions against synthetic records and passed 15 cases. With win=3/draw=1, ordinary 2–1 returns 3/0; 1–1 without extra flags returns 1/1; tied regulation and 2–1 at `+0x47/+0x48` with flag2 returns 3/0; the latter score is ignored with flags0 and returns 1/1; tied prior scores and 5–4 at `+0x49/+0x4a` with flag1 returns 1/0; the latter score is ignored with flags0 and returns 1/1. Nine comparator cases also confirmed points, signed16 goal difference/goals, criteria 3–6, no-games club-ID ordering, and final club-ID fallback.

The initial sandboxed attempt terminated with signal 4 and produced no results; only the subsequent successful run is counted. [The preserved script](evidence/competition-probe.py) is byte-identical to that successful run and [the result record](evidence/competition-probe-results.json) records inputs/identity, versions, script hash, case outputs, and limitations. It uses Unicorn 2.1.4, maps the original into isolated emulator memory, and executes only `0x683210` and `0x68aad0`; it neither starts the desktop game nor writes game files. Its paths are the inspected local paths. This does not validate custom callbacks, the whole-table/head-to-head sort, every flag/sentinel, competition configuration, or a TypeScript port.
