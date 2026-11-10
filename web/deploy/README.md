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

**2. Stage the files (each release).** From your copy of the repo (for example `~/Documents/Projects/cm0102-web`),
straight from the game folder inside the app:

```bash
cd ~/Documents/Projects/cm0102-web
git pull
node web/deploy/stage.mjs --game "/Applications/CM 01-02 Bosman Edition.app/Contents/Resources/game"
```

`--game` names the folder that holds `cm0102.exe` and `Data`; a copy of it works too (`--game ~/cm0102-game`).

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

If `fly launch` objects, this does the same:

```bash
fly apps create <your-app>
sed -i '' 's/^app = .*/app = "<your-app>"/' fly.toml      # on Linux: sed -i 's/…/…/' fly.toml
```

**4. Deploy (each release).**

```bash
cd ~/Documents/Projects/cm0102-web/web/deploy/stage
fly deploy
fly open
```

`fly deploy` uploads the folder (a few hundred MB, so the first time takes some minutes) and builds the server on
Fly. `fly open` opens the address; use Chrome 137 or later.

**A later release** is steps 2 and 4:

```bash
cd ~/Documents/Projects/cm0102-web && git pull
# only if the engine's code changed (no emsdk_env.sh needed: the build folder remembers the compiler):
#   cmake --build build-web --target cm0102_recomp seasons_rules
node web/deploy/stage.mjs --game "/Applications/CM 01-02 Bosman Edition.app/Contents/Resources/game"
cd web/deploy/stage && fly deploy
```

## Accounts and cloud saves (optional)

Sign in with Google and cloud saves (`web/README.md`, "Cloud saves") are off until their settings are on the app. Once:

```bash
cd ~/Documents/Projects/cm0102-web/web/deploy/stage
fly storage create                     # a private Tigris bucket; sets AWS_* and BUCKET_NAME on the app
fly secrets set GOOGLE_CLIENT_ID=<client id> GOOGLE_CLIENT_SECRET=<client secret> \
  CM_SESSION_SECRET="$(openssl rand -base64 48)" CM_PUBLIC_URL=https://<your-app>.fly.dev \
  CM_CLOUD_USERS=<your email>
```

With a key of your own Tigris account instead of `fly storage create`, add `AWS_ENDPOINT_URL_S3=https://t3.storage.dev
AWS_ACCESS_KEY_ID=<key id> AWS_SECRET_ACCESS_KEY=<secret> AWS_REGION=auto BUCKET_NAME=<bucket>` to the
`fly secrets set` line. The Google client and its redirect URI come first: `web/README.md`, "Set it up", step 1.

To sell Cloud Saves with Paddle (`web/README.md`, "Payments"), add its settings and the seller's name and contact,
which the terms, refunds, privacy and pricing pages show:

```bash
fly secrets set PADDLE_ENV=sandbox PADDLE_CLIENT_TOKEN=test_… PADDLE_PRICE_ID=pri_… PADDLE_WEBHOOK_SECRET=pdl_ntfset_… \
  PADDLE_API_KEY=pdl_sdbx_apikey_… CM_SELLER="<your name>" CM_CONTACT=<your email>
```

For real payments: the live token, price, secret and key, and `fly secrets unset PADDLE_ENV`.

`fly secrets set` restarts the app with them. `fly logs` then shows `accounts and cloud saves: on`; if a setting is
missing it names it. The secrets stay on the app: a later `fly deploy` keeps them.

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
| `accounts and cloud saves: off, missing …` | Set the named secrets (Accounts and cloud saves) |
| Google says `redirect_uri_mismatch` | The client's Authorized redirect URI must be exactly `https://<your-app>.fly.dev/auth/google/callback` |
| Only some Google accounts can sign in | The Google app is still "Testing": Audience, Publish app |
| The checkout says "Something went wrong" | Paddle has not approved the domain yet (Website approval), or there is no Default payment link (Checkout settings: `https://<your-app>.fly.dev/`), or the token and the price are of different environments (`test_` is the sandbox) |
| Paid, but Cloud Saves stay off | `fly logs`: a `Paddle notification refused` line means `PADDLE_WEBHOOK_SECRET` is not the destination's secret; `price … is not one of PADDLE_PRICE_ID` names the price to add |

The first visit after a quiet time wakes the machine, which takes a few seconds.

## What a deploy uploads

Only what changed. The Docker image has one layer per part, from what never changes to what changes most: the game
files, the manifest, the engine, then the server and the page. The stage leaves a file that did not change exactly
as it was (with its time and its brotli copy), so Fly's builder reuses the layers whose files are the same:

| You changed | A deploy rebuilds and uploads |
|---|---|
| The page or the server (`web/`) | The last layer: a few KB |
| The engine (a new `build-web`) | The engine layer and the page layer: the size of the engine |
| Nothing (a stage again) | Nothing new |
| The game files | Everything (it should not happen: they are the clean files) |

The first deploy uploads all of it, about 500 MB with the brotli copies. The stage prints how many files changed
since the last one. If Fly hands the app a fresh builder (its cache is gone), one deploy uploads everything again.

## What players download

- The first visit: the game files and the engine, brotli-compressed (the stage prints the size).
- Later visits: nothing. The page keeps the game files in the browser's cache storage and the engine in the browser's
  cache, each under its version; only a file that changed is downloaded again.
- Saves stay in each player's browser, and the page asks the browser to keep that storage. Accounts with cloud saves
  also keep them in Tigris: the page and Tigris talk through the server, so a save that goes up or comes down passes
  the machine (a save is some MB, compressed).
- After one good start the page also starts without a connection, and it can be added to the home screen as an app
  (`web/README.md`, "As an app").

Outgoing traffic is the main cost of hosting: check Fly's prices for your region before you share the address
widely. The machine itself is the smallest (shared CPU, 256 MB), and `fly.toml` stops it when idle.

## Notes

- Players need Chrome 137 or later (JSPI). The page tells any other browser that it cannot run the game, before it
  downloads anything.
- The site hands out the files of the 2008 free release with the official patch, and the original's code translated
  to WebAssembly, as the Bosman Edition downloads do: an unofficial fan project (main README, Legal).
