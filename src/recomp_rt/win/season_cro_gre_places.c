/* The Croatian and Greek places of a rules row (0.7.3 slice 3, open items 281 and 282).
 *
 * CroA2a::set_up_rules 0x515de0 writes 0 promotion places (+0xbe at 0x515e33), and set_places (season_sizes.c) leaves a
 * class with no places in a direction. The champion still goes up through its status 5 (League::
 * update_positions_and_status 0x684cd0, moved by 0x689c80 at 0x689d47), so one place from the row moves the same club.
 * CroA2b::set_up_rules 0x516f00 and GreSecond::set_up_rules 0x5ebde0 write 2 relegation places. Neither has a division
 * of the size table below it, so set_places leaves them too. The exchanges that follow move every row of status 3:
 * CroA1 0x514ed0 (0x515097..0x5150c5) and GrePrm 0x5eb120 (0x5eb25d..0x5eb28a). */
#include "season_cro_gre_places.h"

#include <string.h>

#define MAX_PLACES 30               /* as set_places */

static int row_places(int row) { return row >= 0 && row <= MAX_PLACES; }

int season_cro_gre_up(const char *file, int one_group, int row_up, int class_up)
{
    return file && !strcmp(file, "cro_a2a") && one_group && row_places(row_up) ? row_up : class_up;
}

int season_cro_gre_down(const char *file, int row_down, int class_down)
{
    if (!file || (strcmp(file, "cro_a2b") && strcmp(file, "gre_second"))) return class_down;
    return row_places(row_down) ? row_down : class_down;
}

int season_gre_prm_down(int group_ups, int row_down)
{
    return row_places(row_down) && row_down > group_ups ? row_down : group_ups;
}

int season_tie_played(int po_up, int po_down, int two_groups, int changing_rules)
{
    return po_up == 1 && po_down == 1 && !two_groups && changing_rules;
}

int season_tie_skip(int po_up_first, int direct_up)
{
    return po_up_first > 0 ? po_up_first - 1 : direct_up;
}
