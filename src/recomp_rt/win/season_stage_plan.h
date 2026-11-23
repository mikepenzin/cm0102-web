/* The plan of a qualifying stage's match days (items 203 and 204 of 0.6.2, season_stage_plan.c). Pure code, so
 * ctest season_stage_plan links it with the FIFA days. The game side is season_friendly_dates.c
 * (season_fifa_stage_begin and fifa_target_by). */
#ifndef RECOMP_SEASON_STAGE_PLAN_H
#define RECOMP_SEASON_STAGE_PLAN_H

#define SEASON_STAGE_MAX 64             /* rounds of a stage that the plan holds (a qualifying stage has up to 18) */
#define SEASON_STAGE_SPAN 1200          /* days from the first to the last candidate day, at most */
#define SEASON_STAGE_GAP 3              /* two rounds lie at least 3 days apart: more than CLASH_DAYS (2) of
                                         * season_friendly_dates.c, so no side has two matches within 2 days */

/* 1 when day z (days from 1 January 1970) is a FIFA day that the stage can use (a FIFA day of the list, or a real
 * South American matchday for that section, and not in the days of a final of the stage's confederation). */
typedef int (*SeasonStageFifa)(long z, void *ctx);

/* The plan of the n rounds of a stage, own[] their days in increasing order (days from 1 January 1970). Each round
 * takes a day of its own year, in round order, each at least SEASON_STAGE_GAP days after the one before:
 *   a FIFA day (fifa) from `first` (today + 2) to `last_fifa` (the stage's last day, or the end of the FIFA window
 *   that holds it or starts the day after it), or
 *   its own day, or
 *   another day from `first` to `last` (the stage's last day).
 * The plan puts the most rounds on FIFA days, then has the least total move (days between a round's own and
 * planned day); of equal plans the earliest days. Returns the rounds on FIFA days, and the plan in out[]; -1 when
 * no plan fits (n out of range, a span over SEASON_STAGE_SPAN, or too few days), with out[] = own[]. */
int season_stage_plan(const long own[], int n, long first, long last, long last_fifa, SeasonStageFifa fifa,
                      void *ctx, long out[]);

/* The days round r of plan[] (n rounds) may move a fixture to when it cannot take its planned day: from the day
 * before's share of the days between their two plans (or `first`) to the end of its own share before the next
 * round's (or `last`), in *lo and *hi. Each gap between two planned days is split in two halves SEASON_STAGE_GAP
 * apart, so the windows of two rounds never overlap and any two days in them stay SEASON_STAGE_GAP apart. */
void season_stage_window(const long plan[], int n, int r, long first, long last, long *lo, long *hi);

#endif
