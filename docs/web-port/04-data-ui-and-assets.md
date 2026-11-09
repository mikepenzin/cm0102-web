# Data, saves, screens, assets, and user flows

This chapter specifies the CM 01/02: Bosman Edition behavior visible in repository revision `cf2e8fa5f4cc816f252611a0e9c81e66d088358e`. It separates the existing data contracts from the proposed web contracts. The current repository contains the hand port and Bosman runtime hooks, but most original screen and simulation functions are generated from an external copy of the original. A function-map label alone is not sufficient evidence of a screen's fields or action semantics.

Evidence labels used throughout:

- **S — source inspected:** executable C/Python logic or declared record layout read in this investigation. This establishes the repository implementation, not independent equivalence with the original.
- **T — test specification inspected:** a checked-in test describes a scenario. It was not run against the original during this chapter's investigation.
- **A — artifact inspected:** bytes, hashes or records read from the supplied installation, without executing the game.
- **M — manual:** the supplied English game manual documents the public feature or user interaction. It is a 2001 manual, not proof of the exact 3.9.68/Bosman implementation or probability formula.
- **R — inherited research:** useful investigation material requiring confirmation against the original before treating its claim as established.
- **W — web requirement/design:** a concrete proposed implementation contract, not a claim about original behavior.
- **U — unresolved:** required evidence or semantics remain missing.

## 1. Data model boundaries

Maintain three separate representations (**W**):

1. **Imported data set:** immutable source bytes, file hashes, index metadata, typed decoded fields, and unknown byte ranges. Identified by a content digest, not a folder name.
2. **Game world:** mutable state for one game, its manager slots, current date/slice, competitions, players, contracts, transfers, finances, news, histories, and deterministic engine state. It records the exact imported data-set and rules revisions.
3. **Viewer projections:** data the active manager is entitled to see. Masked attributes remain absent from response bodies; the Angular client must not receive hidden values and merely obscure them visually.

Do not use C memory addresses as persistent web IDs. Assign stable IDs in an explicit namespace: `dataSetId + tableId + originalRowId` for imported records, and game-local monotonic IDs for generated records. Preserve `originalRowId`, original table identity, and original raw bytes for comparison. Clubs and national clubs, and club and national competitions, share arrays in parts of the hand port; retain their source-table identity during import.

No source reviewed here establishes a complete normalized domain schema for all gameplay. A database-record reader is not a saved-game reader; a save-container decoder is not a decoder of every saved subsystem. These are separate implementation gates.

## 2. Source data files and record layouts

### 2.1 Required file set and index

**S:** [`src/recomp_rt/win/dbfolders.c`](../../src/recomp_rt/win/dbfolders.c), lines 11–35, recognizes a custom data set by the following 20 filenames, case-insensitively:

`city.dat`, `club.dat`, `club_comp.dat`, `club_comp_history.dat`, `colour.dat`, `common_names.dat`, `continent.dat`, `first_names.dat`, `index.dat`, `nat_club.dat`, `nation.dat`, `nation_comp.dat`, `nation_comp_history.dat`, `officials.dat`, `second_names.dat`, `stadium.dat`, `staff.dat`, `staff_comp.dat`, `staff_comp_history.dat`, `staff_history.dat`.

This presence test does **not** validate contents, record counts, references, or a playable season. The web importer must validate those separately.

**S:** [`src/cm3/database.c`](../../src/cm3/database.c), `read_index` at lines 458–486, reads an 8-byte header and 22 entries of 67 bytes (`0x43`) from `index.dat`. Each entry is:

| Offset | Bytes | Value |
|---|---:|---|
| `0x00` | 51 | NUL-terminated filename; the hand port forcibly terminates byte 50 |
| `0x33` | 4 | Signed table ID |
| `0x37` | 4 | Signed record count |
| `0x3b` | 4 | Signed byte offset within the named file |
| `0x3f` | 4 | Signed format ID |

The two header integers are retained as `g_db_index_header`; after loading the hand port uses reserve competition and manager-row counts. Do not reinterpret the header as a general version number without evidence. The reader tests one extra byte after the entries and rejects it when nonzero; this is not a complete trailing-data validation policy.

**W importer requirements:** parse integers explicitly as little-endian, reject arithmetic overflow, negative offsets/counts, ranges beyond file length, conflicting entries, unknown format variants, and filenames escaping the staged import directory. Preserve the original bytes and diagnostics before normalization. Do not cast a Node `Buffer` to a host-dependent native struct.

### 2.2 Table IDs and record widths

**S:** [`include/database.h`](../../include/database.h), lines 31–82, is the current hand-port declaration. Widths are packed original-file widths, not `sizeof` of the 64-bit host structs.

| ID | Table | Bytes per record | Principal interpretation and limits |
|---:|---|---:|---|
| 0 | Club | 581 (`0x245`) | Name, nation, division/stadium links, officials/staff/squad/team arrays; many unknown bytes |
| 1 | National club | 581 | Same record layout; merged with clubs at runtime |
| 2 | Colour | 58 | ID, name, unknown tail |
| 3 | Continent | 198 | Names/codes and unknown numeric value |
| 4 | Nation | 290 | Names, code, nationality, continent/city/stadium/colour links, runtime league flags |
| 5 | Stadium | 78 | ID, name, city link, opaque fields and another stadium link |
| 6 | Staff | 157 on disk | Converted into 110-byte original-memory representation plus preferences |
| 7 | Official | 43 | Name IDs, nation/city links, opaque attributes |
| 8 | Staff B | No reader in table switch | Do not invent a decode schema |
| 9 | Non-player | 68 | ID, five signed shorts, 21 bytes, eight staff links, final byte; most meaning unknown in header |
| 10 | Player | 70 | ID, one byte, five signed shorts, 55 bytes; attribute semantics partly recovered elsewhere |
| 11 | Staff competition | 101 | Name, continent/nation/colour references, opaque bytes |
| 12 | Club competition | 107 | Names, flags/opaque bytes, continent/nation/colour references, inferred level |
| 13 | First name | 60 | Name, ID, nation ID, byte count |
| 14 | Second name | 60 | Same |
| 15 | Common name | 60 | Same |
| 16 | Nation competition | 107 | Combined with club competitions plus reserve rows |
| 17 | Staff history | 17 | Staff/club references and opaque season/statistic bytes |
| 18 | Staff competition history | 58 | ID and opaque 54-byte payload in current header |
| 19 | Competition history | 26 | Competition link, four club links, opaque two-byte field |
| 20 | Nation competition history | 26 | Combined history layout |
| 21 | City | 56 | ID, name, nation link, opaque geographic data |
| 22 | Preferences | 52 | Constructed by loader; absent from the 22 index entries |

**Conflict to resolve correctly:** the early research note [`2026-10-13-database-inventory.md`](../research/2026-10-13-database-inventory.md) labels the 68-byte and 70-byte player/non-player records in the opposite way. It predates the implemented header. Use the current executable reader and field layouts for the Bosman implementation, and test the actual data set before assigning meaning to a field. Do not merge the contradictory labels silently.

### 2.3 Important declared fields

All offsets below are **S** from `include/database.h`, with additional executable conversions in `src/cm3/database.c`. “Inferred” explicitly remains the source's confidence, not a verified semantic claim.

| Record | Fields to preserve |
|---|---|
| Nation | ID `+0`; name `[51]` at `+4`; second/sort name `[26]` at `+0x38`; code `[4]` at `+0x53`; nationality `[26]` at `+0x57`; continent `+0x71`; city `+0x7a`; stadium `+0x80`; six colours `+0x90`; three inferred rivals `+0x110`; runtime league flags `+0x11c` |
| Club | ID `+0`; long/short names `+4`, `+0x38`; nation `+0x53`; division/last division `+0x57`, `+0x5b`; stadium `+0x69`; reputation signed short `+0x80`; chairman `+0xbf`; three directors `+0xc3`; manager `+0xcf`; assistant `+0xd3`; 50 squad references `+0xd7`; five coaches `+0x19f`; seven scouts `+0x1b3`; three physios `+0x1cf`; 20 selected team references `+0x1e0`; in-league byte `+0x244` |
| Staff | ID `+0`; first/second/common-name IDs `+4/+8/+0xc`; birth date `+0x10`; inferred age signed short `+0x18`; nationality `+0x1a`, inferred second nationality `+0x1e`; national club/job `+0x24/+0x28`; club/job `+0x39/+0x3d`; joined date `+0x3e`; contract-end date `+0x46`; player/prefs/non-player links in original-memory form `+0x61/+0x65/+0x69` |
| Competition | ID and names as club; continent/nation `+0x59/+0x5d`; two colours `+0x61`; inferred level signed short `+0x69` |
| Name | Text `[51]` at `+0`; ID `+0x33`; nation ID `+0x37` stays unresolved; count byte `+0x3b` is inferred use count |
| Preferences | ID `+0`; six club references `+4`; six staff references `+0x1c`; their “favourite” meaning is inferred |

Preserve the fixed array capacities in the compatibility layer. A web relational table can support additional rows internally, but allowing a 51st senior squad record or a 21st selected-team entry changes behavior unless that extension is separately specified. The 20 selected entries are not a blanket statement that every match permits nine substitutes; competition rules determine the bench.

### 2.4 Staff conversion and reference resolution

**S:** `convert_staff`, `src/cm3/database.c:953`, explicitly performs:

1. Preserve/read the named fields through byte `0x60` of each 157-byte file record.
2. Read the player reference from file offset `0x91`, and non-player reference from `0x99`.
3. Allocate a separate preferences row keyed to this staff row.
4. Copy six 32-bit club references from offsets `0x61 + 4k`; copy six staff references from `0x79 + 4k`.
5. Reserve preference rows receive `-1` references; the temporary 157-byte staff buffer is freed.

The disk field at `0x95` is not assigned by this conversion. Preserve it in the imported raw record even if no typed meaning is known.

`database_read:1020` clears the previous database, reads the index, sizes and allocates tables, prepares reserve competition/staff/manager rows, reads tables, then converts staff. `database_load:1056` adds the reference/staff/national-club passes. [`src/cm3/database_links.c`](../../src/cm3/database_links.c) is the source for exceptions in reference resolution. Do not impose a universal rule that every bad ID becomes null: some fields deliberately keep a high ID, and some missing-name paths point into a different name table.

`pointer_row`, `src/cm3/database.c:341`, is a **test/export normalization convention**, not an original file format: null becomes `-1`; a pointer into another table becomes `0x80000000 | table << 24 | row`; an unresolved ID becomes `0x40000000 | id`. A Node importer must not interpret these markers as ordinary on-disk IDs.

### 2.5 Dates, strings, and unknown values

**S:** [`include/date.h`](../../include/date.h), lines 6–18, defines the original 8-byte date as signed 16-bit zero-based day-of-year, unsigned 16-bit year, and unsigned 32-bit leap flag. Store the game date independent of the server's timezone. Use an explicit ordinal/calendar type rather than JavaScript local-time `Date` arithmetic. Compatibility-sensitive overflow, invalid dates, and weekday behavior belong to the engine's date module, not display formatting.

**S:** [`src/recomp_rt/win/cp1252.h`](../../src/recomp_rt/win/cp1252.h) implements Windows-1252/UTF-8 handling for names and paths. A file name cannot be decoded by “UTF-8 everywhere” without losing names such as Köln. **W:** keep source bytes, decode user-facing values to Unicode with explicit code-page metadata, normalize filenames separately, and never key a player/club by its displayed name. Preserve unknown bytes and distinguish missing (`null`), masked (`unknown to viewer`), unset sentinel, and the numeric value zero.

## 3. Save containers and compatibility

### 3.1 Container format

**S:** [`tools/save/savfile.py`](../../tools/save/savfile.py), lines 16–117, is an executable container reference:

- Header size `0x416ec` = 268,012 bytes.
- First 12 bytes: three little-endian 32-bit integers: version, a second value retained as `unknown` by this tool, and member count.
- Then 1,000 directory slots of `0x10c` = 268 bytes each: unsigned 32-bit offset, unsigned 32-bit decoded size, and 260 bytes for name.
- Versions 1 and 3 store raw member data; versions 2 and 4 use the byte-run code below.
- The decoder itself supplies no checksum. Web save integrity must be supplied separately.
- The Python helper resolves member names case-insensitively. This helper behavior must not be promoted into a claim about original `zopen` name matching.
- The helper only modifies existing members without changing decoded length; output uses exclusive creation and preserves unmodified member bytes.

Byte-run decode: each byte `< 0x81` is a literal; each byte `b >= 0x81` consumes the following byte and emits it `b - 0x80` times. Stop after the declared decoded size. Encoding groups equal bytes in runs of at most 127; a single byte below `0x81` remains literal; all other runs are `[0x80 + count, byte]`.

**W importer:** bound total compressed and decoded size, validate directory/member ranges before allocating, validate every run has a following byte, reject duplicate ambiguous names, and never extract untrusted member names directly into host paths. Retain unknown member payloads for round-trip comparisons. The helper's short-read and range behavior is not sufficient for a public upload endpoint.

### 3.2 Bosman season part

**S:** [`src/recomp_rt/win/season.c`](../../src/recomp_rt/win/season.c), lines 47–62, 156–161, 210–318, 344–449, implements the member `bosman_season.dat`. Its first 16 little-endian bytes are:

| Offset | Type | Meaning |
|---|---|---|
| 0 | u32 | Part version; current writer uses 1 or 2 |
| 4 | u32 | Declared decoded part length |
| 8 | u32 | Start year |
| 12 | u32 | Changing rules, 0 or 1 |

It is written when the start year is not 2001, changing rules are enabled, **or extension state exists**. Do not apply the older rule “a base-game save never has this part” unconditionally: changed names/news/pending qualifier state can require an extension. No member means default start year 2001 and changing rules off. Current source reads the part before running the original load callback and rejects damaged recognized season data.

Reader rules include part size 16 bytes through 1 MiB, version at least 1, declared length within the decoded size, supported start year, and boolean changing rules. It accepts later versions only through the known prefix and extension checks. Original load failure resets the new-game season state. New-game creation and game teardown reset season state so one world's choices cannot leak into another.

The original addresses `0x0082e430` (save), `0x0082a9c0` (load), `0x00828110` (create), and `0x0082ff40` (free) are **source-inspected wrapper bindings**, not a claim that their original bodies were recovered here.

### 3.3 Version 2 extensions, including newer pending fixtures

**S:** [`src/recomp_rt/win/season_change.c`](../../src/recomp_rt/win/season_change.c), lines 430–511, serializes after the 16-byte prefix:

1. `u32 newsCount`.
2. For each news item: `u32 season`, `u32 posted`, `u32 textLength`, followed by exactly that many text bytes.
3. `u32 renameCount`.
4. For each rename: `u32 competitionRecordIndex`, 51 bytes of its **original data name**. This is needed when loading renamed competitions through the original name lookup.
5. `u32 lastSeasonPoint`.
6. Optional pending international-qualifier extension, described below.

The reader bounds news count to 200, rejects duplicate news seasons and invalid `posted`, bounds name count to 100,000 and remaining bytes, and checks season-point range. NUL and `<` bytes in news text become spaces. The old research note and even the shorter header description are not the whole current format.

**S:** [`src/recomp_rt/win/season_friendly_dates.c`](../../src/recomp_rt/win/season_friendly_dates.c), lines 492–538, reads the optional tail. No remaining bytes means an older compatible save. Otherwise it starts with `u32 count` and entries of 20 bytes:

| Entry offset | Bytes | Value |
|---|---:|---|
| 0 | 4 | Competition record ID |
| 4 | 4 | Home club ID |
| 8 | 4 | Away club ID |
| 12 | 2 | Fixture year |
| 14 | 2 | Fixture day |
| 16 | 2 | Stage-end year |
| 18 | 2 | Stage-end day |

The reader also accepts older 16-byte entries without the stage-end fields, inferring entry width from remaining bytes/count. Preserve both forms in a compatibility decoder; use a separately versioned and unambiguous field schema for new web-native saves. Do not silently discard this tail: after-load scheduling can diverge.

### 3.4 Save lifecycle and user-visible behavior

**S:** `season.c` appends season bytes, then writes a directory row, then updates member count for a new row. An extension-write failure is reported as a failed save, because the otherwise readable save may identify the wrong start season. The write checks the 32-bit container limit and rejects a resulting file end beyond `0xfffffffe`.

**S:** [`src/recomp_rt/win/savename.c`](../../src/recomp_rt/win/savename.c), `suggest:46`, uses the active manager's club short name, falling back to long name, converts/sanitizes up to 24 characters and appends `YYYY-MM-DD`. A manager with no club retains the original suggestion. [`files.c`](../../src/recomp_rt/win/files.c), `save_backup:214`, rotates the previous three copies for eligible top-level `.sav` files, excluding `temp_save_file.sav`.

**W web-native transaction:** stop at an engine-defined stable boundary; flush domain events and engine/RNG state; write a new immutable save blob; compute its checksum; atomically publish the save manifest; retain the three prior published revisions when backups are enabled. Show completion only after publication succeeds. Retry must not create duplicate game actions. A failed save must leave the last published save loadable.

Web-native save manifest fields: `schemaVersion`, `engineVersion`, `sourceRevision`, `dataSetDigest`, `rulesDigest`, `gameId`, `gameDate`, `timeSlice`, `activeManagerId`, `stateRevision`, `snapshotDigest`, `rngStateFormat`, `rngState`, `createdAtUtc`, `label`, `parentSaveRevision`, and subsystem payload digests. `activeManagerId` is the canonical stable identity; keep an optional `legacyManagerSlot` only in compatibility metadata with an explicit slot-to-ID mapping. The timestamp is metadata; it must not affect simulation.

**U compatibility gate:** exporting a new web world to a valid original `.sav` requires every original member schema, link restoration, file-backed lists, histories, counters and random state. Only the container and selected extensions are established here. Until that gate passes, label downloads as web saves and treat original-save import as an explicit supported/unsupported capability, never as generic “restore.”

## 4. New-game and manager flow

### 4.1 Source-backed setup behavior

**S:** [`src/cm3/setup.c`](../../src/cm3/setup.c), lines 18–22, declares Start New Game, Quick Start Game, Restore Saved Game, Delete Saved Game, Network Play, Game Settings, Hall Of Fame, Game Credits, and Web Sites. The hand-port `setup_input` at lines 74–99 explicitly does not implement actions 2–9. Thus it is not the full runtime's screen specification.

**S:** Bosman [`database.c`](../../src/recomp_rt/win/database.c), [`season_screen.c`](../../src/recomp_rt/win/season_screen.c), and `options.c` add the season/changing-rules row and Extras/Options presentation. `choose_apply:925` validates the selected season/data folder; prepares a selected season data set; stores season and database together; invalidates the loaded data set; and applies the next-new-game season/rules choice. A failed prepare or invalid folder blocks the choice with an explanation. Persistence failure can leave a choice active for the current native run only; a web client must display unsaved preference state honestly.

**S:** `setup.c:138` defaults real players and attribute masking on. Nation flags are background `1`, selected `2`, extra division `4`. The next step is enabled only when there is a selected nation (`select_leagues_build:309`). A background-only nation therefore does not satisfy this condition. The hand port's original 15-nation presentation and memory-check comments are not the limit of the enhanced runtime; use the Bosman selection hooks and actual supported season data when implementing “Select All.”

**T:** [`tests/smoke/smoke_run.py`](../../tests/smoke/smoke_run.py), `STEPS` near lines 105–136, exercises: intro → first-start settings → Setup Game → Select League(s) → Enter Name → Select Nationality → Select Team → club profile / Take Control → Please Confirm → Manager Status / Finish → first news → manager menu / Go on Holiday → confirmation. This is a test itinerary, not a fresh observed run.

### 4.2 Web state machine

**W:** implement a `NewGameDraft` separate from a running game:

`EDITING → VALIDATING → CREATING → READY_FOR_MANAGERS → READY → RUNNING`.

Validation failure returns to `EDITING` with field errors; import/creation failure becomes `FAILED` with a retryable error and leaves existing games untouched. Cancellation discards only the draft. Creation locks the data-set revision and selected rules profile. Progress events identify actual completed initialization phases; do not show “ready” until all required managers and engine initialization complete.

Required draft fields: start season, data-set ID/digest, changing-rules choice, selected/background/extra-division flags per eligible nation, real-player choice, attribute-masking choice, original settings requiring recovery, and a deterministic seed supplied by the backend or explicit test input. Existing-game settings must not change when the user edits defaults for the next game.

`ManagerDraft` stages: identity → nationality → eligible team choice → control confirmation → manager status. Club selection is a read operation until Take Control/confirmation commits it. Back navigation restores prior selections; adding another manager must not recreate the world. Club availability and slot capacity must be validated server-side. The source reserves 16 manager rows, but the precise UI/active-manager limit and unemployed/international-manager start rules require original callback verification before declaring full compatibility.

## 5. Screen and action inventory

This is the implementation coverage contract, **not** a claim that every screen below has been reconstructed. Each family needs an inventory of views, tabs, columns, filters, sorting, empty states, commands, validation, and permissions. “R/U” rows are required investigation tasks identified by the repository's existing screen families, not permission to invent modern Football Manager features.

| Screen family / route proposal (**W**) | Minimum state and interactions | Evidence / remaining gate |
|---|---|---|
| Setup `/` | New, quick start, restore, delete, options, season choice, extras; persisted next-game preferences | S `setup.c`, Bosman `database.c`, `season_screen.c`; original quick-start paths U |
| Season choice `/new/season` | 2001/02–2025/26 tiles, era grouping, selected data-set details, changing rules, custom folder/season, unavailable-data explanation | S `season_screen.c`; exact labels/spacing from runtime screenshots U |
| League choice `/new/leagues` | Selected/background/extra division, select all/deselect, real players, masking, start eligibility | S `setup.c`, Bosman selection hook; active runtime limits require source/test confirmation |
| Manager setup `/games/:g/managers/new` | Name, nationality, team choice, profile before take control, confirmation, manager status | T smoke itinerary; original callbacks U |
| News `/games/:g/inbox` | List/detail, linked people/clubs/competitions, read state, required response options, dates, blocking decisions | R `news_screens.cpp.md`; event-specific response semantics U |
| Manager `/games/:g/managers/:m` | Profile, job/career, status, manager actions, holiday and return | R manager screens; holiday hooks S; original resignation/retirement/job paths U |
| Squad `/games/:g/clubs/:c/squad` | Squad categories, visible columns/views, ordered selection, links, selection changes, Assistant menu | S `assistant.c`, `columns.c`, `views.c`; original full menu and sorting U |
| Club `/games/:g/clubs/:c` | Overview, staff, fixtures/results, histories/records, finance/board/other existing tabs | R `club_screens.cpp.md`; exact tab/action set requires runtime inventory |
| Person `/games/:g/people/:p` | Identity, nationality, club, positions, displayed attributes, condition/morale/form, career, contract and permitted contextual actions | S groups/name/column hooks; original full profile and action set U |
| Tactics `/games/:g/clubs/:c/tactics` | Formation, slot assignments, team/player instructions, set-piece choices, saved tactics, legal pre/in-match changes | S `tactics.c`, columns hooks; authoritative enums and editor transition details in gameplay chapter / U |
| Training `/games/:g/clubs/:c/training` | Existing schedules, assignments, intensity/category controls, reports | R `training_screens.cpp.md`; exact fields/ranges and apply/cancel behavior U |
| Scouting/search `/games/:g/search` | Player/staff search, filter criteria, visible result fields, shortlist, scout reports/assignments, follow-up actions | S `playersearch.c` for broadened list; remaining predicates and reports U |
| Transfers `/games/:g/transfers` | Current deals, negotiation, listing/status, offers, acceptance/rejection/withdrawal and pending state | R `transfer_screens.cpp.md`; exact clauses and state machine U |
| Contracts `/games/:g/contracts` | Offer/renewal/counteroffer details, financial/term validations, confirmation and negotiation news | R `contract_screens.cpp.md`; exact enums/ranges U |
| Competitions `/games/:g/competitions/:c` | Table(s), stages, fixtures/results, schedule, rules, statistics, history, awards | T competition screen/data sweeps; S season modules; original views U |
| National teams `/games/:g/nations/:n` | Senior/other squads that exist, call-ups, availability, fixtures, tournament participation, management actions | R national-team screens, S call-up hooks; exact permissions and squad workflow U |
| Match `/games/:g/matches/:m` | Pre-match team selection, kick-off, commentary, score/time, ratings/stats, substitutions/tactics, interval/end continuation | S commentary/column hooks, T match smoke; original event/action conditions U |
| Team views `/games/:g/clubs/:c/team-views` | Best XI + depth per slot, formation control; Compare players, two selections, eight-axis polygon | S `views.c`, `best_xi.c`, `compare.c`, `compare_layout.c`; read-only to world |
| Awards/history `/games/:g/history` | Existing award winners, records, manager/player/club career history, hall of fame | R award/history screen maps; exact qualification/stat aggregation U |
| Saves `/saves` | List/restore/create/rename-label/delete/download, date/club/season metadata, backup revisions, corrupt/incompatible messages | S save hooks/container; original member semantics U |
| Options `/options` | Bosman option set and recovered original options; applicable scope and persistence feedback | S `options.h`, `options.c`, `options_screen.c`; original full settings U |
| Printout `/games/:g/printouts` | Read-only text report/PDF download from a supported squad/competition view | S `print_pdf.c`, `net.c`; template details must be recovered per report |
| Extras `/extras` | Hall Of Fame, credits, external website entries | S Bosman extras builder; original content/assets U |

A route is not the implementation unit for gameplay. Each mutation must call the same server-side domain command regardless of whether it originates from news, a profile, a menu, or the match screen. The client cannot bypass negotiations by updating a contract row directly.

[Chapter 07](07-original-negotiation-evidence.md) adds original-instruction evidence and 65,867 passing isolated cases for bounded offer-closure, talks-status, contract-flag and offer-value contracts. It establishes those specific predicates and side effects; complete negotiation state transitions, clause meanings, player willingness and bid acceptance remain unresolved as stated above.

## 6. Verified Bosman display rules

### 6.1 Attribute groups and masking

**S:** [`groups_layout.c`](../../src/recomp_rt/win/groups_layout.c), lines 9–24, sets these outfield groups:

| Group | Attributes in displayed order |
|---|---|
| Technical | Crossing, Dribbling, Finishing, Heading, Long Shots, Marking, Passing, Set Pieces, Tackling, Technique |
| Mental | Aggression, Anticipation, Bravery, Creativity, Decisions, Determination, Flair, Influence, Off The Ball, Positioning, Teamwork, Work Rate |
| Physical | Acceleration, Agility, Balance, Jumping, Pace, Stamina, Strength |

Preferred Foot, Form, Morale, Condition, and On Loan follow Physical in their existing order. A goalkeeper replaces Technical with: Handling, Reflexes, One On Ones, Consistency, Passing, Set Pieces, Technique. Other columns remain.

**S:** [`groups.c`](../../src/recomp_rt/win/groups.c), lines 95–115, implements the two goalkeeper additions: One On Ones reads signed player byte `+0x35` and calls the existing relative-rating conversion at wrapper target `0x544380`; Consistency reads byte `+0x21`. Knowledge checks use attribute IDs 32/33 and wrapper target `0x543280`; an unknown value uses byte `0xf9`. The conversion and knowledge algorithms themselves are unresolved until their original bodies are read. Do not substitute raw `+0x35` as a displayed 1–20 rating.

This is a real exception to the glossary's broad statement that hidden values never appear. Current executable Bosman source explicitly adds these two values for goalkeepers. The profile exception does not authorize disclosing current/potential ability or other hidden values in APIs, scorer features, or exports.

### 6.2 Colours and match columns

**S:** [`columns.c`](../../src/recomp_rt/win/columns.c), `columns_attribute_colour:95`, uses bands 1–5, 6–10, 11–15, and 16–20; values outside 1–20 do not receive an attribute-band colour. Condition uses below 75 red, 75–89 amber, 90+ green. Morale colour uses values below 8 red, below 15 amber, otherwise green; the comment documents six labels with thresholds 4, 8, 12, 15, 18: Very Low, Low, Ok, Good, Very Good, Superb. The label getter remains authoritative for wording.

Match Rating tactics view adds positions and condition; Player Ratings adds condition for both teams. These additions apply to widescreen, while the tactics Condition view adds positions in both classic and widescreen. `ratings_condition:315` identifies players by person reference rather than assuming the summary and live-match rows have the same order. The condition conversion there truncates `floatValue * 0.01` toward zero with negative values clamped to zero.

**W:** expose the semantic condition/rating/position fields, then let Angular choose responsive columns. Use textual status and accessible labels in addition to colours. Merely opening a screen or changing its view must not advance the RNG, alter selected players, or mutate the simulated world.

### 6.3 Team views and polygon analyser

**S:** [`views.c`](../../src/recomp_rt/win/views.c), `views_build:165`, has two meaningful tabs: Best XI and Compare. Squad depth is integrated in Best XI; do not create a third tab from an outdated settings comment. Missing comparison choices are filled with the first two candidates. Compare selections and formation browsing are presentation state; applying an assistant selection is a separate mutation.

**S:** [`compare_layout.c`](../../src/recomp_rt/win/compare_layout.c), lines 8–44, defines:

| Outfield axis | Values |
|---|---|
| Defending | Tackling, Marking, Positioning |
| Physical | Strength, Stamina, Agility, Balance |
| Speed | Pace, Acceleration |
| Vision | Passing, Creativity, Flair |
| Attacking | Finishing, Off The Ball, Long Shots |
| Technique | Technique, Dribbling, Crossing |
| Aerial | Heading, Jumping |
| Mental | Decisions, Anticipation, Work Rate, Teamwork, Determination |

| Goalkeeper axis | Values |
|---|---|
| Shot Stopping | Reflexes, Agility |
| Handling | Handling |
| Aerial | Jumping, Bravery |
| Command | Positioning, Influence |
| Distribution | Passing, Technique |
| Speed | Pace, Acceleration |
| Physical | Strength, Stamina, Balance |
| Mental | Decisions, Anticipation, Work Rate, Teamwork, Determination |

For nonzero input attributes only, the result in tenths is integer division `(sum * 20 + n) / (2 * n)`; if no values are available, result is `-1` and text is `-`. Corner `i` uses angle `-π/2 + i*2π/8` and radius `r*tenths/200`, with missing axes plotted at zero radius. Upstream must normalize masked values to missing, not feed sentinel `0xf9` into the arithmetic. Goalkeeper comparison rows currently include IDs 1–31 and group headings only (`compare_rows:47`), so the new goalkeeper profile extras are not automatically polygon axes or comparison table rows.

### 6.4 Player search

**S:** [`playersearch.c`](../../src/recomp_rt/win/playersearch.c), lines 25–86: `full_player_search` defaults off. When on, extra unknown players stop joining at 30,000 list entries; known players can replace previously inserted extras so the option does not displace all original eligible players; rendered result count is capped at 32,767. These are native UI limits, not an instruction to lose data from a web database.

**W:** implement paginated search over the backend projection with stable sort plus ID tie-break. If strict list parity is required, provide a compatibility result policy reproducing the native cutoffs and inclusion ordering. If removing the native cutoff, record it as an intentional web improvement, while retaining attribute masking and the original knowledge-eligibility rule when full search is off. Listing a player never grants knowledge of every attribute.

## 7. Options and their scope

**S:** [`options.c`](../../src/recomp_rt/win/options.c), `defs` at lines 21–68, and [`options.h`](../../src/recomp_rt/win/options.h) enumerate the following complete Bosman settings set at this revision:

| Key | Default | Allowed form / web mapping (**W** for mapping) |
|---|---|---|
| `display` | `widescreen` | `widescreen` or `classic`; legacy `hd` falls back. Use selectable layout preference |
| `space_to_continue` | on | Boolean; only dispatch while Continue is available and no text entry is active |
| `shortcuts` | on | Boolean; back/popup/fullscreen controls subject to browser capabilities |
| `sharp_picture` | on | Native scaling preference; relevant only if showing a rendered framebuffer/canvas |
| `save_folder` | empty | Native filesystem location; web uses save library/download destination, not arbitrary server paths |
| `sound` | on | Master sound switch; original sound preference also applies |
| `clickable_names` | on | Player/club profile links; selection not world mutation |
| `attribute_colours` | on | Bands described above |
| `faster_days` | on | Holiday pacing; must not skip simulation work |
| `match_columns` | on | Positions/condition/rating display policy |
| `remember_window` | on | Native geometry; web persists layout preferences only |
| `save_backups` | on | Three previous published save revisions |
| `condition_colours` | on | Condition/morale bands |
| `database` | `3.9.68` | `3.9.68`, `3.9.60`, `3.9.65`, or `folder:<name>`; web stores a resolved data-set ID |
| `squad_views` | on | Team views / comparison features |
| `attribute_groups` | on | Grouped profile attributes including current goalkeeper exception |
| `foreign_colours` | on | Foreign-player indication for the next relevant fixture; derive from eligibility rule, not nationality string inequality |
| `full_player_search` | off | Broadens inclusion, not knowledge |
| `changing_rules` | on | Default for **next** new game; each created game preserves its own value |
| `season` | `2001/02` | Through `2025/26`; paired with data-set choice |

Native switches accept case-insensitive on/off, yes/no, true/false. Invalid values retain defaults; settings rewriting preserves unrelated lines/comments. `options_set` may succeed in memory but fail to persist. The web API should use typed booleans/enums and atomic preference updates rather than exposing text-file parsing. Keep account presentation preferences distinct from immutable-per-game choices. Original Game Settings such as database size, currency and sound/display behavior must be separately inventoried; they are not fully enumerated by this Bosman additions table.

## 8. Navigation, input, and accessibility contract

**S:** [`keys.c`](../../src/recomp_rt/win/keys.c), lines 38–88, suppresses Continue/Back while text entry is active and while a popup is open; a command must target a currently clickable left-bar button. A label that looks like Continue but is disabled must not advance. Popup-closing logic finds a point outside open popup areas. Thus keyboard commands must use the same availability checks as mouse commands.

**W Angular behavior:** maintain navigation history with entity ID, selected tab, filters/sort, and scroll position; Back returns to the previous view without undoing committed world commands. Dirty command forms distinguish cancel from submit. Return focus to the initiating control after modal close. Use proper table semantics, keyboard focus, labelled buttons, and text alternatives for colours/polygon information. Provide accessible text equivalents for all match events. Scrolling, sorting, resizing, opening profiles, choosing compare players, and printing must be read-only with respect to simulation.

The native rendering canvases are 800×600 classic and 1280×800 widescreen. These establish reference screenshot sizes, not an instruction to make the Angular interface unusable on smaller screens. A responsive presentation can preserve information/actions while reflowing panes. Pixel parity, if required, needs a separate reference screenshot suite; gameplay parity does not follow from a matching screenshot.

## 9. Assets, localization, and printouts

**S:** [`src/si/langlib.c`](../../src/si/langlib.c) is a limited hand-port formatter with a `3.9.68` version string and token-note stripping; it is not a full recovered language database reader. [`src/si/gui_font.c`](../../src/si/gui_font.c), `gui_font_load:90`, provides a bitmap-font reader. The original language strings, commentary/event configuration, fonts, pictures, and audio are separate artifacts, not present merely because a source reader exists.

**W asset manifest:** for each file record path, SHA-256, size, media/format type, dimensions/sample metadata where relevant, source data set/build, decoder version, and usage references. Include logos/intro, backgrounds, club/nation pictures, bitmap fonts, glyph tables, panel assets, match sounds, language tables, event/commentary strings and configuration. Preserve any ID-based lookup relationship. Do not replace missing commentary with invented event logic.

Decode original text with explicit code page and convert to Unicode. Render imported text as text, never raw HTML. Keep formatting tokens, pluralization, substitution types, and entity links in a structured localization schema. Store canonical entity IDs separately from localized display names. Long names, accents, currency formatting, and RTL layout are distinct presentation concerns; adding new language support is a web enhancement and must not alter stored IDs or numeric simulation inputs.

**S:** `net.c:43` routes the game's print function toward the PDF writer and presents a PDF destination. **W:** a printout request is an immutable read at one state revision, produces a downloadable document containing the same authorized visible values, and cannot reveal masked attributes. The game-data and original-asset licensing provenance is separate from the repository's code licence; preserve the repository's notices and attribution with any derived work. A full rights assessment is outside this technical specification.

## 10. Multiplayer and multiple managers

**S:** [`src/recomp_rt/win/net.c`](../../src/recomp_rt/win/net.c), lines 4–25, intentionally returns `WSASYSNOTREADY` from `WSAStartup` and failure from other socket calls. Therefore “Network Play” appearing in the inherited start menu does not establish a working online multiplayer implementation in this revision. Preserve full current Bosman scope by supporting its local manager-world behavior; do not claim original network compatibility or add an invented network protocol.

**W:** server ownership of one game world is compatible with multiple local human manager slots, but browser-account roles and online invitation flows would be a separate enhancement. Even for one user with multiple tabs, serialize mutations per game and require an expected state revision. An active manager switch changes viewer projection and command authority; it must not expose another manager's private decisions through cached responses. Exact turn order/Continue gating across manager slots remains an original-engine investigation gate.

## 11. Proposed read models and commands

These are **W interface boundaries**, to be bound to verified gameplay handlers; they do not fill unknown gameplay algorithms.

| Read model | Required contents and prohibitions |
|---|---|
| `GameSessionView` | Game ID, revision, date/slice, active manager, processing/paused/match phase, pending blocking decisions, available commands; no engine memory addresses |
| `PlayerProfileView` | Stable person ID, visible names/identity/positions, grouped visible attributes with explicit knowledge state, visible contract/history/condition/morale; never raw hidden values |
| `SquadView` | Club/team category, stable row IDs, selection slots, next-fixture eligibility/status, visible columns, deterministic sort/pagination metadata |
| `CompetitionView` | Season/stage identity, published rules, tables and tie-break explanation when known, fixtures/results/history, qualification/promotion status |
| `MatchView` | Match revision/phase/clock, teams/score, ordered published events, visible statistics, allowed next actions; no future outcome/seed leakage |
| `NewsView` | Item ID, time, text tokens/entity links, read status, response options and expected version; only pending responses can commit |
| `SaveView` | Save revision, label, in-game date, manager/club/season, format compatibility, checksum verification state, backup lineage |
| `DataSetView` | Immutable digest, source identification, season compatibility, record counts, validation findings, readiness |

Commands use the canonical envelope in [the architecture chapter](01-web-architecture.md): `commandId` as the idempotency key, `gameId`, stable `managerId`, `expectedRevision`, `kind`, typed `payload`, and server-side validation. The legacy manager-slot number is an internal engine mapping, not a client authorization identity. Representative command families: create/load/save world; add/switch manager; continue/holiday/return; set lineup/tactics/training; request scouting; submit/respond/withdraw transfer or contract offer; respond to news; make in-match change; finish/continue match. Read navigation and preference changes must not use the world-mutation event stream unless the setting actually changes gameplay. An implementation agent must complete the domain enum/range/action inventory before exposing a “generic update entity” endpoint.

## 12. Acceptance scenarios and unresolved recovery work

These are required checks, not results of tests run in this documentation pass.

| ID | Scenario | Pass rule |
|---|---|---|
| DATA-01 | Load every supported data-set format with the same decoder and reference loader | Counts and normalized bytes/references agree; unsupported formats fail explicitly |
| DATA-02 | Round-trip records with unknown bytes, missing names, invalid/high references, and mixed name-table pointers | Unknown bytes preserved; normalization exceptions match source; no accidental ID coercion |
| DATA-03 | Import missing/truncated/oversized files and hostile filenames | No partial published data set; clear diagnostics; bounded resources |
| SAVE-01 | Decode/encode versions 1–4 with all 256 possible byte values and runs around 1/2/127/128 | Decoded data exactly equal; stable reference encoder behavior |
| SAVE-02 | Load season part v1, v2, no part, future prefix-compatible version, old/new pending-qualifier entries | Correct start-year/rules/news/renames/pending state; malformed parts rejected |
| SAVE-03 | Save before/after a season point and a deferred international fixture, reload in a fresh process | Subsequent fixtures, rule changes and news agree with uninterrupted run |
| SAVE-04 | Interrupt save at data write, manifest write and publication; restore previous revision | No falsely successful save and last committed revision remains loadable |
| SAVE-05 | Save named for Köln/Málaga; no club; empty/sanitized short name | Valid Unicode filename, correct fallback/date; no path escape |
| UI-01 | Follow full new-game/manager smoke itinerary with selected/background nations | Same accepted choices and first playable state; blocked steps cannot be bypassed |
| UI-02 | Type spaces in name or open popup while Continue exists | No accidental world advance; keyboard and pointer enablement agree |
| UI-03 | Browse profiles/Best XI/Compare, change tabs/sort/viewport, return | Engine state/RNG and selected team unchanged |
| UI-04 | Inspect an unknown player as two different managers | Response bodies enforce viewer knowledge; caches cannot leak values |
| UI-05 | Toggle groups; open goalkeeper with known/unknown One On Ones and Consistency | Correct grouped cells and masking; relative attribute goes through correct conversion |
| UI-06 | Compare axis with no values, some missing, all 1s, all 20s | Missing marker or exact integer-tenths formula; no sentinel in mean |
| UI-07 | More than 32,767 search candidates | Stable pagination; explicit compatibility policy; original known eligible players retained |
| UI-08 | Change next-game season/rules while another world is loaded | Existing world's settings unchanged; paired data-set/season draft validated |
| UI-09 | Two browser tabs submit conflicting or duplicate commands | One committed transition; duplicate response replayed or stale revision rejected |
| ASSET-01 | Long localized names, high-byte CP1252 text, malformed tokens, missing asset | No corruption/injection; deterministic fallback and visible missing-resource diagnostic |

Outstanding recovery gates for a complete implementation:

1. **Original screen bodies:** enumerate each builder/input pair, capture all tab/menu variants, and trace each mutation into gameplay state. The function-map families in section 5 are the search queue, not completed recovery.
2. **Original settings and configuration:** recover complete `game.cfg`, language/event formats, database-size trimming, quick-start and auto-setup semantics. The hand port explicitly leaves some paths unimplemented.
3. **Full saved-world schema:** catalogue every member of representative saves (new game, pre-match, in-match if permitted, end season, long game, multiple managers). Decode subsystem-specific records, counts, links, sentinels and after-load reconstruction. Container success alone cannot close this gate.
4. **Player/official/non-player unknown fields:** connect every gameplay-used offset to its accessor/writer and differential fixture. The opaque portions of the current headers cannot be replaced by guessed attributes.
5. **Language and asset completeness:** the base installation Data assets are now matched and counted in section 13. Obtain/hash every selected season data set and resources outside Data; confirm native lookup order and missing-asset behavior.
6. **Multiple-manager semantics:** recover allowed slot count, club/nation control restrictions, turn order, Continue barriers, holiday delegation and manager removal/retirement flows.
7. **Runtime verification:** run existing smoke/same-save/screen-sweep/save-load tests against the supplied original and recompiled game. No runtime execution or complete all-screen visual capture is claimed in this chapter.

The web data layer, viewer projection rules, save publication mechanism, and source-backed Bosman presentation features are concrete implementation tasks now. Exact original-save interoperability and exhaustive original UI/action fidelity remain gated by the listed evidence; they must not be replaced by plausible assumptions and labelled complete. The additional artifact and manual evidence acquired during this investigation is recorded below and narrows several of those gates.

## 13. Actual installation and data-set audit

**A, read-only audit, 2026-10-28:** the installation at `/Applications/CM 01-02 Bosman Edition.app/Contents/Resources/game` contains a matching original:

The complete machine-readable audit, including expected/actual per-file hashes, decoded index ranges, and translated-name section boundaries, is retained in [`evidence/data-audit.json`](evidence/data-audit.json). It contains metadata only, with no copied original data payloads.

- `cm0102.exe`: 7,192,576 bytes; SHA-256 `0c78e7da6d0bcb4603fa179449bbe6d1b6b5e1c627a22c445d14d489bd9bff86`.
- All **106 `Data/` entries** from [`tools/clean_files/manifest.tsv`](../../tools/clean_files/manifest.tsv) match both size and SHA-256; zero missing or differing entries.
- `Data/index.dat`: 1,482 bytes; SHA-256 `cab19a09aa092fdf2200dc8cea0f72506b9ff924396d26e24fc59e7cc894fb03`; initial integers `(0, 0)`.
- The installation is an older Bosman release than the analyzed repository. Matching original/data bytes do not establish that its runtime contains current season enhancements.

Index entries decoded from that exact artifact:

| Ordinal | File | Table ID | Records | Byte offset | Format | Declared record extent ends at |
|---:|---|---:|---:|---:|---:|---:|
| 0 | club.dat | 0 | 13,214 | 0 | 2 | 7,677,334 |
| 1 | nat_club.dat | 1 | 426 | 0 | 2 | 247,506 |
| 2 | colour.dat | 2 | 34 | 0 | 1 | 1,972 |
| 3 | continent.dat | 3 | 6 | 0 | 1 | 1,188 |
| 4 | nation.dat | 4 | 213 | 0 | 2 | 61,770 |
| 5 | officials.dat | 7 | 4,567 | 0 | 1 | 196,381 |
| 6 | stadium.dat | 5 | 8,917 | 0 | 1 | 695,526 |
| 7 | staff.dat | 6 | 169,342 | 0 | 1 | 26,586,694 |
| 8 | staff.dat | 8 | 0 | 26,586,694 | 1 | No records |
| 9 | staff.dat | 9 | 30,865 | 26,586,694 | 2 | 28,685,514 |
| 10 | staff.dat | 10 | 139,769 | 28,685,514 | 2 | 38,469,344 |
| 11 | staff_comp.dat | 11 | 543 | 0 | 2 | 54,843 |
| 12 | city.dat | 21 | 6,682 | 0 | 1 | 374,192 |
| 13 | club_comp.dat | 12 | 433 | 0 | 2 | 46,331 |
| 14 | nation_comp.dat | 16 | 19 | 0 | 2 | 2,033 |
| 15 | first_names.dat | 13 | 44,281 | 0 | 1 | 2,656,860 |
| 16 | second_names.dat | 14 | 104,395 | 0 | 1 | 6,263,700 |
| 17 | common_names.dat | 15 | 13,256 | 0 | 1 | 795,360 |
| 18 | staff_history.dat | 17 | 385,846 | 0 | 1 | 6,559,382 |
| 19 | staff_comp_history.dat | 18 | 2,609 | 0 | 1 | 151,322 |
| 20 | club_comp_history.dat | 19 | 10,766 | 0 | 1 | 279,916 |
| 21 | nation_comp_history.dat | 20 | 273 | 0 | 1 | 7,098 |

Every declared extent fits its physical file. Each single-table file ends exactly at its declared extent; the three nonempty staff sections concatenate to the exact `staff.dat` size. This validates widths/ranges and artifact identity; it does not validate every semantic field or reference.

**A asset inventory:** the actual Data folder contains 20 `.dat`, 40 `.pct`, seven `.fnt`, ten `.lng`, fourteen `.cfg`, seven `.rgn`, three `.t2k`, two `.mbr`, and one each `.ldb`, `.dll`, `.ttf`, `.txt`. The only extra Data file beyond the clean-file manifest is `bosman_logo.rgn`. The seven fonts are `arial_14.fnt`, `arial_16.fnt`, `arial_18.fnt`, `arial_narrow_10.fnt`, `arial_narrow_11.fnt`, `trade_cond_24_bold.fnt`, `trade_cond_28_bold.fnt`. The ten languages use `eng/fra/ger/ita/esp/dan/hol/nor/por/swe.lng`. The fourteen configuration files are the ten `events_*` files plus `euro.cfg`, `player_setup.cfg`, `pundit.cfg`, and `weather.cfg`. This does not inventory resources in other installation directories.

### 13.1 Translated names are row-mapped data

**S:** [`src/recomp_rt/win/season_lng.c`](../../src/recomp_rt/win/season_lng.c), lines 1–75, supplies a concrete translated-name format and Bosman remapping path. The 32-byte header holds eight signed 32-bit counts, in order: clubs, continents, national competitions, nations, cities, club competitions, stadiums, staff competitions. Body section order differs:

| Body section order | Row size | Header count index |
|---|---:|---:|
| Clubs | `0x4f` | 0 |
| Continents | `0xba` | 1 |
| National competitions | `0x53` | 2 |
| Nations | `0x6d` | 3 |
| Stadiums | `0x34` | 6 |
| Cities | `0x34` | 4 |
| Staff competitions | `0x4f` | 7 |
| Club competitions | `0x53` | 5 |

The source maps translated rows using English names, preferring a matching row index and then an unused matching name; records without a translation retain their source names. Copies fitting the data set are retained; mismatching custom/season data gets rebuilt rows. The rebuilt language files become part of the save. Therefore importing a custom data set cannot reuse base `.lng` rows by index blindly. New web localization should key strings by stable IDs but retain enough provenance to reproduce this remapping. The `.lng` name table is distinct from `language.ldb` interface strings and `events_*` commentary.

**A:** the installed `eng.lng` header is `(13640, 6, 19, 213, 6682, 433, 8917, 543)`. Applying the section widths/order above gives exactly 1,993,486 bytes, equal to its physical length and manifest size. The 13,640 club rows include the 13,214 ordinary clubs and 426 national clubs. This validates the section boundaries of this artifact, not every translated string's semantics.

## 14. Manual-backed complete interaction checklist

**M:** the supplied 67-page English manual is `/Users/mikepenzin/Downloads/CM0102/MANUAL/manual_uk.pdf`, 1,708,328 bytes, SHA-256 `629647992bcb4dffd6cd94435671d3fe2c4efa5ecbfc423eb86ef5448d1502e7`. Page references below are PDF pages, matching printed numbering for the cited content. The manual proves that these interfaces/features were documented for the original release. Bosman hooks and the matched original must resolve version differences and precise algorithms. Descriptive advice in the manual must not become deterministic gameplay thresholds.

### 14.1 Original settings to implement alongside Bosman options

| Setting | Documented UI behavior | Manual pages / verification boundary |
|---|---|---|
| Language | English, Danish, Dutch, French, German, Italian, Spanish, Norwegian, Portuguese, Swedish | 4; actual ten language artifacts verified above |
| Currency | Select displayed currency; initial choice derives from machine configuration | 4; full currency list/rates and rounding U |
| Wage period | Weekly, monthly, annual display | 5; conversion/rounding must come from engine |
| Screen mode | Fullscreen/window | 5; browser fullscreen is presentation adaptation |
| Background changes | Enable/disable changing photographs | 5 |
| Sound | Match effects, documented only at slow/very slow commentary | 5, 59–60; current Bosman speed hook may differ |
| Database size | Minimum/Normal described with different player inclusion scopes | 5; exact trimming algorithm U, not a frontend filter |
| Matches stored | Retained match detail count, documented minimum 5,000 | 5; complete available values and pruning order U |
| Auto-save | Never or intervals from weekly through yearly | 6; full enum and scheduling boundaries U |
| Save compressed | Compress saves with a time/size tradeoff | 6; container codec source inspected |
| Save safely | Check storage capacity and avoid losing the save on insufficient capacity | 6; actual native implementation must outrank informal manual explanation |
| Flashing text | Toggle attention indicator animation | 6; web should honor reduced motion |
| Background matches | Normal/On/Off described for concurrently played scores | 6; exact mode effects U |
| Print destination | Printer, file, or both in original | 6; Bosman PDF replacement takes precedence |

Game Options also exposes save, add manager, manager status, settings, selected leagues, hall of fame, credits, websites, restart, and exit (page 15). Exiting with unsaved changes prompts the player. In a browser, an explicit Leave Game operation and autosave status should express the same recoverability; closing a tab cannot be treated as a reliable synchronous save opportunity.

### 14.2 Manager, navigation, and club workflows

| Interaction | Documented contract and required web controls | Pages |
|---|---|---|
| Password-protected manager/tactics access | Optional password, blank allowed, restore requests it when set; no original recovery facility. Web account authorization is separate from decoding a legacy protected save | 7, 11 |
| Foreground/background switching | Initially included background leagues can become foreground during a game; foreground can become background. Do not add a nation absent at creation through this control | 8–9, 15 |
| Random-player world | Use Real Players = No produces fictional players, not merely hidden real names | 9 |
| Multiple managers | Add during creation or later from Game Options; each supplies identity/team choice | 11–12, 15 |
| Continue | Day has morning/afternoon/evening; processes scheduled matches and progresses to a relevant stopping point | 12; exact processor ordering U |
| Manager menu | Senior squad, reserves, reserve-control toggle, board confidence, resign, news, player/staff search, manager stats, jobs, transfers, history, holiday, retire | 13–15 |
| Reserve control | Assistant manages by default; human can take reserve-team control and give it back; manual requires 11 full reserve players for matches and places them before first-team matches | 13, 36; competition-specific actual scheduling must be verified |
| Manager stats | Sortable manager comparison; default reputation, other statistics such as tenure/spending | 14 |
| Jobs | Vacancies and insecure positions; job application is separate from resignation | 14, 16 |
| Manager history | Achievements/deals/competition records/awards/managed clubs plus aggregated career statistics | 14 |
| Transfers overview | Browse agreed transfers and loans by month or year | 14 |
| Holiday | Assistant controls absent manager; support active/holiday status per manager | 14–15; Bosman next-match stop extends this |
| Retirement | Remove this human manager; other managers continue; if none remain, offer adding a new manager | 15, 22 |
| Nations/clubs browser and Find | Browse entities, text search club/person names, follow only genuinely inspectable entities | 12, 15 |
| Club finances | Current/previous month, current season/previous year income and expenditure; club statistics view | 16 |
| Fixture arrangement | Friendly: date then opponent, immediate availability check, later acceptance. Tournament/tour: country/date/opponent invitations | 18 |
| News | Unread indicator, history filtering, linked entities; Respond action only when item requests it | 13–14, 18 |
| Football-association contact | Ask postponement for international absences or widespread illness; complain about prior refereeing | 20; acceptance formula U |
| Board contact | Confidence/expectations, requests, reply via news; ultimatum offered at reply stage with job consequences | 20–22, 62–63 |

### 14.3 Squad selection and indicators

**M pages 17–18:** squad filters include position/side and next-match eligibility. Views/sorts expose attributes, playing statistics, and contract details. Clicking the selection box assigns the next empty slot; clicking an assigned symbol clears it. Dragging a position symbol assigns that slot to a player. **W:** provide both pointer drag/drop and keyboard/select-menu equivalents, with the same backend lineup validation.

| Indicator | Documented meaning | Required view/action |
|---|---|---|
| Inj (red) | Unavailable injury | Injury details; selection blocked |
| Inj (orange) | Returned to light training, can be risked | Show explicit fitness warning; engine resolves risk |
| Sus | Suspended | Applicable competition and ban details |
| Wnt | Other-club interest | Transfer-interest details |
| Bid | Active offer being considered | Negotiation view |
| Yel | One yellow card from a next-competition ban | Discipline context |
| Ret | Announced end-season retirement | Retirement date/status |
| Int | Away on international duty | Call-up/return availability |
| Fgn | Counts toward relevant foreign-player rule | Next-match rule explanation |
| Ine | Ineligible for next match | Specific reason |
| Wpm | Lacks valid work permit | Permit status |
| Tir | Manual describes condition below 65% | Verify exact current accessor; do not confuse with Bosman colour threshold 75% |
| Cup | Cup-tied by earlier appearance for another club | Competition/season |
| Loa | Listed for loan | Loan-status action |
| Lst | Transfer-listed | Transfer-status action |
| Unh | Unhappy | Click through to reasons |
| Abs | Absent from training/matches | Absence details |
| Ctr | Contract expired | Renewal/contract state |

These are independent status facts, not a single mutually exclusive player-status enum. Preserve source priority if only one indicator fits the native column; expose an accessible full reason list in the web view.

### 14.4 Person actions and histories

| Action | Documented inputs, eligibility and visible transition | Pages |
|---|---|---|
| Add/remove shortlist | Add from profile; remove from profile or shortlist removal mode; relevant future news follows shortlisted player | 35 |
| Relegate to reserves / promote to seniors | Moves squad assignment and may affect morale; preserve existing selection/eligibility validation | 35–36 |
| Set transfer status | Squad importance, transfer availability, loan availability | 36; full exact enum U |
| Offer new contract | Open negotiation for existing player; show willingness/demands or refusal | 36, 38–39 |
| Trial | Uncontracted player, duration two to four weeks; reserve/friendly participation only; departure at expiry unless contracted | 36; exact selectable durations to verify |
| Discipline | Warning or wage fine up to one month; reasons: no reason, unprofessional behavior, previous-match dismissal, violent behavior, poor performance | 36–37; amounts/consequences engine-owned |
| Release on a free | Confirmation with contractual cost and board consequences | 37; cost formula U |
| Nickname | Set/edit/remove, restore real name; original multiplayer shows nickname only to its creator | 37; web stores viewer-owned alias |
| Physio report | Select/request club physio assessment, show returned fitness assessment | 38 |
| Coach report | Select/request coach assessment of a club player, show ability/future assessment | 23, 38 |
| Approach to buy / loan | Other-club player; bid workflow or direct contract for unattached player | 38, 55–58 |
| Scout player | Assign scout and later report; can reveal otherwise unknown attributes | 23, 62–63 |
| Leave of absence | Offered for homesickness; return/absence outcomes handled by simulation | 63–64 |
| Rehabilitation | Physio recommendation for recurring injury; treatment/availability timeline | 64 |
| Compare players | Compare visible values; original relative colours and position comparison; unavailable position gets n/a. Bosman adds polygon/views | 64 |
| Create manager note | Categories: promising youngster, transfer prospect, contract renewal, squad member; available categories depend on relationship to club | 64 |
| Histories | Achievements, seasonal playing record/transfers/fees, injury history, suspension history/reasons/durations | 39 |
| Captain/penalties/squad numbers | Captain choice, ordered penalty takers or delegated default, manual/automatic seasonal numbers | 39–40 |

### 14.5 Negotiation, training, and scouting UI payload checklist

**M pages 38–39 and 55–58:** a transfer-offer form needs fee, monthly instalments (documented six months to two years), sell-on percentage (manual maximum 25%), appearance-contingent club and international payments, appearance threshold selectors (international ten to fifty in ten-match steps), completion date, public/private bid, and player exchange selection with profile inspection. A loan offer needs duration, cup-competition permission and wage share. A contract form needs wage, duration, signing/other monetary components confirmed from the actual screen, bonuses (goals/assists documented), release clauses, and squad role. An agreed transfer may be delayed; the manual discusses week-long delay requests, not a guaranteed acceptance. Exact ranges/clauses and negotiation outcomes remain engine recovery work.

**M pages 53–55:** training UI needs player-to-single-routine assignment, routine editor, coach-to-training-area assignment, Fitness/Tactics/Skills/Shooting/Goalkeeping categories, new-position and new-side retraining, and None/Light/Medium/Intensive intensities. A player participates in one routine at a time. Coach capacity, load effects and improvement probabilities must not be inferred from the manual's tactical advice.

**M pages 23–24, 58, 63:** scouting UI needs area/region assignments, youth scouting, individual-player reports, and next-opposition assignment from the staff screen. Opposition reports describe comparative team quality, customary formation and dangerous players. The club can have up to seven scouts according to the manual, matching the declared club array. Scouting costs/timing, knowledge revelation, and report accuracy remain simulation logic.

### 14.6 Match screens and confirmation behavior

**M pages 59–61:** the complete documented match-view set includes Overview, Match Stats, Player Ratings, Team Stats, Action Zones/Possession, Live League Table, Latest Scores, and Match Report. Overview includes goals/penalties/cards/injuries, referee, weather, and commentary. Player ratings use the displayed 1–10 scale. Action zones divide the pitch into three areas with last-five-minute, first-half, second-half and entire-match windows. Live league table appears for league-format matches; concurrent results must be displayed without committing them prematurely as final results. Match reports can be reviewed at half-time and full-time.

Substitutions use the team's tactics screen and **ask for confirmation when leaving**. If the departing player is a designated set-piece taker, the UI offers revision of the specialist list. Web editing should stage a proposed in-match lineup/tactic change and commit it only through the confirmation command; closing the editor must not silently apply it. The allowable substitution count and player eligibility come from the current competition/season, not a universal “three substitutes” constant.

The manual lists five commentary speeds, while Bosman adds Fastest. It also has an internally inconsistent sentence about cycling three speeds. Treat current source speed enum and handlers as authoritative. Weather is visible before the match and may lead to postponement or fitness effects, but its exact engine coefficients are not specified by the manual.

### 14.7 Additional acceptance scenarios from the manual

- A background league already included at creation can switch to foreground; a never-included nation cannot be activated through this control without a separately specified feature.
- A required news response blocks Continue until the applicable engine rule is satisfied; merely reading a different item cannot answer it.
- A trialist appears in reserves/friendlies but cannot be selected for an ineligible competitive fixture.
- Promoting/relegating a player changes squad assignment through one command and preserves a coherent lineup.
- Removing the final human manager offers adding a replacement; retiring one of several does not destroy the shared world.
- Player aliases/manager notes have the correct owner and survive save/load without changing the underlying person identity.
- A substitution draft cancelled on exit changes no engine state; a confirmed draft updates set-piece assignments consistently.
- The same person can have simultaneous injury, foreign-player, contractual, and interest statuses; the UI does not overwrite one with another.
- A staff/player search result that cannot open a full profile is rendered as non-actionable text or has a meaningful limited view, never a broken route.
- Every action in this chapter receives an eligibility predicate, typed payload, preconditions, committed effects, emitted news/events, save/load fields and at least one positive/negative scenario before it is considered implemented.
