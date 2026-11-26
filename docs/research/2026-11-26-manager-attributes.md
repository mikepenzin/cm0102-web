# A human manager's attributes in a saved game

26 November 2026. Asked by a player (the HumanManagerAttributes patch of CM 01/02: a manager's own attributes on
his profile). The game keeps them for every non-player and hides them for the human managers; Squad Hub shows them
(read only), as the Scout shows potential. Read from the clean 3.9.68 `cm0102.exe`; checked on the save "Hertha BSC
2011-12-11" (Klaus Kastelmann, staff 158853, and Mike Penzin, 158854).

## The twelve attributes the game shows

`database_get_non_player_attribute_name` 0x543b10 names attributes 1 to 12 (its jump table 0x543ef8, long and
three-letter names). `staff_compute_non_player_attribute` 0x543fd0 gives each one's 1 to 20 rating (its jump table
0x5442b4, `this` the staff row in edx, the non-player record in eax):

| # | Name | Byte | Rating |
|---|---|---|---|
| 1 | Adaptability | staff row +0x56 | as it is (0x54404f) |
| 2 | Coaching Goalkeepers | non-player +0x11 | relative (0x544059) |
| 3 | Coaching Outfield Players | non-player +0x10 | relative (0x5440ec) |
| 4 | Determination | staff row +0x58 | as it is (0x544144) |
| 5 | Judging Player Ability | non-player +0x17 | relative (0x54414e) |
| 6 | Judging Player Potential | non-player +0x18 | relative (0x54416a) |
| 7 | Level of Discipline | non-player +0x14 | as it is (0x544186) |
| 8 | Man Management | non-player +0x19 | relative (0x544190) |
| 9 | Motivating | non-player +0x1b | relative (0x5441af) |
| 10 | Physiotherapy | non-player +0x1e | relative (0x5441ce) |
| 11 | Tactical Knowledge | non-player +0x21 | relative (0x5441ed) |
| 12 | Working With Youngsters | non-player +0x22 | as it is (0x54420c) |

Relative: the signed byte r and the non-player's current ability a (int16 at +0x04) give
x = 0.05 x (2r + a) + 10, then (x + 0.1 x x x) / 3 + 0.5, cut to 1..20 (constants 0x967888 0.05, 0x9688b0 10,
0x967880 0.1, 0x968a80 1/3, 0x968968 0.5): the formula of a player's relative attributes, `rating` in
`web/savedit.js` (0x544380).

## Where the save keeps them

The member `nonplayer.dat`: 68-byte records, the record's id at +0 equal to its index (the Hertha save: 44,810
records, every sampled id at its index), current ability +0x04, potential +0x06 (int16). A staff row of a save
(110 bytes) names its non-player record at +0x69 (dataset.js: format 2). Klaus: record 44798, ability 200; Mike: 44799,
ability 200. Their ratings by the table: Klaus 13, 20, 18, 8, 17, 15, 5, 20, 20, 20, 20, 7; Mike 6, 20, 20, 11, 20, 20,
20, 20, 20, 20, 20, 20.

The human managers of a save: the places of `scout_manager.dat` with a name (docs/research/2026-11-25-shortlist-in-save.md).
