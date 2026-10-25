/* The Turkish Category B in three groups with Summer 2002 (season_tur_groups.c). */
#ifndef RECOMP_SEASON_TUR_GROUPS_H
#define RECOMP_SEASON_TUR_GROUPS_H

#include <stdint.h>

/* The club moves of a new game (season_clubs.c move_clubs): season_tur_groups_scan finds the three Summer 2002 group
 * rows once, then season_tur_groups_tagged gives 1 when the data's group tag of a club (+0x60) names one of them (the
 * club plays in Category B, so the moves do not keep it in another division). */
void season_tur_groups_scan(void);
int season_tur_groups_tagged(uint32_t club);

#endif
