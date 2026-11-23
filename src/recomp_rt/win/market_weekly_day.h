#ifndef RECOMP_MARKET_WEEKLY_DAY_H
#define RECOMP_MARKET_WEEKLY_DAY_H

#include <stdint.h>

/* 1 when the game date (year, day of the year from 0, as Date holds it at 0xae2c90) is a Monday: the market day of
 * RECOMP_TEST_MARKET_WEEKLY (open item 193). */
int market_weekly_is_day(int year, int day_of_year);

/* The value of g_computer_bids_enabled 0x9cd1f8 that the day loop reads at 0x5cdc4e: the flag itself when the switch
 * is off or on a market day, else 0. */
uint32_t market_weekly_flag(int on, uint32_t flag, int year, int day_of_year);

#endif
