/* The cups of western Europe take the real divisions (0.7.2 league sizes, slice 5, season_cup_sizes_west.c). */
#ifndef SEASON_CUP_SIZES_WEST_H
#define SEASON_CUP_SIZES_WEST_H

#include <stdint.h>

/* After ItaCup::select_entrants 0x63faf0 made the list of the Coppa Italia: every club of Serie A and Serie B takes
 * part (a season game with the sizes on; nothing otherwise). */
void season_cup_sizes_ita_cup(uint32_t cup);

/* After FraLgeCup::collect_entrants 0x5bd160 left its 54 places short: French clubs from below the National fill
 * them. Returns 1 when the list is full (0 outside a season game with the sizes on). */
int season_cup_sizes_fra_lge(uint32_t cup);

#endif
