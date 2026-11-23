# New game speed (0.6.1)

Max saw the "Player histories" step take a very long time on a 2025/26 start. This page gives the time of each progress step of game creation, where the time goes, and the three changes on the branch `claude/fix-061-newgame-speed`. None of the changes alters a game decision (rule Q152).

## How the times are measured

`RECOMP_PROGRESS_LOG=1` prints a line on stderr at each new progress text. The line gives the time of the step, the number of calls of the progress box, and the time spent inside the box (`display.c`, the replacement of the box 0x5e76f0). The first human turn closes the last step (`testrun.c`).

The driver is the new game leg of `tests/seasons/soak_run.py` with `RECOMP_SAVE_AFTER_DAYS=0`: the game is made, saved at the first human turn, and the program ends. The video and audio drivers are the dummy ones, as in all test runs. The machine is an M5 Max Mac. Other sessions ran games at the same time, so the absolute times change by about 10 % from run to run. Each comparison below is of runs that ran at the same time.

The steps are the progress texts in the order of the game. "Initialising game data" shows three times: the first part of the 44 steps of `FUN_00828110`, the same text again after step 13, and the last one before the first news. "Player histories" is inside step 13, the constructor `FUN_007bfd50` of `DAT_00b74350` (docs/research/2026-10-14-game-creation-inventory.md).

## Times before and after (seconds)

Before: the build of main (0eba6cb1) with the progress log only, four games at once, machine load 13 to 17. After: the branch, measured in the same way.

| Step | 2001/02 England | 2025/26 England | 2001/02 Select All | 2025/26 Select All |
|---|---|---|---|---|
| Loading database | 2.7 / 2.8 | 2.8 / 2.8 | 7.3 / 7.4 | 7.4 / 7.4 |
| Player Initialisation | 0.6 / 0.5 | 0.6 / 0.5 | 8.8 / 7.5 | 8.7 / 7.4 |
| Initialising game data (1) | 0.5 / 0.4 | 0.5 / 0.4 | 5.0 / 0.7 | 6.3 / 0.8 |
| Player histories | 2.0 / 1.1 | 2.5 / 1.3 | 204.1 / 20.9 | 260.4 / 26.3 |
| Initialising game data (2) | 0.3 / 0.3 | 0.4 / 0.3 | 7.0 / 4.3 | 60.1 / 13.9 |
| Initialising Leagues | 0.8 / 0.8 | 7.2 / 1.1 | 34.1 / 25.9 | 45.5 / 29.2 |
| Assessing Club Squads | 0.2 / 0.2 | 0.2 / 0.2 | 2.1 / 1.8 | 4.9 / 4.1 |
| Creating Shortlists | 12.2 / 12.1 | 12.3 / 12.1 | 135.1 / 112.6 | 49.7 / 42.8 |
| Initialising game data (3) | 1.9 / 1.7 | 2.1 / 1.6 | 9.1 / 5.4 | 7.9 / 6.7 |
| Total | 21.2 / 19.9 | 28.6 / 20.3 | 412.6 / 186.5 | 450.9 / 138.6 |

The England "after" runs are from the branch before the third change (the list search), which costs 0.3 s or less there.

Pairs that ran at the same time on the same build, with the host sort off (`RECOMP_QSORT=original`) and on:

- "Player histories", 2001/02 Select All: 145.7 s and 21.6 s.
- "Player histories", 2025/26 Select All: 188.9 s and 27.2 s.
- "Initialising game data" (2), 2025/26 Select All, with the list search of the original and of the host: 48.1 s and 13.9 s.

## Where the time goes

The progress box is not the cause. In "Player histories" the box has 1,544 calls, and they take 0.06 to 0.3 s in all. The box draws through the original's functions and the flip `0x5e2eb0`. The window shows a new frame at most every 16 ms (`win_present`, user32.c), and the renderer is made without `SDL_RENDERER_PRESENTVSYNC`, so a box call does not wait for the screen. These runs use the dummy video driver, so the cost of a real window was not measured. By the code it is at most one present each 16 ms.

The step time grows with the number of nations, far more than the number of rows. England has 2.0 s, Select All has 204 s, and the history file has the same 771,692 steps in both. A `sample` of the 2025/26 Select All game in "Player histories" (`run-logs/newgame-speed/old/prof-2025-all-histories.txt`, outside the repo) put 96 % of the main thread in `FUN_007c6a30`:

- `FUN_007c6a30` (thiscall: id, byte, byte, sort flag) adds a row of 6 bytes to a list at this+0x3c (count +0x44, room +0x40, grows by 100 rows). When the flag is set, it calls qsort `0x945333` on the whole list, with the compare `0x7c6d40`, after each new row.
- The compare `0x7c6d40` orders by the 32-bit id at +0 (the result is the difference of the two ids), then by the signed byte at +4. A byte of 0xff in either row counts as equal.
- With all nations the lists are long, and the game makes 8,331 such sorts in "Player histories" of a 2025/26 start. The recompiled qsort makes each compare a call through the runtime (`rt_call`), and the flags of each compare go through `cc_le` and `cc_l`.

After this change, a profile of the same step shows the host sort (`qsort6`, `qsort6_history_cmp`) and file work below `FUN_0058e9b0` (`opendir`, `getdirentries`, `open`). The sort still runs once for each new row, so its work grows with the square of the list length.

The second "Initialising game data" of a 2025/26 Select All start spent most of its time in `FUN_007ae350`, with `cc_l` from its loop. It is a linear search: is the id at [arg] in the list of [this+8] rows of 0x1a bytes from [this+4]? The game calls it 790,068 times in that start.

In "Initialising Leagues" our own `season_rules_find` (season_rules.c) showed in the profile: about a tenth of the samples. It looked at every row of the rules table, and the file list of each row, at each call.

"Creating Shortlists" (113 s for 2001/02 Select All) is the largest step that is left. Its time is in `FUN_0083f3a0` and `FUN_00600a50`. `FUN_0083f3a0` is a very large function. It is not studied here.

## The changes

### 1. The history sort in host code (qsort6.c)

The wrap of qsort `0x945333` (crt.c) sorts in host code when the compare is `0x7c6d40`. Every other compare keeps the recompiled qsort.

qsort is not stable: rows that compare equal can end in any order, and that order depends on the steps of the sort. So `qsort6` is a copy of the steps of the original, read from its code: `0x945333` (quicksort with the middle row swapped to the front and a stack of 30 ranges), `0x945487` (the short sort for 8 rows or fewer, which moves the largest row to the end) and `0x9454d5` (the swap, which does nothing when both rows are the same). Pointer compares are unsigned and the size compare after a partition is signed, as in the original (jl at 0x945420).

The rows alone were not enough. The first version gave the same rows but the same-save check from start.sav gave another `general.dat`. Check mode (both sorts on each call, see below) found the same rows and eax, so the difference was in the stack. The original writes values below esp: its frame, the frames of the short sort, the swap and the compare. It also writes its first two arguments, which it uses as the pointers into its stack of ranges. Later code reads some of these bytes before it writes them. `qsort6` now writes each of these stack words in the same order, and gives back the eax of the original. `FUN_007c6a30` returns the eax of qsort, and its caller at 0x7c0166 sets only al after the call, so the upper bytes of eax stay.

### 2. The list search in host code (hot_loops.c)

`FUN_007ae350` is replaced by a host copy of its 25 instructions. It writes esi and edi below esp as the two pushes do, and gives the same eax (al 1 or 0, the upper bytes of the found row, of the row after the last, or of the argument), edx and esp.

### 3. An index for season_rules_find (season_rules.c)

`season_rules_find(season, file, n)` uses an index made at its first call after a load. The key is a season and a file: each file of the comp_file list of a row, and the new_ file of a new competition. The rows of a key are in table order, so the nth row is the row that the scan finds. The scan is kept as `season_rules_find_scan`. This change touches no game memory.

## Tests

- ctest `qsort6`: the program runs `RECOMP_QSORT_TEST=1` before the game starts. It sorts 11,696 sets of rows from a fixed seed with both sorts and compares the rows, the stack bytes from esp - 0x140 to esp + 0xc, and eax. The sets have 0 to 4,000 rows, many equal ids, ids near 0x80000000 (where the difference of two ids wraps) and bytes of 0xff. Six lists grow by one row and are sorted again each time, as `FUN_007c6a30` does. It then calls `0x7ae350` 1,290 times with both copies and compares eax, edx, esp, esi, edi and the stack. A test with the cutoff of the short sort set to 7 gave 8,963 differences, a test with one stack write left out gave 11,610, and a test with edx off by one gave 1,290.
- ctest `season_rules`: 4,351,932 lookups (each file of each row, the new_ files, a file that does not exist, seasons 1999 to 2032, counts 0 to 5) give the rows of the scan. A test that gave no row for counts above 0 gave 342,215 differences.
- `RECOMP_QSORT=check` and `RECOMP_HOT=check` run both copies on each call in a real game and stop at the first difference. A 2025/26 Select All start: 8,331 sorts and 790,068 searches equal. A 2001/02 England start: 1,576 sorts and 8,975 searches equal.
- Same-save: a new run `host-loops-off` (`RECOMP_QSORT=original`, `RECOMP_HOT=original`) in `SAME_SAVE_RUNS` and `MATCH_RUNS` of `tests/smoke/smoke_run.py`. The new game, start.sav, prematch.sav and match checks compare it with normal: all four PASS on the branch, with ctest 52 of 52 and the smoke runs in classic and widescreen.

## Next

- "Creating Shortlists": 113 s of the 187 s that are left for a 2001/02 Select All start, 43 s for 2025/26, and 12 s for England only. A profile of `FUN_0083f3a0` and `FUN_00600a50` is the next step.
- "Player histories" still sorts the whole list after each new row. Fewer sorts would change the stack bytes and maybe the order of equal rows, so any such change needs the same proof as above.
- "Initialising Leagues" (26 to 29 s with Select All) and the file work below `FUN_0058e9b0` are not studied yet.
