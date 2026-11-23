/* The MLS with clubs that the original does not know (0.6.0 slice 18).
 *
 * The UsaMls class plays three groups of 4 clubs (UsaMls::create_table 0x908560 sets 4 at 0x908572 and takes the
 * clubs of group 0x413, UsaMls::create_second_division_stage 0x908fc0 takes 0x47b, 0x9091e0 takes 0x414). The
 * group of a club comes from usa_mls_get_club_division 0x909ec0, which knows the 12 MLS clubs of the original by
 * their key ids (0x9d037c..0x9d03a8, compares at 0x909ee5..0x909f6b) and stops at usa_mls.cpp 1819 (push 0x71b
 * at 0x909f96, box 0x5e8290 at 0x909fbe) for any other club whose division is the American Major League (key id
 * [0x9cf590], compare at 0x909edd). usa_mls_get_club_all_star_conference 0x90a000 does the same for the two
 * All-Star conferences (0x413 and 0x414, stop at usa_mls.cpp 1859, push 0x743 at 0x90a0d6).
 *
 * The club moves of a season game (season_clubs.c) put the real MLS clubs of the season into the division, up to
 * the 12 that the class takes. So the runtime gives each club that the original does not know a free place:
 * - a club of another division, or a known club, goes to the original (the same result in every game);
 * - an unknown club of the MLS takes the group with the fewest clubs that has fewer than 4 (ties: 0x413, 0x47b,
 *   0x414), counting the clubs of the division in the order of the club table, the known clubs first with their
 *   own group. So every group holds 4 clubs when the division holds 12. A club after the 12th gets 0, the value
 *   of a club that is not in the MLS: no group takes it;
 * - its All-Star conference: 0x413 for group 0x413, 0x414 for group 0x414, and for group 0x47b the conference
 *   with fewer clubs (the original splits group 0x47b 2 and 2).
 * The base game holds only the 12 known clubs, so it never reaches the new path. */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_closed.h"
#include "season_closed_step.h"
#include "season_clubs.h"
#include "season_records.h"
#include "season_mls.h"
#include "season_mls_plan.h"
#include "season_names.h"
#include "season_rules.h"
#include "season_sizes.h"
#include "season_template.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUBS 0xae23b4u             /* the club table (0x245 bytes a club) */
#define CLUB_COUNT 0xae235cu
#define CLUB_SIZE 0x245u
#define CLUB_DIVISION 0x57u         /* club_comp pointer, its +0 is the record id */
#define KEY_MLS 0x9cf590u           /* key id of the American Major League (0x909ed7) */
#define CLOSED_SHORT 0x38u          /* club: short name */

#define GROUP_A 0x413u
#define GROUP_B 0x47bu
#define GROUP_C 0x414u

/* The key ids of the 12 clubs and their group (0x909ee5..0x909f6b) and conference (0x90a025..0x90a0ab). */
static const struct { uint32_t key; uint16_t group, conference; } KNOWN[12] = {
    {0x9d0390u, GROUP_A, GROUP_A}, {0x9d0394u, GROUP_A, GROUP_A}, {0x9d0384u, GROUP_A, GROUP_A},
    {0x9d0380u, GROUP_A, GROUP_A}, {0x9d03a4u, GROUP_B, GROUP_A}, {0x9d0388u, GROUP_B, GROUP_A},
    {0x9d03a0u, GROUP_B, GROUP_C}, {0x9d037cu, GROUP_B, GROUP_C}, {0x9d03a8u, GROUP_C, GROUP_C},
    {0x9d039cu, GROUP_C, GROUP_C}, {0x9d0398u, GROUP_C, GROUP_C}, {0x9d038cu, GROUP_C, GROUP_C},
};

static int in_mls(uint32_t club)
{
    uint32_t div = rd32(g_mem, club + CLUB_DIVISION);
    return div && rd32(g_mem, div) == rd32(g_mem, KEY_MLS);
}

static int known(uint32_t club)
{
    uint32_t id = rd32(g_mem, club);
    for (int k = 0; k < 12; k++)
        if (rd32(g_mem, KNOWN[k].key) == id) return k;
    return -1;
}

/* The group and conference of an unknown club of the MLS: 0 when the three groups are full. */
static void place(uint32_t want, uint16_t *group, uint16_t *conference)
{
    static const uint16_t order[3] = {GROUP_A, GROUP_B, GROUP_C};
    int in_group[3] = {0, 0, 0}, east = 0, west = 0;
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    *group = *conference = 0;
    for (int pass = 0; pass < 2; pass++)            /* the known clubs first, then the others in table order */
        for (int i = 0; i < count; i++) {
            uint32_t c = table + (uint32_t)i * CLUB_SIZE;
            if (!in_mls(c)) continue;
            int k = known(c);
            if ((pass == 0) != (k >= 0)) continue;
            uint16_t g = 0, conf = 0;
            if (k >= 0) {
                g = KNOWN[k].group;
                conf = KNOWN[k].conference;
            } else {
                int best = -1;
                for (int j = 0; j < 3; j++)
                    if (in_group[j] < 4 && (best < 0 || in_group[j] < in_group[best])) best = j;
                if (best >= 0) {
                    g = order[best];
                    conf = g == GROUP_A ? GROUP_A : g == GROUP_C ? GROUP_C : east <= west ? GROUP_A : GROUP_C;
                }
            }
            for (int j = 0; j < 3; j++) in_group[j] += g == order[j];
            east += conf == GROUP_A;
            west += conf == GROUP_C;
            if (c == want) {
                *group = g;
                *conference = conf;
                return;
            }
        }
}

/* 1 when the runtime answers for this club: an unknown club of the MLS, with the season code on. */
static int ours(uint32_t club)
{
    return club && season_code_on() && in_mls(club) && known(club) < 0;
}

void o_00909ec0(void);
void o_0090a000(void);

static int new_conference(uint32_t club);

/* usa_mls_get_club_division 0x909ec0, cdecl (club), returns the group in ax. */
RT_WRAP(00909ec0, mls_club_group)
{
    uint32_t club = rt_arg(0);
    int nc = new_conference(club);
    if (nc) {
        rt_ret_cdecl((rd32(g_mem, club) & 0xffff0000u) | (uint32_t)nc);
        return;
    }
    if (!ours(club)) {
        o_00909ec0();
        return;
    }
    uint16_t group, conference;
    place(club, &group, &conference);
    rt_ret_cdecl((rd32(g_mem, club) & 0xffff0000u) | group);
}

/* usa_mls_get_club_all_star_conference 0x90a000, cdecl (club), returns the conference in ax. */
RT_WRAP(0090a000, mls_club_conference)
{
    uint32_t club = rt_arg(0);
    int nc = new_conference(club);
    if (nc) {
        rt_ret_cdecl((rd32(g_mem, club) & 0xffff0000u) | (uint32_t)nc);
        return;
    }
    if (!ours(club)) {
        o_0090a000();
        return;
    }
    uint16_t group, conference;
    place(club, &group, &conference);
    rt_ret_cdecl((rd32(g_mem, club) & 0xffff0000u) | conference);
}

/* The MLS play-off rounds in a season game (rule O284, row arg-usa-218). UsaMls::create_schedule 0x9072d0 writes
 * the play-off and final round records with flag 0x20 in the match flags (+0x0d 0x230 at 0x907fe7 and 0x908068,
 * +0x0f 0x30 at 0x908003 and 0x90808b, +0x11 0x20 at 0x908009 and 0x908091, the final +0x0d 0x220 at 0x9081c2;
 * cup round record doc, section 2). With 0x20, CM3_MATCH_ENGINE::period_completed 0x6bd930 starts the run-in
 * shootout (state 1 at 0x6bdb1c) in place of penalties (flag 0x1, state 4 at 0x6bdaf7). In it both sides can miss
 * every kick: Kansas City Wizards v Washington DC United (2021 start, October 2021 database, day 287 of 2023) was
 * still 0-0 after 127 kicks a side, the signed kick counter (+0x8e9f) wrapped, kick % 11 read before the taker
 * order (0x6be931..0x6be96a) and the taker became an object that is no player (match_pl.cpp 4885 at 0x6f4731). A
 * run with three substitutes forced met the same 0-0 run-in (San Jose Earthquakes v Atlanta United FC, 40 kicks).
 * MLS dropped the shootout for the 2000 season; from 2002/03 its play-offs go to extra time and then penalties
 * (row arg-usa-218). In a season game the flag becomes 0x3; the base game keeps 0x20 (Q152). The seasons.py sites
 * call this; the init_match wrap (season_match_grid.c) maps a fixture made before (an older save's table). */
uint32_t rt_mls_round_flags(uint32_t flags)
{
    if (!(flags & 0x20u) || !season_code_on() || (season_start_year() == SEASON_FIRST_YEAR && !season_changing_rules()))
        return flags;
    return (flags & ~0x20u) | 0x3u;
}

/* ======== The MLS of real life from 2002 (0.7.2 gap row 7) ========
 *
 * In a season game whose sizes run (season_sizes_on: a changing-rules game, or a start after 2001), an MLS edition
 * of 2002 or later plays the real season (season_mls_plan.h): one table of the division's clubs (the row's count,
 * season_clubs.c class_size), the conference tables as two League_Stages with no fixtures of their own, the
 * unbalanced regular season planned in C, and the play-off by conference. The base game (a 2001 start with fixed
 * rules) keeps the original's form in every season, so the same-save set is unchanged.
 *
 * The original's pieces (re-mls notes, run-logs/0.7.2-gap07/re-mls.md):
 * - UsaMls::create_table 0x908560: +0x3e = 4, +0xb1 = new(0xec), League::init_table_row 0x687430 (thiscall: row,
 *   club, 0, 0) for each club of division +4 and group 0x413 (0x908604..0x90862e).
 * - UsaMls::create_season_fixtures 0x9086e0: the fixture prototype (0x9087eb..0x9088e0), one fixture a match through
 *   Stadium::set_fixture_venue 0x85c260 (this [0xdd7ef4]; record +0x3d, &fixture, +0x24) and
 *   FIXTURE_MANAGER::add_fixture 0x5aa680 (this [0xae2a58]; &fixture, 1) (0x908bf7..0x908c27); at the end the
 *   template of stage -1 (32 records) in +0xba and +0x3a = 0x413 (0x908db9..0x908de3).
 * - The stages 0x908fc0 and 0x9091e0: League_Stage::League_Stage 0x68b100 (new 0xee, 22 arguments); with no template
 *   it makes no fixtures (0x68b3ad..0x68b3b5) and keeps one record of today (0x68b46c..0x68b495). UsaMls::add_result
 *   0x9099d0 gives each result to the main table and to both stages while +0x30 is 1, and a table counts the row of
 *   each of its clubs on its own (League::apply_result_to_table 0x685f60), so the stages are the conference tables.
 * - UsaMls::get_club_array 0x909a40 (12 clubs) and UsaMls::set_predicted_positions 0x909bb0 (places 1, 4, 7, 10 of
 *   three tables, Comp_Stats 0x4a2540) take a table of N in the new form.
 * - UsaMls::create_playoff_stage 0x9094c0: Cup_Stage::Cup_Stage 0x522e00 of 8 clubs with the template of stage 2
 *   (vtable +0x3c, 0x9096d4), stages[2], Cup::set_up_round 0x51c800 (0x9097ac). */

#define MLS_VTABLE 0x970844u
#define F_MALLOC 0x00944e46u
#define F_FREE 0x009452cau
#define F_NEW 0x00944cf1u
#define F_QSORT 0x00945333u
#define F_INIT_ROW 0x00687430u
#define F_VENUE 0x0085c260u
#define F_ADD_FIXTURE 0x005aa680u
#define F_LEAGUE_STAGE 0x0068b100u
#define F_CUP_STAGE 0x00522e00u
#define F_SET_UP_ROUND 0x0051c800u
#define F_PREDICT 0x004a2540u
#define F_REPUTATION_CMP 0x004b07d0u   /* the compare of set_predicted_positions (0x909cda) */
#define STADIUMS 0x00dd7ef4u
#define FIXTURES 0x00ae2a58u
#define CLUB_NAME 4u

/* The conference of a club by its real name: an update database gives a retired record to a newer club
 * (season_closed.h). */
static int club_conference(uint32_t club, int season)
{
    const char *name = win_str(club + CLUB_NAME);
    if (season_changing_rules()) name = season_closed_real_name("usa_mls", name, win_str(club + CLOSED_SHORT));
    return season_mls_conference(name, season);
}
#define ROW_SIZE 0x3bu
#define DATE_SIZE 0x41u
#define FIXTURE_SIZE 0x4fu

#define COMP_RECORD 4u
#define COMP_STATS 8u
#define COMP_STAGES 0xcu
#define COMP_VENUE 0x24u
#define COMP_STAGE_COUNT 0x2cu
#define COMP_STAGE_NOW 0x30u
#define COMP_GROUP 0x3au
#define COMP_CLUBS 0x3eu
#define COMP_YEAR 0x40u
#define COMP_STAGE_INDEX 0x44u
#define COMP_MATCHES 0xa7u
#define COMP_DATES 0xa9u
#define COMP_TABLE 0xb1u
#define COMP_TEMPLATE 0xbau
#define MLS_STAGES 8                  /* the stage array of the new form: 2 tables, at most 4 rounds, the final */
#define F_DRAW_DATE 0x00521e60u       /* cup_set_round_draw_date, cdecl (table, round, day, month, year+, weekday, year) */
#define F_MATCH_DATE 0x00521eb0u      /* cup_set_round_match_date, cdecl (+ a7, year, venue) */
#define NO_WEEKDAY 0xffffffffu
#define GAME_DATE 0x00ae2c90u
#define ROUND_SIZE 0x68u
#define ENTRY_SIZE 6u                 /* Cup_Stage +0xa7 entries: club, +4 seed, +5 state (1: still in) */

static int readable(uint32_t p) { return rt_readable(p); }

static int mls_on(void)
{
    static int off = -1;
    if (off < 0) off = getenv("RECOMP_MLS") && !strcmp(getenv("RECOMP_MLS"), "off");
    return !off && season_sizes_on();
}

static uint32_t call_this(uint32_t fn, uint32_t self, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t call_cdecl(uint32_t fn, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t game_malloc(uint32_t size)
{
    uint32_t a[1] = {size};
    uint32_t p = call_cdecl(F_MALLOC, 1, a);
    if (rt_unwinding || !readable(p)) return 0;
    memset(g_mem + p, 0, size);
    return p;
}

static void game_free(uint32_t p)
{
    uint32_t a[1] = {p};
    if (readable(p) && !rt_unwinding) call_cdecl(F_FREE, 1, a);
}

/* The season whose form the object plays: the start year's with fixed rules or while the game is made, else the
 * edition's (the edition Y of the MLS is the real season Y, season_mls_plan.h). */
static int form_season(uint32_t obj)
{
    if (!season_changing_rules() || season_change_is_creating()) return season_start_year();
    return (int16_t)rd16(g_mem, obj + COMP_YEAR);
}

static int new_form(uint32_t obj)
{
    return mls_on() && readable(obj) && rd32(g_mem, obj) == MLS_VTABLE && form_season(obj) >= MLS_FIRST_SEASON;
}

/* The club count of the division in a season of the new form: the count of its row (row Y - 1 holds the real
 * season Y, season_sizes_edition), at most MLS_MAX_CLUBS. -1 when the original's form plays. */
static int row_clubs(int season)
{
    int s = season_rules_last_season(season_sizes_edition("usa_mls", season), "usa_mls");
    const SeasonRule *r = s ? season_rules_find(s, "usa_mls", 0) : NULL;
    return r && r->clubs >= 4 && r->clubs <= MLS_MAX_CLUBS ? r->clubs : -1;
}

int season_mls_division_size(const char *division, int season)
{
    if (!mls_on() || !division || strncmp(division, "American Major League", 52) || season < MLS_FIRST_SEASON)
        return -1;
    return row_clubs(season);
}

static uint32_t row_club(uint32_t obj, int i)
{
    uint32_t t = rd32(g_mem, obj + COMP_TABLE);
    int n = (int16_t)rd16(g_mem, obj + COMP_CLUBS);
    if (!readable(t) || i < 0 || i >= n) return 0;
    return rd32(g_mem, t + (uint32_t)i * ROW_SIZE);
}

/* The MLS object of the game, for the club wraps (which get only the club). */
static uint32_t mls_object(void)
{
    static uint32_t cached;
    if (readable(cached) && rd32(g_mem, cached) == MLS_VTABLE) return cached;
    uint32_t objs[400];
    int n = season_comp_objects(objs, 400);
    for (int i = 0; i < n; i++)
        if (readable(objs[i]) && rd32(g_mem, objs[i]) == MLS_VTABLE) return cached = objs[i];
    return 0;
}

static int in_table(uint32_t table_obj, uint32_t club)
{
    if (!readable(table_obj)) return 0;
    int n = (int16_t)rd16(g_mem, table_obj + COMP_CLUBS);
    uint32_t t = rd32(g_mem, table_obj + COMP_TABLE);
    if (!readable(t) || n < 0 || n > 64) return 0;
    for (int i = 0; i < n; i++)
        if (rd32(g_mem, t + (uint32_t)i * ROW_SIZE) == club) return 1;
    return 0;
}

/* The conference of a club in the new form: GROUP_A (East) or GROUP_C (West) by the conference stage that holds it,
 * else by its name; 0 when the original answers. */
static uint32_t star_club[4];           /* the clubs of stage 0's rows while select_squads runs (its wrap below) */
static int star_conf[4];

static int new_conference(uint32_t club)
{
    for (int i = 0; i < 4; i++)
        if (club && star_club[i] == club) return star_conf[i];
    if (!club || !mls_on() || !in_mls(club)) return 0;
    uint32_t obj = mls_object();
    if (!new_form(obj)) return 0;
    uint32_t stages = rd32(g_mem, obj + COMP_STAGES);
    if (readable(stages) && (int32_t)rd32(g_mem, obj + COMP_STAGE_NOW) >= 1) {
        if (in_table(rd32(g_mem, stages), club)) return GROUP_A;
        if (in_table(rd32(g_mem, stages + 4), club)) return GROUP_C;
    }
    return club_conference(club, form_season(obj)) == MLS_WEST ? GROUP_C : GROUP_A;
}

/* The clubs that fold and join at the season change (0.7.3 slice 6, issue 157, decision L7). UsaMls::start_new_season
 * 0x908e00 adds 1 to the edition (0x908e68), calls set_up_rules (0x908e6e) and then create_table (0x908eac), so the
 * wrap of create_table sees the new edition before any table of it exists (season_closed_step.h). Only with changing
 * rules on, and never at a new game: the club moves apply the same plan to the first edition (season_clubs.c
 * closed_moves). */
static void closed_step(uint32_t obj)
{
    if (!season_changing_rules() || season_change_is_creating()) return;
    int season = form_season(obj);
    season_closed_step(rd32(g_mem, obj + COMP_RECORD), "usa_mls", "mls", season, (int16_t)rd16(g_mem, obj + COMP_YEAR),
                       row_clubs(season));
}

void o_00908560(void);
void o_009086e0(void);
void o_00909a40(void);
void o_00909bb0(void);
void o_009094c0(void);

/* UsaMls::create_table 0x908560, thiscall, returns 1 when the table is full. New form: every club of the division,
 * in the order of the club table, up to MLS_MAX_CLUBS. */
RT_WRAP(00908560, mls_create_table)
{
    uint32_t obj = cpu.ecx;
    if (!new_form(obj)) {
        o_00908560();
        return;
    }
    closed_step(obj);
    if (rt_unwinding) return;
    uint32_t rec = rd32(g_mem, obj + COMP_RECORD), table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT), n = 0;
    for (int i = 0; i < count; i++) n += rd32(g_mem, table + (uint32_t)i * CLUB_SIZE + CLUB_DIVISION) == rec;
    if (n > MLS_MAX_CLUBS) n = MLS_MAX_CLUBS;
    wr16(g_mem, obj + COMP_CLUBS, (uint16_t)n);
    /* +0xa7, the matches of a club, before the rows: a row keeps one byte a match in a block of +0xa7 bytes (the
     * load sizes it so, League::load_from_temp_file 0x688c59..0x688c61); set_up_rules wrote 32 (0x908660). */
    const MlsSchedule *sched = season_mls_schedule(form_season(obj));
    if (sched) wr16(g_mem, obj + COMP_MATCHES, (uint16_t)sched->games);
    uint32_t rows = game_malloc((uint32_t)(n > 0 ? n : 1) * ROW_SIZE);
    wr32(g_mem, obj + COMP_TABLE, rows);
    if (!rows) {
        rt_ret_stdcall(0, 0);
        return;
    }
    int k = 0;
    for (int i = 0; i < count && k < n; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        if (rd32(g_mem, club + CLUB_DIVISION) != rec) continue;
        uint32_t a[4] = {(uint32_t)k++, club, 0, 0};
        call_this(F_INIT_ROW, obj, 4, a);
        if (rt_unwinding) return;
    }
    int edition = (int16_t)rd16(g_mem, obj + COMP_YEAR);
    fprintf(stderr, "recomp: mls: %d table of %d clubs (season %d form)\n", edition, k, form_season(obj));
    /* closed_step brings the league to its row. This line names an edition that still differs: a human manages a
     * club that folded, or a club of a custom database is missing. The run check (league_size_run.py) fails on it. */
    int want = row_clubs(form_season(obj));
    if (want > 0 && k != want)
        fprintf(stderr, "recomp: mls: %d: %d clubs, the row %d: %s\n", edition, k, want,
                k < want ? "the database has no other club of the division" : "a club that folds has a human manager");
    rt_ret_stdcall(k == n && n >= 2, 0);
}

/* One conference table: a League_Stage of the conference's clubs with no template, so no fixtures (0x68b3b5). */
static int make_stage(uint32_t obj, int index, const uint32_t *clubs, int n, uint16_t group)
{
    uint32_t list = game_malloc((uint32_t)(n > 0 ? n : 1) * 4u);
    if (!list) return 0;
    for (int i = 0; i < n; i++) wr32(g_mem, list + 4u * (uint32_t)i, clubs[i]);
    uint32_t sz[1] = {0xee};
    uint32_t stage = call_cdecl(F_NEW, 1, sz);
    if (rt_unwinding || !readable(stage)) {
        game_free(list);
        return 0;
    }
    uint32_t a[22] = {obj,
                      (uint32_t)n,
                      list,
                      (uint32_t)(int16_t)rd16(g_mem, obj + 0x3c),
                      rd32(g_mem, obj + COMP_RECORD),
                      0,
                      0,
                      g_mem[obj + 0xc2],
                      g_mem[obj + 0xc3],
                      g_mem[obj + 0xc4],
                      obj + 0xc5,
                      obj + 0xbe,
                      (uint32_t)(int16_t)rd16(g_mem, obj + COMP_YEAR),
                      (uint32_t)index,
                      0,
                      g_mem[obj + 0x51],
                      1,
                      rd16(g_mem, obj + 0xab),
                      rd16(g_mem, obj + 0xd9),
                      rd16(g_mem, obj + COMP_MATCHES),
                      0,
                      2};
    call_this(F_LEAGUE_STAGE, stage, 22, a);
    if (rt_unwinding) return 0;
    game_free(list);
    wr32(g_mem, rd32(g_mem, obj + COMP_STAGES) + 4u * (uint32_t)index, stage);
    wr16(g_mem, stage + COMP_GROUP, group);
    wr32(g_mem, obj + COMP_STAGE_NOW, (uint32_t)index);
    return 1;
}

/* The regular season of the new form. Returns 1 when the fixtures are made. */
static int make_season(uint32_t obj)
{
    int season = form_season(obj), year = (int16_t)rd16(g_mem, obj + COMP_YEAR);
    int n = (int16_t)rd16(g_mem, obj + COMP_CLUBS);
    const MlsSchedule *s = season_mls_schedule(season);
    if (!s || n < 2 || n > MLS_MAX_CLUBS) return 0;
    uint32_t clubs[MLS_MAX_CLUBS];
    int conf[MLS_MAX_CLUBS];
    for (int i = 0; i < n; i++) {
        clubs[i] = row_club(obj, i);
        conf[i] = club_conference(clubs[i], season);
    }
    static MlsPlan plan;
    char why[160];
    int rounds = season_mls_plan(s, n, conf, year, &plan, why, sizeof why);
    if (!rounds) {
        fprintf(stderr, "recomp: mls: %d: no plan for %d clubs: %s\n", year, n, why);
        return 0;
    }
    /* The dates: the class's 32 match days (stage -1) planned to the rounds (season_template.h). */
    uint32_t outs = game_malloc(8);
    if (!outs) return 0;
    uint32_t sched = rd32(g_mem, rd32(g_mem, obj) + 0x3c);
    uint32_t ta[4] = {(uint32_t)-1, outs, outs + 4, 0};
    uint32_t old = call_this(sched, obj, 4, ta);
    if (rt_unwinding) return 0;
    int have = (int16_t)rd16(g_mem, outs);
    game_free(outs);
    uint32_t tmpl = season_sizes_build_template(obj, "usa_mls", old, have, rounds, SEASON_TEMPLATE_SEQUENTIAL);
    if (rt_unwinding) return 0;
    game_free(old);
    if (!tmpl) {
        fprintf(stderr, "recomp: mls: %d: no room for %d rounds in %d dates\n", year, rounds, have);
        return 0;
    }
    wr32(g_mem, obj + COMP_TEMPLATE, tmpl);
    wr16(g_mem, obj + COMP_DATES, (uint16_t)rounds);
    wr16(g_mem, obj + COMP_GROUP, 0);
    /* The fixture prototype, as 0x9087eb..0x9088e0. */
    uint32_t fx = game_malloc(FIXTURE_SIZE + 1);
    if (!fx) return 0;
    uint32_t last = tmpl + (uint32_t)(rounds - 1) * DATE_SIZE;
    wr32(g_mem, fx + 0x00, rd32(g_mem, rd32(g_mem, obj + COMP_RECORD)));
    wr32(g_mem, fx + 0x08, 0xffffffffu);
    wr32(g_mem, fx + 0x14, rd32(g_mem, obj + COMP_RECORD));
    wr16(g_mem, fx + 0x24, (uint16_t)((int16_t)rd16(g_mem, last + 2) + year));
    wr16(g_mem, fx + 0x26, rd16(g_mem, last));
    wr16(g_mem, fx + 0x2c, (uint16_t)year);
    wr16(g_mem, fx + 0x32, 0);
    wr16(g_mem, fx + 0x36, rd16(g_mem, obj + 0xdb));
    wr16(g_mem, fx + 0x38, rd16(g_mem, obj + 0xab));
    g_mem[fx + 0x3b] = 0xff;
    g_mem[fx + 0x40] = g_mem[fx + 0x41] = 1;
    g_mem[fx + 0x42] = g_mem[obj + COMP_STAGE_INDEX];
    memset(g_mem + fx + 0x43, 0xff, 10);
    wr16(g_mem, fx + 0x4d, (uint16_t)((g_mem[obj + 0x49] & 0xfu) | ((g_mem[obj + 0x4a] & 0xfu) << 4)));
    int games = 0;
    for (int k = 0; k < plan.matches; k++) {
        const MlsMatch *m = &plan.m[k];
        uint32_t rec = tmpl + (uint32_t)m->round * DATE_SIZE, home = clubs[m->home], away = clubs[m->away];
        wr16(g_mem, fx + 0x28, (uint16_t)((int16_t)rd16(g_mem, rec + 2) + year));
        wr16(g_mem, fx + 0x2a, rd16(g_mem, rec));
        g_mem[fx + 0x3f] = g_mem[rec + 4];
        wr16(g_mem, fx + 0x34, (uint16_t)m->round);
        g_mem[fx + 0x3a] = m->round < rounds - 3 ? 2 : 5;
        wr32(g_mem, fx + 0x0c, rd32(g_mem, home));
        wr32(g_mem, fx + 0x10, rd32(g_mem, away));
        wr32(g_mem, fx + 0x1c, home);
        wr32(g_mem, fx + 0x20, away);
        uint32_t va[3] = {rd32(g_mem, rec + 0x3d), fx, rd32(g_mem, obj + COMP_VENUE)};
        call_this(F_VENUE, rd32(g_mem, STADIUMS), 3, va);
        if (rt_unwinding) return 0;
        uint32_t aa[2] = {fx, 1};
        call_this(F_ADD_FIXTURE, rd32(g_mem, FIXTURES), 2, aa);
        if (rt_unwinding) return 0;
    }
    game_free(fx);
    for (int i = 0; i < n; i++)
        if (plan.games[i] > games) games = plan.games[i];
    wr16(g_mem, obj + COMP_MATCHES, (uint16_t)games);
    /* The stage array: the two conference tables, one stage a play-off round, then the final (+0x2c is the count:
     * advance_stage 0x909400 and the save, which keeps +0x2c (0x6898dc), read it). */
    int ne0 = 0;
    for (int i = 0; i < n; i++) ne0 += plan.conf[i] == MLS_EAST;
    int bracket[2], targets[4];
    int count = 3 + season_mls_playoff_rounds(season_mls_playoff(season), ne0, n - ne0, bracket, targets);
    if ((int32_t)rd32(g_mem, obj + COMP_STAGE_COUNT) < count) {
        uint32_t arr = game_malloc(4u * MLS_STAGES), old_arr = rd32(g_mem, obj + COMP_STAGES);
        if (!arr) return 0;
        game_free(old_arr);
        wr32(g_mem, obj + COMP_STAGES, arr);
    }
    wr32(g_mem, obj + COMP_STAGE_COUNT, (uint32_t)count);
    /* The conference tables. */
    uint32_t east[MLS_MAX_CLUBS], west[MLS_MAX_CLUBS];
    int ne = 0, nw = 0;
    for (int i = 0; i < n; i++) {
        if (plan.conf[i] == MLS_EAST) east[ne++] = clubs[i];
        else west[nw++] = clubs[i];
    }
    if (!make_stage(obj, 0, east, ne, GROUP_A) || !make_stage(obj, 1, west, nw, GROUP_C)) return 0;
    int low = 999;
    for (int i = 0; i < n; i++)
        if (plan.games[i] < low) low = plan.games[i];
    fprintf(stderr, "recomp: mls: %d: %d clubs (East %d, West %d), %d matches, %d to %d games, %d rounds from day %d "
                    "to %d\n", year, n, ne, nw, plan.matches, low, games, rounds, (int16_t)rd16(g_mem, tmpl),
            (int16_t)rd16(g_mem, last));
    return 1;
}

/* UsaMls::create_season_fixtures 0x9086e0, thiscall, returns 1. */
RT_WRAP(009086e0, mls_season_fixtures)
{
    uint32_t obj = cpu.ecx;
    if (!new_form(obj)) {
        o_009086e0();
        return;
    }
    int ok = make_season(obj);
    if (rt_unwinding) return;
    rt_ret_stdcall((uint32_t)ok, 0);
}

/* UsaMls::get_club_array 0x909a40, thiscall (Club ***out, short *count), ret 8. */
RT_WRAP(00909a40, mls_club_array)
{
    uint32_t obj = cpu.ecx, out = rt_arg(0), cnt = rt_arg(1);
    if (!new_form(obj) || (int32_t)rd32(g_mem, obj + COMP_STAGE_NOW) < 1) {
        o_00909a40();
        return;
    }
    int n = (int16_t)rd16(g_mem, obj + COMP_CLUBS);
    uint32_t a[1] = {(uint32_t)(n > 0 ? n : 1) * 4u};
    uint32_t arr = call_cdecl(F_MALLOC, 1, a);
    if (rt_unwinding) return;
    for (int i = 0; i < n && readable(arr); i++) wr32(g_mem, arr + 4u * (uint32_t)i, row_club(obj, i));
    wr16(g_mem, cnt, (uint16_t)n);
    wr32(g_mem, out, arr);
    rt_ret_stdcall(arr, 2);
}

/* UsaMls::set_predicted_positions 0x909bb0, thiscall: the clubs by reputation (compare 0x4b07d0), places 1 to N. */
RT_WRAP(00909bb0, mls_predicted)
{
    uint32_t obj = cpu.ecx;
    if (!new_form(obj) || (int32_t)rd32(g_mem, obj + COMP_STAGE_NOW) < 1) {
        o_00909bb0();
        return;
    }
    int n = (int16_t)rd16(g_mem, obj + COMP_CLUBS);
    uint32_t stats = rd32(g_mem, obj + COMP_STATS), list = game_malloc((uint32_t)(n > 0 ? n : 1) * 4u);
    if (readable(stats) && list) {
        for (int i = 0; i < n; i++) wr32(g_mem, list + 4u * (uint32_t)i, row_club(obj, i));
        uint32_t qa[4] = {list, (uint32_t)n, 4, F_REPUTATION_CMP};
        call_cdecl(F_QSORT, 4, qa);
        for (int i = 0; i < n && !rt_unwinding; i++) {
            uint32_t pa[2] = {rd32(g_mem, list + 4u * (uint32_t)i), (uint32_t)(i + 1)};
            call_this(F_PREDICT, stats, 2, pa);
        }
    }
    if (rt_unwinding) return;
    game_free(list);
    rt_ret_stdcall(0, 0);
}

/* UsaMlsAllStars::select_squads 0x90a980, thiscall, ret 0. It reads the MLS object ([0xadadfc][[0x9cf590]],
 * 0x90a99b..0x90a9bd) and 4 rows (0x90aa1b, row step 0x3b at 0x90ab7d) of three tables. The East squad takes every
 * club of the main table's rows (+0xb1, 0x90aaf2..0x90aafc) and the clubs of stage 0's rows that are East (0x90a000
 * gives 0x413, 0x90aab1..0x90aacb). The West squad takes every club of stage 1's rows (0x90aceb..0x90acf5) and the
 * clubs of stage 0's rows that are West (0x414, 0x90acaa..0x90acc4). The players of each club's squad (+0xd7, 50
 * slots) who are fit and free join the list (0x90aaff..0x90ab50), and 0x6e2000 picks the squad from it (0x90ac8e).
 * In the new form the main table holds every club and stage 0 is the East table, so an East club in the first 4 rows
 * of both gave its players twice, and MATCH_MAN::pick_team 0x6dbc50 stopped at match_man..cpp 1225 (a player twice in
 * the pick, test 0x6dd3c1) on day 214 of 2002 (run1 of gap row 7).
 * New form: for the call, the three tables read 4 rows each that hold no club twice: the main table the first 4
 * clubs of the East table, stage 0 the 5th and 6th clubs of each conference table, stage 1 the first 4 clubs of the
 * West table. So each squad comes from the best 6 clubs of its conference. A missing club is an empty club record
 * (no players, no division: 0x90a000 gives 0 at 0x90a00f). While the call runs, the conference of a club of stage
 * 0's rows is the table it came from. The tables are set back after the call. */
void o_0090a980(void);

static void put_rows(uint32_t rows, const uint32_t *clubs, uint32_t empty)
{
    for (int i = 0; i < 4; i++) wr32(g_mem, rows + (uint32_t)i * ROW_SIZE, clubs[i] ? clubs[i] : empty);
}

RT_WRAP(0090a980, mls_all_star_squads)
{
    uint32_t obj = mls_object();
    uint32_t stages = readable(obj) ? rd32(g_mem, obj + COMP_STAGES) : 0;
    if (!new_form(obj) || !readable(stages) || (int32_t)rd32(g_mem, obj + COMP_STAGE_NOW) < 1 ||
        !readable(rd32(g_mem, stages)) || !readable(rd32(g_mem, stages + 4))) {
        o_0090a980();
        return;
    }
    uint32_t east = rd32(g_mem, stages), west = rd32(g_mem, stages + 4);
    uint32_t block = game_malloc(3u * 4u * ROW_SIZE + CLUB_SIZE);
    if (rt_unwinding) return;
    if (!block) {
        o_0090a980();
        return;
    }
    uint32_t empty = block + 3u * 4u * ROW_SIZE;
    uint32_t first_east[4], first_west[4], next[4];
    for (int i = 0; i < 4; i++) {
        first_east[i] = row_club(east, i);
        first_west[i] = row_club(west, i);
        next[i] = row_club(i < 2 ? east : west, 4 + (i & 1));
    }
    put_rows(block, first_east, empty);
    put_rows(block + 4u * ROW_SIZE, next, empty);
    put_rows(block + 8u * ROW_SIZE, first_west, empty);
    uint32_t saved[3] = {rd32(g_mem, obj + COMP_TABLE), rd32(g_mem, east + COMP_TABLE), rd32(g_mem, west + COMP_TABLE)};
    wr32(g_mem, obj + COMP_TABLE, block);
    wr32(g_mem, east + COMP_TABLE, block + 4u * ROW_SIZE);
    wr32(g_mem, west + COMP_TABLE, block + 8u * ROW_SIZE);
    for (int i = 0; i < 4; i++) {
        star_club[i] = next[i];
        star_conf[i] = i < 2 ? GROUP_A : GROUP_C;
    }
    o_0090a980();
    memset(star_club, 0, sizeof star_club);
    wr32(g_mem, obj + COMP_TABLE, saved[0]);
    wr32(g_mem, east + COMP_TABLE, saved[1]);
    wr32(g_mem, west + COMP_TABLE, saved[2]);
    if (rt_unwinding) return;
    game_free(block);
    fprintf(stderr, "recomp: mls: %d all-star squads from the best 6 clubs of each conference\n",
            form_season(obj));
}

/* ---- The play-off of the new form ----
 * Stage 2 is the first round, each round its own Cup_Stage at the next index, the final at +0x2c - 1. A round's
 * clubs come from the conference tables (stages 0 and 1, which the play-off results do not change) and from the
 * stages of the rounds before: the clubs that had a bye and the entries still in (state 1, as
 * create_final_stage 0x9097d0 reads them, 0x909882..0x90988e). */

static int leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

static void date_in(int n, int *doy, int *year)
{
    int d = (int16_t)rd16(g_mem, GAME_DATE), y = (int16_t)rd16(g_mem, GAME_DATE + 2);
    for (d += n; d >= 365 + leap(y); y++) d -= 365 + leap(y);
    *doy = d;
    *year = y;
}

static void day_month(int doy, int year, int *day, int *month)
{
    static const int len[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int m = 0;
    while (m < 11 && doy >= len[m] + (m == 1 && leap(year))) doy -= len[m] + (m == 1 && leap(year)), m++;
    *day = doy + 1;
    *month = m;
}

/* The draw and match dates of round 0 of a round table, `draw` and `match` days from today. */
static void round_dates(uint32_t table, int draw, int match, uint32_t a7, uint32_t venue, uint32_t year)
{
    int doy, y, day, month;
    date_in(draw, &doy, &y);
    day_month(doy, y, &day, &month);
    uint32_t d[7] = {table, 0, (uint32_t)day, (uint32_t)month, (uint32_t)(y - (int)year), NO_WEEKDAY, year};
    call_cdecl(F_DRAW_DATE, 7, d);
    if (rt_unwinding) return;
    date_in(match, &doy, &y);
    day_month(doy, y, &day, &month);
    uint32_t m[9] = {table, 0, (uint32_t)day, (uint32_t)month, (uint32_t)(y - (int)year), NO_WEEKDAY, a7, year, venue};
    call_cdecl(F_MATCH_DATE, 9, m);
}

typedef struct {
    int n[2];
    uint32_t club[2][MLS_MAX_CLUBS];        /* in seed order */
} Brackets;

static int seed_of(const Brackets *b, int c, uint32_t club)
{
    for (int i = 0; i < b->n[c]; i++)
        if (b->club[c][i] == club) return i;
    return 999;
}

#define ROW_FOR 0x15u        /* s16 goals for, +0x17 against, +0x19 points (the compare of 0x683264..0x683275) */
#define ROW_AGAINST 0x17u
#define ROW_POINTS 0x19u

/* The clubs in the order of the regular season: the rows of both conference tables by points, goal difference and
 * goals for (East first when level). The play-off cannot use the main table: UsaMls::add_result gives every result to
 * it (League::add_result 0x685d30 at 0x9099eb) and to the stages only while +0x30 is 1 (0x9099f0..0x9099f8). Each
 * round reads the brackets again (left_before), so with the main table the wild clubs of a later round were not the
 * clubs of round one: in 2002 (6 wild clubs) the West tie San Jose Earthquakes v Miami Fusion FC was no tie of the
 * stage, both had lost, and the West sent no club to the final round (run5tie of gap row 7). */
static int regular_order(uint32_t obj, uint32_t *out)
{
    uint32_t stages = rd32(g_mem, obj + COMP_STAGES);
    int n = 0, key[MLS_MAX_CLUBS][3];
    for (int c = 0; c < 2; c++) {
        uint32_t st = readable(stages) ? rd32(g_mem, stages + 4u * (uint32_t)c) : 0;
        uint32_t t = readable(st) ? rd32(g_mem, st + COMP_TABLE) : 0;
        int k = readable(t) ? (int16_t)rd16(g_mem, st + COMP_CLUBS) : 0;
        for (int i = 0; i < k && n < MLS_MAX_CLUBS; i++) {
            uint32_t row = t + (uint32_t)i * ROW_SIZE;
            int gf = (int16_t)rd16(g_mem, row + ROW_FOR), ga = (int16_t)rd16(g_mem, row + ROW_AGAINST);
            int k3[3] = {(int16_t)rd16(g_mem, row + ROW_POINTS), gf - ga, gf}, j = n++;
            /* insertion: a club moves up only past a worse club, so level clubs keep the table order */
            while (j > 0 && (k3[0] > key[j - 1][0] || (k3[0] == key[j - 1][0] && (k3[1] > key[j - 1][1] ||
                             (k3[1] == key[j - 1][1] && k3[2] > key[j - 1][2]))))) {
                out[j] = out[j - 1];
                memcpy(key[j], key[j - 1], sizeof key[j]);
                j--;
            }
            out[j] = rd32(g_mem, row);
            memcpy(key[j], k3, sizeof k3);
        }
    }
    return n;
}

/* The brackets: the best clubs of each conference table, then the wild clubs in the order of the regular season,
 * each into its own conference's bracket while that bracket is under the size of season_mls_playoff_rounds, else
 * the other. */
static int brackets(uint32_t obj, Brackets *b, int targets[4])
{
    const MlsPlayoff *p = season_mls_playoff(form_season(obj));
    uint32_t stages = rd32(g_mem, obj + COMP_STAGES);
    uint32_t st[2] = {rd32(g_mem, stages), rd32(g_mem, stages + 4)};
    if (!p || !readable(st[0]) || !readable(st[1])) return -1;
    int nc[2] = {(int16_t)rd16(g_mem, st[0] + COMP_CLUBS), (int16_t)rd16(g_mem, st[1] + COMP_CLUBS)}, size[2];
    int rounds = season_mls_playoff_rounds(p, nc[0], nc[1], size, targets);
    int want[2] = {p->east < nc[0] ? p->east : nc[0], p->west < nc[1] ? p->west : nc[1]};
    for (int c = 0; c < 2; c++) {
        b->n[c] = 0;
        for (int i = 0; i < want[c]; i++) b->club[c][b->n[c]++] = row_club(st[c], i);
    }
    uint32_t order[MLS_MAX_CLUBS];
    int n = regular_order(obj, order);
    for (int i = 0; i < n && (b->n[0] < size[0] || b->n[1] < size[1]); i++) {
        uint32_t club = order[i];
        if (seed_of(b, 0, club) < 999 || seed_of(b, 1, club) < 999) continue;
        int c = in_table(st[1], club) ? 1 : 0;
        if (b->n[c] >= size[c]) c = 1 - c;
        b->club[c][b->n[c]++] = club;
    }
    return rounds;
}

/* The clubs still in each bracket before round r (seed order): the clubs with a bye and the winners of each round
 * before it. */
static void left_before(uint32_t obj, const Brackets *all, const int *targets, int r, Brackets *left)
{
    *left = *all;
    uint32_t stages = rd32(g_mem, obj + COMP_STAGES);
    for (int d = 0; d < r; d++) {
        uint32_t st = rd32(g_mem, stages + 4u * (uint32_t)(2 + d));
        int k = readable(st) ? (int16_t)rd16(g_mem, st + COMP_CLUBS) : 0;
        uint32_t entries = readable(st) ? rd32(g_mem, st + 0xa7) : 0;
        for (int c = 0; c < 2; c++) {
            int pairs[2 * MLS_MAX_CLUBS], m = season_mls_round_pairs(left->n[c], targets[d], pairs);
            int keep = left->n[c] - 2 * m, n2 = 0;
            uint32_t next[MLS_MAX_CLUBS];
            for (int i = 0; i < keep; i++) next[n2++] = left->club[c][i];
            for (int t = 0; t < m; t++) {
                int won = 0;
                char states[2][8] = {"none", "none"};
                for (int side = 0; side < 2; side++) {
                    uint32_t club = left->club[c][pairs[2 * t + side]];
                    for (int e = 0; e < k && readable(entries); e++) {
                        uint32_t at = entries + (uint32_t)e * ENTRY_SIZE;
                        if (rd32(g_mem, at) != club) continue;
                        snprintf(states[side], sizeof states[side], "%u", g_mem[at + 5]);
                        if (g_mem[at + 5] == 1) {
                            next[n2++] = club;
                            won++;
                        }
                    }
                }
                if (won != 1)
                    fprintf(stderr, "recomp: mls: stage %d, %s tie %d: %d clubs go on (%.30s state %s, %.30s state %s, "
                                    "%d entries)\n", 2 + d, c ? "West" : "East", t, won,
                            win_str(left->club[c][pairs[2 * t]] + CLUB_NAME), states[0],
                            win_str(left->club[c][pairs[2 * t + 1]] + CLUB_NAME), states[1], k);
            }
            /* seed order: the order of the bracket */
            for (int i = 1; i < n2; i++)
                for (int j = i; j > 0 && seed_of(all, c, next[j]) < seed_of(all, c, next[j - 1]); j--) {
                    uint32_t t = next[j];
                    next[j] = next[j - 1];
                    next[j - 1] = t;
                }
            left->n[c] = n2;
            for (int i = 0; i < n2; i++) left->club[c][i] = next[i];
        }
    }
}

/* Builds one Cup_Stage at `index` of k clubs (fixed pairs: list[0] v list[1], ...) with round 0 of the template of
 * stage `tstage` (2: two legs, 3: one match), dated from today. */
static int build_stage(uint32_t obj, int index, const uint32_t *clubs, int k, int legs, int tstage, uint32_t venue,
                       uint32_t a7, uint32_t arg11, uint32_t arg12)
{
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, obj + COMP_YEAR);
    uint32_t outs = game_malloc(8), list = game_malloc(4u * (uint32_t)(k > 0 ? k : 1));
    if (!outs || !list) {
        game_free(outs);
        game_free(list);
        return 0;
    }
    uint32_t sched = rd32(g_mem, rd32(g_mem, obj) + 0x3c);
    uint32_t ta[4] = {(uint32_t)tstage, outs, outs + 4, 0};
    uint32_t table = call_this(sched, obj, 4, ta);
    if (rt_unwinding) return 0;
    uint32_t group = (uint32_t)(int16_t)rd16(g_mem, outs + 4);
    game_free(outs);
    if (!readable(table)) {
        game_free(list);
        return 0;
    }
    wr16(g_mem, table + 0x18, (uint16_t)k);
    wr16(g_mem, table + 0x1a, (uint16_t)(k / 2));
    wr16(g_mem, table + 0x1c, (uint16_t)k);
    round_dates(table, 1, legs == 2 ? 3 : 4, a7, venue, year);
    if (rt_unwinding) return 0;
    if (legs == 2) {
        /* a tie on aggregate, as the original's two-leg rounds (the play-off template of the Nations League,
         * 0x57d4ba..: +0x0d 0, +0x0f 3): the template of stage 2 has 0x230 and 0x30 (0x907fe7, 0x908003), and 0x10
         * decides each leg alone (cup round record doc, section 2). The round bits 0x100..0x400 stay. */
        g_mem[table + 0x21] = 2;
        g_mem[table + 0x22] = 4;
        wr16(g_mem, table + 0x0d, rd16(g_mem, table + 0x0d) & 0x0f00u);
        wr16(g_mem, table + 0x0f, 3);
    }
    for (int i = 0; i < k; i++) wr32(g_mem, list + 4u * (uint32_t)i, clubs[i]);
    uint32_t sz[1] = {0xb2};
    uint32_t stage = call_cdecl(F_NEW, 1, sz);
    if (rt_unwinding) return 0;
    if (readable(stage)) {
        uint32_t a[15] = {obj, (uint32_t)k, list, 1, rd32(g_mem, obj + COMP_RECORD), table, year, (uint32_t)index, 1,
                          group, arg11, arg12, 0, 0, 0};
        call_this(F_CUP_STAGE, stage, 15, a);
        if (rt_unwinding) return 0;
    }
    wr32(g_mem, rd32(g_mem, obj + COMP_STAGES) + 4u * (uint32_t)index, readable(stage) ? stage : 0);
    game_free(table);
    game_free(list);
    if (!readable(stage)) return 0;
    uint32_t r0[1] = {0};
    call_this(F_SET_UP_ROUND, stage, 1, r0);
    return !rt_unwinding;
}

/* The place of a club in the regular season (regular_order: the main table holds the play-off results too). */
static int overall(uint32_t obj, uint32_t club)
{
    uint32_t order[MLS_MAX_CLUBS];
    int n = regular_order(obj, order);
    for (int i = 0; i < n; i++)
        if (order[i] == club) return i;
    return 999;
}

/* Builds the stage at `index` (2 to the final). */
static int build_playoff(uint32_t obj, int index)
{
    int year = (int16_t)rd16(g_mem, obj + COMP_YEAR), targets[4];
    const MlsPlayoff *p = season_mls_playoff(form_season(obj));
    Brackets all, left;
    int rounds = brackets(obj, &all, targets);
    if (rounds < 0 || !p) return 0;
    int r = index - 2;
    left_before(obj, &all, targets, r < rounds ? r : rounds, &left);
    uint32_t list[2 * MLS_MAX_CLUBS];
    int k = 0;
    if (r >= rounds) {
        /* The final: the two conference champions, the better club of the overall table first (its ground from 2012;
         * venue 4 is the neutral ground of a one-match final, as season_playoffs.c). */
        if (left.n[0] < 1 || left.n[1] < 1) return 0;
        uint32_t a = left.club[0][0], b = left.club[1][0];
        if (overall(obj, b) < overall(obj, a)) {
            uint32_t t = a;
            a = b;
            b = t;
        }
        list[0] = a;
        list[1] = b;
        int ok = build_stage(obj, index, list, 2, 1, 3, p->final_neutral ? 4 : 0, 2, 0x14, 0);
        fprintf(stderr, "recomp: mls: %d: final %.40s v %.40s\n", year, win_str(a + CLUB_NAME), win_str(b + CLUB_NAME));
        return ok;
    }
    int t = targets[r];
    int legs = p->legs[t == 8 ? 0 : t == 4 ? 1 : t == 2 ? 2 : 3];
    if (legs < 1) legs = 1;
    for (int c = 0; c < 2; c++) {
        int pairs[2 * MLS_MAX_CLUBS], m = season_mls_round_pairs(left.n[c], t, pairs);
        for (int i = 0; i < m; i++) {
            uint32_t better = left.club[c][pairs[2 * i]], worse = left.club[c][pairs[2 * i + 1]];
            /* the first listed club plays at home in the first match: the better club in a one-match tie, the
             * worse club in a two-leg tie (the better hosts the second leg) */
            list[k++] = legs == 2 ? worse : better;
            list[k++] = legs == 2 ? better : worse;
        }
    }
    if (!k) return 0;
    int ok = build_stage(obj, index, list, k, legs, legs == 2 ? 2 : 3, 0, 1, 0x12, 1);
    fprintf(stderr, "recomp: mls: %d: play-off round to %d a conference: %d matches, %d leg%s (East %d left, West %d)\n",
            year, t, k / 2, legs, legs == 2 ? "s" : "", left.n[0], left.n[1]);
    return ok;
}

void o_00909400(void);

/* UsaMls::advance_stage 0x909400, thiscall. New form: the next stage up to +0x2c - 1. */
RT_WRAP(00909400, mls_advance)
{
    uint32_t obj = cpu.ecx;
    int now = (int32_t)rd32(g_mem, obj + COMP_STAGE_NOW), count = (int32_t)rd32(g_mem, obj + COMP_STAGE_COUNT);
    if (!new_form(obj) || now < 1 || now >= count - 1) {
        o_00909400();
        return;
    }
    /* +0x30 moves on only when the stage is built: the save reads the type byte of stages 0..+0x30 (0x689912..
     * 0x68992f) and start_new_season deletes them (0x908e38..0x908e46). With no stage the play-off ends here
     * (+0x2c = now + 1). */
    int ok = build_playoff(obj, now + 1);
    if (rt_unwinding) return;
    if (ok) {
        wr32(g_mem, obj + COMP_STAGE_NOW, (uint32_t)(now + 1));
    } else {
        wr32(g_mem, rd32(g_mem, obj + COMP_STAGES) + 4u * (uint32_t)(now + 1), 0);
        wr32(g_mem, obj + COMP_STAGE_COUNT, (uint32_t)(now + 1));
        fprintf(stderr, "recomp: mls: %d: stage %d not built, the play-off ends at stage %d\n",
                (int16_t)rd16(g_mem, obj + COMP_YEAR), now + 1, now);
    }
    rt_ret_stdcall(0, 0);
}

/* UsaMls::create_playoff_stage 0x9094c0: in the new form advance_stage builds the rounds; a call from elsewhere
 * builds stage 2. */
RT_WRAP(009094c0, mls_playoff)
{
    uint32_t obj = cpu.ecx;
    if (!new_form(obj) || (int32_t)rd32(g_mem, obj + COMP_STAGE_COUNT) <= 3) {
        o_009094c0();
        return;
    }
    build_playoff(obj, 2);
    if (rt_unwinding) return;
    rt_ret_stdcall(0, 0);
}

/* The functions that read a stage number take 2 for the play-off and 3 or more for the final (0x908206, 0x908277,
 * 0x90836d, 0x909de2). In the new form a round at index 2 to +0x2c - 2 is the play-off (2) and +0x2c - 1 the final
 * (3). */
static void map_stage_arg(uint32_t obj, int arg)
{
    if (!new_form(obj)) return;
    uint32_t at = cpu.esp + 4u * (uint32_t)(arg + 1);
    int stage = (int8_t)g_mem[at], count = (int32_t)rd32(g_mem, obj + COMP_STAGE_COUNT);
    if (stage >= 2 && count > 4) g_mem[at] = stage >= count - 1 ? 3 : 2;
}

void o_00908200(void);
void o_00908360(void);
void o_00909dd0(void);

RT_WRAP(00908200, mls_staff_records)   /* (club, status, stage, a4, fixture) ret 0x18 */
{
    map_stage_arg(cpu.ecx, 2);
    o_00908200();
}

RT_WRAP(00908360, mls_fate_news)       /* (club index, fate, stage, ...) ret 0x28 */
{
    map_stage_arg(cpu.ecx, 2);
    o_00908360();
}

RT_WRAP(00909dd0, mls_club_progress)   /* (club, stage, result, position, finish) ret 0x14 */
{
    map_stage_arg(cpu.ecx, 1);
    o_00909dd0();
}

/* UsaOpenCup::collect_entrants 0x90b530, thiscall, ret 0. It sets 32 entrants (+0x3e, 0x90b544), allocates 32
 * entries of 6 bytes (0x90b613..0x90b622), fills 10 clubs of each of two lower competitions (0x90b6a3..0x90b718)
 * and then every club whose division (+0x57) is the MLS (0x90b731..0x90b767), with no limit. The class held 12, so
 * the cup had 32. With the real count of the MLS (29 from 2023) the clubs after the 12th wrote past the block, and
 * MATCH_DAY::MATCH_DAY 0x6b3c10 got a block of the zone from the broken small list (0x6b3e18, "free of 0x51000145",
 * day 80 of 2025 in run3b and run4diag of gap row 7). While the call runs, the MLS clubs after the first 12 of the
 * club table have no division, so the cup keeps its 32. ConChamp::select_qualified_clubs (0x4c1247) and
 * ConMercCup::select_invited_clubs (0x4c2714) take the first 12 in the same way. */
void o_0090b530(void);

#define OPEN_CUP_MLS 12

RT_WRAP(0090b530, mls_open_cup_entrants)
{
    uint32_t clubs = rd32(g_mem, CLUBS), key = rd32(g_mem, KEY_MLS);
    int n = (int32_t)rd32(g_mem, CLUB_COUNT), seen = 0, hidden = 0;
    static uint32_t held[MLS_MAX_CLUBS * 4], held_div[MLS_MAX_CLUBS * 4];
    for (int i = 0; readable(clubs) && i < n && hidden < MLS_MAX_CLUBS * 4; i++) {
        uint32_t club = clubs + (uint32_t)i * CLUB_SIZE, div = rd32(g_mem, club + CLUB_DIVISION);
        if (!readable(div) || rd32(g_mem, div) != key || ++seen <= OPEN_CUP_MLS) continue;
        held[hidden] = club;
        held_div[hidden++] = div;
        wr32(g_mem, club + CLUB_DIVISION, 0);
    }
    o_0090b530();
    for (int i = 0; i < hidden; i++) wr32(g_mem, held[i] + CLUB_DIVISION, held_div[i]);
    if (hidden && !rt_unwinding)
        fprintf(stderr, "recomp: mls: the US Open Cup takes the first %d of %d MLS clubs\n", OPEN_CUP_MLS, seen);
}
