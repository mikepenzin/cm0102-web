/* Japan and Korea at their real sizes (0.7.2 league sizes, slice 14, D8 and D16; spec
 * docs/specs/2026-11-13-league-sizes-0.7.2-spec.md). The table and the dates of J1, J2 and the K League come from the
 * size engine (league_sizes.py GROWN, season_sizes.c), their meetings from the row, and the K League split from the
 * split engine (season_split.c). This file holds what the classes add.
 *
 * Halves in one table. A row of two halves (Apertura and Clausura, two stages: `halves` 1) gives the meetings of each
 * half. JapJ1 plays its halves inside the one table of the season: the table holds both halves, and the halves are
 * stages with no fixtures that take each result of the table (JapJ1::add_result 0x663940, 0x66395a), so the table plays
 * twice the row's meetings. The K League rows of two stages (2004 to 2006, rows 2003 to 2005) play as one table of the
 * same matches, twice the row's meetings: the class has no stage code, and the stages and their championship are out
 * of scope (D16, D2).
 *
 * J1 halves only in their seasons (D8). The class plays two halves and a final every season: start_new_season 0x6636d0
 * and the constructor 0x662b30 set the stage count +0x2c to 3 (0x663745, 0x662ba1) and allocate 3 stage slots +0xc
 * (0x66374c, 0x662ba8) before set_up_rules (0x6637d8, 0x662bb5). The halves are made by add_result as the rounds
 * reach them (0x663968..0x663983, create_half_season_stage 0x663c20), the final by advance_stage 0x6639c0 when the
 * stage +0x30 reaches 2 (0x663a3f..0x663a44, create_championship_final_stage 0x663a50). A club that wins both halves
 * is the champion with no final (the result handler 0x663db0, stage 1: 0x663e54..0x663f13, +0x2c 2). The real J1 plays
 * halves and the final only in the 2002 to 2004 and 2015 and 2016 seasons, and one table in the others.
 *
 * So in a season of one table, after set_up_rules: the stage count goes to 0, as for Série A and B with no play-off
 * (season_sizes_bra.c). League::update_for_date 0x6847c0 then calls no advance_stage. add_result gives the result to
 * the League's add_result 0x685d30 only (the class's first call, 0x66394f), and makes no half. The destructor and
 * start_new_season read the stage slots only while +0x30 >= 0 (0x663718..0x663738); start_new_season frees +0xc with
 * no test (0x66373a), which League::League sets to 0 (0x682291) and League::load allocates only for a stage count
 * above 0 (0x688e44..0x688e5b), so a loaded game of a one-table season frees nothing. The save keeps +0x2c 0.
 * The champion. update_positions_and_status 0x684cd0 gives the top row status 5 and code 5 only with bit 1 of the rule
 * flags +0xd9 (0x684dc0); the class sets 8 (0x6636b2), and its result handler 0x663db0 drops every code of the league
 * stage (stage 0xff: 0x663e4c, 0x663fa7, 0x663fa9 to 0x664094). With one table the flags get bit 1 and lose bit 8,
 * and code 5 of the league stage goes to the base League handler 0x686940 (the title record), as Série A's.
 * The honours. The class's record_season_honours 0x664390 (vtable +0x30, called at 0x684a8f) reads stage 0 and 1 for a
 * double winner (+0x2c 2, 0x664393) and else jumps to +0x30 of stage 2, the final (0x6643f3..0x6643fb). With one
 * table the League's 0x684640 takes rows 0 and 1 of the table.
 *
 * Halves of any count. The half bound (0x66395a, 15) and the matches of a club in a half (0x663d36, 15) are half of
 * the 30 rounds and the 30 matches of 16 clubs: rt_jap_half_rounds and rt_jap_half_matches give half of the template
 * (+0xa9) and of the matches of the table ((+0x3e - 1) x +0x3c). A half is not always one round robin: the shuffled
 * walk of the dates (league_next_schedule_round 0x68a030, blocks at 0x68a0fa..0x68a13b) can put a match of the second
 * meeting before the bound, as in the original's 30 dates. Each club still plays (n - 1) x meetings / 2 matches in each
 * half, which the half stage counts (+0xa7). Both rows of halves have an even count (16, 18).
 *
 * The base game and RECOMP_LEAGUE_SIZES=off change nothing here (Q152): season_sizes_rules calls
 * season_sizes_asia_rules only with the size engine on, the meetings come only from the grown rows, the rt_ functions
 * return the original's value with the engine off, and the wraps act only on a J1 object with a stage count of 0,
 * which only season_sizes_asia_rules writes. The wraps do not test the engine: a game saved in a one-table season
 * and continued with the engine off still has stage count 0 and no final in slot 2, so it must keep the League's
 * add_result and honours (the class's 0x6643f3 would call through the empty slot), as Série A (season_sizes_bra.c). */
#include "win.h"
#include "../rt_host.h"
#include "season_change.h"
#include "season_league.h"
#include "season_sizes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMP_STAGES 0x0cu           /* the stage slots */
#define COMP_STAGE_COUNT 0x2cu      /* s32: the stages after the league stage (3: two halves and the final) */
#define COMP_STAGE_NOW 0x30u        /* s32: the stage being played, -1 for the league stage */
#define COMP_MEETINGS 0x3cu         /* s16 */
#define COMP_CLUBS 0x3eu            /* s16 */
#define COMP_YEAR 0x40u             /* s16: the edition */
#define COMP_DATES 0xa9u            /* s16: the dates of the template */
#define RULE_FLAGS 0xd9u            /* u16: bit 1 a champion row, 8 play-off rows (0x684dc0, 0x685191) */
#define FLAG_CHAMPION 0x01u
#define FLAG_PLAYOFF_ROWS 0x08u
#define F_LEAGUE_ADD_RESULT 0x00685d30u /* League::add_result, thiscall (fixture, a2); JapJ1 calls it at 0x66394f */
#define F_BASE_RESULT 0x00686940u   /* League +0x44: code 5 the title record */
#define F_LEAGUE_HONOURS 0x00684640u /* League::record_season_honours, thiscall () */
#define STATUS_CHAMPION 5
#define STAGE_LEAGUE 0xffu

static int readable(uint32_t p) { return rt_readable(p); }

static int logging(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_SIZE_LOG") && *getenv("RECOMP_SIZE_LOG");
    return on;
}

static int is_j1(uint32_t obj)
{
    const char *file = readable(obj) ? season_change_file(obj) : NULL;
    return file && !strcmp(file, "jap_j1");
}

/* season_sizes_asia_meetings: season_sizes_asia_rows.c. */

/* 1 for a J1 object of a season of one table: stage count 0 (season_sizes_asia_rules; the class's 3 or 2 else). */
static int one_table(uint32_t obj)
{
    return is_j1(obj) && rd32(g_mem, obj + COMP_STAGE_COUNT) == 0;
}

void season_sizes_asia_rules(uint32_t obj, const char *file)
{
    if (!readable(obj) || !file || strcmp(file, "jap_j1")) return;
    if ((int32_t)rd32(g_mem, obj + COMP_STAGE_NOW) >= 0) return;    /* a stage is live: set_up_rules mid-season */
    int season = season_sizes_row_season(obj, file);
    const SeasonLeagueRule *r = season < 0 ? NULL : season_league_row(file, season);
    if (!r) return;
    int year = (int16_t)rd16(g_mem, obj + COMP_YEAR);
    if (r->halves == 1) {
        /* Halves: the class's stages, as set_up_rules left them (start_new_season and the constructor set 3). */
        if (logging())
            fprintf(stderr, "recomp: league sizes: jap_j1 %d: halves and the final (row %d), %d stages\n", year,
                    r->season, (int32_t)rd32(g_mem, obj + COMP_STAGE_COUNT));
        return;
    }
    wr32(g_mem, obj + COMP_STAGE_COUNT, 0);
    uint16_t flags = (uint16_t)((rd16(g_mem, obj + RULE_FLAGS) | FLAG_CHAMPION) & ~FLAG_PLAYOFF_ROWS);
    wr16(g_mem, obj + RULE_FLAGS, flags);
    if (logging())
        fprintf(stderr, "recomp: league sizes: jap_j1 %d: one table, no halves and no final (row %d)\n", year,
                r->season);
}

uint32_t rt_jap_half_rounds(uint32_t league, uint32_t original)
{
    if (!season_sizes_on() || !is_j1(league)) return original;
    int d = (int16_t)rd16(g_mem, league + COMP_DATES);
    uint32_t half = d >= 2 && d % 2 == 0 ? (uint32_t)(d / 2) : original;
    /* The test log: once an edition, the round that begins the second half. */
    static uint32_t logged_obj;
    static int logged_year;
    int year = (int16_t)rd16(g_mem, league + COMP_YEAR);
    if (logging() && (logged_obj != league || logged_year != year)) {
        logged_obj = league;
        logged_year = year;
        fprintf(stderr, "recomp: league sizes: jap_j1 %d: second half from round %u of %d (the class's %u)\n", year,
                half + 1, d, original + 1);
    }
    return half;
}

uint32_t rt_jap_half_matches(uint32_t league, uint32_t original)
{
    if (!season_sizes_on() || !is_j1(league)) return original;
    int n = (int16_t)rd16(g_mem, league + COMP_CLUBS), m = (int16_t)rd16(g_mem, league + COMP_MEETINGS);
    int matches = (n - 1) * m;
    if (n < 2 || m < 2 || matches % 2) return original;
    if (logging() && (uint32_t)(matches / 2) != original)
        fprintf(stderr, "recomp: league sizes: jap_j1 %d: half of %d clubs, %d matches a club (the class makes %u)\n",
                (int16_t)rd16(g_mem, league + COMP_YEAR), n, matches / 2, original);
    return (uint32_t)(matches / 2);
}

/* JapJ1::add_result (thiscall: fixture, a2; ret 8): with one table only the League's add_result, as the class's first
 * call (0x66394f), and the class's return value 1 (0x6639ab). */
void o_00663940(void);
RT_WRAP(00663940, j1_add_result)
{
    if (!one_table(cpu.ecx)) {
        o_00663940();
        return;
    }
    uint32_t a[2] = {rt_arg(0), rt_arg(1)};
    Cpu saved = cpu;
    rt_callback(F_LEAGUE_ADD_RESULT, 2, a);
    if (rt_unwinding) return;
    cpu = saved;
    rt_ret_stdcall(1, 2);
}

/* JapJ1::update_staff_records_for_status (vtable +0x44, thiscall: club, code, stage, a4, a5, a6; ret 0x18): with one
 * table, code 5 of the league stage goes to the base League handler. */
void o_00663db0(void);
RT_WRAP(00663db0, j1_result)
{
    uint32_t self = cpu.ecx;
    if ((rt_arg(1) & 0xff) != STATUS_CHAMPION || (rt_arg(2) & 0xff) != STAGE_LEAGUE || !one_table(self)) {
        /* The test log: a club that wins both halves is the champion with no final (stage 1, +0x2c 2 at 0x663f13). */
        int was = (int32_t)rd32(g_mem, self + COMP_STAGE_COUNT), stage = rt_arg(2) & 0xff;
        o_00663db0();
        if (!rt_unwinding && logging() && is_j1(self) && stage == 1 && was != 2 &&
            (int32_t)rd32(g_mem, self + COMP_STAGE_COUNT) == 2)
            fprintf(stderr, "recomp: league sizes: jap_j1 %d: double winner, no final\n",
                    (int16_t)rd16(g_mem, self + COMP_YEAR));
        return;
    }
    uint32_t a[6];
    for (int i = 0; i < 6; i++) a[i] = rt_arg(i);
    Cpu saved = cpu;
    rt_callback(F_BASE_RESULT, 6, a);
    if (rt_unwinding) return;
    cpu = saved;
    if (logging())
        fprintf(stderr, "recomp: league sizes: jap_j1 %d: champion club %.40s (the top of the table)\n",
                (int16_t)rd16(g_mem, self + COMP_YEAR), readable(a[0]) ? win_str(a[0] + 4) : "?");
    rt_ret_stdcall(0, 6);
}

/* JapJ1::record_season_honours (vtable +0x30): with one table, the League's. */
void o_00664390(void);
RT_WRAP(00664390, j1_honours)
{
    if (one_table(cpu.ecx)) {
        rt_jump(F_LEAGUE_HONOURS);
        return;
    }
    o_00664390();
}

/* JapJ1::create_championship_final_stage (thiscall ()): for the test log, the final of a season of halves. */
void o_00663a50(void);
RT_WRAP(00663a50, j1_final)
{
    uint32_t self = cpu.ecx;
    o_00663a50();
    if (rt_unwinding || !logging() || !season_sizes_on() || !is_j1(self)) return;
    uint32_t slots = rd32(g_mem, self + COMP_STAGES);
    fprintf(stderr, "recomp: league sizes: jap_j1 %d: final of the halves%s\n", (int16_t)rd16(g_mem, self + COMP_YEAR),
            readable(slots) && readable(rd32(g_mem, slots + 8)) ? "" : " not made");
}
