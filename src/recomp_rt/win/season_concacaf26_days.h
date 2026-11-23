/* The match days of the CONCACAF World Cup qualifying in the 2026 shape (0.6.2 item 214, season_concacaf26_days.c).
 * Pure code, so ctest season_concacaf26_days links it with the FIFA days. The game side is season_concacaf26.c. */
#ifndef RECOMP_SEASON_CONCACAF26_DAYS_H
#define RECOMP_SEASON_CONCACAF26_DAYS_H

#include "season_stage_plan.h"          /* SeasonStageFifa */

#define SEASON_CC26_FIRST 2             /* the two legs of the first round */
#define SEASON_CC26_SECOND 5            /* the second round: groups of 5 that meet once, one side rests each day */
#define SEASON_CC26_THIRD 6             /* the third round: groups of 4 that meet twice */

/* Days from 1 January 1970. */
typedef struct {
    long draw;                          /* the draw of the first round */
    long first[SEASON_CC26_FIRST];
    long second[SEASON_CC26_SECOND];
    long third[SEASON_CC26_THIRD];
    int real;                           /* 1: the real days of the cycle, 0: the window rule */
} SeasonCc26Days;

/* The days of the qualifying for the World Cup of year wc (2026 or later): the real days of 2026, else the window
 * rule on the FIFA days (fifa). Returns 1, or 0 when the FIFA days do not give the windows the rule needs. */
int season_cc26_days(int wc, SeasonStageFifa fifa, void *ctx, SeasonCc26Days *out);

#endif
