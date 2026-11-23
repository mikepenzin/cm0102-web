/* Poland: reserve sides follow the real rule (open item 243, season_pol_second.c). */
#ifndef RECOMP_SEASON_POL_SECOND_H
#define RECOMP_SEASON_POL_SECOND_H

#include <stdint.h>

/* Before PolSecond::create_play_off_stage 0x7cc290: hands the places of barred reserve sides on. */
void season_pol_second_before_playoff(uint32_t league);

#endif
