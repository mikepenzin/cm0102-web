/* The Dutch nacompetitie as a knockout from 2005/06 (issue 150). See season_ned_playoff.c. */
#ifndef RECOMP_SEASON_NED_PLAYOFF_H
#define RECOMP_SEASON_NED_PLAYOFF_H

#include <stdint.h>

/* In place of HolFirst::create_play_off_groups 0x5f6720: 1 when the knockout of the season took its place. */
int season_ned_playoff_build(uint32_t self);
/* Before Cup::draw_round 0x51c410: the ties of a round of a bracket. */
void season_ned_playoff_draw_round(uint32_t cup, int round);
/* The last stage of the season of the Eerste Divisie, from the class's last stage. */
uint32_t season_ned_playoff_last_stage(uint32_t obj, uint32_t last);
/* After the rules of a league (season_sizes_rules): the direct and play-off places of the Eredivisie from 2020/21. */
void season_ned_playoff_rules(uint32_t obj);
/* 1 when the season change of the entry of obj waits for the knockout of this season. */
int season_ned_playoff_pending(uint32_t obj);

#endif
