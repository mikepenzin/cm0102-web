# New game speed, second pass (0.6.2 item 151)

Max saw a new game take a long time on "Player histories", the shortlists and the other progress steps, with a 2025/26 start of England, Germany, Italy and Spain (3.9.68 data). This page gives the time of each step on main and on the branch `claude/s062-151-newgame-speed`, where the time goes, and the changes. No change alters a game decision (rule Q152). The first pass is `2026-10-29-new-game-speed.md`.

## How the times are measured

The driver makes one new game with `RECOMP_PROGRESS_LOG=1` and `RECOMP_SAVE_AFTER_DAYS=0` (the new game leg of `tests/seasons/soak_run.py`), with seed 1, the dummy video and audio drivers and `changing_rules = on`. A step ends at the next progress text. Two steps hold waits of the test script: "Loading database" holds the clicks on Select League(s), and "Creating Shortlists" holds the typing of the manager's name. The table gives the time of the game's own work for those two steps: from the progress text to the wait of the script. A run with the real window (SDL cocoa) gives the same times as the dummy driver.

Other sessions ran games at the same time (load 15 to 22), so the times change by about 10 % between runs. Each "before" and "after" pair below ran at the same time.

## Times (seconds)

Main (22e068ce, the game code of db62ef0d) and the branch ran at the same time, load about 22, 3.9.68 data.

| Step | 4 nations, main | 4 nations, branch | All nations, main | All nations, branch |
|---|---|---|---|---|
| Player Initialisation | 3.6 | 4.5 | 7.7 | 8.9 |
| Initialising game data (1) | 0.8 | 0.8 | 0.6 | 0.7 |
| Player histories | 4.7 | 2.4 | 23.7 | 16.9 |
| Initialising game data (2) | 1.8 | 1.3 | 16.3 | 3.4 |
| Initialising Leagues | 4.3 | 4.8 | 32.2 | 30.1 |
| Assessing Club Squads | 1.3 | 1.3 | 2.3 | 2.4 |
| Initialising game data (3) | 2.2 | 1.4 | 3.5 | 1.9 |
| Up to the manager's name, and after it | 29.3 + 2.3 | 27.7 + 1.5 | 141.6 in all | 120.2 in all |

"Loading database" (7.5 s with all nations) and "Creating Shortlists" (47.8 and 48.4 s with all nations) did not change. Player Initialisation varies by about 1 s from run to run under this load.

Max's own case is slower for another reason. His settings use `database = folder:December 2025`, an Update Team data set. With a copy of his user folder, the 4-nation 2025/26 start took 722 s on the 0.6.0 app he has installed, 94.9 s on main and 85.5 s on the branch, up to the manager's name. The leftover .tmp files of his folder made no difference (99.0 s without them on main), and a fresh folder with only that data set took 89.8 s on main. With that data set "Player histories" is 88 % the sort after each new row: 28,339 sorts of about 59,000 rows. That sort is changed on the branch `claude/s062-151b-history-sort` (item 151b, below).

## The history sort (item 151b)

With Max's data set the rows before the new row are always in strict order (no two compare equal), and the new row lands anywhere. Then the rows after the sort are known: the old rows with the new row at its place. `qsort6_insert.c` writes them so, and finds the stack words that the sort leaves and its eax by a walk of the sort's steps backwards in time, which skips every part that can no longer write a word that is still open. Parts in order need no compare. The file's head comment gives the reasoning. In a test harness 20,000 random sorts of 2 to 70,000 rows gave the same words and eax as the full steps, with about 24 parts visited in place of about 15,000. RECOMP_QSORT=check: 28,339 sorts equal with the December 2025 data set, 8,322 with the 3.9.68 data and all nations. "Player histories" with the December 2025 data set: 35.9 s to 3.8 s.

## Where the time went on main

A `sample` of each step (main thread):

- "Creating Shortlists": the own code of fill_shortlist_slot 0x83f3a0 (35 %), human_manager_is_human_club 0x600a50 (13 %), club_is_national_team 0x5409b0 (6 %) and player_needs_work_permit 0x8c7560 (6 %), below create_initial_shortlists 0x83a1b0 and allocate_transfer_budget 0x83b6e0. 0x83f3a0 calls 0x600a50 at 0x83fab3 for each candidate, with the club that makes the shortlist.
- "Initialising Leagues": game_run_to_start_date 0x5cc850 plays the matches before the start date: play_matches 0x6b4360, pick_team 0x6dbc50, score_player_for_role 0x6e2f20 and the match engine 0x6b98c0. Not changed.
- "Player histories": the sort after each new row (add_language 0x7c6a30, qsort 0x945333 in host code since 0.6.1) took 72 % with all nations. With four nations the file work took most of the step: PlayerStats::PlayerStats 0x7bfd50 opens player stats history.tmp 16,816 times through FILE_LLIST::add_node_list 0x58e9b0, each time with a path lookup.
- The second "Initialising game data": is_retirement_pending 0x7ae350 (already a host copy, a linear scan of the retirement list) took 80 %, from is_staff_suitable_for_job 0x69f810 in add_job_vacancy 0x69ba50: 790,068 searches with all nations.
- "Player Initialisation": the own code of database_player_initialisation 0x535a80 (49 %) and database_player_to_attribute_vector 0x53a610 (36 %), about 20 million calls with all nations.
- The last "Initialising game data": FOG_OF_WAR_MAP::FOG_OF_WAR_MAP 0x5aff00 reads the history of each player (build_career_history 0x7c3e40, FILE_LLIST::get_nodes 0x58f120) and opens the history file for read 14,706 times with four nations.

## The changes

All are in `src/recomp_rt/win/hot_loops.c` and `files.c`. The code comments give the evidence for each.

1. An index for 0x7ae350. While a call of add_job_vacancy 0x69ba50 runs, the host copy finds the row in a hash index of the ids (the first row of each id), with the same eax, edx and stack. The index is made again when the list, its rows pointer or its count change. Every member of PLAYER_REGEN that may write the list drops it, whatever calls the member. It lives only while esp is below the esp of the outermost 0x69ba50.
2. A host copy of the common path of 0x600a50: a club of the computer that is no national side. Every other path runs the recompiled code.
3. A host copy of 0x53a610: no branch on the data, 77 writes in the order of the code.
4. The kept history file. The season change 0x7c1350 kept player stats history.tmp open since 0.6.2 (81c036e6). Now the scopes 0x7bfd50 ("Player histories") and 0x5aff00 (the last "Initialising game data") keep it too, for write and for read. A new open with the same name, access and disposition takes the kept file at position 0 without the path lookup. Every delete, rename, new folder, change of the current folder and memory file closes the kept files.

A fifth change was tried and dropped: a host copy of the first tests of the candidate loop of 0x83f3a0 (at 0x83fa25, through an INSNS entry). The check mode passed (899,815 loop heads equal with all nations), but most candidates pass those tests, and the step was not faster. The patch is in the run logs.

## Switches and checks

- `RECOMP_HOT=original` turns off changes 1 to 3; `RECOMP_HOT=check` runs the copy and the recompiled code on each call and stops at the first difference. `RECOMP_KEEP_FILES=0` turns off change 4. The same-save runs host-loops-off and files-off use them.
- ctest qsort6 runs the self-test (`RECOMP_QSORT_TEST`): the index on lists of up to 3,000 rows with repeated ids, a stale row and a search from an older frame; each common case of 0x600a50 and the bound 4984; 0x53a610 on random records.
- RECOMP_HOT=check on a 2025/26 start with all nations: 204,468,233 calls equal.
- Same new-game save as main: a 2001/02 base game of England, a 2025/26 start of the four nations (byte for byte), and a 2025/26 start with all nations (one file differs between two runs of main too, see below).
- A run of the four nations to July 2026, across a season change, with RECOMP_HOT=check: 1,365,775,019 calls equal, and the save the same as main's.
- The same-save set against main (`--ref-build`, `--jobs 1`): new game, start.sav and prematch.sav PASS. The match check gave one difference, in season-code-off, at a load that rose to 18.4; run again at load 15 it passed on main and on the branch, and ref-build was the same as the branch in that check.

## A fault found on the way

The save part bosman_season.dat of a new game holds the changed competition names (season_change.c, the renames). Each name is kept with snprintf in a realloc block of 51 bytes, so the bytes after the name's end are not set, and the save writes them. Two all-nations runs of main gave two saves that differ only in those bytes. The game reads only up to the end of the name, so no decision changes, but the save is not the same from run to run. Fixed on its own branch (open item 152).

## What is left

- "Creating Shortlists" is still the longest step for a few nations. Its time is the straight code of 0x83f3a0 for each candidate that passes the first tests, value_player_for_club 0x840d80 and the work permit test.
- "Initialising Leagues" with all nations is match play before the start date.
- "Player histories" with all nations still sorts the whole list after each new row. A faster exact sort must give the same order of equal rows and the same stack bytes.
