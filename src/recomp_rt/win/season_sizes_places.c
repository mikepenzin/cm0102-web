/* The places of Japan and Finland that the rules table decides (0.7.3 slice 4, open items 283 and 284). This file
 * reads only the tables, so tests/seasons/test_season_sizes_places.c checks it. The game code that uses the answers is
 * in season_playoffs.c (the tie between J1 and J2), season_sizes.c (J2's relegation places) and season_sizes_fin.c. */
#include "season_league.h"
#include "season_rules.h"
#include "season_sizes.h"

#include <stddef.h>

#define J2_THIRD 3                  /* the place of J2's club in the tie with J1's 16th */
#define J2_DIRECT 2                 /* J2's direct promotion places in those seasons */
#define J2_PLAYOFF_CLUBS 4          /* the 3rd to 6th */

int season_places_jap_cross(int season)
{
    const SeasonRule *lr = season_rules_find(season, "jap_j2", 0), *ur = season_rules_find(season, "jap_j1", 0);
    const SeasonLeagueRule *le = season_league_rule_of("jap_j2", lr), *ue = season_league_rule_of("jap_j1", ur);
    return le && ue && lr->up == J2_DIRECT && le->po_up == 1 && le->po_up_first == J2_THIRD && ue->po_down == 1;
}

/* The real 2023: J1 sends 1 down and takes the 2 direct places and the play-off winner, so it grows from 18 to 20. */
int season_places_jap_grows(int season, int direct)
{
    const SeasonRule *now = season_rules_find(season, "jap_j1", 0), *next = season_rules_find(season + 1, "jap_j1", 0);
    const SeasonRule *lr = season_rules_find(season, "jap_j2", 0);
    const SeasonLeagueRule *le = season_league_rule_of("jap_j2", lr);
    if (!now || !next || !le || le->po_up != J2_PLAYOFF_CLUBS || now->down < 0 || now->clubs <= 0) return 0;
    return next->clubs > now->clubs && next->clubs - now->clubs == direct + 1 - now->down;
}

/* The row of the edition plays its own real season, and the row of the next edition holds another count. */
int season_places_jap_j2_down(int edition)
{
    const SeasonRule *r = season_league_edition_row("jap_j2", edition);
    const SeasonRule *next = season_league_edition_row("jap_j2", edition + 1);
    if (!r || !next || !season_league_real_starts(r, edition) || r->down <= 0) return 0;
    return r->clubs > 0 && next->clubs > 0 && next->clubs != r->clubs ? r->down : 0;
}

/* Only the row of the edition's own real season: edition 2001 has no row and falls back to the row of the real 2002. */
int season_places_fin_no_playoff(int edition)
{
    const SeasonRule *r = season_league_edition_row("fin_prm", edition);
    const SeasonLeagueRule *e = season_league_rule_of("fin_prm", r);
    return e && season_league_real_starts(r, edition) && e->po_down == 0;
}
