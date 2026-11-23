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

The script is `tests/smoke/smoke_run.py --same-save`, with `--match` for the match check. `tests/smoke/same_save_views.py` checks the screens. With `RECOMP_SMOKE_KEEP=<folder>`, a check that fails keeps its work folder there, with each run's user folder and stderr.txt, and prints the path.

The file speed-ups have their own runs. The path lookup keeps each folder's names until the folder changes (`RECOMP_FOLDER_CACHE=0` turns it off). The season change keeps the player stats history file open (`RECOMP_KEEP_FILES=0` turns it off). A new game keeps it open too, in "Player histories" and in the last "Initialising game data" (item 151), with the same switch. Both switches are read once and are on by default. The run `files-off` turns both off. The same-save runs end before a season change, so a run across a season change with both switches on and off compares the saves too. The game day speed-ups of 0.6.2 (open item 110) have no option either. Calls through an address use a cache in front of the search of the call table (`RECOMP_CALL_CACHE=0` turns it off), and the CRT call _ftol 0x945640 runs as host code with the same stack bytes and registers (`RECOMP_FTOL=original` turns it off, `RECOMP_FTOL=check` runs both on each call and stops at the first difference). The run `host-loops-off` turns both off with the host qsort and hot loops. The host copies of item 151 (the index of 0x7ae350, 0x600a50 and 0x53a610, `hot_loops.c`) are hot loops too: `RECOMP_HOT=original` turns them off and `RECOMP_HOT=check` runs both on each call. The history sort of item 151b (`qsort6_insert.c`: a list in order that gets one new row) is part of the host qsort: `RECOMP_QSORT_SORTED=0` turns it off alone, `RECOMP_QSORT=original` (in host-loops-off) turns off the whole host sort, and `RECOMP_QSORT=check` compares each sort with the recompiled qsort and prints how many sorts each path did. The Mac build also links the game with ThinLTO (`-DCM0102_LTO=OFF` turns it off); the same-save set with `--ref-build` against a build without it proves that it keeps the game.

The speed of the machine must not change the game either. In the original, the background matches of a match day play one engine step in each pass of the screen loop, between clicks and while a commentary line waits, and they take numbers from the one random pool that the rest of the game uses. So a busy machine played another game (open item 155). The runtime gives the background matches their own pointer, offset and C library seed (`match_numbers.c`, original-faults group `match_numbers`). `tests/smoke/match_timing_run.py` plays the match check at full speed and with a throttle that stops the game for short times, and every save must be the same. It runs by hand, at most two games at once. The match set with `RECOMP_ORIGINAL_FAULTS=keep:match_numbers` and `--ref-build` shows that nothing else changed. The load steps of the tests wait for the settings screen before they click on it (`fixtures.LOAD_STEPS`): steps on a fixed time from the window opening missed it on a slow machine, and that run played with another option. With the fix, the Basel friendly of the match check has an event early in the second half that opens the tactics screen, so the match steps answer it (`smoke_run.MATCH_EVENT`). Runs that keep `match_numbers` play the original's match and leave these steps out (`MATCH_STEPS_ORIGINAL`). The tactics screen moves memory blocks with `match_columns` off, so the match check accepts `LAYOUT_PARTS` for `columns-off`.

Long holiday runs can differ when their pace differs, even with the same seed and steps. So compare long runs only when they ran at the same pace, and repeat a run that differs before treating it as a fault.

Some reads of the original take memory that nothing set, so the game followed the memory layout (open item 138b). Since 0.6.2 each such read takes a defined value (`src/recomp_rt/win/layout_reads.c`): the captain's scale, the match players' unset fields, the daily staff pass's local [esp+0x70] (until then kept as the original's behaviour, open item 85), the simulated fixtures' flags, the database loans' morale, the place masks of a 23-place national squad, the ids of new random officials, a flag of the missed-training records, the new slots of the contract table, a new contract, the levels of a training schedule and a flag of the fitness incident records. These fixes change the game by decision. `RECOMP_LAYOUT_FIXES=keep` turns them all off, and `keep:<groups>` some of them (captain, players, daily, stats, loans, squads, officials, training, contracts, schedules, fitness). The same-save set with `keep` set for every run must equal the reference build from before the fixes. The ctests `layout_reads`, `layout_reads_kept`, `layout_reads_names` and `layout_reads_check` test them.

Open item 188 is not fixed yet. A run of a same-save check can stop on the assert box of `Match_Stats::write_match_record` 0x729820 (match_stats.cpp line 342, a null stats record) when it records a match (`record_match` 0x7293f0). The box shows only in runs with fixes kept, at high load, and item 186 stopped in the same place on earlier builds. A same-save run that stops there is a failure of item 188, and the release waits for its fix.

The checks run before each merge of game code and again on the merged build. The pass rule is equal saves. Five files can differ only when the option opens a screen that the off run does not open, because that moves memory. We record each such case and tell Max. Any other difference stops the merge until we find the cause.

Some changes alter the game by decision. Since 0.6.2 (Q48), the history rows of editions after the start date are removed in every game. This changes the first-season holders and so the play of a new game. The reference run `history-q48-off` plays the same game with that fix off (`RECOMP_HISTORY_AFTER_START=off`). The checks use it this way:
- `season-code-off` must equal `history-q48-off` in every part.
- In a new game, `rules-on` plays with the fix off and must equal `history-q48-off` except in the accepted rules-on parts. The run `rules-on-q48` plays the shipped default (changing rules on, fix on) and must equal the same run of `--ref-build`.
- In start.sav and prematch.sav, `history-q48-off` may differ from `normal` only in `index.dat` and `nation_comp_history.dat`, and there only by the 3.9.68 row of the Copa América 2001, which goes when the game plays its own final. In any other saved game it must equal `normal`. In a new game it is a reference run only.

The runs, parts and rows are in `q48_runs`, `Q48_SAVE_PARTS` and `Q48_SAVE_ROWS` in `smoke_run.py`.

Two runs hold for the base game only (start 2001/02, changing rules off). `season-code-off` starts every game in 2001 and writes no season part, so it equals the game without the switch only in the base game. `kinds-base` proves that the base game calls no rule kind, and every season game calls them. With `--custom-db`, a season data set that the game knows (for example December 2025) starts in its own season, so the normal game is a season game. The check first reads the start year and the changing rules from the part `bosman_season.dat` of the normal save. They must equal the season that the data set gives (`expected_season`, from `docs/research/rules-0.6.0/databases-signatures.tsv`), or the check fails. When that season is not the base game, the check prints `n/a` for those two runs (`BASE_GAME_RUNS`, open item 153). A check whose runs are all `n/a` fails. Without `--custom-db` the check compares them as before. ctest `same_save_base_game` checks this rule. Any other difference still fails, and a check that compares no run fails. The effect of the fix itself is checked by the history checker (`tests/seasons/future/history_check.py`).

## Start-season soaks

A soak plays a new game with all nations loaded, the manager on holiday and the quick result engine. Changing rules are on unless the row says off.

Every soak plays through a World Cup and a European Championship, with their qualifiers and every season change between them. It starts in season Y and stops at about 1 September after the later of the next World Cup and the next European Championship that it reaches. A tournament played in June or July of year Y comes before a start in Y and does not count. The November 2022 World Cup counts for a 2022 start. Euro 2020 was played in 2021, so a 2016 start runs to September 2021. A 2001 start runs to September 2004. The table of end points is `ENDS` in `tests/seasons/gate_run.py`, and the spec `docs/specs/2026-11-12-release-tests-0.7.1-spec.md` lists it.

Each soak saves near each 1 July. A new copy of the program loads that save and plays on. The pass rule is no error box, no stall and a save and load that work at each July. The release set below holds these soaks. A fix that touches a season change gets the same run to the same end point.

Each tile plays the database that the start screen gives it: 3.9.60 for 2001, our own Summer 2002, 2004 and 2006 databases, and the season databases for the other years.

## Full play

Full play uses the match engine, as a player sees it. It runs five starts across the eras: 2002, 2006, 2013, 2021 and 2025, with all nations. Each goes to the same end point as the soaks, with the same save and load near each 1 July.

The script is `tests/seasons/full_play_run.py`. It runs at each release. The pass rule is the same as for the soaks. It stays outside the 12-hour release set.

## Long games

Long games find faults that build up over many seasons. They play in the background set of the release set (see "The background soak" below): a Tier A game to 2300 and a full-world game from 2025 to 2100. They run when the machine has free time and do not hold back a release. A fault that they find goes on the open items of the next release.

## Date jumps

A date jump starts a new game in a far year, with every date of the database moved to that year. It needs an Update Team or Custom database. The test variable `RECOMP_START_SEASON` takes a start year up to 2099. With the date jump switch `RECOMP_DATE_JUMP=1` it takes a year up to 2300. The program reads the switch once at the start, and it is off by default. The same-save set proves that the switch changes nothing in a game of the normal start years (run `date-jump-on`). The season code itself has no year limit: its hooks, rules and save parts work in every year (ctest `future_2300` and `season_part` step them to 2301).

## The release set

From 0.7.1 a release passes a fixed set of 34 games that 10 games on the Mac finish in about 12 hours or less (spec `docs/specs/2026-11-12-release-tests-0.7.1-spec.md`). The list is `tests/seasons/future/run_set.tsv`. `tests/seasons/future/run_set.py` plays it several games at a time. A full-world season takes about 63 minutes with 10 games on the Mac, and a Tier A season about 7 minutes. The first timed run of a release measures the real time of each row.

The full-world rows (tier `world`) are a new game with Select All, the manager on holiday and the quick result engine. Each gets the history checker and the health checks on every save. It gets no Tier B limits, because those need decades of saves. Each row plays one seed. The seed comes from the release version: version 0.M.P gives M*10+P, so 0.7.2 gives 72. It stays fixed within a release, so a fix replay plays the same game, and the next release plays other games. `run_set.py --seed 72` sets it for every row.

| Rows | Settings | Seasons | Protects against |
|---|---|---|---|
| 8 spine soaks `sp2001`, `sp2004`, `sp2008`, `sp2012`, `sp2016`, `sp2021`, `sp2024`, `sp2026` | Plain Select All, each to its end point. Together they play every season from 2001/02 to 2029/30 once, in the real calendar. | about 33 | faults of the running seasons: a squad with one player in 11 slots (item 187), the Date box of 2024 (131), a Champions League place from 7th (206), the CONCACAF windows of 2023 and 2024 (214, 215), the Club World Cup 2025 (197), a Mexican Libertadores winner (219), the 24-team finals (217) |
| 18 tile rows `t2002` to `t2025` (every start that is not a spine row) | Select All with all six extra divisions, a new game through the first season change with the July save and load | about 22 | first-season faults of each database: European entrants of the first season (items 100, 222), rows held twice at the start (189, 176), missing history (197), a box at the new game (123), an extra division that the data does not fit (224) |
| 3 database rows `db3960`, `db3965`, `db3968` | 2001 on 3.9.60, 3.9.65 and 3.9.68 with all six extra divisions. 3.9.65 and 3.9.68 play to September 2004. 3.9.60 plays to September 2002, because its spine soak plays on to 2004. | about 8 | item 224 (line 656 on 3.9.60, line 643 on 3.9.65), item 123 |
| 2 rules-off soaks `off2001` (3.9.68 data to September 2004), `off2016` (to September 2021) | Plain Select All, changing rules off | about 8 | club cups on national match days with rules off (items 157 to 162), item 102 |
| 1 date jump `jump2098` | A new game in 2098 through the season change of 2100/01 | 3 | the non-leap year 2100, the World Cup 2099 (item 169) |
| 2 Tier A rows `ta2001` (2001 to 2060), `ta2026` (2026 to 2066) | Tier A, all nations, legs of 10 seasons, a checkpoint save each decade | 99 Tier A seasons | decade faults (items 201, 183, 218, 102, 194). Their checkpoint saves are the start of a fix replay. |

The six extra divisions are the English Conference, the German Regionalliga, Serie C2, the Spanish Segunda B, the Portuguese Second Division B and the Swedish Second Division. Select All sets none of them. The first three are clicks on the Select League(s) screen. The other three come from the test switch `RECOMP_TEST_EXTRA`.

The columns of `run_set.tsv` that the set needs: `database` (`tile`, `3.9.60`, `3.9.65` or `3.9.68`), `extras` (`none`, `all` or names joined by commas) and `last` (a season, or `tile` for the season before the end point of the start). After leg 0 the runner reads the save of the new game. It finds the database by the club names of its `club.dat` and the extra divisions by the flag of each nation record (the competitions of the extra divisions are in every save). A row whose save does not hold the data or the divisions that the row names fails with "wrong game", so a wrong setting cannot give a false pass.

For each leg the runner runs the history checker (`tests/seasons/future/history_check.py`) on the save and keeps one save a decade. A Tier A game skips the check of real results. A Tier B game also gets the Tier B limits: a national side with no manager for more than a season, a game day twice as slow as in the first seasons, and a save that would pass 4 GB before 2300. It starts no game below 100 GB of free disk. Each game ends with pass or fail. `run_set.py --list` shows the rows with the seed.

The release script (`tools/release/make_release.sh`) lists the set with the seed of the release, runs `run_set.py --verify` on the folder of the run with that seed, and runs online release test 1 (`tests/online/release_mac_run.py`). It stops unless every game passed on the same game code with a clean program, the seed matches, and the checker finds no fault on the kept saves. `make_release.sh --check-set --run-set <folder>` is a dry run that reads the set and runs the check, with no build. ctest `future_run_set` tests the script without a game, and `run_set.py --set quick` plays short games that test it, including a 3.9.60 game with all extra divisions and a 3.9.68 game.

The other release checks do not change: the smoke tests in both modes, the same-save set, the rules report, the screen and data sweeps, the Windows and Linux checks and online release tests 2 to 4.

### The per-fix check

Each fix gets these checks, in this order:

1. The fix's replay: the save before the fault, played with the fixed build to the end of that row. The fix's log line must fire.
2. The same-save set against the main build.
3. h-r2001-e: the rules report of the base game from 2001 to 2027 with England.
4. One fresh Opus review. A second review runs only when the first finds a real problem.
5. A full rerun of every release row whose saves or logs show the fix's trigger. The fix session gives a scan for the trigger. When no scan is possible, every row with the same start, database and rules setting reruns.

Release rows take 7 hours or less each, so a full rerun fits in one night. Every failure is still fixed and retested before the release.

### The background soak

Two games play only while the Mac is idle, from the release build on (`--set background`):

- `bg2300`, a Tier A game from 2001 to 2300, with its checkpoint save each decade
- `bg2100`, a full-world game from 2025 to 2100

They are not part of `--verify` and do not hold a release. A fault that they find goes on the open items of the next release, with its save. Faults of the years 2070, 2074, 2096 and 2222 came to light only in runs this long.

The scripts keep the Tier B, late and weekly tiers for one fix. The rows of 0.7.0 that were release checks (Tier A rows of seven starts with three seeds, the date jumps to 2198 and 2298, the Tier B games from 2001 and 2025, and the late games) are in `tests/seasons/future/run_set_legacy.tsv`, which only the ctest reads. A later release can copy a row back to `run_set.tsv`. The test-only weekly market (`RECOMP_TEST_MARKET_WEEKLY=1`, `market_weekly.c`) lets the AI transfer market of the day loop run on Mondays only. It is not the shipped game. ctest `market_weekly` checks the day rule from 2001 to 2301.

## Tier A runs

A Tier A run reaches a far year quickly. It plays only the competitions: the calendar, draws, entrants, league tables, season changes, history, finances, retirements, new players, contracts, and save and load. It skips the AI transfer market, background matches, new injuries and AI manager moves (sackings). Tier A has no AI sales. So a Tier A step, which is not original code, acts when an AI club fills all 50 squad slots. The club lets its weakest players over 21 go until 37 remain. Tier A has no AI signings either. So an AI club of a played league with fewer than 15 players runs the original's market for itself until it has 15 (open item 92: without it some MLS clubs fell to 1 to 4 players, and the All-Star game stopped the run). This adds about a fifth to the time of a year. Each match gets an instant result from the two sides' reputation, so the stronger side wins more often. The test switch is `RECOMP_TIER_A=1`. The program reads it once at the start, and it is off by default. The same-save set proves that the switch changes nothing when it is off.

A Tier A year takes 4 to 6 minutes with all nations on the M5 Max at load 6 to 12, about 7 times faster than the normal game at the same load. The time does not grow from 2013 to 2066. The save grows by about 9 MB a year.

The script is `tests/seasons/soak_run.py` with `--env RECOMP_TIER_A=1`, and `--checkpoints 10` to keep one save per decade. Tier A counts as the same as the normal game when both play 10 years from the same start, database and seed, and each July save agrees on editions, dates, hosts and entrant rules. The script is `tests/seasons/future/tier_a_compare.py`. Winners can differ. The history checker then reads each checkpoint save.

## Memory and big saves

A game with all nations uses more memory each season. The heap now has room for about 2 GB of game data. A code test (`tests/recomp/test_heap.c`) fills a small heap and checks that it still serves new blocks.

The browser game has less memory room than the native one: its own data sits at guest 0x21000000 to 0x60000000, and its memory ends 64 MB past the heap's top. `RECOMP_GUARD=1` makes those addresses fault in the native game too (`src/recomp_rt/rt.c`), so a stray pointer that would break only the browser game stops the native run where it happens. The data sweep with all nations runs with it (`RECOMP_GUARD=1 python3 tests/seasons/data_sweep_run.py … --nations all`), once from the game's own data and once from a recent data update.

The guard only finds what a run reaches. Exit and Restart Game reached a stray read that no sweep did (November 2026, browser error reports): the clean-up 0x5cc7d0 calls free_game_data 0x82ff40, which at 0x830398 destroys the table of 0x167-byte records at [0xae2a38] through 0x59a5a0 and the array destructor loop 0x9462bb, with the empty function 0x5a8f60 as each record's destructor. The runtime wraps 0x5a8f60 (`season_uefa_cup.c`), and its class test read a record's first word, a sum of money, as a vtable pointer. Pointer tests in the runtime now use `rt_readable` (`rt.h`), which stops at the heap's top, so both builds answer alike. A new game of 2025/26 had 34 such records of 5,326. A guarded run that calls the clean-up after a new game (gdb at the first news screen: `call f_005cc7d0()`) faulted before the change and passes after it.

The same day a screen sweep under the guard with the October 2026 data (2025/26, England, France, Germany, Italy and Holland) found one more: the Friendly competition, which is no cup, opens with the cup rounds tab 0x492980, and that tab read its bytes at +0x45 and +0xa3 as a cup's draw and round table and followed a pointer past the browser's memory (0x492bb0..0x492bbd). `season_round_names_game.c` (cup_view) now lets the rounds and schedule tabs show the original's own "The draw has not yet been made." when the object has no round table. The sweep also left out two placeholders that the game never lists (FraCfaStubComp and FraLowerStubComp, `screen_sweep.c`). With both, the sweep passes the whole season, 280 screens. The Game Options commands (Save Game, Add Manager, Manager Status, Game Settings with every setting and a language change, Bosman Edition Options, Selected Leagues, Hall of Fame, Game Credits, Web Sites, Restart Game, Exit Game) and the manager's Resign and Retire ran under the guard from a loaded game with no fault.

Saves can now be up to 4 GB. A save that would be larger stops with a clear message and keeps the earlier save. `tests/smoke/big_save_run.py` makes a save above 3 GB and loads it again, checks a season part above 2 GB, and checks the clean stop above 4 GB. It needs about 12 GB of free disk for a short time. These checks run at the merge of a memory or save change and in the long games at each release.

A save holds at most 999 parts. The game keeps a temporary file for each season, so the temporary files of one type go into one merged part. `tests/seasons/save_parts_run.py fill` adds temporary files until a save holds 990 and 999 parts, then saves and loads twice. Each save must hold at most 200 parts and every added file. `tests/seasons/save_parts_run.py old` loads a save of 0.6.0, 0.6.1 or an earlier 0.6.2 build, saves and loads twice, and checks that every temporary file of the old save is still there. These checks run at the merge of a save change.

Every game keeps the club records history and the fixtures of the last 50 seasons (open item 192), so a game with all nations stays below 4 GB to 2300. `tests/seasons/history_trim_run.py` loads a Tier A save of July 2061 and plays one season. Each club's list must then hold at most 50 seasons in order, the history file may grow only for clubs with fewer than 50, the Club History list of every club must build, and the save must hold the fixtures of the last 50 years and no older ones. The check runs at the merge of a change to these files. The same-save run `history-keep-all` turns the trim off.

## Screen sweeps and smoke screenshots

The screen sweep opens the tables, results, fixtures, schedule and history of every competition, once each season. The data sweep reads each competition from memory at the start and end of each season. It looks for dates outside the season and for empty tables where the rules give clubs. The scripts are `tests/seasons/screen_sweep_run.py` and `tests/seasons/data_sweep_run.py`. They run at each release.

The smoke test plays a fixed set of clicks through the main screens in the classic and widescreen modes. It compares each screenshot with a reference picture. The script is `tests/smoke/smoke_run.py --mode classic` or `--mode widescreen`. It runs at each merge and at each release. The pass rule is no error box and pictures that match. When a change draws something new on purpose, we look at the picture and record a new reference.

## Online play

Network games (0.7.0) have their own tests. The two-game run (`tests/smoke/network_run.py`) starts a host game and joining games on this Mac with timed clicks, and compares the joiners' screens with the host's. It runs at each change of the network code, also under wine with the Windows build. The server tests in `server/` run against a local copy of the lobby and relay.

Every coordinate on the network is a classic one (item 212), so a host and a joiner may use different modes. `tests/smoke/network_display_run.py` plays the two-game run four times, with the host and the joiner each in Classic mode or Widescreen mode. When both use the same mode, the joiner's screens must equal the host's. When they differ, the joiner's screens must equal those of the run whose host used the joiner's mode. `tests/smoke/network_drag_run.py` plays four games of its own in the same four pairs of modes, to the first news. The joiner drags a player on its tactics screen. The player must move to the place of the drop, and the pitch after the drag must be the same under a host in either mode. Both run at each change of the network or display code.

The games of a network run meet on this Mac's LAN address. When the network drops, the system removes that address and both games lose their connection. The scripts check every half second that the address is still there, and a run in which it went away fails with "the test address went away during the run", so it is not taken for a fault of the game. ctest `address_watch` tests the check.

Before the release, four release tests run (spec Q29). Their scripts are in `tests/online/`. Each script waits for room on the machine, and for any network run of another worktree to end, because two network runs on one Mac collide on port 6667. Each one adds a line to `run-logs/s9-tests/results.tsv`. While several sessions test network play, each run that hosts a network game goes through `the maintainer's notes`, which lets one host at a time use port 6667.

Test 3 starts its workflow with a push of the branch `online-ci-test` (`github_join_run.py --push`), because GitHub starts a manual workflow only from the default branch, and `main` belongs to the 0.6.0 work. It needs no secret: the Mac puts the AppImage and a zip of the Windows program with its game folder on a draft release, which only people with write access see, and deletes it after the run.

| Test | Script | Pass rule |
|---|---|---|
| 1. Two and four games on this Mac through the real lobby and relay | `release_mac_run.py` | Each two-game run with 1 and 3 joiners through the relay passes. Without the relay in the build the script runs the lobby and LAN runs and reports STAGE. |
| 2. Dropped (Q45): the Windows 11 virtual machine joins a game on this Mac | `vm_join_run.py` (kept, not run) | This Mac's Parallels Desktop edition refuses `prlctl resume`, `start` and `exec`, which need the Pro or Business Edition. Test 3's GitHub Windows machine covers a real Windows machine. |
| 3. GitHub's Windows and Linux machines join a game on this Mac | `github_join_run.py --push`, `remote_join.py`, `.github/workflows/online-release-join.yml` | Both joiners pass their checks and their screens equal the host's within 2 colour levels. |
| 4. A four-player game through a World Cup, a Euros and every season change between them | `season_net_run.py` | The host plays a network game from 2001 to 1 September 2004 with the match engine on, one season a leg. Each leg restores the last save, and three joiners take their clubs back with their passwords. Leg 0 also has a drop and rejoin and a game of another version. The last leg ends with a host quit. No box, no stall, every division moves on. |

`tests/online/match_day_run.py` measures the traffic of a match day with two human managers. It passes when both games ran, the joiner made its manager and the host played a match. Its figures are in the network research note, section 16.

Results:

| Date | Test | Build | Result |
|---|---|---|---|
| 2026-10-27 | 4, short check: 1 leg of 3 days, England, LAN, local lobby (`--days 3 --legs 1`) | online-0.7.0 8b7b21bd | Leg 0 passed every check: four human managers with passwords, join3's drop (lobby 3 online) and rejoin with its password (4 online), the box of a game of version 0.0.1 that names the host's version, the host's holiday and its save after 3 game days. The restore leg then restored the save and Player Two took Barnsley back with the password. The quit leg waits for slice 6. |
| 2026-10-29 | 2 | | Dropped (Q45): `prlctl resume` answered "The command is available only in Parallels Desktop for Mac Pro or Business Edition". |
| 2026-10-27 | 3 | | Not run: the workflow is not on GitHub (a push waits for Max), and the build has no relay. |

| 2026-10-28 | Match day, two human managers, LAN | online-0.7.0 8b7b21bd | Measured days 16.7.01 to 19.7.01: 72 KB a minute to the joiner, one 6.2 KB screen for each of its clicks (research note section 16). FAIL as a match day: the joiner's own friendly did not start, and the host waited on Latest Scores. Open. |

| 2026-10-28 | 1, stage before slice 5 | online-0.7.0 8b7b21bd | STAGE: the lobby run (local copy) and the four-game LAN run pass. Against the real server the lobby run failed: the game's lobby client of this build speaks only `http://`, so the script now uses the local copy until the build has slice 5. |
| 2026-10-28 | 4, short check: 2 legs of 10 days, England, LAN, local lobby | online-0.7.0 8b7b21bd | STAGE: every check passes in both legs (the new game with the drop by kill, the rejoin and the version box; then the restore, where all three joiners took their clubs back with their passwords). The quit leg waits for slice 6, the relay for slice 5. |

| 2026-10-29 | 3, GitHub's Linux and Windows machines | online-0.7.0 7d60ee73 with slice 9 (05e55d19) | Both joined the Mac's game `00S9-CI` through the live lobby and relay (https://online.cm0102bosman.co.uk): each ended with code 0, joined through the relay, sent its 4 clicks, and its three screens equal the host's within 2 colour levels (the Mac's picture decode rounds otherwise; Windows and Linux are equal). The packages came from a draft release, deleted after the run; no secret. The run reported FAIL on two faults of its own checks (a tolerance of 0, and two joiners on one relay address counted once); both are fixed, and the same data passes. Run 40927613267. |

| 2026-10-29 | 4, the full run: 2001 to 4 September 2004, England, 10 legs of 120 days and a quit leg | online-0.7.0 6f12558e with slice 9 | PASS. Legs 0 and 1 over the LAN with a local lobby (leg 1 on slice 6's build with the send fix, after leg 1 on the earlier build found the "scrman 2480" box), legs 2 to 10 through the relay and the live lobby. Each leg: the three joiners took their clubs back with their passwords; every division moved on; the run crossed the World Cup 2002, the Euro 2004 and the season changes of 2002, 2003 and 2004. Quit leg: the host saved at its quit, and each joiner showed "The host left the game", then the start screen. Two test faults fixed on the way: Go on Holiday sits higher in the menu of a manager with no club (the board sacked the host in 2004), and the first quit leg's host stayed on Manager Status after Finish for 10 minutes with two joiners "Waiting" (not seen again in the second try; open). |

| 2026-10-30 | 4, the quit leg 10 times through the live relay | online-0.7.0 877cac08 (slice 5's relay close, slice 6's hold of the other joiners' packets) | 10 of 10 PASS: no hang, the host saved at its quit, and every joiner showed "The host left the game", then the start screen. On slice 6's build before these fixes, 1 of 5 passed (2 with "The connection to the host was lost", 2 with the hang after Finish). |

<!-- s9 results -->

## Platform checks

The game ships for Mac, Windows and Linux. At each release the release script builds all three packages. The Windows build runs its smoke test under wine on the Mac. The Linux build is made and checked in a linux/amd64 container. Then `tools/linux/arch_check.sh` runs the AppImage on a current Arch image with Mesa, under X11 and Wayland, with a graphics renderer (the other Linux checks use the SDL dummy drivers). Each platform runs the smoke test in both screen modes and a season change with a save and load.

## Speed benchmark

The speed benchmark measures two numbers for one build on one machine. Every speed change (issue 76) compares its numbers with the numbers of the build before it.

- **New game:** a game of 2001/02 with all nations, from Start New Game to the first human turn. The number is the sum of the progress steps of the game (`RECOMP_PROGRESS_LOG`), in seconds.
- **Holiday:** four weeks of holiday with the match engine on, from a fixed full-world save of 1 September 2001. The number is the time between the first and the fifth game date line of the screen log. The load of the save and the write of the next save are not in it.

The script is `tests/seasons/speed_bench.py`. Each part plays three games one after the other. A part passes when every game ends with code 0 and no error box, and the longest game is less than 5 percent above the shortest. A part that fails the spread gives no number. Repeat it when the machine is quiet. Every game adds a line to `bench.tsv` in the keep folder, with the time, the platform, the build label and the 1-minute load at the start and end. The script starts no game while the load is 20 or more. ctest `speed_bench` tests the script without a game.

The fixed save comes from `--make-save`. It plays a new game with all nations, goes on holiday and saves after 60 game days. Make it once with the build of the first "before" numbers, keep it with its SHA-256, and play every later build on that same file. A faster build may play the same days differently, so a save from a later build is another save.

Steps on the Mac:

1. Build the commit to measure. Use `build-slot.sh` and `-j4`. Close other test games if you can: a load above 10 makes the spread rule fail.
2. Make the fixed save once: `python3 tests/seasons/speed_bench.py build/cm0102_recomp <clean game folder> --keep <run-logs/0.8.0-speed/save> --make-save --label <commit>`
3. Run both parts: `python3 tests/seasons/speed_bench.py build/cm0102_recomp <clean game folder> --keep <run-logs/0.8.0-speed/<commit>> --save <fixed save> --label <commit>`
4. Copy the two medians into the table below with the commit, the date and the load.

Steps on Windows (not run yet, to do when a Windows machine is free; the spec asks for a spot check, not for every change):

1. Use a real Windows 10 or 11 machine with Python 3.10 or later. A wine run on the Mac measures wine, so it gives no Windows number.
2. Unpack the release zip of the Windows build and the clean game files into one folder. The program is `cm0102_recomp.exe`.
3. Copy `tests/`, the fixed save and `seasons/rules.tsv` from the zip to the machine. Keep the folder layout of the repo, because the script reads `tests/seasons`, `tests/smoke` and `tests/fixtures`.
4. Run the same two commands as on the Mac with the `.exe` path and a keep folder on a local disk. The script has no load check on Windows. Close other programs and stop the virus scanner for the folder, and write that in the label.
5. The fixed save is the Mac file. If the Windows game refuses it, make a Windows save with `--make-save` and keep it as the Windows fixed save with its SHA-256.

Steps on Linux:

1. Use a real Linux machine, not the amd64 container on the Mac, which runs under emulation. Give the machine's CPU model in the label.
2. Build with `tools/linux/make_appimage.sh` on that machine or unpack the AppImage with `--appimage-extract`. The program is `AppRun` or `usr/bin/cm0102_recomp`.
3. Set `SDL_VIDEODRIVER=dummy` and `SDL_AUDIODRIVER=dummy`. The script sets both, so no display is needed.
4. Run the same two commands as on the Mac. The script checks the load with `os.getloadavg()`, which Linux has.

Numbers (seconds, median of three):

| Date | Platform and machine | Commit | Load | New game | Holiday | Note |
|---|---|---|---|---|---|---|
| 2026-11-13 | Mac, M5 Max | 3c6af155 (trial, one run each) | 18 to 29 (new game), 18 to 20 (holiday) | 709 | 432 | Trial under the 0.7.2 test games. Not a before number. |

## Rules source checks

The season rules come from a table with one row per competition and season. `tools/research/rules_lint.py` checks that every filled fact has a source. `tests/seasons/rules_report_check.py` compares the competitions that the game holds each season with that table. `tools/release/rule_changes_report.py` writes the list of rule changes for players, and marks each real club count that the league code of the game cannot hold. `tests/seasons/rule_changes_report_check.py` tests it. All three run at each change to the rules and at each release. The pass rule is no missing source, no difference and a passing test.

## Database checks

The duplicate check, `tests/seasons/history_duplicates_check.py`, lists the history rows that a season database holds twice. The game clears the extra copies when it loads the database. `tests/seasons/season_history_run.py --duplicates` checks that no copy is left in the game.

Some history rows of the season databases are one year off against the other rows of their competition (open item 233: the Norwegian league before 2008, the Irish league before 2003, the Swedish Cup 2002 to 2007 and others). `tools/seasons/history_years_check.py` lists them with a reference each in `docs/research/history-years/wrong-rows.tsv`, and the game corrects them at the database load (`ctest -R season_history_years_table` checks the C table). `tests/seasons/history_years_run.py` starts the October 2018, December 2025 and October 2026 databases and checks rows of the Norwegian league, the Swedish Cup, the Irish league and the World Cup.

Other rows hold a wrong club or are missing (issues 145 and 165). `docs/research/history-rows/` gives each corrected row with its source, and every season game applies them at the database load (`ctest -R season_history_rows_table` checks the C table). `tests/seasons/history_rows_run.py` starts four season databases, a 2001 game with changing rules on and the base game, and reads the rows of each save. The base game must hold none of the added rows.

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
