/* Every African nation in CAF World Cup qualifying (item 199 of 0.6.2). Changing rules on only (season_intl_on)
 * and campaigns for the World Cup of 2006 or later: the base game, a game with changing rules off and the campaign
 * of 2002 that a 2001/02 game starts in do not change (Q152).
 *
 * Real life (en.wikipedia.org "<year> FIFA World Cup qualification (CAF)", read 9 November 2026): every CAF member
 * that enters plays, with a two-leg first round of the weakest. 2006: 51 entered, 42 in the first round (21 ties),
 * 9 byes, then 5 groups of 6. 2010: 10 in the first round, then 12 groups of 4 and 5 groups of 4. 2014: 52
 * entered, 24 in the first round, then 10 groups of 4 and 5 ties. 2018: 54 entered, 26 in the first round, 20
 * ties, then 5 groups of 4. 2022: 54 entered, 28 in the first round, then 10 groups of 4 and 5 ties. 2026: 54
 * entered (cafonline.com), 9 groups of 6. Somalia played the first round of 2006, 2010, 2014, 2018 and 2022. The
 * game keeps its own shape (a first round, then groups whose winners qualify), so only the entrants follow real
 * life: every African nation of the database, less the hosts.
 *
 * The original (wc_african_cup, class WcAfricanCup, vtable 0x970ad0). The pool builder collect_entrants 0x9106a0
 * counts the African hosts of find_hosts and the holder into the exempt count +0xba, sets the pool size +0xb6 = 50
 * less +0xba (0x9107e8), allocates 52 slots for the pool +0xb2 (malloc 0xd0 at 0x9107ed) and fills them with the
 * African nations' teams in nation-table order, hosts and holder left out, until 52 (cmp ebx, 0x34 at 0x910896).
 * It sorts the count it found by reputation (0x4b07d0 at 0x9108fc); only the first +0xb6 play. So the nations
 * after the 52nd in the table never play, and of the 52 the two weakest are out: Somalia and Equatorial Guinea in
 * 53 of 55 cycles of the pass 4 game a2001to2300. The first round build_first_round_entries 0x910120 takes the
 * weakest +0x3e (50 with no exempt nation, 0x910133) of the first +0xb6 into two-leg ties and clears their slots.
 * The group builder create_group_stages 0x9109e0 puts the winners in the cleared slots, sorts the +0xb6 slots and
 * deals them into G groups of S, group g taking slots g, g + G, ... (0x910bba): 5 groups of 5 with no exempt nation
 * (0x910a45), 4 groups of 6 with one (0x910a6f). The groups play 10 matchdays unless they hold 4 teams
 * (create_round_table 0x90f600). save_extra_data 0x911110 writes +0xb6 and that many slots, and load_extra_data
 * 0x910f00 reads +0xb6 and then allocates 52 slots (0x910fec) for them.
 *
 * The change. The pool builder reads every nation of the table (the cap at 0x910896 becomes the nation count, the
 * array that count), and after the formats of slice 27 (season_intl_qual.c, F1) and slice 29 (season_wcq26.c) at
 * 0x9108d8, with no exempt nation left, the pool size +0xb6 becomes the plan's entrants (season_caf_plan): the
 * count found, at most two per group place. The first round takes 2 x (entrants - places).
 * - A World Cup of 48 teams (season_wcq26): 9 groups of 5 (8 in 2030), the first round of season_wcq26.c, which
 *   reads +0xb6. With 54 nations: 18 in the first round, 36 byes.
 * - Else the original's 5 groups. A pool of 50 or fewer keeps groups of 5 (50 nations: the original's round of
 *   50). A larger pool takes groups of 6 (30 places, 10 matchdays, as the original's groups with one exempt
 *   nation): 53 nations give 46 in the first round and 7 byes, as the 42 and 9 of 2006. The group builder's size
 *   comes from +0xb6 (above 50 with no exempt nation), so a loaded save gives the same groups.
 * Load allocates the larger of 52 slots and +0xb6, so a save of such a campaign loads; the base game's +0xb6 is
 * at most 50, so its load does not change.
 *
 * set_club_progress 0x9115a0 writes each team's group positions (+0x73, +0x74 and +0x75 of its Comp_Stats entry,
 * from 0x4a4850) as a band of the whole field: with no exempt nation, group position p of stages 0 to 4 becomes
 * 5 x (p - 1) + 1 (the jump tables at 0x911a78, 0x911a8c and 0x911aa0: 1, 6, 11, 16, 21), and a position the
 * tables do not hold is written as it is (0x911600, 0x911631, 0x911a64). With one exempt nation (4 groups of 6)
 * position 6 has its band, 21 (0x911707). So in a group of 6 the last team got 6, a runner-up's band. In the old
 * shape with groups of 6, position 6 gets 5 x 5 + 1 = 26. */
#include "win.h"
#include "season.h"
#include "season_intl.h"
#include "season_wcq26.h"
#include "season_caf_pool.h"

#include <stdio.h>
#include <stdlib.h>

#define NATION_COUNT 0xae2350u
#define F_YEAR 0x40u
#define F_FIRST 0x3eu
#define F_POOL 0xb2u
#define F_POOL_SIZE 0xb6u
#define F_EXEMPT 0xbau
#define CAP 52                          /* the original's array (0xd0 bytes) and cap (0x34) */
#define GROUPS 5                        /* the original's group count with no exempt nation (0x910a45) */

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

static int wc_year(uint32_t self) { return (int16_t)rd16(g_mem, self + F_YEAR) + 2; }

static int caf_on(uint32_t self)
{
    return readable(self) && season_intl_on() && wc_year(self) >= 2006;
}

static int nations(void)
{
    int n = (int)rd32(g_mem, NATION_COUNT);
    return n > CAP ? n : CAP;
}

/* 0x910896 `cmp ebx, 0x34` (this in esi): the most nations the pool reads. */
uint32_t rt_caf_pool_cap(uint32_t self, uint32_t original)
{
    return caf_on(self) ? (uint32_t)nations() : original;
}

/* 0x9107ed `push 0xd0` (this in esi): the bytes of the pool array. */
uint32_t rt_caf_pool_bytes(uint32_t self, uint32_t original)
{
    uint32_t want = 4u * (uint32_t)nations();
    return caf_on(self) && want > original ? want : original;
}

/* 0x910fec `push 0xd0` in load_extra_data (this in ebx, +0xb6 read at 0x910f9e): the bytes of the pool array. */
uint32_t rt_caf_load_bytes(uint32_t self, uint32_t original)
{
    uint32_t n = rd32(g_mem, self + F_POOL_SIZE);
    return n > original / 4u && n < 4096u ? 4u * n : original;
}

/* 0x9108d8 (this in esi, the count found in ebx), after the formats of slices 27 and 29: the pool size. */
void rt_caf_pool(uint32_t self, uint32_t found)
{
    if (!caf_on(self) || rd8(g_mem, self + F_EXEMPT) != 0u) return;
    uint32_t old = rd32(g_mem, self + F_POOL_SIZE);
    int wcq26 = season_wcq26(self), groups = GROUPS;
    if (wcq26) {
        wr32(g_mem, self + F_POOL_SIZE, found);         /* season_wcq26.c reads its pool there */
        groups = (int)rt_wcq26_africa_groups(self, 0u);
    }
    SeasonCafPlan plan;
    if (!season_caf_plan((int)found, groups, wcq26, &plan)) {
        wr32(g_mem, self + F_POOL_SIZE, old);
        if (logging())
            fprintf(stderr, "recomp: season intl: wc qualifying %d: Africa pool of %u, the shape stays\n",
                    wc_year(self), found);
        return;
    }
    wr32(g_mem, self + F_POOL_SIZE, (uint32_t)plan.entrants);
    if (logging())
        fprintf(stderr, "recomp: season intl: wc qualifying %d: Africa takes %d of %u nations: %d in the first "
                        "round, %d groups of %d\n", wc_year(self), plan.entrants, found, plan.first, groups,
                plan.group_size);
}

/* The old shape's plan from the pool size, or 0 (a World Cup of 48 teams, an exempt nation, changing rules off). */
static int old_plan(uint32_t self, SeasonCafPlan *plan)
{
    if (!caf_on(self) || season_wcq26(self) || rd8(g_mem, self + F_EXEMPT) != 0u) return 0;
    return season_caf_plan((int)rd32(g_mem, self + F_POOL_SIZE), GROUPS, 0, plan);
}

/* 0x910133 (this in esi), 0x90f6f0 and 0x90f6f5 (the template, this in edi): the first-round team count, 50. */
uint32_t rt_caf_first(uint32_t self, uint32_t original)
{
    if (season_wcq26(self)) return rt_wcq26_africa_first(self, original);
    SeasonCafPlan plan;
    return old_plan(self, &plan) ? (uint32_t)plan.first : original;
}

/* 0x910a4e (this in esi, eax 5): the group size of create_group_stages with no exempt nation. */
uint32_t rt_caf_group_size(uint32_t self, uint32_t original)
{
    SeasonCafPlan plan;
    return old_plan(self, &plan) ? (uint32_t)plan.group_size : original;
}

/* 0x911600, 0x911631 and 0x911a64 (this in esi, the stage at [esp + 0x10]): a group position the original's
 * tables do not hold (only 1 to 5, for stages 0 to 4 with no exempt nation). In groups of 6, position 6 gets the
 * band 26. 0x911a64 is also the end of stages 5 and up, which keep the position. */
uint32_t rt_caf_band(uint32_t self, uint32_t stage, uint32_t position)
{
    SeasonCafPlan plan;
    if (stage < 5u && position == 6u && old_plan(self, &plan) && plan.group_size == 6) return 26u;
    return position;
}

/* The days of the last group rounds and of the play-off (0.6.2 item 211). The template 0x90f600 (this in edi)
 * gives the groups the 2002 cycle's days, in the year before the World Cup (year offset 1): with 10 rounds (exempt
 * 0, 1 or 3: African hosts and an African holder) rounds 8, 9 and 10 on 30 June (0x90f9e6, month 0x90f9e4),
 * 14 July (0x90fa18, 0x90fa16) and 17 July (0x90fa52, 0x90fa50); with 6 rounds (exempt 2) round 6 on 14 July (the
 * same pushes 0x90fa18 and 0x90fa16). Exempt 2 and 3 then play a two-leg play-off (stage 6 or 4) on 29 September
 * (0x90fb42, 0x90fb40) and 20 October (0x90fb59, 0x90fb57). The rules table's last Africa Cup of Nations (2027:
 * 19 June to 17 July) repeats every 2 years, so from the 2030 cycle the July days lie in the finals, finals_busy
 * (season_friendly_dates.c) holds them, and the stage ends there, so the stage plan cannot move them later. Real
 * life from the 2006 cycle: no group match of the deciding stage in July of that year; the last rounds in September
 * to November (en.wikipedia.org "<year> FIFA World Cup qualification (CAF)": 2006 group winners on 8 October 2005;
 * 2010 6 September, 10 October and 14 November 2009; 2018 28 August to 5 September, 2 to 10 October and 6 to
 * 14 November 2017; 2022 September to November 2021; 2026 matchdays 7 and 8 on 3 to 9 September, 9 and 10 on 8 to
 * 14 October 2025), and the play-off after them (2014 11 to 15 October and 16 to 19 November 2013; 2026 13 and
 * 16 November 2025). From the 2006 cycle: the push at 0x90f9e6 takes 5 September, 0x90fa18 8 October, 0x90fa52
 * 12 October, the play-off 12 and 16 November; the weekday codes and the year offsets stay. The World Cup's own
 * draw and the play-off tournament (season_wcq26.c, from 1 March of the World Cup year) come after them.
 * site: 7, 8 or 9 for the group pushes (the round of 10), 10 and 11 for the play-off legs; which: 0 the day,
 * 1 the month (0 January). */
uint32_t rt_caf_round_date(uint32_t self, int site, int which, uint32_t original)
{
    static const uint8_t day[5] = {5, 8, 12, 12, 16}, month0[5] = {8, 9, 9, 10, 10};
    if (site < 7 || site > 11 || !caf_on(self)) return original;
    return which ? month0[site - 7] : day[site - 7];
}
