/* The day rule of the test switch RECOMP_TEST_MARKET_WEEKLY (open item 193, win/market_weekly.c). No game memory,
 * so ctest market_weekly tests it alone. */
#include "market_weekly_day.h"

/* Days from 1 January 1970 to 1 January of the year (proleptic Gregorian, days_from_civil for month 1, day 1). */
static long days_to_year(long y)
{
    y -= 1;                                          /* March-based years: 1 January lies in the year before */
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;                        /* 0 .. 399 */
    long doy = 306;                                  /* 1 January is day 306 of the March-based year */
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int market_weekly_is_day(int year, int day_of_year)
{
    long d = days_to_year(year) + day_of_year;       /* 1 January 1970 was a Thursday, so d + 3 is 0 on a Monday */
    long m = (d + 3) % 7;
    return (m < 0 ? m + 7 : m) == 0;
}

uint32_t market_weekly_flag(int on, uint32_t flag, int year, int day_of_year)
{
    return on && !market_weekly_is_day(year, day_of_year) ? 0 : flag;
}
