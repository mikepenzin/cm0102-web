# CM 01/02 in the browser

The real recompiled game, built as WebAssembly, running in a browser tab. A small Node app hands the browser the
engine and the game files; the browser does all the work. There is no mock: it is the same translated code as the
desktop downloads, with a browser host under it. To put it on the internet, see `web/deploy/README.md`.

```
Chrome tab: web/loader.js ─ downloads ─▶ the game files into the engine's file system (/game)
            the engine (cm0102_recomp.wasm) runs main(--game /game) and draws on the canvas
            saves and settings.txt stay in the browser (/user, IndexedDB)
web/server.mjs: serves the page, the engine and the clean files, nothing else
```

## Run it on a Mac, step by step

You need the CM 01/02 Bosman Edition app in Applications (its game folder holds the clean files), Homebrew, and
Chrome 137 or later.

**1. The tools (once).**

```bash
xcode-select --install
brew install cmake ninja node python
python3 -m venv ~/.venvs/cm0102 && ~/.venvs/cm0102/bin/pip install capstone
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest
```

The build finds the Python of `~/.venvs/cm0102` by itself, so `capstone` is there for it.

**2. The code (once; later `git pull` in the folder).**

```bash
git clone https://github.com/mikepenzin/cm0102-web.git ~/Projects/cm0102-web
cd ~/Projects/cm0102-web
git checkout claude/engine-worker
```

**3. The game files (once).** Copy the game folder out of the app:

```bash
rm -rf ~/cm0102-game
cp -R "/Applications/CM 01-02 Bosman Edition.app/Contents/Resources/game" ~/cm0102-game
ls ~/cm0102-game          # cm0102.exe, Data, Pictures, Sounds, History, ...
```

The `rm -rf` matters: if `~/cm0102-game` is already there, `cp -R` puts the folder inside it, one level too deep.

**4. Build the engine (once, and again when the engine's code changes).**

```bash
cd ~/Projects/cm0102-web
source ~/emsdk/emsdk_env.sh
emcmake cmake -S . -B build-web -G Ninja -DCM0102_EXE=$HOME/cm0102-game/cm0102.exe
cmake --build build-web --target cm0102_recomp seasons_rules
ls build-web/cm0102_recomp.wasm
```

If the first command prints `cm0102_recomp: … not found, so the recompiled game is not built`, the path to
`cm0102.exe` is wrong: check step 3. The first build translates the 11,706 functions of `cm0102.exe` to C and
compiles them, so it takes a while.

**5. Run it.** In a new terminal tab (so that your own Node runs it, not Emscripten's):

```bash
cd ~/Projects/cm0102-web
CM_GAME_DIR=$HOME/cm0102-game node web/server.mjs
```

It prints `game files: 764 (461 MB)`, `every SHA-256 matches the clean files`, the engine, and `open
http://127.0.0.1:8080 in Chrome`. Open that address in Chrome and press Start the game. The first start loads about
460 MB of game files into the browser (seconds from your own computer; later starts read them from the browser), the
browser compiles the engine, and the game starts as on the desktop: the settings screen of a first start, then Setup
Game.

## Other systems

The same five steps, with these differences:

- Linux, or Windows through WSL (Ubuntu): in step 1, `sudo apt install -y git cmake ninja-build python3 python3-venv
  curl`, then Node 22 (for example with nvm), then the venv and emsdk lines. On Windows, run `wsl --install` first and
  do everything in the Ubuntu app; Chrome on Windows opens `http://127.0.0.1:8080` of WSL.
- The game files: the folder `game` of an installed Bosman Edition on Windows (in WSL under `/mnt/c/...`), or from the
  Linux AppImage (`--appimage-extract`, then the folder that holds `cm0102.exe`). Or make them from the free release
  and the official patch: `python3 tools/clean_files/make_clean_files.py CM0102.iso cm0102_3968.exe ~/cm0102-game`
  (needs `bsdtar` and `unshield`).

## Settings of the server

| Variable | Default | |
|---|---|---|
| `CM_GAME_DIR` | none | the folder with `cm0102.exe` and `Data` (step 3) |
| `CM_WEB_BUILD` | `build-web` | the build folder (step 4) |
| `PORT`, `HOST` | 8080, 127.0.0.1 | `HOST=0.0.0.0` lets other devices of your network open it |
| `CM_WEB_SKIP_VERIFY` | unset | `1` skips the SHA-256 check at the start (the size check stays) |

The server checks every game file against `tools/clean_files/manifest.tsv`, size and SHA-256, when it starts, and
stops if one differs.

## If it does not start

| What you see | What to do |
|---|---|
| `CM_GAME_DIR: … does not exist` or `… has no cm0102.exe` | Step 3: the folder is missing, or the files are one level deeper (the message names that folder) |
| `cm0102_recomp: … not found` while configuring | Step 4: `-DCM0102_EXE` must name `cm0102.exe` in your game folder |
| `engine: not built` | Step 4 did not finish: run the build again and send its last 30 lines |
| A box "This browser cannot run the game" | It names the reason; use Chrome 137 or later on a computer |
| The game stops in the tab | Open "Engine log" under the game and send its last lines |

"Engine log" holds every line the runtime prints (`recomp: ...`) and the page's own lines (`page: ...`).

First run: 9 November 2026, on a MacBook Air in Chrome: the database load, a new game, and days to 12.8.01 with the
cup and Champions League rounds. What to expect:

- The runtime prints `recomp: web memory ... MB, host data at 0x21..., host heap at 0x2...` at the start.
- The tab freezes while the game works for a long time without asking for input (the database load of a big new
  game). The page draws and takes input whenever the game polls for input.
- Sound starts after the first click in the game.

## Saved games

The game saves into the user folder as on the desktop (Game Options, Save Game). The Saved games button above the
game lists those saves: Download writes one to your computer, as a normal `.sav` file; Upload a save puts a `.sav`
file into the folder, and the game's Load Game screen then lists it. An upload is checked first (it must be a save
container of CM 01/02), and a name that is taken gets a number. The saves use the format of the desktop game, so a
save should move between this page and the Bosman Edition both ways.

The saves stay in IndexedDB, not localStorage: localStorage holds a few MB of text per site, and a save holds the
whole game world. A new or changed save goes to IndexedDB about 2 seconds after the game finished writing it. Use
the same address every time: `127.0.0.1:8080` and `localhost:8080` are two sites to the browser, each with its own
saves.

## Browser requirements

The page checks the browser before it downloads anything, and stops with the reasons when the game cannot run:
no WebAssembly; no JSPI for a JSPI build; or no room for the engine's memory (the page tries the engine's own plan:
768 MB, grown to 1,856 MB, with a 4 GB maximum). After Start it compiles the engine once, before the game files
download, so a browser that lacks a WebAssembly feature of the engine stops there. A device with no mouse, or a
browser that keeps no data (a private window), gets a warning but may start.

## Other browsers

`-DCM0102_WEB_SUSPEND=asyncify` in step 4 builds without JSPI, for every browser (Safari, every iPad browser). The
engine is then larger and slower, and the build takes longer. Use another build folder for it and point the server at
it with `CM_WEB_BUILD`.

## How it works

- Memory (`src/recomp_rt/host/web.c`): WebAssembly has one memory of at most 4 GB, so a guest address is a memory
  address. The browser build's own data starts at 0x21000000, in the gap that the game never uses, and the memory
  grows as the game's heap grows. Every block of the game sits where it sits on the desktop.
- Time: the game's screen loop never returns. With JSPI the runtime's sleeps hand the time to the page: the idle
  sleep, and a short one at least every 12 ms while the game polls for input (`rt_web_yield`).
- Files: the page writes the game files into Emscripten's file system before `main` starts; the runtime's POSIX host
  (`host/posix.c`) reads them as on Linux. The page keeps the game files in the browser's cache storage, each under
  its version (its SHA-256 in the manifest), so a later start downloads nothing. The user folder `/user` is kept in
  IndexedDB (a save at once, everything else every 15 seconds and when the tab is hidden or closed), and the page
  asks the browser to keep its storage.
- SDL2 is Emscripten's port: it draws on the canvas, takes the mouse and keys, and plays the sound.

## Tests

- `cd web && npm test`: the server lists and serves only the clean files and the season tables, and refuses the rest;
  brotli and versioned caching; the stage script and the staged server; the save logic.
- `NODE_PATH=$(npm root -g) node web/test/page-check.cjs`: the page at full size (placeholder files with the sizes of
  the manifest, 460 MB) and an Emscripten stand-in in place of the engine (`test/fake-emscripten.js`): the browser
  check, the load, the Saved games panel and a reload from the browser's cache. Needs Playwright.

## Not yet

- Only the base game: the season databases (`Seasons/*.zip`) and the extra data sets are not loaded.
- The engine runs on the page's main thread. The concept design moves it to a Web Worker, with the new screens
  beside it.
