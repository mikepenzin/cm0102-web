# Plan: easy network play (#45, 0.7.0)

Status: plan for the grilling, written 2026-10-19. Nothing is built. It replaces the "ways to connect" part of `2026-10-16-online-play.md` and keeps that file's open points.

## 0. The simple plan (recommended, 2026-10-19)

Rule: change the runtime under the game, not the game. The original's network screens, packets and host logic run unchanged. Only the network layer below them changes (`src/recomp_rt/win/net.c` and a new relay adapter). The game cannot tell the difference, and single-player games do not touch this code, so the same-save tests stay valid.

Step 1, real network (small, low risk):

1. Put real sockets behind the WinSock calls.
2. Give the game its addresses directly, in place of the `ipconfig` text file that breaks on modern Windows (section 2a).
3. Result: LAN play and VPN play work with no setup. All ten steps of section 2a go away except the VPN.
4. Measure the traffic, and look for the cause of the "Contacting Server" freezes and the slow joiners.

Step 2, online play that looks like a LAN to the game:

1. When a host starts a network game, the original already broadcasts that it hosts a game. The adapter also sends that to the lobby.
2. When a player opens Select Network Game, the original broadcasts a request for the list. The adapter answers it with the lobby's games as well as the LAN games.
3. When the player clicks Join, the original opens a TCP connection to the host. The adapter sends it through the relay.
4. Result: the original's own screens show the online list and join the game. There are no new screens. One line in `settings.txt` turns online play off.

Step 3, only if players need it: clear error messages, rejoin after a drop, the connection test and the ping. These need small new screens or messages, so they come after steps 1 and 2 work.

Not in the first version: private codes, invite links, chat changes, pass the host, spectators, Discord, direct connections. Each is a later choice from section 5.

Cost: £0 on the Cloudflare free plan (section 4). If the free plan runs out, the online list says so for that day, and LAN and VPN play still work.

Open for the grilling: public games only at first, or a simple password on a game. The original's manager passwords already protect each club.

## 1. Max's goal (2026-10-19)

A player starts the game and clicks Network Game. The game shows a list of every network game that is open now. The player clicks one and plays. There is no patch, no VPN, no router setting and no IP address. The cost to Max is zero.

## 2. What the original already does

The function map shows that most of this flow is in the original. It works only on one local network. The facts below come from the function map (most rows have high confidence) and the 2026-10-13 inventories, which read the screen manager and Display code. The repo rule says a spec must not rely on a map row alone, so the spec step reads each cited function again and confirms it.

- The start screen has a Network Play button (`setup_build_setup_game_screen`, 0x81bab0). Network Play has Start, Quick Start, Restore and Join Network Game (0x81edd0, 0x81ef70).
- A host types a game name on Enter Network Game Name (0x81f1a0, 0x81f2c0).
- Join opens Select Network Game (0x81f8d0, 0x81fbd0). It sends the UDP broadcast `CM3 LIST SERVER REQ` and lists the games that answer, with Refresh and Join. A host answers with `CM3 LIST SERVER ACK` and says `CM3 LIST SERVER BYE` when it closes (0x77a9f0, 0x77a890, 0x77af20).
- The game port is 6667 (0x1a0b, `Network::Network` 0x77a7d0). TCP carries the game. Each packet has a 4-byte length prefix (`Network::send_packet` 0x77ac30). A host takes up to 16 player slots.
- The host runs the whole game. Each other player is a remote screen: the host sends the screen draws as packets (`Display::add_area` 0x55fba0 writes into the network buffer, `Display` serialiser 0x562370), and the remote game replays them (packet applier 0x564080, client loop 0x7fdc00). The network buffer holds 50,000 bytes.
- A player can join a game in progress and take a free human manager or make a new one (0x838a80). When a player leaves, the host puts that manager on holiday (0x838c00). Manager passwords exist for restored games (0x8387e0).
- The port answers every WinSock call with "no network" (`src/recomp_rt/win/net.c`), so all of this is off today.

What this means for the plan:

1. Max's "list of games, click Join" screen is the original's Select Network Game screen. We change where the list comes from, from a LAN broadcast to an online list.
2. Only the host runs the match engine and holds the save. The other players do not need the same save, and the games cannot drift apart. They need the same Bosman Edition version, because the screen packets must match.
3. A joining player does not need the host's database. Forum players say this from long use (section 2a). Confirm it in the first slice.
4. Unknown: the traffic size. A screen change sends draw packets, so traffic follows clicks and match commentary, not game days. Measure it in the first slice.

## 2a. How players play network games today

Source: the champman0102 forum tutorial thread on network play (https://champman0102.co.uk/viewtopic.php?t=295), all 133 posts on 6 pages, May 2020 to July 2026. Read on 2026-10-19.

### The steps today

A host and each player must do all of this on Windows:

1. Set the game's compatibility mode to Windows 95, XP or NT 4.0. Tick Run as administrator.
2. Install a LAN tunnel program (Hamachi, or Radmin when Hamachi fails), make an account and join the same tunnel network.
3. Delete `cpuinf32.dll` from the game folder.
4. Run `ip.exe` as administrator. It writes the `~net.out` file, a copy of the computer's network addresses.
5. Copy `~net.out` to the desktop, open it in Notepad and type the tunnel's IP address over the first adapter address.
6. Save it as ANSI, not UTF-8. Mark it read-only. Copy it back into the game folder.
7. Apply a community patch's network fix, if the game has other patches.
8. Start the game in a window, click Network Play and allow the game through the firewall. If the firewall question does not come, the game will not work.
9. The host starts a network game. Players click Join. If the game is not in the list, they type the host's tunnel IP address.
10. Talk in another app (Messenger, Discord), because the game's chat is slow.

### What goes wrong, from the thread

| Problem | How often | Cause |
|---|---|---|
| The address file is blank, shows 127.0.0.1, holds the Wi-Fi address and not the tunnel's, will not save, saves as UTF-8, or the game writes a second copy next to it | The most common problem, about 25 posts | The original reads the computer's addresses from this text file (`tcpip_scan_adapter_addresses` 0x8aee20 runs winipcfg, ip.exe or ipconfig into a file and reads it). Modern Windows breaks it. |
| "The network game could not be created" | About 15 posts | Wrong compatibility mode, no administrator rights, a firewall, a missing network fix, the address file |
| The tunnel program fails: cannot log in, drops the connection, or one player cannot see the game | About 10 posts | Third-party tool. Free Hamachi takes 5 players, so bigger groups join two tunnel networks. |
| A player freezes at "Contacting Server" or "Processing", or the host's game carries on without the player | About 8 posts, still in May 2026 | Unknown. The usual answer is a full reinstall. |
| The game is very slow for the joining player | 3 threads | Unknown. The helpers blame the friend's computer. |
| It worked before and stopped after a Windows update | Several posts | Compatibility settings and permissions reset |
| Mac players | 2 posts | One player used an old Mac with an old Windows layer to host. |
| Saves gone after a switch to administrator mode | 1 post | Windows keeps the saves of a program without administrator rights in another folder. |

Other facts from the thread:

- A joining player needs neither the host's database nor the host's update. Only the host does. Forum helpers say this many times.
- "Reinstall everything" is the fix for most problems that nobody understands.
- One helper answered almost every question for six years, often by screen share. Players also ask for a working `~net.out` file to copy.
- Players run groups of 5 and more friends on regular game evenings.

### What this means for the design

Every step in the list above goes away in the port:

| Step today | In Bosman Edition |
|---|---|
| Compatibility mode, administrator rights, `cpuinf32.dll`, network fix patch | Not needed. The port runs natively on Mac, Windows and Linux. |
| Tunnel program and account | Not needed. The relay connects every player. |
| `ip.exe`, `~net.out`, Notepad, ANSI, read-only | Not needed. The port gives the game its addresses directly, and the relay needs no address. |
| Firewall question | Not needed. Each player connects out to the relay, and a firewall asks only about programs that listen for connections. LAN play still listens and may ask once. |
| Type the host's IP address | Not needed. The game list comes from the lobby. Private games use a code. |
| Chat in another app | A chat in the game that works well (section 5). |
| 5 players on free Hamachi | Up to 16, the original's limit. |

The thread also asks for things the plan did not have yet:

1. Clear error messages. Replace "could not be created" and "Contacting Server" with a reason and a next step, for example "The host is offline" or "Your connection dropped. Rejoin?".
2. Rejoin after a drop. A player who drops returns to the same club with one click.
3. A connection test in Network Play: it checks the lobby and the relay and says "Online play works" or what is wrong. This replaces the screen-share help.
4. A ping for each player in the lobby and in the game, so a slow player can see whether the network is the cause.
5. Find the cause of "Contacting Server" freezes and slow joiners in the first slice, with traffic logs from a real two-player evening.

## 3. How it works (recommended design)

Three parts.

1. Real sockets in `net.c`. The WinSock calls go to the real network on Mac, Windows and Linux. With this alone, LAN play works as in 2001, and VPN players need no extra patch.
2. A lobby on a Cloudflare Worker. The host's game registers its game with the lobby: game name, version, database, season, date, human clubs, free seats, and private or public. The Select Network Game screen shows the lobby list together with any LAN games. The game sends a heartbeat every 30 seconds, and the lobby drops a game that stops.
3. A relay so that no router setting is needed. Every player connects out to the relay over a WebSocket on port 443, which every home router and most work networks allow. The relay is a Cloudflare Durable Object, one for each game. It passes the host's packets to each player and back. Inside the game, a small adapter makes the relay look like the original TCP socket, so the original's network code runs unchanged.

Later, if the relay costs too much: try a direct connection first (WebRTC data channel with free STUN for hole punching), and use the relay only when that fails. This adds a library and a second path to test, so it is not in the first version.

The flow for a player:

1. Click Network Play, then Join Network Game.
2. The list shows each open game: name, host, league, season and date, human clubs, free seats, players online and a lock for private games.
3. Click a game, then Join. A private game asks for its code.
4. Pick a club or a free human manager, and play.

The flow for a host:

1. Click Network Play, then Start (or Restore a saved game).
2. Type a game name. Pick Public or Private (with a code such as `TIGER-42`) and the number of seats.
3. The game appears in every player's list at once.

## 4. Cost

Figures checked on 2026-10-19 (sources at the end). Check them again before the build.

| Part | Plan | Free allowance | Expected cost |
|---|---|---|---|
| Lobby (Worker) | Workers Free | 100,000 requests a day | £0. A heartbeat every 30 s is 2,880 requests a day for each game. About 30 open games at once fit. Raise the heartbeat to 60 s for 60 games. |
| Relay (Durable Object, Hibernation API) | Workers Free | 100,000 requests a day. 20 incoming WebSocket messages count as 1 request. 13,000 GB-s of run time a day. | £0 while traffic is light. Unknown until the traffic is measured. |
| Relay, if the free allowance runs out | Workers Paid | 1 million requests a month, then $0.15 a million | About $5 a month (about £4). |
| Direct connections, later (Cloudflare Realtime TURN) | Pay as you go | 1,000 GB a month | £0 at expected use. |
| Domain | none | the Worker gets a free `workers.dev` address | £0 |

Notes:

- On the free plan, a request over the daily limit fails until 00:00 UTC. The game must then show "Online games are full today, try again later" and keep LAN play working. It must never crash.
- To keep the relay inside the free plan, the game sends the host's packets in batches (for example, every 50 ms) and not one message for each packet.
- The first slice measures one real evening of play for 2 and 4 players. Then we know if the free plan is enough.
- If the project grows past the free plan, a donation link for the server is an option. Decide that later.

## 5. Ideas to make it memorable

Each idea is a candidate for the grilling, and none is decided.

1. Derby night. When two human clubs meet, both players watch the same match live, with the same commentary at the same time, and each can make substitutions. The original's network game may already do this. Check it in the first slice.
2. Live chat in the game, and quick reactions during a match (a goal, a red card).
3. Drop in and drop out. The original already puts a leaving manager on holiday. Show "Away" in the lobby and let the player take the club back when they return.
4. Always-on leagues. A host leaves a game running at home, and friends join in the evening. The lobby shows "Day 14 of the 2001/02 season, 3 of 8 managers online".
5. Invite links. A link such as `cm0102://join/TIGER-42` opens the game and joins at once. Players can post it on Discord or the forum.
6. Spectator seats. A player can watch a game in progress without a club, for example to watch a cup final.
7. Results to Discord. The host can add a Discord webhook, and the game posts results, transfers and the final table.
8. A weekly community challenge. Everyone starts the same save (same seed and club) on their own computer, and the lobby shows a table of results at the end of each week. It uses the lobby only, not the relay.
9. Hall of fame for online games: titles won against other human managers.
10. Deadline day. On the last day of a transfer window, every human manager is online for a live event with the transfer news.
11. Pass the host. When the host saves, the save goes to every player in the game, so any of them can host the next evening. This answers the risk that the game stops when the host is away. The save passes through the relay and is not kept on the server.

## 6. Risks and limits

- The host must stay online. When the host quits, the game stops for everyone. The game must save and say so to each player. A server that runs the game in the cloud would fix this, but it costs money and is out of scope.
- Abuse on public games: the host needs Kick and Ban, and the lobby needs a limit on games for each address and a word filter on game names.
- Version check: the join step refuses a different Bosman Edition version and says which version the host uses.
- No feature may change the game's decisions while the players make the same clicks (project rule). The network game runs the original's own code, so this rule holds. The same-save tests must still pass with networking compiled in.
- The relay sees the game traffic. Use TLS (the WebSocket on port 443 has it). Store no game data on the server.
- Never use patch code for the network work (project rule). The work starts from the original's own network code and the function map.

## 7. Build order (draft, for the spec)

1. Slice A, real sockets: `net.c` on BSD sockets and WinSock 2. LAN play on two computers and over a VPN. Measure traffic for 2 and 4 players. Answer the database question in section 2. Test: two games on one computer through 127.0.0.1.
2. Slice B, lobby: the Worker, the host's register and heartbeat, the online list on Select Network Game.
3. Slice C, relay: the Durable Object relay and the socket adapter in the game. Test behind two home routers.
4. Slice D, the polish: private codes, invite links, version check, kick, "Away" status, the full-today message, clear error messages, rejoin after a drop, the connection test and the ping (section 2a).
5. Slice E, one or two ideas from section 5, picked in the grilling.

## 8. Decisions for the grilling

1. The design in section 3 (lobby on a Worker, relay first, direct connections later).
2. Public games in the list for everyone, or only private games with a code.
3. Where the lobby lives: Max's Cloudflare account, with the source in this repo under `server/`.
4. The number of seats (the original allows 16).
5. Which ideas from section 5 go into 0.7.0.
6. What happens when the free plan runs out: wait until the next day, or pay about £4 a month.
7. A player name: free text, or a name kept by the lobby.

## Sources

- champman0102 forum, network play tutorial thread: https://champman0102.co.uk/viewtopic.php?t=295

- Cloudflare Durable Objects pricing: https://developers.cloudflare.com/durable-objects/platform/pricing
- Cloudflare Realtime pricing: https://developers.cloudflare.com/realtime/index.md
