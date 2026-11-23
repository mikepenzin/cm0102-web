<p align="center">
  <img src="https://maxevans60.github.io/cm0102-bosman-edition/assets/logo-readme.png" alt="CM 01/02 Bosman Edition" width="380">
</p>

<h1 align="center">CM 01/02: Bosman Edition</h1>

<p align="center">
  Championship Manager 01/02, rebuilt to run natively on Mac, Windows and Linux.<br>
  <a href="https://maxevans60.github.io/cm0102-bosman-edition/"><b>Download</b></a> ·
  <a href="https://github.com/maxevans60/cm0102-bosman-edition/releases">All releases</a> ·
  <a href="CHANGELOG.md">Changelog</a> ·
  <a href="https://github.com/maxevans60/cm0102-bosman-edition/milestones">Roadmap</a> ·
  <a href="https://github.com/maxevans60/cm0102-bosman-edition/issues/new/choose">Report a problem</a>
</p>

<p align="center">
  <a href="https://github.com/maxevans60/cm0102-bosman-edition/releases"><img alt="Latest release" src="https://img.shields.io/github/v/release/maxevans60/cm0102-bosman-edition?include_prereleases&label=release"></a>
  <a href="https://github.com/maxevans60/cm0102-bosman-edition/releases"><img alt="Downloads" src="https://img.shields.io/github/downloads/maxevans60/cm0102-bosman-edition/total"></a>
  <img alt="Platforms" src="https://img.shields.io/badge/platforms-macOS%20%7C%20Windows%20%7C%20Linux%20%7C%20Steam%20Deck-blue">
  <a href="https://github.com/maxevans60/cm0102-bosman-edition/commits/main"><img alt="Last commit" src="https://img.shields.io/github/last-commit/maxevans60/cm0102-bosman-edition"></a>
</p>

<p align="center">
  <img src="https://maxevans60.github.io/cm0102-bosman-edition/assets/post-060-choose-season.png" alt="The Choose Season screen, with a tile for each season" width="49%">
  <img src="https://maxevans60.github.io/cm0102-bosman-edition/assets/s1@2x.png" alt="The squad screen in widescreen" width="49%">
</p>

## What it is

The 2002 game, version 3.9.68, translated function by function from the original Windows program into C and compiled for each platform. There is no emulator. The match engine, transfers and every game decision are the 2002 code.

Each download is ready to play. It includes the game files from the free release Eidos published in 2008, with the last official patch. No CD and no Wine.

## Features

- macOS 11 or later (Apple silicon and Intel), Windows 10 and 11 (64-bit) and 64-bit Linux. The AppImage also runs on the Steam Deck in desktop mode.
- Widescreen at 1280 × 800 in a resizable window with sharp scaling, or the original 800 × 600 layout.
- Any season from 2001/02 to 2026/27, each with its own database, leagues, cups, European and international competitions, substitute rules, foreign player rules and transfer windows.
- In a long game the rules change each July as they did in real life, with a news item that lists the changes. You can switch this off.
- The 2001/02 tile has three versions of the official data: 3.9.60 (the default), 3.9.65 and 3.9.68.
- A Custom database tile for your own database folder.
- Best XI and squad depth on one screen, and an Assistant that picks the best, fittest or under-21 team.
- Clickable player and club names, coloured attributes, condition and morale.
- Holiday until the next match, faster holiday days and a Fastest commentary speed.
- Save names with your club or national team and the date, with the last three copies of each save kept as backups.
- Keyboard shortcuts, and a window that reopens where you left it.

Every feature is a line in `settings.txt` in the save folder and can be switched off. In the base game (2001/02, 3.9.68, changing rules off) no feature changes a match result. The [site](https://maxevans60.github.io/cm0102-bosman-edition/#questions) lists every setting and its default.

## Install

| | |
|---|---|
| Mac | Open the `.dmg` and drag the game to Applications. If macOS says it cannot check the app, open System Settings, then Privacy & Security, and click Open Anyway. |
| Windows | Run `Setup.exe`. If Windows shows "Windows protected your PC", click More info, then Run anyway. |
| Linux and Steam Deck | Download the `.AppImage`, allow it to run as a program in its Properties, and double-click it. On the Steam Deck, use Desktop Mode. |

Saves are in `~/Library/Application Support/cm0102` (Mac), `%APPDATA%\cm0102` (Windows) and `~/.local/share/cm0102` (Linux). Updates do not touch them. The Open folder button on Load Game opens the folder.

## Play in the browser

This fork also builds the same recompiled game as WebAssembly, to play in Chrome with nothing installed. A small Node
app hands the browser the engine and the game files; the browser runs the game, and saves stay in the browser.

```bash
emcmake cmake -S . -B build-web -G Ninja -DCM0102_EXE=$HOME/cm0102-game/cm0102.exe
cmake --build build-web --target cm0102_recomp seasons_rules
CM_GAME_DIR=$HOME/cm0102-game node web/server.mjs        # then open http://127.0.0.1:8080 in Chrome
```

Every step, from the tools to the game files: [web/README.md](web/README.md). On the internet, on Fly.io:
[web/deploy/README.md](web/deploy/README.md).

## Roadmap

The current release is 0.7.3. Next is 0.7.4, with the play-offs, splits and cups that 0.7.3 does not play yet. Then 0.8.0 brings the features players asked for most, with a Rules tab of options. Then 0.9.0 brings Android. The [changelog](CHANGELOG.md) lists every release, and the [milestones](https://github.com/maxevans60/cm0102-bosman-edition/milestones) show what each version holds.

## How it works

The original program is about 9,400 functions of 32-bit x86 code. The recompiler in [`tools/recomp`](tools/recomp) translates each function into C. A small runtime in [`src/recomp_rt`](src/recomp_rt) replaces the Windows calls (windows, drawing, files, sound, input) with SDL2. Fixes and features hook the game at checked addresses, and the build stops if a hooked address no longer matches the original. Tests play the game from a new game to the first news, play matches, and compare saved games byte for byte.

## Build from source

This section is for developers. To play, use the downloads above, which include everything.

The source code in this repository holds no part of the original game, so a build from source needs your own copy of the 2008 free release (`CM0102.iso`) and the official 3.9.68 patch (`cm0102_3968.exe`).

```bash
python3 -m pip install capstone
python3 tools/clean_files/make_clean_files.py CM0102.iso cm0102_3968.exe ../clean-3968
cmake -S . -B build -DCM0102_EXE=../clean-3968/cm0102.exe -DRECOMP_PYTHON=python3
cmake --build build -j 8 --target cm0102_recomp
```

The first command checks each file against [`tools/clean_files/manifest.tsv`](tools/clean_files/manifest.tsv) and needs `bsdtar` and `unshield`. The build needs CMake 3.20, a C compiler, Python 3 with `capstone`, and SDL2. [`tools/release/make_release.sh`](tools/release/make_release.sh) makes the Mac, Windows and Linux downloads.

## Report a problem

Open an [issue](https://github.com/maxevans60/cm0102-bosman-edition/issues/new/choose) with your computer, the game version and what you did before it happened. For a display problem, press Ctrl+Shift+D in the game (Cmd+Shift+D on a Mac) and paste the report it copies to the clipboard.

## Credits

Sports Interactive made Championship Manager 01/02. Eidos published it and made it free in 2008. The CM 01/02 Update Team at [champman0102.co.uk](https://champman0102.co.uk) makes the yearly databases, and the download includes 21 of them.

## Legal

The Bosman Edition is an unofficial fan project. It is not affiliated with or endorsed by Sports Interactive, SEGA, Eidos or Square Enix. Championship Manager is a trademark of Square Enix. The downloads include the game files from the 2008 free release.

The code in this repository is released under the [GNU General Public License v3.0](LICENSE) from 0.6.0, and under CC0 1.0 up to 0.5.0. The licence covers only the code written for this project, not the original game, its data, its logo or the Update Team databases, which belong to their owners. The function map (`docs/research/function-map/`) and the data map (`docs/research/data-map/`) are under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/), as in their own repository, [cm0102-function-map](https://github.com/maxevans60/cm0102-function-map).
