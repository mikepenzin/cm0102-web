/* New divisions (season_divisions.h). Evidence: docs/research/2026-10-20-new-divisions.md. */
#include "win.h"
#include "../rt_host.h"
#include "season.h"
#include "season_change.h"
#include "season_comps.h"
#include "season_divisions.h"
#include "season_divisions_ita.h"
#include "season_names.h"
#include "season_rules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUBS 0x00ae23b4u           /* the club table (0x245 bytes a club), clubs then national teams */
#define CLUB_COUNT 0x00ae235cu
#define NATION_COUNT 0x00ae2350u    /* the last 2 x nations rows are national teams */
#define CLUB_SIZE 0x245u
#define CLUB_NATION 0x53u           /* nation pointer */
#define CLUB_DIVISION 0x57u         /* club_comp pointer */
#define CLUB_GROUP 0x60u            /* club_comp pointer: the German regional group (GerRegional::create_table 0x5dd8d5) */
#define COMPS 0x00ae23d0u           /* the club_comp table, 0x6b bytes a record (record id x 0x6b, 0x5dbb93..0x5dbba6) */
#define COMP_SIZE 0x6bu
#define NATIONS 0x00ae23a8u         /* the nation table, 0x122 bytes a nation */
#define NATION_SIZE 0x122u
#define NATION_SELECTED 0x11cu      /* bit 0x04: the extra division (create_german_comps 0x8343f0 at 0x8344d5) */
#define EXTRA_DIVISION 0x04u
#define COMP_OBJECTS 0x00adadfcu    /* the competition object of each record id */
#define COMP_SLOTS 0x00adadf8u

#define KEY_GERMANY 0x009cf314u     /* the nation id of Germany (0x5db9c5) */
#define KEY_GER_FIRST 0x009cf54cu
#define KEY_GER_SECOND 0x009cf550u
#define KEY_GER_NORTH 0x009cf55cu   /* German Regional Division North: a group tag at club +0x60 */
#define KEY_GER_REGIONAL 0x009cf564u
#define KEY_LOWER 0x009cf764u       /* A Lower Division */

#define OBJ_RECORD 4u
#define OBJ_OWNER 8u                /* the object that takes the predicted positions (0x5de25f) */
#define OBJ_STAGES 0xcu
#define OBJ_STAGE_COUNT 0x2cu       /* s32: the stages of the league */
#define OBJ_STAGE_NOW 0x30u         /* s32, -1 with no stage */
#define OBJ_COMPLETE 0x4cu          /* 1 when the season is complete (League::update_for_date 0x684a9e) */
#define OBJ_CLUBS 0x3eu
#define OBJ_YEAR 0x40u
#define OBJ_MATCHES 0xa7u
#define OBJ_TABLE 0xb1u
#define OBJ_BELOW 0x20u             /* record id of the division below, -1 for none (GerSecond 0x5df441..0x5df457) */
#define ROW_SIZE 0x3bu
#define ROW_STATUS 0x37u            /* 3: relegated (0x5dba7b) */
#define UP 0xbeu
#define UP_PLAYOFF 0xbfu
#define DOWN_PLAYOFF 0xc0u
#define DOWN 0xc1u
#define DATE_SIZE 0x41u             /* a date of the template: +0 s16 day of the year, +2 s16 year (0x68a1a5..0x68a1b7) */

#define MIN_CLUBS 8
#define MAX_LIGA3 24                /* the most clubs of the 3. Liga object (its table is sized from +0x3e, 0x5dd80f) */
#define POOL_MAX 200                /* the pool of new clubs of the lowest tier (cmp di, 0xc8 at 0x5dba17) */

/* Functions of the original (tools/recomp/callbacks.py). */
#define F_MALLOC 0x00944e46u        /* cdecl (size) */
#define GER_REGIONAL_VTABLE 0x0096b250u
#define F_FREE 0x009452cau          /* cdecl (block) */
#define F_PROMOTE 0x00689c80u       /* league_promote_and_relegate, stdcall (upper, lower, expire, pairwise, up, down) */
#define F_MOVE 0x006830b0u          /* League::move_club_to_division, thiscall (club, record, 1), ret 0xc */
#define F_RELEGATE 0x006831a0u      /* League::relegate_club_to_division, thiscall (club, record, 1), ret 0xc */
#define F_PICK 0x004afbd0u          /* sort a club list for the new clubs of a tier, cdecl (list, count, need) */
#define F_SENIOR 0x00540a50u        /* the senior side of a club, cdecl (club, &flag, 1) */
#define F_QSORT 0x00945333u         /* qsort, cdecl (base, count, width, compare) */
#define F_BY_POSITION 0x004b07d0u   /* the compare of GerRegional's predicted positions (0x5de239) */
#define F_SET_POSITION 0x004a2540u  /* set a predicted position, thiscall (club, position), ret 8 */
#define F_BY_LATITUDE 0x0053fd40u   /* the compare of the Spanish and Portuguese group deals (push at 0x85067f, 0x7d02c5) */

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

static int divisions_on(void)
{
    static int off = -1;
    if (off < 0) off = getenv("RECOMP_NEW_DIVISIONS") && !strcmp(getenv("RECOMP_NEW_DIVISIONS"), "off");
    return !off && season_game_changes();
}

static int logging(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_DIVISION_LOG") && *getenv("RECOMP_DIVISION_LOG");
    return on;
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
    if (p) call_cdecl(F_FREE, 1, a);
}

/* The record of a key id, or 0. */
static uint32_t record_of(uint32_t key)
{
    int32_t id = (int32_t)rd32(g_mem, key);
    uint32_t comps = rd32(g_mem, COMPS);
    if (id < 0 || !readable(comps)) return 0;
    return comps + (uint32_t)id * COMP_SIZE;
}

/* The object of a key id (g_comp_object_table [0xadadfc] at its record id, below [0xadadf8]), or 0. */
static uint32_t object_of(uint32_t key)
{
    int32_t id = (int32_t)rd32(g_mem, key), slots = (int32_t)rd32(g_mem, COMP_SLOTS);
    uint32_t table = rd32(g_mem, COMP_OBJECTS);
    if (id < 0 || id >= slots || !readable(table)) return 0;
    uint32_t o = rd32(g_mem, table + 4u * (uint32_t)id);
    return readable(o) ? o : 0;
}

/* The rows of a file in a season; after the last season of the file the last rows stay (Q248). */
static const SeasonRule *row_in(const char *file, int season, int n)
{
    for (int s = season; s >= SEASON_FIRST_YEAR && s > season - 10; s--)
        if (season_rules_find(s, file, 0)) return season_rules_find(s, file, n);
    return NULL;
}

/* 1 when the rules of a row give play-off places (a text, not a code such as none or n/a). */
static int has_playoff(const SeasonRule *r)
{
    const char *p = r ? r->playoffs : NULL;
    return p && *p && strcmp(p, "none") && strcmp(p, "na") && strcmp(p, "n/a") && strcmp(p, "unknown") &&
           strcmp(p, "not_held");
}

/* The season of the rules of an object: the start season at a new game and with changing rules off (Q232),
 * else the edition it holds (+0x40). */
static int season_of(uint32_t obj)
{
    if (!season_changing_rules() || season_change_is_creating()) return season_start_year();
    return (int16_t)rd16(g_mem, obj + OBJ_YEAR);
}

/* The season that a top league starts after its season change code (GerFirst::start_new_season 0x5db4a0 adds
 * 1 to +0x40 at 0x5db52a, after the exchange and the split). */
static int next_season_of(uint32_t top)
{
    if (!season_changing_rules()) return season_start_year();
    return (int16_t)rd16(g_mem, top + OBJ_YEAR) + 1;
}

/* The clubs whose division is a record. */
static int clubs_in(uint32_t rec)
{
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT), n = 0;
    if (!rec || !readable(table)) return 0;
    for (int i = 0; i < count; i++) n += rd32(g_mem, table + (uint32_t)i * CLUB_SIZE + CLUB_DIVISION) == rec;
    return n;
}


/* ---- Germany: the 3. Liga in the GerRegional object (20a) ---- */

/* The 3. Liga row of a season: the ger_regional rows of the season are one row (the two Regionalliga rows of
 * 2001/02 to 2007/08 are two, docs/research/rules-0.6.0/ger.tsv). */
static const SeasonRule *liga3_row(int season)
{
    const SeasonRule *r = row_in("ger_regional", season, 0);
    if (!r || row_in("ger_regional", season, 1)) return NULL;
    return r->clubs >= MIN_CLUBS && r->clubs <= MAX_LIGA3 ? r : NULL;
}

/* 1 when a nation (key id of its nation id) has the extra division flag. */
static int nation_flag(uint32_t key)
{
    uint32_t nations = rd32(g_mem, NATIONS);
    int32_t id = (int32_t)rd32(g_mem, key);
    if (!readable(nations) || id < 0) return 0;
    return (g_mem[nations + (uint32_t)id * NATION_SIZE + NATION_SELECTED] & EXTRA_DIVISION) != 0;
}

static int ger_flag(void) { return nation_flag(KEY_GERMANY); }

/* 1 when a GerRegional object holds the 3. Liga in the season of its rules. */
static int liga3_object(uint32_t obj)
{
    return divisions_on() && readable(obj) && liga3_row(season_of(obj)) != NULL;
}

/* The 3. Liga's club count: the clubs that its division holds (the club moves of a new game give the row's
 * count; later the game's results decide, Q247), within 8 and 24. create_schedule runs before create_table
 * (set_up_rules 0x5dd9f0 calls vtable +0x3c at 0x5dda7d), so both take it from the division. */
static int liga3_count(uint32_t obj)
{
    int n = clubs_in(rd32(g_mem, obj + OBJ_RECORD));
    return n < MIN_CLUBS ? 0 : n > MAX_LIGA3 ? MAX_LIGA3 : n;
}

uint32_t rt_ger_regional_clubs(uint32_t obj, uint32_t original)
{
    if (!liga3_object(obj)) return original;
    int n = liga3_count(obj);
    if (!n) return original;
    if (logging())
        fprintf(stderr, "recomp: new divisions: 3. Liga %d: %d clubs (class %u)\n", season_of(obj), n, original);
    wr16(g_mem, obj + OBJ_MATCHES, 0xffffu);       /* as create_table at 0x5dd815 */
    return (uint32_t)n;
}

/* GerRegional::create_schedule 0x5dcef0 (thiscall: stage, &count, &code, &flag; ret 0x10): a template of 34
 * dates (D at 0x5dcf12; the writes of 0x68a160 fill dates 0 to 33 only). A 3. Liga of n clubs plays 2 (n - 1)
 * rounds (meetings 2 at 0x5dda00), 38 for 20. The new template takes the 34 dates and puts each extra round
 * on the Wednesday after a Saturday of the template whose next date is at least a week later, spread over the
 * season. The template is in the game's heap; the old one is freed. */
void o_005dcef0(void);
RT_WRAP(005dcef0, ger_regional_schedule)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_005dcef0();
    if (rt_unwinding || stage != -1 || !liga3_object(self)) return;
    uint32_t old = cpu.eax;
    int have = (int16_t)rd16(g_mem, count_at), n = liga3_count(self);
    int rounds = 2 * (n + (n & 1) - 1);
    if (!readable(old) || !readable(count_at) || have <= 0 || rounds <= have) return;
    uint32_t tmpl = game_malloc((uint32_t)rounds * DATE_SIZE);
    if (!tmpl) return;
    int extra = rounds - have, k = 0, done = 0;
    for (int i = 0; i < have; i++) {
        memcpy(g_mem + tmpl + (uint32_t)k * DATE_SIZE, g_mem + old + (uint32_t)i * DATE_SIZE, DATE_SIZE);
        k++;
        /* An extra round after date i when i is at or past the next even spread point. */
        if (done < extra && i + 1 < have && i >= (done + 1) * have / (extra + 1)) {
            uint32_t a = old + (uint32_t)i * DATE_SIZE, b = a + DATE_SIZE;
            int day = (int16_t)rd16(g_mem, a), year = (int16_t)rd16(g_mem, a + 2);
            int next = (int16_t)rd16(g_mem, b), next_year = (int16_t)rd16(g_mem, b + 2);
            if (year == next_year && next - day >= 7 && day + 4 < 365) {
                memcpy(g_mem + tmpl + (uint32_t)k * DATE_SIZE, g_mem + a, DATE_SIZE);
                wr16(g_mem, tmpl + (uint32_t)k * DATE_SIZE, (uint16_t)(day + 4));
                k++;
                done++;
            }
        }
    }
    if (done < extra) {                 /* not enough room: keep the original template */
        game_free(tmpl);
        return;
    }
    game_free(old);
    wr16(g_mem, count_at, (uint16_t)k);
    cpu.eax = tmpl;
    if (logging()) fprintf(stderr, "recomp: new divisions: 3. Liga: %d dates for %d clubs\n", k, n);
}

/* GerRegional::set_up_rules 0x5dd9f0 (thiscall): the places of the 3. Liga row. The play-off of the 3rd
 * against the 16th of the 2. Bundesliga has no code (slice 19 builds play-offs inside one league), so its place
 * goes up and down directly (the nearest form, slice 19's decision); the 2. Bundesliga takes the same count
 * down (season_divisions_rules), so the pairs of 0x689c80 stay even. */
void o_005dd9f0(void);
RT_WRAP(005dd9f0, ger_regional_rules)
{
    uint32_t self = cpu.ecx;
    o_005dd9f0();
    if (rt_unwinding || !liga3_object(self)) return;
    const SeasonRule *r = liga3_row(season_of(self));
    if (r->up >= 0) g_mem[self + UP] = (uint8_t)(r->up + has_playoff(r));
    if (r->down >= 0) g_mem[self + DOWN] = (uint8_t)r->down;
    g_mem[self + UP_PLAYOFF] = 0;
    g_mem[self + DOWN_PLAYOFF] = 0;
}


/* GerRegional's South group (create_south_stage 0x5ddc50, thiscall (slot), ret 4): no group in the 3. Liga. The
 * stage slot is cleared, +0x30 stays -1 and +0x2c is 0, so the save, the load and the destructor (0x5dce80) see no
 * stage and the season ends as a league's with no stages. */
void o_005ddc50(void);
RT_WRAP(005ddc50, ger_regional_south)
{
    uint32_t self = cpu.ecx;
    if (!liga3_object(self)) {
        o_005ddc50();
        return;
    }
    uint32_t stages = rd32(g_mem, self + OBJ_STAGES);
    if (readable(stages)) wr32(g_mem, stages, 0);
    wr32(g_mem, self + OBJ_STAGE_NOW, 0xffffffffu);
    /* No stage at all: the stage count +0x2c (1 from the constructor, 0x5dcc3d) goes to 0. With 1,
     * League::update_for_date 0x6847c0 took -1 < 0 as a stage to come (0x6849d1..0x6849da), called the empty
     * advance_stage and skipped the season's end: the history (Club_History::record_comp_season 0x442e80 at
     * 0x6849fa), the honours (vtable +0x30 at 0x684a8f) and the complete flag +0x4c (0x684a9e). */
    wr32(g_mem, self + OBJ_STAGE_COUNT, 0);
    rt_ret_stdcall(0, 1);
}

static int no_stage(uint32_t obj) { return (int32_t)rd32(g_mem, obj + OBJ_STAGE_NOW) < 0; }

/* After League::load 0x687b10 of a GerRegional object (vtable 0x96b250, set at 0x5dcc17 before the load call at
 * 0x5dcc25) that holds the 3. Liga: the save keeps +0x2c 0, so the load makes no stage array (0x688e49) and +0xc
 * stays 0, where a new game has the constructor's one slot (0x5dcc44) cleared above. GerFirst's season start
 * reads that slot with no test (0x5dbf7f..0x5dbf82, called at 0x5db4c1). A cleared slot again (load sweep). */
void season_divisions_loaded(uint32_t obj)
{
    if (!readable(obj) || rd32(g_mem, obj) != GER_REGIONAL_VTABLE || !liga3_object(obj)) return;
    if (readable(rd32(g_mem, obj + OBJ_STAGES))) return;
    uint32_t slot = game_malloc(4);
    if (slot) wr32(g_mem, obj + OBJ_STAGES, slot);
}

/* The clubs of a table (+0x3e rows of 0x3b bytes at +0xb1, the club at +0). */
static uint32_t row_club(uint32_t league, int i)
{
    return rd32(g_mem, rd32(g_mem, league + OBJ_TABLE) + (uint32_t)i * ROW_SIZE);
}

/* GerRegional::set_predicted_positions 0x5de130 (thiscall): the original sorts North and the South stage
 * (0x5de1f3..0x5de27b) and gives positions 1, 3, 5, ... to each. The 3. Liga sorts its one table and gives
 * 1, 2, 3, ... */
void o_005de130(void);
RT_WRAP(005de130, ger_regional_positions)
{
    uint32_t self = cpu.ecx;
    if (!liga3_object(self) || !no_stage(self)) {
        o_005de130();
        return;
    }
    uint32_t owner = rd32(g_mem, self + OBJ_OWNER);
    int n = (int16_t)rd16(g_mem, self + OBJ_CLUBS);
    uint32_t list = readable(owner) && n > 0 ? game_malloc(4u * (uint32_t)n) : 0;
    if (list) {
        for (int i = 0; i < n; i++) wr32(g_mem, list + 4u * (uint32_t)i, row_club(self, i));
        uint32_t q[4] = {list, (uint32_t)n, 4u, F_BY_POSITION};
        call_cdecl(F_QSORT, 4, q);
        for (int i = 0; i < n && !rt_unwinding; i++) {
            uint32_t a[2] = {rd32(g_mem, list + 4u * (uint32_t)i), (uint32_t)(i + 1)};
            call_this(F_SET_POSITION, owner, 2, a);
        }
        game_free(list);
    }
    rt_ret_cdecl(0);
}

/* GerRegional::get_club_array 0x5ddff0 (thiscall: &array, &count; ret 8): 36 slots for North and South
 * (0x5de005..0x5de018). The 3. Liga gives its own clubs. */
void o_005ddff0(void);
RT_WRAP(005ddff0, ger_regional_clubs)
{
    uint32_t self = cpu.ecx, array_at = rt_arg(0), count_at = rt_arg(1);
    if (!liga3_object(self) || !no_stage(self)) {
        o_005ddff0();
        return;
    }
    int n = (int16_t)rd16(g_mem, self + OBJ_CLUBS);
    uint32_t list = n > 0 ? game_malloc(4u * (uint32_t)n) : 0;
    for (int i = 0; list && i < n; i++) wr32(g_mem, list + 4u * (uint32_t)i, row_club(self, i));
    wr32(g_mem, array_at, list);
    wr16(g_mem, count_at, (uint16_t)(list ? n : 0));
    rt_ret_stdcall(list, 2);
}

static void vcall(uint32_t obj, uint32_t slot)
{
    if (readable(obj)) call_this(rd32(g_mem, rd32(g_mem, obj) + slot), obj, 0, NULL);
}

/* GerFirst::process_promotion_relegation_chain 0x5db6c0 (thiscall (arg), ret 4): the original moves the 2.
 * Bundesliga's clubs to North at 0x5db76f and to the South stage at 0x5db79c, which it reads with no test. With
 * the 3. Liga (no stage) the chain is the same less the South stage. */
void o_005db6c0(void);
RT_WRAP(005db6c0, ger_chain)
{
    uint32_t self = cpu.ecx, reg = object_of(KEY_GER_REGIONAL);
    if (!divisions_on() || !reg || !no_stage(reg) || !liga3_object(reg)) {
        o_005db6c0();
        return;
    }
    uint32_t first = object_of(KEY_GER_FIRST), second = object_of(KEY_GER_SECOND);
    vcall(first, 0xa4);
    vcall(second, 0xa4);
    uint32_t a[6] = {first, second, 1, 1, 0xffffffffu, 0xffffffffu};
    if (first && second && !rt_unwinding) call_this(F_PROMOTE, self, 6, a);
    vcall(reg, 0xa4);
    uint32_t b[6] = {second, reg, 1, 1, 0xffffffffu, 0xffffffffu};
    if (second && !rt_unwinding) call_this(F_PROMOTE, self, 6, b);
    rt_ret_stdcall(0, 1);
}

/* 1 when a club belongs to the pool of new clubs of the lowest German tier, as 0x5db9bc..0x5dba13: a German club
 * with no division or in A Lower Division whose senior side (0x540a50) is not in German Regional or A Lower
 * Division, or has none. */
static int pool_club(uint32_t club, uint32_t flag_at, int32_t germany, int32_t lower, int32_t regional)
{
    uint32_t nation = rd32(g_mem, club + CLUB_NATION), div = rd32(g_mem, club + CLUB_DIVISION);
    if (!readable(nation) || (int32_t)rd32(g_mem, nation) != germany) return 0;
    if (div && (int32_t)rd32(g_mem, div) != lower) return 0;
    wr32(g_mem, flag_at, 0);
    uint32_t a[3] = {club, flag_at, 1};
    uint32_t senior = call_cdecl(F_SENIOR, 3, a);
    if (rt_unwinding || !senior || rd32(g_mem, flag_at)) return 1;
    uint32_t sdiv = rd32(g_mem, senior + CLUB_DIVISION);
    if (!sdiv) return 0;
    int32_t id = (int32_t)rd32(g_mem, sdiv);
    return id != regional && id != lower;
}

/* GerFirst's exchange of the lowest tier, ger_first_exchange_regional_lower_division_clubs 0x5db980 (thiscall,
 * ret): the relegated rows of North and South go to A Lower Division (0x5dba9e) and as many clubs come from the
 * pool (0x5dbb6f, 0x5dbbae). In a season of the 3. Liga the division ends at the count of its row: the relegated
 * rows of each table leave, then (the change from two groups, 2007/08 to 2008/09) the clubs below the row's count
 * leave by table position, North and South in turn, the clubs of the 2. Bundesliga first; then the pool fills it
 * to the count. Leavers go to A Lower Division as in the original (the German Cup takes its lower clubs from
 * there, 0x5d94c6). */
void o_005db980(void);
RT_WRAP(005db980, ger_exchange)
{
    uint32_t self = cpu.ecx, reg = object_of(KEY_GER_REGIONAL);
    const SeasonRule *r = divisions_on() && reg ? liga3_row(next_season_of(self)) : NULL;
    if (!r) {
        o_005db980();
        return;
    }
    int32_t germany = (int32_t)rd32(g_mem, KEY_GERMANY), lower = (int32_t)rd32(g_mem, KEY_LOWER);
    int32_t regional = (int32_t)rd32(g_mem, KEY_GER_REGIONAL);
    uint32_t lower_rec = record_of(KEY_LOWER), reg_rec = record_of(KEY_GER_REGIONAL);
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT) - 2 * (int)rd32(g_mem, NATION_COUNT);
    if (!lower_rec || !reg_rec || !readable(table) || count <= 0) {
        rt_ret_cdecl(0);
        return;
    }
    /* The pool, before any club leaves (as the original). */
    uint32_t pool = game_malloc(4u * POOL_MAX + 4u), flag_at = pool + 4u * POOL_MAX;
    int npool = 0;
    for (int i = 0; pool && i < count && npool < POOL_MAX && !rt_unwinding; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        if (pool_club(club, flag_at, germany, lower, regional)) wr32(g_mem, pool + 4u * (uint32_t)npool++, club);
    }
    /* The tables: North, and the South stage while it exists (the change from two groups). */
    uint32_t tables[2] = {reg, 0};
    uint32_t stages = rd32(g_mem, reg + OBJ_STAGES);
    if (!no_stage(reg) && readable(stages) && readable(rd32(g_mem, stages))) tables[1] = rd32(g_mem, stages);
    int out = 0, cut = 0, in = 0;
    for (int t = 0; t < 2; t++) {
        uint32_t lg = tables[t];
        int n = lg ? (int16_t)rd16(g_mem, lg + OBJ_CLUBS) : 0;
        for (int i = 0; i < n && !rt_unwinding; i++) {
            uint32_t row = rd32(g_mem, lg + OBJ_TABLE) + (uint32_t)i * ROW_SIZE, club = rd32(g_mem, row);
            if (g_mem[row + ROW_STATUS] != 3 || rd32(g_mem, club + CLUB_DIVISION) != reg_rec) continue;
            uint32_t a[3] = {club, lower_rec, 1};
            call_this(F_RELEGATE, self, 3, a);
            wr32(g_mem, club + CLUB_GROUP, 0);
            out++;
        }
    }
    /* Too many: the clubs of the tables below the count leave, by position, North and South in turn. */
    int now = clubs_in(reg_rec), want = r->clubs;
    int most = (tables[0] ? (int16_t)rd16(g_mem, tables[0] + OBJ_CLUBS) : 0);
    if (tables[1] && (int16_t)rd16(g_mem, tables[1] + OBJ_CLUBS) > most) most = (int16_t)rd16(g_mem, tables[1] + OBJ_CLUBS);
    if (now > want) {
        /* Count the kept clubs from the top: those that are not in a table (the 2. Bundesliga's) first. */
        int place = now;
        for (int t = 0; t < 2; t++) {
            uint32_t lg = tables[t];
            int n = lg ? (int16_t)rd16(g_mem, lg + OBJ_CLUBS) : 0;
            for (int i = 0; i < n; i++)
                if (rd32(g_mem, row_club(lg, i) + CLUB_DIVISION) == reg_rec) place--;
        }
        /* place: the clubs of the division in no table. Walk the positions down, North then South. */
        for (int i = 0; i < most && !rt_unwinding; i++)
            for (int t = 0; t < 2; t++) {
                uint32_t lg = tables[t];
                if (!lg || i >= (int16_t)rd16(g_mem, lg + OBJ_CLUBS)) continue;
                uint32_t club = row_club(lg, i);
                if (rd32(g_mem, club + CLUB_DIVISION) != reg_rec) continue;
                if (place < want) {
                    place++;
                    continue;
                }
                uint32_t a[3] = {club, lower_rec, 1};
                call_this(F_RELEGATE, self, 3, a);
                wr32(g_mem, club + CLUB_GROUP, 0);
                cut++;
            }
    }
    now = clubs_in(reg_rec);
    int need = want - now;
    if (need > npool) {
        fprintf(stderr, "recomp: new divisions: 3. Liga: %d new clubs needed, the pool holds %d\n", need, npool);
        need = npool;
    }
    if (need > 0 && !rt_unwinding) {
        uint32_t a[3] = {pool, (uint32_t)npool, (uint32_t)need};
        call_cdecl(F_PICK, 3, a);
        for (int i = 0; i < npool && in < need && !rt_unwinding; i++) {
            uint32_t m[3] = {rd32(g_mem, pool + 4u * (uint32_t)i), reg_rec, 1};
            call_this(F_MOVE, self, 3, m);
            in++;
        }
    }
    game_free(pool);
    /* The season that ends (+0x4c set by League::update_for_date at 0x684a9e when it ended with its history),
     * for tests/seasons/new_divisions_check.py; -1 for the change from two groups (the Regionalliga's season). */
    int complete = liga3_row((int16_t)rd16(g_mem, reg + OBJ_YEAR)) ? g_mem[reg + OBJ_COMPLETE] : -1;
    fprintf(stderr, "recomp: new divisions: 3. Liga %d: %d relegated, %d cut, %d new clubs from %d, %d clubs; season "
                    "%d complete %d, stages %d and %d\n", next_season_of(self), out, cut, in, npool, clubs_in(reg_rec),
            (int16_t)rd16(g_mem, reg + OBJ_YEAR), complete, (int32_t)rd32(g_mem, reg + OBJ_STAGE_COUNT),
            (int32_t)rd32(g_mem, reg + OBJ_STAGE_NOW));
    rt_ret_cdecl(0);
}

/* ger_first_split_regional_north_south 0x5dbbd0 (thiscall, ret): the original splits German Regional into 18
 * North and 18 South clubs by latitude and stops at ger_first.cpp 1049 (0x5dbcd4) with fewer than 36. In a season
 * of the 3. Liga every club of the division takes the North tag, the group of the one table
 * (GerRegional::create_table takes the North clubs, 0x5dd8dc..0x5dd8f1). */
void o_005dbbd0(void);
RT_WRAP(005dbbd0, ger_split)
{
    uint32_t self = cpu.ecx, reg = object_of(KEY_GER_REGIONAL);
    if (!divisions_on() || !reg || !liga3_row(next_season_of(self))) {
        o_005dbbd0();
        return;
    }
    uint32_t reg_rec = record_of(KEY_GER_REGIONAL), north = record_of(KEY_GER_NORTH), table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT);
    for (int i = 0; reg_rec && north && readable(table) && i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        if (rd32(g_mem, club + CLUB_DIVISION) == reg_rec) wr32(g_mem, club + CLUB_GROUP, north);
    }
    rt_ret_cdecl(0);
}

/* ---- Spain and Portugal: Primera Federación and Liga 3 in the Segunda B objects (20c, Q291) ---- */

/* One tier 3 object of groups: its own table is group 1, League_Stage objects in its stage slots (+0xc) are the
 * others. The original builds 4 groups of 20 in Spain (SpaSecondB: stages 0 to 2, 0x857210 called for 0, 1, 2 at
 * 0x85614c and 0x85712a) and 3 zones of 20 in Portugal (PorSecondB: stages 0 and 1, 0x7d2870 at 0x7d1b2c and
 * 0x7d284a). From 2021/22 both have 2 groups: group 1 and stage 0. */
typedef struct {
    const char *file;           /* the rules table file */
    uint32_t key;               /* the parent record (the object's record) */
    uint32_t top_key, second_key;
    uint32_t groups[2];         /* the group records of group 1 and stage 0 (club +0x60) */
    uint32_t ended[2];          /* the group records that end: their clubs go to no division (Q290) */
    const char *group_names[2], *ended_names[2];
    uint32_t nation_key;
    uint32_t out_key;           /* the division of relegated clubs (0x8503e4: A Lower Division; 0x7d010f: Third) */
    uint32_t pool_key;          /* a second pool division besides A Lower Division (0 for none) */
    int min_clubs, max_clubs;   /* the new form: a row with this many clubs in all */
} GroupTier;

static const GroupTier SPAIN = {
    "spa_second_b", 0x009cf7b8u, 0x009cf624u, 0x009cf628u,
    {0x009cf62cu, 0x009cf630u}, {0x009cf634u, 0x009cf638u},
    {"Spanish Second Division B1", "Spanish Second Division B2"},
    {"Spanish Second Division B3", "Spanish Second Division B4"},
    0x009cf488u, KEY_LOWER, 0, 32, 48,
};
static const GroupTier PORTUGAL = {
    "por_second_b", 0x009cf7c0u, 0x009cf60cu, 0x009cf610u,
    {0x009cf614u, 0x009cf618u}, {0x009cf61cu, 0},
    {"Portuguese Second Division B North", "Portuguese Second Division B Central"},
    {"Portuguese Second Division B South", NULL},
    0x009cf434u, 0x009cf620u, 0x009cf620u, 16, 30,
};

/* The row of the new form in a season, or NULL (Spain 2020/21 keeps the database's 4 groups of 20: Q524). */
static const SeasonRule *group_row(const GroupTier *g, int season)
{
    const SeasonRule *r = row_in(g->file, season, 0);
    return r && r->clubs >= g->min_clubs && r->clubs <= g->max_clubs && !(r->clubs & 1) ? r : NULL;
}

static const GroupTier *group_tier(uint32_t obj)
{
    if (!readable(obj)) return NULL;
    const char *f = season_change_file(obj);
    return !strcmp(f, SPAIN.file) ? &SPAIN : !strcmp(f, PORTUGAL.file) ? &PORTUGAL : NULL;
}

/* The clubs a group plays with in the season of an object (each group half of the row). */
static int group_size(const GroupTier *g, int season)
{
    const SeasonRule *r = group_row(g, season);
    return r ? r->clubs / 2 : 0;
}

/* The places of a group. Spain: the winner of each group goes up and the 2 play-off places go to the two
 * runners-up (Q521); 5 down in each group. Portugal: the winners of the two promotion series go up, so one a
 * series (Q521); the row's down count shared by the two series. */
static void group_places(const GroupTier *g, const SeasonRule *r, int *up, int *down)
{
    if (g == &SPAIN) {
        *up = r->up >= 0 ? (r->up + (has_playoff(r) ? 2 : 0)) / 2 : 1;
        *down = r->down >= 0 ? r->down : 0;
    } else {
        *up = r->up >= 0 ? r->up / 2 : 1;
        *down = r->down >= 0 ? r->down / 2 : 2;
    }
}

static int group_new_form(uint32_t obj)
{
    const GroupTier *g = group_tier(obj);
    return divisions_on() && g && group_row(g, season_of(obj)) != NULL;
}

/* Portugal's group sizes: PorSecondB::create_table 0x7d2400 stores 20 at 0x7d2412, and the zone builder
 * 0x7d2870 wants exactly 20 (cmp bp, 0x14 at 0x7d2965 and 0x7d29ff; League_Stage of push 0x14 at 0x7d2ab2). The
 * class does not reset +0xa7 (no store in 0x7d1920..0x7d2e40), so the new count resets it here. */
uint32_t rt_por_group_clubs(uint32_t obj, uint32_t original)
{
    if (!group_new_form(obj)) return original;
    int n = group_size(&PORTUGAL, season_of(obj));
    if (n < MIN_CLUBS / 2 || n > 20) return original;
    if ((uint32_t)n != rd16(g_mem, obj + OBJ_CLUBS)) wr16(g_mem, obj + OBJ_MATCHES, 0xffffu);
    return (uint32_t)n;
}

uint32_t rt_por_stage_clubs(uint32_t obj, uint32_t original)
{
    if (!group_new_form(obj)) return original;
    int n = group_size(&PORTUGAL, season_of(obj));
    return n >= MIN_CLUBS / 2 && n <= 20 ? (uint32_t)n : original;
}

/* The group builders (SpaSecondB 0x857210, PorSecondB 0x7d2870; thiscall (slot), ret 4): in the new form only
 * slot 0 is built. The other slots are cleared (start_new_season deletes the stages but keeps the pointers,
 * 0x856fd6, 0x7d26f6), and the stage count +0x2c becomes 1, so League::update_for_date 0x6847c0 calls no
 * advance_stage (it calls it while +0x30 is below +0x2c - 1, 0x6849d1..0x6849e3): no promotion play-off stage
 * (Spain's 0x857570 reads the four groups). The stage takes the places of a group. */
static void group_build(void (*original)(void), uint32_t class_bytes)
{
    uint32_t self = cpu.ecx;
    int slot = (int8_t)(rt_arg(0) & 0xff);
    if (!group_new_form(self)) {
        /* A season of the class's form after a load of a season in the new form: League::load gave the array
         * the saved count of slots (1), and the class's builder fills its own count (SpaSecondB 9 slots, push
         * 0x24 at 0x855fa5; PorSecondB 2, push 8 at 0x7d1985). The array grows to the class's size first (load
         * sweep). A new game and the base game have the constructor's array, so nothing changes there. */
        uint32_t stages = rd32(g_mem, self + OBJ_STAGES);
        if (divisions_on() && readable(stages) && rt_is_block(stages) && rt_block_size(stages) < class_bytes) {
            uint32_t old_bytes = rt_block_size(stages), grown = game_malloc(class_bytes);
            if (rt_unwinding) return;
            if (grown) {
                memcpy(g_mem + grown, g_mem + stages, old_bytes);
                wr32(g_mem, self + OBJ_STAGES, grown);
                game_free(stages);
                if (rt_unwinding) return;
            }
        }
        original();
        return;
    }
    uint32_t stages = rd32(g_mem, self + OBJ_STAGES);
    if (slot >= 1) {
        /* After a load the array has only the saved count of slots (League::load 0x688e3a..0x688e5b: 1 in the
         * new form), so a slot past the block is not written (load sweep). */
        uint32_t room = readable(stages) && rt_is_block(stages) ? rt_block_size(stages) / 4u
                                                                 : rd32(g_mem, self + 0x2cu);
        if (readable(stages) && (uint32_t)slot < room) wr32(g_mem, stages + 4u * (uint32_t)slot, 0);
        rt_ret_stdcall(0, 1);
        return;
    }
    original();
    if (rt_unwinding) return;
    wr32(g_mem, self + 0x2cu, 1);
    const GroupTier *g = group_tier(self);
    const SeasonRule *r = group_row(g, season_of(self));
    uint32_t stage = readable(stages) ? rd32(g_mem, stages) : 0;
    if (r && readable(stage)) {
        int up, down;
        group_places(g, r, &up, &down);
        g_mem[stage + UP] = (uint8_t)up;
        g_mem[stage + UP_PLAYOFF] = 0;
        g_mem[stage + DOWN_PLAYOFF] = 0;
        g_mem[stage + DOWN] = (uint8_t)down;
    }
}

void o_00857210(void);
RT_WRAP(00857210, spa_group_build) { group_build(o_00857210, 0x24u); }
void o_007d2870(void);
RT_WRAP(007d2870, por_group_build) { group_build(o_007d2870, 8u); }

/* The club moves of a new game in a season of the new form (season_clubs.c): a club of a group division of the tier
 * that no list keeps goes to the division of the original's relegated clubs (out_key: Portugal's Third Division,
 * 0x7d010f; Spain's A Lower Division, 0x8503e4), as the season change sends the clubs that leave the tier (the
 * exchange below, 0x8503e4 and 0x7d010f). Before (O260 A24), the club moves left them with no division: a 2021/22
 * start with Liga 3 had 23 more Portuguese clubs with no division, and the Portuguese Cup (PorCup::collect_entrants
 * 0x7ce220, 164 entrants at 0x7ce234; its second pass takes the Third Division [0x9cf620] and the Portuguese clubs
 * of A Lower Division [0x9cf764], 0x7ce381..0x7ce3a2) stopped at por_cup.cpp 74. 0 else (no division). */
uint32_t season_divisions_leaver_division(uint32_t db_division, int season)
{
    if (!divisions_on() || !db_division) return 0;
    const GroupTier *tiers[2] = {&SPAIN, &PORTUGAL};
    for (int t = 0; t < 2; t++) {
        const GroupTier *g = tiers[t];
        if (!group_row(g, season) || !nation_flag(g->nation_key)) continue;
        for (int k = 0; k < 2; k++)
            if (g->groups[k] && db_division == record_of(g->groups[k])) return record_of(g->out_key);
    }
    return 0;
}

/* The Portuguese Cup's lower clubs (PorCup::collect_entrants 0x7ce220, second pass 0x7ce378..0x7ce3ed): the Third
 * Division and Portuguese clubs of A Lower Division. The club moves now send the clubs that leave Liga 3 to the Third
 * Division (season_divisions_leaver_division), so this is a safety net only: in a game whose Liga 3 object plays
 * the new form the cup also takes Portuguese clubs with no division (Q528), and each such club is logged. The test
 * switch RECOMP_BOX_FIXES=off turns the net off. Else the original's jump. */
uint32_t rt_por_cup_none(uint32_t club)
{
    const char *off = getenv("RECOMP_BOX_FIXES");
    if (off && !strcmp(off, "off")) return 0;
    if (!divisions_on() || !nation_flag(PORTUGAL.nation_key)) return 0;
    /* The season of the cup's draw: the start season with changing rules off (Q232) or at a new game (the cup is
     * made before [0xadadfc] is filled, so the Liga 3 object is not found there); else the game date's season
     * (s16 day of the year and year at 0xae2c90: January to June belong to the season before). */
    int season = season_start_year();
    if (season_changing_rules() && !season_change_is_creating()) {
        int day = (int16_t)rd16(g_mem, 0x00ae2c90u), year = (int16_t)rd16(g_mem, 0x00ae2c92u);
        season = day < 181 ? year - 1 : year;
    }
    /* Clubs only: the last 2 x nations rows of the club table are national teams (0x53b900; the German Cup stops
     * there too, 0x5d9569..0x5d957c), and the original's null-division jump kept them out. */
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT) - 2 * (int)rd32(g_mem, NATION_COUNT);
    if (club < table || (club - table) / CLUB_SIZE >= (uint32_t)(count > 0 ? count : 0)) return 0;
    uint32_t nation = rd32(g_mem, club + CLUB_NATION);
    int take = group_row(&PORTUGAL, season) && readable(nation) &&
               (int32_t)rd32(g_mem, nation) == (int32_t)rd32(g_mem, PORTUGAL.nation_key);
    if (take)
        fprintf(stderr, "recomp: new divisions: Portuguese Cup %d takes %.40s with no division (safety net)\n", season,
                win_str(club + 4));
    return (uint32_t)take;
}

/* The set_up_rules of the two classes (SpaSecondB 0x856ef0: +0xbe 0, +0xbf 4, +0xc0 1, +0xc1 4 at
 * 0x856f2d..0x856f40; PorSecondB 0x7d2610: +0xbe 1, +0xc1 4 at 0x7d264e..0x7d2660): the places of a group. */
static void group_rules(void (*original)(void))
{
    uint32_t self = cpu.ecx;
    original();
    if (rt_unwinding || !group_new_form(self)) return;
    const GroupTier *g = group_tier(self);
    int up, down;
    group_places(g, group_row(g, season_of(self)), &up, &down);
    g_mem[self + UP] = (uint8_t)up;
    g_mem[self + UP_PLAYOFF] = 0;
    g_mem[self + DOWN_PLAYOFF] = 0;
    g_mem[self + DOWN] = (uint8_t)down;
}

void o_00856ef0(void);
RT_WRAP(00856ef0, spa_group_rules) { group_rules(o_00856ef0); }
void o_007d2610(void);
RT_WRAP(007d2610, por_group_rules) { group_rules(o_007d2610); }

/* The tier 3 object of a nation and 1 when it plays the new form in the season that starts after a top
 * league's season change (SpaFirst::start_new_season 0x84feb0 calls the chain, 0x850300 and 0x850530 before
 * +0x40 + 1 at 0x84ff3a; PorPrm 0x7cf9e0 the same at 0x7cfa0c..0x7cfa2d). */
static uint32_t group_next(const GroupTier *g, uint32_t top, const SeasonRule **row)
{
    uint32_t obj = object_of(g->key);
    *row = divisions_on() && obj ? group_row(g, next_season_of(top)) : NULL;
    return *row ? obj : 0;
}

/* The chains (SpaFirst 0x850700, PorPrm 0x7cfbf0; thiscall (arg), ret 4) read every stage slot with no test
 * (0x850836, 0x850864; 0x7cfd0a). With one group stage: the same calls less the other slots. */
static void group_chain(const GroupTier *g, void (*original)(void))
{
    uint32_t self = cpu.ecx, obj = object_of(g->key);
    if (!divisions_on() || !obj || rd32(g_mem, obj + 0x2cu) != 1 || !group_new_form(obj)) {
        original();
        return;
    }
    uint32_t top = object_of(g->top_key), second = object_of(g->second_key);
    uint32_t stages = rd32(g_mem, obj + OBJ_STAGES), stage = readable(stages) ? rd32(g_mem, stages) : 0;
    vcall(top, 0xa4);
    vcall(second, 0xa4);
    uint32_t a[6] = {top, second, 1, 1, 0xffffffffu, 0xffffffffu};
    if (top && second && !rt_unwinding) call_this(F_PROMOTE, self, 6, a);
    vcall(obj, 0xa4);
    if (readable(stage)) vcall(stage, 0xa4);
    uint32_t b[6] = {second, obj, 1, 1, 0xffffffffu, 0xffffffffu};
    if (second && !rt_unwinding) call_this(F_PROMOTE, self, 6, b);
    uint32_t c[6] = {second, stage, 1, 1, 0xffffffffu, 0xffffffffu};
    if (second && readable(stage) && !rt_unwinding) call_this(F_PROMOTE, self, 6, c);
    rt_ret_stdcall(0, 1);
}

void o_00850700(void);
RT_WRAP(00850700, spa_chain) { group_chain(&SPAIN, o_00850700); }
void o_007cfbf0(void);
RT_WRAP(007cfbf0, por_chain) { group_chain(&PORTUGAL, o_007cfbf0); }

/* The tables of a tier 3 object: its own and each stage that exists (0 to +0x30). */
static int tier_tables(uint32_t obj, uint32_t *out, int max)
{
    int n = 0;
    out[n++] = obj;
    uint32_t stages = rd32(g_mem, obj + OBJ_STAGES);
    int last = (int32_t)rd32(g_mem, obj + OBJ_STAGE_NOW);
    for (int i = 0; readable(stages) && i <= last && i < 8 && n < max; i++) {
        uint32_t st = rd32(g_mem, stages + 4u * (uint32_t)i);
        if (readable(st)) out[n++] = st;
    }
    return n;
}

/* Rule of a pool: 1 when a club of the nation is in one of the pool divisions or in none (Q523), and is not a
 * reserve side whose senior side plays in the tier (as 0x8504c9..0x8504ec). */
static int group_pool_club(const GroupTier *g, uint32_t club, uint32_t flag_at)
{
    uint32_t nation = rd32(g_mem, club + CLUB_NATION), div = rd32(g_mem, club + CLUB_DIVISION);
    if (!readable(nation) || (int32_t)rd32(g_mem, nation) != (int32_t)rd32(g_mem, g->nation_key)) return 0;
    if (div) {
        int32_t id = (int32_t)rd32(g_mem, div);
        if (id != (int32_t)rd32(g_mem, KEY_LOWER) && !(g->pool_key && id == (int32_t)rd32(g_mem, g->pool_key))) return 0;
    }
    wr32(g_mem, flag_at, 0);
    uint32_t a[3] = {club, flag_at, 1};
    uint32_t senior = call_cdecl(F_SENIOR, 3, a);
    if (rt_unwinding || !senior || rd32(g_mem, flag_at)) return 1;
    uint32_t sdiv = rd32(g_mem, senior + CLUB_DIVISION);
    return !sdiv || (int32_t)rd32(g_mem, sdiv) != (int32_t)rd32(g_mem, g->key);
}

/* The new clubs of the tier (Spain 0x850300, Portugal 0x7cff70; thiscall, ret): the relegated rows of each table
 * leave to the division of the original (0x8503e4, 0x7d010f) with no group (+0x60 = 0); at the change from more
 * groups the clubs below the row's count leave too, by table position, the groups in turn; then the pool fills the
 * tier to the row's count (0x4afbd0, 0x6830b0, as the original). */
static void group_refill(const GroupTier *g, void (*original)(void))
{
    uint32_t self = cpu.ecx;
    const SeasonRule *r;
    uint32_t obj = group_next(g, self, &r);
    if (!obj) {
        original();
        return;
    }
    uint32_t rec = record_of(g->key), out_rec = record_of(g->out_key), table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT) - 2 * (int)rd32(g_mem, NATION_COUNT);
    uint32_t pool = game_malloc(4u * POOL_MAX + 4u), flag_at = pool + 4u * POOL_MAX;
    if (!rec || !out_rec || !readable(table) || !pool) {
        game_free(pool);
        rt_ret_cdecl(0);
        return;
    }
    int npool = 0;
    for (int i = 0; i < count && npool < POOL_MAX && !rt_unwinding; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        if (group_pool_club(g, club, flag_at)) wr32(g_mem, pool + 4u * (uint32_t)npool++, club);
    }
    uint32_t tables[8];
    int nt = tier_tables(obj, tables, 8), out = 0, cut = 0, in = 0, most = 0;
    for (int t = 0; t < nt; t++) {
        int n = (int16_t)rd16(g_mem, tables[t] + OBJ_CLUBS);
        if (n > most) most = n;
        for (int i = 0; i < n && !rt_unwinding; i++) {
            uint32_t row = rd32(g_mem, tables[t] + OBJ_TABLE) + (uint32_t)i * ROW_SIZE, club = rd32(g_mem, row);
            if (g_mem[row + ROW_STATUS] != 3 || rd32(g_mem, club + CLUB_DIVISION) != rec) continue;
            uint32_t a[3] = {club, out_rec, 1};
            call_this(F_RELEGATE, self, 3, a);
            wr32(g_mem, club + CLUB_GROUP, 0);
            out++;
        }
    }
    int want = r->clubs, now = clubs_in(rec);
    if (now > want) {
        int place = now;            /* the clubs of the tier in no table (relegated from above) come first */
        for (int t = 0; t < nt; t++)
            for (int i = 0; i < (int16_t)rd16(g_mem, tables[t] + OBJ_CLUBS); i++)
                if (rd32(g_mem, row_club(tables[t], i) + CLUB_DIVISION) == rec) place--;
        for (int i = 0; i < most && !rt_unwinding; i++)
            for (int t = 0; t < nt; t++) {
                if (i >= (int16_t)rd16(g_mem, tables[t] + OBJ_CLUBS)) continue;
                uint32_t club = row_club(tables[t], i);
                if (rd32(g_mem, club + CLUB_DIVISION) != rec) continue;
                if (place < want) {
                    place++;
                    continue;
                }
                uint32_t a[3] = {club, out_rec, 1};
                call_this(F_RELEGATE, self, 3, a);
                wr32(g_mem, club + CLUB_GROUP, 0);
                cut++;
            }
    }
    int need = want - clubs_in(rec);
    if (need > npool) need = npool;
    if (need > 0 && !rt_unwinding) {
        uint32_t a[3] = {pool, (uint32_t)npool, (uint32_t)need};
        call_cdecl(F_PICK, 3, a);
        for (int i = 0; i < need && !rt_unwinding; i++) {
            uint32_t m[3] = {rd32(g_mem, pool + 4u * (uint32_t)i), rec, 1};
            call_this(F_MOVE, self, 3, m);
            in++;
        }
    }
    game_free(pool);
    fprintf(stderr, "recomp: new divisions: %s %d: %d relegated, %d cut, %d new clubs from %d, %d clubs\n", g->file,
            next_season_of(self), out, cut, in, npool, clubs_in(rec));
    rt_ret_cdecl(0);
}

/* The first-season merge of the group divisions into the tier (SpaSecondB::load_group_clubs 0x856cd0 at
 * 0x856d0e..0x856d3a for B1 to B4; PorSecondB::create_table 0x7d2400 at 0x7d243e..0x7d2462 for the three zones):
 * in the new form the groups that end are left out, so their clubs stay in their database division, unplayed, as
 * without the flag (Q527). Their keys hold an id that no record has while the original runs. */
static void group_merge(const GroupTier *g, void (*original)(void))
{
    uint32_t self = cpu.ecx, saved[2] = {0, 0};
    int on = group_new_form(self);
    for (int k = 0; on && k < 2; k++)
        if (g->ended[k]) {
            saved[k] = rd32(g_mem, g->ended[k]);
            wr32(g_mem, g->ended[k], 0x7fffffffu);
        }
    original();
    for (int k = 0; on && k < 2; k++)
        if (g->ended[k]) wr32(g_mem, g->ended[k], saved[k]);
}

void o_00856cd0(void);
RT_WRAP(00856cd0, spa_group_merge) { group_merge(&SPAIN, o_00856cd0); }
void o_007d2400(void);
RT_WRAP(007d2400, por_group_merge) { group_merge(&PORTUGAL, o_007d2400); }

void o_00850300(void);
RT_WRAP(00850300, spa_refill) { group_refill(&SPAIN, o_00850300); }
void o_007cff70(void);
RT_WRAP(007cff70, por_refill) { group_refill(&PORTUGAL, o_007cff70); }

/* The group deal (Spain 0x850530 wants exactly 80 clubs, box line 0x3f7; Portugal 0x7d0210 deals up to 60): the
 * clubs of the tier sorted by the original's compare 0x53fd40 (the city's latitude, inference; Portugal gives the
 * first 20 to South, 0x7d02e9), the first half to the southern group (Spain B2, Portugal Central), the rest to the
 * northern (B1, North), as the real Group 1 and Série A are the northern ones. */
static void group_deal(const GroupTier *g, void (*original)(void))
{
    uint32_t self = cpu.ecx;
    const SeasonRule *r;
    uint32_t obj = group_next(g, self, &r);
    if (!obj) {
        original();
        return;
    }
    (void)obj;
    uint32_t rec = record_of(g->key), north = record_of(g->groups[0]), south = record_of(g->groups[1]);
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT), n = 0;
    uint32_t list = rec && north && south ? game_malloc(4u * 64u) : 0;
    for (int i = 0; list && i < count && n < 64; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        if (rd32(g_mem, club + CLUB_DIVISION) == rec) wr32(g_mem, list + 4u * (uint32_t)n++, club);
    }
    if (list && n > 0) {
        uint32_t q[4] = {list, (uint32_t)n, 4u, F_BY_LATITUDE};
        call_cdecl(F_QSORT, 4, q);
        for (int i = 0; i < n && !rt_unwinding; i++)
            wr32(g_mem, rd32(g_mem, list + 4u * (uint32_t)i) + CLUB_GROUP, i < n / 2 ? south : north);
    }
    game_free(list);
    if (n != r->clubs)
        fprintf(stderr, "recomp: new divisions: %s %d: %d clubs dealt, the row has %d\n", g->file,
                next_season_of(self), n, r->clubs);
    rt_ret_cdecl(0);
}

void o_00850530(void);
RT_WRAP(00850530, spa_deal) { group_deal(&SPAIN, o_00850530); }
void o_007d0210(void);
RT_WRAP(007d0210, por_deal) { group_deal(&PORTUGAL, o_007d0210); }

/* The places of the division above a new tier 3 (season_sizes_rules): as many down as the tier sends up. */
static void group_above_rules(uint32_t obj, const GroupTier *g)
{
    if ((int32_t)rd32(g_mem, obj + OBJ_BELOW) != (int32_t)rd32(g_mem, g->key)) return;
    const SeasonRule *r = group_row(g, season_of(obj));
    if (!r) return;
    int up, down;
    group_places(g, r, &up, &down);
    g_mem[obj + DOWN] = (uint8_t)(2 * up);
    g_mem[obj + DOWN_PLAYOFF] = 0;
}

/* ---- Greece: the second group of the second tier (20d) ---- */

/* The second group of the Greek second tier (2013/14, 2014/15, 2021/22 to 2025/26) is a second GreSecond object
 * (constructor 0x5eb2b0, 0xee bytes, ruling body 0x20 at 0x5eb301) with a new record, made by slice 26's module
 * in the Greek entry 18 (create_greek_comps 0x8347c0 is stored at 0xb64278: entry 18 of 0xb63d60, 0x48 bytes an
 * entry). create_greek_comps has no extra division flag (0x834965..0x834b02), so the group is made whenever the
 * table holds it (Q363). Its rows take comp_file gre_second (Q520), so it is the second gre_second object of the
 * rules report and the names table. */
#define KEY_GRE_FIRST 0x009cf86cu
#define KEY_GRE_SECOND 0x009cf870u
#define KEY_GRE_LOWER 0x009cf874u       /* Greek Lower Division: the pool of 0x5eb120 */
#define GRE_GROUP_COMP "league group 2"
#define GRE_GROUP_MAX 14                /* GreSecond's table: malloc(0x33a) at 0x5ebcbb, 14 rows */
#define F_NEW 0x00944cf1u               /* operator new, cdecl (size) */
#define F_GRE_SECOND 0x005eb2b0u        /* GreSecond::GreSecond, thiscall (year, record), ret 8 */

static uint32_t gre_make(const SeasonCompDef *d, uint32_t rec, int year, int loading);
const SeasonCompDef season_div_gre_group = {GRE_GROUP_COMP, "gre_second", SEASON_DIV_GRE_RECORD, 18, gre_make,
                                            "Greek National B Division", "Second Division Group 2", -1};

/* The group's object, or 0: the gre_second object whose record is not the National B record. */
static uint32_t gre_group(void)
{
    uint32_t o = season_comps_object(&season_div_gre_group);
    return readable(o) ? o : 0;
}

static int gre_held(int season) { return season_comps_held(GRE_GROUP_COMP, season) == 1; }

/* The group's club count in a season: its row (the second gre_second row of the season), at most 14. */
static int gre_count(int season)
{
    const SeasonRule *r = season_rules_find(season, "gre_second", 1);
    int n = r && r->clubs >= MIN_CLUBS ? r->clubs : 0;
    return n > GRE_GROUP_MAX ? GRE_GROUP_MAX : n;
}

/* Fills a record from the Greek Lower Division up to n clubs, as 0x5eb120 takes its new clubs: the clubs of the
 * Lower Division sorted by 0x4afbd0 (0x5eb1d7..0x5eb20f), the first ones moved (0x6830b0 at 0x5eb23e). */
static int gre_fill(uint32_t rec, int n)
{
    uint32_t lower = record_of(KEY_GRE_LOWER), table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT), need = n - clubs_in(rec), npool = 0, in = 0;
    if (need <= 0 || !lower || !readable(table)) return 0;
    uint32_t pool = game_malloc(4u * POOL_MAX);
    for (int i = 0; pool && i < count && npool < POOL_MAX; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE;
        if (rd32(g_mem, club + CLUB_DIVISION) == lower) wr32(g_mem, pool + 4u * (uint32_t)npool++, club);
    }
    if (need > npool) need = npool;
    if (need > 0 && !rt_unwinding) {
        uint32_t a[3] = {pool, (uint32_t)npool, (uint32_t)need};
        call_cdecl(F_PICK, 3, a);
        for (int i = 0; i < need && !rt_unwinding; i++) {
            uint32_t m[3] = {rd32(g_mem, pool + 4u * (uint32_t)i), rec, 1};
            call_this(F_MOVE, 0, 3, m);         /* 0x6830b0 reads no this (0x6830b0: mov ecx, [0xdd7ef0]) */
            in++;
        }
    }
    game_free(pool);
    return in;
}

/* The module's make: a GreSecond object with the group's record. A new object (not a load) needs its clubs in
 * the record first, because the constructor calls create_table at once (0x5eb35e) and fails with fewer than its
 * count: the club moves of a new game put the list there, and a group that starts later in a game takes clubs
 * from the Lower Division. */
static uint32_t gre_make(const SeasonCompDef *d, uint32_t rec, int year, int loading)
{
    (void)d;
    if (!loading) {
        int n = gre_count(year), got = gre_fill(rec, n);
        fprintf(stderr, "recomp: new divisions: Greek second group %d: %d clubs (%d from the Lower Division)\n", year,
                clubs_in(rec), got);
        if (clubs_in(rec) < MIN_CLUBS) return 0;
    }
    uint32_t size[1] = {0xee};
    uint32_t obj = call_cdecl(F_NEW, 1, size);
    if (rt_unwinding || !readable(obj)) return 0;
    uint32_t args[2] = {(uint32_t)year, rec};
    call_this(F_GRE_SECOND, obj, 2, args);
    return rt_unwinding ? 0 : obj;
}

/* GreSecond::create_table 0x5ebcb0 (thiscall, ret) takes the clubs of the key id [0x9cf870] (0x5ebd54..0x5ebd64),
 * not of its own record. For the group's object the key holds the group's record id while the original runs. */
void o_005ebcb0(void);
RT_WRAP(005ebcb0, gre_second_table)
{
    uint32_t self = cpu.ecx, rec = rd32(g_mem, self + OBJ_RECORD), key = rd32(g_mem, KEY_GRE_SECOND);
    if (!divisions_on() || !readable(rec) || rec == record_of(KEY_GRE_SECOND)) {
        o_005ebcb0();
        return;
    }
    wr32(g_mem, KEY_GRE_SECOND, rd32(g_mem, rec));
    o_005ebcb0();
    wr32(g_mem, KEY_GRE_SECOND, key);
}

/* The places of the group (after GreSecond::set_up_rules 0x5ebde0, through season_sizes_rules), and of the
 * National A Division above while the group plays: its relegation places are the promotions of both groups, so
 * the pairs of 0x689c80 stay even (0x689df4 and 0x689dff mark a moved pair, so the second call moves the rest). The
 * joint play-offs of 2013/14, 2014/15 and 2021/22 have no code: the group winner goes up (Q521). */
/* A group's promotions in a season with two groups: its own row's up, at least 1 (the joint play-offs of 2013/14,
 * 2014/15 and 2021/22 have no code: the group winner goes up, Q521). Row 0 of gre_second is the National B Division,
 * row 1 the group (Q520). */
static int gre_up(int season, int row)
{
    const SeasonRule *r = season_rules_find(season, "gre_second", row);
    return r && r->up >= 1 ? r->up : 1;
}

static void gre_rules(uint32_t obj)
{
    uint32_t group = gre_group();
    if (!group) return;
    int s = season_of(obj);
    if (obj == group) {
        const SeasonRule *r = season_rules_find(s, "gre_second", 1);
        if (!r) return;
        g_mem[obj + UP] = (uint8_t)gre_up(s, 1);
        g_mem[obj + UP_PLAYOFF] = 0;
        g_mem[obj + DOWN_PLAYOFF] = 0;
        if (r->down >= 0) g_mem[obj + DOWN] = (uint8_t)r->down;
    } else if (!strcmp(season_change_file(obj), "gre_second") && gre_held(s)) {
        /* The National B Division in a season with the group: the two groups of one tier promote by one rule
         * (review 2). It is a floor division of slice 18 (its class keeps 2 up and 2 down), and its relegations stay
         * 2: the original's exchange 0x5eb120 brings 2 clubs back from the Lower Division (0x5eb219). */
        g_mem[obj + UP] = (uint8_t)gre_up(s, 0);
        g_mem[obj + UP_PLAYOFF] = 0;
    } else if (!strcmp(season_change_file(obj), "gre_prm") && gre_held(s)) {
        /* Both groups' promotions, from their rows (at a new game GrePrm's rules run before the groups' own). */
        g_mem[obj + DOWN] = (uint8_t)(gre_up(s, 0) + gre_up(s, 1));
        g_mem[obj + DOWN_PLAYOFF] = 0;
    }
}

/* gre_prm_swap_second_and_third_division_clubs 0x5eb120 (thiscall, ret), after the chain of GrePrm 0x689c20 with
 * the National B Division (0x5eaf62, 0x5eaf6a): the group's exchange with the National A Division (as the chain does
 * at 0x689c60), its new clubs from the Lower Division up to the next row's count, the original for the National B
 * Division, then the group's relegated rows to the Lower Division (as 0x5eb25d..0x5eb28a; all its clubs when the next
 * season has no group, Q522) and its season start. */
void o_005eb120(void);
RT_WRAP(005eb120, gre_swap)
{
    uint32_t self = cpu.ecx, group = divisions_on() ? gre_group() : 0;
    if (!group || rt_unwinding) {
        o_005eb120();
        return;
    }
    int now = (int16_t)rd16(g_mem, self + OBJ_YEAR), next = next_season_of(self), held = gre_held(next);
    uint32_t rec = rd32(g_mem, group + OBJ_RECORD), lower = record_of(KEY_GRE_LOWER);
    int played = gre_held(season_changing_rules() ? now : season_start_year()), out = 0, leaving = 0, in = 0;
    if (played && lower) {
        uint32_t a[6] = {self, group, 1, 1, 0xffffffffu, 0xffffffffu};
        call_this(F_PROMOTE, self, 6, a);
    }
    /* The group's relegated rows (status 3 of its table, still in its record). */
    int n = played ? (int16_t)rd16(g_mem, group + OBJ_CLUBS) : 0;
    for (int i = 0; i < n; i++) {
        uint32_t row = rd32(g_mem, group + OBJ_TABLE) + (uint32_t)i * ROW_SIZE;
        leaving += g_mem[row + ROW_STATUS] == 3 && rd32(g_mem, rd32(g_mem, row) + CLUB_DIVISION) == rec;
    }
    /* The new clubs first, while the leavers are still in the group, so neither the group's pool nor the original's
     * pool for the National B Division (0x5eb1d7..0x5eb20f) can take a club that has just gone down (as the German
     * and Iberian exchanges build their pools before any club leaves). */
    if (held && !rt_unwinding) in = gre_fill(rec, gre_count(next) + leaving);
    o_005eb120();
    if (rt_unwinding) return;
    if (lower && held) {
        for (int i = 0; i < n && !rt_unwinding; i++) {
            uint32_t row = rd32(g_mem, group + OBJ_TABLE) + (uint32_t)i * ROW_SIZE, club = rd32(g_mem, row);
            if (g_mem[row + ROW_STATUS] != 3 || rd32(g_mem, club + CLUB_DIVISION) != rec) continue;
            uint32_t m[3] = {club, lower, 1};
            call_this(F_RELEGATE, self, 3, m);
            out++;
        }
    } else if (lower) {                     /* no group next season: every club to the Lower Division (Q522) */
        uint32_t table = rd32(g_mem, CLUBS);
        int count = (int)rd32(g_mem, CLUB_COUNT);
        for (int i = 0; readable(table) && i < count && !rt_unwinding; i++) {
            uint32_t club = table + (uint32_t)i * CLUB_SIZE;
            if (rd32(g_mem, club + CLUB_DIVISION) != rec) continue;
            uint32_t m[3] = {club, lower, 1};
            call_this(F_RELEGATE, self, 3, m);
            out++;
        }
    }
    fprintf(stderr, "recomp: new divisions: Greek second group %d: %d out, %d in, %d clubs\n", next, out, in,
            clubs_in(rec));
    /* A league with a division above is started by its top league, not by the pass (0x48ce10, season_change.h):
     * GrePrm::start_new_season 0x5eaf50 starts the National B Division at its end (0x5eb0ec..0x5eb0f7, vtable +8)
     * and does not know the group. So the group starts here, with its clubs in place; with changing rules on, the
     * before hook of the rule kind below keeps it idle in a season with no group. */
    if (rt_unwinding) return;
    /* A group that the module makes at this change (gre_make, before GrePrm's start) is already in the next season:
     * the constructor sets its year and makes its table (0x5eb35e). A second start put it one year ahead (seed 3 of
     * the 0.6.0 long games, made in 2013): it then took its class's places (2 up) where GrePrm relegated 1 to
     * it, and at the 2015 end the Greek Cup had fewer than its 44 clubs (gre_cup.cpp 802, 0x5e9c24). */
    /* The edition it starts is the year after the National B Division's finished one (now + 1), not the rules season
     * next: with changing rules off next_season_of gives the start season for every change (Q232: the start season's
     * rules), so the group of a 2021 game (October 2021 database) held 2021 for ever and the league check of gate (a)
     * run 8 found Super League 2 South at edition 2021 at the end of 2022/23. With changing rules on next is now + 1. */
    if (held) {
        /* A group behind the National B Division (a save of a game that the old rule left at 2021) takes the finished
         * year first, so its start makes the edition now + 1, as divisions_before does for a group that comes back. */
        if ((int16_t)rd16(g_mem, group + OBJ_YEAR) < now) wr16(g_mem, group + OBJ_YEAR, (uint16_t)now);
        if ((int16_t)rd16(g_mem, group + OBJ_YEAR) < now + 1) vcall(group, 0x8);
    } else {
        /* No group next season: it stays idle with its finished season and waits with the year before its next
         * edition (2998 when none comes), as the rule kind's before hook does (review 2: its start_new_season would
         * make a table with no club). */
        int edition = season_comps_next_edition(GRE_GROUP_COMP, next);
        wr16(g_mem, group + OBJ_YEAR, (uint16_t)((edition > next ? edition : 2999) - 1));
    }
}

/* Rule kind "divisions": the group plays only in the seasons of the table. In a season with no group it is idle
 * (season_change_skip) with the year before its next edition; when it comes back, +0x40 is the year before, so its
 * start_new_season (0x5ebe90 adds 1) makes the new season's dates (the third case of season_comps.h). */
static void divisions_before(const SeasonChange *c)
{
    uint32_t group = gre_group();
    if (!group || c->comp != group) return;
    if (!gre_held(c->season)) {
        /* Idle: its year goes to the year before its next edition (2998 when none comes), as an object that
         * waits (season_comps.h), so the rules report and the screens do not take it for this season's group. */
        int next = season_comps_next_edition(GRE_GROUP_COMP, c->season);
        wr16(g_mem, group + OBJ_YEAR, (uint16_t)((next > c->season ? next : 2999) - 1));
        season_change_skip();
        return;
    }
    if ((int16_t)rd16(g_mem, group + OBJ_YEAR) < c->season - 1) wr16(g_mem, group + OBJ_YEAR, (uint16_t)(c->season - 1));
}

const SeasonRuleKind season_kind_divisions = {"divisions", NULL, NULL, NULL, NULL, divisions_before, NULL};

void season_divisions_rules(uint32_t obj)
{
    if (!divisions_on() || !readable(obj)) return;
    /* The 2. Bundesliga with the 3. Liga below (GerSecond::set_up_rules sets +0x20 to the regional record only
     * with the extra division flag, 0x5df441..0x5df457). */
    if (!strcmp(season_change_file(obj), "ger_second") &&
        (int32_t)rd32(g_mem, obj + OBJ_BELOW) == (int32_t)rd32(g_mem, KEY_GER_REGIONAL)) {
        const SeasonRule *r = liga3_row(season_of(obj));
        if (r && r->up >= 0) {
            g_mem[obj + DOWN] = (uint8_t)(r->up + has_playoff(r));
            g_mem[obj + DOWN_PLAYOFF] = 0;
        }
    }
    gre_rules(obj);
    if (!strcmp(season_change_file(obj), "spa_second")) group_above_rules(obj, &SPAIN);
    if (!strcmp(season_change_file(obj), "por_second")) group_above_rules(obj, &PORTUGAL);
}

/* ---- Club moves at a new game ---- */

int season_divisions_class_size(const char *division, int season)
{
    if (!divisions_on() || !division) return -1;
    int ita = season_ita_division_size(division, season);     /* slice 20b: Italy (season_divisions_ita.c) */
    if (ita >= 0) return ita;
    /* Germany: in a season of the 3. Liga the North division takes the 3. Liga with the flag, and keeps the
     * Regionalliga pool of 18 and 18 without it (the unplayed exchange 0x5db7b0 and the split need 36). */
    int north = !strcmp(division, "German Regional Division North"), south = !strcmp(division, "German Regional Division South");
    if ((north || south) && liga3_row(season)) {
        if (!ger_flag()) return 18;
        return north ? liga3_row(season)->clubs : 0;
    }
    /* Greece: the second group takes its row's count, at most 14 (its table). */
    if (!strcmp(division, SEASON_DIV_GRE_RECORD)) return gre_held(season) ? gre_count(season) : -1;
    /* Spain and Portugal in the new form, with the flag: the two groups take half of the row each and the
     * other group divisions none. Without the flag the tier has no object and the database stays. */
    const GroupTier *tiers[2] = {&SPAIN, &PORTUGAL};
    for (int t = 0; t < 2; t++) {
        const GroupTier *g = tiers[t];
        if (!group_row(g, season) || !nation_flag(g->nation_key)) continue;
        for (int k = 0; k < 2; k++)
            if (!strcmp(division, g->group_names[k])) return group_size(g, season);
        /* The groups that end keep their database clubs (-1), unplayed (Q527): the cups take them (PorCup
         * 0x7ce2d6..0x7ce2f0 counts the three zones; a 2021/22 start stopped at por_cup.cpp 74 when they had none). */
    }
    return -1;
}

int season_divisions_list_on(const char *division, int season)
{
    if (!division) return 1;
    if (!strcmp(division, "German Regional Division North") && liga3_row(season)) return divisions_on() && ger_flag();
    const GroupTier *tiers[2] = {&SPAIN, &PORTUGAL};
    for (int t = 0; t < 2; t++)
        for (int k = 0; k < 2; k++)
            if (!strcmp(division, tiers[t]->group_names[k]) && group_row(tiers[t], season))
                return divisions_on() && nation_flag(tiers[t]->nation_key);
    return 1;
}

int season_divisions_new_record(const char *division)
{
    return division && !strcmp(division, SEASON_DIV_GRE_RECORD);
}

void season_divisions_test_extra(void)
{
    const char *want = getenv("RECOMP_TEST_EXTRA");
    static const struct { const char *name; uint32_t key; } nations[] = {
        {"germany", KEY_GERMANY}, {"italy", 0x009cf360u}, {"spain", 0x009cf488u}, {"portugal", 0x009cf434u},
    };
    uint32_t table = rd32(g_mem, NATIONS);
    if (!want || !*want || !readable(table)) return;
    for (size_t i = 0; i < sizeof nations / sizeof nations[0]; i++) {
        int32_t id = (int32_t)rd32(g_mem, nations[i].key);
        if (!strstr(want, nations[i].name) || id < 0) continue;
        g_mem[table + (uint32_t)id * NATION_SIZE + NATION_SELECTED] |= EXTRA_DIVISION;
        fprintf(stderr, "recomp: new divisions: test: extra division of %s on\n", nations[i].name);
    }
}

