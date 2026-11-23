/* Russia: second teams ("-2" sides) of Russian clubs (open item 120; season_rus_second.c). */
#ifndef RECOMP_SEASON_RUS_SECOND_H
#define RECOMP_SEASON_RUS_SECOND_H

#include <stdint.h>

/* The parent club of a Russian second team (a club record), or 0 when the club is no second team or its parent is
 * not in the data. */
uint32_t season_rus_parent(uint32_t club);

#endif
