/* The real round names in the game (season_round_names.h, Q370). Evidence: docs/research/2026-10-17-uefa-formats.md,
 * section "Round names".
 *
 * The round name function 0x4b4850, cdecl (buffer, size, id, flag), writes the name of a round code (or of a
 * stage id) into the caller's buffer and returns it: flag 0 the long set (0x4b48d2), flag 3 the plural set
 * (0x4b6937), any other flag the abbreviated set (0x4b7a84). A code + 1 to + 5 names a replay, a leg or a
 * decider. Its 23 calls only put the text on a screen, into a news text or into a printout (the list is in
 * the doc). The name function knows no competition, so the functions below that know theirs set it for their
 * calls. A call with no competition known, a set or season with no row, or a part the runtime does not name
 * gets the original's text.
 *
 * The text goes into the caller's buffer with its size, as the original's (no buffer of the runtime). The base
 * game (start 2001, changing rules off) and RECOMP_SEASON_CODE=off get the original's text. */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_round_names.h"
#include "season_uefa.h"

#include <stdio.h>
#include <string.h>

#define COMP_TABLE 0xadadfcu
#define COMP_SLOTS 0xadadf8u

static uint32_t context;                /* the competition object of the call that runs, or 0 */

static int readable(uint32_t p) { return rt_readable(p); }

/* The competition object of a competition object, stage object or record (club_comp: +0 the id, the slot of
 * g_comp_object_table [0xadadfc]; an object has its vtable at +0 and its record at +4). */
static uint32_t comp_of(uint32_t p)
{
    if (!readable(p)) return 0;
    uint32_t rec = rd32(g_mem, p) >= 0x400000u ? rd32(g_mem, p + 4) : p;
    if (!readable(rec)) return 0;
    int32_t id = (int32_t)rd32(g_mem, rec), slots = (int32_t)rd32(g_mem, COMP_SLOTS);
    uint32_t table = rd32(g_mem, COMP_TABLE);
    if (id < 0 || id >= slots || !readable(table)) return 0;
    uint32_t obj = rd32(g_mem, table + 4u * (uint32_t)id);
    return readable(obj) && rd32(g_mem, obj + 4) == rec ? obj : 0;
}

/* The season of the rules of a competition: its own season with changing rules on, the start year with changing
 * rules off (as the formats, season_uefa_row). 0 in the base game and with the season code off. */
static int rules_season(uint32_t obj)
{
    if (!season_code_on()) return 0;
    if (!season_changing_rules()) return season_start_year() == SEASON_FIRST_YEAR ? 0 : season_start_year();
    return (int16_t)rd16(g_mem, obj + 0x40);
}

void o_004b4850(void);

/* The round name function. */
RT_WRAP(004b4850, round_name)
{
    uint32_t buf = rt_arg(0), size = rt_arg(1);
    int code = (int16_t)rt_arg(2), flag = (int8_t)rt_arg(3);
    int set = flag == 0 ? SEASON_ROUND_NAME_LONG : flag == 3 ? SEASON_ROUND_NAME_PLURAL : SEASON_ROUND_NAME_ABBREVIATED;
    int part = code % 10, season = context ? rules_season(context) : 0;
    /* The words of a replay, a leg or a decider, as the original's names write them: "Quarter Final First
     * Leg" (0x4b48ff on), "Quarter Final First Legs" (plural), "Qtr Final Leg 1", "2nd Rnd Decider"
     * (abbreviated). Part 5 (decider) exists only below 160 in the original (0x4b4981); parts 1 and 2 (replays)
     * only below 160 too; codes 251 and 252 give the original's box, so they go to the original. */
    static const char *const words[3][6] = {
        {"", " Replay", " Second Replay", " First Leg", " Second Leg", " Decider"},
        {"", " Replays", " Second Replays", " First Legs", " Second Legs", " Deciders"},
        {"", " Replay", " Replay 2", " Leg 1", " Leg 2", " Decider"}};
    const char *name = NULL;
    int named_part = part == 0 || part == 3 || part == 4 || ((part == 1 || part == 2 || part == 5) && code < 160);
    if (season && named_part && code > 0 && code < 1000 && readable(buf) && size >= 2 && size <= 0x1000)
        name = season_round_name(season_change_file(context), code - part, season, set);
    /* Slice 25 (Q370): the stage name id of the league phase (0x41f, "Group A" in the original's table) reads
     * "League Phase" in a season with a league phase (season_uefa.c). */
    if (!name && season && readable(buf) && size >= 2 && size <= 0x1000) {
        name = season_uefa_stage_name(context, season, code);
        if (name) part = 0;
    }
    if (!name) {
        o_004b4850();
        return;
    }
    int w = set == SEASON_ROUND_NAME_LONG ? 0 : set == SEASON_ROUND_NAME_PLURAL ? 1 : 2;
    snprintf(win_str(buf), size, "%s%s", name, words[w][part]);
    rt_ret_cdecl(buf);
}

/* The functions that know their competition set the context for their calls of the name function, and put the
 * one before back after the call, also when the call unwinds. The recompiler reads each RT_WRAP line as it is,
 * so they are written out in full. */
static void with_comp(uint32_t comp, void (*original)(void))
{
    uint32_t saved = context;
    context = comp_of(comp);
    original();
    context = saved;
}

void o_004bfa30(void);
void o_00491380(void);
void o_00492980(void);
void o_00493ee0(void);
void o_004947a0(void);
void o_00495050(void);
void o_007d8120(void);
void o_0048cab0(void);
void o_00586fa0(void);

/* comp_util_format_comp_stage_round_label, cdecl (record, stage, round, style, out, size) */
RT_WRAP(004bfa30, label_round_name) { with_comp(rt_arg(0), o_004bfa30); }
/* comp_screens_draw_league_table_tab, cdecl (comp, top, bottom) */
RT_WRAP(00491380, table_tab_round_name) { with_comp(rt_arg(0), o_00491380); }
/* The cup views read a cup's round table through +0x45 (not 0 once the draw is made) and +0xa3 (the table: a Cup,
 * docs/research/2026-10-17-cup-round-record.md). The Friendly competition (Friendly::Friendly 0x5c1f80 sets vtable
 * 0x96ae7c at 0x5c1fee) is no cup, but a click on "Friendly" in a fixture list opens its screen, and its first view is
 * the cup rounds tab 0x492980 (0x490797..0x4907a8: not a League_Stage). In a game of the October 2026 data (2025/26)
 * its bytes at +0x45 are not 0 and at +0xa3 hold no address, so the tab read through them (0x492bb0..0x492bbd) past
 * the end of the browser's memory: "memory access out of bounds" (a guarded native screen sweep, 2026-11-12; the
 * desktop build read zeros there). With +0x45 0 the rounds tab and the schedule tab 0x4947a0 show the original's own
 * text instead ("The draw has not yet been made.", 0x492b97 to 0x492c3d; 0x49485a), as they do for Friendly with the
 * 2001 data. So for the Friendly object, or any whose round table is not a readable address, +0x45 reads 0 during
 * the draw and is put back after it; a cup with a table is drawn as before. */
#define FRIENDLY_VTABLE 0x0096ae7cu
static void cup_view(uint32_t comp, void (*original)(void))
{
    int hide = readable(comp) && rd32(g_mem, comp + 0x45) != 0 &&
               (rd32(g_mem, comp) == FRIENDLY_VTABLE || !readable(rd32(g_mem, comp + 0xa3)));
    uint32_t drawn = hide ? rd32(g_mem, comp + 0x45) : 0;
    if (hide) wr32(g_mem, comp + 0x45, 0);
    with_comp(comp, original);
    if (hide) wr32(g_mem, comp + 0x45, drawn);
}

/* comp_screens_draw_cup_rounds_tab, cdecl (comp, top, bottom) */
RT_WRAP(00492980, rounds_tab_round_name) { cup_view(rt_arg(0), o_00492980); }
/* comp_screens_draw_fixtures_tab, cdecl (comp, top, bottom): calls 0x4bffc0 */
RT_WRAP(00493ee0, fixtures_tab_round_name) { with_comp(rt_arg(0), o_00493ee0); }
/* comp_screens_draw_schedule_tab, cdecl (comp, top, bottom): calls 0x4bffc0; tests +0x45 as the rounds tab does */
RT_WRAP(004947a0, schedule_tab_round_name) { cup_view(rt_arg(0), o_004947a0); }
/* comp_screens_draw_average_points_tab, cdecl (comp, top, bottom) */
RT_WRAP(00495050, points_tab_round_name) { with_comp(rt_arg(0), o_00495050); }
/* printouts_write_league_tables, cdecl (record, file) */
RT_WRAP(007d8120, printout_round_name) { with_comp(rt_arg(0), o_007d8120); }
/* Comp::write_progress_text, thiscall: the progress of a club in a competition (calls 0x4bffc0) */
RT_WRAP(0048cab0, progress_round_name) { with_comp(cpu.ecx, o_0048cab0); }
/* EuropeanCup::write_progress_text, thiscall (calls 0x4bffc0) */
RT_WRAP(00586fa0, cl_progress_round_name) { with_comp(cpu.ecx, o_00586fa0); }

/* The rules report (season_uefa.c): the label of a round of a competition through the label function 0x4bfa30
 * (a wrap above sets the competition), style 1 (long names) or 3 (abbreviated names, 0x4bfc14). Test runs
 * only. Writes at most size bytes to out. */
void season_round_name_label(uint32_t obj, int code, int style, char *out, int size)
{
    snprintf(out, (size_t)size, "-");
    if (!readable(obj) || !readable(rd32(g_mem, obj + 4))) return;
    uint32_t saved_esp = cpu.esp;
    cpu.esp -= 0x400;
    uint32_t buf = cpu.esp + 0x10;
    g_mem[buf] = 0;
    uint32_t a[6] = {rd32(g_mem, obj + 4), 0, (uint32_t)code, (uint32_t)style, buf, 0x100};
    Cpu saved = cpu;
    rt_callback(0x004bfa30u, 6, a);
    if (rt_unwinding) return;
    cpu = saved;
    snprintf(out, (size_t)size, "%.120s", win_str(buf));
    cpu.esp = saved_esp;
}
