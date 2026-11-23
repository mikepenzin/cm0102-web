# Lobby and relay server

The lobby keeps the list of open online games. The relay passes traffic between a host and its human managers. Both run as one Cloudflare Worker on the free plan (0.7.0, #45, slice 3 of `docs/specs/2026-10-27-online-play-0.7.0-spec.md`).

- `src/index.ts`: the Worker. It reads requests, checks fields and sends each call to the lobby or a relay.
- `src/lobby.ts`: the `Lobby` Durable Object. One object holds every open game. The games are in its storage, so they survive when Cloudflare evicts an idle object.
- `src/relay.ts`: the `Relay` Durable Object. There is one object for each game, named by the game code. It uses the WebSocket Hibernation API.
- `src/filter.ts`: the word filter for player names and game names.
- `src/join.ts`: the invite link page (slice 8).
- `test/`: tests that run in the local Workers runtime (workerd), with no Cloudflare account.

## Run the tests

You need Node.js 20 or later.

```bash
cd server && npm ci && npm test
```

To check the types, run `npm run check`. It makes `worker-configuration.d.ts` (not in git) and runs `tsc`.

The tests use `@cloudflare/vitest-plugin` with Vitest 4.1. This is Cloudflare's current recommendation (checked on 2026-10-27). It replaces `@cloudflare/vitest-pool-workers`.

## Deploy (Max only, after his yes)

1. Run `cd server && npm ci`.
2. Run `npx wrangler login`. A browser window opens. Sign in to the Cloudflare account and allow access.
3. Run `npx wrangler deploy`. Wrangler prints the address, for example `https://bosman-online.<account>.workers.dev`.
4. Open `<address>/v1/health`. It must show `{"ok":true,"api":1}`.
5. Give the address to slices 4 and 5. The game keeps it in one setting.

To take the server down, run `npx wrangler delete`. LAN and VPN play still work.

Privacy (Q26) depends on these rules:

- Do not turn on Workers Logs, Logpush or a tail Worker. `wrangler.jsonc` sets `observability.enabled` to false, and a test checks it.
- Do not run `wrangler tail`. It shows each request's headers, and these include the internet address.

## Free plan and paid plan

Figures checked on 2026-10-29 on developers.cloudflare.com (Workers pricing and limits, Durable Objects pricing). The limits are for the whole Cloudflare account. Max's account also runs Elodie's two Durable Objects, which use about 900 Durable Object requests a day.

| Limit | Free plan | Workers Paid ($5 a month minimum) |
|---|---|---|
| Worker requests | 100,000 a day, reset at 00:00 UTC. Over the limit, Cloudflare answers with its own error page (error 1027) and the Worker does not run. | 10 million a month, then $0.30 a million |
| Durable Object requests | 100,000 a day. 20 incoming WebSocket messages count as 1 request. | 1 million a month, then $0.15 a million |
| Durable Object run time | 13,000 GB-s a day | 400,000 GB-s a month, then $12.50 a million GB-s |
| Durable Object rows written | 100,000 a day | 50 million a month |

The free plan stops at its daily limit and the game shows "full today". The paid plan has no daily limit: it charges for use above the included amounts.

### When the relay is billed

Cloudflare bills run time while an object handles a request or a message, and while it is idle in memory but cannot hibernate. A relay that can hibernate costs nothing while it waits, even with its sockets open. The relay keeps every rule for this, and `test/sleep.test.ts` checks them:

- It accepts sockets with the Hibernation API (`acceptWebSocket`), never with `accept()`.
- It starts no timer and sends no request of its own while it passes frames.
- The runtime answers a text `ping` with `pong` (`setWebSocketAutoResponse`). The relay's code does not run, so a ping costs no run time.

The game's own PING (slice 7) goes every 30 s through the relay. It is a binary frame, so it wakes the relay for a few milliseconds.

### Measured on the live server

Two-game run on 2026-10-29, 14:57 to 15:00 UTC: a host and one joiner on this Mac joined through `https://bosman-online.maxvert.workers.dev` (`tests/smoke/network_run.py --relay`, slice 5 build). The figures come from Cloudflare's GraphQL analytics (`durableObjectsPeriodicGroups`, `durableObjectsInvocationsAdaptiveGroups`, `workersInvocationsAdaptive`).

| Part | Calls | Billed run time | For each call |
|---|---|---|---|
| Relay | 22 (2 connections, 20 messages) | 0.21 s awake, 0.027 GB-s, while the sockets were open for about 150 s in total | 0.0012 GB-s |
| Lobby | 45 requests (register, heartbeats, lists, join, close) | 0.136 GB-s | 0.003 GB-s |
| Worker | 47 requests | no run-time charge. Workers bill requests and CPU time, not run time. | |

The relay slept between messages: it was awake for 0.21 s of the 150 s.

### What a game uses

Assumptions: the measured cost of each call, a heartbeat every 30 s, and slice 5's estimate of 7,000 relay messages an hour for 4 human managers who click about 20 times a minute. The message rate is an estimate, not a measurement of play. Slice 9's runs through the live server can measure it: billed requests = relay messages / 20.

| Game | Durable Object requests | Run time | Worker requests |
|---|---|---|---|
| A 4-hour evening, 4 human managers | 1,920 (relay 1,400, lobby 520) | 35 GB-s | 520 |
| An always-on game, open all day, nobody connected | 2,900 (heartbeats and the relay's hourly check) | 9 GB-s | 2,880 |

Each human manager who stays connected but does not click adds 2 messages every 30 s (PING and PONG), which is about 290 billed requests a day.

### How many games each plan carries

| | Free plan, each day | Workers Paid, in the $5 each month | Workers Paid, above the $5 |
|---|---|---|---|
| 4-hour evenings | about 50 (Durable Object requests run out first) | about 520 (about 17 a day) | about $0.0007 for each further evening |
| Always-on games | about 34 (Durable Object and Worker requests run out together) | about 11 | about $0.01 a month for each further game |

The free plan's limits are daily, and the paid plan's are monthly. So the free plan carries more each month than the $5 includes, but it stops for the rest of the day at its limit. On the paid plan, 50 evenings every day (1,500 a month) cost about $5.30 a month. Run time is far from the limit on both plans.

The lobby writes about 1,440 rows a day for each open game: 1 row when a game registers, closes or expires, and at most 1 row a minute from its heartbeats. The 100,000 free rows a day cover about 69 games open all day, so the request limits come first. After an eviction, the list can show fields up to 60 s old until the next heartbeat. The relay writes 1 row when a game registers, 1 row when a host connects and 1 row for each join.

## Floods of calls

The server stays on the free plan (Max, 2026-10-29). On the free plan a flood can stop online play until 00:00 UTC, but it can never cost money. LAN and VPN play still work.

Layer 1 (`src/flood.ts`) has two parts.

A limit for each internet address runs first in the Worker, before it reads a body or asks a Durable Object. It uses the Workers Rate Limiting binding (`ratelimits` in `wrangler.jsonc`). An IPv6 address counts by its first 64 bits, as in the lobby.

| Limit | In 60 s from one address | Why this number |
|---|---|---|
| `RATE_CALLS`, every call | 180 | A busy home makes at most about 64: 2 hosts with 8 heartbeats, 6 joins, and 4 games that each ask for the list every 5 s (the game keeps its list 5 s). The margin is for many players behind one carrier or campus address. |
| `RATE_WRITES`, register, join and close | 20 | These calls write storage, hash a password or make a relay. A home makes at most 2 registers (the limit of 2 games). 20 lets a group of friends at one address join in the same minute. |
| `RATE_WRITES_48`, the same writes from one IPv6 /48 site | 60 | One site has 65,536 /64 networks, so without this limit it could make 20 writes in each of them. |

A refused call gets status 503 with `{"error": "unavailable", ...}`, the header `X-Bosman-Api: 1` and `Retry-After: 60`. The game shows its existing "not available" text and tries again later. The invite link page gets a short page that says to try again in a minute. A call with no `CF-Connecting-IP` header (only a local run) is not limited. If the limiter fails, the call goes through.

The Worker also keeps the list, and the answer for one game, in its memory for 2 s. Many list calls then cost one lobby call. These answers hold no internet address and no token. An error answer and a "game not open" answer are not kept. A register, heartbeat or close through the same Worker drops the copies at once. Through another Worker, the list can be up to 2 s old.

What layer 1 cannot do:

- A refused call still counts as a Worker request, because Cloudflare counts each call that reaches the Worker. A flood of 100,000 calls a day therefore still stops online play until 00:00 UTC, also from one address. Layer 1 saves the Durable Object requests, storage writes, run time and CPU time behind each refused call.
- The limiter's counts are approximate and kept for each Cloudflare location. A flood from many addresses or locations gets through each address's limit.
- The limiter never gets an internet address. Its key is a hash of the address prefix, the UTC date and a salt, so the key of one address changes each day. With the salt in the code only, someone who has a key could find an IPv4 address by trying them all. Only Cloudflare sees the keys, and it sees the addresses anyway. For a secret salt, run `npx wrangler secret put RATE_KEY_SALT` once and enter any long random text. Cloudflare keeps the counts for the limit's period only. Nothing is stored or logged by this server.
- The refusal uses the game's "not available" text. It does not say that the cause is too many calls from the player's network.

Cloudflare's documents (checked 2026-10-29) do not name a plan for the Rate Limiting binding, and a third-party report says the free plan has it at no charge. The deploy shows if the account allows it. If the binding is missing, every call goes through.

A free WAF rate limiting rule would block calls before the Worker runs, so that blocked calls would not count. The free plan has 1 such rule (by IP, 10 s). Its set-up pages are for a domain on Cloudflare. They do not say if it works on a `workers.dev` address. It would need a domain.

## API contract (version 1)

The game uses this contract in slices 4, 5 and 8. The invite link page at the end of this section is for people, not for the game. The base is the deploy address, for example `https://bosman-online.<account>.workers.dev`.

### Rules for the game side

- Each answer from this API is JSON with the header `X-Bosman-Api: 1`. An answer is either a success body or `{"error": "<code>", "message": "<English text>", ...}`.
- An answer without `X-Bosman-Api: 1` does not come from this API. If its body contains `1027`, show "Online games are full today, try again later". In all other cases, treat it as `unavailable`, also a 429: Cloudflare's rate limiting rule of the server's address answers 429 to an address with more than 60 calls in 10 s. Cloudflare does not document the status code of its 1027 page, so the game must also check the body.
- A timeout, a TLS failure or a refused connection is `unavailable`.
- Use a 10 s timeout for each HTTP request. Read at most 64 KiB of an answer.
- The game shows its own text for each error code. The `message` field is a fallback.
- No failure of this server may stop LAN or VPN play.

### Game fields

Text fields hold printable Latin-1 only (U+0020 to U+007E and U+00A0 to U+00FF). The server trims spaces at each end. Request bodies are JSON, at most 4096 bytes.

| Field | Type | Rule |
|---|---|---|
| `gameId` | string | 8 to 128 of `A-Z a-z 0-9 . _ : -`. The game id from the save. It stays private and is never in the list. |
| `name` | string | 1 to 40 characters. Word filter. |
| `hostName` | string | 1 to 40 characters. Word filter. |
| `version` | string | 1 to 24 of `0-9 A-Z a-z . + -`, for example `0.7.0` |
| `database` | string | 1 to 40 characters. Word filter. |
| `season` | integer | Start year of the season, 1900 to 2200 |
| `day` | string | The game date, `YYYY-MM-DD` |
| `humanClubs` | string array | At most 16 club names, each 1 to 40 characters. Word filter. |
| `awayClubs` | string array | Optional, empty by default. The clubs whose human manager is Away: the connection dropped and the club is on holiday. The same rules as `humanClubs`. |
| `freeSeats` | integer | 0 to 16 |
| `managersOnline` | integer | 0 to 16 |
| `pingMs` | integer | Optional, 0 by default. The highest ping of the host's human managers in ms, 0 to 9999, 0 when none is known (slice 7). The game list row shows it. |
| `password` | string | Optional, at most 64 characters. Empty means no password. The lobby keeps only a salted PBKDF2 hash (20,000 rounds) in its storage. |

### Endpoints

`GET /v1/health` gives `{"ok": true, "api": 1}`. It does not use a Durable Object, so the connection test (slice 7) can call it often.

The connection test (slice 7, `src/recomp_rt/win/net_test.c`) calls `GET /v1/health`, then `GET /v1/games`, then a WebSocket request for `/v1/games/22222222/relay?role=manager&ticket=connection-test`. No game has that code, so the relay answers `unknown_game` (404) and stores nothing. If the answer is `upgrade_required`, something on the way took the `Upgrade` header off, and the relay cannot work on that network. Each test costs 3 Worker requests and 2 Durable Object requests.

`POST /v1/games` registers a game. The body holds every game field. The answer is 201 with `{"code", "hostToken", "heartbeatSeconds": 30, "expireSeconds": 90}`.
- `code` is 8 characters from `23456789ABCDEFGHJKMNPQRSTUVWXYZ`. The game list and the invite link use it.
- Keep `hostToken` secret. Heartbeat, close and the host relay connection need it.
- To register the same game again (see the heartbeat), add `code` and `hostToken` to the body. The answer is 200 with the same code and token.

`POST /v1/games/<code>/heartbeat`, with the header `Authorization: Bearer <hostToken>`, keeps the game open. Send it every 30 s. The body is optional and can hold `season`, `day`, `humanClubs`, `awayClubs`, `freeSeats`, `managersOnline` and `pingMs`, and `password` after the host changed it (a new password, or an empty string to remove it; the list's `password` flag follows at once, and the change is stored at once). The answer is `{"ok": true}`. The lobby drops a game with no heartbeat for 90 s and closes its relay. If the answer is `unknown_game`, register again at once with `code` and `hostToken`. This also happens if the lobby loses its storage. If that answer is `not_host`, the game expired and its relay is closed. Then register as a new game, without `code` and `hostToken`, and connect to the new relay. The game gets a new code.

`DELETE /v1/games/<code>`, with the header `Authorization: Bearer <hostToken>`, closes the game and its relay. The host sends it when it quits.

`GET /v1/games` gives `{"games": [...], "heartbeatSeconds": 30}`. Each game is `{"code", "name", "hostName", "version", "database", "season", "day", "humanClubs", "awayClubs", "freeSeats", "managersOnline", "pingMs", "password": true|false}`. The list holds at most 200 games, with the most managers online first.

`GET /v1/games/<code>` gives `{"game": {...}}` for one game, in the same form. The invite link page uses it.

`POST /v1/games/<code>/join` with `{"version", "name", "password"?}` checks a human manager before the relay. The checks are, in order:
1. The fields are valid and the name passes the word filter.
2. The game is open.
3. The version is the host's version.
4. The password is correct.

The answer is `{"ticket", "expireSeconds": 60}`. A ticket works once, within 60 s, for this game only.

Wrong passwords are limited for each game, in each minute: 5 from one internet address, and 30 from all addresses together. Over a limit, the answer is `too_many_attempts` for the rest of that minute.

### Error codes

| Code | HTTP | When | Extra fields |
|---|---|---|---|
| `bad_request` | 400 | A field or the body is not valid | `field` |
| `name_not_allowed` | 400 | The word filter refused a name | `field` |
| `unknown_game` | 404 | The game is not open | |
| `not_host` | 401 | The host token is missing or wrong | |
| `too_many_games` | 429 | This internet address already has 2 open games. An IPv6 address counts by its first 64 bits. | |
| `game_already_open` | 409 | A game from the same save is open | `code` of the open game, so the second host can join it |
| `version_mismatch` | 409 | Join: the versions differ | `hostVersion` |
| `wrong_password` | 403 | Join: the password is missing or wrong | |
| `too_many_attempts` | 429 | Join: too many wrong passwords in this minute | |
| `bad_ticket` | 401 | Relay: the ticket is wrong, used or expired | |
| `relay_full` | 409 | Relay: 16 human managers are connected | |
| `host_not_connected` | 409 | Relay: the host is not connected | |
| `upgrade_required` | 426 | Relay: the request is not a WebSocket request | |
| `not_found`, `method_not_allowed` | 404, 405 | Unknown path or method | |
| `lobby_full` | 503 | 300 games are open | |
| `full_today` | 503 | A free plan limit is used up | `retryAfterSeconds` (to 00:00 UTC), also in the `Retry-After` header |
| `unavailable` | 503 | Any other server failure, or too many calls from one address (then with `Retry-After: 60`) | |

Cloudflare does not document the error text of a used-up Durable Object limit. The Worker gives `full_today` for an error that contains "daily limit", "free tier", "quota", "exceeded allowed" or "exceeded its daily", and `unavailable` for other errors. CPU and memory errors give `unavailable`. Check this against a real limit error once, after the deploy.

### Relay

Connect with a secure WebSocket on port 443:

- Host: `wss://<address>/v1/games/<code>/relay?role=host&token=<hostToken>`
- Human manager: `wss://<address>/v1/games/<code>/relay?role=manager&ticket=<ticket>`

Before the upgrade, the relay can refuse a connection with an HTTP error and a JSON body from the table above.

The relay passes bytes and reads none of them. It does not use the original's 4-byte length prefix, so slices 4 and 5 can put whole batches in one frame.

| Socket | Binary frame | Text frame |
|---|---|---|
| Human manager, both ways | The payload | `ping` gets `pong`. Any other text closes the socket with 1003. |
| Host, both ways | 1 channel byte, then the payload. Channels 0 to 15 are human managers. From the host, channel 255 sends the payload to every human manager. | The relay sends `{"t":"open","ch":n}` when a human manager connects and `{"t":"close","ch":n}` when one leaves. The host sends `{"t":"kick","ch":n}` to remove one. `ping` gets `pong`. |

- The relay gives each new human manager the lowest free channel. A channel is free again after its `close` message.
- A frame can be at most 1 MiB, plus the channel byte. A larger frame closes the socket with 1009.
- Send `ping` as text to keep a quiet connection open. The relay answers it without waking from hibernation.

Close codes:

| Code | Reason | Meaning |
|---|---|---|
| 4000 | `host_left` | To human managers: the host left, or the game closed. Show "The host left the game". |
| 4001 | `replaced` | To the host: a new host connection with the same token took over. |
| 4003 | `kicked` | To a human manager: the host used Kick. |
| 4004 | `game_closed` | To the host: the lobby closed the game, after a close or 90 s with no heartbeat. |
| 1003, 1009 | | A text frame from a human manager, or a frame that is too large |

When the host connection closes, the relay closes every human manager with 4000. A host can connect again with the same token while the game is open.

### Invite link page (slice 8)

`GET /join/<code>` (and `HEAD`) gives an HTML page for a person, not JSON. The game never calls it. A host posts the address, for example `https://<address>/join/K7M2Q9RT`, and a friend opens it in a browser. The code is read in any case.

- An open game: the page shows its name, host, season, game date, database, human clubs, managers online, places left and version. It has an "Open in Bosman Edition" button for `cm0102://join/<code>`, the game code to find the game by hand, a line for a game with a password, and a download link to the latest GitHub release. Status 200.
- A game that is not open, or a code that is not valid: "This game is not open", with the download link. Status 404.
- A lobby failure: the "full today" or "not available" text as a page. Status 503.

The page has no script, no cookie, no picture and no outside file. Its headers forbid all of them (`Content-Security-Policy: default-src 'none'`), send no referrer and allow no cache. It reads the game with the lobby's `get`, so it keeps no record of the visitor. Its last line states the privacy rule (Q26): the lobby keeps internet addresses in memory only while a game is open, and writes them to no log. `src/join.ts` builds it, and `test/join.test.ts` tests it.

Each page view is one Worker request and one Durable Object request. A chat program that shows a preview of the link (Discord, for example) also loads the page once.

## Privacy

- The Worker writes no log lines.
- The lobby keeps a salted SHA-256 hash of each host's internet address (the first 64 bits for IPv6), in memory only, to count open games for each address. It also keeps one for each address that sent a wrong password, for one minute. The salt is random and is never stored. The hash goes when the game closes or expires. When Cloudflare evicts the idle lobby, the hashes go too. The lobby learns each host's hash again from its next heartbeat, so for up to 30 s after an eviction one address can register more than 2 games. A stored hash would not be safe: its salt would have to be stored too, and an IPv4 address could then be found from the hash by trying every address.
- The lobby stores each open game: its code, the save's game id, the listed fields, a hash of the host token, the password hash and the time of the last heartbeat. It deletes the game when it closes or expires. The relay stores the game code, its key, the id of the host connection and used ticket ids. Once an hour, a relay with no host connection asks the lobby if the game is open. If it is not, the relay deletes all of this. It never receives the internet address, because the Worker passes it only the `Upgrade` header and the query.
- The tests check log lines, answers, lobby memory and both storages for the test addresses.
