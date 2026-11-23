/* The preliminary round of OFC World Cup qualifying (item 198 of 0.6.2): two-leg ties of the weakest Oceania
 * nations before the 10 group places (season_ofc_first.c). Pure code (season_ofc_first_plan.c), so ctest
 * season_ofc_first links it with the FIFA days. */
#ifndef RECOMP_SEASON_OFC_FIRST_H
#define RECOMP_SEASON_OFC_FIRST_H

#include <stdint.h>

#define OFC_FIRST_MAX_POOL 32           /* Oceania nations a plan holds (a database holds 11 or 12) */

/* The plan of one cycle. pool: the Oceania nations that can enter (hosts out), sorted best first by the value of
 * the original's sort (national team reputation, club +0x80, compare 0x4b07d0 at 0x91fd04). places: the group
 * places (10 less the exempt count +0xee, 0x91fbda). */
typedef struct {
    int ties;                           /* W = pool - places, 0 when the pool fits */
    int byes;                           /* places - W: the best, straight into the groups */
    int pair[OFC_FIRST_MAX_POOL][2];    /* tie k: the k-th best and the k-th weakest of the bottom 2W (indices) */
} SeasonOfcFirstPlan;

/* Fills plan for a sorted pool of `pool` nations and `places` group places. A pool of more than 2 x places takes
 * its best 2 x places. The list of the groups is the best `places` of the pool, in pool order: the byes, then the
 * W seeds, each a stand-in that the winner of its tie replaces on the draw day. The bottom W of the pool wait
 * outside the list. Tie k: the seed places - W + k against the nation pool - 1 - k. Returns W. */
int season_ofc_first_plan(int pool, int places, SeasonOfcFirstPlan *plan);

/* The days of the round, as days from 1 January 1970, for a section whose groups are drawn on `group_draw`: both
 * legs on FIFA days (season_fifa_year_days) of one FIFA window, 3 to 6 days apart, the first leg on or after
 * group_draw - OFC_FIRST_LEAD and at least 8 days after today, the second leg OFC_FIRST_MARGIN days or more before
 * group_draw (room for a leg that a clash moves: the ties must be decided on the draw day).
 * The round is drawn 7 days before the first leg, not before tomorrow. Returns 1, or 0 when no window fits. */
#define OFC_FIRST_LEAD 150
#define OFC_FIRST_MARGIN 30
int season_ofc_first_days(long group_draw, long today, long *draw, long *leg1, long *leg2);

/* The game side (season_ofc_first.c), called from the Oceania wraps of season_wcq26.c: after the pool builder
 * 0x91fa80, and in advance_stage 0x91f0d0 (1 while the ties are live: advance waits). */
int season_ofc_first_waiting(uint32_t self);
void season_ofc_first_pool(uint32_t self);

/* Item 245 (tests only): RECOMP_TEST_OFC_LEVEL=1 forces each preliminary tie level. Called from the
 * Cup::process_fixture wrap 0x51a150 (season_playoffs.c) with the cup and the fixture, before the original. */
void season_ofc_first_test_fixture(uint32_t cup, uint32_t fixture);

#endif
