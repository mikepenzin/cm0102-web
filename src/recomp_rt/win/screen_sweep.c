/* The screen sweep (0.6.0 slice 30a, check 4 of docs/research/brief-0.6.0/slice-30a.md). Test runs only: without
 * RECOMP_SCREEN_SWEEP=1 every function here returns at once and changes nothing.
 *
 * With RECOMP_SCREEN_SWEEP=1 the human turn (testrun.c) opens one competition screen a turn, as a click on a
 * competition name does (the news item click opens comp_screens_open_comp_screen 0x48f4a0 with (record, -2, -2,
 * 0), docs/research/2026-10-14-clickable-names-inventory.md), so the main loop builds and draws it like any screen
 * of the original. Each competition object (season_change_comps) gets five screens:
 *   - views 1 to 4 of the competition screen. 0x48f4a0 pushes the screen with build 0x48fcd0, input 0x4965d0 and
 *     leave 0x4985e0 (ScreenManager::open_screen 0x7fe600 at 0x48f500), then sets slot 0 to the record and slot 1
 *     to the view 1 (ScreenManager::set_screen_param 0x7ff270 at 0x48f517, 0x48f527). The sweep sets slot 1 again.
 *     The build draws the tab of the view through the jump table at 0x49134c (0x49072d..0x490737): view 1 the
 *     stages tab (league table 0x491380 for a League_Stage, else the cup rounds 0x492980, 0x490797..0x4907a8),
 *     2 the results, 3 the fixtures 0x493ee0, 4 the schedule 0x4947a0;
 *   - the history screen, comp_screens_open_history_screen 0x498780 (record, 0), built by 0x498a30.
 * The screen of a record is only opened when [0xadadfc][record id] is the object, as the news click tests
 * (0x764470..0x7644dc). The objects of record_utils (ReserveComp, BIntComp, Under21IntComp and the others) are left
 * out: they hold the records of reserve and international squads, have no stage, and Comp::get_stage 0x48d200
 * stops at "comp.cpp 878" (0x48d289) for them. The game never opens their screen. A pass starts at the first human
 * turn and at the first turn of each September, and takes one screen a turn, so a season's pass is spread over its
 * first months. The log names each screen
 * ("recomp: screen sweep: ... view n"), writes "recomp: screen sweep: built" at its first build and counts the
 * builds, so a run can tell that each screen was drawn. A box of the game while a screen is up is logged after
 * that screen's line (waits.c). */
#include "win.h"
#include "season_change.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPEN_COMP 0x0048f4a0u       /* cdecl (record, int, int, keep) */
#define OPEN_HISTORY 0x00498780u    /* cdecl (record, keep) */
#define SCREENS 0x00b74450u         /* the screen stack */
#define SET_SLOT 0x007ff270u        /* thiscall on SCREENS (slot, value, copy) */
#define COMP_OBJECTS 0x00adadfcu    /* the object of each competition record id */
#define COMP_SLOTS 0x00adadf8u
#define RESET 0x0055f7c0u          /* thiscall on a screen manager (1): before a build */
#define DRAW 0x0055f300u           /* thiscall on a screen manager (-1, -1, -1, -1): the screen draw */
#define CHANNEL 0x382au            /* S + this: s16, the channel of the turn */
#define RECORD_SIZE 0x300u         /* S + channel x this: its record */
#define REC_PLAYER 0x7d4u          /* s16: the player; 0 is the local one */
#define REC_NODE 0x7e2u            /* the current node; its first dword is the build function */
#define REC_DISPLAY 0x7fcu         /* the screen manager (Display) of the record */
#define NO_DRAW_FLAG 0x12f53du     /* Display + this: 1 while a build runs */
#define BUILD_TIME 0x12f4fau
#define VIEWS 5                     /* views 1 to 4 and the history screen */
#define MAX_OBJECTS 1024

static int on = -1;
static uint32_t objects[MAX_OBJECTS];
static int count, next_item = -1, pass_season = -1, pushed, built_comp, built_history;
static int waiting;                 /* 1: a competition screen, 2: a history screen, pushed and not built yet */

static int readable(uint32_t p) { return rt_readable(p); }

static uint32_t call(uint32_t fn, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, n, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t call_this(uint32_t fn, uint32_t self, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, n, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

/* Builds and draws the current screen now, as one pass of ScreenManager::monitor 0x7fc7e0 does for a screen to
 * rebuild (0x7fc985..0x7fca47): the reset 0x55f7c0(1) on the record's screen manager, +0x12f53d set while the
 * build function of the current node runs, then +0x12f53d and +0x12f4fa cleared and, for the local player, the
 * draw 0x55f300(-1, -1, -1, -1). In a season jump the day loop puts its own screen up before the next pass of the
 * loop, so a pushed screen is built here. */
static void build_now(void)
{
    int ch = (int16_t)rd16(g_mem, SCREENS + CHANNEL);
    uint32_t rec = SCREENS + RECORD_SIZE * (uint32_t)ch;
    uint32_t disp = rd32(g_mem, rec + REC_DISPLAY), node = rd32(g_mem, rec + REC_NODE);
    if (ch < 0 || ch > 15 || !readable(disp) || !readable(node)) return;
    uint32_t build = rd32(g_mem, node), one[1] = {1};
    if (!readable(build)) return;
    call_this(RESET, disp, 1, one);
    if (rt_unwinding) return;
    wr32(g_mem, disp + NO_DRAW_FLAG, 1);
    call(build, 0, NULL);
    if (rt_unwinding) return;
    wr32(g_mem, disp + NO_DRAW_FLAG, 0);
    wr32(g_mem, disp + BUILD_TIME, 0);
    if ((int16_t)rd16(g_mem, rec + REC_PLAYER) == 0) {
        uint32_t m[4] = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
        call_this(DRAW, disp, 4, m);
    }
}

static int sweep_on(void)
{
    if (on < 0) {
        const char *v = getenv("RECOMP_SCREEN_SWEEP");
        on = v && *v == '1';
    }
    return on;
}

/* 1 when the screens of the object can open: its record's slot of [0xadadfc] holds it. */
static int openable(uint32_t obj)
{
    uint32_t rec = readable(obj) ? rd32(g_mem, obj + 4) : 0, table = rd32(g_mem, COMP_OBJECTS);
    if (!readable(rec) || !readable(table)) return 0;
    int32_t id = (int32_t)rd32(g_mem, rec), slots = (int32_t)rd32(g_mem, COMP_SLOTS);
    return id >= 0 && id < slots && rd32(g_mem, table + 4u * (uint32_t)id) == obj;
}

static void start_pass(int season)
{
    uint32_t all[MAX_OBJECTS];
    int n = season_change_comps(all, MAX_OBJECTS);
    count = 0;
    for (int i = 0; i < n; i++)
        if (openable(all[i]) && strcmp(season_change_file(all[i]), "record_utils")) objects[count++] = all[i];
    next_item = 0;
    pass_season = season;
    pushed = built_comp = built_history = 0;
    fprintf(stderr, "recomp: screen sweep: %d/%02d pass of %d competitions (%d objects)\n", season,
            (season + 1) % 100, count, n);
}

void screen_sweep_turn(void)
{
    if (!sweep_on()) return;
    int y = 0, m = 0, d = 0;
    if (!win_game_date(&y, &m, &d)) return;
    int season = m >= 7 ? y : y - 1;
    if (next_item < 0 && (pass_season < 0 || (season != pass_season && m >= 9))) start_pass(season);
    if (next_item < 0) return;
    if (next_item >= count * VIEWS) {
        fprintf(stderr, "recomp: screen sweep: %d/%02d done: %d screens, %d competition screen builds, %d history "
                        "builds\n", pass_season, (pass_season + 1) % 100, pushed, built_comp, built_history);
        next_item = -1;
        return;
    }
    uint32_t obj = objects[next_item / VIEWS];
    int view = next_item % VIEWS + 1;
    next_item++;
    if (!openable(obj)) return;                 /* gone since the pass started */
    uint32_t rec = rd32(g_mem, obj + 4);
    fprintf(stderr, "recomp: screen sweep: %d/%02d on %02d.%02d.%04d: %s (%.51s) view %d\n", pass_season,
            (pass_season + 1) % 100, d, m, y, season_change_file(obj), win_str(rec + 4), view);
    pushed++;
    waiting = view <= 4 ? 1 : 2;
    if (view <= 4) {
        uint32_t a[4] = {rec, 0xfffffffeu, 0xfffffffeu, 0};
        call(OPEN_COMP, 4, a);
        if (rt_unwinding) return;
        if (view > 1) {
            uint32_t s[3] = {1, (uint32_t)view, 0};
            call_this(SET_SLOT, SCREENS, 3, s);
            if (rt_unwinding) return;
        }
    } else {
        uint32_t a[2] = {rec, 0};
        call(OPEN_HISTORY, 2, a);
        if (rt_unwinding) return;
    }
    build_now();
}

void o_0048fcd0(void);
void o_00498a30(void);

/* The builds of the two screens: counted while the sweep runs, else the original as it is. */
static void built(int kind)
{
    if (!sweep_on() || next_item < 0) return;
    if (kind == 1) built_comp++;
    else built_history++;
    if (waiting == kind) {                      /* the first build of the screen pushed last */
        fprintf(stderr, "recomp: screen sweep: built\n");
        waiting = 0;
    }
}

RT_WRAP(0048fcd0, sweep_comp_build)
{
    built(1);
    o_0048fcd0();
}

RT_WRAP(00498a30, sweep_history_build)
{
    built(2);
    o_00498a30();
}
