/* Finland: no second team in the Veikkausliiga (open item 118; season_fin_second.c). */
#ifndef RECOMP_SEASON_FIN_SECOND_H
#define RECOMP_SEASON_FIN_SECOND_H

#include <stdint.h>

/* 1 when club (a club record) is a Finnish second team and div (a club_comp record) is the Premier Division
 * (Veikkausliiga): Palloliitto's competition rules § 6.12 bar it there. */
int season_fin_barred(uint32_t club, uint32_t div);

#endif
