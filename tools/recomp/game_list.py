"""The game list of a joining game holds 50 games in place of 9 (0.7.0 slice 4a, Q31, Q33).

The list lives in the Network object (operator new of 0xc3e54 bytes at 0x7ff562). It holds two arrays of 10 slots
of 50 bytes: the game names at +0xc3a6c and the address texts at +0xc3c60, which end at the end of the object.
The count is the s16 at +0xc3a6a. Network::receive_packet 0x77af20 adds a game for an ACK only while the count is
below 9 (0x77b0eb), so the tenth slot is spare: a BYE from an address that is not in the list adds one row
(0x77b448..0x77b457), and that row is the tenth slot. The game list screen copies the 10 name slots to the global
0xdd6cd0 (0x81fae6..0x81fb06) and makes one button for each game from there (0x81fb19..0x81fb4b). The list area
scrolls when the rows pass a page of 6 (Area::layout 0x403350), and an area takes up to 1,200 widgets
(Area::add_widget 0x403200), so the screen needs no change. The research is in section 10 of
docs/research/2026-10-27-network-code-reading.md.

Here both arrays get 51 slots: 50 games and the spare slot. The names stay at +0xc3a6c and the addresses move to
the new end of the names, +0xc4462. The object grows by 4,100 bytes. The sort and the BYE removal reach the address
array from a name pointer, and back, with the fixed distance 500 between the arrays, that becomes 2,550. The cap
becomes 50. The global 0xdd6cd0 cannot grow (the next global is at 0xdd6ec4, 500 bytes on), so the screen copies
the names to a runtime buffer of 2,552 bytes (rt_game_list_names, win/game_list.c): 638 dwords, the 51 slots and
2 bytes of the address array after them.

The Network object exists only after a network screen makes it (0x7ff500 from 0x81f1cc and 0x81f8f7), so play
without a network game does not change.
"""

SLOTS = 51                  # 50 games and the spare slot of the original
SLOT = 50                   # bytes of a name or an address text
OLD_D = 10 * SLOT           # distance between the arrays in the original: 500
D = SLOTS * SLOT            # new distance: 2,550
NAMES = 0xC3A6C
OLD_ADDR, ADDR = NAMES + OLD_D, NAMES + D      # 0xc3c60, 0xc4462
OLD_SIZE, SIZE = OLD_ADDR + OLD_D, ADDR + D    # 0xc3e54, 0xc4e58
COPY_DWORDS = (D + 3) // 4                     # 638: rt_game_list_names holds COPY_DWORDS * 4 bytes


def c(v):
    """The C text of a 32-bit constant, as translate.py writes it."""
    return "0x%xu" % (v & 0xFFFFFFFF)


# address: (constant in the original, C expression)
PATCHES = {
    # ScreenManager::start_network_session 0x7ff500: the size of the Network object.
    0x7FF562: (OLD_SIZE, c(SIZE)),
    # The address array at +0xc3c60: Network::~Network 0x77a890 (the host name in the BYE),
    # Network::host_or_join_game 0x77a9f0 (gethostname into slot 0, the start-up ACK, the join look-up),
    # Network::receive_packet 0x77af20 (the reply ACK, the new entry, the duplicate test, the BYE search).
    0x77A8FF: (OLD_ADDR, c(ADDR)),
    0x77AA6B: (OLD_ADDR, c(ADDR)),
    0x77AAB2: (OLD_ADDR, c(ADDR)),
    0x77AB71: (OLD_ADDR, c(ADDR)),
    0x77B029: (OLD_ADDR, c(ADDR)),
    0x77B102: (OLD_ADDR, c(ADDR)),
    0x77B156: (OLD_ADDR, c(ADDR)),
    0x77B163: (OLD_ADDR, c(ADDR)),
    0x77B38C: (OLD_ADDR, c(ADDR)),
    # The cap: an ACK adds a game while the count is below it.
    0x77B0EB: (9, c(SLOTS - 1)),
    # The sort by name 0x77b1d2..0x77b32a: ebp is the address slot i (from slot 1), ebp - 500 the name slot i,
    # ebp - 550 the name slot i - 1.
    0x77B1EA: (OLD_ADDR + SLOT, c(ADDR + SLOT)),
    0x77B1F0: (-OLD_D, c(-D)),
    0x77B1F6: (-(OLD_D + SLOT), c(-(D + SLOT))),
    0x77B29D: (-(OLD_D + SLOT), c(-(D + SLOT))),
    0x77B2B2: (-OLD_D, c(-D)),
    0x77B2D5: (-(OLD_D + SLOT), c(-(D + SLOT))),
    # The BYE removal 0x77b3c5..0x77b42f: edx is the address slot k - 1, edx - 450 the name slot k, edx - 500 the
    # name slot k - 1.
    0x77B3CC: (OLD_ADDR - SLOT, c(ADDR - SLOT)),
    0x77B3F5: (-(OLD_D - SLOT), c(-(D - SLOT))),
    0x77B402: (-OLD_D, c(-D)),
    # setup_build_select_network_game_screen 0x81f8d0: the copy of the names and the buttons made from it.
    0x81FAE6: (0xDD6CD0, "rt_game_list_names()"),
    0x81FAFC: (OLD_D // 4, c(COPY_DWORDS)),
    0x81FB01: (0xDD6CD0, "rt_game_list_names()"),
    0x81FB19: (0xDD6CD0, "rt_game_list_names()"),
}
