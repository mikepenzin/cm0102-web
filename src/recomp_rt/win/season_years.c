/* The year limits of the season code (0.6.2 slice 1, season.h). It reads no game memory, so ctest future_2300 and
 * season_part step it to 2301. */
#include "season.h"

#include <stdlib.h>
#include <string.h>

int season_date_jump(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_DATE_JUMP");
        on = v && !strcmp(v, "1");
    }
    return on;
}

int season_last_start_year(void) { return season_date_jump() ? SEASON_JUMP_LAST : SEASON_NEW_GAME_LAST; }

int season_year_ok(int y) { return y >= SEASON_FIRST_YEAR && y <= SEASON_YEAR_MAX; }

int season_rule_year(int y)
{
    if (!season_changing_rules()) return season_start_year();
    return season_year_ok(y) ? y : season_start_year();
}

int season_hooks_due(int season)
{
    int start = season_start_year();
    if (season < start || season > SEASON_YEAR_MAX) return 0;
    return season == start ? season != SEASON_FIRST_YEAR : season_changing_rules();
}
