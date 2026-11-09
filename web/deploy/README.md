# Putting the browser build on Fly.io

The server only hands out files; every player's browser runs the game. One small Fly machine serves many players,
and it stops when nobody is visiting.

## Step by step (Mac)

Do `web/README.md` steps 1 to 4 first: the tools, the code, the game files and the engine build
(`build-web/cm0102_recomp.wasm` must exist).

**1. Fly's command line (once).**

```bash
brew install flyctl
fly auth signup            # or: fly auth login
```

Fly may ask you to add a payment card before it lets you deploy.

**2. Stage the files (each release).** From the repo:

```bash
cd ~/Projects/cm0102-web
node web/deploy/stage.mjs --game ~/cm0102-game
```

`--game` names the folder that holds `cm0102.exe` and `Data`. The game folder inside the app works too, without a
copy:

```bash
node web/deploy/stage.mjs --game "/Applications/CM 01-02 Bosman Edition.app/Contents/Resources/game"
```

The stage checks every game file against `tools/clean_files/manifest.tsv` (size and SHA-256), copies the server, the
page, the engine, the season tables and the game files into `web/deploy/stage`, writes a brotli copy of each file
that compresses, and adds `Dockerfile` and `fly.toml`. It ends with:

```
764 files, … MB; a first visit downloads … MB (brotli)
new fly.toml: run fly launch once (web/deploy/README.md)
next: cd …/web/deploy/stage && fly deploy
```

The folder holds the game files, so git ignores it.

**3. Create the app (first time only).**

```bash
cd web/deploy/stage
fly launch --no-deploy --copy-config --name <your-app> --region fra
```

App names are unique across Fly: pick your own, for example `cm0102-mike`; the game will be at
`https://<your-app>.fly.dev`. `fra` is Frankfurt; `fly platform regions` lists the others. If it asks whether to
tweak the settings, answer no. A later stage keeps this `fly.toml`.

**4. Deploy (each release).**

```bash
cd ~/Projects/cm0102-web/web/deploy/stage
fly deploy
fly open
```

`fly deploy` uploads the folder (a few hundred MB, so the first time takes some minutes) and builds the server on
Fly. `fly open` opens the address; use Chrome 137 or later.

**A later release** is steps 2 and 4: `git pull`, the engine build again if the engine's code changed
(`cmake --build build-web --target cm0102_recomp seasons_rules`), the stage, `fly deploy`.

## If something goes wrong

| What you see | What to do |
|---|---|
| `stage: … does not exist` or `… has no cm0102.exe` | `--game` names the wrong folder; the message names the right one when it is below it |
| `stage: 760 files are missing or of another size` | The same: `--game` is not the folder that holds `cm0102.exe` |
| `stage: … are not the clean files` | The folder holds other versions of the files; use the game folder of the Bosman Edition app |
| `stage: the engine is not built` | `web/README.md` step 4 |
| The upload of `fly deploy` keeps failing | `fly deploy --local-only` builds on your Mac instead (needs Docker Desktop) |
| The site does not answer | `fly logs`: a good start prints `game files: 764 (461 MB)` and `open http://0.0.0.0:8080` |
| The machine runs out of memory | `fly scale memory 512` |

The first visit after a quiet time wakes the machine, which takes a few seconds.

## What players download

- The first visit: the game files and the engine, brotli-compressed (the stage prints the size).
- Later visits: nothing. The page keeps the game files in the browser's cache storage and the engine in the browser's
  cache, each under its version; only a file that changed is downloaded again.
- Saves stay in each player's browser, and the page asks the browser to keep that storage.

Outgoing traffic is the main cost of hosting: check Fly's prices for your region before you share the address
widely. The machine itself is the smallest (shared CPU, 256 MB), and `fly.toml` stops it when idle.

## Notes

- Players need Chrome 137 or later (JSPI). The page tells any other browser that it cannot run the game, before it
  downloads anything.
- The site hands out the files of the 2008 free release with the official patch, and the original's code translated
  to WebAssembly, as the Bosman Edition downloads do: an unofficial fan project (main README, Legal).
