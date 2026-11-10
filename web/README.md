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

A later build, after `git pull` changed the engine's code, is one command; the build folder remembers the compiler:

```bash
cd ~/Projects/cm0102-web
cmake --build build-web --target cm0102_recomp seasons_rules
```

If `source ~/emsdk/emsdk_env.sh` says the file is not found, emsdk is in another folder. The build folder knows
which: `grep CMAKE_TOOLCHAIN_FILE build-web/CMakeCache.txt` prints a path like
`<folder>/emsdk/upstream/emscripten/cmake/...`; the file is `<folder>/emsdk/emsdk_env.sh`. A path with `homebrew` in
it means Emscripten came from Homebrew, which needs no `source` line.

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
| `source: no such file or directory: …/emsdk_env.sh` | emsdk is in another folder: see the end of step 4 |
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

## Cloud saves

Players can sign in with Google. An account with cloud saves keeps its saved games in the cloud as well as in the
browser: a save made on one computer or iPad is there when the game starts on another. Without an account, or on an
account without cloud saves, nothing changes: the saves stay in the browser, and Saved games downloads and uploads
them as files.

**How it works.** The server (`cloud.mjs`) runs the Google sign-in (the authorization code flow, with PKCE) and keeps a
signed cookie; no script of Google's runs on the page. The saves go to an S3 store (Tigris on Fly.io, `s3.mjs`),
gzip-compressed, under the account's Google id. The page (`account.js`):

- Before the game starts, the saves made elsewhere come into the user folder, so Load Game lists them. A slow
  connection gets a "Start without them" button after 5 seconds.
- After each save, the new or changed save goes up ("Saved to the cloud"). With no connection it goes up later.
- A save changed on two devices since they last synced is kept twice, nothing is lost: the other device's version as
  "<name> (other device).sav", in both places.
- While the game runs, a save in the browser is never replaced from the cloud; only new ones come in.
- Saved games shows where each save is, and Remove takes one out of the cloud (it stays in the browser).
- Account has Sign out and Delete account (the account and all its cloud saves). `/privacy` says what is kept.

**Who has cloud saves.** The accounts in `CM_CLOUD_USERS`, every account with `CM_CLOUD_FOR=everyone`, and the
accounts whose record in the store (`accounts/<id>.json`) says `"plan": "cloud"` (with `planUntil`, until then). A
payment can set that record; until there is one, edit it by hand in the Tigris console. Others see "Cloud saves are
not on for this account", and a "Get cloud saves" link when `CM_UPGRADE_URL` is set.

**Set it up** (once). It is off until all of these are set; the server says at the start which ones are missing.

1. Google: in [console.cloud.google.com](https://console.cloud.google.com), make a project. Google Auth Platform,
   Branding: the app name, a support email, and the privacy page `https://<your-app>.fly.dev/privacy`. Audience:
   External, then Publish app (while it is "Testing", only the test users you list can sign in). Clients, Create
   client: Web application, with the Authorized redirect URIs `https://<your-app>.fly.dev/auth/google/callback` and,
   to try it on your computer, `http://127.0.0.1:8080/auth/google/callback`. Keep the client id and the client secret.
   The app asks only for the basic scopes (openid, email, profile), which need no review by Google.
2. Tigris: a private bucket and an access key for it. On Fly, `fly storage create` in `web/deploy/stage` makes both and
   sets `AWS_ENDPOINT_URL_S3`, `AWS_ACCESS_KEY_ID`, `AWS_SECRET_ACCESS_KEY`, `AWS_REGION` and `BUCKET_NAME` on the app.
   With a key of your own Tigris account, set those five yourself (`fly secrets set`, below); `AWS_ENDPOINT_URL_S3` is
   `https://t3.storage.dev` (or `https://fly.storage.tigris.dev`), `AWS_REGION` is `auto`.
3. The rest, as Fly secrets (`web/deploy/README.md`, "Accounts and cloud saves"), or as variables for a local server:

| Variable | |
|---|---|
| `GOOGLE_CLIENT_ID`, `GOOGLE_CLIENT_SECRET` | the client of step 1 |
| `CM_SESSION_SECRET` | 32 characters or more, random (`openssl rand -base64 48`): seals the cookies. A new one signs everyone out |
| `CM_PUBLIC_URL` | the site's address, `https://<your-app>.fly.dev` (default: the address the browser used) |
| `CM_CLOUD_USERS` | emails with cloud saves, comma-separated |
| `CM_CLOUD_FOR` | `everyone`: every signed-in account has cloud saves |
| `CM_UPGRADE_URL` | where "Get cloud saves" leads (a payment page), shown to accounts without cloud saves |
| `CM_CLOUD_MAX_SAVES` | the most saves an account keeps in the cloud (default 30) |

A good start prints `accounts and cloud saves: on`. Each save is gzip-compressed by the browser; the server checks that
it is a saved game of CM 01/02 (64 MB compressed at most) before it stores it.

## The page around the game

- The game fills the page: the canvas has the picture's shape (1280x800, or 800x600 in classic mode), as large as
  the window allows, drawn at the screen's own pixels (sharp on a high-DPI screen), with no bars.
- The bar above it has Saved games, Help and Full screen. In full screen the bar becomes three small buttons over the
  corner, and Escape goes to the game (in Chrome, hold Escape to leave).
- Keys go to the game unless a dialog of the page is open.
- "Saved in this browser" shows when a save has reached the browser's storage.
- "Working…" shows at the top left when the game keeps the tab busy for more than a second or so (a database load, a
  long day): the tab has not stopped.
- The browser asks before the page is left while a game runs, and the screen stays on.
- The second start needs no click: when the game files are in the browser, the game starts by itself.

## On an iPad or another touch device

| Do | The game gets |
|---|---|
| Tap | A click |
| Drag | The button held and moved: scroll bars, the tactics board |
| Long press, or a tap with two fingers | A click of the right button |
| Drag with two fingers | The mouse wheel: lists scroll with your fingers |

The game draws its text entries (a name, a search) on the canvas, so the device does not open its keyboard by
itself. When a text entry of the game takes keys, a "Tap to type" chip shows at it: tap the chip, or the text entry,
and the keyboard opens. The picture moves so the entry stays above the keyboard, and the keyboard closes when the
text is entered. The Keyboard button in the bar opens and closes the keyboard at any time. A mouse, a trackpad or a
keyboard connected to the device work as on a computer.

These need an engine built from this version of the code (`rt_web_type`, `rt_web_key`, `rt_web_picture` and
`rt_web_typing_row` in `src/recomp_rt/win/user32.c`): build it again after `git pull` (step 4). With an older engine
the page still runs; it has no Keyboard button then.

## As an app, and without a connection

Add the page to the home screen (on an iPad: Share, then Add to Home Screen) or install it from Chrome's address
bar. It then opens full screen, without the browser's bars. After one good start the page, the engine and the game
files are all in the browser, so it also starts with no connection (`web/sw.js`). With a connection the server is
always asked first, so a new version is used at once. This needs https, or 127.0.0.1 on your own computer.

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
- The canvas (`web/layout.js`): SDL takes the size of a canvas that CSS sizes, at each resize event of the window. The
  page gives the canvas whole multiples of the picture's shape (8 by 5, or 4 by 3), so SDL's window and the canvas's
  own pixels keep that shape exactly and the runtime's scaling fills them; the page sends a resize event when its
  layout changed the size.
- Touch (`web/touch.js`): SDL's own touch handling knows only the left button. The page takes the touches of the
  canvas before SDL and sends it mouse and wheel events, which SDL reads as from a real mouse.
- Typing (`web/keyboard.js`): a hidden text field of the page gets the device's keyboard; what is typed goes to the
  runtime, which hands it to the game one key at a time. The runtime tells the page which row of the picture a text
  entry is on.

## Tests

- `cd web && npm test`: the server lists and serves only the clean files, the season tables and the page, and refuses
  the rest; brotli and versioned caching; the stage script and the staged server; the save logic; the layout's
  arithmetic; the sign-in (state, PKCE, a token for another app or with no verified email is refused), the cookies, the
  cloud saves API (conflicts, limits, checks, account deletion) and the sync plan. The request signatures of `s3.mjs`
  were also checked against the examples of AWS's Signature Version 4 documentation and the AWS SDK's signer.
- `NODE_PATH=$(npm root -g) node web/test/page-check.cjs`: the page at full size (placeholder files with the sizes of
  the manifest, 460 MB) and an Emscripten stand-in in place of the engine (`test/fake-emscripten.js`): the browser
  check, the load, the Saved games panel, a second start from the browser's cache. Needs Playwright.
- `NODE_PATH=$(npm root -g) node web/test/cloud-check.mjs`: the accounts and cloud saves in Chromium, with stand-ins of
  Google and the S3 store (`test/fakes.mjs`, which checks every request's signature as S3 does) and of the engine:
  the sign-in from the start box and from a small window while the game runs, the saves of the cloud before the game
  starts, a save going up, Remove, a save changed on two devices, a second device, sign-out, and an account without
  cloud saves. Needs Playwright.
- `NODE_PATH=$(npm root -g) node web/test/sdl-check.mjs <probe folder>`: the page against the real SDL browser
  backend. The probe (`test/sdlprobe.c`) opens its window and renderer as the runtime does and prints what SDL tells
  it. The check covers the layout (no bars, the screen's own pixels, a resized window, classic mode, full screen), the
  mouse, the wheel and the keys, every touch gesture, the on-screen keyboard with a text entry, the Working sign, the
  question before leaving, and a start without a connection. Build the probe first:
  `web/test/build-probe.sh <SDL2 source folder> <probe folder>` (an unpacked SDL2 release of libsdl.org, and `emcc`
  on the PATH). Needs Playwright.

## Not yet

- Only the base game: the season databases (`Seasons/*.zip`) and the extra data sets are not loaded.
- The engine runs on the page's main thread. The concept design moves it to a Web Worker, with the new screens
  beside it.
- The touch and keyboard parts are checked in a desktop browser that plays a touch device, with the real SDL
  backend, not on an iPad itself: the device's own keyboard and Safari can still differ.
