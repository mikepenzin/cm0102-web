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
The season's data that a new game loads: the 2001/02 data of the free release (3.9.60), the 2001/02 December update of the official 3.9.65 patch (3.9.65) or the 2002/03 data of the official patch (3.9.68, the default). The option `database` names it. A download holds 3.9.60 and 3.9.65 in the folders Data-3.9.60 and Data-3.9.65 of the game folder.
_Avoid_: database version, data pack, DB

**Progress bar**:
The box that shows how far a long step has gone, such as "Loading database".
_Avoid_: loading screen

### Seasons

**Season**:
One football year from July to June, such as 2010/11, that a new game can start in.
_Avoid_: year (for the whole season), edition

**Season game**:
A game whose start season is not 2001/02, or whose changing rules are on. Only a season game gets the season names, club moves and other season changes. The base game (2001/02 with changing rules off) keeps the original's data.
_Avoid_: modern game, new-season game

**Choose Season screen**:
The screen from the start screen with one tile per season under era headings. The player picks the season, and so its database, there, and sets changing rules for the new game. Each season has one database (Q417).
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

**Rules table**:
The researched season rules of every league and competition, one row per season, each fact with its source.
_Avoid_: fact sheet

**Rules report**:
A test file that lists the competitions a game holds at the start of each season (file, year, club count, rounds, substitutes), for a comparison with the rules table.
_Avoid_: dump, season log

**Season jump**:
A test run that plays from season to season with no match engine, the original's quick results only, and writes the rules report.
_Avoid_: fast forward, simulation run

**Season change**:
The day on which a competition starts its next season: the update day of its league table entry, mostly 20 or 21 June, in January or February for calendar-year leagues. With changing rules on, the competition takes the rules of the new season then, and the player gets the news item of that season's changes.
_Avoid_: July step (in docs for players), rollover, new year

**Rule kind**:
One kind of season rule that the season change applies, such as competition names, with its own function in the list of `season_change.c`. Each build slice adds its kinds there.
_Avoid_: rule handler, plugin

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

### Options and features

**Membership**:
The paid plan of the browser game: Cloud Saves and the Scout. A member is an account that has it.
_Avoid_: Cloud Saves (for the plan), subscription, premium, sponsor

**Cloud Saves**:
The saved games of a member kept in the cloud and the same on every device where the member signs in. One part of Membership.
_Avoid_: cloud sync, backup

**Scout**:
The Editor's tab that finds a saved game's players by age, position, potential, ability and attributes. A free player sees the first 3 matches; a member sees the best 100.
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
