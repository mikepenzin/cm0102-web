---
name: bosman-update
description: Brings a new Bosman Edition release (the original repo, maxevans60/cm0102-bosman-edition) into this browser fork, keeping the browser's changes. Use for "pull Bosman changes", "update from upstream", "take 0.7.4", "a new Bosman release", "sync with the original", "the fork is behind", or when the game code here is older than the original's latest release.
---

# Taking a Bosman Edition release

This repo is a fork: the game code is the original's (src/, tools/, tests/, docs/research), with the browser's
changes on top; web/, docs/web-port, docs/superpowers and the web skills are this fork's alone. The histories do not
merge (the fork once copied the original's work in), so an update applies the original's changes since the commit
this fork carries: `docs/web-port/bosman-base.txt`. 0.6.0 → 0.7.3 was done this way on 23 November 2026.

## Steps

1. `git remote add upstream https://github.com/maxevans60/cm0102-bosman-edition` (once), then
   `git fetch --depth=3000 upstream main`. Work on a branch; main moves only when every check passes.
2. `B=<commit in bosman-base.txt>`, `U=<the release commit>`. Files this fork deleted that the original changed stop
   `git apply`: list them with `git diff --name-only $B $U` against `git ls-files`, leave them out, restore after.
   `git diff --binary $B $U -- . ':!<left out>' > up.patch && git apply --3way --index up.patch`.
3. Resolve the conflicts (`git diff --name-only --diff-filter=U`). The rule: the original's game fixes win; the
   browser's pieces stay. The browser's pieces in the game code:
   - `user32.c`: `web_feed`, `win_play_poll`, the `in_wait` guard around waits, the breathe hooks (`win_sort_step`:
     random 0x90cfc0, qsort's swap 0x9454d5, the comparators 0x7c6d90/0x7c6de0, get_contract 0x4cf5a0, fread
     0x945bfe, fwrite 0x945ce6). qsort 0x945333 and is_human_club 0x600a50 belong to the original's host copies
     (crt.c, hot_loops.c), which call `win_sort_step()`: one RT_WRAP per address, so check for doubles:
     `grep -rhoE "RT_(WRAP|REPLACE)\(0*[0-9a-f]+" src/recomp_rt | sed -E 's/RT_(WRAP|REPLACE)\(//' | sort | uniq -d`.
   - `display.c`: the stage timing of the page's progress bar (`progress_note`, `rt_web_progress`).
   - `play.c`, `keys.c`, `host/web.c`, `rt.c` (the guard, `rt_web_*`), `heap.c` (`rt_heap_top`), `host/posix.c`
     (`host_sync`; `host_rename_new` has an Emscripten branch: no hard links), `language_sort.c`.
   - `savename.c`: the fork keeps the loaded save's name and suggests "Club/Nation YYYY-MM-DD" only for a game's
     first save (a browser held 99 dated saves, 3.4 GB); `tests/smoke/save_name_run.py` stays the fork's.
   - `CMakeLists.txt`: the EMSCRIPTEN host line (`posix.c web.c net_posix.c`), mbedTLS fetched for every build,
     the test stubs `tests/recomp/heap_top_stub.c` and `heap_guard_stub.c` on unit tests that link runtime files
     alone.
   - `tests/smoke/smoke_run.py`: the `rules-on-again` run with the same settings as `rules-on` (`q48_runs`).
   - `.gitignore`: this repo keeps `.claude/` (the original ignores it).
4. `python3 tools/web/rt_readable_sweep.py`: the original's new files test pointers with the old bound.
5. Checks, all before main moves:
   - native: `cmake -S . -B <dir> -G Ninja -DCM0102_EXE=<game>/cm0102.exe` and build `cm0102_recomp`; build the
     original's release the same way from a worktree; `tests/smoke/smoke_run.py <ours> <game> --same-save --days 3
     --ref-build <theirs>`: `ref-build: the save is the same as normal` proves the browser's changes change no game
     decision (skill same-save-check). `ninja -k 0` then `ctest`: on Linux the hand port (src/si, CommonCrypto) does
     not build, and net_core, net_errors, future_run_set and dmg_background fail in the original too.
   - browser: both engines (`emcmake cmake … ` default JSPI, and `-DCM0102_WEB_SUSPEND=asyncify`); load a save of
     the live site and play days, and play a new game to its first news, in Chromium with the real engine; then the
     web checks (skill web-checks).
6. Update `docs/web-port/bosman-base.txt` to the release commit, commit, push. The deploy (web/deploy/README.md)
   rebuilds `build-web` and `build-web-asyncify` on the Mac (CMake reconfigures itself) and stages.
