/* ctest market_weekly (open item 193): the day rule of the test switch RECOMP_TEST_MARKET_WEEKLY
 * (src/recomp_rt/win/market_weekly_day.c). On, the day loop's market runs on one day in seven, the Mondays of the game
 * date, also across year ends and leap years from 2001 to 2301. Off, it runs every day the flag allows. */
#include "market_weekly_day.h"

#include <stdio.h>

static int leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int main(void)
{
    int bad = 0, gap = 0, days = 0, market = 0, max_gap = 0, min_gap = 1 << 30;
    /* Known Mondays (Python datetime): 1 January 2001, 29 February 2016 (day 59), 1 March 2100 (day 59, 2100 is no
     * leap year), 31 December 2300 (day 364). Known Tuesdays: 2 January 2001, 1 January 2301. */
    if (!market_weekly_is_day(2001, 0) || !market_weekly_is_day(2016, 59) || !market_weekly_is_day(2100, 59) ||
        !market_weekly_is_day(2300, 364) || market_weekly_is_day(2001, 1) || market_weekly_is_day(2301, 0)) {
        printf("FAIL  a known Monday or other day\n");
        bad++;
    }
    int last = -1;
    for (int y = 2001; y <= 2301; y++) {
        for (int d = 0; d < 365 + leap(y); d++, days++) {
            uint32_t off = market_weekly_flag(0, 1, y, d), on = market_weekly_flag(1, 1, y, d);
            if (off != 1) { printf("FAIL  switch off: no market on day %d of %d\n", d + 1, y); bad++; }
            if (market_weekly_flag(0, 0, y, d) || market_weekly_flag(1, 0, y, d)) {
                printf("FAIL  computer bids off: a market on day %d of %d\n", d + 1, y);
                bad++;
            }
            if (on) {
                if (last >= 0) {
                    gap = days - last;
                    if (gap > max_gap) max_gap = gap;
                    if (gap < min_gap) min_gap = gap;
                }
                last = days;
                market++;
            }
        }
    }
    if (min_gap != 7 || max_gap != 7) {
        printf("FAIL  market days %d to %d days apart, not 7\n", min_gap, max_gap);
        bad++;
    }
    printf("%s  market_weekly: %d market days of %d (2001 to 2301), 7 days apart; off: every day\n",
           bad ? "FAIL" : "ok  ", market, days);
    return bad != 0;
}
