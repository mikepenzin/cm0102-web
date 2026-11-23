/* Play-off winners that go up with no play-off of the game (0.6.2 open item 63, season_playoffs.h). Host code only. */
#include "season_playoffs.h"
#include "season_league.h"
#include "season_rules.h"

#include <string.h>

int season_playoffs_winners_up(const char *file, int season)
{
    int s = season_rules_last_season(season, file);
    const SeasonRule *r = s ? season_rules_find(s, file, 0) : NULL;
    if (!r || r->up < 0 || !r->playoffs_text || !strstr(r->playoffs_text, "the two winners go up")) return 0;
    for (int i = 0; i < season_league_rule_count; i++) {
        const SeasonLeagueRule *e = &season_league_rules[i];
        if (e->season == s && !strcmp(e->file, file)) return e->po_up == 4 && e->po_up_first == r->up + 1 ? 2 : 0;
    }
    return 0;
}

/* Gap row 57 and open item 249: the rows of Série B 2002 to 2005 send the two finalists of a play-off of the top 8 up
 * (2002: two-legged knockout, the finalists promoted; 2003 to 2005: two groups of 4 and a final group, its top 2
 * promoted). Up 2 counts the finalists; the encoded play-off has places 1 to 8. */
int season_playoffs_finalists_up(const char *file, int season)
{
    int s = season_rules_last_season(season, file);
    const SeasonRule *r = s ? season_rules_find(s, file, 0) : NULL;
    if (!r || r->up != 2) return 0;
    for (int i = 0; i < season_league_rule_count; i++) {
        const SeasonLeagueRule *e = &season_league_rules[i];
        if (e->season == s && !strcmp(e->file, file)) return e->po_up == 8 && e->po_up_first == 1 ? 2 : 0;
    }
    return 0;
}
