/* The European Championship of 24 teams (0.6.0 slice 27): from Euro 2016 the finals have 6 groups of 4. The
 * top two of each group and the four best third-placed teams go to a round of 16, then quarter-finals,
 * semi-finals and the final (no third place). The edition table (season_intl_table.c) gives the team count of
 * each edition. The base game (season_intl_on() 0) and every edition of 16 teams take the original path: each
 * site and wrap here gives the original's value or calls the original then.
 *
 * The class euro_champ (ctor 0x57a890, vtable 0x96a020) of the original. The League object itself is group A
 * (template index -1). stage[0..2] (+0xc, count +0x2c) are groups B to D (League_Stage, group byte = index +
 * 2, 0x57b910 at 0x57b996), stage[3] is the Cup_Stage of 8 (0x57bb40). With 24 teams stage[0..4] are groups
 * B to F and stage[5] is the Cup_Stage of 16. The knockout stage index (3 or 5) is "ko" below.
 *
 * Sites in tools/recomp/seasons.py ("Euro of 24 teams"): the stage array and count (0x57a949, 0x57a951), the
 * team slots (0x57aa72, 0x57bd8b, 0x57bd93), the draw scans (0x57b60b, 0x57b9cb), the group stages of the draw
 * (0x57c2e5), the stage tests (0x57b8fc, 0x57b1d5, 0x57b334), the reads of the knockout stage (0x57b736,
 * 0x57b255) and the new stage array after the step of start_new_season (0x57b7cf).
 *
 * Research: docs/research/2026-10-18-international.md and the slice 27 Euro design (agent report of
 * 2026-10-18). The bracket and the third-place table: UEFA Euro 2016 regulations, as listed at
 * https://en.wikipedia.org/wiki/UEFA_Euro_2016_knockout_stage (read 2026-10-18). The regulations themselves
 * (https://www.uefa.com/MultimediaFiles/Download/Regulations/uefaorg/Regulations/02/09/60/38/2096038_DOWNLOAD.pdf)
 * were not read: second source needed. */
#include "win.h"
#include "season_intl.h"
#include "euro_finals.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EURO_KEY 0x9cf7a4u              /* key global of the European Championship */
#define COMP_RECORDS 0xae23d0u          /* ClubComp records, 0x6b bytes each */
#define COMP_RECORD_SIZE 0x6bu
#define NATIONS 0xae23a8u               /* nation records, 0x122 bytes */
#define NATION_SIZE 0x122u
#define CONTINENTS 0xae23a4u            /* continent records, 0xc6 bytes (0x57cab6..0x57cac6) */
#define CONTINENT_SIZE 0xc6u
#define CONTINENT_EUROPE 0x9cfa10u      /* key continent id (0x57caae) */
#define HOSTS 0xb5e134u                 /* HostCountry */
#define INTL_REPUTATION 0xb74318u       /* object of 0x775070 and 0x775220 (0x57cac9, 0x57cbfe) */
#define RULING_BODIES 0xae28f0u         /* object list read at 0x57bb5e */

#define GAME_NEW 0x00944cf1u            /* operator new, cdecl (size) */
#define GAME_MALLOC 0x00944e46u         /* malloc, cdecl (size) */
#define GAME_CALLOC 0x00945501u         /* calloc, cdecl (count, size): 0x57c341 */
#define GAME_FREE 0x009452cau           /* free, cdecl (block) */
#define GAME_QSORT 0x00945333u          /* qsort, cdecl (base, count, size, compare) */
#define GAME_RANDOM 0x0090cfc0u         /* random, cdecl (n): 0 to n - 1 (0x57c416 indexes 4 bytes with n 4) */
#define NATIONAL_TEAM 0x0053b3d0u       /* national team club of a nation record, cdecl (nation) */
#define LATEST_PLACED 0x004afd80u       /* comp_util_get_latest_placed_club, cdecl (record, place) */
#define FIND_HOSTS 0x005fa730u          /* HostCountry::find_hosts, thiscall (key, year, &host1, &host2, 1) */
#define SLOT_COMPARE 0x004b07b0u        /* compare of 6-byte team slots (seeding, 0x57c7b0) */
#define CLUB_COMPARE 0x004b07d0u        /* compare of club pointers (predicted positions, 0x57cafa) */
#define SET_FINAL_POSITION 0x004a2540u  /* Comp_Stats::set_club_final_position, thiscall (club, position) */
#define REP_START 0x00775070u           /* thiscall (continent record, 1): 0x57cad0 */
#define REP_TEAM 0x00775220u            /* thiscall (nation, comp record): 0x57cc04 */
#define BODY_UPDATE 0x007e9180u         /* thiscall (0): 0x57bb70 */
#define LEAGUE_STAGE_NEW 0x0068b100u    /* League_Stage ctor, thiscall, 22 arguments (ret 0x58 at 0x68b4c1) */
#define CUP_STAGE_NEW 0x00522e00u       /* Cup_Stage ctor, thiscall, 15 arguments (ret 0x3c at 0x5230b9) */
#define TABLE_SORT 0x006827d0u          /* League table sort, thiscall (0) */
#define CUP_START 0x0051c800u           /* thiscall (0): 0x57bd56 */
#define ROUND_FIRST_DATE 0x00521e60u    /* cdecl (rec, round, day, month0, year offset, weekday, year) */
#define ROUND_LAST_DATE 0x00521eb0u     /* cdecl (rec, round, day, month0, year offset, weekday, x, year, y) */
#define ROUND_SLOT 0x00521f10u          /* cdecl (rec, round, slot 0 to 7, weekday, time, 3, flag) */

#define TABLE 0xb1u                     /* League table: rows of 0x3b bytes, +0 the club (0x6826d0) */
#define ROW_SIZE 0x3bu
#define ROUND_SIZE 0x68u                /* knockout round record */

#define TEAMS 24
#define GROUPS 6
#define KO 5                            /* knockout stage index with 24 teams (3 with 16) */

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

/* 1 when the edition at +0x40 is one of 24 teams in a season game. */
static int euro24(uint32_t self)
{
    if (!season_intl_on() || !readable(self)) return 0;
    const SeasonIntlEdition *e = season_intl_rule_edition("euro_champ", year_of(self));
    return e && e->teams == TEAMS;
}

/* ---- Sites of seasons.py ---- */

/* The team count: 16 in the original (slots 0x57bd8b, 0x57bd93, the ctor's 0x57aa72, the draw scans 0x57b60b,
 * 0x57b9cb). */
uint32_t rt_intl_euro_teams(uint32_t self, uint32_t original)
{
    return euro24(self) ? (uint32_t)TEAMS : original;
}

/* The knockout stage index: 3 in the original (0x57b8fc, 0x57c2e5, 0x57b1d5, 0x57b334, and 4 times it at
 * 0x57b736, 0x57b255). The stage count is one more (0x57a949, 0x57a951). */
uint32_t rt_intl_euro_ko(uint32_t self, uint32_t original)
{
    return euro24(self) ? (uint32_t)KO : original;
}

/* start_new_season 0x57b750 deletes the stages (0x57b7b2..0x57b7c8) but keeps the stage array +0xc and the
 * count +0x2c of the ctor (0x57a958). After the step of the year (0x57b7ca) an edition of another size gets a
 * new array, here at 0x57b7cf. A save keeps the count: the League load reads +0x2c and makes the array from
 * it (0x688e11, 0x688e53). */
void rt_intl_euro_stages(uint32_t self)
{
    if (!season_intl_on() || !readable(self)) return;
    uint32_t want = rt_intl_euro_ko(self, 3u) + 1u, have = rd32(g_mem, self + 0x2cu);
    if (have == want) return;
    uint32_t stages = game_malloc(4u * want);
    if (rt_unwinding || !stages) return;
    game_free(rd32(g_mem, self + 0xcu));
    if (rt_unwinding) return;
    wr32(g_mem, self + 0xcu, stages);
    wr32(g_mem, self + 0x2cu, want);
    if (logging())
        fprintf(stderr, "recomp: season intl: euro_champ %d has %u stages\n", year_of(self), want);
}

/* ---- The template function (vtable +0x3c, 0x57ab90) ---- */

static void first_date(uint32_t rec, int round, int day, int month0, int weekday, int year)
{
    uint32_t a[7] = {rec, (uint32_t)round, (uint32_t)day, (uint32_t)month0, 0, (uint32_t)weekday, (uint32_t)year};
    call_cdecl(ROUND_FIRST_DATE, 7, a);
}

static void last_date(uint32_t rec, int round, int day, int month0, int weekday, int year, int y)
{
    uint32_t a[9] = {rec, (uint32_t)round, (uint32_t)day, (uint32_t)month0, 0, (uint32_t)weekday, 1,
                     (uint32_t)year, (uint32_t)y};
    call_cdecl(ROUND_LAST_DATE, 9, a);
}

static void slot(uint32_t rec, int round, int index, int weekday, int time, int c, uint32_t flag)
{
    uint32_t a[7] = {rec, (uint32_t)round, (uint32_t)index, (uint32_t)weekday, (uint32_t)time, (uint32_t)c, flag};
    call_cdecl(ROUND_SLOT, 7, a);
}

/* The fields of a round record as the original writes them after the dates and slots (quarter-final
 * 0x57aff4..0x57b037, semi-final 0x57b0a2..0x57b0ff, final 0x57b137..0x57b1a6, World Cup round of 16
 * 0x92c95d..0x92c9a0). +7: a value per round (meaning unknown: 0x78, 0x82, 0x96 in the Euro, 0x14 for the
 * World Cup round of 16). +9: the round index (0 to 4 in the World Cup). +0xd: the rule flags (sweep section
 * 7). +0x18, +0x1a: teams and matches. +0x1c: the teams of the first round, 0 for later rounds. */
static void round_fields(uint32_t rec, int round, int value, int teams, int first, int year, uint32_t flags)
{
    uint32_t r = rec + (uint32_t)round * ROUND_SIZE;
    wr16(g_mem, r + 7u, (uint32_t)value);
    wr16(g_mem, r + 9u, (uint32_t)round);
    wr16(g_mem, r + 0xbu, 0);
    wr16(g_mem, r + 0xdu, rt_intl_round_flags(INTL_EURO, (uint32_t)year, flags));
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

/* The knockout of 24 teams: 4 rounds. Dates (month from 0, 5 = June): the original's groups play from 9, 15
 * and 19 June (0x57ac35, 0x57ac60, 0x57ac8c) and its quarter-finals from 20 June (0x57af79). Here the round of
 * 16 plays from 23 to 27 June with the 8 kick-off slots of the World Cup round of 16 (0x92c8c0..0x92c958), and
 * the quarter-finals, semi-finals and final keep the original's slots and weekday flags 10 days later (final 7
 * to 10 July). Inference: the real Euro 2016 played 25 June to 10 July. The table's first day moves all dates
 * (the date shift of the caller). */
static uint32_t knockout_template(uint32_t self, uint32_t out_rounds, uint32_t out_id, uint32_t out_flag)
{
    if (readable(out_flag)) wr32(g_mem, out_flag, 0);
    if (readable(out_rounds)) wr16(g_mem, out_rounds, 4);
    if (readable(out_id)) wr16(g_mem, out_id, 0);
    uint32_t rec = game_malloc(4u * ROUND_SIZE);
    if (rt_unwinding || !rec) return 0;
    int y = year_of(self);
    /* Round of 16 */
    first_date(rec, 0, 23, 5, 4, y);
    last_date(rec, 0, 27, 5, 1, y, 1);
    slot(rec, 0, 0, 5, 1, 3, 0x40);
    slot(rec, 0, 1, 5, 2, 3, 0x80);
    slot(rec, 0, 2, 6, 2, 3, 0x200);
    slot(rec, 0, 3, 6, 1, 3, 4);
    slot(rec, 0, 4, 0, 2, 3, 0x4000);
    slot(rec, 0, 5, 0, 1, 3, 0x800);
    slot(rec, 0, 6, 1, 2, 3, 0x1000);
    slot(rec, 0, 7, 1, 1, 3, 0x2000);
    round_fields(rec, 0, 0x14, 16, 1, y, 0x383);
    /* Quarter-finals (the original's 0x57af79..0x57aff4) */
    first_date(rec, 1, 30, 5, 3, y);
    last_date(rec, 1, 2, 6, 5, y, 1);
    slot(rec, 1, 0, 5, 1, 3, 0x40);
    slot(rec, 1, 1, 5, 2, 3, 4);
    slot(rec, 1, 2, 6, 2, 3, 0x80);
    slot(rec, 1, 3, 6, 1, 3, 0x100);
    slot(rec, 1, 4, -1, -1, -1, 0);
    round_fields(rec, 1, 0x78, 8, 0, y, 0x383);
    /* Semi-finals (0x57b048..0x57b0a2) */
    first_date(rec, 2, 4, 6, 0, y);
    last_date(rec, 2, 6, 6, 2, y, 1);
    slot(rec, 2, 0, 2, 2, 3, 0x40);
    slot(rec, 2, 1, 2, 1, 3, 0x80);
    slot(rec, 2, 2, -1, -1, -1, 0);
    round_fields(rec, 2, 0x82, 4, 0, y, 0x383);
    /* Final (0x57b114..0x57b1a6): no slots in the original */
    first_date(rec, 3, 7, 6, 3, y);
    last_date(rec, 3, 10, 6, 6, y, 4);
    round_fields(rec, 3, 0x96, 2, 0, y, 0x283);
    return rt_unwinding ? 0 : rec;
}

/* The template function, thiscall (index as a byte, short *rounds, short *id, int *flag), ret 0x10 (0x57ab90
 * to 0x57b1b7). Called by the wrap of 0x57ab90 in season_intl.c inside its date shift, at the entry state
 * (cpu.esp at the return address, this in cpu.ecx). It always completes the call and gives the result.
 *
 * 24 teams: index -1 to 4 are the groups A to F. The original has 4 group templates (jump table 0x57b1bc,
 * cases -1 to 2, index 3 is its knockout at 0x57aba2), so groups E and F take the templates of A and B
 * (index ((i + 1) % 4) - 1). Their kick-off flags are then those of A and B (inference: the flags place the
 * matches of a matchday). The id that the original writes (index + 0x3e9 at 0x57abd0..0x57abe0) is written for
 * the real index: the texts of 0x3ec and 0x3ed are unknown. Index 5 is the knockout above. */
uint32_t season_intl_euro_template(void (*original)(void))
{
    uint32_t self = cpu.ecx, sp = cpu.esp;
    int idx = (int8_t)rd8(g_mem, sp + 4u);
    if (!euro24(self) || idx < -1 || idx > KO) {
        original();
        return cpu.eax;
    }
    if (idx < KO) {
        int mapped = (idx + 1) % 4 - 1;
        uint32_t out_id = rd32(g_mem, sp + 0xcu);
        wr8(g_mem, sp + 4u, (uint32_t)mapped);
        original();
        if (rt_unwinding) return cpu.eax;
        if (mapped != idx && readable(out_id)) wr16(g_mem, out_id, (uint32_t)(idx + 0x3e9));
        return cpu.eax;
    }
    uint32_t rec = knockout_template(self, rd32(g_mem, sp + 8u), rd32(g_mem, sp + 0xcu), rd32(g_mem, sp + 0x10u));
    if (rt_unwinding) return 0;
    rt_ret_stdcall(rec, 4);
    return rec;
}

/* ---- The draw (0x57c320) and the seeding (0x57c710) ---- */

static uint32_t club_nation(uint32_t club) { return readable(club) ? rd32(g_mem, club + 0x53u) : 0; }

static int same_team(uint32_t a, uint32_t b)
{
    return a && b && (a == b || (club_nation(a) && club_nation(a) == club_nation(b)));
}

static uint32_t host_team(int id)
{
    if (id < 0) return 0;
    uint32_t a[1] = {rd32(g_mem, NATIONS) + (uint32_t)id * NATION_SIZE};
    return call_cdecl(NATIONAL_TEAM, 1, a);
}

static int random_below(int n)
{
    uint32_t a[1] = {(uint32_t)n};
    int r = (int16_t)call_cdecl(GAME_RANDOM, 1, a);
    return r < 0 || r >= n ? 0 : r;
}

/* The draw of 24 teams in 4 pots of 6. The original's seeding 0x57c710 sorts the slots with 0x4b07b0
 * (0x57c7cd), puts the hosts of find_hosts (0x57c7f3) and the holder (place 0 of the history, 0x57c873) first
 * and gives pot i / 4 (0x57c9c4). Its draw 0x57c320 puts pot 1 in groups 1 to 4 in order (0x57c3b7..0x57c3e6)
 * and the others at random, one per group and pot (0x57c403..0x57c46b). Here the same rules with pot i / 6:
 * the first seed (the host) goes to group 1, the rest of pot 1 and every other pot at random, one team per
 * group and pot. The result is the original's: calloc(count, 5) of {club index, group 1 to 6}, freed by
 * run_group_draw (0x57c2f8). The seeding is done at the draw, because the pot array +0xee of the object holds
 * 16 entries only (the object has 0x142 bytes, 0x830cb3, and +0x13e follows the array) and the save writes 16
 * (0x57c492, 0x57c5d9). */
static uint32_t draw24(uint32_t self)
{
    int n = (int16_t)rd16(g_mem, self + 0x36u);
    uint32_t slots = rd32(g_mem, self + 0x14u);
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

    uint32_t front[3] = {0, 0, 0};
    uint32_t out = game_malloc(8);
    if (rt_unwinding) return 0;
    if (out) {
        wr32(g_mem, out, 0xffffffffu);
        wr32(g_mem, out + 4u, 0xffffffffu);
        uint32_t fa[5] = {rd32(g_mem, EURO_KEY), (uint32_t)year_of(self), out, out + 4u, 1};
        call_this(FIND_HOSTS, rd32(g_mem, HOSTS), 5, fa);
        if (rt_unwinding) return 0;
        front[0] = host_team((int32_t)rd32(g_mem, out));
        front[1] = host_team((int32_t)rd32(g_mem, out + 4u));
        game_free(out);
    }
    uint32_t la[2] = {rd32(g_mem, COMP_RECORDS) + rd32(g_mem, EURO_KEY) * COMP_RECORD_SIZE, 0};
    front[2] = call_cdecl(LATEST_PLACED, 2, la);
    if (rt_unwinding) return 0;
    int seeded = 0;
    for (int f = 0; f < 3; f++)
        for (int k = seeded; k < n; k++)
            if (same_team(order[k], front[f])) {
                uint32_t t = order[k];
                memmove(order + seeded + 1, order + seeded, (size_t)(k - seeded) * sizeof order[0]);
                order[seeded++] = t;
                break;
            }

    uint32_t ca[2] = {TEAMS, 5};
    uint32_t res = call_cdecl(GAME_CALLOC, 2, ca);
    if (rt_unwinding || !readable(res)) return 0;
    for (int pot = 0; pot * GROUPS < n; pot++) {
        int groups[GROUPS], m = 0;
        for (int g = 1; g <= GROUPS; g++)
            if (!(pot == 0 && g == 1)) groups[m++] = g;
        for (int i = m - 1; i > 0; i--) {                    /* random order of the free groups */
            int j = random_below(i + 1);
            if (rt_unwinding) return 0;
            int t = groups[i];
            groups[i] = groups[j];
            groups[j] = t;
        }
        for (int i = 0, used = 0; i < GROUPS && pot * GROUPS + i < n; i++) {
            int k = pot * GROUPS + i, g = (pot == 0 && i == 0) ? 1 : groups[used++];
            /* The entry holds the club's own +0 field, as the seeding writes its pots (0x57c9b8..0x57c9be):
             * the group builders take the club as [0xae23b4] + that value * 0x245 (0x57b5e4..0x57b5fd). An
             * empty slot (the fill found no team) gives no entry: its group stays one short. */
            if (!readable(order[k])) continue;
            wr32(g_mem, res + (uint32_t)k * 5u, rd32(g_mem, order[k]));
            wr8(g_mem, res + (uint32_t)k * 5u + 4u, (uint32_t)g);
        }
    }
    if (logging())
        fprintf(stderr, "recomp: season intl: euro_champ %d draw of %d teams in %d groups\n", year_of(self), n,
                GROUPS);
    return res;
}

void o_0057c320(void);

/* EuroChamp draw (pots), thiscall (), returns the draw array (0x57c1db). */
RT_WRAP(0057c320, euro_pots)
{
    uint32_t self = cpu.ecx;
    if (!euro24(self)) {
        o_0057c320();
        return;
    }
    uint32_t res = draw24(self);
    if (rt_unwinding) return;
    rt_ret_stdcall(res, 0);
}

void o_0057c710(void);

/* EuroChamp seeding, thiscall (), on the seeding day (0x57c698). With 24 teams it writes nothing: its pot array
 * +0xee holds 16 (see draw24), and the draw sorts the teams itself. */
RT_WRAP(0057c710, euro_seeding)
{
    uint32_t self = cpu.ecx;
    season_intl_euro_fill(self);            /* the pots take the slots: no empty slot may go in (season_intl.c) */
    if (rt_unwinding) return;
    cpu.ecx = self;
    if (!euro24(self)) {
        /* The 16-team path: the fill of a short list of the staff reserve fix (euro_finals.c), also in the base
         * game, then the original. */
        euro_finals_fill_short_list(self);
        if (rt_unwinding) return;
        cpu.ecx = self;
        o_0057c710();
        return;
    }
    /* 24 teams: no pots here (the draw sorts the teams, draw24). The pot array of 16 entries of 5 bytes at
     * +0xee, which the save writes (0x57c5d9) and the load reads (0x57c4a5), is cleared, so it keeps no clubs
     * of an earlier 16-team edition. */
    memset(g_mem + self + 0xeeu, 0, 16u * 5u);
    rt_ret_stdcall(cpu.eax, 0);
}

/* ---- Predicted positions (vtable +0x5c, 0x57c9f0) ---- */

/* The original sorts the teams with 0x4b07d0 (0x57cb03) and gives 16 fixed positions 1, 2, 3, 3, 5 x 4, 9 x 4,
 * 13 x 4 (0x57cb10..0x57cbe5). With 24 teams the positions keep the scale of 16: 1, 2, 3, 3, 5 x 4, 9 x 8 (out
 * in the round of 16), 13 x 8 (out in the groups). So the thresholds 5 and 9 of 0x57cd30 and 0x57cd80 keep
 * their meaning (inference: they rank a nation's result against its predicted place). */
static const int predicted24[TEAMS] = {1, 2, 3, 3, 5, 5, 5, 5, 9, 9, 9, 9, 9, 9, 9, 9,
                                       13, 13, 13, 13, 13, 13, 13, 13};

void o_0057c9f0(void);

RT_WRAP(0057c9f0, euro_predicted)
{
    uint32_t self = cpu.ecx, stats = readable(self) ? rd32(g_mem, self + 8u) : 0;
    /* The original gives a position to each of its 16 slots (0x57cb10..0x57cbe5) and stops at the box
     * "comp_stats.CPP 1664" for an empty one (0x4a2540, return 0x57cbea): a 2023 start with France met it on
     * 9 February 2024 on a build without this file. The slots are filled first, on both paths. */
    season_intl_euro_fill(self);
    if (rt_unwinding) return;
    cpu.ecx = self;
    if (!euro24(self) || !stats) {
        o_0057c9f0();
        return;
    }
    int n = (int16_t)rd16(g_mem, self + 0x36u);
    if (n > TEAMS) n = TEAMS;
    uint32_t list = n > 0 ? game_malloc((uint32_t)n * 4u) : 0;
    if (rt_unwinding) return;
    if (list) {
        uint32_t body = rd32(g_mem, INTL_REPUTATION);
        uint32_t ra[2] = {rd32(g_mem, CONTINENTS) + rd32(g_mem, CONTINENT_EUROPE) * CONTINENT_SIZE, 1};
        call_this(REP_START, body, 2, ra);
        if (rt_unwinding) return;
        uint32_t slots = rd32(g_mem, self + 0x14u);
        for (int k = 0; k < n; k++) wr32(g_mem, list + 4u * (uint32_t)k, rd32(g_mem, slots + 6u * (uint32_t)k));
        uint32_t qa[4] = {list, (uint32_t)n, 4, CLUB_COMPARE};
        call_cdecl(GAME_QSORT, 4, qa);
        if (rt_unwinding) return;
        for (int k = 0; k < n; k++) {
            uint32_t club = rd32(g_mem, list + 4u * (uint32_t)k);
            if (!club) continue;
            uint32_t pa[2] = {club, (uint32_t)predicted24[k]};
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

/* ---- Final positions (vtable +0x68, 0x57cc30) ---- */

void o_0057cc30(void);

/* thiscall (club, stage as a byte, a, b, c), ret 0x14. The original maps the places of a group stage (stage
 * below 3, 0x57cc3e) to positions 1, 9, 13 and passes the knockout's on. It loses this at 0x57cc34, so the
 * stage test cannot be a site. With 24 teams the group stages 3 and 4 take the group branch and the knockout 5
 * takes the knockout branch. */
RT_WRAP(0057cc30, euro_positions)
{
    if (euro24(cpu.ecx)) {
        int stage = (int8_t)rd8(g_mem, cpu.esp + 8u);
        if (stage == 3 || stage == 4) wr8(g_mem, cpu.esp + 8u, 0);
        else if (stage == KO) wr8(g_mem, cpu.esp + 8u, 3);
    }
    o_0057cc30();
}

/* ---- The knockout stage (0x57bb40) ---- */

/* The third-placed teams against the group winners (Euro 2016 regulations, see the head of the file). Index:
 * the four groups whose thirds go through, as bits (A = 1). Values: the group of the third that meets the
 * winner of A, B, C and D. Groups 0 to 5 are A to F. */
static const struct { unsigned mask; signed char vs[4]; } thirds_table[15] = {
    {0x0f, {2, 3, 0, 1}},   /* A B C D: 3C 3D 3A 3B */
    {0x17, {2, 0, 1, 4}},   /* A B C E: 3C 3A 3B 3E */
    {0x27, {2, 0, 1, 5}},   /* A B C F: 3C 3A 3B 3F */
    {0x1b, {3, 0, 1, 4}},   /* A B D E: 3D 3A 3B 3E */
    {0x2b, {3, 0, 1, 5}},   /* A B D F: 3D 3A 3B 3F */
    {0x33, {4, 0, 1, 5}},   /* A B E F: 3E 3A 3B 3F */
    {0x1d, {2, 3, 0, 4}},   /* A C D E: 3C 3D 3A 3E */
    {0x2d, {2, 3, 0, 5}},   /* A C D F: 3C 3D 3A 3F */
    {0x35, {2, 0, 5, 4}},   /* A C E F: 3C 3A 3F 3E */
    {0x39, {3, 0, 5, 4}},   /* A D E F: 3D 3A 3F 3E */
    {0x1e, {2, 3, 1, 4}},   /* B C D E: 3C 3D 3B 3E */
    {0x2e, {2, 3, 1, 5}},   /* B C D F: 3C 3D 3B 3F */
    {0x36, {4, 2, 1, 5}},   /* B C E F: 3E 3C 3B 3F */
    {0x3a, {4, 3, 1, 5}},   /* B D E F: 3E 3D 3B 3F */
    {0x3c, {2, 3, 5, 4}},   /* C D E F: 3C 3D 3F 3E */
};

static uint32_t table_club(uint32_t stage, int pos)
{
    uint32_t table = readable(stage) ? rd32(g_mem, stage + TABLE) : 0;
    return readable(table) ? rd32(g_mem, table + (uint32_t)pos * ROW_SIZE) : 0;
}

/* The knockout of 24 teams. The 16-team original (0x57bb40) takes the top two of the 4 groups in fixed pairs
 * and makes a Cup_Stage of 8 at stage[3]. The best thirds follow copa_america::create_knockout_stage 0x5e18a0
 * (and the Asian Cup 0x40fa90): a League_Stage of the thirds with no fixtures (0x5e191e: stage index -2, no
 * team list), row 2 of each group table copied in (0x5e193b..0x5e19bb, rows of 0x3b bytes; the store at 0x5e19b3 is +0x44
 * from the start of the copied row i, so it clears +9 of row i + 1, inference: the original's own off-by-one),
 * sorted (0x5e19c7). Here +5 and +9 of each copy are cleared (the blocks stay with the group table). Then for each group vtable +0x94 (0x5e1ace) and the news of
 * vtable +0x40: code 6 and one more place (+0xbe) for a third that goes through, -2 for a third that is out and
 * for the fourth (0x5e1b27..0x5e1b6a). The comp's vtable +0x68 gives a third that is out its final position
 * (0x5e1aa6: -1, 3, 3, 3). Copa then draws the 8 at random (0x4afb40); here the pairs are the Euro 2016
 * bracket. The Cup_Stage pairs teams 0-1, 2-3 and so on, and the winners in the same order (inference from the
 * 16-team order B1-A2, C1-D2, A1-B2, D1-C2 at 0x57bbbf..0x57bc3e, which matches the real bracket of 1996 to
 * 2012). Round of 16 in match order: 2A-2C, 1D-3BEF, 1B-3ACD, 1F-2E, 1C-3ABF, 1E-2D, 1A-3CDE, 2B-2F. */
static void knockout24(uint32_t self)
{
    uint32_t a1[1] = {0};
    uint32_t bodies = rd32(g_mem, rd32(g_mem, RULING_BODIES));
    call_this(BODY_UPDATE, rd32(g_mem, bodies + 4u * (uint32_t)(int8_t)rd8(g_mem, self + 0x50u)), 1, a1);
    if (rt_unwinding) return;

    uint32_t g[GROUPS];
    uint32_t stages = rd32(g_mem, self + 0xcu);
    if ((int)rd32(g_mem, self + 0x2cu) <= KO || !readable(stages)) {
        fprintf(stderr, "recomp: season intl: euro_champ %d: %d stages, no room for the knockout\n", year_of(self),
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
        rank = call_this(LEAGUE_STAGE_NEW, rank, 22, la);       /* the ctor gives the object (0x5e1923) */
        if (rt_unwinding) return;
    }
    if (readable(rank)) {
        uint32_t dst = rd32(g_mem, rank + TABLE);
        for (int i = 0; i < GROUPS && readable(dst); i++) {
            uint32_t src = rd32(g_mem, g[i] + TABLE);
            if (!readable(src)) continue;
            memmove(g_mem + dst + (uint32_t)i * ROW_SIZE, g_mem + src + 2u * ROW_SIZE, ROW_SIZE);
            /* The row's blocks at +5 and +9 stay with the group table: League::free_table 0x687970 frees both
             * (0x6879b7, 0x6879e4) when the temporary stage is deleted below, and again at the season end
             * (0x57b775), "free of ... not a heap block" in a 2015, 2019 and 2023 start. The sort 0x6827d0
             * reads the points and goals of the row, not these blocks (inference). */
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
        if (used != (1 << GROUPS) - 1)                   /* a third not found: group order */
            for (int i = 0; i < GROUPS; i++) rank_group[i] = i;
        uint32_t da[1] = {1};
        vcall(rank, 0, 1, da);                           /* scalar deleting destructor (0x5e1b8b) */
        if (rt_unwinding) return;
    }
    unsigned mask = 0;
    for (int p = 0; p < 4; p++) mask |= 1u << rank_group[p];

    /* Final positions of the thirds that are out, and the group news */
    for (int p = 4; p < GROUPS; p++) {
        uint32_t pa[5] = {table_club(g[rank_group[p]], 2), 0xffffffffu, 3, 3, 3};
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

    /* The round of 16 */
    int row = -1;
    for (int r = 0; r < 15; r++)
        if (thirds_table[r].mask == mask) row = r;
    uint32_t w[GROUPS], ru[GROUPS], t[4];
    for (int i = 0; i < GROUPS; i++) {
        w[i] = table_club(g[i], 0);
        ru[i] = table_club(g[i], 1);
    }
    for (int k = 0; k < 4; k++)
        t[k] = table_club(g[row >= 0 ? thirds_table[row].vs[k] : rank_group[k]], 2);
    const uint32_t bracket[16] = {ru[0], ru[2], w[3], t[3], w[1], t[1], w[5], ru[4],
                                  w[2], t[2], w[4], ru[3], w[0], t[0], ru[1], ru[5]};
    uint32_t teams = game_malloc(16u * 4u);
    uint32_t out = game_malloc(8);
    if (rt_unwinding) return;
    if (!teams || !out) {                               /* no leak when only one block came */
        if (teams) game_free(teams);
        if (out) game_free(out);
        return;
    }
    for (int k = 0; k < 16; k++) wr32(g_mem, teams + 4u * (uint32_t)k, bracket[k]);

    /* Template (0x57bc4f: index, &rounds, &id, 0) and Cup_Stage (0x57bc9c) */
    uint32_t xa[4] = {KO, out, out + 4u, 0};
    uint32_t tmpl = vcall(self, 0x3cu, 4, xa);
    if (rt_unwinding) return;
    uint32_t ba[1] = {0xb2};
    uint32_t cup = call_cdecl(GAME_NEW, 1, ba);
    if (rt_unwinding) return;
    if (readable(cup)) {
        uint32_t ca[15] = {self, 16, teams, (uint32_t)(int16_t)rd16(g_mem, out), rd32(g_mem, self + 4u), tmpl,
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
        fprintf(stderr, "recomp: season intl: euro_champ %d: no knockout stage\n", year_of(self));
        return;
    }
    call_this(CUP_START, cup, 1, a1);
    if (rt_unwinding) return;
    wr32(g_mem, self + 0x30u, KO);
    if (logging())
        fprintf(stderr, "recomp: season intl: euro_champ %d round of 16, best thirds of groups %c%c%c%c\n",
                year_of(self), 'A' + rank_group[0], 'A' + rank_group[1], 'A' + rank_group[2], 'A' + rank_group[3]);
}

void o_0057bb40(void);

/* EuroChamp knockout stage, thiscall (), from the stage step 0x57b880 (0x57b904) when the stage index reaches
 * ko (site 0x57b8fc). */
RT_WRAP(0057bb40, euro_knockout)
{
    uint32_t self = cpu.ecx;
    if (!euro24(self)) {
        o_0057bb40();
        return;
    }
    knockout24(self);
    if (rt_unwinding) return;
    rt_ret_stdcall(cpu.eax, 0);
}

/* ---- Test log of a fixture club with no nation ---- */

void o_0076c120(void);

/* NationalTeams::select_squad, thiscall (nation, flag, date, fixture, flag), ret 0x14 (from 0x76bc23 with the
 * home team's nation; 0x76c266 inside). Test log only (RECOMP_SEASON_INTL_LOG=1): with no nation it logs the
 * fixture and the clubs of each Euro group table, then calls the original. */
void season_national_after_pick(uint32_t nation, int slot);     /* season_national_fill.c (rule O304) */

RT_WRAP(0076c120, euro_select_squad_log)
{
    uint32_t nation = rt_arg(0);
    int slot = (int8_t)rt_arg(1);
    if (logging() && !rt_arg(0)) {
        uint32_t fx = rt_arg(3), euro = 0, comps = rd32(g_mem, 0xadadfcu);
        fprintf(stderr, "recomp: season intl: select_squad with no nation, fixture %08x:", fx);
        for (uint32_t k = 0; readable(fx) && k < 0x40u; k += 4) fprintf(stderr, " %08x", rd32(g_mem, fx + k));
        fprintf(stderr, "\n");
        if (readable(comps)) euro = rd32(g_mem, comps + rd32(g_mem, EURO_KEY) * 4u);
        if (readable(euro)) {
            int stages = (int)rd32(g_mem, euro + 0x2cu);
            for (int g = -1; g < stages && g < 8; g++) {
                uint32_t obj = g < 0 ? euro : rd32(g_mem, rd32(g_mem, euro + 0xcu) + 4u * (uint32_t)g);
                if (!readable(obj)) continue;
                uint32_t table = rd32(g_mem, obj + TABLE);
                int teams = (int16_t)rd16(g_mem, obj + 0x3eu);
                fprintf(stderr, "recomp: season intl:   stage %d obj %08x teams %d:", g, obj, teams);
                for (int r = 0; readable(table) && r < teams && r < 8; r++) {
                    uint32_t c = rd32(g_mem, table + (uint32_t)r * ROW_SIZE), n = club_nation(c);
                    fprintf(stderr, " %08x(%s)", c, readable(n) ? win_str(n + 4u) : "NO NATION");
                }
                fprintf(stderr, "\n");
            }
        }
    }
    o_0076c120();
    season_national_after_pick(nation, slot);   /* the under-21 squad keeps out the senior squad */
}
