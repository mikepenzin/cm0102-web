# Save editor: the players of a saved game (2026-11-10)

Facts for the save editor of the browser build (`web/savedit.js`, `web/editor.js`). Source: `cm0102.exe` of the clean
files (SHA-256 `0c78e7da…bff86`, `tools/clean_files/manifest.tsv`), read with capstone, and one saved game of the
3.9.68 data (a Juventus game saved on 29.7.01, 60,621,406 bytes, version 4). The game itself was not run for this note.

## The container

The format of `tools/save/savfile.py` (zip.cpp): a header of 0x416ec bytes, then the members. Version 4 compresses
each member with the byte run code of the decoder 0x932c30 and the encoder 0x932e10. The editor writes a member it did
not change with its bytes as they were, so a save opened and written with no change is the same file (checked on the
Juventus save: identical, 60,621,406 bytes).

## The database in the save

The save holds 129 members. `index.dat` has the 22 entries of 0x43 bytes that the loader reads (read_index,
0x528734..0x528834), then other bytes (the save writer 0x52dad0 writes 0x1cc bytes of its stack there). Each table is
its own member, rows from offset 0; the entries of Preferences.dat, nonplayer.dat and player.dat have offsets that the
member does not use (each member is exactly count × row size). The tables the editor reads, with the counts of the
Juventus save:

| Table | Member | Row | Rows |
|---|---|---|---|
| 6 staff (format 2) | staff.dat | 110 | 71,263 |
| 10 player | player.dat | 70 | 54,772 |
| 13, 14, 15 names | first_names.dat, second_names.dat, common_names.dat | 60 | 44,297; 104,411; 13,272 |
| 0, 1 clubs | club.dat, nat_club.dat | 581 | 7,171; 426 |
| 4 nations | nation.dat | 290 | 213 |

The staff rows are the 110-byte records that convert_staff (0x52caa1) makes from the 157-byte file rows; the offsets
are those of `staff_fields` in `src/cm3/database.c`: names at +0x04, +0x08, +0x0c, date of birth +0x10 (a Date.cpp
date, 8 bytes), year +0x18, nation +0x1a, club +0x39, player data row +0x61 (-1: none). A player row's id at +0 is its
row (54,772 of 54,772). Current ability i16 at +0x05 (1..190 in the save), potential i16 at +0x07 (1..200), twelve
position bytes at +0x0f (0..20), the 42 attribute bytes at +0x1b..+0x44 in the order of
`docs/research/2026-10-21-goalkeeper-attributes.md`.

## The shown attributes: 0x543540

`staff_compute_player_attribute(staff, player data, n)` takes n = 1..31 (0x543559..0x543565, else an error line and
0xff) and jumps through the table at 0x543a94 (31 entries, 0x5435b7). Each case:

| n | Name | Read | Kind |
|---|---|---|---|
| 1 | Acceleration | +0x1b | the byte (0x5435be) |
| 2 | Aggression | +0x1c | byte |
| 3 | Agility | +0x1d | byte |
| 4 | Anticipation | +0x1e | relative (0x54366f) |
| 5 | Balance | +0x1f | byte |
| 6 | Bravery | +0x20 | byte |
| 7 | Creativity | +0x43 | relative; a tenth of the ability when the goalkeeper rating +0x0f is 20 (0x5436db) |
| 8 | Crossing | +0x23 | as 7 (0x543722) |
| 9 | Decisions | +0x24 | relative (0x5435c8) |
| 10 | Determination | staff +0x58 | byte (0x54376c, `[edx+0x58]`, edx = the staff record) |
| 11 | Dribbling | +0x26 | as 7 (0x543776) |
| 12 | Finishing | +0x27 | 0x5442f0 when the goalkeeper rating is 20, else 0x544380 (0x54396c) |
| 13 | Flair | +0x28 | byte |
| 14 | Handling | +0x2a | relative; a tenth of the ability when the goalkeeper rating is below 15 (0x5437ca, `jge` 15) |
| 15 | Heading | +0x2b | relative (0x543814) |
| 16 | Influence | +0x2f | byte |
| 17 | Jumping | +0x2e | byte |
| 18 | Long Shots | +0x31 | 0x544380 (0x5439d6) |
| 19 | Marking | +0x32 | as 7 (0x543847) |
| 20 | Off The Ball | +0x33 | as 7 (0x543891) |
| 21 | Pace | +0x36 | byte |
| 22 | Passing | +0x37 | relative (0x5438e5) |
| 23 | Positioning | +0x39 | relative (0x543904) |
| 24 | Reflexes | +0x3a | as 14 (0x543923; 0x544380 when the rating is 15 or more) |
| 25 | Set Pieces | +0x29 | byte |
| 26 | Stamina | +0x3c | byte |
| 27 | Strength | +0x3d | byte |
| 28 | Tackling | +0x3e | 0x544380 (0x5439ae) |
| 29 | Teamwork | +0x3f | byte |
| 30 | Technique | +0x40 | byte |
| 31 | Work Rate | +0x44 | byte |

The relative conversion, inline (from 0x5435c8, shared tail 0x543689) and in 0x544380: the byte r as a signed byte and
the current ability a (i16 at +0x05) give x = (2r + a) × 0.05 + 10, then y = (x + x × x × 0.1) × (1/3) + 0.5
(constants 0x967888 = 0.05, 0x9688b0 = 10, 0x967880 = 0.1, 0x968a80 = 1/3, 0x968968 = 0.5); y ≤ 1 gives 1
(0x967890), y ≥ 20 gives 20 (0x968ea8), else y truncated (0x945640). 0x5442f0 is the same with a × 0.1 in place of a
(0x544309). The browser build computes x87 values as C doubles (`src/recomp_rt/rt.h`, `double st[8]`), so the editor's
JavaScript doubles in the same order give the same ratings.

The names are the game's English attribute names, alphabetical, which is the numbering of the cases (the names come
from 0x5428a0).

## In the data

Of the 42 bytes, exactly 18 are outside 1..20 for most players of the Juventus save (signed values from -125 to 100):
the 15 relative ones above and +0x35 One On Ones, +0x38 Penalties and +0x41 Throw Ins. These are the 18 cells that
0x538bc0 shifts when a game starts (goalkeeper attributes note). The other 24 are 1..20 for every player. The game does
not show One On Ones, Penalties and Throw Ins; the runtime shows One On Ones with 0x544380 (`src/recomp_rt/win/groups.c`),
and the editor shows the other two the same way (inference: they are stored as the others are).

## Writing

- A relative byte for a wanted rating: of the bytes -128..127 that give it with the new ability and positions, the
  middle one. Some ratings cannot be reached: with ability 195, Heading shows 4 at the lowest. The editor then writes
  the nearest and says so.
- A byte that already shows the wanted rating stays as it is, so a player changed in one attribute keeps every other
  byte.
- Dates as Date.cpp writes them (FUN_00549f70, tables 0x9b8448 and 0x9b8460, `src/si/date.c`): the day of the year from
  0, the year, and the leap flag. The year at staff +0x18 gets the year of birth too.
- Only staff.dat and player.dat change (checked with `tools/save/savfile.py` on an edited copy of the Juventus save).

## Not done

Contracts, clubs, names, reputations and the other staff bytes at +0x56..+0x5d (Determination is +0x58) are not
edited. Adding a player to a saved game is not attempted: the save holds many tables per person (contracts, history,
statistics) that a new row would need.
