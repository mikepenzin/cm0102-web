/* Titles taken away after the season (0.6.2 item 108). The history rows give today's official record
 * (docs/research/history-0.6.0, Max 3 November 2026: do what real life does), so a title revoked and given to no club
 * has no row. A start season that began before the ruling still took that club as the title holder: the original's
 * holder read comp_util_get_latest_placed_club 0x4afd80 gives it there (wrap in season_history.c). */
#ifndef SEASON_TITLES_H
#define SEASON_TITLES_H

/* The name of the club that held the title of competition `comp` in the start season `start`, place k (0 the winner,
 * 1 the runner-up), when the newest history row has label year `label` and winner `row`. NULL when the history row
 * stands. nation gets the nation name of the clubs. */
const char *season_title_then(const char *comp, int start, int label, const char *row, int k, const char **nation);

/* 1 when the table holds competition `comp` in the start season `start`. */
int season_title_has(const char *comp, int start);

#endif
