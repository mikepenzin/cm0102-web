/* See season_swe_groups_plan.h. */
#include "season_swe_groups_plan.h"

#include <stdio.h>

int season_swe_groups_plan(const char *split, int clubs, int up, int down, SweGroupsPlan *out)
{
    int groups = 0, size = 0;
    if (!split || sscanf(split, "%d groups of %d", &groups, &size) != 2) return 0;
    if (groups != 2 || size < 2 || clubs != groups * size || up < 0 || down < 0) return 0;
    out->clubs = clubs;
    out->size = size;
    out->rounds = 2 * (size - 1);
    out->up = (up + 1) / 2;
    out->down = (down + 1) / 2;
    return 1;
}

void season_swe_groups_deal(const int *north, int n, int size, int *group)
{
    int order[SWE_GROUPS_MAX_CLUBS];
    if (n > SWE_GROUPS_MAX_CLUBS) n = SWE_GROUPS_MAX_CLUBS;
    for (int i = 0; i < n; i++) order[i] = i;
    /* An insertion sort keeps clubs of an equal score in their order. */
    for (int i = 1; i < n; i++) {
        int club = order[i], k = i;
        for (; k > 0 && north[order[k - 1]] < north[club]; k--) order[k] = order[k - 1];
        order[k] = club;
    }
    for (int i = 0; i < n; i++) group[order[i]] = i < size ? 0 : 1;
}
