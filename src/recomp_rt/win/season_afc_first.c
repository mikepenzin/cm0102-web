/* The first round of AFC World Cup qualifying (item 196 of 0.6.2). Changing rules on only (season_intl_on): the
 * base game and a game with changing rules off do not change (Q152).
 *
 * The original (wc_asia_league, class WcAsiaLeague, vtable 0x970b70). The pool builder 0x913f60 reads the Asian
 * nations (continent 0x9cfa0c) in nation-table order, leaves out the hosts of find_hosts and the holder, and stops
 * after 44 less the exempt count +0xee (0x914166); it sorts them by reputation (0x4b07d0, 0x9141cf) and caps the
 * count +0xf3 at 40 (0x9141dd). A nation after the 44th is never read: Brunei and the others at the end of the
 * nation table never play. The ctor 0x911bf0 sets +0xd9 = 40, +0x30 = -1 and 13 stages (+0x2c, 0x911cf6).
 * update_for_date 0x913140 draws the groups on the draw day (+0xb6 day of year +0x40 + +0xb8: 25 March of the year
 * after +0x40, set_up_rules 0x912eb0) when +0x45 is 0 and the update is full (0x9131eb..0x91320a): the draw
 * 0x912bd0 deals the list +0xef in its order into 10 shuffled groups, pot by pot (0x912d02..0x912d9b), the object
 * itself is group A (0x912ad0) and stages 0 to 8 the others (0x913540 writes slot +0xc[g], +0x30 = g); then +0x45 =
 * 1. Before it League::update_for_date 0x6847c0 does nothing (+0x45 0, 0x684811).
 *
 * The change. From 1 August of the campaign year c (+0x40) to the draw day, on the first update day while +0x45
 * is 0 and +0x30 is -1 (the pool of the cycle is made, no round yet; the slots themselves tell nothing: the ctor's
 * stage array comes from malloc uncleared, 0x911d00, and start_new_season frees the stages but keeps their
 * pointers, 0x9133ab..0x9133c1): every Asian nation whose national team names
 * it, less the hosts of the campaign's World Cup (season_intl_host_ids, as the pool wraps of season_wcq26.c; the
 * holder is off in a season game, rt_intl_wc_holder), sorted by the original's compare. P of them, G group places
 * (40 less +0xee, the group sizes of 0x912db0), W = P - G. When W > 0 the bottom 2W play W two-leg ties
 * (season_afc_first_plan) as a Cup_Stage (0x522e00) in slot 0, +0x30 = 0; the list +0xef becomes the G - W byes
 * and, for each tie, its seed as a stand-in. The round is made then, not at the pool, so a game made in 2001/02
 * keeps the base game's sections at the new game (a pool built at the new game would put the round's fixtures of
 * October 2004 into the save of the first day). The days (season_afc_first_days): a FIFA window from 1 October of c
 * where no team of the round has another match within 2 days of a leg (window_free; the store would move one of
 * the two, resolve_same_day_clash 0x5ae380), else such a window from August, else the first window.
 *
 * While slot 0 holds the ties (+0x45 0, +0x30 0, a Cup_Stage, vtable 0x969468) the wrap of update_for_date calls
 * the stage's update (vtable +0x14, as League::update_for_date does at 0x6848f8 once +0x45 is 1), and advance_stage
 * 0x913490 waits. On the draw day, before the original draws, each tie's winner (status 1 in the stage's rows)
 * replaces its stand-in in the list (the last places: pot 4), the stage is freed (vtable +0 with 1, as
 * start_new_season 0x9133b7), slot 0 is cleared and +0x30 is -1, and the original draws.
 *
 * Save and load. League save 0x6891c0 writes stages 0 to +0x30 (a Cup_Stage as type 1) and League load 0x687b10
 * reads them back (0x688ec7, Cup_Stage 0x5230e0), so the ties come back with +0x30 = 0. save_extra_data 0x914410
 * and load_extra_data 0x914200 write and read the list +0xef with its count +0xf3: the stand-ins are teams, so the
 * save keeps the original's shape. A save made before this change has no tie and a list of 40. Loaded before the
 * draw of its cycle with a FIFA window left, it gets the round in that cycle (+0x45 0 and +0x30 -1 are the state of
 * a cycle with no round yet); else it plays that cycle as before and the next cycle gets the round.
 *
 * The rest of the class with a stage 0 that is a tie. The Cup_Stage passes its results to the section's status
 * 0x912500 (Cup 0x519690 at 0x51970c) with stage 0: stage < 9, a winner (status 1) gets nothing, a loser (0xfe)
 * the record of a first-round loser (0x86c1d0 with 0x1e, 0xa) and its competition cleared (0x775000), as a team
 * out of a first-round group. Its positions go through 0x48e380 to set_club_progress 0x914730, which writes the
 * Comp_Stats entry that find_club_entry 0x4a4850 gives: the season's entries are made only at the draw
 * (set_predicted_positions 0x9144d0, 0x4a2540 at 0x9146eb) after start_new_season resets them (0x4a1c50 at
 * 0x9133d2), so the round makes the entries of its 2W teams (0x4a2540 with the weakest band 0x1f, as 0x9146d9) and
 * names the section as their current competition (0x775220, as 0x91470b). A Cup_Stage records no history row
 * (Cup::update_for_date 0x51f890 does the season end only at +0x44 = 0xff, 0x51f8c3; the stage's +0x44 is 0).
 * 0x9144d0 predicts places over the 40 of +0xf3 on the draw day, after the winners are in. */
#include "win.h"
#include "season.h"
#include "season_intl.h"
#include "season_afc_first.h"
#include "season_uefa_days.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VT_ASIA 0x00970b70u
#define VT_CUP_STAGE 0x00969468u
#define KEY_WORLD_CUP 0x9cf79cu
#define CONTINENT_ASIA 0x9cfa0cu
#define NATIONS 0xae23a8u
#define NATION_COUNT 0xae2350u
#define NATION_SIZE 0x122u
#define GAME_DATE 0xae2c90u             /* s16 day of the year (0 is 1 January), s16 year */
#define NATIONAL_TEAMS 0xb74318u        /* the NationalTeams object (0x91470b) */

#define GAME_NEW 0x00944cf1u
#define GAME_MALLOC 0x00944e46u
#define GAME_FREE 0x009452cau
#define GAME_QSORT 0x00945333u
#define CLUB_COMPARE 0x004b07d0u        /* compare_clubs_by_reputation_desc_nulls_last: club +0x80 */
#define CUP_STAGE_NEW 0x00522e00u
#define CUP_START 0x0051c800u           /* Cup::set_up_round */
#define CUP_DRAW_DATE 0x00521e60u       /* cdecl (rec, round, day, month0, yoff, weekday, year) */
#define CUP_MATCH_DATE 0x00521eb0u      /* cdecl (rec, round, day, month0, yoff, weekday, time, year, venue) */
#define NATIONAL_TEAM 0x0053b3d0u
#define STATS_SET_POSITION 0x004a2540u  /* Comp_Stats::set_club_final_position (club, byte) */
#define SET_CURRENT_COMP 0x00775220u    /* NationalTeams::set_current_competition (nation, comp record) */

#define F_RECORD 0x4u
#define F_STATS 0x8u
#define F_STAGES 0xcu
#define F_STAGE 0x30u
#define F_TEAMS 0x3eu
#define F_YEAR 0x40u
#define F_DRAWN 0x45u
#define F_DRAW_DAY 0xb6u
#define F_DRAW_YEAR 0xb8u
#define F_EXEMPT 0xeeu
#define F_LIST 0xefu
#define F_LIST_COUNT 0xf3u
#define F_CUP_ROWS 0xa7u
#define ROUND_SIZE 0x68u
#define GROUP_PLACES 40                 /* +0xd9 (0x911cdd): 10 groups of 4 */
#define ROUND_CODE 0xau                 /* a first round: the record's round id of the African first round
                                           (0x90f6ac) and the level of the first-round group codes (0x4b021c) */

static int logging(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_SEASON_INTL_LOG");
        on = v && *v == '1';
    }
    return on;
}

static int readable(uint32_t p) { return rt_readable(p); }

static uint32_t call_cdecl(uint32_t fn, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t call_this(uint32_t fn, uint32_t self, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t game_malloc(uint32_t size)
{
    uint32_t a[1] = {size};
    uint32_t p = call_cdecl(GAME_MALLOC, 1, a);
    if (readable(p)) memset(g_mem + p, 0, size);
    return readable(p) ? p : 0;
}

static void game_free(uint32_t p)
{
    uint32_t a[1] = {p};
    if (p) call_cdecl(GAME_FREE, 1, a);
}

static int year_of(uint32_t self) { return (int16_t)rd16(g_mem, self + F_YEAR); }

static const char *team_name(uint32_t club)
{
    uint32_t nation = readable(club) ? rd32(g_mem, club + 0x53u) : 0;
    return readable(nation) ? win_str(nation + 4u) : "none";
}

/* The day of `date` (a Date: s16 day of the year, s16 year, as GAME_DATE) as an absolute day. update_for_date 0x913140
 * compares its Date argument (0x91315f, 0x9131ad..0x9131f4), not the game date, so the wrap passes that argument. */
static long today_abs(uint32_t date)
{
    return season_abs_day((int16_t)rd16(g_mem, date + 2), 1, 1) + (int16_t)rd16(g_mem, date);
}

static int on(uint32_t self)
{
    return season_intl_on() && readable(self) && rd32(g_mem, self) == VT_ASIA && year_of(self) + 2 >= 2006;
}

static uint32_t slot0(uint32_t self)
{
    uint32_t stages = rd32(g_mem, self + F_STAGES);
    if (!readable(stages) || (int)rd32(g_mem, self + 0x2cu) < 1) return 0;
    uint32_t s = rd32(g_mem, stages);
    return readable(s) ? s : 0;
}

/* 1 while slot 0 holds the ties: before the draw (+0x45 0), +0x30 0 and a Cup_Stage in slot 0. */
static int ties_live(uint32_t self)
{
    if (!on(self) || rd32(g_mem, self + F_DRAWN) != 0 || (int32_t)rd32(g_mem, self + F_STAGE) != 0) return 0;
    uint32_t s = slot0(self);
    return s && rd32(g_mem, s) == VT_CUP_STAGE;
}

static int group_places(uint32_t self)
{
    int ee = (int8_t)rd8(g_mem, self + F_EXEMPT);
    return ee >= 0 && ee <= 3 ? GROUP_PLACES - ee : -1;      /* 0x912db0 knows 0 to 3 (0x912dbd) */
}

/* The Asian nations that can enter, sorted best first by the original's compare, into out (max). out NULL: the
 * count only, with no allocation (a day of a cycle with no round changes no memory). */
static int asian_pool(uint32_t self, uint32_t *out, int max)
{
    uint32_t nations = rd32(g_mem, NATIONS);
    int count = (int)rd32(g_mem, NATION_COUNT), hosts[6], n = 0;
    if (!readable(nations) || count <= 0 || count > 1000) return 0;
    int n_hosts = season_intl_host_ids(rd32(g_mem, KEY_WORLD_CUP), year_of(self) + 2, hosts);
    for (int id = 0; id < count && n < max; id++) {
        uint32_t nation = nations + (uint32_t)id * NATION_SIZE, c = rd32(g_mem, nation + 0x71u);
        if (!readable(c) || rd32(g_mem, c) != rd32(g_mem, CONTINENT_ASIA)) continue;
        int host = 0;
        for (int k = 0; k < n_hosts; k++) host |= hosts[k] == id;
        if (host) continue;
        uint32_t a[1] = {nation};
        uint32_t team = call_cdecl(NATIONAL_TEAM, 1, a);
        if (rt_unwinding) return 0;
        if (!readable(team) || rd32(g_mem, team + 0x53u) != nation) continue;
        if (out) out[n] = team;
        n++;
    }
    if (!out) return n;
    uint32_t arr = game_malloc((uint32_t)(n ? n : 1) * 4u);
    if (rt_unwinding || !arr) return 0;
    for (int i = 0; i < n; i++) wr32(g_mem, arr + 4u * (uint32_t)i, out[i]);
    uint32_t q[4] = {arr, (uint32_t)n, 4, CLUB_COMPARE};
    call_cdecl(GAME_QSORT, 4, q);
    if (rt_unwinding) return 0;
    for (int i = 0; i < n; i++) out[i] = rd32(g_mem, arr + 4u * (uint32_t)i);
    game_free(arr);
    return n;
}

/* The round record: one two-leg round of 2W teams in the list's pairs, the fields of the class's own two-leg tie
 * (stage 12, 0x9124ac..0x9124ed: +0x0f 3, +0x17 0xa, +0x21 2), with bit 0x200 that keeps the list's pairs (as
 * season_nl_euro.c tie_round) and the African first round's round id 0xa (0x90f6ac). */
static uint32_t round_record(int c, int teams, long draw, long leg1, long leg2)
{
    uint32_t rec = game_malloc(ROUND_SIZE);
    if (rt_unwinding || !rec) return 0;
    int y, m, d;
    season_from_abs_day(draw, &y, &m, &d);
    uint32_t a[7] = {rec, 0, (uint32_t)d, (uint32_t)(m - 1), (uint32_t)(y - c), 0xffffffffu, (uint32_t)c};
    call_cdecl(CUP_DRAW_DATE, 7, a);
    if (rt_unwinding) return 0;
    season_from_abs_day(leg1, &y, &m, &d);
    uint32_t b[9] = {rec, 0, (uint32_t)d, (uint32_t)(m - 1), (uint32_t)(y - c), 0xffffffffu, 1, (uint32_t)c, 0};
    call_cdecl(CUP_MATCH_DATE, 9, b);
    if (rt_unwinding) return 0;
    wr16(g_mem, rec + 7u, ROUND_CODE);
    wr16(g_mem, rec + 9u, 0);
    wr16(g_mem, rec + 0xbu, 0);
    wr16(g_mem, rec + 0xdu, 0x200u);
    wr16(g_mem, rec + 0xfu, 3u);
    wr8(g_mem, rec + 0x17u, 0xa);
    wr16(g_mem, rec + 0x18u, (uint32_t)teams);
    wr16(g_mem, rec + 0x1au, (uint32_t)teams / 2u);
    wr16(g_mem, rec + 0x1cu, (uint32_t)teams);
    wr16(g_mem, rec + 0x1eu, 0);
    wr8(g_mem, rec + 0x20u, 0);
    wr8(g_mem, rec + 0x21u, 2);
    wr8(g_mem, rec + 0x22u, (uint32_t)(leg2 - leg1));
    return rec;
}

/* The day the groups are drawn (+0xb6 day of the year, +0x40 + +0xb8 year: set_up_rules 0x912eb0 sets 25 March of
 * the year after +0x40), as days from 1 January 1970. update_for_date 0x913140 draws on that day only. */
static long groups_draw_day(uint32_t self)
{
    return season_abs_day(year_of(self) + (int16_t)rd16(g_mem, self + F_DRAW_YEAR), 1, 1) +
           (int16_t)rd16(g_mem, self + F_DRAW_DAY);
}

int season_fifa_team_busy_near(uint32_t team, int y, int doy, int days);    /* season_friendly_dates.c */

typedef struct {
    const uint32_t *pool;
    const SeasonAfcFirstPlan *plan;
    int busy;
} FreeCtx;

static int team_near(uint32_t team, long z)
{
    int y, m, d;
    season_from_abs_day(z, &y, &m, &d);
    return season_fifa_team_busy_near(team, y, season_day_of_year(y, m, d), 2);
}

/* 1 when no team of the round has a match within 2 days of either leg (CLASH_DAYS of season_friendly_dates.c). */
static int window_free(long leg1, long leg2, void *ctx)
{
    FreeCtx *fc = ctx;
    for (int k = 0; k < fc->plan->ties && !rt_unwinding; k++)
        for (int s = 0; s < 2; s++) {
            uint32_t team = fc->pool[fc->plan->pair[k][s]];
            if (team_near(team, leg1) || team_near(team, leg2)) {
                fc->busy++;
                return 0;
            }
        }
    return !rt_unwinding;
}

/* Makes the round of the cycle (head comment). Returns 1 when slot 0 holds the ties. */
static int build(uint32_t self, uint32_t date)
{
    static int logged_year;
    int c = year_of(self), places = group_places(self);
    long today = today_abs(date), draw, leg1, leg2;
    uint32_t pool[AFC_FIRST_MAX_POOL];
    SeasonAfcFirstPlan plan;
    if (places <= 0) return 0;
    /* The window and the count first: neither allocates, so a cycle with no round (each day from the build day to
     * the draw tests it again; no state outside the object, so a load or a new game in the same run starts clean)
     * changes no memory of the game. */
    int dated = season_afc_first_days(c, today, groups_draw_day(self), NULL, NULL, &draw, &leg1, &leg2);
    int p = asian_pool(self, NULL, AFC_FIRST_MAX_POOL);
    if (rt_unwinding) return 0;
    int w = season_afc_first_plan(p, places, &plan);
    if (dated && w > 0) {
        p = asian_pool(self, pool, AFC_FIRST_MAX_POOL);
        if (rt_unwinding) return 0;
        w = season_afc_first_plan(p, places, &plan);
        /* The window: the first where no team of the round has another match within 2 days of a leg. */
        FreeCtx fc = {pool, &plan, 0};
        dated = season_afc_first_days(c, today, groups_draw_day(self), window_free, &fc, &draw, &leg1, &leg2);
        if (rt_unwinding) return 0;
    }
    if (!dated || w <= 0) {
        if (logging() && logged_year != c) {
            logged_year = c;
            fprintf(stderr, "recomp: season intl: wc qualifying %d: AFC pool of %d for %d places: %s\n", c + 2, p,
                    places, w > 0 ? "no FIFA window before the draw, no first round" : "no first round");
        }
        return 0;
    }
    uint32_t list = game_malloc((uint32_t)places * 4u), teams = game_malloc((uint32_t)(2 * w) * 4u);
    if (rt_unwinding) return 0;
    if (!list || !teams) {
        game_free(list);
        game_free(teams);
        return 0;
    }
    for (int i = 0; i < places; i++) wr32(g_mem, list + 4u * (uint32_t)i, pool[plan.list[i]]);
    for (int k = 0; k < w; k++) {
        wr32(g_mem, teams + 8u * (uint32_t)k, pool[plan.pair[k][0]]);
        wr32(g_mem, teams + 8u * (uint32_t)k + 4u, pool[plan.pair[k][1]]);
    }
    uint32_t rec = round_record(c, 2 * w, draw, leg1, leg2), cup = 0;
    if (rt_unwinding) return 0;
    uint32_t na[1] = {0xb2};
    if (rec) cup = call_cdecl(GAME_NEW, 1, na);
    if (rt_unwinding) return 0;
    if (readable(cup)) {
        /* As season_wcq26.c build_round: (parent, teams, list, rounds, record, template, year, stage, 4, code,
         * 0x14, 1, 0, 0, 0). */
        uint32_t ca[15] = {self, (uint32_t)(2 * w), teams, 1, rd32(g_mem, self + F_RECORD), rec, (uint32_t)c, 0, 4,
                           ROUND_CODE, 0x14, 1, 0, 0, 0};
        cup = call_this(CUP_STAGE_NEW, cup, 15, ca);
        if (rt_unwinding) return 0;
    } else {
        cup = 0;
    }
    game_free(teams);
    game_free(rec);
    if (!cup) {
        game_free(list);
        return 0;
    }
    game_free(rd32(g_mem, self + F_LIST));
    wr32(g_mem, self + F_LIST, list);
    wr32(g_mem, self + F_LIST_COUNT, (uint32_t)places);
    wr32(g_mem, rd32(g_mem, self + F_STAGES), cup);
    wr32(g_mem, self + F_STAGE, 0);
    uint32_t stats = rd32(g_mem, self + F_STATS), nt = rd32(g_mem, NATIONAL_TEAMS);
    for (int i = 0; i < 2 * w; i++) {
        uint32_t team = pool[plan.pair[i / 2][i % 2]];
        if (readable(stats)) {
            uint32_t sa[2] = {team, 0x1f};
            call_this(STATS_SET_POSITION, stats, 2, sa);
            if (rt_unwinding) return 0;
        }
        if (readable(nt)) {
            uint32_t ta[2] = {rd32(g_mem, team + 0x53u), rd32(g_mem, self + F_RECORD)};
            call_this(SET_CURRENT_COMP, nt, 2, ta);
            if (rt_unwinding) return 0;
        }
    }
    uint32_t z[1] = {0};
    call_this(CUP_START, cup, 1, z);
    if (rt_unwinding) return 0;
    if (logging()) {
        int y1, m1, d1, y2, m2, d2;
        season_from_abs_day(leg1, &y1, &m1, &d1);
        season_from_abs_day(leg2, &y2, &m2, &d2);
        fprintf(stderr, "recomp: season intl: wc qualifying %d: AFC first round of %d ties (pool %d, %d places), "
                        "%04d-%02d-%02d and %04d-%02d-%02d:", c + 2, w, p, places, y1, m1, d1, y2, m2, d2);
        for (int k = 0; k < w; k++)
            fprintf(stderr, " %s v %s;", team_name(pool[plan.pair[k][0]]), team_name(pool[plan.pair[k][1]]));
        fputc('\n', stderr);
    }
    return 1;
}

/* The winner of tie k of the stage (rows 2k and 2k + 1, status +5 = 1), as season_wcq26.c tie_winner. */
static uint32_t tie_winner(uint32_t cup, int k)
{
    uint32_t rows = rd32(g_mem, cup + F_CUP_ROWS);
    for (int i = 2 * k; readable(rows) && i < 2 * k + 2; i++)
        if (rd8(g_mem, rows + (uint32_t)i * 6u + 5u) == 1u) return rd32(g_mem, rows + (uint32_t)i * 6u);
    return 0;
}

/* The draw day: the winners into the list, the stage freed, slot 0 cleared, +0x30 = -1. The second legs end 15 days
 * before the draw at the latest (season_afc_first_days, ctest season_afc_first), so every tie is decided. A tie that
 * is not (a leg moved past the draw) keeps its seed in the list, and the stage is not freed: its fixtures still name
 * it, so it stays allocated (logged) while the original's draw writes slot 0. */
static void finish(uint32_t self)
{
    uint32_t cup = slot0(self), rows = rd32(g_mem, cup + F_CUP_ROWS), list = rd32(g_mem, self + F_LIST);
    int n = (int16_t)rd16(g_mem, cup + F_TEAMS), count = (int)rd32(g_mem, self + F_LIST_COUNT), open = 0;
    for (int k = 0; readable(rows) && readable(list) && k < n / 2; k++) {
        uint32_t a = rd32(g_mem, rows + 12u * (uint32_t)k), b = rd32(g_mem, rows + 12u * (uint32_t)k + 6u);
        uint32_t win = tie_winner(cup, k);
        int at = -1;
        for (int i = count - 1; i >= 0 && at < 0; i--) {
            uint32_t t = rd32(g_mem, list + 4u * (uint32_t)i);
            if (t == a || t == b) at = i;
        }
        if (at < 0) continue;
        if (!win) {
            open++;
            fprintf(stderr, "recomp: season intl: wc qualifying %d: AFC first round tie %s v %s not decided on the "
                            "draw day, %s goes on\n", year_of(self) + 2, team_name(a), team_name(b),
                    team_name(rd32(g_mem, list + 4u * (uint32_t)at)));
            continue;
        }
        wr32(g_mem, list + 4u * (uint32_t)at, win);
        if (logging())
            fprintf(stderr, "recomp: season intl: wc qualifying %d: AFC first round: %s goes to the groups\n",
                    year_of(self) + 2, team_name(win));
    }
    if (!open) {
        uint32_t one[1] = {1};
        call_this(rd32(g_mem, rd32(g_mem, cup)), cup, 1, one);     /* scalar deleting dtor, as 0x9133b7 */
        if (rt_unwinding) return;
    } else {
        fprintf(stderr, "recomp: season intl: wc qualifying %d: AFC first round stage kept (%d ties open)\n",
                year_of(self) + 2, open);
    }
    wr32(g_mem, rd32(g_mem, self + F_STAGES), 0);
    wr32(g_mem, self + F_STAGE, 0xffffffffu);
}

static int draw_day(uint32_t self, uint32_t date)
{
    return (int16_t)rd16(g_mem, date) == (int16_t)rd16(g_mem, self + F_DRAW_DAY) &&
           (int16_t)rd16(g_mem, date + 2) == year_of(self) + (int16_t)rd16(g_mem, self + F_DRAW_YEAR);
}

void o_00913140(void);

/* update_for_date 0x913140 (vtable +0x14), thiscall (Date *today, int full), ret 8. */
RT_WRAP(00913140, afc_first_update)
{
    uint32_t self = cpu.ecx, today = rt_arg(0), full = rt_arg(1);
    if (on(self) && rd32(g_mem, self + F_DRAWN) == 0) {
        long now = today_abs(today);
        if ((int32_t)rd32(g_mem, self + F_STAGE) == -1 && now >= season_afc_first_build_day(year_of(self)) &&
            now < groups_draw_day(self)) {
            build(self, today);
            if (rt_unwinding) return;
        }
        if (ties_live(self)) {
            if (full && draw_day(self, today)) {
                finish(self);
            } else {
                uint32_t cup = slot0(self), a[2] = {today, full};
                call_this(rd32(g_mem, rd32(g_mem, cup) + 0x14u), cup, 2, a);
            }
            if (rt_unwinding) return;
            cpu.ecx = self;
        }
    }
    o_00913140();
}

void o_00913490(void);

/* advance_stage 0x913490 (vtable +0x28), thiscall (), ret 0: no step while slot 0 holds the ties (the step would
 * take +0x30 to 1 and build nothing, 0x91350b..0x91351a). */
RT_WRAP(00913490, afc_first_advance)
{
    if (ties_live(cpu.ecx)) {
        rt_ret_stdcall(0, 0);
        return;
    }
    o_00913490();
}
