/* The shape of CAF World Cup qualifying for a pool of every African nation (item 199 of 0.6.2). See
 * season_caf_pool.h and season_caf_pool.c. */
#include "season_caf_pool.h"

int season_caf_plan(int pool, int groups, int wcq26, SeasonCafPlan *plan)
{
    plan->entrants = plan->first = plan->group_size = 0;
    if (groups <= 0) return 0;
    int size = wcq26 || pool <= 10 * groups ? 5 : 6;
    int places = groups * size;
    int entrants = pool < 2 * places ? pool : 2 * places;
    if (entrants <= places) return 0;
    plan->entrants = entrants;
    plan->first = 2 * (entrants - places);
    plan->group_size = size;
    return 1;
}
