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
When `build-web-asyncify` holds the Asyncify engine (`web/README.md`, "Other browsers"), the stage takes it too and
prints `with the Asyncify engine: Safari and iPad browsers can play`; without it only Chrome 137 and later can play
(`--asyncify <folder>` names another folder).

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
#   cmake --build build-web-asyncify --target cm0102_recomp      (when you have that build: both engines must match)
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
  CM_CLOUD_USERS=<your email> AWS_ENDPOINT_URL_S3=https://t3.storage.dev CM_S3_PATH_STYLE=1
```

The last two point the server at Tigris's canonical address with the bucket in the path, the one that lets players'
browsers send and get their saves directly (Straight to Tigris, below): `fly storage create` sets
`fly.storage.tigris.dev`, which answered the browser's preflight without `Access-Control-Allow-Origin` on the live
site (17 November 2026). The server finds a working address by itself either way; with these two it passes at once.

With a key of your own Tigris account instead of `fly storage create`, add `AWS_ACCESS_KEY_ID=<key id> AWS_SECRET_ACCESS_KEY=<secret> AWS_REGION=auto BUCKET_NAME=<bucket>` to the
`fly secrets set` line. The Google client and its redirect URI come first: `web/README.md`, "Set it up", step 1.

To sell Cloud Saves as a Buy Me a Coffee membership (`web/README.md`, "Payments"), make the webhook in Buy Me a Coffee
(Integrations, Webhooks: `https://<your-app>.fly.dev/api/coffee/webhook`, the four `membership.*` events), then add its
signing secret, the levels that give Cloud Saves, their price, and the seller's name and contact, which the terms,
refunds, privacy and pricing pages show:

```bash
fly secrets set BMC_WEBHOOK_SECRET=<signing secret> CM_LEVELS="Supporter,Squad Player,Club Legend" CM_PRICE='$5 a month' \
  CM_COFFEE_URL=https://buymeacoffee.com/<your name> CM_SELLER="<your name>" CM_CONTACT=<your email>
```

The price in single quotes: in double quotes the shell takes `$5` for a variable and leaves " a month". Players see
`CM_PRICE` as it is (the start box, Account, the tour, the pricing page); keep it the price of the levels on Buy Me a
Coffee.

`fly logs` shows `sold as a Buy Me a Coffee membership (levels …)`. The membership page is `CM_COFFEE_URL` +
`/membership`; `BMC_MEMBERSHIP_URL` sets another. Send test event in Buy Me a Coffee: the log says the signature holds.

The admin page's **Members** tab lists every membership the webhook told the server of (email, level, whether it
gives Membership, whether that email has signed in here) and the last notification with what came of it. A supporter
missing there sent no membership event: a one-off coffee is not a membership, or the webhook is not set up. One listed
but "not seen" has Membership waiting until they sign in with Google using that same email.

The levels: `CM_LEVELS` lists the Buy Me a Coffee level names lowest first (Supporter, Squad Player, Club Legend by
default); it replaces `BMC_CLOUD_LEVELS`, which still works alone (`fly logs` says so; `fly secrets unset
BMC_CLOUD_LEVELS` once `CM_LEVELS` is set). Club Legends get new features and Library databases early: the admin page's
**Early access** tab and the library form's day (`web/README.md`, "Levels and early access").

Paddle, the first way (subscriptions bought through it keep working while its settings stay):

```bash
fly secrets set PADDLE_ENV=sandbox PADDLE_CLIENT_TOKEN=test_… PADDLE_PRICE_ID=pri_… PADDLE_WEBHOOK_SECRET=pdl_ntfset_… \
  PADDLE_API_KEY=pdl_sdbx_apikey_… CM_SELLER="<your name>" CM_CONTACT=<your email>
```

For real payments: the live token, price, secret and key, and `fly secrets unset PADDLE_ENV`.

`fly secrets set` restarts the app with them. `fly logs` then shows `accounts and cloud saves: on`; if a setting is
missing it names it. The secrets stay on the app: a later `fly deploy` keeps them.

## The database library

Databases for players to get, each for a season (`web/README.md`, "Database library"): on the admin page, the
**Database library** section takes a zip of a data set (the 20 database files, as Saved games, Data sets, Download
gives it), its name, the season a new game with it starts in, and a note (who made it, what it holds). Players then find
it in Saved games, Data sets, **Get a database**, free and with no account. It is on whenever the bucket is set
(`fly logs`: `database library: on`); the files live in the bucket under `library/`. Offer only databases you may
share: the makers of community updates usually say how theirs may be passed on.

## Statistics and the admin page

The statistics are on by default and kept in the bucket (`web/README.md`, "Statistics and the admin panel"). The admin
page is at `https://<your-app>.fly.dev/admin`; name who may open it:

```bash
fly secrets set CM_ADMIN_EMAILS=<your email> CM_ADMIN_TOKEN="$(openssl rand -hex 24)"
```

Sign in with that Google account on the game's page: its Account dialog then has an Admin panel button, which
opens `/admin` in a new tab. The token is for scripts:
`curl -H "Authorization: Bearer <token>" https://<your-app>.fly.dev/api/admin/stats`. `fly logs` shows
`statistics: on (opt-in: the page asks first), kept in the bucket`: each player is asked once, and only those who allow
them are counted (the admin page shows the share who said yes). `CM_STATS=optout` counts everyone until they turn them
off in Help, with no question (EU consent rules want the question); `CM_STATS=off` turns them off.

**A notice when a player sends a report.** Set a webhook and each report a player sends (Send to the developer) gives
you a short message: the game's error, the browser, whether there is a note, the save's name and size, and a link to
the admin page. The note, the log and the save stay on the server. It waits up to 90 seconds for the save, so one
report is one message; at most 30 a day, then one that says more came.

```bash
fly secrets set CM_REPORT_WEBHOOK=<the webhook's URL>
```

A Discord channel: Edit Channel, Integrations, Webhooks, New Webhook, Copy Webhook URL. Slack: an incoming webhook
of a Slack app. Telegram: a bot of @BotFather and your chat's id, as
`https://api.telegram.org/bot<token>/sendMessage?chat_id=<id>`. `fly logs` shows `report notices: on`; a webhook that
fails is logged (`stats: report notice not sent: …`), and the report itself is kept all the same. The link goes to
`https://<your-app>.fly.dev/admin`; with your own domain, set `CM_SITE_URL=https://<your domain>`.

## Buy me a coffee

A Buy me a coffee button shows in the bar (next to Help) and a line in Help when the server knows your page:

```bash
fly secrets set CM_COFFEE_URL=https://buymeacoffee.com/<your name>
```

Only an `https` address is taken; without it nothing shows. The button opens the page in a new tab, and it hides in
full screen so it never covers the game.

## The video guides

The game page's footer gets a "Video guides" link and Help a line about the two how-to videos when the server knows
the YouTube channel:

```bash
fly secrets set CM_YOUTUBE_URL=https://www.youtube.com/@CM0102inBrowser
```

Only an `https` address of youtube.com or youtu.be is taken; without it nothing shows.

## Straight to Tigris

Saves and data sets go from the browser to Tigris and back without passing the machine (`web/README.md`, "Straight to
the store"), once the bucket lets the site's page in (its CORS rules). The server sets them at the start when its key
may; the key that `fly storage create` makes may not, and `fly logs` then says `the store's CORS rules were not set
(S3 403 AccessDenied …)`. Saves still work then, through the server. Set the rules by hand, once:

1. `fly storage dashboard` (in `web/deploy/stage`) opens the bucket in the Tigris console.
2. The bucket's Settings, CORS: a rule with
   - Allowed origins: `https://<your-app>.fly.dev` (and your own domain, when you have one)
   - Allowed methods: `GET`, `PUT`, `HEAD`
   - Allowed headers: `*`
   - Expose headers: `ETag`
   - Max age: `3600`
3. `fly secrets set CM_DIRECT=on`: the server then goes direct without setting the rules itself. `fly logs` says
   `saves and data sets go straight between the browser and the store (CORS rules set in the store's console …)`.

If the rule is missing or wrong, a page finds out at its first save (its engine log says `did not answer this page
directly`) and goes through the server for that visit, so a mistake costs speed, not saves.

The server now finds out first, at each start: it makes a signed upload link as the pages get them, sends the
browser's preflight for it from the site's address, and puts a small file with it (then removes it). `fly logs` then
says one of:
- `saves and data sets go straight between the browser and the store (…; checked from https://…: a signed upload
  link and the browser's preflight work)`: all is well.
- `the bucket's CORS rules do not let https://… send (the preflight: HTTP …)`: the rule is missing, or names another
  address; fix it as above.
- `the store refused a signed upload link (HTTP 403: …)`: the rule is fine, but the store does not take the link's
  signature (other Fly users saw Tigris refuse links that sign content-type). Send me that line.
**Tigris's addresses.** One bucket answers at two hosts, `fly.storage.tigris.dev` (which `fly storage create` sets)
and `t3.storage.dev` (Tigris's canonical one, docs.fly.io/tigris), each with the bucket in the host name or in the
path. On the live site (17 November 2026) `fly.storage.tigris.dev` answered the browser's preflight with no
`Access-Control-Allow-Origin`, so no page could go direct. When the configured address fails the check, the server
now tries the others in turn (`t3.storage.dev`, then with the bucket in the path, then the other way at
`fly.storage.tigris.dev`) and signs the pages' links with the first that passes; it keeps the configured address for
its own work. `fly logs` then says `… go straight between the browser and the store (…; the links name
https://t3.storage.dev, in place of the configured address, which did not pass)`. To make that address the configured
one as well: `fly secrets set AWS_ENDPOINT_URL_S3=https://t3.storage.dev`.

The check now tries what a page does: the preflight of an upload with a save's headers, the upload, and the download
of that file with the site's `Origin`, which must come back with `Access-Control-Allow-Origin` too. When no address
passes, the log has each one's reason (`cloud: and at …: …`) and the rules as Tigris gives them back
(`the bucket holds: origins …; methods …`, or `no CORS rules`); the server checks again after 1, 5 and 30 minutes, in
case the rules take a while to reach every place of the store, and goes direct at the first that works
(`the check works … after the start`). Tigris's own CORS page speaks of public buckets only, and the bucket of `fly
storage create` is private: if every address fails, send me these lines.
In the last two cases the pages go through the server from the start. Saves a player sends with Send to the developer
then go through the server too: it writes each to a file on its disk (at most two of 400 MB at once) and streams it
to the store, so the machine's memory holds none of it.

## Size of the machine

One shared-CPU machine with 512 MB (`fly.toml`), which stops when nobody is visiting. The server streams the files
from disk, so more players do not take more memory; what does is a cloud upload, which the server holds while it
checks it (a save up to 64 MB compressed, a data set up to 48 MB zipped). Together the uploads in memory stay under
`CM_UPLOAD_BUDGET_MB` (default 160); one past it is turned away and goes at the next sync. `/healthz` answers Fly's
check, and the limits count requests (a first visit asks for several files at a time).

A stage folder from before keeps its own `fly.toml`; the stage then prints `your fly.toml has no health check`. Once:

```bash
cd web/deploy/stage
fly scale memory 512
```

and copy the `[http_service.concurrency]` and `[[http_service.checks]]` parts of `web/deploy/fly.toml` into
`web/deploy/stage/fly.toml` (keep its `app =` line), then `fly deploy`.

`fly launch` may have made two machines (`fly status` lists them). Both serve the same files, and the statistics
add up across them (each write adds what its machine counted). One is enough: `fly scale count 1` leaves one.

## No rollback past one tab at a time

Since one tab at a time (November 2026) the saves' vault in each browser is IndexedDB version 2 (`web/vault.js`,
`VAULT_VERSION`). A page of an earlier commit opens version 1, which the browser then refuses: that page turns the
vault off and lists none of the player's saves. So do not deploy a commit from before it again; fix forward instead.

## If something goes wrong

| What you see | What to do |
|---|---|
| `stage: … does not exist` or `… has no cm0102.exe` | `--game` names the wrong folder; the message names the right one when it is below it |
| `stage: 760 files are missing or of another size` | The same: `--game` is not the folder that holds `cm0102.exe` |
| `stage: … are not the clean files` | The folder holds other versions of the files; use the game folder of the Bosman Edition app |
| `stage: the engine is not built` | `web/README.md` step 4 |
| The upload of `fly deploy` keeps failing | `fly deploy --local-only` builds on your Mac instead (needs Docker Desktop) |
| `failed to push registry.fly.io/…` with `_api.internal:5000 … connection refused` | Fly's internal registry did not answer; the image itself built. Try, one at a time: `fly deploy --wg=false` (the push goes over HTTPS instead of Fly's private network), `fly deploy --depot=false` (the other builder), `fly deploy --local-only` (builds on your Mac, needs Docker Desktop). If none works, it is on Fly's side: status.flyio.net and community.fly.io |
| The site does not answer | `fly logs`: a good start prints `game files: 764 (461 MB)` and `open http://0.0.0.0:8080` |
| The machine runs out of memory | `fly scale memory 1024`, or a lower `CM_UPLOAD_BUDGET_MB` (Size of the machine) |
| `fly logs` says `the store's CORS rules were not set (… AccessDenied …)` | Saves still work, through the server. The key of `fly storage create` may not change the bucket's settings: set the rules once in the Tigris console (Straight to Tigris, below) |
| The engine log shows `cloud saves: … up: HTTP 502` | The server, or the store behind it, failed that upload; the page tries it again after 3 and 10 s, then each minute. `fly logs` says which: `cloud: PUT /api/saves/…: <the store's error>`, or `Out of memory` and a restart (the machine is too small: `fly scale memory 512`) |
| `accounts and cloud saves: off, missing …` | Set the named secrets (Accounts and cloud saves) |
| Google says `redirect_uri_mismatch` | The client's Authorized redirect URI must be exactly `https://<your-app>.fly.dev/auth/google/callback` |
| Only some Google accounts can sign in | The Google app is still "Testing": Audience, Publish app |
| The checkout says "Something went wrong" | Paddle has not approved the domain yet (Website approval), or there is no Default payment link (Checkout settings: `https://<your-app>.fly.dev/`), or the token and the price are of different environments (`test_` is the sandbox) |
| Joined, but Cloud Saves stay off | the admin page's Members tab: the last notification says what came of it. `refused` means `BMC_WEBHOOK_SECRET` is not the webhook's signing secret; `not in CM_LEVELS` means `CM_LEVELS` lacks that level's name (it counts as Supporter); a Supporter has no Membership by design; a membership listed but "not seen": they joined with another email than their Google account's (Give Squad Player or Club Legend on the admin page) |
| Paid with Paddle, but Cloud Saves stay off | `fly logs`: a `Paddle notification refused` line means `PADDLE_WEBHOOK_SECRET` is not the destination's secret; `price … is not one of PADDLE_PRICE_ID` names the price to add |

The first visit after a quiet time wakes the machine, which takes a few seconds.

## What a deploy uploads

Only what changed. The Docker image has one layer per part, from what never changes to what changes most: the game
files, the manifest, the engine, then the server and the page. The stage leaves a file that did not change exactly
as it was (with its time and its brotli copy), so Fly's builder reuses the layers whose files are the same:

| You changed | A deploy rebuilds and uploads |
|---|---|
| The page or the server (`web/`) | The last layer: a few KB |
| The engine (a new `build-web`) | The engine layer and the page layer: the size of the engine |
| The Asyncify engine (a new `build-web-asyncify`) | Its layer and the page layer: the size of that engine |
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

Outgoing traffic is the main cost of hosting: each new player downloads the game files once (the stage prints the
size of a first visit), and Fly bills outgoing data by the GB; check Fly's prices for your region before you share
the address widely. A returning player downloads nothing, so a CDN in front adds little at first. The machine itself
is small (shared CPU, 512 MB), and `fly.toml` stops it when idle.

## Notes

- Players need a browser with JSPI: Chrome 137 or later, or Safari 27 or later (an iPad on iPadOS 27, in any browser);
  or, when the stage had the Asyncify engine, any browser with enough memory
  (Safari, iPad: `web/README.md`, "Other browsers"). The page tells a browser that cannot run the game so, before it
  downloads anything.
- The site hands out the files of the 2008 free release with the official patch, and the original's code translated
  to WebAssembly, as the Bosman Edition downloads do: an unofficial fan project (main README, Legal).
