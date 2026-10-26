/* The formats of the UEFA club competitions by season (season_uefa.h), 0.6.0 slice 22. Evidence and the
 * reasons for each step: docs/research/2026-10-17-uefa-formats.md.
 *
 * What this file changes in the game, only when season_uefa_row gives a row (never in the base game or with
 * RECOMP_SEASON_CODE=off):
 *
 * The Champions League with one group stage and a round of 16 (2003/04 on, change point U1). The original
 * (EuropeanCup, vtable 0x96a1d0) holds 13 stages: +0xc the stage array, +0x2c the stage count 13 (set at
 * 0x583820), +0x30 the current stage (-1: the qualifying rounds of the Cup base). advance_stage 0x5851d0 draws
 * the 8 groups of phase 1 into stages 0 to 7 (0x585290), the 4 groups of phase 2 into 8 to 11 (0x585ab0) and
 * the quarter-finals into 12 (0x586330). Code of the Cup base walks every stage up to +0x30 with no NULL test
 * (Cup::update_for_date 0x51f8da, Cup::check_complete 0x521e2b, Cup::save_cup_to_disk 0x521573, and the
 * competition screens), and Club_History reads a stage's +0x44 as its index (0x443051). So the new format
 * holds 9 stages: the groups in 0 to 7 and a knockout stage of 16 clubs and 4 rounds in 8 (+0x44 = 8). The
 * save keeps +0x2c and +0x30 (0x521535, 0x521541; the load 0x520c48, 0x520c56), so a stage count of 9 marks
 * a Champions League of this format in a loaded game too. The functions of the class that know the stage
 * numbers of the original are wrapped below for a count of 9: advance_stage, rank_teams 0x586900, and the
 * stage argument 8 of set_club_progress 0x586c00, build_fate_news_text 0x584550 and
 * update_staff_records_for_status 0x584ae0 becomes 12, the knockout of the original. The two reads of the
 * knockout slot (+0x30 of the stage array, 0x584ad3 and 0x584cf2) read rt_uefa_cl_ko_offset (tools/recomp/
 * uefa.py).
 *
 * The stage count of a season is set when the class asks for its qualifying round table (create_round_table
 * with stage -1: the constructor and start_new_season, after the year moves; 0x584d60 frees the stages up to
 * +0x30 and never sets +0x2c). The rule kind "uefa" (season_change.h) writes the news of a change of format. */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_rules.h"
#include "season_uefa.h"
#include "season_round_names.h"
#include "season_uefa_league.h"
#include "season_uefa_days.h"
#include "season_comps.h"

#include <stdio.h>
#include <string.h>

#define COMP_TABLE 0xadadfcu            /* Comp **, one slot per competition record */
#define KEY_CL 0x9cf6e8u                /* record index of the Champions League (match at 0x60efd0) */
#define KEY_UC 0x9cf6f0u                /* record index of the UEFA Cup */
#define CL_VTABLE 0x0096a1d0u
#define SEEDING 0xde1f58u               /* Uefa_Seeding * */
#define FINANCES 0xae2a38u              /* the finance table */
#define RULING_BODIES 0xae28f0u         /* table of the ruling bodies, by the competition's +0x50 */

#define F_LEAGUE_ROW 0x006826d0u        /* League::get_table_row, thiscall (index): the row, club at +0 */
#define F_RANDOM 0x0090cfc0u            /* utils_random, cdecl (range) */
#define F_CALLOC 0x00945501u            /* cdecl (count, size), as 0x586355 */
#define F_MALLOC 0x00944e46u            /* cdecl (size), as the round tables 0x583caa */
#define F_FREE 0x009452cau              /* cdecl (block) */
#define F_NEW 0x00944cf1u               /* operator new, cdecl (size), as 0x58650f */
#define F_BOOKINGS 0x007e9180u          /* RulingBody::reset_bookings_for_club, thiscall (club) */
#define F_NATION_POINTS 0x009058b0u     /* UefaSeeding::add_nation_points, thiscall (club +0x53, points) */
#define F_GET_FINANCE 0x005a0590u       /* FinanceTable::get_club_finance, thiscall (club) */
#define F_ADD_PRIZE 0x005999a0u         /* Finance::add_prize_money, thiscall (amount) */
#define F_PRIZE_NEWS 0x0048e3e0u        /* Comp::send_prize_money_news, thiscall, 7 arguments */
#define F_CUP_STAGE 0x00522e00u         /* Cup_Stage::Cup_Stage, thiscall, 15 arguments */
#define F_SET_UP_ROUND 0x0051c800u      /* Cup::set_up_round, thiscall (round) */
#define F_GROUP_DATE 0x0068a160u        /* league_round_set_date, cdecl (table, round, day, month, year+, weekday, time, year, 0) */
#define F_ROUND_TABLE 0x00583c50u       /* EuropeanCup::create_round_table, thiscall (stage, &rounds, &name, &league) */
#define F_DRAW_DATE 0x00521e60u         /* cup_set_round_draw_date, cdecl (table, round, day, month, year+, weekday, year) */
#define F_MATCH_DATE 0x00521eb0u        /* cup_set_round_match_date, cdecl (..., weekday, time, year, venue) */
#define F_SORT_ENTRANTS 0x004c08f0u     /* cdecl (club list, count), as 0x5869ee */
#define F_SET_POSITION 0x004a2540u      /* Comp_Stats: thiscall (club, position), as 0x586a62 */

#define ROUND_SIZE 0x68u                /* a round of a Cup round table */
#define STAGES_ORIGINAL 13
#define STAGES_ROUND_OF_16 9
#define KO_SLOT_ORIGINAL 12
#define KO_SLOT_ROUND_OF_16 8
#define STAGES_LEAGUE 2                 /* slice 25: the league phase in stage 0, the knockout phase in stage 1 */
#define KO_SLOT_LEAGUE 1
#define LEAGUE_KO_CLUBS 24              /* the knockout phase play-offs (16) and places 1 to 8 */
#define LEAGUE_KO_ROUNDS 5
#define ROUND_CODE_KO_PO 10             /* "First Round"; named "Knockout Phase Play-offs" from 2024/25 (round names) */
#define GROUP_PRIZE 0x1b7740u           /* 1,800,000: the group stage money of each club (0x585803) */
#define LEAGUE_NAME 0x41fu              /* the stage name id ("Group A", 0x4b9a44); named "League Phase" from 2024/25 */
#define PHASE2_PRIZE 0x124f80u          /* 1,200,000: the entry money of phase 2 (0x586065), here of the round of 16 */
#define ROUND_CODE_R16 20               /* "Second Round" (round name function 0x4b4850; codes 10 to 150 are taken) */

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

/* ---- The row of a season ---- */


static uint32_t cl_object(void)
{
    int32_t key = (int32_t)rd32(g_mem, KEY_CL);
    uint32_t table = rd32(g_mem, COMP_TABLE);
    if (key < 0 || !readable(table)) return 0;
    uint32_t obj = rd32(g_mem, table + 4u * (uint32_t)key);
    return readable(obj) && rd32(g_mem, obj) == CL_VTABLE ? obj : 0;
}

static int stage_count(uint32_t cl) { return (int32_t)rd32(g_mem, cl + 0x2c); }
static int round_of_16_layout(uint32_t cl) { return readable(cl) && stage_count(cl) == STAGES_ROUND_OF_16; }
static int league_layout(uint32_t cl) { return readable(cl) && stage_count(cl) == STAGES_LEAGUE; }
static uint32_t stage_at(uint32_t cl, int i) { return rd32(g_mem, rd32(g_mem, cl + 0xc) + 4u * (uint32_t)i); }

uint32_t rt_uefa_cl_ko_offset(uint32_t cl)
{
    return 4u * (league_layout(cl) ? KO_SLOT_LEAGUE : round_of_16_layout(cl) ? KO_SLOT_ROUND_OF_16 : KO_SLOT_ORIGINAL);
}

/* Sets the stage count of the format of row r (NULL: the original's 13). */
static void cl_set_format(uint32_t cl, const SeasonUefa *r)
{
    int want = r && r->cl_league ? STAGES_LEAGUE : r && season_uefa_cl_round_of_16(r) ? STAGES_ROUND_OF_16 : STAGES_ORIGINAL;
    if (stage_count(cl) == want) return;
    wr32(g_mem, cl + 0x2c, (uint32_t)want);
    fprintf(stderr, "recomp: uefa: Champions League %d/%02d with %d stages\n", (int16_t)rd16(g_mem, cl + 0x40),
            ((int16_t)rd16(g_mem, cl + 0x40) + 1) % 100, want);
}

/* ---- No away goals rule (slice 24, U9: 2021/22 on) ----
 *
 * A two-legged tie that is level on aggregate goes to the away goals rule in the Cup's tie result code
 * (0x51f74c..0x51f79e: with flag 0x4 of the flags argument clear, the side with more away goals wins, winner
 * code 6 at +0x3d or +0x3e), and the match engine 0x6bd930 tests the same flag before it compares the away goals
 * (0x6bdab9, 0x6bdbc1), so it plays no extra time when they decide. Flag 0x4 skips both
 * (docs/research/2026-10-17-cup-round-record.md, section 2). The second leg takes its flags from the round's
 * +0x0f (OR'd in after the first leg). So in a season whose row has no away goals rule, each two-legged round
 * (+0x21 = 2) of the three cups gets 0x4 in +0x0f: level on aggregate means extra time, then penalties (the
 * 3 = extra time and penalties that the original's records hold). */
void season_uefa_no_away_goals(uint32_t table, int rounds, int season)
{
    const SeasonUefa *r = season_uefa_row(season);
    if (!r || r->away_goals || !readable(table)) return;
    int n = 0;
    for (int i = 0; i < rounds; i++) {
        uint32_t rec = table + ROUND_SIZE * (uint32_t)i;
        if (g_mem[rec + 0x21] != 2) continue;
        wr16(g_mem, rec + 0xf, (uint16_t)(rd16(g_mem, rec + 0xf) | 0x4));
        n++;
    }
    fprintf(stderr, "recomp: uefa: no away goals rule in %d of %d rounds, first code %d (%d/%02d)\n", n, rounds,
            (int16_t)rd16(g_mem, table + 7), season, (season + 1) % 100);
}

void o_0051f2f0(void);

/* The tie result code 0x51f2f0 (stdcall (result, fixture) ret 8; the flags word fixture +0x38 at 0x51f30f, the
 * winner codes at fixture +0x3d and +0x3e, 6 for away goals at 0x51f6d0, 0x51f6e8, 0x51f76c and 0x51f784). It
 * only watches: a tie of the Champions League or the Europa League that away goals decide writes a log line
 * for the test (tests/seasons/uefa_2021_run.py). The fixture's competition is the record at +0x14, whose first
 * dword is its id (as the eligibility test 0x55d29f..0x55d2a1 reads it). */
RT_WRAP(0051f2f0, cup_tie_result)
{
    uint32_t fixture = rt_arg(1);
    o_0051f2f0();
    if (rt_unwinding || !readable(fixture) || (g_mem[fixture + 0x3d] != 6 && g_mem[fixture + 0x3e] != 6)) return;
    uint32_t rec = rd32(g_mem, fixture + 0x14);
    int32_t id = readable(rec) ? (int32_t)rd32(g_mem, rec) : -1;
    if (id < 0 || (id != (int32_t)rd32(g_mem, KEY_CL) && id != (int32_t)rd32(g_mem, KEY_UC))) return;
    int year = (int16_t)rd16(g_mem, 0xae2c92u), day = (int16_t)rd16(g_mem, 0xae2c90u);
    fprintf(stderr, "recomp: uefa: a %s tie decided on away goals on day %d of %d (flags %04x)\n",
            id == (int32_t)rd32(g_mem, KEY_CL) ? "Champions League" : "Europa League", day, year,
            rd16(g_mem, fixture + 0x38));
}

/* ---- The knockout round table of 4 rounds ---- */

/* The original's table of the quarter-finals, semi-finals and final (create_round_table, stage 12: 3 rounds,
 * codes 0x78, 0x82, 0x96 at 0x58437f..0x584409), with a round of 16 before it. Returns the table (game
 * memory from F_MALLOC, as the original's) and its name id, or 0. */
static uint32_t ko_table(uint32_t cl, uint32_t *name_id)
{
    uint32_t saved_esp = cpu.esp;
    cpu.esp -= 16;                                  /* room for the three outputs, below the frame */
    uint32_t out = cpu.esp;
    for (int i = 0; i < 16; i += 4) wr32(g_mem, out + (uint32_t)i, 0);
    uint32_t a[4] = {KO_SLOT_ORIGINAL, out, out + 4, out + 8};
    uint32_t t3 = call_this(F_ROUND_TABLE, cl, 4, a);
    int16_t rounds = (int16_t)rd16(g_mem, out);
    *name_id = rd16(g_mem, out + 4);
    if (rt_unwinding) return 0;
    cpu.esp = saved_esp;
    if (!readable(t3) || rounds != 3) return 0;
    uint32_t size[1] = {4u * ROUND_SIZE};
    uint32_t t = call_cdecl(F_MALLOC, 1, size);
    if (rt_unwinding || !readable(t)) return 0;
    memcpy(g_mem + t + ROUND_SIZE, g_mem + t3, 3u * ROUND_SIZE);
    memcpy(g_mem + t, g_mem + t3, ROUND_SIZE);      /* the round of 16 from the quarter-final's record */
    uint32_t f[1] = {t3};
    call_cdecl(F_FREE, 1, f);
    if (rt_unwinding) return 0;
    for (int r = 0; r < 4; r++) wr16(g_mem, t + (uint32_t)r * ROUND_SIZE + 9, (uint16_t)r);      /* round index */
    /* Round of 16: 16 clubs, 8 winners, all 16 new from the list (+0x1c) at its start (+0x1e). Flag 0x200 of
     * +0xd: the ties are the pairs of the list, no draw (Cup draw 0x51c410: 0x51c5e4..0x51c5ec skips the
     * random draw 0x51e220). The entry money of the round comes with the draw, so +0x5c holds nothing. */
    wr16(g_mem, t + 7, ROUND_CODE_R16);
    wr16(g_mem, t + 0xd, 0x200);
    wr16(g_mem, t + 0x18, 16);
    wr16(g_mem, t + 0x1a, 8);
    wr16(g_mem, t + 0x1c, 16);
    wr16(g_mem, t + 0x1e, 0);
    wr32(g_mem, t + 0x5c, 0);
    wr32(g_mem, t + 0x60, 0);
    wr32(g_mem, t + 0x64, 0);
    /* Quarter-final: no new clubs and a random draw on its draw date (map_fixture_tree 0x518dc6 links the
     * winners of the round before to a random tie when the next round has no flag 0x200). */
    wr16(g_mem, t + ROUND_SIZE + 0xd, 0);
    wr16(g_mem, t + ROUND_SIZE + 0x1c, 0);
    wr16(g_mem, t + ROUND_SIZE + 0x1e, 0);
    /* Dates: the draw on 22 November and the first legs on 20 February, the weeks of the second group stage
     * of the original (its fourth match day, 0x58415a..0x584171), the second legs 14 days later (+0x22 of the
     * quarter-final record). Weekday and time as the quarter-final (0x584344..0x584368). */
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, cl + 0x40);
    uint32_t d[7] = {t, 0, 22, 10, 0, 3, year};
    call_cdecl(F_DRAW_DATE, 7, d);
    uint32_t m[9] = {t, 0, 20, 1, 1, 2, 2, year, 0};
    if (!rt_unwinding) call_cdecl(F_MATCH_DATE, 9, m);
    if (!rt_unwinding) season_uefa_no_away_goals(t, 4, (int)year);
    /* O259: the real dates of the season (season_uefa_dates.c); a round with no row keeps the dates above. */
    static const int kinds[4] = {SEASON_UEFA_KO_R16, SEASON_UEFA_KO_QF, SEASON_UEFA_KO_SF, SEASON_UEFA_KO_FINAL};
    int dated = rt_unwinding ? 0 : season_uefa_ko_dates(t, 'c', (int)year, kinds, 4);
    if (!rt_unwinding && season_uefa_ko_dates_log('c', (int)year))
        fprintf(stderr, "recomp: uefa: Champions League %u/%02u: knockout dates of %d of 4 rounds from the dates "
                        "table\n", year, (year + 1) % 100, dated);
    return rt_unwinding ? 0 : t;
}

/* ---- The draw of the round of 16 (in place of draw_phase2_groups 0x585ab0) ---- */

static uint32_t row_club(uint32_t stage, int pos)
{
    uint32_t a[1] = {(uint32_t)pos};
    uint32_t row = call_this(F_LEAGUE_ROW, stage, 1, a);
    return rt_unwinding || !readable(row) ? 0 : rd32(g_mem, row);
}

static int random_below(int n)
{
    uint32_t a[1] = {(uint32_t)n};
    return (int)call_cdecl(F_RANDOM, 1, a);
}

/* Pairs each group runner-up with the winner of another group, of another nation when it can (club +0x53,
 * the field draw_phase2_groups compares at 0x585bab). out[i] is the group of the winner for runner-up i. */
static int pair_groups(const uint32_t *win, const uint32_t *run, int *out)
{
    for (int relax = 0; relax < 2; relax++)
        for (int attempt = 0; attempt < 50; attempt++) {
            int used[8] = {0}, ok = 1;
            for (int i = 0; i < 8 && ok; i++) {
                int cand[8], n = 0;
                for (int j = 0; j < 8; j++)
                    if (!used[j] && j != i &&
                        (relax || rd32(g_mem, win[j] + 0x53) != rd32(g_mem, run[i] + 0x53)))
                        cand[n++] = j;
                if (!n) {
                    ok = 0;
                    break;
                }
                int k = random_below(n);
                if (rt_unwinding) return 0;
                out[i] = cand[k < 0 || k >= n ? 0 : k];
                used[out[i]] = 1;
            }
            if (ok) return 1;
        }
    return 0;
}

static void draw_round_of_16(uint32_t cl)
{
    uint32_t win[8], run[8];
    for (int g = 0; g < 8; g++) {
        uint32_t st = stage_at(cl, g);
        if (!readable(st)) return;
        win[g] = row_club(st, 0);
        run[g] = row_club(st, 1);
        if (rt_unwinding || !readable(win[g]) || !readable(run[g])) return;
    }
    int pair[8];
    if (!pair_groups(win, run, pair)) return;
    uint32_t ca[2] = {16, 4};
    uint32_t list = call_cdecl(F_CALLOC, 2, ca);
    if (rt_unwinding || !readable(list)) return;
    /* Each tie: the runner-up first, the group winner second (inference: the second club of a tie plays the
     * second leg at home, as the group winners do in the real competition). */
    for (int i = 0; i < 8; i++) {
        wr32(g_mem, list + 8u * (uint32_t)i, run[i]);
        wr32(g_mem, list + 8u * (uint32_t)i + 4, win[pair[i]]);
    }
    /* As the draws of the original (0x585bdd, 0x5864d5): the bookings of each club start again. */
    uint32_t bodies = rd32(g_mem, RULING_BODIES);
    uint32_t body = readable(bodies) ? rd32(g_mem, rd32(g_mem, bodies) + 4u * (uint32_t)(int8_t)g_mem[cl + 0x50]) : 0;
    for (int i = 0; i < 16 && readable(body) && !rt_unwinding; i++) {
        uint32_t a[1] = {rd32(g_mem, list + 4u * (uint32_t)i)};
        call_this(F_BOOKINGS, body, 1, a);
    }
    /* As phase 2 of the original (0x58604c..0x58608c): a coefficient point and the entry money for each club. */
    for (int i = 0; i < 16 && !rt_unwinding; i++) {
        uint32_t club = rd32(g_mem, list + 4u * (uint32_t)i);
        uint32_t p[2] = {rd32(g_mem, club + 0x53), 1};
        call_this(F_NATION_POINTS, rd32(g_mem, SEEDING), 2, p);
        if (rt_unwinding) return;
        uint32_t c[1] = {club};
        uint32_t fin = call_this(F_GET_FINANCE, rd32(g_mem, FINANCES), 1, c);
        if (rt_unwinding) return;
        uint32_t m[1] = {PHASE2_PRIZE};
        if (readable(fin)) call_this(F_ADD_PRIZE, fin, 1, m);
        if (rt_unwinding) return;
        uint32_t n[7] = {club, PHASE2_PRIZE, 0, 0xffffffffu, ROUND_CODE_R16, 0, 0xfffffffeu};
        call_this(F_PRIZE_NEWS, cl, 7, n);
    }
    if (rt_unwinding) return;
    /* The knockout stage in slot 8 (as 0x586508..0x58656d does in slot 12). */
    uint32_t name_id = 0;
    uint32_t table = ko_table(cl, &name_id);
    if (!table) return;
    uint32_t sz[1] = {0xb2};
    uint32_t obj = call_cdecl(F_NEW, 1, sz);
    if (rt_unwinding || !readable(obj)) return;
    uint32_t args[15] = {cl, 16, list, 4, rd32(g_mem, cl + 4), table, (uint32_t)(int16_t)rd16(g_mem, cl + 0x40),
                         KO_SLOT_ROUND_OF_16, 2, name_id, 0x14, 1, 0, 0, 0};
    call_this(F_CUP_STAGE, obj, 15, args);
    if (rt_unwinding) return;
    wr32(g_mem, rd32(g_mem, cl + 0xc) + 4u * KO_SLOT_ROUND_OF_16, obj);
    uint32_t fl[1] = {list}, ft[1] = {table};
    call_cdecl(F_FREE, 1, fl);
    if (!rt_unwinding) call_cdecl(F_FREE, 1, ft);
    if (rt_unwinding) return;
    /* The group thirds go to the UEFA Cup and the fourths are out (as 0x586282..0x58630b). */
    int32_t uc_key = (int32_t)rd32(g_mem, KEY_UC);
    uint32_t uc = uc_key >= 0 ? rd32(g_mem, rd32(g_mem, COMP_TABLE) + 4u * (uint32_t)uc_key) : 0;
    for (int g = 0; g < 8 && !rt_unwinding; g++) {
        uint32_t st = stage_at(cl, g);
        uint32_t third = row_club(st, 2);
        if (rt_unwinding) return;
        if (readable(uc) && readable(third)) {
            int32_t slots = (int16_t)rd16(g_mem, uc + 0x3e);
            uint32_t slot = rd32(g_mem, uc + 0xa7) + 6u * (uint32_t)(slots - 1 - g);
            wr32(g_mem, slot, third);
            g_mem[slot + 4] = 0;
            g_mem[slot + 5] = 0;
            int16_t n = (int16_t)rd16(g_mem, uc + 0x36);
            uint32_t at = rd32(g_mem, uc + 0x14) + 6u * (uint32_t)n;
            wr32(g_mem, at, third);
            g_mem[at + 4] = 5;
            wr16(g_mem, uc + 0x36, (uint16_t)(n + 1));
            wr32(g_mem, third + 0x1db, rd32(g_mem, KEY_UC));
        }
        uint32_t fourth = row_club(st, 3);
        if (!rt_unwinding && readable(fourth)) wr32(g_mem, fourth + 0x1db, 0xffffffffu);
    }
    if (rt_unwinding) return;
    if (readable(uc)) {
        Cpu saved = cpu;
        cpu.ecx = uc;
        rt_callback(rd32(g_mem, rd32(g_mem, uc) + 0x5c), 0, NULL);      /* the UEFA Cup's rank_teams */
        if (rt_unwinding) return;
        cpu = saved;
    }
    uint32_t r0[1] = {0};
    call_this(F_SET_UP_ROUND, obj, 1, r0);
    if (rt_unwinding) return;
    wr32(g_mem, cl + 0x30, KO_SLOT_ROUND_OF_16);
    fprintf(stderr, "recomp: uefa: Champions League round of 16 drawn\n");
}

/* ---- The league phase (slice 25, 2024/25 on) ----
 *
 * The object holds 2 stages: the league phase of 36 clubs (a League_Stage, season_uefa_league.c) in stage 0 and a
 * knockout stage of 24 clubs in stage 1: the knockout phase play-offs (16 clubs of places 9 to 24 in the fixed
 * ties of the bracket), the round of 16 (the 8 play-off winners and places 1 to 8, a seeded draw), the
 * quarter-finals, semi-finals and final of the original (create_round_table stage 12). */

/* The knockout table of 5 rounds, from the original's table of the quarter-finals, semi-finals and final, or 0. */
static uint32_t league_ko_table(uint32_t cl, uint32_t *name_id)
{
    uint32_t saved_esp = cpu.esp;
    cpu.esp -= 16;
    uint32_t out = cpu.esp;
    for (int i = 0; i < 16; i += 4) wr32(g_mem, out + (uint32_t)i, 0);
    uint32_t a[4] = {KO_SLOT_ORIGINAL, out, out + 4, out + 8};
    uint32_t t3 = call_this(F_ROUND_TABLE, cl, 4, a);
    int16_t rounds = (int16_t)rd16(g_mem, out);
    *name_id = rd16(g_mem, out + 4);
    if (rt_unwinding) return 0;
    cpu.esp = saved_esp;
    if (!readable(t3) || rounds != 3) return 0;
    uint32_t size[1] = {LEAGUE_KO_ROUNDS * ROUND_SIZE};
    uint32_t t = call_cdecl(F_MALLOC, 1, size);
    if (rt_unwinding || !readable(t)) return 0;
    memcpy(g_mem + t + 2 * ROUND_SIZE, g_mem + t3, 3u * ROUND_SIZE);
    memcpy(g_mem + t, g_mem + t3, ROUND_SIZE);                      /* the play-offs and the round of 16 from */
    memcpy(g_mem + t + ROUND_SIZE, g_mem + t3, ROUND_SIZE);         /* the quarter-final's record */
    uint32_t f[1] = {t3};
    call_cdecl(F_FREE, 1, f);
    if (rt_unwinding) return 0;
    for (int r = 0; r < LEAGUE_KO_ROUNDS; r++) wr16(g_mem, t + (uint32_t)r * ROUND_SIZE + 9, (uint16_t)r);
    /* The play-offs: 16 clubs, all new from slot 0, in the ties of the list (0x200, as the round of 16 of the
     * format before). Then the round of 16: the 8 winners and 8 new clubs from slot 16, a seeded draw (0x400:
     * 0x51d2d0 pairs the clubs of the higher tier byte with the winners) that keeps clubs of one nation apart
     * (+0xb 4). The quarter-final keeps no new clubs. */
    uint32_t po = t, r16 = t + ROUND_SIZE, qf = t + 2 * ROUND_SIZE;
    wr16(g_mem, po + 7, ROUND_CODE_KO_PO);
    wr16(g_mem, po + 0xd, 0x200);
    wr16(g_mem, po + 0x18, 16), wr16(g_mem, po + 0x1a, 8), wr16(g_mem, po + 0x1c, 16), wr16(g_mem, po + 0x1e, 0);
    wr16(g_mem, r16 + 7, ROUND_CODE_R16);
    wr16(g_mem, r16 + 0xd, 0x400);
    g_mem[r16 + 0xb] = 4;
    wr16(g_mem, r16 + 0x18, 16), wr16(g_mem, r16 + 0x1a, 8), wr16(g_mem, r16 + 0x1c, 8), wr16(g_mem, r16 + 0x1e, 16);
    for (uint32_t x = po; x <= r16; x += ROUND_SIZE) {
        wr32(g_mem, x + 0x5c, 0), wr32(g_mem, x + 0x60, 0), wr32(g_mem, x + 0x64, 0);
        g_mem[x + 0x22] = 7;                                        /* the second leg a week later */
    }
    wr16(g_mem, qf + 0xd, 0);
    wr16(g_mem, qf + 0x1c, 0);
    wr16(g_mem, qf + 0x1e, 0);
    /* Dates of 2024/25 (Q547): the play-off draw on 31 January, first legs on 12 February; the round of 16 draw
     * on 21 February, first legs on 5 March. Weekday and time as the quarter-final (0x584344..0x584368). */
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, cl + 0x40);
    uint32_t d0[7] = {t, 0, 31, 0, 1, 4, year};
    call_cdecl(F_DRAW_DATE, 7, d0);
    uint32_t m0[9] = {t, 0, 12, 1, 1, 2, 2, year, 0};
    if (!rt_unwinding) call_cdecl(F_MATCH_DATE, 9, m0);
    uint32_t d1[7] = {t, 1, 21, 1, 1, 4, year};
    if (!rt_unwinding) call_cdecl(F_DRAW_DATE, 7, d1);
    uint32_t m1[9] = {t, 1, 5, 2, 1, 2, 2, year, 0};
    if (!rt_unwinding) call_cdecl(F_MATCH_DATE, 9, m1);
    if (!rt_unwinding) season_uefa_no_away_goals(t, LEAGUE_KO_ROUNDS, (int)year);
    /* O259: the real dates of the season (season_uefa_dates.c); a round with no row keeps the dates above. */
    static const int kinds[LEAGUE_KO_ROUNDS] = {SEASON_UEFA_KO_PO, SEASON_UEFA_KO_R16, SEASON_UEFA_KO_QF,
                                                SEASON_UEFA_KO_SF, SEASON_UEFA_KO_FINAL};
    int dated = rt_unwinding ? 0 : season_uefa_ko_dates(t, 'c', (int)year, kinds, LEAGUE_KO_ROUNDS);
    if (!rt_unwinding && season_uefa_ko_dates_log('c', (int)year))
        fprintf(stderr, "recomp: uefa: Champions League %u/%02u: knockout dates of %d of %d rounds from the dates "
                        "table\n", year, (year + 1) % 100, dated, LEAGUE_KO_ROUNDS);
    return rt_unwinding ? 0 : t;
}

/* A coefficient point, the money and its news for each club (as each group of the original's group draw,
 * 0x5857ea..0x58582a; code 0x449, 0x585819). */
static void pay_clubs(uint32_t cl, const uint32_t *clubs, int n, uint32_t amount, uint32_t code)
{
    for (int i = 0; i < n && !rt_unwinding; i++) {
        uint32_t p[2] = {rd32(g_mem, clubs[i] + 0x53), 1};
        call_this(F_NATION_POINTS, rd32(g_mem, SEEDING), 2, p);
        if (rt_unwinding) return;
        uint32_t c[1] = {clubs[i]};
        uint32_t fin = call_this(F_GET_FINANCE, rd32(g_mem, FINANCES), 1, c);
        if (rt_unwinding) return;
        uint32_t m[1] = {amount};
        if (readable(fin)) call_this(F_ADD_PRIZE, fin, 1, m);
        if (rt_unwinding) return;
        uint32_t nw[7] = {clubs[i], amount, 0, 0xffffffffu, code, 0, 0xfffffffeu};
        call_this(F_PRIZE_NEWS, cl, 7, nw);
    }
}

static void reset_bookings(uint32_t cl, const uint32_t *clubs, int n)
{
    uint32_t bodies = rd32(g_mem, RULING_BODIES);
    uint32_t body = readable(bodies) ? rd32(g_mem, rd32(g_mem, bodies) + 4u * (uint32_t)(int8_t)g_mem[cl + 0x50]) : 0;
    for (int i = 0; i < n && readable(body) && !rt_unwinding; i++) {
        uint32_t a[1] = {clubs[i]};
        call_this(F_BOOKINGS, body, 1, a);
    }
}

/* The draw of the league phase, in place of draw_phase1_groups 0x585290 at stage -1. Its clubs as the original
 * takes them (0x585358..0x5853ba): list slots 0 to 15 and every entry with byte +5 = 1 (the play-off winners and
 * the other direct clubs, season_uefa_qual.c). Then the play-off losers go to the Europa League (entry byte
 * +5 = 2, from its slot count - 0x18, as 0x5859fa..0x585a86). */
static void draw_league(uint32_t cl)
{
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, cl + 0x40);
    const SeasonUefa *r = season_uefa_row((int)year);
    uint32_t clubs[SEASON_LEAGUE_CLUBS + 8];
    int n = 0;
    uint32_t list = rd32(g_mem, cl + 0x14), entries = rd32(g_mem, cl + 0xa7);
    for (int i = 0; i < 16 && readable(list); i++)
        if (n < SEASON_LEAGUE_CLUBS + 8) clubs[n++] = rd32(g_mem, list + 6u * (uint32_t)i);
    int count = (int16_t)rd16(g_mem, cl + 0x3e);
    for (int i = 0; i < count && readable(entries); i++)
        if (g_mem[entries + 6u * (uint32_t)i + 5] == 1 && n < SEASON_LEAGUE_CLUBS + 8)
            clubs[n++] = rd32(g_mem, entries + 6u * (uint32_t)i);
    if (!r || !r->cl_league || n != SEASON_LEAGUE_CLUBS) {
        fprintf(stderr, "recomp: uefa: Champions League league phase: %d clubs, not 36\n", n);
        return;
    }
    int weekday = 0;
    const SeasonLeagueDay *days = season_uefa_league_days('c', (int)year, &weekday);
    uint32_t st = season_uefa_league_make(cl, clubs, r->cl_league, 0, LEAGUE_NAME, days, weekday);
    if (rt_unwinding || !st) return;
    wr32(g_mem, rd32(g_mem, cl + 0xc), st);
    wr32(g_mem, cl + 0x30, 0);
    /* The bookings and money only once the stage is in its slot: a draw that fails comes again on a later day. */
    reset_bookings(cl, clubs, n);
    pay_clubs(cl, clubs, n, GROUP_PRIZE, 0x449);
    if (rt_unwinding) return;
    season_uefa_league_news(st);
    if (rt_unwinding) return;
    /* The play-off losers to the Europa League. */
    int32_t uc_key = (int32_t)rd32(g_mem, KEY_UC);
    uint32_t uc = uc_key >= 0 ? rd32(g_mem, rd32(g_mem, COMP_TABLE) + 4u * (uint32_t)uc_key) : 0;
    if (!readable(uc) || !readable(entries)) return;
    uint32_t at = rd32(g_mem, uc + 0xa7) + 6u * (uint32_t)((int16_t)rd16(g_mem, uc + 0x3e) - 0x18);
    for (int i = 0; i < count; i++) {
        if (g_mem[entries + 6u * (uint32_t)i + 5] != 2) continue;
        uint32_t club = rd32(g_mem, entries + 6u * (uint32_t)i);
        wr32(g_mem, at, club);
        g_mem[at + 4] = 0, g_mem[at + 5] = 0;
        at += 6;
        int16_t k = (int16_t)rd16(g_mem, uc + 0x36);
        uint32_t e = rd32(g_mem, uc + 0x14) + 6u * (uint32_t)k;
        wr32(g_mem, e, club);
        g_mem[e + 4] = 5;
        wr16(g_mem, uc + 0x36, (uint16_t)(k + 1));
        wr32(g_mem, club + 0x1db, rd32(g_mem, KEY_UC));
    }
    Cpu saved = cpu;
    cpu.ecx = uc;
    rt_callback(rd32(g_mem, rd32(g_mem, uc) + 0x5c), 0, NULL);          /* the UEFA Cup's rank_teams (0x585a86) */
    if (rt_unwinding) return;
    cpu = saved;
}

/* The knockout stage after the league phase, in slot 1. */
static void draw_league_ko(uint32_t cl)
{
    uint32_t league = stage_at(cl, 0);
    if (!readable(league)) return;
    uint32_t ca[2] = {LEAGUE_KO_CLUBS, 4};
    uint32_t list = call_cdecl(F_CALLOC, 2, ca);
    uint32_t sz[1] = {LEAGUE_KO_CLUBS};
    uint32_t tiers = rt_unwinding || !readable(list) ? 0 : call_cdecl(F_MALLOC, 1, sz);
    if (rt_unwinding) return;
    uint32_t name_id = 0, table = 0, obj = 0;
    uint32_t clubs[LEAGUE_KO_CLUBS];
    if (readable(tiers) && season_uefa_league_knockout_list(league, list, tiers)) {
        for (int i = 0; i < LEAGUE_KO_CLUBS; i++) clubs[i] = rd32(g_mem, list + 4u * (uint32_t)i);
        table = rt_unwinding ? 0 : league_ko_table(cl, &name_id);
        uint32_t ns[1] = {0xb2};
        obj = table && !rt_unwinding ? call_cdecl(F_NEW, 1, ns) : 0;
    }
    if (rt_unwinding) return;
    if (readable(obj)) {
        uint32_t args[15] = {cl, LEAGUE_KO_CLUBS, list, LEAGUE_KO_ROUNDS, rd32(g_mem, cl + 4), table,
                             (uint32_t)(int16_t)rd16(g_mem, cl + 0x40), KO_SLOT_LEAGUE, 2, name_id, 0x14, 1, 0, 0, tiers};
        call_this(F_CUP_STAGE, obj, 15, args);
        if (rt_unwinding) return;
        wr32(g_mem, rd32(g_mem, cl + 0xc) + 4u * KO_SLOT_LEAGUE, obj);
    }
    uint32_t fl[1] = {list};
    if (readable(list)) call_cdecl(F_FREE, 1, fl);
    uint32_t ft[1] = {tiers};
    if (!rt_unwinding && readable(tiers)) call_cdecl(F_FREE, 1, ft);
    uint32_t fb[1] = {table};
    if (!rt_unwinding && readable(table)) call_cdecl(F_FREE, 1, fb);
    if (rt_unwinding || !readable(obj)) return;
    reset_bookings(cl, clubs, LEAGUE_KO_CLUBS);         /* once the stage is in its slot, as draw_league */
    pay_clubs(cl, clubs, LEAGUE_KO_CLUBS, PHASE2_PRIZE, ROUND_CODE_KO_PO);
    if (rt_unwinding) return;
    uint32_t r0[1] = {0};
    call_this(F_SET_UP_ROUND, obj, 1, r0);
    if (rt_unwinding) return;
    wr32(g_mem, cl + 0x30, KO_SLOT_LEAGUE);
    fprintf(stderr, "recomp: uefa: Champions League knockout phase play-offs drawn\n");
}

const char *season_uefa_stage_name(uint32_t obj, int season, int id)
{
    const SeasonUefa *r = season_uefa_row(season);
    if (id != (int)LEAGUE_NAME || !r || !readable(obj)) return NULL;
    int league = obj == cl_object() ? r->cl_league : season_uefa_cup_kind(obj) == 'e' ? r->uc_league
                 : season_uefa_cup_kind(obj) == 'k' ? r->ecl_league : 0;
    return league ? "League Phase" : NULL;
}

/* ---- Wraps of the Champions League class ---- */

void o_005851d0(void);
void o_00586900(void);
void o_00586c00(void);
void o_00584550(void);
void o_00584ae0(void);

void o_00583c50(void);

/* The return addresses of the two calls that set up a season's rounds: the constructor (call at 0x5838d4) and
 * set_up_rules 0x858e70 (call at 0x858e83, from start_new_season 0x584ed7). Other callers only read the tables. */
#define CL_SETUP_CONSTRUCTOR 0x005838d7u
#define CL_SETUP_RULES 0x00858e86u

/* EuropeanCup::create_round_table, thiscall (stage, &rounds, &name, &league) ret 0x10. Stage -1 (the qualifying
 * rounds) is asked for by the constructor (0x5838d4) and at each new season (start_new_season 0x584d60 through
 * set_up_rules 0x858e70), after the season year is set: the format of the season is chosen there. A loaded
 * game takes +0x2c from its save and does not come here. */
RT_WRAP(00583c50, cl_round_table)
{
    uint32_t cl = cpu.ecx, ret = rd32(g_mem, cpu.esp);
    int stage = (int8_t)rt_arg(0);
    if (stage == -1 && (ret == CL_SETUP_CONSTRUCTOR || ret == CL_SETUP_RULES)) {
        const SeasonUefa *r = season_uefa_row((int16_t)rd16(g_mem, cl + 0x40));
        if (r || stage_count(cl) != STAGES_ORIGINAL) cl_set_format(cl, r);
        /* Slice 23: the qualifying rounds of the access list (season_uefa_qual.c), or 0 for the original's. */
        int rounds = 0;
        uint32_t name = 0, league = 0;
        uint32_t t = r ? season_uefa_cl_qual_table(cl, &rounds, &name, &league) : 0;
        if (rt_unwinding) return;
        if (t) {
            /* As the original writes them (0x583c8a..0x583ca7): a word to the rounds and to the name (set_up_rules
             * 0x858e70 passes +0x3c and +0x3a, so a dword at +0x3a would cover the rounds), a dword to the fourth. */
            uint32_t out_rounds = rt_arg(1), out_name = rt_arg(2), out_league = rt_arg(3);
            if (out_rounds >= 0x10000u) wr16(g_mem, out_rounds, (uint16_t)rounds);
            if (out_name >= 0x10000u) wr16(g_mem, out_name, (uint16_t)name);
            if (out_league >= 0x10000u) wr32(g_mem, out_league, league);
            rt_ret_stdcall(t, 4);
            return;
        }
    }
    /* Stage 8 of the format with a round of 16 is its knockout stage. Comp::build_match_date_list 0x48d500 (the
     * Schedule tab) asks for each stage up to +0x2c - 1 (0x48d5b0..0x48d5cb); the original's stage 8 is the
     * first group of phase 2 (0x58406b). */
    /* Slice 25: the league phase (stage 0: 8 match days, a league table) and its knockout stage (stage 1). */
    if ((stage == 0 || stage == KO_SLOT_LEAGUE) && league_layout(cl)) {
        uint32_t out_rounds = rt_arg(1), out_name = rt_arg(2), out_league = rt_arg(3), name_id = 0, t;
        const SeasonUefa *r = season_uefa_row((int16_t)rd16(g_mem, cl + 0x40));
        int weekday = 0, rounds = r && r->cl_league ? r->cl_league : 8;
        if (stage == 0) {
            const SeasonLeagueDay *days = season_uefa_league_days('c', (int16_t)rd16(g_mem, cl + 0x40), &weekday);
            t = season_uefa_league_table((uint32_t)(int16_t)rd16(g_mem, cl + 0x40), rounds, days, weekday);
            name_id = LEAGUE_NAME;
        } else {
            t = league_ko_table(cl, &name_id), rounds = LEAGUE_KO_ROUNDS;
        }
        if (rt_unwinding) return;
        if (t) {
            if (readable(out_rounds)) wr16(g_mem, out_rounds, (uint16_t)rounds);
            if (readable(out_name)) wr16(g_mem, out_name, (uint16_t)name_id);
            if (readable(out_league)) wr32(g_mem, out_league, stage == 0 ? 1u : 0u);
            rt_ret_stdcall(t, 4);
            return;
        }
    }
    if (stage == KO_SLOT_ROUND_OF_16 && round_of_16_layout(cl)) {
        uint32_t out_rounds = rt_arg(1), out_name = rt_arg(2), out_league = rt_arg(3), name_id = 0;
        uint32_t t = ko_table(cl, &name_id);
        if (rt_unwinding) return;
        if (t) {
            if (readable(out_rounds)) wr16(g_mem, out_rounds, 4);
            if (readable(out_name)) wr16(g_mem, out_name, (uint16_t)name_id);
            if (readable(out_league)) wr32(g_mem, out_league, 0);
            rt_ret_stdcall(t, 4);
            return;
        }
    }
    o_00583c50();
    /* Row A18: the real group days of the season (season_uefa_days.c) in place of the original's 2001 days, in the
     * group stages of phase 1 (stages 0 to 7) and, while the format keeps it, phase 2 (8 to 11). The original's days
     * met FIFA days in most seasons (Liverpool fielded 7 at Bordeaux on 10 September 2008, match_eng.cpp 652). */
    uint32_t t = cpu.eax;
    if (rt_unwinding || !readable(t) || stage < 0 || stage > 11 || league_layout(cl)) return;
    if (stage >= 8 && (round_of_16_layout(cl) || stage_count(cl) != STAGES_ORIGINAL)) return;
    int year = (int16_t)rd16(g_mem, cl + 0x40);
    if (!season_uefa_row(year)) return;
    SeasonLeagueDay days[6];
    int wd[6];
    if (!season_uefa_cl_group_days(year, stage < 8 ? 1 : 2, stage < 8 ? stage >= 4 : stage >= 10, days, wd)) return;
    for (int r = 0; r < 6 && !rt_unwinding; r++) {
        uint32_t a[9] = {t, (uint32_t)r, (uint32_t)days[r].day, (uint32_t)days[r].month, (uint32_t)days[r].year_off,
                         (uint32_t)wd[r], 2, (uint32_t)year, 0};
        call_cdecl(F_GROUP_DATE, 9, a);
    }
    if (!rt_unwinding) cpu.eax = t;
}

/* EuropeanCup::advance_stage, thiscall (). */
RT_WRAP(005851d0, cl_advance_stage)
{
    uint32_t cl = cpu.ecx;
    int32_t now = (int32_t)rd32(g_mem, cl + 0x30);
    if (league_layout(cl) && (now == -1 || now == 0)) {
        if (now == -1) draw_league(cl);         /* each sets +0x30 once its stage is in its slot */
        else draw_league_ko(cl);
        if (!rt_unwinding) rt_ret_cdecl(cpu.eax);
        return;
    }
    if (!round_of_16_layout(cl) || now != KO_SLOT_ROUND_OF_16 - 1) {
        o_005851d0();
        return;
    }
    draw_round_of_16(cl);                   /* it sets +0x30 to 8 once the stage is in its slot */
    if (!rt_unwinding) rt_ret_cdecl(cpu.eax);
}

/* EuropeanCup::rank_teams, thiscall (): final positions by bands of the entry list (0x586900). The bands of
 * the original: the knockout rounds from the round table of stage 12 (0x586a08), the thirds and fourths of
 * phase 2 (2 bands of 4, 0x586aef) and of phase 1 (2 bands of 8, 0x586b30), then the qualifying rounds of
 * +0xa3 (0x586b71). Here the knockout has 4 rounds and phase 2 is gone. */
RT_WRAP(00586900, cl_rank_teams)
{
    uint32_t cl = cpu.ecx;
    if (!round_of_16_layout(cl) && !league_layout(cl)) {
        o_00586900();
        return;
    }
    int league = league_layout(cl);
    uint32_t stats = rd32(g_mem, cl + 8);
    int16_t n = (int16_t)rd16(g_mem, cl + 0x36);
    if (!readable(stats) || n <= 0) {
        rt_ret_cdecl(0);
        return;
    }
    uint32_t sz[1] = {4u * (uint32_t)n};
    uint32_t list = call_cdecl(F_MALLOC, 1, sz);
    if (rt_unwinding) return;
    if (!readable(list)) {
        rt_ret_cdecl(0);
        return;
    }
    for (int i = 0; i < n; i++) wr32(g_mem, list + 4u * (uint32_t)i, rd32(g_mem, rd32(g_mem, cl + 0x14) + 6u * (uint32_t)i));
    uint32_t sa[2] = {list, (uint32_t)n};
    call_cdecl(F_SORT_ENTRANTS, 2, sa);
    if (rt_unwinding) return;
    int pos = 1, next = 0;
#define BAND(count)                                                                              \
    do {                                                                                         \
        int start = pos;                                                                         \
        for (int k_ = 0; k_ < (count) && next < n && !rt_unwinding; k_++, pos++) {               \
            uint32_t a_[2] = {rd32(g_mem, list + 4u * (uint32_t)next++), (uint32_t)(start & 0xff)}; \
            call_this(F_SET_POSITION, stats, 2, a_);                                             \
        }                                                                                        \
    } while (0)
    uint32_t name_id = 0;
    int ko_rounds = league ? LEAGUE_KO_ROUNDS : 4;
    uint32_t ko = league ? league_ko_table(cl, &name_id) : ko_table(cl, &name_id);
    if (rt_unwinding) return;
    if (ko) {
        for (int r = ko_rounds - 1; r >= 0; r--) {
            int winners = (int16_t)rd16(g_mem, ko + (uint32_t)r * ROUND_SIZE + 0x1a);
            if (r == ko_rounds - 1) BAND(winners);
            BAND(winners);
        }
        uint32_t f[1] = {ko};
        call_cdecl(F_FREE, 1, f);
        if (rt_unwinding) return;
    }
    if (league) {
        BAND(SEASON_LEAGUE_CLUBS - LEAGUE_KO_CLUBS);        /* places 25 to 36 of the league phase */
    } else {
        BAND(8);
        BAND(8);
    }
    uint32_t qual = rd32(g_mem, cl + 0xa3);
    for (int r = (int16_t)rd16(g_mem, cl + 0x3c) - 1; r >= 0 && readable(qual) && !rt_unwinding; r--)
        BAND((int16_t)rd16(g_mem, qual + (uint32_t)r * ROUND_SIZE + 0x1a));
#undef BAND
    if (rt_unwinding) return;
    uint32_t f[1] = {list};
    call_cdecl(F_FREE, 1, f);
    if (!rt_unwinding) rt_ret_cdecl(0);
}

/* The stage argument at stack index arg: the knockout in slot 8 (slot 1 with a league phase) is the knockout 12
 * of the original. */
static void map_stage_arg(uint32_t cl, int arg)
{
    int slot = league_layout(cl) ? KO_SLOT_LEAGUE : round_of_16_layout(cl) ? KO_SLOT_ROUND_OF_16 : -1;
    if (slot < 0) return;
    uint32_t at = cpu.esp + 4 + 4u * (uint32_t)arg;
    if ((int8_t)g_mem[at] == slot) g_mem[at] = KO_SLOT_ORIGINAL;
}

/* EuropeanCup::set_club_progress, thiscall (club, stage, code, code, code). */
RT_WRAP(00586c00, cl_set_club_progress)
{
    map_stage_arg(cpu.ecx, 1);
    o_00586c00();
}

/* EuropeanCup::build_fate_news_text, thiscall (club index, fate, stage, ...). */
RT_WRAP(00584550, cl_fate_news)
{
    map_stage_arg(cpu.ecx, 2);
    o_00584550();
}

/* EuropeanCup::update_staff_records_for_status, thiscall (club, outcome, stage, ...). */
RT_WRAP(00584ae0, cl_staff_records)
{
    map_stage_arg(cpu.ecx, 2);
    o_00584ae0();
}

/* ---- The rule kind ---- */

/* The name of the UEFA Cup in a season, from the rules table (UEFA Europa League from 2009/10), without "UEFA ". */
static const char *uc_name(int season)
{
    const SeasonRule *r = season_rules_find(season, "uefa_cup", 0);
    const char *n = r && r->name && *r->name ? r->name : "UEFA Cup";
    return strncmp(n, "UEFA ", 5) || !strcmp(n, "UEFA Cup") ? n : n + 5;
}

/* The group stage places of an access list bracket ("G G G PO-LP" gives 3). */
static int group_places(const char *places)
{
    int n = 0;
    for (const char *p = places; p && *p; p++)
        if (*p == 'G' && (p == places || p[-1] == ' ') && (p[1] == ' ' || !p[1])) n++;
    return n;
}

static void uefa_season(int season)
{
    const SeasonUefa *now = season_uefa_row(season), *before = season_uefa_row(season - 1);
    if (!now || !before || now == before) return;
    /* Slice 25: the league phase (2024/25 on). */
    if (now->cl_league && !before->cl_league)
        season_change_news("The Champions League group stage is now a league phase: one table of 36 clubs who play %d "
                           "matches each. Places 1 to 8 go to the round of 16, places 9 to 24 to knockout phase "
                           "play-offs.", now->cl_league);
    if (now->uc_league && !before->uc_league)
        season_change_news("The %s and the Conference League now also have a league phase of 36 clubs, with %d and "
                           "%d matches each.", uc_name(season), now->uc_league, now->ecl_league);
    if (now->cl_league || before->cl_league) {
        /* No group stage news across the change to the league phase: the sentences above say it. */
    } else if (season_uefa_cl_round_of_16(now) && !season_uefa_cl_round_of_16(before))
        season_change_news("The Champions League now has one group stage of 8 groups and a knockout round of 16.");
    else if (!season_uefa_cl_round_of_16(now) && season_uefa_cl_round_of_16(before))
        season_change_news("The Champions League now has two group stages and quarter-finals.");
    if (now->uc_league || before->uc_league) {
    } else if (now->uc_groups && !before->uc_groups)
        season_change_news("The UEFA Cup now has a group stage of %d groups of %d clubs, then a round of %d.",
                           now->uc_groups, now->uc_group_clubs, season_uefa_uc_round(now, "KO"));
    else if (!now->uc_groups && before->uc_groups)
        season_change_news("The UEFA Cup no longer has a group stage.");
    else if (now->uc_groups && (now->uc_groups != before->uc_groups || now->uc_group_clubs != before->uc_group_clubs))
        season_change_news("The %s group stage now has %d groups of %d clubs who meet %s, then a round of %d.",
                           uc_name(season), now->uc_groups, now->uc_group_clubs, now->uc_meetings == 2 ? "twice" : "once",
                           season_uefa_uc_round(now, "KO"));
    int paths_now = season_uefa_cl_round(now, "Q3 CP") || season_uefa_cl_round(now, "Q2 CP");
    int paths_before = season_uefa_cl_round(before, "Q3 CP") || season_uefa_cl_round(before, "Q2 CP");
    if (paths_now && !paths_before)
        season_change_news("The Champions League qualifying rounds now have a Champions Path for league champions "
                           "and a League Path for the other clubs.");
    if (now->uic_clubs < 0 && before->uic_clubs >= 0) season_change_news("The Intertoto Cup is no longer held.");
    /* Slice 24. */
    if (now->uc_ko_po && !before->uc_ko_po && !now->uc_league)
        season_change_news("The %s group winners now go to the round of 16, and the runners-up meet the Champions "
                           "League group thirds in knockout round play-offs.", uc_name(season));
    if (!now->away_goals && before->away_goals)
        season_change_news("Away goals no longer decide UEFA ties: a tie level on aggregate goes to extra time and "
                           "penalties.");
    for (const char *p = now->suspended ? now->suspended : ""; *p;) {
        const char *e = strchr(p, ',');
        int n = e ? (int)(e - p) : (int)strlen(p);
        char name[40];
        if (n > 0 && n < (int)sizeof name) {
            memcpy(name, p, (size_t)n);
            name[n] = 0;
            if (!season_uefa_suspended(before, name))
                season_change_news("Clubs from %s have no places in the UEFA competitions while the suspension lasts.",
                                   name);
        }
        p += n + (e ? 1 : 0);
    }
    /* The access list (slice 23): the group places of the association ranked 4. */
    const char *g_now = season_uefa_access_places(now->season, 'c', 4), *g_before = season_uefa_access_places(before->season, 'c', 4);
    if (g_now && g_before && group_places(g_now) == 4 && group_places(g_before) < 4)
        season_change_news("The four best-ranked associations now send four clubs each to the Champions League "
                           "group stage.");
    else if (now->uic_rounds != before->uic_rounds && now->uic_rounds > 0)
        season_change_news("The Intertoto Cup now has %d rounds, and its %d winners enter the UEFA Cup in the %s.",
                           now->uic_rounds, now->uic_winners,
                           !strcmp(now->uic_entry, "Q2") ? "second qualifying round" : "first round");
}

/* Slice 23: the Intertoto Cup is not held in a season whose row has no Intertoto Cup (2009/10 on): its
 * start_new_season does not run (season_change_skip), and season_uefa_qual.c keeps its object idle. With changing
 * rules off this hook does not run after the start season (Q232): the wraps of season_uefa_qual.c alone keep it
 * idle then. */
static void uefa_before(const SeasonChange *c)
{
    const SeasonUefa *r = season_uefa_row(c->season);
    if (r && r->uic_clubs < 0 && readable(c->comp) && rd32(g_mem, c->comp) == 0x0096bb8cu) season_change_skip();
}

const SeasonRuleKind season_kind_uefa = {"uefa", NULL, NULL, uefa_season, NULL, uefa_before, NULL};

/* ---- Slice 24, part b: the Europa League and the Conference League, two objects of one class ---- */

#define UC_VTABLE 0x00970790u
#define COMP_RECORDS 0xae23d0u

int season_uefa_cup_kind(uint32_t obj)
{
    if (!readable(obj) || rd32(g_mem, obj) != UC_VTABLE) return 0;
    uint32_t rec = rd32(g_mem, obj + 4);
    if (!readable(rec)) return 0;
    if ((int32_t)rd32(g_mem, rec) == (int32_t)rd32(g_mem, KEY_UC)) return 'e';
    const char *n = season_change_data_name(rec);
    return n && !strcmp(n, SEASON_UEFA_ECL_RECORD) ? 'k' : 0;
}

int32_t season_uefa_ecl_id(void)
{
    uint32_t rec = season_comps_record(SEASON_UEFA_ECL_RECORD);
    return rec ? (int32_t)rd32(g_mem, rec) : -1;
}

uint32_t season_uefa_ecl_object(void)
{
    for (int i = 0; i < season_comp_def_count; i++)
        if (season_comp_defs[i] && !strcmp(season_comp_defs[i]->file, SEASON_UEFA_ECL_FILE))
            return season_comps_object(season_comp_defs[i]);
    return 0;
}

/* ---- The rules report ---- */

/* One line for a competition object: its stage count, its current stage, the clubs of the group stages in
 * stages 0 to phase2 - 1 and in the stages after them (phase2: 8 for the Champions League, whose second group
 * stage of the original starts at stage 8, 0x585255; 13 for the UEFA Cup, whose groups are all one stage), and the clubs and rounds of its knockout stage (a League_Stage has
 * type byte 2 at +0x43, a Cup_Stage 1: the load reads the same byte, 0x5230e0 and 0x68b4f0; +0x3e is the club
 * count of both, +0x3c the rounds of a Cup_Stage). It only reads memory. */
static void report_comp(FILE *f, int season, const char *phase, const char *file, uint32_t obj, int phase2)
{
    int count = stage_count(obj), now = (int32_t)rd32(g_mem, obj + 0x30);
    uint32_t arr = rd32(g_mem, obj + 0xc);
    int groups = 0, groups2 = 0, ko_clubs = 0, ko_rounds = 0;
    for (int i = 0; i <= now && i < count && i < 16 && readable(arr); i++) {
        uint32_t st = rd32(g_mem, arr + 4u * (uint32_t)i);
        if (!readable(st)) continue;
        if (g_mem[st + 0x43] == 2) {
            if (i < phase2) groups += (int16_t)rd16(g_mem, st + 0x3e);
            else groups2 += (int16_t)rd16(g_mem, st + 0x3e);
        } else if (g_mem[st + 0x43] == 1) {
            ko_clubs = (int16_t)rd16(g_mem, st + 0x3e);
            ko_rounds = (int16_t)rd16(g_mem, st + 0x3c);
        }
    }
    /* The winner of the history row of the competition with the label year season + 1 (the history table at
     * [0xae23d4], 0x1a bytes a row, count [0xae237c]: +4 the competition record, +8 the label year, +0xa the
     * winner; club +4 the name; season_history.c), or "-". */
    char winner[64] = "-";
    uint32_t rows = rd32(g_mem, 0xae23d4u), rec = rd32(g_mem, obj + 4);
    int32_t n = (int32_t)rd32(g_mem, 0xae237cu);
    for (int32_t i = 0; readable(rows) && readable(rec) && i < n && i < 200000; i++) {
        uint32_t row = rows + 0x1au * (uint32_t)i;
        if (rd32(g_mem, row + 4) != rec || (int16_t)rd16(g_mem, row + 8) != season + 1) continue;
        uint32_t club = rd32(g_mem, row + 0xa);
        if (readable(club)) snprintf(winner, sizeof winner, "%.50s", win_str(club + 4));
    }
    for (char *p = winner; *p; p++)
        if (*p == '\t' || *p == '\n') *p = ' ';
    fprintf(f, "uefa\t%d\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n", season, phase, file, count, now, groups, groups2,
            ko_clubs, ko_rounds, winner);
}

/* The list that the Schedule tab shows: Comp::build_match_date_list 0x48d500, thiscall (&count), a game block of
 * 20-byte entries (+0 stage, +0xc day of the year, +0xe year, +0x12 round code, 0 for a league match day:
 * 0x48d6c2), freed with 0x9452ca as its callers do. One line: the entries, the entries with a year outside
 * the season and the year after it, the league entries of stages 8 and later, and the first and last date. */
static void report_schedule(FILE *f, int season, const char *file, uint32_t obj)
{
    uint32_t saved_esp = cpu.esp;
    cpu.esp -= 0x20;
    uint32_t count_at = cpu.esp + 0x10;
    wr32(g_mem, count_at, 0);
    uint32_t a[1] = {count_at};
    uint32_t list = call_this(0x0048d500u, obj, 1, a);
    if (rt_unwinding) return;
    int n = (int32_t)rd32(g_mem, count_at), bad = 0, late_league = 0, first = 9999999, last = 0;
    cpu.esp = saved_esp;
    for (int i = 0; readable(list) && i < n && i < 5000; i++) {
        uint32_t e = list + 20u * (uint32_t)i;
        int stage = (int32_t)rd32(g_mem, e), day = (int16_t)rd16(g_mem, e + 0xc), year = (int16_t)rd16(g_mem, e + 0xe);
        if (year < season || year > season + 1) bad++;
        if (stage >= 8 && rd16(g_mem, e + 0x12) == 0) late_league++;
        int key = year * 1000 + day;
        if (key < first) first = key;
        if (key > last) last = key;
    }
    if (readable(list)) {
        uint32_t fr[1] = {list};
        call_cdecl(F_FREE, 1, fr);
        if (rt_unwinding) return;
    }
    fprintf(f, "schedule\t%d\tend\t%s\t%d\t%d\t%d\t%d\t%d\n", season, file, n, bad, late_league, first, last);
}

void season_uefa_report(void *file, int season, const char *phase)
{
    FILE *f = (FILE *)file;
    if (!f) return;
    uint32_t cl = cl_object();
    if (cl) report_comp(f, season, phase, "european_cup", cl, 8);
    int32_t key = (int32_t)rd32(g_mem, KEY_UC);
    uint32_t table = rd32(g_mem, COMP_TABLE);
    uint32_t uc = key >= 0 && readable(table) ? rd32(g_mem, table + 4u * (uint32_t)key) : 0;
    if (readable(uc) && rd32(g_mem, uc) == 0x00970790u) report_comp(f, season, phase, "uefa_cup", uc, 13);
    uint32_t ecl = season_uefa_ecl_object();                           /* slice 24 */
    if (season_uefa_cup_kind(ecl) == 'k') report_comp(f, season, phase, SEASON_UEFA_ECL_FILE, ecl, 13);
    /* The labels of the rounds that have a real name in the round names table (Q370), at the end of a season. */
    if (strcmp(phase, "end")) return;
    if (cl) report_schedule(f, season, "european_cup", cl);
    if (readable(uc) && rd32(g_mem, uc) == 0x00970790u) report_schedule(f, season, "uefa_cup", uc);
    static const struct { const char *file; int code; } labels[] = {
        {"european_cup", 20}, {"uefa_cup", 30}, {"uefa_cup", 40}, {"uefa_cup", 250},
        /* slice 23: the qualifying rounds of 2009/10 on */
        {"european_cup", 220}, {"european_cup", 230}, {"european_cup", 240}, {"uefa_cup", 220}, {"uefa_cup", 240},
        {"uefa_cup", 270},
        /* slice 24: the Conference League */
        {SEASON_UEFA_ECL_FILE, 250}, {SEASON_UEFA_ECL_FILE, 260}, {SEASON_UEFA_ECL_FILE, 270}, {SEASON_UEFA_ECL_FILE, 220},
        {SEASON_UEFA_ECL_FILE, 30}, {SEASON_UEFA_ECL_FILE, 40},
        /* slice 25: the Champions League knockout phase play-offs, the stage name of the league phase */
        {"european_cup", 10}, {"european_cup", (int)LEAGUE_NAME}, {"uefa_cup", (int)LEAGUE_NAME}};
    for (unsigned i = 0; i < sizeof labels / sizeof labels[0] && !rt_unwinding; i++)
        for (int style = 1; style <= 3 && !rt_unwinding; style += 2) {      /* 1 long names, 3 abbreviated */
            uint32_t obj = !strcmp(labels[i].file, "european_cup") ? cl : !strcmp(labels[i].file, "uefa_cup") ? uc : ecl;
            if (obj == ecl && !readable(obj)) continue;
            char text[128];
            season_round_name_label(obj, labels[i].code, style, text, sizeof text);
            for (char *p = text; *p; p++)
                if (*p == '\t' || *p == '\n') *p = ' ';
            fprintf(f, "roundname\t%d\t%s\t%s\t%d\t%s\t%d\n", season, phase, labels[i].file, labels[i].code, text,
                    style);
        }
}
