/* A match moved for a call-up stays before its competition's season change (O288: the Danish Superliga stalled one
 * season).
 *
 * The season change pass game_season_update_pass 0x5d5ae0 starts the competitions of a league table entry only on
 * the entry's update day (entry +0x15, the day of the year, compared with the game day at 0x5d5bc8; Denmark: day
 * 170), and only a competition that is ready then (vtable +0xc at 0x5d5ccb; a league is ready when every row has
 * played its matches, League::check_complete 0x689ad0, 0x689b3d..0x689b58). A competition that is not ready waits for
 * the update day of the next year, and so runs one season late from then on.
 *
 * NationalTeams::reschedule_clashing_club_fixtures 0x7777e0 calls FIXTURE_MANAGER::reschedule_club_fixtures_near_date
 * 0x5abb10 (0x77795c) for a club whose players are called up. For each fixture of the club near the date it calls
 * remove_fixture 0x5abc30, find_new_fixture_date 0x5ab510 (0x5abbd2), add_fixture 0x5aa680 with flag 1 (0x5abbdc) and
 * queue_fixture_moved_news 0x5aac70 (0x5abbeb). 0x5ab510 (thiscall (fixture) ret 4; year at fixture +0x28, day at
 * +0x2a, slice at +0x3f) looks for a free weekday up to about 20 days after the old date, else takes today + 2, with
 * no look at the season's end. In the base game the leagues end in May, well before their update days. A season game
 * has international windows in June each year (the Nations League and the qualifiers of slices 23, 28 and 29): in a
 * 2021 Select All game FC Copenhagen's last Superliga match of 2022/23 was moved on days 148, 152, 155 and 156 and
 * came after day 170, so the Superliga was not complete on its update day and started 2023/24 a year late (gate (a)
 * run 7; Denmark alone has too few call-ups to show it).
 *
 * The clash code moves matches too (den-stall-2, run 8, 2021 start, 2022/23): a call-up move put Randers FC v FC
 * Copenhagen on day 169 of 2023, the add_fixture that followed (flag 1, 0x5abbdc) found a clash and moved it with its
 * own call of 0x5ab510 (0x5aa96e) to day 171, and the Superliga missed its update day of 170 again. So the rule
 * covers the moves of 0x5ab510 that take a match away from a clash or a call-up, each with the add_fixture (flag 0)
 * and the news that follow it (SITES): the call-up move 0x5abbd2, add_fixture's move of the new fixture 0x5aa96e and
 * of a weaker stored one 0x5aaa25, and resolve_same_day_clash 0x5ae55c, 0x5ae5d8. The other calls (a date today or
 * past at 0x5aa729, an abandoned match 0x6b6084, the cup and manager calls) keep the original.
 *
 * So in a season game, for those calls: when the new date of 0x5ab510 is on or after the competition's
 * next update day, the match goes to the last free day before that update day, after the old date and not before
 * today + 2 (0x5ab929..0x5ab935). A day is free by 0x5ab510's own test (0x5ab9b6..0x5ab9f2): with the slice of
 * get_time_slice 0x5af2f0, find_fixture_clashes 0x5ad780 gives no clash, or the severity 1, or a severity below the
 * match's priority (+0x3a). An international window of the club fails it (severity 0x15, 0x776fc0). When the update
 * day is in the next year, the search goes on into the rest of the old date's year. When there is no free day and the old date is after today, the match keeps its
 * old date and slice: the leg check record that 0x5ab510 added (list 0xae2a48, round variants 3 and 4) is removed,
 * add_fixture inserts it without its clash moves (flag 0 with the try count 0xae2a40 at 10, as add_fixture does
 * after its tenth try), and no "fixture moved" news is queued. The club plays without its called-up players, as a
 * league plays at the end of its season. When the old date is today or past, the new date of 0x5ab510 stays.
 * The other 7 calls of 0x5ab510, the base game and RECOMP_SEASON_CODE=off keep the original (Q152).
 *
 * Test switch RECOMP_TEST_FIXTURE_HOLD (season games only): "log" prints each covered move; "1" also takes the new
 * date of each covered move later in the same year as the update day, so that the free day search and the kept date
 * act on every such move (forced case of the retest). */
#include "win.h"
#include "../rt.h"
#include "../rt_host.h"
#include "season_change.h"
#include "season_names.h"
#include "fixture_season_end.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENTRIES 0x00b63d60u     /* league table entries, 0x48 bytes each (0x5d5bc8) */
#define ENTRY_SIZE 0x48u
#define ENTRY_COUNT 35
#define ENTRY_UPDATE_DAY 0x15u
#define COMP_OBJECTS 0x00adadfcu
#define TODAY 0x00ae2c90u       /* Date: day of the year +0, year +2 */
#define LEG_CHECKS 0x00ae2a48u  /* LLIST: count (word) +0, head +2, tail +6 */
#define ADD_TRIES 0x00ae2a40u   /* add_fixture's try count (word) */
#define FIX_COMP 0x14u          /* the competition's record */
#define FIX_YEAR 0x28u
#define FIX_DAY 0x2au
#define FIX_PRIORITY 0x3au
#define FIX_SLICE 0x3fu

/* The moves of a fixture that the rule covers: the return addresses of the call of 0x5ab510, of the add_fixture that
 * follows it and of the queue_fixture_moved_news after that (0 for none). */
typedef struct {
    uint32_t new_date, add, news;
    const char *what;
} MoveSite;
static const MoveSite SITES[] = {
    {0x005abbd7u, 0x005abbe1u, 0x005abbf0u, "call-up"},              /* 0x5abb10: 0x5abbd2, 0x5abbdc, 0x5abbeb */
    {0x005aa973u, 0x005aa97du, 0, "clash"},                         /* add_fixture moves the new fixture: 0x5aa96e, 0x5aa978 */
    {0x005aaa2au, 0x005aaa38u, 0x005aaa4bu, "clash"},               /* add_fixture moves a weaker stored one: 0x5aaa25, 0x5aaa33, 0x5aaa46 */
    {0x005ae561u, 0x005ae56fu, 0x005ae57eu, "same-day clash"},      /* resolve_same_day_clash: 0x5ae55c, 0x5ae56a, 0x5ae579 */
    {0x005ae5ddu, 0x005ae5ebu, 0x005ae5fau, "same-day clash"},      /* resolve_same_day_clash: 0x5ae5d8, 0x5ae5e6, 0x5ae5f5 */
};
#define SITE_COUNT ((int)(sizeof SITES / sizeof SITES[0]))

#define DATE_NEW 0x00549ef0u            /* Date::Date (day of the year, year), ret 8 */
#define DATE_WEEKDAY 0x0054a3f0u        /* Date::get_weekday, 0 Monday..6 Sunday */
#define TIME_SLICE 0x005af2f0u          /* FIXTURE_MANAGER::get_time_slice (comp, weekday), ret 8 */
#define FIND_CLASHES 0x005ad780u        /* FIXTURE_MANAGER::find_fixture_clashes (fixture, &severity, flag), ret 0xc */
#define LIST_FREE 0x0068d040u           /* LLIST::~LLIST */
#define GAME_DELETE 0x00944c94u         /* operator delete, cdecl */
#define LIST_REMOVE_NODE 0x0068d280u    /* LLIST::remove_node (node), ret 4 */

static uint32_t kept;           /* the fixture that keeps its date, from its 0x5ab510 call to its add (and news) */
static int kept_site;

/* RECOMP_TEST_FIXTURE_HOLD: 0 off, 1 "log", 2 "1" (forced). */
static int test_mode(void)
{
    static int mode = -1;
    if (mode < 0) {
        const char *v = getenv("RECOMP_TEST_FIXTURE_HOLD");
        mode = !v ? 0 : !strcmp(v, "log") ? 1 : !strcmp(v, "1") ? 2 : 0;
    }
    return mode;
}

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

static uint32_t call_this(uint32_t fn, uint32_t self, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, n, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

/* The update day of the entry that holds the competition of record rec, or -1. */
static int update_day(uint32_t rec)
{
    uint32_t table = rd32(g_mem, COMP_OBJECTS);
    if (!readable(rec) || !readable(table)) return -1;
    int32_t id = (int32_t)rd32(g_mem, rec);
    if (id < 0 || id > 0xffff) return -1;
    uint32_t obj = rd32(g_mem, table + 4u * (uint32_t)id);
    if (!readable(obj)) return -1;
    uint32_t comps[128];
    for (int e = 0; e < ENTRY_COUNT; e++) {
        int n = season_change_entry_comps(e, comps, 128);
        for (int i = 0; i < n; i++)
            if (comps[i] == obj) return (int16_t)rd16(g_mem, ENTRIES + ENTRY_SIZE * (uint32_t)e + ENTRY_UPDATE_DAY);
    }
    return -1;
}

static int before(int y1, int d1, int y2, int d2) { return y1 < y2 || (y1 == y2 && d1 < d2); }

static int days_in(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0 ? 366 : 365; }

/* 1 when day d of year y is free for the fixture: it gets that date and the slice of get_time_slice, and
 * find_fixture_clashes finds no clash. The work space (a Date and the severity) lies below the guest stack. */
static int free_day(uint32_t manager, uint32_t fix, int y, int d)
{
    uint32_t esp = cpu.esp;
    cpu.esp -= 0x40u;
    uint32_t date = cpu.esp + 0x20u, sev = cpu.esp + 0x30u;
    uint32_t a[3] = {(uint32_t)d, (uint32_t)y, 0};
    call_this(DATE_NEW, date, 2, a);
    int weekday = rt_unwinding ? 0 : (int)call_this(DATE_WEEKDAY, date, 0, a);
    int ok = 0;
    if (!rt_unwinding) {
        wr16(g_mem, fix + FIX_YEAR, (uint16_t)y);
        wr16(g_mem, fix + FIX_DAY, (uint16_t)d);
        a[0] = rd32(g_mem, fix + FIX_COMP);
        a[1] = (uint32_t)(int16_t)weekday;
        g_mem[fix + FIX_SLICE] = (uint8_t)call_this(TIME_SLICE, manager, 2, a);
    }
    if (!rt_unwinding) {
        wr32(g_mem, sev, 0);
        a[0] = fix;
        a[1] = sev;
        a[2] = 0;
        uint32_t list = call_this(FIND_CLASHES, manager, 3, a);
        int severity = (int32_t)rd32(g_mem, sev);
        ok = (!list && !severity) || severity == 1 || (int8_t)g_mem[fix + FIX_PRIORITY] > severity;
        if (list && !rt_unwinding) {
            call_this(LIST_FREE, list, 0, a);
            a[0] = list;
            if (!rt_unwinding) rt_callback(GAME_DELETE, 1, a);
        }
    }
    if (!rt_unwinding) cpu.esp = esp;
    return ok;
}

void o_005ab510(void);

RT_WRAP(005ab510, fixture_new_date)
{
    uint32_t fix = rt_arg(0), manager = cpu.ecx, ret = rd32(g_mem, cpu.esp);
    int site = 0;
    while (site < SITE_COUNT && SITES[site].new_date != ret) site++;
    if (site == SITE_COUNT || !season_game_changes() || !readable(fix)) {
        o_005ab510();
        return;
    }
    kept = 0;               /* a kept fixture's add and news come before the next covered move (none nests there) */
    int year = (int16_t)rd16(g_mem, fix + FIX_YEAR), day = (int16_t)rd16(g_mem, fix + FIX_DAY);
    uint8_t slice = g_mem[fix + FIX_SLICE];
    uint16_t legs = rd16(g_mem, LEG_CHECKS);
    o_005ab510();
    if (rt_unwinding) return;
    uint32_t rec = rd32(g_mem, fix + FIX_COMP);
    int last = update_day(rec);
    int new_year = (int16_t)rd16(g_mem, fix + FIX_YEAR), new_day = (int16_t)rd16(g_mem, fix + FIX_DAY);
    if (test_mode())
        fprintf(stderr, "recomp: fixtures: test: a %s move of %.40s from day %d of %d to day %d of %d (update "
                        "day %d)\n", SITES[site].what, win_str(rec + 4), day, year, new_day, new_year, last);
    if (test_mode() == 2 && new_year == year && new_day > day) last = new_day;
    if (last < 0) return;
    int last_year = day < last ? year : year + 1;
    uint8_t new_slice = g_mem[fix + FIX_SLICE];
    if (before(new_year, new_day, last_year, last)) return;
    int today_year = (int16_t)rd16(g_mem, TODAY + 2u), today = (int16_t)rd16(g_mem, TODAY);
    const char *name = win_str(rec + 4);
    /* The last free day before the update day, after the old date and not before today + 2 (0x5ab92d). */
    int min_year = today_year, min_day = today + 2;
    if (min_day >= days_in(min_year)) min_day -= days_in(min_year), min_year++;
    for (int y = last_year, d = last - 1;; d--) {
        if (d < 0) y--, d = days_in(y) - 1;
        if (!before(year, day, y, d) || before(y, d, min_year, min_day)) break;
        if (free_day(manager, fix, y, d)) {
            fprintf(stderr, "recomp: fixtures: a match of %.40s moves from day %d of %d to day %d of %d, before its "
                            "season change on day %d (the %s move gave day %d of %d)\n",
                    name, day, year, d, y, last, SITES[site].what, new_day, new_year);
            return;
        }
        if (rt_unwinding) return;
    }
    if (!before(today_year, today, year, day)) {
        wr16(g_mem, fix + FIX_YEAR, (uint16_t)new_year);
        wr16(g_mem, fix + FIX_DAY, (uint16_t)new_day);
        g_mem[fix + FIX_SLICE] = new_slice;
        fprintf(stderr, "recomp: fixtures: a match of %.40s goes to day %d of %d after its season change on day %d "
                        "(no free day before it, the old day %d of %d is not in the future)\n",
                name, new_day, new_year, last, day, year);
        return;
    }
    wr16(g_mem, fix + FIX_YEAR, (uint16_t)year);
    wr16(g_mem, fix + FIX_DAY, (uint16_t)day);
    g_mem[fix + FIX_SLICE] = slice;
    if (rd16(g_mem, LEG_CHECKS) == (uint16_t)(legs + 1u)) {
        uint32_t a[1] = {rd32(g_mem, LEG_CHECKS + 6u)};
        call_this(LIST_REMOVE_NODE, LEG_CHECKS, 1, a);
        if (rt_unwinding) return;
    }
    kept = fix;
    kept_site = site;
    fprintf(stderr, "recomp: fixtures: a match of %.40s stays on day %d of %d (no free day before its season change "
                    "on day %d; the %s move gave day %d of %d)\n",
            name, day, year, last, SITES[site].what, new_day, new_year);
}

/* add_fixture 0x5aa680 (its wrap is in season_kind_cup_breaks.c): the call at 0x5abbdc for a kept fixture inserts
 * it on its old date with flag 0 and the try count at 10, so the clash code does not move it again. */
void o_005aa680(void);

void fixture_season_end_add_fixture(void)
{
    if (!kept || rt_arg(0) != kept || rd32(g_mem, cpu.esp) != SITES[kept_site].add) {
        o_005aa680();
        return;
    }
    uint16_t tries = rd16(g_mem, ADD_TRIES);
    if (!SITES[kept_site].news) kept = 0;       /* no news call follows */
    wr32(g_mem, cpu.esp + 8u, 0);
    wr16(g_mem, ADD_TRIES, 10);
    o_005aa680();
    if (!rt_unwinding) wr16(g_mem, ADD_TRIES, tries);
}

/* queue_fixture_moved_news 0x5aac70 (thiscall (moved, cause, old date) ret 0xc): no news for a kept fixture. */
void o_005aac70(void);

RT_WRAP(005aac70, fixture_moved_news)
{
    if (kept && rt_arg(0) == kept && rd32(g_mem, cpu.esp) == SITES[kept_site].news) {
        kept = 0;
        rt_ret_stdcall(0, 3);
        return;
    }
    o_005aac70();
}
