/* Euro qualifying places (0.6.0 slice 27): euro_champ_qual (ctor 0x57ce10, vtable 0x96a0d4) gives the real
 * places of the Euro of 24 teams in a season game (season_intl_on()). The Euro year is the campaign year +0x40
 * plus 2. The base game and every Euro of 16 teams keep the original's places.
 *
 * The original (the 2000 format, read from the code):
 * - Groups: the League object is group A (index -1), stage[0..7] are groups B to I (built at 0x57ff2e, loop
 *   "cmp edi, 8" at 0x57ff34). +0xff is the last group stage (7), +0x100 the stage of the runners-up ranking (8,
 *   built by 0x57f920 and refreshed by 0x57fa10), +0x101 the play-off stage (9, built by 0x57ef50 from the stage
 *   step 0x57ec60 when the stage index reaches +0x100 or +0x101). Stage count 10 (0x57d00b).
 * - Places of a group: the rule bytes +0xbe and +0xbf (set_up_rules 0x57e8f0 at 0x57e95c, 0x57e963), shared by
 *   the group stages (0x57ee22 passes &+0xbe). The stage end (0x684db0..0x68529d) gives the first +0xbe places
 *   code 7 (flag 4 of +0xd9, 0x6851c8) and the next +0xbf places code 1 (0x685213). Code 7 in a group stage
 *   makes the Euro add the team (vtable +0x44 0x57d5b0: 0x57d609), and code 1 of the play-off stage too
 *   (0x57d6a0).
 * - Play-off (0x57ef50): the best runners-up of the ranking qualify (3 with one host, 1 with two: 0x57f014), the
 *   last 6 or 8 of the ranking play two-legged ties (0x57f115, template 0x57d4f3).
 *
 * Euro 2016 (24 teams, campaign 2014): 9 groups (the original's), the top two direct (+0xbe 2), the third
 * ranked (+0xbf 1, the ranking takes place 3 at 0x57fb4f), the best third direct and the other 8 in 4 ties (the
 * original's two-host shape: 0x57f014, 0x57f115, 0x57d4f3). Group sizes 8 of six and 1 of five (room for 53:
 * Group sizes spread the pool over the groups (rt_euroq_group_size). 18 + 1 + 4 + host France = 24.
 * Euro 2020 and later (24 teams): 10 groups (+0xff 8, stage[8] built by the wrap of 0x57ed10), the top two
 * direct (+0xbe 2, +0xbf 0), no ranking stage (+0x100 -1: 0x57ff50 and 0x57ff9c skip 0x57f920 and 0x57fa10).
 * The stage step still calls 0x57ef50 at stage 9 (+0x101), which takes its no-ranking path (0x57ef71: news for
 * the teams that are out) and makes no stage, so the qualifying then ends (the wrap of 0x57ec60 below). 20
 * places. Slice 28 adds the Nations League paths there.
 * Inference: Euro 2028 takes the 2020 shape (the brief names 2020 and 2024).
 *
 * The bytes +0xff..+0x101 are not in the save (the extra save 0x580130..0x5806cc writes +0xf6 and others, not
 * these), so they are set again after a load (ctor epilogue 0x57d03b). */
#include "win.h"
#include "season_intl.h"
#include "season_nl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QUAL_VTABLE 0x96a0d4u
#define EURO_VTABLE 0x96a020u
#define GROUP_STAGE 0x0057ed10u         /* group stage builder, thiscall (index, teams, 6), ret 0xc */
#define NATION_NAME 0x4u
#define F_MALLOC 0x00944e46u            /* malloc, cdecl (size) */
#define F_FREE 0x009452cau              /* free, cdecl (block) */

enum { ORIGINAL, NINE_GROUPS, TEN_GROUPS };

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

/* The format of the qualifying of an object, -1 for none (base game, another class). */
static int format_of(uint32_t self)
{
    if (!season_intl_on() || !readable(self) || rd32(g_mem, self) != QUAL_VTABLE) return -1;
    int year = (int16_t)rd16(g_mem, self + 0x40u) + 2;
    const SeasonIntlEdition *e = season_intl_edition("euro_champ", year);
    if (!e || e->teams != 24) return ORIGINAL;
    return year == 2016 ? NINE_GROUPS : year > 2016 ? TEN_GROUPS : ORIGINAL;
}

/* set_up_rules 0x57e8f0 (after 0x57e963) and the ctor epilogue (0x57d03b, also after a load): the places and
 * stage indices of the format. The original's values: +0xbe 1, +0xbf 1 (0x57e95c, 0x57e963), +0xff 7, +0x100 8,
 * +0x101 9 (0x57cea2..0x57ceb0). */
void rt_euroq_rules(uint32_t self)
{
    int f = format_of(self);
    if (f < 0) return;
    static const uint8_t bytes[3][5] = {{1, 1, 7, 8, 9}, {2, 1, 7, 8, 9}, {2, 0, 8, 0xff, 9}};
    if (!season_nl_in_ctor()) {                 /* a Nations League keeps its own (saved) places */
        wr8(g_mem, self + 0xbeu, bytes[f][0]);
        wr8(g_mem, self + 0xbfu, bytes[f][1]);
    }
    wr8(g_mem, self + 0xffu, bytes[f][2]);
    wr8(g_mem, self + 0x100u, bytes[f][3]);
    wr8(g_mem, self + 0x101u, bytes[f][4]);
}

/* The ctor 0x57ce10 at 0x57d021, when the game year is past the campaign year +0x40: 1 skips the load of the
 * 2002 campaign (load_2002_campaign 0x580bd0) for another campaign. Only a season game skips it. The base game
 * (2001/02 start, rules off) keeps the original's load, also in a loaded save of a later year. */
uint32_t rt_euroq_skip_2002(uint32_t self)
{
    int campaign = (int16_t)rd16(g_mem, self + 0x40u);
    uint32_t skip = season_intl_on() && campaign != 2002;
    if (logging())
        fprintf(stderr, "recomp: season intl: euroq campaign %d: the 2002 load %s\n", campaign,
                skip ? "skipped" : "kept");
    return skip;
}

/* The host count that sizes the play-off (0x57f014: best runners-up, 0x57f115: play-off teams, 0x57d4f3:
 * template). Euro 2016 takes the shape of two hosts: 1 best third and 8 in the play-off. A Euro with no host
 * (2020, 2028, O260 A13) takes it too: the template 0x57d4f3 has 6 teams for 1 host, 8 for 2 and the box
 * "euro_champ_qual.cpp 283" (0x57d597) for 0, and 8 teams give the 4 places that no host leaves free. */
uint32_t rt_euroq_hosts(uint32_t self, uint32_t original)
{
    int f = format_of(self);
    return f == NINE_GROUPS || (f >= 0 && (original & 0xffu) == 0) ? 2u : original;
}

/* The place that the ranking takes from each group (0x57fb4f, 1 the runner-up): the third for Euro 2016.
 * Inference: the ranking compares the results against the top 4 of the group (0x57fb74..0x57fb8e), the real
 * rule of 2016 against the first, second, fourth and fifth. */
uint32_t rt_euroq_ranked_place(uint32_t self, uint32_t original)
{
    return format_of(self) == NINE_GROUPS ? 2u : original;
}

/* The direct places of a group that 0x57f920 puts back after the ranking stage (0x57f9e8, 1). */
uint32_t rt_euroq_direct(uint32_t self, uint32_t original)
{
    int f = format_of(self);
    return f == NINE_GROUPS || f == TEN_GROUPS ? 2u : original;
}

/* 1 when the format has no ranking stage (0x57ff50 skips 0x57f920, 0x57ff9c skips 0x57fa10). */
uint32_t rt_euroq_no_ranking(uint32_t self)
{
    return format_of(self) == TEN_GROUPS && (int8_t)rd8(g_mem, self + 0x100u) < 0;
}

/* The size of a group (get_group_team_count 0x57d9d0, its results 5 at 0x57da6e, 0x57da8e and 6 at 0x57da9b).
 * The group index is -1 (group A, 0x57e7e9) to +0xff (the draw pushes order - 1 at 0x57e76b, the group stages
 * their index at 0x57ed5f). The original's sizes fit its pool: with two hosts 4 groups of six and 5 of five
 * hold the 49 teams, with one host 3 of six and 6 of five hold 48 of 50 (two teams stay out). Its draw
 * (0x57e6e6..0x57e7ce) deals the teams of +0xee in 6 rounds, one team per group and round in a random group
 * order, to a group that has fewer teams than its size, until the pool count +0xf2 is dealt. A group stage then
 * copies "size" teams from the 6 places of its group at +0xf7 (0x57edbe..0x57eddb), so a size above the dealt
 * teams gives empty places and fixtures with no team (box "Database.cpp 13471" from 0x53b900). So with 24 teams
 * the sizes spread the pool count over the groups and sum to it: count / groups each, one more for the first
 * count % groups groups (50 in 9 groups: 5 of six, 4 of five; 50 in 10 groups: 10 of five). */
uint32_t rt_euroq_group_size(uint32_t self, uint32_t group, uint32_t original)
{
    int f = format_of(self);
    if (f != NINE_GROUPS && f != TEN_GROUPS) return original;
    int n = (int)rd32(g_mem, self + 0xf2u), groups = (int8_t)rd8(g_mem, self + 0xffu) + 2;
    int index = (int8_t)group + 1;
    if (n <= 0 || groups <= 0 || index < 0 || index >= groups) return original;
    int size = n / groups + (index < n % groups);
    return (uint32_t)(size > 6 ? 6 : size);
}

/* 1 when the format has 10 groups: the draw 0x57dab0 sets its group order 0 to 8 and the dealt counts of 9
 * groups (0x57dae0..0x57db2b), so with 10 groups the order entry 9 and its count are set too (0x57db30). */
uint32_t rt_euroq_ten_groups(uint32_t self)
{
    return format_of(self) == TEN_GROUPS;
}

/* The log of a qualifier added to the Euro (0x57d605 for code 7 of a group stage or of the ranking stage,
 * 0x57d69c for the play-off). euro: the EuroChamp object, stage: the stage index of the result. */
void rt_euroq_log(uint32_t euro, uint32_t club, uint32_t stage, uint32_t playoff)
{
    if (!logging() || !season_intl_on() || !readable(euro) || rd32(g_mem, euro) != EURO_VTABLE || !readable(club))
        return;
    int year = (int16_t)rd16(g_mem, euro + 0x40u);
    uint32_t nation = rd32(g_mem, club + 0x53u);
    const char *name = readable(nation) ? win_str(nation + NATION_NAME) : "?";
    const SeasonIntlEdition *e = season_intl_edition("euro_champ", year);
    const char *kind = "group";
    if (playoff) kind = "play-off";
    else if ((int8_t)stage == 8 && year <= 2016) kind = e && e->teams == 24 ? "best third" : "best runner-up";
    fprintf(stderr, "recomp: season intl: euro qualifier %d: %s (%s)\n", year, name, kind);
}

/* ---- The tenth group ---- */

void o_0057ed10(void);

/* The group stages are built at 0x57ff2e for index 0 to 7 (the loop at 0x57ff34 belongs to slice 28). With 10
 * groups the call for 7 also builds stage[8] (group J, its teams at +0xf7 from the draw 0x57dab0, which takes
 * +0xff + 2 groups). A call for 8 when stage[8] is there already does nothing, so a later loop to 8 builds it
 * once. The stage array (+0xc, 10 entries) is not cleared before the draw: in a 2018 start stage[8] held a
 * stale pointer, so group J was not built, +0x30 stayed 7 and the step to 8 called that pointer (0x6848f8). So
 * the call for 0 clears stage[8] and stage[9] (stage[9] for the step after the groups, 0x57ec60 below). */
RT_WRAP(0057ed10, euroq_group_stage)
{
    uint32_t self = cpu.ecx, index = rd32(g_mem, cpu.esp + 4u);
    if (format_of(self) != TEN_GROUPS) {
        o_0057ed10();
        return;
    }
    uint32_t stages = rd32(g_mem, self + 0xcu);
    int32_t count = (int32_t)rd32(g_mem, self + 0x2cu);
    if (index == 0u && readable(stages) && count >= 0 && count < 10) {
        /* A campaign after one that ended with no stage after the groups (+0x2c 9, euroq_advance below): +0x2c
         * stays 9, as start_new_season 0x57eb20 does not set it, and after a load League::load gave the array
         * only 9 slots, so stage[9] would be past its end. A new array of 10 slots and +0x2c 10 (load sweep). */
        uint32_t size[1] = {40}, fresh = call_cdecl(F_MALLOC, 1, size);
        if (rt_unwinding) return;
        if (readable(fresh)) {
            memset(g_mem + fresh, 0, 40);
            memcpy(g_mem + fresh, g_mem + stages, 4u * (uint32_t)count);
            wr32(g_mem, self + 0xcu, fresh);
            wr32(g_mem, self + 0x2cu, 10);
            uint32_t old[1] = {stages};
            call_cdecl(F_FREE, 1, old);
            if (rt_unwinding) return;
            stages = fresh;
        }
    }
    if (index == 0u && readable(stages)) {      /* the array is not cleared: stage[8] held a stale pointer */
        wr32(g_mem, stages + 32u, 0);
        wr32(g_mem, stages + 36u, 0);
    }
    if (index == 8u && readable(stages) && rd32(g_mem, stages + 32u)) {
        rt_ret_stdcall(1, 3);
        return;
    }
    uint32_t teams = rd32(g_mem, cpu.esp + 8u), six = rd32(g_mem, cpu.esp + 12u);
    o_0057ed10();
    if (rt_unwinding || index != 7u || !readable(stages) || rd32(g_mem, stages + 32u)) return;
    Cpu after = cpu;
    uint32_t a[3] = {8, teams, six};
    call_this(GROUP_STAGE, self, 3, a);
    if (rt_unwinding) return;
    cpu = after;
}

/* ---- The step after the ten groups ---- */

void o_0057ec60(void);

/* advance_stage 0x57ec60 (vtable 0x96a0d4 +0x28): +0x30 + 1, then create_playoff_stage 0x57ef50 when the new
 * index is +0x100 or +0x101 (0x57ecdb..0x57ecf5). The League update calls it when every stage 0 to +0x30 is
 * complete and +0x30 is below the stage count +0x2c minus 1 (0x6849aa..0x6849e0), else it ends the competition
 * (0x6849e8). It also calls each stage 0 to +0x30 every day (0x6848ee..0x684901). With 10 groups +0x30 is 8 after
 * the groups, the step makes it 9, and 0x57ef50 takes its no-ranking path (the news for the teams that are out)
 * and makes no stage[9], so the next update would call an empty stage at 0x6848f8. When stage[9] is still empty
 * after the step (the group builder above clears it), +0x30 goes back to 8 and +0x2c to 9: the next update ends
 * the qualifying. A stage that slice 28 builds there keeps the original's step.
 *
 * The ctor allocates the 10 slots of the stage array with malloc and does not clear them (0x57d00b..0x57d01e),
 * and League::load_from_temp_file fills only the slots 0 to +0x30 of that array (0x688e3a..0x689017). So after a
 * load slot 9 can hold old heap bytes: the test below took them for a stage, +0x30 stayed 9, and the next update
 * called through them (gate run of a 2025 start, 8 October 2027: "call to 0 ... from 0x6848fb"). Only the step
 * makes stage[9], so the slot is cleared before it. */
RT_WRAP(0057ec60, euroq_advance)
{
    uint32_t self = cpu.ecx;
    int ten = format_of(self) == TEN_GROUPS;
    int last = (int32_t)rd32(g_mem, self + 0x30u) == (int8_t)rd8(g_mem, self + 0xffu);
    uint32_t slots = rd32(g_mem, self + 0xcu);
    if (ten && last && readable(slots) && (int32_t)rd32(g_mem, self + 0x2cu) == 10) wr32(g_mem, slots + 4u * 9u, 0);
    o_0057ec60();
    if (rt_unwinding || !ten || !last) return;
    uint32_t stages = rd32(g_mem, self + 0xcu), next = rd32(g_mem, self + 0x30u);
    if (!readable(stages) || next != 9u || rd32(g_mem, stages + 4u * next)) return;
    wr32(g_mem, self + 0x30u, next - 1u);
    wr32(g_mem, self + 0x2cu, next);
    if (logging())
        fprintf(stderr, "recomp: season intl: euro qualifying %d: no stage after the groups, the qualifying ends\n",
                (int16_t)rd16(g_mem, self + 0x40u) + 2);
}
