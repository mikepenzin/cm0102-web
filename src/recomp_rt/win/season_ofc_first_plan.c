/* The plan and the days of the OFC preliminary round (item 198, season_ofc_first.h). Pure code.
 *
 * Real life (docs/research/rules-0.6.0/int-world.tsv, rows wc_oceania_league): every OFC member enters World Cup
 * qualifying, and the lowest-ranked play a first round before the main stage: 2014 cycle, 4 teams in Samoa,
 * 22 to 26 November 2011 (int-world-128); 2018 cycle, 4 teams in Tonga, 31 August to 4 September 2015
 * (int-world-176, en.wikipedia.org "2018 FIFA World Cup qualification - OFC first round"); 2026 cycle, 4 teams in
 * Apia, 6 to 9 September 2024 (int-world-284, en.wikipedia.org "2026 FIFA World Cup qualification (OFC)"). The game
 * keeps the original's 10 group places (0x91fbda) and plays the round as two-leg ties of the 2W weakest, so that
 * W = pool - 10 nations go out and the groups stay as the original makes them (item 198, option a). The game's
 * group draw is in January of the campaign year, so the round plays in the autumn before it. The tie
 * pairs and the list are those of the AFC first round (item 196, season_afc_first_plan.c). */
#include "season_ofc_first.h"
#include "season_uefa_days.h"

#include <stdlib.h>
#include <string.h>

int season_ofc_first_plan(int pool, int places, SeasonOfcFirstPlan *plan)
{
    memset(plan, 0, sizeof *plan);
    if (places < 0) places = 0;
    if (places > OFC_FIRST_MAX_POOL / 2) places = OFC_FIRST_MAX_POOL / 2;
    if (pool > 2 * places) pool = 2 * places;
    if (pool < 0) pool = 0;
    int w = pool > places ? pool - places : 0;
    plan->ties = w;
    plan->byes = (pool < places ? pool : places) - w;
    for (int k = 0; k < w; k++) {
        plan->pair[k][0] = places - w + k;
        plan->pair[k][1] = pool - 1 - k;
    }
    return w;
}

static int cmp_long(const void *a, const void *b)
{
    long x = *(const long *)a, y = *(const long *)b;
    return x < y ? -1 : x > y;
}

int season_ofc_first_days(long group_draw, long today, long *draw, long *leg1, long *leg2)
{
    long days[3 * SEASON_FIFA_YEAR_MAX];
    int n = 0, y0, y1, m, d;
    season_from_abs_day(group_draw - OFC_FIRST_LEAD, &y0, &m, &d);
    season_from_abs_day(group_draw, &y1, &m, &d);
    for (int y = y0; y <= y1 && n + SEASON_FIFA_YEAR_MAX <= 3 * SEASON_FIFA_YEAR_MAX; y++) {
        int doys[SEASON_FIFA_YEAR_MAX], k = season_fifa_year_days(y, doys, SEASON_FIFA_YEAR_MAX);
        long base = season_abs_day(y, 1, 1);
        for (int i = 0; i < k; i++) days[n++] = base + doys[i];
    }
    qsort(days, (size_t)n, sizeof days[0], cmp_long);
    long from = group_draw - OFC_FIRST_LEAD, last = group_draw - OFC_FIRST_MARGIN;
    if (from < today + 8) from = today + 8;
    for (int i = 0; i < n; i++) {
        if (days[i] < from) continue;
        for (int j = i + 1; j < n && days[j] - days[i] <= 6; j++) {
            if (days[j] - days[i] < 3 || days[j] > last) continue;
            *leg1 = days[i];
            *leg2 = days[j];
            *draw = days[i] - 7 > today ? days[i] - 7 : today + 1;
            return 1;
        }
    }
    return 0;
}
