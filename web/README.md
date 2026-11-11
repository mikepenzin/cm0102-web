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

**Where the saves live (the vault, `vault.js`).** IDBFS keeps every file of the user folder in memory and reads all of
it at the start, so a player with many saves ran the tab out of memory. With the vault on, saves, older copies and the
files of the data sets live in an IndexedDB store of their own (`cm0102-vault`), as Blobs that the browser keeps on
disk. The user folder holds a placeholder for each: its name, size and time, no contents, so the game lists it as
before. When the game opens one, the open waits (under JSPI the page makes the engine's `__syscall_openat` a
Suspending import, through `Module.instantiateWasm`) while the page reads the Blob in; when the game closes it
unchanged, the contents go again. A file the game or the page writes goes into the vault once it is closed and has
not changed for 1.5 seconds, and becomes a placeholder; a renamed file keeps its record (an older copy of the save
renaming keeps the old save's bytes), a deleted one loses it. At the first start with the vault the files move from
IDBFS's store, one at a time. The page reads placeholders with `readBytes`, `readView` and `readBlob` (`saves.js`);
Download and the cloud upload hand the Blob on without memory. The data set choice goes through ccall's async form:
the browser takes a Suspending import only inside `WebAssembly.promising`. The Asyncify build (Other browsers) has no
vault; files that an earlier visit put in it come back into the user folder. `?vault=0` or localStorage `cm0102-vault` = `off` turns it
off (the files in it then come back into the user folder).

The browser may delete the saves by itself, without asking: when the disk runs low (unless it agreed to keep the
site's storage, `navigator.storage.persist`), when browsing data is cleared, when a private window closes, and in
Safari after 7 days without a visit unless the page is on the home screen. Saved games and Help say so
(`storageWarning`, `saves.js`), with the advice to download the saves to keep or to use Cloud Saves.

Delete removes a save or an older copy (with Cloud Saves, a save in the cloud goes there too). Download all gives one
zip of every save, older copy and data set, as the save folder holds them (`zipOfBlobs`, `zip.js`: the files are stored
as they are and the zip is made of their Blobs, so the vault's files go from disk and a backup needs no memory). While the game runs,
the page keeps the game files and the whole user folder in memory: every save, older copy and data set. A browser that
runs out of memory stops the game with "Array buffer allocation failed"; the page then says so and suggests deleting
saves and older copies that are no longer played (download them first to keep them). The game's working files
(`*.tmp`, the parts of the game in play, written at a load and at each save) do not go to IndexedDB (`skipWorkFiles`,
`saves.js`): a save packs them into its `.sav`, and copying them at every sync held tens of MB more in memory.

## Statistics and the admin panel

The page sends a heartbeat each minute while the game runs (`stats.js`): the seconds played since the last one (the
tab visible and an input in the last 5 minutes) and how many saves, older copies and data sets the browser keeps,
with their size. A stop of the game sends an error report with the last lines of the engine log (at most 3 a visit).
A person is an account, or without one a browser with a random id (localStorage `cm0102-anon`); no name, email or IP
address of an anonymous player is kept. When a browser signs in, its record points at the account and its play counts
for it. The server (`stats.mjs`) keeps one object in the bucket (`stats/stats.json`, written every 2 minutes and when
the server stops) and the reports as files (`reports/<day>/…`, deleted after 90 days); people idle for 400 days go.
Each write reads the object again and adds what its machine counted since (`mergeStats`), so two machines both count.

`/admin` shows the totals (people, accounts and anonymous, active today, in 7 and 30 days, hours played, saves, older
copies, data sets, errors), the last 90 days, every person (sortable, searchable) and the error reports with their log.
It is open to the Google accounts of `CM_ADMIN_EMAILS` (comma separated, signed in on the game's page) and to a request
with `Authorization: Bearer <CM_ADMIN_TOKEN>` (16 characters or more). Such an account has an Admin panel button in
its Account dialog (`/api/me` says `admin`), which opens `/admin` in a tab of its own, so the game goes on. The people
table's Cloud Saves column shows each account's plan (free, paid until a day, given by an admin, or given by the
server list) and gives Cloud Saves to an account, or takes a gift back (`POST /api/admin/cloud`, Cloud saves below). Help, Statistics turns them off for a browser
(localStorage `cm0102-stats` = `off`). `CM_STATS=optin`: off until the player turns them on (for consent rules that
want it); `CM_STATS=off`: none. Without a bucket they stay in memory only. The privacy page says what is collected.

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

**Gifts and another bucket.** Cloud Saves an admin gives are a file of their own (`grants/<id>.json`), which only Give
and Take back write; the admin page and `/api/me` read the store, not the server's minute-long copy, so both machines
agree at once. The store has an id (`meta/store.json`, made at the first start on a bucket) that the lists name: when
the server is pointed at another bucket (`BUCKET_NAME` or its key changed), each page sends its saves and data sets to
it again instead of showing them as Only here. Gifts, accounts and statistics are files of the bucket too, so another
bucket starts without them.

**Straight to the store (`direct.js`, `cloud.mjs` "direct").** A save or a data set goes from the browser to Tigris and
back without passing the server's machine. The server checks the plan, the name, the size, the version (409) and the
count (507), then signs a link for 15 minutes (`s3.presign`, Signature Version 4 in the query: the length and the
headers are signed). The page sends the file to `incoming/<account>/…`; the server then reads its start (a save's
header; a data set's zip is read whole, 48 MB at most), checks it, and copies it to its place on the store (the bytes do
not come to the server); a file that fails the check is deleted and never replaces the save before it. A download is a
link too. At the start the server sets the bucket's CORS rules for the site's origins (`CM_PUBLIC_URL` and
`https://<FLY_APP_NAME>.fly.dev`, or `CM_CORS_ORIGINS`); the log then says `saves and data sets go straight between the
browser and the store`. `CM_DIRECT=on` skips the setting, for rules set in the store's console (`web/deploy/README.md`, "Straight to Tigris":
the key of `fly storage create` may not set them). When the store takes no rules, with `CM_DIRECT=off`, or when the
store does not answer a page
(the page's log says `did not answer this page directly`), the files go through the server as before. Uploads left in
`incoming/` go after a day.

**Who has cloud saves.** The accounts in `CM_CLOUD_USERS`, every account with `CM_CLOUD_FOR=everyone`, and the
accounts whose record in the store (`accounts/<id>.json`) says `"plan": "cloud"` until `planUntil`: a Paddle
subscription sets that (Payments, below), or you by hand in the Tigris console. And the accounts an admin gave Cloud
Saves on the admin page (Give, in the Cloud Saves column of People): the record then holds `"given": { by, at }`, a
field of its own that Paddle's events leave as it is, and Take back removes it. A gift is as good as a payment: the
player's Account dialog says "Given to this account", from the page's next load.

**What players see.** The start box of a first visit, the Account dialog and Saved games show the two plans: Free
(the whole game, saves in the browser, download and upload) and Cloud Saves (`CM_PRICE`, "$5 a month"), with the
links to `/pricing`, `/terms`, `/refunds` and `/privacy`. Signing in alone changes nothing; it is how cloud saves
know their account. A signed-in account without Cloud Saves gets the "Get Cloud Saves · $5 a month" button, which
opens Paddle's checkout over the page (the game keeps running). After the payment, the page turns Cloud Saves on by
itself once Paddle's webhook has come (a few seconds), and the running game's saves go up. An account with a
subscription has Manage subscription (Paddle's portal: cancel, card, invoices). After the first save in a browser, a
player without Cloud Saves gets one message about them.

**Payments (Paddle).** Paddle is the merchant of record: it sells the subscription, takes the payment and the sales
tax and VAT, and tells the server with signed webhooks (`billing.mjs`). The server keeps no payment data: the
subscription's id, customer id and state in the account record. An active subscription gives Cloud Saves to the end
of its billing period plus three days (for the renewal's webhook), a past-due one a week, a canceled or paused one
none; a cancel for the end of the period keeps them until then. An account with a running subscription cannot be
deleted until the subscription is canceled. In Paddle (start in the sandbox, `sandbox-vendors.paddle.com`):

1. Catalog, Products: Cloud Saves, with a price of $5 a month (recurring). Keep the price id (`pri_...`).
2. Developer tools, Authentication: a client-side token (`test_...` in the sandbox, `live_...` for real), and, for
   Manage subscription, an API key with the permission `customer_portal_session.write`.
3. Developer tools, Notifications: a destination `https://<your-app>.fly.dev/api/paddle/webhook` with the events
   `subscription.created`, `subscription.updated`, `subscription.activated`, `subscription.canceled`,
   `subscription.past_due`, `subscription.paused` and `subscription.resumed`. Keep its secret key.
4. Website approval (My account, Settings, or Checkout): the domain `<your-app>.fly.dev`. Paddle checks the site: the
   pricing, terms, refund and privacy pages are there, named in `CM_SELLER` and `CM_CONTACT`, and the terms say Paddle
   is the merchant of record. Once it is approved, Checkout settings, Default payment link: `https://<your-app>.fly.dev/`.
   Without both, Paddle's checkout opens with "Something went wrong".
5. The settings below (`PADDLE_*`, `CM_SELLER`, `CM_CONTACT`). For real payments, repeat 1 to 3 in the live account
   and leave out `PADDLE_ENV`.

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
| `CM_PRICE` | the price the page and the pricing and terms pages name (default `$5 a month`) |
| `PADDLE_CLIENT_TOKEN`, `PADDLE_PRICE_ID`, `PADDLE_WEBHOOK_SECRET` | Paddle's checkout: the client-side token, the price id (more than one, comma-separated, are all taken; the first is sold), the notification destination's secret |
| `PADDLE_ENV` | `sandbox` for Paddle's sandbox; unset for real payments |
| `PADDLE_API_KEY` | for Manage subscription; without it the button leads to paddle.net |
| `CM_SELLER`, `CM_CONTACT` | your name (as in your Paddle account) and contact email, on the terms, refund, privacy and pricing pages |
| `CM_UPGRADE_URL` | without Paddle: where "Get Cloud Saves" leads |
| `CM_CLOUD_MAX_SAVES` | the most saves an account keeps in the cloud (default 30) |
| `CM_CLOUD_MAX_SETS` | the most data sets an account keeps in the cloud (default 5) |
| `CM_DIRECT` | `off`: saves and data sets go through the server, not straight to the store; `on`: straight, with the bucket's CORS rules set in the store's console (the server does not set them) |
| `CM_CORS_ORIGINS` | the addresses the bucket's CORS rules let in, comma separated (default `CM_PUBLIC_URL` and the fly.dev address) |
| `CM_UPLOAD_BUDGET_MB` | the uploads the server holds in memory at once, through the server (default 160) |

A good start prints `accounts and cloud saves: on`. Each save is gzip-compressed by the browser; the server checks that
it is a saved game of CM 01/02 (64 MB compressed at most) before it stores it.

## Edit a saved game

Saved games has an **Edit** button by each save. It opens the save editor (`editor.js`, `savedit.js`):

- Search a player by name or club; the best players come first.
- Change current and potential ability, date of birth, the twelve position ratings, the 31 attributes the game shows
  (Technical, Mental, Physical, Goalkeeping, with Determination) and the hidden ones (Consistency, Important Matches,
  Injury Proneness, the feet, One On Ones and others). **Apply** puts the change into the save; **Save the game file**
  writes it.
- The numbers are those of the game's player profile. The game keeps 18 attributes relative to current ability (the
  technical ones, Anticipation, Decisions, Positioning): the editor turns them into the right bytes, and keeps them
  when the ability changes. A rating that the game cannot show with that ability (Heading 3 at ability 195, for
  example) gets the nearest one, and the editor says so.
- The save is written under its own name, so Load Game lists it; the file as it was goes to `backups/` as "<name>
  (before edit).sav" (Saved games, Older copies). With Cloud Saves on, the edited save goes up as any other save.
- If the save is loaded in the running game, save the changes in the editor and then load the save again: the game
  keeps its own copy until it loads.

Contracts, clubs, names and reputations are not edited, and a saved game gets no new players. The facts, with the
game's code addresses: `docs/research/2026-11-10-save-editor.md`.

## Edit a data set

The **Editor** button in the bar opens the editors: a saved game (as above), or a data set for new games.

- **New data set** copies the game's own database (the 20 files, about 60 MB) into `Databases/<name>` in the user
  folder. Start a new game and choose it under Database: the game lists such folders as it lists Update Team data
  sets. The game's own data stays as it is.
- Search a player, then change his names, nationality, club, contract end, ability, positions, attributes and
  Determination. The numbers are those of the database: 0 to 20, where 0 means the game sets the attribute when a new
  game starts (most players of the data have 0s).
- **Add a player** to any club, or as a free player. A club takes 50 players at most.
- **Save the data set** writes the changed files; the data set's other files stay.
- The Editor's home lists the data sets, with Delete.

**Data sets in Saved games.** The Saved games dialog lists the data sets under the saves (`setspanel.js`,
`datasets.js`), as it lists the saves:

- **Use** chooses one for the next new game, in a season (2001/02 to 2025/26; a copy of the game's data is 2001/02). It
  does what the Choose Season screen does, through the runtime call `rt_web_choose_data_set` (`season_screen.c`): the
  call sets the options, and Start New Game moves the game date to the season (the page calls between two frames,
  where the runtime must not run the game's code). The button at the top left of the game's first screen shows the
  choice the next time that screen opens. An engine built before that call: the page writes `database` and `season`
  in settings.txt, and the choice counts from the next start of the page. A season database that the game knows keeps
  its own season.
- **Download** gives a zip of a folder with the 20 files, which the desktop Bosman Edition takes too (unpack it into
  Databases in its save folder).
- **Upload** takes a zip (each folder in it with the 20 files becomes a data set, named by the folder: an Update Team
  zip, a data set of another browser) or the 20 `.dat` files (asked for a name). A data set keeps the other Data files
  of its folder that the game reads from a data set (`.cfg`, `.pct`, `.lng`: an update's own player_setup.cfg,
  euro.cfg, weather.cfg and tactics); pictures (`.rgn`) always come from the game's own Data. An uploaded data set
  starts on the latest season under Use: the game measures a database's year from its dates of birth and moves it to
  the season chosen, its players keeping their ages (`season_dates.c`).
- **Delete**; with Cloud Saves, a Cloud column and Remove, as for the saves.
- With Cloud Saves, the data sets go to the cloud too (zips in Tigris, `CM_CLOUD_MAX_SETS`, 5 by default, 48 MB each
  at most) and come to the other devices once their game is up; a data set changed on two devices is kept twice.

Players are added only to the format of the game's own data, not to Update Team data sets. The facts, with the game's
code addresses and the checks: `docs/research/2026-11-10-data-set-editor.md`.

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
no WebAssembly; no JSPI, when the server has no Asyncify build (Other browsers); or no room for the engine's memory (the page tries the engine's own plan:
768 MB, grown to 1,856 MB, with a 4 GB maximum). After Start it compiles the engine once, before the game files
download, so a browser that lacks a WebAssembly feature of the engine stops there. A device with no mouse, or a
browser that keeps no data (a private window), gets a warning but may start.

## Other browsers

The engine of step 4 needs JSPI, which Chrome 137 and later have. Safari, and every browser on an iPad (they are all
Safari inside), need a second build of the engine, with Asyncify in place of JSPI. The server serves both, and each
browser gets the one it can run (`web/engine.js`): JSPI where the browser has it, else Asyncify.

```bash
cd ~/Projects/cm0102-web
source ~/emsdk/emsdk_env.sh
emcmake cmake -S . -B build-web-asyncify -G Ninja -DCM0102_EXE=$HOME/cm0102-game/cm0102.exe -DCM0102_WEB_SUSPEND=asyncify
cmake --build build-web-asyncify --target cm0102_recomp
ls build-web-asyncify/cm0102_recomp.wasm
```

The server finds it in `build-web-asyncify` beside `build-web` (or at `CM_WEB_BUILD_ASYNCIFY`) and prints
`second engine, for browsers without JSPI: …` when it starts. The season tables come from `build-web`. The stage for
Fly takes it too (`web/deploy/README.md`). Engine log, first lines, names the build a browser runs:
`page: engine /engine/asyncify/ (asyncify build, …)`.

What to know about the Asyncify engine:

- It is larger and slower: Asyncify adds code to every function that may be on the stack when the game hands time
  to the page, and in the recompiled game that is nearly every function. The build takes longer too.
- It has no vault (the vault opens a file while the game waits, which needs JSPI): every save, older copy and data
  set file is in memory, as before the vault. A player with many saves can run the tab out of memory; Saved games,
  Download all and Delete keep the folder small.
- The engine needs 1,856 MB of memory (Browser requirements). An iPad with little memory refuses it, and the page
  says so before it downloads anything. Try it on a real iPad before you tell players it works there.

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
  cloud saves API (conflicts, limits, checks, account deletion) and the sync plan; Paddle's signatures, the
  subscription events (buy, cancel for the end of the period, a late event, the cancel), the portal link, and the
  pricing, terms, refunds and privacy pages; the save editor's model (the run code, Date.cpp dates, the ratings of
  0x544380 and 0x5442f0 and the bytes for them, a saved game read and written with only its staff and player tables
  changed); the data set editor's model (names, clubs, a move that keeps the squad lists packed, a new player with the
  tables after him moved, refusals that leave nothing behind). The editors were also checked on the real 3.9.68 files:
  an unchanged save writes back the same bytes; a data set with an edit and a new player loads through the loader's
  port (`src/cm3/database.c`) and passes `check_squad_lists`. The request signatures of `s3.mjs`
  were also checked against the examples of AWS's Signature Version 4 documentation and the AWS SDK's signer.
- `NODE_PATH=$(npm root -g) node web/test/page-check.cjs`: the page at full size (placeholder files with the sizes of
  the manifest, 460 MB) and an Emscripten stand-in in place of the engine (`test/fake-emscripten.js`): the browser
  check, the engine a browser with and without JSPI gets, the load, the Saved games panel, a second start from the
  browser's cache. Needs Playwright.
- `NODE_PATH=$(npm root -g) node web/test/cloud-check.mjs`: the accounts and cloud saves in Chromium, with stand-ins of
  Google, the S3 store (`test/fakes.mjs`, which checks every request's signature as S3 does), Paddle.js and the
  engine: the plans in the start box, the sign-in from the start box and from a small window while the game runs, the
  saves of the cloud before the game starts, a save going up, Remove, a save changed on two devices, a second device,
  sign-out, and a free account that buys Cloud Saves while its game runs (checkout, webhook, the save goes up).
  Needs Playwright.
- `NODE_PATH=$(npm root -g) node web/test/editor-check.mjs`: the editors in Chromium, with a small saved game and a small
  data set in the game's formats (`test/fakes.mjs`). The save editor on a computer and a phone: Edit, search, change,
  Apply, a rating out of reach, Save, the backup, a file that is not a full save. The data set editor: a name refused,
  New data set, an edit with a move to another club, Add a player, an unknown club refused, Save, Delete; the Data sets
  of Saved games: Use (through settings.txt and through the runtime call), Download, Upload of a zip and of the 20
  files, Delete. The cloud check adds a data set uploaded on one device that comes to the other.
- `NODE_PATH=$(npm root -g) node web/test/sdl-check.mjs <probe folder>`: the page against the real SDL browser
  backend. The probe (`test/sdlprobe.c`) opens its window and renderer as the runtime does and prints what SDL tells
  it. The check covers the layout (no bars, the screen's own pixels, a resized window, classic mode, full screen), the
  mouse, the wheel and the keys, every touch gesture, the on-screen keyboard with a text entry, the Working sign, the
  question before leaving, and a start without a connection. Build the probe first:
  `web/test/build-probe.sh <SDL2 source folder> <probe folder>` (an unpacked SDL2 release of libsdl.org, and `emcc`
  on the PATH). Needs Playwright.

GitHub runs the unit tests and the page, editor and cloud checks on each push that changes `web/`
(`.github/workflows/web.yml`).

`RECOMP_GUARD=1` is a test switch of the native game (macOS, Linux): the guest memory that the browser build keeps for
itself (0x21000000 to 0x60000000) and the memory past the heap's top, which the browser does not have, fault there
too. A stray pointer that only breaks the browser game ("memory access out of bounds") then stops the native game at
the access, with `recomp: guard: an access to guest …` in the log and the stack under gdb. Run a season jump with it
(`docs/TESTING.md`, "Memory and big saves").

## Not yet

- The server hands out the base game only: the season databases of the desktop downloads (`Seasons/*.zip`) are not on
  it. A player brings a data set of their own (Saved games, Data sets: Upload) or makes one in the data set editor.
- The engine runs on the page's main thread. The concept design moves it to a Web Worker, with the new screens
  beside it.
- The touch and keyboard parts are checked in a desktop browser that plays a touch device, with the real SDL
  backend, not on an iPad itself: the device's own keyboard and Safari can still differ.
- The Asyncify engine (Other browsers) is checked with the stand-in engine only: a real build of it, its size and speed,
  and its memory on a real iPad are still to try.
