/* The qualifying rounds of the Champions League and the Europa League in a season of the access list (0.6.0
 * slice 23, Q369, Q296; season_uefa.h). Evidence: docs/research/2026-10-18-uefa-2009-2020.md.
 *
 * The seeding (season_uefa_seeding.c) gives each club an entry code of the access list (SEASON_UEFA_CL_*,
 * SEASON_UEFA_EL_*). This file builds both cups from those codes, in place of the original's fixed counts:
 *
 * Champions League (EuropeanCup, vtable 0x96a1d0). build_entrants 0x584fa0 wants exactly 71 clubs with codes 1
 * to 4 (0x584faa, caps 0x585095..0x585135). The entry table 0x584990 makes 55 slots (0x5849a5) of the Q1, Q2
 * and Q3 clubs. Create_round_table 0x583c50 with stage -1 makes 3 rounds (0x583c9b). Here the list +0x14 holds
 * every entrant: the group clubs first, then the qualifying clubs. The entry table holds the qualifying clubs
 * by round from the lowest (PR, Q1, Q2 CP, Q2 LP, Q3 CP, Q3 LP, PO CP, PO LP), then the group clubs after the
 * first 16. The round table has one round for each round the row plays (codes 240 PR, 250 Q1, 260 Q2, 270 Q3,
 * 220 PO: the round names table gives their real names, season_round_names.h). The group draw
 * draw_phase1_groups 0x585290 takes the 16 clubs of list slots 0 to 15 and the clubs with entry byte +5 = 1
 * (0x585358..0x5853ba, 32 in all): the play-off winners get 1 from the last round (0x51af97), and the group
 * clubs after the first 16 get 1 here, after map_fixture_tree. Their slots come after every slot that a round
 * uses, so the completion test 0x51b039..0x51b06d does not see them.
 *
 * The paths. map_fixture_tree 0x518790 (vtable +0x94) makes the ties of every round at the season start: two
 * sides of entry slots (+0, +2; -1 for a winner to come), the source tie and round of each side (+0xa/+0xe
 * home, +0xc/+0x10 away) and where the winner goes (+6 tie, +8 1 home 0 away, +0x12 round; words, 0x518ef1..
 * 0x518fa7). Its draws put winners and new clubs in any tie, so a champion could meet a non-champion. Here the
 * wrap of 0x518790 writes the ties again after the original: a Champions Path tie has only champions (new
 * champions and the winners of the Champions Path ties below), a League Path tie only the other clubs, each
 * winner of the round below meets a new club when it can (as the seeded draw 0x51d2d0 does), and the rest
 * meet each other at random (0x90cfc0). Every round has flag 0x200 of +0xd and the nation rule 4 of +0xb, so the
 * draw 0x51c410 runs the nation draw 0x51d9f0 (0x51c5d4; +0xb wins over 0x200), which moves the known clubs over
 * all ties of the round: its wrap below runs it on each path apart (issue #68). Then the home and away rules
 * (0x51e870, 0x522610).
 *
 * Europa League (Uefa_Cup, vtable 0x970790). build_entrants 0x902b90 (wrapped in season_uefa_cup.c) gives way
 * to uc_access_entrants: the qualifying clubs by round from the lowest (each round: its new clubs, then slots for
 * the Champions League losers that the drops of the row send to it), the group clubs, 16 slots for the
 * Champions League play-off losers (the original's hand-over at count - 0x18, 0x585a02, in draw_phase1_groups),
 * and 8 slots for the Champions League group thirds (count - 8, 0x586282). The rounds are drawn by the
 * original (seeded, 0x400): the new clubs of a round have a higher tier byte +4 than the winners. A round with
 * an empty slot waits (0x51c46d, 0x51c9ab), so the losers of each Champions League round are put into their
 * slots on the days after that round ends (the wrap of Cup::update_for_date 0x51f890 for the Europa League),
 * and the dates of each Europa League round come after the Champions League round that feeds it. The group
 * stage of season_uefa_cup.c takes the play-off winners and every club of the slots after the rounds.
 *
 * Conference League (slice 24, part b: a second object of the Uefa_Cup class, season_uefa_cup_kind 'k'). It is
 * built as the Europa League, with codes 21 to 25 and one path a round: its qualifying clubs by round from the
 * lowest, each round with slots for the losers of the other two cups that the row's ecl_drops send to it, then
 * its group clubs, the slots of the Europa League play-off losers for the groups and 8 slots for the Europa League
 * group thirds (the knockout round play-offs, season_uefa_cup.c). The Champions League losers come as they come
 * to the Europa League. A Europa League loser is a side of a tie of its round whose entry byte +5 is -1 (set at
 * 0x51b5cd when a club goes out) and that is in no tie of the next round, so each loser is found as soon as its
 * tie ends; it takes the slot of its tie. */
#include "win.h"
#include "season.h"
#include "season_uefa.h"
#include "season_cups.h"
#include "season_names.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_CL 0x9cf6e8u
#define KEY_UC 0x9cf6f0u
#define COMP_TABLE 0xadadfcu
#define CL_VTABLE 0x0096a1d0u
#define UC_VTABLE 0x00970790u
#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu

#define F_MALLOC 0x00944e46u
#define F_FREE 0x009452cau
#define F_RANDOM 0x0090cfc0u
#define F_CL_ROUNDS 0x00583c50u         /* EuropeanCup::create_round_table, thiscall (stage, &rounds, &name, &league) */
#define F_DRAW_DATE 0x00521e60u
#define F_NATION_DRAW 0x0051d9f0u       /* Cup::check_round_teams_for_nation, thiscall (round), ret 4 */
#define F_MATCH_DATE 0x00521eb0u

#define ROUND_SIZE 0x68u
#define TIE_SIZE 0x35u
#define ENTRY 6u
#define MAX_ROUNDS 6
#define MAX_ENTRANTS 260
#define DIRECT_IN_LIST 16               /* the group clubs that draw_phase1_groups reads from the list (0x585358) */
#define THIRDS 8
#define HANDOVER (0x18 - THIRDS)        /* the hand-over slots of the play-off losers: from count - 0x18 (0x585a02)
                                         * to the thirds' slots */

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

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
    if (!rt_unwinding && readable(p)) memset(g_mem + p, 0, size);
    return rt_unwinding ? 0 : p;
}

static void game_free(uint32_t p)
{
    uint32_t a[1] = {p};
    if (readable(p)) call_cdecl(F_FREE, 1, a);
}

static int random_below(int n)
{
    if (n <= 1) return 0;
    uint32_t a[1] = {(uint32_t)n};
    int k = (int)call_cdecl(F_RANDOM, 1, a);
    return k < 0 || k >= n ? 0 : k;
}

static uint32_t comp_object(uint32_t key, uint32_t vtable)
{
    int32_t id = (int32_t)rd32(g_mem, key);
    uint32_t table = rd32(g_mem, COMP_TABLE);
    if (id < 0 || !readable(table)) return 0;
    uint32_t obj = rd32(g_mem, table + 4u * (uint32_t)id);
    return readable(obj) && rd32(g_mem, obj) == vtable ? obj : 0;
}

/* The row of a cup's season (+0x40) when it has an access list, else NULL. */
static const SeasonUefa *access_row_of(uint32_t cup)
{
    const SeasonUefa *r = season_uefa_row((int16_t)rd16(g_mem, cup + 0x40));
    return r && season_uefa_access_places(r->season, 'c', 1) ? r : NULL;
}

static void set_dates(uint32_t t, int r, int draw_day, int draw_month, int day, int month, int weekday, uint32_t year)
{
    uint32_t d[7] = {t, (uint32_t)r, (uint32_t)draw_day, (uint32_t)draw_month, 0, 4, year};
    call_cdecl(F_DRAW_DATE, 7, d);
    uint32_t m[9] = {t, (uint32_t)r, (uint32_t)day, (uint32_t)month, 0, (uint32_t)weekday, 2, year, 0};
    if (!rt_unwinding) call_cdecl(F_MATCH_DATE, 9, m);
}

/* Test switch RECOMP_UEFA_PATH_DRAW=off (issue #68 proof): the paths as before the fix, in the layout (a round
 * with no new League Path clubs takes both paths) and in the draw (the wrap of 0x51d9f0 runs the original). */
static int path_draw_off(void)
{
    static int off = -1;
    if (off < 0) {
        const char *v = getenv("RECOMP_UEFA_PATH_DRAW");
        off = v && !strcmp(v, "off");
    }
    return off;
}

/* ---- The layout of a cup's entrants ---- */

/* A qualifying round: its code (the round code of the game), the entry codes of its paths, its legs and the
 * dates. */
typedef struct {
    int round_code;
    int cp, lp;                         /* entry codes of the Champions Path and League Path (lp 0: one path) */
    int final;                          /* 1: a round of the winners of the round before only (no new clubs) */
    int legs, gap;                      /* matches a tie, days between them (+0x21, +0x22) */
    int weekday;                        /* 0 Monday .. 6 Sunday, the match day (cup_set_round_match_date) */
    int draw[2], match[2];              /* draw and first leg: day, month 0 to 11 */
} Level;

/* The dates. A game in a moved season starts on 11 July (the first day of the rules report of a 2010/11
 * game), and the Champions League groups start on 12 September (the original's first group match day, weekday
 * 1, 0x583f41..0x583f5e). The date function moves a match to the nearest day of its weekday, up to 3 days either
 * way (0x549f70 from 0x521ed3; seen in a 2010/11 game: 29 August, a Sunday, gave Wednesday 1 September with
 * weekday 2, and 30 August gave Friday 27 August with weekday 4). The original passes weekday 2 for its Champions
 * League rounds (0x583cef..0x583d0e) and 3 for the UEFA Cup's (0x9026a4). A date d with weekday w moves by the same
 * days as d + n with weekday w + n, so the dates below keep their order in every year:
 *   Champions League: the preliminary round (2018/19 on) is a tournament of single matches, as the real one
 *   (rows uefa-138, uefa-139: "PR 4", one winner): the real days, semi-finals on Tuesday 26 June and the final on
 *   Friday 29 June (2018/19: 26 and 29 June; 2019/20: 25 and 28; 2021/22: 22 and 25; 2022/23: 21 and 24; 2023/24:
 *   27 and 30, en.wikipedia.org "20xx-yy UEFA Champions League qualifying"; 2020/21 was moved to August by COVID).
 *   First legs on Wednesday 18 July, 1, 15 and 29 August, second legs 7 days later (+0x22), the last by 8 September:
 *   a week after the real ones (Q1 on 10 and 11 July 2018), since a new game in a moved season starts on a Saturday
 *   from 10 to 16 July.
 *   Europa League: one day after the Champions League (Thursday). The preliminary round has two legs a week apart,
 *   on Thursday 28 June and 5 July (real 2018/19: 26 and 28 June, 5 July; 2019/20: 27 June, 2 and 4 July).
 *   The tables of a running game are made at the season change, on day 171 (20 or 21 June; measured in 2020 and
 *   2021). A draw date before it is drawn on that day, and the semi-finals come 2 to 9 days after it (3 or more in
 *   2018 to 2023, the seasons of the rows with this round). A new game makes the tables at its setup and plays the
 *   preliminary rounds there.
 *   A winner goes into its tie of the next round when its tie ends (Cup::process_fixture 0x51a150, 0x51a472..). If
 *   that tie's match day is then 2 days away or less (0x54c5b0 with 2, the test after it), its fixture goes to a
 *   later free day (0x5ab510), a week later for a first qualifying round. So each match of a chain is at least 3
 *   days after the one before it (row A18: with the final on 17 July a preliminary round winner played its first
 *   qualifying round a week late, and the Europa and Conference League rounds that take its losers waited). So each
 *   round starts after the Champions League
 *   round whose losers it takes has ended (the preliminary round and Q1 before EL Q2, Q2 before EL Q3, Q3 before
 *   EL PO), and its play-offs end the day after the Champions League's: the hand-over of the play-off losers (at the
 *   Champions League group draw) comes before the Europa League group draw.
 * The real calendar starts in late June (rows uefa-66 to uefa-160): these dates keep its order in fewer weeks. */
static const Level CL_LEVELS[] = {
    {240, SEASON_UEFA_CL_PR, 0, 0, 1, 0, 1, {21, 5}, {26, 5}},
    {230, 0, 0, 1, 1, 0, 4, {21, 5}, {29, 5}},
    {250, SEASON_UEFA_CL_Q1, 0, 0, 2, 7, 2, {12, 6}, {18, 6}},
    {260, SEASON_UEFA_CL_Q2_CP, SEASON_UEFA_CL_Q2_LP, 0, 2, 7, 2, {26, 6}, {1, 7}},
    {270, SEASON_UEFA_CL_Q3_CP, SEASON_UEFA_CL_Q3_LP, 0, 2, 7, 2, {9, 7}, {15, 7}},
    {220, SEASON_UEFA_CL_PO_CP, SEASON_UEFA_CL_PO_LP, 0, 2, 7, 2, {23, 7}, {29, 7}},
};
/* The Conference League plays on the Europa League's days. Each round starts after the rounds whose losers it
 * takes: Q2 after the Champions League's preliminary round and Q1, the play-offs after the Europa League's Q3,
 * the groups after the Europa League's play-offs (the group draw waits until it has its 32 clubs, as the Europa
 * League's does). */
static const Level ECL_LEVELS[] = {
    {250, SEASON_UEFA_ECL_Q1, 0, 0, 2, 7, 3, {12, 6}, {19, 6}},
    {260, SEASON_UEFA_ECL_Q2, 0, 0, 2, 7, 3, {26, 6}, {2, 7}},
    {270, SEASON_UEFA_ECL_Q3, 0, 0, 2, 7, 3, {9, 7}, {16, 7}},
    {220, SEASON_UEFA_ECL_PO, 0, 0, 2, 7, 3, {23, 7}, {30, 7}},
};
#define ECL_N ((int)(sizeof ECL_LEVELS / sizeof ECL_LEVELS[0]))
static const Level EL_LEVELS[] = {
    {240, SEASON_UEFA_EL_PR, 0, 0, 2, 7, 3, {21, 5}, {28, 5}},
    {250, SEASON_UEFA_EL_Q1, 0, 0, 2, 7, 3, {12, 6}, {19, 6}},
    {260, SEASON_UEFA_EL_Q2, 0, 0, 2, 7, 3, {26, 6}, {2, 7}},
    {270, SEASON_UEFA_EL_Q3, 0, 0, 2, 7, 3, {9, 7}, {16, 7}},
    {220, SEASON_UEFA_EL_PO, 0, 0, 2, 7, 3, {23, 7}, {30, 7}},
};
#define CL_N ((int)(sizeof CL_LEVELS / sizeof CL_LEVELS[0]))
#define EL_N ((int)(sizeof EL_LEVELS / sizeof EL_LEVELS[0]))

/* The dates of a running season (row A18 of the 0.6.0 release test, O375). The dates above fit a new game, which
 * starts on a Saturday from 10 to 16 July. A running game makes the season's tables at the season change, on day
 * 171 (20 or 21 June), so its qualifying rounds can keep the real calendar's shape: Q1 from about 1 July, a week
 * between rounds, the August FIFA date free between Q3 and the play-offs, and the play-offs over by 30 August,
 * before the September window. With the dates above, every Wednesday from mid-July to early September held a round:
 * the August friendly date (season_friendly_dates.c) fell on the Champions League Q3 second leg or the Q2/Q3 legs
 * around it, and the Europa League play-off second legs (first week of September) on the September window. A club
 * with more than two players on duty within a day then has its tie moved a week (can_schedule_club_fixture
 * 0x776fc0, clash 21 in find_fixture_clashes 0x5ad780, the move at 0x5aa96e), and a Champions League Q3 tie moved a
 * week brought its losers to the Europa League after the play-off draw was due (gate (b) run on eee3e71a: 2009/10,
 * 2015/16, 2016/17, 2027/28, 2028/29). Real 2009/10: Q1 30 June and 7 July, Q2 14 and 21 July, Q3 28 July and
 * 4 August, FIFA date 12 August, play-offs 18 and 25 August (Europa League play-offs 20 and 27 August); real
 * 2025/26: Q1 8 July, Q3 5 and 12 August, play-offs 19 and 26 August (en.wikipedia.org pages "2009-10 UEFA Champions
 * League qualifying", "2009-10 UEFA Europa League qualifying", "2025-26 UEFA Champions League qualifying", read
 * 25 October 2026). The dates of a running season, each a whole
 * number of weeks from the others, so the weekday move keeps their order:
 *   Champions League: Q1 1 July, Q2 15 July, Q3 29 July, play-offs 19 August (draws 6 days before). A season whose
 *   Champions League has a preliminary round (2018/19 to 2023/24: semi-finals 26 June, final 29 June) starts Q1 on
 *   8 July, so Q1 comes 3 or more days after the final: Q2 22 July, Q3 5 August, play-offs 19 August (no August
 *   FIFA date in those seasons).
 *   Europa League and Conference League: a day after the Champions League round of the same name (draws 7 days
 *   before). With a Europa League preliminary round (2018/19 to 2020/21) that round plays 25 June and 2 July and
 *   Q1 starts 9 July.
 * The last legs: Champions League play-off second legs on the Wednesday nearest 26 August (23 to 29 August), Europa
 * and Conference League a day later, before the first September FIFA date of every season from 2009 (28 August
 * 2017 is a Monday window start; its matches were from 31 August). The start season of a 2001 game keeps the dates above. */
typedef struct {
    int code;
    int draw[2], match[2];              /* day, month 0 to 11 */
} RunDate;
static const RunDate RUN_CL[] = {{250, {25, 5}, {1, 6}}, {260, {9, 6}, {15, 6}}, {270, {23, 6}, {29, 6}},
                                 {220, {13, 7}, {19, 7}}};
static const RunDate RUN_CL_PR[] = {{250, {2, 6}, {8, 6}}, {260, {16, 6}, {22, 6}}, {270, {30, 6}, {5, 7}},
                                    {220, {13, 7}, {19, 7}}};
static const RunDate RUN_EL[] = {{250, {25, 5}, {2, 6}}, {260, {9, 6}, {16, 6}}, {270, {23, 6}, {30, 6}},
                                 {220, {13, 7}, {20, 7}}};
static const RunDate RUN_EL_PR[] = {{240, {21, 5}, {25, 5}}, {250, {2, 6}, {9, 6}}, {260, {16, 6}, {23, 6}},
                                    {270, {30, 6}, {6, 7}}, {220, {13, 7}, {20, 7}}};

/* d: draw day, draw month, match day, match month; replaced in a running season. */
static void run_dates(char comp, const SeasonUefa *r, int code, int year, int d[4])
{
    /* The start season of a 2001 start keeps the dates above (O405, the base game); a later start's start season
     * takes these too (season_friendly_dates.c, keeps_original): its rounds before the start date are played in the
     * run to the start date (game_run_to_start_date 0x5cc850), as the original plays its own. */
    if (!r || !season_game_changes() || (year == season_start_year() && year == SEASON_FIRST_YEAR)) return;
    int cl_pr = season_uefa_code_clubs(r, 'c', SEASON_UEFA_CL_PR) > 0;
    int el_pr = comp == 'e' && season_uefa_code_clubs(r, 'e', SEASON_UEFA_EL_PR) > 0;
    const RunDate *t = comp == 'c' ? (cl_pr ? RUN_CL_PR : RUN_CL) : (el_pr || cl_pr ? RUN_EL_PR : RUN_EL);
    int n = comp == 'c' ? 4 : (el_pr || cl_pr ? 5 : 4);
    for (int k = 0; k < n; k++)
        if (t[k].code == code) {
            d[0] = t[k].draw[0], d[1] = t[k].draw[1], d[2] = t[k].match[0], d[3] = t[k].match[1];
            return;
        }
}

typedef struct {
    int n;                              /* entrants of the list */
    uint32_t clubs[MAX_ENTRANTS];       /* by code: group clubs, then the qualifying codes from the lowest round */
    int count[SEASON_UEFA_CODE_SLOTS];                      /* clubs of each code */
    int drops[SEASON_UEFA_CODE_SLOTS];                      /* Europa League: slots for Champions League losers at each code */
    int rounds;                         /* qualifying rounds played */
    int level[MAX_ROUNDS];              /* the level of each round (index into the LEVELS table) */
    int clubs_in[MAX_ROUNDS], fresh[MAX_ROUNDS], first[MAX_ROUNDS];
    int cp_fresh[MAX_ROUNDS], lp_fresh[MAX_ROUNDS];
    int two[MAX_ROUNDS];                /* Champions League: 1 for a round of two paths (new League Path clubs, or
                                         * League Path winners of the round before) */
    int direct_first, slots;            /* the first slot after the rounds, the slot count */
} Layout;

static int club_code(uint32_t club) { return g_mem[club + 0x1df]; }

/* Slice 24: the tables of a comp ('c', 'e' or 'k'). */
static const Level *levels_of(char comp, int *n)
{
    *n = comp == 'c' ? CL_N : comp == 'k' ? ECL_N : EL_N;
    return comp == 'c' ? CL_LEVELS : comp == 'k' ? ECL_LEVELS : EL_LEVELS;
}
static uint32_t id_of(char comp)
{
    return comp == 'k' ? (uint32_t)season_uefa_ecl_id() : rd32(g_mem, comp == 'c' ? KEY_CL : KEY_UC);
}
static int group_code(char comp) { return comp == 'c' ? SEASON_UEFA_CL_G : comp == 'k' ? SEASON_UEFA_ECL_G : SEASON_UEFA_EL_G; }
static const char *name_of(char comp)
{
    return comp == 'c' ? "Champions League" : comp == 'k' ? "Conference League" : "Europa League";
}

/* The layout of comp ('c' or 'e'). The Champions League's from the list +0x14 of cl when cl is given (after
 * build_entrants: a loser that goes to the Europa League gets that cup's id at +0x1db, 0x585a6a, but stays in the
 * list), else from the clubs with its id at +0x1db. The Europa League's from the clubs with its id and a code of
 * 11 to 16 (the Champions League losers there keep their codes of 1 to 9). Returns 0 when the counts cannot make
 * rounds (an odd number of clubs in a path). */
static int make_layout(const SeasonUefa *r, char comp, Layout *L, uint32_t cl)
{
    memset(L, 0, sizeof *L);
    uint32_t key = id_of(comp), table = rd32(g_mem, CLUBS);
    int n = (int32_t)rd32(g_mem, CLUB_COUNT);
    int lo = group_code(comp);
    int codes = comp == 'c' ? SEASON_UEFA_CL_CODES : comp == 'k' ? SEASON_UEFA_ECL_CODES : SEASON_UEFA_EL_CODES;
    if (comp == 'k' && (int32_t)key < 0) return 0;
    uint32_t list = cl ? rd32(g_mem, cl + 0x14) : 0;
    int listed = cl ? (int16_t)rd16(g_mem, cl + 0x36) : 0;
    for (int code = lo; code < codes; code++) {
        if (cl) {
            for (int i = 0; readable(list) && i < listed && L->n < MAX_ENTRANTS; i++) {
                uint32_t club = rd32(g_mem, list + ENTRY * (uint32_t)i);
                if (readable(club) && club_code(club) == code) L->clubs[L->n++] = club, L->count[code]++;
            }
            continue;
        }
        for (int i = 0; i < n && readable(table); i++) {
            uint32_t club = table + 0x245u * (uint32_t)i;
            if (rd32(g_mem, club + 0x1db) != key || club_code(club) != code || L->n >= MAX_ENTRANTS) continue;
            L->clubs[L->n++] = club;
            L->count[code]++;
        }
    }
    if (comp != 'c')
        for (int code = lo; code < codes; code++) L->drops[code] = season_uefa_dropped(r, code);
    int levels;
    const Level *lv = levels_of(comp, &levels);
    int slot = 0, cp_win = 0, lp_win = 0;
    for (int k = 0; k < levels; k++) {
        int cp = lv[k].cp ? L->count[lv[k].cp] + L->drops[lv[k].cp] : 0, lp = lv[k].lp ? L->count[lv[k].lp] : 0;
        if (lv[k].final && (k == 0 || L->rounds == 0 || L->level[L->rounds - 1] != k - 1)) continue;
        if (!cp && !lp && !cp_win && !lp_win) continue;
        if (!cp && !lp && !lv[k].final) {              /* a round with no new clubs: not in the access list */
            fprintf(stderr, "recomp: uefa: %s round %d has no new clubs\n", name_of(comp), lv[k].round_code);
            return 0;
        }
        int i = L->rounds++;
        if (i >= MAX_ROUNDS) return 0;
        L->level[i] = k;
        /* Champions League: the winners stay in their path, also in a round with no new League Path clubs (the
         * play-offs from 2018/19: PO LP 4 clubs, the Q3 League Path winners; issue #68). A round with no League
         * Path at all (Q1, and Q2 to 2017/18) has one path. */
        if (comp != 'c' || (!lp && path_draw_off())) cp_win += lp_win, lp_win = 0;
        L->two[i] = comp == 'c' && lp + lp_win > 0;
        L->cp_fresh[i] = cp, L->lp_fresh[i] = lp;
        L->fresh[i] = cp + lp, L->first[i] = slot, L->clubs_in[i] = cp + lp + cp_win + lp_win;
        if ((cp + cp_win) % 2 || (lp + lp_win) % 2) {
            fprintf(stderr, "recomp: uefa: %s round %d: %d + %d and %d + %d clubs, not even\n", name_of(comp),
                    lv[k].round_code, cp, cp_win, lp, lp_win);
            return 0;
        }
        slot += cp + lp;
        cp_win = (cp + cp_win) / 2, lp_win = (lp + lp_win) / 2;
    }
    L->direct_first = slot;
    int direct = L->count[group_code(comp)];
    if (comp == 'c') {
        /* Slice 25: the league phase has 36 clubs (2024/25 on), the group stage 32. */
        int group = season_uefa_cl_round(r, "Groups");
        if (direct < DIRECT_IN_LIST || direct + cp_win + lp_win != group) {
            fprintf(stderr, "recomp: uefa: CL groups: %d direct and %d winners, not %d\n", direct, cp_win + lp_win,
                    group);
            return 0;
        }
        L->slots = slot + direct - DIRECT_IN_LIST;
    } else if (comp == 'k') {
        /* Slice 24: the group clubs, the slots of the Europa League play-off losers, the Europa League thirds. */
        L->slots = slot + direct + L->drops[SEASON_UEFA_ECL_G] + THIRDS;
    } else {
        L->drops[SEASON_UEFA_EL_G] = season_uefa_dropped(r, SEASON_UEFA_EL_G);
        L->slots = slot + direct + HANDOVER + THIRDS;
    }
    return L->rounds > 0;
}

/* The slot of the first new club of code in the entry table (the clubs of the list in code order). */
static int list_index_of_code(const Layout *L, int code)
{
    int at = 0;
    for (int i = 0; i < L->n; i++) {
        if (club_code(L->clubs[i]) == code) return at;
        at++;
    }
    return at;
}

/* ---- Champions League ---- */

void o_00584fa0(void);
void o_00584990(void);

/* The list +0x14 of the access list: the group clubs, then the qualifying clubs from the lowest round. List
 * byte +4 as the original's tiers (7 group, 8 Q3, 13 Q2, 0 others: 0x584fa0). */
static int cl_entrants(uint32_t cl, const Layout *L)
{
    uint32_t list = game_malloc(ENTRY * (uint32_t)L->n);
    if (!list) return 0;
    game_free(rd32(g_mem, cl + 0x14));
    if (rt_unwinding) return 0;
    int at = 0;
    for (int i = 0; i < L->n; i++) {                                  /* the group clubs first */
        if (club_code(L->clubs[i]) != SEASON_UEFA_CL_G) continue;
        wr32(g_mem, list + ENTRY * (uint32_t)at, L->clubs[i]);
        g_mem[list + ENTRY * (uint32_t)at + 4] = 7;
        at++;
    }
    for (int k = 0; k < CL_N; k++)
        for (int path = 0; path < 2; path++) {
            int code = path ? CL_LEVELS[k].lp : CL_LEVELS[k].cp;
            for (int i = 0; code && i < L->n; i++) {
                if (club_code(L->clubs[i]) != code) continue;
                wr32(g_mem, list + ENTRY * (uint32_t)at, L->clubs[i]);
                g_mem[list + ENTRY * (uint32_t)at + 4] =
                    CL_LEVELS[k].round_code == 270 ? 8 : CL_LEVELS[k].round_code == 260 ? 13 : 0;
                at++;
            }
        }
    wr32(g_mem, cl + 0x14, list);
    wr16(g_mem, cl + 0x36, (uint16_t)at);
    wr16(g_mem, cl + 0x38, (uint16_t)at);
    return 1;
}

/* EuropeanCup::build_entrants, thiscall () returning 1 or 0. */
RT_WRAP(00584fa0, cl_build_entrants)
{
    uint32_t cl = cpu.ecx;
    const SeasonUefa *r = rd32(g_mem, cl) == CL_VTABLE ? access_row_of(cl) : NULL;
    Layout L;
    if (!r || !make_layout(r, 'c', &L, 0)) {
        if (r) fprintf(stderr, "recomp: uefa: Champions League %d: the access list gives no layout\n", r->season);
        o_00584fa0();
        return;
    }
    int ok = cl_entrants(cl, &L);
    if (rt_unwinding) return;
    fprintf(stderr, "recomp: uefa: Champions League %d/%02d: %d entrants, %d direct, %d qualifying rounds\n",
            (int16_t)rd16(g_mem, cl + 0x40), ((int16_t)rd16(g_mem, cl + 0x40) + 1) % 100, L.n,
            L.count[SEASON_UEFA_CL_G], L.rounds);
    rt_ret_cdecl(ok ? 1u : 0u);
}

/* The entry table of the list: the qualifying clubs (list slots after the group clubs, in order), then the
 * group clubs after the first 16. Entry byte +4 is the round of entry (0 the lowest), +5 0. */
RT_WRAP(00584990, cl_entry_table)
{
    uint32_t cl = cpu.ecx;
    const SeasonUefa *r = rd32(g_mem, cl) == CL_VTABLE ? access_row_of(cl) : NULL;
    Layout L;
    if (!r || !make_layout(r, 'c', &L, cl) || (int16_t)rd16(g_mem, cl + 0x36) != L.n) {
        o_00584990();
        return;
    }
    uint32_t e = game_malloc(ENTRY * (uint32_t)(L.slots > 0 ? L.slots : 1));
    if (!e) {
        if (!rt_unwinding) rt_ret_cdecl(0);
        return;
    }
    uint32_t list = rd32(g_mem, cl + 0x14);
    int direct = L.count[SEASON_UEFA_CL_G], q = L.n - direct;
    for (int i = 0; i < q; i++) {
        uint32_t club = rd32(g_mem, list + ENTRY * (uint32_t)(direct + i));
        wr32(g_mem, e + ENTRY * (uint32_t)i, club);
        int tier = 0;
        for (int k = 0; k < L.rounds; k++)
            if (i >= L.first[k] && i < L.first[k] + L.fresh[k]) tier = k;
        g_mem[e + ENTRY * (uint32_t)i + 4] = (uint8_t)tier;
    }
    for (int i = DIRECT_IN_LIST; i < direct; i++)
        wr32(g_mem, e + ENTRY * (uint32_t)(q + i - DIRECT_IN_LIST), rd32(g_mem, list + ENTRY * (uint32_t)i));
    wr32(g_mem, cl + 0xa7, e);
    wr16(g_mem, cl + 0x3e, (uint16_t)L.slots);
    rt_ret_cdecl(1);
}

static int cl_inner;

/* The round table of stage -1 for the access list (from season_uefa.c's wrap of 0x583c50), or 0: one round
 * for each round of the layout, made from the original's Q2 record (code 260, 0x583d.., as slice 22's tables),
 * with flag 0x200 (the ties of map_fixture_tree stay) and the nation rule flag 4 of +0xb. */
uint32_t season_uefa_cl_qual_table(uint32_t cl, int *rounds_out, uint32_t *name_out, uint32_t *league_out)
{
    const SeasonUefa *r = access_row_of(cl);
    Layout L;
    if (cl_inner || !r || !make_layout(r, 'c', &L, cl)) return 0;
    uint32_t saved_esp = cpu.esp;
    cpu.esp -= 16;
    uint32_t out = cpu.esp;
    for (int i = 0; i < 16; i += 4) wr32(g_mem, out + (uint32_t)i, 0);
    uint32_t a[4] = {0xffffffffu, out, out + 4, out + 8};
    cl_inner = 1;
    uint32_t orig = call_this(F_CL_ROUNDS, cl, 4, a);
    cl_inner = 0;
    int orig_rounds = (int16_t)rd16(g_mem, out);
    *name_out = rd32(g_mem, out + 4);
    *league_out = rd32(g_mem, out + 8);
    if (rt_unwinding) return 0;
    cpu.esp = saved_esp;
    if (!readable(orig) || orig_rounds < 2) return 0;
    uint32_t t = game_malloc(ROUND_SIZE * (uint32_t)L.rounds);
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, cl + 0x40);
    for (int i = 0; t && i < L.rounds && !rt_unwinding; i++) {
        uint32_t rec = t + ROUND_SIZE * (uint32_t)i;
        memcpy(g_mem + rec, g_mem + orig + ROUND_SIZE, ROUND_SIZE);
        const Level *lv = &CL_LEVELS[L.level[i]];
        wr16(g_mem, rec + 7, (uint16_t)lv->round_code);
        wr16(g_mem, rec + 9, (uint16_t)i);
        /* +0xd: the draw bit 0x200, and for a round of single matches (the preliminary round's semi-finals and
         * final) extra time and penalties on the day, as the real one and slice 21a's type 'E' (0x2 | 0x1). Without
         * them a level match is replayed days later: in a 2019 start a semi-final replay put the final after its
         * date, its winner reached its first qualifying round tie a week late, and that round, with every Europa and
         * Conference League round that takes its losers, ended a week late (box Cup.cpp 1278, row A18). The rounds
         * of two legs keep the rule of their second leg (+0xf) from the original's round. */
        wr16(g_mem, rec + 0xd, (uint16_t)(lv->legs == 1 ? 0x200 | 0x2 | 0x1 : 0x200));
        if (lv->legs == 1) g_mem[rec + 0x20] = 0;               /* no replay */
        g_mem[rec + 0xb] = 4;
        wr16(g_mem, rec + 0x18, (uint16_t)L.clubs_in[i]);
        wr16(g_mem, rec + 0x1a, (uint16_t)(L.clubs_in[i] / 2));
        wr16(g_mem, rec + 0x1c, (uint16_t)L.fresh[i]);
        wr16(g_mem, rec + 0x1e, (uint16_t)L.first[i]);
        g_mem[rec + 0x21] = (uint8_t)lv->legs;
        g_mem[rec + 0x22] = (uint8_t)lv->gap;
        int d[4] = {lv->draw[0], lv->draw[1], lv->match[0], lv->match[1]};
        run_dates('c', r, lv->round_code, (int)year, d);
        set_dates(t, i, d[0], d[1], d[2], d[3], lv->weekday, year);
    }
    game_free(orig);
    if (rt_unwinding || !t) return 0;
    season_uefa_no_away_goals(t, L.rounds, (int)year);
    *rounds_out = L.rounds;
    char codes[64] = "";
    for (int i = 0; i < L.rounds; i++)
        snprintf(codes + strlen(codes), sizeof codes - strlen(codes), " %d", CL_LEVELS[L.level[i]].round_code);
    fprintf(stderr, "recomp: uefa: Champions League %u/%02u: %d qualifying rounds (codes%s), %d slots\n", year,
            (year + 1) % 100, L.rounds, codes, L.slots);
    return t;
}

/* ---- The ties of the paths (wrap of map_fixture_tree 0x518790) ---- */

static uint32_t tie_at(uint32_t cup, int round, int t)
{
    uint32_t arr = rd32(g_mem, cup + 0xad);
    return rd32(g_mem, arr + 4u * (uint32_t)round) + TIE_SIZE * (uint32_t)t;
}

/* A side to place: an entry slot (src -1) or the winner of tie src of the round before. */
typedef struct {
    int slot, src;
} Side;

static void shuffle(Side *s, int n)
{
    for (int i = n - 1; i > 0 && !rt_unwinding; i--) {
        int j = random_below(i + 1);
        Side x = s[i];
        s[i] = s[j], s[j] = x;
    }
}

/* Writes side s into tie t of round r (home h). */
static void put_side(uint32_t cup, int r, int t, int h, Side s)
{
    uint32_t tie = tie_at(cup, r, t);
    wr16(g_mem, tie + (h ? 0 : 2), (uint16_t)(int16_t)(s.src < 0 ? s.slot : -1));
    if (s.src < 0) return;
    wr16(g_mem, tie + (h ? 0xa : 0xc), (uint16_t)s.src);
    wr16(g_mem, tie + (h ? 0xe : 0x10), (uint16_t)(r - 1));
    uint32_t from = tie_at(cup, r - 1, s.src);
    wr16(g_mem, from + 6, (uint16_t)t);
    wr16(g_mem, from + 8, (uint16_t)(h ? 1 : 0));
    wr16(g_mem, from + 0x12, (uint16_t)r);
}

/* Pairs the winners w (nw) with the new clubs f (nf) into ties first .. of round r: each winner meets a new
 * club when it can, the rest meet each other. Returns the ties used. */
static int pair_path(uint32_t cup, int r, int first, Side *w, int nw, Side *f, int nf)
{
    shuffle(w, nw);
    shuffle(f, nf);
    int t = first, iw = 0, iff = 0;
    while (iw < nw && iff < nf) {
        int h = random_below(2);
        put_side(cup, r, t, h, w[iw++]);
        put_side(cup, r, t, !h, f[iff++]);
        t++;
    }
    while (iw + 1 < nw) put_side(cup, r, t, 1, w[iw++]), put_side(cup, r, t++, 0, w[iw++]);
    while (iff + 1 < nf) put_side(cup, r, t, 1, f[iff++]), put_side(cup, r, t++, 0, f[iff++]);
    return t - first;
}

/* The Champions Path ties of round r (cl_paths puts them first): its new champions and the winners of the
 * Champions Path ties of the round before (of every tie when round r has one path). */
static int cp_ties_of(const Layout *L, int r)
{
    if (r < 0) return 0;
    int win = r == 0 ? 0 : !L->two[r] ? L->clubs_in[r - 1] / 2 : cp_ties_of(L, r - 1);
    return (win + L->cp_fresh[r]) / 2;
}

static void cl_paths(uint32_t cl, const Layout *L)
{
    /* The ties of round r: Champions Path ties 0 .. cp_ties[r] - 1, League Path ties after them. */
    int cp_ties[MAX_ROUNDS] = {0};
    for (int r = 0; r < L->rounds && !rt_unwinding; r++) {
        int ties = L->clubs_in[r] / 2;
        for (int t = 0; t < ties; t++) {
            uint32_t tie = tie_at(cl, r, t);
            for (int off = 0; off <= 0x12; off += 2) wr16(g_mem, tie + (uint32_t)off, 0xffffu);
            wr16(g_mem, tie + 8, 0);
        }
        Side w[2][64], f[2][64];
        int nw[2] = {0, 0}, nf[2] = {0, 0};
        int one_path = !L->two[r];
        if (r > 0) {
            int prev = L->clubs_in[r - 1] / 2;
            for (int t = 0; t < prev && t < 64; t++) {
                int p = one_path || t < cp_ties[r - 1] ? 0 : 1;
                w[p][nw[p]++] = (Side){-1, t};
            }
        }
        for (int i = 0; i < L->cp_fresh[r] && i < 64; i++) f[0][nf[0]++] = (Side){L->first[r] + i, -1};
        for (int i = 0; i < L->lp_fresh[r] && i < 64; i++) f[1][nf[1]++] = (Side){L->first[r] + L->cp_fresh[r] + i, -1};
        cp_ties[r] = pair_path(cl, r, 0, w[0], nw[0], f[0], nf[0]);
        pair_path(cl, r, cp_ties[r], w[1], nw[1], f[1], nf[1]);
    }
}

void o_00518790(void);

/* Cup::map_fixture_tree, thiscall () (vtable +0x94; 0x5838df, 0x584f14). The one wrap of 0x518790: slice 21c's
 * domestic cups with groups mark their later slots first (season_cup_groups_mapped, season_cups.h). */
RT_WRAP(00518790, cup_map_fixture_tree)
{
    uint32_t cup = cpu.ecx;
    season_cup_fill_entrants(cup);      /* gate fix O151: a domestic cup with empty places (season_cup_entrants.c) */
    if (rt_unwinding) return;
    cpu.ecx = cup;
    o_00518790();
    season_cup_groups_mapped(cup);
    if (rt_unwinding || !readable(cup) || rd32(g_mem, cup) != CL_VTABLE) return;
    const SeasonUefa *r = access_row_of(cup);
    Layout L;
    uint32_t t = rd32(g_mem, cup + 0xa3);
    if (!r) return;
    if (!make_layout(r, 'c', &L, cup) || !readable(t) || (int16_t)rd16(g_mem, cup + 0x3c) != L.rounds ||
        (int16_t)rd16(g_mem, cup + 0x3e) != L.slots || (int16_t)rd16(g_mem, t + 7) != CL_LEVELS[L.level[0]].round_code) {
        fprintf(stderr, "recomp: uefa: Champions League ties: the cup does not hold the access list layout (%d rounds, "
                "%d slots)\n", (int16_t)rd16(g_mem, cup + 0x3c), (int16_t)rd16(g_mem, cup + 0x3e));
        return;
    }
    Cpu saved = cpu;
    cl_paths(cup, &L);
    if (rt_unwinding) return;
    /* The group clubs after the first 16: +5 = 1, as a winner of the last round (0x51af97). */
    uint32_t e = rd32(g_mem, cup + 0xa7);
    for (int i = L.direct_first; i < L.slots; i++) g_mem[e + ENTRY * (uint32_t)i + 5] = 1;
    cpu = saved;
    fprintf(stderr, "recomp: uefa: Champions League ties of %d rounds in two paths\n", L.rounds);
}

/* ---- The nation draw inside each path (issue #68) ----
 *
 * Cup::draw_round 0x51c410 tests +0xb & 4 before +0xd & 0x200 (0x51c5d4..0x51c5f1): a round with the nation rule
 * goes to check_round_teams_for_nation 0x51d9f0 and the 0x200 keep is not reached. 0x51d9f0 (thiscall (round) ret
 * 4) takes every known side of every tie of the round out (marks -2, 0x51dc66, 0x51dd25; a winner to come, -1, stays
 * in its place), sorts them by the count of clubs of their nation (0x945333) and places each in a random tie with a
 * free side, first with no club of its nation on the other side, then without that test (0x51de39..0x51dfc1). It
 * writes the winner links of a moved winner's source tie (+6 tie, +8 1 home 0 away, +0x12 round; 0x51e056..
 * 0x51e17d). Over the whole round, a champion could go into a League Path tie and the reverse. Here, for a Champions
 * League qualifying round of two paths, the wrap runs the original twice: once on the Champions Path ties (0 ..
 * cp_ties_of - 1) and once on the League Path ties after them, by setting the round's tie array (+0xad) and tie count
 * (+0x1a) to the path for each call. So the nation rule of the original acts inside each path. Then it writes the
 * winner links again with the ties' real indices (the second call saw indices from 0). Other cups, rounds of one
 * path and the base game (no access list) keep the original (Q152). */
void o_0051d9f0(void);
static int nation_draw_inner;

RT_WRAP(0051d9f0, cup_nation_draw)
{
    uint32_t cup = cpu.ecx;
    int r = (int16_t)rt_arg(0);
    const SeasonUefa *row = !path_draw_off() && !nation_draw_inner && readable(cup) && rd32(g_mem, cup) == CL_VTABLE ? access_row_of(cup) : NULL;
    Layout L;
    if (!row || !make_layout(row, 'c', &L, cup) || r < 0 || r >= L.rounds || r >= MAX_ROUNDS || !L.two[r] ||
        (int16_t)rd16(g_mem, cup + 0x3c) != L.rounds || !readable(rd32(g_mem, cup + 0xa3)) ||
        !readable(rd32(g_mem, cup + 0xad))) {
        o_0051d9f0();
        return;
    }
    uint32_t rec = rd32(g_mem, cup + 0xa3) + ROUND_SIZE * (uint32_t)r, arr = rd32(g_mem, cup + 0xad) + 4u * (uint32_t)r;
    int ties = (int16_t)rd16(g_mem, rec + 0x1a), cp = cp_ties_of(&L, r);
    uint32_t base = rd32(g_mem, arr);
    if (ties != L.clubs_in[r] / 2 || cp <= 0 || cp >= ties || !readable(base)) {
        o_0051d9f0();
        return;
    }
    Cpu saved = cpu;
    uint32_t a[1] = {(uint32_t)r};
    nation_draw_inner = 1;
    for (int p = 0; p < 2 && !rt_unwinding; p++) {
        wr32(g_mem, arr, base + (p ? TIE_SIZE * (uint32_t)cp : 0));
        wr16(g_mem, rec + 0x1a, (uint16_t)(p ? ties - cp : cp));
        call_this(F_NATION_DRAW, cup, 1, a);
    }
    nation_draw_inner = 0;
    wr32(g_mem, arr, base);                     /* also on an unwind: the saved tie array and count of the round */
    wr16(g_mem, rec + 0x1a, (uint16_t)ties);
    if (rt_unwinding) return;
    for (int t = 0; t < ties; t++) {
        uint32_t tie = base + TIE_SIZE * (uint32_t)t;
        for (int h = 1; h >= 0; h--) {
            int16_t slot = (int16_t)rd16(g_mem, tie + (h ? 0 : 2));
            int16_t src_r = (int16_t)rd16(g_mem, tie + (h ? 0xe : 0x10)), src_t = (int16_t)rd16(g_mem, tie + (h ? 0xa : 0xc));
            if (slot < 0 || src_r < 0 || src_r >= r || src_t < 0 || src_t >= L.clubs_in[src_r] / 2) continue;
            uint32_t from = tie_at(cup, src_r, src_t);
            wr16(g_mem, from + 6, (uint16_t)t);
            wr16(g_mem, from + 8, (uint16_t)h);
            wr16(g_mem, from + 0x12, (uint16_t)r);
        }
    }
    cpu = saved;
    rt_ret_stdcall(0, 1);
    fprintf(stderr, "recomp: uefa: Champions League round code %d drawn in two paths (%d and %d ties), the nation rule "
                    "inside each\n", CL_LEVELS[L.level[r]].round_code, cp, ties - cp);
}

/* ---- Europa League ---- */

/* The entry table and list of the Europa League or (slice 24) the Conference League, comp 'e' or 'k' (from
 * season_uefa_cup.c's wrap of build_entrants 0x902b90). Returns 1 when it made them. */
static int cup_entrants(uint32_t uc, char comp)
{
    const SeasonUefa *r = access_row_of(uc);
    Layout L;
    int levels, g = group_code(comp);
    const Level *lv = levels_of(comp, &levels);
    if (!r || !make_layout(r, comp, &L, 0)) return 0;
    uint32_t e = game_malloc(ENTRY * (uint32_t)L.slots), list = e ? game_malloc(ENTRY * (uint32_t)L.slots) : 0;
    if (!list) return 0;
    game_free(rd32(g_mem, uc + 0xa7));
    game_free(rd32(g_mem, uc + 0x14));
    if (rt_unwinding) return 0;
    int slot = 0;
    for (int i = 0; i < L.rounds; i++) {
        int code = lv[L.level[i]].cp, k = list_index_of_code(&L, code);
        for (int j = 0; j < L.count[code]; j++, slot++) {
            wr32(g_mem, e + ENTRY * (uint32_t)slot, L.clubs[k + j]);
            g_mem[e + ENTRY * (uint32_t)slot + 4] = (uint8_t)(i + 1);
        }
        for (int j = 0; j < L.drops[code]; j++, slot++) g_mem[e + ENTRY * (uint32_t)slot + 4] = (uint8_t)(i + 2);
    }
    int n = 0, k = list_index_of_code(&L, g);
    for (int j = 0; j < L.count[g]; j++, slot++) {
        wr32(g_mem, e + ENTRY * (uint32_t)slot, L.clubs[k + j]);
        wr32(g_mem, list + ENTRY * (uint32_t)n, L.clubs[k + j]);
        g_mem[list + ENTRY * (uint32_t)n + 4] = 3;                      /* as the first round clubs of 0x902deb */
        n++;
    }
    wr32(g_mem, uc + 0xa7, e);
    wr32(g_mem, uc + 0x14, list);
    wr16(g_mem, uc + 0x36, (uint16_t)n);
    wr16(g_mem, uc + 0x38, (uint16_t)L.slots);
    wr16(g_mem, uc + 0x3e, (uint16_t)L.slots);
    g_mem[uc + 0xb2] = 0;
    fprintf(stderr, "recomp: uefa: %s %d/%02d: %d entrants, %d direct, %d qualifying rounds, %d slots\n", name_of(comp),
            (int16_t)rd16(g_mem, uc + 0x40), ((int16_t)rd16(g_mem, uc + 0x40) + 1) % 100, L.n, L.count[g], L.rounds,
            L.slots);
    return 1;
}

int season_uefa_uc_entrants(uint32_t uc) { return cup_entrants(uc, 'e'); }
int season_uefa_ecl_entrants(uint32_t ecl) { return cup_entrants(ecl, 'k'); }

/* The round table of stage -1 of the Europa League (from season_uefa_cup.c's wrap of 0x9025c0), made from the
 * record rec of the original's table (its qualifying round). Seeded draws (0x400; 0x51d2d0: the new clubs have
 * the higher tier) that avoid two clubs of one nation (+0xb 4). Returns the table or 0. */
static uint32_t cup_qual_table(uint32_t uc, char comp, uint32_t rec, int *rounds_out)
{
    const SeasonUefa *r = access_row_of(uc);
    Layout L;
    int levels;
    const Level *lvs = levels_of(comp, &levels);
    if (!r || !readable(rec) || !make_layout(r, comp, &L, 0) || (int16_t)rd16(g_mem, uc + 0x3e) != L.slots) return 0;
    uint32_t t = game_malloc(ROUND_SIZE * (uint32_t)L.rounds);
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, uc + 0x40);
    for (int i = 0; t && i < L.rounds && !rt_unwinding; i++) {
        uint32_t x = t + ROUND_SIZE * (uint32_t)i;
        memcpy(g_mem + x, g_mem + rec, ROUND_SIZE);
        const Level *lv = &lvs[L.level[i]];
        wr16(g_mem, x + 7, (uint16_t)lv->round_code);
        wr16(g_mem, x + 9, (uint16_t)i);
        wr16(g_mem, x + 0xd, i ? 0x400 : 0);
        g_mem[x + 0xb] = 4;
        wr16(g_mem, x + 0x18, (uint16_t)L.clubs_in[i]);
        wr16(g_mem, x + 0x1a, (uint16_t)(L.clubs_in[i] / 2));
        wr16(g_mem, x + 0x1c, (uint16_t)L.fresh[i]);
        wr16(g_mem, x + 0x1e, (uint16_t)L.first[i]);
        g_mem[x + 0x21] = (uint8_t)lv->legs;
        g_mem[x + 0x22] = (uint8_t)lv->gap;
        int d[4] = {lv->draw[0], lv->draw[1], lv->match[0], lv->match[1]};
        run_dates(comp, r, lv->round_code, (int)year, d);
        set_dates(t, i, d[0], d[1], d[2], d[3], lv->weekday, year);
    }
    if (rt_unwinding || !t) return 0;
    season_uefa_no_away_goals(t, L.rounds, (int)year);
    *rounds_out = L.rounds;
    return t;
}

uint32_t season_uefa_uc_qual_table(uint32_t uc, uint32_t rec, int *rounds) { return cup_qual_table(uc, 'e', rec, rounds); }
uint32_t season_uefa_ecl_qual_table(uint32_t ecl, uint32_t rec, int *rounds) { return cup_qual_table(ecl, 'k', rec, rounds); }

/* The layout of the Europa League from its round table +0xa3 (+0x3c rounds), once the season has started: its
 * clubs lose their id at +0x1db when they go out, so make_layout cannot count them any more. Round i takes
 * +0x1c new slots from +0x1e. The last of them are the drop slots of the row. Returns 0 when the table is not
 * one of the access list (a round code that is not of EL_LEVELS, slots that do not follow each other). Slice 24:
 * comp 'k' gives the Conference League's from ECL_LEVELS; any_stage 1 reads the table after the group draw too,
 * else the cup must be before its groups (+0x30 -1). */
static int table_layout(uint32_t uc, const SeasonUefa *r, Layout *L, char comp, int any_stage)
{
    int levels;
    const Level *lvs = levels_of(comp, &levels);
    memset(L, 0, sizeof *L);
    uint32_t t = rd32(g_mem, uc + 0xa3);
    int rounds = (int16_t)rd16(g_mem, uc + 0x3c), slot = 0, k = 0;
    if (!readable(t) || rounds < 1 || rounds > MAX_ROUNDS || (!any_stage && (int32_t)rd32(g_mem, uc + 0x30) != -1))
        return 0;
    for (int i = 0; i < rounds; i++) {
        uint32_t rec = t + ROUND_SIZE * (uint32_t)i;
        int code = (int16_t)rd16(g_mem, rec + 7);
        while (k < levels && lvs[k].round_code != code) k++;
        if (k == levels || (int16_t)rd16(g_mem, rec + 0x1e) != slot) return 0;
        int lv = lvs[k].cp, fresh = (int16_t)rd16(g_mem, rec + 0x1c);
        L->level[i] = k, L->first[i] = slot, L->fresh[i] = fresh, L->cp_fresh[i] = fresh;
        L->clubs_in[i] = (int16_t)rd16(g_mem, rec + 0x18);
        L->drops[lv] = season_uefa_dropped(r, lv);
        L->count[lv] = fresh - L->drops[lv];
        slot += fresh;
        k++;
    }
    L->rounds = rounds;
    L->direct_first = slot;
    L->slots = (int16_t)rd16(g_mem, uc + 0x3e);
    L->drops[group_code(comp)] = season_uefa_dropped(r, group_code(comp));
    if (comp == 'k') {                  /* the group clubs: the slots after the rounds that hold a club */
        uint32_t e = rd32(g_mem, uc + 0xa7);
        for (int i = slot; readable(e) && i < L->slots - THIRDS; i++) {
            uint32_t club = rd32(g_mem, e + ENTRY * (uint32_t)i);
            L->count[SEASON_UEFA_ECL_G] += readable(club) && club_code(club) == SEASON_UEFA_ECL_G;
        }
        return L->slots >= slot + THIRDS;
    }
    return L->slots >= slot + HANDOVER + THIRDS;
}

static int uc_table_layout(uint32_t uc, const SeasonUefa *r, Layout *L, char comp)
{
    return table_layout(uc, r, L, comp, 0);
}

/* The first slot after the rounds of the Europa League (the group clubs, the hand-over, the thirds) or of the
 * Conference League, or -1. */
int season_uefa_uc_direct_first(uint32_t uc)
{
    const SeasonUefa *r = access_row_of(uc);
    Layout L;
    return r && uc_table_layout(uc, r, &L, 'e') ? L.direct_first : -1;
}

int season_uefa_ecl_direct_first(uint32_t ecl)
{
    const SeasonUefa *r = access_row_of(ecl);
    Layout L;
    return r && uc_table_layout(ecl, r, &L, 'k') ? L.direct_first : -1;
}

/* ---- The Champions League losers into the Europa League ---- */

/* The layout of the Champions League from its round table +0xa3 (+0x3c rounds) and its entry table +0xa7, after
 * build_entrants: both are in the save (Cup::save_cup_to_disk 0x5210f0), and the clubs keep their entry codes
 * (+0x1df), so a loaded game gives the same layout. Round i takes +0x1c new slots from +0x1e. The League Path
 * clubs of a round are those with a League Path code. Returns 0 when the table is not one of the access list. */
static int cl_table_layout(uint32_t cl, Layout *L)
{
    memset(L, 0, sizeof *L);
    uint32_t t = rd32(g_mem, cl + 0xa3), e = rd32(g_mem, cl + 0xa7);
    int rounds = (int16_t)rd16(g_mem, cl + 0x3c), slot = 0, k = 0, lp_win = 0;
    if (!readable(t) || !readable(e) || rounds < 1 || rounds > MAX_ROUNDS) return 0;
    for (int i = 0; i < rounds; i++) {
        uint32_t rec = t + ROUND_SIZE * (uint32_t)i;
        int code = (int16_t)rd16(g_mem, rec + 7);
        while (k < CL_N && CL_LEVELS[k].round_code != code) k++;
        if (k == CL_N || (int16_t)rd16(g_mem, rec + 0x1e) != slot) return 0;
        int fresh = (int16_t)rd16(g_mem, rec + 0x1c), lp = 0;
        for (int j = slot; j < slot + fresh; j++) {
            uint32_t club = rd32(g_mem, e + ENTRY * (uint32_t)j);
            int c = readable(club) ? club_code(club) : 0;
            lp += CL_LEVELS[k].lp && c == CL_LEVELS[k].lp;
        }
        L->level[i] = k, L->first[i] = slot, L->fresh[i] = fresh, L->cp_fresh[i] = fresh - lp, L->lp_fresh[i] = lp;
        L->clubs_in[i] = (int16_t)rd16(g_mem, rec + 0x18);
        /* The paths as make_layout has them: the League Path winners stay in their path (issue #68). */
        if (!lp && path_draw_off()) lp_win = 0;
        L->two[i] = lp + lp_win > 0;
        lp_win = L->two[i] ? (lp + lp_win) / 2 : 0;
        slot += fresh;
        k++;
    }
    L->rounds = rounds;
    L->direct_first = slot;
    L->slots = (int16_t)rd16(g_mem, cl + 0x3e);
    return L->slots >= slot;
}

/* The round of the Champions League layout at level k, or -1. */
static int cl_round_of_level(const Layout *L, int k)
{
    for (int i = 0; i < L->rounds; i++)
        if (L->level[i] == k) return i;
    return -1;
}

/* The losers of the decided ties of Champions League round r and path (0 Champions Path, 1 League Path, 2
 * both), in tie order, into out (max). A tie is decided when its winner is in the tie of the next round
 * (+6/+8/+0x12). The loser is the other side. Returns the count, or -1 when a tie is not decided yet. */
static int cl_losers(uint32_t cl, const Layout *L, int r, int path, uint32_t *out, int max)
{
    uint32_t e = rd32(g_mem, cl + 0xa7);
    int ties = L->clubs_in[r] / 2, n = 0;
    int cp_ties = cp_ties_of(L, r);
    for (int t = 0; t < ties; t++) {
        if (path == 0 && t >= cp_ties) continue;
        if (path == 1 && t < cp_ties) continue;
        uint32_t tie = tie_at(cl, r, t);
        int16_t home = (int16_t)rd16(g_mem, tie), away = (int16_t)rd16(g_mem, tie + 2);
        int16_t next_t = (int16_t)rd16(g_mem, tie + 6), side = (int16_t)rd16(g_mem, tie + 8),
                next_r = (int16_t)rd16(g_mem, tie + 0x12);
        if (home < 0 || away < 0) return -1;
        int16_t win;
        if (next_t >= 0 && next_r > r) {
            win = (int16_t)rd16(g_mem, tie_at(cl, next_r, next_t) + (side ? 0 : 2));
            if (win < 0) return -1;
        } else {                                                  /* the last round: +5 1 the winner (0x51af97) */
            int8_t s = (int8_t)g_mem[e + ENTRY * (uint32_t)home + 5];
            if (s != 1 && s != 2) return -1;
            win = s == 1 ? home : away;
        }
        int16_t lose = win == home ? away : home;
        if (n < max) out[n] = rd32(g_mem, e + ENTRY * (uint32_t)lose);
        n++;
    }
    return n;
}

/* The source of a drop ("Q3-CP"): its Champions League level (index of CL_LEVELS) and path (0 Champions Path,
 * 1 League Path, 2 both), or -1. */
#define LEVEL_PR 0
#define LEVEL_PR_FINAL 1
#define LEVEL_PO 5
static int drop_source(const char *tok, int n, int *path)
{
    static const struct { const char *name; int level; } names[] = {
        {"PR", LEVEL_PR}, {"Q1", 2}, {"Q2", 3}, {"Q3", 4}, {"PO", LEVEL_PO}};
    *path = n == 5 && !strncmp(tok + 2, "-CP", 3) ? 0 : n == 5 && !strncmp(tok + 2, "-LP", 3) ? 1 : 2;
    for (unsigned k = 0; k < sizeof names / sizeof names[0]; k++)
        if (n >= 2 && !strncmp(tok, names[k].name, 2)) return names[k].level;
    return -1;
}

/* The losers that Champions League round r gives in a path: one a tie. */
static int expected_losers(const Layout *C, int r, int path)
{
    int ties = C->clubs_in[r] / 2, cp = cp_ties_of(C, r);
    return path == 2 ? ties : path == 0 ? cp : ties - cp;
}

/* Puts club into slot of the cup of comp ('e' or 'k'), as the hand-over 0x585a32..0x585a6a does: the entry, the
 * list +0x14 (+0x36 of +0x38) and the cup's id at +0x1db. A slot that holds a club is left as it is. Returns 1
 * when it put the club. */
static int put_club(uint32_t uc, char comp, int slot, uint32_t club)
{
    uint32_t e = rd32(g_mem, uc + 0xa7), list = rd32(g_mem, uc + 0x14);
    int count = (int16_t)rd16(g_mem, uc + 0x3e);
    if (slot < 0 || slot >= count || !readable(club) || !readable(e)) return 0;
    uint32_t at = e + ENTRY * (uint32_t)slot;
    if (readable(rd32(g_mem, at))) return 0;
    wr32(g_mem, at, club);
    g_mem[at + 5] = 0;
    int16_t m = (int16_t)rd16(g_mem, uc + 0x36);
    if (m < (int16_t)rd16(g_mem, uc + 0x38) && readable(list)) {
        wr32(g_mem, list + ENTRY * (uint32_t)m, club);
        g_mem[list + ENTRY * (uint32_t)m + 4] = 5;
        wr16(g_mem, uc + 0x36, (uint16_t)(m + 1));
    }
    wr32(g_mem, club + 0x1db, id_of(comp));
    return 1;
}

/* Puts the losers of Champions League round cr (path) into the slots of the cup of comp ('e' the Europa League,
 * 'k' the Conference League) from first (down from first when down is 1). */
static void put_losers(uint32_t uc, char comp, uint32_t cl, const Layout *C, int cr, int path, int first, int down)
{
    uint32_t losers[64];
    int n = cl_losers(cl, C, cr, path, losers, 64), moved = 0;
    for (int k = 0; k < n && k < 64; k++) moved += put_club(uc, comp, down ? first - k : first + k, losers[k]);
    if (moved)
        fprintf(stderr, "recomp: uefa: %d losers of Champions League round %d (path %d) into %s slots from %d\n", moved,
                CL_LEVELS[C->level[cr]].round_code, path, name_of(comp), first);
}

/* The losers of each decided Champions League round go into their Europa League slots (the drops of the row).
 * The slots of each source follow those of the sources before it to the same round, by the losers each gives
 * (expected_losers), so the order does not depend on which round ends first. The play-off losers that go to the
 * groups come with the original's hand-over (0x585a02) into the slots from count - 0x18. Other losers for the
 * groups (a League Path third qualifying round, 2018/19 on) take the slots from count - 9 down, so the two never
 * meet. The preliminary round gives the losers of its semi-finals and of its final. */
static void uc_fill_drops(uint32_t uc)
{
    const SeasonUefa *r = access_row_of(uc);
    uint32_t cl = comp_object(KEY_CL, CL_VTABLE);
    Layout L, C;
    if (!r || !cl || !r->drops || !*r->drops || !access_row_of(cl) || !uc_table_layout(uc, r, &L, 'e') ||
        !cl_table_layout(cl, &C))
        return;
    int used[SEASON_UEFA_CODE_SLOTS] = {0}, count = (int16_t)rd16(g_mem, uc + 0x3e);
    for (const char *p = r->drops; *p && !rt_unwinding;) {
        while (*p == ' ') p++;
        const char *gt = strchr(p, '>');
        if (!gt) break;
        const char *end = gt + 1;
        while (*end && *end != ' ') end++;
        int path, level = drop_source(p, (int)(gt - p), &path);
        int to = season_uefa_code('e', gt + 1, (int)(end - gt - 1));
        p = end;
        if (level < 0 || to <= 0 || (to == SEASON_UEFA_EL_G && level == LEVEL_PO)) continue;   /* PO>G: the hand-over */
        int first;
        if (to == SEASON_UEFA_EL_G) {
            first = count - THIRDS - 1;                           /* from count - 9 down */
        } else {
            int i = 0;
            while (i < L.rounds && EL_LEVELS[L.level[i]].cp != to) i++;
            if (i == L.rounds) continue;
            first = L.first[i] + L.count[to];
        }
        int sources[2] = {cl_round_of_level(&C, level), level == LEVEL_PR ? cl_round_of_level(&C, LEVEL_PR_FINAL) : -1};
        for (int k = 0; k < 2 && !rt_unwinding; k++) {
            if (sources[k] < 0) continue;
            int down = to == SEASON_UEFA_EL_G;
            put_losers(uc, 'e', cl, &C, sources[k], path, down ? first - used[to] : first + used[to], down);
            used[to] += expected_losers(&C, sources[k], path);
        }
    }
}

/* For season_uefa_cup.c: the drops before the group draw. */
void season_uefa_uc_fill_drops(uint32_t uc) { uc_fill_drops(uc); }

/* ---- Slice 24: the losers of the other two cups into the Conference League ---- */

/* 1 when slot is a side of a tie of round r of the cup. */
static int in_round(uint32_t cup, const Layout *L, int r, int slot)
{
    for (int t = 0; r < L->rounds && t < L->clubs_in[r] / 2; t++) {
        uint32_t tie = tie_at(cup, r, t);
        if ((int16_t)rd16(g_mem, tie) == slot || (int16_t)rd16(g_mem, tie + 2) == slot) return 1;
    }
    return 0;
}

/* The loser of tie t of Europa League round r, or 0 while the tie goes on: the side whose entry byte +5 is -1
 * (0x51b5cd) and that is in no tie of round r + 1 (a winner of round r that lost later is in one). In the last
 * round the winner gets +5 = 1 (0x51af97, as cl_losers reads it), so the loser is the other side. */
static uint32_t el_loser(uint32_t el, const Layout *E, int r, int t)
{
    uint32_t e = rd32(g_mem, el + 0xa7), tie = tie_at(el, r, t);
    if (r == E->rounds - 1 && readable(e)) {
        int16_t home = (int16_t)rd16(g_mem, tie), away = (int16_t)rd16(g_mem, tie + 2);
        if (home < 0 || away < 0) return 0;
        int8_t h5 = (int8_t)g_mem[e + ENTRY * (uint32_t)home + 5], a5 = (int8_t)g_mem[e + ENTRY * (uint32_t)away + 5];
        if (h5 == 1 && a5 != 1) return rd32(g_mem, e + ENTRY * (uint32_t)away);
        if (a5 == 1 && h5 != 1) return rd32(g_mem, e + ENTRY * (uint32_t)home);
        return 0;
    }
    for (int h = 0; h < 2 && readable(e); h++) {
        int16_t side = (int16_t)rd16(g_mem, tie + (h ? 0 : 2));
        if (side < 0 || (int8_t)g_mem[e + ENTRY * (uint32_t)side + 5] != -1) continue;
        if (r + 1 < E->rounds && in_round(el, E, r + 1, side)) continue;
        return rd32(g_mem, e + ENTRY * (uint32_t)side);
    }
    return 0;
}

/* The round of a layout whose level has round code code, or -1. */
static int round_of_code(const Layout *L, const Level *lvs, int code)
{
    for (int i = 0; i < L->rounds; i++)
        if (lvs[L->level[i]].round_code == code) return i;
    return -1;
}

/* The drops of the row's ecl_drops ("c:PR>Q2 c:Q1>Q2 e:Q3>PO e:PO>G"). The slots of a round: its new clubs, then
 * the losers of each source in the order of the row, each source with the slots of the losers it gives, so the
 * order does not depend on which round ends first. The group slots follow the group clubs. A Champions League
 * round gives its losers when all its ties end (cl_losers), a Europa League round each loser when its tie ends,
 * in the slot of its tie. */
static void ecl_fill_drops(uint32_t ecl)
{
    const SeasonUefa *r = access_row_of(ecl);
    uint32_t cl = comp_object(KEY_CL, CL_VTABLE), el = comp_object(KEY_UC, UC_VTABLE);
    Layout K, C, E;
    if (!r || !r->ecl_drops || !*r->ecl_drops || !uc_table_layout(ecl, r, &K, 'k')) return;
    int have_c = cl && access_row_of(cl) && cl_table_layout(cl, &C);
    /* The Europa League's round table +0xa3, its ties and entry table stay until its start_new_season frees them
     * (0x902ee0), so its losers can be read after its group draw too: its play-off losers on the day it draws. */
    int have_e = el && access_row_of(el) && table_layout(el, r, &E, 'e', 1);
    int used[SEASON_UEFA_CODE_SLOTS] = {0};
    for (const char *p = r->ecl_drops; *p && !rt_unwinding;) {
        while (*p == ' ') p++;
        const char *gt = strchr(p, '>');
        if (!gt || gt - p < 3 || p[1] != ':') break;
        const char *end = gt + 1;
        while (*end && *end != ' ') end++;
        char from = p[0];
        int path, level = from == 'c' ? drop_source(p + 2, (int)(gt - p - 2), &path) : -1;
        int to = season_uefa_code('k', gt + 1, (int)(end - gt - 1));
        int el_code = from == 'e' ? season_uefa_code('e', p + 2, (int)(gt - p - 2)) : 0;
        p = end;
        if (to <= 0) continue;
        int first;
        if (to == SEASON_UEFA_ECL_G) {
            first = K.direct_first + K.count[SEASON_UEFA_ECL_G];
        } else {
            int i = 0;
            while (i < K.rounds && ECL_LEVELS[K.level[i]].cp != to) i++;
            if (i == K.rounds) continue;
            first = K.first[i] + K.count[to];
        }
        if (from == 'c' && level >= 0 && have_c) {
            int sources[2] = {cl_round_of_level(&C, level), level == LEVEL_PR ? cl_round_of_level(&C, LEVEL_PR_FINAL) : -1};
            for (int k = 0; k < 2 && !rt_unwinding; k++) {
                if (sources[k] < 0) continue;
                put_losers(ecl, 'k', cl, &C, sources[k], path, first + used[to], 0);
                used[to] += expected_losers(&C, sources[k], path);
            }
        } else if (from == 'e' && el_code > 0 && have_e) {
            int code = 0;
            for (int k = 0; k < EL_N; k++)
                if (EL_LEVELS[k].cp == el_code) code = EL_LEVELS[k].round_code;
            int er = round_of_code(&E, EL_LEVELS, code), moved = 0;
            if (er < 0) continue;
            int ties = E.clubs_in[er] / 2;
            for (int t = 0; t < ties && !rt_unwinding; t++) {
                uint32_t loser = el_loser(el, &E, er, t);
                if (loser && rd32(g_mem, loser + 0x1db) == 0xffffffffu)
                    moved += put_club(ecl, 'k', first + used[to] + t, loser);
            }
            used[to] += ties;
            if (moved)
                fprintf(stderr, "recomp: uefa: %d losers of Europa League round %d into Conference League slots from %d\n",
                        moved, code, first + used[to] - ties);
        }
    }
}

void season_uefa_ecl_fill_drops(uint32_t ecl) { ecl_fill_drops(ecl); }

void o_0051f890(void);


/* Test only (RECOMP_UEFA_LOG=1): each week, the state of a cup's qualifying rounds: the current stage +0x30,
 * the next round +0xab, the rounds set up +0xac, +0xb1, and for each round its ties by state (+0x34: 0 not
 * drawn, 1 drawn, 2 ready, 3 fixtures made) and its sides still to come (-1). The entry bytes +5 of the slots. */
static void log_cup(const char *name, uint32_t cup)
{
    uint32_t t = rd32(g_mem, cup + 0xa3), e = rd32(g_mem, cup + 0xa7);
    int rounds = (int16_t)rd16(g_mem, cup + 0x3c), slots = (int16_t)rd16(g_mem, cup + 0x3e);
    fprintf(stderr, "recomp: uefa log %s day %d: stage %d next %d set %d done %d slots %d", name,
            (int16_t)rd16(g_mem, 0xae2c90u), (int32_t)rd32(g_mem, cup + 0x30), g_mem[cup + 0xab], g_mem[cup + 0xac],
            g_mem[cup + 0xb1], slots);
    for (int r = 0; r < rounds && r < MAX_ROUNDS && readable(t); r++) {
        uint32_t rec = t + ROUND_SIZE * (uint32_t)r;
        int ties = (int16_t)rd16(g_mem, rec + 0x1a), st[4] = {0}, open = 0, empty = 0;
        for (int k = 0; k < ties; k++) {
            uint32_t tie = tie_at(cup, r, k);
            if (g_mem[tie + 0x34] < 4) st[g_mem[tie + 0x34]]++;
            for (int h = 0; h < 2; h++) {
                int16_t side = (int16_t)rd16(g_mem, tie + (h ? 0 : 2));
                if (side < 0) open++;
                else if (readable(e) && !readable(rd32(g_mem, e + ENTRY * (uint32_t)side))) empty++;
            }
        }
        fprintf(stderr, " | r%d code %d draw %d/%d match %d/%d ties %d st %d/%d/%d/%d open %d empty %d", r,
                (int16_t)rd16(g_mem, rec + 7), (int16_t)rd16(g_mem, rec), g_mem[rec + 2], (int16_t)rd16(g_mem, rec + 3),
                g_mem[rec + 5], ties, st[0], st[1], st[2], st[3], open, empty);
    }
    int h[4] = {0};
    for (int i = 0; i < slots && readable(e); i++) {
        int8_t b5 = (int8_t)g_mem[e + ENTRY * (uint32_t)i + 5];
        h[b5 == -1 ? 0 : b5 == 0 ? 1 : b5 == 1 ? 2 : 3]++;
    }
    fprintf(stderr, " | +5: out %d in %d won %d other %d\n", h[0], h[1], h[2], h[3]);
}

static void log_week(void)
{
    static int last = -1;
    int day = (int16_t)rd16(g_mem, 0xae2c90u);
    if (!getenv("RECOMP_UEFA_LOG") || day == last || day % 7) return;
    last = day;
    uint32_t cl = comp_object(KEY_CL, CL_VTABLE), uc = comp_object(KEY_UC, UC_VTABLE);
    if (cl) log_cup("CL", cl);
    if (uc) log_cup("EL", uc);
    uint32_t ecl = season_uefa_ecl_object();
    if (season_uefa_cup_kind(ecl) == 'k') log_cup("ECL", ecl);
}

/* Test log (RECOMP_UEFA_TIES=1, issue #68): each Champions League qualifying tie of a round with two paths, each
 * day that its sides or its state (+0x34) change, once both sides are known: the tie's path by its index
 * (cp_ties_of) and each club's path by its entry slot (C a new champion of a round, L another; a winner keeps its
 * slot), and "mixed" when they differ. */
static int slot_path(const Layout *L, int s)
{
    for (int k = 0; k < L->rounds; k++) {
        if (s >= L->first[k] && s < L->first[k] + L->cp_fresh[k]) return 0;
        if (s >= L->first[k] + L->cp_fresh[k] && s < L->first[k] + L->fresh[k]) return 1;
    }
    return -1;
}

static void log_cl_ties(void)
{
    static int seen_year = -1;
    static uint32_t seen[MAX_ROUNDS][64];
    if (!getenv("RECOMP_UEFA_TIES")) return;
    uint32_t cl = comp_object(KEY_CL, CL_VTABLE);
    const SeasonUefa *row = cl ? access_row_of(cl) : NULL;
    Layout L;
    if (!row || !make_layout(row, 'c', &L, cl)) return;
    int year = (int16_t)rd16(g_mem, cl + 0x40), day = (int16_t)rd16(g_mem, 0xae2c90u);
    if (year != seen_year) {
        memset(seen, 0, sizeof seen);
        seen_year = year;
    }
    uint32_t e = rd32(g_mem, cl + 0xa7);
    if (!readable(e) || !readable(rd32(g_mem, cl + 0xad))) return;
    int split = 0;
    for (int r = 0; r < L.rounds && r < MAX_ROUNDS; r++) {
        split |= L.two[r];
        if (!split) continue;           /* a round after the split with one path (the switch off): the clubs show it */
        int ties = L.clubs_in[r] / 2, cp = cp_ties_of(&L, r);
        for (int t = 0; t < ties && t < 64; t++) {
            uint32_t tie = tie_at(cl, r, t);
            int16_t a = (int16_t)rd16(g_mem, tie), b = (int16_t)rd16(g_mem, tie + 2);
            uint32_t key = ((uint32_t)(uint16_t)a << 16 | (uint16_t)b) ^ ((uint32_t)g_mem[tie + 0x34] << 30) ^ 1u;
            if (seen[r][t] == key || a < 0 || b < 0 || a >= L.slots || b >= L.slots) continue;
            seen[r][t] = key;
            uint32_t ca = rd32(g_mem, e + ENTRY * (uint32_t)a), cb = rd32(g_mem, e + ENTRY * (uint32_t)b);
            int pa = slot_path(&L, a), pb = slot_path(&L, b), pt = t < cp ? 0 : 1;
            fprintf(stderr, "recomp: uefa: test: CL %d/%02d day %d round code %d tie %d (%s path, state %d): %.30s (%s, "
                            "nation %u) v %.30s (%s, nation %u)%s\n", year, (year + 1) % 100, day,
                    CL_LEVELS[L.level[r]].round_code, t, pt ? "League" : "Champions", g_mem[tie + 0x34],
                    readable(ca) ? win_str(ca + 4) : "?", pa ? "L" : "C", readable(ca) ? rd32(g_mem, ca + 0x53) : 0,
                    readable(cb) ? win_str(cb + 4) : "?", pb ? "L" : "C", readable(cb) ? rd32(g_mem, cb + 0x53) : 0,
                    pa != pt || pb != pt ? " mixed" : "");
        }
    }
}

/* ---- A round that is drawn after the date of its first leg (box Cup.cpp 1278, 2018/19) ----
 *
 * Cup::draw_round 0x51c410 waits for a round with an empty slot: it puts the draw date of the round (+0) a day
 * on, each day (0x51c4e9, date function 0x549ef0), and leaves the date of the first leg (+3, +5). The slots of a
 * Europa League round fill from the Champions League rounds that feed it (put_losers), and a Champions League round
 * that ends late fills them late. When the draw then comes, the first leg date is past: Cup::create_tie_fixtures
 * 0x51cb90 makes no fixture for a leg before today, and Cup::process_fixture 0x51a150 shows the box at 0x51a672
 * (line 0x4fe) when the first leg is played and find_next_fixture 0x5ac920 (call at 0x51a610) finds no second leg.
 * Here, each day before the cup's own update, a round with no drawn tie whose draw is due (the next day at the
 * latest) and whose first leg is less than 4 days away gets its first leg a week from today, on its match day
 * (cup_set_round_match_date 0x521eb0, as the table is made). The second leg follows by the gap of the round. A round
 * on time is not touched. Row A18 of the 0.6.0 release test found two causes in the Champions League preliminary
 * round, both fixed at the source: a level single match was replayed (the round rule, where the table is made), and
 * its dates broke the 3-day rule of the dates block. What is left is a tie that the game itself moves a week, when a
 * club of it already has a match on its day (seen: Steaua Bucharest v Liverpool, 14 August 2019, an England game);
 * this logged net covers that case. */
static void doy_to_dm(int doy, int year, int *day, int *month)
{
    static const int len[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int m = 0;
    for (; m < 11; m++) {
        int n = len[m] + (m == 1 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0));
        if (doy < n) break;
        doy -= n;
    }
    *day = doy + 1;
    *month = m;
}

static void cup_dates_ahead(uint32_t cup)
{
    if (getenv("RECOMP_UEFA_NO_LATE_MOVE")) return;          /* test only: the retest of row A18 without this net */
    uint32_t t = rd32(g_mem, cup + 0xa3);
    int rounds = (int16_t)rd16(g_mem, cup + 0x3c), today = (int16_t)rd16(g_mem, 0xae2c90u);
    int year = (int16_t)rd16(g_mem, cup + 0x40);
    if (!readable(t) || rounds < 1 || rounds > MAX_ROUNDS || (int16_t)rd16(g_mem, 0xae2c92u) != year) return;
    for (int r = 0; r < rounds && !rt_unwinding; r++) {
        uint32_t rec = t + ROUND_SIZE * (uint32_t)r;
        int ties = (int16_t)rd16(g_mem, rec + 0x1a), code = (int16_t)rd16(g_mem, rec + 7), undrawn = ties > 0;
        for (int k = 0; undrawn && k < ties; k++)
            if (g_mem[tie_at(cup, r, k) + 0x34] != 0) undrawn = 0;
        if (!undrawn || g_mem[rec + 2] != 0 || g_mem[rec + 5] != 0 || code == 240) continue;
        int draw = (int16_t)rd16(g_mem, rec), match = (int16_t)rd16(g_mem, rec + 3);
        if (draw > today + 1 || match >= today + 4) continue;
        int day, month;
        doy_to_dm(today + 7, year, &day, &month);
        uint32_t m[9] = {t, (uint32_t)r, (uint32_t)day, (uint32_t)month, 0, 3, 2, (uint32_t)year, 0};
        call_cdecl(F_MATCH_DATE, 9, m);
        fprintf(stderr, "recomp: uefa: %s round %d (code %d) is drawn late: its first leg moves from day %d to day %d of %d\n",
                season_uefa_cup_kind(cup) == 'e' ? "EL" : "ECL", r, code, match, today + 7, year);
    }
}

/* Cup::update_for_date, thiscall (slot 5 of the UEFA Cup, 0x9707a4): the drops before the draws of the day. */
RT_WRAP(0051f890, cup_update_for_date)
{
    uint32_t cup = cpu.ecx;
    season_super_cups_trace(cup, rt_arg(0));
    int kind = season_uefa_cup_kind(cup);
    if (kind && access_row_of(cup)) {
        Cpu saved = cpu;
        if (kind == 'e') log_week();
        if (kind == 'e') log_cl_ties();
        if (kind == 'e') uc_fill_drops(cup);
        else ecl_fill_drops(cup);
        if (!rt_unwinding) cup_dates_ahead(cup);
        if (rt_unwinding) return;
        cpu = saved;
    }
    o_0051f890();
}

/* ---- The Intertoto Cup that is not held ----
 *
 * The constructor of Intertoto_Cup (0x632fb0) makes a whole edition at a new game, and build_entrants 0x6336c0
 * shows a box with fewer than 60 clubs (0x6336d7, 0x6337a0). In a season with no Intertoto Cup (uic_clubs -1
 * in the row) the seeding gives none, so the wrap of 0x6336c0 returns 1 with no entrants, the round table
 * keeps its slots empty, and the wrap of its update_for_date 0x633d40 (slot 5, 0x96bba0) does nothing: no
 * draw, no match, no hand-over to the UEFA Cup (0x633deb). With changing rules on, its start_new_season does not
 * run (the before hook of the kind uefa, season_change_skip). With changing rules off no kind runs after the start
 * season (Q232), so its start_new_season (slot 2, 0x633920) runs each year: it calls build_entrants and the rank
 * again, and the wraps here keep it idle (no entrants, the year of its last edition, no positions). */

#define UIC_VTABLE 0x0096bb8cu

#define GAME_YEAR 0xae2c92u               /* the game year (u16), as Uefa_Seeding::update_season reads it (0x9036b6) */

/* 1 for the Intertoto Cup object in a season whose row has no Intertoto Cup. The season is the game year (in
 * January to June of S + 1 the row of S + 1, which has none either from 2009/10 on): the object keeps the year of
 * its last edition (below), so its own year +0x40 does not tell. */
static int uic_not_held(uint32_t uic)
{
    if (!readable(uic) || rd32(g_mem, uic) != UIC_VTABLE) return 0;
    const SeasonUefa *r = season_uefa_row((int16_t)rd16(g_mem, GAME_YEAR));
    return r && r->uic_clubs < 0;
}

/* The last season up to S whose row holds the Intertoto Cup (2008), or S. */
static int uic_last_year(int season)
{
    int last = season;
    for (int i = 0; i < season_uefa_row_count; i++)
        if (season_uefa_rows[i].season <= season && season_uefa_rows[i].uic_clubs >= 0) last = season_uefa_rows[i].season;
    return last;
}

void o_006336c0(void);
void o_00633d40(void);

/* Intertoto_Cup::build_entrants, thiscall () returning 1 or 0. */
RT_WRAP(006336c0, uic_build_entrants)
{
    uint32_t uic = cpu.ecx;
    if (!uic_not_held(uic)) {
        o_006336c0();
        return;
    }
    wr16(g_mem, uic + 0x36, 0);
    /* An empty entry table of its slots: Cup::save_cup_to_disk 0x5210f0 shows the box "Cup.cpp 5246" for a cup with
     * no entry table (+0xa7, 0x5211a2), and the game's save writes every cup. */
    if (!readable(rd32(g_mem, uic + 0xa7))) {
        int slots = (int16_t)rd16(g_mem, uic + 0x3e);
        uint32_t e = game_malloc(ENTRY * (uint32_t)(slots > 0 && slots < 4096 ? slots : 1));
        if (rt_unwinding) return;
        wr32(g_mem, uic + 0xa7, e);
    }
    /* The year of its last edition, so the object reads as an old, finished edition (the rules report: not
     * held when the year is not the season, one before or one after). */
    wr16(g_mem, uic + 0x40, (uint16_t)uic_last_year((int16_t)rd16(g_mem, GAME_YEAR)));
    rt_ret_cdecl(1);
}

/* Intertoto_Cup::update_for_date, thiscall (2 arguments) ret 8. It returns what Cup::update_for_date 0x51f890
 * returns (0x633e92). */
RT_WRAP(00633d40, uic_update_for_date)
{
    if (!uic_not_held(cpu.ecx)) {
        o_00633d40();
        return;
    }
    rt_ret_stdcall(0, 2);
}

void o_005223a0(void);

/* Cup::rank_teams 0x5223a0, thiscall () (slot 23 of the Intertoto Cup, 0x96bbe8; called by its constructor at
 * 0x633176): it gives every entry slot a band of positions, the first band with no test of an empty slot
 * (0x52254b..0x52256e, comp_stats.cpp 1664 at 0x4a25a8 for a club of 0). An Intertoto Cup that is not held has
 * no clubs, so it ranks none. */
RT_WRAP(005223a0, cup_rank_teams)
{
    if (!uic_not_held(cpu.ecx)) {
        o_005223a0();
        return;
    }
    rt_ret_cdecl(0);
}

/* The rules report (season_report.c): the clubs of a cup of the access list, -1 for any other. Champions
 * League: the entrants of its list (+0x36; its entry table holds only the qualifying clubs). Europa League: the
 * clubs in its slots, the Champions League clubs that came to it included (the rules table counts them: "161
 * entered + 33 from the Champions League"). */
int season_uefa_report_clubs(uint32_t obj)
{
    if (!readable(obj) || (rd32(g_mem, obj) != CL_VTABLE && !season_uefa_cup_kind(obj)) || !access_row_of(obj))
        return -1;
    if (rd32(g_mem, obj) == CL_VTABLE) return (int16_t)rd16(g_mem, obj + 0x36);
    uint32_t e = rd32(g_mem, obj + 0xa7);
    int n = 0, slots = (int16_t)rd16(g_mem, obj + 0x3e);
    for (int i = 0; readable(e) && i < slots; i++) n += readable(rd32(g_mem, e + ENTRY * (uint32_t)i));
    return n;
}

/* ---- The Champions League group draw never leaves a group row empty (0.6.0, box Database 13126) ----
 *
 * The original. EuropeanCup::draw_phase1_groups 0x585290 takes the 32 group clubs (list slots 0 to 15, 0x58535a..
 * 0x585375, and the entries with +5 = 1, 0x585383..0x5853b8), sorts them by entry code up and reputation down
 * (qsort 0x945333 with 0x586f60, 0x585435..0x58543f; pots of 8 by position), and pairs each club with the next
 * club of the same nation that has no partner (0x58544c..0x585496). Then for each pot (0x585528..0x5856c3) it
 * takes the clubs of the pot in random order (0x90cfc0 at 0x585564), gives a club with no half (+8 = 0) the half 2
 * or 1 by the places that are left in each half of this pot (c2 at [esp+0x12], c1 at [esp+0x13], 4 each at the
 * start less the clubs that already have a half: 0x585528..0x585550; the random number below c2 + c1 at 0x5855b7,
 * below c2 is half 2, 0x5855bf..0x5855d1), gives the partner the other half (0x5855e1..0x585656), and puts the club
 * in the first free slot of its pot in the four groups of its half (0x58565a..0x585699). A pot has 4 slots in
 * each half. A club whose half is full is not put anywhere, and its group row stays empty (the pointer of the
 * entry is zeroed only when a club is put, 0x585693). The group draw news then reads the nation of that club
 * (0x5857f4, 0x9058b0, Database.cpp 13126 at 0x53b407): the box. The halves of the partners of an earlier pot
 * come before the pot is drawn: when 5 clubs of a pot have a partner in an earlier pot, and that partner has the
 * same half for all 5, the pot has 5 clubs for the 4 slots of a half. A pair of one nation inside the pot takes
 * one slot of each half, so 4 such clubs and one such pair also leave a club with no slot.
 *
 * This file. For a Champions League of the access list, before the draw: the same sort (the game's qsort) and
 * pairs, and when a pot has 5 or more clubs with a partner in an earlier pot or pairs in the pot (a pair counts
 * once), a choice of the half of each club
 * (the partners in opposite halves, 4 of each half in every pot), found by search. The hook on the random function
 * (win.h) then returns the number that gives that half at the call at 0x5855b7 (return address 0x5855bc). The
 * clubs, the other random numbers and the count of calls stay the original's. A draw that cannot fail runs as the
 * original's. */
#define DG_RET_HALF 0x005855bcu          /* after the call of 0x90cfc0 at 0x5855b7 */
#define F_GAME_QSORT 0x00945333u         /* cdecl (base, count, size, compare) */
#define F_SEED_COMPARE 0x00586f60u
#define DG_N 32

static int dg_on, dg_count;
static uint32_t dg_club[DG_N];
static int8_t dg_half[DG_N];

static uint32_t dg_hook(uint32_t esp, uint32_t edi, uint32_t result)
{
    if (!dg_on || rd32(g_mem, esp) != DG_RET_HALF) return result;
    /* The locals of the draw as the instructions at 0x5855aa..0x5855b6 read them: esp + 8 is their esp. */
    int c2 = (int8_t)g_mem[esp + 8 + 0x12], c1 = (int8_t)g_mem[esp + 8 + 0x13];
    uint32_t club = readable(edi) ? rd32(g_mem, edi) : 0;
    int want = 0;
    for (int i = 0; i < dg_count; i++)
        if (dg_club[i] == club) want = dg_half[i];
    if (want == 2 && c2 > 0) return 0;
    if (want == 1 && c1 > 0) return c2 > 0 ? (uint32_t)c2 : 0;
    return result;
}

/* The search for halves: partners opposite, at most 4 of each half in a pot. */
static int dg_search(const int *partner, int8_t *half, int *used, int i, long *steps)
{
    if (i == DG_N) return 1;
    if (++*steps > 2000000L) return 0;
    int pot = i / 8;
    for (int h = 1; h <= 2; h++) {
        if (partner[i] >= 0 && partner[i] < i && half[partner[i]] == h) continue;
        if (used[pot * 3 + h] >= 4) continue;
        half[i] = (int8_t)h;
        used[pot * 3 + h]++;
        if (dg_search(partner, half, used, i + 1, steps)) return 1;
        used[pot * 3 + h]--;
        half[i] = 0;
    }
    return 0;
}

void o_00585290(void);

RT_WRAP(00585290, cl_group_draw_guard)
{
    uint32_t cl = cpu.ecx;
    dg_on = 0;
    dg_count = 0;
    if (readable(cl) && rd32(g_mem, cl) == CL_VTABLE && access_row_of(cl)) {
        uint32_t list = rd32(g_mem, cl + 0x14), e = rd32(g_mem, cl + 0xa7);
        int slots = (int16_t)rd16(g_mem, cl + 0x3e), n = 0;
        uint32_t club[DG_N];
        for (int i = 0; i < 16 && readable(list) && n < DG_N; i++) club[n++] = rd32(g_mem, list + ENTRY * (uint32_t)i);
        for (int i = 0; readable(e) && i < slots && n < DG_N; i++)
            if (g_mem[e + ENTRY * (uint32_t)i + 5] == 1) club[n++] = rd32(g_mem, e + ENTRY * (uint32_t)i);
        int ok = n == DG_N;
        for (int i = 0; ok && i < n; i++) ok = readable(club[i]) && readable(rd32(g_mem, club[i] + 0x53));
        /* The 9-byte entries of the draw (club, partner, half) go on the game's stack, below the call: no
         * allocation, so the heap of the game keeps its layout (Q152). */
        uint32_t saved_esp = cpu.esp, buf = 0;
        if (ok) {
            cpu.esp = (cpu.esp - 9u * DG_N - 16u) & ~15u;
            buf = cpu.esp;
            for (int i = 0; i < 9 * DG_N; i++) g_mem[buf + (uint32_t)i] = 0;
        }
        if (buf) {
            for (int i = 0; i < DG_N; i++) wr32(g_mem, buf + 9u * (uint32_t)i, club[i]);
            uint32_t q[4] = {buf, DG_N, 9, F_SEED_COMPARE};
            call_cdecl(F_GAME_QSORT, 4, q);
            if (rt_unwinding) return;
            int partner[DG_N], risky = 0, from_earlier[4] = {0, 0, 0, 0};
            uint32_t nat[DG_N];
            for (int i = 0; i < DG_N; i++) {
                club[i] = rd32(g_mem, buf + 9u * (uint32_t)i);
                nat[i] = rd32(g_mem, club[i] + 0x53);
                partner[i] = -1;
            }
            for (int i = 0; i < DG_N; i++)                  /* 0x58544c..0x585496 */
                for (int j = i + 1; partner[i] < 0 && j < DG_N; j++)
                    if (partner[j] < 0 && nat[j] == nat[i]) partner[i] = j, partner[j] = i;
            /* The worst case of a half in a pot: every club with a partner in an earlier pot forced into it (their
             * partners all drew the other half), and one club of each pair inside the pot (a pair takes one place
             * of each half: 0x5855e1..0x585656 gives the partner the other half with no test of its places). Five
             * or more is more than the 4 places of a half (slice 23 review 2, item 1). */
            for (int i = 0; i < DG_N; i++) {
                if (partner[i] >= 0 && partner[i] / 8 < i / 8) from_earlier[i / 8]++;
                if (partner[i] > i && partner[i] / 8 == i / 8) from_earlier[i / 8]++;
            }
            for (int p = 0; p < 4; p++)
                if (from_earlier[p] >= 5) risky = 1;
            cpu.esp = saved_esp;
            if (getenv("RECOMP_DRAW_GUARD") && !strcmp(getenv("RECOMP_DRAW_GUARD"), "force")) risky = 1;   /* a test run */
            if (risky) {
                int8_t half[DG_N] = {0};
                int used[12] = {0};
                long steps = 0;
                if (dg_search(partner, half, used, 0, &steps)) {
                    memcpy(dg_club, club, sizeof club);
                    memcpy(dg_half, half, sizeof half);
                    dg_count = DG_N;
                    dg_on = 1;
                    win_random_hook = dg_hook;
                    fprintf(stderr, "recomp: uefa: Champions League group draw: 5 or more clubs of a pot have a partner "
                            "in an earlier pot or in the pot, the halves of the 32 clubs are set\n");
                } else {
                    fprintf(stderr, "recomp: uefa: Champions League group draw: no balanced halves for the 32 clubs\n");
                }
            }
        }
    }
    o_00585290();
    dg_on = 0;
    win_random_hook = NULL;
}

/* EUROPE_RULES::get_ineligibility_code 0x5832e0, thiscall (player, fixture), ret 8. On its first deadline (4 August,
 * uefa_check_registration_deadlines 0x9058e0, call at 0x9059c5) process_euro_registration 0x85a9b0 sets the
 * registration year, clears every player's mark +0x6d (0x85a9e3..0x85a9fa) and registers the clubs that each
 * European competition lists then. From then on a player of the year's Champions League or Europa League with no
 * mark 2 gets code 8 (0x5833e1..0x5833ed) in every round but the original's qualifying rounds 160 and 230
 * (0x5833bb..0x5833df). The access list's qualifying rounds (codes 220 to 270 above, as 0x4b0870 rounds them) are
 * not in that test, so a club still in them after 4 August, or handed over to the Europa or Conference League,
 * fielded only its registered players: FC Midtjylland had 6 of 50 in its Champions League third qualifying round
 * on 14 August 2020 (2020/21 start, November 2020 database), and the match stopped at match_eng.cpp 652 (init_match
 * 0x6b7f20 counts fewer than 11 at 0x6b89be). With the access list these rounds take the original's base rule
 * (TRANSFER_RULES::get_ineligibility_code 0x8e8a60, as 0x5833ac does), as its own qualifying rounds do. */
void o_005832e0(void);

RT_WRAP(005832e0, uefa_qual_registration)
{
    uint32_t self = cpu.ecx, player = rt_arg(0), fixture = rt_arg(1);
    o_005832e0();
    if (rt_unwinding || (cpu.eax & 0xff) != 8 || !readable(fixture)) return;
    int code = (int16_t)rd16(g_mem, fixture + 0x30), base = code < 1000 ? code - code % 10 : code;
    if (base != 220 && (base < 240 || base > 270)) return;
    uint32_t rec = rd32(g_mem, fixture + 0x14);
    if (!readable(rec) || !season_uefa_row((int16_t)rd16(g_mem, GAME_YEAR))) return;
    uint32_t a[2] = {player, fixture};
    uint32_t r = call_this(0x8e8a60u, self, 2, a);
    if (!rt_unwinding) cpu.eax = (cpu.eax & 0xffffff00u) | (r & 0xff);
}
