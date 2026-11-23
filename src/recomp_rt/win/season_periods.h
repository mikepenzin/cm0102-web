/* The periods of a league of any size (0.7.2 league sizes, slice 10, D6; season_periods.c). Pure: no game memory. */
#ifndef SEASON_PERIODS_H
#define SEASON_PERIODS_H

/* starts: the rounds where the class's periods 1..parts-1 begin (HolFirst 8, 16, 26), of class_rounds (34). The
 * bound k (0..parts-2) scaled to the season's rounds, rounded half up, each period at least one round: the round
 * where period k + 1 begins. With fewer rounds than periods, the class's bound. -1 for no such bound. */
int season_period_bound(const int *starts, int parts, int class_rounds, int rounds, int k);

/* The rounds of period p (0..parts-1) at the season's rounds: the gap of its scaled bounds. */
int season_period_length(const int *starts, int parts, int class_rounds, int rounds, int p);

/* 1 when a period stage of `clubs` clubs and `rounds` rounds has all its matches: the played of its rows add up to
 * played_sum. An even count plays clubs / 2 matches a round, an odd count (clubs - 1) / 2 (a club rests). */
int season_period_stage_done(int clubs, int rounds, int played_sum);

/* Gap 33 (0.7.2): a row whose count of periods p is not the class's plays p periods of rounds / p rounds each, from
 * round 0; the rounds past p x (rounds / p) are in no period. The length, 0 when there is none (fewer rounds than
 * periods, p below 2). */
int season_period_own_length(int rounds, int p);

/* The period (0..p-1) of a round (0-based) in that form, -1 for a round in no period or no form. */
int season_period_own_of(int rounds, int p, int round);

/* The name of period 5 or 6 (gap 33: the class names periods 1 to 4 only), long (also plural) or short; NULL for
 * another period. */
const char *season_period_name(int period, int abbreviated);

#endif
