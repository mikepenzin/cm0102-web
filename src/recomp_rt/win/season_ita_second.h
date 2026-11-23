/* Italy: a second team never plays in its first team's division or above (open item 117; season_ita_second.c). */
#ifndef SEASON_ITA_SECOND_H
#define SEASON_ITA_SECOND_H

#include <stdint.h>

/* The chain of the Serie C era (season_divisions_ita.c), in this order:
 * - before league_promote_and_relegate 0x689c80 of Serie A and B: a, b the league objects, g the group objects;
 * - after it, before the choice of the clubs that go up to Serie B (choose_promoted also asks
 *   season_ita_second_up_barred for each row it may send up);
 * - after the chain's relegations to the Serie D: down sends a club to the Serie D as the chain does (the regroup
 *   does not take it back this season); season the rules season that starts. */
void season_ita_second_before_ab(uint32_t a, uint32_t b, const uint32_t *g, int groups);
void season_ita_second_before_promoted(const uint32_t *g, int groups);
int season_ita_second_up_barred(uint32_t club);
void season_ita_second_settle(uint32_t serie_a, int season, void (*down)(uint32_t serie_a, uint32_t club, uint32_t d));

/* The regroup's pools: 1 when an Italian second team may not join Serie C (its first team plays Serie C or lower). */
int season_ita_second_pool_skip(uint32_t club);

/* After the regroup of the Serie C era: the Serie D rule by season again (the regroup's cut sends clubs there). */
void season_ita_second_after_regroup(void);

#endif
