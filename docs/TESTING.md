# How the Bosman Edition is tested

This page explains how we test CM 01/02: Bosman Edition before each release. For each test it gives what it checks, when it runs, the pass rule and the script.

Each test runs at one or more of these points:

- each change, while a feature or fix is built
- each merge, when finished work joins the main branch
- each release, before Max decides to publish

## Code tests

Small test programs check single parts of the code, such as the options file, the season rules tables, the cup draws and the heap. They run with `ctest` in the `build/` folder.

They run at each change and at each merge. The pass rule is that every test passes.

## Same-save checks

The original game picks its results partly from where its data sits in memory. A new screen or option can move that data and so change later matches, even when the player makes the same clicks. Our rule is that a feature must not change the game, except for the team that the player picks.

A same-save check plays the same clicks twice, once with the option on and once with it off. Then it compares the two saves. It also plays one run again to show that the result repeats. The checks start from a saved game at the start of a season, from a saved game before a match, and from a new game.

The script is `tests/smoke/smoke_run.py --same-save`, with `--match` for the match check. `tests/smoke/same_save_views.py` checks the screens.

The checks run before each merge of game code and again on the merged build. The pass rule is equal saves. Five files can differ only when the option opens a screen that the off run does not open, because that moves memory. We record each such case and tell Max. Any other difference stops the merge until we find the cause.

## Start-season soaks

For each start season from 2001/02 to 2025/26, a soak plays a new game with all nations loaded and the changing rules on. The manager goes on holiday and the quick result engine plays the matches.

Each soak goes on until the game completes at least one World Cup and one European Championship, with their qualifiers and every season change between them. It stops at about 1 September after the later tournament. A 2001 start runs to September 2004, and a 2016 start runs to September 2021 because Euro 2020 was played in 2021.

Near each 1 July the game saves. A new copy of the program loads that save and plays on.

The scripts are `tests/seasons/soak_run.py` and `tests/seasons/rules_report_check.py`. The soaks run at each release on the final main branch. A fix that touches a season change gets the same run to the same end point. The pass rule is no error box, no stall and a save and load that work at each July.

## Full play

Full play uses the match engine, as a player sees it. It runs five starts across the eras: 2002, 2006, 2013, 2021 and 2025, with all nations. Each goes to the same end point as the soaks, with the same save and load near each 1 July.

The script is `tests/seasons/full_play_run.py`. It runs at each release. The pass rule is the same as for the soaks.

## Long games

Long games run from 2001/02 to 2027/28 with all nations and different random seeds, to find faults that build up over many seasons. They use `tests/seasons/soak_run.py` with `--last 2027`. They run when the machine has free time, and they do not hold back a release. A fault that they find is fixed in the same way as any other.

## Memory and big saves

A game with all nations uses more memory each season. The heap now has room for about 2 GB of game data. A code test (`tests/recomp/test_heap.c`) fills a small heap and checks that it still serves new blocks.

The browser game has less memory room than the native one: its own data sits at guest 0x21000000 to 0x60000000, and its memory ends 64 MB past the heap's top. `RECOMP_GUARD=1` makes those addresses fault in the native game too (`src/recomp_rt/rt.c`), so a stray pointer that would break only the browser game stops the native run where it happens. The data sweep with all nations runs with it (`RECOMP_GUARD=1 python3 tests/seasons/data_sweep_run.py … --nations all`), once from the game's own data and once from a recent data update.

The guard only finds what a run reaches. Exit and Restart Game reached a stray read that no sweep did (November 2026, browser error reports): the clean-up 0x5cc7d0 calls free_game_data 0x82ff40, which at 0x830398 destroys the table of 0x167-byte records at [0xae2a38] through 0x59a5a0 and the array destructor loop 0x9462bb, with the empty function 0x5a8f60 as each record's destructor. The runtime wraps 0x5a8f60 (`season_uefa_cup.c`), and its class test read a record's first word, a sum of money, as a vtable pointer. Pointer tests in the runtime now use `rt_readable` (`rt.h`), which stops at the heap's top, so both builds answer alike. A new game of 2025/26 had 34 such records of 5,326. A guarded run that calls the clean-up after a new game (gdb at the first news screen: `call f_005cc7d0()`) faulted before the change and passes after it.

The same day a screen sweep under the guard with the October 2026 data (2025/26, England, France, Germany, Italy and Holland) found one more: the Friendly competition, which is no cup, opens with the cup rounds tab 0x492980, and that tab read its bytes at +0x45 and +0xa3 as a cup's draw and round table and followed a pointer past the browser's memory (0x492bb0..0x492bbd). `season_round_names_game.c` (cup_view) now lets the rounds and schedule tabs show the original's own "The draw has not yet been made." when the object has no round table. The sweep also left out two placeholders that the game never lists (FraCfaStubComp and FraLowerStubComp, `screen_sweep.c`). With both, the sweep passes the whole season, 280 screens. The Game Options commands (Save Game, Add Manager, Manager Status, Game Settings with every setting and a language change, Bosman Edition Options, Selected Leagues, Hall of Fame, Game Credits, Web Sites, Restart Game, Exit Game) and the manager's Resign and Retire ran under the guard from a loaded game with no fault.

Saves can now be up to 4 GB. A save that would be larger stops with a clear message and keeps the earlier save. `tests/smoke/big_save_run.py` makes a save above 3 GB and loads it again, checks a season part above 2 GB, and checks the clean stop above 4 GB. It needs about 12 GB of free disk for a short time. These checks run at the merge of a memory or save change and in the long games at each release.

## Screen sweeps and smoke screenshots

The screen sweep opens the tables, results, fixtures, schedule and history of every competition, once each season. The data sweep reads each competition from memory at the start and end of each season. It looks for dates outside the season and for empty tables where the rules give clubs. The scripts are `tests/seasons/screen_sweep_run.py` and `tests/seasons/data_sweep_run.py`. They run at each release.

The smoke test plays a fixed set of clicks through the main screens in the classic and widescreen modes. It compares each screenshot with a reference picture. The script is `tests/smoke/smoke_run.py --mode classic` or `--mode widescreen`. It runs at each merge and at each release. The pass rule is no error box and pictures that match. When a change draws something new on purpose, we look at the picture and record a new reference.

## Platform checks

The game ships for Mac, Windows and Linux. At each release the release script builds all three packages. The Windows build runs its smoke test under wine on the Mac. The Linux build is made and checked in a linux/amd64 container. Each platform runs the smoke test in both screen modes and a season change with a save and load.

## Rules source checks

The season rules come from a table with one row per competition and season. `tools/research/rules_lint.py` checks that every filled fact has a source. `tests/seasons/rules_report_check.py` compares the competitions that the game holds each season with that table. Both run at each change to the rules and at each release. The pass rule is no missing source and no difference.

## Database checks

The duplicate check, `tests/seasons/history_duplicates_check.py`, lists the history rows that a season database holds twice. The game clears the extra copies when it loads the database. `tests/seasons/season_history_run.py --duplicates` checks that no copy is left in the game.

Our own season databases have two more checks. `tools/seasons/own_db/duplicates.py check <database>` fails while any player is listed twice (same nation, matching names and birth dates, or the same full name at one club). `tools/seasons/own_db/long_test.py` plays a holiday from July 2001 to 1 September 2004 on the database, with a save each 1 July that the next step loads. `tools/seasons/own_db/save_check.py` then checks each save: big clubs keep full squads, and no player or name appears twice. The pass rule is no error box and every check passes.

We also check nation names in every database, such as Yugoslavia, the Netherlands Antilles and Kosovo. Draws and qualifying pools must pick only nations that exist in that season.

## After-load sweeps

Many faults show only after a saved game loads. An after-load sweep reads each season module for data that the save does not keep, or that a load does not rebuild or clear. Each suspect place gets a test that saves before the risky moment, loads, and plays on. These sweeps run at each release that adds season code. The July save and load in the soaks and full play repeat this check on every run.

## Reviews

A fresh reviewer reads each change before it merges. A small fix gets one fresh review. A new slice of work gets two. Each problem found is fixed before the merge.

## The rules we keep

### Every failure is fixed

Every failure that any test finds is fixed before the release (decision O151). Each fix needs a passing retest and a same-save check. We do not ship a known issue or move a failure to a later version.

### Run length

Every holiday or soak run, and every retest of a fix that touches a season change, goes on to the end point above (decision O166).

### Machine limits

The tests run many games at once on one machine, so we keep these limits:

- at most 16 test games run at one time
- no new game starts while the machine load is 24 or more
- builds go through a queue, with at most three at once

A test that fails only under heavy load runs again alone before we treat it as a fault.

### Release approval

When every test passes, we write a test report for the release. Max reads it and approves every release himself.
