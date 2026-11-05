# Server prototype: the game as a server engine

The recompiled game runs on a server, one process per active game, and a Node server drives it. Each step of a
game (a new game, or playing some days) runs in that process and ends with a save. SQLite keeps the games and what
the pages show; a folder keeps the saves (a stand-in for object storage such as S3 or R2). The game world itself
is the engine's memory and its save: nothing changes a game except the engine.

```
browser ⇄ HTTP ⇄ Node (src/server.js) ⇄ socket pair, fd 3 ⇄ cm0102_recomp (RECOMP_WORKER_FD=3), one per game
                        │
                        ├─ data/games.db   games and steps (SQLite)
                        ├─ data/saves/     the last three saves of each game
                        └─ data/logs/      the log of each engine run
```

## Parts

- `src/recomp_rt/win/worker.c` and `worker_proto.c`: the engine worker in the runtime. With `RECOMP_WORKER_FD`
  set, the game stops at each human turn (0x5cde40), sends the date and the club, and takes the commands
  `state`, `save <name>`, `advance <days>` and `quit` (`worker_proto.h` has the protocol). Without the variable
  nothing changes.
- `src/engine.js`: one engine process and the protocol.
- `src/games.js`: one step at a time per game, a save after each step, a warm engine while the game is in use,
  stopped after `CM_IDLE_MS`; the next step loads the latest save into a new engine. A crash fails the step; the
  next step goes on from the last save.
- `src/savfile.js`: reads the save container, as `tools/save/savfile.py` does.
- `steps/`: the clicks that make a new game or load a save, written by `steps/export_steps.py` from the test
  fixtures. They are for classic mode; each run gets `display = classic` and `faster_days = on`.
- `public/index.html`: one page: games, New game, +1, +7 and +30 days, the steps of each game.

## Run with the test stand-in (no game files)

`test/standin.c` speaks the same protocol with the runtime's own `worker_proto.c` and plays pretend days. It is
not the game: no data set, no match engine.

```bash
gcc -std=c11 -D_DEFAULT_SOURCE -Isrc/recomp_rt/win server/test/standin.c src/recomp_rt/win/worker_proto.c -o /tmp/standin
CM_ENGINE=/tmp/standin node server/src/server.js      # http://127.0.0.1:8080
cd server && npm test                                  # needs gcc and python3
```

## Run with the game

Build the recompiled game as the main README says, then:

```bash
CM_ENGINE=build/cm0102_recomp CM_GAME_DIR=../clean-3968 node server/src/server.js
```

| Variable | Default | |
|---|---|---|
| `CM_ENGINE` | none | the game, or the stand-in |
| `CM_GAME_DIR` | none | the clean files (the game's `--game`) |
| `CM_DATA` | `server/data` | the database, saves, work folders and logs |
| `PORT`, `HOST` | 8080, 127.0.0.1 | no logins yet: keep it on 127.0.0.1 |
| `CM_IDLE_MS` | 300000 | a warm engine stops after this time without a step |
| `CM_TURN_TIMEOUT_MS` | 1200000 | the longest wait for a human turn (a new game loads the database first) |

## What is checked and what is not

Checked here (`npm test`, `ctest -R worker`): the protocol, the server with the stand-in (a new game, days, the
stop of an idle engine and the load of its save, a killed engine and the recovery from the last save, a restart of
the server), the save reader against `tools/save/savfile.py`, the step files against the test fixtures, the page in
Chromium.

Not checked: a run with the real game. It needs the clean files, which were not on the machine that wrote this.
Before more than a test:

- The start of the holiday. `advance` calls the holiday setter 0x5fd2f0 with 1. The value that the original passes
  on Yes of "Go on holiday ?" (0x6b2d80) was not read, nor what else that path does (the holiday stamp, the saved
  team selection). The worker checks the holiday flag after the call and answers with an error when it is not set.
  Read 0x6b2d80 first.
- The holiday settings of the manager stay as they are in the save: offers, buying, tactics and team selection
  follow them while days pass.
- One human manager. A game with several shares one save; how the human turn moves between them is not researched.
- The read tables hold the date and the club only. More needs the record layouts (the data map).
- Speed and size: a new game loads the database (about a minute for England only, six minutes with every league,
  `tests/fixtures/fixtures.py`). A save of a long game with every league can be hundreds of MB.
