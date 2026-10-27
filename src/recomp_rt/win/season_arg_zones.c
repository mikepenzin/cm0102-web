/* The zone fill table of the Argentine clubs: the lookup (see season_arg_zones.h). */
#include "season_arg_zones.h"

#include <string.h>

char season_arg_zone_of(int season, int id, const char *name)
{
    for (int i = 0; i < season_arg_zone_count; i++) {
        const SeasonArgZone *z = &season_arg_zones[i];
        if (z->season == season && z->id == id && name && !strncmp(z->name, name, 52)) return z->zone;
    }
    return 0;
}
