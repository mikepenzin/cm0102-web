/* Test switch RECOMP_TEST_MARKET_WEEKLY=1 (open item 193, Max 8 November 2026; run set rows bw2001 and bw2025): the
 * AI transfer market of the day loop runs on one day in seven, the Mondays of the game date, so that a full-world
 * game reaches far years sooner. It is test only and changes the game; the run set's b2001 and b2025 play the real
 * market. Read once (getenv on the first call), off by default: off, the day loop reads the flag as the original does.
 *
 * The day loop game_main_day_loop 0x5cccd0 runs the AI market of the clubs in its morning slice: at 0x5cdc4e it reads
 * g_computer_bids_enabled 0x9cd1f8 (the Enable Computer Bids setting of the new game, set only by
 * create_game_from_setup_file 0x827110 at 0x82780e and 0x827833, read only here) and, when it is 0, jumps to 0x5cdd9f
 * past the club loop. The club loop 0x5cdc68..0x5cdd83 draws its random numbers (0x90cfc0 at 0x5cdc76, 0x5cdcf0,
 * 0x5cdd06), counts each club's days since its last shortlist update (+8 of the club's shortlist row, 0x5cdcce..
 * 0x5cdcd7), calls Shortlist_Manager::update_club_shortlist 0x83a730 (0x5cdd15) and make_club_bids 0x84a020
 * (0x5cdd6d), then the media calls 0x754a30 and 0x757c70 (0x5cdd8f, 0x5cdd9a). In the 2011 profile of b2001 this is
 * about 55 % of a game day (run-logs/i193-fullworld-speed/report.md). On a day that is not a market day the read
 * gives 0 (tools/recomp/market_weekly.py), so the whole loop rests as with computer bids off, a path of the original.
 * The day counts rest too, so a club updates its shortlist after 30 to 100 market days, not game days. The memory
 * flag never changes, so a save holds the setting as the player chose it.
 * Not gated: offers and transfers in progress (TRANSFER_MANAGER::daily_update 0x8c5880), contracts (0x4c7ac0) and
 * the shortlist offers of the daily staff pass (add_player_to_club_shortlists 0x842c00 from 0x5ce2d0). */
#include "win.h"
#include "../rt.h"
#include "market_weekly_day.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TODAY 0x00ae2c90u        /* Date: day of the year (s16, from 0) +0, year (s16) +2 */
#define SLICE 0x00ae2c88u        /* 0 morning, 1 afternoon, 2 evening (0x5cdc16..0x5cdc32) */

static int switch_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_TEST_MARKET_WEEKLY");
        on = v && !strcmp(v, "1");
        if (on) fprintf(stderr, "recomp: test switch: the AI transfer market runs on Mondays only\n");
    }
    return on;
}

/* 0x5cdc4e: mov eax, [0x9cd1f8]. */
uint32_t rt_market_bids_flag(uint32_t flag)
{
    if (!switch_on()) return flag;
    int year = (int16_t)rd16(g_mem, TODAY + 2), day = (int16_t)rd16(g_mem, TODAY);
    uint32_t out = market_weekly_flag(1, flag, year, day);
    if (out && g_mem[SLICE] == 0) fprintf(stderr, "recomp: weekly market: day %d of %d\n", day + 1, year);
    return out;
}
