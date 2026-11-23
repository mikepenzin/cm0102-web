# Easy online play (0.7.0, #45)
Status: Ready

Interview: 2026-10-27, Q1 to Q30, all settled. Background and costs: `docs/research/2026-10-19-easy-network-play-plan.md`. Terms: the "Network play" section of `GLOSSARY.md`.

## Problem

A network game works today only after about ten manual steps on Windows: compatibility mode, administrator rights, a VPN program, a hand-edited address file, a network patch and a firewall question. The forum thread on network play is six years of players who are stuck at one of these steps. In Bosman Edition, network games do not work at all, because the port answers every network call with "no network". Mac and Linux players cannot play network games.

## Solution

A player clicks Network Play, then Join Network Game. The game list shows every open network game online and on the LAN. The player clicks one, clicks Join, picks a club and plays. There is no patch, no VPN, no router setting and no IP address. Mac, Windows and Linux players can play in one game. It costs Max nothing on the Cloudflare free plan.

Only the network layer under the game changes. The original's network screens, packets and host logic run as they are. A free lobby on Cloudflare keeps the list of online games, and a relay on Cloudflare passes the traffic, so no router setting is needed.

## User stories

1. As a human manager, I want to see every open online game in the game list, so that I can join one with no setup.
2. As a human manager, I want LAN games and online games in the same list, so that one screen covers both.
3. As a host, I want my game to show in the lobby when I start a network game, so that my friends can find it.
4. As a host, I want to set a password on my game, so that only my friends can join.
5. As a human manager on a Mac, I want to join a game that a Windows host runs, so that the platform does not matter.
6. As a human manager on a different Bosman Edition version, I want a message that names the host's version, so that I know what to update.
7. As a human manager whose connection drops, I want my club to go on holiday and show Away, and I want to take it back with my manager password when I return.
8. As a human manager, I want the game to tell me when the host quits, and I want to go back to the start screen, while the host's game saves.
9. As a host, I want to leave an always-on game open at home, so that friends can join and leave in the evening. The game moves forward only when a human manager clicks Continue.
10. As a human manager, I want the game list to show the season, the day and the managers online for each game, so that I can see which game is active.
11. As a human manager, I want an invite link that I can post on Discord or the forum, so that a friend clicks it and joins.
12. As a friend who does not have the game, I want the invite link to open a page that says what the game is and how to get it.
13. As a human manager, I want a clear reason and a next step when something fails, in place of "The network game could not be created" or a freeze at "Contacting Server".
14. As a human manager, I want a connection test in Network Play that says online play works or what is wrong.
15. As a human manager, I want to see each human manager's ping, so that I can see if the network is the cause of a slow game.
16. As a host, I want Kick, so that I can remove a human manager from a public game.
17. As a player who does not want online games, I want to turn Online play off on the options screen, so that the game does not contact the lobby.
18. As a player, I want LAN and VPN games to work if the online service is down or full for the day.
19. As a player, I want the lobby to keep no record of my internet address.
20. As a host of a restored save, I want the lobby to stop a second copy of the same game, so that the league does not split into two games.

## Decisions

Design:

- Change only the network layer under the game. The original's network screens, packets, host logic, holiday code and manager passwords run unchanged. Single-player games do not run this code, so the same-save rule (Q152) holds.
- Real sockets replace the "no network" answers for all 18 network calls the game imports. The game gets its own addresses directly in place of the original's address file lookup, which starts a background program that the port does not have.
- The host's game tells the lobby about its game and sends a heartbeat. The lobby drops a game that stops its heartbeat.
- When the game list asks for games, the network layer answers with the lobby's games as well as the LAN games.
- When a human manager joins an online game, the network layer carries the connection through the relay over a secure web connection on port 443. The game sees an ordinary connection.
- The game sends the host's packets to the relay in batches, to stay inside the free plan.

Rules from the interview:

- 0.7.0 ships real sockets, the lobby and the relay together (Q2).
- No new screens for the lobby. The game list is the original's Select Network Game screen. There is no waiting room and no chat (Q9). The original's names Network Play and Join Network Game stay (Q27).
- Small additions that need a message or field: the game password, Kick, the error messages, the connection test and the ping.
- All games are public in the list. A host can set an optional password (Q10).
- Up to 16 human managers in a game, the original's limit (Q4).
- The game list holds up to 50 games with the original's scrollbar. The recompiler raises the original's limit of 9 (research note, section 10). A test fills the list with 50 games (Q31, Q33).
- On a fast connection, the runtime reports the mouse button as up to a joiner's game after it sends a click, until the real button comes up. The game's input code does not change (Q34).
- Tests on one Mac join the host through the Mac's LAN address. The loopback test 0x8af290 stays as it is (Q32).
- Mac, Windows and Linux players play in one game (Q5).
- A player on a different version cannot join. The message names the host's version (Q6).
- When the host quits or drops, the host's game saves at once, and each human manager sees "The host left the game" and goes back to the start screen (Q7).
- Drop in and drop out: a human manager who leaves is Away, and their club is on holiday as in the original. They take it back with their manager password (Q15).
- Always-on game: it moves forward only when a human manager clicks Continue, as in the original. The game list shows the season, the day and the managers online (Q16).
- Invite link: a web link on the lobby address opens a page with an "Open in Bosman Edition" button and the game code. The game registers a `cm0102://` link on Mac, Windows and Linux. A game with a password still asks for it (Q17).
- The lobby allows one open copy of a game. A second host sees that the game is already open and can join it (Q23).
- Player names are free text with no account. A word filter checks player names and game names (Q13).
- The host has Kick. The lobby allows at most 2 open games for each internet address (Q14).
- The host sets the game password, and kicks a human manager, on the Network tab of the options box. A joiner gets a small password box when the game needs one. Kick asks "Kick <name>? Their club goes on holiday." with Yes and No. Both are in the runtime, with no change to the original's screens (Q38, Q39). Slice 6 builds both.
- The lobby keeps internet addresses in memory only while the game is open and writes them to no log. The site says this in one line (Q26).
- Online play is an option on the game's options screen, on by default. When it is off, the game list shows LAN games only and the game does not contact the lobby (Q25).

Server:

- The lobby and relay run on Max's Cloudflare account on the free plan, on a `workers.dev` address. The code lives in the repo under `server/` (Q11).
- When the free allowance runs out, the game list says "Online games are full today, try again later" until 00:00 UTC. LAN and VPN play still work. The game never crashes on a server failure (Q12).
- The relay stores no game data.
- Max deploys the server. A deploy waits for his yes.

Build:

- Slices build in worktrees while the 0.6.0 gates run, with `build-slot.sh` for builds. Nothing merges to main before 0.6.0 ships (Q3).
- The build sessions may start, use and suspend the "Windows 11 (1)" Parallels virtual machine, after the 0.6.0 gates finish (Q30).
- Never use patch code. Work from the original's own network code and the function map. Read each cited function and cite its address (repo rule).

## Testing

Seams:

1. Two-game run. A script starts a host game and a joining game with timed clicks, joins through Network Play and compares screen dumps. It uses the same timed-click method as the smoke tests. It runs against 127.0.0.1, then a local copy of the server, then the real server.
2. Server tests in `server/`, against a local copy of the lobby and relay.
3. The four same-save sets, unchanged, with networking compiled in.

Release tests (Q29), because Max cannot run a test evening:

1. Two and four games on this Mac play through the real lobby and relay.
2. Dropped (Q45): the Windows 11 virtual machine was to join a game that this Mac hosts. This Mac's Parallels Desktop edition refuses `prlctl resume`, `start` and `exec` (they need the Pro or Business Edition), so a script cannot drive the virtual machine. Test 3's GitHub Windows machine covers a real Windows machine with its own firewall, over the internet.
3. GitHub's Windows and Linux machines join a game that this Mac hosts over the internet, with scripted clicks.
4. A four-player game runs through a World Cup, a Euros and every season change between them, with a drop and rejoin, a host quit and a version mismatch.

The one gap: a home router with strict settings.

## Out of scope

Derby night, chat, the weekly challenge, the hall of fame, deadline day, Discord in any form, pass the host, spectators, direct connections without the relay, accounts, ban lists, a cloud host that runs the game.

## Open decisions

None.

## Slices

### 1. Read the original's network code
Blocked by: none
Delivers: a research note in `docs/research/` that cites each network function by address. It covers the packets and the list broadcast, whether the game waits on each call or polls, the address lookup and how to replace it, whether a joining human manager needs the database, the join and holiday flow, and an estimate of traffic. No game code.
Merge danger: two-way door. Blast radius: none, nothing ships.
- [ ] Every claim cites a function address that the author read.
- [ ] The note lists what slice 2 must change and what it must not touch.

### 2. Real sockets
Blocked by: 1
Delivers: LAN and VPN play on Mac, Windows and Linux. The game finds its own addresses directly. Two games on this Mac play together through 127.0.0.1, and the Windows virtual machine joins a game on this Mac. Traffic for 2 and 4 human managers is logged.
Merge danger: two-way door. Blast radius: network games only. The same-save sets prove that single-player games do not change.
- [x] Two-game run passes, through this Mac's LAN address in place of 127.0.0.1 (Q32), with 1 and 3 joiners, and under wine with the Windows build (`tests/smoke/network_run.py`).
- [x] Same-save sets pass.
- [x] Traffic log for 2 and 4 human managers is in the research note (section 11).
- [ ] ~~The Windows virtual machine joins a game on this Mac~~: dropped (Q45), see release test 2.

As built (2026-10-27, branch `claude/online-s2-sockets-b40614`): the merge danger stays a two-way door. Files: `src/recomp_rt/win/net.c`, `net_core.c`, `src/recomp_rt/host/net_posix.c`, `net_win32.c`, `host_net.h`, `CMakeLists.txt`, `packaging/windows/windows.cmake` (links ws2_32 and iphlpapi) and `tools/recomp/widescreen.py` (four entries inside 0x8aee20 removed). A merge conflict is likely only in `CMakeLists.txt` and `widescreen.py`. Open: on a fast connection one click of a human manager acts on several host screens, because the original's client loop sends the click again for each new screen while the button is down (research note, section 11). It must be fixed before 0.7.0 ships, and where to fix it needs a decision. Also open for slice 4: the game list shows one row for each adapter of the host.

### 3. Lobby and relay server
Blocked by: none
Delivers: the Worker and relay in `server/` with tests: register, heartbeat, list, relay, the free-plan limits, the "full today" answer, at most 2 games for each address, one open copy of a game, the word filter and addresses in memory only. Max deploys it.
Merge danger: two-way door, but public, so the Opus review rule applies. Blast radius: the online list is down. LAN play still works.
- [ ] Server tests pass against the local copy.
- [ ] No log line or stored record holds an internet address.
- [ ] Max approves the deploy.

### 2b. One click acts on one screen
Blocked by: 2
Delivers: a joiner's click acts on one host screen only, on a fast LAN and through the relay (Q34, research note section 11).
Merge danger: two-way door. Blast radius: joiners' clicks in network games. Single-player input does not change.
- [x] The two-game run clicks Next on Enter Name and lands on the next screen only.
- [x] Same-save sets pass.

As built (2026-10-27, branch `claude/online-s2-click-fix`): the merge danger stays a two-way door. Files: `src/recomp_rt/win/user32.c` (the rule in the input poll wrapper, only at the poll 0x7fdf17 of the client loop), `src/recomp_rt/win/net_core.c` and `net_core.h` (`nc_resets_in`, the count of host screens read), `tests/smoke/network_run.py`. In a press, the client loop gets a widget once, and a second widget only while no new host screen came (a press that ends a text entry gives the field, then the button). Mac and wine runs pass with 1 and 3 joiners. Not run: a click through the relay (slice 5). Research note, section 11.

### 4a. A game list of 50 games
Blocked by: 2
Delivers: the game list holds up to 50 games with the original's scrollbar (Q31, Q33). Constant patches in the recompiler (`tools/recomp/game_list.py`) and a runtime buffer for the screen's copy of the names. No game function is replaced.
Merge danger: two-way door. Blast radius: the game list and join in network games.
- [x] Game list run passes: 50 ACKs fill the list, 5 BYEs remove games, and a join of the last row reaches its host (`tests/smoke/game_list_run.py`, under wine with `game_list_wine.sh`).
- [x] Two-game run passes.
- [x] ctest and the same-save sets pass.

As built (2026-10-27, branch `claude/online-s4a-list50-211ec8`): the merge danger stays a two-way door. Files: `tools/recomp/game_list.py` (24 constant sites), `tools/recomp/widescreen.py` (the import), `src/recomp_rt/win/game_list.c` and `src/recomp_rt/rt.h` (the buffer of the names), `tests/smoke/game_list_run.py` and `game_list_wine.sh`. Each array has 51 slots, 50 games and the original's spare slot (research note, section 10). The game list run fails on the slice 2b build (9 games) and passes on this one. The two-game run, ctest (50 tests) and the four same-save sets pass, with the slice 2b build as `ref-build`. In the match set, `condition-on` and `condition-off` differed once under load and were equal in a run of the two alone. For a merge with slice 4b: the patches act only while 0x77a890, 0x77a9f0, 0x77af20 and 0x81f8d0 stay translated. If slice 4b replaces one of them with `RT_REPLACE`, the build stops with "no translated instruction" at those sites (the build runs `recomp.py --all`), and the replacement must use the new layout.

### 4. Online games in the game list
Blocked by: 2b, 3
Delivers: a host puts its game in the lobby. The game list shows online and LAN games with the season, the day and the managers online, and one row for each host. Also the Online play option, the version check, the game password and the "full today" message.
Merge danger: two-way door. Players see it, so the Opus review rule applies.
- [x] Two-game run passes against the local server.
- [x] With Online play off, the game makes no lobby request.
- [x] Same-save sets pass.

As built (2026-10-27, branch `claude/online-s4b-lobby-ea2663`, less the 50-game list of slice 4a): the merge danger stays a two-way door. Files: `src/recomp_rt/win/net_lobby.c` (new), `lobby.c`, `lobby_http.c`, `lobby_rows.c` and their headers (new), `net_core.c` and `net_core.h` (hooks, join check, `nc_peers`), `net.c`, `win.h`, `options.c`, `options.h`, `options_screen.c` (a third tab, Network), `host/net_posix.c`, `host/net_win32.c`, `host_net.h` (thread, lock, name look-up, route address), `CMakeLists.txt` (Threads, the `lobby` test), `tests/net/test_lobby.c`, `tests/options/test_options.c`, `tests/smoke/network_run.py` (`--lobby`), `tests/smoke/smoke_run.py` (online-on, online-off). Merge with slice 4a: `LR_CAP` in `lobby_rows.h` must become 50 when its cap of 0x77b0eb does; a conflict is likely only in `CMakeLists.txt` and `smoke_run.py`. Tests: ctest 51 of 51; the two-game run with `--lobby` and with 3 joiners; the four same-save sets (start.sav, prematch.sav, a new game, yeovil.sav with `--days 7`, against `ref-builds/main-batchAX`, with online-on and online-off) and the options box set pass. The sets ran before the second review round, which changed only network code; start.sav and the options set passed again on the final build. Research note, section 12. Open: the screens of the game password (Max's choice), the lobby address of the release, the game id of a restored save, and the lobby's memory (section 12, finding for the server).

### 5. Join through the relay
Blocked by: 4
Delivers: a human manager on another network joins with no router setting. Adds a library for secure web connections on three platforms.
Merge danger: two-way door. Blast radius: online joins fail. A new library ships on three platforms.
- [x] Two-game run passes through the real relay.
- [ ] A GitHub Windows machine and a GitHub Linux machine join a game on this Mac.

As built (2026-10-29, branch `claude/online-s5-relay-16166c`): the merge danger stays a two-way door. Files: `src/recomp_rt/win/relay.c`, `websocket.c`, `secure.c`, `tls_config.h` and their headers (new), `net_core.c` and `net_core.h` (route and incoming hooks, the join password for the lobby), `net_lobby.c`, `lobby.c`, `lobby_http.c` (https), `lobby_rows.h` (`LR_RELAY_PEERS`), `user32.c` (the joiner's client loop runs its poll once more for a press that came up before the host's answer), `host/net_posix.c`, `host/net_win32.c`, `host_net.h`, `CMakeLists.txt` (mbedTLS 3.6.7), `packaging/windows/windows.cmake` (crypt32), `tests/net/test_relay.c`, `tests/net/tls_check.py`, `tests/smoke/network_run.py` (`--relay`, `--remote`, `--game-name`), `tests/smoke/relay_join_remote.py`, `.github/workflows/online-join.yml`. On the final build: the two-game run through the local relay and the LAN run, the deployed relay with 1 and 2 joiners, ctest 55 of 55 and the four same-save sets pass. Earlier builds passed the local relay with 3 joiners. Not run: 3 joiners on the final build, the GitHub machines (the workflow needs a push and two secrets). The downloads ship mbedTLS's licence (`packaging/licences`). Research note, section 15.

### 6. Drop in, drop out and host quit
Blocked by: 5
Delivers: Away status, rejoin with the manager password, the host's save and message when it quits, Kick from a list on the Network tab of the Options box (Q39), and the game password: a field on the Network tab for the host and a small password box for a joiner (Q38).
Merge danger: two-way door. Blast radius: a human manager can lose their club in a network game.
- [x] A dropped human manager rejoins the same club.
- [x] A host quit saves and sends every human manager to the start screen.
- [x] Kick removes a human manager, and their club goes on holiday. No in the confirm keeps them.
- [x] A joiner with a wrong game password gets the password box again, and joins with the right one.

As built (2026-10-27, branch `claude/online-s6-dropin-95a988`, on slice 4b with `online-0.7.0` merged): the merge danger stays a two-way door. Rejoin needed no new code: the original opens Manager Status for a joiner while a manager is on holiday, and a click on that manager opens its password box (research note, section 13). Files: `src/recomp_rt/win/net_session.c` (new: the host's save, the deferred quit, the joiner's box and the way back to the start screen, the list of human managers online and Kick by player slot), `net_password.c` (new: the joiner's password box and the join again), `options_screen.c` (the Game password field and the Kick list on the Network tab), `options.h` and `options.c` (`game_password`), `net_core.c` and `net_core.h` (keepalive timers, the quit packet for a broken host connection, the joiner's own Exit Game, `nc_kick`, `nc_peer_list`), `net_lobby.c`, `lobby.c`, `lobby.h`, `lobby_rows.c` (Away in the entry and the row), `user32.c` (window close and the screen loop poll), `season.c` and `testrun.c` (one call each), `win.h`, `host_net.h`, `host/net_posix.c`, `host/net_win32.c` (`hn_set_keepalive`), `server/src/validate.ts` and `server/README.md` (`awayClubs`), `tests/net/test_net_core.c`, `tests/net/test_lobby.c`, `server/test/lobby.test.ts`, `tests/options/test_options.c`, `tests/smoke/dropin_run.py` and `tests/smoke/password_run.py` (new). Tests (2026-10-28, c5dd33b2): `tests/smoke/dropin_run.py` (drop, rejoin with the manager password, host quit with the save, the start screen), `--leave` and `--kick` (No, then Yes; the joiner's box), `tests/smoke/password_run.py`, the two-game run, ctest 51 of 51, the server tests 143 of 143, the four same-save sets against the slice 4b build and the options set with options-network pass. The options box run passed on the build before the last joiner-side fix. Decisions of 2026-10-28 (lead, for Max to confirm): a kicked joiner sees "The host removed you from the game."; Exit Game keeps the original's "Save Game Before Exit" and its No, and the runtime saves only at a window close; the lobby's password flag follows a change at the next heartbeat (the server takes `password` in the heartbeat: a deploy is needed).

### 7. Clear errors, connection test, ping
Blocked by: 6
Delivers: each failure shows a reason and a next step. Network Play has a connection test. The game list and the game show each human manager's ping.
Merge danger: two-way door.
- [ ] Each failure in a list of forced failures (lobby down, relay down, host gone, version mismatch, wrong password, full today) shows its message.
- [ ] The connection test reports each forced failure.

As built (2026-10-27, branch `claude/online-s7-errors-a814f4`, on slice 6 with `online-0.7.0` merged): the merge danger stays a two-way door. The connection test is on the Network tab of the Options box, not in Network Play: Network Play is a screen of the original, and a button there would change it. Every text that players read is in `src/recomp_rt/win/net_errors.c`. Files: `net_errors.c`, `net_errors.h` and `net_test.c` (new), `net_core.c` and `net_core.h` (control packets for the ping and the kick notice, the errors of connect and bind), `net_lobby.c` (the reasons in "Unable To Join" and "Unable To Create", `pingMs`, the Network tab line), `net_session.c` (the joiner's box: kicked, connection lost, host left; the ping of a player slot), `lobby.c`, `lobby.h`, `lobby_rows.c`, `lobby_rows.h` (`pingMs`, the ping in the row, the "lobby does not answer" row, the refusal code), `lobby_http.c` and `lobby_http.h` (the WebSocket request of the test), `options_screen.c` (the Connection test row and the ping column), `win.h`, `host_net.h`, `host/net_posix.c`, `host/net_win32.c` (`hn_peek`), `server/src/validate.ts`, `server/src/lobby.ts`, `server/README.md` (`pingMs`, the test's requests), `server/test/lobby.test.ts`, `server/test/relay.test.ts`, `tests/net/test_net_errors.c` (new), `tests/smoke/errors_run.py` (new), `tests/smoke/dropin_run.py`, `CMakeLists.txt`, `GLOSSARY.md` (Connection test, Ping). Research note, section 14. Relay down, host not connected, too many wrong passwords and a full game (16) have their texts in `net_errors.c`, but they wait for slice 5's join: its `route` hook should use them, and the run of "relay down" belongs to slice 5's tests. Until then the connection test reports a relay that is down.

### 8. Invite links
Blocked by: 5
Delivers: the join web page on the lobby address and the `cm0102://` link on Mac, Windows and Linux.
Merge danger: one-way door on players' computers, because the game registers a link handler. The Opus review rule applies.
- [ ] A click on the link joins the game on each platform. Today the link reaches the game and the game finds it in the lobby on each platform. The join itself is slice 5's call `invite_join`.
- [x] The page works for a visitor who does not have the game.

As built (2026-10-27, branch `claude/online-s8-invite-7d774c`, cut from `online-0.7.0` with slice 4b merged in, because the lookup uses the lobby client of slice 4b): the merge danger stays a one-way door on players' computers. Files: `server/src/join.ts`, `server/src/index.ts` (the `/join/<code>` route), `server/test/join.test.ts`, `server/README.md`, `src/recomp_rt/win/invite.c` and `invite.h`, `src/recomp_rt/win/lobby.c` and `lobby.h` (one game by its code), `src/recomp_rt/win/main.c` and `user32.c` (the link from the command line and from a drop event), `src/recomp_rt/host/link.c` and `host.h`, `tools/mac/Info.plist.in`, `tools/mac/check_package.py`, `packaging/windows/installer.nsi`, `tests/net/test_invite.c`, `test_link.c` and `link_container.sh`, `CMakeLists.txt`.

The game code is the lobby's 8-character code of slice 3. The page is `https://<lobby address>/join/<code>`. It needs no game, has no script, cookie or outside file, and logs and stores nothing (Q26).

Where the link handler is, and how a player removes it:

| Platform | Where | Written | Removed |
|---|---|---|---|
| macOS | `CFBundleURLTypes` in the app's `Info.plist`. The game writes nothing. | macOS reads it when the app is copied or first opened. macOS ignores an app in a temporary folder. | Move the app to the Bin. |
| Windows | The key `HKEY_CURRENT_USER\Software\Classes\cm0102`: a name, `URL Protocol` and `shell\open\command` = `"<program>" "%1"`. Current user only, no administrator rights, no installer step. | At each start while Online play is on, only when the command differs (a moved or new install). | At a start with Online play off, when the key starts this program. The uninstaller removes it when it starts that copy, for the account that runs the uninstaller only. By hand: `reg delete HKCU\Software\Classes\cm0102 /f`. |
| Linux | `cm0102-bosman-edition.desktop` in `~/.local/share/applications` (or `$XDG_DATA_HOME/applications`), hidden from menus, with `MimeType=x-scheme-handler/cm0102`. Only the AppImage writes it (`$APPIMAGE`), and `update-desktop-database` refreshes the folder when the system has it. | At each start while Online play is on, only when the file differs (a new AppImage path). | At a start with Online play off, when the file starts this AppImage. By hand: delete the file, then run `update-desktop-database ~/.local/share/applications`. |

A test run (`RECOMP_USER_DIR` set) writes and removes nothing.

How the link reaches the game: on Windows and Linux the system starts the program with the link as its only argument. `main.c` gives it to `invite_open` and passes no argument to the game's command line. On macOS the system sends an Apple event, also to a game that runs, and SDL gives it as a drop event (`user32.c`). With Online play on, the runtime asks the lobby for the game (`GET /v1/games/<code>`) on the lobby thread, and `invite_poll` gives a game that is open to `invite_join`. A second link before the answer drops the answer of the first. That function is empty: slice 5 starts its join there. With Online play off, the game does not ask the lobby.

Tests: `server/test/join.test.ts` (11 tests, 152 in the server suite); ctest `invite` (link text, the answer for one game, a lookup in a fake lobby, Online play off) and `link`; `tests/net/link_container.sh` in a linux/amd64 container: both tests built with gcc, the desktop file passes `desktop-file-validate`, `gio open` and `xdg-open` on GNOME start a fake AppImage with the link, and `link_test` built with mingw-w64 under wine writes, keeps another program's key and removes the key. On this Mac, a small SDL app with the same `Info.plist` entry got `cm0102://join/<code>` as a drop event, at its start and while it ran (the app was unregistered after the test).

Open points:
- `xdg-open` of xdg-utils 1.1.3 (Ubuntu 22.04) on a desktop that it does not know (no GNOME, no KDE) takes `cm0102:` for a file name, because its URL test allows no digits in a scheme. GNOME, KDE and `gio open` work. The page's text for a button that does nothing covers this.
- One copy takes the links (Q41, branch `claude/online-s8-handoff`, `src/recomp_rt/host/link_pass.c`). On Windows and Linux a click while the game runs starts a second copy. That copy gives the link to the copy that runs and ends, before it reads the settings or opens a window (`main.c`). The copy that runs opens the link as a macOS link event does, and brings its window to the front (`user32.c`). The channel belongs to the user and the user folder. On Windows it is a named pipe `\\.\pipe\cm0102-link-<user SID>-<hash>` that only this user can open, with no remote clients, and the second copy checks that the pipe's process runs as the same user. Elsewhere it is a Unix socket (`$XDG_RUNTIME_DIR/cm0102-link-<hash>.sock`, else `link.sock` in the user folder) that only its user can connect to. A lock file beside it (`.lock`) stops two copies that start at once from removing each other's socket, and a copy removes only its own socket at the end. It uses no network address and not port 6667. The copy that runs answers when it keeps the link and opens it at its next event poll. A second link before that poll replaces the first (the last link wins). A client that sends nothing is dropped after 0.5 s. When no copy runs, or the copy that runs does not answer within 2 s, the start works as before. macOS uses the same channel for a start from the command line, so the two-start test runs on this Mac. A click on macOS still comes as an Apple event. Tests: ctest `link_pass` (one process); `link_container.sh` runs it with gcc and under wine, also as two processes; two starts of the game on this Mac.
- Needs a real Windows machine: a click in Edge and Chrome (their "open this app" question), the key with a real install under Program Files, and the uninstaller's removal, also when a standard user types another account's administrator password (the key of the player then stays).
- Online play is on by default, so each Windows and Linux (AppImage) player gets the handler at the first start, also a player who never plays online.
- A real game start on this Mac with `cm0102://join/<code>/` in small letters as its argument found its game in the local lobby (`main.c` path). Not run in a real game: the drop event path of `user32.c` (the small SDL app above covers SDL's part).

### 9. Full online test
Blocked by: 6, 7, 8
Delivers: the four release tests in Testing, with results in `docs/TESTING.md`.
Merge danger: two-way door. Tests only.
- [ ] All four release tests pass, and every failure is fixed and tested again.
