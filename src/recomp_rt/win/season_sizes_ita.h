/* Italy at its real league sizes (0.7.2 league sizes, slice 9): the code of the Italian classes that reads a fixed
 * count (season_sizes_ita.c). */
#ifndef RECOMP_SEASON_SIZES_ITA_H
#define RECOMP_SEASON_SIZES_ITA_H

#include <stdint.h>

#include "season_ita_rules.h"

/* Around the original's ita_ser_a_regroup_serie_c1_clubs 0x642630 (the wrap in season_divisions_ita.c, outside the
 * Serie C era): the regroup takes every club of C1/A and C1/B and gives C1/B the count of its next row. */
/* Returns 0 when the regroup has nothing to do (no club in C1/A or C1/B): the wrap then skips the original. */
int season_sizes_ita_regroup_begin(uint32_t serie_a);
void season_sizes_ita_regroup_end(void);

/* Before ItaCup::select_entrants 0x63faf0 (its wrap in season_divisions_ita.c): notes the count of the last Serie B,
 * the largest last place (+0x5f) of a club whose last division is Serie B, for its sort 0x640590 (gap row 32). */
void season_sizes_ita_cup_pick(int season);

/* The C1 play-out of the season (open item 253; season_ita_rules.h ita_c1_playout). shape: the season's play-out of a
 * C1 group or of group C (no points gap). set: the play-out that the builder that runs now takes (its rows, seeds,
 * club count and template), or none (p NULL). template: create_schedule's template t of stage `stage`. places: the
 * play-out and direct places +0xc0 and +0xc1 of the season (from set_up_rules). */
void season_sizes_ita_playout_shape(uint32_t obj, ItaPlayout *p);
void season_sizes_ita_playout_set(uint32_t obj, const ItaPlayout *p);
void season_sizes_ita_playout_template(uint32_t obj, int stage, uint32_t t);
void season_sizes_ita_playout_places(uint32_t obj);
/* The play-out stage's block of a builder run of no tie, or 0 (freed by the caller after the builder). */
uint32_t season_sizes_ita_playout_block(void);
/* The play-out of the season once the table is final: the shape less the ties of the points gap (their lower rows in
 * gone[], higher rows in safe[]). Returns the count of ties taken out (season_divisions_ita.c). */
int season_ita_playout_final(uint32_t obj, ItaPlayout *p, int *gone, int *safe);

#endif
