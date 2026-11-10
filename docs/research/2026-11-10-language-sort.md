# The language table at game creation: one sort per player (2026-11-10)

Why a new game with a large database took minutes in the browser, and what the runtime does about it
(`src/recomp_rt/win/language_sort.c`). Sources: `cm0102.exe` of the clean files (disassembly of the addresses
below), a CPU profile of the deployed browser build in Chromium, and native runs of the recompiled game on the
clean files.

## What the original does

- PlayerStats::add_language 0x7c6a30, thiscall (staff, language, fluency, sort) ret 0x10, appends an entry of 6
  bytes to the table at this+0x3c (count this+0x44, room this+0x40, grown by 100 entries with realloc at
  0x7c6aef): the staff id read from the staff row (0x7c6b7f..0x7c6b88), the language (+4), the fluency at most
  10 (+5, the signed compare at 0x7c6ba6). With sort nonzero it sorts the whole table with qsort 0x945333 (the call
  at 0x7c6bda) and the comparator 0x7c6d40.
- The comparator 0x7c6d40: the difference of the staff ids when they differ (0x7c6d5d..0x7c6d7a), else 0 when
  either language is 0xff (0x7c6d68, 0x7c6d70), else the difference of the languages as signed bytes.
- PlayerStats::PlayerStats 0x7bfd50 seeds the table at the start of a game in the loop 0x7c00fe..0x7c0271 over
  every person (stride 0x6e, count [0xae2364]): the languages of his nations go in with sort 0 (the push at
  0x7c015a), and the language of his club's nation goes in with sort 1 (0x7c0207, 0x7c0251) when the binary
  search 0x946464 (0x7c0228) does not find it. After the loop the table is sorted once more (0x7c0288).

So each person whose club plays in a nation with another language sorts the whole table again. The table holds
an entry for nearly every person, so the work grows with the square of the database.

## Measured

October 2026 Data Update (175,044 people, 865,410 careers), 2025/26, England, Germany and Spain, Germany chosen on
Select Start Season (0x81e8d0):

- In Chromium, the deployed browser build: the creation (Next on Select Start Season to the Enter Name screen)
  took 151 s. Of the 155 s profiled: qsort 0x945333 58.5 s, the comparator 0x7c6d40 12.9 s, the calls between
  them (`rt_call`) 11.5 s, qsort's helper 0x945487 3.4 s, the compare helpers `cc_le` and `cc_l` 6.2 s. The
  runtime's `season_rules_find` took 8.2 s.
- Native (Linux, this build): 48.9 s before, 17.7 s with the two changes below. The 3.9.68 data, England only:
  7.1 s before, 6.3 s after.

The browser build was not made again here, so its time after the change is not measured. The work that the
change removes was about 90 of its 151 s.

## The change

The wrap of 0x7c6a30 handles an entry with sort itself when the result is certain: when no two entries of the
table and the new one compare equal under 0x7c6d40, the sorted order of the table is the only one, so any correct
sort gives the bytes that qsort gives. The wrap finds the sorted start of the table (neighbours below 0), checks
that the entries after it (at most 64) and the new one compare equal to nothing, calls the original with sort 0
(the append and the growth stay the original's), and moves the entries after the sorted start into their places.
Entries that compare equal (a language 0xff, the same language twice), a staff id that the comparator's
subtraction could misorder (0x40000000 or more), or more than 64 unsorted entries: the original runs as it is.
Test switch `RECOMP_LANGUAGE_SORT=off`.

`season_rules_find` (`season_rules.c`) scanned all 7,102 rows of the rules table for each question. It now reads
the rows of the asked season only, through an index in the order of the table, so it returns the same row.

## The same game

New games saved at the first human turn (`RECOMP_SAVE_AFTER_DAYS=0`), the same clicks, with and without the
changes: all 118 parts of the game are byte for byte the same, for the October 2026 data (three leagues) and for
the 3.9.68 data (England). Only the runtime's part `bosman_season.dat` differs, and it differs between two runs
without the change too: bytes after the end of the competition names there are not set before the write.
