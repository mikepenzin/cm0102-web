/* The snap of a qualifier to a FIFA day (season_friendly_dates.c, season_fifa_snap_fixture), open item 79. ctest
 * season_fifa_snap.
 *
 * In run set pass 3 (game a2025s3, leg 2) every UEFA group of the 2046 World Cup qualifying had matchday 5 on day 316
 * of 2045 and matchday 6 on day 319. The snap moved matchday 5 to the FIFA day 319 (the window of 16 to 21 November
 * 2045). Matchday 6 was already on that FIFA day, and the test "on a free FIFA day already" looked only at the days
 * either side of it, so matchday 6 stayed there too: each side then had two matches on day 319. The fixture store
 * (add_fixture 0x5aa680, find_fixture_clashes 0x5ad780, resolve_same_day_clash 0x5ae380) moved matchday 6 to
 * Wednesday 22 November, off the window and the day before the Europa League's Thursday. KF Egnatia had 19 of its
 * registered players with Albania (get_international_duty_club 0x7757d0 holds a player to the day after his side's
 * match) and fielded 9: match_eng.cpp 612.
 *
 * The test builds a small world in game memory: four national sides, England v Luxembourg stored on day 319 of 2045,
 * and the FIFA days of November 2045. England v Albania made for day 319 must move to another FIFA day of the
 * window, 3 or more days from 319; made for day 322 with nothing near, it keeps 322. */
#include "win.h"
#include "season.h"
#include "season_uefa_days.h"
#include "season_names.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEM 0x01000000u
#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define NATION_COUNT 0xae2350u
#define GAME_DATE 0xae2c90u
#define TYPE_COUNT 0xb64738u
#define CLUB_SIZE 0x245u
#define F_DAY_LIST 0x005ae6a0u
#define F_FINALS 0x0076d940u
#define CREATE_CALL 0x00683a5au           /* the return address of League::create_fixtures' add_fixture call */

Cpu cpu;
uint8_t *g_mem;
int rt_unwinding;
void o_005cc510(void) {}
static int tv_moves;                               /* calls of the original TV move */
void o_00685640(void) { tv_moves++; cpu.esp += 8; }
int season_start_year(void) { return 2025; }
int season_game_changes(void) { return 1; }
int season_changing_rules(void) { return 1; }
int season_intl_finals_busy(int conf, int y, int doy) { (void)conf, (void)y, (void)doy; return 0; }
int season_gold_cup_busy(int y, int doy) { (void)y, (void)doy; return 0; }
long season_abs_day(int y, int m, int d) { return (long)y * 372 + m * 31 + d; }
int season_conmebol_real_day(long z) { (void)z; return 0; }
void season_from_abs_day(long z, int *y, int *m, int *d)
{
    *y = (int)(z / 372), *m = (int)(z % 372 / 31), *d = (int)(z % 31);
}

static const int month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};   /* 2045, not a leap year */

int season_day_of_year(int y, int m, int d)
{
    (void)y;
    int doy = d - 1;
    for (int k = 0; k < m - 1; k++) doy += month_days[k];
    return doy;
}

void season_from_day_of_year(int y, int doy, int *m, int *d)
{
    (void)y;
    int k = 0;
    while (k < 11 && doy >= month_days[k]) doy -= month_days[k++];
    *m = k + 1, *d = doy + 1;
}

/* The FIFA days of 2045 that matter here: the November window, Thursday 16 to Tuesday 21 (days 319 to 324), as
 * season_fifa_days_table.c projects the window of 2030. */
int season_fifa_day(int y, int m, int want)
{
    if (y != 2045 || m != 11) return 0;
    int best = 0;
    for (int d = 16; d <= 21; d++)
        if (!best || abs(d - want) < abs(best - want)) best = d;
    return best;
}

/* The day lists: one list (slice 0, type 0) per day that holds stored fixtures. A list: +2 its first node; a node:
 * +0 the fixture, +6 the next node. */
#define LISTS 0x00e80000u
static int list_day[8], list_n;
static uint32_t list_at[8];

static void store(uint32_t fix, int doy)
{
    int i = 0;
    while (i < list_n && list_day[i] != doy) i++;
    if (i == list_n) {
        list_day[i] = doy, list_at[i] = LISTS + 0x1000u * (uint32_t)i, list_n++;
        wr32(g_mem, list_at[i] + 2, 0);
    }
    uint32_t node = list_at[i] + 0x100u, last = list_at[i] + 2;
    while (rd32(g_mem, last)) last = rd32(g_mem, last) + 6, node += 0x10;
    wr32(g_mem, node, fix);
    wr32(g_mem, node + 6, 0);
    wr32(g_mem, last, node);
}

static int finals;                                 /* what national_teams_is_finals_comp answers */

uint32_t rt_callback(uint32_t target, int nargs, const uint32_t *args)
{
    (void)nargs;
    if (target == F_FINALS) return rd32(g_mem, args[0]) == 900 ? (uint32_t)finals : 0xdead;
    if (target != F_DAY_LIST || args[1] || args[2]) return 0;
    int doy = (int16_t)rd16(g_mem, args[0]), y = (int16_t)rd16(g_mem, args[0] + 2);
    for (int i = 0; i < list_n && y == 2045; i++)
        if (list_day[i] == doy) return list_at[i];
    return 0;
}

void season_fifa_snap_fixture(uint32_t fix, uint32_t ret);
void f_00685640(void);

static int failures;

static void check(int ok, const char *text)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", text);
    if (!ok) failures++;
}

static uint32_t side(int i) { return 0x00c00000u + CLUB_SIZE * (uint32_t)i; }

static uint32_t fixture(uint32_t at, int home, int away, int doy)
{
    memset(g_mem + at, 0, 0x50);
    wr32(g_mem, at + 0x14, 0x00e00000u);              /* the competition record */
    wr32(g_mem, at + 0x1c, side(home));
    wr32(g_mem, at + 0x20, side(away));
    wr16(g_mem, at + 0x28, 2045);
    wr16(g_mem, at + 0x2a, (uint16_t)doy);
    g_mem[at + 0x3a] = 10;                             /* a qualifier's priority */
    return at;
}

static void world(void)
{
    memset(g_mem, 0, MEM);
    list_n = 0;
    wr32(g_mem, CLUBS, side(0));
    wr32(g_mem, CLUB_COUNT, 4);
    wr32(g_mem, NATION_COUNT, 2);                      /* all four clubs are national sides (count - 2 x nations) */
    for (int i = 0; i < 4; i++) {
        uint32_t nation = 0x00d00000u + 0x200u * (uint32_t)i;
        wr32(g_mem, side(i) + 0x53, nation);
        wr32(g_mem, nation + 0x71, 0x00d80000u);          /* Europe */
    }
    static const char *names[] = {"England", "Albania", "Luxembourg", "Turkiye"};
    for (int i = 0; i < 4; i++) strcpy((char *)g_mem + side(i) + 4, names[i]);
    strcpy((char *)g_mem + 0x00e00004u, "FIFA World Cup qualification (UEFA)");
    wr32(g_mem, 0x00e00000u, 900);
    wr32(g_mem, 0x00ae2378u, 2000);                    /* the competition count: ids from 1873 are friendlies (0x5c6710) */
    wr16(g_mem, GAME_DATE, 340);                       /* 7 December 2044: the groups are made */
    wr16(g_mem, GAME_DATE + 2, 2044);
    g_mem[TYPE_COUNT] = 2;
    cpu.esp = 0x00f00000u;
}

int main(void)
{
    g_mem = calloc(MEM, 1);
    if (!g_mem) return 2;
    char text[200];

    /* Matchday 5 already moved to the FIFA day 319; matchday 6 is made for the same day. */
    world();
    store(fixture(0x00e10000u, 0, 2, 319), 319);       /* England v Luxembourg */
    uint32_t f = fixture(0x00e20000u, 0, 1, 319);      /* England v Albania */
    season_fifa_snap_fixture(f, CREATE_CALL);
    int d = (int16_t)rd16(g_mem, f + 0x2a);
    snprintf(text, sizeof text, "England v Albania made for day 319, where England v Luxembourg is stored: day %d "
                                "(a FIFA day of the window, 3 days or more from 319)", d);
    check(d >= 322 && d <= 324, text);

    /* Nothing near: a qualifier on a free FIFA day keeps it. */
    world();
    store(fixture(0x00e10000u, 0, 2, 319), 319);
    f = fixture(0x00e20000u, 0, 1, 322);
    season_fifa_snap_fixture(f, CREATE_CALL);
    d = (int16_t)rd16(g_mem, f + 0x2a);
    snprintf(text, sizeof text, "England v Albania made for the free FIFA day 322: day %d (kept)", d);
    check(d == 322, text);

    /* Another pair on the same day is no clash: Albania v Turkiye made for day 319 beside England v Luxembourg. */
    world();
    store(fixture(0x00e10000u, 0, 2, 319), 319);
    f = fixture(0x00e20000u, 1, 3, 319);
    season_fifa_snap_fixture(f, CREATE_CALL);
    d = (int16_t)rd16(g_mem, f + 0x2a);
    snprintf(text, sizeof text, "Albania v Turkiye made for day 319 beside England v Luxembourg: day %d (kept)", d);
    check(d == 319, text);

    /* Item 205: League::move_fixtures_for_tv 0x685640 moves no fixture of a league of national sides that is not a
     * finals competition (the OFC groups of 2033 lost their April days to it). The league: the competition record
     * 0x00e00000 (id 900) at +4, a host stadium id at +0x24 (not read), and a table (+0xb1, count +0x3e) of the
     * four sides. A finals competition, a table with a club side, or the start season keeps the original's move. */
    static const struct { int nations, finals, year, moves; const char *what; } tv[] = {
        {2, 0, 2044, 0, "a qualifying group of four national sides: no TV move"},
        {2, 1, 2044, 1, "a finals competition of four national sides: the TV move"},
        {1, 0, 2044, 1, "a table with club sides (only the last two clubs are national sides): the TV move"},
        {2, 0, 2024, 1, "a qualifying group in a season before the start (keeps_original): the TV move"},
    };
    for (unsigned k = 0; k < sizeof tv / sizeof tv[0]; k++) {
        world();
        wr32(g_mem, NATION_COUNT, (uint32_t)tv[k].nations);
        wr16(g_mem, GAME_DATE + 2, (uint16_t)tv[k].year);
        finals = tv[k].finals;
        uint32_t league = 0x00e30000u, table = 0x00e31000u;
        wr32(g_mem, league + 4, 0x00e00000u);
        wr32(g_mem, league + 0x24, 7);
        wr16(g_mem, league + 0x3e, 4);
        wr32(g_mem, league + 0xb1, table);
        for (int i = 0; i < 4; i++) wr32(g_mem, table + 0x3bu * (uint32_t)i, side(i));
        tv_moves = 0;
        cpu.ecx = league;
        cpu.esp -= 8;                                  /* the return address and the Date * */
        uint32_t esp = cpu.esp;
        f_00685640();
        snprintf(text, sizeof text, "%s (%d calls of the original, esp +%d)", tv[k].what, tv_moves,
                 (int)(cpu.esp - esp));
        check(tv_moves == tv[k].moves && cpu.esp == esp + 8, text);
    }

    free(g_mem);
    printf("%s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
