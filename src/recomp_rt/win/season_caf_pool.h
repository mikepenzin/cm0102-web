/* Every African nation in CAF World Cup qualifying (item 199 of 0.6.2, season_caf_pool.c). The plan is pure code
 * (season_caf_pool_plan.c), so ctest season_caf_pool links it alone. */
#ifndef RECOMP_SEASON_CAF_POOL_H
#define RECOMP_SEASON_CAF_POOL_H

/* The shape of one campaign. entrants: the nations that play (the best of the pool when it is larger than two
 * rounds can take), first: the teams of the two-leg first round (its weakest), group_size: the teams of each group
 * after it. The byes are entrants - first. */
typedef struct {
    int entrants;
    int first;
    int group_size;
} SeasonCafPlan;

/* The plan for a pool of `pool` nations (hosts and holder out) and `groups` groups.
 * - wcq26 (a World Cup of 48 teams): groups of 5, as season_wcq26.c makes them.
 * - else the original's 5 groups: of 5 for a pool of 50 or fewer (the original's 25 places), of 6 for more (30
 *   places, as the original's groups with one exempt nation and the real 2006 cycle).
 * entrants = min(pool, 2 x places), first = 2 x (entrants - places). Returns 1, or 0 when the pool does not fill
 * the places and one tie (the caller keeps the original's shape). */
int season_caf_plan(int pool, int groups, int wcq26, SeasonCafPlan *plan);

#endif
