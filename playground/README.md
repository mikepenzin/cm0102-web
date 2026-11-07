# Engine playground

A hardcoded mock engine and one page that test every kind of interaction between the app and the engine: reads,
actions, refusals, answers that come days later, events the engine starts itself, a live match, and save and load.
The outcomes are scripted or seeded rolls, not football logic, and every name is made up. The point is the contract:
the page holds no game rules, the engine decides, and the page shows what comes back.

```
page (app.js)  ⇄  EngineClient (engine-client.js)  ⇄  Web Worker (engine-worker.js)  ⇄  MockEngine (mock-engine.js)
```

The real engine will sit where `MockEngine` is, behind the same calls (concept design, "Game API").

## Run

```bash
node playground/serve.mjs          # http://127.0.0.1:8090 (a module worker needs http, not file://)
cd playground && npm test          # 11 tests, Node 22, no packages
```

## What to try

| Interaction | Where | What the engine does |
|---|---|---|
| Read the state, squad, a player | Squad; click a name | Answers with shown values only; the hidden `ability` never leaves it |
| An action it refuses | Untick a player, Save eleven | `invalid_selection`: not 11, no goalkeeper, an injured or foreign player |
| A transfer bid | Transfers: bid for Viktor Lund at £1,200,000 | Takes the bid; the answer comes in the inbox 2 game days later |
| A second bid while one waits | Bid again | `bid_pending` |
| Days that stop for a decision | +7 days | Stops after 2 days: the selling club accepts; Sign him or Withdraw |
| A counter-offer, a refusal | Bid £750,000 for Yusuf Demir, £300,000 for Callum Reid | Counter at the asking price after 2 days; refused after 1 |
| The engine acts on its own | To next match | Day 5: Kingsmead United bid £800,000 for Elliot Shaw; Accept or Refuse |
| Busy | Press +1 day while days run | `busy`: the engine is in its day loop and answers nothing else (see the traffic log) |
| Match day | To next match | Stops on 20.7.2001; +1 day is refused with `match_pending` |
| A live match | Matches: Play match | Commentary lines arrive as events, then the result |
| Player status after a match | Squad | The eleven is tired (70%), morale moves with the result, Danny Wren is injured for 20 days |
| An injured player still in the eleven | To next match, Play match | `invalid_selection` until the eleven changes; he is fit again on 9.8.2001 |
| Save and load | Save, +7 days, Load | The date goes back; the step goes on growing, so the page drops nothing as stale |

## The calls

Reads: `getState`, `getSquad`, `getPlayer`, `getTransferTargets`, `getInbox`, `getFixtures`, `getMatch`, `getTable`.
Actions: `selectTeam`, `bid`, `reply`, `continue` (`days` or `toMatch`), `playMatch`, `save`, `load`, `newGame`.
Events: `progress` (a day done), `news`, `commentary`, `result`.

Every answer is `{step, date, result}`; a refusal is `{code, message}`. The step only grows, and the client marks an
answer with a lower step as stale and drops a stale event.

Compared with the Game API table of the concept design, the playground adds `getTransferTargets`, `getMatch`,
`playMatch`, `load` and `newGame`, folds the contract offer into the reply to an accepted bid, and has no
`setTactics`.
