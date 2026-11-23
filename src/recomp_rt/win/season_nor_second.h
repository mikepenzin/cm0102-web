/* Norway: no second team in Eliteserien or the First Division (open item 111; season_nor_second.c). */
#ifndef RECOMP_SEASON_NOR_SECOND_H
#define RECOMP_SEASON_NOR_SECOND_H

#include <stdint.h>

/* 1 when club (a club record) is a Norwegian second or third team and div (a club_comp record) is Eliteserien or
 * the Norwegian First Division: NFF Kampreglement § 2-7 (1) bars it from that division. */
int season_nor_barred(uint32_t club, uint32_t div);

#endif
