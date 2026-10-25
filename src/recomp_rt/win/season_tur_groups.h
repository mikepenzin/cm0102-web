/* The Turkish Category B in three groups with Summer 2002 (season_tur_groups.c). */
#ifndef RECOMP_SEASON_TUR_GROUPS_H
#define RECOMP_SEASON_TUR_GROUPS_H

#include <stdint.h>

/* 1 when the data's group tag of the club (+0x60) names one of the three Summer 2002 group rows: the club plays in
 * Category B, so the club moves of a new game (season_clubs.c) do not keep it in another division. */
int season_tur_groups_tagged(uint32_t club);

#endif
