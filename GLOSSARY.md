# cm0102-mac

A native macOS port of Championship Manager 01/02, rebuilt from the original Windows program.

## Language

### Programs

**Original**:
The shipped Windows program, cm0102.exe. It is the reference for every behaviour.
_Avoid_: binary, the exe, the game

**Recompiled game**:
The original's code translated instruction by instruction into C, with a runtime that replaces the Windows calls. It is the program that every release ships.
_Avoid_: recomp, emulator, the port

**Hand port**:
The clean C program in this repo that rewrites parts of the original by hand and is checked against it by the differential test.
_Avoid_: port (alone), remake, clone, wrapper

### Distribution

**Bosman Edition**:
The product name, written in full as "CM 01/02: Bosman Edition", after the free transfers that older CM players know. It is an unofficial port of Championship Manager 01/02.
_Avoid_: Championship Manager (as the product name), Re-Signed Edition (the old name), CM Modern Edition

**Free release**:
The CD image that Eidos made free to download in 2008. It holds version 3.9.60 of the original and its data.
_Avoid_: freeware, ISO, base game

**Official patch**:
The 3.9.68 update from Sports Interactive, the last official update. It changes the original and its data.
_Avoid_: latest patch, update

**Clean files**:
The free release with the official patch applied, and nothing else. They are the only input to a release. The 3.9.60 data set comes from the same free release.
_Avoid_: vanilla, original files

**Fan pack**:
A community collection that adds a loader, patches or updated data to the game. It is never an input to a release.
_Avoid_: mod pack, starter kit

**Release**:
One numbered version of the Bosman Edition, published with a tag and the downloads for Mac and Windows.
_Avoid_: build, drop

**Download**:
The file that a player gets from a release: the recompiled game with the clean files, ready to start.
_Avoid_: package, installer, bundle

### Display

**Widescreen mode**:
The game drawn at 1280x800. It is the default.
_Avoid_: wide mode

**HD mode**:
The game drawn at 1920x1080, in 0.4.0 to 0.5.0 only (`display = hd`). Removed after 0.5.0 (Max, 2026-10-16, Q267): widescreen looks better on large screens. An old `display = hd` gives widescreen.
_Avoid_: larger text, 4K mode

**Classic mode**:
The game drawn at 800x600, as the original draws it.
_Avoid_: 4:3 mode, original mode

**Sharp bilinear**:
The scaling of `sharp_picture = on` from 0.6.0. The game draws its picture with nearest pixels at a whole number of times its size, then scales that to the window with linear filtering. Letters keep an even shape at every window size.
_Avoid_: whole pixels, integer scaling

**Display report**:
The file display-report.txt in the save folder, from 0.6.0. It lists the system, the screens and their scale, the window and how the game scales its picture, and the last 10 window changes. The game writes it at start and at quit. Ctrl+Shift+D (Cmd+Shift+D on a Mac) writes it again and copies it to the clipboard. Players add it to reports of picture and window problems.
_Avoid_: diagnostics, debug log, system info

### Screen

**Screen manager**:
The single object that owns every area and widget on the current screen and draws them in order. The original keeps it in display.cpp.
_Avoid_: scrman, display, UI manager

**Screen stack**:
The list of screens that a player has open, with the newest on top. Each screen names the functions that build it, handle its input and close it. The original keeps it in scrman.cpp.
_Avoid_: scrman, screen history

**Area**:
A rectangular region of the screen, split into a grid of columns and rows. An area can scroll its rows.
_Avoid_: window, pane, region

**Widget**:
One control on the screen, such as a text cell, a button or a picture. A widget sits in a grid cell of an area or stands free.
_Avoid_: control, cell

**Popup area**:
An area that belongs to a widget and opens next to it.
_Avoid_: dropdown, menu

**Panel**:
The background and border of an area or widget.
_Avoid_: frame, box

**Glyph table**:
A small one-colour picture, such as a scroll arrow, stored as one byte per pixel.
_Avoid_: icon, sprite

### Text

**Bitmap font**:
One of the seven fonts that the original loads from its .fnt files and uses when its font setting is -1.
_Avoid_: raster font, FNT font

**Glyph**:
The picture of one character in a bitmap font.
_Avoid_: character image, letter

**Mark**:
A small picture that a widget draws beside its text: the tick or one of the three arrows.
_Avoid_: icon, symbol

**Text entry**:
A widget that takes typed text, such as the first name of the manager. A click, Tab or Enter starts or ends the editing.
_Avoid_: input field, edit box, text field

### Verification

**Differential test**:
A test that runs one function of the original in an emulator, runs the same function of the hand port, and compares the results.
_Avoid_: emulation test, golden test

**Tier A run**:
A test run to a far year that plays only the competitions: calendar, draws, entrants, league tables, season changes, history, finances, retirements, new players and contracts. Results come at once from club and nation reputation. It skips the transfer market, background matches, new injuries and AI manager moves. A full AI club lets its weakest players over 21 go, because Tier A has no sales, and an AI club of a played league with fewer than 15 players runs its own market, because Tier A has no signings. The Tier A switch `RECOMP_TIER_A=1` turns it on.
_Avoid_: fast mode, fast-forward mode, competition-only run

**Tier B run**:
A test run to a far year with the full game world, to check player pools, managers, money, save size and speed over many decades.
_Avoid_: world run, full run

**Date jump**:
A test start of a new game in a far year (2098, 2198 or 2298), with every date of the database moved to that year.
_Avoid_: time jump, far start

**Run set**:
The list of test games in `run_set.tsv`, in three sets: the release set, the background soak and the quick games that test the script.
_Avoid_: matrix, test grid

**Release set**:
The fixed games of the run set that a release must pass, from 0.7.1 on. Ten games on the Mac finish it in about 12 hours.
_Avoid_: release run set, gate set

**Spine soak**:
A full-world game of the release set from one of eight starts, played to its World Cup and Euro end point. Together the eight play every season from 2001/02 to 2029/30 once.
_Avoid_: era soak, long soak

**Tile row**:
A full-world game of the release set for one start season tile, with Select All and every extra division, played through the first season change to 1 September.
_Avoid_: short soak, start check

**Background soak**:
The run set games that play only while the Mac is idle and hold no release: Tier A to 2300 and the full world from 2025 to 2100.
_Avoid_: nightly run, long run

**Extra division**:
A lower division that the player adds on the Select League(s) screen: the English Conference, the German Regionalliga, Serie C2, the Spanish Segunda B, the Portuguese Second Division B or the Swedish Second Division. Select All sets none of them.
_Avoid_: sub league, optional league, lower league

**Checkpoint save**:
A save that a Tier A or Tier B run keeps at a set year, so a later test can start there.
_Avoid_: snapshot, milestone save

**History checker**:
The test tool that reads a save and checks the history rows and entrants of every competition against the expected rules.
_Avoid_: history audit, save checker

### Data

**Nation table**:
The nations of the original's database, loaded from nation.dat, one record for each nation.
_Avoid_: country list

**League table**:
The 35 leagues that a new game can include, with the nation of each and whether it is selected.
_Avoid_: competition list, division table

**Key nation**:
A nation that the original finds by name after the load, such as England for the default league.
_Avoid_: main nation

**Data set**:
The season's data that a new game loads: the 2001/02 data of the free release (3.9.60, the default since 0.6.1), the 2001/02 December update of the official 3.9.65 patch (3.9.65) or the data of the official patch, which holds the summer 2002 transfers (3.9.68, the data of the base game). The option `database` names it. Each click on the chosen 2001/02 tile of the Choose Season screen moves to the next data set, and Start writes it into the option. A download holds 3.9.60 and 3.9.65 in the folders Data-3.9.60 and Data-3.9.65 of the game folder.
_Avoid_: database version, data pack, DB

**Progress bar**:
The box that shows how far a long step has gone, such as "Loading database".
_Avoid_: loading screen

### Seasons

**Season**:
One football year from July to June, such as 2010/11, that a new game can start in.
_Avoid_: year (for the whole season), edition

**Season game**:
A game whose start season is not 2001/02, or whose changing rules are on. Only a season game gets the season names, club moves and other season changes. The base game (2001/02 with changing rules off) keeps the original's data, except the history rows of editions after its start (0.6.2, Q48).
_Avoid_: modern game, new-season game

**Choose Season screen**:
The screen from the start screen with one tile per season under era headings. The player picks the season, and so its database, there, and sets changing rules for the new game. Each season has one database (Q417), except 2001/02, whose tile has the three data sets (0.6.1).
_Avoid_: season picker, year select

**Season button**:
The left button of the top row of Setup Game: the season of the next new game and its database. A click opens the Choose Season screen. The right button of the row shows changing rules On or Off, and a click switches it (Q422).
_Avoid_: season strip, season bar, Choose Season item

**Extras screen**:
The screen from the Extras button of Setup Game with the original's Hall Of Fame, Game Credits and Web Sites.
_Avoid_: more, other options

**Season database**:
The Update Team database that a season uses, the earliest of that season unless players report it as faulty.
_Avoid_: data update, DB

**Seasons folder**:
The folder `Seasons` in the game folder of a download. It holds one zip for each season database, named by the neutral name of the database, for example `October 2003.zip`. The first start of a season unpacks its zip into `Databases` of the save folder.
_Avoid_: season pack, database zips

**Extra database**:
An Update Team database that is not a season database, such as an April or SIM version. The Choose Season screen does not offer them (Q417).
_Avoid_: more versions, alternative database

**Custom database**:
A database folder that the player adds and that is not a known Update Team database. The player names its season.
_Avoid_: own data, mod database

**Update Team**:
The CM 01/02 Update Team at champman0102.co.uk, who make the updated databases.
_Avoid_: modders, community team

**Era**:
A run of seasons with the same European cup format, shown in one colour on the Choose Season screen.
_Avoid_: period, generation

**Season rules**:
The rules and league structures of one season: club counts, promotion and play-off places, cup formats, substitutes, foreign player rules, transfer windows and names.
_Avoid_: restructure, year patch

**Rule option**:
An option that changes how the game plays, for example no foreign player limits. It is off by default, is set for each new game and stays fixed for the whole game. It is never a season rule.
_Avoid_: rule (alone), cheat, patch

**Rules tab**:
The third tab of the Options box, beside Graphics and Game, with one row per rule option. It sets the default for a new game. The Rules button of Setup Game opens it for that game.
_Avoid_: rules screen, settings

**Rules table**:
The researched season rules of every league and competition, one row per season, each fact with its source.
_Avoid_: fact sheet

**Rules report**:
A test file that lists the competitions a game holds at the start of each season (file, year, club count, rounds, substitutes), for a comparison with the rules table.
_Avoid_: dump, season log

**Season jump**:
A test run that plays from season to season with no match engine, the original's quick results only, and writes the rules report.
_Avoid_: fast forward, simulation run

**After-table rule**:
What a competition or league does in a season after the last row of its rules table: it continues on its last cycle with its last rules, with no end year.
_Avoid_: fall-back, default rule

**Idle year**:
A year that a competition with no next edition was set to wait for (2200). Since 0.6.2 no competition uses it.
_Avoid_: never year, sentinel year

**Season change**:
The day on which a competition starts its next season: the update day of its league table entry, mostly 20 or 21 June, in January or February for calendar-year leagues. With changing rules on, the competition takes the rules of the new season then, and the player gets the news item of that season's changes.
_Avoid_: July step (in docs for players), rollover, new year

**Rule kind**:
One kind of season rule that the season change applies, such as competition names, with its own function in the list of `season_change.c`. Each build slice adds its kinds there.
_Avoid_: rule handler, plugin

**League size row**:
The row of the rules table that gives a league its club count, meetings, halves, zones, splits and periods for one season. In a season game the shared engine builds the table and the fixture dates from it (0.7.2). The base game keeps the class's counts.
_Avoid_: league class count (that is the original's count), league format

**Size step**:
The change at a season change that brings a division to the club count of its new row. It moves the extra or missing clubs by the real promotion and relegation places of the rows. It runs only with changing rules on (0.7.3).
_Avoid_: resize, league expansion

**Closed league**:
A league with no division below it, such as MLS or the A-League. At a season change it loses its folded clubs and takes its new clubs by the closed league table, only with changing rules on (0.7.3).
_Avoid_: franchise league, expansion league

**Cross play-off**:
A tie between a club of an upper division and a club of the division below it, for one place in the upper division, such as J1's 16th against J2's 3rd or the Portuguese tie of the Primeira Liga and the Segunda Liga.
_Avoid_: promotion/relegation play-off (that can be a play-off inside one division), barrage

**Play-off over four**:
A relegation play-off whose winner meets the winner of a play-off of four clubs of the division below, such as in Norway, Ireland, J1 and J2, and Ligue 2 and Ligue 3 (0.7.3).
_Avoid_: five-club play-off (in docs for players), ladder

**League phase pot**:
One of the groups of clubs that the league phase draw of a UEFA competition takes its opponents from, from 2024/25. The pots follow club reputation, highest first (0.7.3).
_Avoid_: seeding band, coefficient pot

**Bench**:
The named substitutes of a match. The rules table column `bench` holds the count. The match engine holds at most 9.
_Avoid_: reserves, subs list

**Substitutes used**:
The substitutes a team may bring on in a match. The rules table column `subs_used` holds the count.
_Avoid_: changes, swaps

**Transfer window**:
The days of a year on which a club may sign players, kept by nation in the game's transfer rules.
_Avoid_: registration period (a research text uses it for the same thing)

**Nation rules**:
The windows, foreign player limits and work permit rule of one nation in one season, as the encoder `tools/seasons/encode_rules.py` writes them.
_Avoid_: transfer rules (that is the game's own object)

**Changing rules**:
The option that lets the season rules change at each season change (mostly in June) as a game moves through the years. It is set for each game.
_Avoid_: evolving rules, dynamic seasons

**Base game**:
The 2001/02 season with the 3.9.68 data set and changing rules off. It plays as the original.
_Avoid_: vanilla, classic game

**Start year**:
The year in which a game's first season starts, such as 2010 for a game that starts in 2010/11. The base game has 2001.
_Avoid_: start season (for the number), base year

**Season part**:
The part `bosman_season.dat` of a saved game that holds its start year and changing rules. A save has it only when the start year is not 2001 or changing rules is on.
_Avoid_: season block, save field, header

**Merged part**:
A part of a saved game that holds all temporary files of one type, such as every `fixtures_<year>.tmp`. Its name is the type with `.parts` at the end, such as `fixtures_#.tmp.parts`. A type with one file stays a part of its own.
_Avoid_: pack, bundle, archive part

### Options and features

**Membership**:
The paid plan of the browser game: Cloud Saves and the Scout. A member is an account that has it.
_Avoid_: Cloud Saves (for the plan), subscription, premium, sponsor

**Level**:
What an account has on the browser game, lowest first: Free, Supporter, Squad Player, Club Legend; each includes everything below it. Squad Player and up have Membership. A paid level is the Buy Me a Coffee level of the account's email; an admin can give Squad Player or Club Legend.
_Avoid_: tier, plan (for the level), rank

**Club Legend**:
The highest level: Membership and early access.
_Avoid_: VIP, top tier

**Early access**:
A new feature or a new Library database that only Club Legends have until its day; then everyone has it under its usual rules.
_Avoid_: beta, preview

**Cloud Saves**:
The saved games of a member kept in the cloud and the same on every device where the member signs in. One part of Membership.
_Avoid_: cloud sync, backup

**Squad Hub**:
The page's dialog for the players of a saved game (Players: view and change them; Scout) and for data sets for new games. Its bar button was called Editor.
_Avoid_: Editor (for the dialog), tools

**Scout**:
Squad Hub's tab that finds a saved game's players by age, position, potential, ability and attributes. A free player sees the first 3 matches; a member sees the best 100.
_Avoid_: search (for the Scout), player finder, Genie Scout

**Settings file**:
The file settings.txt in the save folder. Each line sets one option, such as `display = classic`. The options screen writes to the same file.
_Avoid_: config, preferences file, ini

**Option**:
One line of the settings file that switches a feature on or off or sets its value.
_Avoid_: preference, flag, toggle

**Options screen**:
The Bosman Edition Options screen in the game. It opens from the start screen and from Game Options in the side bar, and it changes options at once.
_Avoid_: preferences, settings menu

**Assistant selection**:
The assistant manager picks the player's team, from the Assistant drop-down on the squad screen (0.3.0, #12): best team, most match-fit team or under-21 team. It scores each player by the average of the key shown attributes for the position times positional fit, and finds the eleven with the highest total (0.3.1).
_Avoid_: auto pick, auto team, AI selection

**Save backup**:
An older copy of a save, kept in `backups/` in the save folder. The game keeps three for each save.
_Avoid_: autosave, snapshot

**Clickable name**:
A person's or club's name drawn on a screen that opens their profile when the player clicks it. It shows an underline only while the mouse is over it.
_Avoid_: link, hyperlink

**Attribute colour**:
The colour of an attribute number by its value on the 1 to 20 scale, as in modern Football Manager.
_Avoid_: attribute highlighting, heatmap

**Team views screen**:
The screen with the Best XI and Compare players tabs (Squad depth was a third tab until 0.6.0), opened from the Assistant drop-down on any club's squad screen (0.4.0, #22). It only shows players and never changes the team.
_Avoid_: team screen, squad analysis

**Best XI**:
The eleven that the assistant selection would pick for a formation, drawn on a pitch with a card for each slot. Each card also shows the next two players for that position, and stars show each player's score.
_Avoid_: best eleven, ideal team, first XI

**Squad depth**:
The top three players for each position of a formation. Since 0.6.0 the Best XI cards show it, and the separate tab is gone.
_Avoid_: depth chart, cover

**Polygon analyser**:
The eight-axis shape of a player's shown attributes on the comparison screen, with the two players' shapes overlapped.
_Avoid_: radar, spider chart

**Attribute groups**:
The player profile's attributes in Technical, Mental and Physical columns with headings, and a Goalkeeping group for goalkeepers (option `attribute_groups`).
_Avoid_: attribute categories, sections

**Hidden value**:
A player value in the data that no screen of the original shows, such as current ability or potential ability. No feature shows it or feeds it into a shown number.
_Avoid_: secret attribute, intrinsic (for the whole set)

**Printout**:
The text lines that the original makes from a print button, on the squad and competition screens. The PDF choice of Print To writes it to a PDF in the `Prints` folder.
_Avoid_: report, export, screenshot

### Network play

**Network game**:
A game that several human managers play together, each on their own computer. One computer hosts it, and the others join it over a LAN, a VPN or online (0.7.0, #45).
_Avoid_: multiplayer game, online game (for the game itself), session

**Host**:
The computer that runs a network game. Only the host runs the match engine and keeps the save. The other computers show the screens that the host sends.
_Avoid_: server (for the host's computer)

**Human manager**:
A person who takes part in a network game and controls one club.
_Avoid_: player, client, user (inside a network game)

**Game list**:
The original's Select Network Game screen. It lists each open network game on the LAN and online, with Join.
_Avoid_: lobby (for the screen), server list, browser

**Lobby**:
The Bosman Edition server that keeps the list of open online games. A host tells the lobby about its game, and the game list asks the lobby for it.
_Avoid_: matchmaker, master server

**Relay**:
The Bosman Edition server that passes traffic between a host and its human managers. No router setting is needed. It keeps no game data.
_Avoid_: proxy, tunnel, TURN

**Invite link**:
A web link to one network game. It opens a page with a button that starts Bosman Edition and joins that game.
_Avoid_: join code, deep link

**Game code**:
The 8 characters that name one online network game in the lobby, for example K7M2Q9RT. The invite link holds it, and the relay uses it.
_Avoid_: join code, room code

**Always-on game**:
A network game that a host leaves open for a long time, so that human managers can join and leave when they want. It moves forward only when a human manager clicks Continue.
_Avoid_: persistent league, server game

**Away**:
The state of a human manager who left a network game. Their club is on holiday as in the original. They can take it back with their manager password.
_Avoid_: offline, AFK, dropped

**Game password**:
The optional password of a network game. The host sets it on the Network tab of the Bosman Edition Options box, and a human manager types it in the password box to join. It is not the manager password, which protects one club.
_Avoid_: server password, join code

**Kick**:
The host removes a human manager from a network game, from the list on the Network tab. Their club goes on holiday, as when they leave.
_Avoid_: ban, boot, remove (for this action)

**Online play**:
The option that lets the game list show online games and lets a host put its game in the lobby. It is on by default. When it is off, the game shows LAN games only.
_Avoid_: internet mode, online mode

**Connection test**:
A check on the Network tab of the Bosman Edition Options box that the lobby and relay can be reached. It says that online play works, or it says what is wrong.
_Avoid_: network check, diagnostics

**Ping**:
The time in ms for a message to go from the host to a human manager's game and back. The Network tab shows it for each human manager online, and the game list row shows the highest one of the game.
_Avoid_: latency, lag

### Research maps

**Function map**:
The list of every function of the original with a name, a source file, a confidence and the evidence for the name.
_Avoid_: name map, symbol table

**Data map**:
The list of every global of the original that a function touches, with a name, a size, a type, its readers and writers, and a confidence.
_Avoid_: globals map, variable map

**Global**:
A fixed address in the original's data that one or more functions read or write.
_Avoid_: variable, static, data symbol

**Address group**:
A run of adjacent globals that the data map names as one object, usually one struct or one table.
_Avoid_: cluster, block, region

**Owner file**:
The source file of most of a global's readers. The data map lists the global on that file's page.
_Avoid_: home file, parent file

**Quote**:
Evidence that pins the purpose of a function or global: a string, an assert, a caller, a vtable slot, a high global or an open source match.
_Avoid_: proof, hint (for a quote)

**Confidence**:
High, medium or low for each name in the maps. High needs a quote. Medium and low names are hints.
_Avoid_: certainty, score

**Dossier**:
The file of facts for one function or global that an agent reads before it names it. Dossiers stay outside the repo.
_Avoid_: brief (for a dossier), context file
