/* The World Cup of 48 teams (0.6.0 slice 29): from 2026 the finals have 12 groups of 4. The top two of each
 * group and the eight best third-placed teams go to a round of 32, then a round of 16, quarter-finals,
 * semi-finals, the third place match and the final. The edition table (season_intl_table.c) gives the team
 * count of each edition. The base game (season_intl_on() 0) and every edition of 32 teams take the original
 * path: each site and wrap here gives the original's value or calls the original then.
 *
 * The class world_cup (ctor 0x92bf50, vtable 0x9712f0, object of 0x192 bytes: push 0x192 at 0x8307b5). The
 * League object itself is group A (template index -1). stage[0..6] (+0xc, count +0x2c) are groups B to H
 * (League_Stage, group byte = index + 2, 0x92d540), stage[7] the Cup_Stage of 16 (0x92d770). With 48 teams
 * stage[0..10] are groups B to L and stage[11] the Cup_Stage of 32. The knockout stage index (7 or 11) is "ko".
 *
 * Sites in tools/recomp/seasons.py ("World Cup of 48 teams"): the slot array and room of the ctor (0x92bff0,
 * 0x92bffa), its stage array and count (0x92c04d, 0x92c063), new arrays after the step of start_new_season
 * (0x92d40e), the stage step (0x92d52c), vtable +0x30 (0x92d316), the news (0x92ccf5, 0x92cd86, 0x92cea5), the
 * draw scans of the group builders (0x92d1d7, 0x92d5fb) and the group stages of run_group_draw (0x92e907).
 *
 * Research: docs/research/2026-10-19-world-cup-48.md (the sites, read in 3.9.68). The bracket and the table of
 * thirds: Annex C of the 2026 regulations as listed by Wikipedia (season_wc48_thirds.c). */
#include "win.h"
#include "season_intl.h"
#include "season_wc48.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WC_KEY 0x9cf79cu                /* key global of the FIFA World Cup */
#define COMP_RECORDS 0xae23d0u
#define COMP_RECORD_SIZE 0x6bu
#define NATIONS 0xae23a8u
#define NATION_SIZE 0x122u
#define INTL_REPUTATION 0xb74318u       /* object of 0x775220 (0x92f9ba) */
#define RULING_BODIES 0xae28f0u         /* object list read at 0x92d7a0 */

#define GAME_NEW 0x00944cf1u
#define GAME_MALLOC 0x00944e46u
#define GAME_CALLOC 0x00945501u         /* calloc, cdecl (count, size): 0x92c000 */
#define GAME_FREE 0x009452cau
#define GAME_QSORT 0x00945333u
#define GAME_RANDOM 0x0090cfc0u
#define NATIONAL_TEAM 0x0053b3d0u
#define LATEST_PLACED 0x004afd80u
#define FIND_HOSTS 0x005fa730u
#define SLOT_COMPARE 0x004b07b0u        /* compare of 6-byte team slots (seed_teams 0x92f34a) */
#define CLUB_COMPARE 0x004b07d0u        /* compare of club pointers (predicted positions) */
#define SET_FINAL_POSITION 0x004a2540u
#define REP_TEAM 0x00775220u            /* thiscall (nation, comp record): 0x92f9ba */
#define BODY_UPDATE 0x007e9180u         /* thiscall (0): 0x92d7a0 */
#define LEAGUE_STAGE_NEW 0x0068b100u
#define CUP_STAGE_NEW 0x00522e00u
#define TABLE_SORT 0x006827d0u
#define CUP_START 0x0051c800u           /* 0x92da17 */
#define DATE_MAKE 0x00549f70u           /* Date::Date, thiscall (day, month from 0, year, flag), ret 0x10 */
#define ROUND_FIRST_DATE 0x00521e60u
#define ROUND_LAST_DATE 0x00521eb0u
#define ROUND_SLOT 0x00521f10u

#define SLOTS 0x14u
#define SLOT_COUNT 0x36u
#define SLOT_ROOM 0x38u
#define POTS 0xeeu                      /* 32 entries of 5 bytes, to +0x18d (load 0x92f000, save 0x92f150) */
#define TABLE 0xb1u
#define ROW_SIZE 0x3bu
#define ROUND_SIZE 0x68u

#define TEAMS 48
#define GROUPS 12
#define KO 11
#define ROUNDS 6

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

int season_wc48(uint32_t self)
{
    if (!season_intl_on() || !readable(self)) return 0;
    const SeasonIntlEdition *e = season_intl_rule_edition("world_cup", year_of(self));
    return e && e->teams == TEAMS;
}

/* ---- Sites of seasons.py ---- */

uint32_t rt_intl_wc_teams(uint32_t self, uint32_t original)
{
    return season_wc48(self) ? (uint32_t)TEAMS : original;
}

uint32_t rt_intl_wc_ko(uint32_t self, uint32_t original)
{
    return season_wc48(self) ? (uint32_t)KO : original;
}

/* start_new_season 0x92d330 deletes the stages (0x92d3e7..0x92d407) but keeps the stage array +0xc and the count
 * +0x2c of the ctor, and build_team_slots 0x92da40 (called at 0x92d41d) writes the slots +0x14 of the ctor's
 * calloc without a new one. After the step of the year (0x92d409) an edition of another size gets new arrays,
 * here at 0x92d40e: the stages (count ko + 1) and the slots (room +0x38). A save keeps both: the League load
 * makes the stage array from +0x2c (0x688e11, 0x688e53) and the slots from +0x38 (0x689035..0x689067). */
void rt_intl_wc_stages(uint32_t self)
{
    if (!season_intl_on() || !readable(self)) return;
    uint32_t want = rt_intl_wc_ko(self, 7u) + 1u, have = rd32(g_mem, self + 0x2cu);
    if (have != want) {
        uint32_t stages = game_malloc(4u * want);
        if (rt_unwinding || !stages) return;
        game_free(rd32(g_mem, self + 0xcu));
        if (rt_unwinding) return;
        wr32(g_mem, self + 0xcu, stages);
        wr32(g_mem, self + 0x2cu, want);
    }
    uint32_t room = rt_intl_wc_teams(self, 0x20u);
    if ((uint32_t)(int16_t)rd16(g_mem, self + SLOT_ROOM) != room) {
        uint32_t ca[2] = {room, 6};
        uint32_t slots = call_cdecl(GAME_CALLOC, 2, ca);
        if (rt_unwinding || !readable(slots)) return;
        game_free(rd32(g_mem, self + SLOTS));
        if (rt_unwinding) return;
        wr32(g_mem, self + SLOTS, slots);
        wr16(g_mem, self + SLOT_ROOM, room);
        wr16(g_mem, self + SLOT_COUNT, 0);
    }
    if (logging())
        fprintf(stderr, "recomp: season intl: world_cup %d has %u stages and %u slots\n", year_of(self), want, room);
}

/* ---- The template function (vtable +0x3c, 0x92c290) ---- */

static void first_date(uint32_t rec, int round, int day, int month0, int weekday, int year)
{
    uint32_t a[7] = {rec, (uint32_t)round, (uint32_t)day, (uint32_t)month0, 0, (uint32_t)weekday, (uint32_t)year};
    call_cdecl(ROUND_FIRST_DATE, 7, a);
}

static void last_date(uint32_t rec, int round, int day, int month0, int weekday, int x, int year, int y)
{
    uint32_t a[9] = {rec, (uint32_t)round, (uint32_t)day, (uint32_t)month0, 0, (uint32_t)weekday, (uint32_t)x,
                     (uint32_t)year, (uint32_t)y};
    call_cdecl(ROUND_LAST_DATE, 9, a);
}

static void slot(uint32_t rec, int round, int index, int weekday, int time, int c, uint32_t flag)
{
    uint32_t a[7] = {rec, (uint32_t)round, (uint32_t)index, (uint32_t)weekday, (uint32_t)time, (uint32_t)c, flag};
    call_cdecl(ROUND_SLOT, 7, a);
}

/* The fields of a round record as the original writes them (round of 16 0x92c95d..0x92c9a0, quarter-final
 * +0x68, semi-final +0xd0, third place +0x138, final +0x1a0). */
static void round_fields(uint32_t rec, int round, int value, int teams, int first, int year, uint32_t flags)
{
    uint32_t r = rec + (uint32_t)round * ROUND_SIZE;
    wr16(g_mem, r + 7u, (uint32_t)value);
    wr16(g_mem, r + 9u, (uint32_t)round);
    wr16(g_mem, r + 0xbu, 0);
    wr16(g_mem, r + 0xdu, rt_intl_round_flags(INTL_WORLD_CUP, (uint32_t)year, flags));
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

/* The knockout of 48 teams: 6 rounds (month from 0, 5 = June, 6 = July). The original's round of 16 to final
 * (0x92c8a1..0x92cc63) keep their dates, weekday codes, x and y arguments, slots, values and flags, 8 days later:
 * round of 16 26 June to 30 June becomes 4 to 8 July, quarter-finals 9 to 12 July, semi-finals 13 to 16 July,
 * third place 17 to 18 July and final 18 to 19 July (the third place first, review 2). The first date of a round must come before its last: a first
 * form with both on the match day for the third place and the final never played them, so the cup never ended
 * and the World Cup and its sections never moved on (a 2025 start: four teams with status 0 on 21 July 2026).
 * The round of 32 (16 matches) plays from 27 June (the groups end on 26 June, groups I to L two days after A to D)
 * to 3 July, with no
 * kick-off slots: a round holds 8 (0x521f10 takes index 0 to 7, 0x521f28), and the matches then fall between
 * the two dates (inference). The real 2026 dates: round of 32 28 June to 3 July, round of 16 4 to 7 July,
 * quarter-finals 9 to 11 July, semi-finals 14 and 15 July, third place 18 July, final 19 July. The table's first
 * day moves all dates (the date shift of the caller). The value +7 of the round of 32 is that of the round of 16
 * (0x14; meaning unknown). */
static uint32_t knockout_template(uint32_t self, uint32_t out_rounds, uint32_t out_id, uint32_t out_flag)
{
    if (readable(out_flag)) wr32(g_mem, out_flag, 0);
    if (readable(out_rounds)) wr16(g_mem, out_rounds, ROUNDS);
    if (readable(out_id)) wr16(g_mem, out_id, 0);
    uint32_t rec = game_malloc(ROUNDS * ROUND_SIZE);
    if (rt_unwinding || !rec) return 0;
    int y = year_of(self);
    /* Round of 32: first 27 June (the day after the last group day of I to L), last 3 July */
    first_date(rec, 0, 27, 5, 5, y);
    last_date(rec, 0, 3, 6, 5, 1, y, 1);
    round_fields(rec, 0, 0x14, 32, 1, y, 0x383);
    /* Round of 16 (0x92c8a1, 0x92c8b8: 26 to 30 June) */
    first_date(rec, 1, 4, 6, 4, y);
    last_date(rec, 1, 8, 6, 1, 1, y, 1);
    slot(rec, 1, 0, 5, 1, 3, 0x40);
    slot(rec, 1, 1, 5, 2, 3, 0x80);
    slot(rec, 1, 2, 6, 2, 3, 0x200);
    slot(rec, 1, 3, 6, 1, 3, 4);
    slot(rec, 1, 4, 0, 2, 3, 0x4000);
    slot(rec, 1, 5, 0, 1, 3, 0x800);
    slot(rec, 1, 6, 1, 2, 3, 0x1000);
    slot(rec, 1, 7, 1, 1, 3, 0x2000);
    round_fields(rec, 1, 0x14, 16, 0, y, 0x383);
    /* Quarter-finals (0x92c9b2, 0x92c9cd: 1 to 4 July) */
    first_date(rec, 2, 9, 6, 2, y);
    last_date(rec, 2, 12, 6, 5, 1, y, 1);
    slot(rec, 2, 0, 4, 2, 3, 4);
    slot(rec, 2, 1, 4, 1, 3, 0x400);
    slot(rec, 2, 2, 5, 2, 3, 0x40);
    slot(rec, 2, 3, 5, 1, 3, 0x100);
    slot(rec, 2, 4, -1, -1, -1, 0);
    round_fields(rec, 2, 0x78, 8, 0, y, 0x383);
    /* Semi-finals (0x92caa8, 0x92cac0: 5 to 8 July, x 2) */
    first_date(rec, 3, 13, 6, 6, y);
    last_date(rec, 3, 16, 6, 2, 2, y, 1);
    slot(rec, 3, 0, 1, 2, 3, 0x40);
    slot(rec, 3, 1, 2, 2, 3, 4);
    slot(rec, 3, 2, -1, -1, -1, 0);
    round_fields(rec, 3, 0x82, 4, 0, y, 0x383);
    /* Third place (0x92cb83, 0x92cb9b: 9 to 12 July, y 4): 17 to 18 July, before the final (review 2) */
    first_date(rec, 4, 17, 6, 3, y);
    last_date(rec, 4, 18, 6, 4, 1, y, 4);
    round_fields(rec, 4, 0x96, 2, 0, y, 0x83);
    /* Final (0x92cc27, 0x92cc42: 9 to 11 July, y 0x80): 18 to 19 July */
    first_date(rec, 5, 18, 6, 4, y);
    last_date(rec, 5, 19, 6, 5, 1, y, 0x80);
    round_fields(rec, 5, 0x8c, 2, 0, y, 0x3);
    return rt_unwinding ? 0 : rec;
}

/* thiscall (index as a byte, short *rounds, short *id, int *flag), ret 0x10. 48 teams: index -1 to 10 are the
 * groups A to L. The original has 8 group templates (jump table 0x92ccd0, index -1 to 6; index 7 its knockout),
 * all with the matchdays 12, 19 and 24 June (0x92c335, 0x92c360, 0x92c38c) and their own kick-off flags. Groups
 * I to L take the templates of A to D (index - 8; inference: the flags place the matches of a matchday). The id
 * (index + 0x420, 0x92c2d0) is written for the real index: the texts of 0x427 to 0x42a are unknown. Index 11 is
 * the knockout above. */
uint32_t season_wc48_template(void (*original)(void))
{
    uint32_t self = cpu.ecx, sp = cpu.esp;
    int idx = (int8_t)rd8(g_mem, sp + 4u);
    if (!season_wc48(self) || idx < -1 || idx > KO) {
        original();
        return cpu.eax;
    }
    if (idx < KO) {
        int mapped = idx < 7 ? idx : idx - 8;
        uint32_t out_id = rd32(g_mem, sp + 0xcu), out_rounds = rd32(g_mem, sp + 8u);
        wr8(g_mem, sp + 4u, (uint32_t)mapped);
        original();
        if (rt_unwinding) return cpu.eax;
        if (mapped != idx && readable(out_id)) wr16(g_mem, out_id, (uint32_t)(idx + 0x420));
        /* Groups E to H play a day after A to D, groups I to L two days after: 8 World Cup matches a day at
         * most (the original's 8 groups play 16 on each of its three days; the real 2026 groups played from 11
         * to 27 June). A league round record is 0x41 bytes: +0 the day of the year, +2 the year offset, +4 the
         * weekday code (league_round_set_date 0x68a160: 0x68a1a5..0x68a1b4). The days stay in June. */
        int later = idx >= 7 ? 2 : idx >= 3 ? 1 : 0, rounds = readable(out_rounds) ? (int16_t)rd16(g_mem, out_rounds) : 0;
        uint32_t rec = cpu.eax;
        for (int i = 0; later && readable(rec) && i < rounds && i < 8; i++) {
            uint32_t r = rec + (uint32_t)i * 0x41u;
            wr16(g_mem, r, rd16(g_mem, r) + (uint32_t)later);
            int wd = rd8(g_mem, r + 4u);
            if (wd <= 6) wr8(g_mem, r + 4u, (uint32_t)((wd + later) % 7));
        }
        return cpu.eax;
    }
    uint32_t rec = knockout_template(self, rd32(g_mem, sp + 8u), rd32(g_mem, sp + 0xcu), rd32(g_mem, sp + 0x10u));
    if (rt_unwinding) return 0;
    rt_ret_stdcall(rec, 4);
    return rec;
}

/* ---- The seeding and draw days ---- */

/* The stage builder 0x92d200 makes the seeding day 15 December (Date at 0x92d290, kept at +0x18e with the year
 * offset -1 at +0x190) and the draw day 27 December (0x92d2bd, +0xb6 and +0xb8 -1) of the year before. The 2026
 * play-off tournament and the UEFA paths play on 26 and 31 March 2026 (int-world-291, int-world-297), after
 * them. The real draw (5 December 2025) left their places open, which the game's draw cannot do. So an edition
 * of 48 seeds on 1 April and draws on 2 April of its year (decision Q561), as slice 28 moved the Euro 2020 and
 * 2024 draws after their path finals. update_for_date 0x92f1a0 compares the day word and +0x40 plus the offset
 * (0x92f1b4..0x92f1ce, 0x92f1e2). */
static uint16_t day_word(int day, int month0, int year)
{
    uint32_t date = game_malloc(8);
    if (rt_unwinding || !date) return 0;
    uint32_t a[4] = {(uint32_t)day, (uint32_t)month0, (uint32_t)year, 0xffffffffu};
    call_this(DATE_MAKE, date, 4, a);
    uint16_t w = rt_unwinding ? 0 : rd16(g_mem, date);
    game_free(date);
    return w;
}

void season_wc48_after_stages(uint32_t self)
{
    if (!season_wc48(self)) return;
    int y = year_of(self);
    uint16_t seed = day_word(1, 3, y), draw = day_word(2, 3, y);
    if (rt_unwinding) return;
    wr16(g_mem, self + 0x18eu, seed);
    wr16(g_mem, self + 0x190u, 0);
    wr16(g_mem, self + 0xb6u, draw);
    wr16(g_mem, self + 0xb8u, 0);
    if (logging())
        fprintf(stderr, "recomp: season intl: world_cup %d seeds on 1 April and draws on 2 April %d\n", y, y);
}

/* ---- Team slots: more than two hosts ---- */

static uint32_t club_nation(uint32_t club) { return readable(club) ? rd32(g_mem, club + 0x53u) : 0; }

static uint32_t host_team(int id)
{
    if (id < 0) return 0;
    uint32_t nation = rd32(g_mem, NATIONS) + (uint32_t)id * NATION_SIZE;
    uint32_t a[1] = {nation};
    uint32_t club = call_cdecl(NATIONAL_TEAM, 1, a);
    return readable(club) && club_nation(club) == nation ? club : 0;
}

void o_0092da40(void);

/* build_team_slots, thiscall (): the holder (0x92da72), the two hosts of find_hosts (0x92daa9..0x92db4a), then
 * the default list in slots 3 to 31 (0x92dbd5..0x92dfb3, no count change). The hosts after the second (2026:
 * the United States; 2030: four more) come from the host row of the table (season_intl_host_ids) into the next
 * slot, flag 1 (as 0x92dac3), when their nation is not in yet. */
RT_WRAP(0092da40, wc_team_slots)
{
    uint32_t self = cpu.ecx;
    o_0092da40();
    if (rt_unwinding || !season_wc48(self)) return;
    int ids[6], n = season_intl_host_ids(rd32(g_mem, WC_KEY), year_of(self), ids);
    uint32_t slots = rd32(g_mem, self + SLOTS);
    for (int k = 2; k < n && readable(slots); k++) {
        uint32_t team = host_team(ids[k]);
        if (rt_unwinding) return;
        int count = (int16_t)rd16(g_mem, self + SLOT_COUNT), in = 0;
        if (!team || count >= TEAMS) continue;
        for (int i = 0; i < count && !in; i++)
            in = club_nation(rd32(g_mem, slots + 6u * (uint32_t)i)) == club_nation(team);
        if (in) continue;
        wr32(g_mem, slots + 6u * (uint32_t)count, team);
        wr8(g_mem, slots + 6u * (uint32_t)count + 4u, 1);
        wr16(g_mem, self + SLOT_COUNT, (uint16_t)(count + 1));
        if (logging())
            fprintf(stderr, "recomp: season intl: world_cup %d host %s in slot %d\n", year_of(self),
                    win_str(club_nation(team) + 4u), count);
    }
}

/* ---- The draw (0x92e940) ---- */

static int same_team(uint32_t a, uint32_t b)
{
    return a && b && (a == b || (club_nation(a) && club_nation(a) == club_nation(b)));
}

static int random_below(int n)
{
    uint32_t a[1] = {(uint32_t)n};
    int r = (int16_t)call_cdecl(GAME_RANDOM, 1, a);
    return r < 0 || r >= n ? 0 : r;
}

/* The group of each host by its place in the table (decision Q562). 2026 (Canada, Mexico, United States): the
 * real draw put Canada in B, Mexico in A and the United States in D. Other editions (2030): A, B, D, C, E, F.
 * A host that is missing from the slots does not move the others (review 2). */
static int host_group_of(int year, int place)
{
    static const int y2026[3] = {2, 1, 4}, other[6] = {1, 2, 4, 3, 5, 6};
    return year == 2026 && place < 3 ? y2026[place] : other[place < 6 ? place : 5];
}

/* The draw of 48 teams in 4 pots of 12. The original's seeding 0x92f230 sorts the slots with 0x4b07b0
 * (0x92f34a), puts the hosts (0x92f370) and the holder (0x92f3f0) first and makes pot 1 of 8; its draw 0x92e940
 * puts each pot one team to a group at random (0x92ea3b, 0x92ead0). Here the same with 12 groups: the hosts first
 * to their groups, then the holder, the rest of pot 1 and every other pot at random, one team per group and
 * pot. No confederation rule (the original has none either beyond its pots by continent byte; the real 2026
 * draw kept nations of one confederation apart, Europe two at most: left out). The result is the original's
 * form: calloc(count, 5) of {club +0 field, group 1 to 12} (0x92e94c), freed by run_group_draw. */
static uint32_t draw48(uint32_t self)
{
    int n = (int16_t)rd16(g_mem, self + SLOT_COUNT);
    uint32_t slots = rd32(g_mem, self + SLOTS);
    if (n > TEAMS) n = TEAMS;
    if (n <= 0 || !readable(slots)) return 0;
    uint32_t buf = game_malloc((uint32_t)n * 6u);
    if (rt_unwinding || !buf) return 0;
    memcpy(g_mem + buf, g_mem + slots, (size_t)n * 6u);
    uint32_t qa[4] = {buf, (uint32_t)n, 6, SLOT_COMPARE};
    call_cdecl(GAME_QSORT, 4, qa);
    if (rt_unwinding) return 0;
    uint32_t order[TEAMS];
    for (int k = 0; k < n; k++) order[k] = rd32(g_mem, buf + (uint32_t)k * 6u);
    game_free(buf);

    uint32_t front[7] = {0};
    int ids[6], hosts = season_intl_host_ids(rd32(g_mem, WC_KEY), year_of(self), ids), seeded_host[6];
    for (int k = 0; k < hosts; k++) {
        front[k] = host_team(ids[k]);
        if (rt_unwinding) return 0;
    }
    uint32_t la[2] = {rd32(g_mem, COMP_RECORDS) + rd32(g_mem, WC_KEY) * COMP_RECORD_SIZE, 0};
    front[hosts] = rt_intl_wc_holder_slot((uint32_t)year_of(self)) ? call_cdecl(LATEST_PLACED, 2, la) : 0;
    if (rt_unwinding) return 0;
    int seeded = 0, host_seeded = 0;
    for (int f = 0; f <= hosts; f++)
        for (int k = seeded; k < n; k++)
            if (same_team(order[k], front[f])) {
                uint32_t t = order[k];
                memmove(order + seeded + 1, order + seeded, (size_t)(k - seeded) * sizeof order[0]);
                order[seeded++] = t;
                if (f < hosts) seeded_host[host_seeded++] = f;      /* the host's place in the table */
                break;
            }

    uint32_t ca[2] = {TEAMS, 5};
    uint32_t res = call_cdecl(GAME_CALLOC, 2, ca);
    if (rt_unwinding || !readable(res)) return 0;
    for (int pot = 0; pot * GROUPS < n; pot++) {
        int fixed = pot == 0 ? host_seeded : 0, taken[GROUPS + 1] = {0}, groups[GROUPS], m = 0;
        for (int h = 0; h < fixed; h++) taken[host_group_of(year_of(self), seeded_host[h])] = 1;
        for (int g = 1; g <= GROUPS; g++)
            if (!taken[g]) groups[m++] = g;
        for (int i = m - 1; i > 0; i--) {
            int j = random_below(i + 1);
            if (rt_unwinding) return 0;
            int t = groups[i];
            groups[i] = groups[j];
            groups[j] = t;
        }
        for (int i = 0, used = 0; i < GROUPS && pot * GROUPS + i < n; i++) {
            int k = pot * GROUPS + i, g = i < fixed ? host_group_of(year_of(self), seeded_host[i]) : groups[used++];
            if (!readable(order[k])) continue;
            wr32(g_mem, res + (uint32_t)k * 5u, rd32(g_mem, order[k]));
            wr8(g_mem, res + (uint32_t)k * 5u + 4u, (uint32_t)g);
        }
    }
    if (logging())
        fprintf(stderr, "recomp: season intl: world_cup %d draw of %d teams in %d groups\n", year_of(self), n,
                GROUPS);
    return res;
}

void o_0092e940(void);

RT_WRAP(0092e940, wc_draw)
{
    uint32_t self = cpu.ecx;
    if (!season_wc48(self)) {
        o_0092e940();
        return;
    }
    uint32_t res = draw48(self);
    if (rt_unwinding) return;
    rt_ret_stdcall(res, 0);
}

/* ---- Predicted positions (vtable +0x5c, 0x92f680) ---- */

/* The original sorts the teams with 0x4b07d0 and gives 32 fixed positions 1, 2, 3, 4, 5 x 4, 9 x 8, 17 x 8, 25 x
 * 8 (0x92f7e9..0x92f99b). With 48 the scale of 32 stays, so the thresholds of +0x70 0x92fae0 and +0x74 0x92fb40
 * (1, 2, 5, 9, 0x11) keep their meaning: 17 x 16 (out in the round of 32), 25 x 16 (out in the groups). */
static const int predicted48[TEAMS] = {1, 2, 3, 4, 5, 5, 5, 5, 9, 9, 9, 9, 9, 9, 9, 9,
                                       17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
                                       25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25};

void o_0092f680(void);

RT_WRAP(0092f680, wc_predicted)
{
    uint32_t self = cpu.ecx, stats = readable(self) ? rd32(g_mem, self + 8u) : 0;
    if (!season_wc48(self) || !stats) {
        o_0092f680();
        return;
    }
    int n = (int16_t)rd16(g_mem, self + SLOT_COUNT);
    if (n > TEAMS) n = TEAMS;
    uint32_t list = n > 0 ? game_malloc((uint32_t)n * 4u) : 0;
    if (rt_unwinding) return;
    if (list) {
        uint32_t slots = rd32(g_mem, self + SLOTS);
        for (int k = 0; k < n; k++) wr32(g_mem, list + 4u * (uint32_t)k, rd32(g_mem, slots + 6u * (uint32_t)k));
        uint32_t qa[4] = {list, (uint32_t)n, 4, CLUB_COMPARE};
        call_cdecl(GAME_QSORT, 4, qa);
        if (rt_unwinding) return;
        for (int k = 0; k < n; k++) {
            uint32_t club = rd32(g_mem, list + 4u * (uint32_t)k);
            if (!club) continue;
            uint32_t pa[2] = {club, (uint32_t)predicted48[k]};
            call_this(SET_FINAL_POSITION, stats, 2, pa);
            if (rt_unwinding) return;
        }
        for (int k = 0; k < n; k++) {
            uint32_t club = rd32(g_mem, list + 4u * (uint32_t)k);
            if (!readable(club)) continue;
            uint32_t ta[2] = {rd32(g_mem, club + 0x53u), rd32(g_mem, self + 4u)};
            call_this(REP_TEAM, rd32(g_mem, INTL_REPUTATION), 2, ta);
            if (rt_unwinding) return;
        }
        game_free(list);
        if (rt_unwinding) return;
    }
    rt_ret_stdcall(cpu.eax, 0);
}

/* ---- Final positions (vtable +0x68, 0x92f9e0) ---- */

void o_0092f9e0(void);

/* thiscall (club, stage as a byte, a, b, c), ret 0x14. The original maps the places of a group stage (stage
 * below 7, 0x92f9ee) to positions and passes the knockout's on. It loses this at 0x92f9e4, so the stage test
 * cannot be a site. With 48 teams the group stages 7 to 10 take the group branch and the knockout 11 the
 * knockout branch. */
RT_WRAP(0092f9e0, wc_positions)
{
    if (season_wc48(cpu.ecx)) {
        int stage = (int8_t)rd8(g_mem, cpu.esp + 8u);
        if (stage >= 7 && stage < KO) wr8(g_mem, cpu.esp + 8u, 0);
        else if (stage == KO) wr8(g_mem, cpu.esp + 8u, 7);
    }
    o_0092f9e0();
}

/* ---- The knockout stage (0x92d770) ---- */

static uint32_t table_club(uint32_t stage, int pos)
{
    uint32_t table = readable(stage) ? rd32(g_mem, stage + TABLE) : 0;
    return readable(table) ? rd32(g_mem, table + (uint32_t)pos * ROW_SIZE) : 0;
}

/* The round of 32 in the order of the knockout page's bracket (doc "Sources"): each pair meets, the winners of
 * pairs 0-1, 2-3 and so on meet in the round of 16 (Cup_Stage pairs teams 0-1, 2-3, as the Euro's knockout of
 * 24). W, R: winner and runner-up of a group (0 = A). T: the third that meets the winner of a group (thirds
 * table column: A 0, B 1, D 2, E 3, G 4, I 5, K 6, L 7). */
enum { W, R, T };
static const struct { signed char kind, group; } bracket32[32] = {
    {W, 4}, {T, 3}, {W, 8}, {T, 5},             /* 74: 1E v 3, 77: 1I v 3 */
    {R, 0}, {R, 1}, {W, 5}, {R, 2},             /* 73: 2A v 2B, 75: 1F v 2C */
    {R, 10}, {R, 11}, {W, 7}, {R, 9},           /* 83: 2K v 2L, 84: 1H v 2J */
    {W, 3}, {T, 2}, {W, 6}, {T, 4},             /* 81: 1D v 3, 82: 1G v 3 */
    {W, 2}, {R, 5}, {R, 4}, {R, 8},             /* 76: 1C v 2F, 78: 2E v 2I */
    {W, 0}, {T, 0}, {W, 11}, {T, 7},            /* 79: 1A v 3, 80: 1L v 3 */
    {W, 9}, {R, 7}, {R, 3}, {R, 6},             /* 86: 1J v 2H, 88: 2D v 2G */
    {W, 1}, {T, 1}, {W, 10}, {T, 6},            /* 85: 1B v 3, 87: 1K v 3 */
};

/* The knockout of 48 teams. The 32-team original (0x92d770) takes the top two of the 8 groups in fixed pairs
 * and makes a Cup_Stage of 16 at stage[7]. The best thirds are ranked as copa_america::create_knockout_stage
 * 0x5e18a0 does (and the Euro of 24, season_intl_euro.c): a League_Stage of the thirds with no fixtures, row 2
 * of each group table copied in (+5 and +9 cleared: the blocks stay with the group table), sorted with
 * 0x6827d0. The eight best go through; the table of Annex C gives their opponents. The news of each group:
 * vtable +0x94, then +0x40 with code 6 and one more place (+0xbe) for a third that goes through, -2 for a third
 * that is out and for the fourth (group codes 6 and 0xfe, 0x92cd00, 0x92cd25). A third that is out gets the
 * final position of a team out in the groups: place 4 maps to 0x19 (25, 0x92faa8..0x92fac8). */
static void knockout48(uint32_t self)
{
    uint32_t a1[1] = {0};
    uint32_t bodies = rd32(g_mem, rd32(g_mem, RULING_BODIES));
    call_this(BODY_UPDATE, rd32(g_mem, bodies + 4u * (uint32_t)(int8_t)rd8(g_mem, self + 0x50u)), 1, a1);
    if (rt_unwinding) return;

    uint32_t g[GROUPS];
    uint32_t stages = rd32(g_mem, self + 0xcu);
    if ((int)rd32(g_mem, self + 0x2cu) <= KO || !readable(stages)) {
        fprintf(stderr, "recomp: season intl: world_cup %d: %d stages, no room for the knockout\n", year_of(self),
                (int)rd32(g_mem, self + 0x2cu));
        return;
    }
    g[0] = self;
    for (int i = 1; i < GROUPS; i++) g[i] = rd32(g_mem, stages + 4u * (uint32_t)(i - 1));

    /* Rank the thirds */
    int rank_group[GROUPS];
    for (int i = 0; i < GROUPS; i++) rank_group[i] = i;
    uint32_t na[1] = {0xee};
    uint32_t rank = call_cdecl(GAME_NEW, 1, na);
    if (rt_unwinding) return;
    if (readable(rank)) {
        uint32_t year = (uint32_t)year_of(self);
        uint32_t la[22] = {self, GROUPS, 0, 0, rd32(g_mem, self + 4u), 0, 0, rd8(g_mem, self + 0xc2u),
                           rd8(g_mem, self + 0xc3u), 0, self + 0xc5u, self + 0xbeu, year, 0xfffffffeu, 0, 0, 1,
                           0, 2, 0xffffffffu, 0, 2};
        rank = call_this(LEAGUE_STAGE_NEW, rank, 22, la);
        if (rt_unwinding) return;
    }
    if (readable(rank)) {
        uint32_t dst = rd32(g_mem, rank + TABLE);
        for (int i = 0; i < GROUPS && readable(dst); i++) {
            uint32_t src = rd32(g_mem, g[i] + TABLE);
            if (!readable(src)) continue;
            memmove(g_mem + dst + (uint32_t)i * ROW_SIZE, g_mem + src + 2u * ROW_SIZE, ROW_SIZE);
            wr32(g_mem, dst + (uint32_t)i * ROW_SIZE + 5u, 0);
            wr32(g_mem, dst + (uint32_t)i * ROW_SIZE + 9u, 0);
        }
        call_this(TABLE_SORT, rank, 1, a1);
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
        uint32_t da[1] = {1};
        vcall(rank, 0, 1, da);
        if (rt_unwinding) return;
    }
    unsigned mask = 0;
    for (int p = 0; p < 8; p++) mask |= 1u << rank_group[p];

    for (int p = 8; p < GROUPS; p++) {
        uint32_t pa[5] = {table_club(g[rank_group[p]], 2), 0xffffffffu, 4, 4, 4};
        if (pa[0]) vcall(self, 0x68u, 5, pa);
        if (rt_unwinding) return;
    }
    for (int i = 0; i < GROUPS; i++) {
        vcall(g[i], 0x94u, 0, NULL);
        if (rt_unwinding) return;
        uint32_t index = (uint32_t)(i - 1);
        uint32_t third = table_club(g[i], 2);
        int through = (mask >> i) & 1u;
        if (through) wr8(g_mem, g[i] + 0xbeu, rd8(g_mem, g[i] + 0xbeu) + 1u);
        uint32_t ta[5] = {third, through ? 6u : 0xfffffffeu, index, 0, 0};
        vcall(g[i], 0x40u, 5, ta);
        if (rt_unwinding) return;
        uint32_t fa[5] = {table_club(g[i], 3), 0xfffffffeu, index, 0, 0};
        vcall(g[i], 0x40u, 5, fa);
        if (rt_unwinding) return;
    }

    /* The thirds against the group winners: the row of Annex C for the mask, else the ranking order */
    int vs[8];
    const SeasonWcThirds *row = NULL;
    for (int r = 0; r < 495; r++)
        if (season_wc48_thirds[r].mask == mask) row = &season_wc48_thirds[r];
    for (int k = 0; k < 8; k++) vs[k] = row ? row->vs[k] : rank_group[k];
    uint32_t teams = game_malloc(32u * 4u);
    uint32_t out = game_malloc(8);
    if (rt_unwinding) return;
    if (!teams || !out) {
        if (teams) game_free(teams);
        if (out) game_free(out);
        return;
    }
    for (int k = 0; k < 32; k++) {
        int grp = bracket32[k].group, place = bracket32[k].kind;
        uint32_t club = place == T ? table_club(g[vs[grp]], 2) : table_club(g[grp], place == W ? 0 : 1);
        wr32(g_mem, teams + 4u * (uint32_t)k, club);
    }

    uint32_t xa[4] = {KO, out, out + 4u, 0};
    uint32_t tmpl = vcall(self, 0x3cu, 4, xa);
    if (rt_unwinding) return;
    uint32_t ba[1] = {0xb2};
    uint32_t cup = call_cdecl(GAME_NEW, 1, ba);
    if (rt_unwinding) return;
    if (readable(cup)) {
        uint32_t ca[15] = {self, 32, teams, (uint32_t)(int16_t)rd16(g_mem, out), rd32(g_mem, self + 4u), tmpl,
                           (uint32_t)year_of(self), KO, 4, (uint32_t)(int16_t)rd16(g_mem, out + 4u), 0x14, 1, 0,
                           0, 0};
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
    if (!cup) {
        fprintf(stderr, "recomp: season intl: world_cup %d: no knockout stage\n", year_of(self));
        return;
    }
    call_this(CUP_START, cup, 1, a1);
    if (rt_unwinding) return;
    wr32(g_mem, self + 0x30u, KO);
    if (logging()) {
        char thirds[GROUPS + 1];
        for (int p = 0; p < 8; p++) thirds[p] = (char)('A' + rank_group[p]);
        thirds[8] = 0;
        fprintf(stderr, "recomp: season intl: world_cup %d round of 32, best thirds of groups %s, option %d\n",
                year_of(self), thirds, row ? (int)(row - season_wc48_thirds) + 1 : 0);
    }
}

void o_0092d770(void);

/* WorldCup knockout stage, thiscall (), from the stage step 0x92d4b0 (0x92d534) when the stage index reaches ko
 * (site 0x92d52c). */
RT_WRAP(0092d770, wc_knockout)
{
    uint32_t self = cpu.ecx;
    if (!season_wc48(self)) {
        o_0092d770();
        return;
    }
    knockout48(self);
    if (rt_unwinding) return;
    rt_ret_stdcall(cpu.eax, 0);
}
