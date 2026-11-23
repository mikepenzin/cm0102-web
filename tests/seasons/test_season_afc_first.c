/* The AFC World Cup qualifying first round (item 196, season_afc_first_plan.c). For pools of 38 to 50 nations and
 * 37 to 40 places: W = pool - places ties of the bottom 2W, seed k against the k-th weakest, every team in one tie,
 * the byes the best, the list of the draw the byes then each tie's seed. For every cycle from 2006 to 2302 (campaign
 * year c = World Cup - 2), the round made on 1 August of c: both legs on FIFA days 3 to 6 days apart, the first
 * in October or November of c with no team busy, the second at least 15 days before the group draw (25 March of
 * c + 1, read from the object), the draw 7 days before the first leg. A late start
 * (made on 25 September of c) still finds a window; one made after 10 March of c + 1 finds none, nor a group draw on
 * 1 August of c. A busy first window gives another window from 1 August, or the first when none is left. */
#include "season_afc_first.h"
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

static int is_fifa_day(long z)
{
    int y, m, d, doys[SEASON_FIFA_YEAR_MAX];
    season_from_abs_day(z, &y, &m, &d);
    int n = season_fifa_year_days(y, doys, SEASON_FIFA_YEAR_MAX), doy = season_day_of_year(y, m, d);
    for (int i = 0; i < n; i++)
        if (doys[i] == doy) return 1;
    return 0;
}

static void plans(void)
{
    for (int places = 37; places <= 40; places++)
        for (int pool = 38; pool <= 50; pool++) {
            SeasonAfcFirstPlan p;
            int w = season_afc_first_plan(pool, places, &p), want = pool > places ? pool - places : 0;
            CHECK(w == want && p.ties == want, "pool %d, places %d: %d ties, want %d", pool, places, w, want);
            int seen[AFC_FIRST_MAX_POOL] = {0};
            for (int k = 0; k < w; k++) {
                CHECK(p.pair[k][0] == pool - 2 * w + k, "pool %d: tie %d seed %d", pool, k, p.pair[k][0]);
                CHECK(p.pair[k][1] == pool - 1 - k, "pool %d: tie %d other %d", pool, k, p.pair[k][1]);
                seen[p.pair[k][0]]++;
                seen[p.pair[k][1]]++;
            }
            int listed = w ? places : pool;
            for (int i = 0; i < listed; i++) {
                int want_i = i < p.byes ? i : p.pair[i - p.byes][0];
                CHECK(p.list[i] == want_i, "pool %d, places %d: place %d holds %d, want %d", pool, places, i,
                      p.list[i], want_i);
                if (i < p.byes) seen[p.list[i]]++;
            }
            for (int i = 0; i < pool; i++)
                CHECK(seen[i] == 1, "pool %d, places %d: nation %d in %d parts", pool, places, i, seen[i]);
            CHECK(!w || p.byes + w == places, "pool %d: %d byes and %d winners for %d places", pool, p.byes, w,
                  places);
        }
    /* Brunei's case: 46 nations, 40 places: 6 ties of the bottom 12 (as 2018 and 2022). */
    SeasonAfcFirstPlan p;
    CHECK(season_afc_first_plan(46, 40, &p) == 6 && p.pair[0][1] == 45 && p.pair[0][0] == 34, "46 nations: 6 ties, the last plays");
    /* A pool of more than twice the places takes the best 2 x places. */
    CHECK(season_afc_first_plan(90, 40, &p) == 40 && p.byes == 0, "90 nations: 40 ties, no bye");
}

static void days(int c, long today, long groups, int want, const char *what)
{
    long draw = 0, leg1 = 0, leg2 = 0;
    int ok = season_afc_first_days(c, today, groups, NULL, NULL, &draw, &leg1, &leg2);
    CHECK(ok == want, "%d %s: %s", c + 2, what, ok ? "a window" : "no window");
    if (!ok || !want) return;
    int y1, m1, d1, y2, m2, d2;
    season_from_abs_day(leg1, &y1, &m1, &d1);
    season_from_abs_day(leg2, &y2, &m2, &d2);
    CHECK(is_fifa_day(leg1) && is_fifa_day(leg2), "%d %s: legs %04d-%02d-%02d and %04d-%02d-%02d not FIFA days",
          c + 2, what, y1, m1, d1, y2, m2, d2);
    CHECK(leg2 - leg1 >= 3 && leg2 - leg1 <= 6, "%d %s: legs %ld days apart", c + 2, what, leg2 - leg1);
    CHECK(leg1 >= season_abs_day(c, 10, 1) && leg1 >= today + 8, "%d %s: first leg %04d-%02d-%02d too early", c + 2,
          what, y1, m1, d1);
    CHECK(leg2 <= groups - AFC_FIRST_DRAW_GAP, "%d %s: second leg %04d-%02d-%02d less than %d days before the draw",
          c + 2, what, y2, m2, d2, AFC_FIRST_DRAW_GAP);
    CHECK(leg2 < groups, "%d %s: a leg on or after the draw", c + 2, what);
    CHECK(draw > today && draw < leg1 && (draw == leg1 - 7 || draw == today + 1), "%d %s: drawn %ld days before",
          c + 2, what, leg1 - draw);
    CHECK(what[0] != 'm' || (y1 == c && (m1 == 10 || m1 == 11)), "%d made in September: first leg %04d-%02d-%02d not in October or November",
          c + 2, y1, m1, d1);
}

static long first_leg;

/* 1 when the FIFA days from 1 August of c to 15 days before the draw hold a window whose first leg is not a1. */
static int other_window(int c, long a1, long groups)
{
    for (long z = season_abs_day(c, 8, 1); z <= groups - AFC_FIRST_DRAW_GAP; z++)
        for (long g = 3; g <= 6 && z != a1; g++)
            if (is_fifa_day(z) && is_fifa_day(z + g) && z + g <= groups - AFC_FIRST_DRAW_GAP) return 1;
    return 0;
}
static int not_first(long leg1, long leg2, void *ctx) { (void)leg2, (void)ctx; return leg1 != first_leg; }
static int none(long leg1, long leg2, void *ctx) { (void)leg1, (void)leg2, (void)ctx; return 0; }

/* A window where a team of the round has another match is passed over; when every window has one, the first. */
static void busy_windows(void)
{
    for (int wc = 2006; wc <= 2302; wc += 4) {
        int c = wc - 2;
        long groups = season_abs_day(c + 1, 3, 25), today = season_afc_first_build_day(c), d, a1, a2, b1, b2;
        CHECK(season_afc_first_days(c, today, groups, NULL, NULL, &d, &a1, &a2), "%d: a window", wc);
        first_leg = a1;
        /* Another window (from 1 August), if the FIFA days hold one: it is taken. */
        CHECK(season_afc_first_days(c, today, groups, not_first, NULL, &d, &b1, &b2), "%d: a window", wc);
        CHECK(b1 != a1 ? is_fifa_day(b1) && is_fifa_day(b2) && b2 - b1 >= 3 && b2 - b1 <= 6 &&
                             b1 >= season_abs_day(c, 8, 1) && b2 <= groups - AFC_FIRST_DRAW_GAP
                       : !other_window(c, a1, groups),
              "%d: the first window busy: %s", wc, b1 != a1 ? "another window" : "no other window, the first");
        CHECK(season_afc_first_days(c, today, groups, none, NULL, &d, &b1, &b2) && b1 == a1 && b2 == a2,
              "%d: every window busy, the first", wc);
    }
}

int main(void)
{
    plans();
    busy_windows();
    for (int wc = 2006; wc <= 2302; wc += 4) {
        int c = wc - 2;
        long groups = season_abs_day(c + 1, 3, 25);              /* +0xb6, +0xb8 of set_up_rules 0x912eb0 */
        days(c, season_afc_first_build_day(c), groups, 1, "made on 1 August");
        days(c, season_abs_day(c, 9, 25), groups, 1, "late, made on 25 September");
        days(c, season_abs_day(c + 1, 3, 11), groups, 0, "too late, made on 11 March");
        /* The last leg day follows the draw day the object holds: a draw on 1 August of c leaves no window. */
        days(c, season_afc_first_build_day(c), season_abs_day(c, 8, 1), 0, "a draw on 1 August");
    }
    if (fails) {
        printf("%d failures\n", fails);
        return 1;
    }
    printf("ok: plans of 38 to 50 nations, days of every cycle 2006 to 2302\n");
    return 0;
}
