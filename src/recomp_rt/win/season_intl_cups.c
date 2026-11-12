/* The African Cup of Nations and the Asian Cup of 24 teams (0.6.0 slice 27): from 2019 both finals have 6
 * groups of 4. The top two of each group and the four best third-placed teams go to a round of 16, then
 * quarter-finals, semi-finals and the final. The African Cup of Nations of 2019 has a third-place match, the
 * Asian Cup of 2019 has none ("there was no third place play-off", source below). The edition table
 * (season_intl_table.c) gives the team count of each edition. The base game (season_intl_on() 0) and every
 * edition of 16 (ACN) or 12 (Asian Cup) teams take the original path: each site and wrap here gives the
 * original's value or calls the original then.
 *
 * The classes of the original (sweep-international section 2):
 * - african_nations: ctor 0x401000, vtable 0x967270. The League object is group A, stage[0..2] (+0xc, count
 *   +0x2c = 4 at 0x4010c5) are groups B to D, stage[3] the Cup_Stage of 8 (0x402090). The entrants and the draw
 *   (0x4022c0) write 16 team pointers in groups of 4 to +0xee (+0xf2 the count, 0x4022d3).
 * - asia_nations: ctor 0x40e860, vtable 0x967614. Group A, stage[0..1] groups B and C, stage[2] the Cup_Stage of
 *   8 (count 3 at 0x40e923), made by 0x40fa90 with the two best thirds. Entrants and draw 0x40fdf0 (12 at
 *   0x40fe02).
 * With 24 teams stage[0..4] are groups B to F and stage[5] the Cup_Stage of 16 in both classes.
 *
 * Sites in tools/recomp/seasons.py ("ACN and Asian Cup of 24 teams").
 *
 * Sources of the brackets and the third-place table (read 2026-10-18):
 * https://en.wikipedia.org/wiki/2019_Africa_Cup_of_Nations_knockout_stage and
 * https://en.wikipedia.org/wiki/2019_AFC_Asian_Cup_knockout_stage. Both give the same 15-row table as
 * UEFA Euro 2016 (season_intl_euro.c). The CAF and AFC regulations were not read: second source needed. */
#include "win.h"
#include "season_intl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GAME_NEW 0x00944cf1u            /* operator new, cdecl (size) */
#define GAME_MALLOC 0x00944e46u         /* malloc, cdecl (size) */
#define GAME_FREE 0x009452cau           /* free, cdecl (block) */
#define GAME_QSORT 0x00945333u          /* qsort, cdecl (base, count, size, compare) */
#define CLUB_COMPARE 0x004b07d0u        /* compare of club pointers (0x4028af, 0x41047f) */
#define SET_FINAL_POSITION 0x004a2540u  /* Comp_Stats::set_club_final_position, thiscall (club, position) */
#define LEAGUE_STAGE_NEW 0x0068b100u    /* League_Stage ctor, thiscall, 22 arguments (ret 0x58 at 0x68b4c1) */
#define CUP_STAGE_NEW 0x00522e00u       /* Cup_Stage ctor, thiscall, 15 arguments (ret 0x3c at 0x5230b9) */
#define TABLE_SORT 0x006827d0u          /* League table sort, thiscall (0) */
#define CUP_START 0x0051c800u           /* thiscall (0): 0x402290, 0x40fdc4 */
#define ROUND_FIRST_DATE 0x00521e60u    /* cdecl (rec, round, day, month0, year offset, weekday, year) */
#define ROUND_LAST_DATE 0x00521eb0u     /* cdecl (rec, round, day, month0, year offset, weekday, x, year, y) */
#define ROUND_SLOT 0x00521f10u          /* cdecl (rec, round, slot 0 to 7, weekday, time, 3, flag) */
#define ASIAN_GROUP_STAGE 0x0040f8a0u   /* asia_nations group stage builder, thiscall (index), ret 4 */

#define TABLE 0xb1u                     /* League table: rows of 0x3b bytes, +0 the club (0x6826d0) */
#define ROW_SIZE 0x3bu
#define ROUND_SIZE 0x68u                /* knockout round record */
#define TEAMS_OF 0xeeu                  /* team pointers of the draw, 4 bytes each */
#define TEAM_COUNT 0xf2u                /* word */

#define TEAMS 24
#define GROUPS 6
#define KO 5                            /* knockout stage index with 24 teams */

enum { ACN, ASIAN, CUPS };

/* A date of a knockout round: day, month from 0, weekday flag of Date::Date (sweep: the fourth argument). */
typedef struct { int day, month0, weekday; } Day;

/* One round of the original's knockout template: first date, last date (x and y: the 7th and 9th arguments of
 * 0x521eb0), up to 4 kick-off slots (index, weekday, time, flag) and the fields (value at +7, rule flags). */
typedef struct {
    Day first, last;
    int x, y;
    int slots;
    struct { int weekday, time; uint32_t flag; } slot[4];
    int value;
    uint32_t flags;
} Round;

static const struct {
    const char *file;
    uint32_t cls, vtable;
    int groups, ko;                     /* the original's group count and knockout stage index */
    int id_base;                        /* id of a group template: index + base (0x40131c, 0x40eb40) */
    int third_place;                    /* 24 teams: 1 when the knockout has a third-place match */
    int top4[4];                        /* predicted positions 1 to 4 (0x4028d2.., 0x4104a2..) */
    Round qf, sf, third, final;         /* the original's knockout template */
    const char *bracket[16];            /* round of 16 in pair order: "1A" winner, "2A" runner-up, "3A" the
                                           third that meets the winner of A */
} cups[CUPS] = {
    /* ACN template 0x4012e0, knockout from 0x401616: quarter-final 0x4016b8..0x401781, semi-final
     * 0x401784..0x401849, third place 0x40184f..0x4018ed, final 0x4018f3..0x401994 (month 1 = February). */
    {"african_nations", INTL_ACN, 0x967270u, 4, 3, 0x3e9, 1, {1, 2, 3, 4},
     {{18, 1, 2}, {21, 1, 5}, 1, 1, 4, {{4, 2, 0x80}, {5, 1, 4}, {5, 2, 0x40}, {6, 1, 0x40}}, 0x78, 0x303},
     {{23, 1, 0}, {25, 1, 2}, 1, 1, 2, {{2, 1, 4}, {2, 2, 0x80}}, 0x82, 0x303},
     {{26, 1, 3}, {28, 1, 5}, 1, 4, 0, {{0}}, 0x96, 0x203},
     {{26, 1, 3}, {27, 1, 4}, 2, 0x40, 0, {{0}}, 0x8c, 0x203},
     /* 2019: QF 38-41, 42-39, 43-40, 37-44, SF QF1-QF4, QF2-QF3, so this order pairs the semi-finals */
     {"2A", "2C", "1D", "3D", "1B", "3B", "1F", "2E", "2B", "2F", "1A", "3A", "1E", "2D", "1C", "3C"}},
    /* Asian Cup template 0x40eb00, knockout from 0x40edb1: quarter-final 0x40ee4a..0x40ef07, semi-final
     * 0x40ef13..0x40efbe, third place 0x40efd6..0x40f05b, final 0x40f073..0x40f113 (month 9 = October). */
    {"asia_nations", INTL_ASIAN, 0x967614u, 3, 2, 0x420, 0, {1, 2, 3, 3},
     {{21, 9, 5}, {23, 9, 0}, 1, 1, 4, {{0, 2, 0x40}, {0, 1, 4}, {1, 2, 0x40}, {1, 1, 4}}, 0x78, 0x303},
     {{25, 9, 2}, {26, 9, 3}, 2, 1, 2, {{3, 2, 0x40}, {3, 1, 4}}, 0x82, 3},
     {{27, 9, 4}, {29, 9, 6}, 2, 4, 0, {{0}}, 0x96, 3},
     {{27, 9, 4}, {29, 9, 6}, 1, 0x40, 0, {{0}}, 0x8c, 3},
     /* 2019: QF 1-2, 3-4, 5-6, 7-8 and SF QF1-QF2, QF3-QF4: the Euro 2016 order */
     {"2A", "2C", "1D", "3D", "1B", "3B", "1F", "2E", "1C", "3C", "1E", "2D", "1A", "3A", "2B", "2F"}},
};

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

static uint32_t vcall(uint32_t self, uint32_t slot, int nargs, const uint32_t *args)
{
    return call_this(rd32(g_mem, rd32(g_mem, self) + slot), self, nargs, args);
}

static int year_of(uint32_t self) { return (int16_t)rd16(g_mem, self + 0x40u); }

/* The class of an object by its vtable, or -1. */
static int cup_of(uint32_t self)
{
    if (!readable(self)) return -1;
    uint32_t vt = rd32(g_mem, self);
    for (int c = 0; c < CUPS; c++)
        if (vt == cups[c].vtable) return c;
    return -1;
}

/* The class when the edition at +0x40 is one of 24 teams in a season game, else -1. */
static int cup24(uint32_t self)
{
    if (!season_intl_on()) return -1;
    int c = cup_of(self);
    if (c < 0) return -1;
    const SeasonIntlEdition *e = season_intl_rule_edition(cups[c].file, year_of(self));
    return e && e->teams == TEAMS ? c : -1;
}

/* ---- Sites of seasons.py ---- */

/* The team count: 16 (ACN 0x4022d3, 0x40241d) or 12 (Asian Cup 0x40fe02), and 4 times it for the arrays
 * (0x40244b, 0x40fdfe, 0x41001c). */
uint32_t rt_intl_cup_teams(uint32_t self, uint32_t original)
{
    return cup24(self) >= 0 ? (uint32_t)TEAMS : original;
}

/* The group count of the draw, which is also the size of a pot: 4 (ACN 0x4024ca, 0x4024de, 0x4024fd, 0x402531)
 * or 3 (Asian Cup 0x41009b, 0x4100af, 0x4100ce, 0x410102). */
uint32_t rt_intl_cup_groups(uint32_t self, uint32_t original)
{
    return cup24(self) >= 0 ? (uint32_t)GROUPS : original;
}

/* The knockout stage index: 3 (ACN) or 2 (Asian Cup). The stage count is one more. 0x79fea6 is shared with the
 * Olympics (vtable 0x96e218), whose objects give the original's value. */
uint32_t rt_intl_cup_ko(uint32_t self, uint32_t original)
{
    return cup24(self) >= 0 ? (uint32_t)KO : original;
}

/* start_new_season (ACN 0x401d20, Asian Cup 0x40f720) keeps the stage array of the ctor. After the step of the
 * year (0x401d80, 0x40f780) an edition of another size gets a new array (0x401d85, 0x40f785). A save keeps
 * the count: the League load makes the array from it (0x688e11, 0x688e53). */
void rt_intl_cup_stages(uint32_t self)
{
    int c = cup_of(self);
    if (!season_intl_on() || c < 0) return;
    uint32_t want = rt_intl_cup_ko(self, (uint32_t)cups[c].ko) + 1u, have = rd32(g_mem, self + 0x2cu);
    if (have == want) return;
    uint32_t stages = game_malloc(4u * want);
    if (rt_unwinding || !stages) return;
    game_free(rd32(g_mem, self + 0xcu));
    if (rt_unwinding) return;
    wr32(g_mem, self + 0xcu, stages);
    wr32(g_mem, self + 0x2cu, want);
    if (logging()) fprintf(stderr, "recomp: season intl: %s %d has %u stages\n", cups[c].file, year_of(self), want);
}

/* ---- The template function (ACN 0x4012e0, Asian Cup 0x40eb00) ---- */

static int leap(int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

/* A date later by days (month lengths of the year, inference: Date::Date is not known to take a day past the
 * end of a month). */
static Day later(Day d, int days, int year)
{
    static const int len[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    d.day += days;
    for (;;) {
        int n = len[d.month0] + (d.month0 == 1 && leap(year));
        if (d.day <= n) break;
        d.day -= n;
        if (++d.month0 == 12) d.month0 = 0;
    }
    return d;
}

static void first_date(uint32_t rec, int round, Day d, int year)
{
    uint32_t a[7] = {rec, (uint32_t)round, (uint32_t)d.day, (uint32_t)d.month0, 0, (uint32_t)d.weekday, (uint32_t)year};
    call_cdecl(ROUND_FIRST_DATE, 7, a);
}

static void last_date(uint32_t rec, int round, Day d, int x, int year, int y)
{
    uint32_t a[9] = {rec, (uint32_t)round, (uint32_t)d.day, (uint32_t)d.month0, 0, (uint32_t)d.weekday, (uint32_t)x,
                     (uint32_t)year, (uint32_t)y};
    call_cdecl(ROUND_LAST_DATE, 9, a);
}

static void slot(uint32_t rec, int round, int index, int weekday, int time, int c, uint32_t flag)
{
    uint32_t a[7] = {rec, (uint32_t)round, (uint32_t)index, (uint32_t)weekday, (uint32_t)time, (uint32_t)c, flag};
    call_cdecl(ROUND_SLOT, 7, a);
}

/* The fields of a round record as the original writes them after the dates and slots (ACN quarter-final
 * 0x40173e..0x401781, World Cup round of 16 0x92c95d..0x92c9a0). +7 a value per round (meaning unknown), +9 the
 * round index, +0xd the rule flags, +0x18 and +0x1a teams and matches, +0x1c the teams of the first round. */
static void round_fields(uint32_t rec, int round, int value, int teams, int first, uint32_t flags)
{
    uint32_t r = rec + (uint32_t)round * ROUND_SIZE;
    wr16(g_mem, r + 7u, (uint32_t)value);
    wr16(g_mem, r + 9u, (uint32_t)round);
    wr16(g_mem, r + 0xbu, 0);
    wr16(g_mem, r + 0xdu, flags);
    wr8(g_mem, r + 0x17u, 0xa);
    wr16(g_mem, r + 0x18u, (uint32_t)teams);
    wr16(g_mem, r + 0x1au, (uint32_t)teams / 2u);
    wr16(g_mem, r + 0x1cu, first ? (uint32_t)teams : 0u);
    wr16(g_mem, r + 0xfu, 0);
    wr16(g_mem, r + 0x1eu, 0);
    wr8(g_mem, r + 0x20u, 0);
    wr8(g_mem, r + 0x21u, 1);
    wr8(g_mem, r + 0x22u, 0);
    wr32(g_mem, r + 0x5cu, 0);
    wr32(g_mem, r + 0x60u, 0);
    wr32(g_mem, r + 0x64u, 0);
}

static void put_round(uint32_t rec, int round, const Round *o, int shift, int teams, int year)
{
    first_date(rec, round, later(o->first, shift, year), year);
    last_date(rec, round, later(o->last, shift, year), o->x, year, o->y);
    for (int k = 0; k < o->slots; k++)
        slot(rec, round, k, o->slot[k].weekday, o->slot[k].time, 3, o->slot[k].flag);
    if (o->slots) slot(rec, round, o->slots, -1, -1, -1, 0);
    round_fields(rec, round, o->value, teams, 0, o->flags);
}

/* The knockout of 24 teams. The round of 16 takes the original's quarter-final dates and the 8 kick-off slots
 * of the World Cup round of 16 (0x92c8c0..0x92c958), and value 0x14 at +7 as there (0x92c95d). The later rounds
 * keep the original's slots, flags and values, 5 days later (inference: in 2019 both finals played the round
 * of 16 from 3 to 6 days after the groups). The table's first day moves all dates (the date shift of the
 * caller). The Asian Cup of 24 has no third-place match: R16, QF, SF, F (4 rounds), the ACN 5 rounds. */
static uint32_t knockout_template(int c, uint32_t self, uint32_t out_rounds, uint32_t out_id, uint32_t out_flag)
{
    int rounds = cups[c].third_place ? 5 : 4;
    if (readable(out_flag)) wr32(g_mem, out_flag, 0);
    if (readable(out_rounds)) wr16(g_mem, out_rounds, (uint32_t)rounds);
    if (readable(out_id)) wr16(g_mem, out_id, 0);
    uint32_t rec = game_malloc((uint32_t)rounds * ROUND_SIZE);
    if (rt_unwinding || !rec) return 0;
    int y = year_of(self);
    const Round *qf = &cups[c].qf;
    first_date(rec, 0, qf->first, y);
    last_date(rec, 0, qf->last, qf->x, y, qf->y);
    slot(rec, 0, 0, 5, 1, 3, 0x40);
    slot(rec, 0, 1, 5, 2, 3, 0x80);
    slot(rec, 0, 2, 6, 2, 3, 0x200);
    slot(rec, 0, 3, 6, 1, 3, 4);
    slot(rec, 0, 4, 0, 2, 3, 0x4000);
    slot(rec, 0, 5, 0, 1, 3, 0x800);
    slot(rec, 0, 6, 1, 2, 3, 0x1000);
    slot(rec, 0, 7, 1, 1, 3, 0x2000);
    round_fields(rec, 0, 0x14, 16, 1, qf->flags);
    put_round(rec, 1, qf, 5, 8, y);
    put_round(rec, 2, &cups[c].sf, 5, 4, y);
    int r = 3;
    if (cups[c].third_place) put_round(rec, r++, &cups[c].third, 5, 2, y);
    put_round(rec, r, &cups[c].final, 5, 2, y);
    if (rt_unwinding) return 0;
    for (int k = 0; k < rounds; k++) wr16(g_mem, rec + (uint32_t)k * ROUND_SIZE + 9u, (uint32_t)k);
    return rec;
}

/* The template function of a class, thiscall (index as a byte, short *rounds, short *id, int *flag), ret 0x10.
 * Called by the wraps of 0x4012e0 and 0x40eb00 in season_intl.c inside their date shift, at the entry state
 * (cpu.esp at the return address, this in cpu.ecx). It always completes the call and gives the result.
 *
 * 24 teams: index -1 to 4 are the groups A to F. The original has a template per group (ACN 4: jump table
 * 0x4019a8 at 0x401406; Asian Cup 3: 0x40ec1b..0x40ec2d), so the later groups take the templates of the first
 * ones (index ((i + 1) % groups) - 1, inference: their kick-off flags then match those groups). The id is
 * written for the real index (index + base, the texts of the new ids are unknown). Index 5 is the knockout. */
uint32_t season_intl_cup_template(uint32_t cls, void (*original)(void))
{
    uint32_t self = cpu.ecx, sp = cpu.esp;
    int c = cup24(self), idx = (int8_t)rd8(g_mem, sp + 4u);
    if (c < 0 || cups[c].cls != cls || idx < -1 || idx > KO) {
        original();
        return cpu.eax;
    }
    if (idx < KO) {
        int mapped = (idx + 1) % cups[c].groups - 1;
        uint32_t out_id = rd32(g_mem, sp + 0xcu);
        wr8(g_mem, sp + 4u, (uint32_t)mapped);
        original();
        if (rt_unwinding) return cpu.eax;
        if (mapped != idx && readable(out_id)) wr16(g_mem, out_id, (uint32_t)(idx + cups[c].id_base));
        return cpu.eax;
    }
    uint32_t rec = knockout_template(c, self, rd32(g_mem, sp + 8u), rd32(g_mem, sp + 0xcu), rd32(g_mem, sp + 0x10u));
    if (rt_unwinding) return 0;
    rt_ret_stdcall(rec, 4);
    return rec;
}

/* ---- Group stages of the Asian Cup ---- */

void o_0040f8a0(void);

/* The Asian Cup's update_for_date 0x410170 builds groups B and C with two calls of 0x40f8a0 (0x410337 with 0,
 * 0x410340 with 1), not a loop. With 24 teams the call for 1 also builds groups D to F (2 to 4). The group
 * builder takes its 4 teams from +0xee at (index + 1) * 4 (0x40f911..0x40f92b). */
RT_WRAP(0040f8a0, asian_group_stage)
{
    uint32_t self = cpu.ecx, index = rd32(g_mem, cpu.esp + 4u);
    o_0040f8a0();
    if (rt_unwinding || cup24(self) != ASIAN || index != 1u) return;
    Cpu after = cpu;
    for (uint32_t k = 2; k < KO; k++) {
        uint32_t a[1] = {k};
        call_this(ASIAN_GROUP_STAGE, self, 1, a);
        if (rt_unwinding) return;
    }
    cpu = after;
}

/* ---- Predicted positions (vtable +0x5c: ACN 0x4027d0, Asian Cup 0x4103a0) ---- */

static void predicted(int c, uint32_t self)
{
    uint32_t stats = rd32(g_mem, self + 8u), teams = rd32(g_mem, self + TEAMS_OF);
    int n = (int16_t)rd16(g_mem, self + TEAM_COUNT);
    if (n > TEAMS) n = TEAMS;
    if (n <= 0 || !readable(teams)) return;
    uint32_t list = game_malloc((uint32_t)n * 4u);
    if (rt_unwinding || !list) return;
    memcpy(g_mem + list, g_mem + teams, (size_t)n * 4u);
    uint32_t qa[4] = {list, (uint32_t)n, 4, CLUB_COMPARE};
    call_cdecl(GAME_QSORT, 4, qa);
    if (rt_unwinding) return;
    for (int k = 0; k < n; k++) {
        uint32_t club = rd32(g_mem, list + 4u * (uint32_t)k);
        int pos = k < 4 ? cups[c].top4[k] : k < 8 ? 5 : k < 16 ? 9 : 17;
        if (!club) continue;
        uint32_t pa[2] = {club, (uint32_t)pos};
        call_this(SET_FINAL_POSITION, stats, 2, pa);
        if (rt_unwinding) return;
    }
    game_free(list);
}

/* The original sorts the teams with 0x4b07d0 and gives fixed positions (ACN 16: 1, 2, 3, 4, 5 x 4, 9 x 4,
 * 13 x 4 at 0x4028c9..0x4029a7; Asian Cup 12: 1, 2, 3, 3, 5 x 4, 9, 9, 11, 11 at 0x4104a2..0x41053f). With 24
 * teams: the first 4 as the original, 5 x 4, 9 x 8 (out in the round of 16), 17 x 8 (out in the groups). The
 * thresholds of vtable +0x70 and +0x74 are not used: both classes have the plain 0x4011c0 and 0x4011d0. */
static void predicted_wrap(void (*original)(void))
{
    uint32_t self = cpu.ecx;
    int c = cup24(self);
    if (c < 0 || !readable(rd32(g_mem, self + 8u))) {
        original();
        return;
    }
    predicted(c, self);
    if (rt_unwinding) return;
    rt_ret_stdcall(cpu.eax, 0);
}

void o_004027d0(void);
void o_004103a0(void);
RT_WRAP(004027d0, acn_predicted) { predicted_wrap(o_004027d0); }
RT_WRAP(004103a0, asian_predicted) { predicted_wrap(o_004103a0); }

/* ---- Final positions (vtable +0x68: ACN 0x4029c0, Asian Cup 0x410560) ---- */

/* thiscall (club, stage as a byte, a, b, c), ret 0x14. The original maps the places of a group stage (stage
 * below its knockout index: 0x4029ce, 0x410576) and passes the knockout's on. It loses this before the test
 * (0x4029c4, 0x410564), so the test cannot be a site. With 24 teams the later group stages take the group branch
 * and the knockout 5 the knockout branch. */
static void positions_wrap(void (*original)(void))
{
    int c = cup24(cpu.ecx);
    if (c >= 0) {
        int stage = (int8_t)rd8(g_mem, cpu.esp + 8u);
        if (stage >= cups[c].ko && stage < KO) wr8(g_mem, cpu.esp + 8u, 0);
        else if (stage == KO) wr8(g_mem, cpu.esp + 8u, (uint32_t)cups[c].ko);
    }
    original();
}

void o_004029c0(void);
void o_00410560(void);
RT_WRAP(004029c0, acn_positions) { positions_wrap(o_004029c0); }
RT_WRAP(00410560, asian_positions) { positions_wrap(o_00410560); }

/* ---- The knockout stage (ACN 0x402090, Asian Cup 0x40fa90) ---- */

/* The third-placed teams against the group winners (same table in both sources). Index: the four groups whose
 * thirds go through, as bits (A = 1). Values: the group of the third that meets the winner of A, B, C, D. */
static const struct { unsigned mask; signed char vs[4]; } thirds_table[15] = {
    {0x0f, {2, 3, 0, 1}}, {0x17, {2, 0, 1, 4}}, {0x27, {2, 0, 1, 5}}, {0x1b, {3, 0, 1, 4}},
    {0x2b, {3, 0, 1, 5}}, {0x33, {4, 0, 1, 5}}, {0x1d, {2, 3, 0, 4}}, {0x2d, {2, 3, 0, 5}},
    {0x35, {2, 0, 5, 4}}, {0x39, {3, 0, 5, 4}}, {0x1e, {2, 3, 1, 4}}, {0x2e, {2, 3, 1, 5}},
    {0x36, {4, 2, 1, 5}}, {0x3a, {4, 3, 1, 5}}, {0x3c, {2, 3, 5, 4}},
};

static uint32_t table_club(uint32_t stage, int pos)
{
    uint32_t table = readable(stage) ? rd32(g_mem, stage + TABLE) : 0;
    return readable(table) ? rd32(g_mem, table + (uint32_t)pos * ROW_SIZE) : 0;
}

/* The knockout of 24 teams, after the Asian Cup's 0x40fa90: a League_Stage of the thirds with no fixtures
 * (0x40fb57..0x40fb90: stage index -2, no team list), row 2 of each group table copied in with the dwords +5 and
 * +9 of each copy cleared (0x40fba9..0x40fc1d, meanings unknown), sorted (0x40fc22), the Cup_Stage made
 * (0x40fd02, argument 11 is 0 in both classes: 0x40fce2, 0x4021b6), the ranking stage deleted (0x40fd37) and the
 * knockout started (0x40fdc4). Neither class sends group news in its knockout builder, so none here. The
 * Cup_Stage pairs teams 0-1, 2-3 and so on and the winners in the same order (inference, see
 * season_intl_euro.c). */
static void knockout24(int c, uint32_t self)
{
    uint32_t stages = rd32(g_mem, self + 0xcu);
    if ((int)rd32(g_mem, self + 0x2cu) <= KO || !readable(stages)) {
        fprintf(stderr, "recomp: season intl: %s %d: %d stages, no room for the knockout\n", cups[c].file,
                year_of(self), (int)rd32(g_mem, self + 0x2cu));
        return;
    }
    uint32_t g[GROUPS];
    g[0] = self;
    for (int i = 1; i < GROUPS; i++) g[i] = rd32(g_mem, stages + 4u * (uint32_t)(i - 1));

    int rank_group[GROUPS];
    for (int i = 0; i < GROUPS; i++) rank_group[i] = i;
    uint32_t zero[1] = {0}, na[1] = {0xee};
    uint32_t rank = call_cdecl(GAME_NEW, 1, na);
    if (rt_unwinding) return;
    if (readable(rank)) {
        uint32_t la[22] = {self, GROUPS, 0, 0, rd32(g_mem, self + 4u), 0, 0, rd8(g_mem, self + 0xc2u),
                           rd8(g_mem, self + 0xc3u), 0, self + 0xc5u, self + 0xbeu, (uint32_t)year_of(self),
                           0xfffffffeu, 0, 0, 1, 0, 2, 0xffffffffu, 0, 2};
        rank = call_this(LEAGUE_STAGE_NEW, rank, 22, la);
        if (rt_unwinding) return;
    }
    if (readable(rank)) {
        uint32_t dst = rd32(g_mem, rank + TABLE);
        for (int i = 0; i < GROUPS && readable(dst); i++) {
            uint32_t src = rd32(g_mem, g[i] + TABLE), row = dst + (uint32_t)i * ROW_SIZE;
            if (!readable(src)) continue;
            memmove(g_mem + row, g_mem + src + 2u * ROW_SIZE, ROW_SIZE);
            wr32(g_mem, row + 9u, 0);
            wr32(g_mem, row + 5u, 0);
        }
        call_this(TABLE_SORT, rank, 1, zero);
        if (rt_unwinding) return;
        int used = 0;
        for (int p = 0; p < GROUPS; p++) {
            uint32_t club = table_club(rank, p);
            for (int i = 0; i < GROUPS; i++)
                if (!(used & (1 << i)) && club && club == table_club(g[i], 2)) {
                    rank_group[p] = i;
                    used |= 1 << i;
                    break;
                }
        }
        if (used != (1 << GROUPS) - 1)
            for (int i = 0; i < GROUPS; i++) rank_group[i] = i;
    }
    unsigned mask = 0;
    for (int p = 0; p < 4; p++) mask |= 1u << rank_group[p];
    int row = -1;
    for (int r = 0; r < 15; r++)
        if (thirds_table[r].mask == mask) row = r;

    uint32_t teams = game_malloc(16u * 4u), out = game_malloc(8);
    if (rt_unwinding) return;
    if (!teams || !out) {                               /* no leak when only one block came */
        if (teams) game_free(teams);
        if (out) game_free(out);
        return;
    }
    for (int k = 0; k < 16; k++) {
        const char *b = cups[c].bracket[k];
        int grp = b[1] - 'A', pos = b[0] - '1';
        uint32_t club;
        if (pos < 2) {
            club = table_club(g[grp], pos);
        } else {
            int third = grp < 4 && row >= 0 ? thirds_table[row].vs[grp] : rank_group[grp < 4 ? grp : 0];
            club = table_club(g[third], 2);
        }
        wr32(g_mem, teams + 4u * (uint32_t)k, club);
    }

    /* Template (index, &rounds, &id, 0: 0x402185, 0x40fcb1) and Cup_Stage (0x4021d6, 0x40fd02) */
    uint32_t xa[4] = {KO, out, out + 4u, 0};
    uint32_t tmpl = vcall(self, 0x3cu, 4, xa);
    if (rt_unwinding) return;
    uint32_t ba[1] = {0xb2};
    uint32_t cup = call_cdecl(GAME_NEW, 1, ba);
    if (rt_unwinding) return;
    if (readable(cup)) {
        uint32_t ca[15] = {self, 16, teams, (uint32_t)(int16_t)rd16(g_mem, out), rd32(g_mem, self + 4u), tmpl,
                           (uint32_t)year_of(self), KO, 4, (uint32_t)(int16_t)rd16(g_mem, out + 4u), 0x14, 0, 0, 0,
                           0};
        cup = call_this(CUP_STAGE_NEW, cup, 15, ca);
        if (rt_unwinding) return;
    } else {
        cup = 0;
    }
    wr32(g_mem, stages + 4u * KO, cup);
    game_free(teams);
    game_free(tmpl);
    game_free(out);
    if (rt_unwinding) return;
    if (readable(rank)) {
        uint32_t da[1] = {1};
        vcall(rank, 0, 1, da);                           /* scalar deleting destructor (0x40fd37) */
        if (rt_unwinding) return;
    }
    if (!cup) {
        fprintf(stderr, "recomp: season intl: %s %d: no knockout stage\n", cups[c].file, year_of(self));
        return;
    }
    call_this(CUP_START, cup, 1, zero);
    if (rt_unwinding) return;
    wr32(g_mem, self + 0x30u, KO);
    if (logging())
        fprintf(stderr, "recomp: season intl: %s %d round of 16, best thirds of groups %c%c%c%c\n", cups[c].file,
                year_of(self), 'A' + rank_group[0], 'A' + rank_group[1], 'A' + rank_group[2], 'A' + rank_group[3]);
}

/* The knockout builders, thiscall (), from the stage step (ACN 0x401e94 at stage 3, site 0x401e8c; Asian Cup
 * 0x40f894 at stage 2, site 0x40f88f). */
static void knockout_wrap(int want, void (*original)(void))
{
    uint32_t self = cpu.ecx;
    int c = cup24(self);
    if (c != want) {
        original();
        return;
    }
    knockout24(c, self);
    if (rt_unwinding) return;
    rt_ret_stdcall(cpu.eax, 0);
}

void o_00402090(void);
void o_0040fa90(void);
RT_WRAP(00402090, acn_knockout) { knockout_wrap(ACN, o_00402090); }
RT_WRAP(0040fa90, asian_knockout) { knockout_wrap(ASIAN, o_0040fa90); }
