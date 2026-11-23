/* The two legs of a qualifying tie (item 207 of 0.6.2). Pure code (season_tie_legs.h).
 *
 * Cup::create_tie_fixtures 0x51cb90 dates a tie's first leg from its round record (+3 day, +5 year offset,
 * 0x51ce6c..0x51ce7c) and the second leg the gap byte +0x22 later (0x51d13c..0x51d175). The snap of single
 * qualifiers (season_friendly_dates.c) does not move these legs one by one, as the second leg could then come before
 * the first. So the second leg kept its template day: in a 2021 start (run set a2021s1, replay of 2031 to 2033) a
 * CONCACAF round had its first leg on day 156 of 2032 and a gap of 14, so the second leg fell on day 170, after the
 * June window (days 154 to 159), and add_fixture's clash move (0x5aaa25) took it to day 167.
 *
 * Real life: both legs of a qualifying tie are played in one FIFA window, in order. In the CONCACAF first round of the
 * 2026 cycle Anguilla v Turks and Caicos Islands played its legs on 22 and 26 March 2024, in the March window
 * (en.wikipedia.org, "2026 FIFA World Cup qualification (CONCACAF)", read 9 November 2026).
 *
 * The rule. Both legs on FIFA days of one window (every day from the first leg to the second a FIFA day), the second
 * SEASON_TIE_GAP_MIN to SEASON_TIE_GAP_MAX days after the first, the first leg on or after `first` (today + 2), the
 * second leg no later than its own day or the last day of the FIFA window that holds its own day, and before `draw`
 * (the next round's draw, so it keeps its day). The window bound lets a round of days 65 and 79 of 2032 (the March
 * window: 77 to 82) play on 77 and 80. Of these pairs the first leg nearest its own day wins (the earlier of two at
 * one distance), with the second leg on its own day when the pair allows it, else the earliest. A pair the sides cannot play (free) is passed over; when no pair is free the nearest
 * pair is taken all the same. A tie already on such a pair keeps its days. */
#include "season_tie_legs.h"

/* 1 when b is SEASON_TIE_GAP_MIN to SEASON_TIE_GAP_MAX days after a and every day from a to b is a FIFA day. */
static int pair_in_window(long a, long b, SeasonStageFifa fifa, void *ctx)
{
    if (b - a < SEASON_TIE_GAP_MIN || b - a > SEASON_TIE_GAP_MAX) return 0;
    for (long z = a; z <= b; z++)
        if (!fifa(z, ctx)) return 0;
    return 1;
}

/* The second leg for a first leg on day a: its own day leg2 when the pair is in one window, else the earliest day
 * that is, no later than last; 0 when none. */
static long second_leg(long a, long leg2, long last, SeasonStageFifa fifa, void *ctx)
{
    if (leg2 <= last && pair_in_window(a, leg2, fifa, ctx)) return leg2;
    for (long b = a + SEASON_TIE_GAP_MIN; b <= a + SEASON_TIE_GAP_MAX && b <= last; b++)
        if (pair_in_window(a, b, fifa, ctx)) return b;
    return 0;
}

int season_tie_legs(long leg1, long leg2, long first, long draw, SeasonStageFifa fifa, SeasonTieFree free_fn,
                    void *ctx, long *out1, long *out2)
{
    *out1 = leg1, *out2 = leg2;
    if (leg2 <= leg1 || leg2 - leg1 > SEASON_STAGE_SPAN) return -1;
    if (leg1 >= first && pair_in_window(leg1, leg2, fifa, ctx)) return 0;
    long last = leg2;
    if (fifa(leg2, ctx))
        while (last < leg2 + SEASON_TIE_GAP_MAX && fifa(last + 1, ctx)) last++;
    if (draw && last >= draw) last = draw - 1 > leg2 ? draw - 1 : leg2;
    long fallback1 = 0, fallback2 = 0;
    for (long dist = 0; dist <= SEASON_STAGE_SPAN; dist++)
        for (int sign = -1; sign <= 1; sign += 2) {
            long a = leg1 + sign * dist;
            if ((dist == 0 && sign > 0) || a < first || a + SEASON_TIE_GAP_MIN > last) continue;
            if (!fifa(a, ctx)) continue;
            long b = second_leg(a, leg2, last, fifa, ctx);
            if (!b) continue;
            if (!free_fn || free_fn(a, b, ctx)) {
                *out1 = a, *out2 = b;
                return 1;
            }
            if (!fallback1) fallback1 = a, fallback2 = b;
        }
    if (!fallback1) return -1;
    *out1 = fallback1, *out2 = fallback2;
    return 2;
}
