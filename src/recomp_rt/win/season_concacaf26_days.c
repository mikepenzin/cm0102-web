/* The match days of the CONCACAF World Cup qualifying in the 2026 shape (0.6.2 item 214), season_concacaf26_days.h.
 *
 * Real life, 2026 cycle (en.wikipedia.org, "2026 FIFA World Cup qualification (CONCACAF)", read 9 November 2026):
 * - first round: the four lowest-ranked teams in two ties, first legs 22 March 2024, second legs 26 March 2024;
 * - second round: six groups of five, single round robin, two home and two away, drawn on 25 January 2024, played
 *   in two windows, 5 to 11 June 2024 and 4 to 10 June 2025;
 * - third round: three groups of four, home and away, drawn on 12 June 2025, matchdays 4/5 and 8/9 September,
 *   9/10 and 13/14 October, 13 and 18 November 2025.
 * The game's League plays a group of 5 that meets once in 5 rounds (one side rests each round, League::create_fixtures
 * 0x6835c0 adds the resting place at 0x683c1b), so the second round takes 3 days in the June 2024 window and 2 in
 * the June 2025 window (inference: the page gives the windows only): 5, 8 and 11 June 2024, 6 and 10 June 2025, all
 * international match days of the FIFA days list. The third round takes the first day of each matchday. The first
 * round has no draw (the ties were set by the December 2023 FIFA ranking): its draw day is the second round's.
 *
 * The window rule (every cycle after 2026, an inference from the 2026 cycle). The FIFA days list has one window of
 * 6 days (Thursday to Tuesday) in March, June, September, October and November; a window here is a run of
 * consecutive FIFA days. With W the World Cup year:
 * - the draw on 25 January of W - 2;
 * - a pair of days of a window: its first day and the day 4 days later (Thursday and Monday, as the real days of
 *   22 and 26 March 2024 and 4 and 8 September 2025), or its last day when the window is shorter; a window of 6
 *   days holds two rounds at least 3 days apart (SEASON_STAGE_GAP), not three, and two windows one day apart (late
 *   September and early October) keep 3 days between their pairs;
 * - the first round on the pair of the March window of W - 2;
 * - the second round on the pair of the June window of W - 2, the first day of the March window of W - 1 and the
 *   pair of the June window of W - 1. Real 2026 played all five rounds in June (the windows of 2024 and 2025 held
 *   9 and 6 days); the list's June windows after 2026 hold 6 days (Thursday to Tuesday), so three rounds 3 days
 *   apart do not fit one of them, and the fifth round takes the March window of W - 1 (an inference; March of
 *   W - 1 holds no Nations League match after item 209, which keeps its editions to W - 4 and W - 3);
 * - the third round on the pairs of the first three windows from 1 September of W - 1. */
#include "season_concacaf26_days.h"
#include "season_uefa_days.h"

#define WINDOWS_MAX 64
#define LEG_GAP_MIN 3
#define PAIR_GAP 4

typedef struct {
    long first, last;
} Window;

/* The windows of the FIFA days from day a to day b. */
static int windows(long a, long b, SeasonStageFifa fifa, void *ctx, Window *w, int max)
{
    int n = 0;
    for (long z = a; z <= b; z++) {
        if (!fifa(z, ctx)) continue;
        if (n && w[n - 1].last == z - 1) w[n - 1].last = z;
        else if (n < max) w[n].first = w[n].last = z, n++;
        else break;
    }
    return n;
}

/* The first window that starts in month m of year y, or 0. */
static const Window *window_in(const Window *w, int n, int y, int m)
{
    for (int i = 0; i < n; i++) {
        int wy, wm, wd;
        season_from_abs_day(w[i].first, &wy, &wm, &wd);
        if (wy == y && wm == m) return &w[i];
    }
    return 0;
}

/* The second day of a pair in window w: 4 days after its first (Thursday and Monday, as the real third round's
 * 4 and 8 September and the first round's 22 and 26 March), or its last day when the window is shorter. */
static long second_day(const Window *w) { return w->last - w->first > PAIR_GAP ? w->first + PAIR_GAP : w->last; }

static int pair_ok(const Window *w) { return w && second_day(w) - w->first >= LEG_GAP_MIN; }

int season_cc26_days(int wc, SeasonStageFifa fifa, void *ctx, SeasonCc26Days *out)
{
    if (wc < 2026) return 0;
    if (wc == 2026) {
        static const short first[SEASON_CC26_FIRST][3] = {{2024, 3, 22}, {2024, 3, 26}};
        static const short second[SEASON_CC26_SECOND][3] = {
            {2024, 6, 5}, {2024, 6, 8}, {2024, 6, 11}, {2025, 6, 6}, {2025, 6, 10}};
        static const short third[SEASON_CC26_THIRD][3] = {{2025, 9, 4},   {2025, 9, 8},   {2025, 10, 9},
                                                          {2025, 10, 13}, {2025, 11, 13}, {2025, 11, 18}};
        out->draw = season_abs_day(2024, 1, 25);
        for (int i = 0; i < SEASON_CC26_FIRST; i++) out->first[i] = season_abs_day(first[i][0], first[i][1], first[i][2]);
        for (int i = 0; i < SEASON_CC26_SECOND; i++)
            out->second[i] = season_abs_day(second[i][0], second[i][1], second[i][2]);
        for (int i = 0; i < SEASON_CC26_THIRD; i++) out->third[i] = season_abs_day(third[i][0], third[i][1], third[i][2]);
        out->real = 1;
        return 1;
    }
    Window w[WINDOWS_MAX];
    int n = windows(season_abs_day(wc - 2, 1, 1), season_abs_day(wc - 1, 12, 31), fifa, ctx, w, WINDOWS_MAX);
    const Window *m2 = window_in(w, n, wc - 2, 3), *j2 = window_in(w, n, wc - 2, 6), *m1 = window_in(w, n, wc - 1, 3),
                 *j1 = window_in(w, n, wc - 1, 6);
    if (!pair_ok(m2) || !pair_ok(j2) || !m1 || !pair_ok(j1)) return 0;
    long sept = season_abs_day(wc - 1, 9, 1);
    int k = 0;
    while (k < n && w[k].first < sept) k++;
    if (k + 3 > n) return 0;
    for (int i = 0; i < 3; i++)
        if (!pair_ok(&w[k + i])) return 0;
    out->draw = season_abs_day(wc - 2, 1, 25);
    out->first[0] = m2->first, out->first[1] = second_day(m2);
    out->second[0] = j2->first, out->second[1] = second_day(j2), out->second[2] = m1->first;
    out->second[3] = j1->first, out->second[4] = second_day(j1);
    for (int i = 0; i < 3; i++) out->third[2 * i] = w[k + i].first, out->third[2 * i + 1] = second_day(&w[k + i]);
    out->real = 0;
    return 1;
}
