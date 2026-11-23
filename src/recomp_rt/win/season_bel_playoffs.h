/* The Belgian play-offs after the Pro League's groups (season_bel_playoffs.c, issue 150). */
#ifndef RECOMP_SEASON_BEL_PLAYOFFS_H
#define RECOMP_SEASON_BEL_PLAYOFFS_H

#include <stdint.h>

/* After set_up_rules of a league: 1 for the Belgian First Division (season_playoffs_rules leaves it alone). */
int season_bel_po_rules(uint32_t obj);

/* advance_stage of a plain league: 1 when the First Division's stages took the call. */
int season_bel_po_advance(uint32_t obj);

/* After the day's update of a league: the groups of Play-off II with second-division clubs. */
void season_bel_po_update(uint32_t obj);

/* 1 when a result of stage belongs to these play-offs (no news of promotion or relegation). */
int season_bel_po_result(uint32_t obj, int stage);

/* The season change waits: 0 no such league in entry e, 1 complete, 2 not complete (*file gets its file). */
int season_bel_po_unfinished(int e, int year, const char **file);

/* Before the promotion chain: the statuses of the First Division clubs of the final round. */
void season_bel_po_settle(void);

/* After the promotion chain: one line for each First Division club that the final round decided. */
void season_bel_po_moved(void);

/* 1 for a First Division that plays these stages: season_bel_po_unfinished holds its season change. */
int season_bel_po_holds(uint32_t obj);

/* Around BelSecond::create_playoff_group 0x420230: the First Division clubs enter the final round. */
void season_bel_po_entrants_in(uint32_t second);
void season_bel_po_entrants_out(void);

#endif
