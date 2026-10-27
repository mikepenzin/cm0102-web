/* Brazil's state championships by state (0.6.0 slice 20e, Q332; design and decisions Q600 to Q608 in
 * docs/research/2026-10-20-brazil-states.md).
 *
 * The original's three region objects pick the clubs of a region: BraRegNorth::create_table 0x435d80,
 * BraRegNortheast 0x436d90 and BraRegCentral 0x430ef0 take every club whose state record (club +0x60) is the
 * class's key id ([0x9cf994], [0x9cf9a4], [0x9cf998] at 0x435e2d, 0x436e3d, 0x430f9d), up to the class's count
 * (+0x3e = 10, 16 and 15 at 0x435d92, 0x436da6, 0x430f06), and return true only with exactly the count
 * (0x435e65..0x435e73). In a season game the wraps below make each of them the championship of one state (the
 * Paraense, the Cearense and the Mato-Grossense), and 12 new objects of the North class (slice 26's module, entry
 * 11, Brazil: [0xb64080] = create_brazilian_comps 0x832980 at 0x667ac7) play the other states of the regions.
 * Each object is told by its record (+4, written at 0x435379 before the constructor's call of create_table at
 * 0x435400).
 *
 * The picker: the clubs of the object's state (season_bra_state_from: the stadium city's code, the city table,
 * the club table), the region's championship record first, then its lower division record, then the rest, the
 * highest reputation (club +0x80) first (Q602), up to the count of the rules row (Q601). It writes +0x3e and the
 * table +0xb1 (count x 0x3b bytes, as 0x435d98) and adds each club with 0x687430 (row, club, 0, 0; as 0x435e36..
 * 0x435e42). A region object whose state has fewer clubs than its format needs (4: the semi-finals; 8: the Central
 * class's knockout) keeps the original's region (Q603).
 *
 * The round table: create_schedule (vtable +0x3c; North 0x435640, Northeast 0x4366f0, Central 0x4307b0, thiscall
 * (char stage, short *rounds, short *code, int *is_league), ret 0x10) makes stage -1 as a list of 0x41-byte rounds
 * (18, 15 and 15: 0x43567d, 0x43672d, 0x4307ed). A round holds its day of the year at +0 and its year offset at +2
 * (league_round_set_date 0x68a160, 0x68a1a5..0x68a1ac). For a count of another size the wrap gives stage -1 the
 * rounds of the class's meetings (rounds / (count - 1), or / count for an odd count), each a copy of a template
 * round, spread evenly from the template's first to its last day (Q604). The later stages stay the class's.
 *
 * This slice writes no club +0x60 (Q605), so the readers of the original's 12 state objects and records keep
 * working: BraChampCup::select_entrants 0x426d80, BraCup::select_entrants 0x427f00, the exchange 0x42b130 and the
 * fills 0x42b760 (each table to its +0x3e, 0x42b8fe) and 0x42bba0 (Q606). */
#include "win.h"
#include "season.h"
#include "season_bra_states.h"
#include "season_change.h"
#include "season_comps.h"
#include "season_rules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUBS 0xae23b4u             /* the club table, 0x245 bytes a club */
#define CLUB_COUNT 0xae235cu
#define NATIONAL_TEAMS 0xae2350u    /* the national teams at the end of the club table: 2 x this count */
#define CLUB_SIZE 0x245u
#define CLUB_NATION 0x53u
#define CLUB_STATE_RECORD 0x60u     /* the state championship record, compared at 0x435e24..0x435e34 */
#define CLUB_STADIUM 0x69u
#define CLUB_REPUTATION 0x80u       /* s16 */
#define STADIUM_CITY 0x38u
#define NAME_AT 4u
#define COMP_RECORD 4u
#define COMP_CLUBS 0x3eu
#define COMP_YEAR 0x40u
#define COMP_TABLE 0xb1u
#define ROW_SIZE 0x3bu
#define ROUND_SIZE 0x41u
#define COMP_OBJECTS 0xadadfcu      /* g_comp_object_table, by record id */
#define KEY_NORTH 0x9cf994u
static const uint32_t REGION_KEYS[3] = {0x9cf994u, 0x9cf9a4u, 0x9cf998u};   /* North, Northeast, Central */

#define F_MALLOC 0x00944e46u        /* cdecl (size), as 0x435d98 */
#define F_FREE 0x009452cau
#define F_NEW 0x00944cf1u           /* operator new, cdecl (size) */
#define F_ADD_ROW 0x00687430u       /* thiscall (row, club, 0, 0) */
#define F_NORTH_CTOR 0x00435340u    /* BraRegNorth::BraRegNorth, thiscall (year, record), ret 8 */
#define NORTH_SIZE 0xeeu
#define BRAZIL_ENTRY 11
#define MAX_PICK 64
#define DEFAULT_COUNT 16            /* Q601: no known count */

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

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

static int logging(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_BRA_STATES_LOG") && *getenv("RECOMP_BRA_STATES_LOG");
    return on;
}

/* ---- The classes, the regions and the states ---- */

typedef struct {
    int count;                  /* the class's count (+0x3e) */
    int min;                    /* the clubs its later stages need (Q603) */
} StateClass;

static const StateClass NORTH = {10, 4}, NORTHEAST = {16, 4}, CENTRAL = {15, 8};
static const StateClass *const REGION_CLASS[3] = {&NORTH, &NORTHEAST, &CENTRAL};

typedef struct {
    const char *record;         /* the region's championship record, its long name in the data */
    const char *lower;          /* the region's lower division record */
    int level;                  /* +0x69 of the region record in the data (October 2014: 7, 8, 7) */
} Region;

static const Region REGIONS[3] = {
    {"Brazilian North State Championship", "Brazilian North Lower Division", 7},
    {"Brazilian Northeast State Championship", "Brazilian Northeast Lower Division", 8},
    {"Brazilian Central State Championship", "Brazilian Central Lower Division", 7},
};

typedef struct {
    const char *code;           /* the state */
    int region;
    const char *record;         /* the record's long name in the data: the region record, or a new one */
} State;

/* The first three are the region objects (Q332: the Paraense, the Cearense, the Mato-Grossense); the others go
 * with the defs below, in the same order. */
static const State STATES[15] = {
    {"PA", 0, "Brazilian North State Championship"},
    {"CE", 1, "Brazilian Northeast State Championship"},
    {"MT", 2, "Brazilian Central State Championship"},
    {"MA", 0, "Campeonato Maranhense"}, {"AM", 0, "Campeonato Amazonense"},
    {"PI", 0, "Campeonato Piauiense"}, {"AC", 0, "Campeonato Acriano"},
    {"RN", 1, "Campeonato Potiguar"}, {"PB", 1, "Campeonato Paraibano"},
    {"AL", 1, "Campeonato Alagoano"}, {"SE", 1, "Campeonato Sergipano"},
    {"DF", 2, "Campeonato Brasiliense"}, {"ES", 2, "Campeonato Capixaba"},
    {"MS", 2, "Campeonato Sul-Mato-Grossense"}, {"TO", 2, "Campeonato Tocantinense"},
};

static const State *state_of_record(uint32_t rec)
{
    const char *name = readable(rec) ? season_change_data_name(rec) : NULL;
    for (int i = 0; name && i < 15; i++)
        if (!strcmp(name, STATES[i].record)) return &STATES[i];
    return NULL;
}

static const State *state_of(uint32_t obj)
{
    return readable(obj) && season_comps_on() ? state_of_record(rd32(g_mem, obj + COMP_RECORD)) : NULL;
}

static const SeasonCompDef *const STATE_DEFS[12];

/* The object of state i (0 to 14) in the game that runs, or 0: a region object by its key id in
 * g_comp_object_table [0xadadfc], a new one through the module. */
static uint32_t object_of_state(int i)
{
    if (i >= 3) return season_comps_object(STATE_DEFS[i - 3]);
    int32_t id = (int32_t)rd32(g_mem, REGION_KEYS[i]);
    uint32_t table = rd32(g_mem, COMP_OBJECTS);
    uint32_t obj = id >= 0 && readable(table) ? rd32(g_mem, table + 4u * (uint32_t)id) : 0;
    return readable(obj) ? obj : 0;
}

/* 1 when club is in the table of another of the 15 state objects (+0xb1 rows of 0x3b bytes, the club at +0, +0x3e
 * rows): a club plays one state championship (review of 2026-10-20). */
static int in_other_table(uint32_t club, uint32_t self)
{
    for (int i = 0; i < 15; i++) {
        uint32_t obj = object_of_state(i), table = obj ? rd32(g_mem, obj + COMP_TABLE) : 0;
        int n = obj ? (int16_t)rd16(g_mem, obj + COMP_CLUBS) : 0;
        for (int r = 0; obj != self && readable(table) && r < n && r < 64; r++)
            if (rd32(g_mem, table + (uint32_t)r * ROW_SIZE) == club) return 1;
    }
    return 0;
}

/* 1 for the record of one of the original's nine single-state championships or its lower division ("Brazilian ...
 * State Championship" or "... Lower Division", not a region's): the original's own classes pick those clubs by club
 * +0x60. A region's records do not count: a database can file a club of a state under another region (October 2021
 * files the Maranhão clubs under the Northeast). */
static int single_state_record(uint32_t rec)
{
    const char *n = readable(rec) ? season_change_data_name(rec) : NULL;
    size_t k = n ? strlen(n) : 0;
    for (int i = 0; n && i < 3; i++)
        if (!strcmp(n, REGIONS[i].record) || !strcmp(n, REGIONS[i].lower)) return 0;
    return n && !strncmp(n, "Brazilian ", 10) &&
           ((k > 19 && !strcmp(n + k - 19, " State Championship")) ||
            (k > 15 && !strcmp(n + k - 15, " Lower Division")));
}

/* ---- The state of a club ---- */

static void copy_name(uint32_t at, char *out, size_t n)
{
    size_t k = 0;
    if (readable(at))
        while (k + 1 < n && k < 51 && g_mem[at + k]) out[k] = (char)g_mem[at + k], k++;
    out[k] = 0;
}

static int club_state(uint32_t club, char out[3])
{
    char name[64], city[64] = "";
    copy_name(club + NAME_AT, name, sizeof name);
    uint32_t stadium = rd32(g_mem, club + CLUB_STADIUM);
    if (readable(stadium)) copy_name(rd32(g_mem, stadium + STADIUM_CITY) + NAME_AT, city, sizeof city);
    return season_bra_state_from(name, city, out);
}

/* Brazil's nation: that of a club of a region record, or 0. */
static uint32_t brazil_nation(void)
{
    uint32_t base = rd32(g_mem, CLUBS);
    int32_t count = (int32_t)rd32(g_mem, CLUB_COUNT) - 2 * (int32_t)rd32(g_mem, NATIONAL_TEAMS);
    for (int r = 0; r < 3; r++) {
        uint32_t region = season_comps_record(REGIONS[r].record);
        for (int32_t i = 0; readable(base) && region && i < count; i++) {
            uint32_t c = base + (uint32_t)i * CLUB_SIZE;
            if (rd32(g_mem, c + CLUB_STATE_RECORD) == region) return rd32(g_mem, c + CLUB_NATION);
        }
    }
    return 0;
}

/* 1 when region object i (0 to 2) keeps the original's region (Q603): its state has fewer clubs than its class needs.
 * Counted on the data only (no table of another object), so the answer is the same whenever it is asked: the region
 * object, the module's make of the region's new objects and every picker agree. */
static int keeps_region(int i)
{
    uint32_t base = rd32(g_mem, CLUBS), brazil = brazil_nation();
    int32_t count = (int32_t)rd32(g_mem, CLUB_COUNT) - 2 * (int32_t)rd32(g_mem, NATIONAL_TEAMS);
    int n = 0;
    for (int32_t k = 0; readable(base) && brazil && k < count; k++) {
        uint32_t c = base + (uint32_t)k * CLUB_SIZE;
        char code[3];
        if (rd32(g_mem, c + CLUB_NATION) == brazil && club_state(c, code) && !strcmp(code, STATES[i].code) &&
            !single_state_record(rd32(g_mem, c + CLUB_STATE_RECORD)))
            n++;
    }
    return n < REGION_CLASS[i]->min;
}

/* ---- The clubs of a state (Q602) ---- */

typedef struct {
    uint32_t club;
    int rank;                   /* 0 region record, 1 region lower record, 2 the rest */
    int reputation, index;
} StatePick;

static int by_rank(const void *a, const void *b)
{
    const StatePick *x = a, *y = b;
    if (x->rank != y->rank) return x->rank - y->rank;
    if (x->reputation != y->reputation) return y->reputation - x->reputation;
    return x->index - y->index;
}

/* The clubs of state st in pick order, at most max, for the object self (0: none yet). Left out (Q611): a club with the
 * record of a single-state championship, a club with the record of a region object that keeps its region (its
 * original picker takes it), and a club in the table of another of the 15 state objects. */
static int state_clubs(const State *st, uint32_t self, uint32_t *out, int max)
{
    uint32_t base = rd32(g_mem, CLUBS), region = season_comps_record(REGIONS[st->region].record),
             lower = season_comps_record(REGIONS[st->region].lower), brazil = 0;
    int32_t count = (int32_t)rd32(g_mem, CLUB_COUNT) - 2 * (int32_t)rd32(g_mem, NATIONAL_TEAMS);
    for (int32_t i = 0; readable(base) && region && i < count && !brazil; i++) {
        uint32_t c = base + (uint32_t)i * CLUB_SIZE;
        if (rd32(g_mem, c + CLUB_STATE_RECORD) == region) brazil = rd32(g_mem, c + CLUB_NATION);
    }
    if (!brazil) return 0;
    int kept[3];
    uint32_t kept_record[3];
    for (int r = 0; r < 3; r++) {
        kept[r] = keeps_region(r);
        kept_record[r] = season_comps_record(REGIONS[r].record);
    }
    static StatePick picks[4096];
    int n = 0;
    for (int32_t i = 0; i < count && n < 4096; i++) {
        uint32_t c = base + (uint32_t)i * CLUB_SIZE;
        char code[3];
        if (rd32(g_mem, c + CLUB_NATION) != brazil || !club_state(c, code) || strcmp(code, st->code)) continue;
        uint32_t rec = rd32(g_mem, c + CLUB_STATE_RECORD);
        if (single_state_record(rec) || in_other_table(c, self)) continue;
        int taken = 0;
        for (int r = 0; r < 3; r++) taken |= kept[r] && rec == kept_record[r] && rec;
        if (taken) continue;
        picks[n].club = c;
        picks[n].rank = rec == region ? 0 : rec == lower && lower ? 1 : 2;
        picks[n].reputation = (int16_t)rd16(g_mem, c + CLUB_REPUTATION);
        picks[n].index = (int)i;
        n++;
    }
    qsort(picks, (size_t)n, sizeof picks[0], by_rank);
    for (int i = 0; i < n && i < max; i++) out[i] = picks[i].club;
    return n < max ? n : max;
}

/* ---- The count (Q601) ---- */

/* The club count of the object's row: the start season at a new game and with changing rules off (Q232), else the
 * row of the edition +0x40. The state objects hold the game year there (2001 in a 2001/02 game, whose state
 * championships play in the first months of 2002: the row 2001/02, real season 2002). The nearest earlier known
 * count (Q248). 0 when none is known. */
static int row_count(uint32_t obj)
{
    const char *file = season_change_file(obj);
    int s = (int16_t)rd16(g_mem, obj + COMP_YEAR);
    if (!season_changing_rules() || season_change_is_creating()) s = season_start_year();
    for (int k = s; k >= SEASON_FIRST_YEAR && k > s - 30; k--) {
        const SeasonRule *r = season_rules_find(k, file, 0);
        if (r && r->clubs > 0) return r->clubs;
    }
    return 0;
}

/* ---- The plan and the picker ---- */

/* The count of a state object this season (Q601, Q603), or 0 when it keeps the original's region. clubs gets the
 * state's clubs in pick order and *have their number. */
static int plan(uint32_t obj, const StateClass *k, uint32_t *clubs, int *have, int *want)
{
    const State *st = state_of(obj);
    if (!st) return 0;
    if (st - STATES < 3 && keeps_region((int)(st - STATES))) {
        *have = *want = 0;
        return 0;
    }
    *have = state_clubs(st, obj, clubs, MAX_PICK);
    *want = row_count(obj);
    if (*have < k->min) return *have;   /* a new object never takes the region's clubs: it plays with what its state
                                         * has (make: 4 or more) */
    int n = *want > 0 ? *want : DEFAULT_COUNT;
    if (n > *have) n = *have;
    return n < k->min ? k->min : n;      /* the fallback hangs on the clubs of the data, not on the season's count */
}

/* The constructor asks for the round table (stage -1) before it calls create_table (in a test run: "schedule ...
 * picked 0"), and start_new_season may step the year between the two. So the count that the round table was made
 * for is kept for the object, and create_table takes it. */
#define PLANS 15                    /* one a state object */
static struct { uint32_t obj, rec; int n; } planned[PLANS];

/* Cleared at a new game and a load (season_bra_states_reset). An entry is found by object and record, so an entry
 * of another object at a reused address is not taken (reviews of 2026-10-20). */
void season_bra_states_reset(void) { memset(planned, 0, sizeof planned); }

static void keep_plan(uint32_t obj, int n)
{
    uint32_t rec = rd32(g_mem, obj + COMP_RECORD);
    int slot = -1;
    for (int i = 0; i < PLANS && slot < 0; i++)
        if (planned[i].obj == obj) slot = i;
    for (int i = 0; i < PLANS && slot < 0; i++)
        if (!planned[i].obj) slot = i;
    if (slot < 0) {                                     /* not possible with 15 objects: say so, keep the first */
        fprintf(stderr, "recomp: brazil states: no room for the plan of %.50s\n",
                (const char *)g_mem + rec + NAME_AT);
        return;
    }
    planned[slot].obj = obj, planned[slot].rec = rec, planned[slot].n = n;
}

static int kept_plan(uint32_t obj)
{
    uint32_t rec = obj ? rd32(g_mem, obj + COMP_RECORD) : 0;
    for (int i = 0; obj && i < PLANS; i++)
        if (planned[i].obj == obj && planned[i].rec == rec) return planned[i].n;
    return -1;
}

/* create_table of a state object: 1 when it took the clubs of its state (the wrap then returns 1). */
static int pick(uint32_t obj, const StateClass *k)
{
    const State *st = state_of(obj);
    if (!st) return 0;
    uint32_t clubs[MAX_PICK];
    int have = 0, want = 0, n = plan(obj, k, clubs, &have, &want), kept = kept_plan(obj);
    if (rt_unwinding) return 1;
    if (kept >= 0 && kept <= have) n = kept;
    if (!n && st - STATES < 3) {
        fprintf(stderr, "recomp: brazil states: %s %d: too few clubs of %s for its format: the original's region\n",
                st->record, (int16_t)rd16(g_mem, obj + COMP_YEAR), st->code);
        return 0;
    }
    uint32_t a[1] = {(uint32_t)(n ? n : 1) * ROW_SIZE};
    uint32_t table = call_cdecl(F_MALLOC, 1, a);
    if (rt_unwinding) return 1;
    if (!readable(table)) {                 /* as the original's failed malloc (0x435daa..0x435e13): false */
        rt_ret_cdecl(0);
        return 1;
    }
    wr16(g_mem, obj + COMP_CLUBS, (uint16_t)n);
    wr32(g_mem, obj + COMP_TABLE, table);
    for (int i = 0; i < n && !rt_unwinding; i++) {
        uint32_t r[4] = {(uint32_t)i, clubs[i], 0, 0};
        call_this(F_ADD_ROW, obj, 4, r);
    }
    if (rt_unwinding) return 1;
    if (logging()) {
        fprintf(stderr, "recomp: brazil states: %s %d: %d clubs of %s (row %d, %d in the data):", st->record,
                (int16_t)rd16(g_mem, obj + COMP_YEAR), n, st->code, want, have);
        for (int i = 0; i < n; i++) fprintf(stderr, " %.40s;", (const char *)g_mem + clubs[i] + NAME_AT);
        fputc('\n', stderr);
    } else {
        fprintf(stderr, "recomp: brazil states: %s %d: %d clubs of %s\n", st->record,
                (int16_t)rd16(g_mem, obj + COMP_YEAR), n, st->code);
    }
    rt_ret_cdecl(1);
    return 1;
}

void o_00435d80(void);
void o_00436d90(void);
void o_00430ef0(void);

/* The test log of a region object that kept its region (the original picked): its clubs, state "RG". */
static void log_region(uint32_t obj)
{
    const State *st = logging() ? state_of(obj) : NULL;
    uint32_t table = st ? rd32(g_mem, obj + COMP_TABLE) : 0;
    int n = st ? (int16_t)rd16(g_mem, obj + COMP_CLUBS) : 0;
    if (!readable(table)) return;
    fprintf(stderr, "recomp: brazil states: %s %d: %d clubs of RG (row 0, %d in the data):", st->record,
            (int16_t)rd16(g_mem, obj + COMP_YEAR), n, n);
    for (int i = 0; i < n && i < 64; i++)
        fprintf(stderr, " %.40s;", (const char *)g_mem + rd32(g_mem, table + (uint32_t)i * ROW_SIZE) + NAME_AT);
    fputc('\n', stderr);
}

RT_WRAP(00435d80, bra_north_create_table)
{
    uint32_t obj = cpu.ecx;
    if (!pick(obj, &NORTH)) o_00435d80(), log_region(obj);
}
RT_WRAP(00436d90, bra_northeast_create_table)
{
    uint32_t obj = cpu.ecx;
    if (!pick(obj, &NORTHEAST)) o_00436d90(), log_region(obj);
}
RT_WRAP(00430ef0, bra_central_create_table)
{
    uint32_t obj = cpu.ecx;
    if (!pick(obj, &CENTRAL)) o_00430ef0(), log_region(obj);
}

/* ---- The round table (Q604) ---- */

static int is_leap(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }

/* Day number of (day of the year, year offset) against the year base. */
static int serial(int base, int doy, int offset)
{
    int n = doy;
    for (int y = base; y < base + offset; y++) n += 365 + is_leap(y);
    for (int y = base + offset; y < base; y++) n -= 365 + is_leap(y);
    return n;
}

static void unserial(int base, int n, int *doy, int *offset)
{
    int y = base;
    while (n < 0) y--, n += 365 + is_leap(y);
    while (n >= 365 + is_leap(y)) n -= 365 + is_leap(y), y++;
    *doy = n;
    *offset = y - base;
}

static void schedule(const StateClass *k, void (*original)(void))
{
    uint32_t obj = cpu.ecx, rounds_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xffu);
    const State *st = stage == -1 ? state_of(obj) : NULL;
    uint32_t clubs[MAX_PICK];
    int have = 0, want = 0, n = st ? plan(obj, k, clubs, &have, &want) : 0;
    if (rt_unwinding) return;
    if (st) keep_plan(obj, n);
    /* The new objects play on the North template's days. The day fixtures screen (build_day_fixtures_screen
     * 0x717db0) lays out at most 30 rows (Area::layout 0x403350, box "Area..cpp 507" at 0x4033b8): 13 state
     * leagues on one day pass it. So each new object starts 0 to 3 days earlier by state, at most 4 a day, and its
     * last day moves with it, before the class's fixed semi-final days. */
    int early = n && st - STATES >= 3 ? (int)((st - STATES - 3) % 4) : 0, active = n > 0;
    original();
    if (rt_unwinding || !active) return;
    uint32_t t = cpu.eax;
    int r0 = (int16_t)rd16(g_mem, rounds_at);
    int meetings = r0 / (k->count - 1 + k->count % 2);
    if (meetings < 1) meetings = 1;
    int r = meetings * (n - 1 + n % 2);
    if (!readable(t) || r0 < 2 || r < 1 || (r == r0 && !early)) return;
    Cpu saved = cpu;
    uint32_t a[1] = {(uint32_t)r * ROUND_SIZE};
    uint32_t nt = call_cdecl(F_MALLOC, 1, a);
    if (rt_unwinding || !readable(nt)) return;
    int base = (int16_t)rd16(g_mem, obj + COMP_YEAR);
    int first = serial(base, (int16_t)rd16(g_mem, t), (int16_t)rd16(g_mem, t + 2));
    uint32_t tl = t + (uint32_t)(r0 - 1) * ROUND_SIZE;
    int last = serial(base, (int16_t)rd16(g_mem, tl), (int16_t)rd16(g_mem, tl + 2));
    first -= early, last -= early;
    int span = last - first;
    if (r > 1 && span < 2 * (r - 1)) first = last - 2 * (r - 1), span = last - first;   /* at least 2 days apart */
    for (int i = 0; i < r; i++) {
        uint32_t dst = nt + (uint32_t)i * ROUND_SIZE, src = t + (uint32_t)((long)i * r0 / r) * ROUND_SIZE;
        memcpy(g_mem + dst, g_mem + src, ROUND_SIZE);
        int day = r > 1 ? first + (int)((long)span * i / (r - 1)) : first, doy, offset;
        unserial(base, day, &doy, &offset);
        wr16(g_mem, dst, (uint16_t)doy);
        wr16(g_mem, dst + 2, (uint16_t)offset);
    }
    uint32_t f[1] = {t};
    call_cdecl(F_FREE, 1, f);
    if (rt_unwinding) return;
    cpu = saved;
    wr16(g_mem, rounds_at, (uint16_t)r);
    cpu.eax = nt;
    if (logging())
        fprintf(stderr, "recomp: brazil states: %.50s %d: %d clubs, %d league rounds (template %d)\n",
                (const char *)g_mem + rd32(g_mem, obj + COMP_RECORD) + NAME_AT, base, n, r, r0);
}

void o_00435640(void);
void o_004366f0(void);
void o_004307b0(void);

RT_WRAP(00435640, bra_north_schedule) { schedule(&NORTH, o_00435640); }
RT_WRAP(004366f0, bra_northeast_schedule) { schedule(&NORTHEAST, o_004366f0); }
RT_WRAP(004307b0, bra_central_schedule) { schedule(&CENTRAL, o_004307b0); }

/* ---- The 12 new objects (slice 26's module) ---- */

/* An object of the North class (0xee bytes, BraRegNorth 0x435340: the semi-finals and final format). The year is
 * that of the original's North object (create_brazilian_comps gives its objects one year, 0x832a1f), so the new
 * championships play in the same calendar year. A state with fewer than 4 clubs gets no object (Q603). */
static uint32_t state_make(const SeasonCompDef *d, uint32_t rec, int year, int loading)
{
    const State *st = NULL;
    for (int i = 3; i < 15 && !st; i++)
        if (!strcmp(STATES[i].record, d->record)) st = &STATES[i];
    if (!st) return 0;
    if (!loading) {
        uint32_t clubs[MAX_PICK];
        /* A region object that keeps its region (its state has fewer clubs than its class needs) holds the clubs of
         * the region's other states: their new objects are not made in this game (review of 2026-10-20). */
        if (keeps_region(st->region)) {
            fprintf(stderr, "recomp: brazil states: %s: %s keeps its region, no championship of %s\n", d->record,
                    STATES[st->region].record, st->code);
            return 0;
        }
        int have = state_clubs(st, 0, clubs, MAX_PICK);
        if (rt_unwinding) return 0;
        if (have < NORTH.min) {
            fprintf(stderr, "recomp: brazil states: %s: %d clubs of %s, no championship\n", d->record, have, st->code);
            return 0;
        }
        int32_t id = (int32_t)rd32(g_mem, KEY_NORTH);
        uint32_t table = rd32(g_mem, COMP_OBJECTS);
        uint32_t north = id >= 0 && readable(table) ? rd32(g_mem, table + 4u * (uint32_t)id) : 0;
        if (readable(north)) year = (int16_t)rd16(g_mem, north + COMP_YEAR);
    }
    uint32_t na[1] = {NORTH_SIZE};
    uint32_t obj = call_cdecl(F_NEW, 1, na);
    if (rt_unwinding || !readable(obj)) return 0;
    uint32_t ca[2] = {(uint32_t)year, rec};
    call_this(F_NORTH_CTOR, obj, 2, ca);
    return rt_unwinding ? 0 : obj;
}

#define STATE_DEF(name, comp, file, record, region, short_name)                                                   \
    const SeasonCompDef name = {comp, file, record, BRAZIL_ENTRY, state_make, REGIONS_LIKE_##region, short_name,  \
                                REGIONS_LEVEL_##region, 0}
#define REGIONS_LIKE_0 "Brazilian North State Championship"
#define REGIONS_LIKE_1 "Brazilian Northeast State Championship"
#define REGIONS_LIKE_2 "Brazilian Central State Championship"
#define REGIONS_LEVEL_0 7
#define REGIONS_LEVEL_1 8
#define REGIONS_LEVEL_2 7

STATE_DEF(season_bra_state_ma, "Maranhense state championship", "new_maranhense_state_championship",
          "Campeonato Maranhense", 0, "Maranhense");
STATE_DEF(season_bra_state_am, "Amazonense state championship", "new_amazonense_state_championship",
          "Campeonato Amazonense", 0, "Amazonense");
STATE_DEF(season_bra_state_pi, "Piauiense state championship", "new_piauiense_state_championship",
          "Campeonato Piauiense", 0, "Piauiense");
STATE_DEF(season_bra_state_ac, "Acriano state championship", "new_acriano_state_championship",
          "Campeonato Acriano", 0, "Acriano");
STATE_DEF(season_bra_state_rn, "Potiguar state championship", "new_potiguar_state_championship",
          "Campeonato Potiguar", 1, "Potiguar");
STATE_DEF(season_bra_state_pb, "Paraibano state championship", "new_paraibano_state_championship",
          "Campeonato Paraibano", 1, "Paraibano");
STATE_DEF(season_bra_state_al, "Alagoano state championship", "new_alagoano_state_championship",
          "Campeonato Alagoano", 1, "Alagoano");
STATE_DEF(season_bra_state_se, "Sergipano state championship", "new_sergipano_state_championship",
          "Campeonato Sergipano", 1, "Sergipano");
STATE_DEF(season_bra_state_df, "Brasiliense state championship", "new_brasiliense_state_championship",
          "Campeonato Brasiliense", 2, "Brasiliense");
STATE_DEF(season_bra_state_es, "Capixaba state championship", "new_capixaba_state_championship",
          "Campeonato Capixaba", 2, "Capixaba");
STATE_DEF(season_bra_state_ms, "Sul-Mato-Grossense state championship", "new_sul-mato-grossense_state_championship",
          "Campeonato Sul-Mato-Grossense", 2, "Sul-Mato-Grossense");
STATE_DEF(season_bra_state_to, "Tocantinense state championship", "new_tocantinense_state_championship",
          "Campeonato Tocantinense", 2, "Tocantinense");

/* ---- The competitions menu rows ---- */

void o_00403350(void);

/* Area::layout 0x403350 (thiscall, no argument) stops with "Area..cpp 507" when an area wants more than 30 rows on a
 * page (+0xbac, 0x4033ec..0x403451). The cause that made it act (O259, guard B6): the Competitions menu of the side bar
 * (Nations A-J, Brazil) listed the 29 competitions of Brazil, a separator and Awards, 31 rows of 21 pixels, more than
 * the 30 rows and than the 600 pixels of the classic screen (hovering over Brazil with only this clamp stopped at
 * "Area..cpp 838"). It is not the day fixtures screen: the first draw after a start or a load lays out the two menus
 * of the guard line. Fixed at the source in menu_rows.c: a menu of more than 24 competitions has a sub menu More, so
 * no menu wants more than 27 rows. This wrap stays as a logged net: a page of more than 30 rows becomes 30. It is not
 * expected to act. The base game never asks for more (it would stop at the box). */
RT_WRAP(00403350, area_layout)
{
    uint32_t area = cpu.ecx;
    if (season_comps_on() && readable(area) && g_mem[area + 0xbac] > 30) {
        static int said;
        if (said++ < 5)
            fprintf(stderr, "recomp: screen rows: net: an area of %d rows a page shows 30 (area %x, called from %x, "
                            "day %d of %d); the menus keep to 27 rows in menu_rows.c\n", g_mem[area + 0xbac], area,
                    rd32(g_mem, cpu.esp), (int16_t)rd16(g_mem, 0xae2c90u), (int16_t)rd16(g_mem, 0xae2c92u));
        g_mem[area + 0xbac] = 30;
    }
    o_00403350();
}

static const SeasonCompDef *const STATE_DEFS[12] = {
    &season_bra_state_ma, &season_bra_state_am, &season_bra_state_pi, &season_bra_state_ac, &season_bra_state_rn,
    &season_bra_state_pb, &season_bra_state_al, &season_bra_state_se, &season_bra_state_df, &season_bra_state_es,
    &season_bra_state_ms, &season_bra_state_to,
};
