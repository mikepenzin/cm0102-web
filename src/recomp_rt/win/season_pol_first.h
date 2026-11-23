/* The Ekstraklasa in its real format of each season (season_pol_first.c, GitHub issue 92). */
#ifndef RECOMP_SEASON_POL_FIRST_H
#define RECOMP_SEASON_POL_FIRST_H

#include <stdint.h>

/* The club count of the Polish First Division at the club moves of a new game in a season: the rules table row's
 * count when that season plays one table (with changing rules on), else -1 (the class's 16). */
int season_pol_first_clubs(int season);

/* 1 when the chain's upper league is the Ekstraklasa in one table: its clubs move on their own (season_sizes.c keeps the
 * call's pairs flag). */
int season_pol_first_moves_alone(uint32_t upper);

/* Slot +0x30 0x595160 of PolFirst (the wrap in season_cup_groups.c): 1 when the object is the Ekstraklasa in one table,
 * after a jump to League::record_season_honours 0x684640 (the original jumps to slot +0x30 of stage 1, which in one
 * table is the object itself). */
int season_pol_first_honours(uint32_t obj);

/* The round table of the I liga's play-off against the Ekstraklasa (the wrap of PolSecond::create_schedule 0x7cb4a0 in
 * season_pol_playoff.c): 2 teams and 1 tie when 1 pair plays. */
void season_pol_first_playoff_round(uint32_t obj, uint32_t t);

/* 1 when the I liga object plays its own play-off (3rd to 6th, two rounds) in its season, with changing rules on. */
int season_pol_first_own_playoff(uint32_t obj);

#endif
