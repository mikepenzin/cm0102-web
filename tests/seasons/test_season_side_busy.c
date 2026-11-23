/* Which fixtures take the day of a qualifier (open item 168, season_friendly_dates.c side_busy): the fixture of a
 * side of the qualifier, or of a national side of its nation, takes it. A league match of a club of that nation
 * does not: a league club has its nation at +0x53 too (0x90db33), and the call-up move moves its league fixtures
 * when more than 2 of its players are called up (0x7777e0 at 0x77795c, the count at 0x77793d). Before item 168 every club of the nation took the day. ctest season_side_busy. */
#include "win.h"
#include "season.h"
#include "season_uefa_days.h"
#include "season_names.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The game side of season_friendly_dates.c: never called by the function under test. */
Cpu cpu;
uint8_t *g_mem;
int rt_unwinding;
uint32_t rt_callback(uint32_t target, int nargs, const uint32_t *args) { (void)target, (void)nargs, (void)args; return 0; }
void o_005cc510(void) {}
void o_00685640(void) {}
int season_start_year(void) { return 2002; }
int season_game_changes(void) { return 1; }
int season_changing_rules(void) { return 1; }
int season_day_of_year(int y, int m, int d) { (void)y, (void)m; return d; }
void season_from_day_of_year(int y, int doy, int *m, int *d) { (void)y; *m = 1, *d = doy; }
int season_fifa_day(int y, int m, int want) { (void)y, (void)m; return want; }
int season_intl_finals_busy(int conf, int y, int doy) { (void)conf, (void)y, (void)doy; return 0; }
long season_abs_day(int y, int m, int d) { return (long)y * 372 + m * 31 + d; }
int season_conmebol_real_day(long z) { (void)z; return 0; }
void season_from_abs_day(long z, int *y, int *m, int *d)
{
    *y = (int)(z / 372), *m = (int)(z % 372 / 31), *d = (int)(z % 31);
}
int season_gold_cup_busy(int y, int doy) { (void)y, (void)doy; return 0; }

int season_fifa_fixture_takes_side(uint32_t h, uint32_t w, uint32_t a, uint32_t b);

#define MEM 0x1000000u
#define CLUBS 0xae23b4u                 /* as season_friendly_dates.c */
#define CLUB_COUNT 0xae235cu
#define NATION_COUNT 0xae2350u
#define CLUB_SIZE 0x245u
#define BASE 0x200000u                  /* the club table */
#define N_CLUBS 10                      /* 0 to 3 league clubs, 4 to 9 national sides of 3 nations (the last
                                         * 2 * nations rows, national_side) */
#define N_NATIONS 3

static int failures;

static void check(int ok, const char *text)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", text);
    if (!ok) failures++;
}

static uint32_t club(int i) { return BASE + (uint32_t)i * CLUB_SIZE; }

static void put32(uint32_t at, uint32_t v) { memcpy(g_mem + at, &v, 4); }

int main(void)
{
    g_mem = calloc(MEM, 1);
    if (!g_mem) return 2;
    put32(CLUBS, BASE);
    put32(CLUB_COUNT, N_CLUBS);
    put32(NATION_COUNT, N_NATIONS);
    /* Nations 0x500000 (Argentina), 0x500100 (Paraguay), 0x500200 (Brazil). League clubs: 0 and 1 of Argentina,
     * 2 of Paraguay, 3 of Argentina (the last league row, before the national sides). National sides: 4 Argentina, 5 Paraguay, 6 Brazil, 7 Argentina Under 21,
     * 8 Paraguay Under 21, 9 Brazil Under 21. */
    static const uint32_t nation[N_CLUBS] = {0x500000, 0x500000, 0x500100, 0x500000, 0x500000,
                                             0x500100, 0x500200, 0x500000, 0x500100, 0x500200};
    for (int i = 0; i < N_CLUBS; i++) put32(club(i) + 0x53, nation[i]);
    uint32_t arg = club(4), par = club(5);           /* the qualifier Argentina v Paraguay */

    check(season_fifa_fixture_takes_side(club(4), club(6), arg, par), "Argentina v Brazil takes the day");
    check(season_fifa_fixture_takes_side(club(9), club(5), arg, par), "Brazil Under 21 v Paraguay takes the day");
    check(season_fifa_fixture_takes_side(club(7), club(9), arg, par), "Argentina Under 21 v Brazil Under 21 takes it");
    check(season_fifa_fixture_takes_side(club(6), club(8), arg, par), "Brazil v Paraguay Under 21 takes it");
    check(!season_fifa_fixture_takes_side(club(0), club(1), arg, par),
          "a league match of two Argentine clubs leaves the day free");
    check(!season_fifa_fixture_takes_side(club(3), club(2), arg, par),
          "the last league row (Argentine) v a Paraguayan club leaves the day free");
    check(!season_fifa_fixture_takes_side(club(6), club(9), arg, par), "Brazil v Brazil Under 21 leaves it free");
    check(!season_fifa_fixture_takes_side(0, club(1), arg, par), "no home club and an Argentine club: free");

    free(g_mem);
    if (failures) {
        printf("%d failures\n", failures);
        return 1;
    }
    printf("season_side_busy: all checks passed\n");
    return 0;
}
