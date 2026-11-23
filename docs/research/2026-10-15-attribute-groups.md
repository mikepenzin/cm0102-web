# Attribute groups (#43): facts for the build

Slice 7 of `docs/specs/2026-10-15-features-0.4.0-spec.md`. Code: `src/recomp_rt/win/groups.c`, `groups_layout.c`, `tools/recomp/attribute_groups.py`.

## The profile build 0x87a510, Profile tab

- Area: `0x55fba0(0x6e, y1, 0x30c, y2, 6 columns, weights, rows, 0, 1, 0, -1)`, call at 0x87a705. Rows = (y2 - y1) / `0x5e5cd0(2)` (0x87a6ae..0x87a6d7, kept in bl). Seen in a run: classic y 160..379, row height 18, 12 rows. Widescreen y 213..505 (scaled), row height 18, 16 rows would fit, the original still asks for 12.
- Table: 36 cells of 0x65 bytes at 0xddf570, a name of 100 bytes and a value byte at +0x64. Cells 0..30: attributes 1..31, name from `0x5428a0(i, text, 100, 0)`, value from `0x543540` (or `0x543440`) (0x87a725..0x87a776). qsort 0x945333 of 31 cells by name, comparator 0x88bc50 (0x87a786). Cells 31..35: Preferred Foot (0xff), Form (0xfe), Morale (0xfd), Condition (0xfc), On Loan (0xfb) (0x87a78e..0x87a80d).
- Columns: `mov edi, 0x24` at 0x87a81e shares the cells out round robin over three column counts at [esp+0x1c]; the most rows, or the area rows if more, go to [esp+0x28] for the blank padding.
- Cell loop: from `[esp+0x14] = 0xddf5d4` (0x87a85b) to `cmp eax, 0x24` (0x87acb5). A column ends when its row count is reached (0x87ac.. `local_204 == count`), the loop also ends after the third column.
- Name label: `sprintf("  %s")` (format 0x987ff0, two spaces) into 0xde1f64, label call at 0x87aa09 (returns 0x87aa0e), colour [0xae3184]. Value label: call at 0x87ac15 (returns 0x87ac1a), see the columns inventory.
- On Loan (0xfb): no labels. A widget at x 0x22e..0x30c in the current row and the text "On loan from ..." through 0x4aeaf0 from x 0x238. So On Loan must stay the last cell of column 3 (x is fixed).

## How the slice changes it

- `attribute_groups.py` turns the three constants (0x87a81e, 0x87a85b, 0x87acb5) into calls of the runtime. With the option off they give 0x24, 0xddf5d4, 0x24.
- With the option on, `rt_groups_cells` copies the game's own cells into a runtime table in group order, finds each attribute by the name that `0x5428a0` gives, and adds heading and blank cells (name starts with byte 1). A goalkeeper is a player whose positions text (`0x53ee50`, as the squad list shows it) starts with the goalkeeper. Since 0.6.1 the runtime reads this from the position ratings (player data +0x0f to +0x15, `groups_is_keeper`), because the text is in the chosen language and "GK" is English only: the text puts the goalkeeper first when his rating is 15 or more, or 10 or more and no higher than the sweeper, defender, defensive midfielder, midfielder and attacker ratings (threshold 15 at 0x53ef62, goalkeeper check 0x53ef9e, threshold lowered to 10 at 0x53f7b9..0x53f7da).
- The area create (call site profile_area) gets 13 rows in both modes. In classic the font's row is 18 pixels and the area 220, so 13 rows are 17 pixels apart. On the classic screenshots (outfield player and goalkeeper, zoomed) no text clips, the descenders and the last row included (Q125).

## Same save and uninitialised stack in index.dat

The save writer 0x52dad0 writes `local_1cc`, 0x1cc bytes of its stack frame that it never sets, into `index.dat` (pseudocode of 0x52dad0, the writes after the 22 index entries: `FUN_00932e10(local_1cc, 0x1cc, 1, file)` in the save path, `FUN_00945ce6` in the file path). So the save holds stack bytes left by earlier code. A run that draws a different screen can leave other bytes there.

The save file's table has the same fault. The open of a part 0x9326e0 takes the save object from `operator_new(0xa3191)` and does not clear it. It sets the offset (+0x25 of the row), the size (+0x29) and the name (+0x2d, only up to its end byte) of the new row, not the rest of the 256-byte name field and not the word at +0x108 of the row (+0x12d). The close 0x932b00 writes the whole table, 0x416ec bytes from +0x19: three words and 1000 rows. In a widescreen run with the option on and off, every part's bytes, offsets and sizes were equal, and only these words and the bytes after the first row's name differed (values such as 0xe320e320, which look like RGB565 pixel pairs).

Test saves (RECOMP_SAVE_AFTER_DAYS, `src/recomp_rt/win/testrun.c`) write zeros in both places. Proof for the pad: with only the pad zeroed, the widescreen saves with the option on and off differed in the row words only. Normal saves are unchanged. Whether the loader reads the pad or the row words: unknown.

## 13 rows in classic mode change the game (2026-10-16)

The lead chose 13 rows in classic too (17 pixels apart). On the screenshots no text clips. But the attribute groups check (start.sav, both profiles, 2 days of holiday) then gives saves that differ in club.dat, finance.dat, transfer.dat, fifa_rankings.dat, general.dat and others. On and off each give the same save twice, so this is not timing.

- A heap log of the profile view shows other block sizes from the allocation at return address 0x94557b (for example 4832 against 4288 bytes). Inference: these are text buffers sized by the label cell, which is 17 pixels high in place of 18. After Back, a heap pointer at 0xadab44 differs by 0x30.
- With every heap block zero-filled (a test probe, not kept), the saves still differ, in fifa_rankings.dat, general.dat, national_teams.dat, staff history.tmp and transfer.dat. So later game decisions depend on the heap layout or on other memory that the probe does not clear. Which code reads it: unknown.
- The same happens with the display mode alone: from the same save and the same steps, with the option off, a classic and a widescreen run save different games (18 parts differ, transfer.dat and contract.dat among them).
- With 12 rows in classic (no Mental and Physical heading rows) the check passes. Widescreen with 13 rows passes.
