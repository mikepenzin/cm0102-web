/* The youth pool rule (0.6.2 slice 20), without game memory: youth_pool.h. */
#include "youth_pool.h"

int youth_pool_age(int year, int day, int byear, int bday)
{
    return year - byear - (day < bday ? 1 : 0);
}

int youth_pool_short(int players, int under_21, int under_23)
{
    return players >= YOUTH_POOL_SQUAD && (under_21 * 20 < players || under_23 * 10 < players);
}

int youth_pool_runs_out(int players, int under_20, int under_22)
{
    return players >= YOUTH_POOL_SQUAD && (under_20 == 0 || under_22 == 0);
}

int youth_pool_older_rule(int league_flags, int start_players)
{
    return league_flags == 0 && start_players <= YOUTH_POOL_OLDER_MAX;
}
