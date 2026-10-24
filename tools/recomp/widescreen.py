"""Screen size constants of the original that become run-time values, for widescreen mode.

The original draws at 800x600 and writes the size as constants in some instructions. Each entry names
the address of one instruction, the constant it holds and the C expression that replaces it. The runtime
sets rt_screen_w and rt_screen_h (rt.c: 800x600 by default, main.c: from the settings file).

Only constants that do not go into the area or widget create functions are here: the runtime scales
those coordinates when it creates the area or widget (src/recomp_rt/win/display.c). The evidence for
each address is in docs/research/2026-10-14-widescreen-inventory.md.
"""

W = "rt_screen_w"
H = "rt_screen_h"
W1 = "(rt_screen_w - 1u)"
H1 = "(rt_screen_h - 1u)"

# address: (constant in the original, C expression)
PATCHES = {
    # Area init 0x402ac0: x2 and y2 are clamped to 799 and 599.
    0x402B7C: (799, W1), 0x402B84: (799, W1), 0x402B8D: (599, H1),
    # Popup placement 0x403760: the popup opens below its widget if it ends above y 600.
    0x40384B: (600, H), 0x403922: (600, H), 0x4039B5: (600, H),
    # Area scroll and drag limits 0x403a70.
    0x403B00: (799, W1), 0x403B64: (799, W1), 0x403BDC: (599, H1), 0x403C40: (599, H1),
    # Full-screen clear and flip before a progress bar 0x55f2c0.
    0x55F2CA: (599, H1), 0x55F2CF: (799, W1), 0x55F2E7: (599, H1), 0x55F2EC: (799, W1),
    # Screen draw 0x55f300: the plain background, the clear, the fade and the flip of the whole screen.
    0x55F38D: (599, H1), 0x55F392: (799, W1), 0x55F56B: (599, H1), 0x55F570: (799, W1),
    0x55F58D: (599, H1), 0x55F592: (799, W1), 0x55F5EB: (599, H1), 0x55F5F0: (799, W1),
    # Widget drag 0x561210: clamped to the screen.
    0x5612A0: (799, W1), 0x5612A8: (799, W1), 0x5612C9: (599, H1), 0x5612D1: (599, H1),
    # Display init calls 0x5e2620(800, 600, ...): start-up, the two settings changes, the network path.
    0x5D79ED: (600, H), 0x5D79F2: (800, W),
    0x5D8645: (600, H), 0x5D864A: (800, W), 0x5D8685: (600, H), 0x5D868A: (800, W),
    0x8AEED3: (600, H), 0x8AEED8: (800, W), 0x8AEEFB: (600, H), 0x8AEF00: (800, W),
    # Outline draw 0x5e7840: the span list starts at x 800, and the lock covers the whole screen.
    0x5E78BA: (800, W), 0x5E78E2: (800, W), 0x5E78EA: (600, H),
    # Error box 0x5e8290: centred in a screen 800 wide. Its y values follow in BOX_Y.
    0x5E8309: (800, W),
    # Widget init 0x5ee150: x2 and y2 are clamped to 799 and 599.
    0x5EE1F5: (799, W1), 0x5EE20A: (599, H1),
    # Tactics input 0x89f710: the nearest position to a drop, by distance in screen pixels, starts at
    # 800 and counts only if it ends below 800.
    0x8A20F8: (800, W), 0x8A21E2: (800, W), 0x8A260F: (800, W), 0x8A26F6: (800, W),
}

# The message box 0x5e8290 keeps its classic size, and its x values are screen pixels already (the width of
# its text, centred by 0x5E8309). Its y values are classic rows: the box at 242..357 is centred on 600 rows.
# Each y constant gets (rt_screen_h - 600) / 2 more rows, so the box is centred on any screen (0.4.0 slice
# 10). In classic mode the values do not change. The constants: the save and the restore of the pixels under
# the box (0x5e3dd0, 0x5e3fd0), the box and its title bar (0x5e5df0), the title and the text (0x5e6d80), the
# Ok and the second button (0x5e5df0, 0x5e6d80, their rects at [esp+0x14] and [esp+0x1c]), the flips
# (0x5e2eb0) and the mouse tests of the two buttons.
DY = "(rt_screen_h - 600u) / 2u"
BOX_Y = {
    0x5E832C: 0x13E, 0x5E8337: 0x156,                       # the second button: y1, y2
    0x5E8374: 0x165, 0x5E837A: 0xF2,                        # save the pixels under the box
    0x5E83AD: 0x165, 0x5E83B3: 0xF2,                        # the box
    0x5E83D0: 0x108, 0x5E83D9: 0xF4,                        # the title bar
    0x5E83FD: 0x108, 0x5E8406: 0xF4,                        # the title
    0x5E842F: 0x13B, 0x5E8438: 0x10A,                       # the text
    0x5E846F: 0x156, 0x5E8475: 0x13E, 0x5E8495: 0x158, 0x5E849E: 0x140,    # Ok, pressed
    0x5E84B3: 0x156, 0x5E84B9: 0x13E, 0x5E84D7: 0x156, 0x5E84DD: 0x13E,    # Ok
    0x5E85C4: 0x165, 0x5E85CA: 0xF2,                        # flip
    0x5E85F6: 0x13E, 0x5E8605: 0x156,                       # mouse over Ok
    0x5E86D3: 0x13E, 0x5E86E2: 0x156, 0x5E86FC: 0x13E, 0x5E870B: 0x156,    # mouse down and up on Ok
    0x5E882A: 0xF2,                                         # restore the pixels under the box
    0x5E8835: 0x165, 0x5E883B: 0xF2,                        # flip
}
PATCHES.update({a: (v, f"({v:#x}u + {DY})") for a, v in BOX_Y.items()})

# Instructions whose whole translation is replaced: address: (the instruction in the original, C code).
# The circle plot 0x5e7aa0 finds the pixel of row y as y * 25 * 32 (lea, lea, shl 5): a row of 800 pixels.
# The replacement makes it y * rt_screen_w. The next instruction (add or sub) sets the flags again.
ROW = "eax = (uint32_t)((int32_t)eax / 25 * (int32_t)rt_screen_w);"
INSNS = {a: ("shl eax, 5", ROW) for a in (
    0x5E7B7D, 0x5E7BE5, 0x5E7C42, 0x5E7CB8, 0x5E7D73, 0x5E7DE2, 0x5E7E44, 0x5E7EBB)}

# Calls of the text width function 0x5e5ad0 that must get the width in screen pixels. Every other call
# (screen code that lays out widgets in classic coordinates) gets the width in classic pixels, so that
# a widget placed after a text, or sized to it, keeps its place on the text once the runtime scales
# the coordinates (src/recomp_rt/win/display.c, text_width). These calls compare the width with
# a rect on the screen, or draw:
TRUE_WIDTH = {
    0x561E60,                                   # text entry 0x5617c0: does the text fit the widget
    0x5E6F5F, 0x5E7205, 0x5E722A,               # text draw 0x5e6d80: wrap and align in the box
    0x5E82DE,                                   # error box 0x5e8290: box width (not scaled)
    0x5EE26F, 0x5EE281,                         # widget init 0x5ee150: popup width
    # comp_text 0x4aeaf0 (news text and its name buttons): left as it is for the clickable names work.
    0x4AEB54, 0x4AEDEF, 0x4AF16F, 0x4AF58B, 0x4AF66F,
}

# The attribute groups (#43) replace constants of the player profile build in the same way.
from attribute_groups import PATCHES as GROUP_PATCHES  # noqa: E402

PATCHES.update(GROUP_PATCHES)

# The season core (0.6.0 slice 11) replaces year constants in the same way.
from seasons import INSNS as SEASON_INSNS, PATCHES as SEASON_PATCHES  # noqa: E402

assert not set(SEASON_PATCHES) & set(PATCHES)
PATCHES.update(SEASON_PATCHES)

# The reserve of blank staff rows (0.6.0, every league with an Update Team database) in the same way.
from staff_reserve import PATCHES as RESERVE_PATCHES  # noqa: E402

assert not set(RESERVE_PATCHES) & set(PATCHES)
PATCHES.update(RESERVE_PATCHES)

# The knockout slot of the Champions League (0.6.0 slice 22) in the same way.
from uefa import PATCHES as UEFA_PATCHES  # noqa: E402

assert not set(UEFA_PATCHES) & set(PATCHES)
PATCHES.update(UEFA_PATCHES)

# Not a screen size: a fix for an endless loop (0.4.0 slice 12, custom data sets). euro.cfg is read by 0x9059e0
# (docs/research/2026-10-16-update-team-databases.md). When a line of it is not a "*" header (after a header
# that names an unknown nation, or a file that does not match the database), the original tests the same line
# again for ever: 0x905a45 cmp [0xde1f64], '*' / 0x905a4c jne 0x905c21 / 0x905c21 test eax, eax / jne 0x905a45.
# The fix reads the next line first (the read call 0x90cba0 of 0x905a35: file in edi, buffer 0xde1f64, cdecl)
# and goes on at 0x905c21 with its result, so the loop ends at the next header or at the end of the file.
# A file that the loop reads without this case, such as the official data, takes the same path as before.
INSNS[0x905A4C] = ("jne 0x905c21",
    "if (!cc_e(F)) { esp -= 4; W32(esp, 0xde1f64u); esp -= 4; W32(esp, edi); esp -= 4; W32(esp, 0x905a52u); "
    "SYNC(); f_0090cba0(); LOAD(); RT_AFTER_CALL(); esp += 8; goto L_905c21; }")

# The full player search option (0.5.0 slice 5, #57; docs/research/2026-10-16-crash-fixes.md, 1c). The player
# search list builder 0x7afa70 tests whether the manager knows a player: score of 0x5407e0 of 4000 or more
# (jge 0x7afcee at 0x7afc4b), a scout report, or the same region and one of 0x544520, 0x544590, 0x76d9b0.
# Changing the jge to a jmp lists every player. That skips 0x544520 and 0x544590, which write a
# cached byte of the club (0x540a50), so on and off could leave different club bytes (Q152). Here the jge and
# every call stay. Only the three places where the original rejects a player accept him when the option is on:
# no region object (je 0x7afe6b at 0x7afc8d), another region (jne 0x7afe6b at 0x7afc9d), and all four tests
# false (je 0x7afcf3 at 0x7afcdf, where ebx stays 0). Accept = go to 0x7afcee (ebx = 1). The screen keeps the
# count of the list in 16 bits, so rt_full_player_search(ebp) is 0 once the list (count at ebp+0x1f6) holds
# 32767 players. The option is read at run time (playersearch.c).
_FPS = "if (%s) { if (rt_full_player_search(ebp)) goto L_7afcee; goto L_7afe6b; }"
INSNS[0x7AFC8D] = ("je 0x7afe6b", _FPS % "cc_e(F)")
INSNS[0x7AFC9D] = ("jne 0x7afe6b", _FPS % "!cc_e(F)")
INSNS[0x7AFCDF] = ("je 0x7afcf3", "if (cc_e(F)) { if (rt_full_player_search(ebp)) ebx = 1; goto L_7afcf3; }")
# The limit (forum report of 2026-10-21, playersearch.c). The screen counts the players it shows in 16 bits, and
# known players and scout lists still join after the extra ones stop, so a full list went above 32767 and the
# screen said "1 player found (N filtered out)". The builder's reset of the list (0x7afbb3) resets the record of
# extra rows, a known player that comes when the list is full takes the row of the last extra player (0x7afdda
# reads the row, 0x7afe5e keeps the count), a player that does not join clears the pending extra (0x7afe6b),
# and 0x7af160 passes at most 32767 players to the screen list (ebx at 0x7af467). Each one acts only with the
# option on: with it off the slot is the count, nothing is replaced and ebx stays.
INSNS[0x7AFBB3] = ("mov dword ptr [ebp + 0x1f6], esi", "W32(ebp + 0x1f6u, esi); rt_full_player_search_reset();")
INSNS[0x7AFDDA] = ("mov eax, dword ptr [ebp + 0x1f6]", "eax = rt_full_player_search_slot(ebp);")
INSNS[0x7AFE5E] = ("mov eax, dword ptr [ebp + 0x1f6]", "eax = R32(ebp + 0x1f6u) - rt_full_player_search_replaced();")
INSNS[0x7AFE6B] = ("mov ebx, dword ptr [esp + 0x14]", "rt_full_player_search_skip(); ebx = R32(esp + 0x14u);")
INSNS[0x7AF467] = ("mov eax, dword ptr [esp + 0x22c]", "ebx = rt_full_player_search_shown(ebx); eax = R32(esp + 0x22cu);")

# The season core (0.6.0 slice 11): whole instructions of the year sites (seasons.py).
assert not set(SEASON_INSNS) & set(INSNS) and not set(SEASON_PATCHES) & set(SEASON_INSNS)
INSNS.update(SEASON_INSNS)

# League sizes (0.6.0 slice 18): the club count stores of create_table (league_sizes.py).
from league_sizes import INSNS as SIZE_INSNS, PATCHES as SIZE_PATCHES  # noqa: E402

assert not set(SIZE_INSNS) & (set(INSNS) | set(PATCHES)) and not set(SIZE_PATCHES) & (set(PATCHES) | set(SIZE_INSNS))
INSNS.update(SIZE_INSNS)
PATCHES.update(SIZE_PATCHES)

# New divisions (0.6.0 slice 20): the club count of the 3. Liga in the German Regional object (new_divisions.py).
from new_divisions import INSNS as DIVISION_INSNS, PATCHES as DIVISION_PATCHES  # noqa: E402

assert not set(DIVISION_INSNS) & (set(INSNS) | set(PATCHES)) and not set(DIVISION_PATCHES) & (set(PATCHES) | set(INSNS))
INSNS.update(DIVISION_INSNS)
PATCHES.update(DIVISION_PATCHES)

# The Turkish Category B in three groups with Summer 2002: the group sizes of TurSecondB (tur_groups.py).
from tur_groups import INSNS as TUR_INSNS, PATCHES as TUR_PATCHES  # noqa: E402

assert not set(TUR_INSNS) & (set(INSNS) | set(PATCHES)) and not set(TUR_PATCHES) & (set(PATCHES) | set(INSNS))
INSNS.update(TUR_INSNS)
PATCHES.update(TUR_PATCHES)

# A fix for the error box league_stage.cpp 533 of a league load (league_load.py): a row array of 0 bytes.
from league_load import INSNS as LOAD_INSNS  # noqa: E402

assert not set(LOAD_INSNS) & (set(INSNS) | set(PATCHES))
INSNS.update(LOAD_INSNS)

# A fix for the error box discipline..cpp 1149 of a ban release (discipline_ban.py): a lost nation index entry.
from discipline_ban import INSNS as BAN_INSNS  # noqa: E402

assert not set(BAN_INSNS) & (set(INSNS) | set(PATCHES))
INSNS.update(BAN_INSNS)

# Saves above 2 GB (0.6.0): unsigned file positions in ftell and the seek of a save part (save_size.py).
from save_size import INSNS as SAVE_SIZE_INSNS  # noqa: E402

assert not set(SAVE_SIZE_INSNS) & (set(INSNS) | set(PATCHES))
INSNS.update(SAVE_SIZE_INSNS)
