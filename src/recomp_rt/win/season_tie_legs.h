/* The two legs of a qualifying tie on the FIFA days of one window (item 207 of 0.6.2, season_tie_legs.c). Pure code,
 * so ctest season_tie_legs links it with the FIFA days. The game side is season_friendly_dates.c
 * (season_fifa_snap_fixture, the add_fixture calls of Cup::create_tie_fixtures 0x51cb90). */
#ifndef RECOMP_SEASON_TIE_LEGS_H
#define RECOMP_SEASON_TIE_LEGS_H

#include "season_stage_plan.h"

#define SEASON_TIE_GAP_MIN 3            /* more than CLASH_DAYS (2) of season_friendly_dates.c */
#define SEASON_TIE_GAP_MAX 6            /* the AFC and OFC first rounds take 3 to 6 days too (season_afc_first.h) */

/* 1 when the sides of the tie can play its first leg on day leg1 and its second on day leg2 (days from 1970). */
typedef int (*SeasonTieFree)(long leg1, long leg2, void *ctx);

/* The days of a tie whose legs lie on days leg1 and leg2 (days from 1 January 1970), by the rule of
 * season_tie_legs.c: fifa(z, ctx) says whether z is a FIFA day the tie can use, free_fn (NULL: every pair) whether
 * its sides can play a pair, `first` is the earliest first leg and `draw` the next round's draw (0: none), which the
 * second leg comes before unless its own day does not. Returns 0 when the legs keep their days (already
 * in one window), 1 when they move to a free pair, 2 when they move to a pair that is not free (no pair is), -1 when
 * no pair fits; *out1 and *out2 get the days (leg1 and leg2 for 0 and -1). */
int season_tie_legs(long leg1, long leg2, long first, long draw, SeasonStageFifa fifa, SeasonTieFree free_fn,
                    void *ctx, long *out1, long *out2);

#endif
