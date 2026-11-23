/* The meetings that the table of J1 and of the K League plays in a season (0.7.2 league sizes, slice 14; the module
 * comment of season_sizes_asia.c). Reads only the encoded rows: no game state, so tests/seasons/test_season_sizes_asia.c
 * checks it. */
#include "season_league.h"
#include "season_sizes.h"

#include <string.h>

/* The files whose table holds both halves of a row of halves. */
static int halves_in_table(const char *file)
{
    return file && (!strcmp(file, "jap_j1") || !strcmp(file, "kor_league"));
}

int season_sizes_asia_meetings(const char *file, int season, int meetings)
{
    if (!halves_in_table(file) || meetings < 1 || meetings > 2) return meetings;
    const SeasonLeagueRule *r = season_league_row(file, season);
    return r && r->halves == 1 ? meetings * 2 : meetings;
}

/* The dates. KorLeague::create_schedule 0x66b130 makes 27 dates, Wednesdays and Saturdays 3 or 4 days apart from 28
 * July (0x66b20d) to late October, so the plan finds few gaps of 7 to 14 days and starts earlier. The rows of 44
 * rounds (2002: 12 clubs in four meetings, 2011: 30 rounds of 16 and 14 of the sections) need 17 dates more, so the
 * earlier starts go back up to 17 weeks, to the end of March, when the real season begins (lead decision LD5,
 * slice 14). */
#define KOR_EARLIER 17

int season_sizes_asia_earlier(const char *file)
{
    return file && !strcmp(file, "kor_league") ? KOR_EARLIER : 0;
}
