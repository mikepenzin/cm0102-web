/* The Asian Cup of 16 teams (0.6.2 open item 44, item 180; src/recomp_rt/win/season_intl_cups.c). The 2004 AFC
 * Asian Cup in China had 16 teams (17 July to 7 August 2004, https://en.wikipedia.org/wiki/2004_AFC_Asian_Cup).
 * A game that starts in 2001/02 builds that edition at the new game: the ctor 0x40e860 keeps the original's array
 * of 3 stages there (Q152), and the entrants wrap of 0x40fdf0 gives it 4 at its draw, so the sites give 16 teams
 * and 4 groups. The checks:
 * - the ctor sites (0x40e90a, 0x40e923) keep 3 stages for 2004 in a 2001 game and give 4 for 2007;
 * - at the draw the 2004 edition (3 stages, as made by the ctor or held by an undrawn edition of a save of 0.6.1
 *   or older) gets 4 stages before the original runs, which then sees 16 teams and 4 groups;
 * - an edition with a built stage (+0x30 0 or more) keeps its array;
 * - the base game (changing rules off) keeps 3 stages and 12 teams.
 * ctest season_asian16. */
#include "win.h"
#include "season.h"
#include "season_intl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The season state of the test (season.c in the game). */
static int changing = 1;
int season_code_on(void) { return 1; }
int season_start_year(void) { return SEASON_FIRST_YEAR; }
int season_changing_rules(void) { return changing; }
int season_change_is_creating(void) { return 0; }

/* The game side: a heap in g_mem for malloc 0x944e46 and free 0x9452ca, nothing else is called. */
Cpu cpu;
uint8_t *g_mem;
int rt_unwinding;
#define MEM 0x200000u
#define HEAP 0x100000u
static uint32_t heap = HEAP, freed;
uint32_t rt_callback(uint32_t target, int nargs, const uint32_t *args)
{
    if (target == 0x00944e46u && nargs == 1) {
        uint32_t p = heap;
        heap += (args[0] + 15u) & ~15u;
        return p;
    }
    if (target == 0x009452cau && nargs == 1) {
        freed = args[0];
        return 0;
    }
    fprintf(stderr, "unexpected call of 0x%x\n", target);
    exit(2);
}
/* The date and host functions of the knockout and hosts code: not reached by these checks. */
long season_abs_day(int y, int m, int d) { (void)y, (void)m, (void)d; return 0; }
void season_from_abs_day(long z, int *y, int *m, int *d) { (void)z; *y = 2004, *m = 1, *d = 1; }
int season_day_of_year(int y, int m, int d) { (void)y, (void)m; return d; }
void season_weekday_date(int y, int m, int d, int wd, int *oy, int *om, int *od) { (void)wd; *oy = y, *om = m, *od = d; }
void season_intl_shift_more(int days) { (void)days; }
int season_intl_shift_now(void) { return 0; }
int season_intl_host_ids(uint32_t key, int year, int ids[6]) { (void)key, (void)year, (void)ids; return 0; }
int season_intl_confed_june(void) { return 1; }    /* original-faults group confed acts (items 157 to 159) */

/* The original functions that season_intl_cups.c wraps. o_0040fdf0 notes what its sites give. */
static uint32_t seen_teams, seen_groups, seen_stages, entrants_calls;
void o_0040fdf0(void)
{
    entrants_calls++;
    seen_stages = rd32(g_mem, cpu.ecx + 0x2cu);
    seen_teams = rt_intl_cup_teams(cpu.ecx, 0xcu);
    seen_groups = rt_intl_cup_groups(cpu.ecx, 3u);
}
void o_0040f8a0(void) {}
void o_004027d0(void) {}
void o_004103a0(void) {}
void o_004029c0(void) {}
void o_00410560(void) {}
void o_00402090(void) {}
void o_0040fa90(void) {}
/* Item 181 wraps the ACN draw 0x4022c0 in season_intl_cups.c too; these stubs let this test link with or
 * without that branch. */
void o_004022c0(void) {}
void season_intl_draw_hosts(const char *file, int year);
void season_intl_draw_hosts(const char *file, int year) { (void)file, (void)year; }

/* Up to item 180 the runtime had no wrap of 0x40fdf0: this stands in, so the test builds and fails on that code. */
__attribute__((weak)) void f_0040fdf0(void) { o_0040fdf0(); }

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

#define SELF 0x10000u
#define ASIA_VTABLE 0x967614u

/* An asia_nations object of the year as the ctor leaves it (0x40e8af, 0x40e8b5, 0x40e915, 0x40e91c, 0x40e923). */
static void make_cup(int year, uint32_t stages)
{
    memset(g_mem + SELF, 0, 0x100);
    wr32(g_mem, SELF, ASIA_VTABLE);
    wr16(g_mem, SELF + 0x40u, (uint32_t)year);
    wr32(g_mem, SELF + 0x45u, 0);
    wr32(g_mem, SELF + 0x30u, 0xffffffffu);
    wr32(g_mem, SELF + 0x2cu, stages);
    uint32_t a[1] = {4u * stages};
    wr32(g_mem, SELF + 0xcu, rt_callback(0x00944e46u, 1, a));
}

static void draw(void)
{
    cpu.ecx = SELF;
    entrants_calls = seen_teams = seen_groups = seen_stages = 0;
    f_0040fdf0();
}

int main(void)
{
    g_mem = calloc(MEM, 1);
    if (!g_mem) return 2;

    /* A 2001 game with changing rules on: the new game keeps the original's 3 stages for 2004 (Q152). */
    changing = 1;
    make_cup(2004, 3);
    CHECK(rt_intl_cup_ko_ctor(SELF, 2u) + 1u == 3u, "2004 at the new game: %u stages, want 3",
          rt_intl_cup_ko_ctor(SELF, 2u) + 1u);
    CHECK(rt_intl_cup_teams(SELF, 0xcu) == 0xcu, "2004 before the draw: %u teams, want 12",
          rt_intl_cup_teams(SELF, 0xcu));
    draw();
    CHECK(entrants_calls == 1, "2004: the original ran %u times", entrants_calls);
    CHECK(seen_stages == 4u, "2004 at the draw: %u stages, want 4", seen_stages);
    CHECK(seen_teams == 16u, "2004 at the draw: %u teams, want 16", seen_teams);
    CHECK(seen_groups == 4u, "2004 at the draw: %u groups, want 4", seen_groups);
    CHECK(rt_intl_cup_ko(SELF, 2u) == 3u, "2004 after the draw: knockout at %u, want 3", rt_intl_cup_ko(SELF, 2u));

    /* 2007: the ctor makes 4 stages, the draw keeps them. */
    make_cup(2007, 3);
    CHECK(rt_intl_cup_ko_ctor(SELF, 2u) + 1u == 4u, "2007 ctor: %u stages, want 4", rt_intl_cup_ko_ctor(SELF, 2u) + 1u);
    make_cup(2007, 4);
    uint32_t arr = rd32(g_mem, SELF + 0xcu);
    draw();
    CHECK(seen_teams == 16u && rd32(g_mem, SELF + 0xcu) == arr, "2007 at the draw: %u teams, array %s", seen_teams,
          rd32(g_mem, SELF + 0xcu) == arr ? "kept" : "new");

    /* An edition with a built stage keeps its array. */
    make_cup(2004, 3);
    wr32(g_mem, SELF + 0x30u, 0);
    draw();
    CHECK(seen_stages == 3u, "2004 with stage 0 built: %u stages, want 3", seen_stages);

    /* The base game: changing rules off in a 2001 game keeps 3 stages and 12 teams. */
    changing = 0;
    make_cup(2004, 3);
    draw();
    CHECK(seen_stages == 3u && seen_teams == 12u && seen_groups == 3u,
          "base game 2004: %u stages, %u teams, %u groups, want 3, 12, 3", seen_stages, seen_teams, seen_groups);

    if (!failures) printf("season_asian16: all checks passed\n");
    return failures ? 1 : 0;
}
