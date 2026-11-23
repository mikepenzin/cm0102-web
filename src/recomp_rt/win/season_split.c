/* The split engine (season_split.h; 0.7.2 league sizes, slice 7, D7).
 *
 * How the original splits the Scottish Premier, and what this file takes from it:
 * - ScoPrm::update_for_date 0x7f2d20 (vtable +0x14) checks, before League::update_for_date 0x6847c0, that the
 *   meetings (+0x3c) are 3 (0x7f2d2d) and that every row has played 33 matches (s8 row +0x11, 0x7f2d56), then calls
 *   create_split_stages 0x7f2d90. Here: the wrap of 0x6847c0, for any league with a split row, after N matches, and
 *   after the original's update of the day (see the wrap).
 * - create_split_stages takes the clubs (row +0) of rows 0..5 and 11..6 (0x7f2e36..0x7f2e4e, 0x7f2f04..0x7f2f2d),
 *   gets a date template from create_schedule (vtable +0x3c, 0x7f2e63, 0x7f2f41) and makes a League_Stage of 6
 *   clubs and 1 meeting with it (0x68b100 at 0x7f2ed7, 22 arguments: parent, count, clubs, meetings, record +4,
 *   template, its dates, the bytes +0xc2..+0xc4, &+0xc5, &+0xbe, the edition +0x40, -1, the name code, 0x14, 1, 0,
 *   3, -1, 0, 2). It deletes each stage at once (vtable +0, 0x7f2ef5, 0x7f2fd3) and frees the template: the
 *   fixtures stay with the parent, whose one table takes the results, so the points carry over. Then +0x3c = 4
 *   (0x7f2feb) and the table order +0xa3 = 0x7f3220 (0x7f2ff1). Here: the same stage for each section of the row,
 *   with the section's meetings and the dates after the table's rounds (season_sizes_split_dates: the wrap of the
 *   class's create_schedule plans the whole season, D4 and D5, and gives the rounds after the table's).
 * - The order 0x7f3220 (cdecl, two rows) puts a row whose position after match 33 (the byte at index 0x20 of the row's
 *   position array, row +9) is 6 or more below one whose position is less (0x7f3227..0x7f324a). The sort of the
 *   table (0x6827d0) calls +0xa3 first through its qsort callback 0x683210 (0x682a9a; 0x683248..0x683255) or
 *   0x683410 (0x68289d; 0x683438..0x683454), each twice, with the league in [0xb679fc] (0x68281e), and compares the
 *   points only when it gives 0. The display copy of a league (League::copy_for_display 0x682380) copies the whole
 *   object first (0xec bytes from +0, 0x68239c), so it holds the same vtable, edition, counts and +0xa3. Here: a host function that gives the section of each row by its
 *   position after match N, the position bounds of the row's sizes.
 * - +0xa7 (the matches of each club, the size of each row's position array, init_table_row 0x687570) is the
 *   matches before the split and those of the largest section (ScoPrm: 38 at 0x7f29d5). League::check_complete
 *   0x689ad0 wants +0xa7 of every row (0x689b47..0x689b58); with sections that play different matches
 *   rt_league_played_target wants each row's own.
 * - The save keeps +0x3c, +0xa7, the rows and their position arrays (League::save_to_temp_file 0x6891c0), and the
 *   fixtures. +0xa3 is not saved (ScoPrm's load sets it at 0x7f0bf1..0x7f0c08): season_split_loaded sets it.
 *
 * - The best and worst places of each row (update_positions_and_status 0x684cd0) follow the sections after the split:
 *   rt_split_bounds below (gap row 24 of 0.7.2).
 *
 * Known limits (D2, the nearest form). A table match put back past the first section date delays the split; the
 * section dates then move on together, so that the first is the day after the split (split, slice 11).
 *
 * The state of a league is in the save: before the split +0x3c holds the row's meetings, after it the marker
 * (meetings + the most section meetings), with +0xa7 the matches of the season. A league whose object does not hold
 * the engine's count, template (+0xa9) and +0xa7 (a 0.7.1 save before its next season change, D18) plays no
 * split. */
#include "season_sizes_arg.h"
#include "win.h"
#include "../rt_host.h"
#include "season_bel_playoffs.h"
#include "season_change.h"
#include "season_league.h"
#include "season_sizes.h"
#include "season_split.h"
#include "season_template.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMP_RECORD 4u              /* object +4: the competition record */
#define COMP_MEETINGS 0x3cu         /* s16 */
#define COMP_CLUBS 0x3eu            /* s16 */
#define COMP_YEAR 0x40u             /* s16: the edition */
#define TODAY 0x00ae2c90u            /* the game date: s16 day of the year (0 = 1 January), +2 s16 year */
#define ORDER_GLOBAL 0x00b67a00u     /* the order that the row comparators call (League::sort_table 0x682884) */
#define COMP_ORDER 0xa3u            /* the table order function, 0 for the points (0x683410) */
#define COMP_MATCHES 0xa7u          /* s16: matches per club */
#define COMP_DATES 0xa9u            /* s16: the dates of the template */
#define COMP_TABLE 0xb1u            /* the table: rows of 0x3b bytes */
#define ROW_SIZE 0x3bu
#define ROW_CLUB 0u
#define ROW_POSITIONS 9u            /* the row's position after each match, a byte each (0x7f3227) */
#define ROW_PLAYED 0x11u            /* s8: matches played (0x7f2d56, 0x689b51) */
#define ROW_POINTS 0x19u            /* s16: points of the overall table (comparator 0x683466) */
#define ROW_HOME_POINTS 0x27u       /* s16: points of the home table (0xe bytes a table) */
#define ROW_AWAY_POINTS 0x35u       /* s16: points of the away table */
#define DATE_SIZE 0x41u
#define GAME_DATE 0xae2c90u         /* s16 day of the year, s16 year (testrun.c) */
#define SORTED_LEAGUE 0xb679fcu     /* the league that the table sort sorts (0x68281e) */
#define F_NEW 0x00944cf1u           /* operator new, cdecl (size) (0x7f2e6d) */
#define F_MALLOC 0x00944e46u
#define F_FREE 0x009452cau
#define F_LEAGUE_STAGE 0x0068b100u  /* League_Stage::League_Stage, thiscall, 22 arguments (ret 0x58) */
#define STAGE_SIZE 0xeeu
#define MAX_CLUBS 60

void testrun_save_in(int days);    /* testrun.c */

static int readable(uint32_t p) { return rt_readable(p); }

static int logging(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_SIZE_LOG") && *getenv("RECOMP_SIZE_LOG");
    return on;
}

static int grown(const char *file)
{
    for (int k = 0; file && k < season_grown_count; k++)
        if (!strcmp(season_grown[k].file, file)) return 1;
    return 0;
}

static const char *record_name(uint32_t obj)
{
    uint32_t rec = rd32(g_mem, obj + COMP_RECORD);
    return readable(rec) ? win_str(rec + 4) : "?";
}

/* Gap 35: splits whose later sections are groups of places that are not next to each other, by season (the rules
 * table gives the section sizes and meetings only). {file, first season, last season, count, the section of each
 * place after match N}. The Superliga 2016/17 to 2019/20: the top 6 in the championship round, then two
 * qualification groups of the 7th, 10th, 11th, 14th and of the 8th, 9th, 12th, 13th, each a double round robin, the
 * points kept (Wikipedia 2016-17 to 2019-20_Danish_Superliga, checked 15 November 2026). The play-offs after the
 * groups (Europe and relegation, with First Division clubs) are not played (D2: the nearest form). The table keeps
 * section 2 above section 3. tests/seasons/league_size_run.py GROUPS holds the same. */
/* Gap 34: the Pro League 2009/10 to 2018/19 (16 clubs): Play-off I of the top 6 (points halved), Play-off II from 0
 * points in two groups, the 7th, 9th, 12th, 14th and the 8th, 10th, 11th, 13th (2016/17 to 2018/19: the 15th in the
 * second group), and the 15th and 16th: the relegation play-off 2010/11 to 2014/15 (five matches, the 15th starting
 * with 3 points), else no more matches (meetings 0 in the row). en.wikipedia.org 2009-10 to 2015-16_Belgian_Pro_League,
 * 2016-17 to 2018-19_Belgian_First_Division_A, RSSSF belg2010 to belg2019, checked 15 November 2026. The finals after
 * the groups (Play-off II final, the Europa League match, the Second Division final round, First Division B clubs in
 * Play-off II) are not played (D2: the nearest form). points: each section's points at the split ("" for the row's). */
static const struct {
    const char *file;
    int first, last, n;
    signed char sec[16];
    const char *points;
} GROUPS[] = {
    {"den_prm", 2016, 2019, 14, {0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 1, 2, 2, 1}, ""},
    {"bel_first", 2009, 2009, 16, {0, 0, 0, 0, 0, 0, 1, 2, 1, 2, 2, 1, 2, 1, 3, 3}, "hzzk"},
    {"bel_first", 2010, 2014, 16, {0, 0, 0, 0, 0, 0, 1, 2, 1, 2, 2, 1, 2, 1, 3, 3}, "hzzb"},
    {"bel_first", 2015, 2015, 16, {0, 0, 0, 0, 0, 0, 1, 2, 1, 2, 2, 1, 2, 1, 3, 3}, "hzzk"},
    /* Issue 150: the groups of 2016/17 to 2018/19 hold second-division clubs, so they are stages of their own
     * (season_bel_playoffs.c) and places 7 to 16 play no more matches in this table. */
    {"bel_first", 2016, 2018, 16, {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, "hk"},
};

int season_split_of(uint32_t obj, const char *file, int n, int meetings, SeasonSplit *sp)
{
    memset(sp, 0, sizeof *sp);
    if (!season_sizes_on() || !file || !strcmp(file, "sco_prm") || meetings < 1 || n < 2) return 0;
    int season = season_sizes_row_season(obj, file);
    const SeasonLeagueRule *r = season < 0 ? NULL : season_league_row(file, season);
    if (!r || r->split_after <= 0 || r->split_after != meetings * (n - 1)) return 0;
    int sum = 0, most = 0;
    for (int k = 0; k < SEASON_LEAGUE_PARTS && r->split_size[k] > 0; k++) {
        int size = r->split_size[k], m = r->split_meetings[k];
        /* Gap 34: meetings 0, places that play no more (one club or more: the 12th of a Super League 2 group of
         * 2023/24, which goes down at once, or the 15th and 16th of the Pro League); 5, the Belgian relegation
         * play-off. Any other section needs its meetings (0 is unknown). */
        if (size < (m ? 2 : 1) || m < 0 || m > 5) return 0;
        sp->size[k] = size;
        sp->meet[k] = m;
        sp->rounds[k] = m * (size + (size & 1) - 1);
        sp->matches[k] = m * (size - 1);
        if (sp->rounds[k] > sp->rounds_max) sp->rounds_max = sp->rounds[k];
        if (sp->matches[k] > sp->matches_max) sp->matches_max = sp->matches[k];
        if (m > most) most = m;
        sum += size;
        sp->parts = k + 1;
    }
    if (sp->parts < 2 || sum != n) return 0;
    sp->meetings = meetings;
    sp->after = r->split_after;
    sp->marker = meetings + most;
    /* The row's points: 1 all halved, 3 the sections of split_halved (slice 11), else kept. */
    for (int k = 0; k < sp->parts; k++)
        sp->points[k] = r->split_points == 1 || (r->split_points == 3 && r->split_halved[k]) ? 'h' : 'k';
    memset(sp->section_of_place, -1, sizeof sp->section_of_place);
    for (int k = 0, top = 0; k < sp->parts; top += sp->size[k], k++)
        for (int p = top; p < top + sp->size[k] && p < (int)sizeof sp->section_of_place; p++)
            sp->section_of_place[p] = (signed char)k;
    for (size_t g = 0; g < sizeof GROUPS / sizeof GROUPS[0]; g++) {
        if (strcmp(file, GROUPS[g].file) || season < GROUPS[g].first || season > GROUPS[g].last || n != GROUPS[g].n)
            continue;
        int count[SEASON_LEAGUE_PARTS] = {0}, ok = 1;
        for (int p = 0; p < n; p++) {
            int k = GROUPS[g].sec[p];
            if (k < 0 || k >= sp->parts) ok = 0;
            else count[k]++;
        }
        for (int k = 0; k < sp->parts; k++) ok &= count[k] == sp->size[k];
        if (ok)
            for (int p = 0; p < n; p++) sp->section_of_place[p] = GROUPS[g].sec[p];
        if (ok && (int)strlen(GROUPS[g].points) == sp->parts)
            for (int k = 0; k < sp->parts; k++) sp->points[k] = GROUPS[g].points[k];
    }
    for (int k = 0; k < sp->parts; k++) {
        sp->halve[k] = sp->points[k] == 'h';
        sp->halved |= sp->halve[k];
    }
    return 1;
}

int season_split_matches(uint32_t obj, const char *file, int n, int meetings)
{
    SeasonSplit sp;
    return season_split_of(obj, file, n, meetings, &sp) ? sp.matches_max : 0;
}

/* The split state of a league object: 0 none (no split row, or an object the engine did not make this edition),
 * 1 before the split, 2 after it. */
static int state(uint32_t obj, SeasonSplit *sp)
{
    if (!readable(obj)) return 0;
    const char *file = season_change_file(obj);
    if (!grown(file)) return 0;
    int season = season_sizes_row_season(obj, file);
    int meetings = season < 0 ? 0 : season_league_meetings(file, season);
    int n = (int16_t)rd16(g_mem, obj + COMP_CLUBS);
    if (n < 2 || n > MAX_CLUBS || !season_split_of(obj, file, n, meetings, sp)) return 0;
    if ((int16_t)rd16(g_mem, obj + COMP_DATES) != meetings * (n + (n & 1) - 1) ||
        (int16_t)rd16(g_mem, obj + COMP_MATCHES) != sp->after + sp->matches_max || !readable(rd32(g_mem, obj + COMP_TABLE)))
        return 0;
    int m = (int16_t)rd16(g_mem, obj + COMP_MEETINGS);
    return m == meetings ? 1 : m == sp->marker ? 2 : 0;
}

/* The section of a row by its position after match N, or -1. */
static int section_of(uint32_t row, const SeasonSplit *sp)
{
    uint32_t at = rd32(g_mem, row + ROW_POSITIONS);
    if (!readable(at)) return -1;
    int pos = (int8_t)g_mem[at + (uint32_t)(sp->after - 1)];
    return pos >= 0 && pos < (int)sizeof sp->section_of_place ? sp->section_of_place[pos] : -1;
}

/* The table order after the split (cdecl: row a, row b): the section of a less that of b, 0 for one section (then
 * the points, 0x68345b). The league is the one the sort sorts. */
/* Keyed by the object, its edition and its record: a display copy on the stack (match_screens_draw_live_league_table
 * calls 0x682380 at 0x7281a0 with ecx = esp + 0x98) can sit at one address for two leagues. */
static struct { uint32_t obj, record; int year; int state; SeasonSplit sp; } cache[8];

static const SeasonSplit *cached(uint32_t obj)
{
    int year = readable(obj) ? (int16_t)rd16(g_mem, obj + COMP_YEAR) : 0, free_at = -1;
    uint32_t record = readable(obj) ? rd32(g_mem, obj + COMP_RECORD) : 0;
    for (int k = 0; k < 8; k++) {
        if (cache[k].obj == obj && cache[k].year == year && cache[k].record == record)
            return cache[k].state == 2 ? &cache[k].sp : NULL;
        if (!cache[k].obj && free_at < 0) free_at = k;
    }
    if (free_at < 0) {
        static int next;
        free_at = next++ % 8;
    }
    cache[free_at].obj = obj;
    cache[free_at].record = record;
    cache[free_at].year = year;
    cache[free_at].state = state(obj, &cache[free_at].sp);
    return cache[free_at].state == 2 ? &cache[free_at].sp : NULL;
}

static void forget(uint32_t obj)
{
    for (int k = 0; k < 8; k++)
        if (cache[k].obj == obj) cache[k].obj = 0;
}

static void split_order(void)
{
    uint32_t a = rt_arg(0), b = rt_arg(1);
    const SeasonSplit *sp = cached(rd32(g_mem, SORTED_LEAGUE));
    int r = 0;
    if (sp) {
        int x = section_of(a, sp), y = section_of(b, sp);
        if (x >= 0 && y >= 0) r = x - y;
    }
    rt_ret_cdecl((uint32_t)r);
}

/* The address of split_order, registered at the first split or load after one (0 before: no league holds it). */
static uint32_t order_addr;

static uint32_t order_fn(void)
{
    if (!order_addr) order_addr = rt_host_fn("split_order", split_order);
    return order_addr;
}

/* League::sort_table's path with no tie rules list (0x682aa8, league_sizes.py): the order of a league after its
 * split goes to the comparators' global [0xb67a00], as the other path does at 0x682884. */
void rt_split_sort_order(uint32_t obj)
{
    if (!season_sizes_on() || !readable(obj) || !order_addr || rd32(g_mem, obj + COMP_ORDER) != order_addr) return;
    wr32(g_mem, ORDER_GLOBAL, order_addr);
}

uint32_t rt_league_played_target(uint32_t obj, uint32_t original)
{
    /* The state of the league, not its order (+0xa3): a load whose order was not set yet still completes. */
    if (!season_sizes_on() || !readable(obj)) return original;
    /* A period stage of an odd count (slice 10, season_sizes_periods.c). */
    uint32_t period = season_periods_played_target(obj, original);
    if (period != original) return period;
    const SeasonSplit *sp = cached(obj);
    if (!sp) return original;
    uint32_t t = rd32(g_mem, obj + COMP_TABLE);
    int n = (int16_t)rd16(g_mem, obj + COMP_CLUBS);
    for (int i = 0; i < n; i++) {
        uint32_t row = t + (uint32_t)i * ROW_SIZE;
        int k = section_of(row, sp);
        if (k < 0 || (int8_t)g_mem[row + ROW_PLAYED] < sp->after + sp->matches[k]) return original;
    }
    return 0;   /* every row reached its own section's matches */
}

/* The sort of a league table (0x6827d0) writes each row's place at index played - 1 of its positions (`mov byte ptr
 * [eax + esi - 1], cl` at 0x682b0f, eax the row's played, ebx the league). After the split the order of the sections
 * and their targets read the place after match N, so it must not change. But a row that is still at N on a later
 * update (a club that rests on a section date, or a postponed match) got its place in the table of that day: the
 * places after N were then not each place once (slice 10 test runs, Belgian Pro League 2010/11 and Challenger Pro
 * League 2022/23). 1 when the store must not happen: a split league and a row at N. */
int rt_league_keep_split_place(uint32_t obj, uint32_t played)
{
    if (!season_sizes_on() || !readable(obj)) return 0;
    const SeasonSplit *sp = cached(obj);
    return sp && (int32_t)played == sp->after;
}

/* League::update_positions_and_status 0x684cd0 works out for each row the best place it can still reach (+0x39) and
 * the worst (+0x3a) in its loop at 0x684ef7..0x6850dc, and from 0x6850e2 sets the status marks from them. The loop
 * reads +0xa7 as every club's matches (0x684f44, 0x684f75, 0x68501e) and compares each row with every other row. After
 * a split a club stays in its section, and its section plays its own matches. So for a split league this loop takes
 * the place of the original's (INSNS 0x684ef7, league_sizes.py): the same rules, a row compared only with the rows of
 * its own section, and the matches of that section. Returns 1 when it ran (the caller jumps to 0x6850e2).
 * The original's rules, row i against a row j above it (best) or below it (worst), T the matches, pw the points of a
 * win (+0xc2), max the points a row can still reach, a row "done" when it played T:
 * - best (0x684f6f..0x684fe1): +0x39 starts at i + 1 (0x684f2f) and loses 1 for each j above that i can still pass:
 *   neither done and max_i >= pts_j; i done, j not, and max_i == pts_j; j done, i not, and max_i >= pts_j; both done,
 *   max_i == pts_j and +0xe6 set.
 * - worst (0x685015..0x6850ba): +0x3a starts at i + 1 (0x684fff) and gains 1 for each j below that can still pass i:
 *   neither done and max_j >= pts_i; i done, j not, and max_j >= pts_i; j done, i not, and max_j == pts_i; both
 *   done, max_j == pts_j and +0xe6 set.
 * - a row whose best and worst places are equal is settled and keeps them (0x684f24). */
#define FIX_YEAR 0x28u              /* s16: a fixture's year (find_new_fixture_date 0x5ab510) */
#define FIX_DAY 0x2au               /* s16: a fixture's day of the year */
#define FIX_HOME 0x1cu              /* a fixture's home club (add_fixture 0x5aa73b) */
#define FIX_FLAGS 0x4du             /* u16: a fixture's flags (add_fixture 0x5aa72e) */
#define FIX_CLUB_DAY_DONE 0x800u    /* the home club's match day rule is done (add_fixture 0x5aa732, 0x5aa76d) */
#define CLUB_MATCH_DAY 0x72u        /* s8: a club's usual match day, 0 Monday (add_fixture 0x5aa774) */
#define ROW_BEST 0x39u
#define ROW_WORST 0x3au
#define COMP_WIN_POINTS 0xc2u       /* s8: the points of a win (0x684f4d) */
#define COMP_TIES 0xe6u             /* the tie flag of two done rows (0x684fcd) */

int rt_split_bounds(uint32_t obj)
{
    if (!season_sizes_on() || !readable(obj)) return 0;
    const SeasonSplit *sp = cached(obj);
    if (!sp) return 0;
    uint32_t t = rd32(g_mem, obj + COMP_TABLE);
    int n = (int16_t)rd16(g_mem, obj + COMP_CLUBS);
    if (!readable(t) || n <= 0 || n > MAX_CLUBS) return 0;
    int sec[MAX_CLUBS], target[MAX_CLUBS], played[MAX_CLUBS], pts[MAX_CLUBS];
    for (int i = 0; i < n; i++) {
        uint32_t row = t + (uint32_t)i * ROW_SIZE;
        sec[i] = section_of(row, sp);
        if (sec[i] < 0) return 0;
        target[i] = sp->after + sp->matches[sec[i]];
        played[i] = (int8_t)g_mem[row + ROW_PLAYED];
        pts[i] = (int16_t)rd16(g_mem, row + ROW_POINTS);
    }
    int pw = (int8_t)g_mem[obj + COMP_WIN_POINTS], ties = rd32(g_mem, obj + COMP_TIES) != 0;
    for (int i = 0; i < n; i++) {
        uint32_t row = t + (uint32_t)i * ROW_SIZE;
        if (g_mem[row + ROW_BEST] == g_mem[row + ROW_WORST]) continue;
        int done_i = played[i] >= target[i], max_i = pts[i] + (target[i] - played[i]) * pw;
        uint8_t best = (uint8_t)(i + 1), worst = (uint8_t)(i + 1);
        for (int j = 0; j < i; j++) {
            if (sec[j] != sec[i]) continue;
            int done_j = played[j] >= target[j];
            int pass = !done_i && !done_j ? max_i >= pts[j]
                     : done_i && !done_j ? max_i == pts[j]
                     : !done_i ? max_i >= pts[j]
                     : max_i == pts[j] && ties;
            best -= (uint8_t)pass;
        }
        for (int j = i + 1; j < n; j++) {
            if (sec[j] != sec[i]) continue;
            int done_j = played[j] >= target[j], max_j = pts[j] + (target[j] - played[j]) * pw;
            int pass = !done_i && !done_j ? max_j >= pts[i]
                     : done_i && !done_j ? max_j >= pts[i]
                     : !done_i ? max_j == pts[i]
                     : max_j == pts[j] && ties;
            worst += (uint8_t)pass;
        }
        g_mem[row + ROW_BEST] = best;
        g_mem[row + ROW_WORST] = worst;
    }
    /* For the test log (tests/seasons/league_size_run.py): each row's places in table order, once a day. */
    static uint32_t logged_obj, logged_day;
    uint32_t day = rd32(g_mem, GAME_DATE);
    if (logging() && (logged_obj != obj || logged_day != day)) {
        logged_obj = obj;
        logged_day = day;
        fprintf(stderr, "recomp: league sizes: %s split bounds %d %d:", season_change_file(obj),
                (int16_t)rd16(g_mem, obj + COMP_YEAR), (int16_t)(day & 0xffffu));
        for (int i = 0; i < n; i++) {
            uint32_t row = t + (uint32_t)i * ROW_SIZE;
            fprintf(stderr, " %d-%d", g_mem[row + ROW_BEST], g_mem[row + ROW_WORST]);
        }
        fprintf(stderr, "\n");
    }
    return 1;
}

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
    uint32_t p = call_cdecl(F_MALLOC, 1, a);
    if (rt_unwinding || !readable(p)) return 0;
    memset(g_mem + p, 0, size);
    return p;
}

static void game_free(uint32_t p)
{
    uint32_t a[1] = {p};
    if (p) call_cdecl(F_FREE, 1, a);
}

/* The section stage that make_section deletes now (its destructor runs League::free_table 0x687970), or 0. */
static uint32_t deleting;

int season_split_deleting(uint32_t obj) { return obj && obj == deleting; }

/* Item 288: a section match never lands on the day of the split. The split runs after the day's matches (the wrap of
 * League::update_for_date 0x6847c0 below), so a fixture dated today or earlier is never played. add_fixture 0x5aa680
 * first moves a date of today or earlier (0x5aa70f..0x5aa729, find_new_fixture_date 0x5ab510). Then the home club's
 * usual match day can take the match back (0x5aa774..0x5aa804): a league match on a Saturday or Sunday goes to the
 * nearest day of the club's byte +0x72 when that is 4, 5 or 6 (Friday to Sunday, Date::get_weekday 0x54a3f0, 0x54a2d0).
 * Fixture flag 0x800 of the word +0x4d marks that rule as done (0x5aa732, 0x5aa76d).
 * NIFL Championship 2018/19: the last table matches moved to Saturday, day 25 of 2019, and the split came that day.
 * Two section matches of Sunday, day 26, went to the home club's Saturday. Four clubs ended on 31 of 32 matches,
 * check_complete 0x689ad0 stayed false and neither Northern Irish league started 2019/20. So while the sections are
 * made, the rule is marked as done for a match dated today or earlier (its new date comes from 0x5ab510) and for a
 * match that the rule would take to today or earlier. */
static int making_sections;

void season_split_add_fixture(void)
{
    if (!making_sections) return;
    uint32_t f = rt_arg(0);
    if (!readable(f)) return;
    int year = (int16_t)rd16(g_mem, f + FIX_YEAR), day = (int16_t)rd16(g_mem, f + FIX_DAY);
    int today_year = (int16_t)rd16(g_mem, TODAY + 2u), today = (int16_t)rd16(g_mem, TODAY);
    uint32_t home = rd32(g_mem, f + FIX_HOME);
    uint16_t flags = rd16(g_mem, f + FIX_FLAGS);
    if (!readable(home) || (flags & FIX_CLUB_DAY_DONE) || year > today_year + 1) return;
    int from_today = year < today_year ? -1 : day - today + (year > today_year ? season_template_year_days(today_year) : 0);
    if (from_today > 0) {
        int club_day = (int8_t)g_mem[home + CLUB_MATCH_DAY], weekday = season_template_weekday(year, day);
        if (club_day < 4 || club_day > 6 || (weekday != 5 && weekday != 6) || from_today + club_day - weekday > 0)
            return;
    }
    wr16(g_mem, f + FIX_FLAGS, (uint16_t)(flags | FIX_CLUB_DAY_DONE));
    if (logging())
        fprintf(stderr, "recomp: league sizes: a section match of day %d of %d keeps off the split day: no move to the "
                        "home club's day\n", day, year);
}

/* One section: a League_Stage of the clubs with the section's meetings on the first `days` dates of the block,
 * deleted at once as ScoPrm's (its fixtures stay with the parent). 1 when made. */
static int make_section(uint32_t obj, const uint32_t *clubs, int size, int meetings, uint32_t dates, int days,
                        uint32_t code)
{
    uint32_t list = game_malloc(4u * (uint32_t)size), tmpl = list ? game_malloc((uint32_t)days * DATE_SIZE) : 0;
    uint32_t sz[1] = {STAGE_SIZE};
    uint32_t stage = tmpl ? call_cdecl(F_NEW, 1, sz) : 0;
    if (rt_unwinding) return 0;
    int made = 0;
    if (readable(stage)) {
        for (int k = 0; k < size; k++) wr32(g_mem, list + 4u * (uint32_t)k, clubs[k]);
        memcpy(g_mem + tmpl, g_mem + dates, (size_t)days * DATE_SIZE);
        uint32_t a[22] = {obj, (uint32_t)size, list, (uint32_t)meetings, rd32(g_mem, obj + COMP_RECORD), tmpl,
                          (uint32_t)days, g_mem[obj + 0xc2u], g_mem[obj + 0xc3u], g_mem[obj + 0xc4u], obj + 0xc5u,
                          obj + 0xbeu, (uint32_t)(int16_t)rd16(g_mem, obj + COMP_YEAR), 0xffffffffu, code, 0x14, 1, 0,
                          3, 0xffffffffu, 0, 2};
        making_sections = 1;
        uint32_t made_at = call_this(F_LEAGUE_STAGE, stage, 22, a);
        making_sections = 0;
        if (rt_unwinding) return 0;
        if (readable(made_at)) {
            uint32_t one[1] = {1};
            deleting = made_at;
            call_this(rd32(g_mem, rd32(g_mem, made_at)), made_at, 1, one);   /* delete, as 0x7f2eef..0x7f2ef5 */
            deleting = 0;
            if (rt_unwinding) return 0;
            made = 1;
        }
    }
    game_free(tmpl);
    game_free(list);
    return made && !rt_unwinding;
}

static void split(uint32_t obj, const SeasonSplit *sp)
{
    const char *file = season_change_file(obj);
    uint32_t t = rd32(g_mem, obj + COMP_TABLE);
    int n = (int16_t)rd16(g_mem, obj + COMP_CLUBS), year = (int16_t)rd16(g_mem, obj + COMP_YEAR);
    /* The clubs by their position after match N; the table's order where the positions are not each place once. */
    uint32_t by_pos[MAX_CLUBS] = {0};
    int ok = 1;
    for (int i = 0; i < n && ok; i++) {
        uint32_t row = t + (uint32_t)i * ROW_SIZE, at = rd32(g_mem, row + ROW_POSITIONS);
        int pos = readable(at) ? (int8_t)g_mem[at + (uint32_t)(sp->after - 1)] : -1;
        if (pos < 0 || pos >= n || by_pos[pos]) ok = 0;
        else by_pos[pos] = rd32(g_mem, row + ROW_CLUB);
    }
    if (!ok && logging())
        for (int i = 0; i < n; i++) {
            uint32_t row = t + (uint32_t)i * ROW_SIZE, at = rd32(g_mem, row + ROW_POSITIONS);
            fprintf(stderr, "recomp: league sizes: %s split %d: row %d played %d positions", file, year, i,
                    (int8_t)g_mem[row + ROW_PLAYED]);
            for (int k = sp->after - 3; readable(at) && k <= sp->after + 1; k++)
                fprintf(stderr, " %d:%d", k, (int8_t)g_mem[at + (uint32_t)k]);
            fprintf(stderr, "\n");
        }
    int days = 0;
    uint32_t code = 0, dates = 0;
    if (ok) {
        dates = season_sizes_split_dates(obj, sp->meetings, &days, &code);
        if (rt_unwinding) return;
    }
    if (!ok || !dates || days < sp->rounds_max) {
        /* No sections: the table ends after N matches, as one table (the season still completes). The order and
         * the targets of the sections read the positions after match N, so without them there is no split. */
        game_free(dates);
        wr16(g_mem, obj + COMP_MATCHES, (uint16_t)sp->after);
        forget(obj);
        fprintf(stderr, "recomp: league sizes: %s split %d: %s; one table of %d matches (%.50s)\n", file, year,
                ok ? "no dates for the sections" : "the positions after the split match are not each place once",
                sp->after, record_name(obj));
        return;
    }
    /* A split made after its first section date: a table match put back past that date delays the split (the Super
     * League 2025/26 of a 2025 start: a match moved by clashes from day 66 to day 90, the sections began on day 67),
     * and a section fixture of a date already past was never played, so the table was never complete and the
     * nation's season change waited for ever. The dates of the block (+0 s16 day of the year, +2 s16 year,
     * DATE_SIZE bytes each) then move on together so that the first is tomorrow, in the same year; their order and
     * gaps stay. */
    {
        int today = (int16_t)rd16(g_mem, TODAY), this_year = (int16_t)rd16(g_mem, TODAY + 2u);
        int first = (int16_t)rd16(g_mem, dates), first_year = (int16_t)rd16(g_mem, dates + 2u);
        int last = (int16_t)rd16(g_mem, dates + (uint32_t)(days - 1) * DATE_SIZE);
        int last_year = (int16_t)rd16(g_mem, dates + (uint32_t)(days - 1) * DATE_SIZE + 2u);
        int shift = first_year == this_year && last_year == this_year && first <= today ? today + 1 - first : 0;
        if (shift > 0 && last + shift < 365) {
            for (int k = 0; k < days; k++) {
                uint32_t d = dates + (uint32_t)k * DATE_SIZE;
                wr16(g_mem, d, (uint16_t)((int16_t)rd16(g_mem, d) + shift));
            }
            fprintf(stderr, "recomp: league sizes: %s split %d: made on day %d, after its first section date (day "
                            "%d): the section dates move on %d days (%.50s)\n", file, year, today, first, shift,
                    record_name(obj));
        }
    }
    for (int k = 0; k < sp->parts; k++) {
        /* The clubs of the section by their places after match N (gap 35: a group of places apart). */
        uint32_t clubs[MAX_CLUBS];
        int m = 0;
        for (int p = 0; p < n && p < (int)sizeof sp->section_of_place; p++)
            if (sp->section_of_place[p] == k) clubs[m++] = by_pos[p];
        if (m == sp->size[k] && !sp->meet[k]) continue;     /* gap 34: no more matches */
        if (m != sp->size[k] || !make_section(obj, clubs, sp->size[k], sp->meet[k], dates, sp->rounds[k], code)) {
            if (rt_unwinding) return;
            /* Only when the game's allocator fails. The section's clubs then play no more matches and the table
             * is never complete; the log names it and the test fails on it. */
            fprintf(stderr, "recomp: league sizes: %s split %d: section %d not made (%.50s)\n", file, year, k + 1,
                    record_name(obj));
        }
    }
    /* Halved points, rounded up. apply_result_to_table 0x685f60 sets the overall points to the home points (+0x27)
     * plus the away points (+0x35) at each result (0x6864a5..0x6864b1, 0x68652a..0x686532), so the home and away
     * points are halved too, and add up to the halved overall points. */
    /* Gap 34: a section from 0 points ('z'), or from 0 with 3 for its best placed club ('b', the home points). */
    int top_of[SEASON_LEAGUE_PARTS];
    for (int k = 0; k < SEASON_LEAGUE_PARTS; k++) top_of[k] = -1;
    for (int p = 0; p < n && p < (int)sizeof sp->section_of_place; p++) {
        int k = sp->section_of_place[p];
        if (k >= 0 && k < sp->parts && top_of[k] < 0) top_of[k] = p;
    }
    for (int i = 0; i < n; i++) {
        uint32_t row = t + (uint32_t)i * ROW_SIZE;
        int k = section_of(row, sp);
        char rule = k >= 0 ? sp->points[k] : 'k';
        int p = (int16_t)rd16(g_mem, row + ROW_POINTS), home = (int16_t)rd16(g_mem, row + ROW_HOME_POINTS);
        int now, home_now;
        if (rule == 'h') {
            now = p >= 0 ? (p + 1) / 2 : p / 2;
            home_now = home >= 0 ? (home + 1) / 2 : home / 2;
            if (home_now > now) home_now = now;
        } else if (rule == 'z' || rule == 'b') {
            uint32_t at = rd32(g_mem, row + ROW_POSITIONS);
            int pos = readable(at) ? (int8_t)g_mem[at + (uint32_t)(sp->after - 1)] : -1;
            now = home_now = rule == 'b' && pos == top_of[k] ? 3 : 0;
        } else {
            continue;
        }
        wr16(g_mem, row + ROW_POINTS, (uint16_t)now);
        wr16(g_mem, row + ROW_HOME_POINTS, (uint16_t)home_now);
        wr16(g_mem, row + ROW_AWAY_POINTS, (uint16_t)(now - home_now));
    }
    wr16(g_mem, obj + COMP_MEETINGS, (uint16_t)sp->marker);
    wr32(g_mem, obj + COMP_ORDER, order_fn());
    forget(obj);
    if (logging()) {
        fprintf(stderr, "recomp: league sizes: %s split %d: after %d, sections", file, year, sp->after);
        for (int k = 0; k < sp->parts; k++)
            fprintf(stderr, "%s%d", k ? "+" : " ", sp->size[k]);
        fprintf(stderr, ", meetings");
        for (int k = 0; k < sp->parts; k++)
            fprintf(stderr, "%s%d", k ? "+" : " ", sp->meet[k]);
        fprintf(stderr, ", rounds");
        for (int k = 0; k < sp->parts; k++)
            fprintf(stderr, "%s%d", k ? "+" : " ", sp->rounds[k]);
        fprintf(stderr, ", %d dates, points", days);
        int same = 1;
        for (int k = 1; k < sp->parts; k++)
            same &= sp->halve[k] == sp->halve[0];
        for (int k = 0; k < (same ? 1 : sp->parts); k++)
            fprintf(stderr, "%s%s", k ? "+" : " ", sp->halve[k] ? "halved" : "kept");
        fprintf(stderr, " (%.50s)\n", record_name(obj));
        fprintf(stderr, "recomp: league sizes: %s split %d: points at the split %.*s,", file, year, sp->parts, sp->points);
        for (int p = 0; p < n; p++)
            for (int i = 0; i < n; i++) {
                uint32_t row = t + (uint32_t)i * ROW_SIZE, at = rd32(g_mem, row + ROW_POSITIONS);
                if (readable(at) && (int8_t)g_mem[at + (uint32_t)(sp->after - 1)] == p)
                    fprintf(stderr, " %d", (int16_t)rd16(g_mem, row + ROW_POINTS));
            }
        fprintf(stderr, "\n");
    }
    /* Test runs (tests/seasons/league_size_run.py --save-in-split): RECOMP_SAVE_AFTER_SPLIT=k saves at the first
     * human turn on or after the k-th section date (testrun.c counts a day as year x 366 + day of the year). */
    const char *v = getenv("RECOMP_SAVE_AFTER_SPLIT");
    static int armed;
    if (v && *v && !armed) {
        int k = atoi(v);
        k = k < 1 ? 1 : k > sp->rounds_max ? sp->rounds_max : k;
        uint32_t rec = dates + (uint32_t)(k - 1) * DATE_SIZE;
        int target = (year + (int16_t)rd16(g_mem, rec + 2)) * 366 + (int16_t)rd16(g_mem, rec);
        int today = (int16_t)rd16(g_mem, GAME_DATE + 2) * 366 + (int16_t)rd16(g_mem, GAME_DATE);
        armed = 1;
        testrun_save_in(target > today ? target - today : 0);
        fprintf(stderr, "recomp: league sizes: %s split %d: a test save after section date %d (%d days)\n", file, year,
                k, target - today);
    }
    game_free(dates);
}

void season_split_update(uint32_t obj)
{
    if (!season_sizes_on()) return;
    SeasonSplit sp;
    int st = state(obj, &sp);
    /* After the split: the order again where a load left it unset (season_split_loaded found no state then). */
    if (st == 2 && rd32(g_mem, obj + COMP_ORDER) != order_fn()) wr32(g_mem, obj + COMP_ORDER, order_fn());
    if (st != 1) return;
    uint32_t t = rd32(g_mem, obj + COMP_TABLE);
    int n = (int16_t)rd16(g_mem, obj + COMP_CLUBS);
    for (int i = 0; i < n; i++)
        if ((int8_t)g_mem[t + (uint32_t)i * ROW_SIZE + ROW_PLAYED] < sp.after) return;
    split(obj, &sp);
}

void season_split_loaded(uint32_t obj)
{
    if (!season_sizes_on()) return;
    SeasonSplit sp;
    forget(obj);
    if (state(obj, &sp) != 2) return;
    wr32(g_mem, obj + COMP_ORDER, order_fn());
    if (logging())
        fprintf(stderr, "recomp: league sizes: %s split %d: loaded after the split (%.50s)\n", season_change_file(obj),
                (int16_t)rd16(g_mem, obj + COMP_YEAR), record_name(obj));
}

void season_split_log_end(uint32_t obj)
{
    SeasonSplit sp;
    if (!season_sizes_on() || state(obj, &sp) != 2) return;
    uint32_t t = rd32(g_mem, obj + COMP_TABLE);
    int n = (int16_t)rd16(g_mem, obj + COMP_CLUBS);
    fprintf(stderr, "recomp: league sizes: %s split end %d:", season_change_file(obj), (int16_t)rd16(g_mem, obj + COMP_YEAR));
    for (int i = 0; i < n; i++) {
        uint32_t row = t + (uint32_t)i * ROW_SIZE, at = rd32(g_mem, row + ROW_POSITIONS);
        fprintf(stderr, " %d:%d", readable(at) ? (int8_t)g_mem[at + (uint32_t)(sp.after - 1)] : -1,
                (int8_t)g_mem[row + ROW_PLAYED]);
    }
    fprintf(stderr, "\n");
}

int season_split_meetings(uint32_t obj)
{
    int m = readable(obj) ? (int16_t)rd16(g_mem, obj + COMP_MEETINGS) : 0;
    if (!season_sizes_on()) return m;
    SeasonSplit sp;
    return state(obj, &sp) == 2 ? sp.meetings : m;
}

void season_split_new_edition(uint32_t obj)
{
    forget(obj);
    if (order_addr && readable(obj) && rd32(g_mem, obj + COMP_ORDER) == order_addr) wr32(g_mem, obj + COMP_ORDER, 0);
}

/* League::update_for_date 0x6847c0 (thiscall: 2 arguments, ret 8): the split after the day's update. ScoPrm's
 * 0x7f2d20 splits before it, but the sort of that update (0x6827d0, from update_positions_and_status 0x684cd0) is what
 * writes each row's position after its last match (0x682ad1..0x682b1d: the row of place i gets i at index played - 1).
 * Before it, index N - 1 holds no places yet (a test log: 0 for every row of the Welsh Premier 2012/13). After it the
 * table is sorted and index N - 1 holds each row's place. */
void o_006847c0(void);
RT_WRAP(006847c0, league_update_for_date)
{
    uint32_t self = cpu.ecx;
    o_006847c0();
    if (rt_unwinding) return;
    Cpu saved = cpu;
    season_split_update(self);
    if (rt_unwinding) return;
    season_arg_update(self);                    /* 0.7.2 gap row 48: the Apertura's knockout */
    if (!rt_unwinding) season_bel_po_update(self);  /* issue 150: the groups of Play-off II */
    if (!rt_unwinding) cpu = saved;
}
