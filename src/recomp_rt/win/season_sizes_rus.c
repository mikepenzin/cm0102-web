/* The Russian First Division at its real count (0.7.2 league sizes, slice 12): the take loop of
 * RusFirst::create_table 0x7ea6c0 follows the count (season_sizes_rus.h). The instruction row is in
 * tools/recomp/sizes_rus_tur.py. The table allocation and the date template are the size engine's (league_sizes.py
 * GROWN, season_sizes.c). */
#include "win.h"
#include "../rt_host.h"
#include "season_sizes.h"
#include "season_sizes_rus.h"

#define COMP_CLUBS 0x3eu   /* s16: the count, stored at 0x7ea6d3 (rt_league_clubs) before the loop */

/* `cmp bl, 0x12` at 0x7ea77d: 18 in the base game (Q152), else the bound of the count. */
uint32_t rt_rus_first_take(uint32_t obj)
{
    if (!season_sizes_on() || obj < 0x10000u) return RUS_FIRST_TAKE;
    return (uint32_t)season_rus_first_take((int16_t)rd16(g_mem, obj + COMP_CLUBS));
}
