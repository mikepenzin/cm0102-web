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
| A box "This browser cannot run the game" | It names the reason; use Chrome 137 or later, or Safari 27 (iPadOS 27 on an iPad, in any browser) |
| The game stops in the tab | Open "Engine log" under the game and send its last lines |

"Engine log" holds every line the runtime prints (`recomp: ...`) and the page's own lines (`page: ...`).
A stage of a long job that takes 2 seconds or more writes its time, for example `recomp: progress: Player histories
took 5.0 s` (`display.c`, progress_note): a new game on a computer with the October 2026 data and six leagues took
8, 5, 2.5, 10 and 14 seconds for Player Initialisation, Player histories, Initialising game data, Initialising
Leagues and Creating Shortlists. A player who says the game stopped at a stage can send these lines.

First run: 9 November 2026, on a MacBook Air in Chrome: the database load, a new game, and days to 12.8.01 with the
cup and Champions League rounds. What to expect:

- The runtime prints `recomp: web memory ... MB, host data at 0x21..., host heap at 0x2...` at the start.
- The tab freezes while the game works for a long time without asking for input (the database load of a big new
  game). The page draws and takes input whenever the game polls for input.
- Sound starts after the first click in the game.

## Saved games

The game saves into the user folder as on the desktop (Game Options, Save Game). The Saved games button above the
game lists those saves: Download writes one to your computer, as a normal `.sav` file; Upload a save puts a `.sav`
file into the folder, and the game's Restore Saved Game screen then lists it. An upload is judged by what it holds,
not its name (`upload.js`): a saved game is taken whatever its name ("Matt", "Matt.sav.txt"); a `.zip` (a save zipped by
another site, as a player's was, or a part of Download all) gives the `.sav` files in it, wherever in its folders,
except its older copies, each read from the zip as it is stored (`zip.js`, zipBlobEntries), so neither is in memory
whole, and its tactics go to Tactics; a `.gz` is unpacked first; a tactic goes to Tactics. Anything else gets its
reason and what to do: a RAR or 7-Zip archive (unpack it first), a web page (the site's link gave its page, not the
file), a data set or one of its files (Data sets below), a picture, a text, an empty file. An upload is checked first (it must be a
save container of CM 01/02), and a name that is taken gets a number. The saves use the format of the desktop game, so a
save should move between this page and the Bosman Edition both ways.

**Play (`play.js`, the runtime's `play.c`).** Each save has a Play button: the game loads that save as a player would
from its start screen. The page calls `rt_web_play` with the file name; the runtime waits for the start screen, clicks
Restore Saved Game, types the name into the dialog's File entry (deleting with Backspace what the entry opened with,
the save loaded last), reads the entry back, and clicks Ok; while the entry still takes keys the first Ok only ends the
typing (a run of 16 November 2026 needed the second), so it clicks Ok again once that click has ended and two input
polls ran, and never while the runtime waits on a held button. The play ends when the dialog is gone. It finds each
button by its action id in the game's own widget list, so it works at every picture size and in every language of the
game. It types the name's UTF-8 bytes as they are, as a click on the save's row of the game's list would put them, so a
name in any script opens (the game's list shows such a name as its bytes, for example "KÃ¶ln" for "Köln"). A name with
a character the entry does not take (`" * / < > | { } [ ] ~ : \ ?`) is refused at once with that reason, and one too
wide for the box after a few tries; the page names Restore Saved Game, or a new name by Download and Upload, as the
way. With a game in play the page asks first ("The game in play closes"), keeps the name in sessionStorage for five
minutes, restarts as Restart Game does, and asks the engine again once it has started. When a step does not come (no
start screen in five minutes, no step forward for 30 seconds) the engine says so through `Module.rtPlay`. An engine
built before Play has no `rt_web_play`, and the page shows no Play button. Tests: `tests/smoke/play_run.py` (native:
a Köln and a Greek name, a second start with the entry filled, a refused name, widescreen) and the page check (the
stand-in engine).

The saves stay in IndexedDB, not localStorage: localStorage holds a few MB of text per site, and a save holds the
whole game world. A new or changed save goes to IndexedDB about 2 seconds after the game finished writing it. Use
the same address every time: `127.0.0.1:8080` and `localhost:8080` are two sites to the browser, each with its own
saves.

**Big files and the browser's memory (`disk.js`).** A cloud save is unpacked, and a save is packed for the cloud or a
report, through a file of the origin's private file system (OPFS) as it streams, not as one Blob in the page's memory:
an iPhone kept a 446 MB cloud save in the tab's memory at the start, beside the game files, and iOS closed the tab at
every start ("A problem repeatedly occurred", 18 November 2026; iOS gives a tab about 2 GB). The vault must keep the
bytes, not a reference: Chromium's IndexedDB keeps only a reference to a Blob that comes from a file (an OPFS file, or
one the player picked for Upload), which broke when the file went (a save uploaded from Downloads and deleted there).
The page finds out once per browser version which its storage does (a 16-byte file), and gives the vault a copy that
the browser makes where it keeps references (`ownable`, `toStoredBlob`); the log says which. A cloud save whose download
closed the tab is left in the cloud on that device at the next start (the sudden-stop mark names it), and Account
brings it with "Get it here". A cloud save does not come as one Blob at all where the vault is on: it is unpacked as it comes and
stored in part records of 4 MB, each written on its own (`vault.adoptStream`; keys `~part:<id>:<n>`, and a main record
`{ size, mtime, partsId, parts }`), so no more than a part is in memory; even with the cloud route through the server,
an iPhone still closed the tab when the whole save was one Blob. Readers join the parts into one Blob of the storage
(`blobOf`), a rename or an older copy keeps or shares them, and each start removes the parts that no record names.

**The game files out of memory (`loader.js` LAZY_MIN, `vault.js` gameFile).** The page once read all 467 MB of game
files into the engine's in-memory file system at every start, though the game reads 18 MB of them on the way to its
first screen: the tab's JS heap was 497 MB at that screen, and an iPhone gives a tab about 2 GB. With a JSPI engine,
the vault wanted and the browser's cache storage, a file larger than 64 KB (491 files, 465 MB) now goes from the
download straight into the cache (counted on the way, never kept in memory), and the engine's file system gets a
placeholder of its size; when the game opens it, the vault takes its Blob from the cache (`response.blob()`) and the
game reads the pieces it needs, as it reads a save (below). The 272 smaller files (1.8 MB) come into memory as before.
The page's own reads of them (the editor copies the game's database for New data set) go through the vault
(`readBytes`). Measured with the live engine and the real files (21 November 2026): the first screen with a JS heap of
30 MB instead of 497 MB and the tab's process 504 MB instead of 936 MB; a new game and a holiday 792 MB at the most
instead of 1151 MB; a start from the cache 1.7 s, its files ready in 0.4 s instead of being read. With `-seed 7` the
game's own dump of its first news screen is the same, byte for byte, as before. Without JSPI (the Asyncify engine of
Safari and the iPad, before JSPI), without a cache (an http address that is not this computer), or when the engine's
reads could not be hooked, every file comes into memory as before.

**Where the saves live (the vault, `vault.js`).** IDBFS keeps every file of the user folder in memory and reads all of
it at the start, so a player with many saves ran the tab out of memory. With the vault on, saves, older copies and the
files of the data sets live in an IndexedDB store of their own (`cm0102-vault`), as Blobs that the browser keeps on
disk. The user folder holds a placeholder for each: its name, size and time, no contents, so the game lists it as
before. When the game opens one to read it, nothing is loaded: each read takes the pieces of the Blob it needs (4 MB
each, the last 12 kept), so a save is never in memory whole. Under JSPI the page makes the engine's `__syscall_openat`
and `fd_read` Suspending imports, through `Module.instantiateWasm`, so they can wait for the browser's storage. Before,
an open read the whole Blob in and a close dropped it, and the game opens a save about 100 times during a load: with
four saves of 690 MB (a player's size; a real 46 MB save padded, 15 November 2026) the Load Game list took 4.1 s and a
load 99 s with 5.6 GB of memory in the tab. In pieces: 0.04 s, and 7 s with 0.9 GB. A file opened to be written (and
not emptied) still comes into memory whole. A save from outside, an upload or a cloud download, goes into the vault as
it is (`storeSave` in `saves.js`, `vault.adopt`): only its header is read for the check, and the browser copies the
file, not the page. The page once read the whole file and copied it into the user folder: uploading one 690 MB save
took the tab to 3.4 GB; now it stays under 1 GB and takes about a second. A file the game or the page writes goes into the vault once it is closed and has
not changed for 1.5 seconds, and becomes a placeholder; a renamed file keeps its record (an older copy of the save
renaming keeps the old save's bytes), a deleted one loses it. At the first start with the vault the files move from
IDBFS's store, one at a time. The page reads placeholders with `readBytes`, `readView` and `readBlob` (`saves.js`);
Download and the cloud upload hand the Blob on without memory. The data set choice goes through ccall's async form:
the browser takes a Suspending import only inside `WebAssembly.promising`. The Asyncify build (Other browsers) has no
vault; files that an earlier visit put in it come back into the user folder. `?vault=0` or localStorage `cm0102-vault` = `off` turns it
off (the files in it then come back into the user folder).

**Room for a save while the game writes it (`room.js`).** The game writes a save as `temp_save_file.sav`, opening it
about once per member to read its header, add the member at the end and write the header again (138 opens for a save
of 46 MB, under strace), then renames it. Emscripten's memory file system grows a file by 1.125 times whenever a write
goes past its room, copying the whole file each time: a save of 690 MB was copied about fifty times, and the tab went
from 0.9 GB to 3.8 GB while it was written. The page gives the file room at once for the largest save of the folder
(5% more); past that, or with no save in the folder, it grows as before. Room that is never written takes no memory of
the computer. Measured with a save of 690 MB written over one of that size (`/user`, the real engine's file system):
the tab's peak while writing 2.2 GB instead of 3.8, the write 0.4 s instead of 6.3, the peak while the vault stores it
2.3 GB instead of 3.0. What is left is the file itself and the vault's copy of it until the browser has stored it; the
first save of a large game, with nothing to size the room from, is as before.

**After a save: one copy, not three (`vault.js` putParts, `saves.js` storedFirst).** An iPhone (Safari 27, the JSPI
engine) closed the tab right after the game saved, "in the game" by the sudden-stop mark (21 November 2026). The
page's check every 2 seconds then started two jobs at once: the vault stored the new save as one Blob of its whole size,
a copy of it beside the file the game had written, and the cloud upload read the save at once, while the vault still
stored it, and made one more Blob of it from memory. With a save of a few hundred MB that is three copies of it and the
game, past what iOS gives a tab. Now the vault stores a written file in parts of 4 MB, each a Blob of its own written
on its own, as a cloud download is stored; a file that changes meanwhile is stored at the next round. The parts of the
version a save replaces go in the same round, when no other record (an older copy) names them. The upload waits for
the vault to store a save it finds in memory, then reads the stored copy from the browser's storage. The vault check
counts the Blobs made from memory: none is larger than 4 MB, for the store or for the upload.

**A write counts when it is committed (`vault.js`, committed).** An IndexedDB request's success comes before its
transaction commits, and the commit can still fail: a full disk, or the room the browser gives the site
(QuotaExceededError). The vault once took the request's success as stored and dropped the save from memory; after a
failed commit the save was in neither place. The tab still listed it at its size, a Download of it was empty, and the
next visit did not list it. A player reported two saves of two days as 0 KB files and gone from the list (13 November
2026); this or two tabs (below) would each do that, and which one did is not known. Now a put or a delete waits for its
transaction's `complete`; on a failure the save stays in the tab's memory, the next round tries again, and the page
says once "A save did not reach this browser's storage … keep the tab open and Download the save" (`vaultRound`,
`loader.js`), then again when a round works.

**A connection the browser closes.** The browser can close a site's IndexedDB connection by itself (Android's Chrome
did, in an error report of November 2026); every transaction on it then fails with "The database connection is
closing". The vault opens it again at its next request and tries that request once more. For the user folder the page
drops IDBFS's kept connection (`IDBFS.dbs`) when a write fails that way and writes again, which opens a new one; a
second failure in a row is reported as before ("the user folder was not saved").

**One tab at a time (`loader.js`, myTurn).** Every tab of the game writes the same storage, and each write once removed
what another tab had stored meanwhile: IDBFS's sync removes the files it does not have, and the vault's round removed
every record it did not know. A save made in one tab was gone from the storage 2 seconds later, while that tab still
listed it, and its Download was an empty file, which an upload refuses as "too short to be a saved game". A player's
screenshot of that message (13 November 2026, with nothing else known about the file) led to the find; whether two
tabs made that file is not known. The tab that runs the game now holds a Web Lock (`cm0102-game`), taken before the game files
load. A second tab says "The game is open in another tab" and offers Play here: it asks the first tab over a
BroadcastChannel to stop, and that tab ends its game (letEngineGo), stores its saves (storeEverything, as Restart Game
does), writes nothing from then on, lets the lock go and says "The game moved to another tab". What was not saved in
the game is lost there, as when the tab closes. A tab that does not answer in 90 seconds (the browser may have paused
it) gets Try again, or Play here anyway, which steals the lock; that tab then stops when it wakes, without storing. As
a second guard, the vault's round removes only the records its own page mounted or stored, and reading a placeholder
whose record is gone fails ("the browser's storage no longer has …") where it once gave an empty file; Download all
leaves such a file out and names it. An upload that is too short says its size, and an empty one says so. A browser
without Web Locks starts as before.

Two more holes are closed. A page loaded before this code takes no turn, so after a deploy a tab left open from the day
before would go on removing a new tab's saves: the vault's database is now version 2 (`VAULT_VERSION`), and opening it
asks the older page's version 1 connection to close (that page's code does, on `versionchange`); it cannot open version
1 again, so each of its vault rounds fails ("The requested version (1) is less than the existing version (2)") and it
removes no saves from the vault. That fences the vault only: the older page's user folder sync (IDBFS, every 15 s)
still runs, so until it is closed or loaded again it can remove the new tab's settings, tactics and data set notes,
and with the Asyncify engine or the vault off, saves too. Its own game can no longer open its vault's saves (the open
fails and the player is told nothing). A page older still, which keeps its connection, holds the opening up: the new page says "An older page of the
game runs in another tab" and goes on by itself once that tab is closed or loaded again. (Checked on the real engine
with the page of commit ce216d4 in one tab and this one in another: the new tab's save stayed whole.) And a tab whose
turn was taken with Play here anyway learns it from its lock request only once it wakes, maybe after a timer of its
own: every write of the storage (the vault's round, the user folder's sync, storeEverything) first asks the browser
who holds the lock (`navigator.locks.query`, stillMine) and stops the tab when it is not this one.

The browser may delete the saves by itself, without asking: when the disk runs low (unless it agreed to keep the
site's storage, `navigator.storage.persist`), when browsing data is cleared, when a private window closes, and in
Safari after 7 days without a visit unless the page is on the home screen. Saved games and Help say so
(`storageWarning`, `saves.js`), with the advice to download the saves to keep or to use Cloud Saves.

Saved games says what the saves and older copies take and what the browser allows more (`spaceState` in `saves.js`,
`navigator.storage.estimate`; a nearly full disk can still refuse what the browser allows). When that is less than two
saves of the largest size, or than 300 MB, the line says so, the bar's Saved games button gets a mark and the player is
told once a visit, before a save fails for it. The list keeps room for about six rows. Older copies show their total
(the game keeps three of each save: 2 GB for one save of 690 MB), and Remove all older copies removes them at one
press. In the browser the game makes none by default: the page turns the engine's option
`save_backups` off once in `settings.txt` (`backupsOffOnce`, `saves.js`; a marker line says it did, so a player who
turns Save backups on again in the game's Options keeps it on). Three copies of each save took gigabytes, and players
keep their own copies (Download, Cloud Saves). With Save backups off, each start removes every older copy (those of
earlier visits, and the save editor's "before edit" copy of the visit before), except a copy the player named and
none while a save is in progress. Delete of a save takes its older copies with it (the question says how many), and with Save backups on, each start removes the older
copies of saves that are gone (deleted in the game, on another device, or before this rule: a player had 18 copies,
1.3 GB, of saves he had deleted; `removeOrphanCopies`, `saves.js`). A copy belongs to a save by its name (the engine's
`<save>.1.sav` to `.3.sav`, the editor's `<save> (before edit).sav`); a copy of a save the cloud keeps stays, a copy with
another name stays, and none goes while a save is in progress (`temp_save_file.sav`: the old save is then a copy and the
new one not named yet).

Delete removes a save or an older copy (with Cloud Saves, a save in the cloud goes there too). Download all gives one
zip of every save, older copy and data set, as the save folder holds them, or, when that is more than a zip holds (4 GB:
six saves of 690 MB pass it), a part of at most 3.5 GB at each press (`zipParts`), the button naming the next one (`zipOfBlobs`, `zip.js`: the files are stored
as they are and the zip is made of their Blobs, so the vault's files go from disk and a backup needs no memory). While the game runs,
without the vault (an Asyncify engine, or `?vault=0`) the page keeps the game files and the whole user folder in memory:
every save, older copy and data set (with the vault, the game files stay in the browser's cache too: The game files
out of memory). A browser that
runs out of memory stops the game with "Array buffer allocation failed"; the page then says so and suggests closing the
tab and opening the game in a new one.

**The engine is let go when the page goes (`loader.js`, letEngineGo).** A JSPI engine never returns from main: between
frames its stack is parked inside `emscripten_sleep`. The browser kept such a parked stack alive after the page was
gone, and with it the engine, its memory and the 467 MB of game files: in Chromium the tab's process grew by about
515 MB at each load of the page (Restart Game, Start again, a plain reload), 12.6 GB after 23 loads, and a player's
Chrome answered "Array buffer allocation failed" while reading the game files (12 November 2026). The page now wraps
the engine's sleep (it finds it as the `WebAssembly.Suspending` that Emscripten makes for the import
`env.emscripten_sleep`), and before the page goes (Restart Game, Start again, and `pagehide` for every other way out)
the sleep in progress ends with the error `EngineLetGo`: the game's stack unwinds through main and nothing stays
parked. With that the process stays near 800 MB over 45 loads. An engine whose sleep the page cannot find logs
"this engine's sleep cannot be ended". The game's working files
(`*.tmp`, the parts of the game in play, written at a load and at each save) do not go to IndexedDB (`skipWorkFiles`,
`saves.js`): a save packs them into its `.sav`, and copying them at every sync held tens of MB more in memory.

## Statistics and the admin panel

The page sends a heartbeat each minute while the game runs (`stats.js`): the seconds played since the last one (the
tab visible and an input in the last 5 minutes), how many saves, older copies and data sets the browser keeps,
with their size, and the browser's language (`navigator.language`, at most 16 characters: the admin page's
Languages list counts the people of the last 30 days by its first part, `pt-BR` as `pt`, to learn which languages the
page needs). A stop of the game sends an error report with the last lines of the engine log (at most 3 a visit).
The game's own fatal errors show a box "Error" with only "v3.9.68 <source>..cpp <line>" and then exit with -1 (419
sites in the original, each `gui_show_message_box` 0x5e8290 then `exit(-1)` 0x945934). The runtime logs every box of
the game with its caller, and a fatal one with the return addresses on its stack (`waits.c`), and the page's error box
quotes it ("The game stopped on an error of its own: …"), so a report names the check that failed. Before that, a
player's save that would not load reported only "The game ended with code -1" (14 November 2026).
The page asks first (`CM_STATS` opt-in, the default): a card with Allow and No thanks, which look the same; before a
yes nothing is sent and no id is stored, and No thanks sends only the answer with no id (`/api/stats/choice`), which the
admin page counts as the share of players who said yes. Help, Statistics changes the answer; off deletes the id.
A person is an account, or without one a browser with a random id (localStorage `cm0102-anon`); no name, email or IP
address of an anonymous player is kept. When a browser signs in, its record points at the account and its play counts
for it. The server (`stats.mjs`) keeps one object in the bucket (`stats/stats.json`, written every 2 minutes and when
the server stops) and the reports as files (`reports/<day>/…`, deleted after 90 days); people idle for 400 days go.
Each write reads the object again and adds what its machine counted since (`mergeStats`), so two machines both count.

`/admin` has five tabs (the address's `#overview`, `#people`, `#members`, `#errors`, `#library` opens one). Members:
every Buy Me a Coffee membership the webhook told the server of, signed in or not (`members/`), and the last
notification with what came of it (`coffee/last.json`, no email). Overview: what to look
at (reports sent by players, reports in the last 24 hours, a message that comes again, databases with no picture), the
totals (people, accounts and anonymous, active today, in 7 and 30 days, hours played, saves, older copies, data sets,
errors) and four charts of the last 7, 30 or 90 days (people active a day, hours played, new people, error reports),
each against the days before. People: 50 at a time, searchable, sortable, with filters (accounts, anonymous, Cloud
Saves, with errors, seen in 7 days); a row opens its details. Error reports: the messages that come most, then each
report; a report opens with its log, and Copy report, Copy log only and Download .txt (Copy in the list copies it
without opening). A phone gets each row as a card.
It is open to the Google accounts of `CM_ADMIN_EMAILS` (comma separated, signed in on the game's page) and to a request
with `Authorization: Bearer <CM_ADMIN_TOKEN>` (16 characters or more). Such an account has an Admin panel button in
its Account dialog (`/api/me` says `admin`), which opens `/admin` in a tab of its own, so the game goes on. The people
table's Cloud Saves column shows each account's plan (free, paid until a day, given by an admin, or given by the
server list) and gives Cloud Saves to an account, or takes a gift back (`POST /api/admin/cloud`, Cloud saves below). Help, Statistics turns them off for a browser
(localStorage `cm0102-stats` = `off`). `CM_STATS=optout`: on until the player turns them off, with no question (not for
players in the EU, whose consent rules want the question); `CM_STATS=off`: none. Without a bucket they stay in memory only. The privacy page says what is collected.

**Send to the developer (`report.js`).** The error box, a message of Saved games that says something went wrong (a
refused upload, a download that failed), and Help (Something wrong?) have Send to the developer. After a refused upload
the dialog offers the refused files first, so the file that did not load goes as it is (an empty one too). It is a dialog
with the error, a note the player may write, and a choice of saved game, preset to the save in play (the last one the
game read from past its 262 KB header, which a load does and the Load Game list does not, or the last one it wrote);
with none in play it is No save, so a save goes only when the player picks one. Send posts the note, the error and the last 2,000 lines of the engine log (`/api/report/send`); the server
keeps it as a report file marked as sent, with no need of the statistics' consent, since the player sends it. The log
goes first, so it arrives even when the save does not. The save then goes packed with gzip in the browser, straight to
the bucket with a link the server signs for its packed size (`/api/report/save`, then `/api/report/save-done`, which
checks it is there), as `reports/<day>/<time>-<random>.sav.gz` beside the report. Only the page that sent the report can
add its save (a token in the report file, gone once the save is there). When the bucket does not let the page in (its
CORS rules missing or wrong: on the live site the first try failed so, "the connection to the store failed", where Cloud
Saves had gone through the server unnoticed), the page sends the packed save through the server instead
(`/api/report/save-body`), and the engine log says why. The server writes it to a file on its disk with its sha256
and streams it to the store (`s3.putFile`), so a large save (a player's was 446 MB, more than 150 MB packed, which
the server once held in memory and refused) needs no memory; at most two such files at once. A server without
a bucket takes the log only, and the dialog says to send the save another way. When the save did not go (the server
busy, the store failing), Send the save again sends it to the same report. The page cuts each log line to the 400
characters the server keeps, so a log of long lines still fits. A save may be 400 MB packed straight to the bucket; a
machine takes 200 reports, 40 saves and 4 GB a day, and the server checks a save it takes itself (its report, token
and announced size) before it reads a byte of it. The
admin page marks such a report "Sent by the player", shows the note and the save, and Download the save gets it from
the bucket (a link for an hour) and unpacks it under its own name. Reports and saves go after 90 days.

**A notice of each report (`notify.mjs`).** With `CM_REPORT_WEBHOOK` set (web/deploy/README.md), each sent report gives
the developer one short message on a Discord, Slack or Telegram webhook: the game's error, the browser, whether there
is a note, the save's name and size, and a link to the admin page (`CM_SITE_URL`, or the app's address on Fly; never the
request's Host, which a sender could set). The note, the log and the save stay on the server. The message waits up to
90 seconds for the save, so a report is one message, and at the end of the wait it reads the report file, so a save that
reached the other machine counts. At most 30 a day, then one that says more came.

**Tactics (`tactics.js`).** The game keeps a player's tactics as it does on a PC: Tactics, Save As on the tactics screen
writes `<name>.tct` in the folder `tactics` of the user folder, and Tactics, Load lists that folder. A saved game
keeps the tactics folder of the computer where the game began (`E:\tactics` from the Bosman Edition on a PC,
`C:\Program Files (x86)\Championship Manager 01-02\tactics` from the original): the engine takes such a path as the
game folder's `tactics` (`src/recomp_rt/win/tactics_path.h`, in `win_host_path`), so an uploaded save saves its tactics
here too (before, Save As said "could not be saved", or wrote where Saved games does not look). Saved games has a
Tactics section to download them, upload others (from the PC game or a friend; several at once) and delete them, and
Download all takes them too (`tactics/` in the zip). An upload must be a tactic of the game: a version word of
0x98EC54 to 0x98EC5E at offset 0, in either byte order (big-endian when the file ends with 0xF1FFFFF1, as the loader
0x5b5d10 reads it). The presets that ship in `Data` (`*.pct`) store the version plus 0x075BCD15 and differ in nothing
else from the same tactic saved by the game; the tactics screen's Load refuses such a file ("Unable To Load"), so an
upload of one, as a `.pct` or a `.tct`, is stored with the plain version and as `.tct`, and then loads. They stay in the
browser's storage with the rest of the user folder, and with Cloud Saves they go to every device of the account
(Cloud saves, below).

## Cloud saves

Cloud Saves are one of the two parts of **Membership**, the paid plan (the other is the Scout, "The Scout"): a member
has both; the account record's flag stays `cloud`, and the settings below keep their names. Players can sign in with
Google. An account with cloud saves keeps its saved games in the cloud as well as in the
browser: a save made on one computer or iPad is there when the game starts on another. Without an account, or on an
account without cloud saves, nothing changes: the saves stay in the browser, and Saved games downloads and uploads
them as files.

**How it works.** The server (`cloud.mjs`) runs the Google sign-in (the authorization code flow, with PKCE) and keeps a
signed cookie; no script of Google's runs on the page. The saves go to an S3 store (Tigris on Fly.io, `s3.mjs`),
gzip-compressed, under the account's Google id. The page (`account.js`):

- Before the game starts, the saves made elsewhere come into the user folder, so Load Game lists them. A slow
  connection gets a "Start without them" button after 5 seconds.
- After each save, the new or changed save goes up ("Saved to the cloud"). With no connection it goes up later. A
  large save (100 MB and more) that went up less than 10 minutes ago waits ("Goes up soon" in Saved games) and goes at
  the next minute's try once that time is past, or at once when the tab is left (hidden): a player who saves a game of
  690 MB every few game days would otherwise send about 300 MB packed at each save, and the other saves would wait
  behind it (`BIG_SAVE`, `BIG_EVERY_MS` in `account.js`). Its first upload goes at once.
- The tactics of the folder tactics go too (`tacticsync.js`, `/api/tactics`): the cloud keeps one record per account
  (at most 200 tactics, 1.5 KB each), and a sync at the start, after an upload or a delete in Saved games, and each
  minute when the folder changed merges it with the folder, per name the newest wins (a file's time, or the time it was
  deleted in Saved games, so a deletion reaches the other devices). It goes up over the version it read; when another
  device wrote meanwhile, it reads and merges again. settings.txt stays on each device: it holds what suits that
  device (the picture size, the window).
- A save changed on two devices since they last synced: when the one in this browser is the newer (its time against
  the time the cloud's was saved), it goes up over the cloud's and nothing comes down; the store keeps the cloud's
  earlier versions (the bucket's snapshots). When the cloud's is the newer, both are kept, nothing is lost: the other
  device's version as "<name> (other device).sav", in both places. A player asked for it (21 November 2026): "if the
  browser's version is newer than the cloud's, use it and update the cloud".
- A cloud copy that this device sent itself is not a change made elsewhere. An upload can go through after its tab
  closed (a large save goes when the tab is left), and the page then never recorded the cloud's new etag: the next start
  took the copy for another device's and brought it down as "(other device)", at every reload (an iPhone, 21 November
  2026). The page now keeps the version it sends before it sends it (`sending` in the sync's state), and a copy whose
  length and save time, to the millisecond, are those of the file here or of that version is this device's own
  (`planSync`): nothing moves, or the newer save here goes up over it. The engine log says why each save moves
  (`page: cloud saves: <name>: up (changed here)`).
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
Google emails with a running Buy Me a Coffee membership of a level that gives them (`members/<sha256 of the
email>.json`, Payments, below), the accounts whose record in the store (`accounts/<id>.json`) says `"plan": "cloud"`
until `planUntil`: an older Paddle subscription sets that, or you by hand in the Tigris console. And the accounts an admin gave Cloud
Saves on the admin page (Give, in the Cloud Saves column of People): the record then holds `"given": { by, at }`, a
field of its own that Paddle's events leave as it is, and Take back removes it. A gift is as good as a payment: the
player's Account dialog says "Given to this account", from the page's next load.

**What players see.** The start box of a first visit, the Account dialog and Saved games show the two plans: Free
(the whole game, saves in the browser, download and upload) and Cloud Saves (`CM_PRICE`, "$5 a month"), with the
links to `/pricing`, `/terms`, `/refunds` and `/privacy`. Signing in alone changes nothing; it is how cloud saves
know their account. A signed-in account without Cloud Saves gets the "Become a member · $5 a month" button and the
email to join with; the button opens the membership page of Buy Me a Coffee in another tab (the game keeps running).
The page then asks the server every 5 seconds for 20 minutes, and at once when the player comes back to its tab, and
turns Cloud Saves on by itself once Buy Me a Coffee's webhook has come; the running game's saves go up. A member has a
Your membership button (Buy Me a Coffee: change, pause, cancel); a member of a level without Cloud Saves is told so.
An account of an older Paddle subscription keeps Manage subscription (Paddle's portal). After the first save in a browser, a
player without Cloud Saves gets one message about them.

**Payments (Buy Me a Coffee).** Cloud Saves are sold as a membership on Buy Me a Coffee, paid by the month or by the year. Its webhook
(`/api/coffee/webhook`, signed: `x-signature-sha256` is the hex HMAC-SHA256 of the body with the webhook's signing
secret, `billing.mjs`) names the supporter's email; the server keeps the membership by that email
(`members/<sha256 of the email>.json`: the email, the membership's id, level, state and the end of its period; no
name, no amount), so a player may join before signing in. The account whose Google email is that email has Cloud
Saves while the membership runs: an active one to the end of its period plus three days (for the renewal's webhook),
one canceled for the period's end until then, a paused or canceled one none. Its level counts as a site level
(`CM_LEVELS`, "Levels and early access" below): Squad Player and up have Cloud Saves. An older event of the same membership than the last one (a retry) changes nothing; a test
event of the dashboard (`live_mode` false) is only logged. A player who joined with another email: give Cloud Saves on
the admin page. An account with a running membership cannot be deleted until it is canceled. In Buy Me a Coffee:

1. Memberships: turn them on and make the levels (for example Supporter, Squad Player, Club Legend); the monthly price
   of the Squad Player level goes in `CM_PRICE`.
2. Integrations, Webhooks: a webhook `https://<your-app>.fly.dev/api/coffee/webhook` with the events
   `membership.started`, `membership.updated`, `membership.cancelled` and `membership.paused`. Keep its signing secret.
   Send test event: `fly logs` shows `Buy Me a Coffee test event …: the signature holds`.
3. The settings below (`BMC_*`, `CM_COFFEE_URL`, `CM_SELLER`, `CM_CONTACT`).

**Payments (Paddle, older subscriptions).** With `BMC_WEBHOOK_SECRET` set, the page sells memberships only; Paddle's
webhook and Manage subscription keep working for the subscriptions bought through it while `PADDLE_*` stay set.
Paddle is the merchant of record: it sells the subscription, takes the payment and the sales
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
| `CM_PRICE` | the monthly price of the Squad Player level on Buy Me a Coffee, which the page and the pricing page name (default `$5 a month`). No page names another level's price, and the terms name no amount: each level's price is its own there |
| `PADDLE_CLIENT_TOKEN`, `PADDLE_PRICE_ID`, `PADDLE_WEBHOOK_SECRET` | Paddle's checkout: the client-side token, the price id (more than one, comma-separated, are all taken; the first is sold), the notification destination's secret |
| `PADDLE_ENV` | `sandbox` for Paddle's sandbox; unset for real payments |
| `PADDLE_API_KEY` | for Manage subscription; without it the button leads to paddle.net |
| `CM_SELLER`, `CM_CONTACT` | your name and contact email, on the terms, refund, privacy and pricing pages; the name also ends the footer "Made with ♥ by …" of those pages and of the game's page (a strip under the game; not in full screen or on a short screen) |
| `CM_YOUTUBE_URL` | the YouTube channel of the how-to videos (an `https` address of youtube.com or youtu.be): "Video guides" in the game page's footer and a line in Help |
| `CM_UPGRADE_URL` | without Buy Me a Coffee and Paddle: where "Get Cloud Saves" leads |
| `BMC_WEBHOOK_SECRET` | the signing secret of the Buy Me a Coffee webhook; with it (and a membership page) Cloud Saves are sold as a membership |
| `BMC_MEMBERSHIP_URL` | the membership page (`https://`); default `CM_COFFEE_URL` + `/membership` |
| `CM_LEVELS` | the Buy Me a Coffee level names (or ids), lowest first, comma separated, any case: the first is Supporter, the second Squad Player, any later one Club Legend (default `Supporter,Squad Player,Club Legend`); a level not in it counts as Supporter |
| `BMC_CLOUD_LEVELS` | the older setting, read only without `CM_LEVELS`: its levels are Squad Player, any other Supporter (the log says `CM_LEVELS` replaces it) |
| `CM_CLOUD_MAX_SAVES` | the most saves an account keeps in the cloud (default 30) |
| `CM_CLOUD_MAX_SETS` | the most data sets an account keeps in the cloud (default 5) |
| `CM_DIRECT` | `off`: saves and data sets go through the server, not straight to the store; `on`: straight, with the bucket's CORS rules set in the store's console (the server does not set them) |
| `CM_CORS_ORIGINS` | the addresses the bucket's CORS rules let in, comma separated (default `CM_PUBLIC_URL` and the fly.dev address) |
| `CM_UPLOAD_BUDGET_MB` | the data set uploads the server holds in memory at once, through the server (default 160) |
| `CM_CLOUD_MAX_PACKED_MB`, `CM_CLOUD_MAX_RAW_MB` | what one cloud save may be, compressed and as it is (default 400 and 1024) |

A good start prints `accounts and cloud saves: on`. Each save is gzip-compressed by the browser; the server checks that
it is a saved game of CM 01/02 (400 MB compressed at most, 1 GB as it is; `CM_CLOUD_MAX_PACKED_MB` and
`CM_CLOUD_MAX_RAW_MB` set others) before it stores it. A save that comes through the server goes to a file on its
disk (`spool.mjs`), is checked from there and streamed to the store, so the machine's memory holds none of it (at
most two of the largest on disk at once; a third waits for the next sync). The limits were 64 MB and 256 MB while the
server held each upload in memory; long careers make larger saves (a player's was 446 MB). The saves list tells
the page both limits (`maxPacked`, `maxRaw`), so a larger save is not sent at all: it stays in the browser, Saved games
says "Too large for the cloud", a message says so once, and the sync goes on with the other saves. It goes again only
when it changes. Before (a player's log, 15 November 2026: "Hertha BSC 2011-12-11.sav up: HTTP 413"), the 413 stopped
the whole sync, so the saves after it never went up, and the failed sync packed and sent that save again each minute.

## Edit a saved game

Saved games has an **Edit** button by each save. It opens the save editor (`editor.js`, `savedit.js`) in **Squad Hub**,
the page's name for the Editor since it also shows and scouts players (its bar button, its title and Help; once a save
is open, the title is the save's name and Players and Scout are its tabs):

- Search a player by name or club; the best players come first.
- Change current and potential ability, date of birth, the twelve position ratings, the 31 attributes the game shows
  (Technical, Mental, Physical, Goalkeeping, with Determination) and the hidden ones (Consistency, Important Matches,
  Injury Proneness, the feet, One On Ones and others). A change counts as soon as its field is left (Tab, Enter or a
  tap elsewhere), with no Apply to find at the end of the form; **Undo changes** beside the name puts the player back
  as he was shown, and **Save the game file**, shown once there is a change, writes them. A new player of a data set
  has **Add this player** and **Clear** beside his name instead.
- In a saved game, also what the GK Save Game Editor changes of a player (`savedit.js` `readMore`, `writeMore`;
  docs/research/2026-11-26-player-editor-fields.md): Player details (nationality, second nationality, caps,
  international goals, transfer value, club first-team and reserve squads; the national squads and the European
  registration shown, as the game sets them), Reputation and morale (home, current and world reputation, morale),
  Preferences (`preferences.dat`, the staff row's +0x65: three favourite and three disliked players or staff, and of
  clubs; a box offers the names that match), Personality (Adaptability, Ambition, Loyalty, Pressure, Professionalism,
  Sportsmanship, Temperament: staff +0x56..),
  and Contract (`contract.dat`: weekly wage, squad number, start and end, goal, assist and clean-sheet bonuses, the
  release fee and the five release clauses, transfer listed by club or by request, listed for loan, squad status).
  Also, as GK's other tabs (docs/research/2026-11-26-condition-retirement-fields.md): the playing condition (`injury.dat`:
  a percentage is full fitness and that match condition; Exhausted or Lacking match practice, words the game works
  out from fatigue, set its fatigue), the retirement state shown (general.dat's staff state; not changed: retire.dat
  would change length), Leaving on a Bosman, the release clauses' active state, played and missed in a row, the
  grievances (contract +0x45) and those about the club (+0x40) bit by bit, and an existing loan (the staff state's
  loan contract: wage share, dates, cup matches, against the parent club, status). These members are read when a
  form first opens (`SaveGame.loadMore`), not when the save does, so a large save opens as quickly as before.
  Only what changed is written; the wage and the contract end go to the contract and to the staff row's copies; a new
  nationality takes the player out of his national team. The game moves morale, reputations, value and squad
  numbers itself as the season goes on. A member's; a feature of `features.js` (`player-details`): Club Legends first.
- A saved game's player (for a member) and a member of staff are shown in tabs, as the GK Save Game Editor's: a
  player's Details, Attributes, Personality, Contract, Unhappiness and Loan; a member of staff's Attributes, Details,
  Personality, Contract and Unhappiness (a tab only when it has something). The tab chosen last stays chosen from
  one player (or member of staff) to the next. Every form of Squad Hub (players, staff, clubs) uses one field system
  (index.html, "The fields"): a field is its words and its control in a row, the control's width says its kind (a
  1–20 rating, money, a date, a choice), and ticks are a group of their own, the box before its words. Money shows
  with the language's separators and is plain digits, all selected, while it is typed in; a tick row is 44px high on
  a touch screen. The 1–20 colours are tokens (`--band-*`), which Red for the best sets anew. Another tab of Squad
  Hub (Players, Scout, Staff, Clubs) empties the right-hand panel. Under a player's + Shortlist and + Scout, a line
  says whose they are ("For Klaus Kastelmann"); with two or more human managers it is a choice of them, the same
  choice as the Scout's Manager list (kept for the save).
- The values are coloured on the game's 1–20 scale (low, middling, good, high; a position only where he plays): green
  for the best, or, by the viewer's choice above the attributes (kept in the browser; `features.js` `attribute-colours`,
  Club Legends first), red for the best with the low end grey. And
  the attribute groups stand side by side on a computer, as on the game's profile screen. On a phone the head is one
  line (the file's name, Save, Close) and a player's form has one way back, to the list.
- The numbers are those of the game's player profile. The game keeps 18 attributes relative to current ability (the
  technical ones, Anticipation, Decisions, Positioning): the editor turns them into the right bytes, and keeps them
  when the ability changes. A rating that the game cannot show with that ability (Heading 3 at ability 195, for
  example) gets the nearest one, and the editor says so.
- The save is written under its own name, so Load Game lists it; the file as it was goes to `backups/` as "<name>
  (before edit).sav" (Saved games, Older copies). With Cloud Saves on, the edited save goes up as any other save.
- With the vault (Where the saves live), a large save is not read whole: the editor reads the save's header and the
  nine tables it edits (a few MB) from the browser's storage, and writes the new file as a Blob of the parts it did not
  change (slices of the old file, which the browser copies on disk) and the tables it changed. The older copy is a copy
  of the save's record (`vault.copy`), so it is not read either, and the save keeps its name all the while (when it was
  renamed away first, a cloud sync in between could bring the cloud's version back over the edit). Before, a save of
  690 MB was read whole and written whole, three copies in the tab's memory. Without the vault (the Asyncify engine)
  the save is in memory anyway, and the editor copies it as before.
- If the save is loaded in the running game, save the changes in the editor and then load the save again: the game
  keeps its own copy until it loads.

Contracts, clubs, names and reputations are not edited, and a saved game gets no new players. The facts, with the
game's code addresses: `docs/research/2026-11-10-save-editor.md`.

## Levels and early access

Every account has a level (`levels.mjs`), each with everything below it: **Free** (what everyone gets), **Supporter**
(the same, as a thank-you), **Squad Player** (Membership: Cloud Saves and the full Scout) and **Club Legend** (Squad
Player, and early access). A player with no account is Free. The level is the highest of: the running Buy Me a Coffee
membership of the account's email (its level through `CM_LEVELS`), a Paddle subscription (Squad Player), a gift of the
admin page (Squad Player or Club Legend; `grants/<id>.json`, one kept before levels is Squad Player), the server's
lists (`CM_CLOUD_FOR`, `CM_CLOUD_USERS`, a plan set by hand: Squad Player) and being an admin (Club Legend, so the
admin sees what Club Legends see). `/api/me` says `user.level` (`free`, `supporter`, `squad`, `legend`); `user.cloud`
is Squad Player and up. The account dialog names the level. The admin page's People shows it and where it comes from
(paid, gift, admin, server list), with Give Squad Player, Give Club Legend and Take back; Members shows the level each
membership counts as; Overview splits the members by level.

**Early access** (`early.mjs`, `early.js`): a new feature or a new Library database is for Club Legends only until its
day, then for everyone under its usual rules. A day is a calendar day and opens at its 00:00 UTC. Until then everyone
else sees the thing dimmed where it would be, with "Club Legends have it now · everyone from {date}", Sign in (signed
out) and Become a Club Legend (the membership page); a Club Legend sees an **Early access** badge. `/api/me` says
`early: { legend, features: { key: day } }` (only what is still early) to everyone, a server with no accounts too.

- **A Library database**: the admin page's library form has "Club Legends only until", 14 days ahead by default, empty
  for everyone at once. Until that day `/api/library` lists it with `early` and, for a player below Club Legend,
  `locked`; its zip and its download link answer 403 with the day. Its picture stays public.
- **A feature of the page**: building one adds its line to `web/features.js` (`{ key, name }`), and its entry point asks
  `earlyState(account.me, key)` (`early.js`): `open`, `early` (draw `badgeHtml()` beside it) or `locked` (draw
  `lockedHtml(day, { signedIn, offer })` in its place). A feature with a route of its own checks the level there too
  (`cloud.planOf(user).level === 'legend'` while `isEarly(day)`); one that lives only in the page is gated in the page
  only: early access is a perk, not a secret. The server gives a key it sees for the first time 14 days
  (`early/features.json`), so no feature waits for someone to remember it; a key whose day cannot be read counts as open.
- The admin page's **Early access** tab lists the features and the databases still early with their day: change it
  (Save) or **Open now**; those already open are listed below. The routes: `GET /api/admin/early`, `PUT
  /api/admin/early { key, day | null }`.

## The Scout

A saved game open in Squad Hub (the Editor) has two tabs over its list: Players (the search, as before) and Scout (`scout.js`,
`editor.js`). The Scout finds the save's players as of its last save by age (from–to), a position (a rating of 15 or
more there), the lowest potential (PA) and ability (CA), and up to three attributes with the lowest value each, on
the 1–20 scale the game shows (savedit.js's `read`: the game's own ratings, 0x543540). The matches come best first
(PA, then CA, then name), at most 100; one opens the player's form, and Back on a phone returns to them with the
filters kept. The players are read once when the tab opens (60,000 in about 200 ms in Node) and a search takes a few
milliseconds; it runs 250 ms after the last change of a filter. A change of a player reads only him again
(`scoutUpdate`). The two tabs are tabs to the keys too: the arrow keys, Home and End move between them (through
keyboard.js's `onPageKey`, since its guard keeps the page's keys from the elements' own listeners). A data set has no
Scout.

**Presets.** Wonderkids (`scout.js`, `WONDERKIDS`): 19 or younger with potential 180 or more, the other filters cleared
(John Locke's School search of CM 01/02).

**To the shortlist** (`shortlist.js`; docs/research/2026-11-25-shortlist-in-save.md). A star beside each Scout result,
and "+ Shortlist" in the player's panel, put the player on a human manager's Short List in the saved game, or take him
off. "Manager" chooses the manager when the save has two or more (for both buttons, the shortlist's and Scout's); the choice is kept for the save
(localStorage `cm0102-shortlist-manager:<save>`). The save's `scout_manager.dat` holds ten lists for each of 16 human
manager places, list 0 the Short List; the entry is built as the game's `add_player` 0x7af720 builds it, the list kept
in staff id order with its capacity at least its count, and a removal sets the removed bit as the game does. A change
counts as a change of the save and goes with Save the game file (the writer then records the member's new size).
Members only, as the full Scout; a free player's star opens the Account dialog. A feature of `features.js`
(`shortlist`): Club Legends first.

**Clubs** (`savedit.js` `clubInfo`, `clubFinances`, `writeClub`; docs/research/2026-11-26-club-finances.md,
docs/research/2026-11-26-club-editor-fields.md). A fourth tab of a saved game: search a club, pick it, and the
right-hand panel shows and changes what the GK Save Game Editor changes of a club, in its groups: General
(professional status, training facilities, reputation, Euro seeding, public limited company; a linked club shown, as
the game sets it), Preferences (three favourite and three disliked players or staff, three rival clubs: a box offers
the names that match what is typed), Kits (the six colours of colour.dat, with their colour), Finances (balance,
highest wage allowed, bankrupt), Stadium (capacity, seating, expansion, owns stadium, roof, undersoil heating, city
attraction, average, minimum and maximum attendance, home match day). Below, the club's finances as `finance.dat` keeps
them (359 bytes a club): the balance, the weekly wage bill and top wage, and this year's and last year's players sold,
prize money, TV money, players bought, wages, bonuses, signing-on fees, other costs and league fines. The game shows
them for the player's own club only. Each change is written at once (`writeClub`, cut to the game's ranges; an id that
names nothing changes nothing) and saved with the game file. The game keeps no transfer budget: it works it out from
the balance (`Finance::get_transfer_budget` 0x598800), so a larger balance is a larger budget; it works out the
highest wage allowed and bankruptcy again in its own finance runs, and the Euro seeding at each season. A stadium and
a city can be shared by clubs: a change applies to all of them. A member's (a free player sees the Membership
invitation). A feature of `features.js` (`club-finances`): Club Legends first.

**Staff** (the tab was Managers; `savedit.js` `managerAttributes`; docs/research/2026-11-26-manager-attributes.md). A third tab of a saved
game: each human manager (the named places of `scout_manager.dat`) with the twelve attributes the game keeps for a
non-player and hides for the human managers (the HumanManagerAttributes patch of CM 01/02 showed them): Coaching
Goalkeepers, Coaching Outfield Players, Physiotherapy, Working With Youngsters; Adaptability, Determination, Level of
Discipline, Man Management, Motivating; Judging Player Ability, Judging Player Potential, Tactical Knowledge. Rated
as the game rates them (0x543fd0: as stored, or relative to the manager's current ability). The list starts with
the human managers; its search finds any manager or coach of the save (`nonPlayers`: every staff row with a
non-player record). The details open in the right-hand panel and can be changed: current and potential ability
(1..200) and each attribute (1..20), stored back as the game reads them (`writeManagerAttributes`, the inverse of the
relative rating) and saved with the game file. A member's,
as the Scout's potential; a free player sees the Membership invitation. A feature of `features.js`
(`manager-attributes`): Club Legends first. The tab is called Staff: its search finds any member of staff of the save by name or club, and its Job list
(`STAFF_JOBS`, the names of `database_get_job_name` 0x53ad0f) shows one job's staff, best first by current ability:
the save's scouts, physios, chairmen and the rest; a row shows the club and the job.

Like GK's Edit Player/Staff screen for a non-player (docs/research/2026-11-26-manager-editor-fields.md): his home,
current and world reputation (nonplayer.dat +0x08, +0x0a, +0x0c) and preferred formation (+0x43); the bytes the game
keeps hidden, as a playing style (Attacking, Directness, Free Roles, Marking, Offsides, Pressing, Coaching Technique)
and as a chairman (Business, Interference, Patience, Resources); and the player form's Details, Personality and
Contract (a contract only where the save holds one: most database managers and chairmen have none). Only a value that
changed is written. The game lowers reputations day by day and gives a new chairman new board values.

**Nationality** (`savedit.js` and `dataset.js` `nationNames()` and `search(text, limit, nation)`, `scout.js` `nation`). A
Nationality list beside the Players search (of a save and of a data set) and a Nationality filter in the Scout: the
nations of the data's players, by the names the data has. A feature of `features.js` (`nationality`): Club Legends
first; while it is early for a player, neither list shows. On a phone the list takes its own line.

**Scout a player** (`shortlist.js`; docs/research/2026-11-25-scout-request-in-save.md). An eye beside the star, and
"+ Scout" in the player's panel, ask the chosen manager's club to scout the player, as the game's own "scout him" does;
a second tap takes the request back. The requests follow the searches in `scout_manager.dat`: 17-byte records (club,
player, the day asked, reports sent) as `add_scout_request` 0x7f9290 builds them, kept in the game's order (player, then
club), which its binary search needs. The game then waits 30 days and has the club's scouts send up to three reports
as news. No button for a player already at that club, or for a manager without a club: the game would drop the
request the next day. Members only; a feature of `features.js` (`scout-request`): Club Legends first.

The Scout is part of Membership. A member sees the best 100 matches; a free player the first 3, then blank shapes, the count
("The Scout is part of Membership: 44 more players match.") and the Account dialog's own button (Sign in, Become a
member, or Sign in with Google, free; none when the account is not known, offline at a first visit, where the
Account dialog would be empty). The held-back players are never put in the page, and the Players list is
emptied while the Scout shows. An open Scout follows the account: a player who joins sees every match at once. A
member is whoever `/api/me` names with `user.cloud`; the last answer is kept in
localStorage (`cm0102-member`), so a member offline keeps the whole Scout. Where no one could pay (no accounts, or
nothing to buy, as on a server that gives Cloud Saves to a list of emails) everyone has the whole Scout. The limit is
the browser's: the save lives there, and the server is not sent saves to scout them.

## Edit a data set

The **Editor** button in the bar opens the editors: a saved game (as above), or a data set for new games.

- **New data set** copies the game's own database (the 20 files, about 60 MB) into `Databases/<name>` in the user
  folder. Start a new game and choose it under Database: the game lists such folders as it lists Update Team data
  sets. The game's own data stays as it is.
- Search a player, then change his names, nationality, club, contract end, ability, positions, attributes and
  Determination. The numbers are those of the database: 0 to 20, where 0 means the game sets the attribute when a new
  game starts (most players of the data have 0s).
- Dates as the game uses them: an Update Team database keeps them years early (a player born in 2007 is in the data
  as born in 1982), and the game moves them when a game starts. The editor measures that shift as the runtime does and
  shows the real dates (born 2007), with a note; it writes the data's own dates back
  (`docs/research/2026-11-10-data-set-editor.md`, "Dates of an Update Team database").
- **Add a player** to any club, or as a free player. A club takes 50 players at most.
- **Save the data set** writes the changed files; the data set's other files stay.
- The Editor's home lists the data sets, with Delete.

**Database library (`library.mjs`).** Databases that the admin offers every player, free and with no account, each
for a season: the Databases tab of Saved games lists them under "Available to download" (name, season, size, note), and **Get** brings one
into Databases with its season noted, straight from the store where the pages go direct (else through the server);
**Use** then opens with that season. A database from the library is marked in its folder's note (`.web.json`:
`library: { id, mtime }`): while its files are as they came, the Cloud column says "From the library" and the data sets'
cloud sync leaves it out (any device gets it from the library again, and it takes none of the player's 5 places); an
edit makes it the player's own. The admin page's **Database library** section adds one (a zip of the 20 files, as
Download gives it; its name, season and a note), changes those or removes it: the zip goes straight to the store with a
signed link (else through the server) to `library/incoming/`, and the server lists it only once it holds the 20
database files (`inspectSetZip`, at most 128 MB). The store keeps `library/index.json` and `library/<id>.zip`. Routes:
`GET /api/library`, `GET /api/library/<id>` (`?direct=get` for a link), `GET /api/library/<id>/card`; for the admin
(`CM_ADMIN_EMAILS` or the token) `GET|POST /api/admin/library`, `PUT …/<id>/body`, `POST …/<id>/done`,
`PATCH|DELETE …/<id>`, `POST /api/admin/library/cards`. It needs the bucket.

**Title cards (`rgn.mjs`).** An Update Team release brings its intro pictures (`logo.rgn`: the release's title card,
800x600, "CM0102 … October 2024 Data Update"). When a database comes into the library, the server reads its
`logo.rgn` (else `splash.rgn`; the game's RGN format: a header of 12 numbers, then 16-bit pixels, `src/si/rgn_loader.c`),
makes it 200x150 and keeps it as `library/<id>.png` (about 50 KB); the list says `card: true`, and the library's list
shows it beside the name (64x48, 80x60 on a phone), or the season in a box of that size. The admin page's **Make their
pictures** reads the zips of the databases added before the cards, once. Only on the page: the game never reads a data
set's pictures (Q205, `src/recomp_rt/win/files.c`), since its intro allocates by each picture's colours, and a card in
place of the 520x520 logo of 46 colours would change the game's later days (Q152, `tools/release/make_intro_logo.py`).

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
- A long job of the game shows in the bar with its share and time once it has run 2 seconds, for example "Player
  histories 45% · 0:38" while a new game is made (`progress.js`; the engine logs `recomp: progress: <stage>` when a
  stage starts, and `rt_web_progress` gives its share, display.c). A player of a new game on a phone waited minutes on
  one stage and thought the game had stopped.
- A tab that the system closes while the game loads or runs (iOS for its memory, then "A problem repeatedly occurred
  on …", an iPhone, 17 November 2026) is found at the next visit (`lastvisit.js`): the page keeps a mark in
  localStorage with its step ("downloading the game files", "Loading game data", "in the game") and takes it off when
  it closes or stops on its own. A mark that the next visit finds (and no other tab plays) puts a line in the start box,
  "The game stopped suddenly on your last visit … most likely … too much memory", with an iPhone's own advice; the log
  and the statistics (a report "sudden stop") have it too.
- A start that stops on an error before the game loads the page again once, then shows "The page did not start" with
  the message. A page that the service worker gave from its copy (a server slow to wake) gets the copy's scripts too.
- The browser asks before the page is left while a game runs, and the screen stays on.
- The second start needs no click: when the game files are in the browser, the game starts by itself.
- **A new version (`release.js`).** The server names its release at `/version.json` (and in `files.json`): a hash of
  the page files' names, sizes and times and of the engines' versions, so every machine of a deploy agrees, and a
  deploy that changes neither keeps it. The page asks every 10 minutes and when it comes back into view; when the
  release changed, a New version button shows in the bar, with one message. It loads the page the way Restart Game
  does (the saves stored first; with a game in play the page asks first). The page never loads itself: a player in the
  middle of a match decides when.

### Look and feel

One system for every part of the page (`index.html`, its `:root`), from the UI skills in `.claude/skills/`
(ui-ux-pro-max's priorities, the taste skill's "one system", Emil Kowalski's `mobile-native` and
`emil-design-eng`, impeccable's audit):

- **Tokens.** Colours, the hover shades (`--line-hi`, `--raised-hi`, `--accent-hi`), one radius scale (`--r-sm` 6,
  `--r` 8, `--r-lg` 12, `--r-xl` 14) and one ease (`--ease`, 140 ms ease-out). Text is 5.4:1 on its ground at the
  least (WCAG AA is 4.5).
- **Every control answers.** A lighter face on hover, only with a mouse (`(hover: hover) and (pointer: fine)`: a tap
  leaves no hover stuck); a press at 0.97 when the finger lands; a faded, not-allowed look when disabled; a green ring
  for the keyboard on every control (`:focus-visible`), and a green line on the field that has the focus. Delete is a
  link that turns red under the pointer, so the row's buttons stay the safe ones.
- **A finger's target is 44px** (Apple's 44pt) where a button is drawn smaller: an invisible margin round it on a
  touch screen; the tabs are 44px tall there.
- **The tabs of Saved games** are a WAI-ARIA tab list: Left and Right, Home and End move between them, one of them is
  in the Tab order, each section is its tab's panel. The handler runs on the window before `guardKeys`, which stops
  every key of an open dialog there so that the game gets none.
- **Less motion asked for** (`prefers-reduced-motion`): nothing slides, pulses or fades in; the long-press ring and
  the busy mark stay, as they tell the state.
- **On a phone held upright** (under 640px) each row of a list is a card: its name, then its size, date and place,
  then its buttons, all on the screen; the lists do not scroll on their own, the dialog does. The columns of a table,
  scrolled sideways, hid Play, Download and Get past the edge (the screens review, `test/screens.mjs`).
- **On a phone held sideways** (landscape, 520px high or less) the bar is a column of icons at the right edge, not a
  row above the game: the game has the screen's whole height. An iPhone showed the bar above taking an eighth of the
  height while the room beside the game stayed empty (21 November 2026); on a phone of 814 by 380 the game is now 608
  by 380 instead of 528 by 330. Full screen keeps its own place for the buttons (`layout.js`, barPlace).
- **Zoom stays off** (`user-scalable=no`), against the usual rule, on purpose: two fingers on the game are its mouse
  wheel and its right click, and a page zoomed by a pinch would leave the game half out of view. Every field is 16px,
  so no browser zooms into one either.

## The first-visit tour

Once in a browser, a moment after the game has started, a card beside each of the page's own buttons says what it is
for (`tour.js`): Saved games, Cloud Saves (the Sign in or account button), Editor, Keyboard and Right click (on a touch
device), Full screen (where the browser offers it: not on an iPhone) and Help, with Back, Next or Done, and Skip the
tour. The Cloud Saves card says what they do (each save up right after it is made, the other devices get it when they
start) and how to turn them on from here, in the Account dialog's own words for the account as it is: sign in with
Google, then "Become a member" on Buy Me a Coffee with the same email or "Get Cloud Saves" with Paddle, at the
site's price; or that they are on, or free (`cloudStepText`). The button it is about wears a green ring. New players arrive from Facebook groups and the video guides and see
a row of icons around the game (on a phone, icons only; held sideways, a column), and one could not find the Editor.
The card sits below its button when the buttons are a row and beside them when they are a column, never over one of
them; it does not hold the game, and Escape ends it while it has the focus. Help's "Show the tour" runs it again. A
step whose button is not shown is left out (Right click on a computer; in full screen with the buttons folded away,
none: the tour then waits for a later visit). The browser keeps `cm0102-tour-seen` in localStorage once the tour has
ended; a private window keeps nothing and shows it at each visit. `test/tour-check.mjs` runs it on a laptop, a phone
held upright and one held sideways.

## Languages

The page around the game speaks English and the nine languages of the game: Spanish, Portuguese, Italian, French,
German, Dutch, Danish, Norwegian and Swedish (`i18n.js`, the tables `i18n-<code>.js`). The game keeps its own
Language setting; the page does not change it. The language is the one chosen in Help (Language), else the browser's
first one the page speaks (`pt-BR` is Portuguese, `nb` and `nn` Norwegian), else English; `?lang=de` picks one for a
link. The choice is kept in localStorage (`cm0102-lang`). A change takes effect at once, with no reload: the open
dialog, the bar, the tour card and the start box are drawn again, and the game keeps running. Only the chosen table
is loaded.

The English is the key: `t('Saved {name}', { name })`, `tn(n, { one: '{n} save', other: '{n} saves' })` for a count
(each language's plural rules, `Intl.PluralRules`), and in `index.html` `data-t` (the text), `data-t-html` (a
sentence with a link or bold in it) and `data-t-attr="title,aria-label"`. A text missing from a table shows in
English, so a new feature never breaks a language. The modules the server imports too (`zip.js`, `savedit.js`,
`dataset.js`, `saves.js`, `upload.js`) never import `i18n.js` themselves (`tactics.js` does, and does nothing with it
on the server): their errors carry `key` and `vars` beside the English
message, and Saved games gets the page's `t` and `tn` when it is mounted. What stays English: the engine log and the
report to the developer, the server's own answers (its error texts, library names), save and file names, and the
legal pages, whose links say "(English)" in the other languages. The price from the server (`CM_PRICE`, "$5 a
month") shows its period in the page's language when it has that shape (`priceText`).

The tables were translated by machine; Help says so in the other languages and offers Send to the developer for a
better word. To change a word, edit its entry in `i18n-<code>.js`; to add a text, write it through `t` and add it to
the nine tables: `node web/test/i18n-delta.mjs [out.json]` lists the texts the tables lack and the entries whose English
left the page, `test/i18n-brief.md` is the brief for a translator (a person or a helper agent), and
`node web/test/i18n-table-check.mjs <code>` checks one table (missing, stale, placeholders, markup, plural forms). Push
the English and the tables together: the English alone leaves stale entries, and CI fails (the skill `web-languages`
has the steps). `node --test web/test/i18n.test.js` (and CI) fails on an entry whose English is no longer on the
page, a wrong `{placeholder}` and a plural without every form of its language, and lists the texts missing from a
table; with `I18N_STRICT=1` a missing text fails too. `test/i18n-check.mjs` chooses the language by `?lang=`, by the browser
and in Help (with a dialog open), and sweeps the start box, the bar, the tour, Saved games, Account, Help, two toasts
and a refused upload with a stand-in table that marks every text: any English left outside the marks fails.
`test/screens.mjs <folder> de` takes every screen in one language on a laptop and a phone held upright and sideways,
and fails when a label, a tab, a dialog button or the tour card is cut or leaves the window. The statistics count
the people of the last 30 days by their browser's language (Statistics and the admin panel).

## On an iPad or another touch device

| Do | The game gets |
|---|---|
| Tap | A click |
| Drag | The button held and moved: scroll bars, the tactics board |
| Right click (the button in the bar), then a tap or a drag | The right button for that touch: a tap is a right click, a drag a right-button drag |
| Long press (a ring fills under the finger), then lift or drag | The right button, down when the ring fills and up when the finger lifts: a right click, or a right-button drag |
| Tap with two fingers | A click of the right button |
| Drag with two fingers | The mouse wheel: lists scroll with your fingers |

The game needs the right button for much of its tactics: a right click on a player, in the list or on the board,
opens his instructions (Passing, Tackling, Forward Runs, Marking and the rest). A long press is that click, but a
finger that wanders more than 12 pixels before 450 ms makes it a drag, and on the board a drag moves the player. On a
small picture (a phone) the limit is 10 of the picture's pixels, at least 6 on the screen: the rows of a list are a few
pixels high there, and a drag onto the next row must be a drag.

A substitution, or a swap of two players, is a drag of one player onto the other in the list (the drop at 0x8a1d22
calls `Tactics_Screen::swap_slots`, 0x8ab7e0). The game reads the mouse once a frame: its pump (0x68d520) handles
every message that waits, and the place it then uses is that of the last move (WM_MOUSEMOVE, 0x68d922; a button
message carries none). The page sent a drag's press and the finger's moves together, so the game read the press where
the finger had got to, two or three rows on. A drag that then starts and ends on the same player is a click on him, and
his page opens: a player on an iPhone could not make a substitution that way (21 November 2026). Now a press made by the page stands alone for
80 ms (`CLICK_MS`): the moves and a lift that come in that time wait and go after it.
The Right click button (`touch.js`, armRight) has no timing: tap it, then tap the player, and that one tap is the
right click; the button turns green while it waits and is off again after it. The long press shows a ring that fills
under the finger and, on Android, the phone buzzes when it becomes the right click. Both opened a player's
instructions on the real tactics screen with touch in Chromium (November 2026).

A player's run (the dashed arrow on the board) is a drag with the right button: the game drags a widget of flag 0x100
while the right button is held (`FUN_00561210`, `b67a38`; docs/research/2026-10-13-input-inventory.md 2.6). A player
on Reddit could not make one (14 November 2026): a long press and Right click then gave a right click only, the button
down and up at once. Now the long press puts the right button down when the ring has filled and lets it go when the
finger lifts, so a finger kept down drags with it; after Right click, the next touch is the right button's for its
whole length, a tap or a drag. On a Mac, Control and a click is the right button (a trackpad makes no other
right-button drag easily), but the browser reports the left button with Control, which the game took for a left
click: on the game, the page turns it into the right button from the press to the release (`mountMacRightButton`,
`touch.js`). The game reads no Control with a click, so nothing is lost. With a mouse the right-button drag worked
already (a run of the real engine in Chromium: the dashed target while held, the run after the release).

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

Checked on the real engine (14 November 2026): one start with a connection, then the browser offline: the game started
in 4 seconds, its 467 MB of files all from the browser, and the save made before was there. The page check does the
same with the stand-in engine. Without a connection, Cloud Saves, sign-in, Send to the developer and the statistics
wait; the saves of the flight stay in the browser, and Cloud Saves send them once the connection is back. The browser
must have room for the game files (a browser that refuses them, as one with a full disk does, has nothing to start
from), and Safari deletes a site's files after 7 days without a visit unless the page is on the home screen.

## Browser requirements

The page checks the browser before it downloads anything, and stops with the reasons when the game cannot run:
no WebAssembly; no JSPI, when the server has no Asyncify build (Other browsers); or no room for the engine's memory (the page tries the engine's own plan:
768 MB, grown to 1,856 MB, with a 4 GB maximum). After Start it compiles the engine once, before the game files
download, so a browser that lacks a WebAssembly feature of the engine stops there. A device with no mouse, or a
browser that keeps no data (a private window), gets a warning but may start.

## Other browsers

The engine of step 4 needs JSPI, which Chrome 137 and later have, and Safari 27 and later (WebKit, WWDC26): so an
iPad or iPhone on iPadOS or iOS 27 runs it in every browser, Chrome included (all of them are WebKit inside). Older
Safari, and every browser of an iPad on iPadOS 26 or before, need a second build of the engine, with Asyncify in place
of JSPI. The server serves both, and each
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
  sleep, and a short one at least every 12 ms while the game polls for input (`rt_web_yield`). Inside the game's long
  work, where it polls for nothing, the page gets a moment every 100 ms (`rt_web_breathe`) from two hooks: the
  random function (0x90cfc0, which a day's processing calls without end) and the steps of the game's sorts: the swap
  of the C library's qsort (0x9454d5) and the two comparators of `PlayerStats::PlayerStats` (0x7bfd50), which a new
  game runs for seconds while it sorts every player's history. It froze the page for 1.4 s and 0.7 s in Chromium;
  now 0.19 s at most (21 November 2026, `docs/superpowers/specs/2026-11-21-sort-breathe-design.md`). A third hook is
  the C library's fread and fwrite (0x945bfe, 0x945ce6, every 4th call): "Save Compressed" packs a save with the
  game's run-length packer zwrite (0x932e10) and unpacks it with zread (0x932c30), which call nothing else the page
  waits on. A 60 MB career froze the page 2.3 s while it was saved and 1.3 s while it was loaded, and a player saw
  Chrome's "Page Unresponsive" (22 November 2026); now 0.27 s and 0.16 s at most, the load as long as before.
  The AI's transfer work of a day (CONTRACT_MANAGER::daily_update 0x4c7ac0, the Shortlist_Manager filling every
  club's shortlist) also called the random function too seldom: a holiday of the same career had 10 to 21 tasks over
  0.3 s a minute, up to 1.3 s. Three more hooks count as sort steps: qsort itself (0x945333, any comparator),
  CONTRACT_MANAGER::get_contract (0x4cf5a0) and human_manager_is_human_club (0x600a50). The same holiday: none over
  0.3 s, the days as fast; a new game: none over 0.3 s.
- A hidden tab (another window in front, a minimised one): the game goes on. Emscripten's sleep waits with
  setTimeout, and Chrome runs a hidden tab's timers once a second, then after five minutes once a minute, so the
  game's days all but stopped until the player came back (22 November 2026). While the tab is hidden, the page's
  wrapper of the engine's sleep (`loader.js`, stoppableSleep) waits through a small dedicated worker's timer instead
  (`ui.js`, workerTimer), which the browser does not slow so; in view, the engine's own sleep runs. A run of the real
  engine with the tab hidden throughout (a new game and a holiday) took its usual time, and the game was the same
  (`-seed 7`). JSPI engine only; Safari on an iPhone or iPad stops a hidden tab whatever its timers do.
- Files: the page puts the game files into Emscripten's file system before `main` starts; the runtime's POSIX host
  (`host/posix.c`) reads them as on Linux. The page keeps the game files in the browser's cache storage, each under
  its version (its SHA-256 in the manifest), so a later start downloads nothing. With a JSPI engine, the files larger
  than 64 KB are placeholders that the game reads from the cache in pieces (The game files out of memory). The user folder `/user` is kept in
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
  cloud saves API (conflicts, limits, checks, account deletion) and the sync plan; Buy Me a Coffee's signatures and
  membership events (join, by email in any case, before the sign-in, cancel for the period's end, pause, a retry, a
  level without Cloud Saves, a test event, the account deletion); Paddle's signatures, the
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
- `NODE_PATH=$(npm root -g) node web/test/vault-check.cjs`: the vault alone in Chromium, with a real IndexedDB and a
  small file system (seconds): a save read in pieces, never more than 4 MB from the storage at once.
- `NODE_PATH=$(npm root -g) node web/test/cloud-check.mjs`: the accounts and cloud saves in Chromium, with stand-ins of
  Google, the S3 store (`test/fakes.mjs`, which checks every request's signature as S3 does), Paddle.js and the
  engine: the plans in the start box, the sign-in from the start box and from a small window while the game runs, the
  saves of the cloud before the game starts, a save going up, Remove, a save changed on two devices, a second device,
  sign-out, a free account that buys Cloud Saves while its game runs (checkout, webhook, the save goes up), and one
  that joins as a Buy Me a Coffee member in another tab (webhook, Cloud Saves on by themselves, Your membership).
  Needs Playwright.
- `CM_VIDEO_SAVE=<a .sav> NODE_PATH=$(npm root -g) node web/test/make-videos.mjs <folder> [editor|app]`: the two
  how-to videos for players, `editor.mp4` (Saved games, Edit, a search, a change, Save; Play, and the player in
  the game's Find with his new Pace; the Editor button, a new data set, Databases, Use) and `app.mp4` (the start; the
  game's main menu; Saved games: Upload, Download, Download all, Play, and the game where it was left; Game Options,
  Save Game; the Tactics tab; the Databases tab: Get a database from the library, Use; Full screen, Cloud Saves, Help,
  Add to Home Screen), 1920x1080 H.264. The page is recorded in Chromium (1280x800) on a real site, `CM_VIDEO_SITE`
  (default `https://cm0102-web.fly.dev`; or a local server with the game's files), with the real engine, so the game's
  own screens are in them; each film starts in a new browser profile. The page's scripts (`web/*.js`) come from this
  checkout (the service worker off), so a change to the page is in the films before it is deployed. `CM_VIDEO_SAVE` is a save of the real game,
  Juventus in 2001/02 (a new game with Italy, Juventus taken, saved on its first day): the films upload it. The waits
  (the game files' download, the game starting and its Game Settings, a save loading) are recorded but cut out of the
  capture. The game's own buttons are pressed at places in its 1280x800 picture (`AT` in the script), held 150 ms as a
  hand would. Each step's words and the box it points at go into a log, and `test/videos-compose.mjs` makes a
  HyperFrames project of it (`<folder>/<name>-project/`, the skill in `.claude/skills/hyperframes`): a title card, the
  capture in a frame with a caption under it for each step and a zoom on a small target, an end card; then renders it
  with the HyperFrames CLI (npm, pinned, its telemetry off). The music is a take of "Matchday Manager", a Suno track made for the films, in
  `test/video-music/` (one take for each film; its README says what the files are): it plays under the film from its
  start, at 0.6 (about -18 LUFS for a track mastered at -13.5), faded out over the end card's last 4 s.
  `CM_VIDEO_MUSIC` (or `CM_VIDEO_MUSIC_APP`, `CM_VIDEO_MUSIC_EDITOR`) names another file, or `none`. `node
  web/test/videos-music.mjs <folder> [editor|app]` makes the films again from their projects with the music, without a
  new recording. Make them again after the page changes.
- `NODE_PATH=$(npm root -g) node web/test/screens.mjs <folder> [code]`: every screen of the page on a laptop
  (1280x800), a phone (an iPhone 13) and a phone held sideways, for a review of the look: the start, the tour, the bar
  over the game, Saved games and its three tabs, Squad Hub (a save, its Scout, a player, its start), the account, Help,
  Send to the developer, the legal pages and the admin page. With a language code, the page in it. It fails when a
  label, a tab, a dialog button or the tour card is cut or leaves the window, and when a button's text is off its
  middle or the items of a row (a flex or grid line: a label and its box) are not level.
- `NODE_PATH=$(npm root -g) node web/test/tour-check.mjs`: the first-visit tour on a laptop, a phone upright and
  sideways: its steps, the cards beside their buttons and over none, Escape, the Membership step's words.
- `NODE_PATH=$(npm root -g) node web/test/i18n-check.mjs`: the languages: `?lang=`, the browser's, Help's choice with a
  dialog open, and the sweep (a stand-in table marks every text; any English left outside the marks in the start box,
  the bar, the tour, Saved games, Account, Help, the toasts, a refused upload and the Scout fails); a server with no
  library. The skill `web-checks` says which check to run for which change.
- `NODE_PATH=$(npm root -g) node web/test/editor-check.mjs`: the editors in Chromium, with a small saved game and a small
  data set in the game's formats (`test/fakes.mjs`). The save editor on a computer and a phone: Edit, search, change,
  a change that counts when its field is left, a rating out of reach, the colours, Save shown after a change, Save,
  the backup, a file that is not a full save. The data set editor: a name refused,
  New data set, an edit with a move to another club, Add a player, an unknown club refused, Save, Delete; the Data sets
  of Saved games: Use (through settings.txt and through the runtime call), Download, Upload of a zip and of the 20
  files, Delete. The cloud check adds a data set uploaded on one device that comes to the other.
- `NODE_PATH=$(npm root -g) node web/test/early-check.mjs [screenshot folder]`: levels and early access in Chromium on
  the demo site: People's levels (paid, a gift and both), Give Club Legend and Take back, the level chips, Members'
  levels and an unknown one, the Early access tab and Open now, the library form's day, the Overview split; then a free
  player's locked database (its day, no Get), unlocked without a reload when the player becomes a Club Legend (the
  badge, Get), and the account dialog's level.
- `NODE_PATH=$(npm root -g) node web/test/scout-check.mjs [screenshot folder]`: the Scout in Chromium on a saved game
  of ten players, with `/api/me` played by Playwright: a free player (3 matches, the rest held back and nowhere in the
  page, the invitation that opens the Account dialog), a filter, a match opened (and Back on a phone), a member (every
  match, and offline the kept answer), signed out (Sign in), and no Scout on a data set. `test/scout.test.js` has the
  filters, the access and the free cut, and a save of 60,000 players.
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

- The server hands out the base game, plus the databases the admin puts in the library (Saved games, Databases).
  The season databases of the desktop downloads (`Seasons/*.zip`) are not on it unless the admin adds them.
- The engine runs on the page's main thread. The concept design moves it to a Web Worker, with the new screens
  beside it.
- The touch and keyboard parts are checked in a desktop browser that plays a touch device, with the real SDL
  backend, not on an iPad itself: the device's own keyboard and Safari can still differ.
- The Asyncify engine (Other browsers) is checked with the stand-in engine only: a real build of it, its size and speed,
  and its memory on a real iPad are still to try.
