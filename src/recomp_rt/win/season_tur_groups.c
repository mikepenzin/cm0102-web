/* The Turkish Category B in three groups with the Summer 2002 database (spec
 * docs/specs/2026-10-24-summer-2002-db-spec.md, slice 5, Q16, Q19, Q29). Evidence and design:
 * docs/research/2026-10-24-summer-2002-turkish-groups.md.
 *
 * The original. TurSecondB (vtable 0x970694, record [0x9cf8e0] "Turkish 2. Division Category B") plays five
 * regional groups of 10: its own table takes the clubs of the division whose group (club +0x60) is G1
 * (create_table 0x9002c0: 10 at 0x9002d2, 10 rows at 0x9002cd), and create_group_stage 0x900780 builds slots 0 to 3
 * for G2 to G5 (keys 0x9cf8e8..0x9cf8f4 at 0x9007e1..0x900808; 10 clubs: push 0x28 at 0x900812, cmp at 0x90089d
 * and 0x900932, League_Stage of push 0xa at 0x9009e4). The constructor 0x8ff4c0 sets 11 stage slots (+0x2c 0xb) for
 * the promotion group (slot 4), five classification groups and a play-off, built by advance_stage 0x9006c0. The
 * clubs come from the five database divisions G1 to G5: TurFirst::create_table 0x8fddc0 moves them into the
 * division in the first season (club +0x60 = the old division, +0x57 = Category B; the 2001 compare 0x8fddce is
 * the start year, tools/recomp/seasons.py). TurFirst::start_new_season 0x8fe040 runs the chain 0x8fe250 (vcall
 * +0xb0), the exchange with the Lower Division 0x8fe3a0 (10 up by 0x4afbd0, the relegated rows down) and the deal
 * 0x8fe580 (all clubs of the division sorted by 0x53fe50, ten to each group).
 *
 * The data. Summer 2002 keeps the five groups of 10 in the divisions G1 to G5 (Q39: every division keeps its 3.9.68
 * club count, so the original game still loads it). Its three rows "Turkish 2. Division Category B Group A", "B" and
 * "C" (from the Xbox data, Q16) are the group tag (club +0x60) of the 52 real clubs of 2002/03
 * (tools/seasons/own_db/xbox0203/build_2002.py, step 6). The original game never reads that tag for a Turkish club:
 * its first-season merge overwrites +0x60 of the clubs of G1 to G5.
 *
 * The game. With those three rows each holding clubs at +0x60 (and the season code on; RECOMP_TURKISH_GROUPS=off is
 * the test switch), the first-season merge first puts each tagged club into G1, G2 or G3 and each untagged club of
 * G1 to G5 into the Lower Division. From then on TurSecondB plays its "three-group form", known from the clubs (no
 * club of the division in G4 or G5) at construction and from +0x2c == 2 after it:
 * - create_table takes the G1 clubs (Group A) and slots 0 and 1 the G2 and G3 clubs (Groups B and C), at most 18 a
 *   group (MAX_GROUP); slots 2 and 3 are not built and +0x2c is 2, so League::update_for_date 0x6847c0 calls no
 *   advance_stage (it calls it while +0x30 is below +0x2c - 1, 0x6849d1..0x6849e3; +0x30 is the last slot built,
 *   0x900a8e); League::load and the save keep the places (+0xbe..+0xc1: 0x6886d7, 0x689589; League_Stage 0x68c113,
 *   0x68cc27);
 * - each group: the winner up, the last two down (Q19: 2002/03, tr.wikipedia "2002-03 2. Futbol Ligi B
 *   Kategorisi"; the 2003/04 rule is the same), a double round robin on 2 (n - 1) dates for n clubs (2 n for an odd
 *   n: one club rests each round), made of the class's autumn dates (stage -1) and its spring dates (stage 4);
 * - the slots of the class that read five groups use the three-table code of the classes that have an own table and
 *   two group stages (PorSecondB, BelThird): positions 0x7d2cd0, progress 0x4241b0, and the base League honours
 *   0x684640, staff records 0x686940, news 0x48c6d0; the club array and the final positions read the three tables;
 * - the season change: the chain moves the three group winners up and Category A's relegated clubs down (the
 *   original's calls less slots 2 and 3), the exchange sends the relegated rows to the Lower Division and fills the
 *   division back to its size from the Lower Division (0x4afbd0), and the deal keeps each club in its group and
 *   gives the new clubs the places left (18, 17, 17 for 52).
 * Any other database (no such rows) runs the original. */
#include "win.h"
#include "../rt_host.h"
#include "season.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUBS 0x00ae23b4u           /* the club table (0x245 bytes a club), clubs then national teams */
#define CLUB_COUNT 0x00ae235cu
#define NATION_COUNT 0x00ae2350u    /* the last 2 x nations rows are national teams */
#define CLUB_SIZE 0x245u
#define CLUB_NATION 0x53u
#define CLUB_DIVISION 0x57u         /* club_comp pointer */
#define CLUB_LAST 0x5bu             /* club_comp pointer: last division */
#define CLUB_POSITION 0x5fu         /* last position */
#define CLUB_GROUP 0x60u            /* club_comp pointer: the group (TurFirst::create_table 0x8fddc0) */
#define COMPS 0x00ae23d0u           /* the club_comp records, 0x6b bytes each */
#define COMP_COUNT 0x00ae2378u
#define COMP_SIZE 0x6bu
#define COMP_OBJECTS 0x00adadfcu
#define COMP_SLOTS 0x00adadf8u

#define KEY_LOWER 0x009cf8ccu       /* Turkish Lower Division */
#define KEY_PRM 0x009cf8d8u         /* Turkish Premier Division */
#define KEY_A 0x009cf8dcu           /* Turkish 2. Division Category A */
#define KEY_B 0x009cf8e0u           /* Turkish 2. Division Category B */
#define KEY_G1 0x009cf8e4u          /* Turkish 2. Division Category B G1 to G5: 0x9cf8e4 + 4 k */

#define TSB_VTABLE 0x00970694u      /* TurSecondB (constructor 0x8ff4c0 at 0x8ff510) */
#define OBJ_RECORD 4u
#define OBJ_STAGES 0xcu
#define OBJ_STAGE_COUNT 0x2cu
#define OBJ_STAGE_NOW 0x30u
#define OBJ_CLUBS 0x3eu
#define OBJ_MATCHES 0xa7u
#define OBJ_TABLE 0xb1u
#define ROW_SIZE 0x3bu
#define ROW_STATUS 0x37u            /* 3: relegated */
#define UP 0xbeu
#define UP_PLAYOFF 0xbfu
#define DOWN_PLAYOFF 0xc0u
#define DOWN 0xc1u
#define CLASS_SLOTS 0xbu            /* +0x2c of the class (0x8ff52a) */
#define FORM_SLOTS 2u               /* +0x2c of the three-group form */
#define DATE_SIZE 0x41u             /* a date of the template (league_round_set_date 0x68a160) */

#define GROUPS 3
#define MIN_GROUP 2
#define MAX_GROUP 18                /* 2 (n - 1) dates fit in the 36 of stages -1 and 4 up to 18 clubs */
#define TIER_CLUBS 52               /* the division's size in every season: the 18, 17 and 17 of 2002/03 (Q16) */

#define F_MALLOC 0x00944e46u
#define F_FREE 0x009452cau
#define F_QSORT 0x00945333u
#define F_BY_LONGITUDE 0x0053fe50u  /* the compare of the original's deal (0x8fe60b) */
#define F_PICK 0x004afbd0u          /* cdecl (list, count, need): the best `need` clubs first (0x8fe40b) */
#define F_MOVE 0x006830b0u          /* League::move_club_to_division, thiscall (club, record, 1), ret 0xc */
#define F_RELEGATE 0x006831a0u      /* League::relegate_club_to_division, thiscall (club, record, 1), ret 0xc */
#define F_PROMOTE 0x00689c80u       /* league_promote_and_relegate (upper, lower, expire, pairwise, up, down) */
#define F_SCHEDULE 0x008ff7e0u      /* TurSecondB::create_schedule (stage, &count, &code, &flag), ret 0x10 */
#define F_POSITIONS3 0x007d2cd0u    /* PorSecondB::set_predicted_positions: own table and slots 0, 1 */
#define F_PROGRESS3 0x004241b0u     /* BelThird::set_club_progress: stages -1, 0, 1 of three groups, ret 0x14 */
#define F_HONOURS 0x00684640u       /* League::record_season_honours */
#define F_STAFF 0x00686940u         /* League::update_staff_records_for_status, ret 0x18 */
#define F_NEWS 0x0048c6d0u          /* Comp::build_fate_news_text, ret 0x28 */

static const char *const GROUP_NAMES[GROUPS] = {
    "Turkish 2. Division Category B Group A",
    "Turkish 2. Division Category B Group B",
    "Turkish 2. Division Category B Group C",
};

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

static int groups_on(void)
{
    static int off = -1;
    if (off < 0) off = getenv("RECOMP_TURKISH_GROUPS") && !strcmp(getenv("RECOMP_TURKISH_GROUPS"), "off");
    return !off && season_code_on();
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

static uint32_t record_of(uint32_t key)
{
    int32_t id = (int32_t)rd32(g_mem, key);
    uint32_t comps = rd32(g_mem, COMPS);
    if (id < 0 || !readable(comps)) return 0;
    return comps + (uint32_t)id * COMP_SIZE;
}

static uint32_t object_of(uint32_t key)
{
    int32_t id = (int32_t)rd32(g_mem, key), slots = (int32_t)rd32(g_mem, COMP_SLOTS);
    uint32_t table = rd32(g_mem, COMP_OBJECTS);
    if (id < 0 || id >= slots || !readable(table)) return 0;
    uint32_t o = rd32(g_mem, table + 4u * (uint32_t)id);
    return readable(o) ? o : 0;
}

static void vcall(uint32_t obj, uint32_t slot)
{
    if (readable(obj) && !rt_unwinding) call_this(rd32(g_mem, rd32(g_mem, obj) + slot), obj, 0, NULL);
}

/* The club rows of the table (national teams excluded). */
static int club_count(void)
{
    int n = (int)rd32(g_mem, CLUB_COUNT) - 2 * (int)rd32(g_mem, NATION_COUNT);
    return n > 0 ? n : 0;
}

static uint32_t club_at(int i) { return rd32(g_mem, CLUBS) + (uint32_t)i * CLUB_SIZE; }

static uint32_t group_record(int k) { return record_of(KEY_G1 + 4u * (uint32_t)k); }

/* The clubs of Category B in each of the five groups G1 to G5 (club +0x60). */
static void group_counts(int counts[5])
{
    uint32_t b = record_of(KEY_B), g[5];
    for (int k = 0; k < 5; k++) {
        g[k] = group_record(k);
        counts[k] = 0;
    }
    if (!b) return;
    for (int i = 0, n = club_count(); i < n; i++) {
        uint32_t club = club_at(i);
        if (rd32(g_mem, club + CLUB_DIVISION) != b) continue;
        uint32_t tag = rd32(g_mem, club + CLUB_GROUP);
        for (int k = 0; k < 5; k++)
            if (tag && tag == g[k]) counts[k]++;
    }
}

/* The three group rows of the data (Summer 2002), by name, or 0 (Q16: the form needs them). Read each time: the
 * loader frees and allocates the record table again for each database, so a kept pointer could name another row. */
static int group_rows(uint32_t tag[GROUPS])
{
    uint32_t comps = rd32(g_mem, COMPS), count = rd32(g_mem, COMP_COUNT);
    for (int k = 0; k < GROUPS; k++) tag[k] = 0;
    for (uint32_t i = 0; readable(comps) && i < count && i < 4096u; i++) {
        uint32_t r = comps + i * COMP_SIZE;
        for (int k = 0; k < GROUPS; k++)
            if (!tag[k] && !strncmp(win_str(r + 4u), GROUP_NAMES[k], 52)) tag[k] = r;
    }
    return tag[0] && tag[1] && tag[2];
}

/* 1 when the clubs make three groups: none in G4 or G5, G1 to G3 each from MIN_GROUP to MAX_GROUP. */
static int clubs_form(void)
{
    int c[5];
    group_counts(c);
    if (c[3] || c[4]) return 0;
    for (int k = 0; k < GROUPS; k++)
        if (c[k] < MIN_GROUP || c[k] > MAX_GROUP) return 0;
    return 1;
}

/* 1 when the TurSecondB object plays the three-group form: the data has the three group rows, and the object has
 * its two stage slots or (at construction) the clubs make three groups. */
static int form(uint32_t obj)
{
    uint32_t tag[GROUPS];
    if (!groups_on() || !readable(obj) || rd32(g_mem, obj) != TSB_VTABLE || !group_rows(tag)) return 0;
    uint32_t slots = rd32(g_mem, obj + OBJ_STAGE_COUNT);
    if (slots == FORM_SLOTS) return 1;
    return slots == CLASS_SLOTS && clubs_form();
}

static uint32_t stage_of(uint32_t obj, int slot)
{
    uint32_t stages = rd32(g_mem, obj + OBJ_STAGES);
    if (!readable(stages) || slot < 0 || (uint32_t)slot >= rd32(g_mem, obj + OBJ_STAGE_COUNT)) return 0;
    uint32_t st = rd32(g_mem, stages + 4u * (uint32_t)slot);
    return readable(st) ? st : 0;
}

/* The three tables of the form: own, slot 0, slot 1 (0 for a slot that is not built). */
static int tables_of(uint32_t obj, uint32_t out[GROUPS])
{
    out[0] = obj;
    out[1] = (int32_t)rd32(g_mem, obj + OBJ_STAGE_NOW) >= 0 ? stage_of(obj, 0) : 0;
    out[2] = (int32_t)rd32(g_mem, obj + OBJ_STAGE_NOW) >= 1 ? stage_of(obj, 1) : 0;
    return GROUPS;
}

static int table_clubs(uint32_t t) { return t ? (int16_t)rd16(g_mem, t + OBJ_CLUBS) : 0; }

static uint32_t row_club(uint32_t t, int i)
{
    return rd32(g_mem, rd32(g_mem, t + OBJ_TABLE) + (uint32_t)i * ROW_SIZE);
}

static void set_places(uint32_t t)
{
    g_mem[t + UP] = 1;
    g_mem[t + UP_PLAYOFF] = 0;
    g_mem[t + DOWN_PLAYOFF] = 0;
    g_mem[t + DOWN] = 2;
}

static const char LETTER[GROUPS] = {'A', 'B', 'C'};

/* The log of a season's groups (tests/seasons/turkish_groups_run.py reads it): one line a group with its clubs. */
static void log_groups(uint32_t obj)
{
    uint32_t t[GROUPS];
    tables_of(obj, t);
    int year = (int16_t)rd16(g_mem, obj + 0x40u);
    for (int k = 0; k < GROUPS; k++) {
        fprintf(stderr, "recomp: turkish groups: %d/%02d Group %c: %d clubs:", year, (year + 1) % 100, LETTER[k],
                table_clubs(t[k]));
        for (int i = 0; i < table_clubs(t[k]); i++)
            fprintf(stderr, "%s %.51s", i ? ";" : "", win_str(row_club(t[k], i) + 4u));
        fprintf(stderr, "\n");
    }
}

/* ---- the first season: the Summer 2002 tags to G1 to G3 */

/* TurFirst::create_table 0x8fddc0 (thiscall): in the first season (+0x40 is the start year, the compare at
 * 0x8fddce) the original moves the clubs of G1 to G5 into Category B. Before it, with the three tag rows in the
 * data, each tagged club in G1 to G5, the Lower Division, Category B or no division goes to G1, G2 or G3. A tagged
 * club of another division stays, its tag cleared: in a 2002/03 season game the club moves fill Category A with two
 * real Category B clubs (Siirt, Batman) where real Category A clubs are missing from the database. Untagged clubs
 * of G1 to G5 fill the places those clubs leave, then go to the Lower Division (+0x60 cleared). */
void o_008fddc0(void);
RT_WRAP(008fddc0, tur_first_table)
{
    uint32_t self = cpu.ecx;
    if (!groups_on() || (int16_t)rd16(g_mem, self + 0x40u) != (int)rt_season_year) {
        o_008fddc0();
        return;
    }
    uint32_t tag[GROUPS];
    group_rows(tag);
    uint32_t lower = record_of(KEY_LOWER), b = record_of(KEY_B), g[5];
    for (int k = 0; k < 5; k++) g[k] = group_record(k);
    int tagged[GROUPS] = {0, 0, 0}, n = club_count();
    for (int i = 0; tag[0] && tag[1] && tag[2] && i < n; i++) {
        uint32_t t = rd32(g_mem, club_at(i) + CLUB_GROUP);
        for (int k = 0; k < GROUPS; k++)
            if (t == tag[k]) tagged[k]++;
    }
    int on = lower && b && g[0] && g[4];
    for (int k = 0; k < GROUPS; k++)
        if (!tag[k] || tagged[k] < MIN_GROUP) on = 0;
    if (!on) {
        o_008fddc0();
        return;
    }
    int in[GROUPS] = {0, 0, 0}, out = 0, kept = 0, filled = 0, nspare = 0;
    static int spare[4096];             /* untagged clubs of the five groups, in the order of the club table */
    for (int i = 0; i < n; i++) {
        uint32_t club = club_at(i), div = rd32(g_mem, club + CLUB_DIVISION), t = rd32(g_mem, club + CLUB_GROUP);
        int k = t == tag[0] ? 0 : t == tag[1] ? 1 : t == tag[2] ? 2 : -1;
        int in_groups = 0, in_row = -1;
        for (int j = 0; j < 5; j++)
            if (div && div == g[j]) in_groups = 1;
        for (int j = 0; j < GROUPS; j++)
            if (div && div == tag[j]) in_row = j;   /* the club moves of a season game place listed clubs there */
        if (k < 0) k = in_row;
        if (k >= 0) {
            if ((!div || div == lower || div == b || in_groups || in_row >= 0) && in[k] < MAX_GROUP) {
                wr32(g_mem, club + CLUB_DIVISION, g[k]);
                in[k]++;
            } else {
                /* A full group (MAX_GROUP) or another division. No object plays a group row, and a club left in G4
                 * or G5 would end the form (clubs_form), so such a club goes to the Lower Division. */
                if (in_row >= 0 || in_groups) wr32(g_mem, club + CLUB_DIVISION, lower);
                kept++;
            }
            wr32(g_mem, club + CLUB_GROUP, 0);
        } else if (in_groups && nspare < 4096) {
            spare[nspare++] = i;
        }
    }
    /* A group short of its tagged count (a tagged club that stays in another division) takes untagged clubs of the
     * five groups, the group with the most places left first; the others go to the Lower Division. */
    for (int s = 0; s < nspare; s++) {
        uint32_t club = club_at(spare[s]);
        int best = -1;
        for (int k = 0; k < GROUPS; k++)
            if (in[k] < tagged[k] && in[k] < MAX_GROUP && (best < 0 || tagged[k] - in[k] > tagged[best] - in[best]))
                best = k;
        wr32(g_mem, club + CLUB_DIVISION, best >= 0 ? g[best] : lower);
        wr32(g_mem, club + CLUB_GROUP, 0);
        if (best >= 0) {
            in[best]++;
            filled++;
        } else {
            out++;
        }
    }
    fprintf(stderr, "recomp: turkish groups: three groups of %d, %d and %d clubs; %d tagged clubs stay in another "
            "division, %d clubs of the five groups fill their places, %d go to the Lower Division\n", in[0], in[1],
            in[2], kept, filled, out);
    o_008fddc0();
}

/* ---- TurSecondB in the three-group form */

static int stage_size;              /* the clubs of the group that create_group_stage builds, 0 outside it */

static int group_size(int k)
{
    int c[5];
    group_counts(c);
    return c[k];
}

/* create_table 0x9002c0: the count store 0x9002d2 and the table's bytes 0x9002cd (tools/recomp/tur_groups.py). */
uint32_t rt_tur_table_clubs(uint32_t obj, uint32_t original)
{
    if (!form(obj)) return original;
    uint32_t n = (uint32_t)group_size(0);
    /* The class never writes +0xa7 (no store in 0x8ff4c0..0x901c70); a class that changes its count resets it, as
     * GerRegional::create_table at 0x5dd815, so the new count resets it here. */
    if (n != rd16(g_mem, obj + OBJ_CLUBS)) wr16(g_mem, obj + OBJ_MATCHES, 0xffffu);
    return n;
}

uint32_t rt_tur_table_bytes(uint32_t obj, uint32_t original)
{
    return form(obj) ? (uint32_t)group_size(0) * ROW_SIZE : original;
}

/* create_group_stage 0x900780: the club array 0x900812, the two compares 0x90089d and 0x900932, the stage's count
 * 0x9009e4. */
uint32_t rt_tur_stage_clubs(uint32_t obj, uint32_t original)
{
    (void)obj;
    return stage_size > 0 ? (uint32_t)stage_size : original;
}

uint32_t rt_tur_stage_bytes(uint32_t obj, uint32_t original)
{
    (void)obj;
    return stage_size > 0 ? 4u * (uint32_t)stage_size : original;
}

/* create_group_stage 0x900780 (thiscall (slot), ret 4): slot 0 is Group B (G2), slot 1 Group C (G3). Slots 2 and 3
 * are not built; after a load the array holds only the saved slots (League::load 0x688e3a..0x688e5b), so a slot past
 * the block is not written. After slot 1 the stage count is 2. */
void o_00900780(void);
RT_WRAP(00900780, tur_group_stage)
{
    uint32_t self = cpu.ecx;
    int slot = (int8_t)(rt_arg(0) & 0xff);
    if (!form(self)) {
        o_00900780();
        return;
    }
    uint32_t stages = rd32(g_mem, self + OBJ_STAGES);
    if (slot >= 2 || slot < 0) {
        uint32_t room = readable(stages) && rt_is_block(stages) ? rt_block_size(stages) / 4u : FORM_SLOTS;
        if (readable(stages) && slot >= 0 && (uint32_t)slot < room) wr32(g_mem, stages + 4u * (uint32_t)slot, 0);
        rt_ret_stdcall(0, 1);
        return;
    }
    stage_size = group_size(slot + 1);
    o_00900780();
    stage_size = 0;
    if (rt_unwinding) return;
    uint32_t st = readable(stages) ? rd32(g_mem, stages + 4u * (uint32_t)slot) : 0;
    if (readable(st)) set_places(st);
    if (slot == 1) {
        wr32(g_mem, self + OBJ_STAGE_COUNT, FORM_SLOTS);
        log_groups(self);
    }
}

/* set_up_rules 0x900460 (thiscall): the class sets 0 up, 2 to the promotion group (+0xbf) and 0 down
 * (0x9004a0..0x9004b4). A group of the form: the winner up, two down. */
void o_00900460(void);
RT_WRAP(00900460, tur_group_rules)
{
    uint32_t self = cpu.ecx;
    o_00900460();
    if (rt_unwinding || !form(self)) return;
    set_places(self);
}

/* create_schedule 0x8ff7e0 (thiscall (stage, &count, &code, &flag), ret 0x10): stage -1 (the own table, 18 dates
 * from 13 August), slots 0 to 3 (stage 0 has 22) and the later stages (stage 4, the promotion group: 18 dates in
 * the spring). A group of n clubs of the form plays 2 (n - 1) rounds (2 n for an odd n): the dates of stage -1,
 * then those of stage 4, up to that count. The other outputs stay those of the call for the stage. */
static int in_schedule;

void o_008ff7e0(void);
RT_WRAP(008ff7e0, tur_group_schedule)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    if (in_schedule || stage < -1 || stage > 1 || !form(self)) {
        o_008ff7e0();
        return;
    }
    o_008ff7e0();
    if (rt_unwinding) return;
    uint32_t first = cpu.eax, saved_eax = cpu.eax;
    int n = group_size(stage + 1);
    int need = 2 * (n + (n & 1) - 1);
    uint32_t scratch = game_malloc(16);
    if (!scratch || !readable(count_at)) {
        cpu.eax = saved_eax;
        return;
    }
    in_schedule = 1;
    uint32_t a[4] = {0xffffffffu, scratch, scratch + 4u, scratch + 8u};
    uint32_t autumn = call_this(F_SCHEDULE, self, 4, a);
    int na = rt_unwinding ? 0 : (int16_t)rd16(g_mem, scratch);
    uint32_t b[4] = {4u, scratch, scratch + 4u, scratch + 8u};
    uint32_t spring = rt_unwinding ? 0 : call_this(F_SCHEDULE, self, 4, b);
    int nb = rt_unwinding ? 0 : (int16_t)rd16(g_mem, scratch);
    in_schedule = 0;
    if (rt_unwinding) return;
    uint32_t dates = need >= 2 && readable(autumn) && readable(spring) && na + nb >= need
                         ? game_malloc((uint32_t)need * DATE_SIZE) : 0;
    if (dates) {
        for (int i = 0; i < need; i++) {
            uint32_t from = i < na ? autumn + (uint32_t)i * DATE_SIZE : spring + (uint32_t)(i - na) * DATE_SIZE;
            memcpy(g_mem + dates + (uint32_t)i * DATE_SIZE, g_mem + from, DATE_SIZE);
        }
        game_free(first);
        wr16(g_mem, count_at, (uint16_t)need);
    } else {
        fprintf(stderr, "recomp: turkish groups: no dates for %d clubs (stage %d: %d and %d), the class's kept\n", n,
                stage, na, nb);
    }
    game_free(autumn);
    game_free(spring);
    game_free(scratch);
    if (rt_unwinding) return;
    cpu.eax = dates ? dates : saved_eax;
}

/* set_predicted_positions 0x901950 (thiscall): the class reads slots 0 to 3; PorSecondB's 0x7d2cd0 reads the own
 * table and slots 0 and 1 (20 clubs at most a table, malloc 0x50). */
void o_00901950(void);
RT_WRAP(00901950, tur_group_positions)
{
    uint32_t self = cpu.ecx;
    if (!form(self)) {
        o_00901950();
        return;
    }
    call_this(F_POSITIONS3, self, 0, NULL);
    if (rt_unwinding) return;
    rt_ret_stdcall(0, 0);
}

/* get_club_array 0x901800 (thiscall (&clubs, &count), ret 8): the class gives 50 clubs of five tables. The form:
 * the clubs of the three tables in that order. */
void o_00901800(void);
RT_WRAP(00901800, tur_group_club_array)
{
    uint32_t self = cpu.ecx, out = rt_arg(0), count_at = rt_arg(1);
    if (!form(self)) {
        o_00901800();
        return;
    }
    uint32_t t[GROUPS];
    tables_of(self, t);
    int total = 0;
    for (int k = 0; k < GROUPS; k++) total += table_clubs(t[k]);
    uint32_t list = game_malloc(4u * (uint32_t)(total > 0 ? total : 1));
    if (rt_unwinding) return;
    int n = 0;
    for (int k = 0; list && k < GROUPS; k++)
        for (int i = 0; i < table_clubs(t[k]); i++) wr32(g_mem, list + 4u * (uint32_t)n++, row_club(t[k], i));
    if (readable(out)) wr32(g_mem, out, list);
    if (readable(count_at)) wr16(g_mem, count_at, (uint16_t)(list ? n : 0));
    rt_ret_stdcall(list, 2);
}

/* record_final_positions 0x901ba0 (thiscall): as League's 0x68aa30 for each of the three tables: a club of the
 * division takes it as its last division (+0x5b) and its place in its group (+0x5f). */
void o_00901ba0(void);
RT_WRAP(00901ba0, tur_group_final)
{
    uint32_t self = cpu.ecx;
    if (!form(self)) {
        o_00901ba0();
        return;
    }
    uint32_t t[GROUPS], rec = rd32(g_mem, self + OBJ_RECORD);
    tables_of(self, t);
    for (int k = 0; k < GROUPS; k++)
        for (int i = 0; i < table_clubs(t[k]); i++) {
            uint32_t club = row_club(t[k], i);
            if (rd32(g_mem, club + CLUB_DIVISION) != rec) continue;
            wr32(g_mem, club + CLUB_LAST, rec);
            g_mem[club + CLUB_POSITION] = (uint8_t)(i + 1);
        }
    rt_ret_stdcall(0, 0);
}

/* record_season_honours 0x901c60 (thiscall, a jump to slot 4's +0x30): no promotion group in the form. League's
 * 0x684640, as PorSecondB (its North zone, the own table, is the record's table). */
void o_00901c60(void);
RT_WRAP(00901c60, tur_group_honours)
{
    uint32_t self = cpu.ecx;
    if (!form(self)) {
        o_00901c60();
        return;
    }
    call_this(F_HONOURS, self, 0, NULL);
    if (rt_unwinding) return;
    rt_ret_stdcall(0, 0);
}

static void forward(uint32_t fn, int nargs)
{
    uint32_t self = cpu.ecx, a[10];
    for (int i = 0; i < nargs; i++) a[i] = rt_arg(i);
    uint32_t r = call_this(fn, self, nargs, a);
    if (rt_unwinding) return;
    rt_ret_stdcall(r, nargs);
}

/* update_staff_records_for_status 0x9012d0 (ret 0x18): the class marks stage 0 to 3 winners as qualifiers
 * (0x9012e2..0x901307, event 0x458) and writes slot 4's results back into the group rows. The form: League's. */
void o_009012d0(void);
RT_WRAP(009012d0, tur_group_staff)
{
    if (!form(cpu.ecx)) {
        o_009012d0();
        return;
    }
    forward(F_STAFF, 6);
}

/* build_fate_news_text 0x9015f0 (ret 0x28): the promotion group and play-off texts. The form: Comp's. */
void o_009015f0(void);
RT_WRAP(009015f0, tur_group_news)
{
    if (!form(cpu.ecx)) {
        o_009015f0();
        return;
    }
    forward(F_NEWS, 10);
}

/* set_club_progress 0x901ac0 (ret 0x14): five groups (codes x 5) and the later stages. The form: BelThird's, three
 * groups (codes x 3 in stages -1 to 1). */
void o_00901ac0(void);
RT_WRAP(00901ac0, tur_group_progress)
{
    if (!form(cpu.ecx)) {
        o_00901ac0();
        return;
    }
    forward(F_PROGRESS3, 5);
}

/* advance_stage 0x9006c0 (thiscall): not reached in the form (create_group_stage stores the slot in +0x30 at
 * 0x900a8e, so +0x30 is 1 = +0x2c - 1); a call builds nothing. */
void o_009006c0(void);
RT_WRAP(009006c0, tur_group_advance)
{
    if (!form(cpu.ecx)) {
        o_009006c0();
        return;
    }
    rt_ret_stdcall(0, 0);
}

/* ---- the season change (TurFirst) */

static uint32_t form_object(void)
{
    uint32_t obj = object_of(KEY_B);
    return obj && form(obj) && rd32(g_mem, obj + OBJ_STAGE_COUNT) == FORM_SLOTS ? obj : 0;
}

/* TurFirst::process_promotion_relegation_chain 0x8fe250 (thiscall (arg), ret 4): the positions of the three
 * leagues, the Premier Division with Category A, then Category A with Category B's own table and slots 0 to 3
 * (0x8fe2e0..0x8fe38c, read with no test). The form: the same less slots 2 and 3. */
void o_008fe250(void);
RT_WRAP(008fe250, tur_chain)
{
    uint32_t self = cpu.ecx, obj = form_object();
    if (!obj) {
        o_008fe250();
        return;
    }
    uint32_t prm = object_of(KEY_PRM), a = object_of(KEY_A), t[GROUPS];
    vcall(self, 0xa4);
    vcall(a, 0xa4);
    vcall(obj, 0xa4);
    tables_of(obj, t);
    uint32_t p[6] = {prm, a, 1, 1, 0xffffffffu, 0xffffffffu};
    if (prm && a && !rt_unwinding) call_this(F_PROMOTE, self, 6, p);
    uint32_t a_rec = a ? rd32(g_mem, a + OBJ_RECORD) : 0, b_rec = rd32(g_mem, obj + OBJ_RECORD), down[8];
    int ndown = 0;
    for (int i = 0; a && i < table_clubs(a) && ndown < 8; i++) {
        uint32_t row = rd32(g_mem, a + OBJ_TABLE) + (uint32_t)i * ROW_SIZE;
        if (g_mem[row + ROW_STATUS] == 3) down[ndown++] = rd32(g_mem, row);
    }
    /* The clubs of the three tables with their place, read before 0x689c80 (which marks each moved row with 0xff,
     * 0x689df4, 0x689dff); after it a club whose division is Category A has gone up. */
    uint32_t was_club[GROUPS * MAX_GROUP];
    int was_group[GROUPS * MAX_GROUP], was_place[GROUPS * MAX_GROUP], nwas = 0;
    for (int k = 0; k < GROUPS; k++)
        for (int i = 0; i < table_clubs(t[k]) && nwas < GROUPS * MAX_GROUP; i++) {
            was_club[nwas] = row_club(t[k], i);
            was_group[nwas] = k;
            was_place[nwas++] = i + 1;
        }
    for (int k = 0; k < GROUPS; k++) {
        uint32_t q[6] = {a, t[k], 1, 0, 0xffffffffu, 0xffffffffu};
        if (a && t[k] && !rt_unwinding) call_this(F_PROMOTE, self, 6, q);
    }
    if (rt_unwinding) return;
    int year = (int16_t)rd16(g_mem, obj + 0x40u);
    for (int u = 0; u < nwas; u++)
        if (a_rec && rd32(g_mem, was_club[u] + CLUB_DIVISION) == a_rec)
            fprintf(stderr, "recomp: turkish groups: %d/%02d Group %c place %d up: %.51s (Category A)\n", year,
                    (year + 1) % 100, LETTER[was_group[u]], was_place[u], win_str(was_club[u] + 4u));
    for (int i = 0; i < ndown; i++)
        fprintf(stderr, "recomp: turkish groups: %d/%02d Category A down: %.51s (%s)\n", year, (year + 1) % 100,
                win_str(down[i] + 4u), rd32(g_mem, down[i] + CLUB_DIVISION) == b_rec ? "Category B" : "not moved");
    rt_ret_stdcall(0, 1);
}

static int division_clubs(uint32_t rec)
{
    int n = 0;
    for (int i = 0, c = club_count(); rec && i < c; i++)
        if (rd32(g_mem, club_at(i) + CLUB_DIVISION) == rec) n++;
    return n;
}

/* tur_first_promote_lower_clubs_to_third_tier 0x8fe3a0 (thiscall): 10 clubs of the Lower Division (club +0x53 set)
 * up by 0x4afbd0, then the relegated rows of the five tables down with +0x60 cleared. The form: the pool of the Lower
 * Division first, as the original (0x8fe3a6..0x8fe474: every club, before any club goes down), so no club that goes
 * down here comes back up; then the relegated rows of the three tables down (+0x60 cleared); at more clubs than the
 * size of the season played (the sum of the three tables), the lowest places go down too, the groups in turn; then
 * the pool fills the division to that size. */
void o_008fe3a0(void);
RT_WRAP(008fe3a0, tur_exchange)
{
    uint32_t self = cpu.ecx, obj = form_object();
    if (!obj) {
        o_008fe3a0();
        return;
    }
    uint32_t b = rd32(g_mem, obj + OBJ_RECORD), lower = record_of(KEY_LOWER), t[GROUPS];
    tables_of(obj, t);
    int want = TIER_CLUBS, most = 0, down = 0, cut = 0, up = 0, spare = 0;
    for (int k = 0; k < GROUPS; k++)
        if (table_clubs(t[k]) > most) most = table_clubs(t[k]);
    if (!readable(b) || !lower) {
        rt_ret_stdcall(0, 0);
        return;
    }
    int npool = 0, c = club_count();
    uint32_t pool = game_malloc(4u * (uint32_t)(c > 0 ? c : 1));
    if (!pool && !rt_unwinding) fprintf(stderr, "recomp: turkish groups: no memory for the pool: no club comes up\n");
    for (int i = 0; pool && i < c; i++) {
        uint32_t club = club_at(i);
        if (rd32(g_mem, club + CLUB_DIVISION) == lower && rd32(g_mem, club + CLUB_NATION))
            wr32(g_mem, pool + 4u * (uint32_t)npool++, club);
    }
    /* A Lower Division too small for the free places: the Turkish clubs with no division join the pool (as Q523's
     * pools of the new tiers), so the division keeps its size. */
    uint32_t nation = table_clubs(t[0]) ? rd32(g_mem, row_club(t[0], 0) + CLUB_NATION) : 0;
    int relegated = 0;
    for (int k = 0; k < GROUPS; k++)
        for (int i = 0; i < table_clubs(t[k]); i++) {
            uint32_t row = rd32(g_mem, t[k] + OBJ_TABLE) + (uint32_t)i * ROW_SIZE;
            if (g_mem[row + ROW_STATUS] == 3 && rd32(g_mem, rd32(g_mem, row) + CLUB_DIVISION) == b) relegated++;
        }
    int free_places = want - (division_clubs(b) - relegated);     /* the places left after the relegation */
    for (int i = 0; pool && nation && npool < free_places && i < c; i++) {
        uint32_t club = club_at(i);
        if (!rd32(g_mem, club + CLUB_DIVISION) && rd32(g_mem, club + CLUB_NATION) == nation) {
            wr32(g_mem, pool + 4u * (uint32_t)npool++, club);
            spare++;
        }
    }
    for (int k = 0; k < GROUPS && !rt_unwinding; k++)
        for (int i = 0; i < table_clubs(t[k]) && !rt_unwinding; i++) {
            uint32_t row = rd32(g_mem, t[k] + OBJ_TABLE) + (uint32_t)i * ROW_SIZE, club = rd32(g_mem, row);
            if (g_mem[row + ROW_STATUS] != 3 || rd32(g_mem, club + CLUB_DIVISION) != b) continue;
            uint32_t m[3] = {club, lower, 1};
            call_this(F_RELEGATE, self, 3, m);
            wr32(g_mem, club + CLUB_GROUP, 0);
            down++;
            fprintf(stderr, "recomp: turkish groups: %d/%02d Group %c place %d down: %.51s (%s)\n",
                    (int16_t)rd16(g_mem, obj + 0x40u), ((int16_t)rd16(g_mem, obj + 0x40u) + 1) % 100, LETTER[k], i + 1,
                    win_str(club + 4u), rd32(g_mem, club + CLUB_DIVISION) == lower ? "Lower Division" : "not moved");
        }
    for (int i = most - 1; i >= 0 && division_clubs(b) > want && !rt_unwinding; i--)
        for (int k = 0; k < GROUPS && division_clubs(b) > want && !rt_unwinding; k++) {
            if (i >= table_clubs(t[k])) continue;
            uint32_t club = row_club(t[k], i);
            if (rd32(g_mem, club + CLUB_DIVISION) != b) continue;
            uint32_t m[3] = {club, lower, 1};
            call_this(F_RELEGATE, self, 3, m);
            wr32(g_mem, club + CLUB_GROUP, 0);
            cut++;
        }
    int need = want - division_clubs(b);
    if (need > npool) need = npool;
    if (need > 0 && !rt_unwinding) {
        uint32_t a[3] = {pool, (uint32_t)npool, (uint32_t)need};
        call_cdecl(F_PICK, 3, a);
        for (int i = 0; i < need && !rt_unwinding; i++) {
            uint32_t m[3] = {rd32(g_mem, pool + 4u * (uint32_t)i), b, 1};
            call_this(F_MOVE, self, 3, m);
            up++;
        }
    }
    game_free(pool);
    if (rt_unwinding) return;
    fprintf(stderr, "recomp: turkish groups: %d relegated, %d cut, %d up from the Lower Division of %d (%d with no "
            "division), %d clubs\n", down, cut, up, npool, spare, division_clubs(b));
    if (division_clubs(b) != want)
        fprintf(stderr, "recomp: turkish groups: the division holds %d clubs, not %d: too few clubs to fill it\n",
                division_clubs(b), want);
    rt_ret_stdcall(0, 0);
}

/* tur_first_assign_third_tier_groups 0x8fe580 (thiscall): up to 50 clubs of the division sorted by 0x53fe50, ten to
 * each of G1 to G5 (the sort is not regional: a 2003/04 run of this deal mixed Igdir, Istanbul and Bursa in one
 * group). The form keeps the real 2002/03 regions: the sizes are three parts (18, 17, 17 for 52; the first groups
 * one more when the count does not divide); a club whose group (+0x60) is G1, G2 or G3 keeps it while the group has
 * room, in the order of the club table (a club back from Category A keeps its old group too); the other clubs,
 * sorted by 0x53fe50 as the original, take the places left, Group A first. */
void o_008fe580(void);
RT_WRAP(008fe580, tur_deal)
{
    uint32_t obj = form_object();
    if (!obj) {
        o_008fe580();
        return;
    }
    uint32_t b = rd32(g_mem, obj + OBJ_RECORD), g[GROUPS], list = game_malloc(4u * 64u);
    if (!list && !rt_unwinding) fprintf(stderr, "recomp: turkish groups: no memory for the deal: no club changes group\n");
    for (int k = 0; k < GROUPS; k++) g[k] = group_record(k);
    int n = 0;
    for (int i = 0, c = club_count(); list && i < c && n < 64; i++)
        if (rd32(g_mem, club_at(i) + CLUB_DIVISION) == b) wr32(g_mem, list + 4u * (uint32_t)n++, club_at(i));
    int size[GROUPS], have[GROUPS] = {0, 0, 0}, rest = 0, kept = 0;
    for (int k = 0; k < GROUPS; k++) size[k] = n / GROUPS + (k < n % GROUPS);
    for (int i = 0; list && i < n; i++) {     /* the clubs that keep their group; the rest move to the front */
        uint32_t club = rd32(g_mem, list + 4u * (uint32_t)i), tag = rd32(g_mem, club + CLUB_GROUP);
        int k = tag == g[0] ? 0 : tag == g[1] ? 1 : tag == g[2] ? 2 : -1;
        if (k >= 0 && have[k] < size[k]) {
            have[k]++;
            kept++;
        } else {
            wr32(g_mem, club + CLUB_GROUP, 0);
            wr32(g_mem, list + 4u * (uint32_t)rest++, club);
        }
    }
    if (list && rest > 1 && !rt_unwinding) {
        uint32_t q[4] = {list, (uint32_t)rest, 4u, F_BY_LONGITUDE};
        call_cdecl(F_QSORT, 4, q);
    }
    for (int i = 0, k = 0; list && i < rest && !rt_unwinding; i++) {
        while (k < GROUPS - 1 && have[k] >= size[k]) k++;
        wr32(g_mem, rd32(g_mem, list + 4u * (uint32_t)i) + CLUB_GROUP, g[k]);
        have[k]++;
    }
    game_free(list);
    if (rt_unwinding) return;
    fprintf(stderr, "recomp: turkish groups: deal of %d clubs: %d keep their group, %d new; %d, %d and %d\n", n, kept,
            rest, have[0], have[1], have[2]);
    /* The exchange keeps the division at the size of the season played (its cut), and the first season caps each
     * group at MAX_GROUP, so the parts stay within MIN_GROUP to MAX_GROUP; this line would show a break of that. */
    if (size[0] > MAX_GROUP || size[GROUPS - 1] < MIN_GROUP)
        fprintf(stderr, "recomp: turkish groups: %d clubs make groups of %d to %d\n", n, size[GROUPS - 1], size[0]);
    rt_ret_stdcall(0, 0);
}
