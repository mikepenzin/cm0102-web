# CM 01/02: Bosman Edition — web implementation dossier

Research date: 28 October 2026. Target: Angular frontend and Node.js backend, preserving Bosman Edition's season selection and enhancements. Repository reference: `cf2e8fa5f4cc816f252611a0e9c81e66d088358e`.

**User tool constraint:** do not use Hopper. Continue recovery with the repository translator, Capstone, source/data inspection and isolated Unicorn checks. Historical Hopper failure records below are provenance only, not a request to enable or retry it.

**Read [the consolidated technical specification](CM0102-WEB-TECHNICAL-SPEC.md).** It contains the chapters below in one agent-readable document. The individual chapters are the editable sources; regenerate the consolidated copy with `python3 docs/web-port/build_book.py` (prefix with `rtk proxy` in this workspace).

The [specification completion plan](08-specification-completion-plan.md) defines the remaining research and validation through a full agent-ready document, with phases R0–R9 and a provisional 60–120 person-day estimate. Chapter 06's P00–P36 packages describe the subsequent game implementation. The [structured research plan](evidence/specification-plan.json) records dependencies, estimates and milestones; all its phases are planned, not claimed complete.

## Readiness and scope

This is a substantial technical specification and reverse-engineering dossier, with source-verified Bosman rules, direct original-code findings, a manual/source gameplay-and-screen inventory, web architecture, data/save contracts and an ordered implementation plan. It is **not yet a complete recovery of every original gameplay algorithm**. An implementation agent can begin the specified foundation and verified features; a faithful full replacement still requires the explicitly listed match, AI, development, finance, negotiation and save-state recovery work. No unknown probability or formula has been replaced with an invented football heuristic.

The supplied `242.exe` is the exact official 3.9.68 patch recorded by this project. All 46 embedded replacement files match the clean manifest. The installed Bosman application's original program matches the repository's required 3.9.68 digest, and all 106 manifest-listed Data files match. The installed host itself is version 0.5.1, so newer behavior is specified from the current repository rather than attributed to that older app.

## Chapters

| Chapter | Contents |
|---|---|
| [01 — Web architecture](01-web-architecture.md) | Node/Angular boundaries, numeric compatibility, commands, APIs, workers, persistence, ownership and privacy |
| [02 — Gameplay](02-gameplay.md) | 24 systems, tactics, assistant scoring, training, match helpers, scheduler, transfers, finances, board/AI/medical/scouting and manual behavior |
| [03 — Competitions and seasons](03-competitions-and-seasons.md) | 7,102 rule rows, all rule families, calendars, qualification, draws, eligibility, substitutions, rollovers and original standings/points |
| [04 — Data, UI and assets](04-data-ui-and-assets.md) | 22 indexed table entries, record layouts, saves, manual/source screen-and-action inventory, settings, assets, language names and actual data audit |
| [05 — Evidence and verification](05-evidence-and-verification.md) | Artifact provenance, successful probes, translator limits, reproducibility and full acceptance matrix |
| [06 — Agent implementation plan](06-agent-implementation-plan.md) | Dependencies, vertical slices, feature handoff template, recovery obligations and release completion contract |
| [07 — Original negotiation evidence](07-original-negotiation-evidence.md) | Exact bounded offer/contract predicates, getter mutation and value aggregation from original instructions |
| [08 — Specification completion plan](08-specification-completion-plan.md) | Remaining research phases, effort/dependencies, verification deliverables and full-document completion criteria |

## What was verified

Isolated original-instruction tests completed **67,714 recorded cases/checks across 11 direct entry points**, using synthetic records: 1,024 training-load combinations; 15 standings/points scenarios; 800 reaction-scaling cases; eight pending-foul checks; and 65,867 negotiation/contract-helper cases. Most of the large negotiation count enumerates a small byte-state predicate. This is strong evidence for those bounded functions, not a whole-game fidelity percentage. No full match or career was validated in this investigation.

The repository translator emitted 11,752 routines and 1,882,553 decoded instructions. Its report flags 31 unsupported instructions and one decode stop; the gap audit additionally distinguishes fallthrough/trap markers. Generated C also includes translator safeguards. It must not be described as an independently verified, pristine recovery of all gameplay.

REA's Hopper route was blocked by macOS Apple Events permission. The useful work continued with Capstone, the repository translator, the manual, source inspection and isolated Unicorn execution. No global agent configuration was changed, and no game installation or existing save was patched.

## Evidence and generated reference code

- [Coverage summary](evidence/coverage-summary.json), [artifact/patch audit](evidence/artifact-audit.json), [data audit](evidence/data-audit.json).
- [Original function inventory](evidence/original-functions.tsv), [module inventory](evidence/original-modules.tsv), [source hashes](evidence/source-manifest.json).
- [Generated-function index](evidence/generated-function-index.tsv), [translation report](evidence/recompiler-report.json), [translation gap audit](evidence/translation-gaps.json).
- [Bounded probe entry-point ledger](evidence/probed-functions.json) and the corresponding scripts/results in `evidence/`.
- Generated reference C: `build-web-evidence/original/gen/` at repository root. This approximately 182 MB directory is intentionally ignored by Git; it can be reproduced from the matched original using the command in the verification chapter's artifact workflow and the implementation notes below.

The original program, manual, and data remain in their local installation paths. The dossier preserves hashes and derived analysis, not a redistributed copy of those assets. A recipient on another machine must supply matching artifacts and regenerate the reference output.

## Reproduce the mechanical source export

With Capstone available in the selected Python environment:

```sh
rtk proxy python3 tools/recomp/recomp.py \
  --exe '/Applications/CM 01-02 Bosman Edition.app/Contents/Resources/game/cm0102.exe' \
  --out build-web-evidence/original \
  --functions tools/recomp/functions.txt --all --shards 64 --jobs 2
rtk proxy python3 docs/web-port/build_inventory.py
rtk proxy python3 docs/web-port/build_book.py
```

The observed run used `/private/tmp/cm0102-analysis-venv/bin/python`, Capstone 5.0.9 and Unicorn 2.1.4. Temporary environments are not permanent project dependencies. To refresh generated-file hashes and patch member checks, use `inspect_inputs.py --help` and supply the actual verified original, patch, extracted `update.dat`, manual and generated output paths. Never treat these commands as a build or full-game test pass.
