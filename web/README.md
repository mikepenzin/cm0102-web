# CM 01/02 in the browser

The real recompiled game, built as WebAssembly, running in a browser tab. A small Node app on your computer hands the
browser the engine and your game files; the browser does all the work. There is no mock: it is the same translated
code as the desktop downloads, with a browser host under it.

```
Chrome tab: web/loader.js ─ downloads ─▶ the game files into the engine's file system (/game)
            the engine (cm0102_recomp.wasm) runs main(--game /game) and draws on the canvas
            saves and settings.txt stay in the browser (/user, IndexedDB)
web/server.mjs: serves the page, the engine and the clean files, nothing else
```

## What you need

- The clean files: `cm0102.exe` 3.9.68 and its `Data`, `Pictures`, `Sounds` and `History` folders (step 1).
- [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) (the latest emsdk), CMake 3.20 or later,
  Python 3 with `capstone` (`python3 -m pip install capstone`), and Node 22.
- Chrome 137 or later. The build uses JSPI, which Chrome has; for another browser see "Other browsers" below.

## Step 1: the clean files

Either of these gives a folder that holds `cm0102.exe` and `Data`:

- From the free release and the official patch, as the main README says:
  `python3 tools/clean_files/make_clean_files.py CM0102.iso cm0102_3968.exe ../clean-3968`
  (needs `bsdtar` and `unshield`).
- From a Bosman Edition download: the folder `game` in it (on a Mac inside the app, `Contents/Resources/game`; on
  Windows the folder `game` of the installed game; from the Linux AppImage, run it with `--appimage-extract` and use
  the folder that holds `cm0102.exe`).

The server checks every file against `tools/clean_files/manifest.tsv`, size and SHA-256, when it starts, and stops if
one differs.

## Step 2: the browser build

From the repo, in a shell where `emcc` works (after `source <emsdk>/emsdk_env.sh`, or `emsdk_env.bat` on Windows):

```bash
emcmake cmake -S . -B build-web -DCM0102_EXE=../clean-3968/cm0102.exe -DRECOMP_PYTHON=python3
cmake --build build-web -j 8 --target cm0102_recomp seasons_rules
```

The first build translates the 11,706 functions of `cm0102.exe` to C and compiles them; it takes a while. It writes
`build-web/cm0102_recomp.js`, `build-web/cm0102_recomp.wasm` and the season tables in `build-web/seasons`. On Windows,
add `-G Ninja` to the first command if CMake finds no make tool.

## Step 3: run it

```bash
CM_GAME_DIR=../clean-3968 node web/server.mjs        # PowerShell: $env:CM_GAME_DIR="..\clean-3968"; node web/server.mjs
```

Open http://127.0.0.1:8080 in Chrome and press Start the game. The page loads about 460 MB of game files into the tab
(seconds from your own computer; later starts read them from the browser's cache), the browser compiles the engine,
and the game starts as on the desktop: the settings screen of a first start, then Setup Game.

| Variable | Default | |
|---|---|---|
| `CM_GAME_DIR` | none | the clean files of step 1 |
| `CM_WEB_BUILD` | `build-web` | the folder of step 2 |
| `PORT`, `HOST` | 8080, 127.0.0.1 | |
| `CM_WEB_SKIP_VERIFY` | unset | `1` skips the SHA-256 check at the start (the size check stays) |

## If it does not start

Open "Engine log" under the game: it holds every line the runtime prints (`recomp: ...`), and the page writes its
own lines there too (`page: ...`). A failed start shows a box with the last lines. Send those, or the build error, back.

First run: 9 November 2026, on a MacBook Air in Chrome: the database load, a new game, and days to 12.8.01 with
the cup and Champions League rounds. What to expect:

- The runtime prints `recomp: web memory ... MB, host data at 0x21..., host heap at 0x2...` at the start. If it says
  the build's data is not above 0x21000000, the link options of `web/web.cmake` did not apply.
- The tab freezes while the game works for a long time without asking for input (the database load of a big new
  game). The page draws and takes input whenever the game polls for input.
- Sound starts after the first click in the game.

## Browser requirements

The page checks the browser before it downloads anything, and stops with the reasons when the game cannot run:
no WebAssembly; no JSPI for a JSPI build; or no room for the engine's memory (the page tries the engine's own plan:
768 MB, grown to 1,856 MB, with a 4 GB maximum). After Start it compiles the engine once, before the game files
download, so a browser that lacks a WebAssembly feature of the engine stops there. A device with no mouse, or a
browser that keeps no data (a private window), gets a warning but may start.

## Other browsers

`-DCM0102_WEB_SUSPEND=asyncify` in step 2 builds without JSPI, for every browser. The engine is then larger and slower,
and the build takes longer.

## How it works

- Memory (`src/recomp_rt/host/web.c`): WebAssembly has one memory of at most 4 GB, so a guest address is a memory
  address. The browser build's own data starts at 0x21000000, in the gap that the game never uses, and the memory
  grows as the game's heap grows. Every block of the game sits where it sits on the desktop.
- Time: the game's screen loop never returns. With JSPI the runtime's sleeps hand the time to the page: the idle
  sleep, and a short one at least every 12 ms while the game polls for input (`rt_web_yield`).
- Files: the page writes the game files into Emscripten's file system before `main` starts; the runtime's POSIX host
  (`host/posix.c`) reads them as on Linux. The page keeps the game files in the browser's cache storage, each under
  its version (its SHA-256 in the manifest), so a later start downloads nothing. The user folder `/user` is kept in
  IndexedDB every 15 seconds and when the tab is hidden or closed, and the page asks the browser to keep its storage.
- SDL2 is Emscripten's port: it draws on the canvas, takes the mouse and keys, and plays the sound.

## Hosting

`web/deploy/README.md`: stage the files and put them on Fly.io.

## Tests

- `cd web && npm test`: the server lists and serves only the clean files and the season tables, and refuses the rest;
  brotli and versioned caching; the stage script and the staged server.
- `NODE_PATH=$(npm root -g) node web/test/page-check.cjs`: the page at full size (placeholder files with the sizes of
  the manifest, 460 MB) and an Emscripten stand-in in place of the engine (`test/fake-emscripten.js`). Needs Playwright.

## Not yet

- Only the base game: the season databases (`Seasons/*.zip`) and the extra data sets are not loaded.
- The engine runs on the page's main thread. The concept design moves it to a Web Worker, with the new screens
  beside it.
