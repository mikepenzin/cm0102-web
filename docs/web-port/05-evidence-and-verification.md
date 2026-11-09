# Evidence, reproducibility, and fidelity verification

## 1. What was actually examined

The investigation used the checked-out source, its tests/research corpus, a supplied installation folder, the supplied official patch, the English manual, and the original program embedded in the installed Bosman Edition. No live career or user save was changed. The Windows patch installer was not executed.

| Artifact | Identity / purpose |
|---|---|
| Repository | `cf2e8fa5f4cc816f252611a0e9c81e66d088358e`; current source target, including newer Bosman season enhancements |
| Matched original | `/Applications/CM 01-02 Bosman Edition.app/Contents/Resources/game/cm0102.exe`; 7,192,576 bytes; SHA-256 `0c78e7da6d0bcb4603fa179449bbe6d1b6b5e1c627a22c445d14d489bd9bff86` |
| Installed Bosman host | Application metadata version `0.5.1`; older than the checked-out season-enhancement target |
| Supplied Downloads original | `/Users/mikepenzin/Downloads/CM0102/cm0102.exe`; SHA-256 `343f359a713a34ae46ef47f9a97bbf61ab9278e9e1f0a74381f224ffc50349f2`; different from the matched original; do not apply the 3.9.68 address map to it |
| Supplied official patch | `/Users/mikepenzin/Downloads/242.exe`; 23,458,816 bytes; SHA-256 `b3f2b7d2ace6e86d0974e5ad68ae7a87a1e8fc3494c90d00cc3758392dcf24f1`; exact `PATCH_SIZE`/`PATCH_SHA256` in `tools/clean_files/make_clean_files.py` |
| Supplied manual | `/Users/mikepenzin/Downloads/CM0102/MANUAL/manual_uk.pdf`; 67 PDF pages; full SHA-256 in the artifact audit |
| Installed Data | Every one of the 106 manifest-listed `Data/` files matches the repository's expected size and SHA-256; indexed file ranges checked separately |

The patch is not identified merely by its filename or embedded strings. Both size and full digest match. Its archive was listed with the system tar reader, `update.dat` was extracted to a temporary directory, and the known 46-entry length table was decoded. All 46 embedded replacements match the clean-file manifest. This establishes the payload identity without running Setup or overwriting the Downloads installation. See [artifact-audit.json](evidence/artifact-audit.json).

The data audit records every comparison and all 22 index entries, not just totals. It also checks translated English name-section boundaries. See [data-audit.json](evidence/data-audit.json). This does not establish every gameplay meaning of every byte.

## 2. Evidence precedence

Use the following distinctions, preserving the more granular labels in each chapter:

1. **Observed instruction/data bytes:** valid only for their exact artifact digest and mapped range. Static control-flow inspection establishes branches, not that a scenario reached them.
2. **Completed isolated execution:** establishes output/mutations for the recorded synthetic input and harness. It does not prove the whole caller, match or season works.
3. **Inspected Bosman/hand-port source:** establishes that implementation, including guards, approximations and delegation to original routines.
4. **Manual statements:** establish documented player-facing behavior for the manual's release, not unstated probabilities or later patch changes.
5. **Inherited research/maps:** investigation leads and previously researched rules. A high-confidence function name is not a complete algorithm or an independent validation result.
6. **Web design:** new interfaces, persistence and concurrency decisions. Clearly label changes to visible behavior or numeric semantics.

When these conflict, first determine which version/layer each describes. Do not resolve a conflict by silently choosing the most convenient text. For example, original multiplayer documentation does not prove that the current native runtime implements networking; old save-extension documentation does not override the current serializer; a real-world competition fact does not override a deliberate Bosman engine cap.

The repository's `CLAUDE.md` explicitly says to read a function's code before relying on its map name. Follow that requirement for future additions.

## 3. Complete inventories versus semantic coverage

The committed function-map inputs contain:

- 11,747 placed function entries: 6,609 game functions, 254 library functions, 387 CRT functions, 3,873 unwind funclets and 624 exception-handler stubs.
- 6,863 named entries across 393 named source modules.
- 5,702 high-, 1,133 medium- and 28 low-confidence names in the inherited naming corpus.
- 4,428 data-map global rows.

These counts are reproduced by [build_inventory.py](build_inventory.py), not inferred from the README's approximate function count. [original-functions.tsv](evidence/original-functions.tsv) and [original-modules.tsv](evidence/original-modules.tsv) retain coverage inventory. Confidence in a name is not confidence that all function behavior has been specified.

The source manifest hashes 1,768 pre-existing tracked repository files at the research revision. It excludes this dossier and its generated outputs to avoid self-referential digests. [source-manifest.json](evidence/source-manifest.json) records the exact boundary. New original-derived C lives in the ignored `build-web-evidence/` directory and has a separate generated-file manifest.

### Mechanical translation performed

The existing translator was run against the matched original with `--all`, the checked-in known-function list, 64 shards and two analysis jobs. Output: `build-web-evidence/original/gen/`, approximately 182 MB of generated C. The output report lists:

| Metric | Result |
|---|---:|
| Functions emitted | 11,752 |
| Decoded instructions | 1,882,553 |
| Instructions reported handled | 1,882,522 |
| Unsupported instructions | 31 |
| Decode stops | 1 |
| Jump tables | 1,270 |
| Resume entries | 11 |

The generated count differs from the function-map count because the translator also discovers entries. Compare address sets; do not call this five independently recovered new features. Unsupported instructions and discovered entries may involve code/data boundary mistakes, but must remain unresolved until proven. See [recompiler-report.json](evidence/recompiler-report.json) and [translation-gaps.json](evidence/translation-gaps.json).

Generated functions are located through [generated-function-index.tsv](evidence/generated-function-index.tsv), including path and line boundaries. It is a navigation aid for the implementation agent. The generated source is **not** a substitute for a verified semantic specification, and the instruction handling percentage is **not** a fidelity percentage.

The translator can introduce guards/patches from its own source even without explicit extra runtime wrappers. In particular, a null-pointer guard is injected in generated `0x8e8800` before original instruction `0x8e8827`. Distinguish original bytes, translator guards and Bosman runtime enhancements. Record all three in a feature dossier; do not cite an injected branch as original behavior. The original-API `o_`/`f_` naming alone is not proof of unmodified semantics.

### Analysis tool outcomes

**Current user constraint:** Hopper must not be used for subsequent work, directly or as a REA provider. The following attempted-use history predates that instruction. Do not repair its Automation permission or retry it; continue with the other analysis methods described here.

REA 6.8.0 was acquired in a temporary npm cache. Its diagnostic found an installed Hopper engine and a Node-version mismatch in the default shell. An already-installed Node 24.22.1 runtime satisfied the CLI requirement. No global Node installation or agent configuration was changed.

REA/Hopper export could not start successfully: macOS denied Apple Events automation permission to Hopper. This is an OS automation permission issue, not absence of the original or lack of an analysis license established by this investigation. No REA pseudocode/evidence export was successfully produced. The investigation continued using the repository's translator, Capstone instruction inspection, and isolated Unicorn probes. Do not label those outputs “REA Evidence.”

Initial sandboxed Unicorn attempts terminated with SIGILL before producing results. Permitted retries outside that shell sandbox successfully completed the explicitly scoped synthetic probes below. Record failed attempts as tool-environment history; they are neither game failures nor successful validation.

## 4. Completed validation and its limits

| Check | Result | What it establishes / does not establish |
|---|---|---|
| Original and patch hashes | Pass | Exact input identities; no runtime behavior |
| Patch payload comparison | 46/46 match | Embedded replacements agree with clean manifest |
| Installed data comparison | 106/106 match | Manifest-covered Data files have expected bytes |
| Data index extents | 22 entries checked | Indexed records fit actual files; not full field semantics |
| English name-table layout | Exact size reconciliation | Counts/order/widths fit the inspected language file |
| Rules table assembly | 7,102 rows in 20 parts accepted | Structural/source-table encoding; not complete implemented football rules |
| Nation and league encoder tests | Pass | Existing test assertions; gaps remain in exported encodings |
| Original training load `0x8b4f60` | 1,024 combinations, zero mismatches | All five-category combinations of the four supported intensities; x87 control `0x27f`; not attribute growth/injury effects |
| Original standings/points routines | 15 cases, all pass | Recorded comparator/points branches; not every league, mini-table, play-off or season |
| Original reaction scaling `0x716450` | 800 cases, zero mismatches | Recorded finite input grid; not NaN/invalid records/full match |
| Original pending-foul setters/reset | 8 checks, zero mismatches | Byte mutations and occupied-slot behavior; not foul probability, advantage or referee decisions |
| Original negotiation/contract helpers | 65,867 cases, all pass | 65,536 closure byte pairs, 256 expiry flags, 64 clause-status combinations, four talks-status paths and seven no-list arithmetic cases; not full negotiations/settlement |
| Whole-game differential run | Not run | No claim of end-to-end gameplay fidelity |
| Browser implementation tests | Not applicable | This task produced documentation and evidence, not a playable web application |

The probe scripts/results under `evidence/` are retained for reproduction. Aggregate counts must not hide their limited input domains. An isolated routine can pass while the caller invokes it in the wrong order or supplies incorrect state.

## 5. Required future comparison harness

Every scenario manifest must record:

```text
scenario ID and version
original SHA-256 + Bosman commit + translator/guard revision
data/rules/asset pack digests
engine/harness versions and build flags
initial virtual date, manager controls and background-match settings
initial memory/state image identity, RNG state, allocation/scheduling conditions
ordered semantic actions with exact decision points
expected return values, state writes, events, messages and stop reasons
excluded/normalized fields with a reason for each exclusion
actual output identity, first divergence and reproduction command
```

Use three comparisons:

1. **Primitive comparison:** isolated numeric/date/RNG/record operations against original instructions. Include signed extremes, zero, sentinel values, rounding boundaries and error cases within valid contracts.
2. **Subsystem comparison:** a real subsystem snapshot plus a command/day/match phase. Compare every mutation, random draw position and event ordering, not only final score or an aggregate statistic.
3. **Career comparison:** repeat the same action stream from a known start, at every match/day/season/save boundary. Save/reload between boundaries and compare continuation with uninterrupted execution.

A useful original emulation harness already exists in `tests/diff/original.py`; its expected digest is the same matched original. Its configured fake surfaces, memory locations and imported-function replacements are part of fixture identity. Extending it requires examining each stub's effect on the routine under test. A fake clock that makes one primitive deterministic can change whole-game scheduling.

## 6. Minimum acceptance suite for the web conversion

### State, commands and persistence

- Two identical command retries produce one outcome and one ledger effect; reused ID/different payload fails.
- Stale commands leave world, RNG and scheduler unchanged.
- Two browsers controlling one manager cannot commit conflicting revisions.
- Crash immediately before/after each commit boundary restores exactly one durable outcome.
- Restore every supported web save version; reject corrupt checksums, unsupported versions, broken references and missing packs before activation.
- Cancel Continue and reconnect at a safe boundary without skipping a due task.
- Add/switch/retire a human manager without losing the world or another manager's decisions.

### People and club management

- First/reserve selections, captain, set-piece order and formation changes survive save/reload.
- Suspended/injured/on-duty/loan/trial players have the same availability as the reference in the same competition context.
- Attribute masking holds across profiles, search/sort, comparisons, assistant advice, reports and exports.
- Training intensities/retraining/coaches affect only the recovered fields in the recovered cadence.
- Injury recovery, recurrence, leave, rehabilitation, birthday, retirement and regeneration respect calendar boundaries and record continuity.
- Every board/FA/media/news action has an explicit accepted/rejected/unavailable response and downstream state effects.

### Transfers and finances

- Ordinary transfer, free agent, expiring contract, renewal, exchange, loan, trial and staff dual-role paths.
- Competing offers; accepted offer expires; permit denied/appealed; registration/window restriction; funds change before completion.
- Immediate fee, installments, bonuses and downstream clauses use exact units/rounding and post once.
- Parent contract/loan consistency after staff-job loss and loan expiry.
- Every income/expenditure total reconciles with the exact ledger and original behavior.

### Matches

- Prematch eligibility and bench limits for every represented rule era.
- Tactical inheritance, with/without-ball zones, substitutions, goalkeeper replacement, captain changes and set-piece choices.
- Full and quick/background paths both update results, player state, standings, histories and news.
- Cards, second yellow, direct red, injury replacement, added time, extra time, shootout, aggregate/away-goals logic where applicable.
- Replay after pause/save/reload and commentary-speed changes under the declared scheduling profile.
- Match ratings/statistics and commentary refer to the actual committed event stream.

### Competition and long-career behavior

- Every supported start season, every represented domestic/international family, active/background league configurations, fixed/changing rules.
- Draw constraints, group/league-phase tables, multi-stage qualifiers, mini-tables, play-offs, promotions/relegations, winners and access lists.
- Capacity/bench approximations match Bosman rather than modern rules assumed from memory.
- Rule changes occur on each competition's real update date, including calendar-year leagues.
- Save/load before qualification selection, draw, end of season, nation-name change and new competition creation.
- Long careers beyond the final researched season exercise each module's actual fallback policy.

### UI and operational behavior

- Every screen/action in chapter 04 is reachable by keyboard and mouse, with a correct empty/loading/error state.
- No loss of visible information at supported viewport sizes; accessible status cues accompany colors.
- Back/forward/profile navigation does not advance gameplay or consume RNG.
- Slow clients, disconnects, duplicate transport events and stream gaps cannot create duplicate game decisions.
- Import size/record bounds and account authorization enforced server-side; hidden data never shipped in client payloads.
- All-nation late-career benchmarks measure memory, Continue throughput, save/load latency and API responsiveness.

The repository's own `docs/TESTING.md` provides additional release criteria: soaks through major international tournaments, full-play samples, season sweeps, after-load sweeps and large-save checks. These are source requirements to adapt, not successful runs from this documentation task.

## 7. Closing a recovery gap

For each unresolved function family: select exact entry points; read complete bodies and callees; recover every accessed field and sentinel; enumerate branches/side effects; capture valid input fixtures; compare original and proposed implementation; test save/load and scheduling interactions; then update the ledger with the test result and precise coverage boundary.

Do not close a gap because generated C exists, a function has a confident name, a scenario produces a plausible football score, or a total statistic resembles a real league. A faithful implementation claim requires the relevant branch and mutation equivalence.
