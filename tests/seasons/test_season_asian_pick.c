/* The Asian Cup entrants by reputation (GitHub issue 136; src/recomp_rt/win/season_intl_cups.c). The entrants of
 * AsiaNations::select_and_draw_teams 0x40fdf0 take the hosts, then call 0x4afbd0 at 0x40ffc8 on the other Asian
 * national teams (list esp + 0x228, count ebp, places 0xf2 - hosts). 0x4afbd0 sorts the list by club reputation
 * (+0x80, compare 0x4b07d0, best first) and then swaps two random places of the top min(3 * places, count) as
 * often (0x4afc2f..0x4afc57), so the entrants were a random pick of the top three times the places. The fix keeps
 * the original's call (the same random numbers) and then gives the list its sorted order again, so the entrants
 * are the best sides after the hosts, as the ACN takes them (qsort 0x945333 at 0x402446). The checks:
 * - 12 teams, 1 host: the 11 best of 40 in order of reputation;
 * - 24 teams, 2 hosts: the 22 best of 45 (the whole list was the band of the original);
 * - the original runs once per pick, so the random numbers stay the same;
 * - changing rules off: the same pick (Max, 13 November 2026: the fix applies in the base game too);
 * - RECOMP_ASIAN_PICK=original: the original's pick, the test switch of the same-save run asian-pick-off.
 * ctest season_asian_pick. */
#include "win.h"
#include "season.h"
#include "season_intl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int changing = 1;
int season_code_on(void) { return 1; }
int season_start_year(void) { return SEASON_FIRST_YEAR; }
int season_changing_rules(void) { return changing; }
int season_change_is_creating(void) { return 0; }

Cpu cpu;
uint8_t *g_mem;
int rt_unwinding;
#define MEM 0x200000u
#define REP 0x80u

/* qsort 0x945333 with compare 0x4b07d0: a null club first, then by reputation, best first. */
static int compare(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    if (!x) return y != 0;
    if (!y) return -1;
    return (int16_t)rd16(g_mem, y + REP) - (int16_t)rd16(g_mem, x + REP);
}

static uint32_t seed = 1, rand_calls, pick_calls;
static int game_rand(int n) { rand_calls++; seed = seed * 1103515245u + 12345u; return (int)((seed >> 16) % (uint32_t)n); }

uint32_t rt_callback(uint32_t target, int nargs, const uint32_t *args)
{
    if (target == 0x00945333u && nargs == 4 && args[2] == 4u && args[3] == 0x004b07d0u) {
        qsort(g_mem + args[0], args[1], 4, compare);
        return 0;
    }
    if (target == 0x004afbd0u && nargs == 3) {      /* the original as read at 0x4afbd0 */
        pick_calls++;
        uint32_t list = args[0];
        int count = (int16_t)args[1], places = (int16_t)args[2];
        if (count < 0 || count < places || places < 1) return 0;
        qsort(g_mem + list, (size_t)count, 4, compare);
        int band = 3 * places < count ? 3 * places : count;
        for (int k = 0; k < band; k++) {
            int i = game_rand(band), j = game_rand(band);
            uint32_t t = rd32(g_mem, list + 4u * (uint32_t)i);
            wr32(g_mem, list + 4u * (uint32_t)i, rd32(g_mem, list + 4u * (uint32_t)j));
            wr32(g_mem, list + 4u * (uint32_t)j, t);
        }
        return 0;
    }
    fprintf(stderr, "unexpected call of 0x%x\n", target);
    exit(2);
}
long season_abs_day(int y, int m, int d) { (void)y, (void)m, (void)d; return 0; }
void season_from_abs_day(long z, int *y, int *m, int *d) { (void)z; *y = 2004, *m = 1, *d = 1; }
int season_day_of_year(int y, int m, int d) { (void)y, (void)m; return d; }
void season_weekday_date(int y, int m, int d, int wd, int *oy, int *om, int *od) { (void)wd; *oy = y, *om = m, *od = d; }
void season_intl_shift_more(int days) { (void)days; }
int season_intl_shift_now(void) { return 0; }
int season_intl_host_ids(uint32_t key, int year, int ids[6]) { (void)key, (void)year, (void)ids; return 0; }
int season_intl_confed_june(void) { return 1; }
void o_0040fdf0(void) {}
void o_0040f8a0(void) {}
void o_004027d0(void) {}
void o_004103a0(void) {}
void o_004029c0(void) {}
void o_00410560(void) {}
void o_00402090(void) {}
void o_0040fa90(void) {}
void o_004022c0(void) {}
void season_intl_draw_hosts(const char *file, int year);
void season_intl_draw_hosts(const char *file, int year) { (void)file, (void)year; }

/* Before issue 136 the site called the original: this stands in, so the test builds and fails on that code. */
__attribute__((weak)) void rt_intl_cup_asian_pick(uint32_t self, uint32_t list, uint32_t count, uint32_t places)
{
    (void)self;
    uint32_t a[3] = {list, count, places};
    rt_callback(0x004afbd0u, 3, a);
}

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

#define SELF 0x10000u
#define LIST 0x20000u
#define CLUBS 0x30000u
#define CLUB_SIZE 0x100u

/* count national teams with the reputations 100 * (k + 1) in a mixed order (7 is prime to every count here). */
static void make_list(int count)
{
    memset(g_mem + SELF, 0, 0x100);
    wr16(g_mem, SELF + 0x40u, 2004);
    for (int k = 0; k < count; k++) {
        uint32_t club = CLUBS + (uint32_t)k * CLUB_SIZE;
        wr16(g_mem, club + REP, (uint32_t)(100 * (k + 1)));
        wr32(g_mem, LIST + 4u * (uint32_t)((k * 7) % count), club);
    }
}

/* The places of the pick hold the best sides, in order of reputation. */
static int best_first(int count, int places)
{
    for (int i = 0; i < places; i++) {
        uint32_t club = rd32(g_mem, LIST + 4u * (uint32_t)i);
        if (!club || (int16_t)rd16(g_mem, club + REP) != 100 * (count - i)) return 0;
    }
    return 1;
}

static void pick(int count, int places)
{
    make_list(count);
    pick_calls = rand_calls = 0;
    rt_intl_cup_asian_pick(SELF, LIST, (uint32_t)count, (uint32_t)places);
}

int main(void)
{
    g_mem = calloc(MEM, 1);
    if (!g_mem) return 2;

    changing = 1;
    pick(40, 11);
    CHECK(best_first(40, 11), "12 teams, 1 host: the 11 places do not hold the 11 best of 40");
    CHECK(pick_calls == 1 && rand_calls == 2u * 33u, "12 teams: the original ran %u times with %u random numbers, "
          "want 1 and 66", pick_calls, rand_calls);

    pick(45, 22);
    CHECK(best_first(45, 22), "24 teams, 2 hosts: the 22 places do not hold the 22 best of 45");
    CHECK(pick_calls == 1 && rand_calls == 2u * 45u, "24 teams: %u calls, %u random numbers", pick_calls, rand_calls);

    changing = 0;
    pick(40, 11);
    CHECK(best_first(40, 11), "base game: the 11 places do not hold the 11 best of 40");

    setenv("RECOMP_ASIAN_PICK", "original", 1);
    seed = 1;
    pick(40, 11);
    CHECK(!best_first(40, 11) && pick_calls == 1, "RECOMP_ASIAN_PICK=original: want the original's random pick");
    unsetenv("RECOMP_ASIAN_PICK");

    if (!failures) printf("season_asian_pick: all checks passed\n");
    return failures ? 1 : 0;
}
