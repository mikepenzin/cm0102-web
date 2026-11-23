/* The youth pool of every nation (0.6.2 slice 20, open item 52). This fixes a fault of the original that every
 * game reaches, the base 2001 game included.
 *
 * A player who retires gives his player data and his nation to the pool of PLAYER_REGEN. process_retirements
 * 0x7a59e0 does this with add_player_data_to_pool 0x7a8350 at 0x7a6463, or with queue_staff_for_regen 0x7a87d0.
 * The second also queues the staff slot. process_regen_queue 0x7a65d0 makes one new player for each due slot with
 * regenerate_as_youth_player 0x7abac0. That function takes the nation of the first pool entry (this +0x3c, entry
 * +4, read at 0x7abb13). So each nation gets back about as many new players as it loses.
 *
 * The age of the new player comes from his nation (0x7abe87..0x7abeee). Some nations are not loaded (league flags
 * +0x11c are 0) and had 349 players or fewer at the start (+0x88 below 0x15e). For them the age is 23 to 27 when
 * rand(+0x88) < rand(1000), else 17 to 23. Nation +0x88 is the number of staff with player data of the nation.
 * database_trim_staff_tables 0x524d70 counts it once, and no code changes it later. Any other nation gets 15 to
 * 20 in nearly all cases (0x7ac3dd). The birth year at staff +0x12 is the game year less the age.
 *
 * So nearly every new player of a small nation that is not loaded is 23 or older. The young players of the data
 * grow up and none follow. In a 2001 game with England only, Croatia, Nigeria and Norway have no player under 21
 * by July 2004. By 2007, 13 nations with 23 players or more have none (0.6.0 long game, seed 5). The forum save of
 * January 2041 (Greece only) has 24 such nations, Belgium among them. Their under-21 and Olympic squads are empty.
 *
 * The fix has two parts. Both keep the code of the original while every nation has its youth pool.
 *
 * 1. Young ages. The pool nation of a new player can have the older ages, 23 players or more, and fewer than 1 in
 *    20 of them under 21 or fewer than 1 in 10 under 23 (youth_pool_short). Part 2 below says when a nation runs
 *    out. For a short nation or one that runs out, the wrap of 0x7abac0 sets the nation's +0x88 to 0x15e for the
 *    call and puts the old value back after it. Only this function reads +0x88
 *    (0x7abcd7, 0x7abd15, 0x7abea0), and no save runs in the call. With 0x15e the nation keeps the new player,
 *    because the change of nation at 0x7abcd7 needs fewer than 100. He gets the young ages of 0x7ac3dd.
 * 2. A new player for a nation that runs out. A nation gets new players only when its own players retire. A young
 *    new player plays longer, so fewer of them come. A nation of 23 to 30 players can still reach none under 21
 *    for a while. In a model of 40 such nations over 300 years, part 1 alone leaves 2 % of the decade checks
 *    with an empty nation. So the wrap looks for a nation with a continent and 23 players or more that will have
 *    none under 21 or none under 23 in a year. That is none under 20 or none under 22 today (youth_pool_runs_out).
 *    When the pool nation is not short and does not run out, the new player goes to that nation with the young
 *    ages. A loaded nation in need keeps its new players, which have the young ages already. The wrap writes
 *    that nation into the pool entry (+4) for the call, and 0 as the second nation (+8). A second nation of 0 is
 *    none (0x7abd22). The original removes the entry in the call (0x7abb1a). When it returns before that
 *    (0x7abae6, 0x7abaf1), the wrap puts the entry back. With both parts the model has no empty nation in any
 *    decade. A nation of under 30 players grows by at most 35 in 300 years (tests/seasons/test_youth_pool.c).
 *
 * A game changes from the first new player of a nation that is short or runs out. At the start of a 2001 game no
 * nation is either. The start, prematch and all-leagues fixtures have no nation of 23 players or more that is
 * short or has none under 20 or none under 22. The lowest shares are 6 % under 21 and 13 % under 23. In an
 * England-only 2001 game (start.sav) the first change is on 24 March 2002, a new player for Croatia. A 2025 start
 * has nations that are short or run out at once (the gate save of July 2027: Algeria and Guinea-Bissau).
 *
 * The counts come from one pass over the staff table at the first new player of each call of 0x7a65d0. Then each
 * new player changes the counts of his slot. A queued slot keeps its old player data until the new player takes
 * it. The wrap takes the old player out of the counts before it decides. */
#include "win.h"
#include "youth_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STAFF_TABLE 0xae23bcu       /* the staff table, 0x6e bytes a staff */
#define STAFF_COUNT 0xae2364u
#define STAFF_SIZE 0x6eu
#define NATION_TABLE 0xae23a8u      /* the nation table, 0x122 bytes a nation (the nation id is its row) */
#define NATION_COUNT 0xae2350u
#define NATION_SIZE 0x122u
#define GAME_DATE 0xae2c90u         /* s16 day of the year from 0, then the u16 year */
#define MAX_NATIONS 1024

static int counted;                 /* the counts hold for this call of 0x7a65d0 */
static int players[MAX_NATIONS], under_20[MAX_NATIONS], under_21[MAX_NATIONS], under_22[MAX_NATIONS],
    under_23[MAX_NATIONS];

void o_007a65d0(void);
void o_007abac0(void);

typedef struct {
    uint32_t nations;               /* the nation table */
    int count;                      /* nations, at most MAX_NATIONS */
    int day, year;                  /* the game date */
} Now;

static Now now(void)
{
    Now t = {rd32(g_mem, NATION_TABLE), (int)rd32(g_mem, NATION_COUNT), (int16_t)rd16(g_mem, GAME_DATE),
             (int)rd16(g_mem, GAME_DATE + 2)};
    if (t.count > MAX_NATIONS) t.count = MAX_NATIONS;
    return t;
}

/* One staff: its nation row and its age, or -1 for a staff with no player data or no nation. */
static int staff_nation(const Now *t, uint32_t s, int *age)
{
    uint32_t n = rd32(g_mem, s + 0x1a);
    if (!rd32(g_mem, s + 0x61) || !n || n < t->nations || (n - t->nations) % NATION_SIZE) return -1;
    int row = (int)((n - t->nations) / NATION_SIZE);
    if (row >= t->count) return -1;
    *age = youth_pool_age(t->year, t->day, (int16_t)rd16(g_mem, s + 0x12), (int16_t)rd16(g_mem, s + 0x10));
    return row;
}

static void add(const Now *t, uint32_t s, int step)
{
    int age, row = staff_nation(t, s, &age);
    if (row < 0) return;
    players[row] += step;
    under_20[row] += step * (age < 20);
    under_21[row] += step * (age < 21);
    under_22[row] += step * (age < 22);
    under_23[row] += step * (age < 23);
}

static void count_all(const Now *t)
{
    memset(players, 0, sizeof players);
    memset(under_20, 0, sizeof under_20);
    memset(under_21, 0, sizeof under_21);
    memset(under_22, 0, sizeof under_22);
    memset(under_23, 0, sizeof under_23);
    uint32_t staff = rd32(g_mem, STAFF_TABLE);
    int n = (int)rd32(g_mem, STAFF_COUNT);
    for (int i = 0; staff && i < n; i++) add(t, staff + (uint32_t)i * STAFF_SIZE, 1);
    counted = 1;
}

/* The nation with a continent that runs out first: the most players, then the lowest row. -1 for none. */
static int runs_out(const Now *t, int except)
{
    int best = -1;
    for (int n = 0; n < t->count; n++) {
        if (n == except || !rd32(g_mem, t->nations + (uint32_t)n * NATION_SIZE + 0x71)) continue;
        if (youth_pool_runs_out(players[n], under_20[n], under_22[n]) && (best < 0 || players[n] > players[best]))
            best = n;
    }
    return best;
}

static void log_act(const Now *t, const char *what, int nation)
{
    if (!getenv("RECOMP_YOUTH_LOG")) return;
    char name[52];
    memcpy(name, g_mem + t->nations + (uint32_t)nation * NATION_SIZE + 4, 51);
    name[51] = 0;
    fprintf(stderr, "recomp: youth pool: %d/%d %s %s: %d players, %d under 21, %d under 23, %d under 20, "
            "%d under 22\n", t->day, t->year, what, name,
            players[nation], under_21[nation], under_23[nation], under_20[nation], under_22[nation]);
}

/* RECOMP_YOUTH_LOG: a new player who gets a club of another nation (staff +0x39 club, club +0x53 nation), as the
 * Welsh and Irish new players of Guiseley (youth_club_lists.c, open item 262). Only the log changes. */
static void log_club(const Now *t, uint32_t slot)
{
    if (rt_unwinding || !getenv("RECOMP_YOUTH_LOG")) return;
    uint32_t club = rd32(g_mem, slot + 0x39), nation = rd32(g_mem, slot + 0x1a);
    if (!club || !nation) return;
    uint32_t home = rd32(g_mem, club + 0x53);
    if (!home || home == nation) return;
    fprintf(stderr, "recomp: youth pool: %d/%d new player of %.30s joins %.40s of %.30s\n", t->day, t->year,
            win_str(nation + 4), win_str(club + 4), win_str(home + 4));
}

/* PLAYER_REGEN::process_regen_queue 0x7a65d0, thiscall, no arguments. */
RT_WRAP(007a65d0, youth_pool_regen_queue)
{
    counted = 0;
    o_007a65d0();
    counted = 0;
}

/* PLAYER_REGEN::regenerate_as_youth_player 0x7abac0, thiscall (ret 4), the staff slot on the stack. */
RT_WRAP(007abac0, youth_pool_new_player)
{
    Now t = now();
    uint32_t self = cpu.ecx, slot = rt_arg(0);
    uint32_t pool = rd32(g_mem, self + 0x3c), pool_count = rd32(g_mem, self + 0x40);
    int nation = pool && pool_count && t.nations ? (int)rd32(g_mem, pool + 4) : -1;
    if (nation < 0 || nation >= t.count) {
        o_007abac0();
        return;
    }
    if (!counted) count_all(&t);
    add(&t, slot, -1);                              /* the old player of the slot leaves the counts */
    uint32_t rec = t.nations + (uint32_t)nation * NATION_SIZE;
    int needs = youth_pool_short(players[nation], under_21[nation], under_23[nation]) ||
                youth_pool_runs_out(players[nation], under_20[nation], under_22[nation]);
    int target = nation, young = needs && youth_pool_older_rule(g_mem[rec + 0x11c], (int)rd32(g_mem, rec + 0x88));
    if (young) log_act(&t, "young", nation);
    else if (!needs && (target = runs_out(&t, nation)) >= 0) {   /* a nation in need keeps its new player */
        young = 1;
        log_act(&t, "new player for", target);
    } else target = nation;
    uint32_t id = rd32(g_mem, pool), old_nation = rd32(g_mem, pool + 4), old_second = rd32(g_mem, pool + 8);
    uint32_t trec = t.nations + (uint32_t)target * NATION_SIZE, start_players = rd32(g_mem, trec + 0x88);
    if (target != nation) {
        wr32(g_mem, pool + 4, (uint32_t)target);
        wr32(g_mem, pool + 8, 0);
    }
    if (young && (int)start_players <= YOUTH_POOL_OLDER_MAX) wr32(g_mem, trec + 0x88, YOUTH_POOL_OLDER_MAX + 1);
    o_007abac0();
    log_club(&t, slot);
    /* No return on rt_unwinding here: +0x88 and the pool entry must go back in every case. */
    add(&t, slot, 1);
    wr32(g_mem, trec + 0x88, start_players);
    if (target != nation && rd32(g_mem, self + 0x3c) == pool && rd32(g_mem, self + 0x40) == pool_count &&
        rd32(g_mem, pool) == id) {                  /* it returned before it took the entry (0x7abae6, 0x7abaf1) */
        wr32(g_mem, pool + 4, old_nation);
        wr32(g_mem, pool + 8, old_second);
    }
}
