# Hosting the browser build on Fly.io

The server only hands out files; every player's browser runs the game. One small Fly machine serves many players,
and it stops when nobody is visiting.

## Once

1. Build the engine and get the clean files as `web/README.md` says (steps 1 and 2).
2. Install Fly's command line and sign in:
   ```bash
   brew install flyctl
   fly auth login            # or: fly auth signup
   ```

## Each release

From the repo:

```bash
node web/deploy/stage.mjs --game ~/cm0102-game
```

It checks every game file against `tools/clean_files/manifest.tsv` (size and SHA-256), copies the server, the page,
the engine, the season tables and the game files into `web/deploy/stage`, writes a brotli copy of each file that
compresses, and adds `Dockerfile` and `fly.toml`. It prints how much a first visit downloads. The folder holds the
game files, so git ignores it.

The first time only, create the app (Fly app names are unique: pick your own; `fly platform regions` lists regions):

```bash
cd web/deploy/stage
fly launch --no-deploy --copy-config --name <your-app> --region ams
```

Then, every time:

```bash
cd web/deploy/stage
fly deploy
```

`fly deploy` uploads the folder and builds the image on Fly. The game is then at `https://<your-app>.fly.dev`. A
later stage keeps the `fly.toml` that `fly launch` wrote.

## What players download

- The first visit: the game files and the engine, brotli-compressed (the stage prints the size).
- Later visits: nothing. The page keeps the game files in the browser's cache storage and the engine in the browser's
  cache, each under its version; only a file that changed is downloaded again.
- Saves stay in each player's browser, and the page asks the browser to keep that storage.

Outgoing traffic is the main cost of hosting: check Fly's prices for your region before you share the address
widely. The machine itself is the smallest (shared CPU, 256 MB), and `fly.toml` stops it when idle.

## Notes

- Players need Chrome 137 or later (JSPI). Another browser gets a message, unless the engine is built with
  `-DCM0102_WEB_SUSPEND=asyncify`.
- The site hands out the files of the 2008 free release with the official patch, and the original's code translated
  to WebAssembly, as the Bosman Edition downloads do: an unofficial fan project (main README, Legal).
