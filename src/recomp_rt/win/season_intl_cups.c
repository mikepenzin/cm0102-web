/* The African Cup of Nations and the Asian Cup of 24 teams (0.6.0 slice 27): from 2019 both finals have 6
 * groups of 4. The top two of each group and the four best third-placed teams go to a round of 16, then
 * quarter-finals, semi-finals and the final. The African Cup of Nations of 2019 has a third-place match, the
 * Asian Cup of 2019 has none ("there was no third place play-off", source below). The edition table
 * (season_intl_table.c) gives the team count of each edition. The base game (season_intl_on() 0) and every
 * edition of 16 (ACN) or 12 (Asian Cup) teams take the original path: each site and wrap here gives the
 * original's value or calls the original then. The Asian Cup of 16 teams (2004 to 2015, 0.6.2 open item 44) has
 * 4 groups of 4 and quarter-finals of the top two (asian16 below). The 2004 edition of a 2001 game gets its
 * shape of 16 at its draw (the entrants wrap of 0x40fdf0, 0.6.2 item 180).
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
#include "season.h"
#include "season_intl.h"
#include "season_uefa_days.h"

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
#define TEAMS16 16                      /* the Asian Cup of 16 teams (2004 to 2015): 4 groups, quarter-finals */
#define GROUPS16 4
#define KO16 3

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
    Round qf, sf, final, third;         /* the original's knockout template, rounds 0 to 3 */
    const char *bracket[16];            /* round of 16 in pair order: "1A" winner, "2A" runner-up, "3A" the
                                           third that meets the winner of A */
} cups[CUPS] = {
    /* ACN template 0x4012e0, knockout from 0x401616: quarter-final 0x4016b8..0x401781, semi-final
     * 0x401784..0x401849, final 0x40184f..0x4018ed (round 2, value 0x96 at 0x40187e), third place
     * 0x4018f3..0x401994 (round 3, value 0x8c at 0x401925) (month 1 = February). */
    {"african_nations", INTL_ACN, 0x967270u, 4, 3, 0x3e9, 1, {1, 2, 3, 4},
     {{18, 1, 2}, {21, 1, 5}, 1, 1, 4, {{4, 2, 0x80}, {5, 1, 4}, {5, 2, 0x40}, {6, 1, 0x40}}, 0x78, 0x303},
     {{23, 1, 0}, {25, 1, 2}, 1, 1, 2, {{2, 1, 4}, {2, 2, 0x80}}, 0x82, 0x303},
     {{26, 1, 3}, {28, 1, 5}, 1, 4, 0, {{0}}, 0x96, 0x203},
     {{26, 1, 3}, {27, 1, 4}, 2, 0x40, 0, {{0}}, 0x8c, 0x203},
     /* 2019: QF 38-41, 42-39, 43-40, 37-44, SF QF1-QF4, QF2-QF3, so this order pairs the semi-finals */
     {"2A", "2C", "1D", "3D", "1B", "3B", "1F", "2E", "2B", "2F", "1A", "3A", "1E", "2D", "1C", "3C"}},
    /* Asian Cup template 0x40eb00, knockout from 0x40edb1: quarter-final 0x40ee4a..0x40ef07, semi-final
     * 0x40ef13..0x40efbe, final 0x40efd6..0x40f05b (round 2, value 0x96 at 0x40f002), third place
     * 0x40f073..0x40f113 (round 3, value 0x8c at 0x40f0bf) (month 9 = October). The final is the round of value
     * 0x96 as in the Euro (season_intl_euro.c, 0x57b114) and the third-place match follows it. Up to 0.6.1 this
     * table named the two the other way round, so the Asian Cup of 24, which has no third-place match, ended with
     * the third-place round, had no final and never ended (0.6.2 open item 54: the Asian Cup 2099 of a 2098
     * date jump, run-logs/0.6.2-s4a/j2098g). */
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

/* 1 when the edition at +0x40 is an Asian Cup of 16 teams in a season game (2004 to 2015; 0.6.2 open item 44:
 * the original's 12 played there). Its shape is the ACN's of 16: group A, stage[0..2] groups B to D, stage[3] the
 * quarter-finals of the top two of each group. */
static int asian16_edition(uint32_t self)
{
    if (!season_intl_on() || cup_of(self) != ASIAN) return 0;
    const SeasonIntlEdition *e = season_intl_rule_edition(cups[ASIAN].file, year_of(self));
    return e && e->teams == TEAMS16;
}

/* 1 for the edition that the ctor builds at the new game of a game that starts in 2001/02 (the table's first
 * edition, 2004). The ctor keeps the original's array of 3 stages for it (Q152: a 2001 game with changing rules on
 * equals the base game at the start, as the dates and round flags of season_intl_rules.c start_edition_2001). The
 * entrants wrap below gives it the array of 4 at its draw in 2004 (0.6.2 item 180). */
static int new_game_edition(uint32_t self)
{
    const SeasonIntlEdition *first = season_intl_first(cups[ASIAN].file, SEASON_FIRST_YEAR);
    return season_start_year() == SEASON_FIRST_YEAR && first && first->year == year_of(self);
}

/* An object of that edition with its array of 4 stages (+0x2c, made by the ctor, by rt_intl_cup_stages at
 * start_new_season or by the entrants wrap at the draw). Until the draw the 2004 edition of a 2001 game and an
 * undrawn edition of a save of 0.6.1 or older have the original's 3 (the League load makes the array from the saved
 * count, 0x688e11, 0x688e53), and the sites give the original's values. An edition drawn with 12 teams (a save of
 * 0.6.1 or older) keeps 3 stages and plays with 12. */
static int asian16(uint32_t self)
{
    return asian16_edition(self) && rd32(g_mem, self + 0x2cu) == KO16 + 1u;
}

/* ---- Sites of seasons.py ---- */

/* The team count: 16 (ACN 0x4022d3, 0x40241d) or 12 (Asian Cup 0x40fe02), and 4 times it for the arrays
 * (0x40244b, 0x40fdfe, 0x41001c). */
uint32_t rt_intl_cup_teams(uint32_t self, uint32_t original)
{
    if (asian16(self)) return TEAMS16;
    return cup24(self) >= 0 ? (uint32_t)TEAMS : original;
}

/* The group count of the draw, which is also the size of a pot: 4 (ACN 0x4024ca, 0x4024de, 0x4024fd, 0x402531)
 * or 3 (Asian Cup 0x41009b, 0x4100af, 0x4100ce, 0x410102). */
uint32_t rt_intl_cup_groups(uint32_t self, uint32_t original)
{
    if (asian16(self)) return GROUPS16;
    return cup24(self) >= 0 ? (uint32_t)GROUPS : original;
}

/* The knockout stage index: 3 (ACN) or 2 (Asian Cup). The stage count is one more. 0x79fea6 is shared with the
 * Olympics (vtable 0x96e218), whose objects give the original's value. */
uint32_t rt_intl_cup_ko(uint32_t self, uint32_t original)
{
    if (asian16(self)) return KO16;
    return cup24(self) >= 0 ? (uint32_t)KO : original;
}

/* The Asian Cup ctor 0x40e860 (0x40E90A, 0x40E923): the stage array is made there, so the edition decides. The
 * edition of the new game of a 2001 game keeps the original's 3 stages (new_game_edition). */
uint32_t rt_intl_cup_ko_ctor(uint32_t self, uint32_t original)
{
    if (asian16_edition(self)) return new_game_edition(self) ? original : KO16;
    return rt_intl_cup_ko(self, original);
}

/* start_new_season (ACN 0x401d20, Asian Cup 0x40f720) keeps the stage array of the ctor. After the step of the
 * year (0x401d80, 0x40f780) an edition of another size gets a new array (0x401d85, 0x40f785). A save keeps
 * the count: the League load makes the array from it (0x688e11, 0x688e53). The Asian Cup entrants wrap calls it
 * too. Both callers come before any stage of the edition is built: start_new_season deletes stage[0..+0x30] first
 * (0x40f75c..0x40f77c) and sets +0x30 to -1 (0x40f787), so the old slots are not copied. */
void rt_intl_cup_stages(uint32_t self)
{
    int c = cup_of(self);
    if (!season_intl_on() || c < 0) return;
    uint32_t want = (c == ASIAN && asian16_edition(self) ? (uint32_t)KO16
                                                          : rt_intl_cup_ko(self, (uint32_t)cups[c].ko)) + 1u,
             have = rd32(g_mem, self + 0x2cu);
    if (have == want) return;
    uint32_t stages = game_malloc(4u * want);
    if (rt_unwinding || !stages) return;
    game_free(rd32(g_mem, self + 0xcu));
    if (rt_unwinding) return;
    wr32(g_mem, self + 0xcu, stages);
    wr32(g_mem, self + 0x2cu, want);
    if (logging()) fprintf(stderr, "recomp: season intl: %s %d has %u stages\n", cups[c].file, year_of(self), want);
}

/* Asian Cup entrants and draw 0x40fdf0, thiscall (), called only by update_for_date 0x410170 at 0x410246, when the
 * draw date comes and +0x45 (drawn) is 0 (0x41023b). The ctor gives the 2004 edition of a 2001 game 3 stages
 * (new_game_edition) and an undrawn edition of a save of 0.6.1 or older has 3 too. Here, before the original
 * runs, such an edition of 16 gets its array of 4, so the sites of the entrants and the draw (0x40fdfe, 0x40fe02,
 * 0x41001c, 0x41009b..0x410102), the group stages (0x410337, 0x410340), the template and the quarter-finals take
 * the path of 16 (0.6.2 item 180: the 2004 Asian Cup in China had 16 teams, 17 July to 7 August 2004,
 * https://en.wikipedia.org/wiki/2004_AFC_Asian_Cup). No stage is built yet: the ctor (0x40e91c) and
 * start_new_season (0x40f787) set +0x30 to -1, and 0x40f8a0 builds groups B and C after the draw (0x410337,
 * 0x410340) and sets +0x30 (0x40fa5d). An edition with a built stage is left as it is. */
void o_0040fdf0(void);
RT_WRAP(0040fdf0, asian_entrants)
{
    uint32_t self = cpu.ecx;
    if (asian16_edition(self) && (int32_t)rd32(g_mem, self + 0x30u) < 0) {
        Cpu entry = cpu;
        rt_intl_cup_stages(self);
        if (rt_unwinding) return;
        cpu = entry;
    }
    o_0040fdf0();
}

/* ---- The hosts after the second (0.6.2 open items 44 and 46) ---- */

#define NATIONS 0xae23a8u               /* nation records, 0x122 bytes (0x402364..0x402369) */
#define NATION_SIZE 0x122u
#define NATIONAL_TEAM 0x0053b3d0u       /* the national team club of a nation record, cdecl (nation) */
#define TEAM_NATION 0x53u               /* the nation record of a national team (0x4023cf, 0x40ff41) */
#define KEY_ACN 0x9cf7acu               /* the ACN key id that find_hosts gets (0x40232f) */
#define ACN_ROOM 50                     /* the ACN entrant array: malloc(0xc8) at 0x4022cc, cap 0x32 at 0x4023b3 */

/* The nation records of the hosts after the second of the edition at +0x40 (2027 ACN: Uganda; 2007 Asian Cup:
 * Thailand and Vietnam). find_hosts 0x5fa730 gives the original's code the first two only; the others are in the
 * host row (season_intl_host_ids). Gives the count, 0 outside a season game. */
static int more_hosts(uint32_t self, uint32_t key, uint32_t nations[4])
{
    if (!season_intl_on()) return 0;
    int ids[6], n = season_intl_host_ids(key, year_of(self), ids), m = 0;
    for (int k = 2; k < n && m < 4; k++) nations[m++] = rd32(g_mem, NATIONS) + (uint32_t)ids[k] * NATION_SIZE;
    return m;
}

/* The place of the team of a nation in teams[from..to), or -1. */
static int team_place(uint32_t teams, int from, int to, uint32_t nation)
{
    for (int i = from; i < to; i++) {
        uint32_t t = rd32(g_mem, teams + 4u * (uint32_t)i);
        if (readable(t) && rd32(g_mem, t + TEAM_NATION) == nation) return i;
    }
    return -1;
}

static uint32_t national_team(uint32_t nation)
{
    uint32_t a[1] = {nation};
    uint32_t t = call_cdecl(NATIONAL_TEAM, 1, a);
    return !rt_unwinding && readable(t) && rd32(g_mem, t + TEAM_NATION) == nation ? t : 0;
}

void o_004022c0(void);

/* AfricanNations::draw_finalists 0x4022c0, thiscall (): called from update_for_date 0x4025a0 on the draw date
 * (0x402679). It takes the hosts from find_hosts 0x5fa730 (key [0x9cf7ac], year +0x40, 0x40232f..0x40233e). Item
 * 181: before that, the row of an edition of 2001/02 in a 2001 game takes the table's host (Mali 2002, not the
 * original's Egypt). */
RT_WRAP(004022c0, acn_draw)
{
    uint32_t self = cpu.ecx;
    season_intl_draw_hosts(cups[ACN].file, year_of(self));
    if (rt_unwinding) return;
    cpu.ecx = self;
    o_004022c0();
}

/* ACN entrants 0x4022c0 at 0x40242f: +0xee holds the hosts of find_hosts (dl of them, 0x402343) and then the
 * other African nations in nation order (count edi). The sort (0x402446) and the draw (0x4024c6) take the hosts
 * first and the best of the others. Each host after the second moves to the end of the hosts (added when it is
 * not in, as the last of the array when the array is full). Gives the count << 8 | the hosts added. */
uint32_t rt_intl_cup_acn_hosts(uint32_t self, uint32_t count, uint32_t hosts)
{
    uint32_t nations[4], teams = rd32(g_mem, self + TEAMS_OF);
    int n = more_hosts(self, rd32(g_mem, KEY_ACN), nations), c = (int)count, h = (int8_t)hosts, added = 0;
    for (int k = 0; k < n && readable(teams) && h >= 0 && h < c; k++) {
        if (team_place(teams, 0, h, nations[k]) >= 0) continue;
        int at = team_place(teams, h, c, nations[k]);
        uint32_t team = at >= 0 ? rd32(g_mem, teams + 4u * (uint32_t)at) : national_team(nations[k]);
        if (rt_unwinding) break;
        if (!team) continue;
        if (at < 0) at = c < ACN_ROOM ? c++ : c - 1;
        for (int i = at; i > h; i--)
            wr32(g_mem, teams + 4u * (uint32_t)i, rd32(g_mem, teams + 4u * (uint32_t)(i - 1)));
        wr32(g_mem, teams + 4u * (uint32_t)h++, team);
        added++;
        if (logging()) fprintf(stderr, "recomp: season intl: african_nations %d host %s plays\n", year_of(self),
                               win_str(nations[k] + 4u));
    }
    return (uint32_t)c << 8 | (uint32_t)added;
}

/* Asian Cup entrants 0x40fdf0 at 0x40ffb2: +0xee holds the hosts of find_hosts (ebx of them, 0x40fee7), the other
 * Asian nations are in a list on the stack (count ebp, 0x40ff71), of which 0x4afbd0 picks the others (0x40ffc8).
 * Each host after the second leaves the list and goes to +0xee after the hosts. Gives the list count << 8 | the
 * hosts added. */
uint32_t rt_intl_cup_asian_hosts(uint32_t self, uint32_t list, uint32_t count, uint32_t placed)
{
    uint32_t nations[4], teams = rd32(g_mem, self + TEAMS_OF);
    int n = more_hosts(self, rd32(g_mem, rd32(g_mem, self + 4u)), nations), c = (int)count, p = (int)placed;
    int room = (int16_t)rd16(g_mem, self + TEAM_COUNT), added = 0;
    for (int k = 0; k < n && readable(teams) && p < room; k++) {
        if (team_place(teams, 0, p, nations[k]) >= 0) continue;
        int at = team_place(list, 0, c, nations[k]);
        uint32_t team = at >= 0 ? rd32(g_mem, list + 4u * (uint32_t)at) : national_team(nations[k]);
        if (rt_unwinding) break;
        if (!team) continue;
        if (at >= 0) {
            for (int i = at; i + 1 < c; i++)
                wr32(g_mem, list + 4u * (uint32_t)i, rd32(g_mem, list + 4u * (uint32_t)(i + 1)));
            c--;
        }
        wr32(g_mem, teams + 4u * (uint32_t)p++, team);
        added++;
        if (logging()) fprintf(stderr, "recomp: season intl: asia_nations %d host %s plays\n", year_of(self),
                               win_str(nations[k] + 4u));
    }
    return (uint32_t)c << 8 | (uint32_t)added;
}

/* ---- The Asian Cup entrants by reputation (GitHub issue 136) ---- */

#define ASIAN_PICK 0x004afbd0u          /* sort and shuffle of a club list, cdecl (list, count, places) */
#define PICK_ROOM 128                   /* the list on the stack of 0x40fdf0: esp + 0x228, count below 0x64 (0x40ff24) */

/* Asian Cup entrants 0x40fdf0 at 0x40ffc8: 0x4afbd0 gets the list of the Asian national teams that are not hosts
 * (list, count ebp, places 0xf2 - hosts), sorts it by club reputation (+0x80, qsort 0x945333 with compare
 * 0x4b07d0 at 0x4afc0e, best first) and then swaps two random places of the top min(3 * places, count), that
 * many times (0x4afc2f..0x4afc57). The caller takes the first places (0x40ffe6..0x410006). So 11 of the top 33
 * of a 12-team edition played, and from 2019 (24 teams) any AFC nation could. The real finals take the sides
 * that win the AFC qualifiers, which the game does not play: the best sides by reputation stand in for them, as
 * the ACN takes them (qsort at 0x402446, no shuffle). The original still runs, so the game draws the same random
 * numbers, and then the list gets its sorted order again: the same sort of the same list. Every game, changing
 * rules on or off (Max, 13 November 2026). RECOMP_ASIAN_PICK=original keeps the original's pick (the same-save
 * run asian-pick-off). */
void rt_intl_cup_asian_pick(uint32_t self, uint32_t list, uint32_t count, uint32_t places)
{
    int c = (int16_t)count, p = (int16_t)places;
    uint32_t keep[PICK_ROOM];
    const char *v = getenv("RECOMP_ASIAN_PICK");
    int fix = !(v && !strcmp(v, "original")) && readable(list) && p >= 1 && p <= c && c <= PICK_ROOM;
    if (fix) memcpy(keep, g_mem + list, 4u * (uint32_t)c);
    uint32_t a[3] = {list, count, places};
    call_cdecl(ASIAN_PICK, 3, a);
    if (rt_unwinding) return;
    if (fix) {
        memcpy(g_mem + list, keep, 4u * (uint32_t)c);
        uint32_t q[4] = {list, (uint32_t)c, 4u, CLUB_COMPARE};
        call_cdecl(GAME_QSORT, 4, q);
        if (rt_unwinding) return;
    }
    if (logging() && readable(list) && p >= 1 && p <= c) {
        fprintf(stderr, "recomp: season intl: asia_nations %d %s %d of %d:", year_of(self),
                fix ? "takes the best" : "takes at random (original)", p, c);
        for (int i = 0; i < p; i++) {
            uint32_t t = rd32(g_mem, list + 4u * (uint32_t)i), n = readable(t) ? rd32(g_mem, t + TEAM_NATION) : 0;
            fprintf(stderr, " %s %d;", readable(n) ? win_str(n + 4u) : "?",
                    readable(t) ? (int16_t)rd16(g_mem, t + 0x80u) : -1);
        }
        fputc('\n', stderr);
    }
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
 * keep the original's slots, flags and values, 12 days later: the round of 16 plays on four weekdays (Saturday
 * to Tuesday) where the original's quarter-finals play on two, so up to 0.6.1 the quarter-finals 5 days later met
 * a team 2 days after its round of 16 match, find_fixture_clashes 0x5ad780 gave severity 22 (0x5adf66) and
 * add_fixture 0x5aa680 moved the match (Jordan v Uzbekistan, Asian Cup 2099 of a 2098 date jump, day 28 to day 34,
 * run-logs/0.6.2-s4a/j2099e.out). 12 days keeps the weekdays of 5 a week later. Inference: in 2019 both finals played
 * the round of 16 from 3 to 6 days after the groups. The table's first day moves all dates (the date shift of the
 * caller). The Asian Cup of 24 has no third-place match: R16, QF, SF, F (4 rounds); the ACN R16, QF, SF, F, third
 * place (5), the original's order.
 * All rounds move by whole weeks when the groups end too late for the round of 16 (rest_delay below). */
#define LATER_ROUNDS 12                 /* days by which the quarter-finals and later rounds move */
#define REST_DAYS 3                     /* the round of 16 starts no sooner after the day it is made */
#define MAX_DELAY 14                    /* the latest move (season_intl_rules.c finals_span counts it) */
#define GAME_DATE 0xae2c90u             /* s16 day of the year from 0, s16 year */

/* The days by which every round must move so that the round of 16 starts REST_DAYS or more after today: whole
 * weeks, so each kick-off slot keeps its weekday (the slots place the matches on the weekdays of the block around
 * the first date: the Asian Cup 2099 of a 2098 date jump, made on day 15, kept its Saturday matches on day 16 when
 * its first date moved 2 days, run-logs/0.6.2-s4a/j2099d.out). The template runs inside the date shift of the class
 * (season_intl.c stage_builder: every Date::Date moves by season_intl_date_shift) and Date::Date then moves the
 * date to its weekday, up to 3 days either way (season_weekday_date), so that date is compared with today. The
 * delay stops at MAX_DELAY (a template made long after its groups, which no run showed), so every date stays in its
 * year. The knockout is made when the groups end (the stage step 0x401e8c, 0x40f88f), and a group of 24 can end
 * later than the original's groups: a team that played the day before got a round of 16 match on its next day,
 * find_fixture_clashes 0x5ad780 gave severity 22 (0x5adf66) and add_fixture moved the match 4 days into the
 * quarter-finals (0.6.2 open item 54, cause 2). */
int season_intl_rest_delay(const char *file, int y, int day, int month0, int weekday)
{
    int today = (int16_t)rd16(g_mem, GAME_DATE), today_year = (int16_t)rd16(g_mem, GAME_DATE + 2u);
    int shift = season_intl_shift_now(), fy, fm, fd, wy, wm0, wd;
    /* The first date as Date::Date 0x549f70 makes it: moved by the builder's shift, then to its weekday (0x54a2d0). */
    season_from_abs_day(season_abs_day(y, month0 + 1, day) + shift, &fy, &fm, &fd);
    wy = fy, wm0 = fm - 1, wd = fd;
    if (weekday >= 0 && weekday <= 6) season_weekday_date(fy, fm - 1, fd, weekday, &wy, &wm0, &wd);
    long from = season_abs_day(wy, wm0 + 1, wd);
    long now = season_abs_day(today_year, 1, 1) + today;
    long delay = now + REST_DAYS - from;
    delay = delay > 0 ? (delay + 6) / 7 * 7 : 0;
    if (delay > MAX_DELAY) {
        fprintf(stderr, "recomp: season intl: %s %d knockout made on day %d of %d, %ld days late: %d kept\n", file, y,
                today, today_year, delay, MAX_DELAY);
        delay = MAX_DELAY;
    }
    if (logging())
        fprintf(stderr, "recomp: season intl: %s %d knockout made on day %d of %d, first day %d of %d, delay %ld\n",
                file, y, today, today_year, season_day_of_year(wy, wm0 + 1, wd), wy, delay);
    return (int)delay;
}

static int rest_delay(int c, uint32_t self, Day first, int y)
{
    (void)self;
    return season_intl_rest_delay(cups[c].file, y, first.day, first.month0, first.weekday);
}

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
    int delay = rest_delay(c, self, qf->first, y);
    first_date(rec, 0, later(qf->first, delay, y), y);
    last_date(rec, 0, later(qf->last, delay, y), qf->x, y, qf->y);
    slot(rec, 0, 0, 5, 1, 3, 0x40);
    slot(rec, 0, 1, 5, 2, 3, 0x80);
    slot(rec, 0, 2, 6, 2, 3, 0x200);
    slot(rec, 0, 3, 6, 1, 3, 4);
    slot(rec, 0, 4, 0, 2, 3, 0x4000);
    slot(rec, 0, 5, 0, 1, 3, 0x800);
    slot(rec, 0, 6, 1, 2, 3, 0x1000);
    slot(rec, 0, 7, 1, 1, 3, 0x2000);
    round_fields(rec, 0, 0x14, 16, 1, qf->flags);
    put_round(rec, 1, qf, delay + LATER_ROUNDS, 8, y);
    put_round(rec, 2, &cups[c].sf, delay + LATER_ROUNDS, 4, y);
    int r = 3;
    put_round(rec, r++, &cups[c].final, delay + LATER_ROUNDS, 2, y);
    if (cups[c].third_place) put_round(rec, r, &cups[c].third, delay + LATER_ROUNDS, 2, y);
    if (rt_unwinding) return 0;
    for (int k = 0; k < rounds; k++) wr16(g_mem, rec + (uint32_t)k * ROUND_SIZE + 9u, (uint32_t)k);
    return rec;
}

/* The venues of a group that takes another group's template (0.6.2 item 217). League_Stage's slot setter 0x68a1c0
 * stores the venue flags of round r, slot k at rec + r * 0x41 + k * 7 + 5. The original gives each group its own
 * weekdays, so two groups never share a day's venues; a group that takes another's template (the ACN and Asian Cup
 * of 24 here, the Asian Cup of 16, the Euro of 24 season_intl_euro.c, the Copa
 * America and Gold Cup of 16 season_intl_copa.c) plays on that group's days with its flags. (The World Cup of 48
 * plays its later groups on days of their own, season_wc48.c, so it keeps its flags.) Stadium::set_fixture_
 * venue 0x85c260 gives flag 4 the host's national stadium (nation +0x80) with no check that it is free. A rank
 * flag 0x40 << k gives the host's stadium k, k + 9, k + 18 or k + 27 (0x85c800 counts the nation's stadiums,
 * index % 9 == k) only when it is free that day
 * (0x85ddb0), else no venue; with two hosts 0x40 becomes flag 4 of the first host and 0x80 << k rank k of the
 * second (0x85c260 after find_hosts 0x5fa730 gives 2). So the twin's flag-4 match and its group's met at the
 * national stadium, find_fixture_clashes 0x5ad780 gave severity 20 for the same venue on the same day (0x5adba0,
 * the venue at +0x18) and add_fixture 0x5aa680 moved one by a week or more: the ACN 2033 of round AG (Ivory Coast
 * v Tunisia, group F, to 2 July; Ghana v South Africa, group B, to 6 July; the knockout 14 days late,
 * run-logs/merge-062ag/lane44/iwcq-2033). The twin's venue flags (4 and the ranks) therefore take ranks from 0x80
 * up that its group's template does not use, in the order of first use: each a stadium of the host that the
 * group does not play in, checked as free, else no venue, never a moved match. Real life: each edition plays in
 * its own stadiums (2019 ACN 6, 2023 ACN 6, 2025 ACN 9, 2019 Asian Cup 8, 2023 Asian Cup 9, Euro 2016 10, Euro 2020
 * 11, Euro 2024 10; en.wikipedia.org, the edition's article, "Venues"). */
void season_intl_twin_venues(uint32_t rec, uint32_t rounds_out)
{
    enum { RANKS = 9, SLOTS = 8 };
    int rounds = readable(rounds_out) ? (int16_t)rd16(g_mem, rounds_out) : 0;
    if (!readable(rec) || rounds < 1 || rounds > 8) return;
    /* The slots of a round in use: from slot 0 to the first whose weekday (+9) is not 0 to 6. Each group template
     * writes slots 0 and 1 and ends with slot 2 at weekday -1 (ACN 0x401395, 0x4013c1, 0x4013ec); the record is
     * malloc(0xc3) (0x40132b), so the slots after it hold whatever the heap held. */
    int slots[8];
    for (int r = 0; r < rounds; r++) {
        slots[r] = 0;
        while (slots[r] < SLOTS && (int8_t)rd8(g_mem, rec + (uint32_t)(r * 0x41 + slots[r] * 7 + 9)) >= 0 &&
               (int8_t)rd8(g_mem, rec + (uint32_t)(r * 0x41 + slots[r] * 7 + 9)) <= 6)
            slots[r]++;
    }
    uint32_t used = 0, from[RANKS + 1], to[RANKS + 1];
    int n = 0;
    for (int r = 0; r < rounds; r++)
        for (int k = 0; k < slots[r]; k++) used |= rd32(g_mem, rec + (uint32_t)(r * 0x41 + k * 7 + 5)) & 0x7fc0u;
    /* Flag 4 first: it is the one that is never checked, so it takes a rank while any is left (the World Cup's
     * group B template uses 4 and five ranks, 0x92c4a4). Then each rank flag in the order of first use. */
    for (int pass = 0; pass < 2; pass++)
        for (int r = 0; r < rounds; r++)
            for (int k = 0; k < slots[r]; k++) {
                uint32_t at = rec + (uint32_t)(r * 0x41 + k * 7 + 5), f = rd32(g_mem, at);
                if (pass == 0 ? f != 4u : (f & ~0x7fc0u || !f || (f & (f - 1u)))) continue;
                int i = 0;
                while (i < n && from[i] != f) i++;
                if (i == n) {
                    uint32_t pick = 0;
                    for (int b = 1; b < RANKS && !pick; b++) {
                        uint32_t c = 0x40u << b;
                        int taken = (used & c) != 0;
                        for (int j = 0; j < n; j++) taken |= to[j] == c;
                        if (!taken) pick = c;
                    }
                    if (!pick) continue;                /* no rank left: the flag stays (checked if a rank) */
                    from[n] = f;
                    to[n++] = pick;
                }
                if (to[i] != f) wr32(g_mem, at, to[i]);
            }
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
    /* The Asian Cup of 16: group D (index 2) takes the template of group A, as the groups of 24 above; the
     * quarter-finals (index 3) take the original's knockout template (index 2: quarter-final, semi-final, final
     * and third place, 0x40edb1..0x40f113), which fits the 8 of the top two. */
    if (cls == INTL_ASIAN && asian16(self) && idx >= -1 && idx <= KO16) {
        int mapped = idx == KO16 ? cups[ASIAN].ko : (idx + 1) % cups[ASIAN].groups - 1;
        uint32_t out_id = rd32(g_mem, sp + 0xcu);
        wr8(g_mem, sp + 4u, (uint32_t)mapped);
        /* Group D plays in the kick-off slots of group A, so the game moves some group matches a week (find_fixture_
         * clashes 0x5ad780, severity 20 at 0x5adf9b: the Asian Cup 2007 of a 2001 game moved Guam v Malaysia from
         * day 187 to 194, run-logs/0.6.2-s4a/a2001t) and the groups end late. The quarter-finals then move by
         * whole weeks as the knockout of 24 does (rest_delay). */
        int delay = idx == KO16 ? rest_delay(ASIAN, self, cups[ASIAN].qf.first, year_of(self)) : 0;
        season_intl_shift_more(delay);
        original();
        season_intl_shift_more(-delay);
        if (rt_unwinding) return cpu.eax;
        if (idx < KO16 && mapped != idx && readable(out_id)) wr16(g_mem, out_id, (uint32_t)(idx + cups[ASIAN].id_base));
        if (idx < KO16 && mapped != idx) season_intl_twin_venues(cpu.eax, rd32(g_mem, sp + 8u));
        return cpu.eax;
    }
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
        if (mapped != idx) season_intl_twin_venues(cpu.eax, rd32(g_mem, sp + 8u));
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
    if (rt_unwinding || index != 1u) return;
    uint32_t last = cup24(self) == ASIAN ? KO : asian16(self) ? KO16 : 0;     /* 16 teams: group D (2) */
    Cpu after = cpu;
    for (uint32_t k = 2; k < last; k++) {
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
    int c = asian16(self) ? ASIAN : cup24(self);       /* 16 teams: 1, 2, 3, 3, 5 x 4, 9 x 8 */
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
    int c = cup24(cpu.ecx), ko = KO;
    if (c < 0 && asian16(cpu.ecx)) c = ASIAN, ko = KO16;    /* 16 teams: group D and the quarter-finals 3 */
    if (c >= 0) {
        int stage = (int8_t)rd8(g_mem, cpu.esp + 8u);
        if (stage >= cups[c].ko && stage < ko) wr8(g_mem, cpu.esp + 8u, 0);
        else if (stage == ko) wr8(g_mem, cpu.esp + 8u, (uint32_t)cups[c].ko);
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

/* The quarter-finals of the Asian Cup of 16 (0.6.2 open item 44), after the original's 0x40fa90 for 12. The
 * original lists 8 teams (0x40fc27..0x40fca4: winner A, best third, winner B, second best third, winner C, runners-up
 * A, B and C), calls the template of the knockout index (0x40fcb1), makes the Cup_Stage of 8 (0x40fce2..0x40fd02:
 * this, 8, list, rounds, record, template, year, knockout index, 4, id, 0x14, 0, 0, 0, 0) into stage[ko]
 * (0x40fd1a), frees the list and the template (0x40fd1d, 0x40fd23), starts it (0x40fdc4) and sets +0x30 to the
 * knockout index (0x40fdd0). Here the list holds the top two of the four groups in the pair order of the real
 * bracket. The Cup_Stage pairs teams 0-1, 2-3 and so on, and the winners in the same order (inference, see
 * season_intl_euro.c). Sources (read 2026-10-30): https://en.wikipedia.org/wiki/2004_AFC_Asian_Cup (QF 1A-2C,
 * 1B-2D, 1C-2A, 1D-2B; SF the winners of 1A-2C and 1B-2D, of 1D-2B and 1C-2A) and the knockout stage pages of
 * 2007, 2011 and 2015 (QF 1A-2B, 1B-2A, 1C-2D, 1D-2C; SF the winners of 1A-2B and 1C-2D, of 1B-2A and 1D-2C). An
 * edition of 16 after 2015 (changing rules off) takes the bracket of 2015. */
static void knockout16(uint32_t self)
{
    static const char *const b2004[8] = {"1A", "2C", "1B", "2D", "1D", "2B", "1C", "2A"};
    static const char *const b2007[8] = {"1A", "2B", "1C", "2D", "1B", "2A", "1D", "2C"};
    const char *const *bracket = year_of(self) == 2004 ? b2004 : b2007;
    uint32_t stages = rd32(g_mem, self + 0xcu);
    if ((int)rd32(g_mem, self + 0x2cu) <= KO16 || !readable(stages)) {
        fprintf(stderr, "recomp: season intl: asia_nations %d: %d stages, no room for the quarter-finals\n",
                year_of(self), (int)rd32(g_mem, self + 0x2cu));
        return;
    }
    uint32_t g[GROUPS16], zero[1] = {0};
    g[0] = self;
    for (int i = 1; i < GROUPS16; i++) g[i] = rd32(g_mem, stages + 4u * (uint32_t)(i - 1));
    uint32_t teams = game_malloc(8u * 4u), out = game_malloc(8);
    if (rt_unwinding) return;
    if (!teams || !out) {
        if (teams) game_free(teams);
        if (out) game_free(out);
        return;
    }
    for (int k = 0; k < 8; k++)
        wr32(g_mem, teams + 4u * (uint32_t)k, table_club(g[bracket[k][1] - 'A'], bracket[k][0] - '1'));
    uint32_t xa[4] = {KO16, out, out + 4u, 0};
    uint32_t tmpl = vcall(self, 0x3cu, 4, xa);
    if (rt_unwinding) return;
    uint32_t ba[1] = {0xb2};
    uint32_t cup = call_cdecl(GAME_NEW, 1, ba);
    if (rt_unwinding) return;
    if (readable(cup)) {
        uint32_t ca[15] = {self, 8, teams, (uint32_t)(int16_t)rd16(g_mem, out), rd32(g_mem, self + 4u), tmpl,
                           (uint32_t)year_of(self), KO16, 4, (uint32_t)(int16_t)rd16(g_mem, out + 4u), 0x14, 0, 0, 0,
                           0};
        cup = call_this(CUP_STAGE_NEW, cup, 15, ca);
        if (rt_unwinding) return;
    } else {
        cup = 0;
    }
    wr32(g_mem, stages + 4u * KO16, cup);
    game_free(teams);
    game_free(tmpl);
    game_free(out);
    if (rt_unwinding) return;
    if (!cup) {
        fprintf(stderr, "recomp: season intl: asia_nations %d: no quarter-final stage\n", year_of(self));
        return;
    }
    call_this(CUP_START, cup, 1, zero);
    if (rt_unwinding) return;
    wr32(g_mem, self + 0x30u, KO16);
    if (logging()) fprintf(stderr, "recomp: season intl: asia_nations %d quarter-finals of 16 teams\n", year_of(self));
}

/* The knockout builders, thiscall (), from the stage step (ACN 0x401e94 at stage 3, site 0x401e8c; Asian Cup
 * 0x40f894 at stage 2, site 0x40f88f). */
static void knockout_wrap(int want, void (*original)(void))
{
    uint32_t self = cpu.ecx;
    if (want == ASIAN && asian16(self)) {
        knockout16(self);
        if (rt_unwinding) return;
        rt_ret_stdcall(cpu.eax, 0);
        return;
    }
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
