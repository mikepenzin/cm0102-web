/* The periods of the Eerste Divisie and the Belgian Second Division at their real sizes (0.7.2 league sizes, slice 10,
 * D6; spec docs/specs/2026-11-13-league-sizes-0.7.2-spec.md). The math is season_periods.c.
 *
 * The classes (read in the original, 3.9.68):
 * - HolFirst::add_result 0x5f65b0 (thiscall: fixture, result) adds the result to the table (0x685d30 at 0x5f65bf),
 *   then gives the fixture a period by its round (+0x34): below 8 (`cmp ax, 8` at 0x5f65ce) period 0, below 16
 *   (0x5f65da) 1, below 26 (0x5f65e6) 2, else 3. It makes the period stages up to it (create_period_stage 0x5f6ad0 at
 *   0x5f6604, while +0x30 is below the period) and adds the result to the stage of the period ([+0xc][period],
 *   vtable +4 at 0x5f662b). create_period_stage gives the stage (League_Stage 0x68b100) its matches per club, +0xa7:
 *   10 for period 2 (`mov byte ptr [esp + 0x13], 0xa` at 0x5f6bd6) and 8 for the others (0x5f6be2), the period in
 *   bl (0x5f6bae), the object in esi (0x5f6aed).
 * - BelSecond::add_result 0x420110 is the same with three periods: below 10 (`cmp ax, 0xa` at 0x42012e) period 0,
 *   below 22 (0x42013a) 1, else 2. create_period_stage 0x4204d0 gives 10 for period 0 (`mov bl, 0xa` at 0x4205b9)
 *   and 12 for periods 1 and 2 (`mov bl, 0xc` at 0x4205cb), the period in al (0x4205a9), the object in esi (0x4204ec).
 * - So each class has fixed bounds of its 34 rounds. A grown league (Eerste Divisie 20: 38 rounds) would put rounds
 *   26 to 37 in period 3, whose stage wants 8 matches, and a club's results past +0xa7 write past the row's position
 *   array (init_table_row 0x687570 allocates +0xa7 bytes). Here (PATCHES of tools/recomp/league_sizes.py) the bounds
 *   and the lengths scale to the rounds of the season, R = meetings (+0x3c) x (count + odd - 1), as D6 says. At the
 *   class's own rounds (the base game, a league at its class count, a 0.7.1 save before its next season change, a
 *   template with no room, D4) they are the original's.
 * - The stages are complete when every row reached +0xa7 (League::check_complete 0x689ad0, 0x689b47..0x689b58), and
 *   League::update_for_date 0x6847c0 ends the season only when the table and every stage up to +0x30 are complete
 *   (0x6849aa..0x6849fa). With an odd count a club rests one round in each cycle, so some clubs play one match fewer
 *   in a period and the stage would never complete. For a period stage of an odd count the stage is complete when
 *   its matches are played (season_period_stage_done: the played of its rows add up to twice the matches), through
 *   rt_league_played_target (season_split.c, INSNS 0x689b47).
 * - Period winners: HolFirst::update_positions_and_status 0x5f6d20 and BelSecond's 0x420680 (vtable +0x94) call
 *   0x684cd0, then take the first period whose flag is 0 (HolFirst +0xee..+0xf1, 0x5f6d2e..0x5f6d66; BelSecond
 *   +0xf6..+0xf8, 0x42068b..0x4206b5), and once its stage exists give its winner status 1 in the table (0x5f6e09,
 *   0x420714) and set the flag (0x5f6e8e.., 0x42075e..). set_up_rules clears the flags each season (0x5f639b..
 *   0x5f63ad, 0x41ff0b..0x41ff17) and the save keeps them (0x5f7550, 0x420e30). The play-off takes the rows of
 *   status 1 and tops up with the rows of 0xff in table order (HolFirst 0x5f6720 to six at 0x5f677e, BelSecond
 *   0x420230 to four at 0x42028e). D6: periods on or off by the row. A row with no periods (0: Eerste Divisie
 *   2001/02 and 2002/03, Second Division 2007/08, 2008/09, 2015/16 and 2020/21 on) gets every flag set before the
 *   call, so no club wins a period and the play-off takes the places after the champion, as in real life (hol_first
 *   2001/02 places 2 to 7, bel_second 2007/08 places 2 to 5). A row with periods (or unknown periods) plays the
 *   class's periods, scaled. The stages are still made, so the season's stage count and the save stay the class's.
 *
 * - A split (slice 7, D7: the Second Division 2022/23, 12 clubs, 22 rounds then 6 and 6). The section fixtures stay
 *   with the parent (season_split.c) and their round (+0x34) is the section's own date index from 0 (League::
 *   create_fixtures 0x6838fe), so add_result would put them in periods 0 and 1, whose stages hold the matches of
 *   those periods only: a club's results past +0xa7 write past its row's position array. After the split (the
 *   marker in +0x3c, season_split_meetings) the wraps of add_result below give a result to the table only, through
 *   League::add_result 0x685d30, as the class does first (0x5f65bf, 0x42011f). The periods are those of the rounds
 *   before the split, so the rounds use the row's meetings, not the marker.
 *
 * The base game and RECOMP_LEAGUE_SIZES=off: every function returns the original (season_sizes_on). */
#include "win.h"
#include "../rt_host.h"
#include "season_change.h"
#include "season_league.h"
#include "season_ned_playoff.h"
#include "season_periods.h"
#include "season_sizes.h"
#include "season_split.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEAGUES 0xadadfcu           /* the competition objects by id */
#define COMP_RECORD 4u
#define COMP_MEETINGS 0x3cu         /* s16 */
#define COMP_CLUBS 0x3eu            /* s16 */
#define COMP_YEAR 0x40u             /* s16: the edition */
#define COMP_NAME_CODE 0x3au        /* s16: a stage's name (League_Stage 0x68b211) */
#define COMP_MATCHES 0xa7u          /* s16: matches per club */
#define COMP_TABLE 0xb1u
#define ROW_SIZE 0x3bu
#define ROW_PLAYED 0x11u            /* s8 */
#define STAGE_VTABLE 0x0096d0f0u    /* League_Stage (0x68b196) */
#define PERIOD_NAME_FIRST 0x439     /* "Period 1".."Period 4" (0x5f6bb9..0x5f6bdd, 0x4205b4..0x4205c6) */
#define PERIOD_NAME_LAST 0x43c
/* Gap 33: the names of periods 5 and 6, past the original's stage ids (0x41f..0x483, jump table 0x4b9a44 at 0x4b6012).
 * The name function 0x4b4850 gives an id it does not know the caller's empty text (0x4b94bd), and the parent of a
 * stage id 0x4b01d0 gives -1 as for 0x439..0x43c, so only the names differ (season_round_names_game.c). */
#define PERIOD_NAME_FIFTH 0x7f05
#define PERIOD_NAME_SIXTH 0x7f06
#define CLASS_ROUNDS 34             /* both classes: 18 clubs, 2 meetings (D 0x22 at 0x5f57ca, 0x41f37e) */

typedef struct {
    const char *file;
    uint32_t key;                   /* the id of the division (HolCup 0x5f4820, BelFaCup 0x41d8d0) */
    int starts[3];                  /* the rounds where periods 1.. begin */
    int parts;
    uint32_t flags;                 /* the "winner given" flag of each period */
    int most;                       /* the most periods of a row (gap 33: HolFirst six, stages 0..5, play-off 6, 7) */
} PeriodClass;

static const PeriodClass CLASSES[] = {
    {"hol_first", 0x9cf56cu, {8, 16, 26}, 4, 0xeeu, 6},
    {"bel_second", 0x9cf5a0u, {10, 22, 0}, 3, 0xf6u, 3},
};
#define NCLASSES ((int)(sizeof CLASSES / sizeof CLASSES[0]))

static int readable(uint32_t p) { return rt_readable(p); }

static int logging(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_SIZE_LOG") && *getenv("RECOMP_SIZE_LOG");
    return on;
}

static uint32_t league_of(uint32_t key)
{
    uint32_t leagues = rd32(g_mem, LEAGUES);
    int32_t id = (int32_t)rd32(g_mem, key);
    if (!readable(leagues) || id < 0 || id > 0xffff) return 0;
    uint32_t league = rd32(g_mem, leagues + 4u * (uint32_t)id);
    return readable(league) ? league : 0;
}

/* The class of a HolFirst or BelSecond object, or NULL. */
static const PeriodClass *class_of(uint32_t obj)
{
    if (!readable(obj)) return NULL;
    for (int i = 0; i < NCLASSES; i++)
        if (league_of(CLASSES[i].key) == obj) return &CLASSES[i];
    return NULL;
}

/* The rounds of the season of the object: meetings x (count rounded up to even - 1). */
static int rounds_of(uint32_t obj)
{
    int m = season_split_meetings(obj), n = (int16_t)rd16(g_mem, obj + COMP_CLUBS);
    if (m < 1 || m > 4 || n < 2 || n > 60) return CLASS_ROUNDS;
    return m * (n + (n & 1) - 1);
}

/* Gap 33: the row's own count of periods when it differs from the class's, else 0. Fewer: First Division B 2016/17 to
 * 2019/20, two periods of 14 rounds (Wikipedia 2016-17_Belgian_First_Division_B to 2019-20; checked 15 November
 * 2026); the stages of the later periods are never made (their bound is past the last round) and their flags are set,
 * so no club wins them. More: the Eerste Divisie 2005/06 to 2008/09, six periods of six rounds, rounds 37 and 38 in
 * none (RSSSF tablesn/ned06.html to ned09.html; checked 15 November 2026); see the HolFirst functions below. The class
 * plays p periods of rounds / p each (season_period_own_length). */
static int own_periods(uint32_t obj, const PeriodClass *c)
{
    int season = season_sizes_row_season(obj, c->file);
    const SeasonLeagueRule *r = season < 0 ? NULL : season_league_row(c->file, season);
    int p = r ? r->periods : 0;
    return p >= 2 && p != c->parts && p <= c->most && season_period_own_length(rounds_of(obj), p) ? p : 0;
}

/* One log line a season of each class: the periods of the season (tests/seasons/league_size_run.py reads it). */
static void log_periods(uint32_t obj, const PeriodClass *c, int rounds)
{
    static int logged[NCLASSES];
    int year = (int16_t)rd16(g_mem, obj + COMP_YEAR), i = (int)(c - CLASSES);
    if (!logging() || logged[i] == year) return;
    logged[i] = year;
    int season = season_sizes_row_season(obj, c->file);
    const SeasonLeagueRule *r = season < 0 ? NULL : season_league_row(c->file, season);
    fprintf(stderr, "recomp: league sizes: %s periods %d: rounds %d, lengths", c->file, year, rounds);
    int own = own_periods(obj, c);
    for (int p = 0; p < (own ? own : c->parts); p++)
        fprintf(stderr, "%s%d", p ? "+" : " ", own ? season_period_own_length(rounds, own)
                                                  : season_period_length(c->starts, c->parts, CLASS_ROUNDS, rounds, p));
    fprintf(stderr, ", winners %s\n", r && r->periods == 0 ? "off" : "on");
}

uint32_t rt_league_period_bound(uint32_t obj, uint32_t original)
{
    if (!season_sizes_on()) return original;
    const PeriodClass *c = class_of(obj);
    if (!c) return original;
    int rounds = rounds_of(obj);
    for (int k = 0; k < c->parts - 1; k++)
        if ((uint32_t)c->starts[k] == original) {
            log_periods(obj, c, rounds);
            int own = own_periods(obj, c);
            if (own) return (uint32_t)(k < own - 1 ? (k + 1) * season_period_own_length(rounds, own) : rounds);
            return (uint32_t)season_period_bound(c->starts, c->parts, CLASS_ROUNDS, rounds, k);
        }
    return original;
}

uint32_t rt_league_period_length(uint32_t obj, uint32_t period, uint32_t original)
{
    if (!season_sizes_on()) return original;
    const PeriodClass *c = class_of(obj);
    if (!c || (int)period < 0 || (int)period >= c->most) return original;
    int own = own_periods(obj, c);
    if (own) return (int)period < own ? (uint32_t)season_period_own_length(rounds_of(obj), own) : original;
    if ((int)period >= c->parts) return original;
    int len = season_period_length(c->starts, c->parts, CLASS_ROUNDS, rounds_of(obj), (int)period);
    return len >= 1 && len <= 127 ? (uint32_t)len : original;
}

uint32_t season_periods_played_target(uint32_t obj, uint32_t original)
{
    if (!season_sizes_on() || !readable(obj) || rd32(g_mem, obj) != STAGE_VTABLE) return original;
    int name = (int16_t)rd16(g_mem, obj + COMP_NAME_CODE), n = (int16_t)rd16(g_mem, obj + COMP_CLUBS);
    int period_name = (name >= PERIOD_NAME_FIRST && name <= PERIOD_NAME_LAST) || name == PERIOD_NAME_FIFTH ||
                      name == PERIOD_NAME_SIXTH;
    if (!period_name || n < 3 || n > 60 || !(n & 1)) return original;
    const PeriodClass *c = NULL;
    for (int i = 0; i < NCLASSES && !c; i++) {
        uint32_t parent = league_of(CLASSES[i].key);
        if (parent && rd32(g_mem, parent + COMP_RECORD) == rd32(g_mem, obj + COMP_RECORD) &&
            rd16(g_mem, parent + COMP_YEAR) == rd16(g_mem, obj + COMP_YEAR))
            c = &CLASSES[i];
    }
    uint32_t t = rd32(g_mem, obj + COMP_TABLE);
    if (!c || !readable(t)) return original;
    int sum = 0;
    for (int i = 0; i < n; i++) sum += (int8_t)g_mem[t + (uint32_t)i * ROW_SIZE + ROW_PLAYED];
    return season_period_stage_done(n, (int16_t)rd16(g_mem, obj + COMP_MATCHES), sum) ? 0 : original;
}

void season_periods_winners(uint32_t obj)
{
    if (!season_sizes_on()) return;
    const PeriodClass *c = class_of(obj);
    if (!c) return;
    int season = season_sizes_row_season(obj, c->file);
    const SeasonLeagueRule *r = season < 0 ? NULL : season_league_row(c->file, season);
    if (!r) return;
    int own = own_periods(obj, c);
    for (int p = r->periods == 0 ? 0 : own ? own : c->parts; p < c->parts; p++) g_mem[obj + c->flags + (uint32_t)p] = 1;
}

/* BelSecond::advance_stage 0x4201a0 makes the play-off group at stage 3 (`cmp eax, 3` at 0x42021c, after its three
 * period stages); create_playoff_group 0x420230 tops the rows of status 1 up to four (`cmp ebp, 4` at 0x42028e), lists
 * four (`cmp ebx, 4` at 0x42036b) and makes a League_Stage of four (`push 4` at 0x420412) that meets twice. Gap 33:
 * with the row's two periods the play-off comes after stage 2 and holds the two period winners, a final over two
 * matches. Every real season of the form had that final between two clubs (Wikipedia, "2016-17 Belgian First
 * Division B" to "2019-20 ...": Antwerp v Roeselare 2017, Beerschot Wilrijk v Cercle Brugge 2018, Beerschot Wilrijk
 * v Mechelen 2019, Beerschot v OH Leuven 2020), so the final plays in each. Known limit (0.7.3): the class gives a
 * later period to the best club with no period title (0xfe in the later tables), so in a game a club never wins both;
 * the real rule sent such a club up with no final. */
uint32_t rt_bel_playoff_stage(uint32_t obj, uint32_t original)
{
    if (!season_sizes_on()) return original;
    const PeriodClass *c = class_of(obj);
    int own = c ? own_periods(obj, c) : 0;
    return own ? (uint32_t)own : original;
}

uint32_t rt_bel_playoff_clubs(uint32_t obj, uint32_t original)
{
    if (!season_sizes_on()) return original;
    const PeriodClass *c = class_of(obj);
    int own = c ? own_periods(obj, c) : 0;
    if (!own) return original;
    static uint32_t logged_obj;
    static int logged_year;
    int year = (int16_t)rd16(g_mem, obj + COMP_YEAR);
    if (logging() && (logged_obj != obj || logged_year != year)) {
        logged_obj = obj;
        logged_year = year;
        fprintf(stderr, "recomp: league sizes: %s play-off %d: %d clubs (the period winners, two matches each)\n",
                c->file, year, own);
    }
    return (uint32_t)own;
}

/* Gap 33, the Eerste Divisie with six periods (own_periods above 4). The class holds four periods (stages 0 to 3)
 * and its play-off groups at stages 4 and 5:
 * - The stage array has room for six (`push 0x18` at 0x5f54f5, `mov [esi + 0x2c], 6` at 0x5f54fd, HolFirst's
 *   constructor); with the size step on it has room for eight (rt_hol_stage_room). +0x2c stays 6: the season ends
 *   at the last stage that rt_league_last_stage gives (League::update_for_date 0x6849d7, advance_stage 0x5f665f).
 * - advance_stage 0x5f6650 makes the play-off when +0x30 reaches 4 (`cmp eax, 4` at 0x5f6709); create_playoff
 *   0x5f6720 gets the dates of stages 4 and 5 (vtable +0x3c, 0x5f68ec and 0x5f6992), makes the groups with those
 *   numbers (0x5f692c, 0x5f69d0), stores them at +0x10 and +0x14 of the array (0x5f697a, 0x5f6a21), checks them
 *   (0x5f6a35, 0x5f6a3a) and sets +0x30 to 5 (0x5f6a3f). create_schedule 0x5f5790 makes dates only for stages 4 and 5
 *   (0x5f604b, 0x5f604f) and names them by `cmp al, 4` (0x5f607a). The news of a club's status (0x5f6ee0) and of a
 *   match (0x5f7110) tell the play-off stages by 4 and 5 (0x5f6f9c, 0x5f6fd1, 0x5f7117, 0x5f711d). With six periods
 *   each of these is the play-off's first stage, 6 (rt_hol_playoff_stage), or the one after it.
 * - add_result 0x5f65b0 knows four periods: the wrap below gives a result the period of its round, rounds 37 and 38
 *   none, and makes the period stages up to it with create_period_stage 0x5f6ad0 (as 0x5f65f3..0x5f6631).
 * - update_positions_and_status 0x5f6d20 has four flags (+0xee..+0xf1) that the save keeps (0x5f7550, 0x5f75b0).
 *   With six periods +0xf1 counts the periods given after the third (1, 2 or 3). For the fifth and sixth period the
 *   wrap shows the function a window: the stage array from the period's stage on as its stage 3, +0x30 less the
 *   shift, the fourth flag clear. The function then gives the winner of that stage (the period number of the news,
 *   `push edx` at 0x5f6e05, gets the shift back: rt_hol_period_news). The game has no name for a fifth period (0x43c at
 *   0x5f6bdd for each period after the third): the fifth and sixth stages get the ids PERIOD_NAME_FIFTH and
 *   PERIOD_NAME_SIXTH when they are made, and season_sizes_period_name names them. */
#define F_CREATE_PERIOD_STAGE 0x005f6ad0u  /* HolFirst::create_period_stage, thiscall (period, a byte), ret 4 */
#define COMP_STAGES 0xcu
#define COMP_STAGE_NOW 0x30u

static int hol_six(uint32_t obj, const PeriodClass **pc)
{
    const PeriodClass *c = season_sizes_on() ? class_of(obj) : NULL;
    if (pc) *pc = c;
    return c && c == &CLASSES[0] && own_periods(obj, c) > c->parts ? own_periods(obj, c) : 0;
}

uint32_t rt_hol_stage_room(uint32_t original)
{
    return season_sizes_on() && original == 0x18u ? 0x20u : original;
}

/* League::load 0x688e4b makes the stage array of +0x2c slots when the object has none (a HolFirst made for a load
 * skips its own at 0x5f54ec). A HolFirst gets room for eight there too: a save made during the six-period play-off
 * holds stages 6 and 7 (0x688f80..0x688fff), and a loaded 2005/06 game writes them (0x5f697a, 0x5f6a21). */
#define HOL_FIRST_VTABLE 0x0096b86cu   /* 0x5f54d7 */
uint32_t rt_league_stage_bytes(uint32_t obj, uint32_t original)
{
    return readable(obj) && rd32(g_mem, obj) == HOL_FIRST_VTABLE ? rt_hol_stage_room(original) : original;
}

/* HolFirst 0x5f73d0 and BelSecond 0x420d40 (vtable +0x68, a match of a stage: 0x48e380 passes the stage's +0x44) tell
 * the play-off stages by number (`cmp cl, 4` at 0x5f7480, `cmp cl, 5` at 0x5f7485, `cmp cl, 3` at 0x420de4) after esi
 * holds the table, so the wraps keep the object for the patches (rt_period_handler). */
static uint32_t handler_obj;

uint32_t rt_period_handler(void) { return handler_obj; }

static void with_handler(void (*original)(void))
{
    uint32_t keep = handler_obj;
    handler_obj = cpu.ecx;
    original();
    handler_obj = keep;
}

void o_005f73d0(void);
RT_WRAP(005f73d0, hol_first_stage_match) { with_handler(o_005f73d0); }

void o_00420d40(void);
RT_WRAP(00420d40, bel_second_stage_match) { with_handler(o_00420d40); }

/* The last stage of a season: League::update_for_date 0x6849aa ends it when +0x30 reaches +0x2c - 1 (0x6849d1..
 * 0x6849da) and else calls advance_stage, which makes the same test (HolFirst 0x5f6659, BelSecond 0x4201a0), as does
 * League::check_complete 0x689ad0 (0x689bdf). The capacity is the class's periods and its play-off stages (HolFirst 6:
 * 4 + 2 groups, BelSecond 4 at 0x41f0a5: 3 + 1 group). With the row's own periods the play-off stages follow them. */
uint32_t rt_league_last_stage(uint32_t obj, uint32_t original)
{
    if (!season_sizes_on()) return original;
    const PeriodClass *c = class_of(obj);
    int own = c ? own_periods(obj, c) : 0;
    uint32_t last = own ? (uint32_t)(own + ((int)original - c->parts)) : original;
    return c == &CLASSES[0] ? season_ned_playoff_last_stage(obj, last) : last;   /* issue 150 */
}

uint32_t rt_hol_playoff_stage(uint32_t obj, uint32_t original)
{
    int six = hol_six(obj, NULL);
    return six ? (uint32_t)six + (original - 4u) : original;
}

static int hol_news_shift;

uint32_t rt_hol_period_news(void) { return (uint32_t)hol_news_shift; }

/* HolFirst::update_positions_and_status 0x5f6d20, thiscall, no arguments (BelSecond's 0x420680 is wrapped in
 * season_bel_second.c, which calls season_periods_winners). */
void o_005f6d20(void);
RT_WRAP(005f6d20, hol_first_status)
{
    uint32_t self = cpu.ecx;
    season_periods_winners(self);
    int six = hol_six(self, NULL);
    uint8_t *f = &g_mem[self + 0xeeu];
    int given = (f[0] != 0) + (f[1] != 0) + (f[2] != 0) + f[3];
    if (!six || given < 4 || given >= six || !f[0] || !f[1] || !f[2]) {
        o_005f6d20();
        return;
    }
    int shift = given - 3;
    static uint32_t logged_obj;
    static int logged_given;
    if (logging() && (logged_obj != self || logged_given != given)) {
        logged_obj = self;
        logged_given = given;
        fprintf(stderr, "recomp: league sizes: hol_first periods %d: period %d of %d (stage %d now %d)\n",
                (int16_t)rd16(g_mem, self + COMP_YEAR), given + 1, six, given, (int)rd32(g_mem, self + COMP_STAGE_NOW));
    }
    uint32_t stages = rd32(g_mem, self + COMP_STAGES), now = rd32(g_mem, self + COMP_STAGE_NOW);
    wr32(g_mem, self + COMP_STAGES, stages + 4u * (uint32_t)shift);
    wr32(g_mem, self + COMP_STAGE_NOW, now - (uint32_t)shift);
    f[3] = 0;
    hol_news_shift = shift;
    o_005f6d20();
    hol_news_shift = 0;
    if (rt_unwinding) return;
    int done = f[3] != 0;
    wr32(g_mem, self + COMP_STAGES, stages);
    wr32(g_mem, self + COMP_STAGE_NOW, now);
    f[3] = (uint8_t)(shift + done);
}

/* 1 for a HolFirst or BelSecond object after its split (the marker in +0x3c). */
static int after_split(uint32_t obj)
{
    return season_sizes_on() && class_of(obj) &&
           season_split_meetings(obj) != (int16_t)rd16(g_mem, obj + COMP_MEETINGS);
}

#define F_LEAGUE_ADD_RESULT 0x00685d30u   /* League::add_result, thiscall (fixture, result), ret 8 */

/* HolFirst::add_result 0x5f65b0 and BelSecond::add_result 0x420110, thiscall (fixture, result), ret 8, return 1
 * (0x5f6638, 0x42018a). */
static void add_result(void (*original)(void))
{
    uint32_t self = cpu.ecx;
    if (!after_split(self)) {
        original();
        return;
    }
    uint32_t a[2] = {rt_arg(0), rt_arg(1)};
    Cpu saved = cpu;
    cpu.ecx = self;
    rt_callback(F_LEAGUE_ADD_RESULT, 2, a);
    if (rt_unwinding) return;
    cpu = saved;
    rt_ret_stdcall(1, 2);
}

/* HolFirst with six periods: add_result 0x5f65b0 as the class does it, with the period of the round (season_period_
 * own_of: rounds 37 and 38 none). */
static void hol_six_add_result(uint32_t self, int six)
{
    uint32_t fixture = rt_arg(0), result = rt_arg(1);
    uint32_t a[2] = {fixture, result};
    log_periods(self, &CLASSES[0], rounds_of(self));
    Cpu saved = cpu;
    cpu.ecx = self;
    rt_callback(F_LEAGUE_ADD_RESULT, 2, a);
    if (rt_unwinding) return;
    if (g_mem[fixture + 0x42u] == 0xffu) {
        int p = season_period_own_of(rounds_of(self), six, (int16_t)rd16(g_mem, fixture + 0x34u));
        while (p >= 0 && (int32_t)rd32(g_mem, self + COMP_STAGE_NOW) < p) {
            uint32_t next = (rd32(g_mem, self + COMP_STAGE_NOW) + 1u) & 0xffu;
            cpu.ecx = self;
            rt_callback(F_CREATE_PERIOD_STAGE, 1, &next);
            if (rt_unwinding) return;
            /* create_period_stage names a period after the third "Period 4" (0x43c at 0x5f6bdd): the fifth and
             * sixth get their own ids. */
            uint32_t made = rd32(g_mem, rd32(g_mem, self + COMP_STAGES) + 4u * next);
            if (next >= 4 && next <= 5 && readable(made) &&
                (int16_t)rd16(g_mem, made + COMP_NAME_CODE) == PERIOD_NAME_LAST)
            {
                int id = next == 4 ? PERIOD_NAME_FIFTH : PERIOD_NAME_SIXTH;
                wr16(g_mem, made + COMP_NAME_CODE, (uint16_t)id);
                if (logging())
                    fprintf(stderr, "recomp: league sizes: hol_first %d: period %u named %s\n",
                            (int16_t)rd16(g_mem, self + COMP_YEAR), next + 1u, season_sizes_period_name(id, 0));
            }
        }
        uint32_t stage = p >= 0 ? rd32(g_mem, rd32(g_mem, self + COMP_STAGES) + 4u * (uint32_t)p) : 0;
        if (readable(stage)) {
            /* As 0x5f65d4..0x5f662e: the fixture holds its period while the stage takes the result, and +8 is
             * cleared. */
            g_mem[fixture + 0x42u] = (uint8_t)p;
            uint32_t keep = rd32(g_mem, self + 8u);
            wr32(g_mem, self + 8u, 0);
            cpu.ecx = stage;
            rt_callback(rd32(g_mem, rd32(g_mem, stage) + 4u), 2, a);
            if (rt_unwinding) return;
            wr32(g_mem, self + 8u, keep);
        }
        g_mem[fixture + 0x42u] = 0xffu;
    }
    cpu = saved;
    rt_ret_stdcall(1, 2);
}

void o_005f65b0(void);
RT_WRAP(005f65b0, hol_first_add_result)
{
    int six = after_split(cpu.ecx) ? 0 : hol_six(cpu.ecx, NULL);
    if (six) hol_six_add_result(cpu.ecx, six);
    else add_result(o_005f65b0);
}

void o_00420110(void);
RT_WRAP(00420110, bel_second_add_result) { add_result(o_00420110); }

/* The names of the fifth and sixth periods of the Eerste Divisie (gap 33) for the round name function
 * (season_round_names_game.c): long and plural "Fifth Period", short "Period 5". NULL for another id. */
const char *season_sizes_period_name(int id, int abbreviated)
{
    return id == PERIOD_NAME_FIFTH ? season_period_name(5, abbreviated)
         : id == PERIOD_NAME_SIXTH ? season_period_name(6, abbreviated) : NULL;
}
