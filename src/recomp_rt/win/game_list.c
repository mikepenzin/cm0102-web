/* The game list of 50 games (0.7.0 slice 4a, tools/recomp/game_list.py).
 *
 * setup_build_select_network_game_screen 0x81f8d0 copies the name slots of the Network object to the global
 * 0xdd6cd0 (0x81fae6..0x81fb06) and makes one button for each game with its text there (0x81fb19..0x81fb4b). The
 * global holds 10 slots of 50 bytes, and the next global starts at 0xdd6ec4. With 51 slots the copy goes to this
 * buffer: 638 dwords, the names and 2 bytes after them. It is runtime memory, not the game's heap, so a network
 * screen does not move the game's own blocks. */
#include "win.h"
#include "../rt.h"

#define NAMES_BYTES (638u * 4u)

uint32_t rt_game_list_names(void)
{
    static uint32_t names;
    if (!names) names = win_static_alloc(NAMES_BYTES);
    return names;
}
