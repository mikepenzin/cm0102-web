/* The CONCACAF World Cup qualifying in the 2026 shape (0.6.2 item 214), season_concacaf26.h.
 *
 * The cause. The CONCACAF section (WcConcacafCup, vtable 0x970c24) kept the original's 2002 shape in every cycle
 * (slice 29 left out its hosts only): three Caribbean knockout rounds (stage -1, create_round_table 0x914de0), two
 * Central American groups (stages 0 and 1, 0x916cd0, made by the constructor at 0x914c0c and start_new_season at
 * 0x916241), a play-off (stage 2, 0x917110), three semi-final groups of 4 (stages 3 to 5, 0x917320) and a final
 * group of 6 that meets twice (stage 6, 0x917830), on the 2002 template moved to the campaign year W - 2. That is
 * 26 match days from March of W - 2 to November of W - 1, with the semi-final groups in the autumn of W - 2. In
 * 2024 they met the 2024-25 CONCACAF Nations League (September to November 2024): 8 Nations League matches within
 * 3 days of a qualifier of the same side (item 214, tests/seasons/nl_wcq_windows_run.py of item 209).
 *
 * Real life (en.wikipedia.org, "2026 FIFA World Cup qualification (CONCACAF)", read 9 November 2026): the 32 teams
 * less the three hosts; the four lowest-ranked in two ties (first round, March 2024); the 28 others and the two
 * winners in six groups of five that meet once (second round, June 2024 and June 2025); the six winners and six
 * runners-up in three groups of four that meet twice (third round, September to November 2025); the three group
 * winners qualify, the two best runners-up go to the play-off tournament. The days: season_concacaf26_days.c.
 *
 * The game plays that shape in the wcq26 gate (season_wcq26: a World Cup of 48 teams, season_intl_on: changing
 * rules on, or any start after 2001) when the pool (+0xb2, the 35 CONCACAF nations less the hosts, select_entrants
 * 0x916250) holds 31 to 36 teams:
 * - build_team_table 0x915d40: the knockout entry table +0xa7 (+0x3e rows) holds the 2 x (pool - 30) teams of the
 *   lowest nation reputation (+0x80 of the national team, as the pool's sort 0x4b0810; the real ranking was FIFA's,
 *   inference), so 30 teams reach the second round;
 * - create_round_table 0x914de0, stage -1: one round of those ties (two legs, the gap of the real days);
 * - 0x916cd0: no Central American groups; the stage count +0x2c becomes 9 and a stage array of 9 is made (+0xc;
 *   the constructor's array holds 7, 0x914ae9; start_new_season sets +0x2c to 7 at 0x9160b8 and keeps the array);
 * - advance_stage 0x917060 (Cup::update_for_date 0x51f890 calls it once the knockout and the stages up to +0x30 are
 *   complete): with +0x30 = -1 the second round, six League_Stages of 5 (0x68b100, each pair once) in stages 0 to
 *   5, drawn in five pots of 6 by reputation (the knockout winners in the last pot, as the real pot 5); with +0x30
 *   = 5 the third round, three League_Stages of 4 (each pair twice) in stages 6 to 8, from the first two of each
 *   group, drawn in four pots of 3 by reputation. Their arguments are the semi-final groups' (0x9174e8, flags 0xf)
 *   and the final group's (0x9179d9, flags 0x14, which gives the direct places status 7);
 * - the staff records 0x915710, the fate news 0x9159b0 and the club progress 0x918040 switch on the stage: the
 *   second round reads as the semi-final groups (3), the third round as the final group (6), so a third-round
 *   winner enters the World Cup as the final group's did (0x915901..0x91596f); the progress text 0x916af0 reads
 *   the stage name code, so a group of the second round gives the semi-final groups' text (0x40c) and a group of
 *   the third round the final group's (0x96);
 * - the direct places: places_of (season_wcq26.c) less the hosts, 3 in 2026; each group takes places / 3 (the
 *   League's places byte) and the best of the next row across the groups take the rest (places % 3) when the
 *   section ends (slot 12 0x90fbe0, called once at 0x51f920); the play-off tournament takes the next two
 *   (season_cc26_tournament).
 * The save keeps +0x2c, +0x30 and the stages (Cup::save_cup_to_disk 0x521532, load 0x520c40 makes the array of the
 * saved count). A campaign made in the original's shape (+0x2c 7) keeps it. */
#include "win.h"
#include "season.h"
#include "season_intl.h"
#include "season_wcq26.h"
#include "season_concacaf26.h"
#include "season_concacaf26_days.h"
#include "season_uefa_days.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VT_CONCACAF 0x00970c24u
#define GAME_DATE 0xae2c90u             /* s16 day of the year, s16 year */

#define F_RECORD 0x4u
#define F_STAGES 0xcu
#define F_STAGE_COUNT 0x2cu
#define F_STAGE 0x30u
#define F_YEAR 0x40u
#define F_ROUNDS 0xa3u                  /* the knockout's round table, 0x68 a round */
#define F_ROWS 0xa7u                    /* the knockout's entry table, 6 bytes a row: +0 team, +5 status */
#define F_ROW_COUNT 0x3eu
#define F_POOL 0xb2u                    /* the pool of national teams (select_entrants 0x916250) */
#define F_POOL_SIZE 0xb6u
#define F_TABLE 0xb1u                   /* a League's table, ROW_SIZE a row */
#define F_TEAMS 0x3eu
#define ROW_SIZE 0x3bu
#define ROW_POINTS 0x19u
#define ROW_FOR 0x15u
#define ROW_AGAINST 0x17u
#define CLUB_REPUTATION 0x80u
#define ROUND_SIZE 0x68u
#define GROUP_ROUND_SIZE 0x41u

#define GAME_NEW 0x00944cf1u            /* operator new, cdecl (size) */
#define GAME_MALLOC 0x00944e46u
#define GAME_FREE 0x009452cau
#define GAME_RANDOM 0x0090cfc0u         /* cdecl (n): 0 to n - 1 */
#define F_LEAGUE_STAGE 0x0068b100u      /* League_Stage::League_Stage, thiscall, 22 arguments */
#define F_GROUP_NEWS 0x00684230u        /* League::send_group_draw_news, thiscall () */
#define F_GROUP_DATE 0x0068a160u        /* league_round_set_date, cdecl (table, round, day, month, year+, weekday, time, year, 0) */
#define F_GROUP_SLOT 0x0068a1c0u        /* league_round_set_match_slot, cdecl (table, round, slot, weekday, kind, option, value) */
#define TABLE_SORT 0x006827d0u          /* League::sort_table, thiscall (1 arg) */

#define STAGES 9
#define SECOND_GROUPS 6
#define SECOND_TEAMS 5
#define THIRD_GROUPS 3
#define THIRD_TEAMS 4
#define SECOND_FIELD 30                 /* SECOND_GROUPS x SECOND_TEAMS */
#define POOL_MAX 40
#define GROUP_NAME 0x41fu               /* "Group A" and on (the name table 0x4b9a44, as season_uefa_cup.c) */
#define CODE_SECOND 0x40cu              /* the semi-final groups' name code (0x91533c) */
#define CODE_THIRD 0x96u                /* the final group's (0x9154cb) */

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
    if (!rt_unwinding && readable(p)) memset(g_mem + p, 0, size);
    return rt_unwinding || !readable(p) ? 0 : p;
}

static void game_free(uint32_t p)
{
    uint32_t a[1] = {p};
    if (readable(p)) call_cdecl(GAME_FREE, 1, a);
}

static int random_below(int n)
{
    uint32_t a[1] = {(uint32_t)n};
    int k = (int)call_cdecl(GAME_RANDOM, 1, a);
    return k < 0 || k >= n ? 0 : k;
}

static int is_cc(uint32_t cc) { return readable(cc) && rd32(g_mem, cc) == VT_CONCACAF; }
static int year_of(uint32_t cc) { return (int16_t)rd16(g_mem, cc + F_YEAR); }
static int reputation(uint32_t team) { return readable(team) ? (int16_t)rd16(g_mem, team + CLUB_REPUTATION) : -32768; }

static const char *team_name(uint32_t club)
{
    uint32_t nation = readable(club) ? rd32(g_mem, club + 0x53u) : 0;
    return readable(nation) ? win_str(nation + 4u) : "none";
}

int season_cc26_on(uint32_t cc) { return is_cc(cc) && (int32_t)rd32(g_mem, cc + F_STAGE_COUNT) == STAGES; }

static uint32_t stage_at(uint32_t cc, int i)
{
    uint32_t arr = rd32(g_mem, cc + F_STAGES);
    uint32_t s = readable(arr) ? rd32(g_mem, arr + 4u * (uint32_t)i) : 0;
    return readable(s) ? s : 0;
}

/* The pool's teams, highest reputation first (ties in pool order). */
static int pool_sorted(uint32_t cc, uint32_t *out, int max)
{
    uint32_t pool = rd32(g_mem, cc + F_POOL);
    int size = (int32_t)rd32(g_mem, cc + F_POOL_SIZE), n = 0;
    for (int i = 0; readable(pool) && i < size && i < 64 && n < max; i++) {
        uint32_t t = rd32(g_mem, pool + 4u * (uint32_t)i);
        if (readable(t)) out[n++] = t;
    }
    for (int i = 1; i < n; i++)                         /* insertion sort: stable */
        for (int j = i; j > 0 && reputation(out[j]) > reputation(out[j - 1]); j--) {
            uint32_t t = out[j];
            out[j] = out[j - 1], out[j - 1] = t;
        }
    return n;
}

/* A FIFA day of the list (the window rule of season_concacaf26_days.c). */
static int fifa_day(long z, void *ctx)
{
    (void)ctx;
    int y, m, d, doys[SEASON_FIFA_YEAR_MAX];
    season_from_abs_day(z, &y, &m, &d);
    int n = season_fifa_year_days(y, doys, SEASON_FIFA_YEAR_MAX), doy = season_day_of_year(y, m, d);
    for (int i = 0; i < n; i++)
        if (doys[i] == doy) return 1;
    return 0;
}

/* The 2026 shape for a campaign that is made now: the gate, a pool of 31 to 36 and the days. */
static int planned(uint32_t cc, SeasonCc26Days *days, int *ko)
{
    if (!is_cc(cc) || !season_wcq26(cc)) return 0;
    uint32_t teams[POOL_MAX];
    int n = pool_sorted(cc, teams, POOL_MAX);
    if (n <= SECOND_FIELD || n > SECOND_FIELD + 6) return 0;
    if (!season_cc26_days(year_of(cc) + 2, fifa_day, NULL, days)) return 0;
    *ko = 2 * (n - SECOND_FIELD);
    return 1;
}

/* ---- The knockout: the first round ---- */

void o_00915d40(void);

/* WcConcacafCup::build_team_table, thiscall () returning 1: the entry table of the knockout. */
RT_WRAP(00915d40, cc26_team_table)
{
    uint32_t cc = cpu.ecx;
    SeasonCc26Days days;
    int ko;
    if (!planned(cc, &days, &ko)) {
        o_00915d40();
        return;
    }
    uint32_t teams[POOL_MAX];
    int n = pool_sorted(cc, teams, POOL_MAX);
    uint32_t rows = game_malloc(6u * (uint32_t)ko);
    if (rt_unwinding) return;
    if (!rows) {
        rt_ret_cdecl(0xffffffffu);
        return;
    }
    for (int i = 0; i < ko; i++) wr32(g_mem, rows + 6u * (uint32_t)i, teams[n - ko + i]);
    wr32(g_mem, cc + F_ROWS, rows);
    wr16(g_mem, cc + F_ROW_COUNT, (uint16_t)ko);
    if (logging()) {
        fprintf(stderr, "recomp: season intl: wc qualifying %d: CONCACAF in the 2026 shape, pool of %d, first round:",
                year_of(cc) + 2, n);
        for (int i = 0; i < ko; i++) fprintf(stderr, "%s %s", i ? "," : "", team_name(teams[n - ko + i]));
        fprintf(stderr, "\n");
    }
    rt_ret_cdecl(1);
}

static void day_fields(long z, int base, int16_t *doy, int8_t *year_off)
{
    int y, m, d;
    season_from_abs_day(z, &y, &m, &d);
    *doy = (int16_t)season_day_of_year(y, m, d);
    *year_off = (int8_t)(y - base);
}

int season_cc26_round_table(uint32_t cc, int stage, uint32_t table, uint32_t count)
{
    SeasonCc26Days days;
    int ko;
    if (stage != -1 || !readable(table) || !readable(count) || !planned(cc, &days, &ko)) return 0;
    if ((int16_t)rd16(g_mem, count) < 1) return 0;
    int base = year_of(cc);
    int16_t doy;
    int8_t off;
    day_fields(days.draw, base, &doy, &off);
    wr16(g_mem, table + 0, (uint16_t)doy);
    g_mem[table + 2] = (uint8_t)off;
    day_fields(days.first[0], base, &doy, &off);
    wr16(g_mem, table + 3, (uint16_t)doy);
    g_mem[table + 5] = (uint8_t)off;
    wr16(g_mem, table + 0x18, (uint16_t)ko);            /* teams, ties, new teams, first entry row */
    wr16(g_mem, table + 0x1a, (uint16_t)(ko / 2));
    wr16(g_mem, table + 0x1c, (uint16_t)ko);
    wr16(g_mem, table + 0x1e, 0);
    g_mem[table + 0x21] = 2;                            /* two legs (0x914f01) */
    g_mem[table + 0x22] = (uint8_t)(days.first[1] - days.first[0]);
    wr16(g_mem, count, 1);
    if (logging()) {
        int y0, m0, d0, y1, m1, d1;
        season_from_abs_day(days.first[0], &y0, &m0, &d0);
        season_from_abs_day(days.first[1], &y1, &m1, &d1);
        fprintf(stderr, "recomp: season intl: wc qualifying %d: CONCACAF first round of %d ties on %04d-%02d-%02d and "
                        "%04d-%02d-%02d (%s days)\n", base + 2, ko / 2, y0, m0, d0, y1, m1, d1,
                days.real ? "the real" : "the window rule's");
    }
    return 1;
}

/* ---- The stages ---- */

void o_00916cd0(void);

/* WcConcacafCup::create_central_american_groups, thiscall (): the first stages of a campaign. */
RT_WRAP(00916cd0, cc26_first_stages)
{
    uint32_t cc = cpu.ecx;
    SeasonCc26Days days;
    int ko;
    if (!planned(cc, &days, &ko)) {
        o_00916cd0();
        return;
    }
    uint32_t old = rd32(g_mem, cc + F_STAGES), arr = game_malloc(4u * STAGES);
    if (rt_unwinding) return;
    if (!arr) {                                         /* no memory: the original's groups */
        o_00916cd0();
        return;
    }
    game_free(old);
    if (rt_unwinding) return;
    wr32(g_mem, cc + F_STAGES, arr);
    wr32(g_mem, cc + F_STAGE_COUNT, STAGES);
    wr32(g_mem, cc + F_STAGE, 0xffffffffu);
    rt_ret_cdecl(0);
}

/* A round table of a League_Stage on the days z[0..n-1] (as season_uefa_cup.c group_table; the fields of each
 * record as season_conmebol_round_fields: +0 day of the year, +2 year offset, +4 the weekend flag). */
static uint32_t group_table(uint32_t cc, const long *z, int n)
{
    int base = year_of(cc);
    uint32_t t = game_malloc((uint32_t)n * GROUP_ROUND_SIZE);
    for (int d = 0; t && d < n && !rt_unwinding; d++) {
        int y, m, dd;
        season_from_abs_day(z[d], &y, &m, &dd);
        uint32_t a[9] = {t, (uint32_t)d, (uint32_t)dd, (uint32_t)m, (uint32_t)(y - base), 3, 2, (uint32_t)base, 0};
        call_cdecl(F_GROUP_DATE, 9, a);
        if (rt_unwinding) return 0;
        uint32_t s[7] = {t, (uint32_t)d, 0, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0};
        call_cdecl(F_GROUP_SLOT, 7, s);
        if (rt_unwinding) return 0;
        uint32_t rec = t + (uint32_t)d * GROUP_ROUND_SIZE;
        int wd = (int)(((z[d] % 7) + 7 + 3) % 7);     /* 1 January 1970 was a Thursday; 0 Monday */
        wr16(g_mem, rec, (uint16_t)season_day_of_year(y, m, dd));
        wr16(g_mem, rec + 2, (uint16_t)(y - base));
        g_mem[rec + 4] = (uint8_t)(wd >= 5 ? 1 : 2);
    }
    return rt_unwinding ? 0 : t;
}

/* One League_Stage in stage `stage` of n teams that meet `meetings` times on the days z[0..days-1]: `through`
 * places (the places byte) and the flags of the semi-final groups (0xf, 0x28) or the final group (0x14, 0x24). */
static int make_group(uint32_t cc, int stage, const uint32_t *teams, int n, int meetings, const long *z, int days,
                      int through, int final, uint32_t name)
{
    uint32_t members = game_malloc(4u * (uint32_t)n);
    uint32_t table = members ? group_table(cc, z, days) : 0;
    uint32_t bytes = table ? game_malloc(8) : 0;
    uint32_t sz[1] = {0xee};                            /* as 0x9174ad */
    uint32_t obj = bytes ? call_cdecl(GAME_NEW, 1, sz) : 0;
    if (rt_unwinding) return 0;
    if (!readable(obj)) {
        game_free(table);
        game_free(members);
        game_free(bytes);
        return 0;
    }
    for (int k = 0; k < n; k++) wr32(g_mem, members + 4u * (uint32_t)k, teams[k]);
    g_mem[bytes] = 1, g_mem[bytes + 1] = 2, g_mem[bytes + 2] = 3, g_mem[bytes + 3] = 0;   /* order (0x917340) */
    g_mem[bytes + 4] = (uint8_t)through;                                                  /* places (0x91735a) */
    /* The arguments of 0x9174e8 and 0x9179d9: parent, teams, list, meetings, record, round table, match days, 3,
     * 1, 10, order, places, year, stage, name, flags, 4, 0, 0x28 or 0x24, -1, 0, 2. */
    uint32_t a[22] = {cc, (uint32_t)n, members, (uint32_t)meetings, rd32(g_mem, cc + F_RECORD), table,
                      (uint32_t)days, 3, 1, 10, bytes, bytes + 4, (uint32_t)year_of(cc), (uint32_t)stage, name,
                      final ? 0x14u : 0xfu, 4, 0, final ? 0x24u : 0x28u, 0xffffffffu, 0, 2};
    call_this(F_LEAGUE_STAGE, obj, 22, a);
    if (rt_unwinding) return 0;
    wr32(g_mem, rd32(g_mem, cc + F_STAGES) + 4u * (uint32_t)stage, obj);
    game_free(table);
    game_free(members);
    game_free(bytes);
    if (rt_unwinding) return 0;
    call_this(F_GROUP_NEWS, obj, 0, NULL);
    return !rt_unwinding;
}

/* Puts n x groups teams (best first) in groups: pot p is teams[p * groups .. p * groups + groups - 1], one of each
 * pot to a group at random. */
static void draw(const uint32_t *teams, int groups, int per, uint32_t *out /* groups x per */)
{
    for (int p = 0; p < per && !rt_unwinding; p++) {
        uint32_t pot[8];
        int left = groups;
        for (int g = 0; g < groups; g++) pot[g] = teams[p * groups + g];
        for (int g = 0; g < groups && !rt_unwinding; g++) {
            int k = random_below(left);
            out[g * per + p] = pot[k];
            pot[k] = pot[--left];
        }
    }
}

static int days_now(uint32_t cc, SeasonCc26Days *days)
{
    return season_cc26_days(year_of(cc) + 2, fifa_day, NULL, days);
}

static void log_groups(uint32_t cc, const char *what, const uint32_t *g, int groups, int per)
{
    if (!logging()) return;
    fprintf(stderr, "recomp: season intl: wc qualifying %d: CONCACAF %s on day %d of %d:", year_of(cc) + 2, what,
            (int16_t)rd16(g_mem, GAME_DATE), (int16_t)rd16(g_mem, GAME_DATE + 2));
    for (int k = 0; k < groups; k++) {
        fprintf(stderr, " %c:", 'A' + k);
        for (int p = 0; p < per; p++) fprintf(stderr, "%s%s", p ? "," : " ", team_name(g[k * per + p]));
    }
    fprintf(stderr, "\n");
}

/* The second round: the pool less the first round's teams, and its winners (status 1). */
static void second_round(uint32_t cc)
{
    SeasonCc26Days days;
    if (!days_now(cc, &days)) return;
    /* The pool's teams that did not play the first round (its rows +0xa7), best first, then its winners (status
     * 1). Not the pool's top: the reputations change between the first round and the draw. */
    uint32_t pool[POOL_MAX], field[SECOND_FIELD];
    int n = pool_sorted(cc, pool, POOL_MAX), ko = (int16_t)rd16(g_mem, cc + F_ROW_COUNT), k = 0;
    uint32_t rows = rd32(g_mem, cc + F_ROWS);
    for (int i = 0; i < n && k < SECOND_FIELD; i++) {
        int played = 0;
        for (int j = 0; readable(rows) && j < ko; j++) played |= rd32(g_mem, rows + 6u * (uint32_t)j) == pool[i];
        if (!played) field[k++] = pool[i];
    }
    for (int i = 0; readable(rows) && i < ko && k < SECOND_FIELD; i++)
        if (g_mem[rows + 6u * (uint32_t)i + 5] == 1) field[k++] = rd32(g_mem, rows + 6u * (uint32_t)i);
    if (k != SECOND_FIELD) {                            /* not expected: the ties give ko / 2 winners */
        fprintf(stderr, "recomp: season intl: wc qualifying %d: CONCACAF second round has %d teams, not %d\n",
                year_of(cc) + 2, k, SECOND_FIELD);
        return;
    }
    uint32_t groups[SECOND_FIELD];
    draw(field, SECOND_GROUPS, SECOND_TEAMS, groups);
    if (rt_unwinding) return;
    log_groups(cc, "second round", groups, SECOND_GROUPS, SECOND_TEAMS);
    for (int g = 0; g < SECOND_GROUPS; g++)
        if (!stage_at(cc, g) && !make_group(cc, g, groups + g * SECOND_TEAMS, SECOND_TEAMS, 1, days.second, SEASON_CC26_SECOND, 2, 0,
                        GROUP_NAME + (uint32_t)g))
            return;
    wr32(g_mem, cc + F_STAGE, SECOND_GROUPS - 1);
}

static uint32_t row_team(uint32_t st, int pos)
{
    uint32_t z[1] = {0};
    if (!readable(st)) return 0;
    call_this(TABLE_SORT, st, 1, z);
    if (rt_unwinding) return 0;
    uint32_t table = rd32(g_mem, st + F_TABLE);
    int teams = (int16_t)rd16(g_mem, st + F_TEAMS);
    return readable(table) && pos < teams ? rd32(g_mem, table + (uint32_t)pos * ROW_SIZE) : 0;
}

/* The direct places of the section (season_wcq26.c places_of: 3 in 2026). */
int season_wcq26_concacaf_places(uint32_t self);

/* The third round: the first two of each group of the second round. */
static void third_round(uint32_t cc)
{
    SeasonCc26Days days;
    if (!days_now(cc, &days)) return;
    uint32_t field[THIRD_GROUPS * THIRD_TEAMS];
    int k = 0;
    for (int g = 0; g < SECOND_GROUPS; g++)
        for (int r = 0; r < 2; r++) {
            uint32_t t = row_team(stage_at(cc, g), r);
            if (rt_unwinding) return;
            if (readable(t)) field[k++] = t;
        }
    if (k != THIRD_GROUPS * THIRD_TEAMS) {
        fprintf(stderr, "recomp: season intl: wc qualifying %d: CONCACAF third round has %d teams\n", year_of(cc) + 2, k);
        return;
    }
    for (int i = 1; i < k; i++)
        for (int j = i; j > 0 && reputation(field[j]) > reputation(field[j - 1]); j--) {
            uint32_t t = field[j];
            field[j] = field[j - 1], field[j - 1] = t;
        }
    uint32_t groups[THIRD_GROUPS * THIRD_TEAMS];
    draw(field, THIRD_GROUPS, THIRD_TEAMS, groups);
    if (rt_unwinding) return;
    log_groups(cc, "third round", groups, THIRD_GROUPS, THIRD_TEAMS);
    int direct = season_wcq26_concacaf_places(cc) / THIRD_GROUPS;
    if (direct < 1) direct = 1;
    for (int g = 0; g < THIRD_GROUPS; g++)
        if (!stage_at(cc, SECOND_GROUPS + g) && !make_group(cc, SECOND_GROUPS + g, groups + g * THIRD_TEAMS, THIRD_TEAMS, 2, days.third, SEASON_CC26_THIRD,
                        direct, 1, GROUP_NAME + (uint32_t)g))
            return;
    wr32(g_mem, cc + F_STAGE, STAGES - 1);
}

void o_00917060(void);

/* WcConcacafCup::advance_stage, thiscall (). */
RT_WRAP(00917060, cc26_advance)
{
    uint32_t cc = cpu.ecx;
    if (!season_cc26_on(cc)) {
        o_00917060();
        return;
    }
    int32_t now = (int32_t)rd32(g_mem, cc + F_STAGE);
    if (now == -1) second_round(cc);
    else if (now == SECOND_GROUPS - 1) third_round(cc);
    if (!rt_unwinding) rt_ret_cdecl(0);
}

/* ---- The stage of a team for the original's switches ---- */

/* The original's stage for stage s of the 2026 shape: the second round as the semi-final groups, the third as the
 * final group. */
static int as_original(int s) { return s >= 0 && s < SECOND_GROUPS ? 3 : s >= SECOND_GROUPS && s < STAGES ? 6 : s; }

static void map_stage_arg(int index)
{
    uint32_t at = cpu.esp + 4 + 4u * (uint32_t)index;
    int s = (int8_t)g_mem[at];
    g_mem[at] = (uint8_t)as_original(s);
}

void o_00915710(void);
void o_009159b0(void);
void o_00918040(void);
void o_00916af0(void);

/* update_staff_records_for_status, thiscall (nation, outcome, stage, ...) ret 0x18. */
RT_WRAP(00915710, cc26_staff_records)
{
    if (season_cc26_on(cpu.ecx)) map_stage_arg(2);
    o_00915710();
}

static int in_third(uint32_t cc, uint32_t team);

/* The stage name code of argument `index` (a group of the 2026 shape: GROUP_NAME + g) as the semi-final groups'
 * (0x40c) or the final group's (0x96), by the team's round. */
static void map_code_arg(uint32_t cc, int index, uint32_t team)
{
    uint32_t at = cpu.esp + 4 + 4u * (uint32_t)index, code = rd32(g_mem, at) & 0xffffu;
    if (code >= GROUP_NAME && code < GROUP_NAME + SECOND_GROUPS)
        wr32(g_mem, at, (rd32(g_mem, at) & 0xffff0000u) | (in_third(cc, team) ? CODE_THIRD : CODE_SECOND));
}

/* build_fate_news_text, thiscall (team index, outcome, stage, stage name code, ...) ret 0x28: its text follows the
 * name code (0x9159b6 0x40f, 0x915ac6 0x40c to 0x40e, 0x915bb5 0x96), its records the stage. The team index is a
 * club's index in the club table (0xae23b4, 0x245 bytes a club, 0x915a0e). */
RT_WRAP(009159b0, cc26_fate_news)
{
    uint32_t cc = cpu.ecx;
    if (season_cc26_on(cc)) {
        uint32_t clubs = rd32(g_mem, 0xae23b4u), team = readable(clubs) ? clubs + rt_arg(0) * 0x245u : 0;
        map_stage_arg(2);
        map_code_arg(cc, 3, team);
        if (logging())                                  /* test log: each fate news of the 2026 shape */
            fprintf(stderr, "recomp: season intl: wc qualifying %d: CONCACAF fate news of %s, outcome %d, stage %d, "
                            "code 0x%x\n", year_of(cc) + 2, team_name(team), (int8_t)rt_arg(1), (int8_t)rt_arg(2),
                    rt_arg(3) & 0xffffu);
    }
    o_009159b0();
}

/* set_club_progress, thiscall (nation, stage, position, best, worst) ret 0x14. */
RT_WRAP(00918040, cc26_club_progress)
{
    if (season_cc26_on(cpu.ecx)) map_stage_arg(1);
    o_00918040();
}

/* 1 when the team is in a group of the third round. */
static int in_third(uint32_t cc, uint32_t team)
{
    for (int g = SECOND_GROUPS; g < STAGES && g <= (int32_t)rd32(g_mem, cc + F_STAGE); g++) {
        uint32_t st = stage_at(cc, g), table = st ? rd32(g_mem, st + F_TABLE) : 0;
        int n = st ? (int16_t)rd16(g_mem, st + F_TEAMS) : 0;
        for (int i = 0; readable(table) && i < n && i < 8; i++)
            if (rd32(g_mem, table + (uint32_t)i * ROW_SIZE) == team) return 1;
    }
    return 0;
}

/* write_progress_text, thiscall (out, round value, stage code, round code, outcome, row) ret 0x18: the stage code
 * of a group of the 2026 shape (GROUP_NAME + g) reads as the semi-final groups' or the final group's. */
RT_WRAP(00916af0, cc26_progress_text)
{
    uint32_t cc = cpu.ecx, code = rt_arg(2) & 0xffffu, row = rt_arg(5);
    if (season_cc26_on(cc) && code >= GROUP_NAME && code < GROUP_NAME + SECOND_GROUPS)
        map_code_arg(cc, 2, readable(row) ? rd32(g_mem, row) : 0);
    o_00916af0();
}

/* ---- The places after the groups ---- */

typedef struct {
    uint32_t team;
    long score;
} Ranked;

/* The teams of row `row` of the three groups of the third round, best first (points, goal difference, goals). */
static int row_ranked(uint32_t cc, int row, Ranked *out)
{
    int n = 0;
    for (int g = 0; g < THIRD_GROUPS; g++) {
        uint32_t st = stage_at(cc, SECOND_GROUPS + g), z[1] = {0};
        if (!st) continue;
        call_this(TABLE_SORT, st, 1, z);
        if (rt_unwinding) return 0;
        uint32_t table = rd32(g_mem, st + F_TABLE);
        if (!readable(table) || row >= (int16_t)rd16(g_mem, st + F_TEAMS)) continue;
        uint32_t r = table + (uint32_t)row * ROW_SIZE;
        int pts = (int16_t)rd16(g_mem, r + ROW_POINTS), gf = (int16_t)rd16(g_mem, r + ROW_FOR),
            ga = (int16_t)rd16(g_mem, r + ROW_AGAINST);
        out[n].team = rd32(g_mem, r);
        out[n].score = 1000000L * pts + 1000L * (gf - ga + 500) + gf;
        n++;
    }
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && out[j].score > out[j - 1].score; j--) {
            Ranked t = out[j];
            out[j] = out[j - 1], out[j - 1] = t;
        }
    return n;
}

/* The teams after the direct places of each group, best first: the extra direct places (places % 3) and then the
 * two of the tournament. */
static int after_direct(uint32_t cc, uint32_t *out, int max)
{
    int direct = season_wcq26_concacaf_places(cc) / THIRD_GROUPS, n = 0;
    if (direct < 1) direct = 1;
    for (int row = direct; row < THIRD_TEAMS && n < max; row++) {
        Ranked r[THIRD_GROUPS];
        int k = row_ranked(cc, row, r);
        for (int i = 0; i < k && n < max; i++) out[n++] = r[i].team;
    }
    return n;
}

int season_cc26_tournament(uint32_t cc, uint32_t out[2])
{
    if (!season_cc26_on(cc)) return 0;
    int extra = season_wcq26_concacaf_places(cc) % THIRD_GROUPS;
    uint32_t next[THIRD_GROUPS * THIRD_TEAMS];
    int n = after_direct(cc, next, THIRD_GROUPS * THIRD_TEAMS);
    out[0] = extra < n ? next[extra] : 0;
    out[1] = extra + 1 < n ? next[extra + 1] : 0;
    if (logging()) {                                    /* test log: the third round's groups and the order */
        fprintf(stderr, "recomp: season intl: wc play-off %d: CONCACAF third round:", year_of(cc) + 2);
        for (int g = 0; g < THIRD_GROUPS; g++) {
            uint32_t st = stage_at(cc, SECOND_GROUPS + g), table = st ? rd32(g_mem, st + F_TABLE) : 0;
            int k = st ? (int16_t)rd16(g_mem, st + F_TEAMS) : 0;
            fprintf(stderr, " %c:", 'A' + g);
            for (int i = 0; readable(table) && i < k && i < 8; i++) {
                uint32_t r = table + (uint32_t)i * ROW_SIZE;
                fprintf(stderr, "%s%s %d %+d", i ? "," : " ", team_name(rd32(g_mem, r)), (int16_t)rd16(g_mem, r + ROW_POINTS),
                        (int16_t)rd16(g_mem, r + ROW_FOR) - (int16_t)rd16(g_mem, r + ROW_AGAINST));
            }
        }
        fprintf(stderr, "; to the tournament: %s, %s\n", team_name(out[0]), team_name(out[1]));
    }
    return 1;
}

void o_0090fbe0(void);

/* Slot 12 0x90fbe0, thiscall () (the class's clear_competition_for_comp_clubs; Cup::update_for_date calls it once
 * when the section ends, 0x51f920): the extra direct places of the 2026 shape (places % 3, the best of the next row
 * across the groups) enter the World Cup as a final group's qualifier does (staff records, outcome 7, stage 6). */
RT_WRAP(0090fbe0, cc26_section_end)
{
    uint32_t cc = cpu.ecx;
    if (season_cc26_on(cc) && (int32_t)rd32(g_mem, cc + F_STAGE) == STAGES - 1) {
        int extra = season_wcq26_concacaf_places(cc) % THIRD_GROUPS;
        uint32_t next[THIRD_GROUPS * THIRD_TEAMS];
        int n = after_direct(cc, next, THIRD_GROUPS * THIRD_TEAMS);
        for (int i = 0; i < extra && i < n && !rt_unwinding; i++) {
            uint32_t flag = game_malloc(4);
            if (!flag) break;
            uint32_t a[6] = {next[i], 7, 6, flag, 0, 0};
            call_this(rd32(g_mem, rd32(g_mem, cc) + 0x44u), cc, 6, a);
            game_free(flag);
            if (logging())
                fprintf(stderr, "recomp: season intl: wc qualifying %d: CONCACAF extra place to %s\n", year_of(cc) + 2,
                        team_name(next[i]));
        }
        if (rt_unwinding) return;
        cpu.ecx = cc;
    }
    o_0090fbe0();
}
