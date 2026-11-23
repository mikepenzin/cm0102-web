/* The two legs of a qualifying tie (item 207, season_tie_legs.c). Every moved tie has both legs on FIFA days of one
 * window, in order, 3 to 6 days apart, the first leg on or after `first` and the second no later than its own day or
 * the end of the window that holds it, and before the next round's draw.
 *   The CONCACAF round of the 2032 replay (run set a2021s1): legs on days 156 and 170 of 2032, June window 154 to
 *   159: the legs move to 156 and 159.
 *   A tie already in one window (the AFC and OFC first rounds) keeps its days.
 *   A pair the sides cannot play is passed over; with none free the nearest pair is taken.
 *   A round of days 65 and 79 of 2032 (second leg in the March window, 77 to 82) moves to 77 and 80.
 *   With no window between `first` and the second leg the legs keep their days. */
#include "season_tie_legs.h"
#include "season_uefa_days.h"

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

static long day(int y, int doy) { return season_abs_day(y, 1, 1) + doy; }

static long refused;                     /* a first leg the sides cannot play */
static int not_on(long a, long b, void *ctx)
{
    (void)b, (void)ctx;
    return a != refused;
}
static int never(long a, long b, void *ctx)
{
    (void)a, (void)b, (void)ctx;
    return 0;
}

int main(void)
{
    long a, b, y0 = day(2032, 0);
    int r = season_tie_legs(day(2032, 156), day(2032, 170), day(2032, 140), 0, table, NULL, NULL, &a, &b);
    CHECK(r == 1 && a == y0 + 156 && b == y0 + 159, "2032 CONCACAF: %d, %ld and %ld", r, a - y0, b - y0);

    r = season_tie_legs(day(2032, 155), day(2032, 159), day(2032, 140), 0, table, NULL, NULL, &a, &b);
    CHECK(r == 0 && a == y0 + 155 && b == y0 + 159, "a tie in one window keeps its days: %d", r);

    /* first leg off a FIFA day, before the window: the nearest first leg of a pair, second leg on its own day */
    r = season_tie_legs(day(2032, 150), day(2032, 158), day(2032, 140), 0, table, NULL, NULL, &a, &b);
    CHECK(r == 1 && a == y0 + 154 && b == y0 + 158, "May 2032 tie: %d, %ld and %ld", r, a - y0, b - y0);

    refused = y0 + 156;
    r = season_tie_legs(day(2032, 156), day(2032, 170), day(2032, 140), 0, table, not_on, NULL, &a, &b);
    CHECK(r == 1 && a == y0 + 155 && b == y0 + 158, "day 156 not free: %d, %ld and %ld", r, a - y0, b - y0);

    r = season_tie_legs(day(2032, 156), day(2032, 170), day(2032, 140), 0, table, never, NULL, &a, &b);
    CHECK(r == 2 && a == y0 + 156 && b == y0 + 159, "no pair free: %d, %ld and %ld", r, a - y0, b - y0);

    /* drawn after the window began: the first leg on or after `first` */
    r = season_tie_legs(day(2032, 156), day(2032, 170), day(2032, 155), 0, table, NULL, NULL, &a, &b);
    CHECK(r == 1 && a == y0 + 156 && b == y0 + 159, "first 155: %d, %ld and %ld", r, a - y0, b - y0);
    r = season_tie_legs(day(2032, 156), day(2032, 170), day(2032, 157), 0, table, NULL, NULL, &a, &b);
    CHECK(r == -1 && a == y0 + 156 && b == y0 + 170, "no window from day 157 to 170: %d", r);

    /* a round before the March window whose second leg lies in it (days 65 and 79, window 77 to 82): 77 and 80;
     * with the next round drawn on day 80 the second leg must come by 79, and no pair fits */
    r = season_tie_legs(day(2032, 65), day(2032, 79), day(2032, 40), 0, table, NULL, NULL, &a, &b);
    CHECK(r == 1 && a == y0 + 77 && b == y0 + 80, "March 2032: %d, %ld and %ld", r, a - y0, b - y0);
    r = season_tie_legs(day(2032, 65), day(2032, 79), day(2032, 40), day(2032, 80), table, NULL, NULL, &a, &b);
    CHECK(r == -1 && a == y0 + 65 && b == y0 + 79, "March 2032, drawn on 80: %d", r);
    r = season_tie_legs(day(2032, 65), day(2032, 79), day(2032, 40), day(2032, 83), table, NULL, NULL, &a, &b);
    CHECK(r == 1 && a == y0 + 77 && b == y0 + 80, "March 2032, drawn on 83: %d, %ld and %ld", r, a - y0, b - y0);

    /* the second leg before its first: no plan */
    r = season_tie_legs(day(2032, 170), day(2032, 156), day(2032, 140), 0, table, NULL, NULL, &a, &b);
    CHECK(r == -1, "legs out of order: %d", r);

    /* every tie of 2025 to 2060, first legs on each day, gaps 7 to 28: the rules hold */
    int moved = 0, kept = 0, none = 0;
    for (int y = 2025; y <= 2060; y++)
        for (int d = 0; d < 360; d += 3)
            for (int g = 7; g <= 28; g += 7) {
                long l1 = day(y, d), l2 = l1 + g, first = l1 - 20;
                r = season_tie_legs(l1, l2, first, 0, table, NULL, NULL, &a, &b);
                if (r == 0) kept++;
                else if (r < 0) {
                    none++;
                    CHECK(a == l1 && b == l2, "%d day %d: no plan changes the days", y, d);
                } else {
                    moved++;
                    CHECK(a >= first && a < b && b <= l2 + SEASON_TIE_GAP_MAX && b - a >= SEASON_TIE_GAP_MIN && b - a <= SEASON_TIE_GAP_MAX,
                          "%d day %d gap %d: %ld and %ld", y, d, g, a - l1, b - l1);
                    for (long z = a; z <= b; z++) CHECK(table(z, NULL), "%d day %d: day %ld not a FIFA day", y, d, z - l1);
                }
            }
    CHECK(moved > 1000, "moved %d", moved);
    printf("ties: %d moved, %d kept, %d with no window\n", moved, kept, none);
    if (fails) printf("%d failures\n", fails);
    else printf("season_tie_legs: all checks pass\n");
    return fails != 0;
}
