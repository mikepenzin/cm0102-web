/* The plan of a qualifying stage's match days (items 203 and 204, season_stage_plan.c). Every plan keeps the rounds
 * in order, at least SEASON_STAGE_GAP days apart (more than CLASH_DAYS 2), each in its own year, on a FIFA day in
 * reach, its own day or a day within the stage's bounds.
 *   The AFC third round of the 2034 cycle (replay of run set a2021s1, 2031 to 2033): 10 weekly rounds on days 230 to
 *   293 of 2033, made on day 150, FIFA days 159 to 164 and 271 to 281: six rounds on FIFA days, in order.
 *   A UEFA group stage of 10 matchdays over two years, each a day after a FIFA day: all ten on FIFA days.
 *   The CONMEBOL section of the 2026 cycle on its real days (season_conmebol_days.c): no round moves.
 *   A stage with fewer FIFA days than rounds: those days are used, the rest keep their days, in order.
 *   Rounds too close for the gap in the days they can take: no plan. */
#include "season_stage_plan.h"
#include "season_uefa_days.h"
#include "season_conmebol_days.h"

#include <stdio.h>

static int fails;

#define CHECK(c, ...)                                                                                                 \
    do {                                                                                                              \
        if (!(c)) {                                                                                                   \
            fails++;                                                                                                  \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                               \
            printf(__VA_ARGS__);                                                                                      \
            printf("\n");                                                                                             \
        }                                                                                                             \
    } while (0)

/* FIFA days as listed in ctx: pairs of first and last day (days from 1970), ended by 0. */
static int listed(long z, void *ctx)
{
    const long *p = ctx;
    for (; p[0]; p += 2)
        if (z >= p[0] && z <= p[1]) return 1;
    return 0;
}

/* The FIFA days of the table. */
static int table(long z, void *ctx)
{
    (void)ctx;
    int y, m, d, doys[SEASON_FIFA_YEAR_MAX];
    season_from_abs_day(z, &y, &m, &d);
    int n = season_fifa_year_days(y, doys, SEASON_FIFA_YEAR_MAX), doy = season_day_of_year(y, m, d);
    for (int i = 0; i < n; i++)
        if (doys[i] == doy) return 1;
    return 0;
}

static int year_of(long z)
{
    int y, m, d;
    season_from_abs_day(z, &y, &m, &d);
    return y;
}

/* The rules every plan keeps. */
static void sound(const char *what, const long own[], const long out[], int n, long first, long last, long last_fifa,
                  SeasonStageFifa fifa, void *ctx)
{
    for (int r = 0; r < n; r++) {
        CHECK(r == 0 || out[r] - out[r - 1] >= SEASON_STAGE_GAP, "%s: round %d on %ld, %ld days after round %d", what,
              r + 1, out[r], out[r] - out[r - 1], r);
        CHECK(year_of(out[r]) == year_of(own[r]), "%s: round %d leaves its year", what, r + 1);
        int in = out[r] == own[r] || (out[r] >= first && out[r] <= last) ||
                 (out[r] >= first && out[r] <= last_fifa && fifa(out[r], ctx));
        CHECK(in, "%s: round %d on %ld, out of reach", what, r + 1, out[r]);
    }
}

static void afc_third(void)
{
    long y0 = season_abs_day(2033, 1, 1), own[10], out[10];
    for (int r = 0; r < 10; r++) own[r] = y0 + 230 + 7 * r;
    long days[] = {y0 + 159, y0 + 164, y0 + 271, y0 + 281, 0};
    long first = y0 + 152, last = y0 + 293;
    int on = season_stage_plan(own, 10, first, last, last, listed, days, out);
    sound("AFC third round", own, out, 10, first, last, last, listed, days);
    CHECK(on == 6, "AFC third round: %d rounds on FIFA days, want 6 (2 in June, 4 in September)", on);
    int june = 0, sept = 0;
    for (int r = 0; r < 10; r++) {
        june += out[r] >= y0 + 159 && out[r] <= y0 + 164;
        sept += out[r] >= y0 + 271 && out[r] <= y0 + 281;
        CHECK(r == 0 || out[r] > out[r - 1], "AFC third round: matchday %d (day %ld) before matchday %d (day %ld)",
              r + 1, out[r] - y0, r, out[r - 1] - y0);
    }
    CHECK(june == 2 && sept == 4, "AFC third round: %d in June, %d in September", june, sept);
    /* Rounds 1 and 2 in June, 3 to 5 keep their days, 6 to 9 in the window of days 271 to 281, 10 keeps its day:
     * the September rounds move 6 + 2 + 1 + 5 = 14 days (7 to 10 on 272, 275, 278 and 281 would move 24). */
    long want[10] = {161, 164, 244, 251, 258, 271, 274, 278, 281, 293};
    for (int r = 0; r < 10; r++)
        CHECK(out[r] == y0 + want[r], "AFC third round: matchday %d on day %ld, want %ld", r + 1, out[r] - y0,
              want[r]);
    /* The window of matchday 8: between matchdays 7 and 9 with the gap. */
    long lo, hi;
    season_stage_window(out, 10, 7, first, last, &lo, &hi);
    CHECK(lo == y0 + 277 && hi == y0 + 278, "AFC third round: matchday 8 window %ld to %ld", lo - y0, hi - y0);
    season_stage_window(out, 10, 0, first, last, &lo, &hi);
    CHECK(lo == first && hi == y0 + 161, "AFC third round: matchday 1 window %ld to %ld", lo - y0, hi - y0);
    /* Matchdays 2 (164) and 3 (244): the 80 days between are split, 2 up to 202 and 3 from 205, so a fallback of
     * one never passes the other's (review of items 203 and 204). */
    long hi2, lo3;
    season_stage_window(out, 10, 1, first, last, &lo, &hi2);
    season_stage_window(out, 10, 2, first, last, &lo3, &hi);
    CHECK(hi2 == y0 + 202 && lo3 == y0 + 205, "AFC third round: matchday 2 window to %ld, matchday 3 from %ld",
          hi2 - y0, lo3 - y0);
    for (int r = 0; r + 1 < 10; r++) {
        long a, b, c, e;
        season_stage_window(out, 10, r, first, last, &a, &b);
        season_stage_window(out, 10, r + 1, first, last, &c, &e);
        CHECK(a <= out[r] && out[r] <= b && c - b >= SEASON_STAGE_GAP,
              "AFC third round: windows of matchdays %d and %d overlap or hold no plan", r + 1, r + 2);
    }
}

/* UEFA qualifying groups: matchdays two a window, September 2030 to November 2031, each one day after a FIFA day of
 * the table (as the original's template days moved to a cycle fall near, not on, the window). Made on 1 August 2030. */
static void uefa_groups(void)
{
    long own[10], out[10];
    int n = 0;
    for (int y = 2030; y <= 2031 && n < 10; y++) {
        int doys[SEASON_FIFA_YEAR_MAX], k = season_fifa_year_days(y, doys, SEASON_FIFA_YEAR_MAX);
        long y0 = season_abs_day(y, 1, 1);
        /* sort the year's days */
        for (int i = 1; i < k; i++)
            for (int j = i; j > 0 && doys[j] < doys[j - 1]; j--) {
                int t = doys[j];
                doys[j] = doys[j - 1];
                doys[j - 1] = t;
            }
        for (int i = 0; i < k && n < 10; i++) {
            long z = y0 + doys[i] + 1;
            if (z < season_abs_day(2030, 9, 1) || (n && z - own[n - 1] < 3)) continue;
            if (n && z - own[n - 1] < 7 && n >= 2 && own[n - 1] - own[n - 2] < 7) continue;   /* two a window */
            own[n++] = z;
        }
    }
    CHECK(n == 10, "UEFA groups: %d matchdays made", n);
    long first = season_abs_day(2030, 8, 3), last = own[n - 1], last_fifa = last;
    while (table(last_fifa + 1, NULL)) last_fifa++;
    int on = season_stage_plan(own, n, first, last, last_fifa, table, NULL, out);
    sound("UEFA groups", own, out, n, first, last, last_fifa, table, NULL);
    CHECK(on == n, "UEFA groups: %d of %d matchdays on FIFA days", on, n);
    for (int r = 0; r < n; r++)
        CHECK(out[r] - own[r] <= 3 && own[r] - out[r] <= 3, "UEFA groups: matchday %d moves %ld days", r + 1,
              out[r] - own[r]);
}

/* The South American section of 2026 on its real days: FIFA days or real matchdays, so the plan moves none. */
static int south_american(long z, void *ctx) { return table(z, ctx) || season_conmebol_real_day(z); }

static void conmebol(void)
{
    long own[18], out[18];
    int n = season_conmebol_days(2026, 18, season_abs_day(2023, 7, 27), NULL, own);
    CHECK(n == 18, "CONMEBOL 2026: %d days", n);
    long first = season_abs_day(2023, 7, 29), last = own[17], last_fifa = last;
    while (table(last_fifa + 1, NULL)) last_fifa++;
    int on = season_stage_plan(own, 18, first, last, last_fifa, south_american, NULL, out);
    sound("CONMEBOL 2026", own, out, 18, first, last, last_fifa, south_american, NULL);
    CHECK(on == 18, "CONMEBOL 2026: %d of 18 rounds on their days", on);
    for (int r = 0; r < 18; r++)
        CHECK(out[r] == own[r], "CONMEBOL 2026: round %d moves %ld days", r + 1, out[r] - own[r]);
}

/* Six weekly rounds, two FIFA days 3 days apart in the middle: two rounds take them, the others keep their days. */
static void few_fifa(void)
{
    long y0 = season_abs_day(2040, 1, 1), own[6], out[6];
    for (int r = 0; r < 6; r++) own[r] = y0 + 100 + 7 * r;
    long days[] = {y0 + 116, y0 + 116, y0 + 119, y0 + 119, 0};
    long first = y0 + 90, last = own[5];
    int on = season_stage_plan(own, 6, first, last, last, listed, days, out);
    sound("few FIFA days", own, out, 6, first, last, last, listed, days);
    CHECK(on == 2, "few FIFA days: %d on FIFA days, want 2", on);
    long want[6] = {100, 107, 116, 119, 128, 135};
    for (int r = 0; r < 6; r++)
        CHECK(out[r] == y0 + want[r], "few FIFA days: round %d on day %ld, want %ld", r + 1, out[r] - y0, want[r]);
    /* No FIFA day at all: every round keeps its day. */
    long none[] = {0};
    CHECK(season_stage_plan(own, 6, first, last, last, listed, none, out) == 0, "no FIFA days: rounds on FIFA days");
    for (int r = 0; r < 6; r++) CHECK(out[r] == own[r], "no FIFA days: round %d moves", r + 1);
    /* Rounds 2 days apart and no room before or after: no plan. */
    long tight[3] = {y0 + 100, y0 + 102, y0 + 104};
    CHECK(season_stage_plan(tight, 3, y0 + 100, y0 + 104, y0 + 104, listed, none, out) == -1, "tight rounds: a plan");
    /* With room the plan spreads them in order. */
    CHECK(season_stage_plan(tight, 3, y0 + 90, y0 + 110, y0 + 110, listed, none, out) == 0, "spread rounds: no plan");
    sound("spread rounds", tight, out, 3, y0 + 90, y0 + 110, y0 + 110, listed, none);
    /* Days not in increasing order: no plan. */
    long back[2] = {y0 + 110, y0 + 100};
    CHECK(season_stage_plan(back, 2, first, last, last, listed, none, out) == -1, "rounds out of order: a plan");
}

int main(void)
{
    afc_third();
    uefa_groups();
    conmebol();
    few_fifa();
    if (fails) {
        printf("%d failures\n", fails);
        return 1;
    }
    printf("season_stage_plan: all passed\n");
    return 0;
}
